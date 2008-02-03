/* nitram.c - Network UPS Tools driver for Nitram Systems units
   This driver support:
    - Nitram Elite 2005 (manufactured by Cyber Power)

   Copyrights:
   (C) 2005 Olivier Albiez <oalbiez@free.fr>
   (C) 2005 Nadine Albiez <oalbiez@free.fr>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/


#include <string.h>

#include "main.h"
#include "serial.h"
#include "nitram.h"

#define ENDCHAR	'\r'
#define IGNCHARS ""
#define UPSDELAY 50000

#define VOLTAGE_RANGE 3


struct status_t
{
	char input_tag;
	char input[5];
	char output_tag;
	char output[5];
	char load_tag;
	char load[3];
	char battery_tag;
	char battery[3];
	char temperature_tag;
	char temperature[3];
	char frequency_tag;
	char frequency[5];
	char flags_tag;
	char flags[2];
};


struct fields_t
{
	char* field[10];
	int count;
};


struct buffer_t
{
	char frame[128];
	char* payload;
	size_t length;
};


static void send_command(const char* command)
{
	ser_send_pace(upsfd, UPSDELAY, "%s\r", command);
}


static void send_command_and_flush(const char* command)
{
	struct buffer_t reply;
	send_command(command);
	usleep(100000);
	ser_get_line(upsfd, reply.frame, sizeof(reply.frame), ENDCHAR, IGNCHARS, 3, 0);
}


static int execute_command(const char* command, struct buffer_t* reply)
{
	int status;
	send_command(command);
	usleep(100000);
	status = ser_get_line(upsfd, reply->frame, sizeof(reply->frame), ENDCHAR, IGNCHARS, 3, 0);

	if (status <= 1)
	{
		ser_comm_fail("Invalid frame size");
		memset(reply, 0, sizeof(struct buffer_t));
		return 0;
	}
	else if (reply->frame[0] != '#')
	{
		ser_comm_fail("Invalid frame marker 0x%02x", reply->frame[0]);
		memset(reply, 0, sizeof(struct buffer_t));
		return 0;
	}

	ser_comm_good();
	reply->payload = reply->frame+1;
	reply->length = status-1;
	return 1;
}


static struct fields_t extract_fields(struct buffer_t* reply)
{
	struct fields_t result;
	char* payload = reply->payload;
	char* position;
	result.count = 0;
	while ((position = strchr(payload, ',')) != NULL)
	{
		*position++ = 0;
		result.field[result.count] = payload;
		++result.count;
		payload = position;
	}
	result.field[result.count] = payload;
	++result.count;
	return result;
}


static int extract_status(struct buffer_t* reply, struct status_t** status)
{
	if (reply->length != sizeof(struct status_t))
	{
		return 0;
	}

	*status = (struct status_t*)(reply->payload);
	#define CHECK_AND_CLEAR_TAG(name, value) if ((*status)->name != (value)) return 0; else (*status)->name = 0
	CHECK_AND_CLEAR_TAG(input_tag, 'I');
	CHECK_AND_CLEAR_TAG(output_tag, 'O');
	CHECK_AND_CLEAR_TAG(load_tag, 'L');
	CHECK_AND_CLEAR_TAG(battery_tag, 'B');
	CHECK_AND_CLEAR_TAG(temperature_tag, 'T');
	CHECK_AND_CLEAR_TAG(frequency_tag, 'F');
	CHECK_AND_CLEAR_TAG(flags_tag, 'S');
	#undef CHECK_AND_CLEAR_TAG
	return 1;
}


static int get_identification(struct buffer_t* reply)
{
	int tries;
	ser_flush_in(upsfd, IGNCHARS, 0);
	for (tries = 0; tries < 3; tries++)
	{
		if (execute_command("P4", reply) == 1)
			return 1;
	}
	upslogx(LOG_INFO, "Giving up on hardware detection after 3 tries");
	return 0;
}


static int instcmd(const char *command, const char *extra)
{
	if (!strcasecmp(command, "beeper.off")) {
		/* compatibility mode for old command */
		upslogx(LOG_WARNING,
			"The 'beeper.off' command has been renamed to 'beeper.disable'");
		return instcmd("beeper.disable", NULL);
	}

	if (!strcasecmp(command, "beeper.on")) {
		/* compatibility mode for old command */
		upslogx(LOG_WARNING,
			"The 'beeper.on' command has been renamed to 'beeper.enable'");
		return instcmd("beeper.enable", NULL);
	}

	#define DEFINE_COMMAND(name, nitram_command) \
		if (strcasecmp(command, (name)) == 0) \
		{ \
			send_command_and_flush(nitram_command); \
			return STAT_INSTCMD_HANDLED; \
		}
	DEFINE_COMMAND("test.battery.start", "T.1");
	DEFINE_COMMAND("test.battery.stop", "CT");
	DEFINE_COMMAND("beeper.enable", "C7:1");
	DEFINE_COMMAND("beeper.disable", "C7:0");
	#undef DEFINE_COMMAND

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", command);
	return STAT_INSTCMD_UNKNOWN;
}


