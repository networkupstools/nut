/*
 * bcmxcp.h -- header for BCM/XCP module
 */

#ifndef _POWERWARE_H
#define _POWERWARE_H

#include "timehead.h"

/* Have to wait at least 0,25 sec max 16 sec */
/* 1 second is too short for PW9120 (leads to communication errors). So we set it to 2 seconds */
#define PW_SLEEP 2

#define PW_MAX_TRY 3 /* How many times we try to send data. */

#define PW_COMMAND_START_BYTE (unsigned char)0xAB
#define PW_LAST_SEQ           (unsigned char)0x80 /* bit flag to indicate final sequence */
#define PW_SEQ_MASK           (unsigned char)0x7F /* bit mask to extract just the sequence # */
#define PW_HEADER_LENGTH			4 /* Size of response header */

#define PW_ANSWER_MAX_SIZE 256

/* No Autorisation required */
#define PW_ID_BLOCK_REQ                 (unsigned char)0x31 /* Model name, ... length 1 */
#define PW_EVENT_HISTORY_LOG_REQ        (unsigned char)0x32 /* List alarms that have occurred. length 1 */
#define PW_STATUS_REQ                   (unsigned char)0x33 /* On Line, On Bypass, ...  length 1-2 */
#define PW_METER_BLOCK_REQ              (unsigned char)0x34 /* Current UPS status (Load, utility,...) length 1 */
#define PW_CUR_ALARM_REQ                (unsigned char)0x35 /* Current alarm and event request. length 1 */
#define PW_CONFIG_BLOCK_REQ             (unsigned char)0x36 /* Model serial#, ... length 1 */
#define PW_UTILITY_STATISTICS_BLOCK_REQ (unsigned char)0x38 /* List utility power quality. length 1 */
#define PW_WAVEFORM_BLOCK_REQ           (unsigned char)0x3A /* Sampled waveform data. length 7 */
#define PW_BATTERY_REQ                  (unsigned char)0x3B /* Charging, floating, ... length 1 */
#define PW_LIMIT_BLOCK_REQ              (unsigned char)0x3C /* Configuration (Bypass thresholds,...). length 1 */
#define PW_TEST_RESULT_REQ              (unsigned char)0x3F /* Get the results for a system test. length 1 */
#define PW_COMMAND_LIST_REQ             (unsigned char)0x40 /* Available commands. length 1 */
#define PW_OUT_MON_BLOCK_REQ            (unsigned char)0x41 /* Outlet monitor request length 1 */
#define PW_COM_CAP_REQ                  (unsigned char)0x42 /* Request communication capabilities. length 2 */
#define PW_UPS_TOP_DATA_REQ             (unsigned char)0x43 /* Request ups topology data requset. length 1 */
#define PW_COM_PORT_LIST_BLOCK_REQ      (unsigned char)0x44 /* Request communication port list. length 1 */
#define PW_REQUEST_SCRATCHPAD_DATA_REQ  (unsigned char)0x45 /* Request data from scratchpad. length 2*/

/* Need autorisation before these commands */
#define PW_GO_TO_BYPASS             (unsigned char)0x88 /* Transfer load from inverter to bypass. length 1 or 3 */
#define PW_UPS_ON                   (unsigned char)0x89 /* UPS on command. length 1-2 */
#define PW_LOAD_OFF_RESTART         (unsigned char)0x8A /* Delayed LoadPowerOff & Restart command. length 2-4 */
#define PW_UPS_OFF                  (unsigned char)0x8B /* UPS off command. length 1-2 */
#define PW_DECREMENT_OUTPUT_VOLTAGE (unsigned char)0x8C /* Decrease output voltage. length 1 */
#define PW_INCREMENT_OUTPUT_VOLTAGE (unsigned char)0x8D /* Increase output voltage. length 1 */
#define PW_SET_TIME_AND_DATE        (unsigned char)0x90 /* Set the real-time clock inside UPS. length 8 */
#define PW_UPS_ON_TIME              (unsigned char)0x91 /* Scheduled UPS on in n minutes. length 3-4 */
#define PW_UPS_ON_AT_TIME           (unsigned char)0x92 /* Schedule UPS on at specified date and time. length 7-8 */
#define PW_UPS_OFF_TIME             (unsigned char)0x93 /* Scheduled UPS off in n minutes. length 3-4 */
#define PW_UPS_OFF_AT_TIME          (unsigned char)0x94 /* Schedule UPS off at specified date and time. length 7-8 */
#define PW_SET_CONF_COMMAND         (unsigned char)0x95 /* Set configuration command. length 4 */
#define PW_SET_OUTLET_COMMAND       (unsigned char)0x97 /* Set outlet parameter command length 5. not in 5115 */
#define PW_SET_COM_COMMAND          (unsigned char)0x98 /* Set communication parameter command. length 5 */
#define PW_SET_SCRATHPAD_SECTOR     (unsigned char)0x99 /* Write data to scratchpad. length 3 or 18 */
#define PW_SET_POWER_STRATEGY       (unsigned char)0x9A /* Set the power strategy. length 2 */
#define PW_SET_REQ_ONLY_MODE        (unsigned char)0xA0 /* Set request only mode command. length 1 */
#define PW_SET_UNREQUESTED_MODE     (unsigned char)0xA1 /* Set unrequested mode command. length 1 */
#define PW_INIT_BAT_TEST            (unsigned char)0xB1 /* Initiate battery test command. length 3 */
#define PW_INIT_SYS_TEST            (unsigned char)0xB2 /* Initiate general system test command. length 2 */
#define PW_SELECT_SUBMODULE         (unsigned char)0xCE /* Select a sub module. length 2-7 */
#define PW_AUTHORIZATION_CODE       (unsigned char)0xCF /* Authorization code. length 4 or 7 */

/* Define the XCP system test */
#define PW_SYS_TEST_GENERAL                     (unsigned char)0x01 /* Initiate General system Test */
#define PW_SYS_TEST_SCHEDULE_BATTERY_COMMISSION (unsigned char)0x02 /* Schedule Battery Commissioning Test */
#define PW_SYS_TEST_ALTERNATE_AC_INPUT          (unsigned char)0x03 /* Test Alternate AC Input */
#define PW_SYS_TEST_FLASH_LIGHTS                (unsigned char)0x04 /* Flash the Lights Test */
#define PW_SYS_TEST_REPORT_CAPABILITIES         (unsigned char)0xFF /* Report Systems Test Capabilities */

/* Outlet operations */
#define PW_ALL_OUTLETS    0
#define PW_AUTO_OFF_DELAY 1
#define PW_AUTO_ON_DELAY  2
/* 0 means Abort countdown */
#define PW_TURN_OFF_DELAY 3
#define PW_TURN_ON_DELAY  4

