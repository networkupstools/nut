/* pijuice.c Driver for the PiJuice HAT (www.pijuice.com), addressed via i2c.

	Copyright (C) 2019 Andrew Anderson <aander07@gmail.com>

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

#include <sys/ioctl.h>
#include <stdint.h>

/*
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

/*
 * i2c-tools pre-4.0 has a userspace header with a name that conflicts
 * with a kernel header, so it may be ignored/removed by distributions
 * when packaging i2c-tools.
 *
 * This will cause the driver to be un-buildable on certain
 * configurations, so include the necessary bits here to handle this
 * situation.
 */
#if WITH_LINUX_I2C
#if !HAVE_DECL_I2C_SMBUS_ACCESS
static inline __s32 i2c_smbus_access(int file, char read_write, __u8 command,
                                     int size, union i2c_smbus_data *data)
{
	struct i2c_smbus_ioctl_data args;
	__s32 err;

	args.read_write = read_write;
	args.command = command;
	args.size = size;
	args.data = data;

	err = ioctl(file, I2C_SMBUS, &args);
	if (err == -1)
		err = -errno;
	return err;
}
#endif

#if !HAVE_DECL_I2C_SMBUS_READ_BYTE_DATA
static inline __s32 i2c_smbus_read_byte_data(int file, __u8 command)
{
	union i2c_smbus_data data;
	int err;

	if ((err = i2c_smbus_access(file, I2C_SMBUS_READ, command,
	                     I2C_SMBUS_BYTE_DATA, &data)) < 0)
		return err;
	else
		return 0x0FF & data.byte;
}
#endif

#if !HAVE_DECL_I2C_SMBUS_WRITE_BYTE_DATA
static inline __s32 i2c_smbus_write_byte_data(int file, __u8 command, __u8 value)
{
	union i2c_smbus_data data;
	int err;

	data.byte = value;
	if ((err = i2c_smbus_access(file, I2C_SMBUS_WRITE, command,
	                     I2C_SMBUS_BYTE_DATA, &data)) < 0)
		return err;
	else
		return 0x0FF & data.byte;
}
#endif

#if !HAVE_DECL_I2C_SMBUS_READ_WORD_DATA
static inline __s32 i2c_smbus_read_word_data(int file, __u8 command)
{
	union i2c_smbus_data data;
	int err;

	if ((err = i2c_smbus_access(file, I2C_SMBUS_READ, command,
	                     I2C_SMBUS_WORD_DATA, &data)) < 0)
		return err;
	else
		return 0x0FFFF & data.word;
}
#endif

#if !HAVE_DECL_I2C_SMBUS_WRITE_WORD_DATA
static inline __s32 i2c_smbus_write_word_data(int file, __u8 command, __u16 value)
{
	union i2c_smbus_data data;
	int err;

	data.word = value;
	if ((err = i2c_smbus_access(file, I2C_SMBUS_WRITE, command,
	                     I2C_SMBUS_WORD_DATA, &data)) < 0)
		return err;
	else
		return 0x0FFFF & data.word;
}
#endif

#if !HAVE_DECL_I2C_SMBUS_READ_BLOCK_DATA
static inline __u8* i2c_smbus_read_i2c_block_data(int file, __u8 command, __u8 length, __u8 *values)
{
	union i2c_smbus_data data;
	int err;

	if ( length > I2C_SMBUS_BLOCK_MAX)
	{
		length = I2C_SMBUS_BLOCK_MAX;
	}

	data.block[0] = length;
	memcpy(data.block + 1, values, length);

	if ((err = i2c_smbus_access(file, I2C_SMBUS_READ, command,
	                     I2C_SMBUS_I2C_BLOCK_DATA, &data)) < 0)
		return NULL;
	else
		memcpy(values, &data.block[1], data.block[0]);

	return values;
}
#endif
#endif // if WITH_LINUX_I2C

