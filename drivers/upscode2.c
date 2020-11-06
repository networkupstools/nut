/* upscode2.c - model specific routines for UPSes using the UPScode II
		command set.  This includes PowerWare, Fiskars,
		Compaq (PowerWare OEM?), some IBM (PowerWare OEM?)

   Copyright (C) 2002 H?vard Lygre <hklygre@online.no>
   Copyright (C) 2004-2006 Niels Baggesen <niels@baggesen.net>
   Copyright (C) 2006 Niklas Edmundsson <nikke@acc.umu.se>

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

/*
 * Currently testing against
 * Fiskars PowerRite Max
 * Fiskars PowerServer 10
 * Fiskars PowerServer 30
 * Powerware Profile (9150)
 * Powerware 9305
 *
 * Also tested against
 * Compaq T1500h (Per J?nsson <per.jonsson@bth.se>)
 * Powerware 9120 (Gorm J. Siiger <gjs@sonnit.dk>)
 * Fiskars PowerServer 10 (Per Larsson <tucker@algonet.se>)
 */

#include "main.h"
#include "serial.h"
#include "timehead.h"
#include "nut_stdint.h"

#include <math.h>

#define DRIVER_NAME	"UPScode II UPS driver"
#define DRIVER_VERSION	"0.89"

/* driver description structure */
upsdrv_info_t	upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"H K Lygre, <hklygre@online.no>\n" \
	"Niels Baggesen <niels@baggesen.net>\n" \
	"Niklas Edmundsson <nikke@acc.umu.se>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

#define ENDCHAR		'\n'

/* default values */
#define OUT_PACE_USEC	200
#define INP_TIMO_SEC	2
#define FULL_UPDATE_TIMER	60

#define UPSC_BUFLEN	256  /* Size of response buffers from UPS */

/* Status messages from UPS */
#define UPSC_STAT_ONLINE       (1UL << 0)
#define UPSC_STAT_ONBATT       (1UL << 1)
#define UPSC_STAT_LOBATT       (1UL << 2)
#define UPSC_STAT_REPLACEBATT  (1UL << 3)
#define UPSC_STAT_BOOST        (1UL << 4)
#define UPSC_STAT_TRIM         (1UL << 5)
#define UPSC_STAT_OVERLOAD     (1UL << 6)
#define UPSC_STAT_CALIBRATION  (1UL << 7)
#define UPSC_STAT_OFF          (1UL << 8)
#define UPSC_STAT_BYPASS       (1UL << 9)
#define UPSC_STAT_NOTINIT      (1UL << 31)


