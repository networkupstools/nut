/*  phoenixcontact_modbus.c - Driver for PhoenixContact-QUINT UPS
 *
 *  Copyright (C)
 *    2017  Spiros Ioannou <sivann@inaccess.com>
 *    2024  Ricardo Rodriguez <rikyrod2001@gmail.com>
 *    2025  Ulfat Hasangarayev <ulfathasangarayev@gmail.com>
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
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "main.h"
#include <modbus.h>
#include "nut_stdint.h"
#include <stdbool.h>

#if !(defined NUT_MODBUS_LINKTYPE_STR)
# define NUT_MODBUS_LINKTYPE_STR	"unknown"
#endif

#define DRIVER_NAME	"NUT PhoenixContact Modbus driver (libmodbus link type: " NUT_MODBUS_LINKTYPE_STR ")"
#define DRIVER_VERSION	"0.09"

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))
#define MODBUS_SLAVE_ID 192

#define QUINT_5A_UPS_1_3AH_BATTERY_PARTNUMBER 2320254
#define QUINT_5A_UPS_1_3AH_BATTERY_DESCRIPTION "QUINT-UPS/24DC/24DC/5/1.3AH"

#define QUINT_10A_UPS_PARTNUMBER 2320225
#define QUINT_10A_UPS_DESCRIPTION "QUINT-UPS/24DC/24DC/10"

#define QUINT_20A_UPS_PARTNUMBER 2320238
#define QUINT_20A_UPS_DESCRIPTION "QUINT-UPS/24DC/24DC/20"

#define QUINT_40A_UPS_PARTNUMBER 2320241
#define QUINT_40A_UPS_DESCRIPTION "QUINT-UPS/24DC/24DC/40"

#define QUINT4_10A_UPS_PARTNUMBER 2907067
#define QUINT4_10A_UPS_DESCRIPTION "QUINT4-UPS/24DC/24DC/10/USB"

#define QUINT4_20A_UPS_PARTNUMBER 2907072
#define QUINT4_20A_UPS_DESCRIPTION "QUINT4-UPS/24DC/24DC/20/USB"

#define QUINT4_40A_UPS_PARTNUMBER 2907078
#define QUINT4_40A_UPS_DESCRIPTION "QUINT4-UPS/24DC/24DC/40/USB"

#define TRIO_5A_UPS_PARTNUMBER 2866611
#define TRIO_5A_UPS_DESCRIPTION "TRIO-UPS/1AC/24DC/5"

#define TRIO_2G_5A_UPS_PARTNUMBER 2907160
#define TRIO_2G_5A_UPS_DESCRIPTION "TRIO-UPS-2G/1AC/24DC/5"

#define TRIO_2G_10A_UPS_PARTNUMBER 2907161
#define TRIO_2G_10A_UPS_DESCRIPTION "TRIO-UPS-2G/1AC/24DC/10"

#define TRIO_2G_20A_UPS_PARTNUMBER 1105556
#define TRIO_2G_20A_UPS_DESCRIPTION "TRIO-UPS-2G/1AC/24DC/20"


/* --- Shutdown & Mode Configuration --- */

/* Macro for fetching a uint16_t config value with fallback*/
#define GETVAL_U16(name, fallback) (getval(name) ? (uint16_t)atoi(getval(name)) : (fallback))

/* Time [s] after entering battery mode before shutdown signal (bit 15) is triggered */
#define REG_PC_SHUTDOWN_DELAY        0x105A  /*default: 60s, range: 1–65535*/

/*Time [s] allowed for PC to shut down before output turns off*/
#define REG_PC_SHUTDOWN_TIME         0x105D  /*default: 120s, range: 1–3600*/

/*Time [s] output is off after PC shutdown before reboot*/
#define REG_PC_RESET_TIME            0x105E  /*default: 10s, range: 0–60*/

/*Time [s] input voltage must be above threshold to return to mains*/
#define REG_MAINS_RETURN_DELAY       0x1058  /*default: 10s*/

/*Selector mode switch: PC-Mode = 9*/
#define REG_MODE_SELECTOR_SWITCH     0x1074  /*default: 0–9*/

/* --- Battery Monitoring --- */

/*SOH warning threshold [%]*/
#define REG_WARNING_SOH_THRESHOLD    0x1071  /*default: 0 (disabled), range: 1–100*/