#define STATUS_CMD                          0x40
#define CHARGE_LEVEL_CMD                    0x41
#define CHARGE_LEVEL_HI_RES_CMD             0x42
#define FAULT_EVENT_CMD                     0x44
#define BUTTON_EVENT_CMD                    0x45
#define BATTERY_TEMPERATURE_CMD             0x47
#define BATTERY_VOLTAGE_CMD                 0x49
#define BATTERY_CURRENT_CMD                 0x4b
#define IO_VOLTAGE_CMD                      0x4d
#define IO_CURRENT_CMD                      0x4f

#define CHARGING_CONFIG_CMD                 0x51
#define BATTERY_PROFILE_ID_CMD              0x52
#define BATTERY_PROFILE_CMD                 0x53
#define BATTERY_EXT_PROFILE_CMD             0x54
#define BATTERY_TEMP_SENSE_CONFIG_CMD       0x5D

#define POWER_INPUTS_CONFIG_CMD             0x5E
#define RUN_PIN_CONFIG_CMD                  0x5F
#define POWER_REGULATOR_CONFIG_CMD          0x60
#define WATCHDOG_ACTIVATION_CMD             0x61
#define POWER_OFF_CMD                       0x62
#define WAKEUP_ON_CHARGE_CMD                0x63
#define SYSTEM_POWER_SWITCH_CTRL_CMD        0x64

#define LED_STATE_CMD                       0x66
#define LED_BLINK_CMD                       0x68
#define LED_CONFIGURATION_CMD               0x6A
#define BUTTON_CONFIGURATION_CMD            0x6E

#define IO1_CONFIGURATION_CMD               0x72
#define IO1_PIN_ACCESS_CMD                  0x75

#define IO2_CONFIGURATION_CMD               0x77
#define IO2_PIN_ACCESS_CMD                  0x7A

#define I2C_ADDRESS_CMD                     0x7C

#define ID_EEPROM_WRITE_PROTECT_CTRL_CMD    0x7E
#define ID_EEPROM_ADDRESS_CMD               0x7F

#define RTC_TIME_CMD                        0xB0
#define RTC_ALARM_CMD                       0xB9
#define RTC_CTRL_STATUS_CMD                 0xC2

#define RESET_TO_DEFAULT_CMD                0xF0
#define FIRMWARE_VERSION_CMD                0xFD

#define BATT_NORMAL                         0
#define BATT_CHARGING_FROM_IN               1
#define BATT_CHARGING_FROM_5V               2
#define BATT_NOT_PRESENT                    3

#define POWER_NOT_PRESENT                   0
#define POWER_BAD                           1
#define POWER_WEAK                          2
#define POWER_PRESENT                       3

#define LOW_BATTERY_THRESHOLD               25.0
#define HIGH_BATTERY_THRESHOLD              75.0
#define NOMINAL_BATTERY_VOLTAGE             4.18

#define DRIVER_NAME                         "PiJuice UPS driver"
#define DRIVER_VERSION                      "0.9"

static uint8_t i2c_address    = 0x14;
static uint8_t shutdown_delay = 30;

/*
 * Flags used to indicate a change in power status
 */
static uint8_t usb_power = 0;
static uint8_t gpio_power = 0;
static uint8_t battery_power = 0;

/*
 * Smooth out i2c read errors by holding the most recent
 * battery charge level reading
 */
static float battery_charge_level = 0;

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Andrew Anderson <aander07@gmail.com>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

/* The macros below all write into a "data" variable defined by the routine
 * scope which calls them, with respective type of uint8_t for "byte" and
 * uint16_t for "word" macros. Native i2c functions operate with __s32 type
 * (currently, signed 32-bit ints?) with negative values for error returns.
 * Note: some manpages refer to "s32" while headers on my and CI systems use
 * a "__s32" type. Maybe this is something to determine in configure script?
 * Code below was fixed to convert the valid values and avoid compiler
 * warnings about comparing whether unsigned ints happened to be negative.
 */
#define I2C_READ_BYTE(fd, cmd, label) \
	{ \
		__s32 sData; \
		if ((sData = i2c_smbus_read_byte_data(upsfd, cmd)) < 0 ) { \
			upsdebugx(2, "Failure reading the i2c bus [%s]", label); \
			return; \
		} ; \
		data = (uint8_t) sData; \
	}

