/* liebert-esp2.c - driver for Liebert UPS, using the ESP-II protocol
 *
 *  Copyright (C)
 *  2009	Richard Gregory <r.gregory liverpool ac uk>
 *  2017	Nash Kaminski <nashkaminski at kaminski dot io>
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
 */

#include "main.h"
#include "serial.h"
#include "timehead.h"
#include "nut_stdint.h"

#define sivann
#define IsBitSet(val, bit) ((val) & (1 << (bit)))

#define DRIVER_NAME	"Liebert ESP-II serial UPS driver"
#define DRIVER_VERSION	"0.04"
#define UPS_SHUTDOWN_DELAY 12 /* it means UPS will be shutdown 120 sec */
#define SHUTDOWN_CMD_LEN  8

/* values for sending to UPS */
enum mult_enum {
	M_10,
	M_0_1,
	M_VOLTAGE_I,
	M_VOLTAGE_O,
	M_VOLTAGE_B,
	M_CURRENT_I,
	M_CURRENT_O,
	M_CURRENT_B,
	M_LOAD_VA,
	M_LOAD_WATT,
	M_FREQUENCY,
	M_VOLT_DC,
	M_TEMPERATURE,
	M_CURRENT_DC ,
	M_BAT_RUNTIME,
	M_NOMPOWER,
	M_POWER,
	M_REALPOWER,
	M_LOADPERC
};

static float multi[19]={
	10.0,
	0.1,
	0.1, /* volt */
	0.1,
	0.1,
	0.1, /* curr */
	0.1,
	0.1,
	100.0, /* va */
	100.0, /* W */
	0.01,  /* FREQ */
	0.1,   /* V DC*/
	0.1,   /* TEMP*/
	0.01,  /* CUR DC*/
	60.0,  /* BAT RUNTIME*/
	100.0, /* NOMPOWER*/
	100.0, /* POWER*/
	100.0, /* REAL POWER*/
	1.0    /* LOADPERC*/
};

static int instcmd(const char *cmdname, const char *extra);
static int setvar(const char *varname, const char *val);

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Richard Gregory <r.gregory liv ac uk>\n" \
	"Robert Jobbagy <jobbagy.robert at gmail dot com\n" \
	"Nash Kaminski <nashkaminski at kaminski dot io>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

static const unsigned char
	/* Bit field information provided by Spiros Ioannou */
	/* Ordered on MSB to LSB. Shown as DESCRIPTION (bit number), starting at 0. */
	cmd_bitfield1[]		= { 1,148,2,1,1,153 },	/* ON_BATTERY(8), INPUT_OVERVOLTAGE(7), BATTERY_TEST_STATE(6), OVERTEMP_WARNING(5), INRUSH_LIMIT_ON(4), UTILITY_STATE(3), ON_INVERTER(2), DC_DC_CONVERTER_STATE(1), PFC_ON(0) */
	cmd_bitfield2[]		= { 1,148,2,1,2,154 },	/* BUCK_ON (9), DIAG_LINK_SET(7), BOOST_ON(6), REPLACE_BATTERY(5), BATTERY_LIFE_ENHANCER_ON(4), BATTERY_CHARGED (1), ON_BYPASS (0) */
	cmd_bitfield3[]		= { 1,148,2,1,3,155 },	/* CHECK_AIR_FILTER (10), BAD_BYPASS_PWR (8), OUTPUT_OVERVOLTAGE (7), OUTPUT_UNDERVOLTAGE (6), LOW_BATTERY (5), CHARGER_FAIL (3), SHUTDOWN_PENDING (2), BAD_INPUT_FREQ (1), UPS_OVERLOAD (0) */
	cmd_bitfield7[]		= { 1,148,2,1,7,159 },	/* AMBIENT_OVERTEMP (2) */
	cmd_battestres[]	= { 1,148,2,1,12,164 },	/* BATTERY_TEST_RESULT */
	cmd_selftestres[]	= { 1,148,2,1,13,165 },	/* SELF_TEST_RESULT */
	cmd_upstype[]  		= { 1,136,2,1,1,141}, 	/* type bits + number of phases in bit groups*/
	cmd_scaling1[] 		= { 1,131,2,1,2,137}, 	/* part of multiplier information*/

	/* Shutdown commands by Robert Jobbagy */
	cmd_setOutOffMode[]	= { 1,156,4,1,6,0,1,169}, /* UPS OutOffMode command */
	cmd_setOutOffDelay[] = {1,156,4,1,5,0,UPS_SHUTDOWN_DELAY,167+UPS_SHUTDOWN_DELAY}, /* UPS Shutdown with delay */
	cmd_sysLoadKey[]	= {1,156,2,1,7,167}, /* UPS SysLoadKey */
	cmd_shutdown[]  	= {1,156,4,1,136,76,76,194}; /* UPS shutdown */

