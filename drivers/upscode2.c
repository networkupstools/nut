/* upscode2.c - model specific routines for UPSes using the UPScode II
		command set.  This includes PowerWare, Fiskars, 
		Compaq (PowerWare OEM?), some IBM (PowerWare OEM?)

   Copyright (C) 2002 H�ard Lygre <hklygre@online.no>
   Copyright (C) 2004 Niels Baggesen <niels@baggesen.net>

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

   $Id$
*/

/*
 * Currently testing against
 * Fiskars PowerRite Max
 * Fiskars PowerServer 10
 * Fiskars PowerServer 30
 * Powerware Profile (9150)
 * Powerware 9305
 *
 * Also tested against
 * Compaq T1500h (Per J�sson <per.jonsson@bth.se>)
 * Powerware 9120 (Gorm J. Siiger <gjs@sonnit.dk>)
 * Fiskars PowerServer 10 (Per Larsson <tucker@algonet.se>)
 */

#include <math.h>

#include "main.h"
#include "serial.h"
#include "upscode2.h"
#include "timehead.h"

#define ENDCHAR		'\n'	
#define IGNCHARS	"\r"

/* default values */
#define OUT_PACE_USEC	200
#define INP_TIMO_SEC	2
#define FULL_UPDATE_TIMER	60

#define UPSC_BUFLEN	256  /* Size of response buffers from UPS */

/* Status messages from UPS */
#define UPSC_STAT_ONLINE       0x0001
#define UPSC_STAT_ONBATT       0x0002
#define UPSC_STAT_LOBATT       0x0004
#define UPSC_STAT_REPLACEBATT  0x0008
#define UPSC_STAT_BOOST        0x0010
#define UPSC_STAT_TRIM         0x0020
#define UPSC_STAT_OVERLOAD     0x0040
#define UPSC_STAT_CALIBRATION  0x0080
#define UPSC_STAT_OFF          0x0100
#define UPSC_STAT_BYPASS       0x0200


typedef enum {
	t_ignore,	/* Ignore this response  */
	t_value,	/* Sets a NUT variable */
	t_final,
	t_setval,	/* Sets a variable in the driver and possibly in NUT */
	t_setrecip,	/* Sets a driver variable to 1/value and possibly NUT */
	t_setpct,
	t_setrecpct,
	t_status,	/* Sets a status bit */
	t_alarm,	/* Sets an alarm message */
	t_list		/* The value must be matched in the list */
} type_t;


typedef struct simple_s {
	const char *code;
	type_t type;
	const char *desc;
	int status;
	float *aux;
	struct simple_s *stats;
} simple_t;

typedef struct {
	const char *cmd;
	const char *upsc;
	const char *upsp;
	int enabled;
} cmd_t;


static int
	can_upda = 0,
	can_upbs = 0,
	can_upid = 0,
	can_uppm = 0;

static char has_uppm_p[100];

static int
	input_timeout_sec = INP_TIMO_SEC,
	output_pace_usec = OUT_PACE_USEC,
	full_update_timer = FULL_UPDATE_TIMER,
	use_crlf = 0,
	use_pre_lf = 0,
	buffer_empty = 0,
	status = 0;

static time_t
	last_full = 0;

static float
	batt_volt_low = 0,
	batt_volt_high = 0,
	batt_volt = 0,
	nom_out_kw = 0,
	min_to_sec = 60.0,
	kilo_to_unity = 1000.0;


