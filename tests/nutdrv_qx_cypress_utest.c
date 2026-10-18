/*  nutdrv_qx_cypress_utest.c - unit tests for the nutdrv_qx "cypress" USB
 *  transport, run against a simulated Cypress 0665:5161 USB-to-serial bridge
 *
 *  Copyright (C)
 *	2026		Arcadiy Ivanov <arcadiy@ivanov.biz>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *  The simulated bridge models what usbmon showed on a Voltronic-based
 *  UPS (OptiUPS OLLV1K0) behind a 0665:5161 bridge:
 *  - the UPS answers a command after 80..408 ms, then emits the rest of the
 *    reply in 8-byte reports with pauses of up to 160 ms between them;
 *  - while nobody reads the interrupt endpoint the bridge keeps only the
 *    newest 8 reports and drops the oldest ones.
 *  So an unread 10-report QGS reply loses its first 16 bytes, and a driver
 *  which reads it as the reply to its next command stays one reply behind.
 */

#include "config.h"

/* Route the libusb calls which nutdrv_qx.c makes through the usb-common.h
 * wrappers to the simulated bridge below. The real libusb library is still
 * linked for the rest of the driver code. */
#define libusb_interrupt_transfer	sim_libusb_interrupt_transfer
#define libusb_control_transfer	sim_libusb_control_transfer
#define libusb_reset_device	sim_libusb_reset_device
#define libusb_clear_halt	sim_libusb_clear_halt

/* The driver's clock follows the simulated time, and what it logs is
 * counted. (A function-like macro, so it also turns the system prototype
 * into one for the simulated call; declared here for systems where that
 * prototype is not spelled as a plain call.) */
struct timeval;
int	sim_gettimeofday(struct timeval *tv);
#define gettimeofday(tv, tz)	sim_gettimeofday(tv)
#define upslogx	sim_upslogx

#include "nutdrv_qx.c"

#ifndef WIN32
# include <sys/wait.h>
#endif

#define SIM_REPORT_SIZE	8
#define SIM_QUEUE_DEPTH	8	/* reports the bridge keeps while nobody reads */
#define SIM_POLL_INTERVAL	8	/* ms between interrupt-IN polls of the host */
#define SIM_MAX_SCHED	64

typedef struct {
	long	at;	/* simulated time (ms) the report reaches the bridge */
	unsigned char	data[SIM_REPORT_SIZE];
} sim_report_t;

static long	sim_now;	/* simulated clock, ms */

/* UPS output which has not reached the bridge yet, in time order */
static sim_report_t	sim_sched[SIM_MAX_SCHED];
static size_t	sim_sched_len;

/* Reports held by the bridge */
static sim_report_t	sim_queue[SIM_QUEUE_DEPTH];
static size_t	sim_queue_len;
static size_t	sim_dropped;

/* UPS timing: first report after sim_latency, then sim_gaps[] between reports */
static long	sim_latency;
static const long	*sim_gaps;
static size_t	sim_gaps_len;
#define SIM_SET_GAPS(g)	do { sim_gaps = (g); sim_gaps_len = SIZEOF_ARRAY(g); } while (0)

/* A device which floods the endpoint with a report per poll, until reset */
static int	sim_flooding;

/* Zero-length reports the bridge delivers before anything else */
static int	sim_zero_length_reports;

static char	sim_cmd[SIM_REPORT_SIZE * 8];
static size_t	sim_cmd_len;

static int	sim_resets, sim_opens, sim_closes;

/* USB ID the simulated device reports when opened */
static uint16_t	sim_vendorid, sim_productid;

static long	sim_udev_storage;
#define SIM_UDEV	((usb_dev_handle *)(void *)&sim_udev_storage)

/* Measured on the OptiUPS: median gaps of a QGS reply, and the largest ones */
static const long	gaps_typical[] = { 24, 40, 24, 40, 24, 24, 40, 8, 8 };
static const long	gaps_worst[] = { 120, 160, 120, 144, 120, 112, 152, 16, 8 };

