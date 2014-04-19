/*!@file tripplite_usb.c 
 * @brief Driver for Tripp Lite non-PDC/HID USB models.
 */
/*
   tripplite_usb.c was derived from tripplite.c by Charles Lepple
   tripplite.c was derived from Russell Kroll's bestups.c by Rik Faith.

   Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>
   Copyright (C) 2001  Rickard E. (Rik) Faith <faith@alephnull.com>
   Copyright (C) 2004  Nicholas J. Kain <nicholas@kain.us>
   Copyright (C) 2005-2008  Charles Lepple <clepple+nut@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/


/* % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % 
 *
 * Protocol 1001
 *
 * OMNIVS Commands: (capital letters are literals, lower-case are variables)
 * :B     -> Bxxxxyy (xxxx/55.0: Hz in, yy/16: battery voltage)
 * :F     -> F1143_A (where _ = \0) Firmware version?
 * :L     -> LvvvvXX (vvvv/2.0: VAC out)
 * :P     -> P01000X (1000VA unit)
 * :S     -> Sbb_XXX (bb = 10: on-line, 11: on battery)
 * :V     -> V102XXX (firmware/protocol version?)
 * :Wt    -> Wt      (watchdog; t = time in seconds (binary, not hex), 
 *                   0 = disable; if UPS is not pinged in this interval, it
 *                   will power off the load, and then power it back on after
 *                   a delay.)
 *
 * % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % 
 *
 * The outgoing commands are sent with HID Set_Report commands over EP0
 * (control message), and incoming commands are received on EP1IN (interrupt
 * endpoint). The UPS completely ignores the conventions of Set_Idle (where
 * you NAK the interrupt read if you have no new data), so you constantly have
 * to poll EP1IN.
 *
 * The descriptors say that bInterval is 10 ms. You generally need to wait at
 * least 80-90 ms to get some characters back from the device.  If it takes
 * more than 250 ms, you probably need to resend the command.
 * 
 * All outgoing commands are followed by a checksum, which is 255 - (sum of
 * characters after ':'), and then by '\r'. All responses should start with
 * the command letter that was sent (no colon), and should be followed by
 * '\r'. If the command is not supported (or apparently if there is a serial
 * timeout internally), the previous response will be echoed back.
 *
 * % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % 
 *
 * SMARTPRO commands (3003):
 *
 * :A     -> ?          (start self-test)
 * :D     -> D7187      (? - doesn't match tripplite.c)
 * :F     -> F1019 A    firmware rev
 * :H__   -> H          (delay before action?)
 * :I_    -> I          (set flags for conditions that cause a reset?)
 * :J__   -> J          (set 16-bit unit ID)
 * :K#0   ->            (turn outlet off: # in 0..2; 0 is main relay)
 * :K#1   ->            (turn outlet on: # in 0..2)
 * :L     -> L290D_X
 * :M     -> M007F      (min/max voltage seen)
 * :N__   -> N
 * :P     -> P01500X    (max power)
 * :Q     ->            (while online: reboot)
 * :R     -> R<01><FF>  (query flags for conditions that cause a reset?)
 * :S     -> S100_Z0    (status?)
 * :T     -> T7D2581    (temperature, frequency)
 * :U     -> U<FF><FF>  (unit ID, 1-65535)
 * :V     -> V1062XX	(outlets in groups of 2-2-4, with the groups of 2
 * 			 individually switchable.)
 * :W_    -> W_		(watchdog)
 * :Z     -> Z		(reset for max/min; takes a moment to complete)
 * 
 * % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % 
 *
 * The SMARTPRO unit seems to be slightly saner with regard to message
 * polling. It specifies an interrupt in interval of 100 ms, but I just
 * started at a 2 second timeout to obtain the above table. 
 *
 * % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % 
 *
 * Commands from serial tripplite.c:
 *
 * :N%02X -- delay the UPS for provided time (hex seconds)
 * :H%06X -- reboot the UPS.  UPS will restart after provided time (hex s)
 * :A     -- begins a self-test
 * :C     -- fetches result of a self-test
 * :K1    -- turns on power receptacles
 * :K0    -- turns off power receptacles
 * :G     -- unconfirmed: shuts down UPS until power returns
 * :Q1    -- enable "Remote Reboot"
 * :Q0    -- disable "Remote Reboot"
 * :W     -- returns 'W' data
 * :L     -- returns 'L' data
 * :V     -- returns 'V' data (firmware revision)
 * :X     -- returns 'X' data (firmware revision)
 * :D     -- returns general status data
 * :B     -- returns battery voltage (hexadecimal decivolts)
 * :I     -- returns minimum input voltage (hexadecimal hertz) [sic]
 * :M     -- returns maximum input voltage (hexadecimal hertz) [sic]
 * :P     -- returns power rating
 * :Z     -- unknown
 * :U     -- unknown
 * :O     -- unknown
 * :E     -- unknown
 * :Y     -- returns mains frequency  (':D' is preferred)
 * :T     -- returns ups temperature  (':D' is preferred)
 * :R     -- returns input voltage    (':D' is preferred)
 * :F     -- returns load percentage  (':D' is preferred)
 * :S     -- enables remote reboot/remote power on
 */

#include "main.h"
#include "libusb.h"
#include <math.h>
#include <ctype.h>
#include <usb.h>
#include "usb-common.h"

#define DRIVER_NAME		"Tripp Lite OMNIVS / SMARTPRO driver"
#define DRIVER_VERSION	"0.20"

/* driver description structure */
upsdrv_info_t	upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Charles Lepple <clepple+nut@gmail.com>\n" \
	"Russell Kroll <rkroll@exploits.org>\n" \
	"Rickard E. (Rik) Faith <faith@alephnull.com>\n" \
	"Nicholas J. Kain <nicholas@kain.us>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

/* TrippLite */
#define TRIPPLITE_VENDORID 0x09ae 

/* USB IDs device table */
static usb_device_id_t tripplite_usb_device_table[] = {
	/* e.g. OMNIVS1000, SMART550USB, ... */
	{ USB_DEVICE(TRIPPLITE_VENDORID, 0x0001), NULL },
	
	/* Terminating entry */
	{ -1, -1, NULL }
};