#define I2C_WRITE_BYTE(fd, cmd, value, label) \
	{ \
		if ( i2c_smbus_write_byte_data(upsfd, cmd, value) < 0 ) { \
			upsdebugx(2, "Failure writing to the i2c bus [%s]", label); \
			return; \
		} ; \
	}

#define I2C_READ_WORD(fd, cmd, label) \
	{ \
		__s32 sData; \
		if ((sData = i2c_smbus_read_word_data(upsfd, cmd)) < 0 ) { \
			upsdebugx(2, "Failure reading the i2c bus [%s]", label); \
			return; \
		} ; \
		data = (uint16_t) sData; \
	}

#define I2C_READ_BLOCK(fd, cmd, size, block, label) \
	if ((i2c_smbus_read_i2c_block_data(upsfd, cmd, size, block)) < 0 ) { \
		upsdebugx(2, "Failure reading the i2c bus [%s]", label); \
		return; \
	}

static inline int open_i2c_bus(char *path, uint8_t addr)
{
	int file;

	if ((file = open(path, O_RDWR)) < 0)
	{
		fatal_with_errno(EXIT_FAILURE, "Failed to open the i2c bus on %s", path);
	}

	if (ioctl(file, I2C_SLAVE, addr) < 0)
	{
		fatal_with_errno(EXIT_FAILURE, "Failed to acquire the i2c bus and/or talk to the UPS");
	}

	return file;
}

static void get_charge_level_hi_res()
{
	uint8_t cmd = CHARGE_LEVEL_HI_RES_CMD;
	uint16_t data;

	upsdebugx( 3, __func__ );

	I2C_READ_WORD( upsfd, cmd, __func__ )

	/*
	 * Use an external variable to allow for missed i2c bus
	 * reads; the charge level data may be slightly stale,
	 * but no other options seem reasonable:
	 *
	 * 1) store 0
	 * 	Leads to a false report of a depleted battery, possibly
	 * 	triggering an immediate shutdown if on battery power only
	 * 2) store -1
	 * 	Adds a lot of logic to "skip" over negative charge levels,
	 * 	which effectively accomplishes the same thing
	 * 3) retry the read immediately
	 * 	Could tie up the i2c bus and make matters exponentially worse
	 */
	battery_charge_level = data / 10.0;

	upsdebugx( 1, "Battery Charge Level: %02.1f%%", battery_charge_level );
	dstate_setinfo( "battery.charge", "%02.1f", battery_charge_level );
}