/* Config vars*/
#define PW_CONF_BYPASS_FREQ_DEV_LIMIT   0x01
#define PW_CONF_LOW_DEV_LIMIT           0x02
#define PW_CONF_HIGH_DEV_LIMIT          0x03
#define PW_CONF_PHASE_DEV_LIMIT         0x04
#define PW_CONF_LOW_BATT                0x05
#define PW_CONF_BEEPER                  0x06
#define PW_CONF_RETURN_DELAY            0x07
#define PW_CONF_RETURN_CAP              0x08
#define PW_CONF_MAX_TEMP                0x0a
#define PW_CONF_NOMINAL_OUT_VOLTAGE     0x0b
#define PW_CONF_SLEEP_TH_LOAD           0x0d
#define PW_CONF_SLEEP_DELAY             0x0e
#define PW_CONF_BATT_STRINGS            0x0f
#define PW_CONF_REQ			0xff

/* Config block offsets */
#define BCMXCP_CONFIG_BLOCK_MACHINE_TYPE_CODE           0
#define BCMXCP_CONFIG_BLOCK_MODEL_NUMBER                2
#define BCMXCP_CONFIG_BLOCK_MODEL_CONF_WORD             4
#define BCMXCP_CONFIG_BLOCK_INPUT_FREQ_DEV_LIMIT        6
#define BCMXCP_CONFIG_BLOCK_NOMINAL_OUTPUT_VOLTAGE      8
#define BCMXCP_CONFIG_BLOCK_NOMINAL_OUTPUT_FREQ        10
#define BCMXCP_CONFIG_BLOCK_OUTPUT_PHASE_ANGLE         12
#define BCMXCP_CONFIG_BLOCK_HW_MODULES_INSTALLED_WORD1 14
#define BCMXCP_CONFIG_BLOCK_HW_MODULES_INSTALLED_BYTE3 16 /* KEEP THIS UNTILL PARSING OK. USE THIS BYTE. */
#define BCMXCP_CONFIG_BLOCK_HW_MODULES_INSTALLED_WORD2 16
#define BCMXCP_CONFIG_BLOCK_HW_MODULES_INSTALLED_WORD3 18
#define BCMXCP_CONFIG_BLOCK_HW_MODULES_INSTALLED_WORD4 20
#define BCMXCP_CONFIG_BLOCK_BATTERY_DATA_WORD1         22 /* Undefined at this time.*/
#define BCMXCP_CONFIG_BLOCK_BATTERY_DATA_WORD2         24 /* Per cell inverter shutdown voltage at full rated load. (volt/cell)* 100 */
#define BCMXCP_CONFIG_BLOCK_BATTERY_DATA_WORD3         26 /* LOW BYTE Number of battery strings. HIGH BYTE undefined at this time.*/
#define BCMXCP_CONFIG_BLOCK_EXTENDED_BLOCK_LENGTH      47
#define BCMXCP_CONFIG_BLOCK_PART_NUMBER                48
#define BCMXCP_CONFIG_BLOCK_SERIAL_NUMBER              64

/*Battery block offsets*/

#define BCMXCP_BATTDATA_BLOCK_BATT_TEST_STATUS          0
#define BCMXCP_BATTDATA_BLOCK_BATT_VOLTS_T1             1
#define BCMXCP_BATTDATA_BLOCK_BATT_VOLTS_T2             5
#define BCMXCP_BATTDATA_BLOCK_TEST_DURATION             9
#define BCMXCP_BATTDATA_BLOCK_UTIL_VOLT                10
#define BCMXCP_BATTDATA_BLOCK_INPUT_CURRENT            14
#define BCMXCP_BATTDATA_BLOCK_NUMBER_OF_STRINGS        18
/*BATT_TEST_STATUS for external strings (1 byte each) if BCMXCP_BATTDATA_BLOCK_NUMBER_OF_STRINGS == 0 no external test statuses at all*/
/*next - number of ABM Statuses - at least 1 for internal batteries*/

/* Index for Extende Limits block offsets */
#define BCMXCP_EXT_LIMITS_BLOCK_NOMINAL_INPUT_VOLTAGE   0
#define BCMXCP_EXT_LIMITS_BLOCK_NOMINAL_INPUT_FREQ      2
#define BCMXCP_EXT_LIMITS_BLOCK_NOMINAL_TRUE_POWER      4
#define BCMXCP_EXT_LIMITS_BLOCK_COMM_SPEC_VERSION       6
#define BCMXCP_EXT_LIMITS_BLOCK_FREQ_DEV_LIMIT          8
#define BCMXCP_EXT_LIMITS_BLOCK_VOLTAGE_LOW_DEV_LIMIT  10
#define BCMXCP_EXT_LIMITS_BLOCK_VOLTAGE_HIGE_DEV_LIMIT 12
#define BCMXCP_EXT_LIMITS_BLOCK_PHASE_DEV_LIMIT        14
#define BCMXCP_EXT_LIMITS_BLOCK_LOW_BATT_WARNING       16
#define BCMXCP_EXT_LIMITS_BLOCK_HORN_STATUS            17
#define BCMXCP_EXT_LIMITS_BLOCK_MIN_INPUT_VOLTAGE      18
#define BCMXCP_EXT_LIMITS_BLOCK_MAX_INPUT_VOLTAGE      20
#define BCMXCP_EXT_LIMITS_BLOCK_RETURN_STAB_DELAY      22
#define BCMXCP_EXT_LIMITS_BLOCK_BATT_CAPACITY_RETURN   24
#define BCMXCP_EXT_LIMITS_BLOCK_AMBIENT_TEMP_LOW       25
#define BCMXCP_EXT_LIMITS_BLOCK_AMBIENT_TEMP_HIGE      26
#define BCMXCP_EXT_LIMITS_BLOCK_SLEEP_TH_LOAD          29
#define BCMXCP_EXT_LIMITS_BLOCK_SLEEP_DELAY            30

