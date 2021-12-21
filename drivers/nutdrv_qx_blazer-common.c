/* nutdrv_qx_blazer-common.c - Common functions/settings for nutdrv_qx_{mecer,megatec,megatec-old,mustek,q1,voltronic-qs,zinto}.{c,h}
 *
 * Copyright (C)
 *   2013 Daniele Pezzini <hyouko@gmail.com>
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

/* == Ranges == */

/* Range for ups.delay.start */
info_rw_t	blazer_r_ondelay[] = {
	{ "0", 0 },
	{ "599940", 0 },
	{ "", 0 }
};

/* Range for ups.delay.shutdown */
info_rw_t	blazer_r_offdelay[] = {
	{ "12", 0 },
	{ "600", 0 },
	{ "", 0 }
};

/* == Support functions == */

/* This function allows the subdriver to "claim" a device: return 1 if the device is supported by this subdriver, else 0. */
int	blazer_claim(void)
{
	/* To tell whether the UPS is supported or not, we'll check both status (Q1/QS/D) and vendor (I/FW?) - provided that we were not told not to do it with the ups.conf flag 'novendor'. */

	item_t	*item = find_nut_info("input.voltage", 0, 0);

	/* Don't know what happened */
	if (!item)
		return 0;

	/* No reply/Unable to get value */
	if (qx_process(item, NULL))
		return 0;

	/* Unable to process value */
	if (ups_infoval_set(item) != 1)
		return 0;

	if (testvar("novendor"))
		return 1;

	/* Vendor */
	item = find_nut_info("ups.firmware", 0, 0);

	/* Don't know what happened */
	if (!item) {
		dstate_delinfo("input.voltage");
		return 0;
	}

	/* No reply/Unable to get value */
	if (qx_process(item, NULL)) {
		dstate_delinfo("input.voltage");
		return 0;
	}

	/* Unable to process value */
	if (ups_infoval_set(item) != 1) {
		dstate_delinfo("input.voltage");
		return 0;
	}

	return 1;
}

/* This function allows the subdriver to "claim" a device: return 1 if the device is supported by this subdriver, else 0.
 * NOTE: this 'light' version only checks for status (Q1/QS/D/..) */
int	blazer_claim_light(void)
{
	/* To tell whether the UPS is supported or not, we'll check just status (Q1/QS/D/..). */

	item_t	*item = find_nut_info("input.voltage", 0, 0);

	/* Don't know what happened */
	if (!item)
		return 0;

	/* No reply/Unable to get value */
	if (qx_process(item, NULL))
		return 0;

	/* Unable to process value */
	if (ups_infoval_set(item) != 1)
		return 0;

	return 1;
}

/* Subdriver-specific flags/vars */
void	blazer_makevartable(void)
{
	addvar(VAR_FLAG, "norating", "Skip reading rating information from UPS");
	addvar(VAR_FLAG, "novendor", "Skip reading vendor information from UPS");

	blazer_makevartable_light();
}

/* Subdriver-specific flags/vars
 * NOTE: this 'light' version only handles vars/flags related to UPS status query (Q1/QS/D/...) */
void	blazer_makevartable_light(void)
{
	addvar(VAR_FLAG, "ignoresab", "Ignore 'Shutdown Active' bit in UPS status");
}

/* Subdriver-specific initups */
void	blazer_initups(item_t *qx2nut)
{
	int	nr, nv, isb;
	item_t	*item;

	nr = testvar("norating");
	nv = testvar("novendor");
	isb = testvar("ignoresab");

	if (!nr && !nv && !isb)
		return;

	for (item = qx2nut; item->info_type != NULL; item++) {

		if (!item->command)
			continue;

		/* norating */
		if (nr && !strncasecmp(item->command, "F\r", strlen("F\r"))) {
			upsdebugx(2, "%s: skipping %s", __func__, item->info_type);
			item->qxflags |= QX_FLAG_SKIP;
		}

		/* novendor */
		if (nv && (!strncasecmp(item->command, "I\r", strlen("I\r"))
		|| !strncasecmp(item->command, "FW?\r", strlen("FW?\r")))
		) {
			upsdebugx(2, "%s: skipping %s", __func__, item->info_type);
			item->qxflags |= QX_FLAG_SKIP;
		}

		/* ignoresab */
		if (isb && !strcasecmp(item->info_type, "ups.status") && item->from == 44 && item->to == 44) {
			upsdebugx(2, "%s: skipping %s ('Shutdown Active' bit)", __func__, item->info_type);
			item->qxflags |= QX_FLAG_SKIP;
		}

	}
}

