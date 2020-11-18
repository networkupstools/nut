/* victronups.c - Model specific routines for GE/IMV/Victron  units
 * Match, Match Lite, NetUps
 *
 * Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>
 * Copyright (C) 2000  Radek Benedikt <benedikt@lphard.cz>
 * old style "victronups"
 * Copyright (C) 2001  Daniel.Prynych <Daniel.Prynych@hornet.cz>
 * porting to now style "newvictron"
 * Copyright (C) 2003  Gert Lynge <gert@lynge.org>
 * Porting to new serial functions. Now removes \n from data (was causing
 * periodic misreadings of temperature and voltage levels)
 * Copyright (C) 2004  Gert Lynge <gert@lynge.org>
 * Implemented some Instant Commands.
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
 */

#include "main.h"
#include "serial.h"

#define DRIVER_NAME	"GE/IMV/Victron UPS driver"
#define DRIVER_VERSION	"0.20"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Russell Kroll <rkroll@exploits.org>\n" \
	"Radek Benedikt <benedikt@lphard.cz>\n" \
	"Daniel Prynych <Daniel.Prynych@hornet.cz>\n" \
	"Gert Lynge <gert@lynge.org>",
	DRV_STABLE,
	{ NULL }
};

#define ENDCHAR	'\r'
#define IGNCHARS "\n"

#define UPS_DELAY 150000
#define UPS_LONG_DELAY 450000

#define VICTRON_OVER 128
#define VICTRON_RB 1
#define VICTRON_OB 2
#define VICTRON_LB 4

#define VICTRON_NO_TEST		1
#define VICTRON_ABORT_TEST	2
#define VICTRON_SYSTEM_TEST	3
#define VICTRON_BATTERY_TEST	4
#define VICTRON_CALIBRATION	5
#define VICTRON_BYPASS_TEST	101

#define LENGTH_TEMP 256

int  sdwdelay = 0;  /* shutdown after 0 second */

char *model_name;

static int start_is_datastale = 1;

static int exist_ups_serial = 0;
static int exist_ups_temperature = 0;
static int exist_output_current = 0;
static int exist_battery_charge = 0;
static int exist_battery_current = 0;
static int exist_battery_temperature = 0;
static int exist_battery_runtime = 0;

static int test_in_progress = VICTRON_NO_TEST;

static int get_data (const char *out_string, char *in_string)
{
	int ret_code;
	ser_send(upsfd, "%s%c", out_string, ENDCHAR);
	usleep (UPS_DELAY);
	ret_code = ser_get_line(upsfd, in_string, LENGTH_TEMP, ENDCHAR,
				IGNCHARS, 3, 0);
	if (ret_code < 1) {
		dstate_datastale();
		return -1;
	}
	return 0;
}

