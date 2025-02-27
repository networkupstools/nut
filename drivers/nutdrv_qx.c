/* nutdrv_qx.c - Driver for USB and serial UPS units with Q* protocols
 *
 * Copyright (C)
 *   2013 Daniele Pezzini <hyouko@gmail.com>
 *   2016 Eaton
 * Based on:
 *  usbhid-ups.c - Copyright (C)
 *    2003-2012 Arnaud Quette <arnaud.quette@gmail.com>
 *    2005      John Stamp <kinsayder@hotmail.com>
 *    2005-2006 Peter Selinger <selinger@users.sourceforge.net>
 *    2007-2009 Arjen de Korte <adkorte-guest@alioth.debian.org>
 *  blazer.c - Copyright (C)
 *    2008-2009 Arjen de Korte <adkorte-guest@alioth.debian.org>
 *    2012      Arnaud Quette <ArnaudQuette@Eaton.com>
 *  blazer_ser.c - Copyright (C)
 *    2008      Arjen de Korte <adkorte-guest@alioth.debian.org>
 *  blazer_usb.c - Copyright (C)
 *    2003-2009 Arjen de Korte <adkorte-guest@alioth.debian.org>
 *    2011-2012 Arnaud Quette <arnaud.quette@free.fr>
 *  Masterguard additions
 *    2020-2021 Edgar Fuß, Mathematisches Institut der Universität Bonn <ef@math.uni-bonn.de>
 *  Armac (Richcomm-variant) additions
 *    2021      Tomasz Fortuna <bla@thera.be>
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

#include "config.h"
#include <ctype.h>
#include "main.h"
#include "attribute.h"
#include "nut_float.h"
#include "nut_stdint.h"

/* note: QX_USB/QX_SERIAL set through Makefile */
#ifdef QX_USB
#	include "nut_libusb.h" /* also includes "usb-common.h" */

#	ifdef QX_SERIAL
#		define DRIVER_NAME	"Generic Q* USB/Serial driver"
#	else
#		define	DRIVER_NAME	"Generic Q* USB driver"
#	endif	/* QX_SERIAL */
#else
#	define DRIVER_NAME	"Generic Q* Serial driver"
#endif	/* QX_USB */

#define DRIVER_VERSION	"0.41"

#ifdef QX_SERIAL
#	include "serial.h"
#	define SER_WAIT_SEC	1	/* 3 seconds for Best UPS */
#endif	/* QX_SERIAL */

#include "nutdrv_qx.h"

/* == Subdrivers == */
/* Include all known subdrivers */
#include "nutdrv_qx_bestups.h"
#include "nutdrv_qx_hunnox.h"
#include "nutdrv_qx_innovart31.h"
#include "nutdrv_qx_mecer.h"
#include "nutdrv_qx_megatec.h"
#include "nutdrv_qx_megatec-old.h"
#include "nutdrv_qx_mustek.h"
#include "nutdrv_qx_q1.h"
#include "nutdrv_qx_q2.h"
#include "nutdrv_qx_q6.h"
#include "nutdrv_qx_voltronic.h"
#include "nutdrv_qx_voltronic-qs.h"
#include "nutdrv_qx_voltronic-qs-hex.h"
#include "nutdrv_qx_zinto.h"
#include "nutdrv_qx_masterguard.h"
#include "nutdrv_qx_ablerex.h"
#include "nutdrv_qx_gtec.h"

/* Reference list of available non-USB subdrivers */
static subdriver_t	*subdriver_list[] = {
	&voltronic_subdriver,
	&voltronic_qs_subdriver,
	&voltronic_qs_hex_subdriver,
	&mustek_subdriver,
	&megatec_old_subdriver,
	&bestups_subdriver,
	&mecer_subdriver,
	&megatec_subdriver,
	&zinto_subdriver,
	&masterguard_subdriver,
	&hunnox_subdriver,
	&ablerex_subdriver,
	&innovart31_subdriver,
	&q2_subdriver,
	&q6_subdriver,
	&gtec_subdriver,
	/* Fallback Q1 subdriver */
	&q1_subdriver,
	NULL
};


/* == Driver description structure == */
upsdrv_info_t	upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Daniele Pezzini <hyouko@gmail.com>" \
	"Arnaud Quette <arnaud.quette@gmail.com>" \
	"John Stamp <kinsayder@hotmail.com>" \
	"Peter Selinger <selinger@users.sourceforge.net>" \
	"Arjen de Korte <adkorte-guest@alioth.debian.org>" \
	"Edgar Fuß <ef@math.uni-bonn.de>",
	DRV_BETA,
#ifdef QX_USB
	{ &comm_upsdrv_info, NULL }
#else
	{ NULL }
#endif	/* QX_USB */
};

/* == Data walk modes == */
typedef enum {
	QX_WALKMODE_INIT = 0,
	QX_WALKMODE_QUICK_UPDATE,
	QX_WALKMODE_FULL_UPDATE
} walkmode_t;


/* == Global vars == */
/* Pointer to the active subdriver object (changed in subdriver_matcher() function) */
static subdriver_t	*subdriver = NULL;

static long	pollfreq = DEFAULT_POLLFREQ;
static unsigned int	ups_status = 0;
static bool_t	data_has_changed = FALSE;	/* for SEMI_STATIC data polling */

static time_t	lastpoll;	/* Timestamp the last polling */

#if defined(QX_USB) && !defined(TESTING)
static int	hunnox_step = 0;
#endif	/* QX_USB && !TESTING */

#if defined(QX_USB) && defined(QX_SERIAL)
static int	is_usb = 0;	/* Whether the device is connected through USB (1) or serial (0) */
#endif	/* QX_USB && QX_SERIAL */

static struct {
	char	command[SMALLBUF];	/* Command sent to the UPS to get answer/to execute an instant command */
	char	answer[SMALLBUF];	/* Answer from the UPS, filled at runtime */
} previous_item = { "", "" };	/* Hold the values of the item processed just before the actual one */


/* == Support functions == */
static int	subdriver_matcher(void);
static ssize_t	qx_command(const char *cmd, char *buf, size_t buflen);
static int	qx_process_answer(item_t *item, const size_t len); /* returns just 0 or -1 */
static bool_t	qx_ups_walk(walkmode_t mode);
static void	ups_status_set(void);
static void	ups_alarm_set(void);
static void	qx_set_var(item_t *item);


/* == Struct & data for status processing == */
typedef struct {
	const char	*status_str;			/* UPS status string */
	const unsigned int	status_mask;	/* UPS status mask */
} status_lkp_t;

static status_lkp_t	status_info[] = {
	/* Map status strings to bit masks */
	{ "OL", STATUS(OL) },
	{ "LB", STATUS(LB) },
	{ "RB", STATUS(RB) },
	{ "CHRG", STATUS(CHRG) },
	{ "DISCHRG", STATUS(DISCHRG) },
	{ "BYPASS", STATUS(BYPASS) },
	{ "CAL", STATUS(CALIB) },
	{ "OFF", STATUS(OFF) },
	{ "OVER", STATUS(OVER) },
	{ "TRIM", STATUS(TRIM) },
	{ "BOOST", STATUS(BOOST) },
	{ "FSD", STATUS(FSD) },
	{ NULL, 0 },
};


/* == battery.{charge,runtime} guesstimation == */
/* Support functions */
static int	qx_battery(void);
static int	qx_load(void);
static void	qx_initbattery(void);

/* Battery data */
static struct {
	double	packs;	/* Battery voltage multiplier */
	struct {
		double	act;	/* Actual runtime on battery */
		double	nom;	/* Nominal runtime on battery (full load) */
		double	est;	/* Estimated runtime remaining (full load) */
		double	exp;	/* Load exponent */
	} runt;
	struct {
		double	act;	/* Actual battery voltage */
		double	high;	/* Battery float voltage */
		double	nom;	/* Nominal battery voltage */
		double	low;	/* Battery low voltage */
	} volt;
	struct {
		double	act;	/* Actual battery charge */
		long	time;	/* Recharge time from empty to full */
	} chrg;
} batt = { 1, { -1, -1, 0, 0 }, { -1, -1, -1, -1 }, { -1, 43200 } };

/* Load data */
static struct {
	double	act;	/* Actual load (reported by the UPS) */
	double	low;	/* Idle load */
	double	eff;	/* Effective load */
} load = { 0, 0.1, 1 };

static time_t	battery_lastpoll = 0;
static int	battery_voltage_reports_one_pack = 0, battery_voltage_reports_one_pack_considered = 0;

/* Optionally multiply device-provided "battery.voltage" reading by
 * the "battery.packs" (reading or common override setting) to end up
 * with the dstate value representing the voltage of battery assembly,
 * not that of a single cell/pack (different devices report different
 * physically meaningful values for that reading).
 * This shared method can be referenced from subdriver mapping tables.
 */
int qx_multiply_battvolt(item_t *item, char *value, const size_t valuelen) {
	float s = 0;

	/* Adjusted here or not, this method was called at all
	 * and other code should not multiply again! */
	battery_voltage_reports_one_pack_considered = 1;
	if (!battery_voltage_reports_one_pack || batt.packs < 2) {
		/* (We assume by default that) this device already reports
		 * the sum-total voltage for the battery assembly - so just
		 * pass it on unmodified.
		 * Or we can't reasonably use a "batt.packs" anyway.
		 */
		snprintf(value, valuelen, "%s", item->value);
		return 0;
	}

	if (sscanf(item->value, "%f", &s) != 1) {
		upsdebugx(2, "unparsable ss.ss %s", item->value);
		return -1;
	}

	snprintf(value, valuelen, "%.2f", s * batt.packs);
	return 0;
}

/* Convert kilo-values to their full representation */
int qx_multiply_x1000(item_t *item, char *value, const size_t valuelen) {
	float s = 0;

	if (sscanf(item->value, "%f", &s) != 1) {
		upsdebugx(2, "unparsable ss.ss %s", item->value);
		return -1;
	}

	snprintf(value, valuelen, "%.2f", s * 1000.0);
	return 0;
}

/* Convert minutes to seconds */
int qx_multiply_m2s(item_t *item, char *value, const size_t valuelen) {
	float s = 0;

	if (sscanf(item->value, "%f", &s) != 1) {
		upsdebugx(2, "unparsable ss.ss %s", item->value);
		return -1;
	}

	snprintf(value, valuelen, "%.0f", s * 60.0);
	return 0;
}

/* Fill batt.volt.act and guesstimate the battery charge
 * if it isn't already available. */
static int	qx_battery(void)
{
	const char	*val = dstate_getinfo("battery.voltage");

	if (!val) {
		upsdebugx(2, "%s: unable to get battery.voltage", __func__);
		return -1;
	}

	batt.volt.act = strtod(val, NULL);
	if (!battery_voltage_reports_one_pack_considered) {
		batt.volt.act *= batt.packs;
	}

	if (d_equal(batt.chrg.act, -1) && batt.volt.low > 0 && batt.volt.high > batt.volt.low) {

		batt.chrg.act = 100 * (batt.volt.act - batt.volt.low) / (batt.volt.high - batt.volt.low);

		if (batt.chrg.act < 0) {
			batt.chrg.act = 0;
		}

		if (batt.chrg.act > 100) {
			batt.chrg.act = 100;
		}

		dstate_setinfo("battery.charge", "%.0f", batt.chrg.act);

	}

	return 0;
}

/* Load for battery.{charge,runtime} from runtimecal */
static int	qx_load(void)
{
	const char	*val = dstate_getinfo("ups.load");

	if (!val) {
		upsdebugx(2, "%s: unable to get ups.load", __func__);
		return -1;
	}

	load.act = strtod(val, NULL);

	load.eff = pow(load.act / 100, batt.runt.exp);

	if (load.eff < load.low) {
		load.eff = load.low;
	}

	return 0;
}

/* Init known (readings, configs) and guessed (if needed) battery related values */
static void	qx_initbattery(void)
{
	const char	*val;
	int batt_packs_known = 0;

	val = dstate_getinfo("battery.voltage.high");
	if (val) {
		batt.volt.high = strtod(val, NULL);
	}

	val = dstate_getinfo("battery.voltage.low");
	if (val) {
		batt.volt.low = strtod(val, NULL);
	}

	val = dstate_getinfo("battery.voltage.nominal");
	if (val) {
		batt.volt.nom = strtod(val, NULL);
	}

	/* If no values are available for both battery.voltage.{low,high}
	 * either from the UPS or provided by the user in ups.conf,
	 * but nominal battery.voltage.nom is known,
	 * try to guesstimate them, but announce it! */
	if ( (!d_equal(batt.volt.nom, -1)) && (d_equal(batt.volt.low, -1) || d_equal(batt.volt.high, -1))) {

		upslogx(LOG_INFO, "No values for battery high/low voltages");

		/* Basic formula, which should cover most cases */
		batt.volt.low = 104 * batt.volt.nom / 120;
		/* Per https://www.csb-battery.com.tw/english/01_product/02_detail.php?fid=17&pid=113
		 * a nominally 12V battery can have "float charging voltage"
		 * at 13.5-13.8V and an "equalization charging voltage" (e.g.
		 * to desulphurize) at 14-15V. Note that per nut-names.txt,
		 * the "battery.voltage.high" is the practical 100% charge
		 * value (so equal or a bit less than actual battery.voltage
		 * reported by the device, when the situation is healthy);
		 * it is not the voltage that the battery CAN reach on the
		 * brink of boiling out:
		 */
		batt.volt.high = 130 * batt.volt.nom / 120;

		/* Publish these data too */
		dstate_setinfo("battery.voltage.low", "%.2f", batt.volt.low);
		dstate_setinfo("battery.voltage.high", "%.2f", batt.volt.high);

		upslogx(LOG_INFO, "Using 'guesstimation' (low: %f, high: %f)!",
			batt.volt.low, batt.volt.high);

	}

	val = dstate_getinfo("battery.packs");
	if (val && (strspn(val, "0123456789 .") == strlen(val))) {
		batt.packs = strtod(val, NULL);
		batt_packs_known = 1;
	}

	if (testvar("battery_voltage_reports_one_pack")) {
		battery_voltage_reports_one_pack = 1;
		/* If we already have a battery.voltage reading from the device,
		 * it is not yet "adjusted" to consider the multiplication for
		 * packs (if known; if not - the guesswork and call below for
		 * qx_battery() should take care of it). Note that it is only
		 * a few lines above that we might have learned the user-set
		 * amount of battery packs and that they know the device only
		 * reports a single pack voltage in the protocol.
		 * Even if the qx_multiply_battvolt() method was called before
		 * and set the battery_voltage_reports_one_pack_considered flag,
		 * it is not too relevant until now *for maths*. However it is
		 * important to know that the mapping table for this subdriver
		 * does reference the adjustment method at all (which it would
		 * encounter and set the flag while querying battery.voltage
		 * from device and having a non-NULL reading now).
		 */
		if (battery_voltage_reports_one_pack_considered) {
			val = dstate_getinfo("battery.voltage");
			if (val && batt_packs_known && batt.packs > 1) {
				batt.volt.act = strtod(val, NULL) * batt.packs;
				dstate_setinfo("battery.voltage", "%.2f", batt.volt.act);
			}
		}
	}

	/* Guesstimation: init values if not provided by device/overrides */
	if (!dstate_getinfo("battery.charge") || !dstate_getinfo("battery.runtime")) {

		if (!batt_packs_known) {

			/* qx_battery -> batt.volt.act */
			if (!qx_battery() && (!d_equal(batt.volt.nom, -1))) {

				const double	packs[] = { 120, 100, 80, 60, 48, 36, 30, 24, 18, 12, 8, 6, 4, 3, 2, 1, 0.5, -1 };
				int		i;

				/* The battery voltage will quickly return to
				 * at least the nominal value after discharging them.
				 * For overlapping battery.voltage.low/high ranges
				 * therefore choose the one with the highest multiplier. */
				for (i = 0; packs[i] > 0; i++) {

					if (packs[i] * batt.volt.act > 1.25 * batt.volt.nom) {
						continue;
					}

					if (packs[i] * batt.volt.act < 0.8 * batt.volt.nom) {
						upslogx(LOG_INFO,
							"Can't autodetect number of battery packs [%.0f/%.2f]",
							batt.volt.nom, batt.volt.act);
						break;
					}

					batt.packs = packs[i];
					upslogx(LOG_INFO,
						"Autodetected %.0f as number of battery packs [%.0f/%.2f]",
						batt.packs, batt.volt.nom, batt.volt.act);
					break;

				}

			} else {
				upslogx(LOG_INFO,
					"Can't autodetect number of battery packs [%.0f/%.2f]",
					batt.volt.nom, batt.volt.act);
			}

		}

		/* Update batt.{chrg,volt}.act */
		qx_battery();

		val = getval("runtimecal");
		if (val) {

			double	rh, lh, rl, ll;

			time(&battery_lastpoll);

			if (sscanf(val, "%lf,%lf,%lf,%lf", &rh, &lh, &rl, &ll) < 4) {
				fatalx(EXIT_FAILURE, "Insufficient parameters for runtimecal");
			}

			if ((rl < rh) || (rh <= 0)) {
				fatalx(EXIT_FAILURE, "Parameter out of range (runtime)");
			}

			if ((lh > 100) || (ll > lh) || (ll <= 0)) {
				fatalx(EXIT_FAILURE, "Parameter out of range (load)");
			}

			batt.runt.exp = log(rl / rh) / log(lh / ll);
			upsdebugx(2, "%s: battery runtime exponent: %.3f",
				__func__, batt.runt.exp);

			batt.runt.nom = rh * pow(lh / 100, batt.runt.exp);
			upsdebugx(2, "%s: battery runtime nominal: %.1f",
				__func__, batt.runt.nom);

		} else {

			upslogx(LOG_INFO, "Battery runtime will not be calculated "
				"(runtimecal not set)");
			return;

		}

		val = dstate_getinfo("battery.charge");
		if (!val && (!d_equal(batt.volt.nom, -1))) {
			batt.volt.low = batt.volt.nom;
			batt.volt.high = 1.15 * batt.volt.nom;

			if (qx_battery())
				fatalx(EXIT_FAILURE, "Initial battery charge undetermined");

			val = dstate_getinfo("battery.charge");
		}

		if (val) {
			batt.runt.est = batt.runt.nom * strtod(val, NULL) / 100;
			upsdebugx(2, "%s: battery runtime estimate: %.1f",
				__func__, batt.runt.est);
		} else {
			fatalx(EXIT_FAILURE, "Initial battery charge undetermined");
		}

		val = getval("chargetime");
		if (val) {
			batt.chrg.time = strtol(val, NULL, 10);

			if (batt.chrg.time <= 0) {
				fatalx(EXIT_FAILURE, "Charge time out of range [1..s]");
			}

			upsdebugx(2, "%s: battery charge time: %ld",
				__func__, batt.chrg.time);
		} else {
			upslogx(LOG_INFO,
				"No charge time specified, "
				"using built in default [%ld seconds]",
				batt.chrg.time);
		}

		val = getval("idleload");
		if (val) {
			load.low = strtod(val, NULL) / 100;

			if ((load.low <= 0) || (load.low > 1)) {
				fatalx(EXIT_FAILURE, "Idle load out of range [0..100]");
			}

			upsdebugx(2,
				"%s: minimum load used (idle): %.3f",
				__func__, load.low);
		} else {
			upslogx(LOG_INFO,
				"No idle load specified, using built in default [%.1f %%]",
				100 * load.low);
		}
	}
}


