/*
 * types.h:		HID Parser types definitions
 *
 * This file is part of the MGE UPS SYSTEMS HID Parser
 *
 * Copyright (C)
 *	1998-2003	MGE UPS SYSTEMS, Luc Descotils
 *	2015		Eaton, Arnaud Quette (Update MAX_REPORT)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * -------------------------------------------------------------------------- */

#ifndef HIDTYPES_H
#define HIDTYPES_H

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif /* __cplusplus */

#include <sys/types.h>

#include "nut_stdint.h"

/*
 * Constants
 * -------------------------------------------------------------------------- */
#define PATH_SIZE         10   /* Deep max for Path                   */
#define USAGE_TAB_SIZE    50   /* Size of usage stack                 */
#define MAX_REPORT        500  /* Including FEATURE, INPUT and OUTPUT */
#define REPORT_DSC_SIZE   6144 /* Size max of Report Descriptor       */
#define MAX_REPORT_TS     3    /* Max time validity of a report       */

/*
 * Items
 * -------------------------------------------------------------------------- */
#define SIZE_0				0x00
#define SIZE_1				0x01
#define SIZE_2				0x02
#define SIZE_4				0x03
#define SIZE_MASK			0x03

#define TYPE_MAIN			0x00
#define TYPE_GLOBAL			0x04
#define TYPE_LOCAL			0x08
#define TYPE_MASK			0x0C

/* Main items */
#define ITEM_COLLECTION			0xA0
#define ITEM_END_COLLECTION		0xC0
#define ITEM_FEATURE			0xB0
#define ITEM_INPUT			0x80
#define ITEM_OUTPUT			0x90

/* Global items */
#define ITEM_UPAGE			0x04
#define ITEM_LOG_MIN			0x14
#define ITEM_LOG_MAX			0x24
#define ITEM_PHY_MIN			0x34
#define ITEM_PHY_MAX			0x44
#define ITEM_UNIT_EXP			0x54
#define ITEM_UNIT			0x64
#define ITEM_REP_SIZE			0x74
#define ITEM_REP_ID			0x84
#define ITEM_REP_COUNT			0x94

/* Local items */
#define ITEM_USAGE			0x08
#define ITEM_STRING			0x78

/* Long item */
#define ITEM_LONG			0xFC

#define ITEM_MASK			0xFC

/* Attribute Flags */
#define ATTR_DATA_CST			0x01
#define ATTR_NVOL_VOL			0x80

/* Usage Pages */
/* For more details, please see docs/hid-subdrivers.txt */
#define PAGE_POWER_DEVICE		0x84
#define PAGE_BATTERY_SYSTEM 		0x85