static int subdriver_match_func(USBDevice_t *hd, void *privdata)
{
	switch (is_usb_device_supported(tripplite_usb_device_table, hd))
	{
	case SUPPORTED:
		return 1;

	case POSSIBLY_SUPPORTED:
		/* by default, reject, unless the productid option is given */
		if (getval("productid")) {
			return 1;
		}
	case NOT_SUPPORTED:
	default:
		return 0;
	}
}

static USBDeviceMatcher_t subdriver_matcher = {
	&subdriver_match_func,
	NULL,
	NULL
};

static enum tl_model_t {
	TRIPP_LITE_UNKNOWN = 0,
	TRIPP_LITE_OMNIVS,
	TRIPP_LITE_OMNIVS_2001,
	TRIPP_LITE_SMARTPRO,
	TRIPP_LITE_SMART_0004
} tl_model = TRIPP_LITE_UNKNOWN;

/*!@brief If a character is not printable, return a dot. */
#define toprint(x) (isalnum((unsigned)x) ? (x) : '.')

#define ENDCHAR 13

#define MAX_SEND_TRIES 10
#define SEND_WAIT_SEC 0
#define SEND_WAIT_NSEC (1000*1000*100)

#define MAX_RECV_TRIES 10
#define RECV_WAIT_MSEC 1000	/*! was 100 for OMNIVS; SMARTPRO units need longer */

#define MAX_RECONNECT_TRIES 10

#define DEFAULT_OFFDELAY   64  /*!< seconds (max 0xFF) */
#define DEFAULT_STARTDELAY 60  /*!< seconds (max 0xFFFFFF) */
#define DEFAULT_BOOTDELAY  64  /*!< seconds (max 0xFF) */
#define MAX_VOLT 13.4          /*!< Max battery voltage (100%) */
#define MIN_VOLT 11.0          /*!< Min battery voltage (10%) */

static USBDevice_t *hd = NULL;
static USBDevice_t curDevice;
static USBDeviceMatcher_t *reopen_matcher = NULL;
static USBDeviceMatcher_t *regex_matcher = NULL;
static usb_dev_handle *udev;
static usb_communication_subdriver_t	*comm_driver = &usb_subdriver;

/* We calculate battery charge (q) as a function of voltage (V).
 * It seems that this function probably varies by firmware revision or
 * UPS model - the Windows monitoring software gives different q for a
 * given V than the old open source Tripp Lite monitoring software.
 *
 * The discharge curve should be the same for any given battery chemistry,
 * so it should only be necessary to specify the minimum and maximum
 * voltages likely to be seen in operation.
 */

/* Interval notation for Q% = 10% <= [minV, maxV] <= 100%  */
static double V_interval[2] = {MIN_VOLT, MAX_VOLT};

static int battery_voltage_nominal = 12,
	   input_voltage_nominal = 120,
	   input_voltage_scaled = 120,
	/* input_voltage_maximum = -1,
	   input_voltage_minimum = -1, */
	   switchable_load_banks = 0,
           unit_id = -1; /*!< range: 1-65535, most likely */

/*! Time in seconds to delay before shutting down. */
static unsigned int offdelay = DEFAULT_OFFDELAY;
/* static unsigned int bootdelay = DEFAULT_BOOTDELAY; */

/*!@brief Try to reconnect once.
 * @return 1 if reconnection was successful.
 */
static int reconnect_ups(void)
{
	int ret;

	if (hd != NULL) {
		return 1;
	}

	upsdebugx(2, "==================================================");
	upsdebugx(2, "= device has been disconnected, try to reconnect =");
	upsdebugx(2, "==================================================");

	ret = comm_driver->open(&udev, &curDevice, reopen_matcher, NULL, 1);
	if (ret < 1) {
		upslogx(LOG_INFO, "Reconnecting to UPS failed; will retry later...");
		dstate_datastale();
		return 0;
	}

	hd = &curDevice;

	return ret;
}


/*!@brief Convert a string to printable characters (in-place)
 *
 * @param[in,out] str	String to convert
 * @param[in] len	Maximum number of characters to convert, or <= 0 to
 * convert all.
 *
 * Uses toprint() macro defined above.
 */
void toprint_str(char *str, int len)
{
	int i;
	if(len <= 0) len = strlen(str);
	for(i=0; i < len; i++)
		str[i] = toprint(str[i]);
}

/*!@brief Convert N characters from hex to decimal
 *
 * @param start		Beginning of string to convert
 * @param len		Maximum number of characters to consider (max 32)
 *
 * @a len characters of @a start are copied to a temporary buffer, then passed
 * to strtol() to be converted to decimal.
 *
 * @return See strtol(3)
 */
static int hex2d(const unsigned char *start, unsigned int len)
{
	unsigned char buf[32];
	buf[31] = '\0';

	strncpy((char *)buf, (const char *)start, (len < (sizeof buf) ? len : (sizeof buf - 1)));
	if(len < sizeof(buf)) buf[len] = '\0';
	return strtol((char *)buf, NULL, 16);
}

/*!@brief Dump message in both hex and ASCII
 *
 * @param[in] msg	Buffer to dump
 * @param[in] len	Number of bytes to dump
 *
 * @return		Pointer to static buffer with decoded message
 */
static const char *hexascdump(unsigned char *msg, size_t len)
{
	size_t i;
	static unsigned char buf[256];
	unsigned char *bufp, *end;

	bufp = buf;
	end = bufp + sizeof(buf);
	buf[0] = 0;

	/* Dump each byte in hex: */
	for(i=0; i<len && end-bufp>=3; i++) {
		bufp += sprintf((char *)bufp, "%02x ", msg[i]);
	}

	/* Dump single-quoted string with printable version of each byte: */
	if (end-bufp > 0) *bufp++ = '\'';

	for(i=0; i<len && end-bufp>0; i++) {
		*bufp++ = toprint(msg[i]);
	}
	if (end-bufp > 0) *bufp++ = '\'';

	if (end-bufp > 0)
		*bufp = '\0';
	else
		*--end='\0';

	return (char *)buf;
}

