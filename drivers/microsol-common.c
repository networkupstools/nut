/* microsol-common.c - common framework for Microsol Solis-based UPS hardware

   Copyright (C) 2004  Silvino B. Magalhães    <sbm2yk@gmail.com>
                 2019  Roberto Panerai Velloso <rvelloso@gmail.com>
                 2021  Ygor A. S. Regados      <ygorre@tutanota.com>

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

   2021/03/19 - Version 0.70 - Initial release, based on solis driver

*/

#include "main.h"	/* Includes "config.h", must be first */

#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include "serial.h"
#include "nut_float.h"
#include "nut_stdint.h"
#include "microsol-common.h"
#include "timehead.h"

#define false 0
#define true 1
#define RESP_END    0xFE
#define ENDCHAR     13		/* replies end with CR */
/* solis commands */
#define CMD_UPSCONT 0xCC
#define CMD_SHUT    0xDD
#define CMD_SHUTRET 0xDE
#define CMD_EVENT   0xCE
#define CMD_DUMP    0xCD

#define M_UNKN     "Unknown solis model"
#define NO_SOLIS   "Solis not detected! aborting ..."
#define UPS_DATE   "UPS Date %4d/%02d/%02d"
#define SYS_DATE   "System Date %4d/%02d/%02d day of week %s"
#define ERR_PACK   "Wrong package"
#define NO_EVENT   "No events"
#define UPS_TIME   "UPS internal Time %0d:%02d:%02d"
#define PRG_DAYS   "Programming Shutdown Sun  Mon  Tue  Wed  Thu  Fri  Sat"
#define PRG_ONON   "External shutdown programming active"
#define PRG_ONOU   "Internal shutdown programming active"
#define TIME_OFF   "UPS Time power off %02d:%02d"
#define TIME_ON    "UPS Time power on %02d:%02d"
#define PRG_ONOF   "Shutdown programming not activated"
#define TODAY_DD   "Shutdown today at %02d:%02d"
#define SHUT_NOW   "Shutdown now!"

#define FMT_DAYS   "                      %d    %d    %d    %d    %d    %d    %d"

/* Date, time and programming group */
static int const BASE_YEAR = 1998; /* Note: code below uses relative "unsigned char" years */
static int device_day, device_month, device_year;
static int device_hour, device_minute, device_second;
static int power_off_hour, power_off_minute;
static int power_on_hour, power_on_minute;
static uint8_t device_days_on = 0, device_days_off = 0, days_to_shutdown = 0;

static int isprogram = 0, progshut = 0, prgups = 0;
static int hourshut, minshut;

static int host_year, host_month, host_day;
static int host_week;
static int host_hour, host_minute, host_second;

/* buffers */
unsigned char received_packet[PACKET_SIZE];

/* Identification */
const char *model_name;
unsigned int ups_model;
bool_t input_220v, output_220v;

/* logical */
bool_t detected = 0;
bool_t line_unpowered, overheat;
bool_t overload, critical_battery, inverter_working;
static bool_t recharging;
static bool_t packet_parsed = false;

double input_voltage, input_current, input_frequency;
double output_voltage, output_current, output_frequency;

double input_low_limit, input_high_limit;

int battery_extension;
double battery_voltage, battery_charge;
double temperature;

double apparent_power, real_power, ups_load;
int load_power_factor, nominal_power;

/**
 * Convert standard days string to firmware format
 * This is needed because UPS sends binary date rotated
 * from current week day (first bit = current day)
 */
static char *convert_days(char *cop)
{
	static char alt[8];

	int ish, fim;
	/* FIXME? Are range-checks needed for values more than 6? wire noise etc? */
	if (host_week == 6)
		ish = 0;
	else
		ish = 1 + host_week;

	fim = 7 - ish;
	/* rotate left only 7 bits */

	if (fim > 0) {
		memcpy(alt, &cop[ish], (size_t)fim);
	} else {
		fatalx(EXIT_FAILURE, "%s: value out of range: %d (%d)",
			__func__, fim, ish);
	}

	if (ish > 0)
		memcpy(&alt[fim], cop, (size_t)ish);

	alt[7] = 0;		/* string terminator */

	return alt;
}

/** Convert bitstring (e.g. 1100101) to binary */
static uint8_t bitstring_to_binary(char *binStr)
{
	uint8_t result = 0;
	unsigned int i;

	for (i = 0; i < 7; ++i) {
		char ch = binStr[i];
		if (ch == '1' || ch == '0')
			result += ((ch - '0') << (6 - i));
		else
			return 0;
	}

	return result;
}

