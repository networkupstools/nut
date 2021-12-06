/* nutdrv_qx_masterguard.c - Subdriver for Masterguard A/E Series
 *
 * Copyright (C)
 *   2020-2021 Edgar Fuß <ef@math.uni-bonn.de>, Mathematisches Institut der Universität Bonn
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version, or a 2-clause BSD License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "main.h"
#include "nutdrv_qx.h"

#include "nutdrv_qx_masterguard.h"
#include <stddef.h>

#define MASTERGUARD_VERSION "Masterguard 0.01"

/* series (for un-SKIP) */
static char masterguard_my_series = '?';
/* slave address for commands that require it */
static char masterguard_my_slaveaddr[3] = "??"; /* null-terminated for strtol() in claim() */
/* next slaveaddr to use after the SS command (which needs the old one) has been run */
static long masterguard_next_slaveaddr;
/* output current/power computation */
static long masterguard_my_power = 0;
/* battery voltage computation */
static long masterguard_my_numcells = 0;


/* ranges */

static info_rw_t masterguard_r_slaveaddr[] = {
	{ "0", NULL },
	{ "99", NULL },
	{ "" , NULL }
};

static info_rw_t masterguard_r_batpacks[] = {
	{ "0", NULL },	/* actually 1 for most models, see masterguard_model() */
	{ "9", NULL },  /* varies across models, see masterguard_model() */
	{ "" , NULL }
};

static info_rw_t masterguard_r_offdelay[] = {
	{ "0", NULL },
	{ "5940", NULL },  /* 99*60 */
	{ "" , NULL }
};

static info_rw_t masterguard_r_ondelay[] = {
	{ "0", NULL },
	{ "599940", NULL },  /* 9999*60 */
	{ "" , NULL }
};


/* enums */

static info_rw_t *masterguard_e_outvolts = NULL; /* set in masterguard_output_voltages() */


/* preprocess functions */

/* set masterguard_my_slaveaddr (for masterguard_add_slaveaddr */
static int masterguard_slaveaddr(item_t *item, char *value, const size_t valuelen) {
	if (strlen(item->value) != 2) {
		upsdebugx(2, "slaveaddr length not 2");
		return -1;
	}
	memcpy(masterguard_my_slaveaddr, item->value, 2);
	if (valuelen >= 3) memcpy(value, item->value, 3);
	return 0;
}

/* set masterguard_my_series (for activating supported commands in masterguard_claim() */
static int masterguard_series(item_t *item, char *value, const size_t valuelen) {
	NUT_UNUSED_VARIABLE(valuelen);

	switch (item->value[0]) {
		case 'A':
			break;
		case 'E':
			break;
		default:
			upsdebugx(2, "unknown series %s", item->value);
			return -1;
	}
	masterguard_my_series = item->value[0];
	memcpy(value, item->value, 2);
	return 0;
}

/* convert strangely formatted model name in WH output (spaces, -19 only after battery packs) to something readable */
/* also set min/max battery packs according to model */
static int masterguard_model(item_t *item, char *value, const size_t valuelen) {
	char *model;
	int rack;
	char min_bp, max_bp;

	rack = (strstr(item->value, "-19") != NULL);
	if (strncmp(item->value, "A  700", 6) == 0) {
		model = "A700";
		min_bp = 0; max_bp = 0;
	} else if (strncmp(item->value, "A 1000", 6) == 0) {
		model = "A1000";
		min_bp = 0; max_bp = 2;
	} else if (strncmp(item->value, "A 2000", 6) == 0) {
		model = "A2000";
		min_bp = rack ? 1 : 0; max_bp = rack ? 5 : 2;
	} else if (strncmp(item->value, "A 3000", 6) == 0) {
		model = "A3000";
		min_bp = rack ? 1 : 0; max_bp = rack ? 5 : 2;
	} else if (strncmp(item->value, "E 60", 4) == 0) {
		model = "E60";
		min_bp = 1; max_bp = 1; /* ??? */
	} else if (strncmp(item->value, "E100", 4) == 0) {
		model = "E100";
		min_bp = 1; max_bp = 1; /* ??? */
	} else if (strncmp(item->value, "E200", 4) == 0) {
		model = "E200";
		min_bp = 1; max_bp = 1; /* ??? */
	} else {
		upsdebugx(2, "unknown T %s", item->value);
		return -1;
	}
	masterguard_r_batpacks[0].value[0] = '0' + min_bp;
	masterguard_r_batpacks[1].value[0] = '0' + max_bp;
	snprintf(value, valuelen, "%s%s", model, rack ? "-19" : "");
	return 0;
}

/* set masterguard_my_power (for power/current calculations) according to model */
static int masterguard_power(item_t *item, char *value, const size_t valuelen) {
	int p;

	if (strncmp(item->value, "A  700", 6) == 0) {
		p = 700;
	} else if (strncmp(item->value, "A 1000", 6) == 0) {
		p = 1000;
	} else if (strncmp(item->value, "A 2000", 6) == 0) {
		p = 2000;
	} else if (strncmp(item->value, "A 3000", 6) == 0) {
		p = 3000;
	} else if (strncmp(item->value, "E 60", 4) == 0) {
		p = 6000;
	} else if (strncmp(item->value, "E100", 4) == 0) {
		p = 10000;
	} else if (strncmp(item->value, "E200", 4) == 0) {
		p = 20000;
	} else {
		upsdebugx(2, "unknown T %s", item->value);
		return -1;
	}
	masterguard_my_power = p;
	snprintf(value, valuelen, "%d", p);
	return 0;
}

/* convert mmm.ss to seconds */
static int masterguard_mmm_ss(item_t *item, char *value, const size_t valuelen) {
	int m, s;

	if (sscanf(item->value, "%d.%d", &m, &s) != 2) {
		upsdebugx(2, "unparsable mmm.ss %s", item->value);
		return -1;
	}
	snprintf(value, valuelen, "%d", 60*m + s);
	return 0;
}

/* convert hhh to seconds */
static int masterguard_hhh(item_t *item, char *value, const size_t valuelen) {
	int h;
	if (sscanf(item->value, "%d", &h) != 1) {
		upsdebugx(2, "unparsable hhh %s", item->value);
		return -1;
	}
	snprintf(value, valuelen, "%d", 60*60*h);
	return 0;
}

/* convert TTTT:hh:mm:dd to seconds */
static int masterguard_tttt_hh_mm_ss(item_t *item, char *value, const size_t valuelen) {
	int t, h, m, s;

	if (sscanf(item->value, "%d:%d:%d:%d", &t, &h, &m, &s) != 4) {
		upsdebugx(2, "unparsable TTTT:hh:mm:ss %s", item->value);
		return -1;
	}
	snprintf(value, valuelen, "%d", 86400*t + 3600*h + 60*m + s);
	return 0;
}