static const struct {
	const char	*cmd;
	const char	*reply;
} sim_replies[] = {
	{ "QGS",	"(122.1 60.0 122.0 60.0 001.7 020 197.8 216.8 027.4 ---.- 025.8 100000000001\r" },
	{ "QMOD",	"(E\r" },
	{ "QWS",	"(0000000000000000000000000000000000000000000000000000000000000000\r" },
	{ "QBV",	"(027.3 02 12 100 00562\r" },
	{ "QLDL",	"(015 020\r" },
	{ "Q1",	"(208.4 140.0 208.4 034 59.9 2.05 35.0 00110000\r" },
	{ NULL,	NULL }
};

static const char	*sim_reply_for(const char *cmd)
{
	size_t	i;

	for (i = 0; sim_replies[i].cmd; i++) {
		if (!strcmp(sim_replies[i].cmd, cmd))
			return sim_replies[i].reply;
	}
	return "(NAK\r";
}

static void	sim_bridge_receive(const sim_report_t *r)
{
	if (sim_queue_len == SIM_QUEUE_DEPTH) {
		memmove(&sim_queue[0], &sim_queue[1], sizeof(sim_queue[0]) * (SIM_QUEUE_DEPTH - 1));
		sim_queue_len--;
		sim_dropped++;
	}
	sim_queue[sim_queue_len++] = *r;
}

/* Deliver to the bridge everything the UPS has sent up to time t */
static void	sim_advance(long t)
{
	while (sim_sched_len && sim_sched[0].at <= t) {
		sim_bridge_receive(&sim_sched[0]);
		memmove(&sim_sched[0], &sim_sched[1], sizeof(sim_sched[0]) * (sim_sched_len - 1));
		sim_sched_len--;
	}
}

/* Queue UPS output starting at time start */
static void	sim_emit(const char *text, long start)
{
	size_t	len = strlen(text), off, n = 0;
	long	t = start;

	for (off = 0; off < len; off += SIM_REPORT_SIZE, n++) {
		sim_report_t	*r;
		size_t	chunk = len - off < SIM_REPORT_SIZE ? len - off : SIM_REPORT_SIZE;

		if (sim_sched_len == SIM_MAX_SCHED) {
			fprintf(stderr, "simulator schedule overflow\n");
			exit(EXIT_FAILURE);
		}
		if (n > 0)
			t += sim_gaps[(n - 1) % sim_gaps_len];
		r = &sim_sched[sim_sched_len++];
		r->at = t;
		memset(r->data, 0, SIM_REPORT_SIZE);
		memcpy(r->data, text + off, chunk);
	}
}

int	sim_gettimeofday(struct timeval *tv)
{
	tv->tv_sec = (time_t)(sim_now / 1000);
	tv->tv_usec = (int)((sim_now % 1000) * 1000);
	return 0;
}

/* NOTICE messages about stale data before a command, and the last one */
static int	sim_stale_notices;
static char	sim_last_stale_notice[LARGEBUF];

void	sim_upslogx(int priority, const char *fmt, ...)
{
	char	msg[LARGEBUF];
	va_list	va;

	va_start(va, fmt);
	vsnprintf(msg, sizeof(msg), fmt, va);
	va_end(va);

	if (priority == LOG_NOTICE && strstr(msg, "stale input report")) {
		sim_stale_notices++;
		snprintf(sim_last_stale_notice, sizeof(sim_last_stale_notice), "%s", msg);
	}
	fprintf(stderr, "upslogx(%d): %s\n", priority, msg);
}

