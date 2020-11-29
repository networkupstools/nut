/*  asem.c - driver for ASEM PB 1300 hardware, accessible through i2c.

	Copyright (C) 2014  Giuseppe Corbelli <giuseppe.corbelli@copanitalia.com>

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

	ASEM SPA contributed with support and documentation.
	Copan Italia SPA funded the development.

	There are 2 versions of the charger. Older one is based on Max1667,
	newer one is a custom solution. Both are on address 0x09.
	To be compatible with both versions just read bit 15 of address 0x13
	to have online/on battery status.
	Battery monitor is a BQ2060 at address 0x0B.

	Beware that the SystemIO memory used by the i2c controller is reserved by ACPI.
	On Linux, as of 3.5.x kernel only a native driver (i2c_i801) is available,
	so you need to boot with acpi_enforce_resources=lax option.
*/

#include "main.h"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>

/* Depends on i2c-dev.h, Linux only
 * Linux I2C userland is a bit of a mess until distros refresh to
 * the i2c-tools 4.x release that profides i2c/smbus.h for userspace
 * instead of (re)using linux/i2c-dev.h, which conflicts with a
 * kernel header of the same name.
 *
 * See:
 * https://i2c.wiki.kernel.org/index.php/Plans_for_I2C_Tools_4
 */
#if HAVE_LINUX_SMBUS_H
#	include <i2c/smbus.h>
#endif
#if HAVE_LINUX_I2C_DEV_H
#	include <linux/i2c-dev.h> /* for I2C_SLAVE */
# if !HAVE_LINUX_SMBUS_H
#  ifndef I2C_FUNC_I2C
#	include <linux/i2c.h>
#  endif
# endif
#endif

#include <sys/ioctl.h>

#ifndef __STR__
#	define __STR__(x) #x
#endif
#ifndef __XSTR__
#	define __XSTR__(x) __STR__(x)
#endif

#define DRIVER_NAME	"ASEM"
#define DRIVER_VERSION	"0.10"

/* Valid on ASEM PB1300 UPS */
#define BQ2060_ADDRESS	0x0B
#define CHARGER_ADDRESS	0x09

#define CMD_DEVICENAME	0x21

#define LOW_BATTERY_THRESHOLD 25
#define HIGH_BATTERY_THRESHOLD 75

#define ACCESS_DEVICE(fd, address) \
	if (ioctl(fd, I2C_SLAVE, address) < 0) { \
		fatal_with_errno(EXIT_FAILURE, "Failed to acquire bus access and/or talk to slave 0x%02X", address); \
	}

static unsigned long lb_threshold = LOW_BATTERY_THRESHOLD;
static unsigned long hb_threshold = HIGH_BATTERY_THRESHOLD;

static char *valid_devicename_data[] = {
	"ASEM SPA",
	NULL
};

upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Giuseppe Corbelli <giuseppe.corbelli@copanitalia.com>",
	DRV_EXPERIMENTAL,
	{NULL}
};

void upsdrv_initinfo(void)
{
	__s32 i2c_status;
	__u8 buffer[10];
	unsigned short year, month, day;

	ACCESS_DEVICE(upsfd, BQ2060_ADDRESS);

	/* Set capacity mode in mA(h) */
	i2c_status = i2c_smbus_read_word_data(upsfd, 0x03);
	if (i2c_status == -1) {
		fatal_with_errno(EXIT_FAILURE, "Could not read BatteryMode word data");
	}
	/* Clear 15th bit */
	i2c_status = i2c_smbus_write_word_data(upsfd, 0x03, i2c_status & ~0x8000);
	if (i2c_status == -1) {
		fatal_with_errno(EXIT_FAILURE, "Could not set BatteryMode word data");
	}

	/* Device name */
	memset(buffer, 0, 10);
	i2c_status = i2c_smbus_read_block_data(upsfd, 0x21, buffer);
	if (i2c_status == -1) {
		fatal_with_errno(EXIT_FAILURE, "Could not read DeviceName block data");
	}
	upsdebugx(1, "UPS model %s", (char *) buffer);
	dstate_setinfo("ups.model", "%s", (char *) buffer);

	/* Manufacturing date */
	i2c_status = i2c_smbus_read_word_data(upsfd, 0x1B);
	if (i2c_status == -1) {
		fatal_with_errno(EXIT_FAILURE, "Could not read ManufactureDate word data");
	}
	/* (Year - 1980) * 512 */
	year = (i2c_status >> 9) & 0x000000FF;
	/* Month * 32 */
	month = (i2c_status >> 4) & 0x0000001F;
	day = i2c_status & 0x0000001F;
	upsdebugx(1, "UPS manufacturing date %d-%02d-%02d (%d)", year + 1980, month, day, i2c_status);
	dstate_setinfo("ups.mfr.date", "%d-%02d-%02d", year + 1980, month, day);

	/* Device chemistry */
	memset(buffer, 0, 10);
	i2c_status = i2c_smbus_read_block_data(upsfd, 0x22, buffer);
	if (i2c_status == -1) {
		fatal_with_errno(EXIT_FAILURE, "Could not read DeviceChemistry block data");
	}
	upsdebugx(1, "Battery chemistry %s", (char *) buffer);
	dstate_setinfo("battery.type", "%s", (char *) buffer);

	/* Serial number */
	i2c_status = i2c_smbus_read_word_data(upsfd, 0x1C);
	if (i2c_status == -1) {
		fatal_with_errno(EXIT_FAILURE, "Could not read SerialNumber block data");
	}
	upsdebugx(1, "Serial Number %d", i2c_status);
	dstate_setinfo("ups.serial", "%d", i2c_status);
}

