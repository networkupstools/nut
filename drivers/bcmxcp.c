/*
 bcmxcp.c - driver for powerware UPS

 Total rewrite of bcmxcp.c (nut ver-1.4.3)
 * Copyright (c) 2002, Martin Schroeder *
 * emes -at- geomer.de *
 * All rights reserved.*

 Copyright (C) 2004 Kjell Claesson <kjell.claesson-at-telia.com>
 and Tore �petveit <tore-at-orpetveit.net>

 Thanks to Tore �petveit <tore-at-orpetveit.net> that sent me the
 manuals for bcm/xcp.

 And to Fabio Di Niro <fabio.diniro@email.it> and his metasys module.
 It influenced the layout of this driver.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "main.h"

#include <math.h>		/* For ldexp() */
#include <float.h> 		/*for FLT_MAX */
#include "serial.h"
#include "bcmxcp.h"

#define DRV_VERSION "0.08"

/* get_word funktion from nut driver metasys.c */
int get_word(const unsigned char *buffer)	/* return an integer reading a word in the supplied buffer */
{
	unsigned char a, b;
	int result;

	a = buffer[0];
	b = buffer[1];
	result = b*256 + a;

	return result;
}

/* get_long funktion from nut driver metasys.c for meter readings*/
long int get_long(const unsigned char *buffer)	/* return a long integer reading 4 bytes in the supplied buffer.*/
{
	unsigned char a, b, c, d;
	long int result;

	a = buffer[0];
	b = buffer[1];
	c = buffer[2];
	d = buffer[3];
	result = (256*256*256*d) + (256*256*c) + (256*b) + a;

	return result;
}

/* get_float funktion for convering IEEE-754 to float */
float get_float(const unsigned char *data)
{
	int s, e;
	unsigned long src;
	long f;

	src = ((unsigned long)data[3] << 24) |
	((unsigned long)data[2] << 16) |
	((unsigned long)data[1] << 8) |
	((unsigned long)data[0]);

	s = (src & 0x80000000UL) >> 31;
	e = (src & 0x7F800000UL) >> 23;
	f = (src & 0x007FFFFFUL);

	if (e == 255 && f != 0)	
	{
		/* NaN (Not a Number) */
		return FLT_MAX;
	}
	
	if (e == 255 && f == 0 && s == 1)
	{
		/* Negative infinity */
		return -FLT_MAX;
	}
	 
	if (e == 255 && f == 0 && s == 0)
	{
		/* Positive infinity */
		return FLT_MAX;
	}
	
	if (e > 0 && e < 255)
	{
		/* Normal number */
		f += 0x00800000UL;
		if (s) f = -f;
		return ldexp(f, e - 150);
	}

	if (e == 0 && f != 0)
	{
		/* Denormal number */
		if (s) f = -f;
		return ldexp(f, -149);
	}
	 
	if (e == 0 && f == 0 && (s == 1 || s == 0))
	{
		/* Zero */
		return 0;
	}

	/* Never happens */
	upslogx(LOG_ERR, "s = %d, e = %d, f = %lu\n", s, e, f);
	return 0;
}


static int checksum_test(const unsigned char *buf)
{
	unsigned char c;
	int i;

	c = 0;
	for(i = 0; i < 5 + buf[2]; i++)
		c += buf[i];

	return c == 0;
}


unsigned char calc_checksum(const unsigned char *buf)
{
	unsigned char c;
	int i;

	c = 0;
	for(i = 0; i < 2 + buf[1]; i++)
		c -= buf[i];

	return c;
}

void init_meter_map()
{
	/* Clean entire map */
	memset(&bcmxcp_meter_map, 0, sizeof(BCMXCP_METER_MAP_ENTRY) * BCMXCP_METER_MAP_MAX);

	/* Set all corresponding mappings NUT <-> BCM/XCP */
	bcmxcp_meter_map[27].nut_entity = "output.frequency";
	bcmxcp_meter_map[28].nut_entity = "input.frequency";
	bcmxcp_meter_map[32].nut_entity = "battery.current";
	bcmxcp_meter_map[33].nut_entity = "battery.voltage";
	bcmxcp_meter_map[34].nut_entity = "battery.charge";
	bcmxcp_meter_map[35].nut_entity = "battery.runtime";
	bcmxcp_meter_map[56].nut_entity = "input.voltage";
	bcmxcp_meter_map[57].nut_entity = "input.voltage";
	bcmxcp_meter_map[58].nut_entity = "input.voltage";
	bcmxcp_meter_map[62].nut_entity = "ambient.temperature";
	bcmxcp_meter_map[63].nut_entity = "ups.temperature";
	bcmxcp_meter_map[65].nut_entity = "output.current";
	bcmxcp_meter_map[66].nut_entity = "output.current";
	bcmxcp_meter_map[67].nut_entity = "output.current";
	bcmxcp_meter_map[77].nut_entity = "battery.temperature";
	bcmxcp_meter_map[78].nut_entity = "output.voltage";
	bcmxcp_meter_map[79].nut_entity = "output.voltage";
	bcmxcp_meter_map[80].nut_entity = "output.voltage";
}