enum tl_model_t decode_protocol(unsigned int proto)
{
	switch(proto) {
		case 0x0004:
			upslogx(3, "Using older SMART protocol (%x)", proto);
			return TRIPP_LITE_SMART_0004;
		case 0x1001:
			upslogx(3, "Using OMNIVS protocol (%x)", proto);
			return TRIPP_LITE_OMNIVS;
		case 0x2001:
			upslogx(3, "Using OMNIVS 2001 protocol (%x)", proto);
			return TRIPP_LITE_OMNIVS_2001;
		case 0x3003:
			upslogx(3, "Using SMARTPRO protocol (%x)", proto);
			return TRIPP_LITE_SMARTPRO;
		default:
			printf("Unknown protocol (%x)", proto);
			break;
	}

	return TRIPP_LITE_UNKNOWN;
}

void decode_v(const unsigned char *value)
{
	unsigned char ivn, lb;
	int bv = hex2d(value+2, 2);

 	ivn = value[1];
	lb = value[4];

	switch(ivn) {
		case '0': input_voltage_nominal = 
			  input_voltage_scaled  = 100;
			  break;

		case '1': input_voltage_nominal = 
			  input_voltage_scaled  = 120;
			  break;

		case '2': input_voltage_nominal = 
			  input_voltage_scaled  = 230;
			  break;

		case '3': input_voltage_nominal = 208;
			  input_voltage_scaled  = 230;
			  break;

		default:
			  upslogx(2, "Unknown input voltage range: 0x%02x", (unsigned int)ivn);
			  break;
	}

	battery_voltage_nominal = bv * 6;
		
	if( (lb >= '0') && (lb <= '9') ) {
		switchable_load_banks = lb - '0';
	} else {
		if( lb != 'X' ) {
			upslogx(2, "Unknown number of switchable load banks: 0x%02x",
					(unsigned int)lb);
		}
	}
}

void upsdrv_initinfo(void);

/*!@brief Report a USB comm failure, and reconnect if necessary
 * 
 * @param[in] res	Result code from libusb/libhid call
 * @param[in] msg	Error message to display
 */
void usb_comm_fail(int res, const char *msg)
{
	static int try = 0;

	switch(res) {
		case -EBUSY:
			upslogx(LOG_WARNING, "%s: Device claimed by another process", msg);
			fatalx(EXIT_FAILURE, "Terminating: EBUSY");
			break;

		default:
			upslogx(LOG_WARNING, "%s: Device detached? (error %d: %s)", msg, res, usb_strerror());

			upslogx(LOG_NOTICE, "Reconnect attempt #%d", ++try);
			hd = NULL;
			reconnect_ups();

			if(hd) {
				upslogx(LOG_NOTICE, "Successfully reconnected");
				try = 0;
				upsdrv_initinfo();
			} else {
				if(try > MAX_RECONNECT_TRIES) {
					fatalx(EXIT_FAILURE, "Too many unsuccessful reconnection attempts");
				}
			}
			break;
	}
}

/*!@brief Send a command to the UPS, and wait for a reply.
 *
 * All of the UPS commands are challenge-response. If a command does not have
 * anything to return, it simply returns the command character.
 *
 * @param[in] msg	Command string, minus the ':' or CR
 * @param[in] msg_len	Be sure to use sizeof(msg) instead of strlen(msg),
 * since some commands have embedded NUL characters
 * @param[out] reply	Reply (but check return code for validity)
 * @param[out] reply_len (currently unused)
 *
 * @return number of chars in reply, excluding terminating NUL
 * @return 0 if command was not accepted
 */
static int send_cmd(const unsigned char *msg, size_t msg_len, unsigned char *reply, size_t reply_len)
{
	unsigned char buffer_out[8];
	unsigned char csum = 0;
	int ret = 0, send_try, recv_try=0, done = 0;
	size_t i = 0;

	upsdebugx(3, "send_cmd(msg_len=%u, type='%c')", (unsigned)msg_len, msg[0]);

	if(msg_len > 5) {
		fatalx(EXIT_FAILURE, "send_cmd(): Trying to pass too many characters to UPS (%u)", (unsigned)msg_len);
	}

	buffer_out[0] = ':';
	for(i=1; i<8; i++) buffer_out[i] = '\0';

	for(i=0; i<msg_len; i++) {
		buffer_out[i+1] = msg[i];
		csum += msg[i];
	}

	buffer_out[i] = 255-csum;
	buffer_out[i+1] = ENDCHAR;

	upsdebugx(5, "send_cmd: sending  %s", hexascdump(buffer_out, sizeof(buffer_out)));

	for(send_try=0; !done && send_try < MAX_SEND_TRIES; send_try++) {
		upsdebugx(6, "send_cmd send_try %d", send_try+1);

		ret = comm_driver->set_report(udev, 0, buffer_out, sizeof(buffer_out));

		if(ret != sizeof(buffer_out)) {
			upslogx(1, "libusb_set_report() returned %d instead of %u",
				ret, (unsigned)(sizeof(buffer_out)));
			return ret;
		}

#if ! defined(__FreeBSD__)
		if(!done) { usleep(1000*100); /* TODO: nanosleep */ }
#endif

		for(recv_try=0; !done && recv_try < MAX_RECV_TRIES; recv_try++) {
			upsdebugx(7, "send_cmd recv_try %d", recv_try+1);
			ret = comm_driver->get_interrupt(udev, reply, sizeof(buffer_out), RECV_WAIT_MSEC);
			if(ret != sizeof(buffer_out)) {
				upslogx(1, "libusb_get_interrupt() returned %d instead of %u while sending %s",
					ret, (unsigned)(sizeof(buffer_out)),
					hexascdump(buffer_out, sizeof(buffer_out)));
			}
			done = (ret == sizeof(buffer_out)) && (buffer_out[1] == reply[0]);
		}
	}

	if(ret == sizeof(buffer_out)) {
		upsdebugx(5, "send_cmd: received %s (%s)", hexascdump(reply, sizeof(buffer_out)),
				done ? "OK" : "bad");
	}
	
	upsdebugx(((send_try > 2) || (recv_try > 2)) ? 3 : 6, 
			"send_cmd: send_try = %d, recv_try = %d\n", send_try, recv_try);

	return done ? sizeof(buffer_out) : 0;
}

