/*
 * nutdrv_siemens_sitop.c - model specific routines for the Siemens SITOP UPS500 series
 *
 *
 * Copyright (C) 2018 Matthijs H. ten Berge <m.tenberge@awl.nl>
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
 */

/*
 * Notes:
 * These UPSes operate at 24V DC (both input and output), instead of 110V/230V AC.
 * Therefore, the line input is also referred to by Siemens as 'DC'.
 *
 * The device is configured via DIP-switches.
 * For correct functioning in combination with NUT, set the DIP-switches to the following:
 * switch 1-4: choose whatever suits your situation. Any combination will work with NUT.
 * switch 5 ('=>' / 't'): set to OFF ('t')
 * switch 6-10 (delay): set to OFF (no additional delay)
 * switch 11 (INTERR.): set to ON
 * switch 12 (ON/OFF): set to ON
 *
 * These UPSes are available with serial or USB port.
 * Both are supported by this driver. The version with USB port simply contains
 * a serial-over-USB chip, so as far as this driver is concerned, all models are
 * actually serial models.
 *
 * The FTDI USB-to-serial converters in the USB-models are programmed with a non-standard
 * Product ID (mine had 0403:e0e3), but can be used with the normal ftdi_sio driver:
 * # modprobe ftdi_sio
 * # echo 0403 e0e3 > /sys/bus/usb-serial/drivers/ftdi_sio/new_id
 *
 * This can also be automated via a udev rule:
 * ACTION=="add", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="e0e3", \
 *   RUN+="/sbin/modprobe ftdi_sio", \
 *   RUN+="/bin/sh -c 'echo 0403 e0e3 > /sys/bus/usb-serial/drivers/ftdi_sio/new_id'"
 *
 * Use the following udev rule to create a persistent device name, for example /dev/ttyUPS:
 * SUBSYSTEM=="tty" ATTRS{idVendor}=="0403", ATTRS{idProduct}=="e0e3" SYMLINK+="ttyUPS"
 */

#include "main.h"
#include "serial.h"
#include "nut_stdint.h"

#define DRIVER_NAME	"Siemens SITOP UPS500 series driver"
#define DRIVER_VERSION	"0.02"

#define RX_BUFFER_SIZE 100

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Matthijs H. ten Berge <m.tenberge@awl.nl>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

/* The maximum number of consecutive polls in which the UPS does not provide any data: */
static int max_polls_without_data;
/* The current number: */
static int nr_polls_without_data;
/* receive buffer */
static char rx_buffer[RX_BUFFER_SIZE];
static int rx_count;

static struct {
	int battery_alarm;
	int dc_input_low;
	int on_battery;
	int battery_above_85_percent;
} current_ups_status;

/* remove n bytes from the head of rx_buffer, shift the remaining bytes to the start */
static void rm_buffer_head(int n) {
	if (rx_count <= n) {
		/* nothing left */
		rx_count = 0;
		return;
	}
	rx_count -= n;
	memmove(rx_buffer, rx_buffer + n, rx_count);
}

/* parse incoming data from the UPS.
 * return true if something new was received.
 */
static int check_for_new_data() {
	int new_data_received = 0;
	int done = 0;
	int num_received;

	while (!done) {
		/* Get new data from the serial port.
		 * No extra delay, just get the chars that were already buffered.
		 */
		num_received = ser_get_buf(upsfd, rx_buffer + rx_count, RX_BUFFER_SIZE - rx_count, 0, 0);
		if (num_received < 0) {
			/* comm error */
			ser_comm_fail("error %d while reading", num_received);
			/* discard any remaining old data from the receive buffer: */
			rx_count = 0;
			/* try to re-open the serial port: */
			if (upsfd) {
				ser_close(upsfd, device_path);
				upsfd = 0;
			}
			upsfd = ser_open_nf(device_path);
			ser_set_speed_nf(upsfd, device_path, B9600);
			done = 1;
		} else if (num_received == 0) {
			/* no (more) new data */
			done = 1;
		} else {
			rx_count += num_received;

			/* parse received input data: */
			while (rx_count >= 5) { /* all valid input messages are strings of 5 characters */
				if (strncmp(rx_buffer, "BUFRD", 5) == 0) {
					current_ups_status.battery_alarm = 0;
				} else if (strncmp(rx_buffer, "ALARM", 5) == 0) {
					current_ups_status.battery_alarm = 1;
				} else if (strncmp(rx_buffer, "DC_OK", 5) == 0) {
					current_ups_status.dc_input_low = 0;
				} else if (strncmp(rx_buffer, "DC_LO", 5) == 0) {
					current_ups_status.dc_input_low = 1;
				} else if (strncmp(rx_buffer, "*****", 5) == 0) {
					current_ups_status.on_battery = 0;
				} else if (strncmp(rx_buffer, "*BAT*", 5) == 0) {
					current_ups_status.on_battery = 1;
				} else if (strncmp(rx_buffer, "BA>85", 5) == 0) {
					current_ups_status.battery_above_85_percent = 1;
				} else if (strncmp(rx_buffer, "BA<85", 5) == 0) {
					current_ups_status.battery_above_85_percent = 0;
				} else {
					/* nothing sensible found at the start of the rx_buffer. */
					rm_buffer_head(1);
					continue; /* skip the code below */
				}
				rm_buffer_head(5);
				new_data_received = 1;
			}
		}
	}
	return new_data_received;
}


