/*  socomec_jbus.c - Driver for Socomec JBUS UPS
 *
 *  Copyright (C)
 *    2021 Thanos Chatziathanassiou <tchatzi@arx.net>
 *
 * Based on documentation found freely on 
 * https://www.socomec.com/files/live/sites/systemsite/files/GB-JBUS-MODBUS-for-Delphys-MP-and-Delphys-MX-operating-manual.pdf
 * but with dubious legal license. The document itself states:
 * ``CAUTION : “This is a product for restricted sales distribution to informed partners. 
 *   Installation restrictions or additional measures may be needed to prevent disturbances''
 * YMMV
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

#define DRIVER_NAME	"Socomec jbus driver"
#define DRIVER_VERSION	"0.09"

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))
#define MODBUS_SLAVE_ID 1
#define BATTERY_RUNTIME_CRITICAL 15

/* Variables */
static modbus_t *modbus_ctx = NULL;

static int mrir(modbus_t * arg_ctx, int addr, int nb, uint16_t * dest);

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Thanos Chatziathanassiou <tchatzi@arx.net>\n",
	DRV_BETA,
	{NULL}
};

void upsdrv_initinfo(void)
{
	uint16_t tab_reg[12];
	int r;
	
	upsdebugx(2, "upsdrv_initinfo");

	dstate_setinfo("device.mfr", "socomec jbus");
	dstate_setinfo("device.model", "Socomec Generic");

	upsdebugx(2, "initial read");

	/* 
		this is a neat trick, but not really helpful right now
		https://stackoverflow.com/questions/25811662/spliting-an-hex-into-2-hex-values/41733170#41733170
		uint8_t *lowbyte;
		uint8_t *hibyte;
	*/

	r = mrir(modbus_ctx, 0x1000, 12, tab_reg);

	if (r == -1) {
		fatalx(EXIT_FAILURE, "failed to read UPS code from JBUS. r is %d error %s", r, modbus_strerror(errno));
	}
	
	upsdebugx(2, "read UPS Code %d", tab_reg[0]);

	if (tab_reg[1]) {
		upsdebugx(2, "read UPS Power %d (kVA * 10)", tab_reg[1]);
		dstate_setinfo("ups.power", "%u", (unsigned int)(tab_reg[1]*100) );
	}

	/* known Socomec Models */
	switch (tab_reg[0]) {
		case 130:
			dstate_setinfo("ups.model", "%s", "DIGYS");
			break;
		
		case 515:
			dstate_setinfo("ups.model", "%s", "DELPHYS MX");
			break;
		
		case 516:
			dstate_setinfo("ups.model", "%s", "DELPHYS MX elite");
			break;

		default:
			dstate_setinfo("ups.model", "Unknown Socomec JBUS. Send id %u and specify the model", tab_reg[0]);
	}

	if (tab_reg[3] && tab_reg[4] && tab_reg[5] && tab_reg[6] && tab_reg[7]) {
		dstate_setinfo("ups.serial", "%c%c%c%c%c%c%c%c%c%c", 
													  (tab_reg[3]&0xFF), (tab_reg[3]>>8),
													  (tab_reg[4]&0xFF), (tab_reg[4]>>8),
													  (tab_reg[5]&0xFF), (tab_reg[5]>>8),
													  (tab_reg[6]&0xFF), (tab_reg[6]>>8),
													  (tab_reg[7]&0xFF), (tab_reg[7]>>8)
												);
	}

	/* upsh.instcmd = instcmd; */
	/* upsh.setvar = setvar; */
}

