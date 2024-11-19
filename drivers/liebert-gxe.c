/* liebert-gxe.c - support for Liebert GXE Series UPS models via serial.

   Copyright (C) 2024  Gong Zhile <goodspeed@mailo.cat>

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

#include "config.h"
#include "main.h"
#include "attribute.h"
#include "serial.h"
#include "ydn23.h"

#define DRIVER_NAME	"Liebert GXE Series UPS driver"
#define DRIVER_VERSION	"0.02"

#define PROBE_RETRIES	3
#define DEFAULT_STALE_RETRIES	3

#define DATAFLAG_WARN_MASK	(1)
#define DATAFLAG_ONOFF_MASK	(1 << 4)

/* Populated in upsdrv_initups() to default or user setting */
static char	*devaddr = NULL;

static int	stale_retries = DEFAULT_STALE_RETRIES;
static int	stale_retry = DEFAULT_STALE_RETRIES;

#define TRY_STALE()							\
	if ((stale_retry--) > 0)					\
		dstate_dataok();					\
	else								\
		dstate_datastale();					\

#define ARRAY_SIZE(x) ((sizeof x) / (sizeof *x))
static const char *gxe_warns[] = {
	NULL,			/* DATAFLAG */
	"Inverter Out-of-Sync",
	"Unhealthy Main Circuit",
	"Rectifier Failure",
	"Inverter Failure",
	"Unhealthy Bypass",
	"Unhealthy Battery Voltage",
	NULL,			/* USER_DEFINED */
	NULL,			/* USER_DEFINED */
	"Power Module Overheated",
	"Unhealthy Fan",
	"Netural Input Missing",
	"Master Line Abnormally Turned-off",
	"Charger Failure",
	"Battery Discharge Declined",
	"Backup Power Supply Failure",
	"Ouput Overloaded",
	"Ouput Shorted",
	"Overload Timed-out",
	"Unhealthy Parallel Machine Current",
	"Parallel Machine Connection Failure",
	"Parallel Machine Address Error",
	"Unhealthy Internal Communication",
	"System Overloaded",
	"Battery Installed Backwards",
	"Battery Not Found",
};

/* Instcmd & Driver Init: SYSPARAM -OK-> WARNING -OK-> ONOFF -OK-> ANALOG */
/* If the dataflag set for WARNING/ONOFF, set next state respectively. */
static enum gxe_poll_state {
	GXE_ONOFF,
	GXE_ANALOG,
	GXE_WARNING,
	/* Scheduled System Parameters, Trigged by Instcmd */
	GXE_SYSPARAM,
} poll_state;

upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Gong Zhile <goodspeed@mailo.cat>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

static int instcmd(const char *cmdname, const char *extra)
{
	enum YDN23_COMMAND_ID	cmd = YDN23_REMOTE_COMMAND;
	struct ydn23_frame	sendframe, recvframe;
	int	retry, ret, len = 4;
	char	*data = NULL;

	if (!strcasecmp(cmdname, "test.battery.start"))
		data = "1002";
	else if (!strcasecmp(cmdname, "test.battery.stop"))
		data = "1003";
	else if (!strcasecmp(cmdname, "load.on"))
		data = "2001";
	else if (!strcasecmp(cmdname, "load.off"))
		data = "2003";
	else {
		upslogx(LOG_NOTICE, "instcmd: unknown command [%s] [%s]", cmdname, extra);
		return STAT_INSTCMD_UNKNOWN;
	}

	ydn23_frame_init(&sendframe, cmd, devaddr, "21", data, len);
	for (retry = 0; retry < PROBE_RETRIES; retry++) {
		ret = ydn23_frame_send(upsfd, &sendframe);
		if (ret <= 0)
			continue;

		ret = ydn23_frame_read(upsfd, &recvframe, 2, 0);
		if (ret > 0) {
			poll_state = GXE_SYSPARAM;
			return STAT_INSTCMD_HANDLED;
		}
	}

	upslogx(LOG_WARNING, "instcmd: remote failed response, try again");
	return STAT_INSTCMD_FAILED;
}