/* Quiesce the compiler warnings about the fields below */
static void NUT_UNUSED_FUNCTION_dummy_bitfields(void)
{
	NUT_UNUSED_VARIABLE(cmd_battestres);
	NUT_UNUSED_VARIABLE(cmd_selftestres);
	NUT_UNUSED_VARIABLE(cmd_bitfield7);
}

static int num_inphases = 1, num_outphases = 1;

static char cksum(const char *buf, const size_t len)
{
	char	sum = 0;
	size_t	i;

	for (i = 0; i < len; i++) {
		sum += buf[i];
	}

	return sum;
}

static int do_command(const unsigned char *command, char *reply, int cmd_len)
{
	int	ret;

	ret = ser_send_buf(upsfd, command, cmd_len);
	if (ret < 0) {
		upsdebug_with_errno(2, "send");
		return -1;
	} else if (ret < cmd_len) {
		upsdebug_hex(2, "send: truncated", command, (size_t)ret);
		return -1;
	}

	upsdebug_hex(2, "send", command, (size_t)ret);

	ret = ser_get_buf_len(upsfd, reply, 8, 1, 0); /* it needs that this driver works with USB to Serial cable */
	if (ret < 0) {
		upsdebug_with_errno(2, "read");
		return -1;
	} else if (ret < 6) {
		upsdebug_hex(2, "read: truncated", reply, (size_t)ret);
		return -1;
	} else if (reply[7] != cksum(reply, 7)) {
		upsdebug_hex(2, "read: checksum error", reply, (size_t)ret);
		return -1;
	}

	upsdebug_hex(2, "read", reply, (size_t)ret);
	return ret;
}