void init_alarm_map()
{
	/* Clean entire map */
	memset(&bcmxcp_alarm_map, 0, sizeof(BCMXCP_ALARM_MAP_ENTRY) * BCMXCP_ALARM_MAP_MAX);
	
	/* Set all alarm descriptions	*/
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_AC_OVER_VOLTAGE].alarm_desc = "INVERTER_AC_OVER_VOLTAGE"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_AC_UNDER_VOLTAGE].alarm_desc = "INVERTER_AC_UNDER_VOLTAGE";
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_OVER_OR_UNDER_FREQ].alarm_desc = "INVERTER_OVER_OR_UNDER_FREQ";
	bcmxcp_alarm_map[BCMXCP_ALARM_BYPASS_AC_OVER_VOLTAGE].alarm_desc = "BYPASS_AC_OVER_VOLTAGE"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_BYPASS_AC_UNDER_VOLTAGE].alarm_desc = "BYPASS_AC_UNDER_VOLTAGE";
	bcmxcp_alarm_map[BCMXCP_ALARM_BYPASS_OVER_OR_UNDER_FREQ].alarm_desc = "BYPASS_OVER_OR_UNDER_FREQ";
	bcmxcp_alarm_map[BCMXCP_ALARM_INPUT_AC_OVER_VOLTAGE].alarm_desc = "INPUT_AC_OVER_VOLTAGE";
	bcmxcp_alarm_map[BCMXCP_ALARM_INPUT_AC_UNDER_VOLTAGE].alarm_desc = "INPUT_AC_UNDER_VOLTAGE"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_INPUT_UNDER_OR_OVER_FREQ].alarm_desc = "INPUT_UNDER_OR_OVER_FREQ"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_OUTPUT_OVER_VOLTAGE].alarm_desc = "OUTPUT_OVER_VOLTAGE";
	bcmxcp_alarm_map[BCMXCP_ALARM_OUTPUT_UNDER_VOLTAGE].alarm_desc = "OUTPUT_UNDER_VOLTAGE"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_OUTPUT_UNDER_OR_OVER_FREQ].alarm_desc = "OUTPUT_UNDER_OR_OVER_FREQ";
	bcmxcp_alarm_map[BCMXCP_ALARM_REMOTE_EMERGENCY_PWR_OFF].alarm_desc = "REMOTE_EMERGENCY_PWR_OFF"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_REMOTE_GO_TO_BYPASS].alarm_desc = "REMOTE_GO_TO_BYPASS";
	bcmxcp_alarm_map[BCMXCP_ALARM_BUILDING_ALARM_6].alarm_desc = "BUILDING_ALARM_6"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_BUILDING_ALARM_5].alarm_desc = "BUILDING_ALARM_5"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_BUILDING_ALARM_4].alarm_desc = "BUILDING_ALARM_4"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_BUILDING_ALARM_3].alarm_desc = "BUILDING_ALARM_3"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_BUILDING_ALARM_2].alarm_desc = "BUILDING_ALARM_2"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_BUILDING_ALARM_1].alarm_desc = "BUILDING_ALARM_1"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_STATIC_SWITCH_OVER_TEMP].alarm_desc = "STATIC_SWITCH_OVER_TEMP";
	bcmxcp_alarm_map[BCMXCP_ALARM_CHARGER_OVER_TEMP].alarm_desc = "CHARGER_OVER_TEMP";
	bcmxcp_alarm_map[BCMXCP_ALARM_CHARGER_LOGIC_PWR_FAIL].alarm_desc = "CHARGER_LOGIC_PWR_FAIL"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_CHARGER_OVER_VOLTAGE_OR_CURRENT].alarm_desc = "CHARGER_OVER_VOLTAGE_OR_CURRENT";
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_OVER_TEMP].alarm_desc = "INVERTER_OVER_TEMP"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_OUTPUT_OVERLOAD].alarm_desc = "OUTPUT_OVERLOAD";
	bcmxcp_alarm_map[BCMXCP_ALARM_RECTIFIER_INPUT_OVER_CURRENT].alarm_desc = "RECTIFIER_INPUT_OVER_CURRENT"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_OUTPUT_OVER_CURRENT].alarm_desc = "INVERTER_OUTPUT_OVER_CURRENT"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_DC_LINK_OVER_VOLTAGE].alarm_desc = "DC_LINK_OVER_VOLTAGE"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_DC_LINK_UNDER_VOLTAGE].alarm_desc = "DC_LINK_UNDER_VOLTAGE";
	bcmxcp_alarm_map[BCMXCP_ALARM_RECTIFIER_FAILED].alarm_desc = "RECTIFIER_FAILED"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_FAULT].alarm_desc = "INVERTER_FAULT"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_BATTERY_CONNECTOR_FAIL].alarm_desc = "BATTERY_CONNECTOR_FAIL"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_BYPASS_BREAKER_FAIL].alarm_desc = "BYPASS_BREAKER_FAIL";
	bcmxcp_alarm_map[BCMXCP_ALARM_CHARGER_FAIL].alarm_desc = "CHARGER_FAIL"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_RAMP_UP_FAILED].alarm_desc = "RAMP_UP_FAILED"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_STATIC_SWITCH_FAILED].alarm_desc = "STATIC_SWITCH_FAILED"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_ANALOG_AD_REF_FAIL].alarm_desc = "ANALOG_AD_REF_FAIL"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_BYPASS_UNCALIBRATED].alarm_desc = "BYPASS_UNCALIBRATED";
	bcmxcp_alarm_map[BCMXCP_ALARM_RECTIFIER_UNCALIBRATED].alarm_desc = "RECTIFIER_UNCALIBRATED"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_OUTPUT_UNCALIBRATED].alarm_desc = "OUTPUT_UNCALIBRATED";
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_UNCALIBRATED].alarm_desc = "INVERTER_UNCALIBRATED";
	bcmxcp_alarm_map[BCMXCP_ALARM_DC_VOLT_UNCALIBRATED].alarm_desc = "DC_VOLT_UNCALIBRATED"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_OUTPUT_CURRENT_UNCALIBRATED].alarm_desc = "OUTPUT_CURRENT_UNCALIBRATED";
	bcmxcp_alarm_map[BCMXCP_ALARM_RECTIFIER_CURRENT_UNCALIBRATED].alarm_desc = "RECTIFIER_CURRENT_UNCALIBRATED"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_BATTERY_CURRENT_UNCALIBRATED].alarm_desc = "BATTERY_CURRENT_UNCALIBRATED"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_ON_OFF_STAT_FAIL].alarm_desc = "INVERTER_ON_OFF_STAT_FAIL";
	bcmxcp_alarm_map[BCMXCP_ALARM_BATTERY_CURRENT_LIMIT].alarm_desc = "BATTERY_CURRENT_LIMIT";
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_STARTUP_FAIL].alarm_desc = "INVERTER_STARTUP_FAIL";
	bcmxcp_alarm_map[BCMXCP_ALARM_ANALOG_BOARD_AD_STAT_FAIL].alarm_desc = "ANALOG_BOARD_AD_STAT_FAIL";
	bcmxcp_alarm_map[BCMXCP_ALARM_OUTPUT_CURRENT_OVER_100].alarm_desc = "OUTPUT_CURRENT_OVER_100";
	bcmxcp_alarm_map[BCMXCP_ALARM_BATTERY_GROUND_FAULT].alarm_desc = "BATTERY_GROUND_FAULT"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_WAITING_FOR_CHARGER_SYNC].alarm_desc = "WAITING_FOR_CHARGER_SYNC"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_NV_RAM_FAIL].alarm_desc = "NV_RAM_FAIL";
	bcmxcp_alarm_map[BCMXCP_ALARM_ANALOG_BOARD_AD_TIMEOUT].alarm_desc = "ANALOG_BOARD_AD_TIMEOUT";
	bcmxcp_alarm_map[BCMXCP_ALARM_SHUTDOWN_IMMINENT].alarm_desc = "SHUTDOWN_IMMINENT";
	bcmxcp_alarm_map[BCMXCP_ALARM_BATTERY_LOW].alarm_desc = "BATTERY_LOW";
	bcmxcp_alarm_map[BCMXCP_ALARM_UTILITY_FAIL].alarm_desc = "UTILITY_FAIL"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_OUTPUT_SHORT_CIRCUIT].alarm_desc = "OUTPUT_SHORT_CIRCUIT"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_UTILITY_NOT_PRESENT].alarm_desc = "UTILITY_NOT_PRESENT";
	bcmxcp_alarm_map[BCMXCP_ALARM_FULL_TIME_CHARGING].alarm_desc = "FULL_TIME_CHARGING"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_FAST_BYPASS_COMMAND].alarm_desc = "FAST_BYPASS_COMMAND";
	bcmxcp_alarm_map[BCMXCP_ALARM_AD_ERROR].alarm_desc = "AD_ERROR"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_INTERNAL_COM_FAIL].alarm_desc = "INTERNAL_COM_FAIL";
	bcmxcp_alarm_map[BCMXCP_ALARM_RECTIFIER_SELFTEST_FAIL].alarm_desc = "RECTIFIER_SELFTEST_FAIL";
	bcmxcp_alarm_map[BCMXCP_ALARM_RECTIFIER_EEPROM_FAIL].alarm_desc = "RECTIFIER_EEPROM_FAIL";
	bcmxcp_alarm_map[BCMXCP_ALARM_RECTIFIER_EPROM_FAIL].alarm_desc = "RECTIFIER_EPROM_FAIL"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_INPUT_LINE_VOLTAGE_LOSS].alarm_desc = "INPUT_LINE_VOLTAGE_LOSS";
	bcmxcp_alarm_map[BCMXCP_ALARM_BATTERY_DC_OVER_VOLTAGE].alarm_desc = "BATTERY_DC_OVER_VOLTAGE";
	bcmxcp_alarm_map[BCMXCP_ALARM_POWER_SUPPLY_OVER_TEMP].alarm_desc = "POWER_SUPPLY_OVER_TEMP"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_POWER_SUPPLY_FAIL].alarm_desc = "POWER_SUPPLY_FAIL";
	bcmxcp_alarm_map[BCMXCP_ALARM_POWER_SUPPLY_5V_FAIL].alarm_desc = "POWER_SUPPLY_5V_FAIL"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_POWER_SUPPLY_12V_FAIL].alarm_desc = "POWER_SUPPLY_12V_FAIL";
	bcmxcp_alarm_map[BCMXCP_ALARM_HEATSINK_OVER_TEMP].alarm_desc = "HEATSINK_OVER_TEMP"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_HEATSINK_TEMP_SENSOR_FAIL].alarm_desc = "HEATSINK_TEMP_SENSOR_FAIL";
	bcmxcp_alarm_map[BCMXCP_ALARM_RECTIFIER_CURRENT_OVER_125].alarm_desc = "RECTIFIER_CURRENT_OVER_125"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_RECTIFIER_FAULT_INTERRUPT_FAIL].alarm_desc = "RECTIFIER_FAULT_INTERRUPT_FAIL"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_RECTIFIER_POWER_CAPASITOR_FAIL].alarm_desc = "RECTIFIER_POWER_CAPASITOR_FAIL"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_PROGRAM_STACK_ERROR].alarm_desc = "INVERTER_PROGRAM_STACK_ERROR"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_BOARD_SELFTEST_FAIL].alarm_desc = "INVERTER_BOARD_SELFTEST_FAIL"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_AD_SELFTEST_FAIL].alarm_desc = "INVERTER_AD_SELFTEST_FAIL";
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_RAM_SELFTEST_FAIL].alarm_desc = "INVERTER_RAM_SELFTEST_FAIL"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_NV_MEMORY_CHECKSUM_FAIL].alarm_desc = "NV_MEMORY_CHECKSUM_FAIL";
	bcmxcp_alarm_map[BCMXCP_ALARM_PROGRAM_CHECKSUM_FAIL].alarm_desc = "PROGRAM_CHECKSUM_FAIL";
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_CPU_SELFTEST_FAIL].alarm_desc = "INVERTER_CPU_SELFTEST_FAIL"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_NETWORK_NOT_RESPONDING].alarm_desc = "NETWORK_NOT_RESPONDING"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_FRONT_PANEL_SELFTEST_FAIL].alarm_desc = "FRONT_PANEL_SELFTEST_FAIL";
	bcmxcp_alarm_map[BCMXCP_ALARM_NODE_EEPROM_VERIFICATION_ERROR].alarm_desc = "NODE_EEPROM_VERIFICATION_ERROR"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_OUTPUT_AC_OVER_VOLT_TEST_FAIL].alarm_desc = "OUTPUT_AC_OVER_VOLT_TEST_FAIL";
	bcmxcp_alarm_map[BCMXCP_ALARM_OUTPUT_DC_OVER_VOLTAGE].alarm_desc = "OUTPUT_DC_OVER_VOLTAGE"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_INPUT_PHASE_ROTATION_ERROR].alarm_desc = "INPUT_PHASE_ROTATION_ERROR"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_RAMP_UP_TEST_FAILED].alarm_desc = "INVERTER_RAMP_UP_TEST_FAILED"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_OFF_COMMAND].alarm_desc = "INVERTER_OFF_COMMAND"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_ON_COMMAND].alarm_desc = "INVERTER_ON_COMMAND";
	bcmxcp_alarm_map[BCMXCP_ALARM_TO_BYPASS_COMMAND].alarm_desc = "TO_BYPASS_COMMAND";
	bcmxcp_alarm_map[BCMXCP_ALARM_FROM_BYPASS_COMMAND].alarm_desc = "FROM_BYPASS_COMMAND";
	bcmxcp_alarm_map[BCMXCP_ALARM_AUTO_MODE_COMMAND].alarm_desc = "AUTO_MODE_COMMAND";
	bcmxcp_alarm_map[BCMXCP_ALARM_EMERGENCY_SHUTDOWN_COMMAND].alarm_desc = "EMERGENCY_SHUTDOWN_COMMAND"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_SETUP_SWITCH_OPEN].alarm_desc = "SETUP_SWITCH_OPEN";
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_OVER_VOLT_INT].alarm_desc = "INVERTER_OVER_VOLT_INT"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_UNDER_VOLT_INT].alarm_desc = "INVERTER_UNDER_VOLT_INT";
	bcmxcp_alarm_map[BCMXCP_ALARM_ABSOLUTE_DCOV_ACOV].alarm_desc = "ABSOLUTE_DCOV_ACOV"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_PHASE_A_CURRENT_LIMIT].alarm_desc = "PHASE_A_CURRENT_LIMIT";
	bcmxcp_alarm_map[BCMXCP_ALARM_PHASE_B_CURRENT_LIMIT].alarm_desc = "PHASE_B_CURRENT_LIMIT";
	bcmxcp_alarm_map[BCMXCP_ALARM_PHASE_C_CURRENT_LIMIT].alarm_desc = "PHASE_C_CURRENT_LIMIT";
	bcmxcp_alarm_map[BCMXCP_ALARM_BYPASS_NOT_AVAILABLE].alarm_desc = "BYPASS_NOT_AVAILABLE"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_RECTIFIER_BREAKER_OPEN].alarm_desc = "RECTIFIER_BREAKER_OPEN"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_BATTERY_CONTACTOR_OPEN].alarm_desc = "BATTERY_CONTACTOR_OPEN"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_CONTACTOR_OPEN].alarm_desc = "INVERTER_CONTACTOR_OPEN";
	bcmxcp_alarm_map[BCMXCP_ALARM_BYPASS_BREAKER_OPEN].alarm_desc = "BYPASS_BREAKER_OPEN";
	bcmxcp_alarm_map[BCMXCP_ALARM_INV_BOARD_ACOV_INT_TEST_FAIL].alarm_desc = "INV_BOARD_ACOV_INT_TEST_FAIL"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_OVER_TEMP_TRIP].alarm_desc = "INVERTER_OVER_TEMP_TRIP";
	bcmxcp_alarm_map[BCMXCP_ALARM_INV_BOARD_ACUV_INT_TEST_FAIL].alarm_desc = "INV_BOARD_ACUV_INT_TEST_FAIL"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_VOLTAGE_FEEDBACK_ERROR].alarm_desc = "INVERTER_VOLTAGE_FEEDBACK_ERROR";
	bcmxcp_alarm_map[BCMXCP_ALARM_DC_UNDER_VOLTAGE_TIMEOUT].alarm_desc = "DC_UNDER_VOLTAGE_TIMEOUT"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_AC_UNDER_VOLTAGE_TIMEOUT].alarm_desc = "AC_UNDER_VOLTAGE_TIMEOUT"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_DC_UNDER_VOLTAGE_WHILE_CHARGE].alarm_desc = "DC_UNDER_VOLTAGE_WHILE_CHARGE";
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_VOLTAGE_BIAS_ERROR].alarm_desc = "INVERTER_VOLTAGE_BIAS_ERROR";
	bcmxcp_alarm_map[BCMXCP_ALARM_RECTIFIER_PHASE_ROTATION].alarm_desc = "RECTIFIER_PHASE_ROTATION"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_BYPASS_PHASER_ROTATION].alarm_desc = "BYPASS_PHASER_ROTATION"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_SYSTEM_INTERFACE_BOARD_FAIL].alarm_desc = "SYSTEM_INTERFACE_BOARD_FAIL";
	bcmxcp_alarm_map[BCMXCP_ALARM_PARALLEL_BOARD_FAIL].alarm_desc = "PARALLEL_BOARD_FAIL";
	bcmxcp_alarm_map[BCMXCP_ALARM_LOST_LOAD_SHARING_PHASE_A].alarm_desc = "LOST_LOAD_SHARING_PHASE_A";
	bcmxcp_alarm_map[BCMXCP_ALARM_LOST_LOAD_SHARING_PHASE_B].alarm_desc = "LOST_LOAD_SHARING_PHASE_B";
	bcmxcp_alarm_map[BCMXCP_ALARM_LOST_LOAD_SHARING_PHASE_C].alarm_desc = "LOST_LOAD_SHARING_PHASE_C";
	bcmxcp_alarm_map[BCMXCP_ALARM_DC_OVER_VOLTAGE_TIMEOUT].alarm_desc = "DC_OVER_VOLTAGE_TIMEOUT";
	bcmxcp_alarm_map[BCMXCP_ALARM_BATTERY_TOTALLY_DISCHARGED].alarm_desc = "BATTERY_TOTALLY_DISCHARGED"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_PHASE_BIAS_ERROR].alarm_desc = "INVERTER_PHASE_BIAS_ERROR";
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_VOLTAGE_BIAS_ERROR_2].alarm_desc = "INVERTER_VOLTAGE_BIAS_ERROR_2";
	bcmxcp_alarm_map[BCMXCP_ALARM_DC_LINK_BLEED_COMPLETE].alarm_desc = "DC_LINK_BLEED_COMPLETE"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_LARGE_CHARGER_INPUT_CURRENT].alarm_desc = "LARGE_CHARGER_INPUT_CURRENT";
	bcmxcp_alarm_map[BCMXCP_ALARM_INV_VOLT_TOO_LOW_FOR_RAMP_LEVEL].alarm_desc = "INV_VOLT_TOO_LOW_FOR_RAMP_LEVEL";
	bcmxcp_alarm_map[BCMXCP_ALARM_LOSS_OF_REDUNDANCY].alarm_desc = "LOSS_OF_REDUNDANCY"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_LOSS_OF_SYNC_BUS].alarm_desc = "LOSS_OF_SYNC_BUS"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_RECTIFIER_BREAKER_SHUNT_TRIP].alarm_desc = "RECTIFIER_BREAKER_SHUNT_TRIP"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_LOSS_OF_CHARGER_SYNC].alarm_desc = "LOSS_OF_CHARGER_SYNC"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_LOW_LEVEL_TEST_TIMEOUT].alarm_desc = "INVERTER_LOW_LEVEL_TEST_TIMEOUT";
	bcmxcp_alarm_map[BCMXCP_ALARM_OUTPUT_BREAKER_OPEN].alarm_desc = "OUTPUT_BREAKER_OPEN";
	bcmxcp_alarm_map[BCMXCP_ALARM_CONTROL_POWER_ON].alarm_desc = "CONTROL_POWER_ON"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_ON].alarm_desc = "INVERTER_ON";
	bcmxcp_alarm_map[BCMXCP_ALARM_CHARGER_ON].alarm_desc = "CHARGER_ON"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_BYPASS_ON].alarm_desc = "BYPASS_ON";
	bcmxcp_alarm_map[BCMXCP_ALARM_BYPASS_POWER_LOSS].alarm_desc = "BYPASS_POWER_LOSS";
	bcmxcp_alarm_map[BCMXCP_ALARM_ON_MANUAL_BYPASS].alarm_desc = "ON_MANUAL_BYPASS"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_BYPASS_MANUAL_TURN_OFF].alarm_desc = "BYPASS_MANUAL_TURN_OFF"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_BLEEDING_DC_LINK_VOLT].alarm_desc = "INVERTER_BLEEDING_DC_LINK_VOLT"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_CPU_ISR_ERROR].alarm_desc = "CPU_ISR_ERROR";
	bcmxcp_alarm_map[BCMXCP_ALARM_SYSTEM_ISR_RESTART].alarm_desc = "SYSTEM_ISR_RESTART"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_PARALLEL_DC].alarm_desc = "PARALLEL_DC";
	bcmxcp_alarm_map[BCMXCP_ALARM_BATTERY_NEEDS_SERVICE].alarm_desc = "BATTERY_NEEDS_SERVICE";
	bcmxcp_alarm_map[BCMXCP_ALARM_BATTERY_CHARGING].alarm_desc = "BATTERY_CHARGING"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_BATTERY_NOT_CHARGED].alarm_desc = "BATTERY_NOT_CHARGED";
	bcmxcp_alarm_map[BCMXCP_ALARM_DISABLED_BATTERY_TIME].alarm_desc = "DISABLED_BATTERY_TIME";
	bcmxcp_alarm_map[BCMXCP_ALARM_SERIES_7000_ENABLE].alarm_desc = "SERIES_7000_ENABLE"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_OTHER_UPS_ON].alarm_desc = "OTHER_UPS_ON"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_PARALLEL_INVERTER].alarm_desc = "PARALLEL_INVERTER";
	bcmxcp_alarm_map[BCMXCP_ALARM_UPS_IN_PARALLEL].alarm_desc = "UPS_IN_PARALLEL";
	bcmxcp_alarm_map[BCMXCP_ALARM_OUTPUT_BREAKER_REALY_FAIL].alarm_desc = "OUTPUT_BREAKER_REALY_FAIL";
	bcmxcp_alarm_map[BCMXCP_ALARM_CONTROL_POWER_OFF].alarm_desc = "CONTROL_POWER_OFF";
	bcmxcp_alarm_map[BCMXCP_ALARM_LEVEL_2_OVERLOAD_PHASE_A].alarm_desc = "LEVEL_2_OVERLOAD_PHASE_A"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_LEVEL_2_OVERLOAD_PHASE_B].alarm_desc = "LEVEL_2_OVERLOAD_PHASE_B"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_LEVEL_2_OVERLOAD_PHASE_C].alarm_desc = "LEVEL_2_OVERLOAD_PHASE_C"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_LEVEL_3_OVERLOAD_PHASE_A].alarm_desc = "LEVEL_3_OVERLOAD_PHASE_A"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_LEVEL_3_OVERLOAD_PHASE_B].alarm_desc = "LEVEL_3_OVERLOAD_PHASE_B"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_LEVEL_3_OVERLOAD_PHASE_C].alarm_desc = "LEVEL_3_OVERLOAD_PHASE_C"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_LEVEL_4_OVERLOAD_PHASE_A].alarm_desc = "LEVEL_4_OVERLOAD_PHASE_A"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_LEVEL_4_OVERLOAD_PHASE_B].alarm_desc = "LEVEL_4_OVERLOAD_PHASE_B"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_LEVEL_4_OVERLOAD_PHASE_C].alarm_desc = "LEVEL_4_OVERLOAD_PHASE_C"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_UPS_ON_BATTERY].alarm_desc = "UPS_ON_BATTERY"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_UPS_ON_BYPASS].alarm_desc = "UPS_ON_BYPASS";
	bcmxcp_alarm_map[BCMXCP_ALARM_LOAD_DUMPED].alarm_desc = "LOAD_DUMPED";
	bcmxcp_alarm_map[BCMXCP_ALARM_LOAD_ON_INVERTER].alarm_desc = "LOAD_ON_INVERTER"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_UPS_ON_COMMAND].alarm_desc = "UPS_ON_COMMAND"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_UPS_OFF_COMMAND].alarm_desc = "UPS_OFF_COMMAND";
	bcmxcp_alarm_map[BCMXCP_ALARM_LOW_BATTERY_SHUTDOWN].alarm_desc = "LOW_BATTERY_SHUTDOWN"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_AUTO_ON_ENABLED].alarm_desc = "AUTO_ON_ENABLED";
	bcmxcp_alarm_map[BCMXCP_ALARM_SOFTWARE_INCOMPABILITY_DETECTED].alarm_desc = "SOFTWARE_INCOMPABILITY_DETECTED";
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_TEMP_SENSOR_FAILED].alarm_desc = "INVERTER_TEMP_SENSOR_FAILED";
	bcmxcp_alarm_map[BCMXCP_ALARM_DC_START_OCCURED].alarm_desc = "DC_START_OCCURED"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_IN_PARALLEL_OPERATION].alarm_desc = "IN_PARALLEL_OPERATION";
	bcmxcp_alarm_map[BCMXCP_ALARM_SYNCING_TO_BYPASS].alarm_desc = "SYNCING_TO_BYPASS";
	bcmxcp_alarm_map[BCMXCP_ALARM_RAMPING_UPS_UP].alarm_desc = "RAMPING_UPS_UP"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_ON_DELAY].alarm_desc = "INVERTER_ON_DELAY";
	bcmxcp_alarm_map[BCMXCP_ALARM_CHARGER_ON_DELAY].alarm_desc = "CHARGER_ON_DELAY"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_WAITING_FOR_UTIL_INPUT].alarm_desc = "WAITING_FOR_UTIL_INPUT"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_CLOSE_BYPASS_BREAKER].alarm_desc = "CLOSE_BYPASS_BREAKER"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_TEMPORARY_BYPASS_OPERATION].alarm_desc = "TEMPORARY_BYPASS_OPERATION"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_SYNCING_TO_OUTPUT].alarm_desc = "SYNCING_TO_OUTPUT";
	bcmxcp_alarm_map[BCMXCP_ALARM_BYPASS_FAILURE].alarm_desc = "BYPASS_FAILURE"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_AUTO_OFF_COMMAND_EXECUTED].alarm_desc = "AUTO_OFF_COMMAND_EXECUTED";
	bcmxcp_alarm_map[BCMXCP_ALARM_AUTO_ON_COMMAND_EXECUTED].alarm_desc = "AUTO_ON_COMMAND_EXECUTED"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_BATTERY_TEST_FAILED].alarm_desc = "BATTERY_TEST_FAILED";
	bcmxcp_alarm_map[BCMXCP_ALARM_FUSE_FAIL].alarm_desc = "FUSE_FAIL";
	bcmxcp_alarm_map[BCMXCP_ALARM_FAN_FAIL].alarm_desc = "FAN_FAIL"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_SITE_WIRING_FAULT].alarm_desc = "SITE_WIRING_FAULT";
	bcmxcp_alarm_map[BCMXCP_ALARM_BACKFEED_CONTACTOR_FAIL].alarm_desc = "BACKFEED_CONTACTOR_FAIL";
	bcmxcp_alarm_map[BCMXCP_ALARM_ON_BUCK].alarm_desc = "ON_BUCK";
	bcmxcp_alarm_map[BCMXCP_ALARM_ON_BOOST].alarm_desc = "ON_BOOST"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_ON_DOUBLE_BOOST].alarm_desc = "ON_DOUBLE_BOOST";
	bcmxcp_alarm_map[BCMXCP_ALARM_BATTERIES_DISCONNECTED].alarm_desc = "BATTERIES_DISCONNECTED"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_UPS_CABINET_OVER_TEMP].alarm_desc = "UPS_CABINET_OVER_TEMP";
	bcmxcp_alarm_map[BCMXCP_ALARM_TRANSFORMER_OVER_TEMP].alarm_desc = "TRANSFORMER_OVER_TEMP";
	bcmxcp_alarm_map[BCMXCP_ALARM_AMBIENT_UNDER_TEMP].alarm_desc = "AMBIENT_UNDER_TEMP"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_AMBIENT_OVER_TEMP].alarm_desc = "AMBIENT_OVER_TEMP";
	bcmxcp_alarm_map[BCMXCP_ALARM_CABINET_DOOR_OPEN].alarm_desc = "CABINET_DOOR_OPEN";
	bcmxcp_alarm_map[BCMXCP_ALARM_CABINET_DOOR_OPEN_VOLT_PRESENT].alarm_desc = "CABINET_DOOR_OPEN_VOLT_PRESENT"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_AUTO_SHUTDOWN_PENDING].alarm_desc = "AUTO_SHUTDOWN_PENDING";
	bcmxcp_alarm_map[BCMXCP_ALARM_TAP_SWITCHING_REALY_PENDING].alarm_desc = "TAP_SWITCHING_REALY_PENDING";
	bcmxcp_alarm_map[BCMXCP_ALARM_UNABLE_TO_CHARGE_BATTERIES].alarm_desc = "UNABLE_TO_CHARGE_BATTERIES"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_STARTUP_FAILURE_CHECK_EPO].alarm_desc = "STARTUP_FAILURE_CHECK_EPO";
	bcmxcp_alarm_map[BCMXCP_ALARM_AUTOMATIC_STARTUP_PENDING].alarm_desc = "AUTOMATIC_STARTUP_PENDING";
	bcmxcp_alarm_map[BCMXCP_ALARM_MODEM_FAILED].alarm_desc = "MODEM_FAILED"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_INCOMING_MODEM_CALL_STARTED].alarm_desc = "INCOMING_MODEM_CALL_STARTED";
	bcmxcp_alarm_map[BCMXCP_ALARM_OUTGOING_MODEM_CALL_STARTED].alarm_desc = "OUTGOING_MODEM_CALL_STARTED";
	bcmxcp_alarm_map[BCMXCP_ALARM_MODEM_CONNECTION_ESTABLISHED].alarm_desc = "MODEM_CONNECTION_ESTABLISHED"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_MODEM_CALL_COMPLETED_SUCCESS].alarm_desc = "MODEM_CALL_COMPLETED_SUCCESS"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_MODEM_CALL_COMPLETED_FAIL].alarm_desc = "MODEM_CALL_COMPLETED_FAIL";
	bcmxcp_alarm_map[BCMXCP_ALARM_INPUT_BREAKER_FAIL].alarm_desc = "INPUT_BREAKER_FAIL"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_SYSINIT_IN_PROGRESS].alarm_desc = "SYSINIT_IN_PROGRESS";
	bcmxcp_alarm_map[BCMXCP_ALARM_AUTOCALIBRATION_FAIL].alarm_desc = "AUTOCALIBRATION_FAIL"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_SELECTIVE_TRIP_OF_MODULE].alarm_desc = "SELECTIVE_TRIP_OF_MODULE"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_INVERTER_OUTPUT_FAILURE].alarm_desc = "INVERTER_OUTPUT_FAILURE";
	bcmxcp_alarm_map[BCMXCP_ALARM_ABNORMAL_OUTPUT_VOLT_AT_STARTUP].alarm_desc = "ABNORMAL_OUTPUT_VOLT_AT_STARTUP";
	bcmxcp_alarm_map[BCMXCP_ALARM_RECTIFIER_OVER_TEMP].alarm_desc = "RECTIFIER_OVER_TEMP";
	bcmxcp_alarm_map[BCMXCP_ALARM_CONFIG_ERROR].alarm_desc = "CONFIG_ERROR"; 
	bcmxcp_alarm_map[BCMXCP_ALARM_REDUNDANCY_LOSS_DUE_TO_OVERLOAD].alarm_desc = "REDUNDANCY_LOSS_DUE_TO_OVERLOAD";
	bcmxcp_alarm_map[BCMXCP_ALARM_ON_ALTERNATE_AC_SOURCE].alarm_desc = "ON_ALTERNATE_AC_SOURCE"; 
}

