/*  must_ep2000pro.c - Driver for MUST EP2000 Pro UPS
 *
 *  Copyright (C)
 *    2025 Mikhail Mironov <mike@darkmike.ru>
 *
 *  Based on documentation received from manafacturer by email
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

#if !(defined NUT_MODBUS_LINKTYPE_STR)
# define NUT_MODBUS_LINKTYPE_STR	"unknown"
#endif

#define DRIVER_NAME	"MUST EP2000Pro driver (libmodbus link type: " NUT_MODBUS_LINKTYPE_STR ")"
#define DRIVER_VERSION	"0.01"

#ifndef STRFY0
#	define STRFY0(x) #x
#endif
#ifndef STRFY
#	define STRFY(x) STRFY0(x)
#endif

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))
#define BAUD_RATE 9600
#define PARITY 'N'
#define DATA_BIT 8
#define STOP_BIT 1
#define MODBUS_SLAVE_ID 10

#define BATTERY_LOW_DEFAULT 15
#define BATTERY_LOW_NAME "lowbatt"

/* Variables */
static modbus_t *modbus_ctx = NULL;
static uint16_t battery_low = BATTERY_LOW_DEFAULT;

static int mb_read(int addr, int nb, uint16_t * dest);
static int mb_write(int addr, const uint16_t value);

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Mikhail Mironov <mike@darkmike.ru>\n",
	DRV_BETA,
	{NULL}
};

/* fault description struct */
struct fault {
	int code;	/* fault code */
	char *desc;	/* description */
};
typedef struct fault fault_t;

static fault_t fault_list [] = {
	/* From documentation */
	{2, "[Inverter over temperature]"},
	{3, "[Battery voltage is too high]"},
	{4, "[Battery voltage is too low]"},
	{5, "[Output short circuited]"},
	{6, "[Inverter output voltage is high]"},
	{7, "[Overload]"},
	{11, "[Main relay failed]"},
	{41, "[Inverter grid voltage is low]"},
	{42, "[Inverter grid voltage is high]"},
	{43, "[Inverter grid under frequency]"},
	{44, "[Inverter grid over frequency]"},
	{45, "[AVR failed]"},
	{51, "[Inverter over current protection error]"},
	{58, "[Inverter output voltage is too low]"},

	/* Found in internet */
	{1, "[Fan is locked when inverter is off]"},
	{8, "[Inverter bus voltage is too high]"},
	{9, "[Bus soft start failed]"},
	{21, "[Inverter output voltage sensor error]"},
	{22, "[Inverter grid voltage sensor error]"},
	{23, "[Inverter output current sensor error]"},
	{24, "[Inverter grid current sensor error]"},
	{25, "[Inverter load current sensor error]"},
	{26, "[Inverter grid over current error]"},
	{27, "[Inverter radiator over temperature]"},
	{31, "[Solar charger battery voltage class error]"},
	{32, "[Solar charger current sensor error]"},
	{33, "[Solar charger current is uncontrollable]"},
	{52, "[Inverter bus voltage is too low]"},
	{53, "[Inverter soft start failed]"},
	{54, "[Over DC voltage in AC output]"},
	{56, "[Battery connection is open]"},
	{57, "[Inverter control current sensor error]"},
	{61, "[Fan is locked when inverter is on]"},
	{62, "[Fan2 is locked when inverter is on]"},
	{63, "[Battery is over-charged]"},
	{64, "[Low battery]"},
	{67, "[Overload]"},
	{70, "[Output power derating]"},
	{72, "[Solar charger stops due to low battery]"},
	{73, "[Solar charger stops due to high PV voltage]"},
	{74, "[Solar charger stops due to over load]"},
	{75, "[Solar charger over temperature]"},
	{76, "[PV charger communication error]"},
	{77, "[Parameter error]"},

	/* End of list */
	{0, NULL}
};

enum state {
	INIT=1,
	SELF_CHECK,
	BACKUP,
	LINE,
	STOP,
	POWER_OFF,
	GRID_CHG,
	SOFT_START,
};
typedef enum state state_t;