/* Status codes for the STAT and STMF status responses */
static simple_t att[] = {
	{ "00", t_ignore  },
	{ "AC", t_alarm,  "Aux contact failure", 0 },
	{ "BA", t_alarm,  "Batteries disconnected", 0 },
	{ "BC", t_alarm,  "Backfeed contact failure", 0 },
	{ "BD", t_alarm,  "Abnormal battery discharge", 0 },
	{ "BF", t_alarm,  "Battery fuse  failure", 0 },
	{ "BL", t_status, "Battery low limit", UPSC_STAT_LOBATT },
	{ "BO", t_alarm,  "Battery over voltage", 0 },
	{ "BP", t_alarm,  "Bypass fuse failure", 0 },
	{ "BR", t_alarm,  "Abnormal battery recharge", 0 },
	{ "BT", t_alarm,  "Battery over temperature", 0 },
	{ "BX", t_alarm,  "Bypass unavailable", 0 },
	{ "BY", t_alarm,  "Battery failure", 0 },
	{ "CE", t_alarm,  "Configuration error", 0 },
	{ "CM", t_alarm,  "Battery converter failure", 0 },
	{ "CT", t_alarm,  "Cabinet over temperature", 0 },
	{ "DO", t_alarm,  "DC over voltage", 0 },
	{ "DU", t_alarm,  "DC under voltage", 0 },
	{ "EP", t_alarm,  "Emergency power off", 0 },
	{ "FF", t_alarm,  "Fan failure", 0 },
	{ "FH", t_alarm,  "Line frequency high", 0 },
	{ "FL", t_alarm,  "Line frequency low", 0 },
	{ "FT", t_alarm,  "Filter over temperature", 0 },
	{ "GF", t_alarm,  "Ground failure", 0 },
	{ "HT", t_alarm,  "Charger over temperature", 0 },
	{ "IB", t_alarm,  "Internal data bus failure", 0 },
	{ "IF", t_alarm,  "Inverter fuse failure", 0 },
	{ "IM", t_alarm,  "Inverter failure", 0 },
	{ "IO", t_alarm,  "Inverter over voltage", 0 },
	{ "IP", t_alarm,  "Internal power supply failure", 0 },
	{ "IT", t_alarm,  "Inverter over temperature", 0 },
	{ "IU", t_alarm,  "Inverter under voltage", 0 },
	{ "IV", t_alarm,  "Inverter off", 0 },
	{ "LR", t_alarm,  "Loss of redundancy", 0 },
	{ "NF", t_alarm,  "Neutral fault", 0 },
	{ "OD", t_status, "UPS not supplying load", UPSC_STAT_OFF },
	{ "OF", t_alarm,  "Oscillator failure", 0 },
	{ "OL", t_status, "Overload", UPSC_STAT_OVERLOAD },
	{ "OR", t_alarm,  "Redundancy overload", 0 },
	{ "OV", t_alarm,  "Abnormal output voltage", 0 },
	{ "OW", t_alarm,  "Output failure", 0 },
	{ "PB", t_alarm,  "Parallel bus failure", 0 },
	{ "PE", t_alarm,  "Phase rotation error", 0 },
	{ "RE", t_alarm,  "Rectifier off", 0 },
	{ "RF", t_alarm,  "Rectifier fuse failure", 0 },
	{ "RM", t_alarm,  "Rectifier failure", 0 },
	{ "RT", t_alarm,  "Rectifier over temperature", 0 },
	{ "SM", t_alarm,  "Static switch failure", 0 },
	{ "ST", t_alarm,  "Static switch over temperature", 0 },
	{ "TT", t_alarm,  "Trafo over temperature", 0 },
	{ "UD", t_alarm,  "UPS disabled", 0 },
	{ "UO", t_alarm,  "Utility over voltage", 0 },
	{ "US", t_alarm,  "Unsynchronized", 0 },
	{ "UU", t_alarm,  "Utility under voltage", 0 },
	{ "VE", t_alarm,  "internal voltage error", 0 },
	{ NULL }
};


/* Status code for the STLR response */
static simple_t stlr[] = {
	{ "NO", t_ignore },
	{ "SD", t_status, NULL, UPSC_STAT_TRIM },
	{ "SU", t_status, NULL, UPSC_STAT_BOOST },
	{ "DU", t_status, NULL, UPSC_STAT_BOOST },
	{ NULL }
};


/* Status code for the STEA and STEM responses */
static simple_t env[] = {
	{ "HH", t_ignore, "Humidity high", 0 },
	{ "HL", t_ignore, "Humudity low", 0 },
	{ "TH", t_ignore, "Temperature high", 0 },
	{ "TL", t_ignore, "Temperature low", 0 },
	{ "01", t_ignore, "Environment alarm 1", 0 },
	{ "02", t_ignore, "Environment alarm 2", 0 },
	{ "03", t_ignore, "Environment alarm 3", 0 },
	{ "04", t_ignore, "Environment alarm 4", 0 },
	{ "05", t_ignore, "Environment alarm 5", 0 },
	{ "06", t_ignore, "Environment alarm 6", 0 },
	{ "07", t_ignore, "Environment alarm 7", 0 },
	{ "08", t_ignore, "Environment alarm 8", 0 },
	{ "09", t_ignore, "Environment alarm 9", 0 },
	{ NULL }
};


