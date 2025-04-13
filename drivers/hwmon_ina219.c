/* hwmon.c Driver for INA219 hwmon-based power monitors.

	Copyright (C) 2024 Jan Viktorin <jan.viktorin@gmail.com>

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

#include "main.h"
#include "nut_float.h"
#include "nut_stdint.h"

#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#ifndef STRFY0
#	define STRFY0(x) #x
#endif
#ifndef STRFY
#	define STRFY(x) STRFY0(x)
#endif

#define SYSFS_HWMON_DIR                     "/sys/class/hwmon"
#define BATTERY_CHARGE_LOW                  15

#define DRIVER_NAME                         "hwmon-INA219 UPS driver"
#define DRIVER_VERSION                      "0.03"

upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Jan Viktorin <jan.viktorin@gmail.com>",
	DRV_EXPERIMENTAL,
	{ NULL },
};

/**
 * @brief Path usually pointing to /sys/class/hwmon/hwmonX.
 */
static char ina219_base_path[NUT_PATH_MAX + 1];

/**
 * @brief Threshold for detection of LB status.
 */
static unsigned int battery_charge_low = BATTERY_CHARGE_LOW;

/**
 * @brief Battery voltage (mV) when it is considered depleted.
 */
static unsigned int battery_voltage_min;

/**
 * @brief Battery voltage (mV) when it is considered fully charged.
 */
static unsigned int battery_voltage_max;

/**
 * @brief Voltage value as recently read from in1_input (mV).
 * @see https://docs.kernel.org/hwmon/ina2xx.html
 */
static int voltage = 0;

/**
 * @brief Current value as recently read from curr1_input (mA).
 * @see https://docs.kernel.org/hwmon/ina2xx.html
 */
static int current = 0;

static int file_contains(const char *path, const char *text)
{
	FILE *f;
	char buf[128];
	size_t len;

	if ((f = fopen(path, "r")) == NULL) {
		upslog_with_errno(LOG_ERR, "unable to open %s", path);
		return 0;
	}

	len = fread(buf, 1, sizeof(buf), f);
	fclose(f);
	upsdebugx(4, "read %" PRIuSIZE " bytes", len);

	if (strlen(text) != len)
		return 0;

	if (memcmp(buf, text, len))
		return 0;

	return 1;
}

static int file_read_number(const char *path, int *value)
{
	char buf[128];
	ssize_t ret;
	int fd;

	if ((fd = open(path, O_RDONLY)) < 0)
		return -errno;

	if ((ret = pread(fd, buf, sizeof(buf), 0)) < 0) {
		const int _e = -errno;
		close(fd);
		return _e;
	}
	else {
		close(fd);

		buf[ret] = '\0';
		*value = atoi(buf);
		return 0;
	}
}

static int detect_ina219(const char *ina219_dir)
{
	char namepath[NUT_PATH_MAX + 1];

	upsdebugx(3, "checking %s", ina219_dir);

	snprintf(namepath, sizeof(namepath), "%s/name", ina219_dir);

	if (!file_contains(namepath, "ina219\n"))
		return -ENODEV;

	upsdebugx(3, "detected ina219 at %s", ina219_dir);
	return 0;
}

static int scan_hwmon_ina219(const char *sysfs_hwmon_dir)
{
	DIR *sysfs;
	struct dirent *entry;
	int ret;

	if (strcmp(device_path, "auto")) {
		if ((ret = detect_ina219(device_path)) < 0) {
			fatal_with_errno(EXIT_FAILURE,
					"not a valid hwmon ina219 dir: '%s'\n", device_path);
		}

		snprintf(ina219_base_path, sizeof(ina219_base_path), "%s", device_path);
		return 0;
	}

	upslogx(LOG_NOTICE, "scanning %s for ina219", sysfs_hwmon_dir);

	if ((sysfs = opendir(sysfs_hwmon_dir)) == NULL) {
		const int _e = -errno;
		upslog_with_errno(LOG_ERR, "unable to open %s", sysfs_hwmon_dir);
		return _e;
	}

	while ((entry = readdir(sysfs)) != NULL) {
		char hwmon_dir[NUT_PATH_MAX + 1];

		if (entry->d_type != DT_DIR && entry->d_type != DT_LNK) {
			upsdebugx(3, "path %s/%s is not directory/symlink", sysfs_hwmon_dir,
					entry->d_name);
			continue;
		}

		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
			upsdebugx(3, "skipping path %s/%s", sysfs_hwmon_dir, entry->d_name);
			continue;
		}

		snprintf(hwmon_dir, sizeof(hwmon_dir), "%s/%s",
					sysfs_hwmon_dir, entry->d_name);
		if (detect_ina219(hwmon_dir) < 0) {
			/* Log this one only if really troubleshooting,
			 * this is quite an anticipated and noisy situation,
			 * to disregard most of the directories in sysfs :)
			 */
			upsdebugx(6, "skipping path %s: not the expected subsystem", hwmon_dir);
			continue;
		}

		snprintf(ina219_base_path, sizeof(ina219_base_path), "%s", hwmon_dir);
		closedir(sysfs);
		return 0;
	}

	closedir(sysfs);
	return -ENODEV;
}