typedef enum {
	t_ignore,	/* Ignore this response  */
	t_value,	/* Sets a NUT variable */
	t_final,	/* Marks the end of UPS data for this command */
	t_string,	/* Set a NUT string variable */
	t_finstr,	/* Set a NUT string variable, and marks end of data */
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
	can_uppm = 0,
	can_updt = 0,
	can_uptm = 0,
	can_upsd = 0,
	can_uppc = 0;

static char has_uppm_p[100];

static int
	input_timeout_sec = INP_TIMO_SEC,
	output_pace_usec = OUT_PACE_USEC,
	full_update_timer = FULL_UPDATE_TIMER,
	use_crlf = 0,
	use_pre_lf = 0,
	buffer_empty = 0;

static uint32_t
	status = UPSC_STAT_NOTINIT;

static time_t
	last_full = 0;

static float
	batt_volt_low = 0,
	batt_volt_nom = 0,
	batt_volt_high = 0,
	batt_volt = 0,
	batt_cap_nom = -1,
	batt_charge = -1,
	batt_current = -1,
	batt_disch_curr_max = 0,
	batt_runtime = -1,
	batt_runtime_max = -1,
	kilo_to_unity = 1000.0,
	outpwr_factor = 1000.0,
	nom_out_current = -1,
	max_out_current = -1;

/* To get average battery current */
#define NUM_BATTHIST 60
	static float batthist[NUM_BATTHIST];
	static int numbatthist=0, lastbatthist=0;

static int
	inited_phaseinfo = 0,
	num_inphases = 1,
	num_outphases = 1;


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
	{ "HL", t_ignore, "Humidity low", 0 },
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

	{ "BTIME", t_setval, NULL, 0, &batt_runtime },

	{ "METE1", t_value, "ambient.temperature" },
	{ "MERH1", t_value, "ambient.humidity" },

	{ "MIFFF", t_value, "input.frequency" },
	{ "MIIL1", t_value, "input.current" },
	{ "MIIL2", t_value, "input.L2.current" },
	{ "MIIL3", t_value, "input.L3.current" },
	{ "MIPL1", t_value, "input.realpower" },
	{ "MIPL2", t_value, "input.L2.realpower" },
	{ "MIPL3", t_value, "input.L3.realpower" },
	{ "MISL1", t_value, "input.power" },
	{ "MISL2", t_value, "input.L2.power" },
	{ "MISL3", t_value, "input.L3.power" },
	{ "MIUL1", t_value, "input.voltage" },
	{ "MIUL2", t_value, "input.L2-N.voltage" },
	{ "MIUL3", t_value, "input.L3-N.voltage" },
	{ "MIU12", t_value, "input.L1-L2.voltage" },
	{ "MIU23", t_value, "input.L2-L3.voltage" },
	{ "MIU31", t_value, "input.L3-L1.voltage" },

	{ "MBCH1", t_setval, NULL, 0, &batt_charge }, /* battery.charge */
	{ "MBIII", t_setval, "battery.current", 0, &batt_current },
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
	{ "MOIL2", t_value, "output.L2.current" },
	{ "MOIL3", t_value, "output.L3.current" },
	{ "MOIP1", t_value, "output.current.peak" },
	{ "MOIP2", t_value, "output.L2.current.peak" },
	{ "MOIP3", t_value, "output.L3.current.peak" },
	{ "MOPL1", t_value, "output.realpower", 0, &kilo_to_unity },
	{ "MOPL2", t_value, "output.L2.realpower", 0, &kilo_to_unity },
	{ "MOPL3", t_value, "output.L3.realpower", 0, &kilo_to_unity },
	{ "MOSL1", t_value, "output.power" },
	{ "MOSL2", t_value, "output.L2.power" },
	{ "MOSL3", t_value, "output.L3.power" },
	{ "MOUL1", t_value, "output.voltage" },
	{ "MOUL2", t_value, "output.L2-N.voltage" },
	{ "MOUL3", t_value, "output.L3-N.voltage" },
	{ "MOU12", t_value, "output.L1-L2.voltage" },
	{ "MOU23", t_value, "output.L2-L3.voltage" },
	{ "MOU31", t_value, "output.L3-L1.voltage" },

	{ "MPUL1", t_value, "input.bypass.L1-N.voltage" },
	{ "MPUL2", t_value, "input.bypass.L2-N.voltage" },
	{ "MPUL3", t_value, "input.bypass.L3-N.voltage" },

	{ "MUTE1", t_value, "ups.temperature" },
	{ NULL }
};


/* Responses for UPDV */
static simple_t nominal[] = {
	{ "NIUHH", t_value, "input.voltage.maximum" },
	{ "NIULL", t_value, "input.voltage.minimum" },
	{ "NIUNN", t_value, "input.voltage.nominal" },

	{ "NIIHH", t_value, "input.current.maximum" },
	{ "NIILL", t_value, "input.current.minimum" },
	{ "NIINN", t_value, "input.current.nominal" },

	{ "NIPHH", t_value, "input.realpower.maximum" },
	{ "NIPNN", t_value, "input.realpower.nominal" },

	{ "NISHH", t_value, "input.power.maximum" },
	{ "NISNN", t_value, "input.power.nominal" },

	{ "NBAHN", t_setval, "battery.capacity.nominal", 0, &batt_cap_nom },
	{ "NBIHH", t_ignore, "battery charge current maximum" },
	{ "NBILL", t_setval, NULL, 0, &batt_disch_curr_max},
	{ "NBINN", t_value, "battery.current.nominal" },
	{ "NBTHH", t_setval, NULL, 0, &batt_runtime_max},
	{ "NBUHH", t_setval, "battery.voltage.maximum", 0, &batt_volt_high },
	{ "NBULL", t_setval, "battery.voltage.minimum", 0, &batt_volt_low },
	{ "NBUNN", t_setval,  "battery.voltage.nominal", 0, &batt_volt_nom },

	{ "NOFHH", t_value,  "output.frequency.maximum" },
	{ "NOFLL", t_final,  "output.frequency.minimum" },
	{ "NOIHH", t_setval,  "output.current.maximum", 0, &max_out_current },
	{ "NOINN", t_setval, "output.current.nominal", 0, &nom_out_current },
	{ "NOPNN", t_value, "output.realpower.nominal", 0, &outpwr_factor },
	{ "NOSNN", t_value,  "ups.power.nominal", 0, &outpwr_factor },
	{ "NOUHH", t_value,  "output.voltage.maximum" },
	{ "NOULL", t_value,  "output.voltage.minimum" },
	{ "NOUNN", t_value,  "output.voltage.nominal" },

	{ "NUTEH", t_value,  "ups.temperature.maximum" },
	{ NULL }
};