void upsdrv_updateinfo(void)
{
	static char online;
	static char discharging;
	static char fully_charged;
	static unsigned short charge_percentage;
	static unsigned short voltage;
	static unsigned short capacity;
	static signed short current;
	static __s32 i2c_status;
	static __s32 temperature;
	static __s32 runtime_to_empty;

	ACCESS_DEVICE(upsfd, CHARGER_ADDRESS);
	/* Charger only supplies online/offline status */
	i2c_status = i2c_smbus_read_word_data(upsfd, 0x13);
	if (i2c_status == -1) {
		dstate_datastale();
		upslogx(LOG_ERR, "Could not read charger status word at address 0x13");
		return;
	}
	online = (i2c_status & 0x8000) != 0;
	upsdebugx(3, "Charger status 0x%02X, online %d", i2c_status, online);

	ACCESS_DEVICE(upsfd, BQ2060_ADDRESS);
	i2c_status = i2c_smbus_read_word_data(upsfd, 0x16);
	if (i2c_status == -1) {
		dstate_datastale();
		upslogx(LOG_ERR, "Could not read bq2060 status word at address 0x16");
		return;
	}
	upsdebugx(3, "bq2060 status 0x04%X", i2c_status);
	/* Busy, leave data as stale, try next time */
	if (i2c_status & 0x0001) {
		dstate_datastale();
		upslogx(LOG_NOTICE, "bq2060 is busy");
		return;
	}
	/* Error, leave data as stale, try next time */
	if (i2c_status & 0x000F) {
		dstate_datastale();
		upslogx(LOG_WARNING, "bq2060 returned error code 0x%02X", i2c_status & 0x000F);
		return;
	}

	discharging = (i2c_status & 0x0040);
	fully_charged = (i2c_status & 0x0020);

	/* Charge percentage */
	i2c_status = i2c_smbus_read_word_data(upsfd, 0x0D);
	if (i2c_status == -1) {
		dstate_datastale();
		upslogx(LOG_ERR, "Could not read charge percentage from bq2060 at address 0x0D");
		return;
	}
	charge_percentage = i2c_status & 0xFFFF;
	upsdebugx(3, "Charge percentage %03d", charge_percentage);

	/* Battery voltage in mV */
	i2c_status = i2c_smbus_read_word_data(upsfd, 0x09);
	if (i2c_status == -1) {
		dstate_datastale();
		upslogx(LOG_ERR, "Could not read voltage from bq2060 at address 0x09");
		return;
	}
	voltage = i2c_status & 0x0000FFFF;
	upsdebugx(3, "Battery voltage %d mV", voltage);

	/* Temperature in °K */
	temperature = i2c_smbus_read_word_data(upsfd, 0x08);
	if (temperature == -1) {
		dstate_datastale();
		upslogx(LOG_ERR, "Could not read temperature from bq2060 at address 0x08");
		return;
	}
	upsdebugx(3, "Temperature %4.1f K", temperature / 10.0);

	/* Current load in mA, positive for charge, negative for discharge */
	i2c_status = i2c_smbus_read_word_data(upsfd, 0x0A);
	if (i2c_status == -1) {
		dstate_datastale();
		upslogx(LOG_ERR, "Could not read current from bq2060 at address 0x0A");
		return;
	}
	current = i2c_status & 0x0000FFFF;
	upsdebugx(3, "Current %d mA", current);

	/* Current capacity */
	i2c_status = i2c_smbus_read_word_data(upsfd, 0x0F);
	if (i2c_status == -1) {
		dstate_datastale();
		upslogx(LOG_ERR, "Could not read RemainingCapacity word data");
		return;
	}
	capacity = i2c_status & 0x0000FFFF;
	upsdebugx(3, "Current capacity %d mAh", capacity);

	/* Expected runtime capacity, averaged by gauge */
	runtime_to_empty = i2c_smbus_read_word_data(upsfd, 0x12);
	if (runtime_to_empty == -1) {
		dstate_datastale();
		upslogx(LOG_ERR, "Could not read AverageTimeToEmpty word data");
		return;
	}
	upsdebugx(3, "Expected run-time to empty %d m", runtime_to_empty);

	status_init();
	status_set(online ? "OL" : "OB");
	if (!discharging && !fully_charged)
		status_set("CHRG");
	else if (discharging && current < 0)
		status_set("DISCHRG");

	if (charge_percentage >= hb_threshold)
		status_set("HB");
	else if (charge_percentage <= lb_threshold)
		status_set("LB");

	/* In V */
	dstate_setinfo("battery.voltage", "%2.3f", voltage / 1000.0);
	/* In mAh */
	dstate_setinfo("battery.current", "%2.3f", current / 1000.0);
	dstate_setinfo("battery.charge", "%d", charge_percentage);
	/* In mAh */
	dstate_setinfo("battery.capacity", "%2.3f", capacity / 1000.0);
	/* In °C */
	dstate_setinfo("ups.temperature", "%4.1f", (temperature / 10.0) - 273.15);
	/* In seconds */
	dstate_setinfo("battery.runtime", "%d", runtime_to_empty * 60);
	status_commit();
	dstate_dataok();
}