/**
 * Device register map:
 *
 * RO Block
 * 30000 Type of machine (Reserved)
 * 30001 Software version
 * 30002 Work state (See state enum)
 * 30003 BatClass, V (12, 24)
 * 30004 Rated power, W
 * 30005 Grid voltage, 0.1V
 * 30006 Grid frequency, 0.1Hz
 * 30007 Output Voltage, 0.1V
 * 30008 Output Frequency, 0.1Hz
 * 30009 Load current, 0.1A
 * 30010 Load Power, W
 * 30011 Reserved
 * 30012 Load percent, %
 * 30013 LoadState (0: LOAD_NORMAL, 1: LOAD_ALARM, 2: OVER_LOAD)
 * 30014 Battery Voltage, 0.1V
 * 30015 Battery current, 0.1A
 * 30016 Reserved
 * 30017 Battery SOC, %
 * 30018 Transformer Temp, ℃
 * 30019 AVR State (0: AVR_BYPASS, 1: AVR_STEPDWON, 2: AVR_BOOST, 4: AVR_EBOOST)
 * 30020 Buzzer State (0: BUZZ_OFF, 1: BUZZ_BLEW, 2: BUZZ_ALARM)
 * 30021 System fault ID (See fault_list)
 * 30022 System alarm ID (Bit field, see below)
 * 30023 Charge Stage (0: CC, 1: CV, 2: FV)
 * 30024 Charge Flag (1: charge, 0: no charge)
 * 30025 Main SW (1: on, 0: off)
 * 30026 DelayType (0: standard, 1: long delay, 2: integration)
 *
 * Alarm ID:
 * Bit 0
 * Bit 1	Battery voltage is too Low
 * Bit 2	Over load
 * Bit 3	Battery voltage is too high
 * Bit 4	Parameter error
 * Bit 5-15 Reserved
 *
 * RW Block:
 * 31000 Grid frequency type (0: 50Hz, 1: 60Hz)
 * 31001 Grid voltage Type, V (220, 230)
 * 31002 Shutdown voltage, 0.1V (12V: 100-120; 24V: x2)
 * 31003 Absorption charge voltage, 0.1V (12V: 138-145; 24V: x2)
 * 31004 Float Charge Voltage, 0.1V (12V: 135-145; 24V: x2)
 * 31005 Bulk Current, A (5 10 15 20 25 30; Max current depends on Rated power and BatClass)
 * 31006 Buzzer (1: silence, 0: normal)
 * 31007 Enable grid charge (0: enable, 1: disable)
 * 31009 Enable backlight (1: enable, 0: disable)
 * 31016 Utility power on (0: enable, 1: disable)
 * 31017 Enable over load recover (1: enable, 0: disable)
 *
 * Write only block. 0 - ignore, 1 - action
 * 32000 Restore factory settings
 * 32001 Remote reset
 * 32002 Remote shutdown
 */

static int instcmd(const char *cmdname, const char *extra);
static int setvar(const char *varname, const char *value);

void upsdrv_initinfo(void)
{
	uint16_t base_reg[5];
	uint16_t conf_reg[4];
	int r;
	int tmp;

	upsdebugx(2, "upsdrv_initinfo");

	/* check battery low from arguments */
	if (testvar(BATTERY_LOW_NAME)) {
		tmp = atoi(getval(BATTERY_LOW_NAME));
		if (tmp < 1 || tmp > 99) {
			fatalx(EXIT_FAILURE, "Given value '%d' of " BATTERY_LOW_NAME " is out of range (1-99)", tmp);
		}
		battery_low = (uint16_t) tmp;
	}

	dstate_setinfo("battery.charge.low", "%u", battery_low);

	dstate_setinfo("ups.mfr", "MUST");
	dstate_setinfo("ups.model", "EP2000Pro");

	upsdebugx(2, "initial read");

	r = mb_read(30000, 5, base_reg);
	if (r == -1) {
		fatalx(EXIT_FAILURE, "failed to read base registers from UPS: %s", modbus_strerror(errno));
	}
	r = mb_read(31000, 4, conf_reg);
	if (r == -1) {
		fatalx(EXIT_FAILURE, "failed to read config registers from UPS: %s", modbus_strerror(errno));
	}

	dstate_setinfo("ups.firmware", "%u", base_reg[1]); /*30001 Software version*/
	dstate_setinfo("battery.voltage.nominal", "%u", base_reg[3]); /*30003 BatClass*/
	dstate_setinfo("ups.power.nominal", "%u", base_reg[4]); /*30004 Rated power*/
	dstate_setinfo("output.frequency.nominal", "%u", conf_reg[0] ? 60 : 50); /*31000 Grid frequency type*/
	dstate_setinfo("output.voltage.nominal", "%u", conf_reg[1]); /*31001 Grid voltage Type*/
	dstate_setinfo("battery.voltage.low", "%.1f", conf_reg[2] * 0.1); /*31002 Shutdown voltage*/
	dstate_setinfo("battery.voltage.high", "%.1f", conf_reg[3] * 0.1); /*31003 Absorption charge voltage*/

	dstate_addcmd("shutdown.return");
	dstate_addcmd("beeper.disable");
	dstate_addcmd("beeper.enable");

	upsh.instcmd = instcmd;

	dstate_setflags("output.frequency.nominal", ST_FLAG_RW);
	dstate_setflags("output.voltage.nominal", ST_FLAG_RW);
	dstate_setflags("battery.voltage.low", ST_FLAG_RW);
	dstate_setflags("battery.voltage.high", ST_FLAG_RW);

	dstate_addenum("output.frequency.nominal", "50");
	dstate_addenum("output.frequency.nominal", "60");
	dstate_addenum("output.voltage.nominal", "220");
	dstate_addenum("output.voltage.nominal", "230");

	upsh.setvar = setvar;
}