/* set masterguard_my_numcells (for nominal battery voltage computation) */
static int masterguard_numcells(item_t *item, char *value, const size_t valuelen) {
	int v;
	if (sscanf(item->value, "%d", &v) != 1) {
		upsdebugx(2, "unparsable vvv %s", item->value);
		return -1;
	}
	masterguard_my_numcells = v;
	snprintf(value, valuelen, "%d", v);
	return 0;
}

/* compute nominal battery voltage */
static int masterguard_battvolt(item_t *item, char *value, const size_t valuelen) {
	float s;
	if (sscanf(item->value, "%f", &s) != 1) {
		upsdebugx(2, "unparsable ss.ss %s", item->value);
		return -1;
	}
	snprintf(value, valuelen, "%.2f", masterguard_my_numcells * s);
	return 0;
}

/* compute output power from load percentage */
static int masterguard_ups_power(item_t *item, char *value, const size_t valuelen) {
	int q;

	if (sscanf(item->value, "%d", &q) != 1) {
		upsdebugx(2, "unparsable qqq %s", item->value);
		return -1;
	}
	snprintf(value, valuelen, "%.0f", q / 100.0 * masterguard_my_power + 0.5);
	return 0;
}

/* helper routine, not to be called from table */
static int masterguard_output_current_fraction(item_t *item, char *value, const size_t valuelen, double fraction) {
	NUT_UNUSED_VARIABLE(item);

	snprintf(value, valuelen, "%.2f",
		fraction * masterguard_my_power / strtod(dstate_getinfo("output.voltage") , NULL) + 0.005);
	return 0;
}

/* compute output current from load percentage and output voltage */
static int masterguard_output_current(item_t *item, char *value, const size_t valuelen) {
	int q;

	if (sscanf(item->value, "%d", &q) != 1) {
		upsdebugx(2, "unparsable qqq %s", item->value);
		return -1;
	}
	return masterguard_output_current_fraction(item, value, valuelen, q/100.0);
}

/* compute nominal output current from output voltage */
static int masterguard_output_current_nominal(item_t *item, char *value, const size_t valuelen) {
	return masterguard_output_current_fraction(item, value, valuelen, 1.0);
}

/* digest status bits */
static int masterguard_status(item_t *item, char *value, const size_t valuelen) {
	int neg;
	char *s;

	switch (item->value[0]) {
		case '0': neg = 1; break;
		case '1': neg = 0; break;
		default:
			upsdebugx(2, "unknown flag value %c", item->value[0]);
			return -1;
	}
	switch (item->from) {
		case 53: /* B7 */ s = "OL"; neg = !neg; break;
		case 54: /* B6 */ s = "LB"; break;
		case 55: /* B5 */ s = "BYPASS"; break;
		case 56: /* B4 */ s = neg ? "" : "UPS Failed"; neg = 0; break;
		case 57: /* B3 */ s = neg ? "online" : "offline"; neg = 0; break;
		case 58: /* B2 */ s = "CAL"; break;
		case 59: /* B1 */ s = "FSD"; break;
	/*	     60:    blank */
	/*	     61:    B0 reserved */
	/*	     62:    T7 reserved */
		case 63: /* T6 */ s = neg ? "" : "problems in parallel operation mode"; neg = 0; break;
	/*	     64:    T5 part of a parallel set */
		case 65: /* T4 */ s = "RB"; break;
		case 66: /* T3 */ s = neg ? "" : "no battery connected"; neg = 0; break;
		case 67: /* T210 */
			neg = 0;
			if (strncmp(item->value, "000", 3) == 0) {
				s = "no test in progress";
			} else if (strncmp(item->value, "001", 3) == 0) {
				s = "in progress";
			} else if (strncmp(item->value, "010", 3) == 0) {
				s = "OK";
			} else if (strncmp(item->value, "011", 3) == 0) {
				s = "failed";
			} else if (strncmp(item->value, "100", 3) == 0) {
				s = "not possible";
			} else if (strncmp(item->value, "101", 3) == 0) {
				s = "aborted";
			} else if (strncmp(item->value, "110", 3) == 0) {
				s = "autonomy time calibration in progress";
			} else if (strncmp(item->value, "111", 3) == 0) {
				s = "unknown";
			} else {
				upsdebugx(2, "unknown test result %s", item->value);
				return -1;
			}
			break;
		default:
			upsdebugx(2, "unknown flag position %d", item->from);
			return -1;
	}
	snprintf(value, valuelen, "%s%s", neg ? "!" : "", s);
	return 0;
}

/* convert beeper status bit to string required by NUT */
static int masterguard_beeper_status(item_t *item, char *value, const size_t valuelen) {
	switch (item->value[0]) {
		case '0':
			if (valuelen >= 9)
				strcpy(value, "disabled");
			else
				*value = '\0';
			break;
		case '1':
			if (valuelen >= 8)
				strcpy(value, "enabled");
			else
				*value = '\0';
			break;
		default:
			upsdebugx(2, "unknown beeper status %c", item->value[0]);
			return -1;
	}
	return 0;
}

/* parse list of available (nominal) output voltages into masterguard_w_outvolts enum */
static int masterguard_output_voltages(item_t *item, char *value, const size_t valuelen) {
	char sep[] = " ";
	char *w;
	size_t n = 0;

	strncpy(value, item->value, valuelen); /* save before strtok mangles it */
	for (w = strtok(item->value, sep); w; w = strtok(NULL, sep)) {
		n++;
		upsdebugx(4, "output voltage #%zu: %s", n, w);
		if ((masterguard_e_outvolts = realloc(masterguard_e_outvolts, n * sizeof(info_rw_t))) == NULL) {
			upsdebugx(1, "output voltages: allocating #%zu failed", n);
			return -1;
		}
		strncpy(masterguard_e_outvolts[n - 1].value, w, SMALLBUF - 1);
		masterguard_e_outvolts[n - 1].preprocess = NULL;
	}
	/* need to do this seperately in case the loop is run zero times */
	if ((masterguard_e_outvolts = realloc(masterguard_e_outvolts, (n + 1) * sizeof(info_rw_t))) == NULL) {
		upsdebugx(1, "output voltages: allocating terminator after #%zu failed", n);
		return -1;
	}
	masterguard_e_outvolts[n].value[0] = '\0';
	masterguard_e_outvolts[n].preprocess = NULL;
	return 0;
}

