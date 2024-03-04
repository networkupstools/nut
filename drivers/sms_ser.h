/*
 * sms_ser.h: defines/macros protocol for SMS Brazil UPSes
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

#ifndef NUT_SMS_SER_H_SEEN
#define NUT_SMS_SER_H_SEEN 1

#include <stdbool.h> /* bool type */

#include "nut_stdint.h"

#define DEFAULT_BOOTDELAY 64 /* seconds (max 0xFF) */
#define MAXTRIES 3

typedef struct {
    char model[25];          /* device.model */
    char version[7];         /* ups.firmware */
    char voltageRange[15];   /* garbage from sms (it's a string with some strange items) */
    char currentRange[7];    /* garbage from sms (it's 000) maybe with some cleanup on string input.voltage.nominal */
    uint8_t voltageBattery;  /* battery.voltage.nominal (DeviceData.voltageBattery / 12 = battery.packs) */
    uint8_t frequency;       /* output.frequency.nominal */

    bool beepon;      /* ups.beeper.status */
    bool shutdown;    /* ups.status = FSD (the shutdown has started by another via) */
    bool test;        /* the UPS is testing the battery, need a status ? */
    bool upsok;       /* ups.status or battery.status ? (Maybe RB if is False ?) */
    bool boost;       /* ups.status = BOOST */
    bool bypass;      /* ups.status = BYPASS */
    bool lowbattery;  /* ups.status = LB (OL + LB or OB + LB ?) */
    bool onbattery;   /* ups.status = OB + battery.charger.status = discharging */

    float lastinputVac;      /* garbage ? always 000 */
    float inputVac;          /* input.voltage */
    float outputVac;         /* output.voltage */
    float outputpower;       /* ups.load */
    float outputHz;          /* output.frequency */
    float batterylevel;      /* battery.charge (batterylevel < 100 ? battery.charger.status = charging if onacpower/discharging if onbattery : resting)
                              * battery.voltage = (voltageBattery * batterylevel) / 100)
                              */
    float temperatureC;      /* ups.temperature */
} SmsData;

void sms_parse_features(uint8_t *rawvalues, SmsData *results);
void sms_parse_information(uint8_t *rawvalues, SmsData *results);
void sms_parse_results(uint8_t* rawvalues, SmsData* results);

uint8_t sms_prepare_get_status(uint8_t* buffer);
uint8_t sms_prepare_get_information(uint8_t* buffer);
uint8_t sms_prepare_get_features(uint8_t* buffer);
uint8_t sms_prepare_set_beep(uint8_t* buffer);
uint8_t sms_prepare_test_battery_low(uint8_t* buffer);
uint8_t sms_prepare_test_battery_nsec(uint8_t* buffer, uint16_t delay);
uint8_t sms_prepare_shutdown_nsec(uint8_t* buffer, uint16_t delay);
uint8_t sms_prepare_shutdown_restore(uint8_t* buffer);
uint8_t sms_prepare_cancel_test(uint8_t* buffer);
uint8_t sms_prepare_cancel_shutdown(uint8_t* buffer);

#endif	/* NUT_SMS_SER_H_SEEN */
