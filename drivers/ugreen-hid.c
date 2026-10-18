/* ugreen-hid.c - subdriver to monitor UGREEN USB/HID UPS devices with NUT
 *
 * Supports the UGREEN US2000 (72W DC UPS, USB VID:PID 2b89:ffff).
 *
 * Device behavior (reverse engineered from US2000_TB_V1.2 firmware,
 * confirmed by telemetry captures across mains/battery/charge states):
 *
 * - Standard PDC feature reports answer GET_FEATURE as usual, but several
 *   are hardcoded dummies in firmware: ups.load is always 1, input.voltage
 *   is always 1, output.voltage is a coarse integer that reads ~0.8 V high,
 *   and the LB status bits are never set.
 * - Live telemetry arrives unprompted as 64-byte HID input reports with
 *   report ID 0x71 on the interrupt IN endpoint (about 2 Hz).  Subtype 0x01
 *   carries ADC channels; subtypes 0x01 and 0x02 share the pack millivolt /
 *   charge / gauge tail.  All volt-class words are millivolts (no offsets);
 *   current words are milliamps.  Only the first 33 bytes are visible
 *   through the core report buffer (see below); everything this driver
 *   publishes lies within them.
 * - Telemetry needs an exact-size interrupt read (interruptsize 64,
 *   defaulted in ugreen_claim(); the core 512-byte default never
 *   completes on this firmware).
 * - Pack current arrives as two unsigned magnitudes that the firmware
 *   holds mutually exclusive: a measured charge current and a pack-side
 *   discharge magnitude.  battery.current carries whichever one is
 *   nonzero, and battery.charger.status names the direction.
 * - Low-battery signaling is intentionally NOT synthesized here: use the
 *   core "ignorelb" driver option together with "battery.charge.low"
 *   instead (the charge reading itself is live).
 * - Sibling products are known to share this VID:PID but not the
 *   telemetry framing.  Every device on these IDs is therefore claimed
 *   and gets the standard PDC mappings, while the pushed telemetry is
 *   decoded only for the product strings listed in
 *   ugreen_known_products[].  See that array.
 *
 *  Copyright (C) 2026 reklrekl
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

#include "config.h" /* must be first */

#include <string.h>

#include "usbhid-ups.h"
#include "ugreen-hid.h"
#include "dstate.h"
#include "main.h"	/* for getval() */
#include "usb-common.h"

#define UGREEN_HID_VERSION	"UGREEN HID 0.5"

/* UGREEN */
#define UGREEN_VENDORID	0x2b89

/* Product strings whose telemetry framing this subdriver has decoded.
 * Sibling products are known to report on this same VID:PID while
 * pushing a different 0x71 frame layout.  Decoding one model's field
 * layout at another model's offsets would publish plausible-looking
 * nonsense, so the telemetry decoder is restricted to the product
 * strings listed here.  Such a device still claims the subdriver and
 * keeps every standard PDC mapping below.  Matched on the product prefix
 * so that a firmware revision suffix does not disable the decoder. */
static const char *ugreen_known_products[] = {
	"US2000",
	NULL
};

/* Set from ugreen_claim() and consumed by ugreen_updateinfo(), which
 * receives no HIDDevice_t.  Holds for the lifetime of the driver process,
 * which monitors a single device. */
static int ugreen_telemetry_known = 0;

/* USB IDs device table */
static usb_device_id_t ugreen_usb_device_table[] = {
	/* US2000 (72W DC UPS) */
	{ USB_DEVICE(UGREEN_VENDORID, 0xffff), NULL },

	/* Terminating entry */
	{ 0, 0, NULL }
};

static usb_communication_subdriver_t *usb = &usb_subdriver;


/* --------------------------------------------------------------- */
/*      Vendor-specific usage table                                */
/* --------------------------------------------------------------- */

/* NOTE: the live vendor telemetry (report 0x71) looks like a single
 * opaque blob to the HID parser, so it is deliberately NOT mapped
 * through the table below.  ugreen_updateinfo() cracks the raw report
 * bytes out of the core report buffer instead.  Only the standard PDC
 * feature reports are mapped here. */
