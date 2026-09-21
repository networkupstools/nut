/*
 * sms_ser.c: code for mono protocol for SMS Brazil UPSes
 *
 * Copyright (C) 2023 - Alex W. Baule <alexwbaule@gmail.com>
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
 * Reference of the derivative work: riello driver
 */

#include "config.h" /* must be the first header */
#include <string.h>

#include "common.h" /* for upsdebugx() etc */
#include "sms_ser.h"
#include "main.h"
#include "serial.h"

#define ENDCHAR '\r'

#define DRIVER_NAME	"SMS Brazil UPS driver"
#define DRIVER_VERSION	"1.06"

#define QUERY_SIZE 7
#define BUFFER_SIZE 18
#define RESULT_SIZE 18
#define HUMAN_VALUES 7

static uint16_t bootdelay = DEFAULT_BOOTDELAY;
static uint16_t offdelay = DEFAULT_OFFDELAY;
static uint16_t ondelay = DEFAULT_ONDELAY;
static uint8_t bufOut[BUFFER_SIZE];
static uint8_t bufIn[BUFFER_SIZE];
static SmsData DeviceData;
static int comm_failures = 0;

/* driver description structure */
upsdrv_info_t upsdrv_info = {
    DRIVER_NAME,
    DRIVER_VERSION,
    "Alex W. Baulé <alexwbaule@gmail.com>",
    DRV_BETA,
    {NULL}};

void sms_parse_features(uint8_t *rawvalues, SmsData *results) {
    char tbattery[6];
    char frequency[4];
    int i;

    memset(results->voltageRange, 0, sizeof(results->voltageRange));
    memset(results->currentRange, 0, sizeof(results->currentRange));
    memset(tbattery, 0, sizeof(tbattery));
    memset(frequency, 0, sizeof(frequency));

    for (i = 1; i < BUFFER_SIZE - 2; i++) {
        if (i <= 7) {
            snprintfcat(results->voltageRange, 14, "%c", rawvalues[i]);
        } else if (i <= 10) {
            snprintfcat(results->currentRange, 6, "%c", rawvalues[i]);
        } else if (i <= 13) {
            snprintfcat(tbattery, sizeof(tbattery), "%c", rawvalues[i]);
        } else {
            snprintfcat(frequency, sizeof(frequency), "%c", rawvalues[i]);
        }
    }

    results->voltageBattery = atoi(tbattery);
    results->frequency = atoi(frequency);
}

void sms_parse_information(uint8_t *rawvalues, SmsData *results) {
    /* Count from 1 to ignore first char and remove 2 from BUFFER_SIZE
     *  to compensate the start and ignore '\r' from end. */
    int i;

    memset(results->model, 0, sizeof(results->model));
    memset(results->version, 0, sizeof(results->version));

    for (i = 1; i < BUFFER_SIZE - 2; i++) {
        if (i <= 12) {
            snprintfcat(results->model, 24, "%c", rawvalues[i]);
        } else {
            snprintfcat(results->version, 6, "%c", rawvalues[i]);
        }
    }
}

