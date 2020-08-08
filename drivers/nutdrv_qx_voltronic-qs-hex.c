/* nutdrv_qx_voltronic-qs-hex.c - Subdriver for Voltronic Power UPSes with QS-Hex protocol
 *
 * Copyright (C)
 *   2014 Daniele Pezzini <hyouko@gmail.com>
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

#include "nutdrv_qx_voltronic-qs-hex.h"

#define VOLTRONIC_QS_HEX_VERSION "Voltronic-QS-Hex 0.10"

/* Support functions */
static int	voltronic_qs_hex_claim(void);
static void	voltronic_qs_hex_initups(void);

/* Answer preprocess functions */
static int	voltronic_qs_hex_preprocess_qs_answer(item_t *item, const int len);
static int	voltronic_qs_hex_char_to_binary(const unsigned char value);

/* Preprocess functions */
static int	voltronic_qs_hex_protocol(item_t *item, char *value, const size_t valuelen);
static int	voltronic_qs_hex_input_output_voltage(item_t *item, char *value, const size_t valuelen);
static int	voltronic_qs_hex_load(item_t *item, char *value, const size_t valuelen);
static int	voltronic_qs_hex_frequency(item_t *item, char *value, const size_t valuelen);
static int	voltronic_qs_hex_battery_voltage(item_t *item, char *value, const size_t valuelen);
static int	voltronic_qs_hex_process_ratings_bits(item_t *item, char *value, const size_t valuelen);


/* == Ranges == */

/* Range for ups.delay.start */
static info_rw_t	voltronic_qs_hex_r_ondelay[] = {
	{ "60", 0 },
	{ "599940", 0 },
	{ "", 0 }
};

/* Range for ups.delay.shutdown */
static info_rw_t	voltronic_qs_hex_r_offdelay[] = {
	{ "12", 0 },
	{ "540", 0 },
	{ "", 0 }
};