static int update_intvar(
	const char *base_path,
	const char *name,
	int *value)
{
	char path[NUT_PATH_MAX + 1];
	int ret;

	if (snprintf(path, sizeof(path), "%s/%s", base_path, name) >= NUT_PATH_MAX) {
		errno = ENAMETOOLONG;
		upslog_with_errno(LOG_ERR, "snprintf(%s/%s) has failed", base_path, name);
		return -ENAMETOOLONG;
	}

	if ((ret = file_read_number(path, value)) < 0) {
		errno = -ret;
		upslog_with_errno(LOG_ERR, "file_read_number(%s) has failed", path);
		return ret;
	}

	return 0;
}

static int update_voltage(void)
{
	return update_intvar(ina219_base_path, "in1_input", &voltage);
}

static int update_current(void)
{
	return update_intvar(ina219_base_path, "curr1_input", &current);
}

void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "sysfs_dir",
			"Path to sysfs dir of hwmon if port=auto (" SYSFS_HWMON_DIR ")");
}

static int parse_voltage(const char *s, double *v)
{
	errno = 0;
	*v = strtod(s, NULL);

	if (errno)
		return -errno;

	if (!isnormal(*v) || !isfinite(*v))
		return -EDOM;

	return 0;
}

static void battery_voltage_params_init(void)
{
	/* Note: with this device, we do not really expect these values
	 * to be reported by hardware or change over time, so this is
	 * only parsed (from built-in or user-provided defaults) into
	 * C variables once.
	 */
	const char *d_min = dstate_getinfo("battery.voltage.low");
	const char *d_max = dstate_getinfo("battery.voltage.high");
	double volt_nominal;
	int ret;

	if (!d_min || !d_max) {
		const char *d_nom;

		d_nom = dstate_getinfo("battery.voltage.nominal");
		upsdebugx(4, "battery.voltage.nominal = '%s'\n", d_nom);

		if (d_nom == NULL) {
			volt_nominal = 3.6;
		}
		else {
			if ((ret = parse_voltage(d_nom, &volt_nominal)) < 0) {
				errno = -ret;
				fatal_with_errno(EXIT_FAILURE,
						"battery.voltage.nominal is invalid: '%s'\n", d_nom);
			}
		}

		upslogx(LOG_NOTICE,
				"guess battery params from battery.voltage.nominal = %.1lf V\n",
				volt_nominal);

		if (d_equal(volt_nominal, 3.6)) {
			battery_voltage_min = 3000;
			battery_voltage_max = 4250;
		}
		else if (d_equal(volt_nominal, 3.7)) {
			battery_voltage_min = 3000;
			battery_voltage_max = 4250;
		}
		else if (d_equal(volt_nominal, 3.8)) {
			battery_voltage_min = 3000;
			battery_voltage_max = 4350;
		}
		else if (d_equal(volt_nominal, 3.85)) {
			battery_voltage_min = 3000;
			battery_voltage_max = 4400;
		}
		else {
			fatalx(EXIT_FAILURE, "unsupported battery.voltage.nominal: %lf\n",
					volt_nominal);
		}
	}
	else {
		double tmp;

		if ((ret = parse_voltage(d_min, &tmp)) < 0) {
			errno = -ret;
			fatal_with_errno(EXIT_FAILURE,
					"invalid battery.voltage.low: '%s'\n", d_min);
		}
		else {
			battery_voltage_min = (unsigned int) (tmp * 1000.0);
		}

		if ((ret = parse_voltage(d_max, &tmp)) < 0) {
			errno = -ret;
			fatal_with_errno(EXIT_FAILURE,
					"invalid battery.voltage.high: '%s'\n", d_max);
		}
		else {
			battery_voltage_max = (unsigned int) (tmp * 1000.0);
		}
	}

	upslogx(LOG_NOTICE, "battery.voltage.low = %u mV\n", battery_voltage_min);
	upslogx(LOG_NOTICE, "battery.voltage.high = %u mV\n", battery_voltage_max);
}