static void get_status()
{
	uint8_t cmd = STATUS_CMD;
	uint8_t data;
	char status_buf[ST_MAX_VALUE_LEN];

	upsdebugx( 3, __func__ );

	memset( status_buf, 0, ST_MAX_VALUE_LEN );

	I2C_READ_BYTE( upsfd, cmd, __func__ )

	uint8_t batteryStatus = data >> 2 & 0x03;
	switch( batteryStatus )
	{
		case BATT_NORMAL:
			upsdebugx( 1, "Battery Status: Normal" );
			dstate_setinfo( "battery.packs", "%d", 1 );
			dstate_setinfo( "battery.packs.bad", "%d", 0 );
			break;
		case BATT_CHARGING_FROM_IN:
			upsdebugx( 1, "Battery Status: Charging from IN" );
			dstate_setinfo( "battery.packs", "%d", 1 );
			dstate_setinfo( "battery.packs.bad", "%d", 0 );
			break;
		case BATT_CHARGING_FROM_5V:
			upsdebugx( 1, "Battery Status: Charging from 5V" );
			dstate_setinfo( "battery.packs", "%d", 1 );
			dstate_setinfo( "battery.packs.bad", "%d", 0 );
			break;
		case BATT_NOT_PRESENT:
			upsdebugx( 1, "Battery Status: Not Present" );
			dstate_setinfo( "battery.packs", "%d", 0 );
			dstate_setinfo( "battery.packs.bad", "%d", 1 );
			break;
		default:
			upsdebugx( 1, "battery.status: UNKNOWN" );
	}

	uint8_t powerInput = data >> 4 & 0x03;
	switch( powerInput )
	{
		case POWER_NOT_PRESENT:
			upsdebugx( 1, "Power Input: Not Present" );
			break;
		case POWER_BAD:
			upsdebugx( 1, "Power Input: Bad" );
			break;
		case POWER_WEAK:
			upsdebugx( 1, "Power Input: Weak" );
			break;
		case POWER_PRESENT:
			upsdebugx( 1, "Power Input: Present" );
			break;
		default:
			upsdebugx( 1, "Power Input: UNKNOWN" );
	}

	uint8_t powerInput5vIo = data >> 6 & 0x03;
	switch( powerInput5vIo )
	{
		case POWER_NOT_PRESENT :
			upsdebugx(1, "Power Input 5v: Not Present");
			break;
		case POWER_BAD:
			upsdebugx(1, "Power Input 5v: Bad");
			break;
		case POWER_WEAK:
			upsdebugx(1, "Power Input 5v: Weak");
			break;
		case POWER_PRESENT:
			upsdebugx(1, "Power Input 5v: Present");
			break;
		default:
			upsdebugx(1, "Power Input 5v: UNKNOWN");
	}

	if ( batteryStatus == BATT_NORMAL ||
	     batteryStatus == BATT_CHARGING_FROM_IN ||
	     batteryStatus == BATT_CHARGING_FROM_5V )
	{
		get_charge_level_hi_res();

		if ( battery_charge_level <= LOW_BATTERY_THRESHOLD )
		{
			upsdebugx( 1, "Battery Charge Status: LOW" );
			snprintfcat( status_buf, ST_MAX_VALUE_LEN, "LB " );
		}
		else if ( battery_charge_level > HIGH_BATTERY_THRESHOLD )
		{
			upsdebugx( 1, "Battery Charge Status: HIGH" );
			snprintfcat( status_buf, ST_MAX_VALUE_LEN, "HB " );
		}
	}
	else if ( batteryStatus == BATT_NOT_PRESENT )
	{
		snprintfcat( status_buf, ST_MAX_VALUE_LEN, "RB " );
	}

	if ( batteryStatus  <= BATT_NOT_PRESENT &&
	     powerInput     <= POWER_PRESENT &&
	     powerInput5vIo <= POWER_PRESENT )
	{
		if ( powerInput       == POWER_NOT_PRESENT &&
		     ( powerInput5vIo != POWER_NOT_PRESENT ))
		{
			if ( usb_power != 1 || gpio_power != 0 )
			{
				upslogx( LOG_NOTICE, "On USB power" );
			}
			usb_power     = 1;
			gpio_power    = 0;
			battery_power = 0;
			upsdebugx( 1, "On USB power [%d:%d:%d]", usb_power, gpio_power, battery_power );

			snprintfcat( status_buf, sizeof(status_buf), "OL" );
			if ( batteryStatus == BATT_CHARGING_FROM_5V )
			{
				snprintfcat( status_buf, sizeof( status_buf ), " CHRG" );
				upsdebugx( 1, "Battery Charger Status: charging" );
				dstate_setinfo( "battery.charger.status", "%s", "charging" );
			}
			else if ( batteryStatus == BATT_NORMAL )
			{
				upsdebugx( 1, "Battery Charger Status: resting" );
				dstate_setinfo( "battery.charger.status", "%s", "resting" );
			}
			status_set( status_buf );
		}
		else if ( powerInput5vIo == POWER_NOT_PRESENT &&
		      ( powerInput   != POWER_NOT_PRESENT &&
		        powerInput   <= POWER_PRESENT ))
		{
			if ( gpio_power != 1 || usb_power != 0 )
			{
				upslogx( LOG_NOTICE, "On 5V_GPIO power" );
			}
			usb_power     = 0;
			gpio_power    = 1;
			battery_power = 0;
			upsdebugx( 1, "On 5V_GPIO power [%d:%d:%d]", usb_power, gpio_power, battery_power );

			snprintfcat( status_buf, sizeof(status_buf), "OL" );
			if ( batteryStatus == BATT_CHARGING_FROM_IN )
			{
				snprintfcat( status_buf, sizeof(status_buf), " CHRG" );
				status_set( status_buf );
				upsdebugx( 1, "Battery Charger Status: charging" );
				dstate_setinfo( "battery.charger.status", "%s", "charging" );
			}
			else if ( batteryStatus == BATT_NORMAL )
			{
				status_set( status_buf );
				upsdebugx( 1, "Battery Charger Status: resting" );
				dstate_setinfo( "battery.charger.status", "%s", "resting" );
			}
		}
		else if ( ( powerInput     != POWER_NOT_PRESENT && powerInput     <= POWER_PRESENT ) &&
		          ( powerInput5vIo != POWER_NOT_PRESENT && powerInput5vIo <= POWER_PRESENT ))
		{
			if ( usb_power != 1 || gpio_power != 1 )
			{
				upslogx( LOG_NOTICE, "On USB and 5V_GPIO power" );
			}
			usb_power     = 1;
			gpio_power    = 1;
			battery_power = 0;
			upsdebugx( 1, "On USB and 5V_GPIO power [%d:%d:%d]", usb_power, gpio_power, battery_power );

			snprintfcat( status_buf, sizeof( status_buf ), "OL" );
			if ( batteryStatus == BATT_CHARGING_FROM_IN )
			{
				snprintfcat( status_buf, sizeof(status_buf), " CHRG");
				status_set( status_buf );
				upsdebugx( 1, "Battery Charger Status: charging" );
				dstate_setinfo("battery.charger.status", "%s", "charging");
			}
			else if ( batteryStatus == BATT_NORMAL )
			{
				status_set( status_buf );
				upsdebugx( 1, "Battery Charger Status: resting" );
				dstate_setinfo( "battery.charger.status", "%s", "resting" );
			}
		}
		else if ( powerInput == POWER_NOT_PRESENT && powerInput5vIo == POWER_NOT_PRESENT )
		{
			if ( usb_power != 0 || gpio_power != 0 )
			{
				upslogx( LOG_NOTICE, "On Battery power" );
			}
			usb_power     = 0;
			gpio_power    = 0;
			battery_power = 1;
			upsdebugx( 1, "On Battery power [%d:%d:%d]", usb_power, gpio_power, battery_power );

			snprintfcat( status_buf, sizeof(status_buf), "OB DISCHRG" );
			status_set( status_buf );
		}
	}
}

