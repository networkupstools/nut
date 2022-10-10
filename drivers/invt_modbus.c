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
#include "modbus-ascii.h"
#include "invt_modbus.h"


#define DRIVER_NAME "INVT modbus driver"
#define DRIVER_VERSION  "0.01"

/* Variables */
static modbus_t *modbus_ctx = NULL;
static int ser_baud_rate = BAUD_RATE;	/* serial port baud rate */
static char ser_parity = PARITY;	/* serial port parity */
static int ser_data_bit = DATA_BIT;	/* serial port data bit */
static int ser_stop_bit = STOP_BIT;	/* serial port stop bit */
static int mbio_slave_id = MODBUS_SLAVE_ID;	/* set device ID to default value */



static int mrir(modbus_t * arg_ctx, int addr, int nb, uint16_t * dest);
static int mrhr(modbus_t * arg_ctx, int addr, int nb, uint16_t * dest);

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Michael Manerko <splien.ma@gmail.com>\n",
	DRV_BETA,
	{ NULL }
};



void upsdrv_initinfo(void)
{
	upsdebugx(2, "upsdrv_initinfo");

	uint16_t tab_reg[12];
	int r;

	dstate_setinfo("device.mfr", "unknown");
	dstate_setinfo("device.model", "Unknown");

	upsdebugx(2, "initial read");

	/* 
	   function 0x3, holding register
	   reg    value
	   78     7: HT11(6-20 KVA)
	   8: HT11(1-3 KVA) 
	   79     1: 3in - 3out phase
	   2: 3in - 1out phase
	   3: 1in - 1out phase
	   4: 1in - 3out phase
	   5: 2in - 2out phase
	 */

	r = mrhr(modbus_ctx, 78, 2, tab_reg);

	if (r == -1) {
		fatalx(EXIT_FAILURE, "failed to read UPS, retcode is %d error %s", r, modbus_strerror(errno));
	}

	upsdebugx(2, "read UPS Code %d", tab_reg[0]);

	/* known INVT Models */
	switch (tab_reg[0]) {
	case 7:
		dstate_setinfo("ups.model", "HT11(6-20 KVA)");
		break;

	case 8:
		dstate_setinfo("ups.model", "HT11(1-3 KVA)");
		break;

	default:
		dstate_setinfo("ups.model", "Unknown UPS. Model id %u", tab_reg[0]);
	}

	/* upsh.instcmd = instcmd; */
	/* upsh.setvar = setvar; */
}