/* == qx2nut lookup table == */
static item_t	voltronic_qs_hex_qx2nut[] = {

	/* Query UPS for protocol
	 * > [M\r]
	 * < [P\r]
	 *    01
	 *    0
	 */

	{ "ups.firmware.aux",		0,	NULL,	"M\r",	"",	2,	0,	"",	0,	0,	"PM-%s",	QX_FLAG_STATIC,	NULL,	NULL,	voltronic_qs_hex_protocol },

	/* Query UPS for status
	 * > [QS\r]
	 * < [#6C01 35 6C01 35 03 519A 1312D0 E6 1E 00001001\r]			('P' protocol, after being preprocessed)
	 * < [#6901 6C 6802 6C 00 5FD7 12C000 E4 1E 00001001 00000010\r]	('T' protocol, after being preprocessed)
	 *    01234567890123456789012345678901234567890123456789012345
	 *    0         1         2         3         4         5
	 */

	{ "input.voltage",		0,	NULL,	"QS\r",	"",	47,	'#',	"",	1,	7,	"%.1f",	0,	NULL,	voltronic_qs_hex_preprocess_qs_answer,	voltronic_qs_hex_input_output_voltage },
	{ "output.voltage",		0,	NULL,	"QS\r",	"",	47,	'#',	"",	9,	15,	"%.1f",	0,	NULL,	voltronic_qs_hex_preprocess_qs_answer,	voltronic_qs_hex_input_output_voltage },
	{ "ups.load",			0,	NULL,	"QS\r",	"",	47,	'#',	"",	17,	18,	"%d",	0,	NULL,	voltronic_qs_hex_preprocess_qs_answer,	voltronic_qs_hex_load },
	{ "output.frequency",		0,	NULL,	"QS\r",	"",	47,	'#',	"",	20,	30,	"%.1f",	0,	NULL,	voltronic_qs_hex_preprocess_qs_answer,	voltronic_qs_hex_frequency },
	{ "battery.voltage",		0,	NULL,	"QS\r",	"",	47,	'#',	"",	32,	36,	"%.2f",	0,	NULL,	voltronic_qs_hex_preprocess_qs_answer,	voltronic_qs_hex_battery_voltage },
	/* Status bits */
	{ "ups.status",			0,	NULL,	"QS\r",	"",	47,	'#',	"",	38,	38,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	voltronic_qs_hex_preprocess_qs_answer,	blazer_process_status_bits },	/* Utility Fail (Immediate) */
	{ "ups.status",			0,	NULL,	"QS\r",	"",	47,	'#',	"",	39,	39,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	voltronic_qs_hex_preprocess_qs_answer,	blazer_process_status_bits },	/* Battery Low */
	{ "ups.status",			0,	NULL,	"QS\r",	"",	47,	'#',	"",	40,	40,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	voltronic_qs_hex_preprocess_qs_answer,	blazer_process_status_bits },	/* Bypass/Boost or Buck Active */
	{ "ups.alarm",			0,	NULL,	"QS\r",	"",	47,	'#',	"",	41,	41,	NULL,	0,			NULL,	voltronic_qs_hex_preprocess_qs_answer,	blazer_process_status_bits },	/* UPS Failed */
	{ "ups.type",			0,	NULL,	"QS\r",	"",	47,	'#',	"",	42,	42,	"%s",	QX_FLAG_STATIC,		NULL,	voltronic_qs_hex_preprocess_qs_answer,	blazer_process_status_bits },	/* UPS Type */
	{ "ups.status",			0,	NULL,	"QS\r",	"",	47,	'#',	"",	43,	43,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	voltronic_qs_hex_preprocess_qs_answer,	blazer_process_status_bits },	/* Test in Progress */
	{ "ups.status",			0,	NULL,	"QS\r",	"",	47,	'#',	"",	44,	44,	NULL,	QX_FLAG_QUICK_POLL,	NULL,	voltronic_qs_hex_preprocess_qs_answer,	blazer_process_status_bits },	/* Shutdown Active */
	{ "ups.beeper.status",		0,	NULL,	"QS\r",	"",	47,	'#',	"",	45,	45,	"%s",	0,			NULL,	voltronic_qs_hex_preprocess_qs_answer,	blazer_process_status_bits },	/* Beeper status */
	/* Ratings bits */
	{ "output.frequency.nominal",	0,	NULL,	"QS\r",	"",	56,	'#',	"",	47,	47,	"%.1f",	QX_FLAG_SKIP,		NULL,	voltronic_qs_hex_preprocess_qs_answer,	voltronic_qs_hex_process_ratings_bits },
	{ "battery.voltage.nominal",	0,	NULL,	"QS\r",	"",	56,	'#',	"",	48,	49,	"%.1f",	QX_FLAG_SKIP,		NULL,	voltronic_qs_hex_preprocess_qs_answer,	voltronic_qs_hex_process_ratings_bits },
/*	{ "reserved.1",			0,	NULL,	"QS\r",	"",	56,	'#',	"",	50,	50,	"%s",	QX_FLAG_SKIP,		NULL,	voltronic_qs_hex_preprocess_qs_answer,	voltronic_qs_hex_process_ratings_bits },	*//* Reserved */
/*	{ "reserved.2",			0,	NULL,	"QS\r",	"",	56,	'#',	"",	51,	51,	"%s",	QX_FLAG_SKIP,		NULL,	voltronic_qs_hex_preprocess_qs_answer,	voltronic_qs_hex_process_ratings_bits },	*//* Reserved */
	{ "output.voltage.nominal",	0,	NULL,	"QS\r",	"",	56,	'#',	"",	52,	54,	"%.1f",	QX_FLAG_SKIP,		NULL,	voltronic_qs_hex_preprocess_qs_answer,	voltronic_qs_hex_process_ratings_bits },

	/* Instant commands */
	{ "beeper.toggle",		0,	NULL,	"Q\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "load.off",			0,	NULL,	"S00R0000\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "load.on",			0,	NULL,	"C\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "shutdown.return",		0,	NULL,	"S%s\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	blazer_process_command },
	{ "shutdown.stayoff",		0,	NULL,	"S%sR0000\r",	"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	blazer_process_command },
	{ "shutdown.stop",		0,	NULL,	"C\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD,	NULL,	NULL,	NULL },
	{ "test.battery.start.quick",	0,	NULL,	"T\r",		"",	0,	0,	"",	0,	0,	NULL,	QX_FLAG_CMD | QX_FLAG_SKIP,	NULL,	NULL,	NULL },

	/* Server-side settable vars */
	{ "ups.delay.start",		ST_FLAG_RW,	voltronic_qs_hex_r_ondelay,	NULL,	"",	0,	0,	"",	0,	0,	DEFAULT_ONDELAY,	QX_FLAG_ABSENT | QX_FLAG_SETVAR | QX_FLAG_RANGE,	NULL,	NULL,	blazer_process_setvar },
	{ "ups.delay.shutdown",		ST_FLAG_RW,	voltronic_qs_hex_r_offdelay,	NULL,	"",	0,	0,	"",	0,	0,	DEFAULT_OFFDELAY,	QX_FLAG_ABSENT | QX_FLAG_SETVAR | QX_FLAG_RANGE,	NULL,	NULL,	blazer_process_setvar },

	/* End of structure. */
	{ NULL,				0,	NULL,	NULL,		"",	0,	0,	"",	0,	0,	NULL,	0,	NULL,	NULL,	NULL }
};


/* == Testing table == */
#ifdef TESTING
static testing_t	voltronic_qs_hex_testing[] = {
	{ "QS\r",	"#\x6C\x01 \x35 \x6C\x01 \x35 \x03 \x51\x9A \x28\x02\x12\xD0 \xE6 \x1E \x09\r",	27 },
	{ "M\r",	"P\r",	-1 },
	{ "Q\r",	"",	-1 },
	{ "S00R0000\r",	"",	-1 },
	{ "C\r",	"",	-1 },
	{ "S02R0005\r",	"",	-1 },
	{ "S.5R0000\r",	"N\r",	-1 },
	{ "T\r",	"",	-1 },
	{ NULL }
};
#endif	/* TESTING */


/* == Support functions == */

/* This function allows the subdriver to "claim" a device: return 1 if the device is supported by this subdriver, else 0. */
static int	voltronic_qs_hex_claim(void)
{
	/* We need at least M and QS to run this subdriver */

	/* UPS Protocol */
	item_t	*item = find_nut_info("ups.firmware.aux", 0, 0);

	/* Don't know what happened */
	if (!item)
		return 0;

	/* No reply/Unable to get value */
	if (qx_process(item, NULL))
		return 0;

	/* Unable to process value/Protocol not supported */
	if (ups_infoval_set(item) != 1)
		return 0;

	item = find_nut_info("input.voltage", 0, 0);

	/* Don't know what happened */
	if (!item) {
		dstate_delinfo("ups.firmware.aux");
		return 0;
	}

	/* No reply/Unable to get value */
	if (qx_process(item, NULL)) {
		dstate_delinfo("ups.firmware.aux");
		return 0;
	}

	/* Unable to process value */
	if (ups_infoval_set(item) != 1) {
		dstate_delinfo("ups.firmware.aux");
		return 0;
	}

	return 1;
}

/* Subdriver-specific initups */
static void	voltronic_qs_hex_initups(void)
{
	blazer_initups_light(voltronic_qs_hex_qx2nut);
}


/* == Answer preprocess functions == */

/* Preprocess the answer we got back from the UPS when queried with 'QS\r' */
static int	voltronic_qs_hex_preprocess_qs_answer(item_t *item, const int len)
{
	int	i, token;
	char	refined[SMALLBUF] = "";

	if (len <= 0)
		return len;

	if (item->answer[0] != '#') {
		upsdebugx(4, "%s: wrong leading character [%s: 0x%0x]", __func__, item->info_type, item->answer[0]);
		return -1;
	}

	snprintf(refined, sizeof(refined), "%s", "#");

	/* e.g.: item->answer = "#\x6C\x01 \x35 \x6C\x01 \x35 \x03 \x51\x9A \x28\x02\x12\xD0 \xE6 \x1E \x09\r" */
	upsdebug_hex(4, "read", item->answer, len);

	for (i = 1, token = 1; i < len; i++) {

		/* New token */
		if (item->answer[i] == 0x20) {
			snprintfcat(refined, sizeof(refined), "%s", " ");
			token++;
			continue;
		}

		/* 'Unescape' raw data */
		if (i < len && item->answer[i] == 0x28) {

			switch (item->answer[i + 1])
			{
			case 0x00:	/* Escaped because: CR */
				snprintfcat(refined, sizeof(refined), "%02x", 0x0D);
				break;
			case 0x01:	/* Escaped because: XON */
				snprintfcat(refined, sizeof(refined), "%02x", 0x11);
				break;
			case 0x02:	/* Escaped because: XOFF */
				snprintfcat(refined, sizeof(refined), "%02x", 0x13);
				break;
			case 0x03:	/* Escaped because: LF */
				snprintfcat(refined, sizeof(refined), "%02x", 0x0A);
				break;
			case 0x04:	/* Escaped because: space */
				snprintfcat(refined, sizeof(refined), "%02x", 0x20);
				break;
			default:
				if (token != 10 && token != 11)
					snprintfcat(refined, sizeof(refined), "%02x", ((unsigned char *)item->answer)[i]);
				else
					snprintfcat(refined, sizeof(refined), "%08d", voltronic_qs_hex_char_to_binary(((unsigned char *)item->answer)[i]));
				continue;
			}

			i++;
			continue;

		}

		/* Trailing CR */
		if (item->answer[i] == 0x0D)
			break;

		if (token != 10 && token != 11)
			snprintfcat(refined, sizeof(refined), "%02x", ((unsigned char *)item->answer)[i]);
		else
			snprintfcat(refined, sizeof(refined), "%08d", voltronic_qs_hex_char_to_binary(((unsigned char *)item->answer)[i]));

	}

	if (
		token < 10 ||
		token > 11 ||
		(token == 10 && strlen(refined) != 46) ||
		(token == 11 && strlen(refined) != 55)
	) {
		upsdebugx(2, "noncompliant reply: %s", refined);
		return -1;
	}

	upsdebugx(4, "read: %s", refined);

	/* e.g.: item->answer = "#6C01 35 6C01 35 03 519A 1312D0 E6 1E 00001001" */
	return snprintf(item->answer, sizeof(item->answer), "%s\r", refined);
}

/* Transform a char into its binary form (as an int) */
static int	voltronic_qs_hex_char_to_binary(const unsigned char value)
{
	unsigned char	remainder = value;
	int		ret = 0,
			power = 1;

	while (remainder) {

		if (remainder & 1)
			ret += power;

		power *= 10;
		remainder >>= 1;

	}

	return ret;
}


/* == Preprocess functions == */

/* Protocol used by the UPS */
static int	voltronic_qs_hex_protocol(item_t *item, char *value, const size_t valuelen)
{
	item_t	*unskip;
	int	i;
	const struct {
		const char		*info_type;	/* info_type of the item to be unskipped */
		const unsigned long	flags;		/* qxflags that have to be set in the item */
		const unsigned long	noflags;	/* qxflags that have to be absent in the item */
	} items_to_be_unskipped[] = {
		{ "test.battery.start.quick",	QX_FLAG_CMD,	0 },
		{ "output.frequency.nominal",	0,		0 },
		{ "battery.voltage.nominal",	0,		0 },
		{ "output.voltage.nominal",	0,		0 },
		{ NULL,				0,		0 }
	};

	if (strcasecmp(item->value, "P") && strcasecmp(item->value, "T")) {
		upsdebugx(2, "%s: invalid protocol [%s]", __func__, item->value);
		return -1;
	}

	snprintf(value, valuelen, item->dfl, item->value);

	/* Unskip items supported only by devices that implement 'T' protocol */

	if (!strcasecmp(item->value, "P"))
		return 0;

	for (i = 0; items_to_be_unskipped[i].info_type; i++) {
		unskip = find_nut_info(items_to_be_unskipped[i].info_type, items_to_be_unskipped[i].flags, items_to_be_unskipped[i].noflags);
		/* Don't know what happened */
		if (!unskip)
			return -1;
		unskip->qxflags &= ~QX_FLAG_SKIP;
	}

	return 0;
}

/* Input/Output voltage */
static int	voltronic_qs_hex_input_output_voltage(item_t *item, char *value, const size_t valuelen)
{
	int	val;
	double	ret;
	char	*str_end;

	if (strspn(item->value, "0123456789ABCDEFabcdef ") != strlen(item->value)) {
		upsdebugx(2, "%s: non numerical value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	val = strtol(item->value, &str_end, 16) * strtol(str_end, NULL, 16) / 51;
	ret = val / 256.0;

	snprintf(value, valuelen, item->dfl, ret);

	return 0;
}

/* Device load */
static int	voltronic_qs_hex_load(item_t *item, char *value, const size_t valuelen)
{
	if (strspn(item->value, "0123456789ABCDEFabcdef") != strlen(item->value)) {
		upsdebugx(2, "%s: non numerical value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	snprintf(value, valuelen, item->dfl, strtol(item->value, NULL, 16));

	return 0;
}

/* Output frequency */
static int	voltronic_qs_hex_frequency(item_t *item, char *value, const size_t valuelen)
{
	double	val1, val2, ret;
	char	*str_end;

	if (strspn(item->value, "0123456789ABCDEFabcdef ") != strlen(item->value)) {
		upsdebugx(2, "%s: non numerical value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	val1 = strtol(item->value, &str_end, 16);
	val2 = strtol(str_end, NULL, 16);

	ret = val2 / val1;
	ret = ret > 99.9 ? 99.9 : ret;

	snprintf(value, valuelen, item->dfl, ret);

	return 0;
}

/* Battery voltage */
static int	voltronic_qs_hex_battery_voltage(item_t *item, char *value, const size_t valuelen)
{
	int	val1, val2;
	char	*str_end;

	if (strspn(item->value, "0123456789ABCDEFabcdef ") != strlen(item->value)) {
		upsdebugx(2, "%s: non numerical value [%s: %s]", __func__, item->info_type, item->value);
		return -1;
	}

	val1 = strtol(item->value, &str_end, 16);
	val2 = strtol(str_end, NULL, 16);

	snprintf(value, valuelen, item->dfl, (val1 * val2) / 510.0);

	return 0;
}

/* Ratings bits */
static int	voltronic_qs_hex_process_ratings_bits(item_t *item, char *value, const size_t valuelen)
{
	int	val;
	double	ret;

	if (strspn(item->value, "01") != strlen(item->value)) {
		upsdebugx(3, "%s: unexpected value %s@%d->%s", __func__, item->info_type, item->from, item->value);
		return -1;
	}

	val = strtol(item->value, NULL, 10);

	switch (item->from)
	{
	case 47:	/* Nominal output frequency */
		if (val == 0)	/* 0 -> 50 Hz */
			ret = 50;
		else		/* 1 -> 60 Hz */
			ret = 60;
		break;
	case 48:	/* Nominal battery voltage */
		if (val == 0)		/*  0 -> 12 V */
			ret = 12;
		else if (val == 1)	/*  1 -> 24 V */
			ret = 24;
		else if (val == 10)	/* 10 -> 36 V */
			ret = 36;
		else			/* 11 -> 48 V */
			ret = 48;
		break;
/*	case 50:	*//* Reserved */
/*		break;*/
/*	case 51:	*//* Reserved */
/*		break;*/
	case 52:	/* Nominal output voltage */
		switch (val)
		{
		case   0:
			ret = 110;
			break;
		case   1:
			ret = 120;
			break;
		case  10:
			ret = 220;
			break;
		case  11:
			ret = 230;
			break;
		case 100:
			ret = 240;
			break;
		default:
			/* Unknown */
			return -1;
		}
		break;
	default:
		/* Don't know what happened */
		return -1;
	}

	snprintf(value, valuelen, item->dfl, ret);

	return 0;
}


/* == Subdriver interface == */
subdriver_t	voltronic_qs_hex_subdriver = {
	VOLTRONIC_QS_HEX_VERSION,
	voltronic_qs_hex_claim,
	voltronic_qs_hex_qx2nut,
	voltronic_qs_hex_initups,
	NULL,
	blazer_makevartable_light,
	NULL,
	"N\r",
#ifdef TESTING
	voltronic_qs_hex_testing,
#endif	/* TESTING */
};