static void upsdrv_updateinfo_onoff(void)
{
	int	retry, ret = -1, dflag, pwrval, rectval;
	struct ydn23_frame	sendframe, frame;

	ydn23_frame_init(&sendframe, YDN23_GET_ONOFF_DATA,
		"21", devaddr, NULL, 0);

	for (retry = 0; retry < PROBE_RETRIES; retry++) {
		ret = ydn23_frame_send(upsfd, &sendframe);
		if (ret <= 0)
			continue;
		ret = ydn23_frame_read(upsfd, &frame, 2, 0);
		if (ret > 0)
			break;
	}

	if (ret <= 0) {
		upslogx(LOG_WARNING, "failed reading ONOFF data, retry");
		TRY_STALE()
		return;
	}

	stale_retry = stale_retries;
	poll_state = GXE_ANALOG;

	/* DATAFLAG */
	dflag = ydn23_val_from_hex(YDN23_FRAME_REG(frame, 0), 2);
	if (dflag & DATAFLAG_ONOFF_MASK)
		poll_state = GXE_ONOFF;
	if (dflag & DATAFLAG_WARN_MASK)
		poll_state = GXE_WARNING;

	status_init();

	/* Field 1, Power Supply (01=UPS, 02=Bypass) */
	pwrval = ydn23_val_from_hex(YDN23_FRAME_REG(frame, 1), 2);
	/* Field 3, Rectifier Power Supply (E0=None, E1=CITYPWR, E2=BAT) */
	rectval = ydn23_val_from_hex(YDN23_FRAME_REG(frame, 3), 2);

	if (pwrval == 0x01 && rectval == 0xe2)
		status_set("OB");
	else if (pwrval == 0x01)
		status_set("OL");
	else if (pwrval == 0x02)
		status_set("OL BYPASS");
	else
		upslogx(LOG_WARNING, "unknown ups state: %x %x", pwrval, rectval);

	status_commit();

	/* Field 4, Battery Status */
	switch(ydn23_val_from_hex(YDN23_FRAME_REG(frame, 4), 2)) {
	case 0xe0:
		dstate_setinfo("battery.charger.status", "resting");
		break;
	case 0xe1:
	case 0xe2:
		dstate_setinfo("battery.charger.status", "charging");
		break;
	case 0xe3:
		dstate_setinfo("battery.charger.status", "discharging");
		break;
	default:
		upslogx(LOG_WARNING, "unknown battery status, ignored");
		break;
	}

	/* Field 5, Battery Test State */
	switch(ydn23_val_from_hex(YDN23_FRAME_REG(frame, 5), 2)) {
	case 0xe0:
		dstate_setinfo("ups.test.result", "In progress");
		break;
	case 0xe1:
		dstate_setinfo("ups.test.result", "Idle");
		break;
	default:
		upslogx(LOG_WARNING, "unknown battery test state, ignored");
		break;
	}

	dstate_dataok();
}

static void upsdrv_updateinfo_analog(void)
{
	struct ydn23_frame	sendframe, frame;
	int	retry, ret = -1, dflag, volt;

	ydn23_frame_init(&sendframe, YDN23_GET_ANALOG_DATA_D,
		"21", devaddr, NULL, 0);

	for (retry = 0; retry < PROBE_RETRIES; retry++) {
		ret = ydn23_frame_send(upsfd, &sendframe);
		if (ret <= 0)
			continue;
		ret = ydn23_frame_read(upsfd, &frame, 2, 0);
		if (ret > 0)
			break;
	}

	if (ret <= 0) {
		upslogx(LOG_WARNING, "failed reading ANALOG data, retry");
		TRY_STALE()
		return;
	}

	stale_retry = stale_retries;

	/* DATAFLAG, NOT RELIABLE SOMEHOW */
	dflag = ydn23_val_from_hex(frame.INFO, 2);
	if (dflag & DATAFLAG_ONOFF_MASK)
		poll_state = GXE_ONOFF;
	if (dflag & DATAFLAG_WARN_MASK)
		poll_state = GXE_WARNING;

	/* Field 1, AC_IN VOLTAGE */
	volt = ydn23_val_from_hex(YDN23_FRAME_REG(frame, 1), 4)/100;

	if (volt == 0 && status_get("OL")) {
		/* Oh no, power failed still online? */
		status_init();
		status_set("OB");
		status_commit();
		poll_state = GXE_WARNING;
	}

	if (volt > 0 && status_get("OB")) {
		/* Hum, power recovered still on battery? */
		status_init();
		status_set("OL");
		status_commit();
		poll_state = GXE_WARNING;
	}

	dstate_setinfo("input.voltage", "%.02f",
		ydn23_val_from_hex(YDN23_FRAME_REG(frame, 1), 4)/100.0f);
	/* Field 4, AC_OUT VOLTAGE */
	dstate_setinfo("output.voltage", "%.02f",
		ydn23_val_from_hex(YDN23_FRAME_REG(frame, 7), 4)/100.0f);
	/* Field 7, AC_OUT CURRENT */
	dstate_setinfo("output.current", "%.02f",
		ydn23_val_from_hex(YDN23_FRAME_REG(frame, 13), 4)/100.0f);
	/* Field 10, DC VOLTAGE */
	dstate_setinfo("battery.voltage", "%.02f",
		ydn23_val_from_hex(YDN23_FRAME_REG(frame, 19), 4)/100.0f);
	/* Field 11, AC_OUT FREQUENCY */
	dstate_setinfo("output.frequency", "%.02f",
		ydn23_val_from_hex(YDN23_FRAME_REG(frame, 21), 4)/100.0f);
	/* Field 15, AC_IN FREQUENCY */
	dstate_setinfo("input.frequency", "%.02f",
		ydn23_val_from_hex(YDN23_FRAME_REG(frame, 27), 4)/100.0f);
	/* Field 18, AC_OUT REALPOWER, kW */
	dstate_setinfo("ups.realpower", "%d",
		ydn23_val_from_hex(YDN23_FRAME_REG(frame, 33), 4)*10);
	/* Field 19, AC_OUT POWER, kVA */
	dstate_setinfo("ups.power", "%d",
		ydn23_val_from_hex(YDN23_FRAME_REG(frame, 35), 4)*10);
	/* Field 22, BATTERY BACKUP TIME, Min */
	dstate_setinfo("battery.runtime.low", "%.2f",
		ydn23_val_from_hex(YDN23_FRAME_REG(frame, 41), 4)/100.0f*60.0f);

	dstate_dataok();
}