/*Switch to battery mode if below this input voltage [mV]*/
#define REG_VOLTAGE_BELOW_BATTERY    0x1056  /*example: 21000*/

/*Switch to mains mode if above this voltage [mV]*/
#define REG_VOLTAGE_ABOVE_MAINS      0x1057  /*example: 23000*/

typedef enum
{
	NONE,
	QUINT_UPS,
	QUINT4_UPS,
	TRIO_UPS,
	TRIO_2G_UPS
} models;

/* Variables */
static modbus_t *modbus_ctx = NULL;
static int errcount = 0;

static int mrir(modbus_t * arg_ctx, int addr, int nb, uint16_t * dest);

static models UPSModel = NONE;

typedef struct {
	const char *nut_name;
	uint16_t reg_addr;
} delay_param_t;

static const delay_param_t delay_params[] = {
	{ "ups.delay.shutdown",		REG_PC_SHUTDOWN_DELAY},
	{ "ups.timer.shutdown",		REG_PC_SHUTDOWN_TIME},
	{ "ups.timer.start",		REG_PC_RESET_TIME},
	{ "ups.delay.start",		REG_MAINS_RETURN_DELAY},
	{ "ups.mode.selector",		REG_MODE_SELECTOR_SWITCH},
	{ "battery.warning_soh",	REG_WARNING_SOH_THRESHOLD},
	{ "input.voltage.low.critical",	REG_VOLTAGE_BELOW_BATTERY},
	{ "input.voltage.high.critical",	REG_VOLTAGE_ABOVE_MAINS }
};


/*
	For the QUINT ups (first implementation of the driver) the modbus addresses
	are reported in dec format,for the QUINT4 ups they are reported in hex format.
	The difference is caused from the way they are reported in the datasheet,
	keeping them in the same format as the datasheet make more simple the maintenence
	of the driver avoiding conversions while coding.
*/

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Spiros Ioannou <sivann@inaccess.com>\n"
	"Ricardo Rodriguez <rikyrod2001@gmail.com>\n"
	"Ulfat Hasangarayev <ulfathasangarayev@gmail.com>",
	DRV_BETA,
	{NULL}
};

int write_uint32_register(modbus_t *ctx, int reg, uint32_t value)
{
	uint16_t regs[2];
	regs[0] = value >> 16;      /*High word*/
	regs[1] = value & 0xFFFF;   /*Low word*/

	int ret = modbus_write_registers(ctx, reg, 2, regs);
	if (ret == -1) {
		upslogx(LOG_ERR, "Failed to write 32-bit value to reg 0x%04X: %s", reg, modbus_strerror(errno));
	}
	return ret;
}


int write_uint32_reg_bit(modbus_t *ctx, int reg, int bit_index, bool bit_value)
{
	uint16_t regs[2];
	uint32_t val;

	if (modbus_read_registers(ctx, reg, 2, regs) != 2) {
		upslogx(LOG_ERR, "Failed to read 32-bit register 0x%04X: %s", reg, modbus_strerror(errno));
		return -1;
	}

	val = ((uint32_t)regs[0] << 16) | regs[1];

	if (bit_value)
		val |= (1U << bit_index);
	else
		val &= ~(1U << bit_index);

	regs[0] = val >> 16;
	regs[1] = val & 0xFFFF;

	if (modbus_write_registers(ctx, reg, 2, regs) == -1) {
		upslogx(LOG_ERR, "Failed to write modified 32-bit value to 0x%04X: %s", reg, modbus_strerror(errno));
		return -1;
	}
	return 0;
}

static void phoenixcontact_apply_advanced_config(modbus_t *ctx)
{
	upslogx(LOG_NOTICE, "Applying advanced Phoenix Contact configuration...");

	write_uint32_reg_bit(ctx, 0x1040, 5, false);

	modbus_write_register(ctx, REG_PC_SHUTDOWN_DELAY,      GETVAL_U16("delay.shutdown",                    60));
	modbus_write_register(ctx, REG_PC_SHUTDOWN_TIME,       GETVAL_U16("timer.shutdown",                    60));
	modbus_write_register(ctx, REG_PC_RESET_TIME,          GETVAL_U16("timer.start",                       5));
	modbus_write_register(ctx, REG_WARNING_SOH_THRESHOLD,  GETVAL_U16("battery.warning_soh",               20));
	modbus_write_register(ctx, REG_MODE_SELECTOR_SWITCH,   GETVAL_U16("mode.selector",                     9));
	modbus_write_register(ctx, REG_VOLTAGE_BELOW_BATTERY,  GETVAL_U16("voltage.low.critical",              21000));
	modbus_write_register(ctx, REG_VOLTAGE_ABOVE_MAINS,    GETVAL_U16("voltage.high.critical",             29000));
	modbus_write_register(ctx, REG_MAINS_RETURN_DELAY,     GETVAL_U16("delay.start",                       10));

	/* the value 0xFFFDFFFF sets bit 17 low so that the mode selector switch is overwritten in software */
	write_uint32_register(ctx, 0x1076, 0xFFFDFFFF);
}