/*!@brief Send an unknown command to the UPS, and store response in a variable
 *
 * @param msg Command string (usually a character and a null)
 * @param len Length of command plus null
 *
 * The variables are of the form "ups.debug.X" where "X" is the command
 * character.
 */
void debug_message(const char *msg, int len)
{
	int ret;
	unsigned char tmp_value[9];
	char var_name[20], err_msg[80];

	snprintf(var_name, sizeof(var_name), "ups.debug.%c", *msg);

	ret = send_cmd((const unsigned char *)msg, len, tmp_value, sizeof(tmp_value));
	if(ret <= 0) {
		sprintf(err_msg, "Error reading '%c' value", *msg);
		usb_comm_fail(ret, err_msg);
		return;
	}
	dstate_setinfo(var_name, "%s", hexascdump(tmp_value+1, 7));
}

#if 0
/* using the watchdog to reboot won't work while polling */
static void do_reboot_wait(unsigned dly)
{
	int ret;
	char buf[256], cmd_W[]="Wx"; 

	cmd_W[1] = dly;
	upsdebugx(3, "do_reboot_wait(wait=%d): N", dly);

	ret = send_cmd(cmd_W, sizeof(cmd_W), buf, sizeof(buf));
}

static int do_reboot_now(void)
{
	do_reboot_wait(1);
	return 0;
}

static void do_reboot(void)
{
	do_reboot_wait(bootdelay);
}
#endif

/*! Called by 'tripplite_usb -k' */
static int soft_shutdown(void)
{
	int ret;
	unsigned char buf[256], cmd_N[]="N\0x", cmd_G[] = "G";

	cmd_N[2] = offdelay;
	cmd_N[1] = offdelay >> 8;
	upsdebugx(3, "soft_shutdown(offdelay=%d): N", offdelay);

	ret = send_cmd(cmd_N, sizeof(cmd_N), buf, sizeof(buf));

	if(ret != 8) {
		upslogx(LOG_ERR, "Could not set offdelay to %d", offdelay);
		return ret;
	}

	sleep(2);
	
	/*! The unit must be on battery for this to work. 
	 *
	 * @todo check for on-battery condition, and print error if not.
	 * @todo Find an equivalent command for non-OMNIVS models.
	 */
	ret = send_cmd(cmd_G, sizeof(cmd_G), buf, sizeof(buf));

	if(ret != 8) {
		upslogx(LOG_ERR, "Could not turn off UPS (is it still on battery?)");
		return 0;
	}

	return 1;
}

#if 0
static int hard_shutdown(void)
{
	int ret;
	char buf[256], cmd_N[]="N\0x", cmd_K[] = "K\0";

	cmd_N[2] = offdelay;
	cmd_N[1] = offdelay >> 8;
	upsdebugx(3, "hard_shutdown(offdelay=%d): N", offdelay);

	ret = send_cmd(cmd_N, sizeof(cmd_N), buf, sizeof(buf));
	if(ret != 8) return ret;

	sleep(2);
	
	ret = send_cmd(cmd_K, sizeof(cmd_K), buf, sizeof(buf));
	return (ret == 8);
}
#endif

/*!@brief Turn an outlet on or off.
 *
 * @return 1 if the command worked, 0 if not.
 */
static int control_outlet(int outlet_id, int state)
{
	char k_cmd[10], buf[10];
	int ret;

	switch(tl_model) {
		case TRIPP_LITE_SMARTPRO:   /* tested */
		case TRIPP_LITE_SMART_0004: /* untested */
			snprintf(k_cmd, sizeof(k_cmd)-1, "N%02X", 5);
			ret = send_cmd((unsigned char *)k_cmd, strlen(k_cmd) + 1, (unsigned char *)buf, sizeof buf);
			snprintf(k_cmd, sizeof(k_cmd)-1, "K%d%d", outlet_id, state & 1);
			ret = send_cmd((unsigned char *)k_cmd, strlen(k_cmd) + 1, (unsigned char *)buf, sizeof buf);

			if(ret != 8) {
				upslogx(LOG_ERR, "Could not set outlet %d to state %d, ret = %d", outlet_id, state, ret);
				return 0;
			} else {
				return 1;
			}
		default:
			upslogx(LOG_ERR, "control_outlet unimplemented for this UPS model");
	}
	return 0;
}

/*!@brief Handler for "instant commands"
 */
