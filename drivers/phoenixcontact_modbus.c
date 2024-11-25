/*  phoenixcontact_modbus.c - Driver for PhoenixContact-QUINT UPS
 *
 *  Copyright (C)
 *    2017  Spiros Ioannou <sivann@inaccess.com>
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

#define DRIVER_NAME	"NUT PhoenixContact Modbus driver"
#define DRIVER_VERSION	"0.05"

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))
#define MODBUS_SLAVE_ID 192

#define QUINT_UPS 0
#define QUINT4_UPS 1

/* Variables */
static modbus_t *modbus_ctx = NULL;
static int errcount = 0;

static int mrir(modbus_t * arg_ctx, int addr, int nb, uint16_t * dest);

static uint16_t UPSModel;

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Spiros Ioannou <sivann@inaccess.com>\n",
	DRV_BETA,
	{NULL}
};

void upsdrv_initinfo(void)
{
	uint16_t FWVersion;
	upsdebugx(2, "upsdrv_initinfo");

	dstate_setinfo("device.mfr", "Phoenix Contact");

	/* upsh.instcmd = instcmd; */
	/* upsh.setvar = setvar; */	

	mrir(modbus_ctx, 0x0004, 1, &FWVersion);

	if (FWVersion == 544)
	{
		UPSModel = QUINT_UPS;
		dstate_setinfo("device.model", "QUINT-UPS/24DC OLD");
	}
	else if (FWVersion == 305)
	{
		UPSModel = QUINT4_UPS;
		dstate_setinfo("device.model", "QUINT-UPS/24DC NEW");
	}

	dstate_setinfo("ups.firmware", "%d", FWVersion);
}

void upsdrv_updateinfo(void)
{
	uint16_t tab_reg[64];
	uint16_t actual_code_functions;
	uint16_t actual_alarms = 0;
	uint16_t actual_alarms1 = 0;
	uint16_t battery_voltage;
	uint16_t battery_temperature;
	uint16_t battery_runtime;
	uint16_t battery_capacity;
	uint16_t output_current;

	errcount = 0;

	upsdebugx(2, "upsdrv_updateinfo");

	switch (UPSModel)
	{
	case QUINT4_UPS:

		mrir(modbus_ctx, 0x2000, 1, &actual_code_functions);

		tab_reg[0] = CHECK_BIT(actual_code_functions, 2); /* Battery mode is the 2nd bit of the register 0x2000 */
		tab_reg[2] = CHECK_BIT(actual_code_functions, 5); /* Battery charging is the 5th bit of the register 0x2000 */

		mrir(modbus_ctx, 0x3001, 1, &actual_alarms1);

		tab_reg[1] = CHECK_BIT(actual_alarms1, 2); /* Battery discharged is the 2nd bit of the register 0x3001 */
		break;
	case QUINT_UPS:

		mrir(modbus_ctx, 29697, 3, tab_reg); /* LB is actually called "shutdown event" on this ups */
		break;
	default:
		break;
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
	case QUINT4_UPS:

		mrir(modbus_ctx, 0x2006, 1, tab_reg);
		break;
	case QUINT_UPS:

		mrir(modbus_ctx, 29745, 1, tab_reg);
		break;
	default:
		break;
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
	default:
		break;
	}

	dstate_setinfo("battery.charge", "%d", tab_reg[0]);
	/* dstate_setinfo("battery.runtime",tab_reg[1]*60); */ /* also reported on this address, but less accurately */

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
	default:
		break;
	}

	dstate_setinfo("battery.voltage", "%f", (double) (tab_reg[0]) / 1000.0);
	dstate_setinfo("battery.temperature", "%d", tab_reg[1] - 273);
	dstate_setinfo("battery.runtime", "%d", tab_reg[3]);
	dstate_setinfo("battery.capacity", "%d", tab_reg[8] * 10);
	dstate_setinfo("output.current", "%f", (double) (tab_reg[6]) / 1000.0);

	/* ALARMS */

	alarm_init();

	switch (UPSModel)
	{
	case QUINT4_UPS:

		tab_reg[0] = 0;
		actual_alarms = 0;
		actual_alarms1 = 0;

		mrir(modbus_ctx, 0x3000, 1, &actual_alarms);
		mrir(modbus_ctx, 0x3000, 1, &actual_alarms1);

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
	default:
		break;
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
	/* replace with a proper shutdown function */
	upslogx(LOG_ERR, "shutdown not supported");
	set_exit_flag(-1);
}

void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
}

void upsdrv_initups(void)
{
	int r;
	upsdebugx(2, "upsdrv_initups");

	modbus_ctx = modbus_new_rtu(device_path, 115200, 'E', 8, 1);
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