void sms_parse_results(uint8_t *rawvalues, SmsData *results) {
    char buf[BUFFER_SIZE];
    uint8_t byte, mask;
    long v;
    double h;

    results->upstype = (char)rawvalues[0];

    memset(buf, 0, BUFFER_SIZE);
    snprintf(buf, sizeof(buf), "0x%02x%02x", (unsigned char)rawvalues[1], (unsigned char)rawvalues[2]);
    v = strtol(buf, NULL, 16); /* 16 == hex */
    h = (double)v / 10;
    results->lastinputVac = h;

    memset(buf, 0, BUFFER_SIZE);
    snprintf(buf, sizeof(buf), "0x%02x%02x", (unsigned char)rawvalues[3], (unsigned char)rawvalues[4]);
    v = strtol(buf, NULL, 16); /* 16 == hex */
    h = (double)v / 10;
    results->inputVac = h;

    memset(buf, 0, BUFFER_SIZE);
    snprintf(buf, sizeof(buf), "0x%02x%02x", (unsigned char)rawvalues[5], (unsigned char)rawvalues[6]);
    v = strtol(buf, NULL, 16); /* 16 == hex */
    h = (double)v / 10;
    results->outputVac = h;

    memset(buf, 0, BUFFER_SIZE);
    snprintf(buf, sizeof(buf), "0x%02x%02x", (unsigned char)rawvalues[7], (unsigned char)rawvalues[8]);
    v = strtol(buf, NULL, 16); /* 16 == hex */
    h = (double)v / 10;
    results->outputpower = h;

    memset(buf, 0, BUFFER_SIZE);
    snprintf(buf, sizeof(buf), "0x%02x%02x", (unsigned char)rawvalues[9], (unsigned char)rawvalues[10]);
    v = strtol(buf, NULL, 16); /* 16 == hex */
    h = (double)v / 10;
    results->outputHz = h;

    memset(buf, 0, BUFFER_SIZE);
    snprintf(buf, sizeof(buf), "0x%02x%02x", (unsigned char)rawvalues[11], (unsigned char)rawvalues[12]);
    v = strtol(buf, NULL, 16); /* 16 == hex */
    h = (double)v / 10;
    results->batterylevel = h;

    memset(buf, 0, BUFFER_SIZE);
    snprintf(buf, sizeof(buf), "0x%02x%02x", (unsigned char)rawvalues[13], (unsigned char)rawvalues[14]);
    v = strtol(buf, NULL, 16); /* 16 == hex */
    h = (double)v / 10;
    results->temperatureC = h;

    byte = rawvalues[15];
    mask = 1;

    results->beepon = ((byte & (mask << 0)) != 0) ? true : false;
    results->shutdown = ((byte & (mask << 1)) != 0) ? true : false;
    results->test = ((byte & (mask << 2)) != 0) ? true : false;
    results->upsok = ((byte & (mask << 3)) != 0) ? true : false;
    results->boost = ((byte & (mask << 4)) != 0) ? true : false;
    results->bypass = ((byte & (mask << 5)) != 0) ? true : false;
    results->lowbattery = ((byte & (mask << 6)) != 0) ? true : false;
    results->onbattery = ((byte & (mask << 7)) != 0) ? true : false;
}

/* Sends the request prepared in bufOut and reads the reply into bufIn.
 * Every reply is RESULT_SIZE bytes: a header byte, 15 bytes of payload, a
 * checksum (the sum of all bytes up to and including it is 0 modulo 256)
 * and ENDCHAR. Returns the reply length, or -1 after logging the reason. */
static ssize_t sms_query(uint8_t length) {
    ssize_t ret;
    uint8_t sum = 0;
    size_t i;

    /* Drop whatever is left of an earlier, late or partial reply */
    ser_flush_in(upsfd, "", 0);

    upsdebug_hex(4, "sms_ser send", bufOut, length);
    if (ser_send_buf(upsfd, bufOut, length) != (ssize_t)length) {
        upslogx(LOG_ERR, "Communication error while writing to port");
        dstate_datastale();
        return -1;
    }
    memset(bufIn, 0, sizeof(bufIn));
    ret = ser_get_buf_len(upsfd, &bufIn[0], sizeof(bufIn), 3, 1000);
    upsdebug_hex(4, "sms_ser read", bufIn, ret > 0 ? (size_t)ret : 0);

    if (ret < RESULT_SIZE) {
        upslogx(LOG_ERR, "Short read from UPS");
        dstate_datastale();
        return -1;
    }

    for (i = 0; i < RESULT_SIZE - 1; i++) {
        sum += bufIn[i];
    }
    if (sum != 0 || bufIn[RESULT_SIZE - 1] != ENDCHAR) {
        upslogx(LOG_ERR, "Bad checksum or terminator in reply from UPS");
        dstate_datastale();
        return -1;
    }

    return ret;
}

/* Closes and reopens the serial port without giving up if that fails:
 * the next polls will try again. */
static void sms_reconnect(void) {
    upslogx(LOG_WARNING, "No valid reply from UPS, reopening %s", device_path);

    if (VALID_FD_SER(upsfd)) {
        ser_close(upsfd, device_path);
    }
    upsfd = ser_open_nf(device_path);
    if (INVALID_FD_SER(upsfd)) {
        upslogx(LOG_ERR, "Could not reopen %s, will retry", device_path);
        return;
    }
    ser_set_speed_nf(upsfd, device_path, B2400);
}