/* parse fault record string into readable form */
static int masterguard_fault(item_t *item, char *value, const size_t valuelen) {
	char c;
	float f;
	int t, h, m, s;
	long l;

	if (sscanf(item->value, "%c %f %d:%d:%d:%d", &c, &f, &t, &h, &m, &s) != 6) {
		upsdebugx(1, "unparsable fault record %s", item->value);
		return -1;
	}
	l = 86400*t + 3600*h + 60*m + s;
	snprintf(value, valuelen, "%ld: ", l);
	switch (c) {
		case '0':
			snprintfcat(value, valuelen, "none");
			break;
		case '1':
			snprintfcat(value, valuelen, "bus fault (%.0fV)", f);
			break;
		case '2':
			snprintfcat(value, valuelen, "inverter fault (%.0fV)", f);
			break;
		case '3':
			snprintfcat(value, valuelen, "overheat fault (%.0fC)", f);
			break;
		case '4':
			snprintfcat(value, valuelen, "battery overvoltage fault (%.2fV)", f);
			break;
		case '5':
			snprintfcat(value, valuelen, "battery mode overload fault (%.0f%%)", f);
			break;
		case '6':
			snprintfcat(value, valuelen, "bypass mode overload fault (%.0f%%)", f);
			break;
		case '7':
			snprintfcat(value, valuelen, "inverter mode outpt short-circuit fault (%.0fV)", f);
			break;
		case '8':
			snprintfcat(value, valuelen, "fan lock fault");
			break;
		case '9':
			snprintfcat(value, valuelen, "battery fault (%.0fV)", f);
			break;
		case 'A':
			snprintfcat(value, valuelen, "charger fault");
			break;
		case 'B':
			snprintfcat(value, valuelen, "EPO activated");
			break;
		case 'C':
			snprintfcat(value, valuelen, "parallel error");
			break;
		case 'D':
			snprintfcat(value, valuelen, "MCU communication error");
			break;
		case 'E':
		case 'F':
			upsdebugx(1, "reserved fault id %c", c);
			return -1;
		default: 
			upsdebugx(1, "unknown fault id %c", c);
			return -1;
	}
	return 0;
}


/* pre-command preprocessing functions */

/* add slave address (from masterguard_my_slaveaddr) to commands that require it */
static int masterguard_add_slaveaddr(item_t *item, char *command, const size_t commandlen) {
	NUT_UNUSED_VARIABLE(item);
	NUT_UNUSED_VARIABLE(commandlen);

	size_t l;

	l = strlen(command);
	if (strncmp(command + l - 4, ",XX\r", 4) != 0) {
		upsdebugx(1, "add slaveaddr: no ,XX\\r at end of command %s", command);
		return -1;
	}
	upsdebugx(4, "add slaveaddr %s to command %s", masterguard_my_slaveaddr, command);
	memcpy(command + l - 3, masterguard_my_slaveaddr, 2);
	return 0;
}


/* instant command preprocessing functions */

/* helper, not to be called directly from table */
/*!! use parameter from the value field instead of ups.delay.{shutdown,return}?? */
static int masterguard_shutdown(item_t *item, char *value, const size_t valuelen, const int stayoff) {
	NUT_UNUSED_VARIABLE(item);

	int offdelay;
	char *p;
	const char *val, *name;
	char offstr[3];

	offdelay = strtol((val = dstate_getinfo(name = "ups.delay.shutdown")), &p, 10);
	if (*p != '\0') goto ill;
	if (offdelay < 0) {
		goto ill;
	} else if (offdelay < 60) {
		offstr[0] = '.'; offstr[1] = '0' + offdelay / 6;
	} else if (offdelay <= 99*60) {
		int m = offdelay / 60;
		offstr[0] = '0' + m / 10; offstr[1] = '0' + m % 10;
	} else goto ill;
	offstr[2] = '\0';
	if (stayoff) {
		snprintf(value, valuelen, "S%s\r", offstr);
	} else {
		int ondelay;

		ondelay = strtol((val = dstate_getinfo(name = "ups.delay.return")), &p, 10);
		if (*p != '\0') goto ill;
		if (ondelay < 0 || ondelay > 9999*60) goto ill;
		snprintf(value, valuelen, "S%sR%04d\r", offstr, ondelay);
	}
	return 0;

ill:	upsdebugx(2, "shutdown: illegal %s %s", name, val);
	return -1;
}

static int masterguard_shutdown_return(item_t *item, char *value, const size_t valuelen) {
	return masterguard_shutdown(item, value, valuelen, 0);
}

static int masterguard_shutdown_stayoff(item_t *item, char *value, const size_t valuelen) {
	return masterguard_shutdown(item, value, valuelen, 1);
}

static int masterguard_test_battery(item_t *item, char *value, const size_t valuelen) {
	NUT_UNUSED_VARIABLE(item);

	int duration;
	char *p;

	if (value[0] == '\0') {
		upsdebugx(2, "battery test: no duration");
		return -1;
	}
	duration = strtol(value, &p, 10);
	if (*p != '\0') goto ill;
	if (duration == 10) {
		strncpy(value, "T\r", valuelen);
		return 0;
	}
	if (duration < 60 || duration > 99*60) goto ill;
	snprintf(value, valuelen, "T%02d\r", duration / 60);
	return 0;

ill:	upsdebugx(2, "battery test: illegal duration %s", value);
	return -1;
}


/* variable setting preprocessing functions */

/* set variable, input format specifier (d/f/s, thms) in item->dfl */
static int masterguard_setvar(item_t *item, char *value, const size_t valuelen) {
	char *p;
	char t = 's';
	long i = 0;
	double f = 0.0;
	char s[80];

	if (value[0] == '\0') {
		upsdebugx(2, "setvar: no value");
		return -1;
	}
	if (!item->dfl || item->dfl[0] == '\0') {
		upsdebugx(2, "setvar: no dfl");
		return -1;
	}
	if (item->dfl[1] == '\0') {
		t = item->dfl[0];
		switch (t) {
			case 'd':
				i = strtol(value, &p, 10);
				if (*p != '\0') goto ill;
				break;
			case 'f':
				f = strtod(value, &p);
				if (*p != '\0') {
					goto ill;
				} else if (errno) {
					upsdebug_with_errno(2, "setvar: f value %s", value);
					return -1;
				}
				break;
			case 's':
				/* copy to s to avoid snprintf()ing value to itself */
				if (strlen(value) >= sizeof s) goto ill;
				strcpy(s, value);
				break;
			default:
				upsdebugx(2, "setvar: unknown dfl %c", item->dfl[0]);
				return -1;
		}
	} else if (strncmp(item->dfl, "thms", 4) == 0) {
		int tt, h, m, sec;
		if (sscanf(item->value, "%d:%d:%d:%d", &tt, &h, &m, &sec) == 4) {
			if (tt < 0 || tt > 9999 || h < 0 || h > 23 || m < 0 || m > 59 || sec < 0 || sec > 59) goto ill;
		} else {
			long l;
			char *pl;

			l = strtol(value, &pl, 10);
			if (*pl != '\0') goto ill;
			sec = l % 60; l /= 60;
			m = l % 60; l /= 60;
			h = l % 24; l /= 24;
			if (l > 9999) goto ill;
			tt = (int)l;
		}
		snprintf(s, sizeof s, "%04d:%02d:%02d:%02d", tt, h, m, sec);
	} else {
		upsdebugx(2, "setvar: unknown dfl %s", item->dfl);
		return -1;
	}
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	switch (t) {
		case 'd':
			snprintf(value, valuelen, item->command, i);
			break;
		case 'f':
			snprintf(value, valuelen, item->command, f);
			break;
		case 's':
			snprintf(value, valuelen, item->command, s);
			break;
	}
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
	return 0;
ill:
	upsdebugx(2, "setvar: illegal %s value %s", item->dfl, value);
	return -1;
}