void init_ups_meter_map(const unsigned char *map, unsigned char len)
{
	unsigned int iIndex, iOffset = 0;
	
	/* In case of debug - make explanation of values */
	upsdebugx(2, "Index\tOffset\tFormat\tNUT\n");
	
	/* Loop thru map */
	for (iIndex = 0; iIndex < len && iIndex < BCMXCP_METER_MAP_MAX; iIndex++)
	{
		bcmxcp_meter_map[iIndex].format = map[iIndex];
		if (map[iIndex] != 0)
		{
			/* Set meter map entry offset */
			bcmxcp_meter_map[iIndex].meter_block_index = iOffset;

			/* Debug info */
			upsdebugx(2, "%04d\t%04d\t%2x\t%s", iIndex, iOffset, bcmxcp_meter_map[iIndex].format,
					(bcmxcp_meter_map[iIndex].nut_entity == NULL ? "None" :bcmxcp_meter_map[iIndex].nut_entity));

			iOffset += 4;
		}
	}

	upsdebugx(2, "\n");
}

void decode_meter_map_entry(const unsigned char *entry, const unsigned char format, char* value)
{
	long lValue = 0;
	char sFormat[32];
	float fValue;

	/* Paranoid input sanity checks */
	if (value == NULL)
		return;
	*value = '\0';
	if (entry == (unsigned char *)NULL || format == 0x00)
		return;
	
	/* Get data based on format */
	if (format == 0xf0) {
		/* Long integer */
		lValue = get_long(entry);
		snprintf(value, 127, "%d", (int)lValue);
	}
	else if ((format & 0xf0) == 0xf0) {
		/* Fixed point integer */
		fValue = get_long(entry) / ldexp(1, format & 0x0f);
		snprintf(value, 127, "%.2f", fValue);
	}
	else if (format <= 0x97) {
		/* Floating point */
		fValue = get_float(entry);
		snprintf(sFormat, 31, "%%%d.%df", ((format & 0xf0) >> 4), (format & 0x0f));
		snprintf(value, 127, sFormat, fValue);
	}
	else if (format == 0xe2) {
		/* Seconds */
		lValue = get_long(entry);
		snprintf(value, 127, "%d", (int)lValue);
	}
	else if (format == 0xe0) {
		/* Date */
		unsigned char
		dd = entry[0],
		mm = entry[1],
		yy = entry[2],
		cc = entry[3];

		/* Check format type */
		if (cc & 0x80) {
			/* Julian format */
			snprintf(value, 127, "%2d%2d:%3d", (cc & 0x7f), yy, (mm * 0x100)+dd);
		}
		else {
		/* Month:Day format	*/
		snprintf(value, 127, "%2d/%2d/%2d%2d", dd, mm, (cc & 0x7f), yy);
		}
	}
	else if (format == 0xe1) {
		/* Time */
		unsigned char
		cc = entry[0],
		ss = entry[1],
		mm = entry[2],
		hh = entry[3];

		snprintf(value, 127, "%2d:%2d:%2d.%2d", hh, mm, ss, cc);
	}
	else {
		/* Unknown format */
		snprintf(value, 127, "???");
		return;
	}
	return;
}