void upsdrv_initinfo(void)
{
	struct {
		const char	*var;
		unsigned char	len;
	} vartab[] = {
		{ "ups.model", 15 },
		{ "ups.firmware", 8 },
		{ "ups.serial", 10 },
		{ "ups.mfr.date", 4 },
		{ NULL, 0 }
	};

	char	buf[LARGEBUF];
	int	i, bitn, vari, ret=0, offset=4, readok=0;
	char	command[6], reply[8];
	unsigned int	value;

	dstate_setinfo("ups.mfr", "%s", "Liebert");

	for (vari = 0; vartab[vari].var; vari++) {
		upsdebugx(1, "reading: %s", vartab[vari].var);

		for (i = 0; i < vartab[vari].len; i++) {
			snprintf(command, sizeof(command), "\x01\x88\x02\x01%c", i+offset);
			command[5] = cksum(command, 5);

			ret = do_command((unsigned char *)command, reply, 6);
			if (ret < 8) {
				upsdebug_hex(2, "send: truncated", command, (size_t)ret);
				break;
			}

			buf[i<<1] = reply[6];
			buf[(i<<1)+1] = reply[5];
		}

		buf[i<<1] = 0;
		upsdebugx(1, "return: %d (8=success)", ret);

		if (ret == 8) { /* last command successful */
			dstate_setinfo(vartab[vari].var,"%s",buf);
			readok++;
		}
		offset+=vartab[vari].len;
	} /* for */
	if (!readok) {
		fatalx(EXIT_FAILURE, "ESP-II capable UPS not detected");
	}

	/* determine number of input & output phases and ups type */
	memcpy(command,cmd_upstype,6);
	ret = do_command((unsigned char *)command, reply, 6);
	if (ret < 8) {
		upsdebug_hex(2, "send: phase detection: truncated", command, (size_t)ret);
	}
	else {
		/* input: from bit 0 to bit 1 (2 bits) */
		for (value=0,bitn=0;bitn<2;bitn++) {
			if (IsBitSet(reply[6],(unsigned short int)bitn)) /* bit range measurement on LSByte*/
				value+=(1<<(unsigned short int)(bitn - 0));
		}
		num_inphases=value;
		dstate_setinfo("input.phases", "%d", value);

		/* output: from bit 4 to bit 5  (2 bits)*/
		for (value=0,bitn=4;bitn<6;bitn++) {
			if (IsBitSet(reply[6],(unsigned short int)bitn)) /* bit range measurement on LSByte*/
				value+=(1<<(unsigned short int)(bitn - 4));
		}
		num_outphases=value;
		dstate_setinfo("output.phases", "%d", value);

		if (reply[5] & (1<<4)) {	/* ISOFFLINE */
			dstate_setinfo("ups.type", "offline") ;
		}
		else if (reply[5] & (1<<5)) { /* ISINTERACTIVE */
			dstate_setinfo("ups.type", "line-interactive") ;
		}
		else {
			dstate_setinfo("ups.type", "online") ;
		}
	}

	/* determine scaling */
	/* full scaling output not defined yet, but we can differentiate sets of
	 * multipliers based on a sample scaling reading */
	memcpy(command,cmd_scaling1,6);
	ret = do_command((unsigned char *)command, reply, 6);
	if (ret < 8) {
		upsdebug_hex(2, "send: scaling detection: truncated", command, (size_t)ret);
	}
	else { /* add here multipliers that differentiate between models */
		switch (reply[6]) {
		case 1: /* GXT-2 */
			multi[M_FREQUENCY]=0.1;
			/* Confirmed correct on a Liebert GXT2-6000RT208 */
			multi[M_VOLT_DC]=0.1;
			multi[M_POWER]=1.0;
			multi[M_NOMPOWER]=1.0;
			break;
		case 2: /* NXe */
			multi[M_FREQUENCY]=0.01;
			multi[M_VOLT_DC]=0.1;
			multi[M_POWER]=100.0;
			multi[M_NOMPOWER]=100.0;
			break;
		default: /* the default values from definition of multi will be used */
			break;
		}
	}

	upsh.instcmd = instcmd;
	upsh.setvar = setvar;
}