/* Indexes for meter map */
#define BCMXCP_METER_MAP_OUTPUT_VOLTS_AB                         0 /* mapped */
#define BCMXCP_METER_MAP_OUTPUT_VOLTS_BC                         1 /* mapped */
#define BCMXCP_METER_MAP_OUTPUT_VOLTS_CA                         2 /* mapped */
#define BCMXCP_METER_MAP_INPUT_VOLTS_AB                          3 /* mapped */
#define BCMXCP_METER_MAP_INPUT_VOLTS_BC                          4 /* mapped */
#define BCMXCP_METER_MAP_INPUT_VOLTS_CA                          5 /* mapped */
#define BCMXCP_METER_MAP_INVERTER_VOLTS_AB                       6
#define BCMXCP_METER_MAP_INVERTER_VOLTS_BC                       7
#define BCMXCP_METER_MAP_INVERTER_VOLTS_CA                       8
#define BCMXCP_METER_MAP_BYPASS_VOLTS_AB                         9
#define BCMXCP_METER_MAP_BYPASS_VOLTS_BC                        10
#define BCMXCP_METER_MAP_BYPASS_VOLTS_CA                        11
#define BCMXCP_METER_MAP_MAIN_LOGIC_POWER                       12
#define BCMXCP_METER_MAP_SECONDARY_V_PLUS_POWER                 13
#define BCMXCP_METER_MAP_SECONDARY_V_MINUS_POWER                14
#define BCMXCP_METER_MAP_INVERTER_AVG_CURRENT_PHASE_A           15
#define BCMXCP_METER_MAP_INVERTER_AVG_CURRENT_PHASE_B           16
#define BCMXCP_METER_MAP_INVERTER_AVG_CURRENT_PHASE_C           17
#define BCMXCP_METER_MAP_INPUT_CURRENT_PHASE_A                  18 /* mapped */
#define BCMXCP_METER_MAP_INPUT_CURRENT_PHASE_B                  19 /* mapped */
#define BCMXCP_METER_MAP_INPUT_CURRENT_PHASE_C                  20 /* mapped */
#define BCMXCP_METER_MAP_OUTPUT_WATTS                           21
#define BCMXCP_METER_MAP_INPUT_WATTS                            22 /* mapped */
#define BCMXCP_METER_MAP_OUTPUT_VA                              23 /* mapped */
#define BCMXCP_METER_MAP_INPUT_VA                               24 /* mapped */
#define BCMXCP_METER_MAP_OUTPUT_POWER_FACTOR                    25 /* mapped */
#define BCMXCP_METER_MAP_INPUT_POWER_FACTOR                     26 /* mapped */
#define BCMXCP_METER_MAP_OUTPUT_FREQUENCY                       27 /* mapped */
#define BCMXCP_METER_MAP_INPUT_FREQUENCY                        28 /* mapped */
#define BCMXCP_METER_MAP_INVERTER_FREQUENCY                     29
#define BCMXCP_METER_MAP_BYPASS_FREQUENCY                       30 /* mapped */
#define BCMXCP_METER_MAP_DC_LINK_VOLTS_DC                       31
#define BCMXCP_METER_MAP_BATTERY_CURRENT                        32 /* mapped */
#define BCMXCP_METER_MAP_BATTERY_VOLTAGE                        33 /* mapped */
#define BCMXCP_METER_MAP_PERCENT_BATTERY_LEFT                   34 /* mapped */
#define BCMXCP_METER_MAP_BATTERY_TIME_REMAINING                 35 /* mapped */
#define BCMXCP_METER_MAP_BATTERY_CHARGE_TIME                    36
#define BCMXCP_METER_MAP_PEAK_INVERTER_CURRENT_PHASE_A          37
#define BCMXCP_METER_MAP_PEAK_INVERTER_CURRENT_PHASE_B          38
#define BCMXCP_METER_MAP_PEAK_INVERTER_CURRENT_PHASE_C          39
#define BCMXCP_METER_MAP_AVG_INPUT_CURRENT_3_PHASE_SUM          40
#define BCMXCP_METER_MAP_BATTERY_DCUV_BAR_CHART                 41 /* mapped */
#define BCMXCP_METER_MAP_INPUT_CURRENT_BAR_CHART                42
#define BCMXCP_METER_MAP_LOW_BATTERY_WARNING_V_BAR_CHART        43 /* mapped */
#define BCMXCP_METER_MAP_DC_VOLTS_BAR_CHART                     44
#define BCMXCP_METER_MAP_BATTERY_CHARGING_CURRENT_BAR_CHART     45
#define BCMXCP_METER_MAP_BATTERY_DISCHARGING_CURRENT_BAR_CHART  46 /* mapped */
#define BCMXCP_METER_MAP_PERCENT_LOAD_PHASE_A                   47 /* mapped */
#define BCMXCP_METER_MAP_PERCENT_LOAD_PHASE_B                   48 /* mapped */
#define BCMXCP_METER_MAP_PERCENT_LOAD_PHASE_C                   49 /* mapped */
#define BCMXCP_METER_MAP_OUTPUT_VA_PHASE_A                      50 /* mapped */
#define BCMXCP_METER_MAP_OUTPUT_VA_PHASE_B                      51 /* mapped */
#define BCMXCP_METER_MAP_OUTPUT_VA_PHASE_C                      52 /* mapped */
#define BCMXCP_METER_MAP_BYPASS_VOLTS_PHASE_A                   53 /* mapped */
#define BCMXCP_METER_MAP_BYPASS_VOLTS_PHASE_B                   54 /* mapped */
#define BCMXCP_METER_MAP_BYPASS_VOLTS_PHASE_C                   55 /* mapped */
#define BCMXCP_METER_MAP_INPUT_VOLTS_PHASE_A                    56 /* mapped */
#define BCMXCP_METER_MAP_INPUT_VOLTS_PHASE_B                    57 /* mapped */
#define BCMXCP_METER_MAP_INPUT_VOLTS_PHASE_C                    58 /* mapped */
#define BCMXCP_METER_MAP_INVERTER_VOLTS_PHASE_A                 59
#define BCMXCP_METER_MAP_INVERTER_VOLTS_PHASE_B                 60
#define BCMXCP_METER_MAP_INVERTER_VOLTS_PHASE_C                 61
#define BCMXCP_METER_MAP_AMBIENT_TEMPERATURE                    62 /* mapped */
#define BCMXCP_METER_MAP_HEATSINK_TEMPERATURE                   63 /* mapped */
#define BCMXCP_METER_MAP_POWER_SUPPLY_TEMPERATURE               64 /* mapped */
#define BCMXCP_METER_MAP_LOAD_CURRENT_PHASE_A                   65 /* mapped */
#define BCMXCP_METER_MAP_LOAD_CURRENT_PHASE_B                   66 /* mapped */
#define BCMXCP_METER_MAP_LOAD_CURRENT_PHASE_C                   67 /* mapped */
#define BCMXCP_METER_MAP_LOAD_CURRENT_PHASE_A_BAR_CHART         68 /* mapped */
#define BCMXCP_METER_MAP_LOAD_CURRENT_PHASE_B_BAR_CHART         69 /* mapped */
#define BCMXCP_METER_MAP_LOAD_CURRENT_PHASE_C_BAR_CHART         70 /* mapped */
#define BCMXCP_METER_MAP_OUTPUT_VA_BAR_CHART                    71 /* mapped */
#define BCMXCP_METER_MAP_DATE                                   72 /* mapped */
#define BCMXCP_METER_MAP_TIME                                   73 /* mapped */
#define BCMXCP_METER_MAP_POSITIVE_DC_LINK_RAIL_VOLTAGE          74
#define BCMXCP_METER_MAP_NEGATIVE_DC_LINK_RAIL_VOLTAGE          75
#define BCMXCP_METER_MAP_AUTO_BALANCE_VOLTAGE_DC                76
#define BCMXCP_METER_MAP_BATTERY_TEMPERATURE                    77 /* mapped */
#define BCMXCP_METER_MAP_OUTPUT_VOLTS_A                         78 /* mapped */
#define BCMXCP_METER_MAP_OUTPUT_VOLTS_B                         79 /* mapped */
#define BCMXCP_METER_MAP_OUTPUT_VOLTS_C                         80 /* mapped */
#define BCMXCP_METER_MAP_NEUTRAL_CURRENT                        81
#define BCMXCP_METER_MAP_OUTPUT_WATTS_PHASE_A                   82 /* mapped */
#define BCMXCP_METER_MAP_OUTPUT_WATTS_PHASE_B                   83 /* mapped */
#define BCMXCP_METER_MAP_OUTPUT_WATTS_PHASE_C                   84 /* mapped */
#define BCMXCP_METER_MAP_OUTPUT_WATTS_PHASE_A_B_C_BAR_CHART     85 /* mapped */
#define BCMXCP_METER_MAP_RECTIFIER_DC_CURRENT                   86
#define BCMXCP_METER_MAP_POSITIVE_BATTERY_VOLTAGE               87
#define BCMXCP_METER_MAP_NEGATIVE_BATTERY_VOLTAGE               88
#define BCMXCP_METER_MAP_POSITIVE_BATTERY_CURRENT               89
#define BCMXCP_METER_MAP_NEGATIVE_BATTERY_CURRENT               90
#define BCMXCP_METER_MAP_LINE_EVENT_COUNTER                     91 /* mapped */
#define BCMXCP_METER_MAP_OUTPUT_V1_PERCENT                      92
#define BCMXCP_METER_MAP_OUTPUT_V2_PERCENT                      93
#define BCMXCP_METER_MAP_OUTPUT_V3_PERCENT                      94
#define BCMXCP_METER_MAP_OUTPUT_I1_PERCENT                      95
#define BCMXCP_METER_MAP_OUTPUT_I2_PERCENT                      96
#define BCMXCP_METER_MAP_OUTPUT_I3_PERCENT                      97
#define BCMXCP_METER_MAP_INPUT_V1_PERCENT                       98
#define BCMXCP_METER_MAP_INPUT_V2_PERCENT                       99
#define BCMXCP_METER_MAP_INPUT_V3_PERCENT                      100
#define BCMXCP_METER_MAP_INPUT_I1_PERCENT                      101
#define BCMXCP_METER_MAP_INPUT_I2_PERCENT                      102
#define BCMXCP_METER_MAP_INPUT_I3_PERCENT                      103
#define BCMXCP_METER_MAP_GROUND_CURRENT                        104
#define BCMXCP_METER_MAP_OUTPUT_CREST_FACTOR_L1                105
#define BCMXCP_METER_MAP_OUTPUT_CREST_FACTOR_L2                106
#define BCMXCP_METER_MAP_OUTPUT_CREST_FACTOR_L3                107
#define BCMXCP_METER_MAP_OUTPUT_KW_HOUR                        108
#define BCMXCP_METER_MAP_INPUT_VOLTAGE_THD_LINE1               109
#define BCMXCP_METER_MAP_INPUT_VOLTAGE_THD_LINE2               110
#define BCMXCP_METER_MAP_INPUT_VOLTAGE_THD_LINE3               111
#define BCMXCP_METER_MAP_INPUT_CURRENT_THD_LINE1               112
#define BCMXCP_METER_MAP_INPUT_CURRENT_THD_LINE2               113
#define BCMXCP_METER_MAP_INPUT_CURRENT_THD_LINE3               114
#define BCMXCP_METER_MAP_OUTPUT_VOLTAGE_THD_LINE1              115
#define BCMXCP_METER_MAP_OUTPUT_VOLTAGE_THD_LINE2              116
#define BCMXCP_METER_MAP_OUTPUT_VOLTAGE_THD_LINE3              117
#define BCMXCP_METER_MAP_OUTPUT_CURRENT_THD_LINE1              118
#define BCMXCP_METER_MAP_OUTPUT_CURRENT_THD_LINE2              119
#define BCMXCP_METER_MAP_OUTPUT_CURRENT_THD_LINE3              120
#define BCMXCP_METER_MAP_INPUT_CREST_FACTOR_L1                 121
#define BCMXCP_METER_MAP_INPUT_CREST_FACTOR_L2                 122
#define BCMXCP_METER_MAP_INPUT_CREST_FACTOR_L3                 123
#define BCMXCP_METER_MAP_INPUT_KW_HOUR                         124
#define BCMXCP_METER_MAP_BATTERY_LIFE_REMAINING                125
#define BCMXCP_METER_MAP_SECONDARY_NEUTRAL_CURRENT             126
#define BCMXCP_METER_MAP_SECONDARY_GROUND_CURRENT              127
#define BCMXCP_METER_MAP_HOURS_OF_OPERATION                    128

