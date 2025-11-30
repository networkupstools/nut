/*
 * NUT driver for Mean Well NTU Series Inverter/UPS
 * Supports: NTU-1200 / NTU-1700 / NTU-2200 / NTU-3200
 * All voltage variants: -112 -124 -148 -212 -224 -248
 *
 * Protocol: 9600 baud, 8N1, CR-terminated ("\r"), RS232/USB-serial
 *
 * License: GPLv2 or later
 */

#include "main.h"
#include "serial.h"

#include <string.h>
#include <stdlib.h>

/* Driver identification */
#define DRIVER_NAME      "meanwell_ntu"
#define DRIVER_VERSION   "1.60"

/* UART Commands (sent as plain strings with CR terminator) */
#define CMD_Q            "Q\r"
#define CMD_I            "I\r"
#define CMD_SHUTDOWN     "C100000000000000\r"
#define CMD_ON           "C010000000000000\r"

/* Standard NUT driver info structure */
struct upsdrv_info_s upsdrv_info = {
    DRIVER_NAME,                        /* name */
    DRIVER_VERSION,                     /* version (driver.version.internal) */
    "Jonathan Hite",                    /* authors */
    DRV_EXPERIMENTAL,                   /* status */
    { NULL, NULL }                      /* subdrv_info */
};

/*
 * NTU Q-response fields:
 *
 * Example:
 * (120 025 28.8 100 19.7 000 59.9 000 011 1000000000000000000)
 *  0    1    2     3    4    5    6    7    8           9
 *
 * 0  output voltage raw*10     (only shows when on battery)
 * 1  digital load step range   (0–100% in coarse steps)
 * 2  battery voltage
 * 3  battery capacity %
 * 4  inverter temperature °C
 * 5  utility voltage (AC input, bypass mode only)
 * 6  output frequency (battery mode only)
 * 7  unused
 * 8  PPP bits (actual load %)
 * 9  19-bit status flags (string of '0'/'1')
 */

/* Human-readable names for the 19 status bits (info vars) */
static const char *status_labels[19] = {
    "experimental.inverter.mode",        /* bit 0 */
    "experimental.bypass.mode",          /* bit 1 */
    "experimental.utility.present",      /* bit 2 */
    "experimental.charge.utility",       /* bit 3 */
    "experimental.charge.solar",         /* bit 4 */
    "experimental.saving.mode",          /* bit 5 */
    "experimental.battery.low",          /* bit 6 */
    "experimental.shutdown.mode",        /* bit 7 */
    "experimental.battery.ovp",          /* bit 8 */
    "experimental.remote.shutdown",      /* bit 9 */
    "experimental.overload.level1",      /* bit 10 (100–115%) */
    "experimental.overload.level2",      /* bit 11 (115–150%) */
    "experimental.overload.level3",      /* bit 12 (150%+) */
    "experimental.overtemp",             /* bit 13 */
    "experimental.inverter.uvp",         /* bit 14 */
    "experimental.inverter.ovp",         /* bit 15 */
    "experimental.inverter.fault",       /* bit 16 */
    "experimental.eeprom.error",         /* bit 17 */
    "experimental.system.shutdown"       /* bit 18 */
};

/* ------------------ Parsing Functions ------------------ */