/* Status responses for UPBS command */
static simple_t battery[] = {
	{ "MBTE1", t_value, "battery.1.temperature" },
	{ "MBIN1", t_ignore, NULL /* aging index */ },
	{ "BDAT1", t_string, "battery.1.date" },
	{ "MBTE2", t_value, "battery.2.temperature.2" },
	{ "MBIN2", t_ignore, NULL },
	{ "BDAT2", t_string, "battery.2.date" },
	{ "MBTE3", t_value, "battery.3.temperature" },
	{ "MBIN3", t_ignore, NULL },
	{ "BDAT3", t_string, "battery.3.date" },
	{ "MBTE4", t_value, "battery.4.temperature" },
	{ "MBIN4", t_ignore, NULL },
	{ "BDAT4", t_string, "battery.4.date" },
	{ "MBTE5", t_value, "battery.5.temperature" },
	{ "MBIN5", t_ignore, NULL },
	{ "BDAT5", t_string, "battery.5.date" },
	{ "MBTE6", t_value, "battery.6.temperature" },
	{ "MBIN6", t_ignore, NULL },
	{ "BDAT6", t_string, "battery.6.date" },
	{ "MBTE7", t_value, "battery.7.temperature" },
	{ "MBIN7", t_ignore, NULL },
	{ "BDAT7", t_string, "battery.7.date" },
	{ "MBTE8", t_value, "battery.8.temperature" },
	{ "MBIN8", t_ignore, NULL },
	{ "BDAT8", t_finstr, "battery.8.date" },
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
	{ "beeper.enable",		NULL, NULL },
	{ "beeper.disable",		NULL, NULL },
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
static int upsc_commandlist(void);
static int upsc_getparams(const char *cmd, const simple_t *table);
static int upsc_getvalue(const char *cmd, const char *param,
	const char *resp, const char *var, char *ret);
static int upscsend(const char *cmd);
static int upscrecv(char *buf);
static int upsc_simple(const simple_t *sp, const char *var, const char *val);
static void check_uppm(void);
static float batt_charge_pct(void);



void upsdrv_help(void)
{
}


void upsdrv_initups(void)
{
	struct termios tio;
	int baud = B1200;
	char *str;

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
			fatalx(EXIT_FAILURE, "Unrecognized baudrate: %s", str);
		}
		upsdebugx(1, "baud_rate = %d", temp);
	}
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, baud);

	if (tcgetattr(upsfd, &tio) != 0)
		fatal_with_errno(EXIT_FAILURE, "tcgetattr(%s)", device_path);
	tio.c_lflag = ICANON;
	tio.c_iflag |= IGNCR;	/* Ignore CR */
	tio.c_cc[VMIN] = 0;
	tio.c_cc[VTIME] = 0;
	tcsetattr(upsfd, TCSANOW, &tio);

	if ((str = getval("input_timeout")) != NULL) {
		int temp = atoi(str);
		if (temp <= 0)
			fatalx(EXIT_FAILURE, "Bad input_timeout parameter: %s", str);
		input_timeout_sec = temp;
	}
	upsdebugx(1, "input_timeout = %d Sec", input_timeout_sec);

	if ((str = getval("output_pace")) != NULL) {
		int temp = atoi(str);
		if (temp <= 0)
			fatalx(EXIT_FAILURE, "Bad output_pace parameter: %s", str);
		output_pace_usec = temp;
	}
	upsdebugx(1, "output_pace = %d uSec", output_pace_usec);

	if ((str = getval("full_update_timer")) != NULL) {
		int temp = atoi(str);
		if (temp <= 0)
			fatalx(EXIT_FAILURE, "Bad full_update_timer parameter: %s", str);
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
	if (!upsc_commandlist()) {
		upslogx(LOG_ERR, "No contact with UPS, delaying init.");
		status = UPSC_STAT_NOTINIT;
		return;
	} else {
		status = 0;
	}

	upsc_getbaseinfo();
	if (can_upda) {
		upsc_flush_input();
		upscsend("UPDA");
	}
	if (can_upid) {
		upsc_getvalue("UPID", NULL, "ACID", "ups.id", NULL);
	}
	if (can_uppm) {
		check_uppm();
	}

	/* make sure we have some sensible defaults */
	setvar("ups.delay.shutdown", "10");
	setvar("ups.delay.reboot", "60");

	upsh.instcmd = instcmd;
	upsh.setvar = setvar;
}