static void upsdrv_updateinfo_sysparam(void)
{
	struct ydn23_frame	sendframe, frame;
	int	retry, ret = -1;

	ydn23_frame_init(&sendframe, YDN23_GET_SYS_PARAM_D,
		"21", devaddr, NULL, 0);

	for (retry = 0; retry < PROBE_RETRIES; retry++) {
		ret = ydn23_frame_send(upsfd, &sendframe);
		if (ret <= 0)
			continue;
		ret = ydn23_frame_read(upsfd, &frame, 2, 0);
		if (ret > 0)
			break;
	}

	if (ret <= 0) {
		upslogx(LOG_WARNING, "failed reading SYSPARAM data, retry");
		TRY_STALE()
		return;
	}

	stale_retry = stale_retries;
	poll_state = GXE_WARNING;

	/* Field 6, Nominal Voltage */
	dstate_setinfo("output.voltage.nominal", "%d",
		ydn23_val_from_hex(YDN23_FRAME_REG(frame, 9), 4));
	/* Field 7, Nominal Frequency */
	dstate_setinfo("output.frequency.nominal", "%d",
		ydn23_val_from_hex(YDN23_FRAME_REG(frame, 11), 4));
	/* Field 10, Bypass Working Voltage Max, ALWAYS 115% */
	if (ydn23_val_from_hex(YDN23_FRAME_REG(frame, 17), 4) == 1)
		dstate_setinfo("input.transfer.bypass.high", "%f",
			ydn23_val_from_hex(YDN23_FRAME_REG(frame, 9), 4)*1.15f);
	/* Field 11, Bypass Working Voltage Min, Volt */
	if (ydn23_val_from_hex(YDN23_FRAME_REG(frame, 19), 4) == 1)
		dstate_setinfo("input.transfer.bypass.low", "%d", 120);
	/* Field 21, Battery Test Interval, per 3 mons */
	dstate_setinfo("ups.test.interval", "%lu",
		(long) ydn23_val_from_hex(YDN23_FRAME_REG(frame, 39), 4)*3*108000);

	dstate_dataok();
}