/* record new slave address in masterguard_next_slaveaddr; moved to masterguard_my_slaveaddr in masterguard_new_slaveaddr() after the slaveaddr-changing command finished */
static int masterguard_set_slaveaddr(item_t *item, char *value, const size_t valuelen) {
	char *p;

	masterguard_next_slaveaddr = strtol(value, &p, 10);
	if (*p != '\0') {
		upsdebugx(2, "set_slaveaddr: illegal value %s", value);
		return -1;
	}
	upsdebugx(3, "next slaveaddr %ld", masterguard_next_slaveaddr);
	return masterguard_setvar(item, value, valuelen);
}


/* variable setting answer preprocessing functions */

/* set my_slaveaddr to next_slaveaddr /after/ issuing the SS command (which, itself, needs the /old/ slaveaddr) */
static int masterguard_new_slaveaddr(item_t *item, const int len) {
	NUT_UNUSED_VARIABLE(item);

	upsdebugx(3, "saved slaveaddr %ld", masterguard_next_slaveaddr);
	if (masterguard_next_slaveaddr < 0 || masterguard_next_slaveaddr > 99) {
		upsdebugx(2, "%s: illegal value %ld", __func__, masterguard_next_slaveaddr);
		return -1;
	}
	masterguard_my_slaveaddr[0] = '0' + (char)(masterguard_next_slaveaddr / 10);
	masterguard_my_slaveaddr[1] = '0' + (char)(masterguard_next_slaveaddr % 10);
	upsdebugx(3, "new slaveaddr %s", masterguard_my_slaveaddr);
	return len;
}