void upsdrv_updateinfo(void)
{
	upsdebugx(2, "upsdrv_updateinfo");

	uint16_t i_tab_reg[80];
	uint16_t h_tab_reg[80];
	int r;

	status_init();


	/* measurements and config */
	r = mrhr(modbus_ctx, 1, 79, h_tab_reg);

	if (r == -1) {
		upsdebugx(2, "Did not receive any data from the UPS at 0x1 error %s", modbus_strerror(errno));
	}


	dstate_setinfo("battery.charge", "%.2f", (double) h_tab_reg[56 - 1] / 10);
	upsdebugx(2, "battery.charge %.2f", (double) h_tab_reg[56 - 1] / 10);
	//dstate_setinfo("battery.capacity", "%u", (tab_reg[56-1]/10) );
	dstate_setinfo("battery.voltage", "%.2f", (double) h_tab_reg[50 - 1] / 10);
	upsdebugx(2, "battery.voltage %.2f", (double) h_tab_reg[50 - 1] / 10);
	dstate_setinfo("battery.current", "%.2f", (double) h_tab_reg[52 - 1] / 10);
	upsdebugx(2, "battery.current %.2f", (double) h_tab_reg[52 - 1] / 10);
	dstate_setinfo("battery.runtime", "%.2f", (double) h_tab_reg[55 - 1]);
	upsdebugx(2, "battery.runtime %.2f", (double) h_tab_reg[55 - 1]);


	if (h_tab_reg[79 - 1] == 3) {
		/* this a 1-phase model */
		dstate_setinfo("input.phases", "1");
		dstate_setinfo("ups.load", "%u", h_tab_reg[46 - 1] / 10);
		upsdebugx(2, "ups.load %u", h_tab_reg[46 - 1] / 10);
		dstate_setinfo("input.bypass.voltage", "%u", h_tab_reg[1 - 1] / 10);
		upsdebugx(2, "input.bypass.voltage %u", h_tab_reg[1 - 1] / 10);
		dstate_setinfo("input.voltage", "%u", h_tab_reg[13 - 1] / 10);
		upsdebugx(2, "input.voltage %u", h_tab_reg[13 - 1] / 10);
		dstate_setinfo("input.frequency", "%.2f", (double) h_tab_reg[19 - 1] / 100);
		upsdebugx(2, "input.frequency %.2f", (double) h_tab_reg[19 - 1] / 100);
		dstate_setinfo("input.bypass.frequency", "%.2f", (double) h_tab_reg[7 - 1] / 100);
		upsdebugx(2, "input.bypass.frequency %.2f", (double) h_tab_reg[7 - 1] / 100);
		dstate_setinfo("input.current", "%.1f", (double) h_tab_reg[16 - 1] / 10);
		upsdebugx(2, "input.current %.1f", (double) h_tab_reg[16 - 1] / 10);

		dstate_setinfo("output.voltage", "%u", h_tab_reg[25 - 1] / 10);
		upsdebugx(2, "output.voltage %u", h_tab_reg[25 - 1] / 10);
		dstate_setinfo("input.frequency", "%.2f", (double) h_tab_reg[19 - 1] / 100);
		upsdebugx(2, "input.frequency %.2f", (double) h_tab_reg[19 - 1] / 100);
		dstate_setinfo("output.current", "%.1f", (double) h_tab_reg[28 - 1] / 10);
		upsdebugx(2, "output.current %.1f", (double) h_tab_reg[28 - 1] / 10);
	} else {
		/* this a 3-phase model */
		dstate_setinfo("input.phases", "3");

		dstate_setinfo("ups.load", "%u", h_tab_reg[46 - 1] / 10);

		dstate_setinfo("ups.L1.load", "%u", h_tab_reg[46 - 1] / 10);
		dstate_setinfo("ups.L2.load", "%u", h_tab_reg[47 - 1] / 10);
		dstate_setinfo("ups.L3.load", "%u", h_tab_reg[48 - 1] / 10);

		dstate_setinfo("input.bypass.L1-N.voltage", "%u", h_tab_reg[1 - 1] / 10);
		dstate_setinfo("input.bypass.L2-N.voltage", "%u", h_tab_reg[2 - 1] / 10);
		dstate_setinfo("input.bypass.L3-N.voltage", "%u", h_tab_reg[3 - 1] / 10);

		dstate_setinfo("output.L1-N.voltage", "%u", h_tab_reg[25 - 1] / 10);
		dstate_setinfo("output.L2-N.voltage", "%u", h_tab_reg[26 - 1] / 10);
		dstate_setinfo("output.L3-N.voltage", "%u", h_tab_reg[27 - 1] / 10);

		dstate_setinfo("output.L1.current", "%u", h_tab_reg[28 - 1] / 10);
		dstate_setinfo("output.L2.current", "%u", h_tab_reg[29 - 1] / 10);
		dstate_setinfo("output.L3.current", "%u", h_tab_reg[30 - 1] / 10);
	}


	if (h_tab_reg[49 - 1] != 0) {
		dstate_setinfo("ambient.1.present", "yes");
		dstate_setinfo("ambient.1.temperature", "%.1f", (double) h_tab_reg[49 - 1] / 10);
	}


	/* alarms */
	r = mrir(modbus_ctx, 81, 16, i_tab_reg);

	alarm_init();

	if (r == -1) {
		upsdebugx(2, "Did not receive any data from the UPS at 81 ! Ignoring ? error %s", modbus_strerror(errno));
	}

	if (i_tab_reg[85 - 81] == 1) {
		upsdebugx(2, "Alarm EPO.");
		alarm_set("EPO.");
	}
	if (i_tab_reg[88 - 81] == 1) {
		upsdebugx(2, "Alarm Input Fail.");
		alarm_set("Input Fail.");
	}
	if (i_tab_reg[89 - 81] == 1) {
		upsdebugx(2, "Alarm Bypass Sequence Fail.");
		alarm_set("Bypass Sequence Fail.");
	}
	if (i_tab_reg[90 - 81] == 1) {
		upsdebugx(2, "Alarm Bypass Voltage Fail.");
		alarm_set("Bypass Voltage Fail.");
	}
	if (i_tab_reg[91 - 81] == 1) {
		upsdebugx(2, "Alarm Bypass Fail.");
		alarm_set("Bypass Fail.");
	}
	if (i_tab_reg[92 - 81] == 1) {
		upsdebugx(2, "Alarm Bypass Over Load.");
		alarm_set("Bypass Over Load.");
	}
	if (i_tab_reg[93 - 81] == 1) {
		upsdebugx(2, "Alarm Bypass Over Load Timeout.");
		alarm_set("Bypass Over Load Timeout.");
	}
	if (i_tab_reg[96 - 81] == 1) {
		upsdebugx(2, "Alarm Output Shorted.");
		alarm_set("Output Shorted.");
	}
	if (i_tab_reg[97 - 81] == 1) {
		upsdebugx(2, "Alarm Battery EOD.");
		alarm_set("Battery EOD.");
	}



	if ((i_tab_reg[88 - 81] == 0)&&(i_tab_reg[81-81] == 1)) {
		status_set("OL");
		upsdebugx(2, "OL");
	} else if ((i_tab_reg[88 - 81] == 1)&&(i_tab_reg[81-81] == 1)&&(h_tab_reg[55-1] < 5)) {
		status_set("LB");
		upsdebugx(2, "LB");
	} else if ((i_tab_reg[88 - 81] == 1)&&(i_tab_reg[81-81] == 1)) {
		status_set("OB");
		upsdebugx(2, "OB");
	} else if (i_tab_reg[81-81] == 2) {
		status_set("BYPASS");
		upsdebugx(2, "BYPASS");
	}
	if (i_tab_reg[83-81] == 3) {
		status_set("DISCHRG");
	 	upsdebugx(2, "DISCHRG");
	} else if (i_tab_reg[83-81] == 2) {
		status_set("CHRG");
		upsdebugx(2, "CHRG");
	} 
	if (i_tab_reg[81-81] == 0) {
		status_set("OFF");
		upsdebugx(2, "OFF");
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
    __attribute__((noreturn));

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
	addvar(VAR_VALUE, "ser_baud_rate", "serial port baud rate default 9600");
	addvar(VAR_VALUE, "ser_parity", "serial port parity default N");
	addvar(VAR_VALUE, "ser_data_bit", "serial port data bit default 8");
	addvar(VAR_VALUE, "ser_stop_bit", "serial port stop bit default 1");
	addvar(VAR_VALUE, "rio_slave_id", "RIO modbus slave ID default 1");
}

void upsdrv_initups(void)
{
	int r;
	upsdebugx(2, "upsdrv_initups");


/* check if serial baud rate is set ang get the value */
	if (testvar("ser_baud_rate")) {
		ser_baud_rate = (int) strtol(getval("ser_baud_rate"), NULL, 10);
	}
	upsdebugx(2, "ser_baud_rate %d", ser_baud_rate);

	/* check if serial parity is set ang get the value */
	if (testvar("ser_parity")) {
		/* Dereference the char* we get */
		char *sp = getval("ser_parity");
		if (sp) {
			/* TODO? Sanity-check the char we get? */
			ser_parity = *sp;
		} else {
			upsdebugx(2, "Could not determine ser_parity, will keep default");
		}
	}
	upsdebugx(2, "ser_parity %c", ser_parity);

	/* check if serial data bit is set ang get the value */
	if (testvar("ser_data_bit")) {
		ser_data_bit = (int) strtol(getval("ser_data_bit"), NULL, 10);
	}
	upsdebugx(2, "ser_data_bit %d", ser_data_bit);

	/* check if serial stop bit is set ang get the value */
	if (testvar("ser_stop_bit")) {
		ser_stop_bit = (int) strtol(getval("ser_stop_bit"), NULL, 10);
	}
	upsdebugx(2, "ser_stop_bit %d", ser_stop_bit);

	/* check if device ID is set ang get the value */
	if (testvar("rio_slave_id")) {
		mbio_slave_id = (int) strtol(getval("rio_slave_id"), NULL, 10);
	}
	upsdebugx(2, "rio_slave_id %d", mbio_slave_id);


	modbus_ctx = modbus_new_ascii(device_path, ser_baud_rate, ser_parity, ser_data_bit, ser_stop_bit);
	if (modbus_ctx == NULL)
		fatalx(EXIT_FAILURE, "Unable to create the libmodbus context");

	r = modbus_set_slave(modbus_ctx, mbio_slave_id);	/* slave ID */
	if (r < 0) {
		modbus_free(modbus_ctx);
		fatalx(EXIT_FAILURE, "Invalid modbus slave ID %d", mbio_slave_id);
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

/* Modbus Read Input Registers function 0x4*/
static int mrir(modbus_t * arg_ctx, int addr, int nb, uint16_t * dest)
{
	int r, i;

	/* zero out the thing, because we might have reused it */
	for (i = 0; i < nb; i++) {
		dest[i] = 0;
	}

	/*r = modbus_read_input_registers(arg_ctx, addr, nb, dest); */
	r = modbus_read_input_registers(arg_ctx, addr, nb, dest);
	if (r == -1) {
		upslogx(LOG_ERR, "mrir: modbus_read_input_registers(addr:%d, count:%d): %s (%s)", addr, nb, modbus_strerror(errno), device_path);
	}
	return r;
}


/* Modbus Read Holding Registers function 0x3*/
static int mrhr(modbus_t * arg_ctx, int addr, int nb, uint16_t * dest)
{
	int r, i;

	/* zero out the thing, because we might have reused it */
	for (i = 0; i < nb; i++) {
		dest[i] = 0;
	}

	/*r = modbus_read_input_registers(arg_ctx, addr, nb, dest); */
	r = modbus_read_registers(arg_ctx, addr, nb, dest);
	if (r == -1) {
		upslogx(LOG_ERR, "mrhr: modbus_read_holding_registers(addr:%d, count:%d): %s (%s)", addr, nb, modbus_strerror(errno), device_path);
	}
	return r;
}