static usage_lkp_t ugreen_usage_lkp[] = {
	{  NULL, 0 }
};

static usage_tables_t ugreen_utab[] = {
	ugreen_usage_lkp,
	hid_usage_lkp,
	NULL,
};

/* --------------------------------------------------------------- */
/* HID2NUT lookup table                                            */
/* --------------------------------------------------------------- */

/* PDC feature-report mappings.  Deliberately identical to the Arduino
 * PDC mappings, with three exceptions: input.voltage, ups.load and
 * output.voltage are NOT mapped here.  Reports 0x11 (always 1), 0x07
 * (always 1) and 0x0E (coarse integer, reads ~0.8 V high) are firmware
 * dummies; the live values arrive via interrupt report 0x71 and are
 * published by ugreen_updateinfo() below. */
static hid_info_t ugreen_hid2nut[] = {

	/* USB HID PDC defaults */
	{ "ups.delay.start", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.PowerSummary.DelayBeforeStartup", NULL, DEFAULT_ONDELAY, HU_FLAG_ABSENT, NULL},
	{ "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.PowerSummary.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_FLAG_ABSENT, NULL},
	{ "ups.timer.start", 0, 0, "UPS.PowerSummary.DelayBeforeStartup", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},
	{ "ups.timer.shutdown", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},
	{ "ups.timer.reboot", 0, 0, "UPS.PowerSummary.DelayBeforeReboot", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},

	/* USB HID PDC defaults */
	{ "load.off.delay", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_TYPE_CMD, NULL },
	{ "load.on.delay", 0, 0, "UPS.PowerSummary.DelayBeforeStartup", NULL, DEFAULT_ONDELAY, HU_TYPE_CMD, NULL },
	{ "shutdown.stop", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, "-1", HU_TYPE_CMD, NULL },
	{ "shutdown.reboot", 0, 0, "UPS.PowerSummary.DelayBeforeReboot", NULL, "10", HU_TYPE_CMD, NULL },

	/* Battery */
	{ "battery.type", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%s", HU_FLAG_STATIC, stringid_conversion},
	{ "battery.voltage.nominal", 0, 0, "UPS.PowerSummary.ConfigVoltage", NULL, "%.2f", HU_FLAG_QUICK_POLL, NULL},
	{ "battery.voltage", 0, 0, "UPS.PowerSummary.Voltage", NULL, "%.2f", HU_FLAG_QUICK_POLL, NULL},
	{ "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL},
	{ "battery.runtime.low", 0, 0, "UPS.PowerSummary.RemainingTimeLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL },
	/* Parse `*Capacity` values assuming `UPS.PowerSummary.CapacityMode` is set to 2, which means the `*Capacity`
	 * units are percent. */
	{ "battery.charge", 0, 0, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", 0, NULL },
	{ "battery.charge.low", 0, 0, "UPS.PowerSummary.RemainingCapacityLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL},
	{ "battery.charge.warning", 0, 0, "UPS.PowerSummary.WarningCapacityLimit", NULL, "%.0f", HU_FLAG_SEMI_STATIC, NULL},

	/* NOTE: no ups.load entry (report 0x07 is a hardcoded 1; the live
	 * load comes from telemetry 0x71, see ugreen_updateinfo()). */
	/* NOTE: no input.voltage entry (see comment above); the live value
	 * is published by ugreen_updateinfo(). */
	/* NOTE: no output.voltage entry (report 0x0E is a coarse integer
	 * that reads high; the live value comes from telemetry 0x71). */

	/* USB HID PresentStatus Flags */
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, NULL, HU_FLAG_QUICK_POLL, online_info},
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Discharging", NULL, NULL, HU_FLAG_QUICK_POLL, discharging_info},
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Charging", NULL, NULL, HU_FLAG_QUICK_POLL, charging_info},
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.NeedReplacement", NULL, NULL, 0, replacebatt_info},
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ShutdownImminent", NULL, NULL, 0, shutdownimm_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL, NULL, HU_FLAG_QUICK_POLL, lowbatt_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Overload", NULL, NULL, 0, overload_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.RemainingTimeLimitExpired", NULL, NULL, 0, timelimitexpired_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.BatteryPresent", NULL, NULL, 0, nobattery_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.FullyCharged", NULL, NULL, HU_FLAG_QUICK_POLL, fullycharged_info },
	{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.FullyDischarged", NULL, NULL, HU_FLAG_QUICK_POLL, depleted_info },

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

/* Vendor telemetry frame (report 0x71) field offsets (subtype 0x01).
 * All multi-byte fields are big-endian.  Volt-class words are millivolts
 * and current words milliamps.  Every offset below lies within the
 * 33-byte core report slot; fields past byte 32 (temp sensor 2, SOH,
 * fullness flag, gauge words) stay dormant until the core sizes the
 * slot larger. */
#define UGREEN_TLM71_SUBTYPE	1
#define UGREEN_TLM71_OUT_MV	9
#define UGREEN_TLM71_OUT_MA	11
#define UGREEN_TLM71_IN_MV	15
#define UGREEN_TLM71_TEMP_C	17
#define UGREEN_TLM71_CHG_MA	27
#define UGREEN_TLM71_DIS_MA	29
#define UGREEN_TLM71_PACK_MV	31

/* ups.load divisor: milliamps over the 72 W / 6 A nameplate rating. */
#define UGREEN_RATED_OUT_MA	6000

/* Last-resort sanity bounds on the decoded words, in the native units.
 * The product-string gate above is the primary discriminator; these only
 * catch a frame that still parses within the 33-byte slot but carries a
 * different field layout, so that a mis-decode is dropped instead of
 * published.  Generous enough to keep any in-range reading. */
#define UGREEN_PLAUS_OUT_MV	1000	/* 1 V */
#define UGREEN_PLAUS_OUT_MV_MAX	20000	/* 20 V */
#define UGREEN_PLAUS_OUT_MA_MAX	(UGREEN_RATED_OUT_MA * 2)
#define UGREEN_PLAUS_IN_MV_MAX	30000	/* 30 V */

/* Exact-size interrupt read the 64-byte telemetry frames need; the core
 * default (512) never completes on this firmware. */
#define UGREEN_TLM71_INTERRUPTSIZE	64

/* Publish live values cracked out of the pushed vendor telemetry frame.
 * Called by the core driver after each successful HID update; the stock
 * table above keeps serving the polled feature reports untouched. */
static void ugreen_updateinfo(void)
{
	unsigned char	*f;
	size_t		len;
	int		off;
	unsigned	sub, out_mv, out_ma, in_mv, temp_c, chg_ma, dis_ma,
			pack_mv;
	const char	*status;

	if (!reportbuf)
		return;

	/* Only decode telemetry for a product string whose framing we have
	 * mapped.  An unrecognised product on this VID:PID keeps the
	 * standard PDC mappings and simply publishes no vendor values,
	 * rather than having another model's field layout read at these
	 * offsets. */
	if (!ugreen_telemetry_known) {
		upsdebugx(4, "%s: product not in the known list, "
			"not decoding vendor telemetry", __func__);
		return;
	}

	len = reportbuf->len[0x71];
	/* The core sizes this slot from the report descriptor (33 bytes
	 * here) and truncates longer frames; all offsets used below lie
	 * within the first 33 bytes. */
	if (len < 33)
		return;

	f = reportbuf->data[0x71];
	if (!f)
		return;

	/* Accept both store layouts: with the report-ID byte (full or
	 * truncated frames) or payload only (63 bytes).  The ID byte alone
	 * disambiguates; no length check, the slot may hold a prefix. */
	if (f[0] == 0x71)
		off = 0;
	else if (f[0] == 0x01 || f[0] == 0x02)
		off = -1;
	else
		return;

	pack_mv = ((unsigned)f[off + UGREEN_TLM71_PACK_MV] << 8)
		| f[off + UGREEN_TLM71_PACK_MV + 1];
	sub = f[off + UGREEN_TLM71_SUBTYPE];

	/* Subtype is the framing discriminator within a known product:
	 * 0x01 carries the ADC block and 0x02 only the shared pack tail on
	 * the US2000.  A different subtype means a different frame layout,
	 * so nothing below applies to it. */
	if (sub != 0x01 && sub != 0x02) {
		upsdebugx(4, "%s: ignoring vendor frame subtype %u",
			__func__, sub);
		return;
	}

	if (sub == 0x01) {
		out_mv = ((unsigned)f[off + UGREEN_TLM71_OUT_MV] << 8)
			| f[off + UGREEN_TLM71_OUT_MV + 1];
		out_ma = ((unsigned)f[off + UGREEN_TLM71_OUT_MA] << 8)
			| f[off + UGREEN_TLM71_OUT_MA + 1];
		in_mv = ((unsigned)f[off + UGREEN_TLM71_IN_MV] << 8)
			| f[off + UGREEN_TLM71_IN_MV + 1];
		temp_c = ((unsigned)f[off + UGREEN_TLM71_TEMP_C] << 8)
			| f[off + UGREEN_TLM71_TEMP_C + 1];
		chg_ma = ((unsigned)f[off + UGREEN_TLM71_CHG_MA] << 8)
			| f[off + UGREEN_TLM71_CHG_MA + 1];
		dis_ma = ((unsigned)f[off + UGREEN_TLM71_DIS_MA] << 8)
			| f[off + UGREEN_TLM71_DIS_MA + 1];

		/* Output voltage is the anchor word of this block: its
		 * offset is known for the mapped framing, so an
		 * out-of-band reading means these offsets do not
		 * describe this frame.  Drop the whole block rather than
		 * publish a mis-decode. */
		if (out_mv < UGREEN_PLAUS_OUT_MV
		 || out_mv > UGREEN_PLAUS_OUT_MV_MAX
		 || out_ma > UGREEN_PLAUS_OUT_MA_MAX
		 || in_mv > UGREEN_PLAUS_IN_MV_MAX) {
			upsdebugx(3, "%s: implausible ADC block "
				"(out=%u mV, %u mA, in=%u mV), ignoring",
				__func__, out_mv, out_ma, in_mv);
			return;
		}

		/* Input rail reads a ~5 V residual with the brick gone;
		 * only publish on mains so OB never reports a fake 5 V.
		 * A sagging-but-present brick still shows (status stays OL). */
		status = dstate_getinfo("ups.status");
		if (!status || !strstr(status, "OB")) {
			if (in_mv > 0)
				dstate_setinfo("input.voltage", "%.1f",
					(double)in_mv / 1000.0);
		} else {
			dstate_delinfo("input.voltage");
		}

		if (out_mv > 0)
			dstate_setinfo("output.voltage", "%.2f",
				(double)out_mv / 1000.0);

		/* Output current, milliamps.  Linear above ~0.5 A with an
		 * exact zero; burst-unreliable below (firmware samples the
		 * switching waveform, the mean sags). */
		dstate_setinfo("output.current", "%.2f",
			(double)out_ma / 1000.0);
		dstate_setinfo("ups.load", "%.1f",
			(double)out_ma * 100.0 / (double)UGREEN_RATED_OUT_MA);

		/* Pack current.  The frame carries two separate unsigned
		 * magnitudes that firmware holds mutually exclusive:
		 * [27..28] is the measured charge current and [29..30] the
		 * pack-side discharge magnitude.  battery.current is an
		 * unsigned magnitude in NUT, so it carries whichever of
		 * the two is nonzero, and battery.charger.status names the
		 * direction - the standard NUT variable for it, which
		 * replaces the historic CHRG / DISCHRG status flags. */
		if (chg_ma > 0) {
			dstate_setinfo("battery.current", "%.2f",
				(double)chg_ma / 1000.0);
			dstate_setinfo("battery.charger.status", "%s",
				"charging");
		} else if (dis_ma > 0) {
			dstate_setinfo("battery.current", "%.2f",
				(double)dis_ma / 1000.0);
			dstate_setinfo("battery.charger.status", "%s",
				"discharging");
		} else {
			/* Neither magnitude is nonzero: the frame offers
			 * no direction, so drop the variable instead of
			 * guessing one. */
			dstate_setinfo("battery.current", "%.2f", 0.0);
			dstate_delinfo("battery.charger.status");
		}

		/* Second temp sensor; follows pack heat, exact placement
		 * unconfirmed (teardown-gated). */
		dstate_setinfo("battery.temperature", "%u", temp_c);

		if (pack_mv > 0)
			dstate_setinfo("battery.voltage", "%.2f",
				(double)pack_mv / 1000.0);

		/* Nameplate-proven 2S Li-ion (the PDC iDeviceChemistry
		 * mapping has nothing to say on this firmware). */
		dstate_setinfo("battery.type", "%s", "Li-ion");
		upsdebugx(3, "%s: sub=01 in=%.1f out=%.2fV %.2fA load=%.1f%% batt=%.2fV chg=%.2fA dis=%.2fA temp=%uC",
			__func__, (double)in_mv / 1000.0,
			(double)out_mv / 1000.0, (double)out_ma / 1000.0,
			(double)out_ma * 100.0 / (double)UGREEN_RATED_OUT_MA,
			(double)pack_mv / 1000.0,
			(double)chg_ma / 1000.0, (double)dis_ma / 1000.0,
			temp_c);
		return;
	}

	if (sub == 0x02) {
		/* No ADC block in this subtype ([15..16] read 0); the pack
		 * millivolt / charge tail overlaps subtype 0x01. */
		if (pack_mv > 0)
			dstate_setinfo("battery.voltage", "%.2f", (double)pack_mv / 1000.0);
		upsdebugx(3, "%s: sub=02 battery.voltage=%.2f",
			__func__, (double)pack_mv / 1000.0);
	}
}

static const char *ugreen_format_model(HIDDevice_t *hd) {
	return hd->Product;
}

static const char *ugreen_format_mfr(HIDDevice_t *hd) {
	return hd->Vendor ? hd->Vendor : "UGREEN";
}

static const char *ugreen_format_serial(HIDDevice_t *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int ugreen_claim(HIDDevice_t *hd)
{
	int status = is_usb_device_supported(ugreen_usb_device_table, hd);
	int i;

	if (status == SUPPORTED) {
		/* Decide whether the pushed telemetry framing of this
		 * product has been mapped.  The product strings are read
		 * from the device before any claim runs. */
		ugreen_telemetry_known = 0;

		if (hd->Product) {
			for (i = 0; ugreen_known_products[i]; i++) {
				size_t	len = strlen(ugreen_known_products[i]);

				if (!strncmp(hd->Product, ugreen_known_products[i], len)) {
					ugreen_telemetry_known = 1;
					break;
				}
			}
		}

		if (ugreen_telemetry_known) {
			upsdebugx(2, "%s: product \"%s\" has mapped telemetry framing",
				__func__, hd->Product);
		} else {
			upsdebugx(2, "%s: product \"%s\" not in the known list; "
				"standard mappings only, no vendor telemetry",
				__func__, hd->Product ? hd->Product : "(none)");
		}

		/* Telemetry and flashing both live on endpoint 1; make sure
		 * the core reads interrupts from there by default. */
		if (!getval("usb_hid_ep_in")) {
			upsdebugx(1, "%s: defaulting usb_hid_ep_in=1 for this model", __func__);
			usb->hid_ep_in = 1;
		}
		if (!getval("usb_hid_ep_out")) {
			upsdebugx(1, "%s: defaulting usb_hid_ep_out=1 for this model", __func__);
			usb->hid_ep_out = 1;
		}
		/* The 64-byte telemetry frames only complete with an
		 * exact-size interrupt read; the core 512-byte default
		 * never returns on this firmware. */
		if (!getval("interruptsize") && interrupt_size == 0) {
			upsdebugx(1, "%s: defaulting interruptsize=64 for this model", __func__);
			interrupt_size = UGREEN_TLM71_INTERRUPTSIZE;
		}
		return 1;
	}

	return 0;
}

static subdriver_aux_t ugreen_aux = {
	NULL,
	NULL,
	NULL,
	ugreen_updateinfo,
	NULL
};

subdriver_t ugreen_subdriver = {
	UGREEN_HID_VERSION,
	ugreen_claim,
	ugreen_utab,
	ugreen_hid2nut,
	ugreen_format_model,
	ugreen_format_mfr,
	ugreen_format_serial,
	fix_report_desc,
	&ugreen_aux,
};