int	LIBUSB_CALL sim_libusb_interrupt_transfer(libusb_device_handle *dev_handle,
	unsigned char endpoint, unsigned char *data, int length,
	int *actual_length, unsigned int timeout)
{
	NUT_UNUSED_VARIABLE(dev_handle);
	NUT_UNUSED_VARIABLE(endpoint);

	if (sim_flooding) {
		sim_now += SIM_POLL_INTERVAL;
		*actual_length = length < SIM_REPORT_SIZE ? length : SIM_REPORT_SIZE;
		memcpy(data, "#FLOOD#\r", (size_t)*actual_length);
		return LIBUSB_SUCCESS;
	}

	if (sim_zero_length_reports > 0) {
		sim_zero_length_reports--;
		sim_now += SIM_POLL_INTERVAL;
		*actual_length = 0;
		return LIBUSB_SUCCESS;
	}

	sim_advance(sim_now);
	if (sim_queue_len) {
		sim_now += SIM_POLL_INTERVAL;
	} else {
		long	next = sim_sched_len ? sim_sched[0].at : -1;

		if (next < 0 || next > sim_now + (long)timeout) {
			sim_now += (long)timeout;
			sim_advance(sim_now);
			*actual_length = 0;
			return LIBUSB_ERROR_TIMEOUT;
		}
		sim_now = next;
		sim_advance(sim_now);
	}

	*actual_length = length < SIM_REPORT_SIZE ? length : SIM_REPORT_SIZE;
	memcpy(data, sim_queue[0].data, (size_t)*actual_length);
	memmove(&sim_queue[0], &sim_queue[1], sizeof(sim_queue[0]) * (sim_queue_len - 1));
	sim_queue_len--;
	return LIBUSB_SUCCESS;
}

int	LIBUSB_CALL sim_libusb_control_transfer(libusb_device_handle *dev_handle,
	uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
	unsigned char *data, uint16_t wLength, unsigned int timeout)
{
	size_t	i;

	NUT_UNUSED_VARIABLE(dev_handle);
	NUT_UNUSED_VARIABLE(request_type);
	NUT_UNUSED_VARIABLE(bRequest);
	NUT_UNUSED_VARIABLE(wValue);
	NUT_UNUSED_VARIABLE(wIndex);
	NUT_UNUSED_VARIABLE(timeout);

	sim_now++;
	for (i = 0; i < wLength; i++) {
		if (!data[i])
			break;
		if (data[i] != '\r') {
			if (sim_cmd_len < sizeof(sim_cmd) - 1)
				sim_cmd[sim_cmd_len++] = (char)data[i];
			continue;
		}
		sim_cmd[sim_cmd_len] = '\0';
		sim_emit(sim_reply_for(sim_cmd), sim_now + sim_latency);
		sim_cmd_len = 0;
	}
	return wLength;
}

int	LIBUSB_CALL sim_libusb_reset_device(libusb_device_handle *dev_handle)
{
	NUT_UNUSED_VARIABLE(dev_handle);

	sim_resets++;
	sim_queue_len = 0;
	sim_sched_len = 0;
	sim_flooding = 0;
	return LIBUSB_SUCCESS;
}

int	LIBUSB_CALL sim_libusb_clear_halt(libusb_device_handle *dev_handle, unsigned char endpoint)
{
	NUT_UNUSED_VARIABLE(dev_handle);
	NUT_UNUSED_VARIABLE(endpoint);
	return LIBUSB_SUCCESS;
}

static int	sim_open_dev(usb_dev_handle **sdevp, USBDevice_t *curDevice,
	USBDeviceMatcher_t *matcher,
	int (*callback)(usb_dev_handle *udev, USBDevice_t *hd,
		usb_ctrl_charbuf rdbuf, usb_ctrl_charbufsize rdlen))
{
	USBDeviceMatcher_t	*m;

	NUT_UNUSED_VARIABLE(callback);

	curDevice->VendorID = sim_vendorid;
	curDevice->ProductID = sim_productid;
	/* Like the real open_dev: the driver's matchers pick (and, without an
	 * explicit subdriver, identify) the device */
	for (m = matcher; m; m = m->next) {
		if (m->match_function(curDevice, m->privdata) != 1)
			return -1;
	}
	sim_opens++;
	*sdevp = SIM_UDEV;
	return 1;
}