/* Indexes for alarm map */
#define BCMXCP_ALARM_INVERTER_AC_OVER_VOLTAGE               0
#define BCMXCP_ALARM_INVERTER_AC_UNDER_VOLTAGE              1
#define BCMXCP_ALARM_INVERTER_OVER_OR_UNDER_FREQ            2
#define BCMXCP_ALARM_BYPASS_AC_OVER_VOLTAGE                 3
#define BCMXCP_ALARM_BYPASS_AC_UNDER_VOLTAGE                4
#define BCMXCP_ALARM_BYPASS_OVER_OR_UNDER_FREQ              5
#define BCMXCP_ALARM_INPUT_AC_OVER_VOLTAGE                  6
#define BCMXCP_ALARM_INPUT_AC_UNDER_VOLTAGE                 7
#define BCMXCP_ALARM_INPUT_UNDER_OR_OVER_FREQ               8
#define BCMXCP_ALARM_OUTPUT_OVER_VOLTAGE                    9
#define BCMXCP_ALARM_OUTPUT_UNDER_VOLTAGE                  10
#define BCMXCP_ALARM_OUTPUT_UNDER_OR_OVER_FREQ             11
#define BCMXCP_ALARM_REMOTE_EMERGENCY_PWR_OFF              12
#define BCMXCP_ALARM_REMOTE_GO_TO_BYPASS                   13
#define BCMXCP_ALARM_BUILDING_ALARM_6                      14
#define BCMXCP_ALARM_BUILDING_ALARM_5                      15
#define BCMXCP_ALARM_BUILDING_ALARM_4                      16
#define BCMXCP_ALARM_BUILDING_ALARM_3                      17
#define BCMXCP_ALARM_BUILDING_ALARM_2                      18
#define BCMXCP_ALARM_BUILDING_ALARM_1                      19
#define BCMXCP_ALARM_STATIC_SWITCH_OVER_TEMP               20
#define BCMXCP_ALARM_CHARGER_OVER_TEMP                     21
#define BCMXCP_ALARM_CHARGER_LOGIC_PWR_FAIL                22
#define BCMXCP_ALARM_CHARGER_OVER_VOLTAGE_OR_CURRENT       23
#define BCMXCP_ALARM_INVERTER_OVER_TEMP                    24
#define BCMXCP_ALARM_OUTPUT_OVERLOAD                       25
#define BCMXCP_ALARM_RECTIFIER_INPUT_OVER_CURRENT          26
#define BCMXCP_ALARM_INVERTER_OUTPUT_OVER_CURRENT          27
#define BCMXCP_ALARM_DC_LINK_OVER_VOLTAGE                  28
#define BCMXCP_ALARM_DC_LINK_UNDER_VOLTAGE                 29
#define BCMXCP_ALARM_RECTIFIER_FAILED                      30
#define BCMXCP_ALARM_INVERTER_FAULT                        31
#define BCMXCP_ALARM_BATTERY_CONNECTOR_FAIL                32
#define BCMXCP_ALARM_BYPASS_BREAKER_FAIL                   33
#define BCMXCP_ALARM_CHARGER_FAIL                          34
#define BCMXCP_ALARM_RAMP_UP_FAILED                        35
#define BCMXCP_ALARM_STATIC_SWITCH_FAILED                  36
#define BCMXCP_ALARM_ANALOG_AD_REF_FAIL                    37
#define BCMXCP_ALARM_BYPASS_UNCALIBRATED                   38
#define BCMXCP_ALARM_RECTIFIER_UNCALIBRATED                39
#define BCMXCP_ALARM_OUTPUT_UNCALIBRATED                   40
#define BCMXCP_ALARM_INVERTER_UNCALIBRATED                 41
#define BCMXCP_ALARM_DC_VOLT_UNCALIBRATED                  42
#define BCMXCP_ALARM_OUTPUT_CURRENT_UNCALIBRATED           43
#define BCMXCP_ALARM_RECTIFIER_CURRENT_UNCALIBRATED        44
#define BCMXCP_ALARM_BATTERY_CURRENT_UNCALIBRATED          45
#define BCMXCP_ALARM_INVERTER_ON_OFF_STAT_FAIL             46
#define BCMXCP_ALARM_BATTERY_CURRENT_LIMIT                 47
#define BCMXCP_ALARM_INVERTER_STARTUP_FAIL                 48
#define BCMXCP_ALARM_ANALOG_BOARD_AD_STAT_FAIL             49
#define BCMXCP_ALARM_OUTPUT_CURRENT_OVER_100               50
#define BCMXCP_ALARM_BATTERY_GROUND_FAULT                  51
#define BCMXCP_ALARM_WAITING_FOR_CHARGER_SYNC              52
#define BCMXCP_ALARM_NV_RAM_FAIL                           53
#define BCMXCP_ALARM_ANALOG_BOARD_AD_TIMEOUT               54
#define BCMXCP_ALARM_SHUTDOWN_IMMINENT                     55
#define BCMXCP_ALARM_BATTERY_LOW                           56
#define BCMXCP_ALARM_UTILITY_FAIL                          57
#define BCMXCP_ALARM_OUTPUT_SHORT_CIRCUIT                  58
#define BCMXCP_ALARM_UTILITY_NOT_PRESENT                   59
#define BCMXCP_ALARM_FULL_TIME_CHARGING                    60
#define BCMXCP_ALARM_FAST_BYPASS_COMMAND                   61
#define BCMXCP_ALARM_AD_ERROR                              62
#define BCMXCP_ALARM_INTERNAL_COM_FAIL                     63
#define BCMXCP_ALARM_RECTIFIER_SELFTEST_FAIL               64
#define BCMXCP_ALARM_RECTIFIER_EEPROM_FAIL                 65
#define BCMXCP_ALARM_RECTIFIER_EPROM_FAIL                  66
#define BCMXCP_ALARM_INPUT_LINE_VOLTAGE_LOSS               67
#define BCMXCP_ALARM_BATTERY_DC_OVER_VOLTAGE               68
#define BCMXCP_ALARM_POWER_SUPPLY_OVER_TEMP                69
#define BCMXCP_ALARM_POWER_SUPPLY_FAIL                     70
#define BCMXCP_ALARM_POWER_SUPPLY_5V_FAIL                  71
#define BCMXCP_ALARM_POWER_SUPPLY_12V_FAIL                 72
#define BCMXCP_ALARM_HEATSINK_OVER_TEMP                    73
#define BCMXCP_ALARM_HEATSINK_TEMP_SENSOR_FAIL             74
#define BCMXCP_ALARM_RECTIFIER_CURRENT_OVER_125            75
#define BCMXCP_ALARM_RECTIFIER_FAULT_INTERRUPT_FAIL        76
#define BCMXCP_ALARM_RECTIFIER_POWER_CAPASITOR_FAIL        77
#define BCMXCP_ALARM_INVERTER_PROGRAM_STACK_ERROR          78
#define BCMXCP_ALARM_INVERTER_BOARD_SELFTEST_FAIL          79
#define BCMXCP_ALARM_INVERTER_AD_SELFTEST_FAIL             80
#define BCMXCP_ALARM_INVERTER_RAM_SELFTEST_FAIL            81
#define BCMXCP_ALARM_NV_MEMORY_CHECKSUM_FAIL               82
#define BCMXCP_ALARM_PROGRAM_CHECKSUM_FAIL                 83
#define BCMXCP_ALARM_INVERTER_CPU_SELFTEST_FAIL            84
#define BCMXCP_ALARM_NETWORK_NOT_RESPONDING                85
#define BCMXCP_ALARM_FRONT_PANEL_SELFTEST_FAIL             86
#define BCMXCP_ALARM_NODE_EEPROM_VERIFICATION_ERROR        87
#define BCMXCP_ALARM_OUTPUT_AC_OVER_VOLT_TEST_FAIL         88
#define BCMXCP_ALARM_OUTPUT_DC_OVER_VOLTAGE                89
#define BCMXCP_ALARM_INPUT_PHASE_ROTATION_ERROR            90
#define BCMXCP_ALARM_INVERTER_RAMP_UP_TEST_FAILED          91
#define BCMXCP_ALARM_INVERTER_OFF_COMMAND                  92
#define BCMXCP_ALARM_INVERTER_ON_COMMAND                   93
#define BCMXCP_ALARM_TO_BYPASS_COMMAND                     94
#define BCMXCP_ALARM_FROM_BYPASS_COMMAND                   95
#define BCMXCP_ALARM_AUTO_MODE_COMMAND                     96
#define BCMXCP_ALARM_EMERGENCY_SHUTDOWN_COMMAND            97
#define BCMXCP_ALARM_SETUP_SWITCH_OPEN                     98
#define BCMXCP_ALARM_INVERTER_OVER_VOLT_INT                99
#define BCMXCP_ALARM_INVERTER_UNDER_VOLT_INT              100
#define BCMXCP_ALARM_ABSOLUTE_DCOV_ACOV                   101
#define BCMXCP_ALARM_PHASE_A_CURRENT_LIMIT                102
#define BCMXCP_ALARM_PHASE_B_CURRENT_LIMIT                103
#define BCMXCP_ALARM_PHASE_C_CURRENT_LIMIT                104
#define BCMXCP_ALARM_BYPASS_NOT_AVAILABLE                 105
#define BCMXCP_ALARM_RECTIFIER_BREAKER_OPEN               106
#define BCMXCP_ALARM_BATTERY_CONTACTOR_OPEN               107
#define BCMXCP_ALARM_INVERTER_CONTACTOR_OPEN              108
#define BCMXCP_ALARM_BYPASS_BREAKER_OPEN                  109
#define BCMXCP_ALARM_INV_BOARD_ACOV_INT_TEST_FAIL         110
#define BCMXCP_ALARM_INVERTER_OVER_TEMP_TRIP              111
#define BCMXCP_ALARM_INV_BOARD_ACUV_INT_TEST_FAIL         112
#define BCMXCP_ALARM_INVERTER_VOLTAGE_FEEDBACK_ERROR      113
#define BCMXCP_ALARM_DC_UNDER_VOLTAGE_TIMEOUT             114
#define BCMXCP_ALARM_AC_UNDER_VOLTAGE_TIMEOUT             115
#define BCMXCP_ALARM_DC_UNDER_VOLTAGE_WHILE_CHARGE        116
#define BCMXCP_ALARM_INVERTER_VOLTAGE_BIAS_ERROR          117
#define BCMXCP_ALARM_RECTIFIER_PHASE_ROTATION             118
#define BCMXCP_ALARM_BYPASS_PHASER_ROTATION               119
#define BCMXCP_ALARM_SYSTEM_INTERFACE_BOARD_FAIL          120
#define BCMXCP_ALARM_PARALLEL_BOARD_FAIL                  121
#define BCMXCP_ALARM_LOST_LOAD_SHARING_PHASE_A            122
#define BCMXCP_ALARM_LOST_LOAD_SHARING_PHASE_B            123
#define BCMXCP_ALARM_LOST_LOAD_SHARING_PHASE_C            124
#define BCMXCP_ALARM_DC_OVER_VOLTAGE_TIMEOUT              125
#define BCMXCP_ALARM_BATTERY_TOTALLY_DISCHARGED           126
#define BCMXCP_ALARM_INVERTER_PHASE_BIAS_ERROR            127
#define BCMXCP_ALARM_INVERTER_VOLTAGE_BIAS_ERROR_2        128
#define BCMXCP_ALARM_DC_LINK_BLEED_COMPLETE               129
#define BCMXCP_ALARM_LARGE_CHARGER_INPUT_CURRENT          130
#define BCMXCP_ALARM_INV_VOLT_TOO_LOW_FOR_RAMP_LEVEL      131
#define BCMXCP_ALARM_LOSS_OF_REDUNDANCY                   132
#define BCMXCP_ALARM_LOSS_OF_SYNC_BUS                     133
#define BCMXCP_ALARM_RECTIFIER_BREAKER_SHUNT_TRIP         134
#define BCMXCP_ALARM_LOSS_OF_CHARGER_SYNC                 135
#define BCMXCP_ALARM_INVERTER_LOW_LEVEL_TEST_TIMEOUT      136
#define BCMXCP_ALARM_OUTPUT_BREAKER_OPEN                  137
#define BCMXCP_ALARM_CONTROL_POWER_ON                     138
#define BCMXCP_ALARM_INVERTER_ON                          139
#define BCMXCP_ALARM_CHARGER_ON                           140
#define BCMXCP_ALARM_BYPASS_ON                            141
#define BCMXCP_ALARM_BYPASS_POWER_LOSS                    142
#define BCMXCP_ALARM_ON_MANUAL_BYPASS                     143
#define BCMXCP_ALARM_BYPASS_MANUAL_TURN_OFF               144
#define BCMXCP_ALARM_INVERTER_BLEEDING_DC_LINK_VOLT       145
#define BCMXCP_ALARM_CPU_ISR_ERROR                        146
#define BCMXCP_ALARM_SYSTEM_ISR_RESTART                   147
#define BCMXCP_ALARM_PARALLEL_DC                          148
#define BCMXCP_ALARM_BATTERY_NEEDS_SERVICE                149
#define BCMXCP_ALARM_BATTERY_CHARGING                     150
#define BCMXCP_ALARM_BATTERY_NOT_CHARGED                  151
#define BCMXCP_ALARM_DISABLED_BATTERY_TIME                152
#define BCMXCP_ALARM_SERIES_7000_ENABLE                   153
#define BCMXCP_ALARM_OTHER_UPS_ON                         154
#define BCMXCP_ALARM_PARALLEL_INVERTER                    155
#define BCMXCP_ALARM_UPS_IN_PARALLEL                      156
#define BCMXCP_ALARM_OUTPUT_BREAKER_REALY_FAIL            157
#define BCMXCP_ALARM_CONTROL_POWER_OFF                    158
#define BCMXCP_ALARM_LEVEL_2_OVERLOAD_PHASE_A             159
#define BCMXCP_ALARM_LEVEL_2_OVERLOAD_PHASE_B             160
#define BCMXCP_ALARM_LEVEL_2_OVERLOAD_PHASE_C             161
#define BCMXCP_ALARM_LEVEL_3_OVERLOAD_PHASE_A             162
#define BCMXCP_ALARM_LEVEL_3_OVERLOAD_PHASE_B             163
#define BCMXCP_ALARM_LEVEL_3_OVERLOAD_PHASE_C             164
#define BCMXCP_ALARM_LEVEL_4_OVERLOAD_PHASE_A             165
#define BCMXCP_ALARM_LEVEL_4_OVERLOAD_PHASE_B             166
#define BCMXCP_ALARM_LEVEL_4_OVERLOAD_PHASE_C             167
#define BCMXCP_ALARM_UPS_ON_BATTERY                       168
#define BCMXCP_ALARM_UPS_ON_BYPASS                        169
#define BCMXCP_ALARM_LOAD_DUMPED                          170
#define BCMXCP_ALARM_LOAD_ON_INVERTER                     171
#define BCMXCP_ALARM_UPS_ON_COMMAND                       172
#define BCMXCP_ALARM_UPS_OFF_COMMAND                      173
#define BCMXCP_ALARM_LOW_BATTERY_SHUTDOWN                 174
#define BCMXCP_ALARM_AUTO_ON_ENABLED                      175
#define BCMXCP_ALARM_SOFTWARE_INCOMPABILITY_DETECTED      176
#define BCMXCP_ALARM_INVERTER_TEMP_SENSOR_FAILED          177
#define BCMXCP_ALARM_DC_START_OCCURED                     178
#define BCMXCP_ALARM_IN_PARALLEL_OPERATION                179
#define BCMXCP_ALARM_SYNCING_TO_BYPASS                    180
#define BCMXCP_ALARM_RAMPING_UPS_UP                       181
#define BCMXCP_ALARM_INVERTER_ON_DELAY                    182
#define BCMXCP_ALARM_CHARGER_ON_DELAY                     183
#define BCMXCP_ALARM_WAITING_FOR_UTIL_INPUT               184
#define BCMXCP_ALARM_CLOSE_BYPASS_BREAKER                 185
#define BCMXCP_ALARM_TEMPORARY_BYPASS_OPERATION           186
#define BCMXCP_ALARM_SYNCING_TO_OUTPUT                    187
#define BCMXCP_ALARM_BYPASS_FAILURE                       188
#define BCMXCP_ALARM_AUTO_OFF_COMMAND_EXECUTED            189
#define BCMXCP_ALARM_AUTO_ON_COMMAND_EXECUTED             190
#define BCMXCP_ALARM_BATTERY_TEST_FAILED                  191
#define BCMXCP_ALARM_FUSE_FAIL                            192
#define BCMXCP_ALARM_FAN_FAIL                             193
#define BCMXCP_ALARM_SITE_WIRING_FAULT                    194
#define BCMXCP_ALARM_BACKFEED_CONTACTOR_FAIL              195
#define BCMXCP_ALARM_ON_BUCK                              196
#define BCMXCP_ALARM_ON_BOOST                             197
#define BCMXCP_ALARM_ON_DOUBLE_BOOST                      198
#define BCMXCP_ALARM_BATTERIES_DISCONNECTED               199
#define BCMXCP_ALARM_UPS_CABINET_OVER_TEMP                200
#define BCMXCP_ALARM_TRANSFORMER_OVER_TEMP                201
#define BCMXCP_ALARM_AMBIENT_UNDER_TEMP                   202
#define BCMXCP_ALARM_AMBIENT_OVER_TEMP                    203
#define BCMXCP_ALARM_CABINET_DOOR_OPEN                    204
#define BCMXCP_ALARM_CABINET_DOOR_OPEN_VOLT_PRESENT       205
#define BCMXCP_ALARM_AUTO_SHUTDOWN_PENDING                206
#define BCMXCP_ALARM_TAP_SWITCHING_REALY_PENDING          207
#define BCMXCP_ALARM_UNABLE_TO_CHARGE_BATTERIES           208
#define BCMXCP_ALARM_STARTUP_FAILURE_CHECK_EPO            209
#define BCMXCP_ALARM_AUTOMATIC_STARTUP_PENDING            210
#define BCMXCP_ALARM_MODEM_FAILED                         211
#define BCMXCP_ALARM_INCOMING_MODEM_CALL_STARTED          212
#define BCMXCP_ALARM_OUTGOING_MODEM_CALL_STARTED          213
#define BCMXCP_ALARM_MODEM_CONNECTION_ESTABLISHED         214
#define BCMXCP_ALARM_MODEM_CALL_COMPLETED_SUCCESS         215
#define BCMXCP_ALARM_MODEM_CALL_COMPLETED_FAIL            216
#define BCMXCP_ALARM_INPUT_BREAKER_FAIL                   217
#define BCMXCP_ALARM_SYSINIT_IN_PROGRESS                  218
#define BCMXCP_ALARM_AUTOCALIBRATION_FAIL                 219
#define BCMXCP_ALARM_SELECTIVE_TRIP_OF_MODULE             220
#define BCMXCP_ALARM_INVERTER_OUTPUT_FAILURE              221
#define BCMXCP_ALARM_ABNORMAL_OUTPUT_VOLT_AT_STARTUP      222
#define BCMXCP_ALARM_RECTIFIER_OVER_TEMP                  223
#define BCMXCP_ALARM_CONFIG_ERROR                         224
#define BCMXCP_ALARM_REDUNDANCY_LOSS_DUE_TO_OVERLOAD      225
#define BCMXCP_ALARM_ON_ALTERNATE_AC_SOURCE               226
#define BCMXCP_ALARM_IN_HIGH_EFFICIENCY_MODE              227
#define BCMXCP_ALARM_SYSTEM_NOTICE_ACTIVE                 228
#define BCMXCP_ALARM_SYSTEM_ALARM_ACTIVE                  229
#define BCMXCP_ALARM_ALTERNATE_POWER_SOURCE_NOT_AVAILABLE 230
#define BCMXCP_ALARM_CURRENT_BALANCE_FAILURE              231
#define BCMXCP_ALARM_CHECK_AIR_FILTER                     232
#define BCMXCP_ALARM_SUBSYSTEM_NOTICE_ACTIVE              233
#define BCMXCP_ALARM_SUBSYSTEM_ALARM_ACTIVE               234
#define BCMXCP_ALARM_CHARGER_ON_COMMAND                   235
#define BCMXCP_ALARM_CHARGER_OFF_COMMAND                  236
#define BCMXCP_ALARM_UPS_NORMAL                           237
#define BCMXCP_ALARM_INVERTER_PHASE_ROTATION              238
#define BCMXCP_ALARM_UPS_OFF                              239
#define BCMXCP_ALARM_EXTERNAL_COMMUNICATION_FAILURE       240
#define BCMXCP_ALARM_BATTERY_TEST_INPROGRESS              256
#define BCMXCP_ALARM_SYSTEM_TEST_INPROGRESS               257
#define BCMXCP_ALARM_BATTERY_TEST_ABORTED                 258