/* == USB communication subdrivers == */
#if defined(QX_USB) && !defined(TESTING)
static usb_communication_subdriver_t	*usb = &usb_subdriver;
static usb_dev_handle			*udev = NULL;
static USBDevice_t			usbdevice;
static USBDeviceMatcher_t		*reopen_matcher = NULL;
static USBDeviceMatcher_t		*regex_matcher = NULL;
static int				langid_fix = -1;

static int	(*subdriver_command)(const char *cmd, char *buf, size_t buflen) = NULL;

/* Cypress communication subdriver */
static int	cypress_command(const char *cmd, char *buf, size_t buflen)
{
	char	tmp[SMALLBUF];
	int	ret = 0;
	size_t	i;

	if (buflen > INT_MAX) {
		upsdebugx(3, "%s: requested to read too much (%" PRIuSIZE "), "
			"reducing buflen to (INT_MAX-1)",
			__func__, buflen);
		buflen = (INT_MAX - 1);
	}

	/* Send command */
	memset(tmp, 0, sizeof(tmp));
	snprintf(tmp, sizeof(tmp), "%s", cmd);

	for (i = 0; i < strlen(tmp); i += (size_t)ret) {

		/* Write data in 8-byte chunks */
		/* ret = usb->set_report(udev, 0, (unsigned char *)&tmp[i], 8); */
		ret = usb_control_msg(udev,
			USB_ENDPOINT_OUT + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
			0x09, 0x200, 0,
			(usb_ctrl_charbuf)&tmp[i], 8, 5000);

		if (ret <= 0) {
			upsdebugx(3, "send: %s (%d)",
				ret ? nut_usb_strerror(ret) : "timeout",
				ret);
			return ret;
		}

	}

	upsdebugx(3, "send: %.*s", (int)strcspn(tmp, "\r"), tmp);

	/* Read reply */
	memset(buf, 0, buflen);

	for (i = 0; (i <= buflen-8) && (memchr(buf, '\r', buflen) == NULL); i += (size_t)ret) {

		/* Read data in 8-byte chunks */
		/* ret = usb->get_interrupt(udev, (unsigned char *)&buf[i], 8, 1000); */
		ret = usb_interrupt_read(udev,
			0x81,
			(usb_ctrl_charbuf)&buf[i], 8, 1000);

		/* Any errors here mean that we are unable to read a reply
		 * (which will happen after successfully writing a command
		 * to the UPS) */
		if (ret <= 0) {
			upsdebugx(3, "read: %s (%d)",
				ret ? nut_usb_strerror(ret) : "timeout",
				ret);
			return ret;
		}

		snprintf(tmp, sizeof(tmp), "read [% 3d]", (int)i);
		upsdebug_hex(5, tmp, &buf[i], (size_t)ret);

	}

	upsdebugx(3, "read: %.*s", (int)strcspn(buf, "\r"), buf);

	if (i > INT_MAX) {
		upsdebugx(3, "%s: read too much (%" PRIuSIZE ")", __func__, i);
		return -1;
	}
	return (int)i;
}

/* SGS communication subdriver */
static int	sgs_command(const char *cmd, char *buf, size_t buflen)
{
	char	tmp[SMALLBUF];
	int	ret = 0;
	size_t  cmdlen, i;

	if (buflen > INT_MAX) {
		upsdebugx(3, "%s: requested to read too much (%" PRIuSIZE "), "
			"reducing buflen to (INT_MAX-1)",
			__func__, buflen);
		buflen = (INT_MAX - 1);
	}

	/* Send command */
	cmdlen = strlen(cmd);

	for (i = 0; i < cmdlen; i += (size_t)ret) {

		memset(tmp, 0, sizeof(tmp));

		/* i and cmdlen are size_t nominally, but diff is not large */
		ret = (int)((cmdlen - i) < 7 ? (cmdlen - i) : 7);

		/* ret is between 0 and 7 */
		tmp[0] = (char)ret;
		memcpy(&tmp[1], &cmd[i], (unsigned char)ret);

		/* Write data in 8-byte chunks */
		ret = usb_control_msg(udev,
			USB_ENDPOINT_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
			0x09, 0x200, 0,
			(usb_ctrl_charbuf)tmp, 8, 5000);

		if (ret <= 0) {
			upsdebugx(3, "send: %s (%d)",
				ret ? nut_usb_strerror(ret) : "timeout",
				ret);
			return ret;
		}

		ret--;

	}

	upsdebugx(3, "send: %.*s", (int)strcspn(cmd, "\r"), cmd);

	/* Read reply */
	memset(buf, 0, buflen);

	for (i = 0; i <= buflen - 8; i += (size_t)ret) {

		memset(tmp, 0, sizeof(tmp));

		/* Read data in 8-byte chunks */
		ret = usb_interrupt_read(udev,
			0x81,
			(usb_ctrl_charbuf)tmp, 8, 1000);

		/* No error!!! */
		/* if (ret == -110) */
		if (ret == LIBUSB_ERROR_TIMEOUT)
			break;

		/* Any errors here mean that we are unable to read a reply
		 * (which will happen after successfully writing a command
		 * to the UPS) */
		if (ret <= 0) {
			upsdebugx(3, "read: %s (%d)",
				ret ? nut_usb_strerror(ret) : "timeout",
				ret);
			return ret;
		}

		/* Every call to read returns 8 bytes
		 * -> actually returned bytes: */
		ret = tmp[0] <= 7 ? tmp[0] : 7;

		if (ret > 0)
			memcpy(&buf[i], &tmp[1], (unsigned char)ret);

		snprintf(tmp, sizeof(tmp), "read [% 3d]", (int)i);
		upsdebug_hex(5, tmp, &buf[i], (size_t)ret);

	}

	/* If the reply lacks the expected terminating CR, add it (if there's enough space) */
	if (i && memchr(buf, '\r', i) == NULL) {
		upsdebugx(4, "%s: the reply lacks the expected terminating CR.", __func__);
		if (i < buflen - 1) {
			upsdebugx(4, "%s: adding missing terminating CR.", __func__);
			buf[i++] = '\r';
			buf[i] = 0;
		}
	}

	upsdebugx(3, "read: %.*s", (int)strcspn(buf, "\r"), buf);

	if (i > INT_MAX) {
		upsdebugx(3, "%s: read too much (%" PRIuSIZE ")", __func__, i);
		return -1;
	}
	return (int)i;
}

/* Phoenix communication subdriver */
static int	phoenix_command(const char *cmd, char *buf, size_t buflen)
{
	char	tmp[SMALLBUF];
	int	ret;
	size_t	i;

	if (buflen > INT_MAX) {
		upsdebugx(3, "%s: requested to read too much (%" PRIuSIZE "), "
			"reducing buflen to (INT_MAX-1)",
			__func__, buflen);
		buflen = (INT_MAX - 1);
	}

	for (i = 0; i < 8; i++) {

		/* Read data in 8-byte chunks */
		/* ret = usb->get_interrupt(udev, (unsigned char *)tmp, 8, 1000); */
		ret = usb_interrupt_read(udev,
			0x81,
			(usb_ctrl_charbuf)tmp, 8, 1000);

		/* This USB to serial implementation is crappy.
		 * In order to read correct replies we need to flush the
		 * output buffers of the converter until we get no more
		 * data (e.g. it times out). */
		switch (ret)
		{
		case LIBUSB_ERROR_PIPE:	/* Broken pipe */
			usb_clear_halt(udev, 0x81);
			break;

		case LIBUSB_ERROR_TIMEOUT:	/* Connection timed out */
			break;

		default:
			break;
		}

		if (ret < 0) {
			upsdebugx(3, "flush: %s (%d)",
				nut_usb_strerror(ret), ret);
			break;
		}

		upsdebug_hex(4, "dump", tmp, (size_t)ret);

	}

	/* Send command */
	memset(tmp, 0, sizeof(tmp));
	snprintf(tmp, sizeof(tmp), "%s", cmd);

	for (i = 0; i < strlen(tmp); i += (size_t)ret) {

		/* Write data in 8-byte chunks */
		/* ret = usb->set_report(udev, 0, (unsigned char *)&tmp[i], 8); */
		ret = usb_control_msg(udev,
			USB_ENDPOINT_OUT + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
			0x09, 0x200, 0, (usb_ctrl_charbuf)&tmp[i], 8, 1000);

		if (ret <= 0) {
			upsdebugx(3, "send: %s (%d)",
				ret ? nut_usb_strerror(ret) : "timeout", ret);
			return ret;
		}

	}

	upsdebugx(3, "send: %.*s", (int)strcspn(tmp, "\r"), tmp);

	/* Read reply */
	memset(buf, 0, buflen);

	for (i = 0; (i <= buflen-8) && (memchr(buf, '\r', buflen) == NULL); i += (size_t)ret) {

		/* Read data in 8-byte chunks */
		/* ret = usb->get_interrupt(udev, (unsigned char *)&buf[i], 8, 1000); */
		ret = usb_interrupt_read(udev,
			0x81,
			(usb_ctrl_charbuf)&buf[i], 8, 1000);

		/* Any errors here mean that we are unable to read a reply
		 * (which will happen after successfully writing a command
		 * to the UPS) */
		if (ret <= 0) {
			upsdebugx(3, "read: %s (%d)",
				ret ? nut_usb_strerror(ret) : "timeout", ret);
			return ret;
		}

		snprintf(tmp, sizeof(tmp), "read [% 3d]", (int)i);
		upsdebug_hex(5, tmp, &buf[i], (size_t)ret);

	}

	upsdebugx(3, "read: %.*s", (int)strcspn(buf, "\r"), buf);

	if (i > INT_MAX) {
		upsdebugx(3, "%s: read too much (%" PRIuSIZE ")", __func__, i);
		return -1;
	}
	return (int)i;
}

/* Ippon communication subdriver */
static int	ippon_command(const char *cmd, char *buf, size_t buflen)
{
	char	tmp[64];
	int	ret;
	size_t	i, len;

	if (buflen > INT_MAX) {
		upsdebugx(3, "%s: requested to read too much (%" PRIuSIZE "), "
			"reducing buflen to (INT_MAX-1)",
			__func__, buflen);
		buflen = (INT_MAX - 1);
	}

	/* Send command */
	snprintf(tmp, sizeof(tmp), "%s", cmd);

	for (i = 0; i < strlen(tmp); i += (size_t)ret) {

		/* Write data in 8-byte chunks */
		ret = usb_control_msg(udev,
			USB_ENDPOINT_OUT + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
			0x09, 0x2, 0, (usb_ctrl_charbuf)&tmp[i], 8, 1000);

		if (ret <= 0) {
			upsdebugx(3, "send: %s (%d)",
				(ret != LIBUSB_ERROR_TIMEOUT) ? nut_usb_strerror(ret) : "Connection timed out",
				ret);
			return ret;
		}

	}

	upsdebugx(3, "send: %.*s", (int)strcspn(tmp, "\r"), tmp);

	/* Read all 64 bytes of the reply in one large chunk */
	ret = usb_interrupt_read(udev,
		0x81,
		(usb_ctrl_charbuf)tmp, sizeof(tmp), 1000);

	/* Any errors here mean that we are unable to read a reply
	 * (which will happen after successfully writing a command
	 * to the UPS) */
	if (ret <= 0) {
		upsdebugx(3, "read: %s (%d)",
			(ret != LIBUSB_ERROR_TIMEOUT) ? nut_usb_strerror(ret) : "Connection timed out",
			ret);
		return ret;
	}

	/* As Ippon will always return 64 bytes in response,
	 * we have to calculate and return length of actual
	 * response data here.
	 * Empty response will look like 0x00 0x0D, otherwise
	 * it will be data string terminated by 0x0D. */

	for (i = 0, len = 0; i < (size_t)ret; i++) {

		if (tmp[i] != '\r')
			continue;

		len = ++i;
		break;

	}

	/* Just in case there wasn't any '\r', fallback to string length, if any */
	if (!len)
		len = strlen(tmp);

	upsdebug_hex(5, "read", tmp, (size_t)len);
	upsdebugx(3, "read: %.*s", (int)strcspn(tmp, "\r"), tmp);

	len = len < buflen ? len : buflen - 1;

	memset(buf, 0, buflen);
	memcpy(buf, tmp, len);

	/* If the reply lacks the expected terminating CR, add it (if there's enough space) */
	if (len && memchr(buf, '\r', len) == NULL) {
		upsdebugx(4, "%s: the reply lacks the expected terminating CR.", __func__);
		if (len < buflen - 1) {
			upsdebugx(4, "%s: adding missing terminating CR.", __func__);
			buf[len++] = '\r';
			buf[len] = 0;
		}
	}

	if (len > INT_MAX) {
		upsdebugx(3, "%s: read too much (%" PRIuSIZE ")", __func__, len);
		return -1;
	}
	return (int)len;
}

static int 	hunnox_protocol(int asking_for)
{
	char	buf[1030];

	int langid_fix_local = 0x0409;

	if (langid_fix != -1) {
		langid_fix_local = langid_fix;
	}

	switch (hunnox_step) {
		case 0:
			upsdebugx(3, "asking for: %02X", (unsigned int)0x00);
			usb_get_string(udev, 0x00,
				langid_fix_local, (usb_ctrl_charbuf)buf, 1026);
			usb_get_string(udev, 0x00,
				langid_fix_local, (usb_ctrl_charbuf)buf, 1026);
			usb_get_string(udev, 0x01,
				langid_fix_local, (usb_ctrl_charbuf)buf, 1026);
			usleep(10000);
			break;
		case 1:
			if (asking_for != 0x0d) {
				upsdebugx(3, "asking for: %02X", (unsigned int)0x0d);
				usb_get_string(udev, 0x0d,
					langid_fix_local, (usb_ctrl_charbuf)buf, 102);
			}
			break;
		case 2:
			if (asking_for != 0x03) {
				upsdebugx(3, "asking for: %02X", (unsigned int)0x03);
				usb_get_string(udev, 0x03,
					langid_fix_local, (usb_ctrl_charbuf)buf, 102);
			}
			break;
		case 3:
			if (asking_for != 0x0c) {
				upsdebugx(3, "asking for: %02X", (unsigned int)0x0c);
				usb_get_string(udev, 0x0c,
					langid_fix_local, (usb_ctrl_charbuf)buf, 102);
			}
			break;
		default:
			hunnox_step = 0;
	}
	hunnox_step++;
	if (hunnox_step > 3) {
		hunnox_step = 1;
	}

	return 0;
}

/* Krauler communication subdriver */
static int	krauler_command(const char *cmd, char *buf, size_t buflen)
{
	/* Still not implemented:
	 * 0x6	T<n>	(don't know how to pass the parameter)
	 * 0x68 and 0x69 both cause shutdown after an undefined interval */
	const struct {
		const char	*str;	/* Megatec command */
		const int	index;	/* Krauler string index for this command */
		const char	prefix;	/* Character to replace the first byte in reply */
	} command[] = {
		{ "Q1\r", 0x03, '(' },
		{ "F\r", 0x0d, '#' },
		{ "I\r", 0x0c, '#' },
		{ "T\r", 0x04, '\r' },
		{ "TL\r", 0x05, '\r' },
		{ "Q\r", 0x07, '\r' },
		{ "C\r", 0x0b, '\r' },
		{ "CT\r", 0x0b, '\r' },
		{ NULL, 0, '\0' }
	};

	int	i;

	upsdebugx(3, "send: %.*s", (int)strcspn(cmd, "\r"), cmd);

	if (buflen > INT_MAX) {
		upsdebugx(3, "%s: requested to read too much (%" PRIuSIZE "), "
			"reducing buflen to (INT_MAX-1)",
			__func__, buflen);
		buflen = (INT_MAX - 1);
	}

	for (i = 0; command[i].str; i++) {

		int	retry;

		if (strcmp(cmd, command[i].str)) {
			continue;
		}

		for (retry = 0; retry < 10; retry++) {

			int	ret;

			if (langid_fix != -1) {
				/* Apply langid_fix value */
				ret = usb_get_string(udev,
					command[i].index, langid_fix,
					(usb_ctrl_charbuf)buf, buflen);
			} else {
				ret = usb_get_string_simple(udev,
					command[i].index,
					(usb_ctrl_charbuf)buf, buflen);
			}

			if (ret <= 0) {
				upsdebugx(3, "read: %s (%d)",
					ret ? nut_usb_strerror(ret) : "timeout", ret);
				return ret;
			}

			/* This may serve in the future */
			upsdebugx(1, "received %d (%d)", ret, buf[0]);

			if (langid_fix != -1) {
				unsigned int	di, si, size;
				/* Limit this check, at least for now */
				/* Invalid receive size - message corrupted */
				if (ret != buf[0]) {
					upsdebugx(1, "size mismatch: %d / %d", ret, buf[0]);
					continue;
				}

				/* Simple unicode -> ASCII inplace conversion
				 * FIXME: this code is at least shared with mge-shut/libshut
				 * Create a common function? */
				size = (unsigned int)buf[0];
				for (di = 0, si = 2; si < size; si += 2) {

					if (di >= (buflen - 1))
						break;

					if (buf[si + 1])	/* high byte */
						buf[di++] = '?';
					else
						buf[di++] = buf[si];

				}

				/* Note: effective range of di should be unsigned char */
				buf[di] = 0;
				ret = (int)di;
			}

			/* If the reply lacks the expected terminating CR, add it (if there's enough space) */
			if (ret && memchr(buf, '\r', ret) == NULL) {
				upsdebugx(4, "%s: the reply lacks the expected terminating CR.", __func__);
				if ((size_t)ret < buflen - 1) {
					upsdebugx(4, "%s: adding missing terminating CR.", __func__);
					buf[ret++] = '\r';
					buf[ret] = 0;
				}
			}

			/* "UPS No Ack" has a special meaning */
			if (
				strcspn(buf, "\r") == 10 &&
				!strncasecmp(buf, "UPS No Ack", 10)
			) {
				upsdebugx(3, "read: %.*s", (int)strcspn(buf, "\r"), buf);
				continue;
			}

			/* Replace the first byte of what we received with the correct one */
			buf[0] = command[i].prefix;

			upsdebug_hex(5, "read", buf, (size_t)ret);
			upsdebugx(3, "read: %.*s", (int)strcspn(buf, "\r"), buf);

			return ret;

		}

		return 0;

	}

	/* Echo the unknown command back */
	upsdebugx(3, "read: %.*s", (int)strcspn(cmd, "\r"), cmd);
	return snprintf(buf, buflen, "%s", cmd);
}