/* Change a variable name in a table */
static void change_name(simple_t *sp,
		const char *oldname, const char *newname)
{
	while(sp->code) {
		if (sp->desc && !strcmp(sp->desc, oldname)) {
			sp->desc = strdup(newname);
			if (dstate_getinfo(oldname)) {
				dstate_setinfo(newname, "%s",
						dstate_getinfo(oldname));
			}
			dstate_delinfo(oldname);
			upsdebugx(1, "Changing name: %s => %s", oldname, newname);
			break;
		}
		sp++;
	}
}

static float calc_upsload(void) {

	float 	load=-1, nom_out_power=-1, nom_out_realpower=-1, maxcurr, tmp;
	const char	*s;

	/* Some UPSen (Fiskars 9000 for example) only reports current, and
	 * only the max current */
	if (nom_out_current > 0) {
		maxcurr = nom_out_current;
	}
	else {
		maxcurr = max_out_current;
	}

	if (maxcurr > 0) {
		if ((s=dstate_getinfo("output.L1.current")) ||
				(s=dstate_getinfo("output.current"))) {
			if (sscanf(s, "%f", &tmp) == 1) {
				load = tmp/maxcurr;
			}
		}
		if ((s=dstate_getinfo("output.L2.current"))) {
			if (sscanf(s, "%f", &tmp) == 1) {
				tmp=tmp/maxcurr;
				if (tmp>load) {
					load = tmp;
				}
			}
		}
		if ((s=dstate_getinfo("output.L3.current"))) {
			if (sscanf(s, "%f", &tmp) == 1) {
				tmp=tmp/maxcurr;
				if (tmp>load) {
					load = tmp;
				}
			}
		}
	}

	/* This is aggregated (all phases) */
	if ((s=dstate_getinfo("ups.power.nominal"))) {
		if (sscanf(s, "%f", &nom_out_power) != 1) {
			nom_out_power = -1;
		}
	}

	if (nom_out_power > 0) {
		if ((s=dstate_getinfo("output.L1.power"))) {
			if (sscanf(s, "%f", &tmp) == 1) {
				tmp /= (nom_out_power/num_outphases);
				if (tmp>load) {
					load = tmp;
				}
				dstate_setinfo("output.L1.power.percent",
						"%.1f", tmp*100);
			}
		}
		if ((s=dstate_getinfo("output.L2.power"))) {
			if (sscanf(s, "%f", &tmp) == 1) {
				tmp /= (nom_out_power/num_outphases);
				if (tmp>load) {
					load = tmp;
				}
				dstate_setinfo("output.L2.power.percent",
						"%.1f", tmp*100);
			}
		}
		if ((s=dstate_getinfo("output.L3.power"))) {
			if (sscanf(s, "%f", &tmp) == 1) {
				tmp /= (nom_out_power/num_outphases);
				if (tmp>load) {
					load = tmp;
				}
				dstate_setinfo("output.L3.power.percent",
						"%.1f", tmp*100);
			}
		}
	}

	/* This is aggregated (all phases) */
	if ((s=dstate_getinfo("output.realpower.nominal"))) {
		if (sscanf(s, "%f", &nom_out_realpower) != 1) {
			nom_out_realpower = -1;
		}
	}
	if (nom_out_realpower >= 0) {
		if ((s=dstate_getinfo("output.L1.realpower"))) {
			if (sscanf(s, "%f", &tmp) == 1) {
				tmp /= (nom_out_realpower/num_outphases);
				if (tmp>load) {
					load = tmp;
				}
				dstate_setinfo("output.L1.realpower.percent",
						"%.1f", tmp*100);
			}
		}
		if ((s=dstate_getinfo("output.L2.realpower"))) {
			if (sscanf(s, "%f", &tmp) == 1) {
				tmp /= (nom_out_realpower/num_outphases);
				if (tmp>load) {
					load = tmp;
				}
				dstate_setinfo("output.L2.realpower.percent",
						"%.1f", tmp*100);
			}
		}
		if ((s=dstate_getinfo("output.L3.realpower"))) {
			if (sscanf(s, "%f", &tmp) == 1) {
				tmp /= (nom_out_realpower/num_outphases);
				if (tmp>load) {
					load = tmp;
				}
				dstate_setinfo("output.L3.realpower.percent",
						"%.1f", tmp*100);
			}
		}
	}

	return load;
}


