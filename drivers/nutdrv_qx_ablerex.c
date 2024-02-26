/* nutdrv_qx_ablerex.c - Subdriver for Ablerex Qx protocol based UPSes
 *
 * Copyright (C)
 *   2013 Daniele Pezzini <hyouko@gmail.com>
 *   2021 Ablerex Software <Ablerex.software@ablerex.com.tw>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
#include "nutdrv_qx_blazer-common.h"

#include "nutdrv_qx_ablerex.h"

#define ABLEREX_VERSION "Ablerex 0.01"

static int Q5_Vbc = -1;
static int ablerexQ5Vb = -1;

static int ablerex_Q5(item_t *item, char *value, const size_t valuelen) {
	int	Q5_Fout, Q5_Vb, Q5_O_Cur, Q5_Err;
#ifdef ABLEREX_WITH_Q5_InvW
	int	Q5_InvW;
#endif
#ifdef ABLEREX_WITH_RawValue
	int	RawValue = ((int)(unsigned char)item->value[0]) * 256 + (unsigned char)item->value[1];
#endif

/* // real code below, this is for dev-testing
	ablerexQ5Vb = ((int)(unsigned char)buf[7]) * 256 + (unsigned char)buf[8];
	Q5_Vbc = ((int)(unsigned char)buf[9]) * 256 + (unsigned char)buf[10];
*/

	upsdebugx(2, "Q51: %d %d %d %d %d %d", item->answer[0], item->answer[1], item->answer[2], item->answer[3], item->answer[4], item->answer[5]);
	upsdebugx(2, "Q52: %d %d %d %d %d %d", item->answer[6], item->answer[7], item->answer[8], item->answer[9], item->answer[10], item->answer[11]);
	upsdebugx(2, "Q53: %d %d %d %d", item->answer[12], item->answer[13], item->answer[14], item->answer[15]);

	Q5_Fout = (unsigned char)item->answer[1] * 256 + (unsigned char)item->answer[2];
	Q5_Vb = (unsigned char)item->answer[7] * 256 + (unsigned char)item->answer[8];
	Q5_Vbc = (unsigned char)item->answer[9] * 256 + (unsigned char)item->answer[10];
#ifdef ABLEREX_WITH_Q5_InvW
	Q5_InvW = (unsigned char)item->answer[11] * 256 + (unsigned char)item->answer[12];
#endif
	Q5_Err = (unsigned char)item->answer[13] * 256 + (unsigned char)item->answer[14];
	Q5_O_Cur = (unsigned char)item->answer[15] * 256 + (unsigned char)item->answer[16];

	ablerexQ5Vb = Q5_Vb;
	upsdebugx(2, "Q5: %.1f %d %.1f", 0.1 * Q5_Fout, Q5_Err, 0.1 * Q5_O_Cur);
	upsdebugx(2, "Q5Vb: %d Vbc %d", Q5_Vb, Q5_Vbc);
#ifdef ABLEREX_WITH_Q5_InvW
	upsdebugx(2, "Q5_InvW: %d", Q5_InvW);
#endif
	dstate_setinfo("output.frequency", "%.1f", 0.1 * Q5_Fout);
	dstate_setinfo("ups.alarm", "%d", Q5_Err);
	dstate_setinfo("output.current", "%.1f", 0.1 * Q5_O_Cur);

	snprintf(value, valuelen, "%.1f", Q5_Fout * 0.1);

#ifdef ABLEREX_WITH_RawValue
	switch (item->from)
	{
	case 1:
		snprintf(value, valuelen, "%.1f", RawValue * 0.1);
		upsdebugx(2, "Q51: %.1f", 0.1*RawValue);
		break;
	case 13:
		snprintf(value, valuelen, "%.0f", RawValue);
		upsdebugx(2, "Q52: %.0f", 0.1*RawValue);
		break;
	case 15:
		snprintf(value, valuelen, "%.1f", RawValue * 0.1);
		upsdebugx(2, "Q53: %.1f", 0.1*RawValue);
		break;

	default:
		/* Don't know what happened */
		return -1;
	}
#endif

	return 0;
}