static int get_ups_nominal(void) {
    uint8_t length;
    ssize_t ret;

    upsdebugx(LOG_DEBUG, "get_ups_nominal");

    length = sms_prepare_get_status(&bufOut[0]);

    ret = sms_query(length);
    if (ret < 0) {
        return -1;
    }

    upsdebugx(3, "Get nominal Ok: received byte %" PRIiSIZE, ret);

    if (bufIn[0] == '=' || bufIn[0] == '<' || bufIn[0] == '>') {
        sms_parse_results(&bufIn[0], &DeviceData);
        return 0;
    }

    upsdebugx(3, "Invalid query response from 'Q' command");
    return -1;
}

static int get_ups_information(void) {
    uint8_t length;
    ssize_t ret;

    upsdebugx(LOG_DEBUG, "get_ups_information");

    length = sms_prepare_get_information(&bufOut[0]);

    ret = sms_query(length);
    if (ret < 0) {
        return -1;
    }

    upsdebugx(3, "Get information Ok: received byte %" PRIiSIZE, ret);

    if (bufIn[0] == ';' || bufIn[0] == ':') {
        sms_parse_information(&bufIn[0], &DeviceData);
        return 0;
    }

    upsdebugx(3, "Invalid query response from 'I' command");
    return -1;
}

static int get_ups_features(void) {
    uint8_t length;
    ssize_t ret;

    upsdebugx(LOG_DEBUG, "get_ups_features");

    length = sms_prepare_get_features(&bufOut[0]);

    ret = sms_query(length);
    if (ret < 0) {
        return -1;
    }

    upsdebugx(LOG_DEBUG, "Get features Ok: received byte %" PRIiSIZE, ret);

    if (bufIn[0] == ';' || bufIn[0] == ':') {
        sms_parse_features(&bufIn[0], &DeviceData);
        return 0;
    }

    upsdebugx(LOG_ERR, "Invalid query response from 'F' command");
    return -1;
}

/* Parses a delay given as text; returns 0 and leaves *delay alone unless it
 * is a number that fits the 16 bits the protocol has for it */
static int sms_parse_delay(const char *text, uint16_t *delay) {
    char *end = NULL;
    long value;

    if (!text || !*text) {
        return 0;
    }
    value = strtol(text, &end, 10);
    if (*end != '\0' || value < 0 || value > UINT16_MAX) {
        return 0;
    }
    *delay = (uint16_t)value;
    return 1;
}

/* Converts seconds into the time unit of the UPS. Line-interactive models
 * count in hundredths of a minute (0.6 s): an SMS Premium 1500 tested for
 * 6, 18 and 59 seconds when sent 10, 30 and 100. The vendor software scales
 * its values by 0.6 for the on-line type, which therefore counts in seconds.
 * Measured for the battery test only; assumed for the shutdown delays. */
static uint16_t sms_seconds_to_units(uint16_t seconds) {
    unsigned long units;

    if (DeviceData.upstype == SMS_TYPE_ONLINE) {
        return seconds;
    }
    units = ((unsigned long)seconds * 10 + 3) / 6;
    return (units > UINT16_MAX) ? UINT16_MAX : (uint16_t)units;
}

/* Sends the command prepared in bufOut. Commands are not acknowledged. */
static int sms_send_command(size_t length, const char *cmdname) {
    upsdebug_hex(4, "sms_ser send", bufOut, length);
    if (ser_send_buf(upsfd, bufOut, length) != (ssize_t)length) {
        upslogx(LOG_ERR, "failed to send %s", cmdname);
        return STAT_INSTCMD_FAILED;
    }
    upsdebugx(3, "command %s OK!", cmdname);
    return STAT_INSTCMD_HANDLED;
}