static void get_battery_temperature()
{
	uint8_t cmd = BATTERY_TEMPERATURE_CMD;
	int16_t data;

	upsdebugx( 3, __func__ );

	I2C_READ_WORD( upsfd, cmd, __func__ )

	upsdebugx( 1, "Battery Temperature: %d°C", data );
	dstate_setinfo( "battery.temperature", "%d", data );
}

static void get_battery_voltage()
{
	uint8_t cmd = BATTERY_VOLTAGE_CMD;
	int16_t data;

	upsdebugx( 3, __func__ );

	I2C_READ_WORD( upsfd, cmd, __func__ )

	upsdebugx( 1, "Battery Voltage: %0.3fV", data / 1000.0 );
	dstate_setinfo( "battery.voltage", "%0.3f", data / 1000.0 );
}

static void get_battery_current()
{
	uint8_t cmd = BATTERY_CURRENT_CMD;
	int16_t data;

	upsdebugx( 3, __func__ );

	/*
	 * The reported current can actually be negative, so we cannot
	 * check for I2C failure by looking for negative values
	 */
	data = i2c_smbus_read_word_data(upsfd, cmd);

	if ( data & ( 1 << 15 ) )
	{
		data = data - ( 1 << 16 );
	}

	upsdebugx( 1, "Battery Current: %0.3fA", data / 1000.0 );
	dstate_setinfo( "battery.current", "%0.3f", data / 1000.0 );
}

static void get_io_voltage()
{
	uint8_t cmd = IO_VOLTAGE_CMD;
	int16_t data;

	upsdebugx( 3, __func__ );

	I2C_READ_WORD( upsfd, cmd, __func__ )

	upsdebugx( 1, "Input Voltage: %.3fV", data / 1000.0 );
	dstate_setinfo( "input.voltage", "%.3f", data / 1000.0 );
}