#define BCMXCP_COMMAND_MAP_MAX 208 /* Max no of entries in BCM/XCP meter map (adjusted upwards to nearest multi of 8) */
#define BCMXCP_METER_MAP_MAX   136 /* Max no of entries in BCM/XCP meter map (adjusted upwards to nearest multi of 8) */
#define BCMXCP_ALARM_MAP_MAX   264 /* Max no of entries in BCM/XCP alarm map (adjusted upwards to nearest multi of 8) */

/* Return codes for XCP ACK block responses */
#define BCMXCP_RETURN_ACCEPTED                  0x31 /* Accepted and executed (or execution in progress) */
#define BCMXCP_RETURN_NOT_IMPLEMENTED           0x32 /* Recognized but not implemented */
#define BCMXCP_RETURN_BUSY                      0x33 /* Recognized but not currently able to execute (busy) */
#define BCMXCP_RETURN_UNRECOGNISED              0x34 /* Unrecognized -- e.g., command byte not in valid range, or command has been corrupted (bad checksum) */
#define BCMXCP_RETURN_PARAMETER_OUT_OF_RANGE    0x35 /* Command recognized, but its Parameter value is out of range */
#define BCMXCP_RETURN_INVALID_PARAMETER         0x36 /* Command recognized, but its Parameter is invalid (e.g., no such parameter, bad Outlet number) */
#define BCMXCP_RETURN_ACCEPTED_PARAMETER_ADJUST 0x37 /* Accepted, with parameter adjusted to nearest good value */
/*#define BCMXCP_RETURN_READONLY                0x38 */ /* Parameter is Read-only - cannot be written (at this privilege level) (this is not listed in spec document */