/* Usage within Power Device page */
#define USAGE_POW_UNDEFINED			0x00840000
#define USAGE_POW_I_NAME			0x00840001
#define USAGE_POW_PRESENT_STATUS		0x00840002
#define USAGE_POW_CHANGED_STATUS		0x00840003
#define USAGE_POW_UPS				0x00840004
#define USAGE_POW_POWER_SUPPLY			0x00840005
#define USAGE_POW_PERIPHERAL_DEVICE		0x00840006
#define USAGE_POW_BATTERY_SYSTEM		0x00840010
#define USAGE_POW_BATTERY_SYSTEM_ID		0x00840011
#define USAGE_POW_BATTERY			0x00840012
#define USAGE_POW_BATTERY_ID			0x00840013
#define USAGE_POW_CHARGER			0x00840014
#define USAGE_POW_CHARGER_ID			0x00840015
#define USAGE_POW_POWER_CONVERTER		0x00840016
#define USAGE_POW_POWER_CONVERTER_ID		0x00840017
#define USAGE_POW_OUTLET_SYSTEM			0x00840018
#define USAGE_POW_OUTLET_SYSTEM_ID		0x00840019
#define USAGE_POW_INPUT				0x0084001A
#define USAGE_POW_INPUT_ID			0x0084001B
#define USAGE_POW_OUTPUT			0x0084001C
#define USAGE_POW_OUTPUT_ID			0x0084001D
#define USAGE_POW_FLOW				0x0084001E
#define USAGE_POW_FLOW_ID			0x0084001F
#define USAGE_POW_OUTLET			0x00840020
#define USAGE_POW_OUTLET_ID			0x00840021
#define USAGE_POW_GANG				0x00840022
#define USAGE_POW_GANG_ID			0x00840023
#define USAGE_POW_POWER_SUMMARY			0x00840024
#define USAGE_POW_POWER_SUMMARY_ID		0x00840025
#define USAGE_POW_VOLTAGE			0x00840030
#define USAGE_POW_CURRENT			0x00840031
#define USAGE_POW_FREQUENCY			0x00840032
#define USAGE_POW_APPARENT_POWER		0x00840033
#define USAGE_POW_ACTIVE_POWER			0x00840034
#define USAGE_POW_PERCENT_LOAD			0x00840035
#define USAGE_POW_TEMPERATURE			0x00840036
#define USAGE_POW_HUMIDITY			0x00840037
#define USAGE_POW_BAD_COUNT			0x00840038
#define USAGE_POW_CONFIG_VOLTAGE		0x00840040
#define USAGE_POW_CONFIG_CURRENT		0x00840041
#define USAGE_POW_CONFIG_FREQUENCY		0x00840042
#define USAGE_POW_CONFIG_APPARENT_POWER		0x00840043
#define USAGE_POW_CONFIG_ACTIVE_POWER		0x00840044
#define USAGE_POW_CONFIG_PERCENT_LOAD		0x00840045
#define USAGE_POW_CONFIG_TEMPERATURE		0x00840046
#define USAGE_POW_CONFIG_HUMIDITY		0x00840047
#define USAGE_POW_SWITCH_ON_CONTROL		0x00840050
#define USAGE_POW_SWITCH_OFF_CONTROL		0x00840051
#define USAGE_POW_TOGGLE_CONTROL		0x00840052
#define USAGE_POW_LOW_VOLTAGE_TRANSFER		0x00840053
#define USAGE_POW_HIGH_VOLTAGE_TRANSFER		0x00840054
#define USAGE_POW_DELAY_BEFORE_REBOOT		0x00840055
#define USAGE_POW_DELAY_BEFORE_STARTUP		0x00840056
#define USAGE_POW_DELAY_BEFORE_SHUTDOWN		0x00840057
#define USAGE_POW_TEST				0x00840058
#define USAGE_POW_MODULE_RESET			0x00840059
#define USAGE_POW_AUDIBLE_ALARM_CONTROL		0x0084005A
#define USAGE_POW_PRESENT			0x00840060
#define USAGE_POW_GOOD				0x00840061
#define USAGE_POW_INTERNAL_FAILURE		0x00840062
#define USAGE_POW_VOLTAGE_OUT_OF_RANGE		0x00840063
#define USAGE_POW_FREQUENCY_OUT_OF_RANGE	0x00840064
#define USAGE_POW_OVERLOAD			0x00840065
#define USAGE_POW_OVER_CHARGED			0x00840066
#define USAGE_POW_OVER_TEMPERATURE		0x00840067
#define USAGE_POW_SHUTDOWN_REQUESTED		0x00840068
#define USAGE_POW_SHUTDOWN_IMMINENT		0x00840069
#define USAGE_POW_SWITCH_ON_OFF			0x0084006B
#define USAGE_POW_SWITCHABLE			0x0084006C
#define USAGE_POW_USED				0x0084006D
#define USAGE_POW_BOOST				0x0084006E
#define USAGE_POW_BUCK				0x0084006F
#define USAGE_POW_INITIALIZED			0x00840070
#define USAGE_POW_TESTED			0x00840071
#define USAGE_POW_AWAITING_POWER		0x00840072
#define USAGE_POW_COMMUNICATION_LOST		0x00840073
#define USAGE_POW_I_MANUFACTURER		0x008400FD
#define USAGE_POW_I_PRODUCT			0x008400FE
#define USAGE_POW_I_SERIAL_NUMBER		0x008400FF