static int instcmd(const char *cmdname, const char *extra)
{
	char temp[ LENGTH_TEMP ];

	if(!strcasecmp(cmdname, "calibrate.start"))
	{
		if(get_data("vTi5!",temp))
		{
			upsdebugx(1, "instcmd: ser_send calibrate.start failed");
			return STAT_INSTCMD_UNKNOWN; /* Or Failed when it get defined */
		}
		else
		{
			upsdebugx(1, "instcmd: calibrate.start returned: %s", temp);
			test_in_progress = VICTRON_CALIBRATION;
			return STAT_INSTCMD_HANDLED;
		}
	}
	else if(!strcasecmp(cmdname, "calibrate.stop"))
	{
		if(get_data("vTi2!",temp))
		{
			upsdebugx(1, "instcmd: ser_send calibrate.stop failed");
			return STAT_INSTCMD_UNKNOWN; /* Or Failed when it get defined */
		}
		else
		{
			upsdebugx(1, "instcmd: calibrate.stop returned: %s", temp);
			return STAT_INSTCMD_HANDLED;
		}
	}
	else if(!strcasecmp(cmdname, "test.battery.stop"))
	{
		if(get_data("vTi2!",temp))
		{
			upsdebugx(1, "instcmd: ser_send test.battery.stop failed");
			return STAT_INSTCMD_UNKNOWN; /* Or Failed when it get defined */
		}
		else
		{
			upsdebugx(1, "instcmd: test.battery.stop returned: %s", temp);
			return STAT_INSTCMD_HANDLED;
		}
	}
	else if(!strcasecmp(cmdname, "test.battery.start"))
	{
		if(get_data("vTi4!",temp))
		{
			upsdebugx(1, "instcmd: ser_send test.battery.start failed");
			return STAT_INSTCMD_UNKNOWN; /* Or Failed when it get defined */
		}
		else
		{
			upsdebugx(1, "instcmd: test.battery.start returned: %s", temp);
			test_in_progress = VICTRON_BATTERY_TEST;
			return STAT_INSTCMD_HANDLED;
		}
	}
	else if(!strcasecmp(cmdname, "test.panel.stop"))
	{
		if(get_data("vTi2!",temp))
		{
			upsdebugx(1, "instcmd: ser_send test.panel.stop failed");
			return STAT_INSTCMD_UNKNOWN; /* Or Failed when it get defined */
		}
		else
		{
			upsdebugx(1, "instcmd: test.panel.stop returned: %s", temp);
			return STAT_INSTCMD_HANDLED;
		}
	}
	else if(!strcasecmp(cmdname, "test.panel.start"))
	{
		if(get_data("vTi3!",temp))
		{
			upsdebugx(1, "instcmd: ser_send test.panel.start failed");
			return STAT_INSTCMD_UNKNOWN; /* Or Failed when it get defined */
		}
		else
		{
			upsdebugx(1, "instcmd: test.panel.start returned: %s", temp);
			test_in_progress = VICTRON_SYSTEM_TEST;
			return STAT_INSTCMD_HANDLED;
		}
	}
	else if(!strcasecmp(cmdname, "bypass.stop"))
	{
		if(get_data("vTi2!",temp))
		{
			upsdebugx(1, "instcmd: ser_send bypass.stop failed");
			return STAT_INSTCMD_UNKNOWN; /* Or Failed when it get defined */
		}
		else
		{
			upsdebugx(1, "instcmd: bypass.stop returned: %s", temp);
			return STAT_INSTCMD_HANDLED;
		}
	}
	else if(!strcasecmp(cmdname, "bypass.start"))
	{
		if(get_data("vTi101!",temp))
		{
			upsdebugx(1, "instcmd: ser_send bypass.start failed");
			return STAT_INSTCMD_UNKNOWN; /* Or Failed when it get defined */
		}
		else
		{
			upsdebugx(1, "instcmd: bypass.start returned: %s", temp);
			test_in_progress = VICTRON_BYPASS_TEST;
			return STAT_INSTCMD_HANDLED;
		}
	}
	else
	{
		upsdebugx(1, "instcmd: unknown command: [%s] [%s]", cmdname, extra);
		return STAT_INSTCMD_UNKNOWN;
	}
}

void upsdrv_initinfo(void)
{

	if (model_name)
		dstate_setinfo("ups.model", "%s", model_name);

	upsh.instcmd = instcmd;

	dstate_addcmd("test.battery.start");
	dstate_addcmd("test.battery.stop");
	dstate_addcmd("calibrate.start");
	dstate_addcmd("calibrate.stop");
	dstate_addcmd("test.panel.start");	/* We need a GeneralSystemTest, but use this one instead */
	dstate_addcmd("test.panel.stop");	/* We need a GeneralSystemTest, but use this one instead */
	dstate_addcmd("bypass.start");
	dstate_addcmd("bypass.stop");
}