void init_ups_alarm_map(const unsigned char *map, unsigned char len)
{
	unsigned int iIndex = 0, iOffset = 0, alarm = 0;
	
	/* In case of debug - make explanation of values */
	upsdebugx(2, "Index\tAlarm\tSupported\n");

	/* Loop thru map */
	for (iIndex = 0; iIndex < len && iIndex < BCMXCP_ALARM_MAP_MAX / 8; iIndex++)
	{ 
		/* Bit 0 */
		iOffset = iIndex * 8;
		if (map[iIndex] & 0x01)
		{
			/* Set alarm active */
			bcmxcp_alarm_map[iOffset].alarm_block_index = alarm;

			/* Debug info */
			upsdebugx(2, "%04d\t%s\tYes", alarm, bcmxcp_alarm_map[iOffset].alarm_desc);
			alarm++;
		}
		else
		{
			/* Set alarm inactive */
			bcmxcp_alarm_map[iOffset].alarm_block_index = -1;

			/* Debug info */
			upsdebugx(2, "%04d\t%s\tNo", -1, bcmxcp_alarm_map[iOffset].alarm_desc);
		}

		/* Bit 1 */
		iOffset = iIndex*8 + 1;
		if (map[iIndex] & 0x02)
		{
			/* Set alarm active */
			bcmxcp_alarm_map[iOffset].alarm_block_index = alarm;

			/* Debug info */
			upsdebugx(2, "%04d\t%s\tYes", alarm, bcmxcp_alarm_map[iOffset].alarm_desc);
			alarm++;
		}
		else
		{
			/* Set alarm inactive */
			bcmxcp_alarm_map[iOffset].alarm_block_index = -1;

			/* Debug info */
			upsdebugx(2, "%04d\t%s\tNo", -1, bcmxcp_alarm_map[iOffset].alarm_desc);
		}

		/* Bit 2 */
		iOffset = iIndex*8 + 2;
		if (map[iIndex] & 0x04)
		{
			/* Set alarm active */
			bcmxcp_alarm_map[iOffset].alarm_block_index = alarm;

			/* Debug info */
			upsdebugx(2, "%04d\t%s\tYes", alarm, bcmxcp_alarm_map[iOffset].alarm_desc);
			alarm++;
		}
		else
		{
			/* Set alarm inactive */
			bcmxcp_alarm_map[iOffset].alarm_block_index = -1;

			/* Debug info */
			upsdebugx(2, "%04d\t%s\tNo", -1, bcmxcp_alarm_map[iOffset].alarm_desc);
		}

		/* Bit 3 */
		iOffset = iIndex*8 + 3;
		if (map[iIndex] & 0x08)
		{
			/* Set alarm active */
			bcmxcp_alarm_map[iOffset].alarm_block_index = alarm;

			/* Debug info */
			upsdebugx(2, "%04d\t%s\tYes", alarm, bcmxcp_alarm_map[iOffset].alarm_desc);
			alarm++;
		}
		else
		{
			/* Set alarm inactive */
			bcmxcp_alarm_map[iOffset].alarm_block_index = -1;

			/* Debug info */
			upsdebugx(2, "%04d\t%s\tNo", -1, bcmxcp_alarm_map[iOffset].alarm_desc);
		}

		/* Bit 4 */
		iOffset = iIndex*8 + 4;
		if (map[iIndex] & 0x10)
		{
			/* Set alarm active */
			bcmxcp_alarm_map[iOffset].alarm_block_index = alarm;

			/* Debug info */
			upsdebugx(2, "%04d\t%s\tYes", alarm, bcmxcp_alarm_map[iOffset].alarm_desc);
			alarm++;
		}
		else
		{
			/* Set alarm inactive */
			bcmxcp_alarm_map[iOffset].alarm_block_index = -1;

			/* Debug info */
			upsdebugx(2, "%04d\t%s\tNo", -1, bcmxcp_alarm_map[iOffset].alarm_desc);
		}

		/* Bit 5 */
		iOffset = iIndex*8 + 5;
		if (map[iIndex] & 0x20)
		{
			/* Set alarm active */
			bcmxcp_alarm_map[iOffset].alarm_block_index = alarm;

			/* Debug info */
			upsdebugx(2, "%04d\t%s\tYes", alarm, bcmxcp_alarm_map[iOffset].alarm_desc);
			alarm++;
		}
		else
		{
			/* Set alarm inactive */
			bcmxcp_alarm_map[iOffset].alarm_block_index = -1;

			/* Debug info */
			upsdebugx(2, "%04d\t%s\tNo", -1, bcmxcp_alarm_map[iOffset].alarm_desc);
		}

		/* Bit 6 */
		iOffset = iIndex*8 + 6;
		if (map[iIndex] & 0x40)
		{
			/* Set alarm active */
			bcmxcp_alarm_map[iOffset].alarm_block_index = alarm;

			/* Debug info */
			upsdebugx(2, "%04d\t%s\tYes", alarm, bcmxcp_alarm_map[iOffset].alarm_desc);
			alarm++;
		}
		else
		{
			/* Set alarm inactive */
			bcmxcp_alarm_map[iOffset].alarm_block_index = -1;

			/* Debug info */
			upsdebugx(2, "%04d\t%s\tNo", -1, bcmxcp_alarm_map[iOffset].alarm_desc);
		}

		/* Bit 7 */
		iOffset = iIndex*8 + 7;
		if (map[iIndex] & 0x80)
		{
			/* Set alarm active */
			bcmxcp_alarm_map[iOffset].alarm_block_index = alarm;

			/* Debug info */
			upsdebugx(2, "%04d\t%s\tYes", alarm, bcmxcp_alarm_map[iOffset].alarm_desc);
			alarm++;
		}
		else
		{
			/* Set alarm inactive */
			bcmxcp_alarm_map[iOffset].alarm_block_index = -1;

			/* Debug info */
			upsdebugx(2, "%04d\t%s\tNo", -1, bcmxcp_alarm_map[iOffset].alarm_desc);
		}
	}
	upsdebugx(2, "\n");
}


