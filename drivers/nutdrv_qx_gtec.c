/* nutdrv_qx_gtec.c - Subdriver for Gtec UPSes
 *
 * Copyright (C)
 *   2025 Lukas Turek <lukas@turek.eu>
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

#include "nutdrv_qx_gtec.h"

#define GTEC_VERSION "Gtec 0.01"

/* Process status letters */
static int	gtec_process_status(item_t *item, char *value, const size_t valuelen)
{
	/* Return value is set by letters A/C/M, exactly one of them *
	 * should be present in the status unless the UPS is off     */
	char	*val = "OFF";
	char	*letters;

	update_status("OL"); /* UPS is on line unless indicated otherwise */
	upsdebugx(10, "%s: Processing status letters %s", __func__, item->value);

	/* NOTE: The main driver calls status_init() and alarm_init()      *
	 * for us in nutdrv_qx.c::upsdrv_updateinfo()                      */

	for (letters = item->value; *letters; letters++)
	{
		switch (*letters)
		{
		case 'A': /* Utility fail */

			val = "!OL"; /* "OB" is not defined in status_info */
			break;

		case 'B': /* Battery low */

			update_status("LB");
			break;

		case 'C': /* Bypass/boost active */

			val = "BYPASS";
			break;

		case 'D': /* UPS failed */

			alarm_set("UPS failed");
			break;

		case 'E': /* Test in progress */

			dstate_setinfo("battery.test.status", "Test in progress");
			break;

		case 'F': /* Shutdown active */

			update_status("FSD");
			break;

		case 'G': /* Site fault */

			alarm_set("Site fault");
			break;

		case 'H': /* EPROM fail */

			alarm_set("EPROM fail");
			break;

		case 'I': /* Test passed - Result: OK */
			dstate_setinfo("battery.test.status", "Test passed - Result: OK");
			break;

		case 'J': /* Test passed - Result: Failed */

			dstate_setinfo("battery.test.status", "Test passed - Result: Failed");
			update_status("RB");
			break;

		case 'K': /* Test Result: Battery disconnected */

			dstate_setinfo("battery.test.status", "Battery disconnected");
			break;

		case 'L': /* Test status unknown */

			dstate_setinfo("battery.test.status", "Test status unknown");
			break;

		case 'M': /* UPS normal mode */

			val = "OL";
			break;

		case 'N': /* UPS overload */

			update_status("OVER");
			break;

		case 'P': /* Battery disconnected */

			alarm_set("Battery disconnected");
			break;

		default:
			upsdebugx(2, "%s: ignoring unknown status character %c", __func__, *letters);
		}
	}

	/* NOTE: The main driver calls status_commit() and alarm_commit()  *
	 * for us in nutdrv_qx.c::upsdrv_updateinfo()                      */

	snprintf(value, valuelen, "%s", val);
	return 0;
}

/* qx2nut lookup table */
static item_t	gtec_qx2nut[] = {

	/*
	 * > [Q4\r]
	 * < [(236.7 243.2 212.1 220.1 230.4 002 001 50.0 371 372 041.3 22.9 IM\r]
	 *    012345678901234567890123456789012345678901234567890123456789012345
	 *    0         1         2         3         4         5         6
	 */

	{ "input.voltage",		0,	NULL,	"Q4\r",	"",	64,	'(',	"",	1,	5,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "input.voltage.fault",	0,	NULL,	"Q4\r",	"",	64,	'(',	"",	19,	23,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "output.voltage",		0,	NULL,	"Q4\r",	"",	64,	'(',	"",	25,	29,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "ups.load",			0,	NULL,	"Q4\r",	"",	64,	'(',	"",	35,	37,	"%.0f",	0,	NULL,	NULL,	NULL },
	{ "input.frequency",		0,	NULL,	"Q4\r",	"",	64,	'(',	"",	39,	42,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "battery.voltage",		0,	NULL,	"Q4\r",	"",	64,	'(',	"",	52,	56,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "ups.temperature",		0,	NULL,	"Q4\r",	"",	64,	'(',	"",	58,	61,	"%.1f",	0,	NULL,	NULL,	NULL },
	{ "ups.status",			0,	NULL,	"Q4\r",	"",	64,	'(',	"",	63,	0,	NULL,	0,	NULL,	NULL,	gtec_process_status },

	/*
	 * > [F\r]
	 * < [#220.0 000 024.0 50.0\r]
	 *    0123456789012345678901
	 *    0         1         2
	 */

	{ "input.voltage.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	1,	5,	"%.0f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },
	{ "input.current.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	7,	9,	"%.1f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },
	{ "battery.voltage.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	11,	15,	"%.1f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },
	{ "input.frequency.nominal",	0,	NULL,	"F\r",	"",	22,	'#',	"",	17,	20,	"%.0f",	QX_FLAG_STATIC,	NULL,	NULL,	NULL },

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
	{ "load.off",			0,	NULL,	"S00R0000\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
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
static testing_t	gtec_testing[] = {
	{ "Q4\r",	"(236.7 243.2 212.1 220.1 230.4 002 001 50.0 371 372 041.3 22.9 IM\r",	-1 },
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
static void	gtec_initups(void)
{
	blazer_initups(gtec_qx2nut);
}

/* Subdriver interface */
subdriver_t	gtec_subdriver = {
	GTEC_VERSION,
	blazer_claim,
	gtec_qx2nut,
	gtec_initups,
	NULL,
	blazer_makevartable,
	"ACK",
	NULL,
#ifdef TESTING
	gtec_testing,
#endif	/* TESTING */
};