void upsdrv_updateinfo(void)
{
	uint16_t tab_reg[64];
	int r;
	
	upsdebugx(2, "upsdrv_updateinfo");

	status_init();

	/* ups configuration */
	r = mrir(modbus_ctx, 0x10E0, 32, tab_reg);

	if (r == -1 || !tab_reg[0]) {
		upsdebugx(2, "Did not receive any data from the UPS at 0x10E0 ! Going stale r is %d error %s", r, modbus_strerror(errno));
		dstate_datastale();
		return;
	}

	dstate_setinfo("input.voltage", "%u", tab_reg[0]);
	dstate_setinfo("output.voltage", "%u", tab_reg[1]);
	dstate_setinfo("input.frequency", "%u", tab_reg[2]);
	dstate_setinfo("output.frequency", "%u", tab_reg[3]);

	upsdebugx(2, "battery capacity (Ah * 10) %u", tab_reg[8]);
	upsdebugx(2, "battery elements %u", tab_reg[9]);
	
	/* time and date */
	r = mrir(modbus_ctx, 0x1360, 4, tab_reg);
	if (r == -1) {
		upsdebugx(2, "Did not receive any data from the UPS at 0x1360 ! Ignoring ? r is %d error %s", r, modbus_strerror(errno));
	}

	dstate_setinfo("ups.time", "%02d:%02d:%02d", (tab_reg[1]&0xFF), (tab_reg[0]>>8), (tab_reg[0]&0xFF) );
	dstate_setinfo("ups.date", "%04d/%02d/%02d", (tab_reg[3]+2000), (tab_reg[2]>>8), (tab_reg[1]>>8) );

	/* ups status */
	r = mrir(modbus_ctx, 0x1020, 6, tab_reg);
	
	if (r == -1) {
		upsdebugx(2, "Did not receive any data from the UPS at 0x1020 ! Ignoring ? r is %d error %s", r, modbus_strerror(errno));
		/* 
		dstate_datastale();
		return;
		*/
	}

	if (CHECK_BIT(tab_reg[0], 0))
		upsdebugx(2, "Rectifier Input supply present");
	if (CHECK_BIT(tab_reg[0], 1))
		upsdebugx(2, "Inverter ON ");
	if (CHECK_BIT(tab_reg[0], 2))
		upsdebugx(2, "Rectifier ON");
	if (CHECK_BIT(tab_reg[0], 3))
		upsdebugx(2, "Load protected by inverter");
	if (CHECK_BIT(tab_reg[0], 4))
		upsdebugx(2, "Load on automatic bypass");
	if (CHECK_BIT(tab_reg[0], 5))
		upsdebugx(2, "Load on battery");
	if (CHECK_BIT(tab_reg[0], 6))
		upsdebugx(2, "Remote controls disable");
	if (CHECK_BIT(tab_reg[0], 7))
		upsdebugx(2, "Eco-mode ON");

	if (CHECK_BIT(tab_reg[0], 14))
		upsdebugx(2, "Battery Test failed");
	if (CHECK_BIT(tab_reg[0], 15))
		upsdebugx(2, "Battery near end of backup time");
	if (CHECK_BIT(tab_reg[0], 16))
		upsdebugx(2, "Battery disacharged");

	if (CHECK_BIT(tab_reg[1], 0))
		upsdebugx(2, "Battery OK");
	if (CHECK_BIT(tab_reg[1], 10))
		upsdebugx(2, "Bypass input supply present");
	if (CHECK_BIT(tab_reg[1], 11))
		upsdebugx(2, "Battery charging");
	if (CHECK_BIT(tab_reg[1], 12))
		upsdebugx(2, "Bypass input frequency out of tolerance");
	
	if (CHECK_BIT(tab_reg[2], 0))
		upsdebugx(2, "Unit operating");

	if (CHECK_BIT(tab_reg[3], 0))
		upsdebugx(2, "Maintenance mode active");

	if (CHECK_BIT(tab_reg[4], 0))
		upsdebugx(2, "Boost charge ON");
	if (CHECK_BIT(tab_reg[4], 2))
		upsdebugx(2, "Inverter switch closed");
	if (CHECK_BIT(tab_reg[4], 3))
		upsdebugx(2, "Bypass breaker closed");
	if (CHECK_BIT(tab_reg[4], 4))
		upsdebugx(2, "Maintenance bypass breaker closed");
	if (CHECK_BIT(tab_reg[4], 5))
		upsdebugx(2, "Remote maintenance bypass breaker closed");
	if (CHECK_BIT(tab_reg[4], 6))
		upsdebugx(2, "Output breaker closed (Q3)");
	if (CHECK_BIT(tab_reg[4], 9))
		upsdebugx(2, "Unit working");
	if (CHECK_BIT(tab_reg[4], 12))
		upsdebugx(2, "normal mode active");

	/* alarms */
	r = mrir(modbus_ctx, 0x1040, 4, tab_reg);
	
	alarm_init();

	if (r == -1) {
		upsdebugx(2, "Did not receive any data from the UPS at 0x1040 ! Ignoring ? r is %d error %s", r, modbus_strerror(errno));
		/*
		dstate_datastale();
		return;
		*/
	}

	if (CHECK_BIT(tab_reg[0], 0)) {
		upsdebugx(2, "General Alarm");
		alarm_set("General Alarm present.");
	}
	if (CHECK_BIT(tab_reg[0], 1)) {
		upsdebugx(2, "Battery failure");
		alarm_set("Battery failure.");
	}
	if (CHECK_BIT(tab_reg[0], 2)) {
		upsdebugx(2, "UPS overload");
		alarm_set("Overload fault.");
	}
	if (CHECK_BIT(tab_reg[0], 4)) {
		upsdebugx(2, "Control failure (com, internal supply...)");
		alarm_set("Control failure (com, internal supply...)");
	}
	if (CHECK_BIT(tab_reg[0], 5)) {
		upsdebugx(2, "Rectifier input supply out of tolerance ");
		alarm_set("Rectifier input supply out of tolerance.");
	}
	if (CHECK_BIT(tab_reg[0], 6)) {
		upsdebugx(2, "Bypass input supply out of tolerance ");
		alarm_set("Bypass input supply out of tolerance.");
	}
	if (CHECK_BIT(tab_reg[0], 7)) {
		upsdebugx(2, "Over temperature alarm ");
		alarm_set("Over temperature fault.");
	}
	if (CHECK_BIT(tab_reg[0], 8)) {
		upsdebugx(2, "Maintenance bypass closed");
		alarm_set("Maintenance bypass closed.");
	}
	if (CHECK_BIT(tab_reg[0], 10)) {
		upsdebugx(2, "Battery charger fault");
		alarm_set("Battery charger fault.");
	}
	
	if (CHECK_BIT(tab_reg[1], 1))
		upsdebugx(2, "Improper condition of use");
	if (CHECK_BIT(tab_reg[1], 2))
		upsdebugx(2, "Inverter stopped for overload (or bypass transfer)");
	if (CHECK_BIT(tab_reg[1], 3))
		upsdebugx(2, "Microprocessor control system");
	if (CHECK_BIT(tab_reg[1], 5))
		upsdebugx(2, "Synchronisation fault (PLL fault)");
	if (CHECK_BIT(tab_reg[1], 6))
		upsdebugx(2, "Rectifier input supply fault");
	if (CHECK_BIT(tab_reg[1], 7))
		upsdebugx(2, "Rectifier preventive alarm");
	if (CHECK_BIT(tab_reg[1], 9))
		upsdebugx(2, "Inverter preventive alarm");
	if (CHECK_BIT(tab_reg[1], 10))
		upsdebugx(2, "Charger general alarm");
	if (CHECK_BIT(tab_reg[1], 13))
		upsdebugx(2, "Bypass preventive alarm");
	if (CHECK_BIT(tab_reg[1], 15)) {
		upsdebugx(2, "Imminent STOP");
		alarm_set("Imminent STOP.");
	}

	if (CHECK_BIT(tab_reg[2], 12)) {
		upsdebugx(2, "Servicing alarm");
		alarm_set("Servicing alarm.");
	}
	if (CHECK_BIT(tab_reg[2], 15))
		upsdebugx(2, "Battery room alarm");

	if (CHECK_BIT(tab_reg[3], 0)) {
		upsdebugx(2, "Maintenance bypass alarm");
		alarm_set("Maintenance bypass.");
	}
	if (CHECK_BIT(tab_reg[3], 1)) {
		upsdebugx(2, "Battery discharged");
		alarm_set("Battery discharged.");
	}
	if (CHECK_BIT(tab_reg[3], 3))
		upsdebugx(2, "Synoptic alarm");
	if (CHECK_BIT(tab_reg[3], 4)) {
		upsdebugx(2, "Critical Rectifier fault"); 
		alarm_set("Critical Rectifier fault.");
	}
	if (CHECK_BIT(tab_reg[3], 6)) {
		upsdebugx(2, "Critical Inverter fault");
		alarm_set("Critical Inverter fault.");
	}
	if (CHECK_BIT(tab_reg[3], 10))
		upsdebugx(2, "ESD activated");
	if (CHECK_BIT(tab_reg[3], 11)) {
		upsdebugx(2, "Battery circuit open");
		alarm_set("Battery circuit open.");
	}
	if (CHECK_BIT(tab_reg[3], 14)) {
		upsdebugx(2, "Bypass critical alarm");
		alarm_set("Bypass critical alarm.");
	}

	/* measurements */
	r = mrir(modbus_ctx, 0x1060, 48, tab_reg);

	if (r == -1) {
		upsdebugx(2, "Did not receive any data from the UPS at 0x1060 ! Ignoring ? r is %d error %s", r, modbus_strerror(errno));
		/*
		dstate_datastale();
		return;
		*/
	}

	if (tab_reg[1] == 0xFFFF && tab_reg[2] == 0xFFFF) {
		/* this a 1-phase model */
		dstate_setinfo("input.phases", "1" );
		dstate_setinfo("ups.load", "%u", tab_reg[0] );

		dstate_setinfo("input.bypass.voltage", "%u", tab_reg[6] );

		dstate_setinfo("output.voltage", "%u", tab_reg[9] );

		if (tab_reg[15] != 0xFFFF)
			dstate_setinfo("output.current", "%u", tab_reg[15] );
	}
	else {
		/* this a 3-phase model */
		dstate_setinfo("input.phases", "3" );

		dstate_setinfo("ups.load", "%u", tab_reg[3] );

		dstate_setinfo("ups.L1.load", "%u", tab_reg[0] );
		dstate_setinfo("ups.L2.load", "%u", tab_reg[1] );
		dstate_setinfo("ups.L3.load", "%u", tab_reg[2] );

		dstate_setinfo("input.bypass.L1-N.voltage", "%u", tab_reg[6] );
		dstate_setinfo("input.bypass.L2-N.voltage", "%u", tab_reg[7] );
		dstate_setinfo("input.bypass.L3-N.voltage", "%u", tab_reg[8] );

		dstate_setinfo("output.L1-N.voltage", "%u", tab_reg[9] );
		dstate_setinfo("output.L2-N.voltage", "%u", tab_reg[10] );
		dstate_setinfo("output.L3-N.voltage", "%u", tab_reg[11] );

		if (tab_reg[15] != 0xFFFF)
			dstate_setinfo("output.L1.current", "%u", tab_reg[15] );
		
		if (tab_reg[16] != 0xFFFF)
		dstate_setinfo("output.L2.current", "%u", tab_reg[16] );

		if (tab_reg[17] != 0xFFFF)
			dstate_setinfo("output.L3.current", "%u", tab_reg[17] );
	}

	dstate_setinfo("battery.charge", "%u", tab_reg[4] );
	dstate_setinfo("battery.capacity", "%u", (unsigned int)(tab_reg[5]/10) );
	dstate_setinfo("battery.voltage", "%.2f", (double) (tab_reg[20]) / 10);
	dstate_setinfo("battery.current", "%.2f", (double) (tab_reg[24]) / 10 );
	dstate_setinfo("battery.runtime", "%u", tab_reg[23] );

	dstate_setinfo("input.bypass.frequency", "%u", (unsigned int)(tab_reg[18]/10) );
	dstate_setinfo("output.frequency", "%u", (unsigned int)(tab_reg[19]/10) );

	if (tab_reg[22] != 0xFFFF) {
		dstate_setinfo("ambient.1.present", "yes");
		dstate_setinfo("ambient.1.temperature", "%u", tab_reg[22] );
	}

	if (tab_reg[23] == 0xFFFF) {
		/* battery.runtime == 0xFFFF means we're on mains */
		status_set("OL");
	}
	else if (tab_reg[23] > BATTERY_RUNTIME_CRITICAL) {
		/* we still have mora than BATTERY_RUNTIME_CRITICAL min left ? */
		status_set("OB");
	}
	else {
		status_set("LB");
	}

	/*TODO:
	--essential
	ups.status TRIM/BOOST/OVER
	ups.alarm
	
	--dangerous
	ups.shutdown
	shutdown.return
	shutdown.stop
	shutdown.reboot
	shutdown.reboot.graceful
	bypass.start
	beeper.enable
	beeper.disable
	*/

	alarm_commit();
	status_commit();
	dstate_dataok();

	return;
}

void upsdrv_shutdown(void)
{
	/* Only implement "shutdown.default"; do not invoke
	 * general handling of other `sdcommands` here */

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
}

void upsdrv_initups(void)
{
	int r;
	upsdebugx(2, "upsdrv_initups");

	modbus_ctx = modbus_new_rtu(device_path, 9600, 'N', 8, 1);
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
	int r, i;
	
	/* zero out the thing, because we might have reused it */
	for (i=0; i<nb; i++) {
		dest[i] = 0;
	}

	/*r = modbus_read_input_registers(arg_ctx, addr, nb, dest);*/
	r = modbus_read_registers(arg_ctx, addr, nb, dest);
	if (r == -1) {
		upslogx(LOG_ERR, "mrir: modbus_read_input_registers(addr:%d, count:%d): %s (%s)", addr, nb, modbus_strerror(errno), device_path);
	}
	return r;
}