static int sms_instcmd(const char *cmdname, const char *extra) {
    uint16_t delay;

    upsdebug_INSTCMD_STARTING(cmdname, extra);

    if (!strcasecmp(cmdname, "test.battery.start")) {
        /* Test for the number of seconds given, or the quick test's */
        delay = DEFAULT_TESTDELAY;
        if (extra && !sms_parse_delay(extra, &delay)) {
            upslogx(LOG_ERR, "%s: invalid duration [%s]", cmdname, extra);
            return STAT_INSTCMD_CONVERSION_FAILED;
        }
        upslog_INSTCMD_POWERSTATE_MAYBE(cmdname, extra);
        return sms_send_command(sms_prepare_test_battery_nsec(&bufOut[0], sms_seconds_to_units(delay)), cmdname);
    }

    if (!strcasecmp(cmdname, "test.battery.start.quick")) {
        upslog_INSTCMD_POWERSTATE_MAYBE(cmdname, extra);
        return sms_send_command(sms_prepare_test_battery_nsec(&bufOut[0], sms_seconds_to_units(DEFAULT_TESTDELAY)), cmdname);
    }

    if (!strcasecmp(cmdname, "test.battery.start.deep")) {
        upslog_INSTCMD_POWERSTATE_MAYBE(cmdname, extra);
        /* Runs until the UPS reports a low battery */
        return sms_send_command(sms_prepare_test_battery_low(&bufOut[0]), cmdname);
    }

    if (!strcasecmp(cmdname, "test.battery.stop")) {
        upslog_INSTCMD_POWERSTATE_MAYBE(cmdname, extra);
        return sms_send_command(sms_prepare_cancel_test(&bufOut[0]), cmdname);
    }

    if (!strcasecmp(cmdname, "beeper.toggle")) {
        return sms_send_command(sms_prepare_set_beep(&bufOut[0]), cmdname);
    }

    if (!strcasecmp(cmdname, "shutdown.return")) {
        upslog_INSTCMD_POWERSTATE_CHANGE(cmdname, extra);
        return sms_send_command(sms_prepare_shutdown_restore(&bufOut[0], sms_seconds_to_units(offdelay), ondelay), cmdname);
    }

    if (!strcasecmp(cmdname, "shutdown.reboot")) {
        delay = bootdelay;
        if (extra && !sms_parse_delay(extra, &delay)) {
            upslogx(LOG_ERR, "%s: invalid delay [%s]", cmdname, extra);
            return STAT_INSTCMD_CONVERSION_FAILED;
        }
        upslog_INSTCMD_POWERSTATE_CHANGE(cmdname, extra);
        return sms_send_command(sms_prepare_shutdown_nsec(&bufOut[0], sms_seconds_to_units(delay)), cmdname);
    }

    if (!strcasecmp(cmdname, "shutdown.stop")) {
        upslog_INSTCMD_POWERSTATE_MAYBE(cmdname, extra);
        return sms_send_command(sms_prepare_cancel_shutdown(&bufOut[0]), cmdname);
    }

    upslog_INSTCMD_UNKNOWN(cmdname, extra);
    return STAT_INSTCMD_UNKNOWN;
}

static int sms_setvar(const char *varname, const char *val) {
    uint16_t *target = NULL;

    upsdebug_SET_STARTING(varname, val);

    if (!strcasecmp(varname, "ups.delay.reboot")) {
        target = &bootdelay;
    } else if (!strcasecmp(varname, "ups.delay.shutdown")) {
        target = &offdelay;
    } else if (!strcasecmp(varname, "ups.delay.start")) {
        target = &ondelay;
    } else {
        upslog_SET_UNKNOWN(varname, val);
        return STAT_SET_UNKNOWN;
    }

    if (!sms_parse_delay(val, target)) {
        return STAT_SET_CONVERSION_FAILED;
    }
    dstate_setinfo(varname, "%u", *target);
    return STAT_SET_HANDLED;
}

/* Publishes a delay as a writable number */
static void sms_publish_delay(const char *varname, uint16_t value) {
    dstate_setinfo(varname, "%u", value);
    dstate_setflags(varname, ST_FLAG_RW | ST_FLAG_NUMBER);
}