static void	sim_close_dev(usb_dev_handle *sdev)
{
	NUT_UNUSED_VARIABLE(sdev);
	sim_closes++;
}

static usb_communication_subdriver_t	sim_usb_subdriver;

static void	sim_setup(void)
{
	sim_now = 0;
	sim_sched_len = 0;
	sim_queue_len = 0;
	sim_dropped = 0;
	sim_latency = 150;
	SIM_SET_GAPS(gaps_typical);
	sim_flooding = 0;
	sim_zero_length_reports = 0;
	sim_cmd_len = 0;
	sim_resets = sim_opens = sim_closes = 0;
	sim_vendorid = 0x0665;
	sim_productid = 0x5161;
	sim_stale_notices = 0;
	sim_last_stale_notice[0] = '\0';

	memset(&sim_usb_subdriver, 0, sizeof(sim_usb_subdriver));
	sim_usb_subdriver.name = "simulated 0665:5161 bridge";
	sim_usb_subdriver.open_dev = sim_open_dev;
	sim_usb_subdriver.close_dev = sim_close_dev;

	/* Reported by the reconnection messages */
	upsname = "simulated";
	device_name = "simulated";

	usb = &sim_usb_subdriver;
	udev = SIM_UDEV;
#ifdef QX_SERIAL
	is_usb = 1;	/* as upsdrv_initups() chooses with "port = auto" */
#endif
	subdriver_command = &cypress_command;
	cypress_0665_5161_quirk = TRUE;
	cypress_reply_outstanding = FALSE;
	cypress_stale_reported = FALSE;
}

/* == Test helpers == */

static int	case_failed;

static void	expect(int cond, const char *what)
{
	if (!cond) {
		printf("    FAILED: %s\n", what);
		case_failed = 1;
	}
}

/* Send cmd through qx_command() like the driver does and check the reply
 * (up to, excluding, its CR) against expected; NULL expects a failure. */
static void	expect_reply(const char *cmd, const char *expected)
{
	char	cmdbuf[SMALLBUF], buf[SMALLBUF], what[SMALLBUF * 3];
	ssize_t	ret;
	size_t	len;

	snprintf(cmdbuf, sizeof(cmdbuf), "%s\r", cmd);
	memset(buf, 0, sizeof(buf));
	ret = qx_command(cmdbuf, strlen(cmdbuf), buf, sizeof(buf));
	len = ret > 0 ? strcspn(buf, "\r") : 0;

	if (expected == NULL) {
		snprintf(what, sizeof(what), "%s at %ld ms: expected a failure, got %" PRIiSIZE " '%.*s'",
			cmd, sim_now, ret, (int)len, buf);
		expect(ret <= 0, what);
		return;
	}

	snprintf(what, sizeof(what), "%s at %ld ms: expected '%.*s', got %" PRIiSIZE " '%.*s'",
		cmd, sim_now, (int)strcspn(expected, "\r"), expected, ret, (int)len, buf);
	expect(ret > 0 && len == strcspn(expected, "\r") && !strncmp(buf, expected, len), what);
}

static void	expect_walk(void)
{
	size_t	i;

	for (i = 0; sim_replies[i].cmd; i++)
		expect_reply(sim_replies[i].cmd, sim_replies[i].reply);
}

/* Commands against a device which floods again after every reset */
static void	expect_desyncs(int count)
{
	int	i;

	for (i = 0; i < count; i++) {
		sim_flooding = 1;
		expect_reply("QGS", NULL);
	}
}

/* The state found on the stuck driver: the bridge holds the last 8 reports
 * of a QGS reply which nobody read */