static void get_io_current()
{
	uint8_t cmd = IO_CURRENT_CMD;
	int16_t data;

	upsdebugx( 3, __func__ );

	/*
	 * The reported current can actually be negative, so we cannot
	 * check for I2C failure by looking for negative values
	 */
	data = i2c_smbus_read_word_data(upsfd, cmd);

	if ( data & ( 1 << 15 ) )
	{
		data = data - ( 1 << 16 );
	}

	upsdebugx( 1, "Input Current: %.3fA", data / 1000.0 );
	dstate_setinfo( "input.current", "%.3f", data / 1000.0 );
}

static void get_firmware_version()
{
	uint8_t cmd = FIRMWARE_VERSION_CMD;
	uint16_t data;
	uint8_t major, minor;

	upsdebugx( 3, __func__ );

	I2C_READ_WORD( upsfd, cmd, __func__ )

	major = data >> 4;
	minor = ( data << 4 & 0xf0 ) >> 4;

	if (( major != 1 ) || ( minor > 3 ))
	{
		upslogx( LOG_WARNING, "Unknown Firmware release: %d.%d", major, minor );
	}

	upsdebugx( 1, "UPS Firmware Version: %d.%d", major, minor );
	dstate_setinfo( "ups.firmware", "%d.%d", major, minor );
}

static void get_battery_profile()
{
	uint8_t cmd = BATTERY_PROFILE_CMD;
	__u8 block[I2C_SMBUS_BLOCK_MAX];

	upsdebugx( 3, __func__ );

	I2C_READ_BLOCK( upsfd, cmd, 14, block, __func__ )

	upsdebugx( 1, "Battery Capacity: %0.3fAh", ( block[1] << 8 | block[0] ) / 1000.0 );
	dstate_setinfo( "battery.capacity", "%0.3f", ( block[1] << 8 | block[0] ) / 1000.0 );
}

static void get_battery_profile_ext()
{
	uint8_t cmd = BATTERY_EXT_PROFILE_CMD;
	__u8 block[I2C_SMBUS_BLOCK_MAX];

	upsdebugx( 3, __func__ );

	I2C_READ_BLOCK( upsfd, cmd, 17, block, __func__ )

	switch( block[0] & 0xFF00 )
	{
		case 0:
			upsdebugx( 1, "Battery Chemistry: LiPO" );
			dstate_setinfo( "battery.type", "%s", "LiPO" );
			break;
		case 1:
			upsdebugx( 1, "Battery Chemistry: LiFePO4" );
			dstate_setinfo( "battery.type", "%s", "LiFePO4" );
			break;
		default:
			upsdebugx( 1, "Battery Chemistry: UNKNOWN" );
			dstate_setinfo( "battery.type", "%s", "UNKNOWN" );
	}
}

static void get_power_off()
{
	uint8_t cmd = POWER_OFF_CMD;
	uint8_t data;

	upsdebugx( 3, __func__ );

	I2C_READ_BYTE( upsfd, cmd, __func__ )

	if ( data == 255 )
	{
		upsdebugx( 1, "Power Off: DISABLED" );
	}
	else if ( data <= 250 )
	{
		upsdebugx( 1, "Power Off: %d Seconds", data );
	}
}

static void set_power_off()
{
	uint8_t cmd = POWER_OFF_CMD;

	upsdebugx( 3, __func__ );

	/*
	 * Acceptable values for shutdown_delay are 1-250,
	 * use 0/255 to clear a scheduled power off command
	 */

	if ( shutdown_delay > 250 )
	{
		upslogx(
			LOG_WARNING,
			"shutdown delay of >250 seconds requested, shortening to 250 seconds"
		);
		shutdown_delay = 250;
	}

	if ( shutdown_delay == 0 )
	{
		upslogx(
			LOG_WARNING,
			"shutdown delay of 0 seconds requested, using 1 second instead"
		);
		shutdown_delay = 1;
	}

	I2C_WRITE_BYTE( upsfd, cmd, shutdown_delay, __func__ )
}