/* UPS status */
#define BCMXCP_STATUS_ONLINE    0x50
#define BCMXCP_STATUS_ONBATTERY 0xf0
#define BCMXCP_STATUS_OVERLOAD  0xe0
#define BCMXCP_STATUS_TRIM      0x63
#define BCMXCP_STATUS_BOOST1    0x61
#define BCMXCP_STATUS_BOOST2    0x62
#define BCMXCP_STATUS_BYPASS    0x60
#define BCMXCP_STATUS_OFF       0x10

/* UPS topology block info */
#define BCMXCP_TOPOLOGY_NONE                        0x0000 /* None; use the Table of Elements */
#define BCMXCP_TOPOLOGY_OFFLINE_SWITCHER_1P         0x0010 /* Off-line switcher, Single Phase */
#define BCMXCP_TOPOLOGY_LINEINT_UPS_1P              0x0020 /* Line-Interactive UPS, Single Phase */
#define BCMXCP_TOPOLOGY_LINEINT_UPS_2P              0x0021 /* Line-Interactive UPS, Two Phase */
#define BCMXCP_TOPOLOGY_LINEINT_UPS_3P              0x0022 /* Line-Interactive UPS, Three Phase */
#define BCMXCP_TOPOLOGY_DUAL_AC_ONLINE_UPS_1P       0x0030 /* Dual AC Input, On-Line UPS, Single Phase */
#define BCMXCP_TOPOLOGY_DUAL_AC_ONLINE_UPS_2P       0x0031 /* Dual AC Input, On-Line UPS, Two Phase */
#define BCMXCP_TOPOLOGY_DUAL_AC_ONLINE_UPS_3P       0x0032 /* Dual AC Input, On-Line UPS, Three Phase */
#define BCMXCP_TOPOLOGY_ONLINE_UPS_1P               0x0040 /* On-Line UPS, Single Phase */
#define BCMXCP_TOPOLOGY_ONLINE_UPS_2P               0x0041 /* On-Line UPS, Two Phase */
#define BCMXCP_TOPOLOGY_ONLINE_UPS_3P               0x0042 /* On-Line UPS, Three Phase */
#define BCMXCP_TOPOLOGY_PARA_REDUND_ONLINE_UPS_1P   0x0050 /* Parallel Redundant On-Line UPS, Single Phase */
#define BCMXCP_TOPOLOGY_PARA_REDUND_ONLINE_UPS_2P   0x0051 /* Parallel Redundant On-Line UPS, Two Phase */
#define BCMXCP_TOPOLOGY_PARA_REDUND_ONLINE_UPS_3P   0x0052 /* Parallel Redundant On-Line UPS, Three Phase */
#define BCMXCP_TOPOLOGY_PARA_CAPACITY_ONLINE_UPS_1P 0x0060 /* Parallel for Capacity On-Line UPS, Single Phase */
#define BCMXCP_TOPOLOGY_PARA_CAPACITY_ONLINE_UPS_2P 0x0061 /* Parallel for Capacity On-Line UPS, Two Phase */
#define BCMXCP_TOPOLOGY_PARA_CAPACITY_ONLINE_UPS_3P 0x0062 /* Parallel for Capacity On-Line UPS, Three Phase */
#define BCMXCP_TOPOLOGY_SYSTEM_BYPASS_MODULE_3P     0x0102 /* System Bypass Module, Three Phase */
#define BCMXCP_TOPOLOGY_HOT_TIE_CABINET_3P          0x0122 /* Hot-Tie Cabinet, Three Phase */
#define BCMXCP_TOPOLOGY_OUTLET_CONTROLLER_1P        0x0200 /* Outlet Controller, Single Phase */
#define BCMXCP_TOPOLOGY_DUAL_AC_STATIC_SWITCH_3P    0x0222 /* Dual AC Input Static Switch Module, 3 Phase */