static int parse_q_response(const char *buf)
{
    int raw_vout = 0, load_step = 0, bat_pct = 0;
    int util_v = 0, unused = 0;
    float vbat = 0.0f, temp = 0.0f, freq = 0.0f;
    char ppp[8] = {0};
    char flags[32] = {0};
    int i;

    int fields = sscanf(buf,
        "( %d %d %f %d %f %d %f %d %7s %31s",
        &raw_vout,
        &load_step,
        &vbat,
        &bat_pct,
        &temp,
        &util_v,
        &freq,
        &unused,
        ppp,
        flags
    );

    if (fields < 10) {
        upslogx(LOG_ERR,
            "meanwell_ntu: short/invalid Q response (%d fields): '%s'",
            fields, buf);
        return 0;
    }

    /* Output voltage: protocol doc says “raw*10”, but samples are real volts */
    dstate_setinfo("output.voltage", "%.1f", (double)raw_vout);

    dstate_setinfo("battery.voltage", "%.2f", vbat);
    dstate_setinfo("battery.charge", "%d", bat_pct);
    dstate_setinfo("ups.temperature", "%.1f", temp);

    if (freq > 0.0f) {
        dstate_setinfo("output.frequency", "%.1f", freq);
    }

    /* ---- Load from PPP (actual load %) ----
     * PPP is sent as a decimal string (e.g. "011" means 11%).
     */
    {
        int load_pct = (int)strtol(ppp, NULL, 10);

        /* If PPP is zero/bogus but coarse step is non-zero, fall back */
        if (load_pct <= 0 && load_step > 0) {
            load_pct = load_step;
        }

        dstate_setinfo("ups.load", "%d", load_pct);
    }

    /* ---- Bit-field handling: info vars + ups.status ---- */

    /* Clear all 19 status info vars each poll so they reflect current state */
    for (i = 0; i < 19; i++) {
        dstate_setinfo(status_labels[i], "0");
    }

    /* Reset standard ups.status bits */
    status_init();

    /* Track key bits so we can derive CHRG/DISCHRG and ups.mode */
    {
        int inv_mode     = 0;
        int bypass_mode  = 0;
        int util_present = 0;
        int chg_util     = 0;
        int chg_solar    = 0;

        for (i = 0; i < 19 && flags[i] != '\0'; i++) {
            if (flags[i] == '1') {
                dstate_setinfo(status_labels[i], "1");

                switch (i) {
                case 0: /* inverter.mode */
                    inv_mode = 1;
                    status_set("OB");      /* on battery */
                    break;

                case 1: /* bypass.mode */
                    bypass_mode = 1;
                    status_set("BYPASS");
                    break;

                case 2: /* utility.present */
                    util_present = 1;
                    status_set("OL");      /* on line */
                    break;

                case 3: /* charge.utility */
                    chg_util = 1;
                    break;

                case 4: /* charge.solar */
                    chg_solar = 1;
                    break;

                case 6: /* battery.low */
                    status_set("LB");
                    break;

                case 13: /* overtemp */
                    status_set("OVER");
                    break;

                default:
                    /* Bit 16 (inverter.fault) is exposed as info only; no FSD. */
                    break;
                }
            }
        }

        /* ---- Derived status flags: CHRG / DISCHRG ---- */
        if (chg_util || chg_solar) {
            status_set("CHRG");
        }

        if (inv_mode && !util_present && !(chg_util || chg_solar)) {
            status_set("DISCHRG");
        }

        /* ---- Input voltage validity ----
         * Only trust/display input.voltage when utility.present bit is set.
         * Otherwise force it to 0 so stale or noisy values don't linger.
         */
        if (util_present && util_v > 0) {
            dstate_setinfo("input.voltage", "%d", util_v);
        } else {
            dstate_setinfo("input.voltage", "0");
        }

        /* ---- Mirror output voltage when in bypass ----
         * In bypass, raw_vout is 0, but the real output equals input.voltage.
         */
        if (bypass_mode && raw_vout == 0 && util_v > 0) {
            dstate_setinfo("output.voltage", "%d", util_v);
        }

        /* ---- Synthetic mode summary ----
         * Per nut-names.txt, ups.mode should be one of:
         *   online | line-interactive | bypass
         * The NTU is a line-interactive unit which can enter bypass.
         */
        if (bypass_mode) {
            dstate_setinfo("ups.mode", "bypass");
        } else {
            /* Inverter + transfer behavior is line-interactive regardless
             * of whether it is currently on battery or on mains.
             */
            dstate_setinfo("ups.mode", "line-interactive");
        }
    }

    status_commit();
    return 1;
}