/* Subdriver-specific initups
 * NOTE: this 'light' version only checks for status (Q1/QS/D/..) related items */
void	blazer_initups_light(item_t *qx2nut)
{
	item_t	*item;

	if (!testvar("ignoresab"))
		return;

	for (item = qx2nut; item->info_type != NULL; item++) {

		if (strcasecmp(item->info_type, "ups.status") || item->from != 44 || item->to != 44)
			continue;

		upsdebugx(2, "%s: skipping %s ('Shutdown Active' bit)", __func__, item->info_type);
		item->qxflags |= QX_FLAG_SKIP;
		break;

	}
}

/* == Preprocess functions == */

/* Preprocess setvars */
int	blazer_process_setvar(item_t *item, char *value, const size_t valuelen)
{
	if (!strlen(value)) {
		upsdebugx(2, "%s: value not given for %s", __func__, item->info_type);
		return -1;
	}

	if (!strcasecmp(item->info_type, "ups.delay.start")) {

		long	ondelay = strtol(value, NULL, 10);

		if (ondelay < 0) {
			upslogx(LOG_ERR, "%s: ondelay '%ld' should not be negative",
				item->info_type, ondelay);
			return -1;
		}

		/* Truncate to minute */
		ondelay -= (ondelay % 60);
		snprintf(value, valuelen, "%ld", ondelay);

	} else if (!strcasecmp(item->info_type, "ups.delay.shutdown")) {

		long	offdelay = strtol(value, NULL, 10);

		if (offdelay < 0) {
			upslogx(LOG_ERR, "%s: offdelay '%ld' should not be negative",
				item->info_type, offdelay);
			return -1;
		}

		/* Truncate to nearest settable value */
		if (offdelay < 60) {
			offdelay -= (offdelay % 6);
		} else {
			offdelay -= (offdelay % 60);
		}

		snprintf(value, valuelen, "%ld", offdelay);

	} else {

		/* Don't know what happened */
		return -1;

	}

	return 0;
}