/* Responses for UPSS and UPDS */
static simple_t simple[] = {
	{ "STAT", t_list,   NULL, 0, 0, att },
	{ "STBO", t_status, NULL, UPSC_STAT_ONBATT },
	{ "STBL", t_status, NULL, UPSC_STAT_LOBATT },
	{ "STBM", t_ignore },
	{ "STBP", t_status, NULL, UPSC_STAT_BYPASS },
	{ "STEA", t_list,   NULL, 0, 0, env },
	{ "STEM", t_list,   NULL, 0, 0, env },
	{ "STLR", t_list,   NULL, 0, 0, stlr },
	{ "STMF", t_list,   NULL, 0, 0, att },
	{ "STOK", t_ignore },
	{ "STUF", t_status, NULL, UPSC_STAT_ONBATT },

	{ "BTIME", t_value, "battery.runtime", 0, &min_to_sec },

	{ "METE1", t_value, "ambient.temperature" },
	{ "MERH1", t_value, "ambient.humidity" },

	{ "MIFFF", t_value, "input.frequency" },
	{ "MIIL1", t_value,  /* "input.current" */ },
	{ "MIIL2", t_value,  /* "input.2.current" */ },
	{ "MIIL3", t_value,  /* "input.3.current" */ },
	{ "MIPL1", t_ignore, /* "input.kw" */ },
	{ "MIPL2", t_ignore, /* "input.kw" */ },
	{ "MIPL3", t_ignore, /* "input.kw" */ },
	{ "MISL1", t_ignore, /* "input.kva" */ },
	{ "MISL2", t_ignore, /* "input.kva" */ },
	{ "MISL3", t_ignore, /* "input.kva" */ },
	{ "MIUL1", t_value, "input.voltage" },
	{ "MIUL2", t_value, "input.2.voltage" },
	{ "MIUL3", t_value, "input.3.voltage" },
	{ "MIU12", t_value, "input.L1-L2.voltage" },
	{ "MIU23", t_value, "input.L2-L3.voltage" },
	{ "MIU31", t_value, "input.L3-L1.voltage" },

	{ "MBCH1", t_value, "battery.charge" },
	{ "MBIII", t_value, "battery.current" },
	{ "MBINE", t_ignore, /* "battery.current.negative" */ },
	{ "MBIPO", t_ignore, /* "battery.current.positive" */ },
	{ "MBUNE", t_ignore, /* "battery.voltage.negative" */ },
	{ "MBUPO", t_ignore, /* "battery.voltage.positive" */},
	{ "MBUUU", t_setval, "battery.voltage", 0, &batt_volt },

	{ "MLUNE", t_ignore, /* "dc.voltage.negative" */ },
	{ "MLUPO", t_ignore, /* "dc.voltage.positive" */ },
	{ "MLUUU", t_ignore, /* "dc.voltage" */ },

	{ "MOFFF", t_final, "output.frequency" },
	{ "MOIL1", t_value, "output.current" },
	{ "MOIL2", t_value, "output.2.current" },
	{ "MOIL3", t_value, "output.3.current" },
	{ "MOIP1", t_ignore, /* "output.current.peak" */ },
	{ "MOPL1", t_value, "ups.load", 0, &nom_out_kw },
	{ "MOPL2", t_value, "ups.2.load", 0, &nom_out_kw },
	{ "MOPL3", t_value, "ups.3.load", 0, &nom_out_kw },
	{ "MOSL1", t_ignore, /* "output.kva" */ },
	{ "MOUL1", t_value, "output.voltage" },
	{ "MOUL2", t_value, "output.2.voltage" },
	{ "MOUL3", t_value, "output.3.voltage" },

	{ "MPUL1", t_value, "bypass.voltage" },
	{ "MPUL2", t_value, "bypass.2.voltage" },
	{ "MPUL3", t_value, "bypass.3.voltage" },

	{ "MUTE1", t_value, "ups.temperature" },
	{ NULL }
};