void upsdrv_initinfo(void) {
    char *battery_status;
    const char *range_digits;

	upsdebugx(LOG_DEBUG, "upsdrv_initinfo");

    if (get_ups_features() != 0) {
        upslogx(LOG_ERR, "Short read from UPS");
        dstate_datastale();
        return;
    }
    if (get_ups_information() != 0) {
        upslogx(LOG_ERR, "Short read from UPS");
        dstate_datastale();
        return;
    }

    if (get_ups_nominal() == 0) {
        dstate_setinfo("device.model", "%s", DeviceData.model);
        dstate_setinfo("ups.firmware", "%s", DeviceData.version);
        dstate_setinfo("input.voltage.nominal", "%s", DeviceData.voltageRange);
        dstate_setinfo("input.current.nominal", "%s", DeviceData.currentRange);
        /* The voltage range is a label like "EBiS115" or "EBiS220" whose
         * number matches the output voltage of the units seen so far */
        range_digits = DeviceData.voltageRange + strcspn(DeviceData.voltageRange, "0123456789");
        if (*range_digits) {
            dstate_setinfo("output.voltage.nominal", "%d", atoi(range_digits));
        }

        switch (DeviceData.upstype) {
            case SMS_TYPE_LINE_INTERACTIVE:
                dstate_setinfo("ups.type", "%s", "line-interactive");
                break;
            case SMS_TYPE_ONLINE_LINE_INTERACTIVE:
                dstate_setinfo("ups.type", "%s", "online line-interactive");
                break;
            case SMS_TYPE_ONLINE:
                dstate_setinfo("ups.type", "%s", "online");
                break;
            default:
                break;
        }
        dstate_setinfo("output.frequency.nominal", "%d", DeviceData.frequency);
        dstate_setinfo("ups.beeper.status", "%s", (DeviceData.beepon == 1) ? "enabled" : "disabled");

        dstate_setinfo("input.voltage.extended", "%.2f", DeviceData.lastinputVac);

        dstate_setinfo("input.voltage", "%.2f", DeviceData.inputVac);
        dstate_setinfo("output.voltage", "%.2f", DeviceData.outputVac);
        dstate_setinfo("ups.load", "%.2f", DeviceData.outputpower);
        dstate_setinfo("output.frequency", "%.2f", DeviceData.outputHz);
        dstate_setinfo("battery.charge", "%.2f", DeviceData.batterylevel);

        dstate_setinfo("battery.voltage.nominal", "%d", DeviceData.voltageBattery);
        dstate_setinfo("battery.packs", "%d", DeviceData.voltageBattery / 12);
        dstate_setinfo("battery.voltage", "%.2f", (DeviceData.voltageBattery * DeviceData.batterylevel) / 100);
        dstate_setinfo("ups.temperature", "%.2f", DeviceData.temperatureC);

        if (DeviceData.onbattery && (uint8_t)DeviceData.batterylevel < 100) {
            upsdebugx(LOG_DEBUG, "on battery and battery < last battery");
            battery_status = "discharging";
        } else if (!DeviceData.onbattery && (uint8_t)DeviceData.batterylevel < 100) {
            upsdebugx(LOG_DEBUG, "on power and battery > last battery");
            battery_status = "charging";
        } else if (!DeviceData.onbattery && (uint8_t)DeviceData.batterylevel == 100) {
            upsdebugx(LOG_DEBUG, "on power and battery == 100");
            battery_status = "resting";
        } else {
            upsdebugx(LOG_DEBUG, "none, floating");
            battery_status = "floating";
        }
        dstate_setinfo("battery.charger.status", "%s", battery_status);
    } else {
        upslogx(LOG_ERR, "Short read from UPS");
        dstate_datastale();
        return;
    }

    dstate_addcmd("test.battery.start");
    dstate_addcmd("test.battery.start.quick");
    dstate_addcmd("test.battery.start.deep");
    dstate_addcmd("test.battery.stop");
    dstate_addcmd("beeper.toggle");
    dstate_addcmd("shutdown.return");
    dstate_addcmd("shutdown.stop");
    dstate_addcmd("shutdown.reboot");

    sms_publish_delay("ups.delay.shutdown", offdelay);
    sms_publish_delay("ups.delay.start", ondelay);
    sms_publish_delay("ups.delay.reboot", bootdelay);

    upsh.instcmd = sms_instcmd;
    upsh.setvar = sms_setvar;
}