/* Fabula communication subdriver */
static int	fabula_command(const char *cmd, char *buf, size_t buflen)
{
	const struct {
		const char	*str;	/* Megatec command */
		const int	index;	/* Fabula string index for this command */
	} commands[] = {
		{ "Q1\r",	0x03, },	/* Status */
		{ "F\r",	0x0d, },	/* Ratings */
		{ "I\r",	0x0c, },	/* Vendor infos */
		{ "Q\r",	0x07, },	/* Beeper toggle */
		{ "C\r",	0x0a, },	/* Cancel shutdown/Load on [0x(0..F)A]*/
		{ NULL, 0 }
	};
	int	i, ret, index = 0;

	upsdebugx(3, "send: %.*s", (int)strcspn(cmd, "\r"), cmd);

	if (buflen > INT_MAX) {
		upsdebugx(3, "%s: requested to read too much (%" PRIuSIZE "), "
			"reducing buflen to (INT_MAX-1)",
			__func__, buflen);
		buflen = (INT_MAX - 1);
	}

	for (i = 0; commands[i].str; i++) {

		if (strcmp(cmd, commands[i].str))
			continue;

		index = commands[i].index;
		break;

	}

	if (!index) {

		int	val2 = -1;
		double	val1 = -1;

		/* Shutdowns */
		if (
			sscanf(cmd, "S%lfR%d\r", &val1, &val2) == 2 ||
			sscanf(cmd, "S%lf\r", &val1) == 1
		) {

			double	delay;

			/* 0x(1+)0 -> shutdown.stayoff (SnR0000)
			 * 0x(1+)8 -> shutdown.return (Sn[Rm], m != 0)
			 *   [delay before restart is always 10 seconds]
			 * +0x10 (16dec) = next megatec delay
			 *   (min .5 = hex 0x1*; max 10 = hex 0xF*) -> n < 1 ? -> n += .1; n >= 1 ? -> n += 1 */

			/* delay: [.5..10] (-> seconds: [30..600]) */
			delay = val1 < .5 ? .5 : val1 > 10 ? 10 : val1;

			if (delay < 1)
				index = 16 + round((delay - .5) * 10) * 16;
			else
				index = 96 + (delay - 1) * 16;

			/* shutdown.return (Sn[Rm], m != 0) */
			if (val2)
				index += 8;

		/* Unknown commands */
		} else {

			/* Echo the unknown command back */
			upsdebugx(3, "read: %.*s", (int)strcspn(cmd, "\r"), cmd);
			return snprintf(buf, buflen, "%s", cmd);

		}

	}

	upsdebugx(4, "command index: 0x%02x", (unsigned int)index);

	/* Send command/Read reply */
	ret = usb_get_string_simple(udev, index, (usb_ctrl_charbuf)buf, buflen);

	if (ret <= 0) {
		upsdebugx(3, "read: %s (%d)",
			ret ? nut_usb_strerror(ret) : "timeout", ret);
		return ret;
	}

	/* If the reply lacks the expected terminating CR, add it (if there's enough space) */
	if (memchr(buf, '\r', ret) == NULL) {
		upsdebugx(4, "%s: the reply lacks the expected terminating CR.", __func__);
		if ((size_t)ret < buflen - 1) {
			upsdebugx(4, "%s: adding missing terminating CR.", __func__);
			buf[ret++] = '\r';
			buf[ret] = 0;
		}
	}

	upsdebug_hex(5, "read", buf, (size_t)ret);
	upsdebugx(3, "read: %.*s", (int)strcspn(buf, "\r"), buf);

	/* The UPS always replies "UPS No Ack" when a supported command
	 * is issued (either if it fails or if it succeeds).. */
	if (
		strcspn(buf, "\r") == 10 &&
		!strncasecmp(buf, "UPS No Ack", 10)
	) {
		/* ..because of that, always return 0 (with buf empty,
		 * as if it was a timeout): queries will see it as a failure,
		 * instant commands ('megatec' protocol) as a success */
		memset(buf, 0, buflen);
		return 0;
	}

	return ret;
}

/* Hunnox communication subdriver, based on Fabula code above so repeats
 * much of it currently. Possible future optimization is to refactor shared
 * code into new routines to be called from both (or more) methods.*/
static int	hunnox_command(const char *cmd, char *buf, size_t buflen)
{
	/* The hunnox_patch was an argument in initial implementation of PR #638
	 * which added "hunnox" support; keeping it fixed here helps to visibly
	 * track the modifications compared to original fabula_command() e.g. to
	 * facilitate refactoring commented above, in the future.
	 */
/*	char hunnox_patch = 1; */
	const struct {
		const char	*str;	/* Megatec command */
		const int	index;	/* Fabula string index for this command */
	} commands[] = {
		{ "Q1\r",	0x03, },	/* Status */
		{ "F\r",	0x0d, },	/* Ratings */
		{ "I\r",	0x0c, },	/* Vendor infos */
		{ "Q\r",	0x07, },	/* Beeper toggle */
		{ "C\r",	0x0a, },	/* Cancel shutdown/Load on [0x(0..F)A]*/
		{ NULL, 0 }
	};
	int	i, ret, index = 0;

	upsdebugx(3, "send: %.*s", (int)strcspn(cmd, "\r"), cmd);

	if (buflen > INT_MAX) {
		upsdebugx(3, "%s: requested to read too much (%" PRIuSIZE "), "
			"reducing buflen to (INT_MAX-1)",
			__func__, buflen);
		buflen = (INT_MAX - 1);
	}

	for (i = 0; commands[i].str; i++) {

		if (strcmp(cmd, commands[i].str))
			continue;

		index = commands[i].index;
		break;

	}

	if (!index) {

		int	val2 = -1;
		double	val1 = -1;

		/* Shutdowns */
		if (
			sscanf(cmd, "S%lfR%d\r", &val1, &val2) == 2 ||
			sscanf(cmd, "S%lf\r", &val1) == 1
		) {

			double	delay;

			/* 0x(1+)0 -> shutdown.stayoff (SnR0000)
			 * 0x(1+)8 -> shutdown.return (Sn[Rm], m != 0)
			 *   [delay before restart is always 10 seconds]
			 * +0x10 (16dec) = next megatec delay
			 *   (min .5 = hex 0x1*; max 10 = hex 0xF*) -> n < 1 ? -> n += .1; n >= 1 ? -> n += 1 */

			/* delay: [.5..10] (-> seconds: [30..600]) */
			delay = val1 < .5 ? .5 : val1 > 10 ? 10 : val1;

			if (delay < 1)
				index = 16 + round((delay - .5) * 10) * 16;
			else
				index = 96 + (delay - 1) * 16;

			/* shutdown.return (Sn[Rm], m != 0) */
			if (val2)
				index += 8;

		/* Unknown commands */
		} else {

			/* Echo the unknown command back */
			upsdebugx(3, "read: %.*s", (int)strcspn(cmd, "\r"), cmd);
			return snprintf(buf, buflen, "%s", cmd);

		}

	}

	upsdebugx(4, "command index: 0x%02x", (unsigned int)index);

/*	if (hunnox_patch) { */
		/* Enable lock-step protocol for Hunnox */
		if (hunnox_protocol(index) != 0) {
			return 0;
		}

		/* Seems that if we inform a large buffer, the USB locks.
		 * This value was captured from the Windows "official" client.
		 * Note this should not be a problem programmatically: it just
		 * means that the caller reserved a longer buffer that we need
		 * in practice to write a response into.
		 */
		if (buflen > 102) {
			buflen = 102;
		}
/*	} */

	/* Send command/Read reply */
	if (langid_fix != -1) {
		ret = usb_get_string(udev,
			index, langid_fix, (usb_ctrl_charbuf)buf, buflen);
	} else {
		ret = usb_get_string_simple(udev,
			index, (usb_ctrl_charbuf)buf, buflen);
	}

	if (ret <= 0) {
		upsdebugx(3, "read: %s (%d)",
			ret ? nut_usb_strerror(ret) : "timeout",
			ret);
		return ret;
	}

/*	if (hunnox_patch) { */
		if (langid_fix != -1) {
			unsigned int	di, si, size;

			/* Limit this check, at least for now */
			/* Invalid receive size - message corrupted */
			if (ret != buf[0]) {
				upsdebugx(1, "size mismatch: %d / %d", ret, buf[0]);
				return 0;
			}

			/* Simple unicode -> ASCII inplace conversion
			 * FIXME: this code is at least shared with mge-shut/libshut
			 * Create a common function? */
			size = (unsigned int)buf[0];
			for (di = 0, si = 2; si < size; si += 2) {
				if (di >= (buflen - 1))
					break;

				if (buf[si + 1])	/* high byte */
					buf[di++] = '?';
				else
					buf[di++] = buf[si];
			}

			/* Note: effective range of di should be unsigned char */
			buf[di] = 0;
			ret = (int)di;
		}
/*	} */

	upsdebug_hex(5, "read", buf, (size_t)ret);
	upsdebugx(3, "read: %.*s", (int)strcspn(buf, "\r"), buf);

	/* The UPS always replies "UPS No Ack" when a supported command
	 * is issued (either if it fails or if it succeeds).. */
	if (
		strcspn(buf, "\r") == 10 &&
		!strncasecmp(buf, "UPS No Ack", 10)
	) {
		/* ..because of that, always return 0 (with buf empty,
		 * as if it was a timeout): queries will see it as a failure,
		 * instant commands ('megatec' protocol) as a success */
		memset(buf, 0, buflen);
		return 0;
	}

	return ret;
}

/* Fuji communication subdriver */
static int	fuji_command(const char *cmd, char *buf, size_t buflen)
{
	unsigned char	tmp[8];
	char		command[SMALLBUF] = "",
			read[SMALLBUF] = "";
	int		ret, val2;
	unsigned char	answer_len;
	double		val1;
	size_t		i;
	const struct {
		const char	*command;	/* Megatec command */
		const unsigned char	answer_len;	/* Expected length of the answer
										 * to the ongoing query */
	} query[] = {
		{ "Q1",	47 },
		{ "F",	22 },
		{ "I",	39 },
		{ NULL, 0 }
	};

	if (buflen > INT_MAX) {
		upsdebugx(3, "%s: requested to read too much (%" PRIuSIZE "), "
			"reducing buflen to (INT_MAX-1)",
			__func__, buflen);
		buflen = (INT_MAX - 1);
	}

	/*
	 * Queries (b1..b8) sent (as a 8-bytes interrupt) to the UPS
	 * adopt the following scheme:
	 *
	 *	b1:		0x80
	 *	b2:		0x06
	 *	b3:		<LEN>
	 *	b4:		0x03
	 *	b5..bn:		<COMMAND>
	 *	bn+1..b7:	[<PADDING>]
	 *	b8:		<ANSWER_LEN>
	 *
	 * Where:
	 *	<LEN>		Length (in Hex) of the command (without the trailing CR) + 1
	 *	<COMMAND>	Command/query (without the trailing CR)
	 *	[<PADDING>]	0x00 padding to the 7th byte
	 *	<ANSWER_LEN>	Expected length (in Hex) of the answer to the ongoing
	 *	                query (0 when no reply is expected, i.e. commands)
	 *
	 * Replies to queries (commands are followed by action without
	 * any reply) are sent from the UPS (in 8-byte chunks) with
	 * 0x00 padding after the trailing CR to full 8 bytes.
	 *
	 */

	/* Send command */

	/* Remove the CR */
	snprintf(command, sizeof(command), "%.*s", (int)strcspn(cmd, "\r"), cmd);

	/* Length of the command that will be sent to the UPS can be
	 * at most: 8 - 5 (0x80, 0x06, <LEN>, 0x03, <ANSWER_LEN>) = 3.
	 * As a consequence also 'SnRm' commands (shutdown.{return,stayoff}
	 * and load.off) are not supported.
	 * So, map all the 'SnRm' shutdown.returns (m != 0) as the
	 * corresponding 'Sn' commands, meanwhile ignoring ups.delay.start
	 * and making the UPS turn on the load as soon as power is back. */
	if (sscanf(cmd, "S%lfR%d\r", &val1, &val2) == 2 && val2) {
		upsdebugx(4, "%s: trimming '%s' to '%.*s'", __func__, command, 3, command);
		command[3] = 0;
	}
	/* Too long command */
	if (strlen(command) > 3) {
		/* Be 'megatec-y': echo the unsupported command back */
		upsdebugx(3, "%s: unsupported command %s", __func__, command);
		return snprintf(buf, buflen, "%s", cmd);
	}

	/* Expected length of the answer to the ongoing query
	 * (0 when no reply is expected, i.e. commands) */
	answer_len = 0;
	for (i = 0; query[i].command; i++) {

		if (strcmp(command, query[i].command))
			continue;

		answer_len = query[i].answer_len;
		break;

	}

	memset(tmp, 0, sizeof(tmp));

	/* 0x80 */
	tmp[0] = 0x80;
	/* 0x06 */
	tmp[1] = 0x06;
	/* <LEN>; per above under 3 */
	tmp[2] = (unsigned char)strlen(command) + 1;
	/* 0x03 */
	tmp[3] = 0x03;
	/* <COMMAND> */
	memcpy(&tmp[4], command, strlen(command));
	/* <ANSWER_LEN> */
	tmp[7] = answer_len;

	upsdebug_hex(4, "command", (char *)tmp, 8);

	/* Write data */
	ret = usb_interrupt_write(udev,
		USB_ENDPOINT_OUT | 2,
		(const usb_ctrl_charbuf)tmp,
		8, USB_TIMEOUT);

	if (ret <= 0) {
		upsdebugx(3, "send: %s (%d)",
			ret ? nut_usb_strerror(ret) : "timeout", ret);
		return ret;
	}

	upsdebugx(3, "send: %s", command);

	/* Read reply */

	memset(buf, 0, buflen);

	for (i = 0; (i <= buflen - 8) && (memchr(buf, '\r', buflen) == NULL); i += (size_t)ret) {

		/* Read data in 8-byte chunks */
		ret = usb_interrupt_read(udev,
			USB_ENDPOINT_IN | 1,
			(usb_ctrl_charbuf)&buf[i], 8, 1000);

		/* Any errors here mean that we are unable to read a reply
		 * (which will happen after successfully writing a command
		 * to the UPS) */
		if (ret <= 0) {
			upsdebugx(3, "read: %s (%d)",
				ret ? nut_usb_strerror(ret) : "timeout", ret);
			return ret;
		}

		snprintf(read, sizeof(read), "read [%3d]", (int)i);
		upsdebug_hex(5, read, &buf[i], (size_t)ret);

	}

	upsdebugx(3, "read: %.*s", (int)strcspn(buf, "\r"), buf);

	/* As Fuji units return the reply in 8-byte chunks always padded to the 8th byte with 0x00, we need to calculate and return the length of the actual response here. */
	return (int)strlen(buf);
}

/* Phoenixtec (Masterguard) communication subdriver */
static int	phoenixtec_command(const char *cmd, char *buf, size_t buflen)
{
	int ret;
	char *p, *e = NULL;
	char *l[] = { "T", "TL", "S", "C", "CT", "M", "N", "O", "SRC", "FCLR", "SS", "TUD", "SSN", NULL }; /* commands that don't return an answer */
	char **lp;
	size_t cmdlen = strlen(cmd);

	if (cmdlen > INT_MAX) {
		upsdebugx(3, "%s: requested command is too long (%" PRIuSIZE ")",
			__func__, cmdlen);
		return 0;
	}

	if (buflen > INT_MAX) {
		upsdebugx(3, "%s: requested to read too much (%" PRIuSIZE "), "
			"reducing buflen to (INT_MAX-1)",
			__func__, buflen);
		buflen = (INT_MAX - 1);
	}

	if ((ret = usb_control_msg(udev,
			USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_ENDPOINT,
			0x0d, 0, 0, (usb_ctrl_charbuf)cmd, (int)cmdlen, 1000)) <= 0
	) {
		upsdebugx(3, "send: %s (%d)",
			ret ? nut_usb_strerror(ret) : "timeout",
			ret);
		*buf = '\0';
		return ret;
	}

	for (lp = l; *lp != NULL; lp++) {
		const char *q;
		int b;

		p = *lp; q = cmd; b = 1;
		while (*p != '\0') {
			if (*p++ != *q++) {
				b = 0;
				break;
			}
		}
		if (b && *q >= 'A' && *q <= 'Z') b = 0; /* "M" not to match "MSO" */
		if (b) {
			upsdebugx(4, "command %s returns no answer", *lp);
			*buf = '\0';
			return 0;
		}
	}

	for (p = buf; p < buf + buflen; p += ret) {
		/* buflen constrained to INT_MAX above, so we can cast: */
		if ((ret = usb_interrupt_read(udev,
				USB_ENDPOINT_IN | 1,
				(usb_ctrl_charbuf)p, (int)(buf + buflen - p), 1000)) <= 0
		) {
			upsdebugx(3, "read: %s (%d)",
				ret ? nut_usb_strerror(ret) : "timeout",
				ret);
			*buf = '\0';
			return ret;
		}
		if ((e = memchr(p, '\r', (size_t)ret)) != NULL) break;
	}
	if (e != NULL && ++e < buf + buflen) {
		*e = '\0';
		/* buflen constrained to INT_MAX above, so we can cast: */
		return (int)(e - buf);
	} else {
		upsdebugx(3, "read: buflen %" PRIuSIZE " too small", buflen);
		*buf = '\0';
		return 0;
	}
}