/* Responses for UPDV */
static simple_t nominal[] = {
	{ "NIUHH", t_value, "input.voltage.maximum" },
	{ "NIULL", t_value, "input.voltage.minimim" },
    	{ "NIUNN", t_value, "input.voltage.nominal" },

	{ "NBAHN", t_ignore, /* "battery.capacity.nominal" */ },
	{ "NBIHH", t_ignore, /* "battery.current.maximum" */ },
	{ "NBILL", t_ignore, /* "battery.current.minimum" */ },
	{ "NBINN", t_ignore },
	{ "NBTHH", t_ignore, /* "battery.time.maximum" */ },
	{ "NBUHH", t_setval, NULL, 0, &batt_volt_high },
	{ "NBULL", t_setval, NULL, 0, &batt_volt_low },
	{ "NBUNN", t_value, "battery.voltage.nominal" },

	{ "NOFHH", t_ignore, /* "output.frequency.maximum" */ },
	{ "NOFLL", t_final,  /* "output.frequency.minimum" */ },
	{ "NOIHH", t_ignore, /* "output.current.maximum" */ },
	{ "NOINN", t_ignore, /* "output.current.target" */ },
	{ "NOPNN", t_setrecpct, NULL, 0, &nom_out_kw },
	{ "NOSNN", t_value,  "ups.power.nominal", 0, &kilo_to_unity },
	{ "NOUHH", t_ignore, /* "output.voltage.maximum" */ },
	{ "NOULL", t_ignore, /* "output.voltage.minimum" */ },
	{ "NOUNN", t_ignore, /* "output.voltage.target" */ },

	{ "NUTEH", t_value,  /* "ups.temperature.maximum" */ },
    	{ NULL }
};


/* Status responses for UPBS command */
static simple_t battery[] = {
    	{ "MBTE1", t_value, "battery.temperature" },
	{ "MBIN1", t_ignore, NULL },
	{ "BDAT1", t_value, "battery.date" },
	{ NULL }
};


static cmd_t commands[] = {
	{ "load.off",			NULL, NULL },
	{ "load.on",			NULL, NULL },
	{ "shutdown.return",		"UPPF", "IJHLDMGCIU" },
	{ "shutdown.stayoff",		"UPPD", "LGGNLMDPGV" },
	{ "shutdown.stop",		"UPPU", NULL },
	{ "shutdown.reboot",		"UPPC", "IJHLDMGCIU" },
	{ "shutdown.reboot.graceful",	NULL, NULL },
	{ "test.panel.start",		"UPIS", NULL },
	{ "test.panel.stop",		NULL, NULL },
	{ "test.failure.start",		NULL, NULL },
	{ "test.failure.stop",		NULL, NULL },
	{ "test.battery.start",		"UPBT", "1" },
	{ "test.battery.stop",		NULL, NULL },
	{ "calibrate.start",		NULL, NULL },
	{ "calibrate.stop",		NULL, NULL },
	{ "bypass.start",		NULL, NULL },
	{ "bypass.stop",		NULL, NULL },
	{ "reset.input.minmax",		NULL, NULL },
	{ "reset.watchdog",		NULL, NULL },
	{ "beeper.on",			NULL, NULL },
	{ "beeper.off",			NULL, NULL },
	{ NULL }
};


static cmd_t variables[] = {
	{ "ups.delay.reboot",		"UPCD", "ACCD" },
	{ "ups.delay.shutdown",		"UPSD", "ACSD" },
    	{ NULL }
};


static int instcmd (const char *auxcmd, const char *data);
static int setvar (const char *var, const char *data);
static void upsc_setstatus(unsigned int status);
static void upsc_flush_input(void);
static void upsc_getbaseinfo(void);
static void upsc_commandlist(void);
static int upsc_getparams(const char *cmd, const simple_t *table);
static int upsc_getvalue(const char *cmd, const char *param,
	const char *resp, const char *var);
static void upscsend(const char *);
static char *upscrecv(char *);
static int upsc_simple(const simple_t *sp, const char *var, const char *val);
static void check_uppm(void);
static float batt_charge_pct(void);


void upsdrv_banner(void)
{
	printf("Network UPS Tools - UPScode II UPS driver %s (%s)\n", 
		DRV_VERSION, UPS_VERSION);
	printf("Copyright (C) 2001-2002 H�ard Lygre, <hklygre@online.no>\n");
	printf("Copyright (C) 2004 Niels Baggesen <niels@baggesen.net>\n\n");

	experimental_driver = 1;
}


void upsdrv_help(void)
{
}