void send_read_command(unsigned char command)
{
	int retry, sent;
	unsigned char buf[4];
	retry = 0;
	sent = 0;
	
	while ((sent != 4) && (retry < PW_MAX_TRY)) {
		buf[0]=PW_COMMAND_START_BYTE;
		buf[1]=0x01;			/* data length */
		buf[2]=command;			/* command to send */
		buf[3]=calc_checksum(buf);	/* checksum */
		
		if (retry == 4) ser_send_char(upsfd, 0x1d);	/* last retry is preceded by a ESC.*/
			sent = ser_send_buf(upsfd, (char*)buf, 4);
		retry += 1;
	}
}


void send_write_command(unsigned char *command, int command_length)
{
	int  retry, sent;
	unsigned char sbuf[128];

	/* Prepare the send buffer */
	sbuf[0] = PW_COMMAND_START_BYTE;
	sbuf[1] = (unsigned char)(command_length);
	memcpy(sbuf+2, command, command_length);
	command_length += 2;

	/* Add checksum */
	sbuf[command_length] = calc_checksum(sbuf);
	command_length += 1;

	/* Try to send the command */
	retry = 0;
	sent = 0;

	while ((sent != (command_length)) && (retry < PW_MAX_TRY)) {
		sent = ser_send_buf(upsfd, (char*)sbuf, (command_length));
		if (sent != (command_length)) printf("Error sending command %x\n", (unsigned char)sbuf[2]);
			retry += 1;
	}
}