typedef struct { /* Entry in BCM/XCP - UPS mapping table */
	const char *command_desc;   /* Description of this command */
	unsigned char command_byte; /* The command byte for this command, 0 = not supported */
} BCMXCP_COMMAND_MAP_ENTRY_t;

extern BCMXCP_COMMAND_MAP_ENTRY_t bcmxcp_command_map[BCMXCP_COMMAND_MAP_MAX];

typedef struct { /* Entry in BCM/XCP - UPS - NUT mapping table */
	const char *nut_entity;         /* The NUT variable name */
	unsigned char format;           /* The format of the data - float, long etc */
	unsigned int meter_block_index; /* The position of this meter in the UPS meter block */
} BCMXCP_METER_MAP_ENTRY_t;

extern BCMXCP_METER_MAP_ENTRY_t bcmxcp_meter_map[BCMXCP_METER_MAP_MAX];

typedef struct { /* Entry in BCM/XCP - UPS mapping table */
	int alarm_block_index;  /* Index of this alarm in alarm block. -1 = not existing */
	const char *alarm_desc; /* Description of this alarm */
} BCMXCP_ALARM_MAP_ENTRY_t;

extern BCMXCP_ALARM_MAP_ENTRY_t bcmxcp_alarm_map[BCMXCP_ALARM_MAP_MAX];

typedef struct { /* A place to store status info and other data not for NUT */
	unsigned char topology_mask; /* Configuration block byte 16, masks valid status bits */
	unsigned int lowbatt;        /* Seconds of runtime left left when LB alarm is set */
	unsigned int shutdowndelay;  /* Shutdown delay in seconds, from ups.conf */
	int alarm_on_battery;        /* On Battery alarm active? */
	int alarm_low_battery;       /* Battery Low alarm active? */
	int alarm_replace_battery;   /* Battery needs replacement! */
} BCMXCP_STATUS_t;

extern BCMXCP_STATUS_t bcmxcp_status;

int checksum_test(const unsigned char*);
unsigned char calc_checksum(const unsigned char *buf);

/* from usbhid-ups.h */
typedef struct {
	const long xcp_value;  /* XCP value */
	const char *nut_value; /* NUT value */
	const char *(*fun)(double xcp_value); /* optional XCP to NUT mapping */
	double (*nuf)(const char *nut_value); /* optional NUT to HID mapping */
} info_lkp_t;

/* use explicit booleans */
#ifndef FALSE
typedef enum ebool { FALSE, TRUE } bool_t;
#else
typedef int bool_t;
#endif

#endif /*_POWERWARE_H */