void upsdrv_initups(void)
{
    	struct termios tio;
	int baud = B1200;
	char *str;

	upsdebugx(1, "upscode2 version %s", DRV_VERSION);
	if ((str = getval("baudrate")) != NULL) {
	    	int temp = atoi(str);
		switch (temp) {
		case   300:
		    	baud =   B300; break;
		case   600:
			baud =   B600; break;
		case  1200:
			baud =  B1200; break;
		case  2400:
			baud =  B2400; break;
		case  4800:
			baud =  B4800; break;
		case  9600:
			baud =  B9600; break;
		case 19200:
			baud = B19200; break;
		case 38400:
			baud = B38400; break;
		default:
		    	fatalx("Unrecognized baudrate: %s", str);
		}
		upsdebugx(1, "baud_rate = %d", temp);
	}
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, baud);

	if (tcgetattr(upsfd, &tio) != 0)
	    	fatal_with_errno("tcgetattr(%s)", device_path);
	tio.c_lflag = ICANON;
	tio.c_cc[VMIN] = 0;
	tio.c_cc[VTIME] = 0;
	tcsetattr(upsfd, TCSANOW, &tio);

	if ((str = getval("input_timeout")) != NULL) {
	    	int temp = atoi(str);
		if (temp <= 0)
		    	fatalx("Bad input_timeout parameter: %s", str);
		input_timeout_sec = temp;
	}
	upsdebugx(1, "input_timeout = %d Sec", input_timeout_sec);

	if ((str = getval("output_pace")) != NULL) {
	    	int temp = atoi(str);
		if (temp <= 0)
		    	fatalx("Bad output_pace parameter: %s", str);
		output_pace_usec = temp;
	}
	upsdebugx(1, "output_pace = %d uSec", output_pace_usec);

	if ((str = getval("full_update_timer")) != NULL) {
	    	int temp = atoi(str);
		if (temp <= 0)
		    	fatalx("Bad full_update_timer parameter: %s", str);
		full_update_timer = temp;
	}
	upsdebugx(1, "full_update_timer = %d Sec", full_update_timer);

	use_crlf = testvar("use_crlf");
	upsdebugx(1, "use_crlf = %d", use_crlf);
	use_pre_lf = testvar("use_pre_lf");
	upsdebugx(1, "use_pre_lf = %d", use_pre_lf);
}


void upsdrv_initinfo(void)
{
	dstate_setinfo("driver.version.internal", "%s", DRV_VERSION);

	upsc_getbaseinfo();
	upsc_commandlist();
	if (can_upda) {
	    	upsc_flush_input();
		upscsend("UPDA");
	}
	if (can_upid)
		upsc_getvalue("UPID", NULL, "ACID", "ups.id");
	if (can_uppm)
		check_uppm();

	upsh.instcmd = instcmd;
	upsh.setvar = setvar;
}


void upsdrv_updateinfo(void)
{
	time_t now;
	float batt_charge;
	int ok;

	alarm_init();
	status = 0;

	ok = upsc_getparams("UPDS", simple);

	time(&now);
	if (ok && now - last_full > full_update_timer) {
		last_full = now;
		ok = upsc_getparams("UPDV", nominal);
		if (ok && can_upbs)
			ok = upsc_getparams("UPBS", battery);
	}

	if (!ok) {
		dstate_datastale();
		last_full = 0;
		return;
	}

	batt_charge = batt_charge_pct();
	if (batt_charge >= 0)
	    	dstate_setinfo("battery.charge", "%6.2f", batt_charge);

	if (!(status & UPSC_STAT_ONBATT))
		status |= UPSC_STAT_ONLINE;
	
	upsc_setstatus(status);
	alarm_commit();
	
	dstate_dataok();
	ser_comm_good();
}


void upsdrv_shutdown(void)
{
	upslogx(LOG_EMERG, "Emergency shutdown");
	upscsend("UPSD");	/* Set shutdown delay */
	upscsend("1");		/* 1 second (lowest possible. 0 returns current.*/

	upslogx(LOG_EMERG, "Shutting down...");
	upscsend("UPPC");	/* Powercycle UPS */
	upscsend("IJHLDMGCIU"); /* security code */
}