void upsdrv_updateinfo(void) {
    char *battery_status;
    const char *charge_low;
    bool lowbattery;
    static bool test_seen = false;

	upsdebugx(LOG_DEBUG, "upsdrv_updateinfo");

    if (get_ups_nominal() != 0) {
        dstate_datastale();
        /* A USB adapter that was unplugged leaves a dead descriptor behind,
         * and may come back under another device node: reopen by name */
        if (++comm_failures >= MAXTRIES) {
            sms_reconnect();
            comm_failures = 0;
        }
        return;
    }
    comm_failures = 0;
    dstate_setinfo("device.mfr", "%s", "SMS");
    dstate_setinfo("ups.mfr", "%s", "SMS");
    dstate_setinfo("ups.model", "%s", DeviceData.model);
    dstate_setinfo("device.model", "%s", DeviceData.model);
    dstate_setinfo("ups.firmware", "%s", DeviceData.version);
    dstate_setinfo("output.frequency.nominal", "%d", DeviceData.frequency);
    dstate_setinfo("ups.beeper.status", "%s", (DeviceData.beepon == 1) ? "enabled" : "disabled");

    dstate_setinfo("input.voltage.extended", "%.2f", DeviceData.lastinputVac);

    dstate_setinfo("input.voltage", "%.2f", DeviceData.inputVac);
    dstate_setinfo("output.voltage", "%.2f", DeviceData.outputVac);
    dstate_setinfo("ups.load", "%.2f", DeviceData.outputpower);
    dstate_setinfo("output.frequency", "%.2f", DeviceData.outputHz);
    dstate_setinfo("battery.charge", "%.2f", DeviceData.batterylevel);

    dstate_setinfo("battery.voltage.nominal", "%d", DeviceData.voltageBattery);
    dstate_setinfo("battery.packs", "%d", DeviceData.voltageBattery / 12);
    dstate_setinfo("battery.voltage", "%.2f", (DeviceData.voltageBattery * DeviceData.batterylevel) / 100);
    dstate_setinfo("ups.temperature", "%.2f", DeviceData.temperatureC);

    upsdebugx(LOG_DEBUG, "battery level: %.2f", DeviceData.batterylevel);
    upsdebugx(LOG_DEBUG, "type: %c", DeviceData.upstype);
    upsdebugx(LOG_DEBUG, "bypass: %d", DeviceData.bypass);
    upsdebugx(LOG_DEBUG, "onBattery: %d", DeviceData.onbattery);
    upsdebugx(LOG_DEBUG, "lowBattery: %d", DeviceData.lowbattery);
    upsdebugx(LOG_DEBUG, "test: %d", DeviceData.test);

    if (DeviceData.onbattery && (uint8_t)DeviceData.batterylevel < 100) {
        upsdebugx(LOG_DEBUG, "on battery and battery < last battery");
        battery_status = "discharging";
    } else if (!DeviceData.onbattery && (uint8_t)DeviceData.batterylevel < 100) {
        upsdebugx(LOG_DEBUG, "on power and battery > last battery");
        battery_status = "charging";
    } else if (!DeviceData.onbattery && (uint8_t)DeviceData.batterylevel == 100) {
        upsdebugx(LOG_DEBUG, "on power and battery == 100");
        battery_status = "resting";
    } else {
        upsdebugx(LOG_DEBUG, "none, floating");
        battery_status = "floating";
    }
    dstate_setinfo("battery.charger.status", "%s", battery_status);

    /* The UPS has a low battery flag of its own; like SMS PowerView, also
     * accept a charge threshold (e.g. "default.battery.charge.low" in
     * ups.conf). The charge figure only means something while on battery:
     * on mains it is a recharge ramp that restarts near 20% after any
     * battery use. */
    lowbattery = DeviceData.lowbattery;
    charge_low = dstate_getinfo("battery.charge.low");
    if (!lowbattery && DeviceData.onbattery && charge_low
     && DeviceData.batterylevel <= strtod(charge_low, NULL)) {
        upsdebugx(LOG_DEBUG, "on battery and charge <= battery.charge.low (%s)", charge_low);
        lowbattery = true;
    }

    /* The flags are independent of each other, so are the NUT statuses */
    status_init();

    status_set(DeviceData.onbattery ? "OB" : "OL");

    if (lowbattery) {
        status_set("LB");
    }
    if (DeviceData.test) {
        /* A battery test runs on battery with mains present; CAL lets
         * upsmon tell it from an outage */
        status_set("CAL");
    }
    if (!DeviceData.upsok) {
        status_set("RB");
    }
    if (DeviceData.boost) {
        status_set("BOOST");
    }
    /* Line-interactive models have no bypass and raise this flag whenever
     * the inverter runs (on battery, battery test); SMS PowerView ignores
     * it for them too. On battery it could never be true anyway. */
    if (DeviceData.bypass && !DeviceData.onbattery
     && DeviceData.upstype != SMS_TYPE_LINE_INTERACTIVE) {
        status_set("BYPASS");
    }

    status_commit();

    if (DeviceData.test) {
        dstate_setinfo("ups.test.result", "%s", "In progress");
        test_seen = true;
    } else if (test_seen) {
        dstate_setinfo("ups.test.result", "%s", "Done");
    }
    dstate_dataok();

    poll_interval = 5;
}