static void upsdrv_updateinfo_warning(void)
{
	struct ydn23_frame	sendframe, frame;
	int	retry, ret = -1, val;
	size_t	i;

	ydn23_frame_init(&sendframe, YDN23_GET_WARNING_DATA,
		"21", devaddr, NULL, 0);

	for (retry = 0; retry < PROBE_RETRIES; retry++) {
		ret = ydn23_frame_send(upsfd, &sendframe);
		if (ret <= 0)
			continue;
		ret = ydn23_frame_read(upsfd, &frame, 2, 0);
		if (ret > 0)
			break;
	}

	if (ret <= 0) {
		upslogx(LOG_WARNING, "failed reading WARNING data, retry");
		TRY_STALE()
		return;
	}

	stale_retry = stale_retries;
	poll_state = GXE_ONOFF;

	alarm_init();
	for (i = 0; i < ARRAY_SIZE(gxe_warns); i++) {
		if (!gxe_warns[i])
			continue;
		val = ydn23_val_from_hex(YDN23_FRAME_REG(frame, 1+i), 2);
		switch(val)  {
		case 0x00:
		case 0x0e:
		case 0xe0:
			break;
		case 0x01:
		case 0x02:
		case 0x03:
		case 0xf0:
			alarm_set(gxe_warns[i]);
			break;
		default:
			upslogx(LOG_WARNING, "unexpected warning val %x", val);
			break;
		}
	}
	alarm_commit();

	dstate_dataok();
}

void upsdrv_updateinfo(void)
{
	switch(poll_state) {
	case GXE_ANALOG:
		upslogx(LOG_DEBUG, "Polling ANALOG data");
		upsdrv_updateinfo_analog();
		break;
	case GXE_ONOFF:
		upslogx(LOG_DEBUG, "Polling ONOFF data");
		upsdrv_updateinfo_onoff();
		break;
	case GXE_WARNING:
		upslogx(LOG_DEBUG, "Polling WARNING data");
		upsdrv_updateinfo_warning();
		break;
	case GXE_SYSPARAM:
		upslogx(LOG_DEBUG, "Polling SYSPARAM data");
		upsdrv_updateinfo_sysparam();
		break;
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT
# pragma GCC diagnostic ignored "-Wcovered-switch-default"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
# pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
/* Older CLANG (e.g. clang-3.4) seems to not support the GCC pragmas above */
#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
# pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
	default:
		/* Must not occur. */
		upslogx(LOG_WARNING,
			"Unknown State Reached, "
			"Fallback to ANALOG data");
		poll_state = GXE_ANALOG;
		upsdrv_updateinfo();
		break;
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
	}
}

void upsdrv_initinfo(void)
{
	struct ydn23_frame	sendframe, recvframe;
	char	databuf[11];
	int	retry, ret = -1;

	ydn23_frame_init(&sendframe, YDN23_GET_VENDOR_INFO,
		"21", devaddr, NULL, 0);

	for (retry = 0; retry < PROBE_RETRIES; retry++) {
		ret = ydn23_frame_send(upsfd, &sendframe);
		if (ret <= 0)
			continue;
		ret = ydn23_frame_read(upsfd, &recvframe, 2, 0);
		if (ret > 0)
			break;
	}

	if (ret <= 0)
		fatal_with_errno(EXIT_FAILURE, "liebert-gxe: failed reading response");

	/* UPS Name, 10 bytes */
	ydn23_substr_from_hex(databuf, 11, YDN23_FRAME_REG(recvframe, 0), 20);
	dstate_setinfo("ups.mfr", "EmersonNetworkPower");
	dstate_setinfo("ups.model", "%s", databuf);

	dstate_setinfo("ups.id", "%s", devaddr);

	dstate_addcmd("test.battery.start");
	dstate_addcmd("test.battery.stop");
	dstate_addcmd("load.off");
	dstate_addcmd("load.on");

	upsh.instcmd = instcmd;

	poll_state = GXE_SYSPARAM;
}

void upsdrv_help(void)
{
}

void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "addr", "Override default UPS address");
	addvar(VAR_VALUE, "retry", "Override default retry");
}

void upsdrv_initups(void)
{
	upsfd = ser_open(device_path);

	devaddr = "01";		/* Default Address is 0x01 */
	if (testvar("addr"))
		devaddr = getval("addr");

	if (testvar("retry"))
		stale_retries = atoi(getval("retry"));

	stale_retry = stale_retries;
	poll_interval = 5;

	usleep(100000);
}

void upsdrv_shutdown(void)
{
	/* Only implement "shutdown.default"; do not invoke
	 * general handling of other `sdcommands` here */

	/* FIXME: There seems to be instcmd(load.off), why not that? */
	upslogx(LOG_INFO, "Liebert GXE UPS can't fully shutdown, NOOP");
	if (handling_upsdrv_shutdown > 0)
		set_exit_flag(EF_EXIT_FAILURE);
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