static void	sim_queue_truncated_qgs(void)
{
	sim_emit(sim_reply_for("QGS"), sim_now);
	sim_advance(sim_now + 5000);
	sim_now += 5000;
	expect(sim_queue_len == SIM_QUEUE_DEPTH && sim_dropped == 2,
		"setup: bridge holds the newest 8 reports of the QGS reply");
}

static int	driver_state_is(const char *state)
{
	const char	*val = dstate_getinfo("driver.state");

	return val != NULL && !strcmp(val, state);
}

/* == Test cases == */

/* Normal polling with the measured latencies and pauses: every reply belongs
 * to its own command, nothing is discarded and nothing is reset. */
static void	test_normal_polling(void)
{
	int	i;

	for (i = 0; i < 3; i++) {
		expect_walk();
		sim_now += 2000;
	}

	sim_latency = 408;
	SIM_SET_GAPS(gaps_worst);
	for (i = 0; i < 3; i++) {
		expect_walk();
		sim_now += 2000;
	}

	expect(sim_dropped == 0, "no reports dropped by the bridge");
	expect(sim_resets == 0, "no USB resets");
}

/* The state found on the stuck driver: the next command must get its own
 * reply, not the truncated one queued in the bridge. */
static void	test_truncated_reply_queued(void)
{
	sim_queue_truncated_qgs();

	expect_reply("QGS", sim_reply_for("QGS"));
	sim_now += 2000;
	expect_walk();
	expect(sim_resets == 0, "no USB resets");
}

/* The UPS answers a command only after the driver gave up waiting. That late
 * reply must not be taken for the reply to the next command. */
static void	test_late_reply_after_timeout(void)
{
	sim_latency = 3100;	/* beyond the reply timeout */
	expect_reply("QMOD", NULL);
	sim_latency = 150;

	expect_reply("QBV", sim_reply_for("QBV"));
	expect_reply("QLDL", sim_reply_for("QLDL"));
	sim_now += 2000;
	expect_walk();
	expect(sim_resets == 0, "no USB resets");
}

/* Output the driver did not ask for (or a late reply of unknown origin)
 * starts arriving just before a command; the UPS pauses within it for
 * longer than a short drain waits. None of it may be read as the reply. */
static void	test_unsolicited_output_with_pauses(void)
{
	SIM_SET_GAPS(gaps_worst);
	sim_emit(sim_reply_for("QGS"), sim_now + 5);

	expect_reply("QLDL", sim_reply_for("QLDL"));
	expect_reply("QBV", sim_reply_for("QBV"));
	sim_now += 2000;
	expect_walk();
	expect(sim_resets == 0, "no USB resets");
}

/* The endpoint never goes quiet, so draining cannot resynchronize: the
 * driver must reset the USB device and try again, not exit. */
static void	test_flood_resets_device(void)
{
	expect_desyncs(1);
	expect(sim_resets == 1, "the USB device was reset");
	expect(sim_closes == 1 && udev == NULL, "the device was closed for reconnection");

	/* The reset cleared the flood; the reconnected device works again */
	expect_reply("QGS", sim_reply_for("QGS"));
	expect(sim_opens == 1 && udev == SIM_UDEV, "the device was reopened");
	sim_now += 2000;
	expect_walk();
	expect(sim_resets == 1, "no further USB resets");
}

/* The device still floods when it is reopened: that is no reconnection */
static void	test_flood_while_reconnecting(void)
{
	expect_desyncs(2);
	expect(sim_resets == 2 && udev == NULL, "the USB device was reset again");
	expect(driver_state_is("reconnect.trying"),
		"a reconnection which failed is not reported as re-established");

	expect_reply("QGS", sim_reply_for("QGS"));
	expect(driver_state_is("quiet"), "the reconnection which worked is reported");
}

/* Stays busy for longer than a reply takes, but reports come too slowly for
 * the report count limit: the driver must give up in bounded time (upsd
 * declares the data stale after 15 s by default) and reset the device. */