static int instcmd (const char *auxcmd, const char *data)
{
	cmd_t *cp = commands;

	upsdebugx(1, "Instcmd: %s %s", auxcmd, data ? data : "\"\"");
	while (cp->cmd) {
		if (strcmp(cp->cmd, auxcmd) == 0) {
			upscsend(cp->upsc);
			if (cp->upsp)
				upscsend(cp->upsp);
			else if (data)
			    	upscsend(data);
			return STAT_INSTCMD_HANDLED;
		}
		cp++;
	}
	upslogx(LOG_INFO, "instcmd: unknown command %s", auxcmd);
	return STAT_INSTCMD_UNKNOWN;
}


static int setvar (const char *var, const char *data)
{
	cmd_t *cp = variables;

	upsdebugx(1, "Setvar: %s %s", var, data);
	while (cp->cmd) {
		if (strcmp(cp->cmd, var) == 0) {
		    	upsc_getvalue(cp->upsc, data, cp->upsp, cp->cmd);
			return STAT_SET_HANDLED;
		}
		cp++;
	}
	upslogx(LOG_INFO, "Setvar: unsettable variable %s", var);
	return STAT_SET_UNKNOWN;
}


/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "manufacturer", "manufacturer [unknown]");
	addvar(VAR_VALUE, "baudrate", "Serial interface baudrate [1200]");
	addvar(VAR_VALUE, "input_timeout", "Command response timeout [2]");
	addvar(VAR_VALUE, "output_pace", "Output character delay in usecs [200]");
	addvar(VAR_VALUE, "full_update", "Delay between full value downloads [60]");
	addvar(VAR_FLAG, "use_crlf", "Use CR-LF to terminate commands to UPS");
	addvar(VAR_FLAG, "use_pre_lf", "Use LF to introduce commands to UPS");
}


void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}

   
/*
  Generate status string from bitfield
*/
static void upsc_setstatus(unsigned int status)
{

   /*
	* I'll look for all available statuses, even though they might not be
	*  supported in the UPScode II protocol.
	*/

	status_init();

	if(status & UPSC_STAT_ONLINE)
		status_set("OL");
	if(status & UPSC_STAT_ONBATT)
		status_set("OB");
	if(status & UPSC_STAT_LOBATT)
		status_set("LB");
	if(status & UPSC_STAT_REPLACEBATT)
		status_set("RB");
	if(status & UPSC_STAT_BOOST)
		status_set("BOOST");
	if(status & UPSC_STAT_TRIM)
		status_set("TRIM");
	if(status & UPSC_STAT_OVERLOAD)
		status_set("OVER");
	if(status & UPSC_STAT_CALIBRATION)
		status_set("CAL");
	if(status & UPSC_STAT_OFF)
		status_set("OFF");
	if(status & UPSC_STAT_BYPASS)
		status_set("BYPASS");
 
	status_commit();
}


/* Add \r to end of command and send to UPS */
static void upscsend(const char *cmd)
{
	upsdebugx(3, "upscsend: '%s'", cmd);
	ser_send_pace(upsfd, output_pace_usec, "%s%s%s",
		use_pre_lf ? "\n" : "",
		cmd,
		use_crlf ? "\r\n" : "\r");
	return;
}


/* Return a string read from UPS */
static char *upscrecv(char *buf)
{
    	int res = 0;

	while (res == 0) {
		res = ser_get_line(upsfd, buf, UPSC_BUFLEN, ENDCHAR, IGNCHARS,
			       input_timeout_sec, 0);
	    	if (strlen(buf) == 0)
		    	upsdebugx(3, "upscrecv: Empty line");
	}

	if (res == -1)
	    	upsdebugx(3, "upscrecv: Timeout");
	else
		upsdebugx(3, "upscrecv: %u bytes:\t'%s'",
			(unsigned int)strlen(buf), buf);
	return buf;
}


static void upsc_flush_input(void)
{
    	char buf[UPSC_BUFLEN];

	do {
		upscrecv(buf);
		if (strlen(buf) > 0)
			upsdebugx(1, "Skipping input: %s", buf);
	} while (strlen(buf) > 0);
}