static int ablerex_battery(item_t *item, char *value, const size_t valuelen) {
	double	BattV = 0.0;
	double	nomBattV = 0.0;
	double	battvoltact = 0.0;

	BattV = strtod(item->value, NULL);
	upsdebugx(2, "battvoltact2: %.2f", BattV);
	if (!dstate_getinfo("battery.voltage.nominal"))
	{
		snprintf(value, valuelen, "%.2f", BattV);
		return 0;
	}

	nomBattV = strtod(dstate_getinfo("battery.voltage.nominal"),  NULL);
	upsdebugx(2, "battvoltact1: %.2f", nomBattV);
/*
 *	//return 0;
 */

	if (ablerexQ5Vb > 0) {
		battvoltact = ablerexQ5Vb * nomBattV / 1200;
	} else {
		if (BattV > 3.0) {
			battvoltact = BattV;
		} else {
			battvoltact = BattV * 6 * nomBattV / 12;
		}
	}

	snprintf(value, valuelen, "%.2f", battvoltact);
	upsdebugx(2, "battvoltact: %.2f / %.2f", battvoltact, BattV);

	return 0;
}

static int ablerex_battery_charge(double BattIn)
{
	const double onlineP[] = {
		2.22, 2.21, 2.20, 2.19, 2.18, 2.17, 2.16, 2.15, 2.14, 2.13,
		2.12, 2.11, 2.10, 2.09, 2.08, 2.07, 2.06, 2.05, 2.04, 2.03,
		2.02, 2.01, 2.00, 1.99, 1.98, 1.97, 1.96, 1.95, 1.94, 1.93,
		1.92, 1.91, 1.90, 1.89, 1.88, 1.87, 1.86, 1.85, 1.84, 1.83,
		1.82, 1.81, 1.80, 1.79, 1.78, 1.77, 1.76, 1.75, 1.74, 1.73,
		1.72, 1.71, 1.70, 1.69, 1.68, 1.67
	};
	const int onlineC[] = {
		100, 90, 88, 87, 85, 83, 82, 80, 78, 77,
		75, 73, 72, 70, 68, 65, 65, 62, 62, 58,
		58, 55, 55, 53, 52, 50, 48, 47, 45, 43,
		42, 40, 38, 37, 35, 33, 32, 30, 28, 27,
		25, 23, 22, 20, 18, 17, 15, 13, 12, 10,
		8, 7, 5, 3, 2, 0, -1
	};
	const double offlineP[] = {
		13.5, 13.3, 13.2, 13.1, 13, 12.9, 12.8, 12.7, 12.6, 12.5,
		12.4, 12.3, 12.2, 12.1, 12, 11.9, 11.8, 11.7, 11.6, 11.5,
		11.4, 11.3, 11.2, 11.1, 11, 10.9, 10.8, 10.7, 10.6, 10.5,
		10.4, 10.3, 10.2, 10.1, 10
	};
	const int offlineC[] = {
		100, 90, 88, 86, 83, 80, 77, 74, 72, 69,
		66, 63, 61, 58, 55, 52, 49, 47, 44, 41,
		38, 36, 33, 30, 27, 24, 22, 19, 16, 13,
		11, 8, 5, 2, 0, -1
	};

	int charge = 0;
	int i;

	if (BattIn < 3.0) {
		for (i = 0; onlineC[i] > 0; i++) {
			if (BattIn >= onlineP[i]) {
				charge = onlineC[i];
				break;
			}
		}
	} else {
/*
 *		//double nomBattV = strtod(dstate_getinfo("battery.voltage.nominal"),  NULL);
 *		//double battV = BattIn / (nomBattV / 12);
 */

		for (i = 0; offlineC[i] > 0; i++) {
			if (BattIn >= offlineP[i]) {
				charge = offlineC[i];
				break;
			}
		}
	}
	return charge;
}

