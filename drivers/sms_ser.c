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

#define DRIVER_NAME "SMS Brazil UPS driver"
#define DRIVER_VERSION "1.01"

#define QUERY_SIZE 7
#define BUFFER_SIZE 18
#define RESULT_SIZE 18
#define HUMAN_VALUES 7

static uint16_t bootdelay = DEFAULT_BOOTDELAY;
static uint8_t bufOut[BUFFER_SIZE];
static uint8_t bufIn[BUFFER_SIZE];
static SmsData DeviceData;

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

    memset(buf, 0, BUFFER_SIZE);
    sprintf(buf, "0x%02x%02x", (unsigned char)rawvalues[1], (unsigned char)rawvalues[2]);
    v = strtol(buf, NULL, 16); /* 16 == hex */
    h = v / 10;
    results->lastinputVac = h;

    memset(buf, 0, BUFFER_SIZE);
    sprintf(buf, "0x%02x%02x", (unsigned char)rawvalues[3], (unsigned char)rawvalues[4]);
    v = strtol(buf, NULL, 16); /* 16 == hex */
    h = v / 10;
    results->inputVac = h;

    memset(buf, 0, BUFFER_SIZE);
    sprintf(buf, "0x%02x%02x", (unsigned char)rawvalues[5], (unsigned char)rawvalues[6]);
    v = strtol(buf, NULL, 16); /* 16 == hex */
    h = v / 10;
    results->outputVac = h;

    memset(buf, 0, BUFFER_SIZE);
    sprintf(buf, "0x%02x%02x", (unsigned char)rawvalues[7], (unsigned char)rawvalues[8]);
    v = strtol(buf, NULL, 16); /* 16 == hex */
    h = v / 10;
    results->outputpower = h;

    memset(buf, 0, BUFFER_SIZE);
    sprintf(buf, "0x%02x%02x", (unsigned char)rawvalues[9], (unsigned char)rawvalues[10]);
    v = strtol(buf, NULL, 16); /* 16 == hex */
    h = v / 10;
    results->outputHz = h;

    memset(buf, 0, BUFFER_SIZE);
    sprintf(buf, "0x%02x%02x", (unsigned char)rawvalues[11], (unsigned char)rawvalues[12]);
    v = strtol(buf, NULL, 16); /* 16 == hex */
    h = v / 10;
    results->batterylevel = h;

    memset(buf, 0, BUFFER_SIZE);
    sprintf(buf, "0x%02x%02x", (unsigned char)rawvalues[13], (unsigned char)rawvalues[14]);
    v = strtol(buf, NULL, 16); /* 16 == hex */
    h = v / 10;
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