/* SNR communication subdriver */
static int	snr_command(const char *cmd, char *buf, size_t buflen)
{
	/*ATTENTION: This subdriver uses short buffer with length 102 byte*/
	const struct {
		const char	*str;	/* Megatec command */
		const int	index;	/* String index for this command */
		const char	prefix;	/* Character to replace the first byte in reply */
	} command[] = {
		{ "Q1\r", 0x03, '(' },
		{ "F\r", 0x0d, '#' },
		{ "I\r", 0x0c, '#' },
		{ NULL, 0, '\0' }
	};

	int	i;

	upsdebugx(3, "send: %.*s", (int)strcspn(cmd, "\r"), cmd);

	if (buflen > INT_MAX) {
		upsdebugx(3, "%s: requested to read too much (%" PRIuSIZE "), "
			"reducing buflen to (INT_MAX-1)",
			__func__, buflen);
		buflen = (INT_MAX - 1);
	}

	if (buflen < 102) {
		upsdebugx(4, "size of buf less than 102 byte!");
		return 0;
	}

	/* Prepare SNR-UPS for communication.
	 * Without the interrupt UPS returns zeros for some time,
	 * and afterwards NUT returns a communications error.
	 */
	usb_interrupt_read(udev,
		0x81,
		(usb_ctrl_charbuf)buf, 102, 1000);

	for (i = 0; command[i].str; i++) {

		int	retry;

		if (strcmp(cmd, command[i].str)) {
			continue;
		}

		for (retry = 0; retry < 10; retry++) {
			unsigned int	di, si, size;
			int	ret;

			ret = usb_get_string(udev,
				command[i].index, langid_fix,
				(usb_ctrl_charbuf)buf, 102);

			if (ret <= 0) {
				upsdebugx(3, "read: %s (%d)",
					ret ? nut_usb_strerror(ret) : "timeout",
					ret);
				return ret;
			}

			/* This may serve in the future */
			upsdebugx(1, "received %d (%d)", ret, buf[0]);

			if (ret != buf[0]) {
				upsdebugx(1, "size mismatch: %d / %d", ret, buf[0]);
				continue;
			}

			/* Simple unicode -> ASCII inplace conversion
			 * FIXME: this code is at least shared with mge-shut/libshut
			 * Create a common function? */
			size = (unsigned int)buf[0];
			for (di = 0, si = 2; si < size; si += 2) {

				if (di >= (buflen - 1))
					break;

				if (buf[si + 1])	/* high byte */
					buf[di++] = '?';
				else
					buf[di++] = buf[si];

			}

			/* Note: effective range of di should be unsigned char */
			buf[di] = 0;
			ret = (int)di;

			/* "UPS No Ack" has a special meaning */
			if (
				strcspn(buf, "\r") == 10 &&
				!strncasecmp(buf, "UPS No Ack", 10)
			) {
				upsdebugx(3, "read: %.*s", (int)strcspn(buf, "\r"), buf);
				continue;
			}

			/* Replace the first byte of what we received with the correct one */
			buf[0] = command[i].prefix;

			upsdebug_hex(5, "read", buf, (size_t)ret);
			upsdebugx(3, "read: %.*s", (int)strcspn(buf, "\r"), buf);

			return ret;

		}

		return 0;

	}

	/* Echo the unknown command back */
	upsdebugx(3, "read: %.*s", (int)strcspn(cmd, "\r"), cmd);
	return snprintf(buf, buflen, "%s", cmd);
}

static int ablerex_command(const char *cmd, char *buf, size_t buflen)
{
	int	iii;
	int	len;
	int	idx;
	int	retry;
	char	tmp[64];
	char	tmpryy[64];

	upsdebugx(3, "send: %.*s", (int)strcspn(cmd, "\r"), cmd);

	if (buflen > INT_MAX) {
		upsdebugx(3, "%s: requested to read too much (%" PRIuSIZE "), reducing buflen to (INT_MAX-1)",
			__func__, buflen);
		buflen = (INT_MAX - 1);
	}

	for (retry = 0; retry < 3; retry++) {
		int	ret;

		memset(buf, 0, buflen);
		tmp[0] = 0x05;
		tmp[1] = 0;
		tmp[2] = 1 + (char)strcspn(cmd, "\r");

		for (iii = 0 ; iii < tmp[2] ; iii++)
		{
			tmp[3+iii] = cmd[iii];
		}

		ret = usb_control_msg(udev,
			0x21,
			0x09, 0x305, 0,
			(usb_ctrl_charbuf)tmp, 47, 1000);

		upsdebugx(3, "R11 read: %s", ret ? nut_usb_strerror(ret) : "timeout");

		usleep(500000);
		tmpryy[0] = 0x05;
		ret = usb_control_msg(udev,
			0xA1,
			0x01, 0x305, 0,
			(usb_ctrl_charbuf)tmpryy, 47, 1000);
		upsdebugx(3, "R2 read%d: %.*s", ret, ret, tmpryy);

		len = 0;
		for (idx = 0 ; idx < 47 ; idx++)
		{
			buf[idx] = tmpryy[idx];
			if (tmpryy[idx] == '\r')
			{
				len = idx;
				break;
			}
		}
		upsdebugx(3, "R3 read%d: %.*s", len, len, tmpryy);

		if (len > 0) {
			len ++;
		}
		if (ret <= 0) {
			upsdebugx(3, "read: %s", ret ? nut_usb_strerror(ret) : "timeout");
			return ret;
		}

		upsdebugx(1, "received %d (%d)", ret, buf[0]);

		if ((!strcasecmp(cmd, "Q1\r")) && len != 47) continue;
		if ((!strcasecmp(cmd, "I\r")) && len != 39) continue;
		if ((!strcasecmp(cmd, "F\r")) && len != 22) continue;
		if ((!strcasecmp(cmd, "Q5\r")) && len != 22)
		{
			buf[0] = '(';
			for (idx = 1 ; idx < 47 ; idx++)
			{
				buf[idx] = 0;
			}
			upsdebugx(3, "read Q5 Fail...");
			return 22;
		}

		upsdebugx(3, "read: %.*s", (int)strcspn(buf, "\r"), buf);
		return len;
	}

	return 0;
}

static void	*ablerex_subdriver_fun(USBDevice_t *device)
{
	NUT_UNUSED_VARIABLE(device);

	subdriver_command = &ablerex_command;
	return NULL;
}

/* Gtec communication subdriver (based on Cypress) */
static int	gtec_command(const char *cmd, char *buf, size_t buflen)
{
	char	tmp[SMALLBUF];
	int	ret = 0;
	size_t	i;

	if (buflen > INT_MAX) {
		upsdebugx(3, "%s: requested to read too much (%" PRIuSIZE "), "
			"reducing buflen to (INT_MAX-1)",
			__func__, buflen);
		buflen = (INT_MAX - 1);
	}

	/* Send command */
	memset(tmp, 0, sizeof(tmp));
	snprintf(tmp, sizeof(tmp), "%s", cmd);

	for (i = 0; i < strlen(tmp); i += (size_t)ret) {

		/* Write data in 8-byte chunks */
		/* ret = usb->set_report(udev, 0, (unsigned char *)&tmp[i], 8); */
		ret = usb_control_msg(udev,
			USB_ENDPOINT_OUT + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
			0x09, 0x02, 0,
			(usb_ctrl_charbuf)&tmp[i], 8, 5000);

		if (ret <= 0) {
			upsdebugx(3, "send: %s (%d)",
				ret ? nut_usb_strerror(ret) : "timeout",
				ret);
			return ret;
		}

	}

	upsdebugx(3, "send: %.*s", (int)strcspn(tmp, "\r"), tmp);

	/* Read reply */
	memset(buf, 0, buflen);

	for (i = 0; (i <= buflen-128) && (memchr(buf, '\r', buflen) == NULL); i += (size_t)ret) {

		/* Read data in 8-byte chunks */
		/* ret = usb->get_interrupt(udev, (unsigned char *)&buf[i], 8, 1000); */
		ret = usb_interrupt_read(udev,
			0x81,
			(usb_ctrl_charbuf)&buf[i], 128, 1000);

		/* Any errors here mean that we are unable to read a reply
		 * (which will happen after successfully writing a command
		 * to the UPS) */
		if (ret <= 0) {
			upsdebugx(3, "read: %s (%d)",
				ret ? nut_usb_strerror(ret) : "timeout",
				ret);
			return ret;
		}

		snprintf(tmp, sizeof(tmp), "read [% 3d]", (int)i);
		upsdebug_hex(5, tmp, &buf[i], (size_t)ret);

	}

	upsdebugx(3, "read: %.*s", (int)strcspn(buf, "\r"), buf);

	if (i > INT_MAX) {
		upsdebugx(3, "%s: read too much (%" PRIuSIZE ")", __func__, i);
		return -1;
	}
	return (int)i;
}

static struct {
	bool_t	initialized;
	bool_t	ok;
	uint8_t	in_endpoint_address;
	uint8_t in_bmAttributes;
	uint16_t in_wMaxPacketSize;
	uint8_t	out_endpoint_address;
	uint8_t out_bmAttributes;
	uint16_t out_wMaxPacketSize;
} armac_endpoint_cache = { .initialized = FALSE, .ok = FALSE };

static void load_armac_endpoint_cache(void)
{
#if WITH_LIBUSB_1_0
	int ret;
	struct libusb_device *dev;
	struct libusb_config_descriptor *config_descriptor;
	bool_t found_in = FALSE;
	bool_t found_out = FALSE;
#endif /* WITH_LIBUSB_1_0 */

	if (armac_endpoint_cache.initialized) {
		return;
	}

	armac_endpoint_cache.initialized = TRUE;
	armac_endpoint_cache.ok = FALSE;

#if WITH_LIBUSB_1_0
	dev = libusb_get_device(udev);
	if (!dev) {
		upsdebugx(4, "load_armac_endpoint_cache: unable to libusb_get_device");
		return;
	}

	ret = libusb_get_active_config_descriptor(dev, &config_descriptor);
	if (ret) {
		upsdebugx(4, "load_armac_endpoint_cache: libusb_get_active_config_descriptor error=%d", ret);
		libusb_free_config_descriptor(config_descriptor);
		return;
	}

	if (config_descriptor->bNumInterfaces != 1) {
		upsdebugx(4, "load_armac_endpoint_cache: unexpected config_descriptor->bNumInterfaces=%d", config_descriptor->bNumInterfaces);
		libusb_free_config_descriptor(config_descriptor);
		return;
	} else {
		/* Here and below, the "else" is for C99-satisfying new variable scoping */
		const struct libusb_interface *interface = &config_descriptor->interface[0];

		if (interface->num_altsetting != 1) {
			upsdebugx(4, "load_armac_endpoint_cache: unexpected interface->num_altsetting=%d", interface->num_altsetting);
			libusb_free_config_descriptor(config_descriptor);
			return;
		} else {
			uint8_t	i;
			const struct libusb_interface_descriptor *interface_descriptor = &interface->altsetting[0];

			if (interface_descriptor->bNumEndpoints != 2) {
				upsdebugx(4, "load_armac_endpoint_cache: unexpected interface_descriptor->bNumEndpoints=%d", interface_descriptor->bNumEndpoints);
				libusb_free_config_descriptor(config_descriptor);
				return;
			}
		
			for (i = 0; i < interface_descriptor->bNumEndpoints; i++) {
				const struct libusb_endpoint_descriptor *endpoint = &interface_descriptor->endpoint[i];
		
				if (endpoint->bEndpointAddress & LIBUSB_ENDPOINT_IN) {
					found_in = TRUE;
					armac_endpoint_cache.in_endpoint_address = endpoint->bEndpointAddress;
					armac_endpoint_cache.in_bmAttributes = endpoint->bmAttributes;
					armac_endpoint_cache.in_wMaxPacketSize = endpoint->wMaxPacketSize;
				} else {
					found_out = TRUE;
					armac_endpoint_cache.out_endpoint_address = endpoint->bEndpointAddress;
					armac_endpoint_cache.out_bmAttributes = endpoint->bmAttributes;
					armac_endpoint_cache.out_wMaxPacketSize = endpoint->wMaxPacketSize;
				}
			}
		}
	}

	if (found_in || found_out) {
		armac_endpoint_cache.ok = TRUE;

		upsdebugx(4, "%s: in_endpoint_address=%02x, in_bmAttributes=%02d, out_endpoint_address=%02d, out_bmAttributes=%02d",
			__func__, armac_endpoint_cache.in_endpoint_address, armac_endpoint_cache.in_bmAttributes,
			armac_endpoint_cache.out_endpoint_address, armac_endpoint_cache.out_bmAttributes);
	}

	libusb_free_config_descriptor(config_descriptor);
#else	/* WITH_LIBUSB_1_0 */
	upsdebugx(4, "%s: SKIP: not implemented for libusb-0.1 or serial connections", __func__);
#endif	/* !WITH_LIBUSB_1_0 */
}

/* Armac communication subdriver
 *
 * This reproduces a communication protocol used by an old PowerManagerII
 * software, which doesn't seem to be Armac specific. The banner is: "2004
 * Richcomm Technologies, Inc. Dec 27 2005 ver 1.1." Maybe other Richcomm UPSes
 * would work with this - better than with the richcomm_usb driver.
 */
#define ARMAC_READ_SIZE_FOR_CONTROL 6
#define ARMAC_READ_SIZE_FOR_INTERRUPT 64
static int	armac_command(const char *cmd, char *buf, size_t buflen)
{
	char tmpbuf[ARMAC_READ_SIZE_FOR_INTERRUPT];
	int ret = 0;
	size_t i, bufpos;
	const size_t cmdlen = strlen(cmd);
	bool_t use_interrupt = FALSE;
	int read_size = ARMAC_READ_SIZE_FOR_CONTROL;

	/* UPS ignores (doesn't echo back) unsupported commands which makes
	 * the initialization long. List commands tested to be unsupported:
	 */
	const char *unsupported[] = {
		"QGS\r",
		"QS\r",
		"QPI\r",
		"M\r",
		"D\r",
		NULL
	};

	if (!armac_endpoint_cache.initialized) {
		load_armac_endpoint_cache();
	}

	for (i = 0; unsupported[i] != NULL; i++) {
		if (strcmp(cmd, unsupported[i]) == 0) {
			upsdebugx(2,
				"armac: unsupported cmd: %.*s",
				(int)strcspn(cmd, "\r"), cmd);
			return snprintf(buf, buflen, "%s", cmd);
		}
	}
	upsdebugx(4, "armac command %.*s", (int)strcspn(cmd, "\r"), cmd);

#if WITH_LIBUSB_1_0
	/* Be conservative and do not break old Armac UPSes */
	use_interrupt = armac_endpoint_cache.ok
		&& armac_endpoint_cache.in_endpoint_address == 0x82
		&& armac_endpoint_cache.in_bmAttributes & LIBUSB_TRANSFER_TYPE_INTERRUPT
		&& armac_endpoint_cache.out_endpoint_address == 0x02
		&& armac_endpoint_cache.out_bmAttributes & LIBUSB_TRANSFER_TYPE_INTERRUPT
		&& armac_endpoint_cache.in_wMaxPacketSize == 64;
#endif /* WITH_LIBUSB_1_0 */

	if (use_interrupt && cmdlen + 1 < armac_endpoint_cache.in_wMaxPacketSize) {
		memset(tmpbuf, 0, sizeof(tmpbuf));
		tmpbuf[0] = 0xa0 + cmdlen;
		memcpy(tmpbuf + 1, cmd, cmdlen);

		ret = usb_interrupt_write(udev,
			armac_endpoint_cache.out_endpoint_address,
			(usb_ctrl_charbuf)tmpbuf, cmdlen + 1, 5000);

		read_size = ARMAC_READ_SIZE_FOR_INTERRUPT;
	} else {
		/* Cleanup buffer before sending a new command */
		for (i = 0; i < 10; i++) {
			ret = usb_interrupt_read(udev, 0x81,
				(usb_ctrl_charbuf)tmpbuf, ARMAC_READ_SIZE_FOR_CONTROL, 100);
			if (ret != ARMAC_READ_SIZE_FOR_CONTROL) {
				/* Timeout - buffer is clean. */
				break;
			}
			upsdebugx(4, "armac cleanup ret i=%" PRIuSIZE " ret=%d ctrl=%02hhx", i, ret, tmpbuf[0]);
		}

		/* Send command to the UPS in 3-byte chunks. Most fit 1 chunk, except for eg.
		 * parameterized tests. */
		for (i = 0; i < cmdlen;) {
			const size_t bytes_to_send = (cmdlen <= (i + 3)) ? (cmdlen - i) : 3;
			memset(tmpbuf, 0, sizeof(tmpbuf));
			tmpbuf[0] = 0xa0 + bytes_to_send;
			memcpy(tmpbuf + 1, cmd + i, bytes_to_send);
			ret = usb_control_msg(udev,
				USB_ENDPOINT_OUT + USB_TYPE_CLASS + USB_RECIP_INTERFACE,
				0x09, 0x200, 0,
				(usb_ctrl_charbuf)tmpbuf, 4, 5000);
			i += bytes_to_send;
		}
	}

	if (ret <= 0) {
		upsdebugx(1,
			"send control: %s (%d)",
			ret ? nut_usb_strerror(ret) : "timeout",
			ret);
		return ret;
	}

	/* Wait for response to buffer */
	usleep(2000);
	memset(buf, 0, buflen);

	bufpos = 0;
	while (bufpos + read_size + 1 < buflen) {
		size_t bytes_available;

		/* Read data in 6-byte chunks */
		ret = usb_interrupt_read(udev, use_interrupt ? armac_endpoint_cache.in_endpoint_address : 0x81,
			(usb_ctrl_charbuf)tmpbuf, read_size, 1000);

		/* Any errors here mean that we are unable to read a reply
		 * (which will happen after successfully writing a command
		 * to the UPS) */
		if (ret != read_size) {
			/* NOTE: If end condition is invalid for particular UPS we might make one
			 * request more and get this error. If bufpos > (say) 10 this could be ignored
			 * and the reply correctly read. */
			upsdebugx(1,
				"interrupt read error: %s (%d)",
				ret ? nut_usb_strerror(ret) : "timeout",
				ret);
			return ret < 0 ? ret : (int)bufpos;
		}

		upsdebugx(4,
			"read: ret %d buf %02hhx: %02hhx %02hhx %02hhx %02hhx %02hhx  >%c%c%c%c%c<",
			ret,
			tmpbuf[0], tmpbuf[1], tmpbuf[2], tmpbuf[3], tmpbuf[4], tmpbuf[5],
			tmpbuf[1], tmpbuf[2], tmpbuf[3], tmpbuf[4], tmpbuf[5]);

		/*
		 * On most tested devices (including R/2000I/PSW) this was equal to the number of
		 * bytes returned in the buffer, but on some newer UPS (R/3000I/PF1) it was 1 more
		 * (1 control + 5 bytes transferred and bytes_available equal to 6 instead of 5).
		 *
		 * Current assumption is that this is number of bytes available on the UPS side
		 * with up to 5 (ret - 1) transferred.
		 */
		bytes_available = (unsigned char)tmpbuf[0] & 0x3f;
		if (bytes_available == 0) {
			/* End of transfer */
			break;
		}

		if (bytes_available > (unsigned)read_size - 1) {
			/* Single interrupt transfer has 1 control + 5 data bytes */
			bytes_available = read_size - 1;
		}

		/* Copy bytes into the final buffer while detecting end of line - \r */
		for (i = 0; i < bytes_available; i++) {
			if (tmpbuf[i + 1] == 0x00 && bufpos == 0) {
				/* Happens when a manually turned off UPS is connected to the USB */
				upsdebugx(3, "null byte read - is UPS off?");
				return 0;
			}

			/* Vultech V2000 seems to use 0x00 within status bits. This might mean "unsupported".
			 * or something else completely. */
			if (tmpbuf[i + 1] == 0x00) {
				if (bufpos >= 38) {
					upsdebugx(3, "found null byte in status bits at %" PRIuSIZE " byte, assuming 0.", bufpos);
					buf[bufpos++] = '0';
					continue;
				} else {
					upsdebugx(3, "found null byte in data stream - interrupting read.");
					/* Break through two loops */
					goto end_of_message;
				}
			}

			buf[bufpos++] = tmpbuf[i + 1];

			if (tmpbuf[i + 1] == 0x0d) {
				if (i + 1 != bytes_available) {
					upsdebugx(3, "trailing bytes in serial transmission found: %" PRIuSIZE "  copied out of %" PRIuSIZE,
						i + 1, bytes_available
					);
				}
				/* Break through two loops */
				goto end_of_message;
			}
		}

		if (bytes_available <= 2) {
			/* Slow down, let the UPS buffer more bytes */
			usleep(10000);
		}
	}
end_of_message:

	if (bufpos + read_size >= buflen) {
		upsdebugx(2, "Protocol error, too much data read.");
		return -1;
	}

	upsdebugx(3, "armac command %.*s response read: '%.*s'",
		(int)strcspn(cmd, "\r"), cmd,
		(int)strcspn(buf, "\r"), buf
		);

	return (int)bufpos;
}