static int ablerex_batterycharge(item_t *item, char *value, const size_t valuelen) {
	double	BattV = 0.0;
	double	nomBattV = 0.0;
	int	BattP;

	BattV = strtod(item->value, NULL);
	upsdebugx(2, "battvoltc2: %.2f", BattV);
	if (!dstate_getinfo("battery.voltage.nominal"))
	{
		snprintf(value, valuelen, "%d", 100);
		return 0;
	}

	nomBattV = strtod(dstate_getinfo("battery.voltage.nominal"),  NULL);
	upsdebugx(2, "battvv1: %.2f", nomBattV);
/*
 *	//return 0;
 */

	if (BattV > 3.0) {
		BattV = BattV / (nomBattV / 12);
	}
	BattP = ablerex_battery_charge(BattV);
/*
 *	//dstate_setinfo("battery.charge", "%.0f", BattP);
 */

	snprintf(value, valuelen, "%d", BattP);
	upsdebugx(2, "battcharge: %d", BattP);

	return 0;
}

static int ablerex_initbattery(item_t *item, char *value, const size_t valuelen) {

	double nomBattV = strtod(dstate_getinfo("battery.voltage.nominal"),  NULL);
	double batthigh = 0.0;
	double battlow = 0.0;

	if (Q5_Vbc > 0) {
		battlow = Q5_Vbc * nomBattV / 1200;
	} else {
		battlow = 960 * nomBattV / 1200;
	}
	batthigh = 1365 * nomBattV / 1200;

	switch (item->from)
	{
	case 1:
		snprintf(value, valuelen, "%.2f", battlow);
		upsdebugx(2, "BattLow: %.2f", battlow);
		break;
	case 2:
		snprintf(value, valuelen, "%.2f", batthigh);
		upsdebugx(2, "BattHigh: %.2f", batthigh);
		break;

	default:
		/* Don't know what happened */
		return -1;
	}

	return 0;
}

static int ablerex_At(item_t *item, char *value, const size_t valuelen) {
	int RawValue = 0;

	RawValue = (unsigned char)item->answer[1] * 65536 * 256 + (unsigned char)item->answer[2] * 65536
	         + (unsigned char)item->answer[3] * 256 + (unsigned char)item->answer[4];

	snprintf(value, valuelen, "%d", RawValue);
	upsdebugx(2, "At: %d", RawValue);

	return 0;
}

static int ablerex_TR(item_t *item, char *value, const size_t valuelen) {
	char TR[8];

	TR[0] = item->answer[1];
	TR[1] = item->answer[2];
	TR[2] = item->answer[3];
	TR[3] = item->answer[4];
	TR[4] = 0;

	snprintf(value, valuelen, "%s", TR);
	upsdebugx(2, "At: %s", TR);

	return 0;
}

static int	ablerex_process_status_bits(item_t *item, char *value, const size_t valuelen)
{
	char	*val = "";

	switch (item->from)
	{
	case 40:	/* Bypass/Boost or Buck Active */

		if (item->value[0] == '1') {

			double	vi, vo;

			vi = strtod(dstate_getinfo("input.voltage"), NULL);
			vo = strtod(dstate_getinfo("output.voltage"), NULL);

			if (item->value[2] == '1') {/* UPS Type is Standby (0 is On_line) */
				if (vo < 0.5 * vi) {
					upsdebugx(2, "%s: output voltage too low", __func__);
					return -1;
				} else if (vo < 0.95 * vi) {
					status_set("TRIM");
				} else if (vo < 1.05 * vi) {
					status_set("BYPASS");
				} else if (vo < 1.5 * vi) {
					status_set("BOOST");
				} else {
					upsdebugx(2, "%s: output voltage too high", __func__);
					return -1;
				}
			} else {
				status_set("BYPASS");
			}
		}

		break;

	case 41:	/* UPS Failed - ups.alarm */

		if (item->value[0] == '1') {	/* Battery abnormal */
			status_set("RB");
		}

		{ /* scope */
			double vout = strtod(dstate_getinfo("output.voltage"), NULL);

			if (vout < 50.0) {
				status_set("OFF");
			}
		}
		break;

	default:
		/* Don't know what happened */
		return -1;
	}

	snprintf(value, valuelen, "%s", val);

	return 0;
}