void upsdrv_updateinfo(void)
{
	uint16_t base_reg[25];
	uint16_t conf_reg[7];
	int r;
	state_t state;
	fault_t *fault_cur;
	char *fault_desc;

	upsdebugx(2, "upsdrv_updateinfo");

	r = mb_read(30000, 25, base_reg);
	if (r == -1) {
		dstate_datastale();
		return;
	}
	r = mb_read(31000, 7, conf_reg);
	if (r == -1) {
		dstate_datastale();
		return;
	}

	if (base_reg[2] < INIT || base_reg[2] > SOFT_START) {
		upslogx(LOG_WARNING, "Unexpected UPS state: %u", base_reg[2]);
		dstate_datastale();
		return;
	} else {
		state = (state_t) base_reg[2];
	}

	status_init();
	alarm_init();

	dstate_setinfo("input.voltage", "%.1f", base_reg[5] * 0.1); /*30005 Grid voltage*/
	dstate_setinfo("input.frequency", "%.1f", base_reg[6] * 0.1); /*30006 Grid frequency*/
	dstate_setinfo("output.voltage", "%.1f", base_reg[7] * 0.1); /*30007 Output Voltage*/
	dstate_setinfo("output.frequency", "%.1f", base_reg[8] * 0.1); /*30008 Output Frequency*/
	dstate_setinfo("output.current", "%.1f", base_reg[9] * 0.1); /*30009 Load current*/
	dstate_setinfo("ups.power", "%u", base_reg[10]); /*30010 Load Power*/
	dstate_setinfo("ups.load", "%u", base_reg[12]); /*30012 Load percent*/
	dstate_setinfo("battery.voltage", "%.1f", base_reg[14] * 0.1); /*30014 Battery Voltage*/
	dstate_setinfo("battery.current", "%.1f", base_reg[15] * 0.1); /*30015 Battery current*/
	dstate_setinfo("battery.charge", "%u", base_reg[17]); /*30017 Battery SOC*/
	dstate_setinfo("ups.temperature", "%u", base_reg[18]); /*30018 Transformer Temp*/
	dstate_setinfo("battery.voltage.low", "%.1f", conf_reg[2] * 0.1); /*31002 Shutdown voltage*/
	dstate_setinfo("battery.voltage.high", "%.1f", conf_reg[3] * 0.1); /*31003 Absorption charge voltage*/
	dstate_setinfo("ups.beeper.status", conf_reg[6] ? "disabled" : "enabled"); /*31006 Buzzer*/

	upsdebugx(2, "Float Charge Voltage %.1f", conf_reg[4] * 0.1); /*31004 Float Charge Voltage*/
	upsdebugx(2, "Bulk Current %u", conf_reg[5]); /*31005 Bulk Current*/

	if (base_reg[19] != 0) {
		if (base_reg[19] == 1) {
			status_set("TRIM");
		} else {
			status_set("BOOST");
		}
	}

	if (base_reg[13] != 0) {
		status_set("OVER");
		if (base_reg[13] > 1) {
			alarm_set("[OVERLOAD]");
		}
	}

	if (state == BACKUP) {
		status_set("OB");
	} else if (state == LINE) {
		status_set("OL");
	} else if (state == STOP || state == POWER_OFF) {
		status_set("OFF");
	}

	if (base_reg[17] <= battery_low) {/*30017 Battery SOC*/
		status_set("LB");
	}

	upsdebugx(2, "Is Charge: %s", base_reg[24] ? "yes" : "no"); /*30024 Charge Flag*/
	upsdebugx(2, "Charger mode: %s", base_reg[23] == 0 ? "CC" : base_reg[23] == 1 ? "CV" : "FV"); /*30023 Charge Stage*/

	if (state == BACKUP) {
		dstate_setinfo("battery.charger.status", "discharging");
	} else if (!base_reg[24]) {/*30024 Charge Flag*/
		dstate_setinfo("battery.charger.status", "resting");
	} else if (base_reg[23] < 2) {/*30023 Charge Stage*/
		dstate_setinfo("battery.charger.status", "charging");
	} else {
		dstate_setinfo("battery.charger.status", "floating");
	}

	if (base_reg[21]) {
		fault_desc = "[Generic fault]";
		for (fault_cur = fault_list; fault_list->desc; fault_cur++) {
			if (fault_cur->code == base_reg[21]) {
				fault_desc = fault_cur->desc;
				break;
			}
		}
		alarm_set(fault_desc);
	}

	if (CHECK_BIT(base_reg[22], 1)) {/*Battery voltage is too Low*/
		status_set("LB");
	}
	if (CHECK_BIT(base_reg[22], 2)) {/*Over load*/
		status_set("OVER");
	}
	if (CHECK_BIT(base_reg[22], 3)) {/*Battery voltage is too high*/
		status_set("HB");
	}
	if (CHECK_BIT(base_reg[22], 4)) {/*Parameter error*/
		alarm_set("[Parameter error]");
	}

	alarm_commit();
	status_commit();
	dstate_dataok();

	return;
}