static void	*cypress_subdriver(USBDevice_t *device)
{
	NUT_UNUSED_VARIABLE(device);

	subdriver_command = &cypress_command;
	return NULL;
}

static void	*sgs_subdriver(USBDevice_t *device)
{
	NUT_UNUSED_VARIABLE(device);

	subdriver_command = &sgs_command;
	return NULL;
}

static void	*ippon_subdriver(USBDevice_t *device)
{
	NUT_UNUSED_VARIABLE(device);

	subdriver_command = &ippon_command;
	return NULL;
}

static void	*krauler_subdriver(USBDevice_t *device)
{
	NUT_UNUSED_VARIABLE(device);

	subdriver_command = &krauler_command;
	return NULL;
}

static void	*phoenix_subdriver(USBDevice_t *device)
{
	NUT_UNUSED_VARIABLE(device);

	subdriver_command = &phoenix_command;
	return NULL;
}

static void	*fabula_subdriver(USBDevice_t *device)
{
	NUT_UNUSED_VARIABLE(device);

	subdriver_command = &fabula_command;
	return NULL;
}

static void	*phoenixtec_subdriver(USBDevice_t *device)
{
	NUT_UNUSED_VARIABLE(device);

	subdriver_command = &phoenixtec_command;
	return NULL;
}

/* Note: the "hunnox_subdriver" name is taken by the subdriver_t structure */
static void *fabula_hunnox_subdriver(USBDevice_t *device)
{
	NUT_UNUSED_VARIABLE(device);

	subdriver_command = &hunnox_command;
	return NULL;
}

static void	*fuji_subdriver(USBDevice_t *device)
{
	NUT_UNUSED_VARIABLE(device);

	subdriver_command = &fuji_command;
	return NULL;
}

static void	*snr_subdriver(USBDevice_t *device)
{
	NUT_UNUSED_VARIABLE(device);

	subdriver_command = &snr_command;
	return NULL;
}

static void	*armac_subdriver(USBDevice_t *device)
{
	NUT_UNUSED_VARIABLE(device);

	subdriver_command = &armac_command;
	return NULL;
}

/* USB device match structure */
typedef struct {
	const int	vendorID;		/* USB device's VendorID */
	const int	productID;		/* USB device's ProductID */
	const char	*vendor;		/* USB device's iManufacturer string */
	const char	*product;		/* USB device's iProduct string */
	void		*(*fun)(USBDevice_t *);	/* Handler for specific processing */
} qx_usb_device_id_t;

/* USB VendorID/ProductID/iManufacturer/iProduct match - note: rightmost comment is used for naming rules by tools/nut-usbinfo.pl */
static qx_usb_device_id_t	qx_usb_id[] = {
	{ USB_DEVICE(0x05b8, 0x0000),	NULL,		NULL,			&cypress_subdriver },	/* Agiler UPS */
	{ USB_DEVICE(0xffff, 0x0000),	NULL,		NULL,			&ablerex_subdriver_fun },	/* Ablerex 625L USB (Note: earlier best-fit was "krauler_subdriver" before PR #1135) */
	{ USB_DEVICE(0x1cb0, 0x0035),	NULL,		NULL,			&krauler_subdriver },	/* Legrand Daker DK / DK Plus */
	{ USB_DEVICE(0x0665, 0x5161),	NULL,		NULL,			&cypress_subdriver },	/* Belkin F6C1200-UNV/Voltronic Power UPSes */
	{ USB_DEVICE(0x06da, 0x0002),	"Phoenixtec Power","USB Cable (V2.00)",	&phoenixtec_subdriver },/* Masterguard A Series */
	{ USB_DEVICE(0x06da, 0x0002),	NULL,		NULL,			&cypress_subdriver },	/* Online Yunto YQ450 */
	{ USB_DEVICE(0x06da, 0x0003),	NULL,		NULL,			&ippon_subdriver },	/* Mustek Powermust */
	{ USB_DEVICE(0x06da, 0x0004),	NULL,		NULL,			&cypress_subdriver },	/* Phoenixtec Innova 3/1 T */
	{ USB_DEVICE(0x06da, 0x0005),	NULL,		NULL,			&cypress_subdriver },	/* Phoenixtec Innova RT */
	{ USB_DEVICE(0x06da, 0x0201),	NULL,		NULL,			&cypress_subdriver },	/* Phoenixtec Innova T */
	{ USB_DEVICE(0x06da, 0x0601),	NULL,		NULL,			&phoenix_subdriver },	/* Online Zinto A */
	{ USB_DEVICE(0x0f03, 0x0001),	NULL,		NULL,			&cypress_subdriver },	/* Unitek Alpha 1200Sx */
	{ USB_DEVICE(0x14f0, 0x00c9),	NULL,		NULL,			&phoenix_subdriver },	/* GE EP series */
	{ USB_DEVICE(0x0483, 0x0035),	NULL,		NULL,			&sgs_subdriver },	/* TS Shara UPSes; vendor ID 0x0483 is from ST Microelectronics - with product IDs delegated to different OEMs */
	{ USB_DEVICE(0x0001, 0x0000),	"MEC",		"MEC0003",		&fabula_subdriver },	/* Fideltronik/MEC LUPUS 500 USB */
	{ USB_DEVICE(0x0001, 0x0000),	NULL,		"MEC0003",		&fabula_hunnox_subdriver },	/* Hunnox HNX 850, reported to also help support Powercool and some other devices; closely related to fabula with tweaks */
	{ USB_DEVICE(0x0001, 0x0000),	"ATCL FOR UPS",	"ATCL FOR UPS",		&fuji_subdriver },	/* Fuji UPSes */
	{ USB_DEVICE(0x0001, 0x0000),	NULL,		NULL,			&krauler_subdriver },	/* Krauler UP-M500VA */
	{ USB_DEVICE(0x0001, 0x0000),	NULL,		"MEC0003",		&snr_subdriver },	/* SNR-UPS-LID-XXXX UPSes */
	{ USB_DEVICE(0x0925, 0x1234),	NULL,		NULL,			&armac_subdriver },	/* Armac UPS and maybe other richcomm-like or using old PowerManagerII software */
	/* End of list */
	{ -1,	-1,	NULL,	NULL,	NULL }
};

static int qx_is_usb_device_supported(qx_usb_device_id_t *usb_device_id_list, USBDevice_t *device)
{
	int			retval = NOT_SUPPORTED;
	qx_usb_device_id_t	*usbdev;

	for (usbdev = usb_device_id_list; usbdev->vendorID != -1; usbdev++) {

		if (usbdev->vendorID != device->VendorID)
			continue;

		/* Flag as possibly supported if we see a known vendor */
		retval = POSSIBLY_SUPPORTED;

		if (usbdev->productID != device->ProductID)
			continue;

		if (usbdev->vendor
		&& (!device->Vendor || strcasecmp(usbdev->vendor, device->Vendor))
		) {
			continue;
		}

		if (usbdev->product
		&& (!device->Product || strcasecmp(usbdev->product, device->Product))
		) {
			continue;
		}

		/* Call the specific handler, if it exists */
		if (usbdev->fun != NULL)
			(*usbdev->fun)(device);

		return SUPPORTED;

	}

	return retval;
}

static int	device_match_func(USBDevice_t *hd, void *privdata)
{
	NUT_UNUSED_VARIABLE(privdata);

	if (subdriver_command) {
		return 1;
	}

	switch (qx_is_usb_device_supported(qx_usb_id, hd))
	{
	case SUPPORTED:
		return 1;

	case POSSIBLY_SUPPORTED:
	case NOT_SUPPORTED:
	default:
		return 0;
	}
}

static USBDeviceMatcher_t	device_matcher = {
	&device_match_func,
	NULL,
	NULL
};
#endif	/* QX_USB && !TESTING */


/* == Driver functions implementations == */

/* See header file for details. */
int	instcmd(const char *cmdname, const char *extradata)
{
	item_t	*item;
	char	value[SMALLBUF];

	if (!strcasecmp(cmdname, "beeper.off")) {
		/* Compatibility mode for old command */
		upslogx(LOG_WARNING,
			"The 'beeper.off' command has been renamed to 'beeper.disable'");
		return instcmd("beeper.disable", NULL);
	}

	if (!strcasecmp(cmdname, "beeper.on")) {
		/* Compatibility mode for old command */
		upslogx(LOG_WARNING,
			"The 'beeper.on' command has been renamed to 'beeper.enable'");
		return instcmd("beeper.enable", NULL);
	}

	upslogx(LOG_INFO, "%s(%s, %s)",
		__func__, cmdname,
		extradata ? extradata : "[NULL]");

	/* Retrieve item by command name */
	item = find_nut_info(cmdname, QX_FLAG_CMD, QX_FLAG_SKIP);

	/* Check for fallback if not found */
	if (item == NULL) {

		if (!strcasecmp(cmdname, "load.on")) {
			return instcmd("load.on.delay", "0");
		}

		if (!strcasecmp(cmdname, "load.off")) {
			return instcmd("load.off.delay", "0");
		}

		if (!strcasecmp(cmdname, "shutdown.return")) {

			int	ret;

			/* Ensure "ups.start.auto" is set to "yes", if supported */
			if (dstate_getinfo("ups.start.auto")) {
				if (setvar("ups.start.auto", "yes") != STAT_SET_HANDLED) {
					upslogx(LOG_ERR, "%s: FAILED", __func__);
					return STAT_INSTCMD_FAILED;
				}
			}

			ret = instcmd("load.on.delay", dstate_getinfo("ups.delay.start"));
			if (ret != STAT_INSTCMD_HANDLED) {
				return ret;
			}

			return instcmd("load.off.delay",
				dstate_getinfo("ups.delay.shutdown"));

		}

		if (!strcasecmp(cmdname, "shutdown.stayoff")) {

			int	ret;

			/* Ensure "ups.start.auto" is set to "no", if supported */
			if (dstate_getinfo("ups.start.auto")) {
				if (setvar("ups.start.auto", "no") != STAT_SET_HANDLED) {
					upslogx(LOG_ERR, "%s: FAILED", __func__);
					return STAT_INSTCMD_FAILED;
				}
			}

			ret = instcmd("load.on.delay", "-1");
			if (ret != STAT_INSTCMD_HANDLED) {
				return ret;
			}

			return instcmd("load.off.delay",
				dstate_getinfo("ups.delay.shutdown"));

		}

		upsdebugx(2, "%s: command %s unavailable", __func__, cmdname);
		return STAT_INSTCMD_INVALID;
	}

	/* If extradata is empty, use the default value
	 * from the QX to NUT table, if any */
	extradata = extradata ? extradata : item->dfl;
	snprintf(value, sizeof(value), "%s", extradata ? extradata : "");

	/* Preprocess command */
	if (item->preprocess != NULL
	&&  item->preprocess(item, value, sizeof(value))
	) {
		/* Something went wrong */
		upslogx(LOG_ERR, "%s: FAILED", __func__);
		return STAT_INSTCMD_FAILED;
	}

	/* No preprocess function -> nothing to do with extradata */
	if (item->preprocess == NULL)
		snprintf(value, sizeof(value), "%s", "");

	/* Send the command, get the reply */
	if (qx_process(item, strlen(value) > 0 ? value : NULL)) {
		/* Something went wrong */
		upslogx(LOG_ERR, "%s: FAILED", __func__);
		return STAT_INSTCMD_FAILED;
	}

	/* We got a reply from the UPS:
	 * either subdriver->accepted (-> command handled)
	 * or the command itself echoed back (-> command failed)
	 */
	if (strlen(item->value) > 0) {

		if (subdriver->accepted != NULL
		&& !strcasecmp(item->value, subdriver->accepted)
		) {
			upslogx(LOG_INFO, "%s: SUCCEED", __func__);
			/* Set the status so that SEMI_STATIC vars are polled */
			data_has_changed = TRUE;
			return STAT_INSTCMD_HANDLED;
		}

		upslogx(LOG_ERR, "%s: FAILED", __func__);
		return STAT_INSTCMD_FAILED;

	}

	/* No reply from the UPS -> command handled */
	upslogx(LOG_INFO, "%s: SUCCEED", __func__);
	/* Set the status so that SEMI_STATIC vars are polled */
	data_has_changed = TRUE;
	return STAT_INSTCMD_HANDLED;
}