/* Parse I-response (Identification & thresholds) */
static void parse_i_response(const char *buf)
{
    float eq = 0.0f, flt = 0.0f, alarm_v = 0.0f, shutdown_v = 0.0f, xfer_v = 0.0f;
    char mfr[32] = {0}, serial[32] = {0}, fw[32] = {0}, model[32] = {0};

    /* Sample line (may vary by firmware):
       #00.0 00.0 22.0 20.0 00.0 MEANWELL LOC-0123456789 REV:01.8 NTU-1200-124 1 00/00/0000\
    */

    {
        int fields = sscanf(buf,
            "#%f %f %f %f %f %31s %31s REV:%31s %31s %*s %*s",
            &eq, &flt, &alarm_v, &shutdown_v, &xfer_v,
            mfr, serial, fw, model
        );

        if (fields < 5) {
            /* Not enough numeric fields; leave defaults alone */
            return;
        }
    }

    if (mfr[0]) {
        dstate_setinfo("device.mfr", "%s", mfr);
        dstate_setinfo("ups.mfr", "%s", mfr);
    }

    if (serial[0]) {
        dstate_setinfo("device.serial", "%s", serial);
        dstate_setinfo("ups.serial", "%s", serial);
    }

    if (fw[0]) {
        dstate_setinfo("device.firmware", "%s", fw);
        dstate_setinfo("ups.firmware", "%s", fw);
    }

    if (model[0]) {
        dstate_setinfo("device.model", "%s", model);
        dstate_setinfo("ups.model", "%s", model);
    }

    /* Now that we trust the thresholds */
    dstate_setinfo("battery.alarm.voltage", "%.1f", alarm_v);
    dstate_setinfo("battery.shutdown.voltage", "%.1f", shutdown_v);
}

/* ------------------ Update Loop ------------------ */

static int meanwell_ntu_update(void)
{
    char buf[256];

    /* Send Q command */
    if (ser_send(upsfd, CMD_Q) < 0) {
        ser_comm_fail("meanwell_ntu: failed to send Q command");
        return 0;
    }

    /* Read line terminated by '\r', ignore nothing, timeout 2s */
    if (ser_get_line(upsfd, buf, sizeof(buf), '\r', "", 2, 0) <= 0) {
        ser_comm_fail("meanwell_ntu: timeout/no data for Q response");
        return 0;
    }

    if (!parse_q_response(buf)) {
        ser_comm_fail("meanwell_ntu: failed to parse Q response: '%s'", buf);
        return 0;
    }

    return 1;
}

/* ------------------ Instant Commands ------------------ */

static int instcmd(const char *cmd, const char *extra)
{
    (void)extra; /* unused */

    if (!strcasecmp(cmd, "load.off")) {
        if (ser_send(upsfd, CMD_SHUTDOWN) < 0)
            return STAT_INSTCMD_FAILED;
        return STAT_INSTCMD_HANDLED;
    }

    if (!strcasecmp(cmd, "load.on")) {
        if (ser_send(upsfd, CMD_ON) < 0)
            return STAT_INSTCMD_FAILED;
        return STAT_INSTCMD_HANDLED;
    }

    return STAT_INSTCMD_UNKNOWN;
}

/* ------------------ NUT Driver Hooks ------------------ */

void upsdrv_initups(void)
{
    char buf[256];

    /* Open serial port (device_path from main.h / ups.conf) */
    upsfd = ser_open(device_path);
    if (!VALID_FD(upsfd)) {
        fatalx(EXIT_FAILURE, "Cannot open %s", device_path);
    }

    /* Configure 9600 8N1 */
    ser_set_speed(upsfd, device_path, B9600);
    /* Current NUT serial layer defaults to 8N1. */

    /* Probe with I-command for model/info (if supported) */
    if (ser_send(upsfd, CMD_I) >= 0) {
        if (ser_get_line(upsfd, buf, sizeof(buf), '\r', "", 2, 0) > 0) {
            parse_i_response(buf);
        }
    }
}

void upsdrv_initinfo(void)
{
    /* Register instant commands this driver supports */
    dstate_addcmd("load.off");
    dstate_addcmd("load.on");

    /* Tell the core we have an instant command handler */
    upsh.instcmd = instcmd;
}

void upsdrv_updateinfo(void)
{
    if (meanwell_ntu_update()) {
        ser_comm_good();
        dstate_dataok();
    } else {
        dstate_datastale();
    }
}

void upsdrv_makevartable(void)
{
    /* No custom vars/commands beyond standard ones for now */
}

void upsdrv_cleanup(void)
{
    if (VALID_FD(upsfd)) {
        ser_close(upsfd, device_path);
        upsfd = ERROR_FD;
    }
}

void upsdrv_tweak_prognames(void)
{
    /* No custom process-name tweaks */
}

void upsdrv_shutdown(void)
{
    /* Best-effort shutdown; do not exit() here */
    ser_send(upsfd, CMD_SHUTDOWN);
}

void upsdrv_help(void)
{
    /* Optional help text could be added here if desired */
}