/* get the answer of a command from the ups. And check that the answer is for this command */
int get_answer(unsigned char *data, unsigned char command)
{
	unsigned char my_buf[128];	/* packet has a maximum length of 121+5 bytes */
	int length, end_length, res, endblock, start;
	unsigned char block_number, sequence, pre_sequence;

	end_length = 0;
	endblock = 0;
	pre_sequence = 0;
	start = 0;

	while (endblock != 1){

		do {
			/* Read PW_COMMAND_START_BYTE byte */
			res = ser_get_char(upsfd, (char*)my_buf, 1, 0);
			if (res != 1) {
				ser_comm_fail("Receive error (PW_COMMAND_START_BYTE): %d!!!\n", res);
				return -1;
			}
			start++;

		} while ((my_buf[0] != PW_COMMAND_START_BYTE) || start == 128);
		
		if (start == 128) {
			ser_comm_fail("Receive error (PW_COMMAND_START_BYTE): packet not on start!!%x\n", my_buf[0]);
			return -1;
		}

		/* Read block number byte */
		res = ser_get_char(upsfd, (char*)(my_buf+1), 1, 0);
		if (res != 1) {
			ser_comm_fail("Receive error (Block number): %d!!!\n", res);
			return -1;
		}
		block_number = (unsigned char)my_buf[1];

		if (command <= 0x43) {
			if ((command - 0x30) != block_number){
				ser_comm_fail("Receive error (Request command): %x!!!\n", block_number);
				return -1;
			}
		}

		if (command >= 0x89) {
			if ((command == 0xA0) && (block_number != 0x01)){
				ser_comm_fail("Receive error (Requested only mode command): %x!!!\n", block_number);
				return -1;
			}
			else if ((command != 0xA0) && (block_number != 0x09)){
				ser_comm_fail("Receive error (Control command): %x!!!\n", block_number);
				return -1;
			}
		}

		/* Read data length byte */
		res = ser_get_char(upsfd, (char*)(my_buf+2), 1, 0);
		if (res != 1) {
			ser_comm_fail("Receive error (length): %d!!!\n", res);
			return -1;
		}
		length = (unsigned char)my_buf[2];

		if (length < 1) {
			ser_comm_fail("Receive error (length): packet length %x!!!\n", length);
			return -1;
		}

		/* Read sequence byte */
		res = ser_get_char(upsfd, (char*)(my_buf+3), 1, 0);
		if (res != 1) {
			ser_comm_fail("Receive error (sequence): %d!!!\n", res);
			return -1;
		}

		sequence = (unsigned char)my_buf[3];
		if ((sequence & 0x80) == 0x80) {
			endblock = 1;
		}

		if ((sequence & 0x07) != (pre_sequence + 1)) {
			ser_comm_fail("Not the right sequence received %x!!!\n", sequence);
			return -1;
		}

		pre_sequence = sequence;

		/* Try to read all the remainig bytes */
		res = ser_get_buf_len(upsfd, (char*)(my_buf+4), length, 1, 0);
		if (res != length) {
			ser_comm_fail("Receive error (data): got %d bytes instead of %d!!!\n", res, length);
			return -1;
		}

		/* Get the checksum byte */
		res = ser_get_char(upsfd, (char*)(my_buf+(4+length)), 1, 0);

		if (res != 1) {
			ser_comm_fail("Receive error (checksum): %x!!!\n", res);
			return -1;
		}

		/* now we have the whole answer from the ups, we can checksum it */
		if (!checksum_test(my_buf)) {
			ser_comm_fail("checksum error! ");
		return -1;
		}

		memcpy(data+end_length, my_buf+4, length);
		end_length += length;

	}
	return end_length;
}

/* Sends a single command (length=1). and get the answer */
int command_read_sequence(unsigned char command, unsigned char *data)
{
	int bytes_read = 0;
	int retry = 0;
	
	while ((bytes_read < 1) && (retry < 5)) {
		send_read_command(command);
		bytes_read = get_answer(data, command);
		
		if (retry > 2) 
			ser_flush_in(upsfd, "", 0);
		retry++;
	}

	if (bytes_read < 1) {
		ser_comm_fail("Error executing command");
		dstate_datastale();
		return -1;
	}

	ser_comm_good();

	return bytes_read;
}

/* Sends a setup command (length > 1) */
int command_write_sequence(unsigned char *command, int command_length, unsigned	char *answer)
{
	int bytes_read = 0;
	int retry = 0;
	
	while ((bytes_read < 1) && (retry < 5)) {
		send_write_command(command, command_length);
		bytes_read = get_answer(answer, command[0]);
		
		if (retry > 2) 
			ser_flush_in(upsfd, "", 0);
		retry ++;
	}

	if (bytes_read < 1) {
		ser_comm_fail("Error executing command");
		dstate_datastale();
		return -1;
	}

	ser_comm_good();

	return bytes_read;
}