void upsdrv_updateinfo(void)
{
	typedef struct {
		const unsigned char	cmd[6];
		const char	*var;
		const char	*fmt;
		const int	multindex;
	} cmd_s;

	static cmd_s vartab[] = { /* common vars */
		{ { 1,149,2,1,1,154 },	"battery.runtime", "%.0f", M_BAT_RUNTIME },
		{ { 1,149,2,1,2,155 },	"battery.voltage", "%.1f", M_VOLT_DC },
		{ { 1,149,2,1,3,156 },	"battery.current", "%.2f", M_CURRENT_DC },
		{ { 1,161,2,1,13,178 },	"battery.voltage.nominal", "%.1f", M_VOLT_DC },
		{ { 1,149,2,1,12,165 },	"battery.temperature", "%.1f", M_TEMPERATURE },
		{ { 1,149,2,1,14,167 },	"ups.temperature", "%.1f", M_TEMPERATURE },
		{ { 1,161,2,1,8,173 },	"ups.power.nominal", "%.0f", M_NOMPOWER },
		{ { 1,161,2,1,4,169 },	"ups.delay.start", "%.0f", M_10 },
		{ { 1,161,2,1,14,179  },"battery.runtime.low", "%.0f", M_BAT_RUNTIME },
		{ { 1,149,2,1,8,161 },	"input.frequency", "%.1f", M_FREQUENCY },
		{ { 1,149,2,1,10,163 },	"input.bypass.frequency", "%.1f", M_FREQUENCY },
		{ { 1,161,2,1,9,174 },	"input.frequency.nominal", "%.1f", M_FREQUENCY },
		{ { 1,149,2,1,9,162 },	"output.frequency", "%.1f", M_FREQUENCY },
		{ { 1,161,2,1,10,175 },	"output.frequency.nominal", "%.1f", M_FREQUENCY },
		{ { 0 }, NULL, NULL, 0 }
	};

	static cmd_s vartab1o[] = { /* 1-phase out */
		{ { 1,149,2,1,7,160 },	"ups.load", "%.0f", M_LOADPERC },
		{ { 1,149,2,1,6,159 },	"ups.power", "%.0f", M_POWER },
		{ { 1,149,2,1,5,158 },	"ups.realpower", "%.0f", M_POWER },
		{ { 1,144,2,1,3,151 },	"output.voltage", "%.1f", M_VOLTAGE_O },
		{ { 1,144,2,1,4,152 },	"output.current", "%.1f", M_CURRENT_O },
		{ { 0 }, NULL, NULL, 0 }
	};

	static cmd_s vartab1i[] = { /* 1-phase in*/
		{ { 1,144,2,1,1,149 },	"input.voltage", "%.1f", M_VOLTAGE_I },
		{ { 1,144,2,1,5,153 },	"input.bypass.voltage", "%.1f", M_VOLTAGE_B },
		{ { 1,144,2,1,6,154 },	"input.bypass.current", "%.1f", M_CURRENT_B },
		{ { 0 }, NULL, NULL, 0 }
	};

	static cmd_s vartab2o[] = { /*split-phase out, only V line-line is reported*/
		{ { 1,144,2,1,24,172 },	"output.L1.power.percent", "%.0f", M_LOADPERC },
		{ { 1,145,2,1,24,173 },	"output.L2.power.percent", "%.0f", M_LOADPERC },
		{ { 1,144,2,1,22,170 },	"output.L1.power", "%.0f", M_POWER },
		{ { 1,145,2,1,22,171 },	"output.L2.power", "%.0f", M_POWER },
		{ { 1,144,2,1,21,169 },	"output.L1.realpower", "%.0f", M_POWER },
		{ { 1,145,2,1,21,170 },	"output.L2.realpower", "%.0f", M_POWER },
		{ { 1,144,2,1,3,151 },	"output.voltage", "%.1f", M_VOLTAGE_O },
		{ { 0 }, NULL, NULL, 0 }
	};

	static cmd_s vartab3o[] = { /*3-phase out */
		{ { 1,144,2,1,24,172 },	"output.L1.power.percent", "%.0f", M_LOADPERC },
		{ { 1,145,2,1,24,173 },	"output.L2.power.percent", "%.0f", M_LOADPERC },
		{ { 1,146,2,1,24,174 },	"output.L3.power.percent", "%.0f", M_LOADPERC },
		{ { 1,144,2,1,22,170 },	"output.L1.power", "%.0f", M_POWER },
		{ { 1,145,2,1,22,171 },	"output.L2.power", "%.0f", M_POWER },
		{ { 1,146,2,1,22,172 },	"output.L3.power", "%.0f", M_POWER },
		{ { 1,144,2,1,21,169 },	"output.L1.realpower", "%.0f", M_POWER },
		{ { 1,145,2,1,21,170 },	"output.L2.realpower", "%.0f", M_POWER },
		{ { 1,146,2,1,21,171 },	"output.L3.realpower", "%.0f", M_POWER },
		{ { 1,144,2,1,3,151 },	"output.L1-N.voltage", "%.1f", M_VOLTAGE_O },
		{ { 1,145,2,1,3,152 },	"output.L2-N.voltage", "%.1f", M_VOLTAGE_O },
		{ { 1,146,2,1,3,153 },	"output.L3-N.voltage", "%.1f", M_VOLTAGE_O },
		{ { 1,144,2,1,14,162 },	"output.L1.crestfactor", "%.1f", M_0_1 },
		{ { 1,145,2,1,14,163 },	"output.L2.crestfactor", "%.1f", M_0_1 },
		{ { 1,146,2,1,14,164 },	"output.L3.crestfactor", "%.1f", M_0_1 },
		{ { 0 }, NULL, NULL, 0 }
	};

	static cmd_s vartab2i[] = { /*split-phase in, only reports V L-L */
		{ { 1,144,2,1,1,149 },	"input.voltage", "%.1f", M_VOLTAGE_I },

		{ { 1,144,2,1,5,153 },	"input.bypass.voltage", "%.1f", M_VOLTAGE_B },

		{ { 1,144,2,1,6,154 },	"input.bypass.current", "%.1f", M_CURRENT_B },

		{ { 1,144,2,1,2,150 },	"input.current", "%.1f", M_CURRENT_I },
		{ { 0 }, NULL, NULL, 0 }
	};

	static cmd_s vartab3i[] = { /*3-phase in */
		{ { 1,144,2,1,1,149 },	"input.L1-N.voltage", "%.1f", M_VOLTAGE_I },
		{ { 1,145,2,1,1,150 },	"input.L2-N.voltage", "%.1f", M_VOLTAGE_I },
		{ { 1,146,2,1,1,151 },	"input.L3-N.voltage", "%.1f", M_VOLTAGE_I },

		{ { 1,144,2,1,5,153 },	"input.L1-N.bypass.voltage", "%.1f", M_VOLTAGE_B },
		{ { 1,145,2,1,5,154 },	"input.L2-N.bypass.voltage", "%.1f", M_VOLTAGE_B },
		{ { 1,146,2,1,5,155 },	"input.L3-N.bypass.voltage", "%.1f", M_VOLTAGE_B },

		{ { 1,144,2,1,6,154 },	"input.L1-N.bypass.current", "%.1f", M_CURRENT_B },
		{ { 1,145,2,1,6,155 },	"input.L2-N.bypass.current", "%.1f", M_CURRENT_B },
		{ { 1,146,2,1,6,156 },	"input.L3-N.bypass.current", "%.1f", M_CURRENT_B },

		{ { 1,144,2,1,2,150 },	"input.L1.current", "%.1f", M_CURRENT_I },
		{ { 1,145,2,1,2,151 },	"input.L2.current", "%.1f", M_CURRENT_I },
		{ { 1,146,2,1,2,152 },	"input.L3.current", "%.1f", M_CURRENT_I },
		{ { 0 }, NULL, NULL, 0 }
	};

	static cmd_s * cmdin_p;
	static cmd_s * cmdout_p;

	const char	*val;
	char	reply[8];
	int	ret, i;

	for (i = 0; vartab[i].var; i++) {
		int16_t	intval;
		ret = do_command(vartab[i].cmd, reply, 6);
		if (ret < 8) {
			continue;
		}
		intval = (unsigned char)reply[5];
		intval <<= 8;
		intval += (unsigned char)reply[6];
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
		dstate_setinfo(vartab[i].var, vartab[i].fmt, multi[vartab[i].multindex] * intval);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
	}

	if (num_inphases==3){
		cmdin_p=vartab3i;
	}
	else if(num_inphases==2){
		cmdin_p=vartab2i;
	}
	else {
		cmdin_p=vartab1i;
	}

	if (num_outphases==3){
		cmdout_p=vartab3o;
	}
	else if(num_outphases==2){
		cmdout_p=vartab2o;
	}
	else {
		cmdout_p=vartab1o;
	}

	for (i = 0; cmdin_p[i].var; i++) {
		int16_t	intval;
		ret = do_command(cmdin_p[i].cmd, reply, 6);
		if (ret < 8) {
			continue;
		}
		intval = (unsigned char)reply[5];
		intval <<= 8;
		intval += (unsigned char)reply[6];
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
		dstate_setinfo(cmdin_p[i].var, cmdin_p[i].fmt, multi[cmdin_p[i].multindex] * intval);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
	}

	for (i = 0; cmdout_p[i].var; i++) {
		int16_t	intval;
		ret = do_command(cmdout_p[i].cmd, reply, 6);
		if (ret < 8) {
			continue;
		}
		intval = (unsigned char)reply[5];
		intval <<= 8;
		intval += (unsigned char)reply[6];
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
		dstate_setinfo(cmdout_p[i].var, cmdout_p[i].fmt, multi[cmdout_p[i].multindex] * intval);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
	}

	status_init();

	ret = do_command(cmd_bitfield1, reply, 6);
	if (ret < 8) {
		upslogx(LOG_ERR, "Failed reading bitfield #1");
		dstate_datastale();
		return;
	}

	if (reply[5] & (1<<0)) {	/* ON_BATTERY */
		status_set("OB");
	} else {
		status_set("OL");
	}

	val = dstate_getinfo("battery.current");
	if (val) {
		if (atof(val) > 0.05) {
			status_set("CHRG");
		}
		if (atof(val) < -0.05) {
			status_set("DISCHRG");
		}
	}

	ret = do_command(cmd_bitfield2, reply, 6);
	if (ret < 8) {
		upslogx(LOG_ERR, "Failed reading bitfield #2");
		dstate_datastale();
		return;
	}

	if (reply[6] & (1<<0)) {	/* ON_BYPASS */
		status_set("BYPASS");
	}

	if (reply[6] & (1<<5)) {	/* REPLACE_BATTERY */
		status_set("RB");
	}

	if (reply[6] & (1<<6)) {	/* BOOST_ON */
		status_set("BOOST");
	}

	if (reply[5] & (1<<1)) {	/* BUCK_ON */
		status_set("TRIM");
	}

	ret = do_command(cmd_bitfield3, reply, 6);
	if (ret < 8) {
		upslogx(LOG_ERR, "Failed reading bitfield #3");
		dstate_datastale();
		return;
	}

	if (reply[6] & (1<<0) ) {	/* UPS_OVERLOAD */
		status_set("OVER");
	}

	if (reply[6] & (1<<5) ) {	/* LOW_BATTERY */
		status_set("LB");
	}

	status_commit();

	dstate_dataok();
}