static int get_ups_nominal(void) {
    uint8_t length;
    ssize_t ret;

    upsdebugx(LOG_DEBUG, "get_ups_nominal");

    length = sms_prepare_get_status(&bufOut[0]);

    if (ser_send_buf(upsfd, bufOut, length) == 0) {
        upsdebugx(LOG_ERR, "Communication error while writing to port");
        return -1;
    }
    memset(bufIn, 0, BUFFER_SIZE);
    ret = ser_get_buf_len(upsfd, &bufIn[0], BUFFER_SIZE, 3, 1000);

    if (ret < RESULT_SIZE) {
        upslogx(LOG_ERR, "Short read from UPS");
        dstate_datastale();
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

    if (ser_send_buf(upsfd, bufOut, length) == 0) {
        upsdebugx(LOG_ERR, "Communication error while writing to port");
        return -1;
    }
    memset(bufIn, 0, BUFFER_SIZE);
    ret = ser_get_buf_len(upsfd, &bufIn[0], BUFFER_SIZE, 3, 1000);

    if (ret < RESULT_SIZE) {
        upslogx(LOG_ERR, "Short read from UPS");
        dstate_datastale();
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

    if (ser_send_buf(upsfd, bufOut, length) == 0) {
        upsdebugx(LOG_ERR, "Communication error while writing to port");
        return -1;
    }
    memset(bufIn, 0, BUFFER_SIZE);
    ret = ser_get_buf_len(upsfd, &bufIn[0], BUFFER_SIZE, 3, 1000);

    if (ret < RESULT_SIZE) {
        upslogx(LOG_ERR, "Short read from UPS");
        dstate_datastale();
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

static int sms_instcmd(const char *cmdname, const char *extra) {
    size_t length;

    if (!strcasecmp(cmdname, "test.battery.start")) {
        long delay = extra ? strtol(extra, NULL, 10) : 10;
        length = sms_prepare_test_battery_nsec(&bufOut[0], delay);

        if (ser_send_buf(upsfd, bufOut, length) == 0) {
            upsdebugx(3, "failed to send test.battery.start");
            return STAT_INSTCMD_FAILED;
        }
        upsdebugx(3, "command test.battery.start OK!");
        return STAT_INSTCMD_HANDLED;
    }

    if (!strcasecmp(cmdname, "test.battery.start.quick")) {
        length = sms_prepare_test_battery_low(&bufOut[0]);

        if (ser_send_buf(upsfd, bufOut, length) == 0) {
            upsdebugx(3, "failed to send test.battery.start.quick");
            return STAT_INSTCMD_FAILED;
        }
        upsdebugx(3, "command test.battery.start.quick OK!");

        return STAT_INSTCMD_HANDLED;
    }

    if (!strcasecmp(cmdname, "test.battery.stop")) {
        length = sms_prepare_cancel_test(&bufOut[0]);

        if (ser_send_buf(upsfd, bufOut, length) == 0) {
            upsdebugx(3, "failed to send test.battery.stop");
            return STAT_INSTCMD_FAILED;
        }
        upsdebugx(3, "command test.battery.stop OK!");

        return STAT_INSTCMD_HANDLED;
    }

    if (!strcasecmp(cmdname, "beeper.toggle")) {
        length = sms_prepare_set_beep(&bufOut[0]);

        if (ser_send_buf(upsfd, bufOut, length) == 0) {
            upsdebugx(3, "failed to send beeper.toggle");
            return STAT_INSTCMD_FAILED;
        }
        upsdebugx(3, "command beeper.toggle OK!");

        return STAT_INSTCMD_HANDLED;
    }

    if (!strcasecmp(cmdname, "shutdown.return")) {
        length = sms_prepare_shutdown_restore(&bufOut[0]);

        if (ser_send_buf(upsfd, bufOut, length) == 0) {
            upsdebugx(3, "failed to send shutdown.return");
            return STAT_INSTCMD_FAILED;
        }
        upsdebugx(3, "command shutdown.return OK!");

        return STAT_INSTCMD_HANDLED;
    }

    if (!strcasecmp(cmdname, "shutdown.reboot")) {
        uint16_t delay = bootdelay;
        if (extra) {
            long ldelay = strtol(extra, NULL, bootdelay);
            if (ldelay >= 0 && (intmax_t)ldelay < (intmax_t)UINT16_MAX) {
                delay = (uint16_t)ldelay;
            } else {
                upsdebugx(3, "tried to set up extra shutdown.reboot delay ut it was out of range, keeping default");
            }
        }
        length = sms_prepare_shutdown_nsec(&bufOut[0], delay);

        if (ser_send_buf(upsfd, bufOut, length) == 0) {
            upsdebugx(3, "failed to send shutdown.reboot");
            return STAT_INSTCMD_FAILED;
        }
        upsdebugx(3, "command shutdown.reboot OK!");

        return STAT_INSTCMD_HANDLED;
    }

    if (!strcasecmp(cmdname, "shutdown.stop")) {
        length = sms_prepare_cancel_shutdown(&bufOut[0]);

        if (ser_send_buf(upsfd, bufOut, length) == 0) {
            upsdebugx(3, "failed to send shutdown.stop");
            return STAT_INSTCMD_FAILED;
        }
        upsdebugx(3, "command shutdown.stop OK!");

        return STAT_INSTCMD_HANDLED;
    }

    upslogx(LOG_NOTICE, "sms_instcmd: unknown command [%s]", cmdname);
    return STAT_INSTCMD_UNKNOWN;
}

static int sms_setvar(const char *varname, const char *val) {
    if (!strcasecmp(varname, "ups.delay.reboot")) {
        int ipv = atoi(val);
        if (ipv >= 0)
            bootdelay = (unsigned int)ipv;
        dstate_setinfo("ups.delay.reboot", "%u", bootdelay);
        return STAT_SET_HANDLED;
    }
    return STAT_SET_UNKNOWN;
}

void upsdrv_initinfo(void) {
    char *battery_status;

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
    dstate_addcmd("test.battery.stop");
    dstate_addcmd("beeper.toggle");
    dstate_addcmd("shutdown.return");
    dstate_addcmd("shutdown.stop");
    dstate_addcmd("shutdown.reboot");

    upsh.instcmd = sms_instcmd;
    upsh.setvar = sms_setvar;
}

void upsdrv_updateinfo(void) {
    char *battery_status;

	upsdebugx(LOG_DEBUG, "upsdrv_updateinfo");

    if (get_ups_nominal() != 0) {
        upslogx(LOG_ERR, "Short read from UPS");
        dstate_datastale();
        return;
    }
    dstate_setinfo("device.model", "%s", DeviceData.model);
    dstate_setinfo("ups.firmware", "%s", DeviceData.version);
    dstate_setinfo("battery.voltage.nominal", "%s", DeviceData.voltageRange);
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
    upsdebugx(LOG_DEBUG, "bypass: %d", DeviceData.bypass);
    upsdebugx(LOG_DEBUG, "onBattery: %d", DeviceData.onbattery);

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

    status_init();

    if (DeviceData.bypass) {
        upsdebugx(LOG_DEBUG, "setting status to BYPASS");
        status_set("BYPASS");
    } else if (DeviceData.onbattery) {
        upsdebugx(LOG_DEBUG, "setting status to OB");
        status_set("OB");
    } else if (DeviceData.lowbattery) {
        upsdebugx(LOG_DEBUG, "setting status to LB");
        status_set("LB");
    } else if (!DeviceData.upsok) {
        upsdebugx(LOG_DEBUG, "setting status to RB");
        status_set("RB");
    } else if (DeviceData.boost) {
        upsdebugx(LOG_DEBUG, "setting status to BOOST");
        status_set("BOOST");
    } else if (!DeviceData.onbattery) {
        /* sometimes the flag "onacpower" is not set */
        upsdebugx(LOG_DEBUG, "setting status to OL");
        status_set("OL");
    } else {
        /* None of these parameters is ON, but we got some response,
		 * so the device is (administratively) OFF ? */
        upsdebugx(LOG_DEBUG, "setting status to OFF");
        status_set("OFF");
    }

    status_commit();
    dstate_dataok();

    poll_interval = 5;
}

void upsdrv_shutdown(void) {
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
        if (sms_instcmd("shutdown.stop", NULL) != STAT_INSTCMD_HANDLED) {
            continue;
        }

        if (sms_instcmd("shutdown.return", NULL) != STAT_INSTCMD_HANDLED) {
            continue;
        }

        upslogx(LOG_ERR, "Shutting down");
        set_exit_flag(-2); /* EXIT_SUCCESS */
        return;
    }

    upslogx(LOG_ERR, "Shutdown failed!");
    set_exit_flag(-1);
}

void upsdrv_help(void) {
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void) {
    char msg[256];

    upsdebugx(LOG_DEBUG, "upsdrv_makevartable");

    snprintf(msg, sizeof msg, "Set reboot delay, in seconds (default=%d).",
             DEFAULT_BOOTDELAY);
    addvar(VAR_VALUE, "rebootdelay", msg);
}

void upsdrv_initups(void) {
    char *val;

	upsdebugx(LOG_DEBUG, "upsdrv_initups");

    upsfd = ser_open(device_path);
    ser_set_speed(upsfd, device_path, B2400);

    if ((val = getval("rebootdelay"))) {
        int ipv = atoi(val);
        if (ipv >= 0)
            bootdelay = (unsigned int)ipv;
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
    buffer[0] = 'T';
    buffer[1] = (uint8_t)(delay % 256);
    buffer[2] = 255;
    buffer[3] = 255;
    buffer[4] = 255;
    buffer[5] = (buffer[0] + buffer[1] + buffer[2] + buffer[3] + buffer[4]) * 255;
    buffer[6] = ENDCHAR;

    return 7;
}

uint8_t sms_prepare_shutdown_nsec(uint8_t *buffer, uint16_t delay) {
    buffer[0] = 'S';
    buffer[1] = (uint8_t)(delay % 256);
    buffer[2] = 255;
    buffer[3] = 255;
    buffer[4] = 255;
    buffer[5] = (buffer[0] + buffer[1] + buffer[2] + buffer[3] + buffer[4]) * 255;
    buffer[6] = ENDCHAR;

    return 7;
}

uint8_t sms_prepare_shutdown_restore(uint8_t *buffer) {
    buffer[0] = 'R';
    buffer[1] = 255;
    buffer[2] = 255;
    buffer[3] = 255;
    buffer[4] = 255;
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