void upsdrv_updateinfo(void)
{
	time_t now;
	int ok;
	float load;

	if (status & UPSC_STAT_NOTINIT) {
		upsdrv_initinfo();
	}

	if (status & UPSC_STAT_NOTINIT) {
		return;
	}

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

	if (!inited_phaseinfo) {
		if (dstate_getinfo("input.L3-L1.voltage") ||
				dstate_getinfo("input.L3-N.voltage")) {
			num_inphases = 3;

			change_name(simple,
				"input.current", "input.L1.current");
			change_name(simple,
				"input.realpower", "input.L1.realpower");
			change_name(simple,
				"input.power", "input.L1.power");
			change_name(simple,
				"input.voltage", "input.L1-N.voltage");
		}
		if (dstate_getinfo("output.L3-L1.voltage") ||
				dstate_getinfo("output.L3-N.voltage")) {
			const char *s;

			num_outphases = 3;

			if ((s=dstate_getinfo("ups.model")) &&
					(!strncmp(s, "UPS9075", 7) ||
					!strncmp(s, "UPS9100", 7) ||
					!strncmp(s, "UPS9150", 7) ||
					!strncmp(s, "UPS9200", 7) ||
					!strncmp(s, "UPS9250", 7) ||
					!strncmp(s, "UPS9300", 7) ||
					!strncmp(s, "UPS9400", 7) ||
					!strncmp(s, "UPS9500", 7) ||
					!strncmp(s, "UPS9600", 7)) ) {
				/* Insert kludges for Fiskars UPS9000 here */
				upslogx(LOG_INFO, "Fiskars UPS9000 detected, protocol kludges activated");
				batt_volt_nom = 384;
				dstate_setinfo("battery.voltage.nominal", "%.0f", batt_volt_nom);

			}
			else {
				outpwr_factor *= 3;
			}

			change_name(simple,
				"output.current", "output.L1.current");
			change_name(simple,
				"output.current.peak", "output.L1.current.peak");
			change_name(simple,
				"output.realpower", "output.L1.realpower");
			change_name(simple,
				"output.power", "output.L1.power");
			change_name(simple,
				"output.voltage", "output.L1-N.voltage");
		}

		dstate_setinfo("input.phases", "%d", num_inphases);
		dstate_setinfo("output.phases", "%d", num_outphases);

		inited_phaseinfo=1;
	}

	load = calc_upsload();

	if (load >= 0) {
		upsdebugx(2, "ups.load: %.1f", load*100);
		dstate_setinfo("ups.load", "%.1f", load*100);
	}
	else {
		upsdebugx(2, "ups.load: No value");
	}

	/* TODO/FIXME: Set UPS date/time on startup and daily if needed */
	if (can_updt) {
		char dtbuf[UPSC_BUFLEN];
		if (upsc_getvalue("UPDT", "0", "ACDT", NULL, dtbuf)) {
			dstate_setinfo("ups.date", "%s", dtbuf);
		}
	}
	if (can_uptm) {
		char tmbuf[UPSC_BUFLEN];
		if (upsc_getvalue("UPTM", "0", "ACTM", NULL, tmbuf)) {
			dstate_setinfo("ups.time", "%s", tmbuf);
		}
	}


	if (batt_charge < 0) {
		if (batt_current < 0) {
			/* Reset battery current history if discharging */
			numbatthist = lastbatthist = 0;
		}
		batt_charge = batt_charge_pct();
	}
	if (batt_charge >= 0) {
		dstate_setinfo("battery.charge", "%.1f", batt_charge);
	}
	else {
		dstate_delinfo("battery.charge");
	}

	/* 9999 == unknown value */
	if (batt_runtime >= 0 && batt_runtime < 9999) {
		dstate_setinfo("battery.runtime", "%.0f", batt_runtime*60);
	}
	else if (load > 0 && batt_disch_curr_max != 0) {
		float est_battcurr = load * fabs(batt_disch_curr_max);
		/* Peukert equation */
		float runtime = (batt_cap_nom*3600)/pow(est_battcurr, 1.35);

		upsdebugx(2, "Calculated runtime: %.0f seconds", runtime);
		if (batt_runtime_max > 0 && runtime > batt_runtime_max*60) {
			runtime = batt_runtime_max*60;
		}
		dstate_setinfo("battery.runtime", "%.0f", runtime);

	}
	else if (batt_runtime_max > 0) {
		/* Show max possible runtime as reported by UPS */
		dstate_setinfo("battery.runtime", "%.0f", batt_runtime_max*60);
	}
	else {
		dstate_delinfo("battery.runtime");
	}
	/* Some UPSen only provides this when on battery, so reset between
	 * each iteration to make sure we use the right value */
	batt_charge = -1;
	batt_runtime = -1;


	if (!(status & UPSC_STAT_ONBATT))
		status |= UPSC_STAT_ONLINE;

	upsc_setstatus(status);

	dstate_dataok();
	ser_comm_good();
}