/* See header file for details. */
int	setvar(const char *varname, const char *val)
{
	item_t		*item;
	char		value[SMALLBUF];
	st_tree_t	*root = (st_tree_t *)dstate_getroot();
	int		ok = 0;

	/* Retrieve variable */
	item = find_nut_info(varname, QX_FLAG_SETVAR, QX_FLAG_SKIP);

	if (item == NULL) {
		upsdebugx(2, "%s: element %s unavailable", __func__, varname);
		return STAT_SET_UNKNOWN;
	}

	/* No NUT variable is available for this item, so we're handling
	 * a one-time setvar from ups.conf */
	if (item->qxflags & QX_FLAG_NONUT) {

		const char	*userval;

		/* Nothing to do */
		if (!testvar(item->info_type)) {
			upsdebugx(2, "%s: nothing to do... [%s]",
				__func__, item->info_type);
			return STAT_SET_HANDLED;
		}

		userval = getval(item->info_type);

		upslogx(LOG_INFO, "%s(%s, %s)",
			__func__, varname,
			userval ? userval : "[NULL]");

		snprintf(value, sizeof(value), "%s", userval ? userval : "");

	/* This item is available in NUT */
	} else {

		upslogx(LOG_INFO, "%s(%s, %s)",
			__func__, varname,
			strlen(val) ? val : "[NULL]");

		if (!strlen(val)) {
			upslogx(LOG_ERR, "%s: value not given for %s",
				__func__, item->info_type);
			return STAT_SET_UNKNOWN;	/* TODO: HANDLED but FAILED, not UNKNOWN! */
		}

		snprintf(value, sizeof(value), "%s", val);

		/* Nothing to do */
		if (!strcasecmp(dstate_getinfo(item->info_type), value)) {
			upslogx(LOG_INFO, "%s: nothing to do... [%s]",
				__func__, item->info_type);
			return STAT_SET_HANDLED;
		}

	}

	/* Check if given value is in the range of accepted values (range) */
	if (item->qxflags & QX_FLAG_RANGE) {

		long	valuetoset, min, max;

		if (strspn(value, "0123456789 .") != strlen(value)) {
			upslogx(LOG_ERR, "%s: non numerical value [%s: %s]",
				__func__, item->info_type, value);
			return STAT_SET_UNKNOWN;	/* TODO: HANDLED but FAILED, not UNKNOWN! */
		}

		valuetoset = strtol(value, NULL, 10);

		/* No NUT var is available for this item, so
		 * take its range from qx2nut table */
		if (item->qxflags & QX_FLAG_NONUT) {

			info_rw_t	*rvalue;

			if (!strlen(value)) {
				upslogx(LOG_ERR, "%s: value not given for %s",
					__func__, item->info_type);
				return STAT_SET_UNKNOWN;	/* TODO: HANDLED but FAILED, not UNKNOWN! */
			}

			min = max = -1;

			/* Loop on all existing values */
			for (rvalue = item->info_rw; rvalue != NULL && strlen(rvalue->value) > 0; rvalue++) {

				if (rvalue->preprocess
				&&  rvalue->preprocess(rvalue->value, sizeof(rvalue->value))
				) {
					continue;
				}

				if (min < 0) {
					min = strtol(rvalue->value, NULL, 10);
					continue;
				}

				max = strtol(rvalue->value, NULL, 10);

				/* valuetoset is in the range */
				if (min <= valuetoset && valuetoset <= max) {
					ok = 1;
					break;
				}

				min = -1;
				max = -1;

			}

		/* We have a NUT var for this item, so check given value
		 * against the already set range */
		} else {

			const range_t	*range = state_getrangelist(root, item->info_type);

			/* Unable to find tree node for var */
			if (!range) {
				upsdebugx(2, "%s: unable to find tree node for %s",
					__func__, item->info_type);
				return STAT_SET_UNKNOWN;
			}

			while (range) {

				min = range->min;
				max = range->max;

				/* valuetoset is in the range */
				if (min <= valuetoset && valuetoset <= max) {
					ok = 1;
					break;
				}
				range = range->next;
			}

		}

		if (!ok) {
			upslogx(LOG_ERR, "%s: value out of range [%s: %s]",
				__func__, item->info_type, value);
			return STAT_SET_UNKNOWN;	/* TODO: HANDLED but FAILED, not UNKNOWN! */
		}

	/* Check if given value is in the range of accepted values (enum) */
	} else if (item->qxflags & QX_FLAG_ENUM) {

		/* No NUT var is available for this item, so
		 * take its range from qx2nut table */
		if (item->qxflags & QX_FLAG_NONUT) {

			info_rw_t	*envalue;

			if (!strlen(value)) {
				upslogx(LOG_ERR, "%s: value not given for %s",
					__func__, item->info_type);
				return STAT_SET_UNKNOWN;	/* TODO: HANDLED but FAILED, not UNKNOWN! */
			}

			/* Loop on all existing values */
			for (envalue = item->info_rw; envalue != NULL && strlen(envalue->value) > 0; envalue++) {

				if (envalue->preprocess
				&&  envalue->preprocess(envalue->value, sizeof(envalue->value))
				) {
					continue;
				}

				if (strcasecmp(envalue->value, value))
					continue;

				/* value found */
				ok = 1;
				break;

			}

		/* We have a NUT var for this item, so check given value
		 * against the already set range */
		} else {

			const enum_t	*enumlist = state_getenumlist(root, item->info_type);

			/* Unable to find tree node for var */
			if (!enumlist) {
				upsdebugx(2, "%s: unable to find tree node for %s",
					__func__, item->info_type);
				return STAT_SET_UNKNOWN;
			}

			while (enumlist) {

				/* If this is not the right value, go on to the next */
				if (strcasecmp(enumlist->val, value)) {
					enumlist = enumlist->next;
					continue;
				}

				/* value found in enumlist */
				ok = 1;
				break;
			}

		}

		if (!ok) {
			upslogx(LOG_ERR, "%s: value out of range [%s: %s]",
				__func__, item->info_type, value);
			return STAT_SET_UNKNOWN;	/* TODO: HANDLED but FAILED, not UNKNOWN! */
		}

	/* Check if given value is not too long (string) */
	} else if (item->info_flags & ST_FLAG_STRING) {

		const long	aux = state_getaux(root, item->info_type);

		/* Unable to find tree node for var */
		if (aux < 0) {
			upsdebugx(2, "%s: unable to find tree node for %s",
				__func__, item->info_type);
			return STAT_SET_UNKNOWN;
		}

		/* FIXME? Should this cast to "long"?
		 * An int-size string is quite a lot already,
		 * even on architectures with a moderate INTMAX
		 */
		if (aux < (int)strlen(value)) {
			upslogx(LOG_ERR, "%s: value is too long [%s: %s]",
				__func__, item->info_type, value);
			return STAT_SET_UNKNOWN;	/* TODO: HANDLED but FAILED, not UNKNOWN! */
		}

	}

	/* Preprocess value: from NUT-compliant to UPS-compliant */
	if (item->preprocess != NULL
	&&  item->preprocess(item, value, sizeof(value))
	) {
		/* Something went wrong */
		upslogx(LOG_ERR, "%s: FAILED", __func__);
		return STAT_SET_UNKNOWN;	/* TODO: HANDLED but FAILED, not UNKNOWN! */
	}

	/* Handle server side variable */
	if (item->qxflags & QX_FLAG_ABSENT) {
		upsdebugx(2, "%s: setting server side variable %s",
			__func__, item->info_type);
		dstate_setinfo(item->info_type, "%s", value);
		upslogx(LOG_INFO, "%s: SUCCEED", __func__);
		return STAT_SET_HANDLED;
	}

	/* No preprocess function -> nothing to do with val */
	if (item->preprocess == NULL)
		snprintf(value, sizeof(value), "%s", "");

	/* Actual variable setting */
	if (qx_process(item, strlen(value) > 0 ? value : NULL)) {
		/* Something went wrong */
		upslogx(LOG_ERR, "%s: FAILED", __func__);
		return STAT_SET_UNKNOWN;	/* TODO: HANDLED but FAILED, not UNKNOWN! */
	}

	/* We got a reply from the UPS:
	 * either subdriver->accepted (-> command handled)
	 * or the command itself echoed back (-> command failed) */
	if (strlen(item->value) > 0) {

		if (subdriver->accepted != NULL
		&&  !strcasecmp(item->value, subdriver->accepted)
		) {
			upslogx(LOG_INFO, "%s: SUCCEED", __func__);
			/* Set the status so that SEMI_STATIC vars are polled */
			data_has_changed = TRUE;
			return STAT_SET_HANDLED;
		}

		upslogx(LOG_ERR, "%s: FAILED", __func__);
		return STAT_SET_UNKNOWN;	/* TODO: HANDLED but FAILED, not UNKNOWN! */

	}

	/* No reply from the UPS -> command handled */
	upslogx(LOG_INFO, "%s: SUCCEED", __func__);
	/* Set the status so that SEMI_STATIC vars are polled */
	data_has_changed = TRUE;
	return STAT_SET_HANDLED;
}

/* Try to shutdown the UPS */
void	upsdrv_shutdown(void)
{
	/* Only implement "shutdown.default"; do not invoke
	 * general handling of other `sdcommands` here */

	int		retry;
	item_t		*item;
	const char	*val;

	upsdebugx(1, "%s...", __func__);

	/* FIXME: Use common "sdcommands" feature to
	 * replace tunables used below ("stayoff" etc).
	 */

	/* Get user-defined delays */

	/* Start delay */
	item = find_nut_info("ups.delay.start", 0, QX_FLAG_SKIP);

	/* Don't know what happened */
	if (!item) {
		upslogx(LOG_ERR, "Unable to set start delay");
		if (handling_upsdrv_shutdown > 0)
			set_exit_flag(EF_EXIT_FAILURE);
		return;
	}

	/* Set the default value */
	dstate_setinfo(item->info_type, "%s", item->dfl);

	/* Set var flags/range/enum */
	qx_set_var(item);

	/* Retrieve user defined delay settings */
	val = getval(QX_VAR_ONDELAY);

	if (val && setvar(item->info_type, val) != STAT_SET_HANDLED) {
		upslogx(LOG_ERR, "Start delay '%s' out of range", val);
		if (handling_upsdrv_shutdown > 0)
			set_exit_flag(EF_EXIT_FAILURE);
		return;
	}

	/* Shutdown delay */
	item = find_nut_info("ups.delay.shutdown", 0, QX_FLAG_SKIP);

	/* Don't know what happened */
	if (!item) {
		upslogx(LOG_ERR, "Unable to set shutdown delay");
		if (handling_upsdrv_shutdown > 0)
			set_exit_flag(EF_EXIT_FAILURE);
		return;
	}

	/* Set the default value */
	dstate_setinfo(item->info_type, "%s", item->dfl);

	/* Set var flags/range/enum */
	qx_set_var(item);

	/* Retrieve user defined delay settings */
	val = getval(QX_VAR_OFFDELAY);

	if (val && setvar(item->info_type, val) != STAT_SET_HANDLED) {
		upslogx(LOG_ERR, "Shutdown delay '%s' out of range", val);
		if (handling_upsdrv_shutdown > 0)
			set_exit_flag(EF_EXIT_FAILURE);
		return;
	}

	/* Stop pending shutdowns */
	if (find_nut_info("shutdown.stop", QX_FLAG_CMD, QX_FLAG_SKIP)) {
		for (retry = 1; retry <= MAXTRIES; retry++) {

			if (instcmd("shutdown.stop", NULL) != STAT_INSTCMD_HANDLED) {
				continue;
			}

			break;

		}

		if (retry > MAXTRIES) {
			upslogx(LOG_NOTICE, "No shutdown pending");
		}
	}

	/* Shutdown */
	for (retry = 1; retry <= MAXTRIES; retry++) {
		if (testvar("stayoff")) {
			if (instcmd("shutdown.stayoff", NULL) != STAT_INSTCMD_HANDLED) {
				continue;
			}
		} else {
			if (instcmd("shutdown.return", NULL) != STAT_INSTCMD_HANDLED) {
				continue;
			}
		}

		upslogx(LOG_ERR, "Shutting down in %s seconds",
			dstate_getinfo("ups.delay.shutdown"));
		if (handling_upsdrv_shutdown > 0)
			set_exit_flag(EF_EXIT_SUCCESS);
		return;
	}

	upslogx(LOG_ERR, "Shutdown failed!");
	if (handling_upsdrv_shutdown > 0)
		set_exit_flag(EF_EXIT_FAILURE);
}

#ifdef QX_USB
#	ifndef TESTING
		static const struct {
			const char	*name;
			int		(*command)(const char *cmd, char *buf, size_t buflen);
		} usbsubdriver[] = {
			{ "cypress", &cypress_command },
			{ "phoenixtec", &phoenixtec_command },
			{ "phoenix", &phoenix_command },
			{ "ippon", &ippon_command },
			{ "krauler", &krauler_command },
			{ "fabula", &fabula_command },
			{ "hunnox", &hunnox_command },
			{ "fuji", &fuji_command },
			{ "sgs", &sgs_command },
			{ "snr", &snr_command },
			{ "ablerex", &ablerex_command },
			{ "armac", &armac_command },
			{ "gtec", &gtec_command },
			{ NULL, NULL }
		};
#	endif
#endif


void	upsdrv_help(void)
{
#ifndef TESTING
	size_t i;

# ifdef QX_USB
	/* Subdrivers have special SOMETHING_command() handling and
	 * are listed in usbsubdriver[] array (just above in this
	 * source file).
	 */
	printf("\nAcceptable values for USB 'subdriver' via -x or ups.conf in this driver: ");
	for (i = 0; usbsubdriver[i].name != NULL; i++) {
		if (i>0)
			printf(", ");
		printf("%s", usbsubdriver[i].name);
	}
	printf("\n");
# endif	/* QX_USB*/

	/* Protocols are the first token from "name" field in
	 * subdriver_t instances in files like nutdrv_qx_mecer.c
	 */
	printf("\nAcceptable values for 'protocol' via -x or ups.conf in this driver: ");
	for (i = 0; subdriver_list[i] != NULL; i++) {
		char	subdrv_name[SMALLBUF], *p;

		/* Get rid of subdriver version */
		snprintf(subdrv_name, sizeof(subdrv_name), "%.*s",
			(int)strcspn(subdriver_list[i]->name, " "),
			subdriver_list[i]->name);

		/* lowercase the (ASCII) string */
		for (p = subdrv_name; *p; ++p)
			*p = tolower(*p);

		if (i>0)
			printf(", ");
		printf("%s", subdrv_name);
	}
	printf("\n");
#endif	/* TESTING */
}

/* Adding flags/vars */
void	upsdrv_makevartable(void)
{
	char	temp[SMALLBUF];
	int	i;

	upsdebugx(1, "%s...", __func__);

	snprintf(temp, sizeof(temp),
		"Set shutdown delay, in seconds (default=%s)", DEFAULT_OFFDELAY);
	addvar(VAR_VALUE, QX_VAR_OFFDELAY, temp);

	snprintf(temp, sizeof(temp),
		"Set startup delay, in seconds (default=%s)", DEFAULT_ONDELAY);
	addvar(VAR_VALUE, QX_VAR_ONDELAY, temp);

	addvar(VAR_FLAG, "stayoff",
		"If invoked the UPS won't return after a shutdown when FSD arises");

	snprintf(temp, sizeof(temp),
		"Set polling frequency, in seconds, to reduce data flow (default=%d)",
			 DEFAULT_POLLFREQ);
	addvar(VAR_VALUE, QX_VAR_POLLFREQ, temp);

	addvar(VAR_VALUE, "protocol",
		"Preselect communication protocol (skip autodetection)");

	/* battery.{charge,runtime} guesstimation */
	addvar(VAR_VALUE, "runtimecal",
		"Parameters used for runtime calculation");
	addvar(VAR_VALUE, "chargetime",
		"Nominal charge time for UPS battery");
	addvar(VAR_VALUE, "idleload",
		"Minimum load to be used for runtime calculation");

	addvar(VAR_FLAG, "battery_voltage_reports_one_pack",
		"If your device natively reports battery.voltage of a single cell/pack, "
		"multiply that into voltage of the whole battery assembly. "
		"You may need an override.battery.packs=N setting also.");

#ifdef QX_USB
	addvar(VAR_VALUE, "subdriver", "Serial-over-USB subdriver selection");

	/* allow -x vendor=X, vendorid=X, product=X, productid=X, serial=X */
	nut_usb_addvars();

	addvar(VAR_VALUE, "langid_fix",
		"Apply the language ID workaround to the krauler subdriver "
		"(0x409 or 0x4095)");
	addvar(VAR_FLAG, "noscanlangid", "Don't autoscan valid range for langid");
#endif	/* QX_USB */

#ifdef QX_SERIAL
	addvar(VAR_VALUE, "cablepower", "Set cable power for serial interface");
#endif	/* QX_SERIAL */

	/* Subdrivers flags/vars */
	for (i = 0; subdriver_list[i] != NULL; i++) {

		if (subdriver_list[i]->makevartable != NULL)
			subdriver_list[i]->makevartable();

	}
}

/* Update UPS status/infos */
void	upsdrv_updateinfo(void)
{
	time_t		now;
	static int	retry = 0;

	upsdebugx(1, "%s...", __func__);

	time(&now);

	/* Clear status buffer before beginning */
	status_init();

	/* Do a full update (polling) every pollfreq or upon data change
	 * (i.e. setvar/instcmd) */
	if ((now > (lastpoll + pollfreq)) || (data_has_changed == TRUE)) {

		upsdebugx(1, "Full update...");

		/* Clear ups_status */
		ups_status = 0;

		alarm_init();

		if (qx_ups_walk(QX_WALKMODE_FULL_UPDATE) == FALSE) {

			if (retry < MAXTRIES || retry == MAXTRIES) {
				upsdebugx(1,
					"Communications with the UPS lost: status read failed!");
				retry++;
			} else {
				dstate_datastale();
			}

			return;
		}

		lastpoll = now;
		data_has_changed = FALSE;

		ups_alarm_set();
		alarm_commit();

	} else {

		upsdebugx(1, "Quick update...");

		/* Quick poll data only to see if the UPS is still connected */
		if (qx_ups_walk(QX_WALKMODE_QUICK_UPDATE) == FALSE) {

			if (retry < MAXTRIES || retry == MAXTRIES) {
				upsdebugx(1,
					"Communications with the UPS lost: status read failed!");
				retry++;
			} else {
				dstate_datastale();
			}

			return;
		}

	}

	ups_status_set();
	status_commit();

	if (retry > MAXTRIES) {
		upslogx(LOG_NOTICE, "Communications with the UPS re-established");
	}

	retry = 0;

	dstate_dataok();
}

/* Initialise data from UPS */
void	upsdrv_initinfo(void)
{
	char	*val;

	upsdebugx(1, "%s...", __func__);

	dstate_setinfo("driver.version.data", "%s", subdriver->name);

	/* Initialise data */
	if (qx_ups_walk(QX_WALKMODE_INIT) == FALSE) {
		fatalx(EXIT_FAILURE, "Can't initialise data from the UPS");
	}

	/* Init battery guesstimation */
	qx_initbattery();

	if (dstate_getinfo("ups.delay.start")) {

		/* Retrieve user defined delay settings */
		val = getval(QX_VAR_ONDELAY);

		if (val && setvar("ups.delay.start", val) != STAT_SET_HANDLED) {
			fatalx(EXIT_FAILURE, "Start delay '%s' out of range", val);
		}

	}

	if (dstate_getinfo("ups.delay.shutdown")) {

		/* Retrieve user defined delay settings */
		val = getval(QX_VAR_OFFDELAY);

		if (val && setvar("ups.delay.shutdown", val) != STAT_SET_HANDLED) {
			fatalx(EXIT_FAILURE, "Shutdown delay '%s' out of range", val);
		}

	}

	if (!find_nut_info("load.off", QX_FLAG_CMD, QX_FLAG_SKIP)
	&&  find_nut_info("load.off.delay", QX_FLAG_CMD, QX_FLAG_SKIP)
	) {
		/* Adds default with a delay value of '0' (= immediate) */
		dstate_addcmd("load.off");
	}

	if (!find_nut_info("load.on", QX_FLAG_CMD, QX_FLAG_SKIP)
	&&  find_nut_info("load.on.delay", QX_FLAG_CMD, QX_FLAG_SKIP)
	) {
		/* Adds default with a delay value of '0' (= immediate) */
		dstate_addcmd("load.on");
	}

	/* Init polling frequency */
	val = getval(QX_VAR_POLLFREQ);
	if (val)
		pollfreq = strtol(val, NULL, 10);

	dstate_setinfo("driver.parameter.pollfreq", "%ld", pollfreq);

	time(&lastpoll);

	/* Install handlers */
	upsh.setvar = setvar;
	upsh.instcmd = instcmd;

	/* Subdriver initinfo */
	if (subdriver->initinfo != NULL)
		subdriver->initinfo();
}