/* Preprocess instant commands */
int	blazer_process_command(item_t *item, char *value, const size_t valuelen)
{
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif

	if (!strcasecmp(item->info_type, "shutdown.return")) {

		/* Sn: Shutdown after n minutes and then turn on when mains is back
		 * SnRm: Shutdown after n minutes and then turn on after m minutes
		 * Accepted values for n: .2 -> .9 , 01 -> 10
		 * Accepted values for m: 0001 -> 9999
		 * Note: "S01R0001" and "S01R0002" may not work on early (GE) firmware versions.
		 * The failure mode is that the UPS turns off and never returns.
		 * The fix is to push the return value up by 2, i.e. S01R0003, and it will return online properly.
		 * (thus the default of ondelay=3 mins) */

		long	offdelay = strtol(dstate_getinfo("ups.delay.shutdown"), NULL, 10),
				ondelay  = strtol(dstate_getinfo("ups.delay.start"), NULL, 10) / 60;
		char	buf[SMALLBUF] = "";

		if (ondelay <= 0) {

			if (offdelay < 0) {
				upslogx(LOG_ERR, "%s: offdelay '%ld' should not be negative",
					item->info_type, offdelay);
				return -1;
			}

			if (offdelay < 60) {
				snprintf(buf, sizeof(buf), ".%ld", offdelay / 6);
			} else {
				snprintf(buf, sizeof(buf), "%02ld", offdelay / 60);
			}

		} else if (offdelay < 60) {

			if (offdelay < 0) {
				upslogx(LOG_ERR, "%s: offdelay '%ld' should not be negative",
					item->info_type, offdelay);
				return -1;
			}

			snprintf(buf, sizeof(buf), ".%ldR%04ld", offdelay / 6, ondelay);

		} else {

			if (offdelay < 0) {
				upslogx(LOG_ERR, "%s: offdelay '%ld' should not be negative",
					item->info_type, offdelay);
				return -1;
			}

			snprintf(buf, sizeof(buf), "%02ldR%04ld", offdelay / 60, ondelay);

		}

		snprintf(value, valuelen, item->command, buf);

	} else if (!strcasecmp(item->info_type, "shutdown.stayoff")) {

		/* SnR0000
		 * Shutdown after n minutes and stay off
		 * Accepted values for n: .2 -> .9 , 01 -> 10 */

		long	offdelay = strtol(dstate_getinfo("ups.delay.shutdown"), NULL, 10);
		char	buf[SMALLBUF] = "";

		if (offdelay < 0) {
			upslogx(LOG_ERR, "%s: offdelay '%ld' should not be negative",
				item->info_type, offdelay);
			return -1;
		}

		if (offdelay < 60) {
			snprintf(buf, sizeof(buf), ".%ld", offdelay / 6);
		} else {
			snprintf(buf, sizeof(buf), "%02ld", offdelay / 60);
		}

		snprintf(value, valuelen, item->command, buf);

	} else if (!strcasecmp(item->info_type, "test.battery.start")) {

		long	delay = strlen(value) > 0 ? strtol(value, NULL, 10) : 600;

		if ((delay < 60) || (delay > 5940)) {
			upslogx(LOG_ERR, "%s: battery test time '%ld' out of range [60..5940] seconds", item->info_type, delay);
			return -1;
		}

		delay = delay / 60;

		snprintf(value, valuelen, item->command, delay);

	} else {

		/* Don't know what happened */
		return -1;

	}

#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif

	return 0;
}

/* Process status bits */
int	blazer_process_status_bits(item_t *item, char *value, const size_t valuelen)
{
	char	*val = "";

	if (strspn(item->value, "01") != 1) {
		upsdebugx(3, "%s: unexpected value %s@%d->%s", __func__, item->value, item->from, item->value);
		return -1;
	}

	switch (item->from)
	{
	case 38:	/* Utility Fail (Immediate) */

		if (item->value[0] == '1')
			val = "!OL";
		else
			val = "OL";
		break;

	case 39:	/* Battery Low */

		if (item->value[0] == '1')
			val = "LB";
		else
			val = "!LB";
		break;

	case 40:	/* Bypass/Boost or Buck Active */

		if (item->value[0] == '1') {

			double	vi, vo;

			vi = strtod(dstate_getinfo("input.voltage"), NULL);
			vo = strtod(dstate_getinfo("output.voltage"), NULL);

			if (vo < 0.5 * vi) {
				upsdebugx(2, "%s: output voltage too low", __func__);
				return -1;
			} else if (vo < 0.95 * vi) {
				val = "TRIM";
			} else if (vo < 1.05 * vi) {
				val = "BYPASS";
			} else if (vo < 1.5 * vi) {
				val = "BOOST";
			} else {
				upsdebugx(2, "%s: output voltage too high", __func__);
				return -1;
			}

		}

		break;

	case 41:	/* UPS Failed - ups.alarm */

		if (item->value[0] == '1')
			val = "UPS selftest failed!";
		break;

	case 42:	/* UPS Type - ups.type */

		if (item->value[0] == '1')
			val = "offline / line interactive";
		else
			val = "online";
		break;

	case 43:	/* Test in Progress */

		if (item->value[0] == '1')
			val = "CAL";
		else
			val = "!CAL";
		break;

	case 44:	/* Shutdown Active */

		if (item->value[0] == '1')
			val = "FSD";
		else
			val = "!FSD";
		break;

	case 45:	/* Beeper status - ups.beeper.status */

		if (item->value[0] == '1')
			val = "enabled";
		else
			val = "disabled";
		break;

	default:
		/* Don't know what happened */
		return -1;
	}

	snprintf(value, valuelen, "%s", val);

	return 0;
}