void upsdrv_initinfo(void)
{
	uint16_t tab_reg[4];
	uint64_t PartNumber = 0;
	size_t i;

	for (i = 0; i < (sizeof(tab_reg) / sizeof(tab_reg[0])); i++)
	{
		tab_reg[i] = 0;
	}

	upsdebugx(2, "upsdrv_initinfo");

	dstate_setinfo("device.mfr", "Phoenix Contact");

	/* upsh.instcmd = instcmd; */
	/* upsh.setvar = setvar; */

	mrir(modbus_ctx, 0x0005, 4, tab_reg);

	/*
	 * Method provided from Phoenix Conatct to establish the UPS model:
	 * Read registers from 0x0005 to 0x0008 and "concatenate" them with the order
	 * 0x0008 0x0007 0x0006 0x0005 in hex form, convert the obtained number from hex to dec.
	 * The first 7 most significant digits of the number in dec form are the part number of
	 * the UPS.
	 */

	PartNumber = (tab_reg[3] << 16) + tab_reg[2];
	PartNumber = (PartNumber << 16) + tab_reg[1];
	PartNumber = (PartNumber << 16) + tab_reg[0];

	while (PartNumber > 10000000)
	{
		PartNumber /= 10;
	}

	switch (PartNumber)
	{
	case QUINT_5A_UPS_1_3AH_BATTERY_PARTNUMBER:
		UPSModel = QUINT_UPS;
		dstate_setinfo("device.model", QUINT_5A_UPS_1_3AH_BATTERY_DESCRIPTION);
		break;
	case QUINT_10A_UPS_PARTNUMBER:
		UPSModel = QUINT_UPS;
		dstate_setinfo("device.model", QUINT_10A_UPS_DESCRIPTION);
		break;
	case QUINT_20A_UPS_PARTNUMBER:
		UPSModel = QUINT_UPS;
		dstate_setinfo("device.model", QUINT_20A_UPS_DESCRIPTION);
		break;
	case QUINT_40A_UPS_PARTNUMBER:
		UPSModel = QUINT_UPS;
		dstate_setinfo("device.model", QUINT_40A_UPS_DESCRIPTION);
		break;
	case QUINT4_10A_UPS_PARTNUMBER:
		UPSModel = QUINT4_UPS;
		dstate_setinfo("device.model", QUINT4_10A_UPS_DESCRIPTION);
		break;
	case QUINT4_20A_UPS_PARTNUMBER:
		UPSModel = QUINT4_UPS;
		dstate_setinfo("device.model", QUINT4_20A_UPS_DESCRIPTION);
		break;
	case QUINT4_40A_UPS_PARTNUMBER:
		UPSModel = QUINT4_UPS;
		dstate_setinfo("device.model", QUINT4_40A_UPS_DESCRIPTION);
		break;
	case TRIO_5A_UPS_PARTNUMBER:
		UPSModel = TRIO_UPS;
		dstate_setinfo("device.model", TRIO_5A_UPS_DESCRIPTION);
		break;
	case TRIO_2G_5A_UPS_PARTNUMBER:
		UPSModel = TRIO_2G_UPS;
		dstate_setinfo("device.model", TRIO_2G_5A_UPS_DESCRIPTION);
		break;
	case TRIO_2G_10A_UPS_PARTNUMBER:
		UPSModel = TRIO_2G_UPS;
		dstate_setinfo("device.model", TRIO_2G_10A_UPS_DESCRIPTION);
		break;
	case TRIO_2G_20A_UPS_PARTNUMBER:
		UPSModel = TRIO_2G_UPS;
		dstate_setinfo("device.model", TRIO_2G_20A_UPS_DESCRIPTION);
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
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
# pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
		/* All enum cases defined as of the time of coding
		 * have been covered above. Handle later definitions,
		 * memory corruptions and buggy inputs below...
		 */
	default:
		fatalx(EXIT_FAILURE, "Unknown UPS part number: %" PRIu64, PartNumber);
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
	}
}