/* check which commands this ups supports */
static void upsc_commandlist(void)
{
    	char buf[UPSC_BUFLEN];
	cmd_t *cp;

	upsc_flush_input();
	upscsend("UPCL");
	while (1) {
	    	upscrecv(buf);
		if (strlen(buf) == 0) {
		    	upslogx(LOG_ERR, "Missing UPCL after UPCL");
		    	break;
		}
		upsdebugx(2, "Supports command: %s", buf);

		if (strcmp(buf, "UPBS") == 0)
			can_upbs = 1;
		else if (strcmp(buf, "UPPM") == 0)
			can_uppm = 1;
		else if (strcmp(buf, "UPID") == 0)
			can_upid = 1;
		else if (strcmp(buf, "UPDA") == 0)
			can_upda = 1;
		cp = commands;
		while (cp->cmd) {
		    	if (cp->upsc && strcmp(cp->upsc, buf) == 0) {
			    	upsdebugx(1, "instcmd: %s %s", cp->cmd, cp->upsc);
			    	dstate_addcmd(cp->cmd);
				cp->enabled = 1;
				break;
			}
			cp++;
		}
		cp = variables;
		while (cp->cmd) {
		    	if (cp->upsc && strcmp(cp->upsc, buf) == 0) {
			    	upsdebugx(1, "setvar: %s %s", cp->cmd, cp->upsc);
				cp->enabled = 1;
				break;
			}
			cp++;
		}

		if (strcmp(buf, "UPCL") == 0)
		    	break;
	}

	cp = variables;
	while (cp->cmd) {
		if (cp->enabled) {
		    	upsc_getvalue(cp->upsc, "0", cp->upsp, cp->cmd);
			dstate_setflags(cp->cmd, ST_FLAG_RW | ST_FLAG_STRING);
			dstate_setaux(cp->cmd, 7);
		}
		cp++;
	}
}


/* get limits and parameters */
static int upsc_getparams(const char *cmd, const simple_t *table)
{
    	char var[UPSC_BUFLEN];
    	char val[UPSC_BUFLEN];
	int first = 1;

	upsc_flush_input();

	upscsend(cmd);
	buffer_empty = 0;
	while (!buffer_empty) {
	    	upscrecv(var);
		if (strlen(var) == 0) {
		    	if (first)
			    	upscrecv(var);
			if (strlen(var) == 0) {
				ser_comm_fail("Empty string from UPS for %s!",
					cmd);
				break;
			}
		}
		first = 0;
	    	upscrecv(val);
		if (strlen(val) == 0) {
			ser_comm_fail("Empty value from UPS for %s %s!", cmd, var);
		    	break;
		}
		upsdebugx(2, "Parameter %s %s", var, val);
		if (!upsc_simple(table, var, val))
		    	upslogx(LOG_ERR, "Unknown response to %s: %s %s",
				cmd, var, val);
	}
	return buffer_empty;
}


static void check_uppm(void)
{
	int i, last = 0;
	char var[UPSC_BUFLEN];
	char val[UPSC_BUFLEN];

	upsc_flush_input();
	upscsend("UPPM");
	upscsend("0");
	upscrecv(var);
	if (strcmp(var, "ACPM"))
		upslogx(LOG_ERR, "Bad response to UPPM: %s", var);
	while (1) {
		int val, stat;
		upscrecv(var);
		if (strlen(var) == 0)
			break;
		upsdebugx(2, "UPPM available: %s", var);
		stat = sscanf(var, "P%2d", &val);
		if (stat != 1) {
		    	upslogx(LOG_ERR, "Bad response to UPPM: %s", var);
			return;
		}
		has_uppm_p[val] = 1;
		if (val > last)
			last = val;
	}

	for (i = 0; i <= last; i++) {
		if (!has_uppm_p[i])
			continue;
		upscsend("UPPM");
		snprintf(var, sizeof(var), "P%.2d", i);
		upscsend(var);
		upscrecv(val);
		if (strcmp(val, "ACPM")) {
		    	upslogx(LOG_ERR, "Bad response to UPPM %s: %s", var, val);
			continue;
		}
		upscrecv(var);
		upsdebugx(1, "UPPM value: %s", var);
	}
}


static int upsc_getvalue(const char *cmd, const char *param,
			const char *resp, const char *nutvar)
{
    	char var[UPSC_BUFLEN];
    	char val[UPSC_BUFLEN];

	upsdebugx(2, "Request value: %s %s", cmd, param ? param : "\"\"");
	upscsend(cmd);
	if (param)
	    	upscsend(param);
	upscrecv(var);
	upscrecv(val);
	upsdebugx(2, "Got value: %s %s", var, val);
	if (strcmp(var, resp)) {
		upslogx(LOG_ERR, "Bad response to %s %s: %s %s",
			cmd, param ? param : "\"\"", var, val);
		return 0;
	}
	else {
		dstate_setinfo(nutvar, "%s", val);
	}
	return 1;
}