void upsdrv_shutdown(void)
{
	/* tell the UPS to shut down, then return - DO NOT SLEEP HERE */

	/* maybe try to detect the UPS here, but try a shutdown even if
	   it doesn't respond at first if possible */

	/* replace with a proper shutdown function */
	fatalx(EXIT_FAILURE, "shutdown not supported");

	/* you may have to check the line status since the commands
	   for toggling power are frequently different for OL vs. OB */

	/* OL: this must power cycle the load if possible */

	/* OB: the load must remain off until the power returns */
}

void upsdrv_help(void)
{
	/* Redundant */
	printf("\nASEM options\n");
	printf(" HIGH/low battery thresholds\n");
	printf("  lb = " __XSTR__(LOW_BATTERY_THRESHOLD) " (battery is low under this level)\n");
	printf("  hb = " __XSTR__(HIGH_BATTERY_THRESHOLD) " (battery is high above this level)\n");
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "lb", "Low battery threshold, default " __XSTR__(LOW_BATTERY_THRESHOLD));
	addvar(VAR_VALUE, "hb", "High battery threshold, default " __XSTR__(HIGH_BATTERY_THRESHOLD));
}

void upsdrv_initups(void)
{
	__s32 i2c_status;
	__u8 DeviceName_buffer[10];
	unsigned int i;
	unsigned long x;
	char *DeviceName;
	char *option;

	upsfd = open(device_path, O_RDWR);
	if (upsfd < 0) {
		fatal_with_errno(EXIT_FAILURE, "Could not open device port '%s'", device_path);
	}

	ACCESS_DEVICE(upsfd, BQ2060_ADDRESS);

	/* Get ManufacturerName */
	memset(DeviceName_buffer, 0, 10);
	i2c_status = i2c_smbus_read_block_data(upsfd, 0x20, DeviceName_buffer);
	if (i2c_status == -1) {
		fatal_with_errno(EXIT_FAILURE, "Could not read DeviceName block data");
	}
	i = 0;
	while ( (DeviceName = valid_devicename_data[i++]) ) {
		if (0 == memcmp(DeviceName, DeviceName_buffer, i2c_status))
			break;
	}
	if (!DeviceName) {
		fatal_with_errno(EXIT_FAILURE, "Device '%s' unknown", (char *) DeviceName_buffer);
	}
	upsdebugx(1, "Found device '%s' on port '%s'", (char *) DeviceName, device_path);
	dstate_setinfo("ups.mfr", "%s", (char *) DeviceName);

	option = getval("lb");
	if (option) {
		x = strtoul(option, NULL, 0);
		if ((x == 0) && (errno != 0)) {
			upslogx(LOG_WARNING, "Invalid value specified for low battery threshold: '%s'", option);
		} else {
			lb_threshold = x;
		}
	}
	option = getval("hb");
	if (option) {
		x = strtoul(option, NULL, 0);
		if ((x == 0) && (errno != 0)) {
			upslogx(LOG_WARNING, "Invalid value specified for high battery threshold: '%s'", option);
		} else if ((x < 1) || (x > 100)) {
			upslogx(LOG_WARNING, "Invalid value specified for high battery threshold: '%s' (must be 1 < hb <= 100)", option);
		} else {
			hb_threshold = x;
		}
	}
	/* Invalid values specified */
	if (lb_threshold > hb_threshold) {
		upslogx(LOG_WARNING, "lb > hb specified in options. Returning to defaults.");
		lb_threshold = LOW_BATTERY_THRESHOLD;
		hb_threshold = HIGH_BATTERY_THRESHOLD;
	}

	upslogx(LOG_NOTICE, "High battery threshold is %lu, low battery threshold is %lu", lb_threshold, hb_threshold);
}

void upsdrv_cleanup(void)
{
	close(upsfd);
}