void upsdrv_updateinfo(void)
{
	uint16_t tab_reg[64];
	uint16_t actual_code_functions;
	uint16_t actual_code_functions1;
	uint16_t actual_code_functions2;
	uint16_t actual_alarms = 0;
	uint16_t actual_alarms1 = 0;
	uint16_t actual_alarms2 = 0;
	uint16_t battery_voltage;
	uint16_t battery_temperature;
	uint16_t battery_runtime;
	uint16_t battery_capacity;
	uint16_t output_current;
	uint16_t value = 0;
	size_t i;

	errcount = 0;

	upsdebugx(2, "upsdrv_updateinfo");

	switch (UPSModel)
	{
	case QUINT4_UPS:
		for (i = 0; i < sizeof(delay_params) / sizeof(delay_params[0]); i++) {
			if (modbus_read_registers(modbus_ctx, delay_params[i].reg_addr, 1, &value) != -1) {
				dstate_setinfo(delay_params[i].nut_name, "%d", value);
			} else {
				upslogx(LOG_WARNING, "Failed to read %s (0x%04X): %s",
						delay_params[i].nut_name,
						delay_params[i].reg_addr,
						modbus_strerror(errno));
			}
		}

		mrir(modbus_ctx, 0x2000, 1, &actual_code_functions);

		tab_reg[0] = CHECK_BIT(actual_code_functions, 2); /* Battery mode is the 2nd bit of the register 0x2000 */
		tab_reg[2] = CHECK_BIT(actual_code_functions, 5); /* Battery charging is the 6th bit of the register 0x2000 */

		mrir(modbus_ctx, 0x3001, 1, &actual_alarms1);

		tab_reg[1] = CHECK_BIT(actual_alarms1, 2); /* Battery discharged is the 2nd bit of the register 0x3001 */
		break;
	case TRIO_UPS:
	case QUINT_UPS:
		mrir(modbus_ctx, 29697, 3, tab_reg); /* LB is actually called "shutdown event" on this ups */
		break;
	case TRIO_2G_UPS:
		mrir(modbus_ctx, 0x2000, 1, &actual_code_functions);
		mrir(modbus_ctx, 0x2002, 1, &actual_code_functions1);
		mrir(modbus_ctx, 0x3001, 1, &actual_code_functions2);

		tab_reg[0] = CHECK_BIT(actual_code_functions1, 2);
		tab_reg[2] = CHECK_BIT(actual_code_functions, 5);
		tab_reg[1] = CHECK_BIT(actual_code_functions2, 2);
		break;
	case NONE:
		fatalx(EXIT_FAILURE, "Unknown UPS model.");
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
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
# pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
		/* All enum cases defined as of the time of coding
		 * have been covered above. Handle later definitions,
		 * memory corruptions and buggy inputs below...
		 */
	default:
		fatalx(EXIT_FAILURE, "Unknown UPS model.");
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
	}

	status_init();

	if (tab_reg[0])
		status_set("OB");
	else
		status_set("OL");

	if (tab_reg[2]) {
		status_set("CHRG");
	}

	if (tab_reg[1]) {
		status_set("LB");
	}

	switch (UPSModel)
	{
	case TRIO_2G_UPS:
	case QUINT4_UPS:
		mrir(modbus_ctx, 0x2006, 1, tab_reg);
		break;
	case QUINT_UPS:
		mrir(modbus_ctx, 29745, 1, tab_reg);
		break;
	case TRIO_UPS:
		mrir(modbus_ctx, 29702, 1, tab_reg);
		break;
	case NONE:
		fatalx(EXIT_FAILURE, "Unknown UPS model.");
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
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
# pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
		/* All enum cases defined as of the time of coding
		 * have been covered above. Handle later definitions,
		 * memory corruptions and buggy inputs below...
		 */
	default:
		fatalx(EXIT_FAILURE, "Unknown UPS model.");
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
	}

	dstate_setinfo("output.voltage", "%d", (int) (tab_reg[0] / 1000));

	switch (UPSModel)
	{
	case QUINT4_UPS:
		mrir(modbus_ctx, 0x200F, 1, tab_reg);
		break;
	case QUINT_UPS:
		mrir(modbus_ctx, 29749, 5, tab_reg);
		break;
	case TRIO_UPS:
		/*battery.charge is not available for TRIO and TRIO-2G models*/
		break;
	case TRIO_2G_UPS:
		/*battery.charge is not available for TRIO and TRIO-2G models*/
		break;
	case NONE:
		fatalx(EXIT_FAILURE, "Unknown UPS model.");
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
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
# pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
		/* All enum cases defined as of the time of coding
		 * have been covered above. Handle later definitions,
		 * memory corruptions and buggy inputs below...
		 */
	default:
		fatalx(EXIT_FAILURE, "Unknown UPS model.");
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
	}
	if (UPSModel != TRIO_2G_UPS && UPSModel != TRIO_UPS)
		dstate_setinfo("battery.charge", "%d", tab_reg[0]);
	/* dstate_setinfo("battery.runtime", tab_reg[1]*60); */ /* also reported on this address, but less accurately */

	switch (UPSModel)
	{
	case QUINT4_UPS:
		mrir(modbus_ctx, 0x200A, 1, &battery_voltage);
		tab_reg[0] = battery_voltage;

		mrir(modbus_ctx, 0x200D, 1, &battery_temperature);
		tab_reg[1] = battery_temperature;

		mrir(modbus_ctx, 0x2010, 1, &battery_runtime);
		tab_reg[3] = battery_runtime;

		mrir(modbus_ctx, 0x4211, 1, &battery_capacity);
		tab_reg[8] = battery_capacity;

		mrir(modbus_ctx, 0x2007, 1, &output_current);
		tab_reg[6] = output_current;

		break;
	case QUINT_UPS:
		mrir(modbus_ctx, 29792, 10, tab_reg);
		break;
	case TRIO_UPS:
		mrir(modbus_ctx, 29700, 1, &battery_voltage);
		tab_reg[0] = battery_voltage;

		/*output.current variable is not available for TRIO model*/
		break;
	case TRIO_2G_UPS:
		/*battery.voltage variable is not available for TRIO-2G model*/

		/*battery.temperature variable is not available for TRIO and TRIO-2G models*/

		/*battery.runtime variable is not available for TRIO and TRIO-2G models*/

		/*battery.capacity variable is not available for TRIO and TRIO-2G models*/

		mrir(modbus_ctx, 0x2007, 1, &output_current);
		tab_reg[6] = output_current;
		break;
	case NONE:
		fatalx(EXIT_FAILURE, "Unknown UPS model.");
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
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
# pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
		/* All enum cases defined as of the time of coding
		 * have been covered above. Handle later definitions,
		 * memory corruptions and buggy inputs below...
		 */
	default:
		fatalx(EXIT_FAILURE, "Unknown UPS model.");
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
	}

	if (UPSModel != TRIO_2G_UPS)
	{
		dstate_setinfo("battery.voltage", "%f", (double) (tab_reg[0]) / 1000.0);
		if (UPSModel != TRIO_UPS)
		{
			dstate_setinfo("battery.capacity", "%d", tab_reg[8] * 10);
			dstate_setinfo("battery.temperature", "%d", tab_reg[1] - 273);
			dstate_setinfo("battery.runtime", "%d", tab_reg[3]);
		}
	}

	if (UPSModel != TRIO_UPS)
	{
		dstate_setinfo("output.current", "%f", (double) (tab_reg[6]) / 1000.0);
	}


	/* ALARMS */

	alarm_init();

	switch (UPSModel)
	{
	case QUINT4_UPS:
		tab_reg[0] = 0;
		actual_alarms = 0;
		actual_alarms1 = 0;

		mrir(modbus_ctx, 0x3000, 1, &actual_alarms);
		mrir(modbus_ctx, 0x3001, 1, &actual_alarms1);

		if (CHECK_BIT(actual_alarms, 9) && CHECK_BIT(actual_alarms, 9))
			alarm_set("End of life (Resistance)");

		if (CHECK_BIT(actual_alarms1, 0))
			alarm_set("End of life (Time)");

		if (CHECK_BIT(actual_alarms, 10))
			alarm_set("End of life (Voltage)");

		if (CHECK_BIT(actual_alarms, 3))
			alarm_set("No Battery");

		if (CHECK_BIT(actual_alarms, 5))
			alarm_set("Inconsistent technology");

		if (CHECK_BIT(actual_alarms1, 8))
			alarm_set("Overload Cutoff");

		if (CHECK_BIT(actual_alarms1, 3))
			alarm_set("Low Battery (Voltage)");

		if (CHECK_BIT(actual_alarms1, 4))
			alarm_set("Low Battery (Charge)");

		if (CHECK_BIT(actual_alarms1, 5))
			alarm_set("Low Battery (Time)");

		if (CHECK_BIT(actual_alarms1, 14))
			alarm_set("Low Battery (Service)");

		break;
	case TRIO_2G_UPS:
		tab_reg[0] = 0;
		actual_alarms = 0;
		actual_alarms1 = 0;

		mrir(modbus_ctx, 0x3000, 1, &actual_alarms);
		mrir(modbus_ctx, 0x3001, 1, &actual_alarms1);
		mrir(modbus_ctx, 0x3012, 1, &actual_alarms2);

		if (CHECK_BIT(actual_alarms, 10))
			alarm_set("End of life (Voltage)");

		if (CHECK_BIT(actual_alarms, 3))
			alarm_set("No Battery");

		if (CHECK_BIT(actual_alarms1, 8))
			alarm_set("Overload Cutoff");

		if (CHECK_BIT(actual_alarms1, 17))
			alarm_set("Low Battery (Voltage)");

		if (CHECK_BIT(actual_alarms1, 14))
			alarm_set("Low Battery (Service)");

		break;
	case QUINT_UPS:
		mrir(modbus_ctx, 29840, 1, tab_reg);

		if (CHECK_BIT(tab_reg[0], 4) && CHECK_BIT(tab_reg[0], 5))
			alarm_set("End of life (Resistance)");
		if (CHECK_BIT(tab_reg[0], 6))
			alarm_set("End of life (Time)");
		if (CHECK_BIT(tab_reg[0], 7))
			alarm_set("End of life (Voltage)");
		if (CHECK_BIT(tab_reg[0], 9))
			alarm_set("No Battery");
		if (CHECK_BIT(tab_reg[0], 10))
			alarm_set("Inconsistent technology");
		if (CHECK_BIT(tab_reg[0], 11))
			alarm_set("Overload Cutoff");
		/* We don't use those low-battery indicators below.
		 * No info or configuration exists for those alarm low-bat
		 */
		if (CHECK_BIT(tab_reg[0], 12))
			alarm_set("Low Battery (Voltage)");
		if (CHECK_BIT(tab_reg[0], 13))
			alarm_set("Low Battery (Charge)");
		if (CHECK_BIT(tab_reg[0], 14))
			alarm_set("Low Battery (Time)");
		if (CHECK_BIT(tab_reg[0], 16))
			alarm_set("Low Battery (Service)");
		break;
	case TRIO_UPS:
		mrir(modbus_ctx, 29706, 1, tab_reg);

		if (CHECK_BIT(tab_reg[0], 0))
			alarm_set("End of life (Time)");
		if (CHECK_BIT(tab_reg[0], 2))
			alarm_set("End of life (Voltage)");
		if (CHECK_BIT(tab_reg[0], 1))
			alarm_set("No Battery");
		if (CHECK_BIT(tab_reg[0], 3))
			alarm_set("Low Battery (Voltage)");
		break;
	case NONE:
		fatalx(EXIT_FAILURE, "Unknown UPS model.");
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
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunreachable-code"
# pragma clang diagnostic ignored "-Wcovered-switch-default"
#endif
		/* All enum cases defined as of the time of coding
		 * have been covered above. Handle later definitions,
		 * memory corruptions and buggy inputs below...
		 */
	default:
		fatalx(EXIT_FAILURE, "Unknown UPS model.");
#ifdef __clang__
# pragma clang diagnostic pop
#endif
#if (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_PUSH_POP) && ( (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_COVERED_SWITCH_DEFAULT) || (defined HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE) )
# pragma GCC diagnostic pop
#endif
	}

	if (errcount == 0) {
		alarm_commit();
		status_commit();
		dstate_dataok();
	}
	else
		dstate_datastale();

}