/**
 * Revert firmware format to standard string binary days
 * This is needed because UPS sends binary date rotated
 * from current week day (first bit = current day)
 */
static uint8_t revert_days(unsigned char firmware_week)
{
	char ordered_week[8];
	int i;

	for (i = 0; i < (6 - host_week); ++i)
		ordered_week[i] = (firmware_week >> (5 - host_week - i)) & 0x01;

	for (i = 0; i < host_week + 1; ++i)
		ordered_week[i + (6 - host_week)] = (firmware_week >> (6 - i)) & 0x01;

	for (i = 0; i < 7; i++)
		ordered_week[i] += '0';

	ordered_week[7] = 0;	/* string terminator */

	return bitstring_to_binary(ordered_week);
}

/** Parse time string from parameters and store their values */
static bool_t set_schedule_time(char *hour, bool_t off_time)
{
	int string_hour, string_minute;

	if ((strlen(hour) != 5) || (sscanf(hour, "%d:%d", &string_hour, &string_minute) != 2))
		return 0;

	if (off_time) {
		power_off_hour = string_hour;
		power_off_minute = string_minute;
	} else {
		power_on_hour = string_hour;
		power_on_minute = string_minute;
	}
	return 1;
}

/** Send immediate shutdown command to UPS */
static void send_shutdown(void)
{
	unsigned int i;

	for (i = 0; i < 10; i++)
		ser_send_char(upsfd, CMD_SHUT);

	upslogx(LOG_NOTICE, "UPS shutdown command sent");
}

/** Store clock updates and shutdown schedules to UPS */
static void save_ups_config(void)
{
	unsigned int i;
	int checksum = 0;
	unsigned char configuration_packet[12];

	/* Prepare configuration packet */
	/* FIXME? Check for overflows with int => char truncations? */
	configuration_packet[0] = (unsigned char)0xCF;
	configuration_packet[1] = (unsigned char)host_hour;
	configuration_packet[2] = (unsigned char)host_minute;
	configuration_packet[3] = (unsigned char)host_second;
	configuration_packet[4] = (unsigned char)power_on_hour;
	configuration_packet[5] = (unsigned char)power_on_minute;
	configuration_packet[6] = (unsigned char)power_off_hour;
	configuration_packet[7] = (unsigned char)power_off_minute;
	configuration_packet[8] = (unsigned char)(host_week << 5);
	configuration_packet[8] = (unsigned char)configuration_packet[8] | (unsigned char)host_day;
	configuration_packet[9] = (unsigned char)(host_month << 4);
	configuration_packet[9] = (unsigned char)configuration_packet[9] | (unsigned char)(host_year - BASE_YEAR);
	configuration_packet[10] = (unsigned char)device_days_off;

	/* MSB zero */
	configuration_packet[10] = configuration_packet[10] & (~(0x80));

	/* Calculate packet content checksum */
	for (i = 0; i < 11; i++) {
		checksum += configuration_packet[i];
	}
	/* FIXME? Does truncation to char have same effect as %256 ? */
	configuration_packet[11] = (unsigned char)(checksum % 256);

	/* Send final packet and checksum to serial port */
	for (i = 0; i < 12; i++) {
		ser_send_char(upsfd, configuration_packet[i]);
	}
}

/** Log shut-down schedule data stored in UPS */
static void print_info(void)
{
	/* sunday, monday, tuesday, wednesday, thursday, friday, saturday */
	char week_days[7] = { 0, 0, 0, 0, 0, 0, 0 };
	unsigned int i;

	upslogx(LOG_NOTICE, UPS_DATE, device_year, device_month, device_day);
	upslogx(LOG_NOTICE, UPS_TIME, device_hour, device_minute, device_second);

	if (prgups > 0) {
		/* this is the string to binary standard */
		for (i = 0; i < 7; i++) {
			week_days[i] = (days_to_shutdown >> (6 - i)) & 0x01;
		}

		if (prgups == 3)
			upslogx(LOG_NOTICE, PRG_ONOU);
		else
			upslogx(LOG_NOTICE, PRG_ONON);

		upslogx(LOG_NOTICE, TIME_ON, power_on_hour, power_on_minute);
		upslogx(LOG_NOTICE, TIME_OFF, power_off_hour, power_off_minute);

		upslogx(LOG_NOTICE, PRG_DAYS);

		upslogx(LOG_NOTICE, FMT_DAYS, week_days[0], week_days[1], week_days[2], week_days[3], week_days[4], week_days[5], week_days[6]);
	} else {
		upslogx(LOG_NOTICE, PRG_ONOF);
	}
}