/* qx2nut lookup table */
static item_t masterguard_qx2nut[] = {
	/* static values */

	/* type				flags	rw	command	answer	len	leading	value	from	to	dfl		qxflags				precmd	preans	preproc */
	{ "device.mfr",			0,	NULL,	"",	"",	0,	'\0',	"",	0,	0,	"Masterguard",	QX_FLAG_STATIC | QX_FLAG_ABSENT,NULL,	NULL,	NULL },
	{ "load.high",			0,	NULL,	"",	"",	0,	'\0',	"",	0,	0,	"140",		QX_FLAG_STATIC | QX_FLAG_ABSENT,NULL,	NULL,	NULL },
	/* battery.charge.low */
	/* battery.charge.warning */
	{ "battery.type",		0,	NULL,	"",	"",	0,	'\0',	"",	0,	0,	"PbAc",		QX_FLAG_STATIC | QX_FLAG_ABSENT,NULL,	NULL,	NULL },


	/* variables */

	/*
	 * > [WH\r]
	 * < [(XX VV.VV PP.PP TTTTTTTTTTTTTTTTTTTTTTTTTTTTTT B MMM FF.FF VVV SS.SS HHH.hh GGG.gg RRR mm nn MMM NNN FF.FF FF.FF\r]
	 *    01234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012
	 *    0         1         2         3         4         5         6         7         8         9         0         1
	 *    (00 10.06 03.09 A  700  + 0 Bat Pack-19        0 230 50.00 012 02.30 006.00 012.00 018 10 40 160 276 47.00 53.00
	 */
	/* type				flags		rw			command	answer	len	leading	value	from	to	dfl	qxflags					precmd	preans	preproc */
	{ "ups.id",			ST_FLAG_RW,	masterguard_r_slaveaddr,"WH\r",	"",	113,	'(',	"",	1,	2,	"%.0f",	QX_FLAG_SEMI_STATIC | QX_FLAG_RANGE,	NULL,	NULL,	masterguard_slaveaddr },
	{ "ups.firmware",		0,		NULL,			"WH\r",	"",	113,	'(',	"",	4,	8,	"%s",	QX_FLAG_STATIC,				NULL,	NULL,	NULL },
	{ "ups.firmware.aux",		0,		NULL,			"WH\r",	"",	113,	'(',	"",	10,	14,	"%s",	QX_FLAG_STATIC,				NULL,	NULL,	NULL },
	/* several values are deduced from the T field */
	{ "series",			0,		NULL,			"WH\r",	"",	113,	'(',	"",	16,	16,	"%s",	QX_FLAG_STATIC | QX_FLAG_NONUT,		NULL,	NULL,	masterguard_series },
	{ "device.model",		0,		NULL,			"WH\r",	"",	113,	'(',	"",	16,	45,	"%s",	QX_FLAG_STATIC,				NULL,	NULL,	masterguard_model },
	{ "ups.power.nominal",		0,		NULL,			"WH\r",	"",	113,	'(',	"",	16,	45,	"%s",	QX_FLAG_STATIC,				NULL,	NULL,	masterguard_power },
/* not used, use GS instead because the value is settable
	{ "battery.packs",		0,		NULL,			"WH\r",	"",	113,	'(',	"",	47,	47,	"%.0f",	QX_FLAG_STATIC,				NULL,	NULL,	NULL },
*/
	{ "input.voltage.nominal",	0,		NULL,			"WH\r",	"",	113,	'(',	"",	49,	51,	"%.0f",	QX_FLAG_STATIC,				NULL,	NULL,	NULL },
	{ "input.frequency.nominal",	0,		NULL,			"WH\r",	"",	113,	'(',	"",	53,	57,	"%.2f",	QX_FLAG_STATIC,				NULL,	NULL,	NULL },
	{ "number_of_battery_cells",	0,		NULL,			"WH\r",	"",	113,	'(',	"",	59,	61,	"%.0f",	QX_FLAG_STATIC | QX_FLAG_NONUT,		NULL,	NULL,	masterguard_numcells },
	{ "nominal_cell_voltage",	0,		NULL,			"WH\r",	"",	113,	'(',	"",	63,	67,	"%.2f",	QX_FLAG_STATIC | QX_FLAG_NONUT,		NULL,	NULL,	NULL },
	{ "battery.voltage.nominal",	0,		NULL,			"WH\r",	"",	113,	'(',	"",	63,	67,	"%.2f",	QX_FLAG_STATIC,				NULL,	NULL,	masterguard_battvolt},
	{ "runtime_half",		0,		NULL,			"WH\r",	"",	113,	'(',	"",	69,	74,	"%.0f",	QX_FLAG_STATIC | QX_FLAG_NONUT,		NULL,	NULL,	masterguard_mmm_ss },
	{ "runtime_full",		0,		NULL,			"WH\r",	"",	113,	'(',	"",	76,	81,	"%.0f",	QX_FLAG_STATIC | QX_FLAG_NONUT,		NULL,	NULL,	masterguard_mmm_ss },
	{ "recharge_time",		0,		NULL,			"WH\r",	"",	113,	'(',	"",	83,	85,	"%.0f",	QX_FLAG_STATIC | QX_FLAG_NONUT,		NULL,	NULL,	masterguard_hhh },
/*!! what's the difference between low/high and low.critical/high.critical?? */
	{ "ambient.0.temperature.low",	0,		NULL,			"WH\r",	"",	113,	'(',	"",	87,	88,	"%.0f",	QX_FLAG_STATIC,				NULL,	NULL,	NULL },
	{ "ambient.0.temperature.high",	0,		NULL,			"WH\r",	"",	113,	'(',	"",	90,	91,	"%.0f",	QX_FLAG_STATIC,				NULL,	NULL,	NULL },
	{ "input.voltage.low.critical",	0,		NULL,			"WH\r",	"",	113,	'(',	"",	93,	95,	"%.0f",	QX_FLAG_STATIC,				NULL,	NULL,	NULL },
	{ "input.voltage.high.critical",0,		NULL,			"WH\r",	"",	113,	'(',	"",	97,	99,	"%.0f",	QX_FLAG_STATIC,				NULL,	NULL,	NULL },
	{ "input.frequency.low",	0,		NULL,			"WH\r",	"",	113,	'(',	"",	101,	105,	"%.2f",	QX_FLAG_STATIC,				NULL,	NULL,	NULL },
	{ "input.frequency.high",	0,		NULL,			"WH\r",	"",	113,	'(',	"",	107,	111,	"%.2f",	QX_FLAG_STATIC,				NULL,	NULL,	NULL },

	/*
	 * > [Q3\r]
	 *                                                         76543210 76543210
	 * < [(XX MMM.M NNN.N PPP.P QQQ RR.R SS.SS TT.T ttt.tt CCC BBBBBBBB TTTTTTTT\r]
	 *    01234567890123456789012345678901234567890123456789012345678901234567890
	 *    0         1         2         3         4         5         6         7
	 *    (00 225.9 225.9 229.3 043 50.0 02.27 23.4 017.03 100 00000000 00000000
	 *    (01 226.9 226.9 226.9 039 50.0 02.30 21.8 000.00 000 01100000 00011000
	 */
	/* type				flags	rw	command	answer	len	leading	value	from	to	dfl	qxflags			precmd	preans	preproc */
	{ "input.voltage",		0,	NULL,	"Q3\r",	"",	71,	'(',	"",	4,	8,	"%.1f",	0,			NULL,	NULL,	NULL },
	{ "input_fault_voltage",	0,	NULL,	"Q3\r",	"",	71,	'(',	"",	10,	14,	"%.1f",	QX_FLAG_NONUT,		NULL,	NULL,	NULL },
	{ "output.voltage",		0,	NULL,	"Q3\r",	"",	71,	'(',	"",	16,	20,	"%.1f",	0,			NULL,	NULL,	NULL },
	{ "ups.load",			0,	NULL,	"Q3\r",	"",	71,	'(',	"",	22,	24,	"%.0f",	0,			NULL,	NULL,	NULL },
	{ "ups.power",			0,	NULL,	"Q3\r",	"",	71,	'(',	"",	22,	24,	"%.0f",	0,			NULL,	NULL,	masterguard_ups_power },
	{ "output.current",		0,	NULL,	"Q3\r",	"",	71,	'(',	"",	22,	24,	"%f",	0,			NULL,	NULL,	masterguard_output_current },
	{ "input.frequency",		0,	NULL,	"Q3\r",	"",	71,	'(',	"",	26,	29,	"%.1f",	0,			NULL,	NULL,	NULL },
	{ "battery.voltage",		0,	NULL,	"Q3\r",	"",	71,	'(',	"",	31,	35,	"%.1f",	0,			NULL,	NULL, 	masterguard_battvolt },
	{ "ups.temperature",		0,	NULL,	"Q3\r",	"",	71,	'(',	"",	37,	40,	"%.1f",	0,			NULL,	NULL,	NULL },
/*!! report both ups.temperature and ambient.0.temperature?? */
	{ "ambient.0.temperature",	0,	NULL,	"Q3\r",	"",	71,	'(',	"",	37,	40,	"%.1f",	0,			NULL,	NULL,	NULL },
	{ "battery.runtime",		0,	NULL,	"Q3\r",	"",	71,	'(',	"",	42,	47,	"%.0f",	0,			NULL,	NULL,	masterguard_mmm_ss },
	{ "battery.charge",		0,	NULL,	"Q3\r",	"",	71,	'(',	"",	49,	51,	"%.0f",	0,			NULL,	NULL,	NULL },
	/* Status bits, first half (B) */
	{ "ups.status",			0,	NULL,	"Q3\r",	"",	71,	'(',	"",	53,	53,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	masterguard_status },	/* B7: Utility Fail */
	{ "ups.status",			0,	NULL,	"Q3\r",	"",	71,	'(',	"",	54,	54,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	masterguard_status },	/* B6: Battery Low */
	{ "ups.status",			0,	NULL,	"Q3\r",	"",	71,	'(',	"",	55,	55,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	masterguard_status },	/* B5: Bypass/Boost Active */
	{ "ups.alarm",			0,	NULL,	"Q3\r",	"",	71,	'(',	"",	56,	56,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	masterguard_status },	/* B4: UPS Failed */
	{ "ups.type",			0,	NULL,	"Q3\r",	"",	71,	'(',	"",	57,	57,	NULL,	QX_FLAG_STATIC,		NULL,	NULL,	masterguard_status },	/* B3: UPS Type */
	{ "ups.status",			0,	NULL,	"Q3\r",	"",	71,	'(',	"",	58,	58,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	masterguard_status },	/* B2: Test in Progress */
	{ "ups.status",			0,	NULL,	"Q3\r",	"",	71,	'(',	"",	59,	59,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	masterguard_status },	/* B1: Shutdown Active */
	/* unused */																					/* B0: unused */
	/* Status bits, second half (T) */
	/* unused */																					/* T7: unused */
	{ "ups.alarm",			0,	NULL,	"Q3\r",	"",	69,	'(',	"",	63,	63,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	masterguard_status },	/* T6: problems in parallel operation mode */
	/* part of a parallel set */																			/* T5: is part of a parallel set */
	{ "ups.status",			0,	NULL,	"Q3\r",	"",	71,	'(',	"",	65,	65,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	masterguard_status },	/* T4: Battery: end of service life */
	{ "ups.alarm",			0,	NULL,	"Q3\r",	"",	71,	'(',	"",	66,	66,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	masterguard_status },	/* T3: battery connected */
	{ "ups.test.result",		0,	NULL,	"Q3\r",	"",	71,	'(',	"",	67,	69,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	masterguard_status },	/* T210: Test Status */

	/*
	 * > [GS,XX\r]
	 * < [(XX,ii p a\r]
	 *    01234567890
	 *    0         1
	 *    (00,00 0 1
	 */
	/* type				flags		rw			command		answer	len	leading	value	from	to	dfl	qxflags					precmd				preans				preproc */
	/* ups.id obtained via WH */
	{ "ups.id",			0,		NULL,			"SS%02d--,XX\r","",	0,	'\0',	"",	0,	0,	"d",	QX_FLAG_SETVAR | QX_FLAG_RANGE,		masterguard_add_slaveaddr,	masterguard_new_slaveaddr,	masterguard_set_slaveaddr },
	{ "battery.packs",		ST_FLAG_RW,	masterguard_r_batpacks,	"GS,XX\r",	"",	0,	'(',	"",	7,	7,	"%.0f",	QX_FLAG_SEMI_STATIC | QX_FLAG_RANGE,	masterguard_add_slaveaddr,	NULL,				NULL },
	{ "battery.packs",		0,		NULL,			"SS--%1d-,XX\r","",	0,	'\0',	"",	0,	0,	"d",	QX_FLAG_SETVAR | QX_FLAG_RANGE,		masterguard_add_slaveaddr,	NULL,				masterguard_setvar },
/*!! which QX_FLAGs to use?? (changed by instcmd) */
	{ "ups.beeper.status",		0,		NULL,			"GS,XX\r",	"",	11,	'(',	"",	9,	9,	NULL,	QX_FLAG_SEMI_STATIC,			masterguard_add_slaveaddr,	NULL,				masterguard_beeper_status },
	/* set with beeper.{en,dis}able */

	/*
	 * > [GBS,XX\r]
	 * < [(XX,CCC hhhh HHHH AAAA BBBB DDDD EEE SS.SS\r]
	 *    0123456789012345678901234567890123456789012
	 *    0         1         2         3         4
	 *    (00,100 0017 0000 0708 0712 0994 115 02.28
	 *    (01,000 0000 0360 0708 0712 0994 076 02.30
	 */
	/* type			flags	rw	command		answer	len	leading	value	from	to	dfl	qxflags	precmd				preans	preproc */
	{ "battery.charge",	0,	NULL,	"GBS,XX\r",	"",	43,	'(',	"",	4,	6,	"%.0f",	0,	masterguard_add_slaveaddr,	NULL,	NULL },
	/* 
	 * hhhh: hold time (minutes)
	 * HHHH: recharge time to 90% (minutes)
	 * AAAA: Ageing factor (promilles)
	 * BBBB: Ageing factor time dependant (promilles)
	 * DDDD: Ageing factor cyclic use (promilles)
	 * EEE: Calibration factor (percent)
	 * SS.SS: Actual battery cell voltage
	*/

	/*
	 * > [GSN,XX\r]
	 * < [(XX,SSN=SSSSSSSSSSSnnnnn\r]
	 *    0123456789012345678901234
	 *    0         1         2
	 *    (00,SSN=6A1212      2782
	 */
	/* type			flags	rw	command		answer	len	leading	value	from	to	dfl	qxflags				precmd				preans	preproc */
	{ "device.part",	0,	NULL,	"GSN,XX\r",	"",	25,	'(',	"",	8,	18,	"%s",	QX_FLAG_STATIC | QX_FLAG_TRIM,	masterguard_add_slaveaddr,	NULL,	NULL },
	{ "device.serial",	0,	NULL,	"GSN,XX\r",	"",	25,	'(',	"",	20,	23,	"%s",	QX_FLAG_STATIC,			masterguard_add_slaveaddr,	NULL,	NULL },

	/*
	 * > [DRC,XX\r]
	 * < [(XX,TTTT:hh:mm:ss\r]
	 *    012345678901234567
	 *    0         1
	 *    (00,1869:19:06:37
	 */
	/* type			flags		rw	command		answer	len	leading	value	from	to	dfl	qxflags			precmd				preans	preproc */
	/* this is not really the uptime, but the running time since last maintenance */
	{ "device.uptime",	ST_FLAG_RW,	NULL,	"DRC,XX\r",	"",	17,	'(',	"",	4,	16,	"%.0f",	QX_FLAG_SEMI_STATIC,	masterguard_add_slaveaddr,	NULL,	masterguard_tttt_hh_mm_ss },
	{ "device.uptime",	0,		NULL,	"SRC%s,XX\r",	"",	0,	'\0',	"",	0,	0,	"thms",	QX_FLAG_SETVAR,		masterguard_add_slaveaddr,	NULL,	masterguard_setvar },

	/*
	 * > [MSO\r]
	 * < [(220 230 240\r]
	 *    0123456789012
	 *    0         1
	 *    (220 230 240
	 */
	/* type			flags	rw	command		answer	len	leading	value	from	to	dfl	qxflags				precmd	preans	preproc */
	{ "output_voltages",	0,	NULL,	"MSO\r",	"",	5,	'(',	"",	1,	0,	"%s",	QX_FLAG_STATIC | QX_FLAG_NONUT,	NULL,	NULL,	masterguard_output_voltages },

	/*
	 * > [PNV\r]
	 * < [(PNV=nnn\r]
	 *    012345678
	 *    0
	 *    (PNV=230
	 */
	/* type				flags		rw			command		answer	len	leading	value	from	to	dfl	qxflags					precmd	preans	preproc */
	{ "output.voltage.nominal",	ST_FLAG_RW,	NULL /* see claim */,	"PNV\r",	"",	8,	'(',	"",	5,	7,	"%.0f",	QX_FLAG_SEMI_STATIC | QX_FLAG_ENUM,	NULL,	NULL,	NULL },
	{ "output.voltage.nominal",	0,		NULL,			"PNV=%03d\r",	"",	0,	'\0',	"",	0,	0,	"d",	QX_FLAG_SETVAR,				NULL,	NULL,	masterguard_setvar },
	{ "output.current.nominal",	0,		NULL,			"PNV\r",	"",	8,	'(',	"",	5,	7,	"%.0f",	QX_FLAG_SEMI_STATIC,			NULL,	NULL,	masterguard_output_current_nominal },

	/*
	 * > [FLT,XX\r]
	 * < [(XX,A aaaa TTTT:hh:mm:ss B bbbb TTTT:hh:mm:ss C cccc TTTT:hh:mm:ss D dddd TTTT:hh:mm:ss E eeee TTTT:hh:mm:ss\r
	 *    0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678
	 *    0         1         2         3         4         5         6         7         8         9         0
	 *    (00,7 0043 0000:16:48:06 0 0000 0000:00:00:00 0 0000 0000:00:00:00 0 0000 0000:00:00:00 0 0000 0000:00:00:00
	 *    (01,9 0010 1780:14:57:19 7 0046 0000:21:14:41 0 0000 0000:00:00:00 0 0000 0000:00:00:00 0 0000 0000:00:00:00
	 */
	/* type		flags	rw	command		answer	len	leading	value	from	to	dfl	qxflags		precmd				preans	preproc */
	{ "fault_1",	0,	NULL,	"FLT,XX\r",	"",	108,	'(',	"",	4,	23,	"%s",	QX_FLAG_NONUT,	masterguard_add_slaveaddr,	NULL,	masterguard_fault },
	{ "fault_2",	0,	NULL,	"FLT,XX\r",	"",	108,	'(',	"",	25,	44,	"%s",	QX_FLAG_NONUT,	masterguard_add_slaveaddr,	NULL,	masterguard_fault },
	{ "fault_3",	0,	NULL,	"FLT,XX\r",	"",	108,	'(',	"",	46,	65,	"%s",	QX_FLAG_NONUT,	masterguard_add_slaveaddr,	NULL,	masterguard_fault },
	{ "fault_4",	0,	NULL,	"FLT,XX\r",	"",	108,	'(',	"",	67,	86,	"%s",	QX_FLAG_NONUT,	masterguard_add_slaveaddr,	NULL,	masterguard_fault },
	{ "fault_5",	0,	NULL,	"FLT,XX\r",	"",	108,	'(',	"",	88,	107,	"%s",	QX_FLAG_NONUT,	masterguard_add_slaveaddr,	NULL,	masterguard_fault },


	/* instant commands */
	/* type				flags	rw	command		answer	len	leading	value	from	to	dfl	qxflags		precmd				preans	preproc */
/*!! what's the difference between load.off.delay and shutdown.stayoff?? */
#if 0
	{ "load.off",			0,	NULL,	"S.0\r",	"",	0,	'\0',	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,				NULL,	NULL },
	{ "load.on",			0,	NULL,	"C\r",		"",	0,	'\0',	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,				NULL,	NULL },
	/* load.off.delay */
	/* load.on.delay */
#endif
	{ "shutdown.return",		0,	NULL,	NULL,		"",	0,	'\0',	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,				NULL,	masterguard_shutdown_return },
	{ "shutdown.stayoff",		0,	NULL,	NULL,		"",	0,	'\0',	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,				NULL,	masterguard_shutdown_stayoff },
	{ "shutdown.stop",		0,	NULL,	"C\r",		"",	0,	'\0',	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,				NULL,	NULL },
	/* shutdown.reboot */
	/* shutdown.reboot.graceful */
	/* test.panel.start */
	/* test.panel.stop */
	/* test.failure.start */
	/* test.failure.stop */
	{ "test.battery.start",		0,	NULL,	NULL,		"",	0,	'\0',	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,				NULL,	masterguard_test_battery },
	{ "test.battery.start.quick",	0,	NULL,	"T\r",		"",	0,	'\0',	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,				NULL,	NULL },
	{ "test.battery.start.deep",	0,	NULL,	"TUD\r",	"",	0,	'\0',	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,				NULL,	NULL },
	{ "test.battery.stop",		0,	NULL,	"CT\r",		"",	0,	'\0',	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,				NULL,	NULL },
	/* test.system.start */
	/* calibrate.start */
	/* calibrate.stop */
	{ "bypass.start",		0,	NULL,	"FOFF\r",	"",	0,	'\0',	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,				NULL,	NULL },
	{ "bypass.stop",		0,	NULL,	"FON\r",	"",	0,	'\0',	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,				NULL,	NULL },
	/* reset.input.minmax */
	/* reset.watchdog */
	{ "beeper.enable",		0,	NULL,	"SS---1,XX\r",	"",	0,	'\0',	"",	0,	0,	NULL,	QX_FLAG_CMD,	masterguard_add_slaveaddr,	NULL,	NULL },
	{ "beeper.disable",		0,	NULL,	"SS---0,XX\r",	"",	0,	'\0',	"",	0,	0,	NULL,	QX_FLAG_CMD,	masterguard_add_slaveaddr,	NULL,	NULL },
	/* beeper.mute */
	/* beeper.toggle */
	/* outlet.* */


	/* server variables */
	/* type			flags		rw			command	answer	len	leading	value	from	to	dfl			qxflags		precmd				preans	preproc */
	{ "ups.delay.shutdown",	ST_FLAG_RW,	masterguard_r_offdelay,	NULL,	"",	0,	'\0',	"",	0,	0,	DEFAULT_OFFDELAY,	QX_FLAG_ABSENT | QX_FLAG_SETVAR | QX_FLAG_RANGE,	NULL,	NULL,	NULL },
	{ "ups.delay.start",	ST_FLAG_RW,	masterguard_r_ondelay,	NULL,	"",	0,	'\0',	"",	0,	0,	DEFAULT_ONDELAY,	QX_FLAG_ABSENT | QX_FLAG_SETVAR | QX_FLAG_RANGE,	NULL,	NULL,	NULL },


	/* end marker */
	{ NULL, 0, NULL, NULL, "", 0, 0, "", 0, 0, NULL, 0, NULL, NULL, NULL }
};