/* Open the port and the like and choose the subdriver */
void	upsdrv_initups(void)
{
#ifdef QX_USB
# ifndef TESTING
	int	ret, langid;
	char	tbuf[255];	/* Some devices choke on size > 255 */
	char	*regex_array[USBMATCHER_REGEXP_ARRAY_LIMIT];
	char	*subdrv;
# endif
#endif

	upsdebugx(1, "%s...", __func__);

#if defined(QX_SERIAL) && defined(QX_USB)

	/* Whether the device is connected through USB or serial */
	if (
		!strcasecmp(dstate_getinfo("driver.parameter.port"), "auto") ||
		getval("subdriver") ||
		getval("vendorid") ||
		getval("productid") ||
		getval("vendor") ||
		getval("product") ||
		getval("serial") ||
		getval("bus") ||
		getval("langid_fix")
#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
		|| getval("busport")
#endif
	) {
		/* USB */
		is_usb = 1;
	} else {
		/* Serial */
		is_usb = 0;
	}

#endif	/* QX_SERIAL && QX_USB */

/* Serial */
#ifdef QX_SERIAL

#	ifdef QX_USB
	if (!is_usb) {
#	else
	{ /* scoping */
#	endif	/* QX_USB */

#	ifndef TESTING

		const struct {
			const char	*val;
			const int	dtr;
			const int	rts;
		} cablepower[] = {
			{ "normal",	1, 0 },	/* Default */
			{ "reverse",	0, 1 },
			{ "both",	1, 1 },
			{ "none",	0, 0 },
			{ NULL, 0, 0 }
		};

		int		i;
		const char	*val;
		struct termios	tio;

		/* Open and lock the serial port and set the speed to 2400 baud. */
		upsfd = ser_open(device_path);
		ser_set_speed(upsfd, device_path, B2400);

		if (tcgetattr(upsfd, &tio)) {
			fatal_with_errno(EXIT_FAILURE, "tcgetattr");
		}

		/* Use canonical mode input processing (to read reply line) */
		tio.c_lflag |= ICANON;	/* Canonical input (erase and kill processing) */

		tio.c_cc[VEOF] = _POSIX_VDISABLE;
		tio.c_cc[VEOL] = '\r';
		tio.c_cc[VERASE] = _POSIX_VDISABLE;
		tio.c_cc[VINTR] = _POSIX_VDISABLE;
		tio.c_cc[VKILL] = _POSIX_VDISABLE;
		tio.c_cc[VQUIT] = _POSIX_VDISABLE;
		tio.c_cc[VSUSP] = _POSIX_VDISABLE;
		tio.c_cc[VSTART] = _POSIX_VDISABLE;
		tio.c_cc[VSTOP] = _POSIX_VDISABLE;

		if (tcsetattr(upsfd, TCSANOW, &tio)) {
			fatal_with_errno(EXIT_FAILURE, "tcsetattr");
		}

		val = getval("cablepower");
		for (i = 0; val && cablepower[i].val; i++) {

			if (!strcasecmp(val, cablepower[i].val)) {
				break;
			}
		}

		if (!cablepower[i].val) {
			fatalx(EXIT_FAILURE, "Value '%s' not valid for 'cablepower'", val);
		}

		ser_set_dtr(upsfd, cablepower[i].dtr);
		ser_set_rts(upsfd, cablepower[i].rts);

		/* Allow some time to settle for the cablepower */
		usleep(100000);

#	endif	/* TESTING */

#	ifdef QX_USB
	} else {	/* is_usb */
#	else
	} /* end of scoping */
#	endif	/* QX_USB */

#endif	/* QX_SERIAL */

/* USB */
#ifdef QX_USB

		warn_if_bad_usb_port_filename(device_path);

# ifndef TESTING
		subdrv = getval("subdriver");

		regex_array[0] = getval("vendorid");
		regex_array[1] = getval("productid");
		regex_array[2] = getval("vendor");
		regex_array[3] = getval("product");
		regex_array[4] = getval("serial");
		regex_array[5] = getval("bus");
		regex_array[6] = getval("device");
#  if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
		regex_array[7] = getval("busport");
#  else
		if (getval("busport")) {
			upslogx(LOG_WARNING, "\"busport\" is configured for the device, but is not actually handled by current build combination of NUT and libusb (ignored)");
		}
#  endif

		/* Check for language ID workaround (#1) */
		if (getval("langid_fix")) {
			/* Skip "0x" prefix and set back to hexadecimal */
			unsigned int u_langid_fix;
			if ( (sscanf(getval("langid_fix") + 2, "%x", &u_langid_fix) != 1)
			||   (u_langid_fix > INT_MAX)
			) {
				upslogx(LOG_NOTICE, "Error enabling language ID workaround");
			} else {
				langid_fix = (int)u_langid_fix;
				upsdebugx(2,
					"Language ID workaround enabled (using '0x%x')",
					(unsigned int)langid_fix);
			}
		}

		/* Pick up the subdriver name if set explicitly */
		if (subdrv) {

			int	i;

			if (!regex_array[0] || !regex_array[1]) {
				fatalx(EXIT_FAILURE,
					"When specifying a USB 'subdriver', "
					"'vendorid' and 'productid' are mandatory.");
			}

			for (i = 0; usbsubdriver[i].name; i++) {

				if (strcasecmp(subdrv, usbsubdriver[i].name)) {
					continue;
				}

				subdriver_command = usbsubdriver[i].command;
				break;
			}

			if (!subdriver_command) {
				fatalx(EXIT_FAILURE, "Subdriver '%s' not found!", subdrv);
			}

		}

		ret = USBNewRegexMatcher(&regex_matcher,
			regex_array,
			REG_ICASE | REG_EXTENDED);
		switch (ret)
		{
		case -1:
			fatal_with_errno(EXIT_FAILURE, "USBNewRegexMatcher");
		case 0:
			break;	/* All is well */
		default:
			fatalx(EXIT_FAILURE,
				"Invalid regular expression: %s",
				 regex_array[ret]);
		}

		/* Link the matchers */
		regex_matcher->next = &device_matcher;

		ret = usb->open_dev(&udev, &usbdevice, regex_matcher, NULL);
		if (ret < 0) {
			fatalx(EXIT_FAILURE,
				"No supported devices found. "
				"Please check your device availability with 'lsusb'\n"
				"and make sure you have an up-to-date version of NUT. "
				"If this does not help,\n"
				"try running the driver with at least 'subdriver', "
				"'vendorid' and 'productid'\n"
				"options specified. Please refer to the man page "
				"for details about these options\n"
				"(man 8 nutdrv_qx).\n");
		}

		if (!subdriver_command) {
			fatalx(EXIT_FAILURE, "No subdriver selected");
		}

		/* Create a new matcher for later reopening */
		ret = USBNewExactMatcher(&reopen_matcher, &usbdevice);
		if (ret) {
			fatal_with_errno(EXIT_FAILURE, "USBNewExactMatcher");
		}

		/* Link the matchers */
		reopen_matcher->next = regex_matcher;

		dstate_setinfo("ups.vendorid", "%04x", usbdevice.VendorID);
		dstate_setinfo("ups.productid", "%04x", usbdevice.ProductID);

		/* Check for language ID workaround (#2) */
		if ((langid_fix != -1) && (!getval("noscanlangid"))) {
			/* Future improvement:
			 *   Asking for the zero'th index is special - it returns
			 *       a string descriptor that contains all the language
			 *       IDs supported by the device.
			 *   Typically there aren't many - often only one.
			 *   The language IDs are 16 bit numbers, and they start at
			 *       the third byte in the descriptor.
			 *   See USB 2.0 specification, section 9.6.7, for more
			 *       information on this.
			 * This should allow automatic application of the workaround */
			ret = usb_get_string(udev, 0, 0,
				(usb_ctrl_charbuf)tbuf, sizeof(tbuf));
			if (ret >= 4) {
				langid = ((uint8_t)tbuf[2]) | (((uint8_t)tbuf[3]) << 8);
				upsdebugx(1,
					"First supported language ID: 0x%x "
					"(please report to the NUT maintainer!)",
					(unsigned int)langid);
			}
		}

# endif	/* TESTING */

# ifdef QX_SERIAL
	}	/* is_usb */
# endif	/* QX_SERIAL */

#endif	/* QX_USB */

	/* Choose subdriver */
	if (!subdriver_matcher())
		fatalx(EXIT_FAILURE, "Device not supported!");

	/* Subdriver initups */
	if (subdriver->initups != NULL)
		subdriver->initups();
}

/* Close the ports and the like */
void	upsdrv_cleanup(void)
{
	upsdebugx(1, "%s...", __func__);

#ifndef TESTING

# ifdef QX_SERIAL

#  ifdef QX_USB
	if (!is_usb) {
#  endif	/* QX_USB */

		ser_set_dtr(upsfd, 0);
		ser_close(upsfd, device_path);

#  ifdef QX_USB
	} else {	/* is_usb */
#  endif	/* QX_USB */

# endif	/* QX_SERIAL */

# ifdef QX_USB
		usb->close_dev(udev);
		USBFreeExactMatcher(reopen_matcher);
		USBFreeRegexMatcher(regex_matcher);
		free(usbdevice.Vendor);
		free(usbdevice.Product);
		free(usbdevice.Serial);
		free(usbdevice.Bus);
		free(usbdevice.Device);
#  if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
		free(usbdevice.BusPort);
#  endif

#  ifdef QX_SERIAL
	}	/* is_usb */
#  endif	/* QX_SERIAL */

# endif	/* QX_USB */

#endif	/* TESTING */

}


/* == Support functions == */

/* Generic command processing function: send a command and read a reply.
 * Returns < 0 on error, 0 on timeout and the number of bytes read on success. */
static ssize_t	qx_command(const char *cmd, char *buf, size_t buflen)
{
#ifndef TESTING
	ssize_t	ret = -1;
#endif

/* NOTE: Could not find in which ifdef-ed codepath, but clang complained
 * about unused parameters here. Reference them just in case...
 */
	NUT_UNUSED_VARIABLE(cmd);
	NUT_UNUSED_VARIABLE(buf);
	NUT_UNUSED_VARIABLE(buflen);

#ifndef TESTING

# ifdef QX_USB

#  ifdef QX_SERIAL
	/* Communication: USB */
	if (is_usb) {
#  endif	/* QX_SERIAL (&& QX_USB)*/

		if (udev == NULL) {
			dstate_setinfo("driver.state", "reconnect.trying");

			ret = usb->open_dev(&udev, &usbdevice, reopen_matcher, NULL);

			if (ret < 1) {
				return ret;
			}

			dstate_setinfo("driver.state", "reconnect.updateinfo");
		}

		ret = (*subdriver_command)(cmd, buf, buflen);

		if (ret >= 0) {
			return ret;
		}

		switch (ret)
		{
		case LIBUSB_ERROR_BUSY:	/* Device or resource busy */
			fatal_with_errno(EXIT_FAILURE, "Got disconnected by another driver");
#ifndef HAVE___ATTRIBUTE__NORETURN
# if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunreachable-code"
# endif
			exit(EXIT_FAILURE);	/* Should not get here in practice, but compiler is afraid we can fall through */
# if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE)
#  pragma GCC diagnostic pop
# endif
#endif

#	if WITH_LIBUSB_0_1			/* limit to libusb 0.1 implementation */
		case -EPERM:		/* Operation not permitted */
			fatal_with_errno(EXIT_FAILURE, "Permissions problem");
#ifndef HAVE___ATTRIBUTE__NORETURN
# if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunreachable-code"
# endif
			exit(EXIT_FAILURE);	/* Should not get here in practice, but compiler is afraid we can fall through */
# if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE)
#  pragma GCC diagnostic pop
# endif
#endif
#	endif	/* WITH_LIBUSB_0_1 */

		case LIBUSB_ERROR_PIPE:	/* Broken pipe */
			if (usb_clear_halt(udev, 0x81) == 0) {
				upsdebugx(1, "Stall condition cleared");
				break;
			}
#if (defined ETIME) && ETIME && WITH_LIBUSB_0_1		/* limit to libusb 0.1 implementation */
			goto fallthrough_case_ETIME;
		case -ETIME:		/* Timer expired */
		fallthrough_case_ETIME:
#endif	/* ETIME && WITH_LIBUSB_0_1 */
			if (usb_reset(udev) == 0) {
				upsdebugx(1, "Device reset handled");
			}
			goto fallthrough_case_reconnect;
		case LIBUSB_ERROR_NO_DEVICE:	/* No such device */
		case LIBUSB_ERROR_ACCESS:	/* Permission denied */
		case LIBUSB_ERROR_IO:		/* I/O error */
#if WITH_LIBUSB_0_1			/* limit to libusb 0.1 implementation */
		case -ENXIO:		/* No such device or address */
#endif	/* WITH_LIBUSB_0_1 */
		case LIBUSB_ERROR_NOT_FOUND:	/* No such file or directory */
		fallthrough_case_reconnect:
			/* Uh oh, got to reconnect! */
			dstate_setinfo("driver.state", "reconnect.trying");
			usb->close_dev(udev);
			udev = NULL;
			break;

		case LIBUSB_ERROR_TIMEOUT:	/* Connection timed out */
		case LIBUSB_ERROR_OVERFLOW:	/* Value too large for defined data type */
#if EPROTO && WITH_LIBUSB_0_1		/* limit to libusb 0.1 implementation */
		case -EPROTO:		/* Protocol error */
#endif
		default:
			break;
		}

#  ifdef QX_SERIAL
	/* Communication: serial */
	} else {	/* !is_usb */
#  endif	/* QX_SERIAL (&& QX_USB) */

# endif	/* QX_USB (&& TESTING) */

# ifdef QX_SERIAL

		ser_flush_io(upsfd);

		ret = ser_send(upsfd, "%s", cmd);

		if (ret <= 0) {
			upsdebugx(3, "send: %s (%" PRIiSIZE ")",
				ret ? strerror(errno) : "timeout", ret);
			return ret;
		}

		upsdebugx(3, "send: '%.*s'",
			(int)strcspn(cmd, "\r"), cmd);

		ret = ser_get_buf(upsfd, buf, buflen, SER_WAIT_SEC, 0);

		if (ret <= 0) {
			upsdebugx(3, "read: %s (%" PRIiSIZE ")",
				ret ? strerror(errno) : "timeout", ret);
			return ret;
		}

		upsdebug_hex(5, "read", buf, (size_t)ret);
		upsdebugx(3, "read: '%.*s'", (int)strcspn(buf, "\r"), buf);

#  ifdef QX_USB
	}	/* !is_usb */
#  endif	/* QX_USB (&& QX_SERIAL) */

# endif	/* QX_SERIAL (&& TESTING) */

	return ret;

#else	/* TESTING */

	testing_t	*testing = subdriver->testing;
	int		i;

	memset(buf, 0, buflen);

	upsdebugx(3, "send: '%.*s'", (int)strcspn(cmd, "\r"), cmd);

	for (i = 0; cmd && testing[i].cmd; i++) {

		if (strcasecmp(cmd, testing[i].cmd)) {
			continue;
		}

		upsdebugx(3, "read: '%.*s'",
			(int)strcspn(testing[i].answer, "\r"),
			testing[i].answer);

		/* If requested to do so and this is the case, try to preserve inner '\0's (treat answer as a sequence of bytes) */
		if (testing[i].answer_len > 0 && strlen(testing[i].answer) < (size_t)testing[i].answer_len) {

			size_t	len;

			len = buflen <= (size_t)testing[i].answer_len ? buflen - 1 : (size_t)testing[i].answer_len;
			len = len <= sizeof(testing[i].answer) ? len : sizeof(testing[i].answer);

			memcpy(buf, testing[i].answer, len);
			upsdebug_hex(4, "read", buf, (int)len);

			return len;

		}

		return snprintf(buf, buflen, "%s", testing[i].answer);

	}

	/* If the driver expects some kind of reply in case of error.. */
	if (subdriver->rejected != NULL) {

		/* ..fulfill its expectations.. */
		upsdebugx(3, "read: '%.*s'",
			(int)strcspn(subdriver->rejected, "\r"),
			subdriver->rejected);
		return snprintf(buf, buflen, "%s", subdriver->rejected);

	/* ..otherwise.. */
	} else {

		/* ..echo back the command */
		upsdebugx(3, "read: '%.*s'", (int)strcspn(cmd, "\r"), cmd);
		return snprintf(buf, buflen, "%s", cmd);

	}

#endif	/* TESTING */
}

/* See header file for details.
 * Interpretation is done in ups_status_set(). */
void	update_status(const char *value)
{
	status_lkp_t	*status_item;
	int		clear = 0;

	upsdebugx(5, "%s: %s", __func__, value);

	if (*value == '!') {
		value++;
		clear = 1;
	}

	for (status_item = status_info; status_item->status_str != NULL ; status_item++) {

		if (strcasecmp(status_item->status_str, value))
			continue;

		if (clear) {
			ups_status &= ~status_item->status_mask;
		} else {
			ups_status |= status_item->status_mask;
		}

		return;
	}

	upsdebugx(5, "%s: Warning! %s not in list of known values",
		__func__, value);
}

/* Choose subdriver */
static int	subdriver_matcher(void)
{
	const char	*protocol = getval("protocol");
	int		i;

	/* Select the subdriver for this device */
	for (i = 0; subdriver_list[i] != NULL; i++) {

		int	j;

		/* If protocol is set in ups.conf, use it */
		if (protocol) {

			char	subdrv_name[SMALLBUF];

			/* Get rid of subdriver version */
			snprintf(subdrv_name, sizeof(subdrv_name), "%.*s",
				(int)strcspn(subdriver_list[i]->name, " "),
				subdriver_list[i]->name);

			if (strcasecmp(subdrv_name, protocol)) {
				upsdebugx(2, "Skipping protocol %s",
					subdriver_list[i]->name);
				continue;
			}

		}

		/* Give every subdriver some tries */
		for (j = 0; j < MAXTRIES; j++) {

			subdriver = subdriver_list[i];

			if (subdriver->claim()) {
				break;
			}

			subdriver = NULL;

		}

		if (subdriver != NULL)
			break;

	}

	if (!subdriver) {
		upslogx(LOG_ERR, "Device not supported!");
		return 0;
	}

	upslogx(LOG_INFO, "Using protocol: %s", subdriver->name);

	return 1;
}