/** Parses received packet with UPS readings and configuration. */
static void scan_received_pack(void)
{
	/* UPS internal time */
	device_year = (received_packet[19] & 0x0F) + BASE_YEAR;
	device_month = (received_packet[19] & 0xF0) >> 4;
	device_day = (received_packet[18] & 0x1F);

	device_hour = received_packet[11];
	device_minute = received_packet[10];
	device_second = received_packet[9];

	/* UPS power cycle schedule if in programmed shutdown mode */
	if (prgups == 3) {
		device_days_on = received_packet[17];
		days_to_shutdown = revert_days(device_days_on);

		/* Automatic UPS power-off time */
		power_off_hour = received_packet[15];
		power_off_minute = received_packet[16];

		/* Automatic UPS power-on time */
		power_on_hour = received_packet[13];
		power_on_minute = received_packet[14];
	}

	/* These UPS have 110V- or 220V-output models */
	if ((0x01 & received_packet[20]) == 0x01) {
		output_220v = 1;
	}

	/* UPS state flags */
	critical_battery = (0x04 & received_packet[20]) == 0x04;
	inverter_working = (0x08 & received_packet[20]) == 0x08;
	overheat = (0x10 & received_packet[20]) == 0x10;
	line_unpowered = (0x20 & received_packet[20]) == 0x20;
	overload = (0x80 & received_packet[20]) == 0x80;

	recharging = (0x02 & received_packet[20]) == 0x02;
	if (line_unpowered) {
		recharging = false;
	}

	/* Check if input voltage is 110V or 220V */
	if ((0x40 & received_packet[20]) == 0x40) {
		input_220v = 1;
	} else {
		input_220v = 0;
	}

	/* Internal battery temperature */
	temperature = 0x7F & received_packet[4];
	if (0x80 & received_packet[4]) {
		temperature -= 128;
	}

	/* Parse model-specific data (current and voltages).
	 * Doing it here as these values are used for the next calculations. */
	scan_received_pack_model_specific();

	ups_load = (apparent_power / nominal_power) * 100.0;

	if (battery_charge > 100.0) {
		battery_charge = 100.0;
	} else if (battery_charge < 0.0) {
		battery_charge = 0.0;
	}

	output_frequency = 60;
	if (!inverter_working) {
		output_voltage = 0;
		output_frequency = 0;
	}

	if (!line_unpowered && inverter_working)
		output_frequency = input_frequency;

	if (apparent_power < 0)
		load_power_factor = 0;
	else {
		if (d_equal(apparent_power, 0))
			load_power_factor = 100;
		else
			load_power_factor = ((real_power / apparent_power) * 100);

		if (load_power_factor > 100) {
			load_power_factor = 100;
		}
	}

	/* input 110V or 220v */
	if (input_220v == 0) {
		input_low_limit = 75;
		input_high_limit = 150;
	} else {
		input_low_limit = 150;
		input_high_limit = 300;
	}
}

/**
 * Start processing of received packets
 *
 * Packet format: 25-bytes binary structure
 *  Byte 1: Packet type/UPS model
 *  Byte 2: Output voltage data
 *  Byte 3: Input voltage data
 *  Byte 4: Battery voltage data
 *  Byte 5: UPS temperature data
 *  Byte 6: Output current data
 *  Byte 7: Electrical relay setup
 *  Byte 8-9: Real power data
 *  Byte 10: UPS clock - seconds
 *  Byte 11: UPS clock - minutes
 *  Byte 12: UPS clock - hours
 *  Byte 13: Zero
 *  Byte 14: UPS scheduler - power-on hour
 *  Byte 15: UPS scheduler - power-on minute
 *  Byte 16: UPS scheduler - power-off hour
 *  Byte 17: UPS scheduler - power-off minute
 *  Byte 18: UPS scheduler - weekdays
 *  Byte 19: UPS clock - day of month
 *  Byte 20: UPS clock - year (since 1998) (left 4 bits) and month (right 4 bits)
 *  Byte 21: UPS flags (power status, battery status, overload, overheat, nominal input voltage, nominal output voltage)
 *  Byte 22-23: Input frequency data
 *  Byte 24: Packet checksum
 *  Byte 25: Packet delimiter, always 0xFE
 */