static int parse_charge(const char *s, unsigned int *v)
{
	long tmp;

	errno = 0;
	tmp = strtol(s, NULL, 0);

	if (errno)
		return -errno;

	if (tmp < 1 || tmp > 100)
		return -ERANGE;

	*v = (int) tmp;
	return 0;
}

static void battery_charge_params_init(void)
{
	const char *v_lb = dstate_getinfo("battery.charge.low");
	int ret;

	if (v_lb) {
		if ((ret = parse_charge(v_lb, &battery_charge_low)) < 0) {
			errno = -ret;
			fatal_with_errno(EXIT_FAILURE,
					"battery.charge.low is invalid: '%s'", v_lb);
		}
	}

	upslogx(LOG_NOTICE, "battery.charge.low = %u\n", battery_charge_low);
}

void upsdrv_initups(void)
{
	const char *hwmon_dir = SYSFS_HWMON_DIR;
	int ret;

	if (getval("sysfs_dir"))
		hwmon_dir = getval("sysfs_dir");

	if ((ret = scan_hwmon_ina219(hwmon_dir)) < 0) {
		errno = -ret;
		fatal_with_errno(EXIT_FAILURE, "scan_hwmon_ina219(%s) has failed",
				hwmon_dir);
	}

	battery_voltage_params_init();
	battery_charge_params_init();
}

void upsdrv_initinfo(void)
{
	dstate_setinfo("ups.mfr", "%s", "Texas Instruments");
	dstate_setinfo("ups.model", "%s", "INA219");
	dstate_setinfo("ups.type", "%s", "ups");
	dstate_setinfo("device.mfr", "%s", "Texas Instruments");
	dstate_setinfo("device.model", "%s", "INA219");
	dstate_setinfo("device.type", "%s", "ups");
	dstate_setinfo("device.description", "%s",
			"Bidirectional Current/Power Monitor With I2C Interface");

	dstate_setinfo("battery.charge.low", "%u", battery_charge_low);
}

static unsigned int battery_charge_compute(void)
{
	const double divisor = (battery_voltage_max - battery_voltage_min) / 100.0;
	double charge;

	if (voltage < 0)
		return 0;

	if (((unsigned int) voltage) > battery_voltage_min)
		charge = voltage - battery_voltage_min;
	else
		charge = 0;

	charge /= divisor;
	charge = charge > 100 ? 100 : charge;

	return (unsigned int) charge;
}

void upsdrv_updateinfo(void)
{
	unsigned int charge = 0;
	int stale = 0;

	if (update_voltage() < 0)
		stale = 1;

	upsdebugx(3, "Battery voltage: %.3fV", voltage / 1000.0);

	if (update_current() < 0)
		stale = 1;

	upsdebugx(3, "Battery current: %.3fA", current / 1000.0);

	if (stale) {
		dstate_datastale();
		return;
	}

	status_init();

	charge = battery_charge_compute();

	status_set(current <= 0 ? "OL" : "OB");

	if (current < 0)
		status_set("CHRG");
	else if (current > 0)
		status_set("DISCHRG");

	if (charge <= battery_charge_low)
		status_set("LB");

	dstate_setinfo("battery.voltage", "%.3f", voltage / 1000.0);
	dstate_setinfo("battery.current", "%.3f", current / 1000.0);
	dstate_setinfo("battery.charge", "%u", charge);

	if (charge <= battery_charge_low && current > 0)
		dstate_setinfo("battery.runtime", "%d", 60); // 1 minute

	status_commit();
	dstate_dataok();
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
	/* No special options in this driver (vars/flags are auto-documented) */
}

void upsdrv_cleanup(void)
{
}
