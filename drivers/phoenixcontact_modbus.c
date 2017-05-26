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
#define DRIVER_VERSION	"0.01"

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))
#define MODBUS_SLAVE_ID 192

/* Variables */
modbus_t *ctx = NULL;
int errcount = 0;

static int mrir(modbus_t * ctx, int addr, int nb, uint16_t * dest);

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Spiros Ioannou <sivann@inaccess.com>\n",
	DRV_EXPERIMENTAL,
	{NULL}
};

void upsdrv_initinfo(void)
{
	upsdebugx(2, "upsdrv_initinfo");

	dstate_setinfo("device.mfr", "Phoenix Contact");
	dstate_setinfo("device.model", "QUINT-UPS/24DC");

	/* upsh.instcmd = instcmd; */
	/* upsh.setvar = setvar; */
}

void upsdrv_updateinfo(void)
{
	upsdebugx(2, "upsdrv_updateinfo");

	uint16_t tab_reg[64];

	mrir(ctx, 29697, 3, tab_reg);

	status_init();

	if (tab_reg[0])
		status_set("OB");
	else
		status_set("OL");

	if (tab_reg[2]) {
		status_set("CHRG");
	}

	if (tab_reg[1]) {
		status_set("LB");	/* LB is actually called "shutdown event" on this ups */
	}

	mrir(ctx, 29745, 1, tab_reg);
	dstate_setinfo("output.voltage", "%d", (int) (tab_reg[0] / 1000));

	mrir(ctx, 29749, 5, tab_reg);
	dstate_setinfo("battery.charge", "%d", tab_reg[0]);
	/* dstate_setinfo("battery.runtime",tab_reg[1]*60); */ /* also reported on this address, but less accurately */

	mrir(ctx, 29792, 10, tab_reg);
	dstate_setinfo("battery.voltage", "%f", (double) (tab_reg[0]) / 1000.0);
	dstate_setinfo("battery.temperature", "%d", tab_reg[1] - 273);
	dstate_setinfo("battery.runtime", "%d", tab_reg[3]);
	dstate_setinfo("battery.capacity", "%d", tab_reg[8] * 10);
	dstate_setinfo("output.current", "%f", (double) (tab_reg[6]) / 1000.0);

	/* ALARMS */
	mrir(ctx, 29840, 1, tab_reg);
	alarm_init();
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
	if (CHECK_BIT(tab_reg[0], 12))
		alarm_set("Low Battery (Voltage)");
	if (CHECK_BIT(tab_reg[0], 13))
		alarm_set("Low Battery (Charge)");
	if (CHECK_BIT(tab_reg[0], 14))
		alarm_set("Low Battery (Time)");
	if (CHECK_BIT(tab_reg[0], 16))
		alarm_set("Low Battery (Service)");
	alarm_commit();

	status_commit();
	if (errcount == 0)
		dstate_dataok();

}

void upsdrv_shutdown(void)
{
	fatalx(EXIT_FAILURE, "shutdown not supported");
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

	ctx = modbus_new_rtu(device_path, 115200, 'E', 8, 1);
	if (ctx == NULL)
		fatalx(EXIT_FAILURE, "Unable to create the libmodbus context");

	r = modbus_set_slave(ctx, MODBUS_SLAVE_ID);	/* slave ID */
	if (r < 0) {
		modbus_free(ctx);
		fatalx(EXIT_FAILURE, "Invalid slave ID %d",MODBUS_SLAVE_ID);
	}

	if (modbus_connect(ctx) == -1) {
		modbus_free(ctx);
		fatalx(EXIT_FAILURE, "modbus_connect: unable to connect: %s", modbus_strerror(errno));
	}

}


void upsdrv_cleanup(void)
{
	if (ctx != NULL) {
		modbus_close(ctx);
		modbus_free(ctx);
	}
}

static int mrir(modbus_t * ctx, int addr, int nb, uint16_t * dest)
{
	int r;
	r = modbus_read_input_registers(ctx, addr, nb, dest);
	if (r == -1) {
		upslogx(LOG_ERR, "mrir: modbus_read_input_registers(addr:%d, count:%d): %s (%s)", addr, nb, modbus_strerror(errno), device_path);
		errcount++;
	}
	errcount = 0;
	return r;
}