static void comm_receive(const unsigned char *bufptr, size_t size)
{
	size_t i;

	if (size == PACKET_SIZE) {
		int checksum = 0;

		upsdebug_hex(3, "comm_receive: bufptr", bufptr, size);

		/* Calculate packet checksum */
		for (i = 0; i < PACKET_SIZE - 2; i++) {
			checksum += bufptr[i];
		}
		checksum = checksum % 256;
		upsdebugx(4, "%s: calculated checksum = 0x%02x, bufptr[23] = 0x%02x", __func__, checksum, bufptr[23]);

		/* Only proceed if checksum matches and packet delimiter is found */
		if (checksum == bufptr[23] && bufptr[24] == 254) {
			upsdebugx(4, "%s: valid packet received", __func__);
			memcpy(received_packet, bufptr, PACKET_SIZE);

			if ((received_packet[0] & 0xF0) == 0xA0 || (received_packet[0] & 0xF0) == 0xB0) {
				/* If UPS still not detected, compare with available lists */
				if (!detected) {
					ups_model = received_packet[0];

					detected = true;
				}

				if (!ups_model_defined()) {
					upslogx(LOG_DEBUG, M_UNKN);
				}

				scan_received_pack();
			}
		}
	}
}

/** Refresh host time variables */
static void refresh_host_time(void)
{
	const time_t epoch = time(NULL);
	struct tm now;

	localtime_r(&epoch, &now);
	host_year = now.tm_year + 1900;
	host_month = now.tm_mon + 1;
	host_day = now.tm_mday;
	host_week = now.tm_wday;
	host_hour = now.tm_hour;
	host_minute = now.tm_min;
	host_second = now.tm_sec;
}

/** Query shut-down schedule configuration */
static void setup_poweroff_schedule(void)
{
	bool_t i1 = 0, i2 = 0;
	char *daysoff;

	refresh_host_time();

	if (testvar("prgshut")) {
		prgups = atoi(getval("prgshut"));
	}

	if (prgups > 0 && prgups < 3) {
		if (testvar("daysweek")) {
			device_days_on = bitstring_to_binary(convert_days(getval("daysweek")));
		}

		if (testvar("daysoff")) {
			daysoff = getval("daysoff");
			days_to_shutdown = bitstring_to_binary(daysoff);
			device_days_off = bitstring_to_binary(convert_days(daysoff));
		}

		if (testvar("houron")) {
			i1 = set_schedule_time(getval("houron"), 0);
		}

		if (testvar("houroff")) {
			i2 = set_schedule_time(getval("houroff"), 1);
		}

		if (i1 && i2 && (device_days_on > 0)) {
			isprogram = 1;

			/* If configured to shut-down UPS, push schedule to internal configuration */
			if (prgups == 2) {
				save_ups_config();
			}
		} else {
			if (i2 == 1 && device_days_off > 0) {
				isprogram = 1;
				device_days_on = device_days_off;
			}
		}
	}
}

/** Check shut-down schedule and sets system to shut down if needed */
static void check_shutdown_schedule(void)
{
	bool_t is_shutdown_day = 0;

	if (isprogram || prgups == 3) {
		refresh_host_time();

		is_shutdown_day = (days_to_shutdown >> (6 - host_week)) & 0x01;

		if (is_shutdown_day) {
			upslogx(LOG_NOTICE, TODAY_DD, hourshut, minshut);

			if (host_hour == hourshut && host_minute >= minshut) {
				upslogx(LOG_NOTICE, SHUT_NOW);
				progshut = 1;
			}
		}
	}
}

/** Resynchronizes packet boundaries */
static void resynchronize_packet(void) {
	unsigned char sync_received_byte = 0;
	unsigned short i;

	/* Flush serial port buffers */
	ser_flush_io(upsfd);

	upsdebugx(3, "%s: Synchronizing packet boundaries...", __func__);

	/*
	 * - Read until end-of-response character (0xFE):
	 * read up to 3 packets in size before giving up
	 * synchronizing with the device.
	 */
	for (i = 0; i < PACKET_SIZE * 3 && sync_received_byte != RESP_END; i++) {
		ser_get_char(upsfd, &sync_received_byte, 3, 0);
	}

	/* If no packet boundary was found, terminate communication */
	if (sync_received_byte != RESP_END) {
		fatalx(EXIT_FAILURE, NO_SOLIS);
	}
}