/* Set vars boundaries */
static void	qx_set_var(item_t *item)
{
	if (!(item->qxflags & QX_FLAG_NONUT))
		dstate_setflags(item->info_type, item->info_flags);

	/* Set max length for strings, if needed */
	if (item->info_flags & ST_FLAG_STRING && !(item->qxflags & QX_FLAG_NONUT))
		dstate_setaux(item->info_type, strtol(item->info_rw[0].value, NULL, 10));

	/* Set enum list */
	if (item->qxflags & QX_FLAG_ENUM) {

		info_rw_t	*envalue;
		char		buf[LARGEBUF] = "";

		/* Loop on all existing values */
		for (envalue = item->info_rw; envalue != NULL && strlen(envalue->value) > 0; envalue++) {

			if (envalue->preprocess
			&&  envalue->preprocess(envalue->value, sizeof(envalue->value))
			) {
				continue;
			}

			/* This item is not available yet in NUT, so publish these data in the logs */
			if (item->qxflags & QX_FLAG_NONUT) {

				snprintfcat(buf, sizeof(buf), " %s", envalue->value);

			/* This item is available in NUT, add its enum to the variable */
			} else {

				dstate_addenum(item->info_type, "%s", envalue->value);

			}

		}

		if (item->qxflags & QX_FLAG_NONUT)
			upslogx(LOG_INFO, "%s, settable values:%s",
				item->info_type,
				strlen(buf) > 0 ? buf : " none");

	}

	/* Set range */
	if (item->qxflags & QX_FLAG_RANGE) {

		info_rw_t	*rvalue, *from = NULL, *to = NULL;
		int		ok = 0;

		/* Loop on all existing values */
		for (rvalue = item->info_rw; rvalue != NULL && strlen(rvalue->value) > 0; rvalue++) {

			if (rvalue->preprocess
			&&  rvalue->preprocess(rvalue->value, sizeof(rvalue->value))
			) {
				continue;
			}

			if (!from) {
				from = rvalue;
				continue;
			}

			to = rvalue;

			/* This item is not available yet in NUT, so
			 * publish these data in the logs */
			if (item->qxflags & QX_FLAG_NONUT) {

				upslogx(LOG_INFO, "%s, settable range: %s..%s",
					item->info_type, from->value, to->value);
				ok++;

			/* This item is available in NUT, add its range to the variable */
			} else {
				long lFrom = strtol(from->value, NULL, 10),
					lTo = strtol(to->value, NULL, 10);

				if (lFrom > INT_MAX || lTo > INT_MAX) {
					upslogx(LOG_INFO,
						"%s, settable range exceeds INT_MAX: %ld..%ld",
						item->info_type, lFrom, lTo);
				} else {
					dstate_addrange(item->info_type, (int)lFrom, (int)lTo);
				}
			}

			from = NULL;
			to = NULL;

		}

		/* This item is not available yet in NUT and we weren't able to
		 * get its range; let people know it */
		if ((item->qxflags & QX_FLAG_NONUT) && !ok)
			upslogx(LOG_INFO, "%s, settable range: none", item->info_type);

	}
}

/* Walk UPS variables and set elements of the qx2nut array. */
static bool_t	qx_ups_walk(walkmode_t mode)
{
	item_t	*item;
	int	retcode;

	/* Clear batt.{chrg,runt}.act for guesstimation */
	if (mode == QX_WALKMODE_FULL_UPDATE) {
		batt.runt.act = -1;
		batt.chrg.act = -1;
		battery_voltage_reports_one_pack_considered = 0;
	}

	/* Clear data from previous_item */
	memset(previous_item.command, 0, sizeof(previous_item.command));
	memset(previous_item.answer, 0, sizeof(previous_item.answer));

	/* 3 modes: QX_WALKMODE_INIT, QX_WALKMODE_QUICK_UPDATE
	 *      and QX_WALKMODE_FULL_UPDATE */

	/* Device data walk */
	for (item = subdriver->qx2nut; item->info_type != NULL; item++) {

		/* Skip this item */
		if (item->qxflags & QX_FLAG_SKIP)
			continue;

		upsdebugx(10, "%s: processing: %s", __func__, item->info_type);

		/* Filter data according to mode */
		switch (mode)
		{
		/* Device capabilities enumeration */
		case QX_WALKMODE_INIT:

			/* Special case for handling server side variables */
			if (item->qxflags & QX_FLAG_ABSENT) {

				/* Already set */
				if (dstate_getinfo(item->info_type))
					continue;

				dstate_setinfo(item->info_type, "%s", item->dfl);

				/* Set var flags/range/enum */
				qx_set_var(item);

				continue;
			}

			/* Allow duplicates for these NUT variables */
			if (!strncmp(item->info_type, "ups.alarm", 9)
			||  !strncmp(item->info_type, "ups.status", 10)
			) {
				break;
			}

			/* This one doesn't exist yet */
			if (dstate_getinfo(item->info_type) == NULL)
				break;

			continue;

		case QX_WALKMODE_QUICK_UPDATE:

			/* Quick update only deals with status and alarms! */
			if (!(item->qxflags & QX_FLAG_QUICK_POLL))
				continue;

			break;

		case QX_WALKMODE_FULL_UPDATE:

			/* These don't need polling after initinfo() */
			if (item->qxflags & (QX_FLAG_ABSENT | QX_FLAG_CMD | QX_FLAG_SETVAR | QX_FLAG_STATIC))
				continue;

			/* These need to be polled after user changes (setvar / instcmd) */
			if ((item->qxflags & QX_FLAG_SEMI_STATIC)
			&&  (data_has_changed == FALSE)
			) {
				continue;
			}

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
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunreachable-code"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
	/* All enum cases defined as of the time of coding
	 * have been covered above. Handle later definitions,
	 * memory corruptions and buggy inputs below...
	 */
		default:
			fatalx(EXIT_FAILURE, "%s: unknown update mode!", __func__);
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif

		}

		/* Instant commands */
		if (item->qxflags & QX_FLAG_CMD) {
			dstate_addcmd(item->info_type);
			continue;
		}

		/* Setvars */
		if (item->qxflags & QX_FLAG_SETVAR) {

			if (item->qxflags & QX_FLAG_NONUT) {
				setvar(item->info_type, NULL);
				item->qxflags |= QX_FLAG_SKIP;
			}

			continue;

		}

		/* Check whether the previous item uses the same command
		 * and then use its answer, if available.. */
		if (strlen(previous_item.command) > 0
		&&  strlen(previous_item.answer) > 0
		&&  !strcasecmp(previous_item.command, item->command)
		) {

			snprintf(item->answer, sizeof(item->answer), "%s",
				previous_item.answer);

			/* Process the answer */
			retcode = qx_process_answer(item, strlen(item->answer));

		/* ..otherwise: execute command to get answer from the UPS */
		} else {

			retcode = qx_process(item, NULL);

		}

		/* Record item as previous_item */
		snprintf(previous_item.command, sizeof(previous_item.command), "%s",
			item->command);
		snprintf(previous_item.answer, sizeof(previous_item.answer), "%s",
			item->answer);

		if (retcode) {

			/* Clear data from the item */
			memset(item->answer, 0, sizeof(item->answer));
			memset(item->value, 0, sizeof(item->value));

			if (item->qxflags & QX_FLAG_QUICK_POLL)
				return FALSE;

			if (mode == QX_WALKMODE_INIT)
				/* Skip this item from now on */
				item->qxflags |= QX_FLAG_SKIP;

			/* Don't know what happened, try again later... */
			continue;

		}

		/* Process the value we got back (set status bits
		 * and set the value of other parameters) */
		retcode = ups_infoval_set(item);

		/* Clear data from the item */
		memset(item->answer, 0, sizeof(item->answer));
		memset(item->value, 0, sizeof(item->value));

		/* Uh-oh! Some error! */
		if (retcode == -1) {

			if (item->qxflags & QX_FLAG_QUICK_POLL)
				return FALSE;

			continue;

		}

		/* Set var flags/range/enum (not for ups.{alarm.status},
		 * hence the retcode check) */
		if (retcode && mode == QX_WALKMODE_INIT) {
			qx_set_var(item);
		}

	}

	/* Update battery guesstimation */
	if (mode == QX_WALKMODE_FULL_UPDATE
	&&  (d_equal(batt.runt.act, -1) || d_equal(batt.chrg.act, -1))
	) {

		if (getval("runtimecal")) {
			const char	*val;
			time_t	battery_now;

			time(&battery_now);

			/* OL */
			if (ups_status & STATUS(OL)) {

				batt.runt.est += batt.runt.nom * difftime(battery_now, battery_lastpoll) / batt.chrg.time;
				if (batt.runt.est > batt.runt.nom) {
					batt.runt.est = batt.runt.nom;
				}

			/* OB */
			} else {

				batt.runt.est -= load.eff * difftime(battery_now, battery_lastpoll);
				if (batt.runt.est < 0) {
					batt.runt.est = 0;
				}

			}

			val = dstate_getinfo("battery.voltage");

			if (!val) {
				upsdebugx(2, "%s: unable to get battery.voltage", __func__);
			} else {
				/* For age-corrected estimates below,
				 * see theory and experimental graphs at
				 * https://github.com/networkupstools/nut/pull/1027
				 */

				batt.volt.act = strtod(val, NULL);
				if (!battery_voltage_reports_one_pack_considered) {
					batt.volt.act *= batt.packs;
				}

				if (batt.volt.act > 0 && batt.volt.low > 0 && batt.volt.high > batt.volt.low) {

					double voltage_battery_charge = (batt.volt.act - batt.volt.low) / (batt.volt.high - batt.volt.low);

					if (voltage_battery_charge < 0) {
						voltage_battery_charge = 0;
					}

					if (voltage_battery_charge > 1) {
						voltage_battery_charge = 1;
					}

					/* Correct estimated runtime remaining for old batteries:
					 * this value replacement only happens if the actual
					 * voltage_battery_charge is smaller than expected by
					 * previous (load-based) estimation, thus adapting to a
					 * battery too old and otherwise behaving non-linearly
					 */
					if (voltage_battery_charge < (batt.runt.est / batt.runt.nom)) {
						double estPrev = batt.runt.est;
						batt.runt.est = voltage_battery_charge * batt.runt.nom;
						upsdebugx(3, "%s: updating batt.runt.est from '%g' to '%g'",
							__func__, estPrev, batt.runt.est);
					}

				}
			}

			if (d_equal(batt.chrg.act, -1))
				dstate_setinfo("battery.charge", "%.0f",
					100 * batt.runt.est / batt.runt.nom);

			if (d_equal(batt.runt.act, -1) && !qx_load())
				dstate_setinfo("battery.runtime", "%.0f",
					batt.runt.est / load.eff);

			battery_lastpoll = battery_now;

		} else {

			qx_battery();

		}
	}

	return TRUE;
}

/* Convert the local status information to NUT format and set NUT alarms. */
static void	ups_alarm_set(void)
{
	if (ups_status & STATUS(RB)) {
		alarm_set("Replace battery!");
	}
	if (ups_status & STATUS(FSD)) {
		alarm_set("Shutdown imminent!");
	}
}

/* Convert the local status information to NUT format and set NUT status. */
static void	ups_status_set(void)
{
	if (ups_status & STATUS(OL)) {
		status_set("OL");		/* On line */
	} else {
		status_set("OB");		/* On battery */
	}
	if (ups_status & STATUS(DISCHRG)) {
		status_set("DISCHRG");		/* Discharging */
	}
	if (ups_status & STATUS(CHRG)) {
		status_set("CHRG");		/* Charging */
	}
	if (ups_status & STATUS(LB)) {
		status_set("LB");		/* Low battery */
	}
	if (ups_status & STATUS(OVER)) {
		status_set("OVER");		/* Overload */
	}
	if (ups_status & STATUS(RB)) {
		status_set("RB");		/* Replace battery */
	}
	if (ups_status & STATUS(TRIM)) {
		status_set("TRIM");		/* SmartTrim */
	}
	if (ups_status & STATUS(BOOST)) {
		status_set("BOOST");		/* SmartBoost */
	}
	if (ups_status & STATUS(BYPASS)) {
		status_set("BYPASS");		/* On bypass */
	}
	if (ups_status & STATUS(OFF)) {
		status_set("OFF");		/* UPS is off */
	}
	if (ups_status & STATUS(CALIB)) {
		status_set("CAL");		/* Calibration */
	}
	if (ups_status & STATUS(FSD)) {
		status_set("FSD");		/* Forced shutdown */
	}
}

/* See header file for details. */
item_t	*find_nut_info(const char *varname, const unsigned long flag, const unsigned long noflag)
{
	item_t	*item;

	for (item = subdriver->qx2nut; item->info_type != NULL; item++) {

		if (strcasecmp(item->info_type, varname))
			continue;

		if (flag && ((item->qxflags & flag) != flag))
			continue;

		if (noflag && (item->qxflags & noflag))
			continue;

		return item;
	}

	upsdebugx(2, "%s: info type %s not found", __func__, varname);
	return NULL;
}

/* Process the answer we got back from the UPS
 * Return -1 on errors, 0 on success */
static int	qx_process_answer(item_t *item, const size_t len)
{
	/* Query rejected by the UPS */
	if (subdriver->rejected && !strcasecmp(item->answer, subdriver->rejected)) {
		upsdebugx(2, "%s: query rejected by the UPS (%s)",
			__func__, item->info_type);
		return -1;
	}

	/* Short reply */
	if (item->answer_len && len < item->answer_len) {
		upsdebugx(2, "%s: short reply (%s)",
			__func__, item->info_type);
		return -1;
	}

	/* Wrong leading character */
	if (item->leading && item->answer[0] != item->leading) {
		upsdebugx(2,
			"%s: %s - invalid start character [%02x], expected [%02x]",
			__func__, item->info_type, item->answer[0], item->leading);
		return -1;
	}

	/* Check boundaries */
	if (item->to && item->to < item->from) {
		upsdebugx(1,
			"%s: in %s, starting char's position (%d) "
			"follows ending char's one (%d)",
			__func__, item->info_type, item->from, item->to);
		return -1;
	}

	/* Get value */
	if (strlen(item->answer)) {
		snprintf(item->value, sizeof(item->value), "%.*s",
			item->to ? 1 + item->to - item->from : (int)strcspn(item->answer, "\r") - item->from,
			item->answer + item->from);
	} else {
		snprintf(item->value, sizeof(item->value), "%s", "");
	}

	return 0;
}

/* See header file for details. */
int	qx_process(item_t *item, const char *command)
{
	char	buf[sizeof(item->answer) - 1] = "", *cmd;
	ssize_t	len;
	size_t cmdlen = command ?
		(strlen(command) >= SMALLBUF ? strlen(command) + 1 : SMALLBUF) :
		(item->command && strlen(item->command) >= SMALLBUF ? strlen(item->command) + 1 : SMALLBUF);
	size_t cmdsz = (sizeof(char) * cmdlen); /* in bytes, to be pedantic */

	if ( !(cmd = xmalloc(cmdsz)) ) {
		upslogx(LOG_ERR, "qx_process() failed to allocate buffer");
		return -1;
	}

	/* Prepare the command to be used */
	memset(cmd, 0, cmdsz);
	snprintf(cmd, cmdsz, "%s", command ? command : item->command);

	/* Preprocess the command */
	if (
		item->preprocess_command != NULL &&
		item->preprocess_command(item, cmd, cmdsz) == -1
	) {
		upsdebugx(4, "%s: failed to preprocess command [%s]",
			__func__, item->info_type);
		free (cmd);
		return -1;
	}

	/* Send the command */
	len = qx_command(cmd, buf, sizeof(buf));

	memset(item->answer, 0, sizeof(item->answer));

	if (len < 0 || len > INT_MAX) {
		upsdebugx(4, "%s: failed to preprocess answer [%s]",
			__func__, item->info_type);
		free (cmd);
		return -1;
	}

	memcpy(item->answer, buf, sizeof(buf));

	/* Preprocess the answer */
	if (item->preprocess_answer != NULL) {
		len = item->preprocess_answer(item, (int)len);
		if (len < 0 || len > INT_MAX) {
			upsdebugx(4, "%s: failed to preprocess answer [%s]",
				__func__, item->info_type);
			/* Clear the failed answer, preventing it from
			 * being reused by next items with same command */
			memset(item->answer, 0, sizeof(item->answer));
			free (cmd);
			return -1;
		}
	}

	free (cmd);

	/* Process the answer to get the value */
	return qx_process_answer(item, (size_t)len);
}

/* See header file for details. */
int	ups_infoval_set(item_t *item)
{
	char	value[SMALLBUF] = "";

	/* Item need to be preprocessed? */
	if (item->preprocess != NULL){

		/* Process the value returned by the UPS to NUT standards */
		if (item->preprocess(item, value, sizeof(value))) {
			upsdebugx(4, "%s: failed to preprocess value [%s: %s]",
				__func__, item->info_type, item->value);
			return -1;
		}

		/* Deal with status items */
		if (!strncmp(item->info_type, "ups.status", 10)) {
			if (strlen(value) > 0)
				update_status(value);
			return 0;
		}

		/* Deal with alarm items */
		if (!strncmp(item->info_type, "ups.alarm", 9)) {
			if (strlen(value) > 0)
				alarm_set(value);
			return 0;
		}

	} else {

		snprintf(value, sizeof(value), "%s", item->value);

		/* Cover most of the cases: either left/right filled with hashes,
		 * spaces or a mix of both */
		if (item->qxflags & QX_FLAG_TRIM)
			str_trim_m(value, "# ");

		if (strcasecmp(item->dfl, "%s")) {

			if (strspn(value, "0123456789 .") != strlen(value)) {
				upsdebugx(2, "%s: non numerical value [%s: %s]",
					__func__, item->info_type, value);
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
			snprintf(value, sizeof(value), item->dfl, strtod(value, NULL));
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
		}

	}

	if (item->qxflags & QX_FLAG_NONUT) {
		upslogx(LOG_INFO, "%s: %s", item->info_type, value);
		return 1;
	}

	if (!strlen(value)) {
		upsdebugx(1, "%s: non significant value [%s]",
			__func__, item->info_type);
		return -1;
	}

	dstate_setinfo(item->info_type, "%s", value);

	/* Fill batt.{chrg,runt}.act for guesstimation */
	if (!strcasecmp(item->info_type, "battery.charge"))
		batt.chrg.act = strtol(value, NULL, 10);
	else if (!strcasecmp(item->info_type, "battery.runtime"))
		batt.runt.act = strtol(value, NULL, 10);

	return 1;
}

/* See header file for details. */
unsigned int	qx_status(void)
{
	return ups_status;
}