/*!! todo

untested:
Sxx (.n/nn)
C after S.0

unused:
>G01,00
(00,00000

additional E series commands:
PSR
BUS*
V
INVDC

use ups.{delay,timer}.{start,reboot,shutdown}?
report ups.contacts?
how to report battery.charger.status?
set battery.packs.bad?

how to report battery aeging/run time?
how to report nominal hold time at half/full load?

*/


/* commands supported by A series */
static char *masterguard_commands_a[] = {
	"Q", "Q1", "Q3", "T", "TL", "S", "C", "CT", "WH", "M", "N", "O", "DECO", "DRC", "SRC", "FLT", "FCLR", "G", "SS", "GS", "MSO", "PNV", "FOFF", "FON", "TUD", "GBS", "SSN", "GSN", NULL
};

/* commands supported by E series */
static char *masterguard_commands_e[] = {
	"Q", "Q1", "Q3", "PSR", "T", "TL", "S", "C", "CT", "WH", "DRC", "SRC", "FLT", "FCLR", "SS", "GS", "MSO", "PNV", "FOFF", "FON", "TUD", "GBS", "SSN", "GSN", "BUS", "V", "INVDC", "BUSP", "BUSN", NULL
};

/* claim function. fetch some mandatory values, disable unsupported commands, set enum for supported output voltages */
static int masterguard_claim(void) {
	item_t *item;
	/* mandatory values */
	char *mandatory[] = {
		"series",		/* SKIP */
		"device.model",		/* minimal number of battery packs */
		"ups.power.nominal",	/* load computation */
		"ups.id",		/* slave address */
		"output_voltages",	/* output voltages enum */
#if 0
		"battery.packs",	/* battery voltage computation */
#endif
		NULL
	};
	char **sp;
	long config_slaveaddr;
	char *sa;
	char **commands;

	if ((sa = getval("slave_address")) != NULL) {
		char *p;

		if (*sa == '\0') {
			upsdebugx(2, "claim: empty slave_address");
			return 0;
		}
		config_slaveaddr = strtol(sa, &p, 10);
		if (*p != '\0' || config_slaveaddr < 0 || config_slaveaddr > 99) {
			upsdebugx(2, "claim: illegal slave_address %s", sa);
			return 0;
		}
	} else {
		config_slaveaddr = -1;
	}
	for (sp = mandatory; *sp != NULL; sp++) {
		char value[SMALLBUF] = "";

		if ((item = find_nut_info(*sp, 0, QX_FLAG_SETVAR)) == NULL) {
			upsdebugx(2, "claim: cannot find %s", *sp);
			return 0;
		}
		/* since qx_process_answer() is not exported, there's no way to avoid sending the same command to the UPS again */
		if (qx_process(item, NULL) < 0) {
			upsdebugx(2, "claim: cannot process %s", *sp);
			return 0;
		}
		/* only call the preprocess function; don't call ups_infoval_set() because that does a dstate_setinfo() before dstate_setflags() is called (via qx_set_var() in qx_ups_walk() with QX_WALKMODE_INIT); that leads to r/w vars ending up r/o. */
		if (item->preprocess == NULL ) {
			upsdebugx(2, "claim: no preprocess function for %s", *sp);
			return 0;
		}
		if (item->preprocess(item, value, sizeof value)) {
			upsdebugx(2, "claim: failed to preprocess %s", *sp);
			return 0;
		}
	}

	if (config_slaveaddr >= 0 && config_slaveaddr != strtol(masterguard_my_slaveaddr, NULL, 10)) {
		upsdebugx(2, "claim: slave address mismatch: want %02ld, have %s", config_slaveaddr, masterguard_my_slaveaddr);
		return 0;
	}

	switch (masterguard_my_series) {
		case 'A':
			commands = masterguard_commands_a;
			break;
		case 'E':
			commands = masterguard_commands_e;
			break;
		default:
			return 0;
	}

	/* set SKIP flag for unimplemented commands */
	for (item = masterguard_qx2nut; item->info_type != NULL; item++) {
		int match = 0;
		if (item->command == NULL || item->command[0] == '\0') continue;
		for (sp = commands; sp != NULL; sp++) {
			const char *p = *sp, *q = item->command;

			while (1) {
				if (*p == '\0' && (*q < 'A' || *q > 'Z')) {
					match = 1; break;
				} else if (*p == '\0' || *q < 'A' || *q > 'Z' || *p != *q) {
					match = 0; break;
				}
				p++; q++;
			}
			if (match) break;
		}
		if (nut_debug_level >= 3) {
			char cmd[10];
			char *p = cmd; const char *q = item->command;
			while (*q >= 'A' && *q <= 'Z') {
				*p++ = *q++;
				if (p - cmd >= (ptrdiff_t)sizeof cmd - 1) break;
			}
			*p++ = '\0';
			upsdebugx(3, "command %s %simplemented", cmd, match ? "" : "NOT ");

		}
		if (!match)
			item->qxflags |= QX_FLAG_SKIP;
	}

	/* set enum for output.voltage.nominal */
	if ((item = find_nut_info("output.voltage.nominal", QX_FLAG_ENUM, QX_FLAG_SETVAR)) == NULL) {
		upsdebugx(2, "claim: cannot find output.voltage.nominal");
		return 0;
	}
	item->info_rw = masterguard_e_outvolts;

	return 1;
}