void upsdrv_shutdown(void)
{
	/* Only implement "shutdown.default"; do not invoke
	 * general handling of other `sdcommands` here */

	/*
	 * WARNING: When using RTU TCP, this driver will probably
	 * never support shutdowns properly, except on some systems:
	 * In order to be of any use, the driver should be called
	 * near the end of the system halt script (or a service
	 * management framework's equivalent, if any). By that
	 * time we, in all likelyhood, won't have basic network
	 * capabilities anymore, so we could never send this
	 * command to the UPS. This is not an error, but rather
	 * a limitation (on some platforms) of the interface/media
	 * used for these devices.
	 */

	/* replace with a proper shutdown function */
	upslogx(LOG_ERR, "shutdown not supported");
	if (handling_upsdrv_shutdown > 0)
		set_exit_flag(EF_EXIT_FAILURE);
}

void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "delay.shutdown", "Delay before initiating PC shutdown (in seconds)");
	addvar(VAR_VALUE, "timer.shutdown", "Time allowed for PC to shutdown (in seconds)");
	addvar(VAR_VALUE, "timer.start", "Duration of output off before reboot (in seconds)");
	addvar(VAR_VALUE, "delay.start", "Delay before switching back to mains (in seconds)");
	addvar(VAR_VALUE, "mode.selector", "UPS operating mode (e.g. 9 = PC-Mode)");
	addvar(VAR_VALUE, "battery.warning_soh", "Battery warning SOH threshold (%)");
	addvar(VAR_VALUE, "voltage.low.critical", "Threshold [V] to switch to battery mode");
	addvar(VAR_VALUE, "voltage.high.critical", "Threshold [V] to return to mains mode");
}