void upsdrv_shutdown(void)
{
	/* Only implement "shutdown.default"; do not invoke
	 * general handling of other `sdcommands` here */

	int ret = -1;

	upsdebugx(2, "upsdrv_shutdown");

	ret = do_loop_shutdown_commands("shutdown.return", NULL);
	if (handling_upsdrv_shutdown > 0)
		set_exit_flag(ret == STAT_INSTCMD_HANDLED ? EF_EXIT_SUCCESS : EF_EXIT_FAILURE);
}

static int set_beeper(int enabled)
{
	return mb_write(31006, enabled ? 0 : 1); /*31006 Buzzer (1: silence, 0: normal)*/
}

static int instcmd(const char *cmdname, const char *extra)
{
	int r;

	/* May be used in logging below, but not as a command argument */
	NUT_UNUSED_VARIABLE(extra);
	upsdebug_INSTCMD_STARTING(cmdname, extra);

	if (!strcasecmp(cmdname, "beeper.enable")) {
		r = set_beeper(1);
		return r != -1 ? STAT_INSTCMD_HANDLED : STAT_INSTCMD_FAILED;
	} else if (!strcasecmp(cmdname, "beeper.disable")) {
		r = set_beeper(0);
		return r != -1 ? STAT_INSTCMD_HANDLED : STAT_INSTCMD_FAILED;
	} else if (!strcasecmp(cmdname, "shutdown.return")) {
		return mb_write(32002, 1); /*32002 Remote shutdown*/
		return r != -1 ? STAT_INSTCMD_HANDLED : STAT_INSTCMD_FAILED;
	}

	upslog_INSTCMD_UNKNOWN(cmdname, extra);
	return STAT_INSTCMD_UNKNOWN;
}