static void	test_trickle_gives_up_in_time(void)
{
	static const long	gaps_trickle[] = { 400, 380 };
	char	trickle[SIM_REPORT_SIZE * 40 + 1], what[SMALLBUF];
	long	start = sim_now;
	size_t	i;

	for (i = 0; i + 1 < sizeof(trickle); i++)
		trickle[i] = "#TRICKLE"[i % SIM_REPORT_SIZE];
	trickle[i] = '\0';
	SIM_SET_GAPS(gaps_trickle);
	sim_emit(trickle, sim_now);

	expect_reply("QGS", NULL);
	snprintf(what, sizeof(what), "the driver gave up within 4000 ms, took %ld ms", sim_now - start);
	expect(sim_now - start <= 4000, what);
	expect(sim_resets == 1, "the USB device was reset");

	SIM_SET_GAPS(gaps_typical);
	expect_reply("QGS", sim_reply_for("QGS"));
}

/* A device which sends something unasked before every command is reported
 * once, not on every poll; it is reported again once it was quiet between. */
static void	test_stale_notice_throttled(void)
{
	int	i;

	for (i = 0; i < 4; i++) {
		sim_emit("#STALE1#STALE2\r", sim_now);
		expect_reply("QBV", sim_reply_for("QBV"));
	}
	expect(sim_stale_notices == 1, "stale data before consecutive commands is reported once");
	expect(strstr(sim_last_stale_notice, "discarded 2 stale input report(s) before QBV:") != NULL,
		"the notice counts both stale reports");

	expect_reply("QLDL", sim_reply_for("QLDL"));
	sim_emit("#STALE1#STALE2\r", sim_now);
	expect_reply("QBV", sim_reply_for("QBV"));
	expect(sim_stale_notices == 2, "stale data after a quiet endpoint is reported again");
}

/* Commands of some protocols end with binary CRC bytes (here the Axpert
 * QPI command): the notice must name the command in readable text only */
static void	test_stale_notice_readable(void)
{
	char	what[LARGEBUF + SMALLBUF];
	size_t	i;

	sim_emit("#STALE1#STALE2\r", sim_now);
	expect_reply("QPI\xbe\xac", sim_reply_for("QPI"));
	snprintf(what, sizeof(what), "the notice names the command readably: '%s'", sim_last_stale_notice);
	expect(strstr(sim_last_stale_notice, " before QPI:") != NULL, what);
	for (i = 0; sim_last_stale_notice[i]; i++) {
		if (!isprint((unsigned char)sim_last_stale_notice[i]))
			break;
	}
	expect(sim_last_stale_notice[i] == '\0', "the notice has no unprintable characters");
}

/* A zero-length report before a command carries no reply: the command must
 * still be sent, and its own reply returned */
static void	test_zero_length_reports(void)
{
	sim_zero_length_reports = 2;
	expect_reply("QGS", sim_reply_for("QGS"));
	expect(sim_zero_length_reports == 0, "setup: the zero-length reports were read");
}

/* The driver may only give up once the reset limit was used up twice below,
 * and only if nothing else in the case failed before */
static void	check_reset_limit_exit(void)
{
	if (case_failed)
		_exit(2);
	if (sim_resets != 2 * QX_USB_DESYNC_RESET_TRIES) {
		printf("    FAILED: the driver exited after %d USB resets, expected %d\n",
			sim_resets, 2 * QX_USB_DESYNC_RESET_TRIES);
		fflush(stdout);
		_exit(3);
	}
}

/* Resets are bounded: as long as they help, the count starts over, but a
 * device which stays flooded after several resets in a row is given up on. */