static void upsc_getbaseinfo(void)
{
	char *buf;

	dstate_setinfo("ups.mfr", "%s",
		((buf = getval("manufacturer")) != NULL) ? buf : "unknown");

	if (!upsc_getvalue("UPTP", NULL, "NNAME", "ups.model"))
		upsc_getvalue("UPTP", NULL, "NNAME", "ups.model");
	upsc_getvalue("UPSN", "0", "ACSN", "ups.serial");
}


static int upsc_simple(const simple_t *sp, const char *var, const char *val)
{
	char buf[UPSC_BUFLEN];
	int stat;
	float fval;

	while (sp->code) {
		if (strcmp(sp->code, var) == 0) {
			switch (sp->type) {
			case t_setval:
				stat = sscanf(val, "%f", &fval);
				if (stat != 1)
					upslogx(LOG_ERR, "Bad float: %s %s", var, val);
				if (sp->desc)
					dstate_setinfo(sp->desc, val);
				*sp->aux = fval;
				break;
			case t_setrecip:
				stat = sscanf(val, "%f", &fval);
				if (stat != 1)
					upslogx(LOG_ERR, "Bad float: %s %s", var, val);
				if (sp->desc)
					dstate_setinfo(sp->desc, val);
				*sp->aux = 1/fval;
				break;
			case t_setpct:
				stat = sscanf(val, "%f", &fval);
				if (stat != 1)
					upslogx(LOG_ERR, "Bad float: %s %s", var, val);
				*sp->aux = fval*100;
				if (sp->desc)
					dstate_setinfo(sp->desc, val);
				break;
			case t_setrecpct:
				stat = sscanf(val, "%f", &fval);
				if (stat != 1)
					upslogx(LOG_ERR, "Bad float: %s %s", var, val);
				*sp->aux = 1/fval*100;
				if (sp->desc)
					dstate_setinfo(sp->desc, val);
				break;
			case t_final:
				buffer_empty = 1;
			case t_value:
				if (sp->aux != NULL) {
					stat = sscanf(val, "%f", &fval);
					if (stat != 1)
						upslog_with_errno(LOG_ERR, "Bad float in %s: %s", var, val);
					else {
						snprintf(buf, UPSC_BUFLEN, "%5.3f", fval**(sp->aux));
						if (sp->desc)
							dstate_setinfo(sp->desc, buf);
					}
				}
				else if (sp->desc)
					dstate_setinfo(sp->desc, val);
				break;
			case t_status:
				if (strcmp(val, "00") == 0)
					;
				else if (strcmp(val, "11") == 0)
					status |= sp->status;
				else
					upslogx(LOG_ERR, "Unknown status value: '%s' '%s'", var, val);
				break;
			case t_alarm:
				if (strcmp(val, "00") == 0)
					;
				else if (strcmp(val, "11") == 0)
					status |= sp->status;
				else
					upslogx(LOG_ERR, "Unknown alarm value: '%s' '%s'", var, val);
				break;
			case t_ignore:
				break;
			case t_list:
				if (!upsc_simple(sp->stats, val, "11"))
					upslogx(LOG_ERR, "Unknown value: %s %s",
						var, val);
				break;
			default:
				upslogx(LOG_ERR, "Unknown type for %s", var);
				break;
			}
			return 1;
		}
		sp++;
	}
	return 0;
}


static float batt_charge_pct(void)
{
    	float dqdv;

    	upsdebugx(2, "battery.charge: %f %f %f",
		batt_volt_low, batt_volt, batt_volt_high);

    	if (batt_volt_low == 0 || batt_volt_high == 0 || batt_volt == 0)
	    	return -1;

	if (batt_volt > batt_volt_high)
	    	return 100;
	if (batt_volt < batt_volt_low)
	    	return 0;
	dqdv = (batt_volt - batt_volt_low) / (batt_volt_high - batt_volt_low);

	upsdebugx(2, "dqdv: %.2f %.2f", dqdv, sqrt(dqdv));
	return sqrt(dqdv)*100;
}