static int set_dvar(const char *varname, const char *value, int addr, double low, double high) {
	const char *rate;
	int r;
	double val;

	val = strtod(value, NULL);

	rate = dstate_getinfo("battery.voltage.nominal");
	if (rate != NULL && strcmp(rate, "12") != 0) {
		low *= 2;
		high *= 2;
	}

	if (val < low || val > high) {
		upslogx(LOG_WARNING, "%s %s out of range: %.1f-%.1f", varname, value, low, high);
		return STAT_SET_FAILED;
	} else {
		r = mb_write(addr, (uint16_t) (val * 10));
		if (r == -1) {
			return STAT_SET_FAILED;
		} else {
			dstate_setinfo(varname, "%.1f", val);
			return STAT_SET_HANDLED;
		}
	}
}

static int set_ivar(const char *varname, int val, int addr, uint16_t setval, int val1, int val2) {
	int r;

	if (val != val1 && val != val2) {
		upslogx(LOG_WARNING, "%s %u out list of valid values: %u, %u", varname, val, val1, val2);
		return STAT_SET_FAILED;
	} else {
		r = mb_write(addr, setval);
		if (r == -1) {
			return STAT_SET_FAILED;
		} else {
			dstate_setinfo(varname, "%u", val);
			return STAT_SET_HANDLED;
		}
	}
}

static int setvar(const char *varname, const char *value)
{
	int i;

	upsdebug_SET_STARTING(varname, value);

	if (!strcasecmp(varname, "battery.voltage.low")) {
		return set_dvar("battery.voltage.low", value, 31002, 10, 12);
	} else if (!strcasecmp(varname, "battery.voltage.high")) {
		return set_dvar("battery.voltage.high", value, 31003, 13.8, 14.5);
	} else if (!strcasecmp(varname, "output.frequency.nominal")) {
		i = atoi(value);
		return set_ivar("output.frequency.nominal", i, 31000, i == 50 ? 0 : 1, 50, 60);
	} else if (!strcasecmp(varname, "output.voltage.nominal")) {
		i = atoi(value);
		return set_ivar("output.voltage.nominal", i, 31000, (uint16_t) i, 220, 230);
	}

	upslog_SET_UNKNOWN(varname, value);
	return STAT_SET_UNKNOWN;
}

void upsdrv_help(void)
{
}

/* optionally tweak prognames[] entries */
void upsdrv_tweak_prognames(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, BATTERY_LOW_NAME, "Low battery level (default " STRFY(BATTERY_LOW_DEFAULT) ")");
}

void upsdrv_initups(void)
{
	int r;
	upsdebugx(2, "upsdrv_initups");

	modbus_ctx = modbus_new_rtu(device_path, BAUD_RATE, PARITY, DATA_BIT, STOP_BIT);
	if (modbus_ctx == NULL)
		fatalx(EXIT_FAILURE, "Unable to create the libmodbus context");

	r = modbus_set_slave(modbus_ctx, MODBUS_SLAVE_ID);	/* slave ID */
	if (r < 0) {
		modbus_free(modbus_ctx);
		fatalx(EXIT_FAILURE, "Invalid modbus slave ID %d",MODBUS_SLAVE_ID);
	}

	if (modbus_connect(modbus_ctx) == -1) {
		modbus_free(modbus_ctx);
		fatalx(EXIT_FAILURE, "modbus_connect: unable to connect: %s", modbus_strerror(errno));
	}

}

void upsdrv_cleanup(void)
{
	if (modbus_ctx != NULL) {
		modbus_close(modbus_ctx);
		modbus_free(modbus_ctx);
	}
}

/* Modbus Read Holding Registers */
static int mb_read(int addr, int nb, uint16_t * dest)
{
	int r, i;

	/* zero out the thing, because we might have reused it */
	for (i=0; i<nb; i++) {
		dest[i] = 0;
	}

	r = modbus_read_registers(modbus_ctx, addr, nb, dest);
	if (r == -1) {
		upslogx(LOG_ERR, "Read error: modbus_read_registers(addr:%d, count:%d): %s (%d)", addr, nb, modbus_strerror(errno), errno);
	}
	return r;
}

/* Modbus Write Holding Register */
static int mb_write(int addr, const uint16_t value)
{
	int r;
	uint16_t regs[1];

	regs[0] = value;
	r = modbus_write_registers(modbus_ctx, addr, 1, regs);
	if (r == -1) {
		upslogx(LOG_ERR, "Write error: modbus_write_registers(addr:%d, nb: 1, val:%d): %s (%d)", addr, value, modbus_strerror(errno), errno);
	}
	return r;
}