void upsdrv_shutdown(void)
{
	upslogx(LOG_EMERG, "Shutting down...");

	/* send shutdown command twice, just to be sure */
	instcmd("shutdown.reboot", NULL);
	sleep(1);
	instcmd("shutdown.reboot", NULL);
	sleep(1);
}


static int instcmd (const char *auxcmd, const char *data)
{
	cmd_t *cp;

	if (!strcasecmp(auxcmd, "beeper.off")) {
		/* compatibility mode for old command */
		upslogx(LOG_WARNING,
			"The 'beeper.off' command has been renamed to 'beeper.disable'");
		return instcmd("beeper.disable", NULL);
	}

	if (!strcasecmp(auxcmd, "beeper.on")) {
		/* compatibility mode for old command */
		upslogx(LOG_WARNING,
			"The 'beeper.on' command has been renamed to 'beeper.enable'");
		return instcmd("beeper.enable", NULL);
	}

	upsdebugx(1, "Instcmd: %s %s", auxcmd, data ? data : "\"\"");

	for (cp = commands; cp->cmd; cp++) {
		if (strcasecmp(cp->cmd, auxcmd)) {
			continue;
		}
		upscsend(cp->upsc);
		if (cp->upsp) {
			upscsend(cp->upsp);
		} else if (data) {
			upscsend(data);
		}
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_INFO, "instcmd: unknown command %s", auxcmd);
	return STAT_INSTCMD_UNKNOWN;
}