void upsdrv_shutdown(void)
{
	char reply[8];

	if(!(do_command(cmd_setOutOffMode, reply, 8) != -1) &&
	    (do_command(cmd_setOutOffDelay, reply, 8) != -1) &&
	    (do_command(cmd_sysLoadKey, reply, 6) != -1) &&
	    (do_command(cmd_shutdown, reply, 8) != -1))
			upslogx(LOG_ERR, "Failed to shutdown UPS");
}

static int instcmd(const char *cmdname, const char *extra)
{
/*
	if (!strcasecmp(cmdname, "test.battery.stop")) {
		ser_send_buf(upsfd, ...);
		return STAT_INSTCMD_HANDLED;
	}
*/
	upslogx(LOG_NOTICE, "instcmd: unknown command [%s] [%s]", cmdname, extra);
	return STAT_INSTCMD_UNKNOWN;
}

static int setvar(const char *varname, const char *val)
{
/*
	if (!strcasecmp(varname, "ups.test.interval")) {

	ser_send_buf(upsfd, ...);
		return STAT_SET_HANDLED;
	}
*/
	upslogx(LOG_NOTICE, "setvar: unknown variable [%s] [%s]", varname, val);
	return STAT_SET_UNKNOWN;
}

void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	addvar (VAR_VALUE, "baudrate", "serial line speed");
}

void upsdrv_initups(void)
{
	const char *val = getval("baudrate");
	speed_t baudrate = B2400;

	/* No-op, just made to quiesce the compiler warnings */
	NUT_UNUSED_FUNCTION_dummy_bitfields();

	if (val) {
		switch (atoi(val))
		{
		case 1200:
			baudrate = B1200;
			break;
		case 2400:
			baudrate = B2400;
			break;
		case 4800:
			baudrate = B4800;
			break;
		case 9600:
			baudrate = B9600;
			break;
		case 19200:
			baudrate = B19200;
			break;
		default:
			fatalx(EXIT_FAILURE, "Baudrate [%s] unsupported", val);
		}
	}

	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, baudrate);
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