void upsdrv_updateinfo(void)
{
	int flags;
	char temp[ LENGTH_TEMP ];
	char test_result[ LENGTH_TEMP ];
	int runtime_sec = -1;

	if (start_is_datastale)
	{
		if (get_data("vDS?",temp)) return;
		if (strcmp(temp+3,"NA"))
			exist_ups_serial=1;

		if (get_data("vBT?",temp)) return;
		if (strcmp(temp+3,"NA"))
			exist_ups_temperature =1;

		if (get_data("vO0I?",temp)) return;
		if (strcmp(temp+4,"NA"))
			exist_output_current =1;

		if (get_data("vBC?",temp)) return;
		if (strcmp(temp+3,"NA"))
			exist_battery_charge = 1;

		if (get_data("vBI?",temp)) return;
		if (strcmp(temp+3,"NA"))
			exist_battery_charge = 1;

		if (get_data("vBT?",temp)) return;
		if (strcmp(temp+3,"NA"))
			exist_battery_temperature = 1;

		if (get_data("vBt?",temp)) return;
		if (strcmp(temp+3,"NA"))
			exist_battery_runtime = 1;

		start_is_datastale = 0;
	}




	/* ups.status */
	if (get_data("vAa?",temp)) return;
	flags = atoi (temp+3);

	status_init();

	if (flags & VICTRON_OVER)
		status_set("OVER");

	if (flags & VICTRON_RB)
		status_set("RB");

	if (flags & VICTRON_LB)
		status_set("LB");

	if (flags & VICTRON_OB)
		status_set("OB");
	else
		status_set("OL");

	/* Get UPS test results */
	if (get_data("vTr?",temp)) return;
	if (get_data("vTd?",test_result)) return;

	switch(atoi(temp+3))
	{
		case 1:
			upsdebugx(1, "upsdrv_updateinfo: test %i result = Done, Passed: %s",test_in_progress,test_result+3);
			test_in_progress = VICTRON_NO_TEST;
			break;

		case 2:
			upsdebugx(1, "upsdrv_updateinfo: test %i result = Done, Warning: %s",test_in_progress,test_result+3);
			test_in_progress = VICTRON_NO_TEST;
			break;

		case 3:
			upsdebugx(1, "upsdrv_updateinfo: test %i result = Done, Error: %s",test_in_progress,test_result+3);
			test_in_progress = VICTRON_NO_TEST;
			break;

		case 4:
			upsdebugx(1, "upsdrv_updateinfo: test %i result = Aborted: %s",test_in_progress,test_result+3);
			test_in_progress = VICTRON_NO_TEST;
			break;

		case 5:
			if(test_in_progress==VICTRON_CALIBRATION)
				status_set("CAL");    /* calibration in progress */
			upsdebugx(1, "upsdrv_updateinfo: test %i result = In Progress: %s",
			test_in_progress,test_result+3);
			break;

		case 6:
			upsdebugx(1, "upsdrv_updateinfo: test result = No test initiated: %s",
			test_result+3);
			break;

		default:
			upsdebugx(1, "upsdrv_updateinfo: unknown test result: %s / %s",temp+3,test_result+3);
			break;
	}

	status_commit();
	/* dstate_dataok(); */

	upsdebugx(1, "upsdrv_updateinfo: ups.status = %s\n", dstate_getinfo("ups.status"));


	/************** ups.x ***************************/

	/* ups model */
	if (!model_name)
	{
		if (get_data("vDM?",temp)) return;
		dstate_setinfo("ups.model", "%s", temp+3);
		upsdebugx(1, "ups.model >%s<>%s<\n",temp,temp+3);
	}



	/* ups.mfr */
	if (get_data("vDm?",temp)) return;
	dstate_setinfo("ups.mfr", "%s", temp+3);
	upsdebugx(1, "ups.mfr >%s<>%s<\n",temp,temp+3);


	/* ups.serial */
	if (exist_ups_serial)
	{
		if (get_data("vDS?",temp)) return;
		dstate_setinfo("ups.serial", "%s", temp+3);
	}
	upsdebugx(1, "ups.serial >%s<>%s<\n",temp,temp+3);

	/* ups.firmware */
	if (get_data("vDV?",temp)) return;
	dstate_setinfo("ups.firmware", "%s", temp+3);
	upsdebugx(1, "ups.firmware >%s<>%s<\n",temp,temp+3);

	/* ups.temperature */
	if (exist_ups_temperature)
	{
		if (get_data("vBT?",temp)) return;
		dstate_setinfo("ups.temperature", "%s", temp+3);
	}
	upsdebugx(1, "ups.temperature >%s<>%s<\n",temp,temp+3);

	/* ups.load */
	if (get_data("vO0L?",temp)) return;
	dstate_setinfo("ups.load", "%s", temp+4);
	upsdebugx(1, "ups.load >%s<>%s<\n",temp,temp+4);



	/* ups protocol */
	/*
	if (get_data("vDC?",temp)) return;
	dstate_setinfo("ups.protocol", "%s", temp+3;
	upsdebugx(1, "ups.protocol >%s<>%s<\n",temp,temp+3;
	*/


	/************** input.x *****************/

	/* input.voltage */
	if (get_data("vI0U?",temp)) return;
	dstate_setinfo("input.voltage", "%s", temp+4);
	upsdebugx(1, "input.voltage >%s<>%s<\n",temp,temp+4);


	/* input.transfer.low */
	if (get_data("vFi?",temp)) return;
	dstate_setinfo("input.transfer.low", "%s", temp+3);
	upsdebugx(1, "input.transfer.low >%s<>%s<\n",temp,temp+3);


	/* input.transfer.high */
	if (get_data("vFj?",temp)) return;
	dstate_setinfo("input.transfer.high", "%s", temp+3);
	upsdebugx(1, "input.transfer.high >%s<>%s<\n",temp,temp+3);


	/* input.frequency */
	if (get_data("vI0f?",temp)) return;
	dstate_setinfo("input.frequency", "%2.1f", atof(temp+4) / 10.0);
	upsdebugx(1, "input.frequency >%s<>%s<\n",temp,temp+4);


	/*************** output.x ********************************/


	/* output.voltage */
	if (get_data("vO0U?",temp)) return;
	dstate_setinfo("output.voltage", "%s", temp+4);
	upsdebugx(1, "output.voltage >%s<>%s<\n",temp,temp+4);


	/* output.frequency */
	if (get_data("vOf?",temp)) return;
	dstate_setinfo("output.frequency", "%2.1f", atof(temp+3) / 10.0);
	upsdebugx(1, "output.frequency >%s<>%s<\n",temp,temp+3);


	/* output.current */
	if (exist_output_current)
	{
		if (get_data("vO0I?",temp)) return;
		dstate_setinfo("output.current", "%2.1f", atof(temp+4) / 10.0);
	}
	upsdebugx(1, "output.current >%s<>%s<\n",temp,temp+4);


	/*************** battery.x *******************************/

	/* battery charge */
	if (exist_battery_charge)
	{
		if (get_data("vBC?",temp)) return;
		dstate_setinfo("battery.charge", "%s", temp+3);
	}
	upsdebugx(1, "battery.charge >%s<>%s<\n",temp,temp+3);

	/* battery.voltage */
	if (get_data("vBU?",temp)) return;
	dstate_setinfo("battery.voltage", "%2.1f", atof(temp+3) / 10.0);
	upsdebugx(1, "battery.voltage >%s<>%s<\n",temp,temp+3);


	/* battery.current */
	if (exist_battery_current)
	{
		if (get_data("vBI?",temp)) return;
		dstate_setinfo("battery.current", "%2.1f", atof(temp+3) / 10.0);
	}
	upsdebugx(1, "battery.current >%s<>%s<\n",temp,temp+3);

	/* battery.temperature */
	if (exist_battery_temperature)
	{
		if (get_data("vBT?",temp)) return;
		dstate_setinfo("battery.temperature", "%s", temp+3);
	}
	upsdebugx(1, "battery.temperature >%s<>%s<\n",temp,temp+3);


	/* battery.runtime */
	if (exist_battery_runtime)
	{
		if (get_data("vBt?",temp)) return;
		runtime_sec = strtol(temp+3, NULL, 10)*60;
		snprintf(temp, sizeof(temp), "%d", runtime_sec);
		dstate_setinfo("battery.runtime", "%s", temp);
	}
	upsdebugx(1, "battery.runtime >%s<>%d<\n",temp,runtime_sec);

	dstate_dataok();
}