void upsdrv_initups(void)
{
	int r, result;
	uint16_t FWVersion;
	upsdebugx(2, "upsdrv_initups");

	modbus_ctx = modbus_new_rtu(device_path, 115200, 'E', 8, 1);

	if (modbus_ctx == NULL)
		fatalx(EXIT_FAILURE, "Unable to create the libmodbus context");

	r = modbus_set_slave(modbus_ctx, MODBUS_SLAVE_ID);	/* slave ID */
	if (r < 0)
	{
		modbus_free(modbus_ctx);
		fatalx(EXIT_FAILURE, "Invalid modbus slave ID %d", MODBUS_SLAVE_ID);
	}

	if (modbus_connect(modbus_ctx) == -1)
	{
		modbus_free(modbus_ctx);
		fatalx(EXIT_FAILURE, "modbus_connect: unable to connect: %s", modbus_strerror(errno));
	}

	result = mrir(modbus_ctx, 0x0004, 1, &FWVersion);
	if (result == -1)
	{
		modbus_close(modbus_ctx);
		modbus_free(modbus_ctx);

		modbus_ctx = modbus_new_rtu(device_path, 19200, 'E', 8, 1);
		if (modbus_ctx == NULL)
			fatalx(EXIT_FAILURE, "Unable to create the libmodbus context");

		r = modbus_set_slave(modbus_ctx, MODBUS_SLAVE_ID);	/* slave ID */
		if (r < 0)
		{
			modbus_free(modbus_ctx);
			fatalx(EXIT_FAILURE, "Invalid modbus slave ID %d", MODBUS_SLAVE_ID);
		}

		if (modbus_connect(modbus_ctx) == -1)
		{
			modbus_free(modbus_ctx);
			fatalx(EXIT_FAILURE, "modbus_connect: unable to connect: %s", modbus_strerror(errno));
		}

		r = mrir(modbus_ctx, 0x0004, 1, &FWVersion);

		if (r < 0)
		{
			fatalx(EXIT_FAILURE, "UPS does not repond to read requests.");
		}

	}

	phoenixcontact_apply_advanced_config(modbus_ctx);
	dstate_setinfo("ups.firmware", "%" PRIu16, FWVersion);
}


void upsdrv_cleanup(void)
{
	if (modbus_ctx != NULL) {
		modbus_close(modbus_ctx);
		modbus_free(modbus_ctx);
	}
}

/* Modbus Read Input Registers */
static int mrir(modbus_t * arg_ctx, int addr, int nb, uint16_t * dest)
{
	int r;
	r = modbus_read_input_registers(arg_ctx, addr, nb, dest);
	if (r == -1) {
		upslogx(LOG_ERR, "mrir: modbus_read_input_registers(addr:%d, count:%d): %s (%s)", addr, nb, modbus_strerror(errno), device_path);
		errcount++;
	}
	return r;
}