static void	test_flood_reset_limit(void)
{
	atexit(check_reset_limit_exit);

	expect_desyncs(QX_USB_DESYNC_RESET_TRIES);
	expect(sim_resets == QX_USB_DESYNC_RESET_TRIES, "one reset per desync, up to the limit");

	/* The device recovers: a good reply forgets the earlier resets */
	expect_reply("QGS", sim_reply_for("QGS"));

	expect_desyncs(QX_USB_DESYNC_RESET_TRIES);
	expect(sim_resets == 2 * QX_USB_DESYNC_RESET_TRIES, "the limit applies to resets in a row");

	/* One more desync in a row: the driver gives up (exits) */
	expect_desyncs(1);
	expect(0, "the driver should have exited after too many resets in a row");
}

/* Starts the driver on the simulated device as configured in ups.conf:
 * with "subdriver = subdrv" (and the device's USB ID) unless that is NULL,
 * so the USB ID table picks the subdriver, and with
 * "cypress_drain_quirk = drain_quirk" unless that is NULL */
static void	init_from_ups_conf(const char *subdrv, const char *drain_quirk)
{
	char	subdrv_name[16], protocol[] = "q1";
	char	vendorid[8], productid[8], quirk[16];

	udev = NULL;
	subdriver_command = NULL;
	cypress_0665_5161_quirk = FALSE;
	device_path = xstrdup("auto");	/* owned (and freed) by the driver core */

	upsdrv_makevartable();
	if (subdrv) {
		snprintf(subdrv_name, sizeof(subdrv_name), "%s", subdrv);
		snprintf(vendorid, sizeof(vendorid), "%04x", sim_vendorid);
		snprintf(productid, sizeof(productid), "%04x", sim_productid);
		storeval("subdriver", subdrv_name);
		storeval("vendorid", vendorid);
		storeval("productid", productid);
	}
	storeval("protocol", protocol);
	if (drain_quirk) {
		snprintf(quirk, sizeof(quirk), "%s", drain_quirk);
		storeval("cypress_drain_quirk", quirk);
	}

	upsdrv_initups();
	expect(subdriver_command == &cypress_command, "setup: the cypress transport is used");
}

/* An explicit subdriver still gets the 0665:5161 quirk by default, and the
 * resync works: the stuck state from the field recovers at once */
static void	test_explicit_subdriver_default_quirk(void)
{
	init_from_ups_conf("cypress", NULL);
	expect(cypress_0665_5161_quirk == TRUE, "the quirk is enabled for 0665:5161");

	sim_now += 2000;
	sim_queue_truncated_qgs();
	expect_reply("QGS", sim_reply_for("QGS"));
}

static void	test_explicit_subdriver_quirk_off(void)
{
	init_from_ups_conf("cypress", "off");
	expect(!cypress_0665_5161_quirk, "cypress_drain_quirk = off disables the quirk for 0665:5161");
}

static void	test_explicit_subdriver_other_device(void)
{
	sim_vendorid = 0x04b4;
	sim_productid = 0x5500;
	init_from_ups_conf("cypress", NULL);
	expect(!cypress_0665_5161_quirk, "the quirk is not enabled by default for other USB IDs");
}

static void	test_explicit_subdriver_other_device_quirk_on(void)
{
	sim_vendorid = 0x04b4;
	sim_productid = 0x5500;
	init_from_ups_conf("cypress", "on");
	expect(cypress_0665_5161_quirk == TRUE, "cypress_drain_quirk = on enables the quirk for other USB IDs");
}

static void	test_detected_subdriver_default_quirk(void)
{
	init_from_ups_conf(NULL, NULL);
	expect(cypress_0665_5161_quirk == TRUE, "the quirk is enabled for 0665:5161");
}

static void	test_detected_subdriver_quirk_off(void)
{
	init_from_ups_conf(NULL, "off");
	expect(!cypress_0665_5161_quirk, "cypress_drain_quirk = off disables the quirk for 0665:5161");
}

/* A device of another USB ID which the table also assigns to "cypress" */
static void	test_detected_subdriver_other_device(void)
{
	sim_vendorid = PHOENIXTEC_VENDORID;
	sim_productid = 0x0002;
	init_from_ups_conf(NULL, NULL);
	expect(!cypress_0665_5161_quirk, "the quirk is not enabled by default for other USB IDs");
}