static void masterguard_makevartable(void) {
	addvar(VAR_VALUE, "series", "Series (A/E)");
	addvar(VAR_VALUE, "slave_address", "Slave address (UPS id) to match");
	addvar(VAR_VALUE, "input_fault_voltage", "Input fault voltage (whatever that means)");
	addvar(VAR_VALUE, "number_of_battery_cells", "Number of battery cells in series");
	addvar(VAR_VALUE, "nominal_cell_voltage", "Nominal battery cell voltage");
	addvar(VAR_VALUE, "runtime_half", "Nominal battery run time at 50% load (seconds)");
	addvar(VAR_VALUE, "runtime_full", "Nominal battery run time at 100% load (seconds)");
	addvar(VAR_VALUE, "recharge_time", "Nominal battery recharge time to 95% capacity (seconds)");
	addvar(VAR_VALUE, "output_voltages", "Possible output voltages (volts)");
	addvar(VAR_VALUE, "fault_1", "Fault record 1 (newest)");
	addvar(VAR_VALUE, "fault_2", "Fault record 2");
	addvar(VAR_VALUE, "fault_3", "Fault record 3");
	addvar(VAR_VALUE, "fault_4", "Fault record 4");
	addvar(VAR_VALUE, "fault_5", "Fault record 5 (oldest)");
}


#ifdef TESTING
static testing_t masterguard_testing[] = {
	{ NULL }
};
#endif	/* TESTING */

subdriver_t masterguard_subdriver = {
	MASTERGUARD_VERSION,
	masterguard_claim,
	masterguard_qx2nut,
	NULL, /* initups */
	NULL, /* intinfo */
	masterguard_makevartable,
	NULL, /* accepted */
	NULL, /* rejected */
#ifdef TESTING
	masterguard_testing,
#endif	/* TESTING */
};