static int instcmd(const char *cmdname, const char *extra) {
	/* Note: the UPS does not really like to receive data.
	 * For example, sending an "R" without \n hangs the serial port.
	 * In that situation, the UPS will no longer send any status updates.
	 * For this reason, an additional \n is appended here.
	 * The commands are sent twice, because the first command is sometimes
	 * lost as well.
	 */
	if (!strcasecmp(cmdname, "shutdown.return")) {
		upslogx(LOG_NOTICE, "instcmd: sending command R");
		ser_send_pace(upsfd, 200000, "\n\nR\n\n");
		ser_send_pace(upsfd, 200000, "R\n\n");
		return STAT_INSTCMD_HANDLED;
	}
	if (!strcasecmp(cmdname, "shutdown.stayoff")) {
		upslogx(LOG_NOTICE, "instcmd: sending command S");
		ser_send_pace(upsfd, 200000, "\n\nS\n\n");
		ser_send_pace(upsfd, 200000, "S\n\n");
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s] [%s]", cmdname, extra);
	return STAT_INSTCMD_UNKNOWN;
}


void upsdrv_initinfo(void) {
	int max_attempts = 5;
	int found = 0;
	while (!found && max_attempts > 0) {
		if (check_for_new_data()) {
			found = 1;
		} else {
			sleep(1); /* Sleep a while, then try again */
		}
		max_attempts--;
	}
	if (!found) {
		fatalx(EXIT_FAILURE, "No data received from the UPS");
	}

	dstate_setinfo("device.mfr", "Siemens");
	dstate_setinfo("device.model", "SITOP UPS500 series");

	/* supported commands: */
	dstate_addcmd("shutdown.stayoff");
	dstate_addcmd("shutdown.return");

	upsh.instcmd = instcmd;
}


void upsdrv_updateinfo(void) {
	if (check_for_new_data()) {
		nr_polls_without_data = 0;
	} else {
		nr_polls_without_data++;
		if (nr_polls_without_data < 0)
			nr_polls_without_data = INT_MAX;
	}

	if (nr_polls_without_data > max_polls_without_data) {
		/* data is stale */
		dstate_datastale();
		return;
	}

	/* This is all we know about the charge level... */
	dstate_setinfo("battery.charge.approx",
		(current_ups_status.battery_above_85_percent) ? ">85" : "<85");

	status_init();

	if (current_ups_status.dc_input_low || current_ups_status.on_battery) {
		status_set("OB");
	} else {
		status_set("OL");
	}
	if (current_ups_status.battery_alarm) {
		status_set("LB");
	}

	status_commit();
	dstate_dataok();
	ser_comm_good();
}

void upsdrv_shutdown(void) {
	/* tell the UPS to shut down, then return - DO NOT SLEEP HERE */
	instcmd("shutdown.return", NULL);
}


void upsdrv_help(void) {
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void) {
	/* allow '-x max_polls_without_data=<some value>' */
	addvar(VAR_VALUE, "max_polls_without_data", "The maximum number of consecutive polls in which the UPS does not provide any data.");
}

void upsdrv_initups(void) {
	char * maxPollsString;
	unsigned int parsed;

	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B9600);

	/*
	 * Fast polling is preferred, because
	 * A) the UPS spits out new data every 75 msec,
	 * B) some models in this SITOP series have a _very_ small capacity
	 *    (< 10sec runtime), so every second might count.
	 */
	if (poll_interval > 5) {
		upslogx(LOG_NOTICE,
			"Option poll_interval is recommended to be lower than 5 (found: %jd)",
			(intmax_t)poll_interval);
	}

	/* option max_polls_without_data: */
	max_polls_without_data = 2;
	maxPollsString = getval("max_polls_without_data");
	if (maxPollsString) {
		if (str_to_uint(maxPollsString, &parsed, 10) == 1) {
			max_polls_without_data = parsed;
		} else {
			upslog_with_errno(LOG_ERR, "Cannot parse option max_polls_without_data");
		}
	}
}

void upsdrv_cleanup(void) {
	ser_close(upsfd, device_path);
}