/** Synchronize packet receiving and setup basic variables */
static void get_base_info(void)
{
	unsigned char packet[PACKET_SIZE];
	ssize_t tam;

	if (testvar("battext")) {
		battery_extension = atoi(getval("battext"));
	}

	setup_poweroff_schedule();

	/* dummy read attempt to sync - throw it out */
	upsdebugx(3, "%s: sending CMD_UPSCONT and ENDCHAR", __func__);
	ser_send(upsfd, "%c%c", CMD_UPSCONT, ENDCHAR);

	resynchronize_packet ();

	upsdebugx(4, "%s: requesting %d bytes from ser_get_buf_len()", __func__, PACKET_SIZE);
	tam = ser_get_buf_len(upsfd, packet, PACKET_SIZE, 3, 0);
	upsdebugx(2, "%s: received %zd bytes from ser_get_buf_len()", __func__, tam);
	if (tam > 0 && nut_debug_level >= 4) {
		upsdebug_hex(4, "received from ser_get_buf_len()", packet, (size_t)tam);
	}
	comm_receive(packet, (size_t)tam);

	if (!detected) {
		fatalx(EXIT_FAILURE, NO_SOLIS);
	}

	set_ups_model();

	/* Setup power-off times */
	if (prgups != 0) {
		if (prgups == 1) {
			/* If only this host is meant to be powered off, use proper time. */
			hourshut = power_off_hour;
			minshut = power_off_minute;
		} else {
			/* If the UPS is to be powered off too, give
			 * a 5-minute grace time to shutdown hosts */
			if (power_off_minute < 5) {
				if (power_off_hour > 1)
					hourshut = power_off_hour - 1;
				else
					hourshut = 23;

				minshut = 60 - (5 - power_off_minute);
			} else {
				hourshut = power_off_hour;
				minshut = power_off_minute - 5;
			}
		}
	}

	/* manufacturer */
	dstate_setinfo("ups.mfr", "%s", "APC");

	dstate_setinfo("ups.model", "%s", model_name);
	dstate_setinfo("input.transfer.low", "%03.1f", input_low_limit);
	dstate_setinfo("input.transfer.high", "%03.1f", input_high_limit);

	dstate_addcmd("shutdown.return");	/* CMD_SHUTRET */
	dstate_addcmd("shutdown.stayoff");	/* CMD_SHUT */

	upslogx(LOG_NOTICE, "Detected %s on %s", dstate_getinfo("ups.model"), device_path);

	print_info();
}

/** Retrieves new packet from serial connection and parses it */
static void get_updated_info(void)
{
	unsigned char temp[256];
	ssize_t tam;

	check_shutdown_schedule();

	/* get update package */
	temp[0] = 0;		/* flush temp buffer */

	upsdebugx(3, "%s: requesting %d bytes from ser_get_buf_len()", __func__, PACKET_SIZE);
	tam = ser_get_buf_len(upsfd, temp, PACKET_SIZE, 3, 0);

	upsdebugx(2, "%s: received %zd bytes from ser_get_buf_len()", __func__, tam);
	if (tam > 0 && nut_debug_level >= 4)
		upsdebug_hex(4, "received from ser_get_buf_len()", temp, (size_t)tam);

	packet_parsed = false;
	if (temp[24] == RESP_END) {
		/* Packet boundary found, process packet */
		comm_receive(temp, (size_t)tam);

		packet_parsed = true;
	} else {
		/* Malformed packet received, possible boundary desynchronization. */
		upsdebugx(3, "%s: Malformed packet received, trying to resynchronize...", __func__);

		resynchronize_packet ();
	}
}

static int instcmd(const char *cmdname, const char *extra)
{
	/* Power-cycle UPS */
	if (!strcasecmp(cmdname, "shutdown.return")) {
		ser_send_char(upsfd, CMD_SHUTRET);	/* 0xDE */
		return STAT_INSTCMD_HANDLED;
	}

	/* Power-off UPS */
	if (!strcasecmp(cmdname, "shutdown.stayoff")) {
		ser_send_char(upsfd, CMD_SHUT);	/* 0xDD */
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s] [%s]", cmdname, extra);
	return STAT_INSTCMD_UNKNOWN;
}

void upsdrv_initinfo(void)
{
	get_base_info();

	upsh.instcmd = instcmd;
}