void upsdrv_shutdown(void) {
	/* Only implement "shutdown.default"; do not invoke
	 * general handling of other `sdcommands` here */

    /* tell the UPS to shut down, then return - DO NOT SLEEP HERE */
    int retry;

    /* maybe try to detect the UPS here, but try a shutdown even if
     * it doesn't respond at first if possible */

    /* replace with a proper shutdown function */

    /* you may have to check the line status since the commands
     * for toggling power are frequently different for OL vs. OB */

    /* OL: this must power cycle the load if possible */

    /* OB: the load must remain off until the power returns */
    upsdebugx(2, "upsdrv Shutdown execute");

    for (retry = 1; retry <= MAXTRIES; retry++) {
        /* By default, abort a previously requested shutdown
         * (if any) and schedule a new one from this moment. */
        if (sms_instcmd("shutdown.stop", NULL) != STAT_INSTCMD_HANDLED) {
            continue;
        }

        if (sms_instcmd("shutdown.return", NULL) != STAT_INSTCMD_HANDLED) {
            continue;
        }

        upslogx(LOG_ERR, "Shutting down");
        if (handling_upsdrv_shutdown > 0)
            set_exit_flag(EF_EXIT_SUCCESS);
        return;
    }

    upslogx(LOG_ERR, "Shutdown failed!");
    if (handling_upsdrv_shutdown > 0)
        set_exit_flag(EF_EXIT_FAILURE);
}

void upsdrv_help(void) {
}

/* optionally tweak prognames[] entries */
void upsdrv_tweak_prognames(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void) {
    char msg[256];

    upsdebugx(LOG_DEBUG, "upsdrv_makevartable");

    snprintf(msg, sizeof msg, "Set reboot delay, in seconds (default=%d).",
             DEFAULT_BOOTDELAY);
    addvar(VAR_VALUE, "rebootdelay", msg);

    snprintf(msg, sizeof msg, "Set shutdown delay for shutdown.return, in seconds (default=%d).",
             DEFAULT_OFFDELAY);
    addvar(VAR_VALUE, "offdelay", msg);

    snprintf(msg, sizeof msg, "Set delay before the output returns after shutdown.return (default=%d).",
             DEFAULT_ONDELAY);
    addvar(VAR_VALUE, "ondelay", msg);
}

void upsdrv_initups(void) {
    char *val;

	upsdebugx(LOG_DEBUG, "upsdrv_initups");

    upsfd = ser_open(device_path);
    ser_set_speed(upsfd, device_path, B2400);

    if ((val = getval("rebootdelay")) && !sms_parse_delay(val, &bootdelay)) {
        fatalx(EXIT_FAILURE, "Invalid rebootdelay [%s]", val);
    }
    if ((val = getval("offdelay")) && !sms_parse_delay(val, &offdelay)) {
        fatalx(EXIT_FAILURE, "Invalid offdelay [%s]", val);
    }
    if ((val = getval("ondelay")) && !sms_parse_delay(val, &ondelay)) {
        fatalx(EXIT_FAILURE, "Invalid ondelay [%s]", val);
    }
}

void upsdrv_cleanup(void) {
    upsdebugx(LOG_DEBUG, "upsdrv_cleanup");
    /* free(dynamic_mem); */
    ser_close(upsfd, device_path);
}

uint8_t sms_prepare_get_status(uint8_t *buffer) {
    buffer[0] = 'Q';
    buffer[1] = 255;
    buffer[2] = 255;
    buffer[3] = 255;
    buffer[4] = 255;
    buffer[5] = (buffer[0] + buffer[1] + buffer[2] + buffer[3] + buffer[4]) * 255;
    buffer[6] = ENDCHAR;

    return 7;
}