static void get_time()
{
	uint8_t cmd = RTC_TIME_CMD;
	__u8 block[I2C_SMBUS_BLOCK_MAX];
	uint8_t second, minute, hour, day, month, subsecond;
	uint16_t year;

	upsdebugx( 3, __func__ );

	I2C_READ_BLOCK( upsfd, cmd, 9, block, __func__ )

	second     = (( (block[0] >> 4 ) & 0x07) * 10 ) + ( block[0] & 0x0F );
	minute     = (( (block[1] >> 4 ) & 0x07) * 10 ) + ( block[1] & 0x0F );
	hour       = (( (block[2] >> 4 ) & 0x03) * 10 ) + ( block[2] & 0x0F );
	day        = (( (block[4] >> 4 ) & 0x03) * 10 ) + ( block[4] & 0x0F );
	month      = (( (block[5] >> 4 ) & 0x01) * 10 ) + ( block[5] & 0x0F );
	year       = (( (block[6] >> 4 ) & 0x0F) * 10 ) + ( block[6] & 0x0F ) + 2000;
	subsecond  = block[7] * 100 / 256;

	upsdebugx( 1, "UPS Time: %02d:%02d:%02d.%02d", hour, minute, second, subsecond );
	dstate_setinfo( "ups.time", "%02d:%02d:%02d.%02d", hour, minute, second, subsecond );

	upsdebugx( 1, "UPS Date: %04d-%02d-%02d", year, month, day );
	dstate_setinfo( "ups.date", "%04d-%02d-%02d", year, month, day );
}

static void get_i2c_address()
{
	uint8_t cmd = I2C_ADDRESS_CMD;
	uint8_t data;

	upsdebugx( 3, __func__ );

	I2C_READ_BYTE( upsfd, cmd, __func__ )

	upsdebugx( 1, "I2C Address: 0x%0x", data );

	if ( data == i2c_address )
	{
		upsdebugx( 1, "Found device '0x%0x' on port '%s'",
			(unsigned int) i2c_address, device_path );
	}
	else
	{
		fatalx( EXIT_FAILURE,
			"Could not find PiJuice HAT at I2C address 0x%0x",
			i2c_address );
	}
}

void upsdrv_initinfo(void)
{

	dstate_setinfo( "ups.mfr", "%s", "PiJuice" );
	dstate_setinfo( "ups.type", "%s", "HAT" );

	/* note: for a transition period, these data are redundant */

	dstate_setinfo( "device.mfr", "%s", "PiJuice" );
	dstate_setinfo( "device.type", "%s", "HAT" );

	upsdebugx( 1, "Low Battery Threshold: %0.0f%%", LOW_BATTERY_THRESHOLD );
	dstate_setinfo( "battery.charge.low", "%0.0f", LOW_BATTERY_THRESHOLD );

	upsdebugx( 1, "Nominal Battery Voltage: %0.3fV", NOMINAL_BATTERY_VOLTAGE );
	dstate_setinfo( "battery.voltage.nominal", "%0.3f", NOMINAL_BATTERY_VOLTAGE );

	get_i2c_address();
	get_battery_profile();
	get_battery_profile_ext();
}

void upsdrv_updateinfo(void)
{
	status_init();

	get_status();
	get_battery_temperature();
	get_battery_voltage();
	get_battery_current();
	get_io_voltage();
	get_io_current();
	get_time();
	get_power_off();

	status_commit();
	dstate_dataok();
}

void upsdrv_shutdown(void)
{
	set_power_off();
}

void upsdrv_help(void)
{
	printf("\nThe default I2C address is 20 [0x14]\n");
	printf("\n");
}

void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "i2c_address", "Override i2c address setting");
}

void upsdrv_initups(void)
{
	upsfd = open_i2c_bus( device_path, i2c_address );

	/* probe ups type */
	get_firmware_version();

	/* get variables and flags from the command line */

	if (getval("i2c_address"))
		i2c_address = atoi(getval("i2c_address"));
}

void upsdrv_cleanup(void)
{
	close(upsfd);
}