typedef struct {
	const char	*name;
	void	(*run)(void);
	int	expect_exit;	/* the case ends with the driver exiting */
	int	own_process;	/* the case leaves state behind which no setup clears
				 * (e.g. ups.conf values), so it needs fork() */
} test_case_t;

static const test_case_t	test_cases[] = {
	{ "normal polling with measured timing",	test_normal_polling,	0,	0 },
	{ "truncated stale reply queued in the bridge",	test_truncated_reply_queued,	0,	0 },
	{ "late reply after a reply timeout",	test_late_reply_after_timeout,	0,	0 },
	{ "unsolicited output with pauses",	test_unsolicited_output_with_pauses,	0,	0 },
	{ "stale data notice is not repeated",	test_stale_notice_throttled,	0,	0 },
	{ "stale data notice is readable",	test_stale_notice_readable,	0,	0 },
	{ "zero-length reports before a command",	test_zero_length_reports,	0,	0 },
	{ "endpoint flood resets the device",	test_flood_resets_device,	0,	0 },
	{ "endpoint flood while reconnecting",	test_flood_while_reconnecting,	0,	0 },
	{ "slow endpoint trickle gives up in time",	test_trickle_gives_up_in_time,	0,	0 },
	{ "endpoint flood reset limit",	test_flood_reset_limit,	1,	1 },
	{ "explicit subdriver: quirk by USB ID",	test_explicit_subdriver_default_quirk,	0,	1 },
	{ "explicit subdriver: quirk turned off",	test_explicit_subdriver_quirk_off,	0,	1 },
	{ "explicit subdriver: other USB ID",	test_explicit_subdriver_other_device,	0,	1 },
	{ "explicit subdriver: other USB ID, quirk turned on",	test_explicit_subdriver_other_device_quirk_on,	0,	1 },
	{ "detected subdriver: quirk by USB ID",	test_detected_subdriver_default_quirk,	0,	1 },
	{ "detected subdriver: quirk turned off",	test_detected_subdriver_quirk_off,	0,	1 },
	{ "detected subdriver: other USB ID",	test_detected_subdriver_other_device,	0,	1 },
	{ NULL,	NULL,	0,	0 }
};

/* Runs a case in a child process where possible: the driver keeps state in
 * static variables, and giving up on a device ends the process. */
static int	run_case(const test_case_t *tc)
{
#ifndef WIN32
	pid_t	pid;
	int	status;

	fflush(stdout);
	fflush(stderr);
	pid = fork();
	if (pid < 0) {
		perror("fork");
		return 1;
	}
	if (pid == 0) {
		sim_setup();
		tc->run();
		fflush(stdout);
		_exit(case_failed ? 2 : 0);
	}
	if (waitpid(pid, &status, 0) != pid) {
		perror("waitpid");
		return 1;
	}
	if (tc->expect_exit)
		return !(WIFEXITED(status) && WEXITSTATUS(status) == EXIT_FAILURE);
	return !(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#else	/* WIN32 */
	if (tc->own_process) {
		printf("    SKIPPED: needs fork()\n");
		return 0;
	}
	case_failed = 0;
	sim_setup();
	tc->run();
	return case_failed;
#endif	/* WIN32 */
}

int	main(int argc, char **argv)
{
	const test_case_t	*tc;
	int	failed = 0, passed = 0;

	NUT_UNUSED_VARIABLE(argc);
	NUT_UNUSED_VARIABLE(argv);

	for (tc = test_cases; tc->name; tc++) {
		int	result;

		printf("Test: %s\n", tc->name);
		result = run_case(tc);
		printf("  %s\n", result ? "FAIL" : "pass");
		if (result)
			failed++;
		else
			passed++;
	}

	printf("%d passed, %d failed\n", passed, failed);
	return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