void upsdrv_initinfo(void)
{
	unsigned char answer[256];
	char *pTmp, sValue[17];
	int iRating = 0, iIndex = 0, res, len;
	int voltage = 0;

	/* Set driver version info */
	dstate_setinfo("driver.version.internal", "%s", DRV_VERSION);

	/* Init BCM/XCP <-> NUT meter map */
	init_meter_map();

	/* Init BCM/XCP alarm descriptions */
	init_alarm_map();

	/* Get vars from ups.conf */
	if (getval("shutdown_delay") != NULL)
		bcmxcp_status.shutdowndelay = atoi(getval("shutdown_delay"));
	else
		bcmxcp_status.shutdowndelay = 120;

	/* Get information on UPS from UPS */
	res = command_read_sequence(PW_ID_BLOCK_REQ, answer);
	if (res <= 0)
		fatal("Could not communicate with the ups");

	/* Get number of CPU's in ID block */
	len = answer[iIndex++];

	/* If there is one or more CPU number, get it */
	if (len > 0) {
		do {
			/* Get the ups firmware. The major number is in the last byte, the minor is in the first */
			dstate_setinfo("ups.firmware", "%02x.%02x", (unsigned char)answer[iIndex+1],
				(unsigned char)answer[iIndex]);
			iIndex += 2;
			len--;
		} while ((strcmp("00.00", dstate_getinfo("ups.firmware")) == 0) && len > 0);

		/* Increment index to point at end of CPU bytes. */
		iIndex += len * 2;
	}

	/* Get rating in kVA, if present */
	if ((iRating = answer[iIndex++]) > 0)
		iRating *= 1000;
	else
	{
		/* The rating is given as 2 byte VA */
		iRating = get_word(answer+iIndex) * 50;
		iIndex += 2;
	}
	dstate_setinfo("ups.power.nominal", "%d", iRating);

	/* Skip	UPS' number of phases and phase angle, as NUT do not care */
	iIndex += 2;
	
	/* Get length of UPS description */
	len = answer[iIndex++];

	/* Make a temporary buffer and store model description and VA rating in it */
	pTmp = xmalloc(len+10);
	snprintf(pTmp, len + 1, "%s", answer + iIndex);
	pTmp[len+1] = 0;
	iIndex += len;

	/* Make a nice model string and tell NUT about it */
	snprintfcat(pTmp, len+10, " %dVA", iRating);
	dstate_setinfo("ups.model", pTmp);
	free(pTmp);

	/* Display startup banner */
	printf("Modell = %s\n", dstate_getinfo("ups.model"));
	printf("Firmware = %s\n", dstate_getinfo("ups.firmware"));
	upslogx(LOG_INFO,"Shutdown delay =  %d seconds", bcmxcp_status.shutdowndelay);
	
	/* Get meter map info from ups, and init our map */
	len = answer[iIndex++];
	init_ups_meter_map(answer+iIndex, len);
	iIndex += len;

	/* Next is alarm map */
	len = answer[iIndex++];
	upsdebugx(2, "Length of alarm map: %d\n", len); 
	init_ups_alarm_map(answer+iIndex, len);
	iIndex += len;

	/* Get information on UPS configuration */
	res = command_read_sequence(PW_CONFIG_BLOC_REQ, answer);
	if (res <= 0)
		fatal("Could not communicate with the ups");

	/* Get validation mask for status bitmap */
	bcmxcp_status.topology_mask = answer[BCMXCP_CONFIG_BLOCK_HW_MODULES_INSTALLED_BYTE4];

	/* Nominal input voltage of ups */
	voltage = get_word((answer + BCMXCP_CONFIG_BLOCK_NOMINAL_OUTPUT_VOLTAGE));
	
	if (voltage != 0)
		dstate_setinfo("ups.voltage.nominal", "%d", voltage);

	/* UPS serial number */
	sValue[16] = 0;
	
	snprintf(sValue, 16, "%s", answer + BCMXCP_CONFIG_BLOCK_SERIAL_NUMBER);
	len = 0;

	for (len = 0; len < 16; len++) {
		if (sValue[len] == 0x20) {
			sValue[len] = 0;
			break;
		}
	}
	
	dstate_setinfo("ups.serial", sValue);
	
	/* Get information on UPS extended limits */
	res = command_read_sequence(PW_LIMIT_BLOCK_REQ, answer);
	if (res <= 0)
		fatal("Could not communicate with the ups");

	/* Low battery warning */
	bcmxcp_status.lowbatt = answer[BCMXCP_EXT_LIMITS_BLOCK_LOW_BATT_WARNING] * 60;

	/* Check if we should warn the user that her shutdown delay is to long? */
	if (bcmxcp_status.shutdowndelay > bcmxcp_status.lowbatt)
		upslogx(LOG_WARNING, "Shutdown delay longer than battery capacity when Low Battery warning is given. (max %d seconds)", bcmxcp_status.lowbatt);
	
	/* Add instant commands */
	dstate_addcmd("shutdown.return");
	dstate_addcmd("shutdown.stayoff");
	dstate_addcmd("test.battery.start");
	upsh.instcmd = instcmd;
	return;
}

void upsdrv_updateinfo(void)
{
	unsigned char answer[256]; 
	char sValue[128];
	int iIndex, res; 
	float output, max_output, fValue = 0.0f;

	/* Get info from UPS */
	res = command_read_sequence(PW_METER_BLOCK_REQ, answer);
	
	if (res <= 0){
		upslogx(LOG_ERR, "Short read from UPS");
		dstate_datastale();
		return;
	}

	/* Loop thru meter map, get all data UPS is willing to offer */
	for (iIndex = 0; iIndex < BCMXCP_METER_MAP_MAX; iIndex++){
		if (bcmxcp_meter_map[iIndex].format != 0 && bcmxcp_meter_map[iIndex].nut_entity != NULL) {
			decode_meter_map_entry(answer + bcmxcp_meter_map[iIndex].meter_block_index,
						 bcmxcp_meter_map[iIndex].format, sValue);

			/* Set result */
			dstate_setinfo(bcmxcp_meter_map[iIndex].nut_entity, sValue);
		}
	}

	/* Set max load, if possible (must be calculated) */
	if (bcmxcp_meter_map[BCMXCP_METER_MAP_OUTPUT_VA].format != 0 && 			/* Output VA */
			bcmxcp_meter_map[BCMXCP_METER_MAP_OUTPUT_VA_BAR_CHART].format != 0)	/* Max output VA */
	{
		decode_meter_map_entry(answer + bcmxcp_meter_map[BCMXCP_METER_MAP_OUTPUT_VA].meter_block_index,
					 bcmxcp_meter_map[BCMXCP_METER_MAP_OUTPUT_VA].format, sValue);
		output = atof(sValue);
		decode_meter_map_entry(answer + bcmxcp_meter_map[BCMXCP_METER_MAP_OUTPUT_VA_BAR_CHART].meter_block_index,
					 bcmxcp_meter_map[BCMXCP_METER_MAP_OUTPUT_VA_BAR_CHART].format, sValue);
		max_output = atof(sValue);
		
		fValue = 0.0;
		if (max_output > 0.0)
			fValue = 100 * (output / max_output);
		dstate_setinfo("ups.load", "%5.1f", fValue);
	}
	else if (bcmxcp_meter_map[BCMXCP_METER_MAP_LOAD_CURR_PHASE_A].format != 0 && /* Output A */
					 bcmxcp_meter_map[BCMXCP_METER_MAP_LOAD_CURR_PHASE_A_BAR_CHART].format != 0)	/* Max output A */
	{
		decode_meter_map_entry(answer + bcmxcp_meter_map[BCMXCP_METER_MAP_LOAD_CURR_PHASE_A].meter_block_index,
					 bcmxcp_meter_map[BCMXCP_METER_MAP_LOAD_CURR_PHASE_A].format, sValue);
		output = atof(sValue);
		decode_meter_map_entry(answer + bcmxcp_meter_map[BCMXCP_METER_MAP_LOAD_CURR_PHASE_A_BAR_CHART].meter_block_index,
					 bcmxcp_meter_map[BCMXCP_METER_MAP_LOAD_CURR_PHASE_A_BAR_CHART].format, sValue);
		max_output = atof(sValue);
		
		fValue = 0.0;
		if (max_output > 0.0)
			fValue = 100 * (output / max_output);
		dstate_setinfo("ups.load", "%5.1f", fValue);
	}

	/* Get alarm info from UPS */
	res = command_read_sequence(PW_CUR_ALARM_REQ, answer);
	if (res <= 0){
		upslogx(LOG_ERR, "Short read from UPS");
		dstate_datastale();
		return;
	}
	else
	{
		bcmxcp_status.alarm_on_battery = 0;
		bcmxcp_status.alarm_low_battery = 0;

		/* Set alarms	*/
		/* alarm_init(); Alarms are not	supported by NUT */

		/* Check "On Battery"	alarm	*/
		if (bcmxcp_alarm_map[BCMXCP_ALARM_UPS_ON_BATTERY].alarm_block_index	>= 0 &&
				answer[bcmxcp_alarm_map[BCMXCP_ALARM_UPS_ON_BATTERY].alarm_block_index]	>	0)
		{ 
			bcmxcp_status.alarm_on_battery = 1;
			/* alarm_set(ONBATTERY); */
		}
		
		if (bcmxcp_alarm_map[BCMXCP_ALARM_BATTERY_LOW].alarm_block_index >= 0 &&
			answer[bcmxcp_alarm_map[BCMXCP_ALARM_BATTERY_LOW].alarm_block_index] > 0)
		{ 
			bcmxcp_status.alarm_low_battery = 1;
			/* alarm_set(LOWBATTERY);	*/ 
		}
		
		/* Confirm alarms	*/
		/* alarm_commit(); */
	}

	/* Get status info from UPS */
	res = command_read_sequence(PW_STATUS_REQ, answer);
	if (res <= 0)
	{
		upslogx(LOG_ERR, "Short read from UPS");
		dstate_datastale();
		return;
	}
	else
	{
		unsigned char status, topology;

		/* Get overall status */
		memcpy(&status, answer, sizeof(status));

		/* Get topology status bitmap, validate */
		memcpy(&topology, answer+1, sizeof(topology));
		topology &= bcmxcp_status.topology_mask;

		/* Set status */
		status_init();
	
		switch (status) {
			case 0x50: /* On line, everything is fine */
				status_set("OL");
				break;
			case 0xf0: /* Off line */
				if (bcmxcp_status.alarm_on_battery == 0)
				  status_set("OB");
				break;
			case 0xe0: /* Overload */
				status_set("OVER");
				break;
			case 0x63: /* Trim */
				status_set("TRIM");
				break;
			case 0x62:
			case 0x61: /* Boost */
				status_set("BOOST");
				break;
			case 0x60: /* Bypass */
				status_set("BYPASS");
				break;
			case 0x10: /* Mostly off */
				status_set("OFF");
				break;
			default: /* Unknown, assume it is OK... */
				status_set("OL");
				break;
		}

		/* We might have to modify status based on topology status */
		if (topology & 0x20 && bcmxcp_status.alarm_low_battery == 0)
			status_set("LB");

		/* And finally, we might need to modify status based on alarms - the most correct way */
		if (bcmxcp_status.alarm_on_battery)
			status_set("OB");
		if (bcmxcp_status.alarm_low_battery)
			status_set("LB");

		status_commit();
	} 
	dstate_dataok();
}