void upsdrv_shutdown(void)
{
	ser_send(upsfd, "vCc0!%c", ENDCHAR);
	usleep(UPS_DELAY);

	ser_send(upsfd, "vCb%i!%c", sdwdelay, ENDCHAR);
}

void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "usd", "Seting delay before shutdown");
	addvar(VAR_VALUE, "modelname", "Seting model name");
}

void upsdrv_initups(void)
{
	char temp[ LENGTH_TEMP ], *usd = NULL;  /* = NULL je dulezite jen pro prekladac */


	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B1200);


	if ((usd = getval("usd")))
	{
		sdwdelay=atoi(usd);
		upsdebugx(1, "(-x) Delay before shutdown %i",sdwdelay);
	}

	if ((model_name = getval("modelname")))
	{
		/* kdyz modelname nebylo zadano je vraceno NULL*/
		upsdebugx(1, "(-x) UPS Name %s",model_name);
	}

	/* inicializace a synchronizace UPS */

	ser_send_char(upsfd, ENDCHAR);
	usleep (UPS_LONG_DELAY);
	ser_send(upsfd, "?%c", ENDCHAR);
	usleep (UPS_LONG_DELAY);
	ser_get_line(upsfd, temp, sizeof(temp), ENDCHAR, IGNCHARS, 3, 0);
	ser_send(upsfd, "?%c", ENDCHAR);
	usleep (UPS_LONG_DELAY);
	ser_get_line(upsfd, temp, sizeof(temp), ENDCHAR, IGNCHARS, 3, 0);
	ser_send(upsfd, "?%c", ENDCHAR);
	usleep (UPS_DELAY);
	ser_get_line(upsfd, temp, sizeof(temp), ENDCHAR, IGNCHARS, 3, 0);


	/* the upsh handlers can't be done here, as they get initialized
	 * shortly after upsdrv_initups returns to main.
	 */
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