void upsdrv_updateinfo(void)
{
	get_updated_info();

	if (packet_parsed) {
		dstate_setinfo("battery.charge", "%03.1f", battery_charge);
		dstate_setinfo("battery.voltage", "%02.1f", battery_voltage);

		dstate_setinfo("input.frequency", "%2.1f", input_frequency);
		dstate_setinfo("input.voltage", "%03.1f", input_voltage);

		dstate_setinfo("output.current", "%03.1f", output_current);
		dstate_setinfo("output.power", "%03.1f", apparent_power);
		dstate_setinfo("output.powerfactor", "%0.2f", load_power_factor / 100.0);
		dstate_setinfo("output.realpower", "%03.1f", real_power);
		dstate_setinfo("output.voltage", "%03.1f", output_voltage);

		dstate_setinfo("ups.temperature", "%2.2f", temperature);
		dstate_setinfo("ups.load", "%03.1f", ups_load);

		status_init();

		if (!line_unpowered) {
			status_set("OL");	/* On line */
		} else {
			status_set("OB");	/* On battery */
		}

		if (overload) {
			status_set("OVER");	/* Overload */
		}

		if (overheat) {
			status_set("OVERHEAT");	/* Overheat */
		}

		if (recharging) {
			status_set("CHRG");	/* Charging battery */
		}

		if (critical_battery) {
			status_set("LB");	/* Critically low battery */
		}

		if (progshut) {
			/* Software-based shutdown now */
			if (prgups == 2)
				send_shutdown();	/* Send command to shutdown UPS in 4-5 minutes */

			/* Workaround for triggering servers' power-off before UPS power-off */
			status_set("LB");
		}

		status_commit();

		dstate_dataok();
	} else {
		/*
		 * If no packet was processed, report data as stale.
		 * Most likely to be fixed on next received packet.
		 */
		dstate_datastale ();
	}
}

/*! @brief Power down the attached load immediately.
 * Basic idea: find out line status and send appropriate command.
 *  - on battery: send normal shutdown, UPS will return by itself on utility
 *  - on line: send shutdown+return, UPS will cycle and return soon.
 */
void upsdrv_shutdown(void)
{
	if (!line_unpowered) {	/* on line */
		upslogx(LOG_NOTICE, "On line, sending power cycle command...");
		ser_send_char(upsfd, CMD_SHUTRET);
	} else {
		upslogx(LOG_NOTICE, "On battery, sending power off command...");
		ser_send_char(upsfd, CMD_SHUT);
	}
}

void upsdrv_help(void)
{
	printf("\nAPC/Microsol options\n\n");
	printf(" Battery extension (AH)\n");
	printf("  battext = 80\n\n");
	printf(" Scheduled UPS power on/off\n");
	printf("  prgshut = 0 (default, no scheduled shutdown)\n");
	printf("  prgshut = 1 (software-based shutdown schedule without UPS power-off)\n");
	printf("  prgshut = 2 (software-based shutdown schedule with UPS power-off)\n");
	printf("  prgshut = 3 (internal UPS shutdown schedule)\n\n");
	printf(" Schedule configuration:\n");
	printf("  daysweek = 1010101 (power on days)\n");
	printf("   daysoff = 1010101 (power off days)\n");
	printf(" where each digit is a day from sun...sat with 0 = off and 1 = on\n\n");
	printf("  houron = hh:mm hh = hour 0-23 mm = minute 0-59 separated with :\n");
	printf("  houroff = hh:mm hh = hour 0-23 mm = minute 0-59 separated with :\n");
	printf(" where houron is power-on hour and houroff is shutdown and power-off hour\n\n");
	printf(" Use daysweek and houron to programming and save UPS power on/off\n");
	printf(" These are valid only if prgshut = 2 or 3\n");
}

void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "battext", "Battery extension (0-80AH)");
	addvar(VAR_VALUE, "prgshut", "Scheduled power-off mode (0-3)");
	addvar(VAR_VALUE, "daysweek", "Days of week for UPS shutdown");
	addvar(VAR_VALUE, "daysoff", "Days of week for driver-induced shutdown");
	addvar(VAR_VALUE, "houron", "Power on hour (hh:mm)");
	addvar(VAR_VALUE, "houroff", "Power off hour (hh:mm)");
}

void upsdrv_initups(void)
{
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B9600);

	ser_set_dtr(upsfd, 1);
	ser_set_rts(upsfd, 0);
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