void upsdrv_shutdown(void)
{
	/* tell the UPS to shut down, then return - DO NOT SLEEP HERE */
	unsigned char answer[5], cbuf[3];
	
	int res, sec;

	/* maybe try to detect the UPS here, but try a shutdown	even if
		 it doesn't respond at first if possible */
	send_write_command(AUTHOR, 4);
	
	sleep(1);	/* Need to. Have to wait at least 0,25 sec max 16 sec */

	cbuf[0] = PW_LOAD_OFF_RESTART;
	cbuf[1] = (unsigned char)(bcmxcp_status.shutdowndelay && 0x00ff);	/* "delay" sec delay for shutdown, */
	cbuf[2] = (unsigned char)(bcmxcp_status.shutdowndelay >> 8);		/* hige byte sec. From ups.conf. */
	
	res = command_write_sequence(cbuf, 3, answer);
	if (res <= 0) {
		upslogx(LOG_ERR, "Short read from UPS");
		dstate_datastale();
		return;
	}
	
	sec = (256 * (unsigned char)answer[3]) + (unsigned char)answer[2];
	
	/* NOTE: get the respons, and return info code located as first data byte after 4 header bytes.
		Is implemented in answers to command packet's.
	0x31 Accepted
	0x32 not implemented
	0x33 Busy
	0x34 Unrecognized
	0x35 Parameter out of range
	0x36 Parameter invalid
	0x37 Accepted with parameter adjusted
	*/
	switch ((unsigned char) answer[0]) {
	
		case 0x31: {
			ser_comm_good();
			upslogx(LOG_NOTICE,"Going down in %d sec", sec);
			break;
			}
		case 0x33: {
			fatalx("shutdown disbled by front panel");
			break;
			}
		case 0x36: {
			fatalx("Invalid parameter");
			break;
			}
		default: {
			fatalx("shutdown not supported");
			break;
			}
	}
}


static int instcmd(const char *cmdname, const char *extra)
{
	unsigned char answer[5], cbuf[3];

	int res, sec;
	
	
	if (!strcasecmp(cmdname, "shutdown.return")) {
		send_write_command(AUTHOR, 4);
	
		sleep(1);	/* Need to. Have to wait at least 0,25 sec max 16 sec */
	
		cbuf[0] = PW_LOAD_OFF_RESTART;
		cbuf[1] = (unsigned char)(bcmxcp_status.shutdowndelay && 0x00ff);	/* "delay" sec delay for shutdown, */
		cbuf[2] = (unsigned char)(bcmxcp_status.shutdowndelay >> 8);		/* hige byte sec. From ups.conf. */
	
		res = command_write_sequence(cbuf, 3, answer);
		if (res <= 0) {
			upslogx(LOG_ERR, "Short read from UPS");
			dstate_datastale();
			return -1;
		}
	
		sec = (256 * (unsigned char)answer[3]) + (unsigned char)answer[2];
	
		switch ((unsigned char) answer[0]) {
	
			case 0x31: {
				upslogx(LOG_NOTICE,"Going down in %d sec", sec);
				return STAT_INSTCMD_HANDLED;
				break;
				}
			case 0x33: {
				upslogx(LOG_NOTICE, "[%s] disbled by front panel", cmdname);
				return STAT_INSTCMD_UNKNOWN;
				break;
				}
			case 0x36: {
				upslogx(LOG_NOTICE, "[%s] Invalid parameter", cmdname);
				return STAT_INSTCMD_UNKNOWN;
				break;
				}
			default: {
				upslogx(LOG_NOTICE, "[%s] not supported", cmdname);
				return STAT_INSTCMD_UNKNOWN;
				break;
				}
		}

	}

	if (!strcasecmp(cmdname, "shutdown.stayoff")) {
		send_write_command(AUTHOR, 4);
	
		sleep(1);	/* Need to. Have to wait at least 0,25 sec max 16 sec */
	
		res = command_read_sequence(PW_UPS_OFF, answer);
		if (res <= 0) {
			upslogx(LOG_ERR, "Short read from UPS");
			dstate_datastale();
			return -1;
		}
	
		switch ((unsigned char) answer[0]) {
	
			case 0x31: {
				upslogx(LOG_NOTICE,"[%s] Going down NOW", cmdname);
				return STAT_INSTCMD_HANDLED;
				break;
				}
			case 0x33: {
				upslogx(LOG_NOTICE, "[%s] disbled by front panel", cmdname);
				return STAT_INSTCMD_UNKNOWN;
				break;
				}
			case 0x36: {
				upslogx(LOG_NOTICE, "[%s] Invalid parameter", cmdname);
				return STAT_INSTCMD_UNKNOWN;
				break;
				}
			default: {
				upslogx(LOG_NOTICE, "[%s] not supported", cmdname);
				return STAT_INSTCMD_UNKNOWN;
				break;
				}
		}

	}

	if (!strcasecmp(cmdname, "test.battery.start")) {
		send_write_command(AUTHOR, 4);
	
		sleep(1);	/* Need to. Have to wait at least 0,25 sec max 16 sec */
	
		cbuf[0] = PW_INIT_BAT_TEST;
		cbuf[1] = 0x0A;			/* 10 sec start delay for test.*/
		cbuf[2] = 0x1E;			/* 30 sec test duration.*/
		
		res = command_write_sequence(cbuf, 3, answer);
		if (res <= 0) {
			upslogx(LOG_ERR, "Short read from UPS");
			dstate_datastale();
			return -1;
		}
	
		switch ((unsigned char) answer[0]) {
	
			case 0x31: {
				upslogx(LOG_NOTICE,"[%s] Testing now", cmdname);
				return STAT_INSTCMD_HANDLED;
				break;
				}
			case 0x33: {
				upslogx(LOG_NOTICE, "[%s] disbled by front panel", cmdname);
				return STAT_INSTCMD_UNKNOWN;
				break;
				}
			case 0x36: {
				upslogx(LOG_NOTICE, "[%s] Invalid parameter", cmdname);
				return STAT_INSTCMD_UNKNOWN;
				break;
				}
			default: {
				upslogx(LOG_NOTICE, "[%s] not supported", cmdname);
				return STAT_INSTCMD_UNKNOWN;
				break;
				}
		}
		/* Get test info from UPS ?
			 Should we wait for 50 sec and get the
			 answer from the test.
			 Or return, as we may lose line power
			 and need to do a shutdown.*/
		
	}
	
	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}


void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "shutdown_delay", "Specify shutdown delay (seconds)");
}

void upsdrv_banner(void)
{
	printf("Network UPS Tools - BCMXCP UPS driver %s (%s)\n\n",
		DRV_VERSION, UPS_VERSION);
}

void upsdrv_initups(void)
{
	int i;
	experimental_driver=1;	

	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B9600);

	for(i = 0;i < 2; i++)	/* send ESC two times to take it out of menu */
	ser_send_char(upsfd, 0x1d);
}

void upsdrv_cleanup(void)
{
	/* free(dynamic_mem); */
	ser_close(upsfd, device_path);
}