/* Usage within Battery System page */
#define USAGE_BAT_UNDEFINED			0x00850000
#define USAGE_BAT_SMB_BATTERY_MODE		0x00850001
#define USAGE_BAT_SMB_BATTERY_STATUS		0x00850002
#define USAGE_BAT_SMB_ALARM_WARNING		0x00850003
#define USAGE_BAT_SMB_CHARGER_MODE		0x00850004
#define USAGE_BAT_SMB_CHARGER_STATUS		0x00850005
#define USAGE_BAT_SMB_CHARGER_SPEC_INFO		0x00850006
#define USAGE_BAT_SMB_SELECTOR_STATE		0x00850007
#define USAGE_BAT_SMB_SELECTOR_PRESETS		0x00850008
#define USAGE_BAT_SMB_SELECTOR_INFO		0x00850009
#define USAGE_BAT_OPTIONAL_MFG_FUNCTION_1	0x00850010
#define USAGE_BAT_OPTIONAL_MFG_FUNCTION_2	0x00850011
#define USAGE_BAT_OPTIONAL_MFG_FUNCTION_3	0x00850012
#define USAGE_BAT_OPTIONAL_MFG_FUNCTION_4	0x00850013
#define USAGE_BAT_OPTIONAL_MFG_FUNCTION_5	0x00850014
#define USAGE_BAT_CONNECTION_TO_SMBUS		0x00850015
#define USAGE_BAT_OUTPUT_CONNECTION		0x00850016
#define USAGE_BAT_CHARGER_CONNECTION		0x00850017
#define USAGE_BAT_BATTERY_INSERTION		0x00850018
#define USAGE_BAT_USE_NEXT			0x00850019
#define USAGE_BAT_OK_TO_USE			0x0085001A
#define USAGE_BAT_BATTERY_SUPPORTED		0x0085001B
#define USAGE_BAT_SELECTOR_REVISION		0x0085001C
#define USAGE_BAT_CHARGING_INDICATOR		0x0085001D
#define USAGE_BAT_MANUFACTURER_ACCESS		0x00850028
#define USAGE_BAT_REMAINING_CAPACITY_LIMIT	0x00850029
#define USAGE_BAT_REMAINING_TIME_LIMIT		0x0085002A
#define USAGE_BAT_AT_RATE			0x0085002B
#define USAGE_BAT_CAPACITY_MODE			0x0085002C
#define USAGE_BAT_BROADCAST_TO_CHARGER		0x0085002D
#define USAGE_BAT_PRIMARY_BATTERY		0x0085002E
#define USAGE_BAT_CHARGE_CONTROLLER		0x0085002F
#define USAGE_BAT_TERMINATE_CHARGE		0x00850040
#define USAGE_BAT_TERMINATE_DISCHARGE		0x00850041
#define USAGE_BAT_BELOW_REMAINING_CAPACITY_LIMIT 0x00850042
#define USAGE_BAT_REMAINING_TIME_LIMIT_EXPIRED	0x00850043
#define USAGE_BAT_CHARGING			0x00850044
#define USAGE_BAT_DISCHARGING			0x00850045
#define USAGE_BAT_FULLY_CHARGED			0x00850046
#define USAGE_BAT_FULLY_DISCHARGED		0x00850047
#define USAGE_BAT_CONDITIONING_FLAG		0x00850048
#define USAGE_BAT_AT_RATE_OK			0x00850049
#define USAGE_BAT_SMB_ERROR_CODE		0x0085004A
#define USAGE_BAT_NEED_REPLACEMENT		0x0085004B
#define USAGE_BAT_AT_RATE_TIME_TO_FULL		0x00850060
#define USAGE_BAT_AT_RATE_TIME_TO_EMPTY		0x00850061
#define USAGE_BAT_AVERAGE_CURRENT		0x00850062
#define USAGE_BAT_MAX_ERROR			0x00850063
#define USAGE_BAT_RELATIVE_STATE_OF_CHARGE	0x00850064
#define USAGE_BAT_ABSOLUTE_STATE_OF_CHARGE	0x00850065
#define USAGE_BAT_REMAINING_CAPACITY		0x00850066
#define USAGE_BAT_FULL_CHARGE_CAPACITY		0x00850067
#define USAGE_BAT_RUN_TIME_TO_EMPTY		0x00850068
#define USAGE_BAT_AVERAGE_TIME_TO_EMPTY		0x00850069
#define USAGE_BAT_AVERAGE_TIME_TO_FULL		0x0085006A
#define USAGE_BAT_CYCLE_COUNT			0x0085006B
#define USAGE_BAT_BATT_PACK_MODEL_LEVEL		0x00850080
#define USAGE_BAT_INTERNAL_CHARGE_CONTROLLER	0x00850081
#define USAGE_BAT_PRIMARY_BATTERY_SUPPORT	0x00850082
#define USAGE_BAT_DESIGN_CAPACITY		0x00850083
#define USAGE_BAT_SPECIFICATION_INFO		0x00850084
#define USAGE_BAT_MANUFACTURER_DATE		0x00850085
#define USAGE_BAT_SERIAL_NUMBER			0x00850086
#define USAGE_BAT_I_MANUFACTURER_NAME		0x00850087
#define USAGE_BAT_I_DEVICE_NAME			0x00850088
#define USAGE_BAT_I_DEVICE_CHEMISTRY		0x00850089
#define USAGE_BAT_MANUFACTURER_DATA		0x0085008A
#define USAGE_BAT_RECHARGEABLE			0x0085008B
#define USAGE_BAT_WARNING_CAPACITY_LIMIT	0x0085008C
#define USAGE_BAT_CAPACITY_GRANULARITY_1	0x0085008D
#define USAGE_BAT_CAPACITY_GRANULARITY_2	0x0085008E
#define USAGE_BAT_I_OEMINFORMATION		0x0085008F
#define USAGE_BAT_INHIBIT_CHARGE		0x008500C0
#define USAGE_BAT_ENABLE_POLLING		0x008500C1
#define USAGE_BAT_RESET_TO_ZERO			0x008500C2
#define USAGE_BAT_AC_PRESENT			0x008500D0
#define USAGE_BAT_BATTERY_PRESENT		0x008500D1
#define USAGE_BAT_POWER_FAIL			0x008500D2
#define USAGE_BAT_ALARM_INHIBITED		0x008500D3
#define USAGE_BAT_THERMISTOR_UNDER_RANGE	0x008500D4
#define USAGE_BAT_THERMISTOR_HOT		0x008500D5
#define USAGE_BAT_THERMISTOR_COLD		0x008500D6
#define USAGE_BAT_THERMISTOR_OVER_RANGE		0x008500D7
#define USAGE_BAT_VOLTAGE_OUT_OF_RANGE		0x008500D8
#define USAGE_BAT_CURRENT_OUT_OF_RANGE		0x008500D9
#define USAGE_BAT_CURRENT_NOT_REGULATED		0x008500DA
#define USAGE_BAT_VOLTAGE_NOT_REGULATED		0x008500DB
#define USAGE_BAT_MASTER_MODE			0x008500DC
#define USAGE_BAT_CHARGER_SELECTOR_SUPPORT	0x008500F0
#define USAGE_BAT_CHARGER_SPEC			0x008500F1
#define USAGE_BAT_LEVEL_2			0x008500F2
#define USAGE_BAT_LEVEL_3			0x008500F3