uint8_t sms_prepare_get_information(uint8_t *buffer) {
    buffer[0] = 'I';
    buffer[1] = 255;
    buffer[2] = 255;
    buffer[3] = 255;
    buffer[4] = 255;
    buffer[5] = (buffer[0] + buffer[1] + buffer[2] + buffer[3] + buffer[4]) * 255;
    buffer[6] = ENDCHAR;

    return 7;
}

uint8_t sms_prepare_get_features(uint8_t *buffer) {
    buffer[0] = 'F';
    buffer[1] = 255;
    buffer[2] = 255;
    buffer[3] = 255;
    buffer[4] = 255;
    buffer[5] = (buffer[0] + buffer[1] + buffer[2] + buffer[3] + buffer[4]) * 255;
    buffer[6] = ENDCHAR;

    return 7;
}

uint8_t sms_prepare_set_beep(uint8_t *buffer) {
    buffer[0] = 'M';
    buffer[1] = 255;
    buffer[2] = 255;
    buffer[3] = 255;
    buffer[4] = 255;
    buffer[5] = (buffer[0] + buffer[1] + buffer[2] + buffer[3] + buffer[4]) * 255;
    buffer[6] = ENDCHAR;

    return 7;
}

uint8_t sms_prepare_test_battery_low(uint8_t *buffer) {
    buffer[0] = 'L';
    buffer[1] = 255;
    buffer[2] = 255;
    buffer[3] = 255;
    buffer[4] = 255;
    buffer[5] = (buffer[0] + buffer[1] + buffer[2] + buffer[3] + buffer[4]) * 255;
    buffer[6] = ENDCHAR;

    return 7;
}

uint8_t sms_prepare_test_battery_nsec(uint8_t *buffer, uint16_t delay) {
    /* The delay is a 16-bit big-endian value, the unused pair is zero */
    buffer[0] = 'T';
    buffer[1] = (uint8_t)(delay >> 8);
    buffer[2] = (uint8_t)(delay & 0xFF);
    buffer[3] = 0;
    buffer[4] = 0;
    buffer[5] = (buffer[0] + buffer[1] + buffer[2] + buffer[3] + buffer[4]) * 255;
    buffer[6] = ENDCHAR;

    return 7;
}

uint8_t sms_prepare_shutdown_nsec(uint8_t *buffer, uint16_t delay) {
    /* The delay is a 16-bit big-endian value, the unused pair is zero */
    buffer[0] = 'S';
    buffer[1] = (uint8_t)(delay >> 8);
    buffer[2] = (uint8_t)(delay & 0xFF);
    buffer[3] = 0;
    buffer[4] = 0;
    buffer[5] = (buffer[0] + buffer[1] + buffer[2] + buffer[3] + buffer[4]) * 255;
    buffer[6] = ENDCHAR;

    return 7;
}

uint8_t sms_prepare_shutdown_restore(uint8_t *buffer, uint16_t shutdown_delay, uint16_t restore_delay) {
    /* Two 16-bit big-endian values: delay before the output is cut, then
     * delay before it returns once mains is back */
    buffer[0] = 'R';
    buffer[1] = (uint8_t)(shutdown_delay >> 8);
    buffer[2] = (uint8_t)(shutdown_delay & 0xFF);
    buffer[3] = (uint8_t)(restore_delay >> 8);
    buffer[4] = (uint8_t)(restore_delay & 0xFF);
    buffer[5] = (buffer[0] + buffer[1] + buffer[2] + buffer[3] + buffer[4]) * 255;
    buffer[6] = ENDCHAR;

    return 7;
}

uint8_t sms_prepare_cancel_test(uint8_t *buffer) {
    buffer[0] = 'D';
    buffer[1] = 255;
    buffer[2] = 255;
    buffer[3] = 255;
    buffer[4] = 255;
    buffer[5] = (buffer[0] + buffer[1] + buffer[2] + buffer[3] + buffer[4]) * 255;
    buffer[6] = ENDCHAR;

    return 7;
}

uint8_t sms_prepare_cancel_shutdown(uint8_t *buffer) {
    buffer[0] = 'C';
    buffer[1] = 255;
    buffer[2] = 255;
    buffer[3] = 255;
    buffer[4] = 255;
    buffer[5] = (buffer[0] + buffer[1] + buffer[2] + buffer[3] + buffer[4]) * 255;
    buffer[6] = ENDCHAR;

    return 7;
}