/* qx2nut lookup table */
static item_t	ablerex_qx2nut[] = {

	/*
	 * > [Q1\r]
	 * < [(226.0 195.0 226.0 014 49.0 27.5 30.0 00001000\r]
	 *    01234567890123456789012345678901234567890123456
	 *    0         1         2         3         4
	 */

	{ "input.voltage",		   0,	NULL,	"Q1\r",	"",	47,	'(',	"",	1,	5,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "input.voltage.fault",   0,	NULL,	"Q1\r",	"",	47,	'(',	"",	7,	11,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "output.voltage",		   0,	NULL,	"Q1\r",	"",	47,	'(',	"",	13,	17,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "ups.load",			   0,	NULL,	"Q1\r",	"",	47,	'(',	"",	19,	21,	"%.0f",	0,	NULL,	NULL,	NULL },
	{ "input.frequency",		0,	NULL,	"Q1\r",	"",	47,	'(',	"",	23,	26,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "battery.voltage",		0,	NULL,	"Q1\r",	"",	47,	'(',	"",	28,	31,	"%.2f",	0,	NULL,	NULL,	ablerex_battery },
	{ "battery.charge",		   0,	NULL,	"Q1\r",	"",	47,	'(',	"",	28,	31,	"%.2f",	0,	NULL,	NULL,	ablerex_batterycharge },
	{ "ups.temperature",		0,	NULL,	"Q1\r",	"",	47,	'(',	"",	33,	36,	"%.1f",	0,	NULL,	NULL,	NULL },
	/* Status bits */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	38,	38,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Utility Fail (Immediate) */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	39,	39,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Battery Low */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	40,	42,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	ablerex_process_status_bits },	/* Ablerex Bypass/Boost or Buck Active */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	41,	41,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	ablerex_process_status_bits },	/* Ablerex UPS Failed */
	{ "ups.alarm",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	41,	41,	NULL,	0,			NULL,	NULL,	blazer_process_status_bits },	/* UPS Failed */
	{ "ups.type",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	42,	42,	"%s",	QX_FLAG_STATIC,		NULL,	NULL,	blazer_process_status_bits },	/* UPS Type */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	43,	43,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Test in Progress */
	{ "ups.status",			0,	NULL,	"Q1\r",	"",	47,	'(',	"",	44,	44,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	NULL,	blazer_process_status_bits },	/* Shutdown Active */
	{ "ups.beeper.status",		0,	NULL,	"Q1\r",	"",	47,	'(',	"",	45,	45,	"%s",	0,			NULL,	NULL,	blazer_process_status_bits },	/* Beeper status */

	/*
	 * > [F\r]
	 * < [#220.0 000 024.0 50.0\r]
	 *    0123456789012345678901
	 *    0         1         2
	 */

	{ "output.voltage.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	1,	5,	"%.1f",	QX_FLAG_QUICK_POLL,	NULL,	NULL,	NULL },
	{ "output.current.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	7,	9,	"%.1f",	QX_FLAG_QUICK_POLL,	NULL,	NULL,	NULL },
	{ "battery.voltage.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	11,	15,	"%.1f",	QX_FLAG_QUICK_POLL,	NULL,	NULL,	NULL },
	{ "output.frequency.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	17,	20,	"%.1f",	QX_FLAG_QUICK_POLL,	NULL,	NULL,	NULL },
	{ "battery.voltage.low",	0,	NULL,	"F\r",	"",	22,	'#',	"",	1,	2,	"%.2f",	QX_FLAG_QUICK_POLL,	NULL,	NULL,	ablerex_initbattery },
	{ "battery.voltage.high",	0,	NULL,	"F\r",	"",	22,	'#',	"",	2,	3,	"%.2f",	QX_FLAG_QUICK_POLL,	NULL,	NULL,	ablerex_initbattery },

	/* Ablerex */
	{ "output.frequency",	0,	NULL,	"Q5\r",	"",	22,	'(',	"",	1,	18,	"%.1f",	0,	NULL,	NULL,	ablerex_Q5 },
	{ "battery.runtime",	0,	NULL,	"At\r",	"",	0,	'(',	"",	0,	0,	"%d",	0,	NULL,	NULL,	ablerex_At },
/*
 *	//{ "ups.alarm",		0,	NULL,	"Q5\r",	"",	22,	'(',	"",	1,	14,	"%.0f",	0,	QX_FLAG_QUICK_POLL,	NULL,	ablerex_Q5 },
 */
	{ "ups.test.result",	0,	NULL,	"TR\r",	"",	0,	'#',	"",	0,	0,	"%s",	0,	NULL,	NULL,	ablerex_TR },
/*
 *	//{ "output.current",		0,	NULL,	"Q5\r",	"",	22,	'(',	"",	1,	16,	"%.1f",	0,	QX_FLAG_QUICK_POLL,	NULL,	ablerex_Q5 },
 */

	/*
	 * > [I\r]
	 * < [#-------------   ------     VT12046Q  \r]
	 *    012345678901234567890123456789012345678
	 *    0         1         2         3
	 */

	{ "device.mfr",			0,	NULL,	"I\r",	"",	39,	'#',	"",	1,	15,	"%s",	QX_FLAG_STATIC | QX_FLAG_TRIM,	NULL,	NULL,	NULL },
	{ "device.model",		0,	NULL,	"I\r",	"",	39,	'#',	"",	17,	26,	"%s",	QX_FLAG_STATIC | QX_FLAG_TRIM,	NULL,	NULL,	NULL },
	{ "ups.firmware",		0,	NULL,	"I\r",	"",	39,	'#',	"",	28,	37,	"%s",	QX_FLAG_STATIC | QX_FLAG_TRIM,	NULL,	NULL,	NULL },

	/* Instant commands */
	{ "beeper.toggle",		0,	NULL,	"Q\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "load.off",			0,	NULL,	"S.2\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "load.on",			0,	NULL,	"C\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "shutdown.return",		0,	NULL,	"S%s\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	blazer_process_command },
	{ "shutdown.stayoff",		0,	NULL,	"S%sR0000\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	blazer_process_command },
	{ "shutdown.stop",		0,	NULL,	"C\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "test.battery.start",		0,	NULL,	"T%02d\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	blazer_process_command },
	{ "test.battery.start.deep",	0,	NULL,	"TL\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "test.battery.start.quick",	0,	NULL,	"T\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "test.battery.stop",		0,	NULL,	"CT\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },

	/* Server-side settable vars */
	{ "ups.delay.start",		ST_FLAG_RW,	blazer_r_ondelay,	NULL,	"",	0,	0,	"",	0,	0,	DEFAULT_ONDELAY,	QX_FLAG_ABSENT | QX_FLAG_SETVAR | QX_FLAG_RANGE,	NULL,	NULL,	blazer_process_setvar },
	{ "ups.delay.shutdown",		ST_FLAG_RW,	blazer_r_offdelay,	NULL,	"",	0,	0,	"",	0,	0,	DEFAULT_OFFDELAY,	QX_FLAG_ABSENT | QX_FLAG_SETVAR | QX_FLAG_RANGE,	NULL,	NULL,	blazer_process_setvar },

	/* End of structure. */
	{ NULL,				0,	NULL,	NULL,		"",	0,	0,	"",	0,	0,	NULL,	0,	NULL,	NULL,	NULL }
};

/* Testing table */
#ifdef TESTING
static testing_t	ablerex_testing[] = {
	{ "Q1\r",	"(215.0 195.0 230.0 014 49.0 22.7 30.0 00000000\r",	-1 },
	{ "F\r",	"#230.0 000 024.0 50.0\r",	-1 },
	{ "I\r",	"#NOT_A_LIVE_UPS  TESTING    TESTING   \r",	-1 },
	{ "Q\r",	"",	-1 },
	{ "S03\r",	"",	-1 },
	{ "C\r",	"",	-1 },
	{ "S02R0005\r",	"",	-1 },
	{ "S.5R0000\r",	"",	-1 },
	{ "T04\r",	"",	-1 },
	{ "TL\r",	"",	-1 },
	{ "T\r",	"",	-1 },
	{ "CT\r",	"",	-1 },
	{ NULL }
};
#endif	/* TESTING */

/* Subdriver-specific initups */
static void	ablerex_initups(void)
{
	blazer_initups(ablerex_qx2nut);
}

/* Subdriver interface */
subdriver_t	ablerex_subdriver = {
	ABLEREX_VERSION,
	blazer_claim,
	ablerex_qx2nut,
	ablerex_initups,
	NULL,
	blazer_makevartable,
	"ACK",
	NULL,
#ifdef TESTING
	ablerex_testing,
#endif	/* TESTING */
};