static int setvar(const char *varname, const char *val)
{
	struct buffer_t reply;
	char command[50];

	if (strcasecmp(varname, "battery.charge.low") == 0)
	{
		snprintf(command, sizeof(command), "C4:%s", val);
		if (execute_command(command, &reply) == 1)
		{
			dstate_setinfo("battery.charge.low", val);
		}
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "setvar: unknown var [%s]", varname);
	return STAT_SET_UNKNOWN;
}


void upsdrv_initinfo(void)
{
	struct buffer_t reply;
	struct fields_t fields;

	if (get_identification(&reply) == 1)
	{
		fields = extract_fields(&reply);
		dstate_setinfo("ups.model", "%s", fields.field[0]);
		dstate_setinfo("ups.firmware", "%s", fields.field[1]);
		dstate_setinfo("ups.mfr", "CyberPower for Nitram");
		dstate_setinfo("driver.version.internal", "%s", DRV_VERSION);
	}
	else
	{
		fatalx(EXIT_FAILURE, "Unable to get initial hardware info string");
	}

	dstate_setinfo("battery.charge.low", "20");

	if (execute_command("P1", &reply) == 1)
	{
		fields = extract_fields(&reply);
		dstate_setinfo("output.voltage.nominal", "%s", fields.field[0]);
		dstate_setinfo("input.voltage.maximum", "%s", fields.field[1]);
		dstate_setinfo("input.voltage.minimum", "%s", fields.field[2]);
		dstate_setinfo("battery.charge.low", "%s", fields.field[3]);
	}

	if (execute_command("P3", &reply) == 1)
	{
		fields = extract_fields(&reply);
		dstate_setinfo("battery.voltage", "%s", fields.field[0]);
		dstate_setinfo("battery.packs", "%s", fields.field[1]);
		dstate_setinfo("battery.current", "%s", fields.field[2]);
	}

	dstate_setflags("battery.charge.low", ST_FLAG_STRING | ST_FLAG_RW);
	dstate_setaux("battery.charge.low", 20);
	dstate_addcmd("test.battery.start");
	dstate_addcmd("test.battery.stop");
	dstate_addcmd("beeper.enable");
	dstate_addcmd("beeper.disable");
	dstate_addcmd("beeper.on");
	dstate_addcmd("beeper.off");
	upsh.instcmd = instcmd;
	upsh.setvar = setvar;
}


void upsdrv_updateinfo(void)
{
	struct buffer_t reply;
	struct status_t* status;

	if (execute_command("D", &reply) == 0)
	{
		dstate_datastale();
		return;
	}

	if (extract_status(&reply, &status) == 0)
	{
		dstate_datastale();
		return;
	}

	dstate_setinfo("input.frequency", "%s", status->frequency);
	dstate_setinfo("ups.temperature", "%s", status->temperature);
	dstate_setinfo("battery.charge", "%s", status->battery);
	dstate_setinfo("ups.load", "%s", status->load);
	dstate_setinfo("input.voltage", "%s", status->input);
	dstate_setinfo("output.voltage", "%s", status->output);

	status_init();
	if (status->flags[0] & 0x40)
		status_set("OB");
	else
		status_set("OL");

	if (status->flags[0] & 0x20)
		status_set("LB");

	if (atoi(status->input) > atoi(status->output) + VOLTAGE_RANGE)
		status_set("TRIM");

	if (atoi(status->input) < atoi(status->output) - VOLTAGE_RANGE)
		status_set("BOOST");

	if ((atoi(status->battery) == 100) && !(status->flags[0] & 0x40))
		status_set("BYPASS");

	status_commit();
	dstate_dataok();

	upsdebugx(LOG_DEBUG, "status:%s; load:%s; battery:%s; input:%s; output:%s; frequency:%s;",
		dstate_getinfo("ups.status"),
		status->load,
		status->battery,
		status->input,
		status->output,
		status->frequency);
}


void upsdrv_shutdown(void)
{
	fatalx(EXIT_FAILURE, "shutdown not supported");
}


void upsdrv_help(void)
{
}


void upsdrv_makevartable(void)
{
}


void upsdrv_banner(void)
{
	printf("Network UPS Tools - Nitram UPS driver %s (%s)\n\n",
		DRV_VERSION, UPS_VERSION);
}


void upsdrv_initups(void)
{
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B2400);

	/* dtr high, rts high */
	ser_set_dtr(upsfd, 1);
	ser_set_rts(upsfd, 1);
}


void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