/*
 * HIDNode_t struct
 *
 * Describe a HID Path point: Usage = bits 0..15, UPage = bits 16..31
 * -------------------------------------------------------------------------- */
typedef uint32_t HIDNode_t;

/*
 * HIDPath struct
 *
 * Describe a HID Path
 * -------------------------------------------------------------------------- */
typedef struct {
	uint8_t		Size;				/* HID Path size			*/
	HIDNode_t	Node[PATH_SIZE];		/* HID Path				*/
} HIDPath_t;

/*
 * HIDData struct
 *
 * Describe a HID Data with its location in report
 * -------------------------------------------------------------------------- */
typedef struct {
	HIDPath_t	Path;				/* HID Path				*/

	uint8_t		ReportID;			/* Report ID				*/
	uint8_t		Offset;				/* Offset of data in report	*/
	uint8_t		Size;				/* Size of data in bit		*/

	uint8_t		Type;				/* Type : FEATURE / INPUT / OUTPUT */
	uint8_t		Attribute;			/* Report field attribute		*/

	long		Unit;				/* HID Unit				*/
	int8_t		UnitExp;			/* Unit exponent			*/

	long		LogMin;				/* Logical Min			*/
	long		LogMax;				/* Logical Max			*/
	long		PhyMin;				/* Physical Min			*/
	long		PhyMax;				/* Physical Max			*/
	int8_t		have_PhyMin;			/* Physical Min defined?		*/
	int8_t		have_PhyMax;			/* Physical Max defined?		*/
} HIDData_t;

/*
 * HIDDesc struct
 *
 * Holds a parsed report descriptor
 * -------------------------------------------------------------------------- */
typedef struct {
	size_t		nitems;				/* number of items in descriptor */
	HIDData_t	*item;				/* list of items			*/
	size_t		replen[256];		/* list of report lengths, in byte */
} HIDDesc_t;

#ifdef __cplusplus
/* *INDENT-OFF* */
} /* extern "C" */
/* *INDENT-ON* */
#endif /* __cplusplus */

#endif /* HIDTYPES_H */