static int instcmd(const char *cmdname, const char *extra)
{
	unsigned char buf[10];

	if(tl_model == TRIPP_LITE_SMARTPRO || tl_model == TRIPP_LITE_SMART_0004) {
		if (!strcasecmp(cmdname, "test.battery.start")) {
			send_cmd((const unsigned char *)"A", 2, buf, sizeof buf);
			return STAT_INSTCMD_HANDLED;
		}

		if(!strcasecmp(cmdname, "reset.input.minmax")) {
			return (send_cmd((const unsigned char *)"Z", 2, buf, sizeof buf) == 2) ? STAT_INSTCMD_HANDLED : STAT_INSTCMD_UNKNOWN;
		}
	}

	if (!strcasecmp(cmdname, "load.off")) {
		return control_outlet(0, 0) ? STAT_INSTCMD_HANDLED : STAT_INSTCMD_UNKNOWN;
	}
	if (!strcasecmp(cmdname, "load.on")) {
		return control_outlet(0, 1) ? STAT_INSTCMD_HANDLED : STAT_INSTCMD_UNKNOWN;
	}
	/* code for individual outlets is in setvar() */
#if 0
	if (!strcasecmp(cmdname, "shutdown.reboot")) {
		do_reboot_now();
		return STAT_INSTCMD_HANDLED;
	}
	if (!strcasecmp(cmdname, "shutdown.reboot.graceful")) {
		do_reboot();
		return STAT_INSTCMD_HANDLED;
	}
	if (!strcasecmp(cmdname, "shutdown.stayoff")) {
		hard_shutdown();
		return STAT_INSTCMD_HANDLED;
	}
#endif
	if (!strcasecmp(cmdname, "shutdown.return")) {
		soft_shutdown();
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}

static int setvar(const char *varname, const char *val)
{
	if (!strcasecmp(varname, "ups.delay.shutdown")) {
		offdelay = atoi(val);
		dstate_setinfo("ups.delay.shutdown", "%d", offdelay);
		return STAT_SET_HANDLED;
	}

	if (unit_id >= 0 && !strcasecmp(varname, "ups.id")) {
                int new_unit_id, ret;
		unsigned char J_msg[] = "J__", buf[9];

		new_unit_id = atoi(val);
		J_msg[1] = new_unit_id >> 8;
		J_msg[2] = new_unit_id & 0xff;
                ret = send_cmd(J_msg, sizeof(J_msg), buf, sizeof(buf));

		if(ret <= 0) {
			upslogx(LOG_NOTICE, "Could not set Unit ID (return code: %d).", ret);
			return STAT_SET_UNKNOWN;
		}

		dstate_setinfo("ups.id", "%s", val);
		return STAT_SET_HANDLED;
	}

	if(!strncmp(varname, "outlet.", strlen("outlet."))) {
		char outlet_name[80];
		char index_str[10], *first_dot, *next_dot;
		int index_chars, index, state, ret;

		first_dot = strstr(varname, ".");
		next_dot = strstr(first_dot + 1, ".");
		index_chars = next_dot - (first_dot + 1);

		if(index_chars > 9) return STAT_SET_UNKNOWN;
		if(strcmp(next_dot, ".switch")) return STAT_SET_UNKNOWN;

		strncpy(index_str, first_dot + 1, index_chars);
		index_str[index_chars] = 0;

		index = atoi(index_str);
		upslogx(LOG_DEBUG, "outlet.%d.switch = %s", index, val);

		if(!strcasecmp(val, "on") || !strcmp(val, "1")) {
			state = 1;
		} else {
			state = 0;
		}

		upslogx(LOG_DEBUG, "outlet.%d.switch = %s -> %d", index, val, state);

		snprintf(outlet_name, sizeof(outlet_name)-1, "outlet.%d.switch", index);

		ret = control_outlet(index, state);
		if(ret) {
			dstate_setinfo(outlet_name, "%d", state);
			return STAT_SET_HANDLED;
		} else {
			return STAT_SET_UNKNOWN;
		}
	}

#if 0
	if (!strcasecmp(varname, "ups.delay.start")) {
		startdelay = atoi(val);
		dstate_setinfo("ups.delay.start", val);
		return STAT_SET_HANDLED;
	}
	if (!strcasecmp(varname, "ups.delay.reboot")) {
		bootdelay = atoi(val);
		dstate_setinfo("ups.delay.reboot", val);
		return STAT_SET_HANDLED;
	}
#endif
	return STAT_SET_UNKNOWN;
}

void upsdrv_initinfo(void)
{
	const unsigned char proto_msg[] = "\0", f_msg[] = "F", p_msg[] = "P",
		s_msg[] = "S", u_msg[] = "U", v_msg[] = "V", w_msg[] = "W\0";
	char *model, *model_end;
	unsigned char proto_value[9], f_value[9], p_value[9], s_value[9],
	     u_value[9], v_value[9], w_value[9];
	int  va, ret;
	unsigned int proto_number = 0;

	/* Read protocol: */
	ret = send_cmd(proto_msg, sizeof(proto_msg), proto_value, sizeof(proto_value)-1);
	if(ret <= 0) {
		fatalx(EXIT_FAILURE, "Error reading protocol");
	}

	proto_number = ((unsigned)(proto_value[1]) << 8) 
			          | (unsigned)(proto_value[2]);
	tl_model = decode_protocol(proto_number);

	if(tl_model == TRIPP_LITE_UNKNOWN)
		dstate_setinfo("ups.debug.0", "%s", hexascdump(proto_value+1, 7));

	dstate_setinfo("ups.firmware.aux", "protocol %04x", proto_number);

	/* - * - * - * - * - * - * - * - * - * - * - * - * - * - * - */

	/* Reset watchdog: */
	/* Watchdog not supported on TRIPP_LITE_SMARTPRO models */
	if(tl_model != TRIPP_LITE_SMARTPRO ) {
		ret = send_cmd(w_msg, sizeof(w_msg), w_value, sizeof(w_value)-1);
		if(ret <= 0) {
			if(ret == -EPIPE) {
				fatalx(EXIT_FAILURE, "Could not reset watchdog. Please check and"
						"see if usbhid-ups(8) works with this UPS.");
			} else {
				upslogx(3, "Could not reset watchdog. Please send model "
						"information to nut-upsdev mailing list");
			}
		}
	}

	/* - * - * - * - * - * - * - * - * - * - * - * - * - * - * - */

	ret = send_cmd(s_msg, sizeof(s_msg), s_value, sizeof(s_value)-1);
	if(ret <= 0) {
		fatalx(EXIT_FAILURE, "Could not retrieve status ... is this an OMNIVS model?");
	}

	/* - * - * - * - * - * - * - * - * - * - * - * - * - * - * - */

	dstate_setinfo("ups.mfr", "%s", "Tripp Lite");

	/* Get nominal power: */
	ret = send_cmd(p_msg, sizeof(p_msg), p_value, sizeof(p_value)-1);
	va = strtol((char *)(p_value+1), NULL, 10);

	if(tl_model == TRIPP_LITE_SMART_0004) {
		dstate_setinfo("ups.debug.P","%s", hexascdump(p_value+1, 7));
		va *= 10; /* TODO: confirm? */
	}

	/* - * - * - * - * - * - * - * - * - * - * - * - * - * - * - */

	/* trim "TRIPP LITE" from beginning of model */
	model = strdup(hd->Product);
	if(strstr(model, hd->Vendor) == model) {
		model += strlen(hd->Vendor);
	}

	/* trim leading spaces: */
	for(; *model == ' '; model++);

	/* Trim trailing spaces */
	for(model_end = model + strlen(model) - 1;
			model_end > model && *model_end == ' ';
			model_end--) {
		*model_end = '\0';
	}

	dstate_setinfo("ups.model", "%s", model);

	dstate_setinfo("ups.power.nominal", "%d", va);

	/* - * - * - * - * - * - * - * - * - * - * - * - * - * - * - */

        /* Fetch firmware version: */
	ret = send_cmd(f_msg, sizeof(f_msg), f_value, sizeof(f_value)-1);

	toprint_str((char *)(f_value+1), 6);
	f_value[7] = 0;

	dstate_setinfo("ups.firmware", "F%s", f_value+1);

	/* - * - * - * - * - * - * - * - * - * - * - * - * - * - * - */

	/* Get configuration: */

	ret = send_cmd(v_msg, sizeof(v_msg), v_value, sizeof(v_value)-1);

	decode_v(v_value);

	/* FIXME: redundant, but since it's static, we don't need to poll
	 * every time.
	 */
	debug_message("V", 2);

	if(switchable_load_banks > 0) {
		int i;
		char outlet_name[80];

		for(i = 1; i <= switchable_load_banks + 1; i++) {
			snprintf(outlet_name, sizeof(outlet_name), "outlet.%d.id", i);
			dstate_setinfo(outlet_name, "%d", i);

			snprintf(outlet_name, sizeof(outlet_name), "outlet.%d.desc", i);
			dstate_setinfo(outlet_name, "Load %d", i);

			snprintf(outlet_name, sizeof(outlet_name), "outlet.%d.switchable", i);
			if( i <= switchable_load_banks ) {
				dstate_setinfo(outlet_name, "1");
				snprintf(outlet_name, sizeof(outlet_name)-1, "outlet.%d.switch", i);
				dstate_setinfo(outlet_name, "1");
				dstate_setflags(outlet_name, ST_FLAG_RW | ST_FLAG_STRING);
				dstate_setaux(outlet_name, 3);
			} else {
				/* Last bank is not switchable: */
				dstate_setinfo(outlet_name, "0");
			}

		}

		/* For the main relay: */
		dstate_addcmd("load.on");
		dstate_addcmd("load.off");
	}

	/* - * - * - * - * - * - * - * - * - * - * - * - * - * - * - */

	if(tl_model != TRIPP_LITE_OMNIVS && tl_model != TRIPP_LITE_SMART_0004) {
		/* Unit ID might not be supported by all models: */
		ret = send_cmd(u_msg, sizeof(u_msg), u_value, sizeof(u_value)-1);
		if(ret <= 0) {
			upslogx(LOG_INFO, "Unit ID not retrieved (not available on all models)");
		} else {
			unit_id = (int)((unsigned)(u_value[1]) << 8) 
				| (unsigned)(u_value[2]);
		}

		if(tl_model == TRIPP_LITE_SMART_0004) {
			debug_message("U", 2);
		}
	}

	if(unit_id >= 0) {
		dstate_setinfo("ups.id", "%d", unit_id);
		dstate_setflags("ups.id", ST_FLAG_RW | ST_FLAG_STRING);
		dstate_setaux("ups.id", 5);
		upslogx(LOG_DEBUG,"Unit ID: %d", unit_id);
	}

	/* - * - * - * - * - * - * - * - * - * - * - * - * - * - * - */

	dstate_setinfo("input.voltage.nominal", "%d", input_voltage_nominal);
	dstate_setinfo("battery.voltage.nominal", "%d", battery_voltage_nominal);
	dstate_setinfo("ups.debug.load_banks", "%d", switchable_load_banks);

	dstate_setinfo("ups.delay.shutdown", "%d", offdelay);
	dstate_setflags("ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING);
	dstate_setaux("ups.delay.shutdown", 3);

#if 0
	dstate_setinfo("ups.delay.start", "%d", startdelay);
	dstate_setflags("ups.delay.start", ST_FLAG_RW | ST_FLAG_STRING);
	dstate_setaux("ups.delay.start", 8);

	dstate_setinfo("ups.delay.reboot", "%d", bootdelay);
	dstate_setflags("ups.delay.reboot", ST_FLAG_RW | ST_FLAG_STRING);
	dstate_setaux("ups.delay.reboot", 3);
#endif

	if(tl_model == TRIPP_LITE_SMARTPRO || tl_model == TRIPP_LITE_SMART_0004) {
		dstate_addcmd("test.battery.start");
		dstate_addcmd("reset.input.minmax");
	}

	dstate_addcmd("shutdown.return");

#if 0
	dstate_addcmd("shutdown.stayoff");

	dstate_addcmd("test.battery.start"); /* Turns off automatically */

	dstate_addcmd("load.off");
	dstate_addcmd("load.on");

	dstate_addcmd("shutdown.reboot");
	dstate_addcmd("shutdown.reboot.graceful");
#endif

	upsh.instcmd = instcmd;
	upsh.setvar = setvar;

	printf("Attached to %s %s\n",
			dstate_getinfo("ups.mfr"), dstate_getinfo("ups.model"));
}

void upsdrv_shutdown(void)
{
	soft_shutdown();
}

void upsdrv_updateinfo(void)
{
	unsigned char b_msg[] = "B", d_msg[] = "D", l_msg[] = "L",
			s_msg[] = "S", m_msg[] = "M", t_msg[] = "T";
	unsigned char b_value[9], d_value[9], l_value[9], s_value[9],
			m_value[9], t_value[9];
	int bp, freq;
	double bv;

	int ret;
	unsigned battery_charge;

	status_init();

	/* - * - * - * - * - * - * - * - * - * - * - * - * - * - * - */

	/* General status (e.g. "S10") */
	ret = send_cmd(s_msg, sizeof(s_msg), s_value, sizeof(s_value));
	if(ret <= 0) {
		dstate_datastale();
		usb_comm_fail(ret, "Error reading S value");
		return;
	}

	if(tl_model != TRIPP_LITE_OMNIVS && tl_model != TRIPP_LITE_OMNIVS_2001) {
		dstate_setinfo("ups.debug.S","%s", hexascdump(s_value+1, 7));
	}

	/* - * - * - * - * - * - * - * - * - * - * - * - * - * - * - */

	if(tl_model == TRIPP_LITE_OMNIVS) {
		switch(s_value[2]) {
			case '0':
				status_set("OL");
				break;
			case '1':
				status_set("OB");
				break;
			case '2':
				/* "charge-only" mode, no AC in or out... the PC
				 * shouldn't see this, because there is no power in
				 * that case (OMNIVS), but it's here for testing.
				 *
				 * Entered by holding down the power button.
				 */
				status_set("BYPASS");
				break;
			case '3':
				/* I have seen this once when switching from off+LB to charging */
				upslogx(LOG_WARNING, "Unknown value for s[2]: 0x%02x", s_value[2]);
				break;
			default:
				upslogx(LOG_ERR, "Unknown value for s[2]: 0x%02x", s_value[2]);
				dstate_datastale();
				break;
		}
	}

	/* - * - * - * - * - * - * - * - * - * - * - * - * - * - * - */

	if(tl_model == TRIPP_LITE_SMARTPRO || tl_model == TRIPP_LITE_OMNIVS_2001 || tl_model == TRIPP_LITE_SMART_0004) {
		switch(s_value[2]) {
			case '0':
				dstate_setinfo("battery.test.status", "Battery OK");
				break;
			case '1':
				dstate_setinfo("battery.test.status", "Battery bad - replace");
				break;
			case '2':
				status_set("CAL");
				break;
			case '3':
				status_set("OVER");
				dstate_setinfo("battery.test.status", "Overcurrent?");
				break;
			case '4':
				/* The following message is confusing, and may not be accurate: */
				/* dstate_setinfo("battery.test.status", "Battery state unknown"); */
				break;
			case '5':
				status_set("OVER");
				dstate_setinfo("battery.test.status", "Battery fail - overcurrent?");
				break;
			default:
				upslogx(LOG_ERR, "Unknown value for s[2]: 0x%02x", s_value[2]);
				dstate_datastale();
				break;
		}


		if(s_value[4] & 4) {
			status_set("OFF");
		} else {
			/* Online/on battery: */
			if(s_value[4] & 1) {
				status_set("OB");
			} else {
				status_set("OL");
			}
		}

		/* This may not be right... */
		if(tl_model == TRIPP_LITE_SMARTPRO) {
			battery_charge = (unsigned)(s_value[5]);
			dstate_setinfo("battery.charge",  "%u", battery_charge);
		}
	}

	/* - * - * - * - * - * - * - * - * - * - * - * - * - * - * - */

	switch(s_value[1]) {
		case '0':
			status_set("LB");
			break;
		case '1': /* Depends on s_value[2] */
			break;
		case '2':
			if( tl_model == TRIPP_LITE_SMARTPRO ) {
				status_set("RB");
				break;
			} /* else fall through: */
		default:
			upslogx(LOG_ERR, "Unknown value for s[1]: 0x%02x", s_value[1]);
			dstate_datastale();
			break;
	}

	status_commit();

	/* - * - * - * - * - * - * - * - * - * - * - * - * - * - * - */

	if( tl_model == TRIPP_LITE_OMNIVS || tl_model == TRIPP_LITE_OMNIVS_2001 ) {
		ret = send_cmd(b_msg, sizeof(b_msg), b_value, sizeof(b_value));
		if(ret <= 0) {
			dstate_datastale();
			usb_comm_fail(ret, "Error reading B value");
			return;
		}

		dstate_setinfo("input.voltage", "%.2f", hex2d(b_value+1, 4)/30.0);

		bv = hex2d(b_value+5, 2)/16.0;

		/* dq ~= sqrt(dV) is a reasonable approximation
		 * Results fit well against the discrete function used in the Tripp Lite
		 * source, but give a continuous result. */
		if (bv >= V_interval[1])
			bp = 100;
		else if (bv <= V_interval[0])
			bp = 10;
		else
			bp = (int)(100*sqrt((bv - V_interval[0])
						/ (V_interval[1] - V_interval[0])));

		dstate_setinfo("battery.voltage", "%.2f", bv);
		dstate_setinfo("battery.charge",  "%3d", bp);
	}

	/* - * - * - * - * - * - * - * - * - * - * - * - * - * - * - */

	if( tl_model == TRIPP_LITE_SMARTPRO || tl_model == TRIPP_LITE_SMART_0004 ) {
		ret = send_cmd(d_msg, sizeof(d_msg), d_value, sizeof(d_value));
		if(ret <= 0) {
			dstate_datastale();
			usb_comm_fail(ret, "Error reading D value");
			return;
		}

		dstate_setinfo("input.voltage", "%d",
				hex2d(d_value+1, 2) * input_voltage_scaled / 120);

		bv = hex2d(d_value+3, 2) * battery_voltage_nominal / 120.0 ;

		dstate_setinfo("battery.voltage", "%.2f", bv);

		/* - * - * - * - * - * - * - * - * - * - * - * - * - * - * - */

		ret = send_cmd(m_msg, sizeof(m_msg), m_value, sizeof(m_value));

                if(m_value[5] != 0x0d) { /* we only expect 4 hex digits */
			dstate_setinfo("ups.debug.M", "%s", hexascdump(m_value+1, 7));
		}

		if(ret <= 0) {
			dstate_datastale();
			usb_comm_fail(ret, "Error reading M (min/max input voltage)");
			return;
		}

		dstate_setinfo("input.voltage.minimum", "%3d", hex2d(m_value+1, 2));
		dstate_setinfo("input.voltage.maximum", "%3d", hex2d(m_value+3, 2));

		/* - * - * - * - * - * - * - * - * - * - * - * - * - * - * - */

		ret = send_cmd(t_msg, sizeof(t_msg), t_value, sizeof(t_value));
		dstate_setinfo("ups.debug.T", "%s", hexascdump(t_value+1, 7));
		if(ret <= 0) {
			dstate_datastale();
			usb_comm_fail(ret, "Error reading T value");
			return;
		}

		if( tl_model == TRIPP_LITE_SMARTPRO ) {
			freq = hex2d(t_value + 3, 3);
			dstate_setinfo("input.frequency", "%.1f", freq / 10.0);

			switch(t_value[6]) {
				case '1':
					dstate_setinfo("input.frequency.nominal", "%d", 60);
					break;
				case '0':
					dstate_setinfo("input.frequency.nominal", "%d", 50);
					break;
			}
                }

		if( tl_model == TRIPP_LITE_SMART_0004 ) {
			freq = hex2d(t_value + 3, 4);
			dstate_setinfo("input.frequency", "%.1f", freq / 10.0);
		}

		/* I'm guessing this is a calibration constant of some sort. */
		dstate_setinfo("ups.temperature", "%.1f", (unsigned)(hex2d(t_value+1, 2)) * 0.3636 - 21);
	}

	/* - * - * - * - * - * - * - * - * - * - * - * - * - * - * - */

	ret = send_cmd(l_msg, sizeof(l_msg), l_value, sizeof(l_value));
	if(ret <= 0) {
		dstate_datastale();
		usb_comm_fail(ret, "Error reading L value");
		return;
	}

	switch(tl_model) {
		case TRIPP_LITE_OMNIVS:
		case TRIPP_LITE_OMNIVS_2001:
			dstate_setinfo("output.voltage", "%.1f", hex2d(l_value+1, 4)/2.0);
			break;
		case TRIPP_LITE_SMARTPRO:
			dstate_setinfo("ups.load", "%d", hex2d(l_value+1, 2));
			break;
		case TRIPP_LITE_SMART_0004:
			dstate_setinfo("ups.load", "%d", hex2d(l_value+1, 2));
			dstate_setinfo("ups.debug.L","%s", hexascdump(l_value+1, 7));
			break;
		default:
			dstate_setinfo("ups.debug.L","%s", hexascdump(l_value+1, 7));
			break;
	}

	/* - * - * - * - * - * - * - * - * - * - * - * - * - * - * - */

	if(tl_model != TRIPP_LITE_OMNIVS && tl_model != TRIPP_LITE_OMNIVS_2001) {
		debug_message("D", 2);

		/* We already grabbed these above: */
		if(tl_model != TRIPP_LITE_SMARTPRO) {
			debug_message("V", 2); /* Probably not necessary - seems to be static */
			debug_message("M", 2);
			debug_message("T", 2);
			debug_message("P", 2);
		}
		/* debug_message("U", 2); */
	}

	dstate_dataok();
}

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
	char msg[256];

	snprintf(msg, sizeof msg, "Set shutdown delay, in seconds (default=%d).",
		DEFAULT_OFFDELAY);
	addvar(VAR_VALUE, "offdelay", msg);

        /* allow -x vendor=X, vendorid=X, product=X, productid=X, serial=X */
	addvar(VAR_VALUE, "vendor", "Regular expression to match UPS Manufacturer string");
	addvar(VAR_VALUE, "product", "Regular expression to match UPS Product string");
	addvar(VAR_VALUE, "serial", "Regular expression to match UPS Serial number");
	addvar(VAR_VALUE, "productid", "Regular expression to match UPS Product numerical ID (4 digits hexadecimal)");
	addvar(VAR_VALUE, "bus", "Regular expression to match USB bus name");

#if 0
	snprintf(msg, sizeof msg, "Set start delay, in seconds (default=%d).",
		DEFAULT_STARTDELAY);
	addvar(VAR_VALUE, "startdelay", msg);
	snprintf(msg, sizeof msg, "Set reboot delay, in seconds (default=%d).",
		DEFAULT_BOOTDELAY);
	addvar(VAR_VALUE, "rebootdelay", msg);
#endif
}

/*!@brief Initialize UPS and variables from ups.conf
 *
 * @todo Allow binding based on firmware version (which seems to vary wildly
 * from unit to unit)
 */
void upsdrv_initups(void)
{
	char *regex_array[6];
	int r;

	/* process the UPS selection options */
	regex_array[0] = NULL; /* handled by USB IDs device table */
	regex_array[1] = getval("productid");
	regex_array[2] = getval("vendor"); /* vendor string */
	regex_array[3] = getval("product"); /* product string */
	regex_array[4] = getval("serial"); /* probably won't see this */
	regex_array[5] = getval("bus");

	r = USBNewRegexMatcher(&regex_matcher, regex_array, REG_ICASE | REG_EXTENDED);
	if (r==-1) {
		fatal_with_errno(EXIT_FAILURE, "USBNewRegexMatcher");
	} else if (r) {
		fatalx(EXIT_FAILURE, "invalid regular expression: %s", regex_array[r]);
	}

	/* link the matchers */
	regex_matcher->next = &subdriver_matcher;

	/* Search for the first supported UPS matching the regular
	 *            expression */
	r = comm_driver->open(&udev, &curDevice, regex_matcher, NULL, 1);
	if (r < 1) {
		fatalx(EXIT_FAILURE, "No matching USB/HID UPS found");
	}

	hd = &curDevice;
	
	upslogx(1, "Detected a UPS: %s/%s", hd->Vendor ? hd->Vendor : "unknown", hd->Product ? hd->Product : "unknown");

	dstate_setinfo("ups.vendorid", "%04x", hd->VendorID);
	dstate_setinfo("ups.productid", "%04x", hd->ProductID);

	/* create a new matcher for later reopening */
	r = USBNewExactMatcher(&reopen_matcher, hd);
	if (r) {
		fatal_with_errno(EXIT_FAILURE, "USBNewExactMatcher");
	}
	/* link the two matchers */
	reopen_matcher->next = regex_matcher;

	if (getval("offdelay"))
		offdelay = atoi(getval("offdelay"));
#if 0
	if (getval("startdelay"))
		startdelay = atoi(getval("startdelay"));
	if (getval("rebootdelay"))
		bootdelay = atoi(getval("rebootdelay"));
#endif
}

void upsdrv_cleanup(void)
{
	comm_driver->close(udev);
	USBFreeExactMatcher(reopen_matcher);
	USBFreeRegexMatcher(regex_matcher);
}