static int setvar (const char *var, const char *data)
{
	cmd_t *cp;

	upsdebugx(1, "Setvar: %s %s", var, data);

	for (cp = variables; cp->cmd; cp++) {
		if (strcasecmp(cp->cmd, var)) {
			continue;
		}
		upsc_getvalue(cp->upsc, data, cp->upsp, cp->cmd, NULL);
		return STAT_SET_HANDLED;
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
 * Generate status string from bitfield
 */
static void upsc_setstatus(unsigned int status)
{

	/*
	 * I'll look for all available statuses, even though they might not be
	 *  supported in the UPScode II protocol.
	 */

	status_init();

	if (status & UPSC_STAT_ONLINE)
		status_set("OL");
	if (status & UPSC_STAT_ONBATT)
		status_set("OB");
	if (status & UPSC_STAT_LOBATT)
		status_set("LB");
	if (status & UPSC_STAT_REPLACEBATT)
		status_set("RB");
	if (status & UPSC_STAT_BOOST)
		status_set("BOOST");
	if (status & UPSC_STAT_TRIM)
		status_set("TRIM");
	if (status & UPSC_STAT_OVERLOAD)
		status_set("OVER");
	if (status & UPSC_STAT_CALIBRATION)
		status_set("CAL");
	if (status & UPSC_STAT_OFF)
		status_set("OFF");
	if (status & UPSC_STAT_BYPASS)
		status_set("BYPASS");

	status_commit();
}


/* Add \r to end of command and send to UPS */
/* returns < 0 on errors, 0 on timeout and > 0 on success. */
static int upscsend(const char *cmd)
{
	int	res;

	res = ser_send_pace(upsfd, output_pace_usec, "%s%s%s",
		use_pre_lf ? "\n" : "",
		cmd,
		use_crlf ? "\r\n" : "\r");

	if (res < 0) {
		upsdebug_with_errno(3, "upscsend");
	} else if (res == 0) {
		upsdebugx(3, "upscsend: Timeout");
	} else {
		upsdebugx(3, "upscsend: '%s'", cmd);
	}

	return res;
}


/* Return a string read from UPS */
/* returns < 0 on errors, 0 on timeout and > 0 on success. */
static int upscrecv(char *buf)
{
	int	res;

	/* NOTE: the serial port is set to use Canonical Mode Input Processing,
	   which means ser_get_buf() either returns one line terminated with
	   ENDCHAR, an error or times out. */

	while (1) {
		res = ser_get_buf(upsfd, buf, UPSC_BUFLEN, input_timeout_sec, 0);
		if (res != 1) {
			break;
		}

		/* Only one character, must be ENDCHAR */
		upsdebugx(3, "upscrecv: Empty line");
	}

	if (res < 0) {
		upsdebug_with_errno(3, "upscrecv");
	} else if (res == 0) {
		upsdebugx(3, "upscrecv: Timeout");
	} else {
		upsdebugx(3, "upscrecv: %u bytes:\t'%s'", res-1, str_rtrim(buf, ENDCHAR));
	}

	return res;
}


static void upsc_flush_input(void)
{
/*
	char buf[UPSC_BUFLEN];

	do {
		upscrecv(buf);
		if (strlen(buf) > 0)
			upsdebugx(1, "Skipping input: %s", buf);
	} while (strlen(buf) > 0);
*/
	ser_flush_in(upsfd, "", nut_debug_level);
}


/* check which commands this ups supports.
 * Returns TRUE if command list was recieved, FALSE otherwise */
static int upsc_commandlist(void)
{
	char buf[UPSC_BUFLEN];
	cmd_t *cp;

	upsc_flush_input();
	upscsend("UPCL");
	while (1) {
		upscrecv(buf);
		if (strlen(buf) == 0) {
			upslogx(LOG_ERR, "Missing UPCL after UPCL");
			return 0;
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
		else if (strcmp(buf, "UPDT") == 0)
			can_updt = 1;
		else if (strcmp(buf, "UPTM") == 0)
			can_uptm = 1;
		else if (strcmp(buf, "UPSD") == 0)
			can_upsd = 1;
		else if (strcmp(buf, "UPPC") == 0)
			can_uppc = 1;

		for (cp = commands; cp->cmd; cp++) {
			if (cp->upsc && strcmp(cp->upsc, buf) == 0) {
				upsdebugx(1, "instcmd: %s %s", cp->cmd, cp->upsc);
				dstate_addcmd(cp->cmd);
				cp->enabled = 1;
	            break;
			}
		}

		for (cp = variables; cp->cmd; cp++) {
			if (cp->upsc && strcmp(cp->upsc, buf) == 0) {
				upsdebugx(1, "setvar: %s %s", cp->cmd, cp->upsc);
				cp->enabled = 1;
				break;
			}
		}

		if (strcmp(buf, "UPCL") == 0)
			break;
	}

	for (cp = variables; cp->cmd; cp++) {
		if (cp->enabled) {
			upsc_getvalue(cp->upsc, "0000", cp->upsp, cp->cmd, NULL);
			dstate_setflags(cp->cmd, ST_FLAG_RW | ST_FLAG_STRING);
			dstate_setaux(cp->cmd, 7);
		}
	}

	return 1;
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
			const char *resp, const char *nutvar, char *ret)
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
		if (nutvar)
			dstate_setinfo(nutvar, "%s", val);
		if (ret)
			strcpy(ret, val);
	}
	return 1;
}


static void upsc_getbaseinfo(void)
{
	char *buf;

	dstate_setinfo("ups.mfr", "%s",
		((buf = getval("manufacturer")) != NULL) ? buf : "unknown");

	if (!upsc_getvalue("UPTP", NULL, "NNAME", "ups.model", NULL))
		upsc_getvalue("UPTP", NULL, "NNAME", "ups.model", NULL);
	upsc_getvalue("UPSN", "0", "ACSN", "ups.serial", NULL);
}


static int upsc_simple(const simple_t *sp, const char *var, const char *val)
{
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
					dstate_setinfo(sp->desc, "%.2f", fval);
				*sp->aux = fval;
				break;
			case t_setrecip:
				stat = sscanf(val, "%f", &fval);
				if (stat != 1)
					upslogx(LOG_ERR, "Bad float: %s %s", var, val);
				if (sp->desc)
					dstate_setinfo(sp->desc, "%s", val);
				*sp->aux = 1/fval;
				break;
			case t_setpct:
				stat = sscanf(val, "%f", &fval);
				if (stat != 1)
					upslogx(LOG_ERR, "Bad float: %s %s", var, val);
				*sp->aux = fval*100;
				if (sp->desc)
					dstate_setinfo(sp->desc, "%s", val);
				break;
			case t_setrecpct:
				stat = sscanf(val, "%f", &fval);
				if (stat != 1)
					upslogx(LOG_ERR, "Bad float: %s %s", var, val);
				*sp->aux = 1/fval*100;
				if (sp->desc)
					dstate_setinfo(sp->desc, "%s", val);
				break;
			case t_final:
				buffer_empty = 1;
			case t_value:
				if (!sp->desc) {
					break;
				}
				if (sscanf(val, "%f", &fval) == 1) {
					if (sp->aux != NULL) {
						fval *= *(sp->aux);
					}
					dstate_setinfo(sp->desc, "%.2f", fval);
				}
				else {
					upslogx(LOG_ERR, "Bad float in %s: %s", var, val);
					dstate_setinfo(sp->desc, "%s", val);
				}
				break;
			case t_finstr:
				buffer_empty = 1;
			case t_string:
				if (!sp->desc) {
					break;
				}
				dstate_setinfo(sp->desc, "%s", val);
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
				upsdebugx(3, "Ignored value: %s %s", var, val);
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
	float chg=-1;

	/* This is only a rough estimate of charge status while charging.
	 * When on battery something like Peukert's equation should be used */
	if (batt_current >= 0) {
		float maxcurr = 10;	/* Assume 10A max as default */
		float avgcurr = 0;
		int i;

		batthist[lastbatthist] = batt_current;
		lastbatthist = (lastbatthist+1) % NUM_BATTHIST;
		if (numbatthist < NUM_BATTHIST) {
			numbatthist++;
		}
		for(i=0; i<numbatthist; i++) {
			avgcurr += batthist[i];
		}
		avgcurr /= numbatthist;

		if (batt_cap_nom > 0) {
			/* Estimate max charge current based on battery size */
			maxcurr = batt_cap_nom * 0.3;
		}
		chg = maxcurr - avgcurr;
		chg *= (100/maxcurr);
	}
	/* Old method, assumes battery high/low-voltages provided by UPS are
	 * applicable to battery charge, but they usually aren't */
	else if (batt_volt_low > 0 && batt_volt_high > 0 && batt_volt > 0) {
		if (batt_volt > batt_volt_high) {
			chg=100;
		}
		else if (batt_volt < batt_volt_low) {
			chg=0;
		}
		else {
			chg = (batt_volt - batt_volt_low) /
				(batt_volt_high - batt_volt_low);
			chg*=100;
		}
	}
	else {
		return -1;
	}

	if (chg < 0) {
		chg = 0;
	}
	else if (chg > 100) {
		chg = 100;
	}

	return chg;
}

/*
vim:noet:ts=8:sw=8:cindent
*/
