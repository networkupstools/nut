/*
 * nhs_ser.c - NUT support for NHS Nobreaks, senoidal line
 *
 *
 * Copyright (C) 2024   Lucas Willian Bocchi <lucas@lucas.inf.br>
 *     Initial Release (as nhs-nut.c)
 * Copyright (C) 2024 - 2025 Jim Klimov <jimklimov+nut@gmail.com>
 *     Codebase adjusted to NUT standards
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

#include "config.h"
#include "main.h"
#include "common.h"
#include "nut_stdint.h"
#include "nut_float.h"
#include "serial.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <termios.h>
#include <ctype.h>
#include <math.h>

#define DRIVER_NAME	"NHS Nobreak Drivers"
#define DRIVER_VERSION	"0.05"
#define MANUFACTURER	"NHS Sistemas Eletronicos LTDA"

#define DEFAULTBAUD	2400
#define DEFAULTPORT	"/dev/ttyACM0"
#define DEFAULTPF	0.9
#define DEFAULTPERC	2.0
#define DATAPACKETSIZE	100	/* NOTE: Practical anticipated max is 50 */
#define DEFAULTBATV	12.0

/*
 * Keep the historical 2400 8N1 + RTS/CTS behavior when ups.conf does not
 * specify serial options. The maximum values bound user-provided delays.
 */
#define DEFAULT_SERIAL_DATA_BITS	8
#define DEFAULT_SERIAL_PARITY		"none"
#define DEFAULT_SERIAL_STOP_BITS	1
#define DEFAULT_SERIAL_FLOW_CONTROL	"hardware"
#define DEFAULT_SERIAL_READ_TIMEOUT_MS	100
#define DEFAULT_SERIAL_SEND_PACE_US	0
#define MAX_SERIAL_READ_TIMEOUT_MS	60000
#define MAX_SERIAL_SEND_PACE_US	999999

/* comms revival attempts before declaring them stale */
#define MAXTRIES	3

/* driver description structure */
upsdrv_info_t upsdrv_info =
{
	DRIVER_NAME,
	DRIVER_VERSION,
	"Lucas Willian Bocchi <lucas@lucas.inf.br>",
	DRV_BETA,
	{ NULL }
};


/* Struct to represent serial conventions in termios.h */
typedef struct {
	speed_t	rate;	/* Constant in termios.h */
	int	speed;	/* Numeric speed, used in NUT */
	const char	*description;	/* Description */
} baud_rate_t;

static baud_rate_t baud_rates[] = {
	{ B50,		50,	"50 bps" },
	{ B75,		75,	"75 bps" },
	{ B110,		110,	"110 bps" },
	{ B134,		134,	"134.5 bps" },
	{ B150,		150,	"150 bps" },
	{ B200,		200,	"200 bps" },
	{ B300,		300,	"300 bps" },
	{ B600,		600,	"600 bps" },
	{ B1200,	1200,	"1200 bps" },
	{ B2400,	2400,	"2400 bps" },
	{ B4800,	4800,	"4800 bps" },
	{ B9600,	9600,	"9600 bps" },
	{ B19200,	19200,	"19200 bps" },
	{ B38400,	38400,	"38400 bps" },
	{ B57600,	57600,	"57600 bps" },
	{ B115200,	115200,	"115200 bps" },
	{ B230400,	230400,	"230400 bps" },
	{ B460800,	460800,	"460800 bps" },
	{ B921600,	921600,	"921600 bps" },
	{ B1500000,	1500000,	"1.5 Mbps" },
	{ B2000000,	2000000,	"2 Mbps" },
/* NOTE: Per https://github.com/networkupstools/nut/issues/3163
 * not all platforms offer all baud rates, so we wrap some into
 * conditional uses.
 */
#ifdef B2500000
	{ B2500000,	2500000,	"2.5 Mbps" },
#endif
#ifdef B3000000
	{ B3000000,	3000000,	"3 Mbps" },
#endif
#ifdef B3500000
	{ B3500000,	3500000,	"3.5 Mbps" },
#endif
#ifdef B4000000
	{ B4000000,	4000000,	"4 Mbps" },
#endif
};
#define NUM_BAUD_RATES (sizeof(baud_rates) / sizeof(baud_rates[0]))

/* Struct that contains nobreak info */
typedef struct {
	unsigned int	header;
	unsigned int	size;
	char		type;
	unsigned int	model;
	unsigned int	hardwareversion;
	unsigned int	softwareversion;
	unsigned int	configuration;
	unsigned int	configuration_array[5];
	bool		c_oem_mode;
	bool		c_buzzer_disable;
	bool		c_potmin_disable;
	bool		c_rearm_enable;
	bool		c_bootloader_enable;
	unsigned int	numbatteries;
	unsigned int	undervoltagein120V;
	unsigned int	overvoltagein120V;
	unsigned int	undervoltagein220V;
	unsigned int	overvoltagein220V;
	unsigned int	tensionout120V;
	unsigned int	tensionout220V;
	unsigned int	statusval;
	unsigned int	status[6];
	bool		s_220V_in;
	bool		s_220V_out;
	bool		s_sealed_battery;
	bool		s_show_out_tension;
	bool		s_show_temperature;
	bool		s_show_charger_current;
	unsigned int	chargercurrent;
	unsigned char	checksum;
	unsigned char	checksum_calc;
	bool		checksum_ok;
	char		serial[17];
	unsigned int	year;
	unsigned int	month;
	unsigned int	wday;
	unsigned int	hour;
	unsigned int	minute;
	unsigned int	second;
	unsigned int	alarmyear;
	unsigned int	alarmmonth;
	unsigned int	alarmwday;
	unsigned int	alarmday;
	unsigned int	alarmhour;
	unsigned int	alarmminute;
	unsigned int	alarmsecond;
	unsigned int	end_marker;
} pkt_hwinfo;

/* Struct that contains the data packet info */
typedef struct {
	unsigned int	header;
	unsigned int	length;
	char		packet_type;
	unsigned int	vacinrms_high;
	unsigned int	vacinrms_low;
	float		vacinrms;
	unsigned int	vdcmed_high;
	unsigned int	vdcmed_low;
	float		vdcmed;
	float		vdcmed_real;
	unsigned int	potrms;
	unsigned int	vacinrmsmin_high;
	unsigned int	vacinrmsmin_low;
	float		vacinrmsmin;
	unsigned int	vacinrmsmax_high;
	unsigned int	vacinrmsmax_low;
	float		vacinrmsmax;
	unsigned int	vacoutrms_high;
	unsigned int	vacoutrms_low;
	float		vacoutrms;
	unsigned int	tempmed_high;
	unsigned int	tempmed_low;
	float		tempmed;
	float		tempmed_real;
	unsigned int	icarregrms;
	unsigned int	icarregrms_real;
	float		battery_tension;
	unsigned int	perc_output;
	unsigned int	statusval;
	unsigned int	status[8];
	unsigned int	nominaltension;
	float		timeremain;
	bool		s_battery_mode;
	bool		s_battery_low;
	bool		s_network_failure;
	bool		s_fast_network_failure;
	bool		s_220_in;
	bool		s_220_out;
	bool		s_bypass_on;
	bool		s_charger_on;
	unsigned char	checksum;
	unsigned char	checksum_calc;
	bool		checksum_ok;
	unsigned int	end_marker;
} pkt_data;

/* struct that describes the nobreak model */
typedef struct {
	unsigned int	upscode;
	char		upsdesc[100];
	unsigned int	VA;
} upsinfo;

static const unsigned int	string_initialization_long[9] = {0xFF, 0x09, 0x53, 0x83, 0x00, 0x00, 0x00, 0xDF, 0xFE};
static const unsigned int	string_initialization_short[9] = {0xFF, 0x09, 0x53, 0x03, 0x00, 0x00, 0x00, 0x5F, 0xFE};

static int		debug_pkt_data = 0, debug_pkt_hwinfo = 0, debug_pkt_raw = 0;

static TYPE_FD_SER	serial_fd = ERROR_FD_SER;
static unsigned char	chr;
static size_t		datapacket_index = 0;
static bool		datapacketstart = false;
static time_t		lastdp = 0;
static unsigned int	checktime = 2000000;	/* 2 seconds */
static unsigned int	max_checktime = 6000000;	/* max wait time: 6 seconds */
static unsigned int	send_extended = 0;
static int		bwritten = 0;
static unsigned char	datapacket[DATAPACKETSIZE];
static char		porta[NUT_PATH_MAX + 1] = DEFAULTPORT;

/* Validated serial settings used for initial connection and reconnection. */
static int		baudrate = DEFAULTBAUD;
static unsigned int	serial_data_bits = DEFAULT_SERIAL_DATA_BITS;
static char		serial_parity[8] = DEFAULT_SERIAL_PARITY;
static unsigned int	serial_stop_bits = DEFAULT_SERIAL_STOP_BITS;
static char		serial_flow_control[16] = DEFAULT_SERIAL_FLOW_CONTROL;
static unsigned int	serial_read_timeout_ms = DEFAULT_SERIAL_READ_TIMEOUT_MS;
static unsigned int	serial_send_pace_us = DEFAULT_SERIAL_SEND_PACE_US;
static float		minpower = 0;
static float		maxpower = 0;
static unsigned int	minpowerperc = 0;
static unsigned int	maxpowerperc = 0;

static pkt_hwinfo lastpkthwinfo = {
	0xFF,	/* header */
	0,	/* size */
	'S',	/* type */
	0,	/* model */
	0,	/* hardwareversion */
	0,	/* softwareversion */
	0,	/* configuration */
	{0, 0, 0, 0, 0},	/* configuration_array */
	false,	/* c_oem_mode */
	false,	/* c_buzzer_disable */
	false,	/* c_potmin_disable */
	false,	/* c_rearm_enable */
	false,	/* c_bootloader_enable */
	0,	/* numbatteries */
	0,	/* undervoltagein120V */
	0,	/* overvoltagein120V */
	0,	/* undervoltagein220V */
	0,	/* overvoltagein220V */
	0,	/* tensionout120V */
	0,	/* tensionout220V */
	0,	/* statusval */
	{0, 0, 0, 0, 0, 0},	/* status */
	false,	/* s_220V_in */
	false,	/* s_220V_out */
	false,	/* s_sealed_battery */
	false,	/* s_show_out_tension */
	false,	/* s_show_temperature */
	false,	/* s_show_charger_current */
	0,	/* chargercurrent */
	0,	/* checksum */
	0,	/* checksum_calc */
	false,	/* checksum_ok */
	"----------------",	/* serial */
	0,	/* year */
	0,	/* month */
	0,	/* wday */
	0,	/* hour */
	0,	/* minute */
	0,	/* second */
	0,	/* alarmyear */
	0,	/* alarmmonth */
	0,	/* alarmwday */
	0,	/* alarmday */
	0,	/* alarmhour */
	0,	/* alarmminute */
	0,	/* alarmsecond */
	0xFE	/* end_marker */
};

static pkt_data lastpktdata = {
	0xFF,	/* header */
	0x21,	/* length */
	'D',	/* packet_type */
	0,	/* vacinrms_high */
	0,	/* vacinrms_low */
	0,	/* vacinrms */
	0,	/* vdcmed_high */
	0,	/* vdcmed_low */
	0,	/* vdcmed */
	0,	/* vdcmed_real */
	0,	/* potrms */
	0,	/* vacinrmsmin_high */
	0,	/* vacinrmsmin_low */
	0,	/* vacinrmsmin */
	0,	/* vacinrmsmax_high */
	0,	/* vacinrmsmax_low */
	0,	/* vacinrmsmax */
	0,	/* vacoutrms_high */
	0,	/* vacoutrms_low */
	0,	/* vacoutrms */
	0,	/* tempmed_high */
	0,	/* tempmed_low */
	0,	/* tempmed */
	0,	/* tempmed_real */
	0,	/* icarregrms */
	0,	/* icarregrms_real */
	0,	/* battery_tension */
	0,	/* perc_output */
	0,	/* statusval */
	{0, 0, 0, 0, 0, 0, 0, 0},	/* status */
	0,	/* nominaltension */
	0.0f,	/* timeremain */
	false,	/* s_battery_mode */
	false,	/* s_battery_low */
	false,	/* s_network_failure */
	false,	/* s_fast_network_failure */
	false,	/* s_220_in */
	false,	/* s_220_out */
	false,	/* s_bypass_on */
	false,	/* s_charger_on */
	0,	/* checksum */
	0,	/* checksum_calc */
	false,	/* checksum_ok */
	0xFE	/* end_marker */
};

/* internal methods */
static int get_bit_in_position(void *ptr, size_t size, size_t bit_position, int invertorder);
static float createfloat(int integer, int decimal);
#if 0
static char * strtolow(char* s);
#endif

static unsigned char calculate_checksum(unsigned char *pacote, int inicio, int fim);
static float calculate_efficiency(float vacoutrms, float vacinrms);

static void parse_serial_options(void);
static void close_serial_port(void);
static TYPE_FD_SER openfd(const char *portarg, int requested_baudrate);
#if 0
static int write_serial(int fd, const char * dados, int size);
#endif
static int write_serial_int(TYPE_FD_SER fd, const unsigned int *data, size_t size);

static void print_pkt_hwinfo(pkt_hwinfo data);
static void print_pkt_data(pkt_data data);
static void pdatapacket(unsigned char *datapkt, size_t size);
static pkt_data mount_datapacket(unsigned char *datapkt, size_t size, double tempodecorrido, pkt_hwinfo pkt_upsinfo);
static pkt_hwinfo mount_hwinfo(unsigned char *datapkt, size_t size);
static upsinfo getupsinfo(unsigned int upscode);

static unsigned int get_va(int equipment);
static unsigned int get_vbat(void);
static float get_pf(void);
static unsigned int get_ah(void);
static float get_vin_perc(char *var);
static unsigned int get_numbat(void);


/* method implementations */

static int get_bit_in_position(void *ptr, size_t size, size_t bit_position, int invertorder) {
	unsigned char	*byte_ptr = (unsigned char *)ptr;
	int	retval = -2;
	size_t	byte_index = bit_position / 8;
	size_t	bit_index = bit_position % 8;

	if (bit_position >= size * 8) {
		return -3;	/* Invalid Position */
	}

	if (invertorder == 0)
		retval = (byte_ptr[byte_index] >> (7 - bit_index)) & 1 ? 1 : 0;
	else
		retval = (byte_ptr[byte_index] >> (7 - bit_index)) & 1 ? 0 : 1;
	return retval;
}

static void print_pkt_hwinfo(pkt_hwinfo data) {
	int	i = 0;

	if (!debug_pkt_hwinfo)
		return;

	upsdebugx(1, "%s: logging packet details at debug verbosity 5 or more", __func__);

	upsdebugx(5, "Header: %u", data.header);
	upsdebugx(5, "Size: %u", data.size);
	upsdebugx(5, "Type: %c", data.type);
	upsdebugx(5, "Model: %u", data.model);
	upsdebugx(5, "Hardware Version: %u", data.hardwareversion);
	upsdebugx(5, "Software Version: %u", data.softwareversion);
	upsdebugx(5, "Configuration: %u", data.configuration);

	upsdebugx(5, "Configuration Array: ");
	upsdebugx(5, "-----");
	for (i = 0; i < 5; i++) {
		int	retorno = get_bit_in_position(&data.configuration, sizeof(data.configuration), i, 0);
		upsdebugx(5, "Binary value is %d", retorno);
		upsdebugx(5, "%u ", data.configuration_array[i]);
	}
	upsdebugx(5, "-----");

	upsdebugx(5, "OEM Mode: %s", data.c_oem_mode ? "true" : "false");
	upsdebugx(5, "Buzzer Disable: %s", data.c_buzzer_disable ? "true" : "false");
	upsdebugx(5, "Potmin Disable: %s", data.c_potmin_disable ? "true" : "false");
	upsdebugx(5, "Rearm Enable: %s", data.c_rearm_enable ? "true" : "false");
	upsdebugx(5, "Bootloader Enable: %s", data.c_bootloader_enable ? "true" : "false");
	upsdebugx(5, "Number of Batteries: %u", data.numbatteries);
	upsdebugx(5, "Undervoltage In 120V: %u", data.undervoltagein120V);
	upsdebugx(5, "Overvoltage In 120V: %u", data.overvoltagein120V);
	upsdebugx(5, "Undervoltage In 220V: %u", data.undervoltagein220V);
	upsdebugx(5, "Overvoltage In 220V: %u", data.overvoltagein220V);
	upsdebugx(5, "Tension Out 120V: %u", data.tensionout120V);
	upsdebugx(5, "Tension Out 220V: %u", data.tensionout220V);
	upsdebugx(5, "Status Value: %u", data.statusval);

	upsdebugx(5, "Status: ");
	upsdebugx(5, "-----");
	for (i = 0; i < 6; i++) {
		upsdebugx(5, "Binary value is %d", get_bit_in_position(&data.statusval, sizeof(data.statusval), i, 0));
		upsdebugx(5, "status %d --> %u ", i, data.status[i]);
	}
	upsdebugx(5, "-----");

	upsdebugx(5, "220V In: %s", data.s_220V_in ? "true" : "false");
	upsdebugx(5, "220V Out: %s", data.s_220V_out ? "true" : "false");
	upsdebugx(5, "Sealed Battery: %s", data.s_sealed_battery ? "true" : "false");
	upsdebugx(5, "Show Out Tension: %s", data.s_show_out_tension ? "true" : "false");
	upsdebugx(5, "Show Temperature: %s", data.s_show_temperature ? "true" : "false");
	upsdebugx(5, "Show Charger Current: %s", data.s_show_charger_current ? "true" : "false");
	upsdebugx(5, "Charger Current: %u", data.chargercurrent);
	upsdebugx(5, "Checksum: %u", data.checksum);
	upsdebugx(5, "Checksum Calc: %u", data.checksum_calc);
	upsdebugx(5, "Checksum OK: %s", data.checksum_ok ? "true" : "false");
	upsdebugx(5, "Serial: %s", data.serial);
	upsdebugx(5, "Year: %u", data.year);
	upsdebugx(5, "Month: %u", data.month);
	upsdebugx(5, "Weekday: %u", data.wday);
	upsdebugx(5, "Hour: %u", data.hour);
	upsdebugx(5, "Minute: %u", data.minute);
	upsdebugx(5, "Second: %u", data.second);
	upsdebugx(5, "Alarm Year: %u", data.alarmyear);
	upsdebugx(5, "Alarm Month: %u", data.alarmmonth);
	upsdebugx(5, "Alarm Weekday: %u", data.alarmwday);
	upsdebugx(5, "Alarm Day: %u", data.alarmday);
	upsdebugx(5, "Alarm Hour: %u", data.alarmhour);
	upsdebugx(5, "Alarm Minute: %u", data.alarmminute);
	upsdebugx(5, "Alarm Second: %u", data.alarmsecond);
	upsdebugx(5, "End Marker: %u", data.end_marker);
}

static void print_pkt_data(pkt_data data) {
	int	i = 0;

	if (!debug_pkt_data)
		return;

	upsdebugx(1, "%s: logging packet details at debug verbosity 5 or more", __func__);

	upsdebugx(5, "Header: %u", data.header);
	upsdebugx(5, "Length: %u", data.length);
	upsdebugx(5, "Packet Type: %c", data.packet_type);
	upsdebugx(5, "Vacin RMS High: %u", data.vacinrms_high);
	upsdebugx(5, "Vacin RMS Low: %u", data.vacinrms_low);
	upsdebugx(5, "Vacin RMS: %0.2f", data.vacinrms);
	upsdebugx(5, "VDC Med High: %u", data.vdcmed_high);
	upsdebugx(5, "VDC Med Low: %u", data.vdcmed_low);
	upsdebugx(5, "VDC Med: %0.2f", data.vdcmed);
	upsdebugx(5, "VDC Med Real: %0.2f", data.vdcmed_real);
	upsdebugx(5, "Pot RMS: %u", data.potrms);
	upsdebugx(5, "Vacin RMS Min High: %u", data.vacinrmsmin_high);
	upsdebugx(5, "Vacin RMS Min Low: %u", data.vacinrmsmin_low);
	upsdebugx(5, "Vacin RMS Min: %0.2f", data.vacinrmsmin);
	upsdebugx(5, "Vacin RMS Max High: %u", data.vacinrmsmax_high);
	upsdebugx(5, "Vacin RMS Max Low: %u", data.vacinrmsmax_low);
	upsdebugx(5, "Vacin RMS Max: %0.2f", data.vacinrmsmax);
	upsdebugx(5, "Vac Out RMS High: %u", data.vacoutrms_high);
	upsdebugx(5, "Vac Out RMS Low: %u", data.vacoutrms_low);
	upsdebugx(5, "Vac Out RMS: %0.2f", data.vacoutrms);
	upsdebugx(5, "Temp Med High: %u", data.tempmed_high);
	upsdebugx(5, "Temp Med Low: %u", data.tempmed_low);
	upsdebugx(5, "Temp Med: %0.2f", data.tempmed);
	upsdebugx(5, "Temp Med Real: %0.2f", data.tempmed_real);
	upsdebugx(5, "Icar Reg RMS: %u", data.icarregrms);
	upsdebugx(5, "Icar Reg RMS Real: %u", data.icarregrms_real);
	upsdebugx(5, "Battery Tension: %0.2f", data.battery_tension);
	upsdebugx(5, "Perc Output: %u", data.perc_output);
	upsdebugx(5, "Status Value: %u", data.statusval);

	upsdebugx(5, "Status: ");
	upsdebugx(5, "-----");
	for (i = 0; i < 8; i++) {
		upsdebugx(5, "Binary value is %d", get_bit_in_position(&data.statusval, sizeof(data.statusval), i, 0));
		upsdebugx(5, "status %d --> %u ", i, data.status[i]);
	}
	upsdebugx(5, "-----");

	upsdebugx(5, "Nominal Tension: %u", data.nominaltension);
	upsdebugx(5, "Time Remain: %0.2f", data.timeremain);
	upsdebugx(5, "Battery Mode: %s", data.s_battery_mode ? "true" : "false");
	upsdebugx(5, "Battery Low: %s", data.s_battery_low ? "true" : "false");
	upsdebugx(5, "Network Failure: %s", data.s_network_failure ? "true" : "false");
	upsdebugx(5, "Fast Network Failure: %s", data.s_fast_network_failure ? "true" : "false");
	upsdebugx(5, "220 In: %s", data.s_220_in ? "true" : "false");
	upsdebugx(5, "220 Out: %s", data.s_220_out ? "true" : "false");
	upsdebugx(5, "Bypass On: %s", data.s_bypass_on ? "true" : "false");
	upsdebugx(5, "Charger On: %s", data.s_charger_on ? "true" : "false");
	upsdebugx(5, "Checksum: %u", data.checksum);
	upsdebugx(5, "Checksum Calc: %u", data.checksum_calc);
	upsdebugx(5, "Checksum OK: %s", data.checksum_ok ? "true" : "false");
	upsdebugx(5, "End Marker: %u", data.end_marker);
}

/*
 * Load the seven supported serial options from ups.conf. Defaults are reset
 * first so each setting has one source of truth, and every supplied value is
 * fully validated before the serial port is touched.
 */
static void parse_serial_options(void)
{
	const char	*value;
	char		*endptr;
	long		number;
	size_t		i;
	int		supported;

	baudrate = DEFAULTBAUD;
	serial_data_bits = DEFAULT_SERIAL_DATA_BITS;
	snprintf(serial_parity, sizeof(serial_parity), "%s", DEFAULT_SERIAL_PARITY);
	serial_stop_bits = DEFAULT_SERIAL_STOP_BITS;
	snprintf(serial_flow_control, sizeof(serial_flow_control), "%s",
		DEFAULT_SERIAL_FLOW_CONTROL);
	serial_read_timeout_ms = DEFAULT_SERIAL_READ_TIMEOUT_MS;
	serial_send_pace_us = DEFAULT_SERIAL_SEND_PACE_US;

	/*
	 * The existing table is authoritative: platform-dependent entries guarded
	 * in the original source are accepted only when present in this build.
	 */
	value = getval("baud");
	if (value) {
		errno = 0;
		endptr = NULL;
		number = strtol(value, &endptr, 10);
		if (errno != 0 || endptr == value || *endptr != '\0' || number <= 0) {
			fatalx(EXIT_FAILURE,
				"Invalid baud value '%s': expected a supported baud rate",
				value);
		}

		supported = 0;
		for (i = 0; i < NUM_BAUD_RATES; i++) {
			if (number == baud_rates[i].speed) {
				supported = 1;
				break;
			}
		}
		if (!supported) {
			fatalx(EXIT_FAILURE,
				"Invalid baud value '%s': rate is not available in this build",
				value);
		}
		baudrate = (int)number;
	}

	value = getval("serial_data_bits");
	if (value) {
		/* Reject partial numbers and anything outside the termios CS5..CS8 set. */
		errno = 0;
		endptr = NULL;
		number = strtol(value, &endptr, 10);
		if (errno != 0 || endptr == value || *endptr != '\0' ||
			(number != 5 && number != 6 && number != 7 && number != 8)) {
			fatalx(EXIT_FAILURE,
				"Invalid serial_data_bits value '%s': expected 5, 6, 7 or 8",
				value);
		}
		serial_data_bits = (unsigned int)number;
	}

	value = getval("serial_parity");
	if (value) {
		if (strcasecmp(value, "none") != 0 &&
			strcasecmp(value, "even") != 0 &&
			strcasecmp(value, "odd") != 0) {
			fatalx(EXIT_FAILURE,
				"Invalid serial_parity value '%s': expected none, even or odd",
				value);
		}
		snprintf(serial_parity, sizeof(serial_parity), "%s", value);
	}

	value = getval("serial_stop_bits");
	if (value) {
		errno = 0;
		endptr = NULL;
		number = strtol(value, &endptr, 10);
		if (errno != 0 || endptr == value || *endptr != '\0' ||
			(number != 1 && number != 2)) {
			fatalx(EXIT_FAILURE,
				"Invalid serial_stop_bits value '%s': expected 1 or 2",
				value);
		}
		serial_stop_bits = (unsigned int)number;
	}

	value = getval("serial_flow_control");
	if (value) {
		if (strcasecmp(value, "none") != 0 &&
			strcasecmp(value, "hardware") != 0 &&
			strcasecmp(value, "software") != 0 &&
			strcasecmp(value, "both") != 0) {
			fatalx(EXIT_FAILURE,
				"Invalid serial_flow_control value '%s': expected "
				"none, hardware, software or both",
				value);
		}
		snprintf(serial_flow_control, sizeof(serial_flow_control), "%s", value);
	}

	value = getval("serial_read_timeout_ms");
	if (value) {
		/* A zero timeout requests a non-blocking poll from ser_get_char. */
		errno = 0;
		endptr = NULL;
		number = strtol(value, &endptr, 10);
		if (errno != 0 || endptr == value || *endptr != '\0' ||
			number < 0 || number > MAX_SERIAL_READ_TIMEOUT_MS) {
			fatalx(EXIT_FAILURE,
				"Invalid serial_read_timeout_ms value '%s': expected 0 to %d",
				value, MAX_SERIAL_READ_TIMEOUT_MS);
		}
		serial_read_timeout_ms = (unsigned int)number;
	}

	value = getval("serial_send_pace_us");
	if (value) {
		/* Zero selects an unpaced send; positive values delay each byte. */
		errno = 0;
		endptr = NULL;
		number = strtol(value, &endptr, 10);
		if (errno != 0 || endptr == value || *endptr != '\0' ||
			number < 0 || number > MAX_SERIAL_SEND_PACE_US) {
			fatalx(EXIT_FAILURE,
				"Invalid serial_send_pace_us value '%s': expected 0 to %d",
				value, MAX_SERIAL_SEND_PACE_US);
		}
		serial_send_pace_us = (unsigned int)number;
	}
}

static void close_serial_port(void)
{
	/* Use NUT's portable descriptor checks and always invalidate after closing. */
	if (VALID_FD_SER(serial_fd)) {
		if (ser_close(serial_fd, porta) != 0) {
			upsdebug_with_errno(1, "%s: Error closing serial port %s",
				__func__, porta);
		}
		serial_fd = ERROR_FD_SER;
	}
}

static TYPE_FD_SER openfd(const char *portarg, int requested_baudrate)
{
	TYPE_FD_SER	fd;
	struct termios	tty;
	speed_t		rate = 0;
	size_t		i;

	/* Translate the numeric ups.conf value to the matching termios constant. */
	for (i = 0; i < NUM_BAUD_RATES; i++) {
		if (baud_rates[i].speed == requested_baudrate) {
			rate = baud_rates[i].rate;
			upsdebugx(1, "%s: Selected baud rate %d -- %s",
				__func__, baud_rates[i].speed,
				baud_rates[i].description);
			break;
		}
	}

	if (rate == 0) {
		upslogx(LOG_ERR, "%s: Unsupported baud rate %d",
			__func__, requested_baudrate);
		return ERROR_FD_SER;
	}

	/*
	 * Establish NUT's standard raw serial baseline first, then customize only
	 * data bits, parity, stop bits and flow control below.
	 */
	fd = ser_open_nf(portarg);
	if (INVALID_FD_SER(fd)) {
		upsdebug_with_errno(1, "%s: Error opening %s", __func__, portarg);
		return ERROR_FD_SER;
	}

	if (ser_set_speed_nf(fd, portarg, rate) != 0) {
		upsdebug_with_errno(1, "%s: Error setting baud rate on %s",
			__func__, portarg);
		ser_close(fd, portarg);
		return ERROR_FD_SER;
	}

	if (tcgetattr(fd, &tty) != 0) {
		upsdebug_with_errno(1, "%s: Error reading serial settings from %s",
			__func__, portarg);
		ser_close(fd, portarg);
		return ERROR_FD_SER;
	}

	/* Replace the baseline character size with the validated selection. */
	tty.c_cflag &= ~CSIZE;
	if (serial_data_bits == 5)
		tty.c_cflag |= CS5;
	else if (serial_data_bits == 6)
		tty.c_cflag |= CS6;
	else if (serial_data_bits == 7)
		tty.c_cflag |= CS7;
	else
		tty.c_cflag |= CS8;

	/*
	 * Configure parity as a complete unit. Parity errors are ignored for
	 * "none", but checked for the even and odd modes.
	 */
	tty.c_cflag &= ~(PARENB | PARODD);
	tty.c_iflag &= ~INPCK;
	if (strcasecmp(serial_parity, "none") == 0) {
		tty.c_iflag |= IGNPAR;
	}
	else if (strcasecmp(serial_parity, "even") == 0) {
		tty.c_iflag &= ~IGNPAR;
		tty.c_cflag |= PARENB;
		tty.c_iflag |= INPCK;
	}
	else {
		tty.c_iflag &= ~IGNPAR;
		tty.c_cflag |= PARENB | PARODD;
		tty.c_iflag |= INPCK;
	}

	/* CSTOPB clear means one stop bit; set means two stop bits. */
	if (serial_stop_bits == 2)
		tty.c_cflag |= CSTOPB;
	else
		tty.c_cflag &= ~CSTOPB;

	/*
	 * Disable every flow-control mechanism before enabling the selected mode.
	 * In particular, "none" leaves RTS/CTS off for CDC-ACM and 3-wire links.
	 * Both mechanisms are disabled here; the configured mode is enabled below.
	 */
	tty.c_cflag &= ~CRTSCTS;
	tty.c_iflag &= ~(IXON | IXOFF | IXANY);

	if (strcasecmp(serial_flow_control, "none") == 0) {
		/* Hardware and software flow control remain disabled. */
	}
	else if (strcasecmp(serial_flow_control, "hardware") == 0) {
		tty.c_cflag |= CRTSCTS;
	}
	else if (strcasecmp(serial_flow_control, "software") == 0) {
		tty.c_iflag |= IXON | IXOFF;
	}
	else if (strcasecmp(serial_flow_control, "both") == 0) {
		tty.c_cflag |= CRTSCTS;
		tty.c_iflag |= IXON | IXOFF;
	}

	/* Apply the four conventional settings together. */
	if (tcsetattr(fd, TCSANOW, &tty) != 0) {
		upsdebug_with_errno(1, "%s: Error applying serial settings to %s",
			__func__, portarg);
		ser_close(fd, portarg);
		return ERROR_FD_SER;
	}

	/* Discard bytes queued under any previous port configuration. */
	if (ser_flush_io(fd) != 0) {
		upsdebug_with_errno(1, "%s: Error flushing serial port %s",
			__func__, portarg);
		ser_close(fd, portarg);
		return ERROR_FD_SER;
	}

	upsdebugx(1, "%s: Serial settings: %d baud, %u data bits, %s parity, "
		"%u stop bit(s), %s flow control, read timeout %u ms, "
		"send pace %u us",
		__func__, requested_baudrate, serial_data_bits, serial_parity,
		serial_stop_bits, serial_flow_control,
		serial_read_timeout_ms, serial_send_pace_us);

	return fd;
}

static unsigned char calculate_checksum(unsigned char *pacote, int inicio, int fim) {
	int	soma = 0, i = 0;

	for (i = inicio; i <= fim; i++) {
		soma += pacote[i];
	}
	return (soma & 0xFF);
}

static void pdatapacket(unsigned char *datapkt, size_t size) {
	size_t	i = 0;

	if (!debug_pkt_raw)
		return;

	if (datapkt != NULL) {
		/* FIXME: convert to upsdebug_hex()? */
		upsdebugx(1, "%s: logging received data packet bytes at debug verbosity 5 or more", __func__);

		for (i = 0; i < size; i++) {
			upsdebugx(5, "\tPosition %" PRIuSIZE " -- 0x%02X -- Decimal %d -- Char %c", i, datapkt[i], datapkt[i], datapkt[i]);
		}
	}
}

static float createfloat(int integer, int decimal) {
	char	flt[1024];
	snprintf(flt, sizeof(flt), "%d.%d", integer, decimal);
	return atof(flt);
}

static unsigned int get_vbat(void) {
	char	*v = getval("vbat");
	if (v) {
		return atoi(v);
	}
	else {
		return DEFAULTBATV;
	}
}

static pkt_data mount_datapacket(unsigned char *datapkt, size_t size, double tempodecorrido, pkt_hwinfo pkt_upsinfo)  {
	size_t	i = 0;
	unsigned int	vbat = 0;
	unsigned char	checksum = 0x00;
	pkt_data	pktdata = {
		0xFF,	/* header */
		0x21,	/* length */
		'D',	/* packet_type */
		0,	/* vacinrms_high */
		0,	/* vacinrms_low */
		0,	/* vacinrms */
		0,	/* vdcmed_high */
		0,	/* vdcmed_low */
		0,	/* vdcmed */
		0,	/* vdcmed_real */
		0,	/* potrms */
		0,	/* vacinrmsmin_high */
		0,	/* vacinrmsmin_low */
		0,	/* vacinrmsmin */
		0,	/* vacinrmsmax_high */
		0,	/* vacinrmsmax_low */
		0,	/* vacinrmsmax */
		0,	/* vacoutrms_high */
		0,	/* vacoutrms_low */
		0,	/* vacoutrms */
		0,	/* tempmed_high */
		0,	/* tempmed_low */
		0,	/* tempmed */
		0,	/* tempmed_real */
		0,	/* icarregrms */
		0,	/* icarregrms_real */
		0,	/* battery_tension */
		0,	/* perc_output */
		0,	/* statusval */
		{0, 0, 0, 0, 0, 0, 0, 0},	/* status */
		0,	/* nominaltension */
		0.0f,	/* timeremain */
		false,	/* s_battery_mode */
		false,	/* s_battery_low */
		false,	/* s_network_failure */
		false,	/* s_fast_network_failure */
		false,	/* s_220_in */
		false,	/* s_220_out */
		false,	/* s_bypass_on */
		false,	/* s_charger_on */
		0,	/* checksum */
		0,	/* checksum_calc */
		false,	/* checksum_ok */
		0xFE	/* end_marker */
	};

	NUT_UNUSED_VARIABLE(tempodecorrido);

	pktdata.length = (int)datapkt[1];
	pktdata.packet_type = datapkt[2];
	pktdata.vacinrms_high = (int)datapkt[3];
	pktdata.vacinrms_low = (int)datapkt[4];
	pktdata.vacinrms = createfloat(pktdata.vacinrms_high, pktdata.vacinrms_low);
	pktdata.vdcmed_high = (int)datapkt[5];
	pktdata.vdcmed_low = (int)datapkt[6];
	pktdata.vdcmed = createfloat(pktdata.vdcmed_high, pktdata.vdcmed_low);
	pktdata.vdcmed_real = pktdata.vdcmed;
	if (pktdata.vdcmed_low == 0)
		pktdata.vdcmed_real = pktdata.vdcmed / 10;
	pktdata.potrms = (int)datapkt[7];
	pktdata.vacinrmsmin_high = (int)datapkt[8];
	pktdata.vacinrmsmin_low = (int)datapkt[9];
	pktdata.vacinrmsmin = createfloat(pktdata.vacinrmsmin_high, pktdata.vacinrmsmin_low);
	pktdata.vacinrmsmax_high = (int)datapkt[10];
	pktdata.vacinrmsmax_low = (int)datapkt[11];
	pktdata.vacinrmsmax = createfloat(pktdata.vacinrmsmax_high, pktdata.vacinrmsmax_low);
	pktdata.vacoutrms_high = (int)datapkt[12];
	pktdata.vacoutrms_low = (int)datapkt[13];
	pktdata.vacoutrms = createfloat(pktdata.vacoutrms_high, pktdata.vacoutrms_low);
	pktdata.tempmed_high = (int)datapkt[14];
	pktdata.tempmed_low = (int)datapkt[15];
	pktdata.tempmed = createfloat(pktdata.tempmed_high, pktdata.tempmed_low);
	pktdata.tempmed_real = pktdata.tempmed;
	pktdata.icarregrms = (int)datapkt[16];
	/* 25 units = 750mA, then 1 unit = 30mA */
	pktdata.icarregrms_real = pktdata.icarregrms * 30;
	pktdata.statusval = datapkt[17];
	for (i = 0; i < 8; i++)
		pktdata.status[i] = get_bit_in_position(&datapkt[17], sizeof(datapkt[17]), i, 0);

	/* I don't know WHY, but bit order is INVERTED here.
	 * Discovered on clyra's github python implementation
	 */
	/* TODO: check if ANY OTHER VARIABLES (like hardware array bits)
	 *  have same problem. I won't have so much equipment to test
	 *  these, then we need help to test more
	 */
	pktdata.s_battery_mode = (bool)pktdata.status[7];
	pktdata.s_battery_low = (bool)pktdata.status[6];
	pktdata.s_network_failure = (bool)pktdata.status[5];
	pktdata.s_fast_network_failure = (bool)pktdata.status[4];
	pktdata.s_220_in = (bool)pktdata.status[3];
	pktdata.s_220_out = (bool)pktdata.status[2];
	pktdata.s_bypass_on = (bool)pktdata.status[1];
	pktdata.s_charger_on = (bool)pktdata.status[0];
	/* Position 18 means status, but I won't discover what's it */
	pktdata.checksum = datapkt[19];
	checksum = calculate_checksum(datapkt, 1, 18);
	pktdata.checksum_calc = checksum;
	if (pktdata.checksum == checksum)
		pktdata.checksum_ok = true;
	/* Then, the calculations to obtain some useful information */
	if (pkt_upsinfo.size > 0) {
		pktdata.battery_tension = pkt_upsinfo.numbatteries * pktdata.vdcmed_real;
		/* Calculate battery percent utilization:
		 * if one battery cell has 12V, then the
		 * maximum out voltage is `numbatteries * 12V`
		 * This is the watermark to low battery
		 */
		/* TODO: test with external battery bank to see if
		 * calculation is valid. Can generate false positive
		 */
		vbat = get_vbat();
		pktdata.nominaltension = vbat * pkt_upsinfo.numbatteries;
		if (pktdata.nominaltension > 0) {
			pktdata.perc_output = (pktdata.battery_tension * 100) / pktdata.nominaltension;
			if (pktdata.perc_output > 100)
				pktdata.perc_output = 100;
		}	/* end if */
	}	/* end if */

	if (debug_pkt_data) {
		pdatapacket(datapkt, size);
		print_pkt_data(pktdata);
	}

	return pktdata;
}

static pkt_hwinfo mount_hwinfo(unsigned char *datapkt, size_t size) {
	size_t	i = 0;
	unsigned char	checksum = 0x00;
	pkt_hwinfo	pkthwinfo = {
		0xFF,	/* header */
		0,	/* size */
		'S',	/* type */
		0,	/* model */
		0,	/* hardwareversion */
		0,	/* softwareversion */
		0,	/* configuration */
		{0, 0, 0, 0, 0},	/* configuration_array */
		false,	/* c_oem_mode */
		false,	/* c_buzzer_disable */
		false,	/* c_potmin_disable */
		false,	/* c_rearm_enable */
		false,	/* c_bootloader_enable */
		0,	/* numbatteries */
		0,	/* undervoltagein120V */
		0,	/* overvoltagein120V */
		0,	/* undervoltagein220V */
		0,	/* overvoltagein220V */
		0,	/* tensionout120V */
		0,	/* tensionout220V */
		0,	/* statusval */
		{0, 0, 0, 0, 0, 0},	/* status */
		false,	/* s_220V_in */
		false,	/* s_220V_out */
		false,	/* s_sealed_battery */
		false,	/* s_show_out_tension */
		false,	/* s_show_temperature */
		false,	/* s_show_charger_current */
		0,	/* chargercurrent */
		0,	/* checksum */
		0,	/* checksum_calc */
		false,	/* checksum_ok */
		"----------------",	/* serial */
		0,	/* year */
		0,	/* month */
		0,	/* wday */
		0,	/* hour */
		0,	/* minute */
		0,	/* second */
		0,	/* alarmyear */
		0,	/* alarmmonth */
		0,	/* alarmwday */
		0,	/* alarmday */
		0,	/* alarmhour */
		0,	/* alarmminute */
		0,	/* alarmsecond */
		0xFE	/* end_marker */
	};

	pkthwinfo.size = (int)datapkt[1];
	pkthwinfo.type = datapkt[2];
	pkthwinfo.model = (int)datapkt[3];
	pkthwinfo.hardwareversion = (int)datapkt[4];
	pkthwinfo.softwareversion = (int)datapkt[5];
	pkthwinfo.configuration = datapkt[6];
	for (i = 0; i < 5; i++)
		pkthwinfo.configuration_array[i] = get_bit_in_position(&datapkt[6], sizeof(datapkt[6]), i, 0);
	/* TODO: check if ANY OTHER VARIABLES (like hardware array bits)
	 * have same problem. I won't have so much equipment to test
	 * these, then we need help to test more */
	pkthwinfo.c_oem_mode = (bool)pkthwinfo.configuration_array[0];
	pkthwinfo.c_buzzer_disable = (bool)pkthwinfo.configuration_array[1];
	pkthwinfo.c_potmin_disable = (bool)pkthwinfo.configuration_array[2];
	pkthwinfo.c_rearm_enable = (bool)pkthwinfo.configuration_array[3];
	pkthwinfo.c_bootloader_enable = (bool)pkthwinfo.configuration_array[4];
	pkthwinfo.numbatteries = (int)datapkt[7];
	pkthwinfo.undervoltagein120V = (int)datapkt[8];
	pkthwinfo.overvoltagein120V = (int)datapkt[9];
	pkthwinfo.undervoltagein220V = (int)datapkt[10];
	pkthwinfo.overvoltagein220V = (int)datapkt[11];
	pkthwinfo.tensionout120V = (int)datapkt[12];
	pkthwinfo.tensionout220V = (int)datapkt[13];
	pkthwinfo.statusval = datapkt[14];
	for (i = 0; i < 6; i++)
		pkthwinfo.status[i] = get_bit_in_position(&datapkt[14], sizeof(datapkt[14]), i, 0);
	/* TODO: check if ANY OTHER VARIABLES (like hardware array bits)
	 * have same problem. I won't have so much equipment to test
	 * these, then we need help to test more */
	pkthwinfo.s_220V_in = (bool)pkthwinfo.status[0];
	pkthwinfo.s_220V_out = (bool)pkthwinfo.status[1];
	pkthwinfo.s_sealed_battery = (bool)pkthwinfo.status[2];
	pkthwinfo.s_show_out_tension = (bool)pkthwinfo.status[3];
	pkthwinfo.s_show_temperature = (bool)pkthwinfo.status[4];
	pkthwinfo.s_show_charger_current = (bool)pkthwinfo.status[5];
	pkthwinfo.chargercurrent = (int)datapkt[15];
	if (pkthwinfo.size > 18) {
		for (i = 0; i < 16; i++)
			pkthwinfo.serial[i] = datapkt[16 + i];
		pkthwinfo.year = datapkt[32];
		pkthwinfo.month = datapkt[33];
		pkthwinfo.wday = datapkt[34];
		pkthwinfo.hour = datapkt[35];
		pkthwinfo.minute = datapkt[36];
		pkthwinfo.second = datapkt[37];
		pkthwinfo.alarmyear = datapkt[38];
		pkthwinfo.alarmmonth = datapkt[39];
		pkthwinfo.alarmday = datapkt[40];
		pkthwinfo.alarmhour = datapkt[41];
		pkthwinfo.alarmminute = datapkt[42];
		pkthwinfo.alarmsecond = datapkt[43];
		pkthwinfo.checksum = datapkt[48];
		checksum = calculate_checksum(datapkt, 1, 47);
	}	/* end if */
	else {
		pkthwinfo.checksum = datapkt[16];
		checksum = calculate_checksum(datapkt, 1, 15);
	}
	pkthwinfo.checksum_calc = checksum;
	if (pkthwinfo.checksum == checksum)
		pkthwinfo.checksum_ok = true;

	if (debug_pkt_hwinfo) {
		pdatapacket(datapkt, size);
		print_pkt_hwinfo(pkthwinfo);
	}

	return pkthwinfo;
}

#if 0
static int write_serial(int fd, const char *dados, size_t size) {
	if (fd > 0) {
		ssize_t	bytes_written = write(fd, dados, size);
		if (bytes_written < 0)
			return -1;
		if (tcdrain(fd) != 0)
			return -2;
		return size;
	}
	else
		return fd;
}
#endif

static int write_serial_int(TYPE_FD_SER fd, const unsigned int *data, size_t size)
{
	uint8_t	*message;
	ssize_t	sent;
	size_t	i;

	if (INVALID_FD_SER(fd))
		return -1;

	/*
	 * NHS commands are declared as unsigned int arrays in the original
	 * protocol code. Convert each element to the byte buffer expected by the
	 * NUT serial API without changing command contents or framing.
	 */
	message = (uint8_t *)xcalloc(size, sizeof(*message));
	for (i = 0; i < size; i++)
		message[i] = (uint8_t)data[i];

	/* Avoid per-byte delays unless serial_send_pace_us explicitly requests one. */
	if (serial_send_pace_us == 0)
		sent = ser_send_buf(fd, message, size);
	else
		sent = ser_send_buf_pace(fd,
			(useconds_t)serial_send_pace_us, message, size);

	free(message);

	/* A partial transmission is a communication failure, not a successful send. */
	if (sent < 0 || (size_t)sent != size)
		return -1;

	return (int)sent;
}

#if 0
static char * strtolow(char* s) {
	char	*p;
	for (p = s; *p; p++)
		*p = tolower(*p);
	return s;
}
#endif

static upsinfo getupsinfo(unsigned int upscode) {
	upsinfo	data;
	switch (upscode) {
		case 1:
			data.upscode = 1;
			strncpy(data.upsdesc, "NHS COMPACT PLUS", sizeof(data.upsdesc));
			data.VA = 1000;
			break;

		case 2:
			data.upscode = 2;
			strncpy(data.upsdesc, "NHS COMPACT PLUS SENOIDAL", sizeof(data.upsdesc));
			data.VA = 1000;
			break;

		case 3:
			data.upscode = 3;
			strncpy(data.upsdesc, "NHS COMPACT PLUS RACK", sizeof(data.upsdesc));
			data.VA = 1000;
			break;

		case 4:
			data.upscode = 4;
			strncpy(data.upsdesc, "NHS PREMIUM PDV", sizeof(data.upsdesc));
			data.VA = 1500;
			break;

		case 5:
			data.upscode = 5;
			strncpy(data.upsdesc, "NHS PREMIUM PDV SENOIDAL", sizeof(data.upsdesc));
			data.VA = 1500;
			break;

		case 6:
			data.upscode = 6;
			strncpy(data.upsdesc, "NHS PREMIUM 1500VA", sizeof(data.upsdesc));
			data.VA = 1500;
			break;

		case 7:
			data.upscode = 7;
			strncpy(data.upsdesc, "NHS PREMIUM 2200VA", sizeof(data.upsdesc));
			data.VA = 2200;
			break;

		case 8:
			data.upscode = 8;
			strncpy(data.upsdesc, "NHS PREMIUM SENOIDAL", sizeof(data.upsdesc));
			data.VA = 1500;
			break;

		case 9:
			data.upscode = 9;
			strncpy(data.upsdesc, "NHS LASER 2600VA", sizeof(data.upsdesc));
			data.VA = 2600;
			break;

		case 10:
			data.upscode = 10;
			strncpy(data.upsdesc, "NHS LASER 3300VA", sizeof(data.upsdesc));
			data.VA = 3300;
			break;

		case 11:
			data.upscode = 11;
			strncpy(data.upsdesc, "NHS LASER 2600VA ISOLADOR", sizeof(data.upsdesc));
			data.VA = 2600;
			break;

		case 12:
			data.upscode = 12;
			strncpy(data.upsdesc, "NHS LASER SENOIDAL", sizeof(data.upsdesc));
			data.VA = 2600;
			break;

		case 13:
			data.upscode = 13;
			strncpy(data.upsdesc, "NHS LASER ON-LINE", sizeof(data.upsdesc));
			data.VA = 2600;
			break;

		case 15:
			data.upscode = 15;
			strncpy(data.upsdesc, "NHS COMPACT PLUS 2003", sizeof(data.upsdesc));
			data.VA = 1000;
			break;

		case 16:
			data.upscode = 16;
			strncpy(data.upsdesc, "COMPACT PLUS SENOIDAL 2003", sizeof(data.upsdesc));
			data.VA = 1000;
			break;

		case 17:
			data.upscode = 17;
			strncpy(data.upsdesc, "COMPACT PLUS RACK 2003", sizeof(data.upsdesc));
			data.VA = 1000;
			break;

		case 18:
			data.upscode = 18;
			strncpy(data.upsdesc, "PREMIUM PDV 2003", sizeof(data.upsdesc));
			data.VA = 1500;
			break;

		case 19:
			data.upscode = 19;
			strncpy(data.upsdesc, "PREMIUM PDV SENOIDAL 2003", sizeof(data.upsdesc));
			data.VA = 1500;
			break;

		case 20:
			data.upscode = 20;
			strncpy(data.upsdesc, "PREMIUM 1500VA 2003", sizeof(data.upsdesc));
			data.VA = 1500;
			break;

		case 21:
			data.upscode = 21;
			strncpy(data.upsdesc, "PREMIUM 2200VA 2003", sizeof(data.upsdesc));
			data.VA = 2200;
			break;

		case 22:
			data.upscode = 22;
			strncpy(data.upsdesc, "PREMIUM SENOIDAL 2003", sizeof(data.upsdesc));
			data.VA = 1500;
			break;

		case 23:
			data.upscode = 23;
			strncpy(data.upsdesc, "LASER 2600VA 2003", sizeof(data.upsdesc));
			data.VA = 2600;
			break;

		case 24:
			data.upscode = 24;
			strncpy(data.upsdesc, "LASER 3300VA 2003", sizeof(data.upsdesc));
			data.VA = 3300;
			break;

		case 25:
			data.upscode = 25;
			strncpy(data.upsdesc, "LASER 2600VA ISOLADOR 2003", sizeof(data.upsdesc));
			data.VA = 2600;
			break;

		case 26:
			data.upscode = 26;
			strncpy(data.upsdesc, "LASER SENOIDAL 2003", sizeof(data.upsdesc));
			data.VA = 2600;
			break;

		case 27:
			data.upscode = 27;
			strncpy(data.upsdesc, "PDV ONLINE 2003", sizeof(data.upsdesc));
			data.VA = 1500;
			break;

		case 28:
			data.upscode = 28;
			strncpy(data.upsdesc, "LASER ONLINE 2003", sizeof(data.upsdesc));
			data.VA = 3300;
			break;

		case 29:
			data.upscode = 29;
			strncpy(data.upsdesc, "EXPERT ONLINE 2003", sizeof(data.upsdesc));
			data.VA = 5000;
			break;

		case 30:
			data.upscode = 30;
			strncpy(data.upsdesc, "MINI 2", sizeof(data.upsdesc));
			data.VA = 500;
			break;

		case 31:
			data.upscode = 31;
			strncpy(data.upsdesc, "COMPACT PLUS 2", sizeof(data.upsdesc));
			data.VA = 1000;
			break;

		case 32:
			data.upscode = 32;
			strncpy(data.upsdesc, "LASER ON-LINE", sizeof(data.upsdesc));
			data.VA = 2600;
			break;

		case 33:
			data.upscode = 33;
			strncpy(data.upsdesc, "PDV SENOIDAL 1500VA", sizeof(data.upsdesc));
			data.VA = 1500;
			break;

		case 34:
			data.upscode = 34;
			strncpy(data.upsdesc, "PDV SENOIDAL 1000VA", sizeof(data.upsdesc));
			data.VA = 1000;
			break;

		case 36:
			data.upscode = 36;
			strncpy(data.upsdesc, "LASER ONLINE 3750VA", sizeof(data.upsdesc));
			data.VA = 3750;
			break;

		case 37:
			data.upscode = 37;
			strncpy(data.upsdesc, "LASER ONLINE 5000VA", sizeof(data.upsdesc));
			data.VA = 5000;
			break;

		case 38:
			data.upscode = 38;
			strncpy(data.upsdesc, "PREMIUM SENOIDAL 2000VA", sizeof(data.upsdesc));
			data.VA = 2000;
			break;

		case 39:
			data.upscode = 39;
			strncpy(data.upsdesc, "LASER SENOIDAL 3500VA", sizeof(data.upsdesc));
			data.VA = 3500;
			break;

		case 40:
			data.upscode = 40;
			strncpy(data.upsdesc, "PREMIUM PDV 1200VA", sizeof(data.upsdesc));
			data.VA = 1200;
			break;

		case 41:
			data.upscode = 41;
			strncpy(data.upsdesc, "PREMIUM 1500VA", sizeof(data.upsdesc));
			data.VA = 1500;
			break;

		case 42:
			data.upscode = 42;
			strncpy(data.upsdesc, "PREMIUM 2200VA", sizeof(data.upsdesc));
			data.VA = 2200;
			break;

		case 43:
			data.upscode = 43;
			strncpy(data.upsdesc, "LASER 2600VA", sizeof(data.upsdesc));
			data.VA = 2600;
			break;

		case 44:
			data.upscode = 44;
			strncpy(data.upsdesc, "LASER 3300VA", sizeof(data.upsdesc));
			data.VA = 3300;
			break;

		case 45:
			data.upscode = 45;
			strncpy(data.upsdesc, "COMPACT PLUS SENOIDAL 700VA", sizeof(data.upsdesc));
			data.VA = 700;
			break;

		case 46:
			data.upscode = 46;
			strncpy(data.upsdesc, "PREMIUM ONLINE 2000VA", sizeof(data.upsdesc));
			data.VA = 2000;
			break;

		case 47:
			data.upscode = 47;
			strncpy(data.upsdesc, "EXPERT ONLINE 10000VA", sizeof(data.upsdesc));
			data.VA = 10000;
			break;

		case 48:
			data.upscode = 48;
			strncpy(data.upsdesc, "LASER SENOIDAL 4200VA", sizeof(data.upsdesc));
			data.VA = 4200;
			break;

		case 49:
			data.upscode = 49;
			strncpy(data.upsdesc, "NHS COMPACT PLUS EXTENDIDO 1500VA", sizeof(data.upsdesc));
			data.VA = 1500;
			break;

		case 50:
			data.upscode = 50;
			strncpy(data.upsdesc, "LASER ONLINE 6000VA", sizeof(data.upsdesc));
			data.VA = 6000;
			break;

		case 51:
			data.upscode = 51;
			strncpy(data.upsdesc, "LASER EXT 3300VA", sizeof(data.upsdesc));
			data.VA = 3300;
			break;

		case 52:
			data.upscode = 52;
			strncpy(data.upsdesc, "NHS COMPACT PLUS 1200VA", sizeof(data.upsdesc));
			data.VA = 1200;
			break;

		case 53:
			data.upscode = 53;
			strncpy(data.upsdesc, "LASER SENOIDAL 3000VA GII", sizeof(data.upsdesc));
			data.VA = 3000;
			break;

		case 54:
			data.upscode = 54;
			strncpy(data.upsdesc, "LASER SENOIDAL 3500VA GII", sizeof(data.upsdesc));
			data.VA = 3500;
			break;

		case 55:
			data.upscode = 55;
			strncpy(data.upsdesc, "LASER SENOIDAL 4200VA GII", sizeof(data.upsdesc));
			data.VA = 4200;
			break;

		case 56:
			data.upscode = 56;
			strncpy(data.upsdesc, "LASER ONLINE 3000VA", sizeof(data.upsdesc));
			data.VA = 3000;
			break;

		case 57:
			data.upscode = 57;
			strncpy(data.upsdesc, "LASER ONLINE 3750VA", sizeof(data.upsdesc));
			data.VA = 3750;
			break;

		case 58:
			data.upscode = 58;
			strncpy(data.upsdesc, "LASER ONLINE 5000VA", sizeof(data.upsdesc));
			data.VA = 5000;
			break;

		case 59:
			data.upscode = 59;
			strncpy(data.upsdesc, "LASER ONLINE 6000VA", sizeof(data.upsdesc));
			data.VA = 6000;
			break;

		case 60:
			data.upscode = 60;
			strncpy(data.upsdesc, "PREMIUM ONLINE 2000VA", sizeof(data.upsdesc));
			data.VA = 2000;
			break;

		case 61:
			data.upscode = 61;
			strncpy(data.upsdesc, "PREMIUM ONLINE 1500VA", sizeof(data.upsdesc));
			data.VA = 1500;
			break;

		case 62:
			data.upscode = 62;
			strncpy(data.upsdesc, "PREMIUM ONLINE 1200VA", sizeof(data.upsdesc));
			data.VA = 1200;
			break;

		case 63:
			data.upscode = 63;
			strncpy(data.upsdesc, "COMPACT PLUS II MAX 1400VA", sizeof(data.upsdesc));
			data.VA = 1400;
			break;

		case 64:
			data.upscode = 64;
			strncpy(data.upsdesc, "PREMIUM PDV MAX 2200VA", sizeof(data.upsdesc));
			data.VA = 2200;
			break;

		case 65:
			data.upscode = 65;
			strncpy(data.upsdesc, "PREMIUM PDV 3000VA", sizeof(data.upsdesc));
			data.VA = 3000;
			break;

		case 66:
			data.upscode = 66;
			strncpy(data.upsdesc, "PREMIUM SENOIDAL 2200VA GII", sizeof(data.upsdesc));
			data.VA = 2200;
			break;

		case 67:
			data.upscode = 67;
			strncpy(data.upsdesc, "LASER PRIME SENOIDAL 3200VA GII", sizeof(data.upsdesc));
			data.VA = 3200;
			break;

		case 68:
			data.upscode = 68;
			strncpy(data.upsdesc, "PREMIUM RACK ONLINE 3000VA", sizeof(data.upsdesc));
			data.VA = 3000;
			break;

		case 69:
			data.upscode = 69;
			strncpy(data.upsdesc, "PREMIUM ONLINE 3000VA", sizeof(data.upsdesc));
			data.VA = 3000;
			break;

		case 70:
			data.upscode = 70;
			strncpy(data.upsdesc, "LASER ONLINE 4000VA", sizeof(data.upsdesc));
			data.VA = 4000;
			break;

		case 71:
			data.upscode = 71;
			strncpy(data.upsdesc, "LASER ONLINE 7500VA", sizeof(data.upsdesc));
			data.VA = 7500;
			break;

		case 72:
			data.upscode = 72;
			strncpy(data.upsdesc, "LASER ONLINE BIFASICO 5000VA", sizeof(data.upsdesc));
			data.VA = 5000;
			break;

		case 73:
			data.upscode = 73;
			strncpy(data.upsdesc, "LASER ONLINE BIFASICO 6000VA", sizeof(data.upsdesc));
			data.VA = 6000;
			break;

		case 74:
			data.upscode = 74;
			strncpy(data.upsdesc, "LASER ONLINE BIFASICO 7500VA", sizeof(data.upsdesc));
			data.VA = 7500;
			break;

		case 75:
			data.upscode = 75;
			strncpy(data.upsdesc, "NHS MINI ST", sizeof(data.upsdesc));
			data.VA = 500;
			break;

		case 76:
			data.upscode = 76;
			strncpy(data.upsdesc, "NHS MINI 120", sizeof(data.upsdesc));
			data.VA = 120;
			break;

		case 77:
			data.upscode = 77;
			strncpy(data.upsdesc, "NHS MINI BIVOLT", sizeof(data.upsdesc));
			data.VA = 500;
			break;

		case 78:
			data.upscode = 78;
			strncpy(data.upsdesc, "PDV 600", sizeof(data.upsdesc));
			data.VA = 600;
			break;

		case 79:
			data.upscode = 79;
			strncpy(data.upsdesc, "NHS MINI MAX", sizeof(data.upsdesc));
			data.VA = 500;
			break;

		case 80:
			data.upscode = 80;
			strncpy(data.upsdesc, "NHS MINI EXT", sizeof(data.upsdesc));
			data.VA = 500;
			break;

		case 81:
			data.upscode = 81;
			strncpy(data.upsdesc, "NHS AUTONOMY PDV 4T", sizeof(data.upsdesc));
			data.VA = 4000;
			break;

		case 82:
			data.upscode = 82;
			strncpy(data.upsdesc, "NHS AUTONOMY PDV 8T", sizeof(data.upsdesc));
			data.VA = 8000;
			break;

		case 83:
			data.upscode = 83;
			strncpy(data.upsdesc, "NHS COMPACT PLUS RACK 1200VA", sizeof(data.upsdesc));
			data.VA = 1200;
			break;

		case 84:
			data.upscode = 84;
			strncpy(data.upsdesc, "PDV SENOIDAL ISOLADOR 1500VA", sizeof(data.upsdesc));
			data.VA = 1500;
			break;

		case 85:
			data.upscode = 85;
			strncpy(data.upsdesc, "NHS PDV RACK 1500VA", sizeof(data.upsdesc));
			data.VA = 1500;
			break;

		case 86:
			data.upscode = 86;
			strncpy(data.upsdesc, "NHS PDV 1400VA S GII", sizeof(data.upsdesc));
			data.VA = 1400;
			break;

		case 87:
			data.upscode = 87;
			strncpy(data.upsdesc, "PDV SENOIDAL ISOLADOR 1500VA", sizeof(data.upsdesc));
			data.VA = 1500;
			break;

		case 88:
			data.upscode = 88;
			strncpy(data.upsdesc, "LASER PRIME SENOIDAL ISOLADOR 2000VA", sizeof(data.upsdesc));
			data.VA = 2000;
			break;

		case 89:
			data.upscode = 89;
			strncpy(data.upsdesc, "PREMIUM SENOIDAL 2400VA GII", sizeof(data.upsdesc));
			data.VA = 2400;
			break;

		case 90:
			data.upscode = 90;
			strncpy(data.upsdesc, "NHS PDV 1400VA S 8T GII", sizeof(data.upsdesc));
			data.VA = 1400;
			break;

		case 91:
			data.upscode = 91;
			strncpy(data.upsdesc, "PREMIUM ONLINE 2000VA", sizeof(data.upsdesc));
			data.VA = 2000;
			break;

		case 92:
			data.upscode = 92;
			strncpy(data.upsdesc, "LASER PRIME ONLINE 2200VA", sizeof(data.upsdesc));
			data.VA = 2200;
			break;

		case 93:
			data.upscode = 93;
			strncpy(data.upsdesc, "PREMIUM RACK ONLINE 2200VA", sizeof(data.upsdesc));
			data.VA = 2200;
			break;

		case 94:
			data.upscode = 94;
			strncpy(data.upsdesc, "PREMIUM SENOIDAL 2400VA GII", sizeof(data.upsdesc));
			data.VA = 2400;
			break;

		case 95:
			data.upscode = 95;
			strncpy(data.upsdesc, "LASER ONLINE 10000VA", sizeof(data.upsdesc));
			data.VA = 10000;
			break;

		case 96:
			data.upscode = 96;
			strncpy(data.upsdesc, "LASER ONLINE BIFASICO 10000VA", sizeof(data.upsdesc));
			data.VA = 10000;
			break;

		case 97:
			data.upscode = 97;
			strncpy(data.upsdesc, "LASER SENOIDAL 3300VA GII", sizeof(data.upsdesc));
			data.VA = 3300;
			break;

		case 98:
			data.upscode = 98;
			strncpy(data.upsdesc, "LASER SENOIDAL 2600VA GII", sizeof(data.upsdesc));
			data.VA = 2600;
			break;

		case 99:
			data.upscode = 99;
			strncpy(data.upsdesc, "PREMIUM SENOIDAL 3000VA GII", sizeof(data.upsdesc));
			data.VA = 3000;
			break;

		case 100:
			data.upscode = 100;
			strncpy(data.upsdesc, "PREMIUM SENOIDAL 2200VA GII", sizeof(data.upsdesc));
			data.VA = 2200;
			break;

		case 101:
			data.upscode = 101;
			strncpy(data.upsdesc, "LASER ONLINE BIFASICO 4000VA", sizeof(data.upsdesc));
			data.VA = 4000;
			break;

		case 102:
			data.upscode = 102;
			strncpy(data.upsdesc, "LASER ONLINE 12000VA", sizeof(data.upsdesc));
			data.VA = 12000;
			break;

		case 103:
			data.upscode = 103;
			strncpy(data.upsdesc, "LASER ONLINE 8000VA", sizeof(data.upsdesc));
			data.VA = 8000;
			break;

		case 104:
			data.upscode = 104;
			strncpy(data.upsdesc, "PDV SENOIDAL ISOLADOR 1000VA", sizeof(data.upsdesc));
			data.VA = 1000;
			break;

		case 105:
			data.upscode = 105;
			strncpy(data.upsdesc, "MINI SENOIDAL 500VA", sizeof(data.upsdesc));
			data.VA = 500;
			break;

		case 106:
			data.upscode = 106;
			strncpy(data.upsdesc, "LASER SENOIDAL 5000VA GII", sizeof(data.upsdesc));
			data.VA = 5000;
			break;

		case 107:
			data.upscode = 107;
			strncpy(data.upsdesc, "COMPACT PLUS SENOIDAL 1000VA", sizeof(data.upsdesc));
			data.VA = 1000;
			break;

		case 108:
			data.upscode = 108;
			strncpy(data.upsdesc, "QUAD_COM 80A", sizeof(data.upsdesc));
			data.VA = 0;
			break;

		case 109:
			data.upscode = 109;
			strncpy(data.upsdesc, "LASER ONLINE 5000VA", sizeof(data.upsdesc));
			data.VA = 5000;
			break;

		case 113:
			data.upscode = 113;
			strncpy(data.upsdesc, "PDV SENOIDAL ISOLADOR 700VA", sizeof(data.upsdesc));
			data.VA = 700;
			break;

		default:
			data.upscode = -1;
			strncpy(data.upsdesc, "NHS UNKNOWN", sizeof(data.upsdesc));
			data.VA = 0;
			break;
	}
	return data;
}

static unsigned int get_va(int equipment) {
	upsinfo	ups;
	char	*va = getval("va");

	ups = getupsinfo(equipment);
	if (ups.VA > 0)
		return ups.VA;
	else {
		if (va)
			return atoi(va);
		else
			fatalx(EXIT_FAILURE, "Please set VA (Volt Ampere) nominal capacity value to your equipment in ups.conf.");
	}
}

static float get_pf(void) {
	char	*pf = getval("pf");
	if (pf)
		return atof(pf);
	else
		return DEFAULTPF;
}

static unsigned int get_ah(void) {
	char	*ah = getval("ah");
	if (ah)
		return (unsigned int)atoi(ah);
	else
		fatalx(EXIT_FAILURE, "Please set AH (Ampere Hour) value to your battery's equipment in ups.conf.");
}

static float get_vin_perc(char * var) {
	char	*perc = getval(var);
	if (perc)
		return atof(perc);
	else
		return DEFAULTPERC;
}

void upsdrv_initinfo(void) {
	/* From docs/new-drivers.txt:
	 * Try to detect what kind of UPS is out there,
	 * if any, assuming that's possible for your hardware.
	 * If there is a way to detect that hardware and it
	 * doesn't appear to be connected, display an error
	 * and exit. This is the last time your driver is
	 * allowed to bail out.
	 * This is usually a good place to create variables
	 * like `ups.mfr`, `ups.model`, `ups.serial`, register
	 * instant commands, and other "one time only" items.
	 */

	upsdebugx(3, "%s: starting...", __func__);

	/* TODO: Any instant commands? */

	upsdebugx(3, "%s: finished", __func__);
}

static float calculate_efficiency(float vacoutrms, float vacinrms) {
	return (vacoutrms * vacinrms) / 100.0;
}

static unsigned int get_numbat(void) {
	char	*nb = getval("numbat");
	unsigned int	retval = 0;

	if (nb)
		retval = atoi(nb);
	return retval;
}

/*
 * Return serial_fd after preserving the original bounded retry behavior.
 * Reopened ports pass through openfd, so the same validated settings are
 * restored after a communication failure.
 */
static TYPE_FD_SER reconnect_ups_if_needed(void) {
	/* retries to open port */
	static unsigned int	retries = 0;

	/* If comms failed earlier, try to resuscitate */
	if (INVALID_FD_SER(serial_fd)) {
		upsdebugx(1, "%s: Serial port '%s' communications problem",
			__func__, porta);

		/* Uh oh, got to reconnect! */
		dstate_setinfo("driver.state", "reconnect.trying");

		/* Close any surviving handle and mark it invalid before reopening. */
		close_serial_port();

		while (INVALID_FD_SER(serial_fd)) {
			upsdebugx(1, "%s: Trying to reopen serial...", __func__);
			serial_fd = openfd(porta, baudrate);
			retries++;
			/* Try above at least once per main cycle */
			if (retries >= MAXTRIES)
				break;
			usleep(checktime);
		}

		if (VALID_FD_SER(serial_fd)) {
			if (retries > MAXTRIES) {
				upslogx(LOG_NOTICE, "Communications with UPS re-established");
			}
			retries = 0;
			dstate_setinfo("driver.state", "quiet");
		} else {
			if (retries == MAXTRIES) {
				upslogx(LOG_WARNING, "Communications with UPS lost: port reopen failed!");
			}
			dstate_datastale();
		}
	}

	return serial_fd;
}

static void interpret_pkt_hwinfo(void) {
	/* TOTHINK: Consider passing in the packet struct as parameter? */
	upsinfo	ups;
	char	hw_scratch_buf[1024];	/* General-purpose string buffer */

	if (!lastpktdata.checksum_ok) {
		upslogx(LOG_WARNING, "%s: bad lastpkthwinfo.checksum",
			__func__);
		return;
	}

	if (lastpkthwinfo.size < 1) {
		upslogx(LOG_WARNING, "%s: Pkt HWINFO is not OK. "
			"See if will be requested next time!",
			__func__);
		return;
	}

	/* checksum is OK, then use it to set values */
	upsdebugx(4, "Pkt HWINFO is OK. Model code is %u, hwversion is %u "
		"and swversion is %u",
		lastpkthwinfo.model,
		lastpkthwinfo.hardwareversion,
		lastpkthwinfo.softwareversion);

	/* We need to set data on NUT with data
	 * that I believe that I can calculate.
	 * Now setting data on NUT
	 */
	ups = getupsinfo(lastpkthwinfo.model);
	upsdebugx(4, "UPS Struct data: Code %u Model %s VA %u", ups.upscode, ups.upsdesc, ups.VA);
	dstate_setinfo("device.model", "%s", ups.upsdesc);
	dstate_setinfo("device.mfr", "%s", MANUFACTURER);
	dstate_setinfo("device.serial", "%s", lastpkthwinfo.serial);
	dstate_setinfo("device.type", "%s", "ups");

	dstate_setinfo("ups.model", "%s", ups.upsdesc);
	dstate_setinfo("ups.mfr", "%s", MANUFACTURER);
	dstate_setinfo("ups.serial", "%s", lastpkthwinfo.serial);
	dstate_setinfo("ups.firmware", "%u", lastpkthwinfo.softwareversion);

	/* Setting hardware version here.
	 * Did not find another place to do this.
	 * Feel free to correct it.
	 * FIXME: move to upsdrv_initinfo() or so
	 */
	dstate_setinfo("ups.firmware.aux", "%u", lastpkthwinfo.hardwareversion);

	if (debug_pkt_hwinfo) {
		unsigned int	i = 0;

		/* Now, creating a structure called NHS.HW, for latest HW
		 * info packet contents and raw data points, including those
		 * that were sorted above into NUT standard variables -
		 * for debug.
		 */
		dstate_setinfo("experimental.nhs.hw.header", "%u", lastpkthwinfo.header);
		dstate_setinfo("experimental.nhs.hw.size", "%u", lastpkthwinfo.size);
		dstate_setinfo("experimental.nhs.hw.type", "%c", lastpkthwinfo.type);
		dstate_setinfo("experimental.nhs.hw.model", "%u", lastpkthwinfo.model);
		dstate_setinfo("experimental.nhs.hw.hardwareversion", "%u", lastpkthwinfo.hardwareversion);
		dstate_setinfo("experimental.nhs.hw.softwareversion", "%u", lastpkthwinfo.softwareversion);
		dstate_setinfo("experimental.nhs.hw.configuration", "%u", lastpkthwinfo.configuration);
		for (i = 0; i < 5; i++) {
			/* Reusing variable */
			snprintf(hw_scratch_buf, sizeof(hw_scratch_buf), "experimental.nhs.hw.configuration_array_p%u", i);
			dstate_setinfo(hw_scratch_buf, "%u", lastpkthwinfo.configuration_array[i]);
		}
		dstate_setinfo("experimental.nhs.hw.c_oem_mode", "%s", lastpkthwinfo.c_oem_mode ? "true" : "false");
		dstate_setinfo("experimental.nhs.hw.c_buzzer_disable", "%s", lastpkthwinfo.c_buzzer_disable ? "true" : "false");
		dstate_setinfo("experimental.nhs.hw.c_potmin_disable", "%s", lastpkthwinfo.c_potmin_disable ? "true" : "false");
		dstate_setinfo("experimental.nhs.hw.c_rearm_enable", "%s", lastpkthwinfo.c_rearm_enable ? "true" : "false");
		dstate_setinfo("experimental.nhs.hw.c_bootloader_enable", "%s", lastpkthwinfo.c_bootloader_enable ? "true" : "false");
		dstate_setinfo("experimental.nhs.hw.numbatteries", "%u", lastpkthwinfo.numbatteries);
		dstate_setinfo("experimental.nhs.hw.undervoltagein120V", "%u", lastpkthwinfo.undervoltagein120V);
		dstate_setinfo("experimental.nhs.hw.overvoltagein120V", "%u", lastpkthwinfo.overvoltagein120V);
		dstate_setinfo("experimental.nhs.hw.undervoltagein220V", "%u", lastpkthwinfo.undervoltagein220V);
		dstate_setinfo("experimental.nhs.hw.overvoltagein220V", "%u", lastpkthwinfo.overvoltagein220V);
		dstate_setinfo("experimental.nhs.hw.tensionout120V", "%u", lastpkthwinfo.tensionout120V);
		dstate_setinfo("experimental.nhs.hw.tensionout220V", "%u", lastpkthwinfo.tensionout220V);
		dstate_setinfo("experimental.nhs.hw.statusval", "%u", lastpkthwinfo.statusval);
		for (i = 0; i < 6; i++) {
			/* Reusing variable */
			snprintf(hw_scratch_buf, sizeof(hw_scratch_buf), "experimental.nhs.hw.status_p%u", i);
			dstate_setinfo(hw_scratch_buf, "%u", lastpkthwinfo.status[i]);
		}
		dstate_setinfo("experimental.nhs.hw.s_220V_in", "%s", lastpkthwinfo.s_220V_in ? "true" : "false");
		dstate_setinfo("experimental.nhs.hw.s_220V_out", "%s", lastpkthwinfo.s_220V_out ? "true" : "false");
		dstate_setinfo("experimental.nhs.hw.s_sealed_battery", "%s", lastpkthwinfo.s_sealed_battery ? "true" : "false");
		dstate_setinfo("experimental.nhs.hw.s_show_out_tension", "%s", lastpkthwinfo.s_show_out_tension ? "true" : "false");
		dstate_setinfo("experimental.nhs.hw.s_show_temperature", "%s", lastpkthwinfo.s_show_temperature ? "true" : "false");
		dstate_setinfo("experimental.nhs.hw.s_show_charger_current", "%s", lastpkthwinfo.s_show_charger_current ? "true" : "false");
		dstate_setinfo("experimental.nhs.hw.chargercurrent", "%u", lastpkthwinfo.chargercurrent);
		dstate_setinfo("experimental.nhs.hw.checksum", "%u", lastpkthwinfo.checksum);
		dstate_setinfo("experimental.nhs.hw.checksum_calc", "%u", lastpkthwinfo.checksum_calc);
		dstate_setinfo("experimental.nhs.hw.checksum_ok", "%s", lastpkthwinfo.checksum_ok ? "true" : "false");
		dstate_setinfo("experimental.nhs.hw.serial", "%s", lastpkthwinfo.serial);
		dstate_setinfo("experimental.nhs.hw.year", "%u", lastpkthwinfo.year);
		dstate_setinfo("experimental.nhs.hw.month", "%u", lastpkthwinfo.month);
		dstate_setinfo("experimental.nhs.hw.wday", "%u", lastpkthwinfo.wday);
		dstate_setinfo("experimental.nhs.hw.hour", "%u", lastpkthwinfo.hour);
		dstate_setinfo("experimental.nhs.hw.minute", "%u", lastpkthwinfo.minute);
		dstate_setinfo("experimental.nhs.hw.second", "%u", lastpkthwinfo.second);
		dstate_setinfo("experimental.nhs.hw.alarmyear", "%u", lastpkthwinfo.alarmyear);
		dstate_setinfo("experimental.nhs.hw.alarmmonth", "%u", lastpkthwinfo.alarmmonth);
		dstate_setinfo("experimental.nhs.hw.alarmwday", "%u", lastpkthwinfo.alarmwday);
		dstate_setinfo("experimental.nhs.hw.alarmday", "%u", lastpkthwinfo.alarmday);
		dstate_setinfo("experimental.nhs.hw.alarmhour", "%u", lastpkthwinfo.alarmhour);
		dstate_setinfo("experimental.nhs.hw.alarmminute", "%u", lastpkthwinfo.alarmminute);
		dstate_setinfo("experimental.nhs.hw.alarmsecond", "%u", lastpkthwinfo.alarmsecond);
		dstate_setinfo("experimental.nhs.hw.end_marker", "%u", lastpkthwinfo.end_marker);
	}
}

static void interpret_pkt_data(void) {
	/* TOTHINK: Consider passing in the packet struct as parameter?
	 * Note that certain points from lastpkthwinfo do play a role
	 * in decisions here (so maybe two parameters?)
	 */
	static unsigned int	va = 0;
	static unsigned int	ah = 0;
	static unsigned int	numbat = 0;
	static unsigned int	vbat = 0;
	static int	min_input_power = 0;
	static float	pf = 0;

	int	got_hwinfo = (lastpkthwinfo.checksum_ok && lastpkthwinfo.size > 0);
	char	data_scratch_buf[1024];	/* General-purpose string buffer */
	unsigned int	vin_underv = 0;
	unsigned int	vin_overv = 0;
	unsigned int	perc = 0;
	unsigned int	vin = 0;
	unsigned int	vout = 0;
	unsigned int	autonomy_secs = 0;
	float	vin_low_warn = 0;
	float	vin_low_crit = 0;
	float	vin_high_warn = 0;
	float	vin_high_crit = 0;
	float	vpower = 0;
	long	bcharge = 0;
	float	abat = 0;
	float	actual_current = 0;

	if (!lastpktdata.checksum_ok) {
		upslogx(LOG_WARNING, "%s: bad lastpktdata.checksum", __func__);
		return;
	}

	/* checksum is OK, then use it to set values */
	upsdebugx(4, "%s: Data Packet seems be OK", __func__);

	if (!got_hwinfo) {
		upsdebugx(2, "%s: Pkt HWINFO is not OK. "
			"See if will be requested next time. "
			"Some data points will not be set on this pass!",
			__func__);
		/* Not return, but we would miss some data points */
	}

	/* Setting UPS Status:
	 * OL	  -- On line (mains is present): Code below
	 * OB	  -- On battery (mains is not present) : Code below
	 * LB	  -- Low battery: Code below
	 * HB	  -- High battery: NHS doesn't have any variable with that information. Feel free to discover a way to set it
	 * RB	  -- The battery needs to be replaced: Well, as mentioned, we can write some infos on nobreak fw, on structures like pkt_hwinfo.year, pkt_hwinfo.month, etc. I never found any equipment with these values.
	 * CHRG	-- The battery is charging: Code below
	 * DISCHRG -- The battery is discharging (inverter is providing load power): Code Below
	 * BYPASS  -- UPS bypass circuit is active -- no battery protection is available: It's another PROBLEM, because NHS can work in bypass mode in some models, even if you have sealed batteries on it (without any external battery device). On the moment, i'll won't work with that. Feel free to discover how it work correctly.
	 * CAL	 -- UPS is currently performing runtime calibration (on battery)
	 * OFF	 -- UPS is offline and is not supplying power to the load
	 * OVER	-- UPS is overloaded
	 * TRIM	-- UPS is trimming incoming voltage (called "buck" in some hardware)
	 * BOOST   -- UPS is boosting incoming voltage
	 * FSD	 -- Forced Shutdown (restricted use, see the note below)
	 */

	/* Decision Chain commented below */

	/* First we check if system is on battery or not */
	upsdebugx(4, "Set UPS status as OFF and start checking. s_battery_mode is %d",
		lastpktdata.s_battery_mode);

	if (got_hwinfo) {
		if (lastpkthwinfo.s_220V_in) {
			upsdebugx(4, "I'm on 220v IN!. My undervoltage is %u", lastpkthwinfo.undervoltagein220V);
			min_input_power = lastpkthwinfo.undervoltagein220V;
		}
		else {
			upsdebugx(4, "I'm on 120v IN!. My undervoltage is %u", lastpkthwinfo.undervoltagein120V);
			min_input_power = lastpkthwinfo.undervoltagein120V;
		}
	} else {
		if (!min_input_power) {
			min_input_power = 96;
			upsdebugx(4, "I'm on unknown input!. My undervoltage is default %d", min_input_power);
		}
	}

	/* No ups.status changes above this line */
	status_init();

	if (lastpktdata.s_battery_mode) {
		/* ON BATTERY */
		upsdebugx(4, "UPS is on Battery Mode");
		status_set("OB");
		if (lastpktdata.s_battery_low) {
			/* If battery is LOW, warn user! */
			upsdebugx(4, "UPS is on Battery Mode and in Low Battery State");
			status_set("LB");
		}	/* end if */
	}	/* end if */
	else {
		/* Check if MAINS (power) is not preset.
		 * Well, we can check pkt_data.s_network_failure too... */
		if ((lastpktdata.vacinrms <= min_input_power) || (lastpktdata.s_network_failure)) {
			upsdebugx(4, "UPS has power-in value %0.2f "
				"and min_input_power is %d, "
				"or network is in failure. Network failure is %d",
				lastpktdata.vacinrms,
				min_input_power,
				lastpktdata.s_network_failure);
			status_set("DISCHRG");
		}	/* end if */
		else {
			/* MAINS is present. We need to check some situations.
			 * NHS only charge if have more than min_input_power.
			 * If MAINS is less than or equal to min_input_power,
			 * then the UPS goes to BATTERY
			 */
			if (lastpktdata.vacinrms > min_input_power) {
				upsdebugx(4, "UPS is on MAINS");
				if (lastpktdata.s_charger_on) {
					upsdebugx(4, "UPS Charging...");
					status_set("CHRG");
				}
				else {
					if ((lastpktdata.s_network_failure) || (lastpktdata.s_fast_network_failure)) {
						upsdebugx(4, "UPS is on battery mode because network failure or fast network failure");
						status_set("OB");
					}	/* end if */
					else {
						upsdebugx(4, "All is OK. UPS is on ONLINE!");
						status_set("OL");
					}	/* end else */
				}	/* end else */
			}	/* end if */
			else {
				/* Energy is below limit.
				* Nobreak is probably in battery mode... */
				if (lastpktdata.s_battery_low)
					status_set("LB");
				else {
					/* ...or network failure */
					status_set("OB");
				}	/* end else */
			}	/* end else */
		}	/* end else */
	}	/* end else */

	/* TODO: Report in NUT datapoints (battery.packs etc.),
	 *  Perhaps set in upsdrv_initinfo() and refresh in
	 *  interpret_pkt_hwinfo() if needed, but not here?
	 * NOTE: values are cached in static C vars so as to
	 *  not re-getval on every loop.
	 */
	if (!vbat)
		vbat = get_vbat();
	if (!ah)
		ah = get_ah();
	if (va == 0 && got_hwinfo)
		va = get_va(lastpkthwinfo.model);
	if (!pf)
		pf = get_pf();
	if (!numbat) {
		numbat = get_numbat();
		if (numbat == 0 && got_hwinfo)
			numbat = lastpkthwinfo.numbatteries;
		else
			upsdebugx(4, "Number of batteries is set to %u", numbat);
	}

	/* No ups.alarm changes above this line */
	alarm_init();

	if (lastpktdata.s_battery_low)
		alarm_set("[LOW BATTERY]");
	if (lastpktdata.s_bypass_on)
		alarm_set("[ON BYPASS]");

	if (lastpktdata.s_network_failure)
		alarm_set("[NETWORK FAILURE]");
	if (lastpktdata.s_fast_network_failure)
		alarm_set("[FAST NETWORK FAILURE]");

	if (lastpktdata.s_220_in)
		alarm_set("[220V IN]");
	if (lastpktdata.s_220_out)
		alarm_set("[220V OUT]");

	/* Commit alarm and status information */
	alarm_commit(); /* alarm first */
	status_commit();

	dstate_setinfo("ups.temperature", "%0.2f", lastpktdata.tempmed_real);
	dstate_setinfo("ups.load", "%u", lastpktdata.potrms);
	dstate_setinfo("ups.efficiency", "%0.2f", calculate_efficiency(lastpktdata.vacoutrms, lastpktdata.vacinrms));

	/* We've got the power? */
	if (va > 0 && pf > 0) {
		/* vpower is the power in Watts */
		vpower = ((va * pf) * (lastpktdata.potrms / 100.0));
		/* abat is the battery's consumption in Amperes */
		abat = ((vpower / lastpktdata.vdcmed_real) / numbat);
		if (vpower > maxpower)
			maxpower = vpower;
		if (vpower < minpower)
			minpower = vpower;
		dstate_setinfo("ups.power", "%0.2f", vpower);
		dstate_setinfo("ups.realpower", "%ld", lrint(round(vpower)));

		/* FIXME: Move nominal settings to upsdrv_initinfo()
		 *  or at worst to interpret_pkt_hwinfo() */
		dstate_setinfo("ups.power.nominal", "%u", va);
		dstate_setinfo("ups.realpower.nominal", "%ld", lrint(round((double)va * (double)pf)));

		dstate_setinfo("output.realpower", "%ld", lrint(round(va * (lastpktdata.potrms / 100.0))));
		dstate_setinfo("output.power", "%0.2f", vpower);
		dstate_setinfo("output.power.maximum", "%0.2f", maxpower);
		dstate_setinfo("output.power.minimum", "%0.2f", minpower);
		dstate_setinfo("output.power.percent", "%u", lastpktdata.potrms);

		if (lastpktdata.potrms > maxpowerperc)
			maxpowerperc = lastpktdata.potrms;
		if (lastpktdata.potrms < minpowerperc)
			minpowerperc = lastpktdata.potrms;
		dstate_setinfo("output.power.maximum.percent", "%u", maxpowerperc);
		dstate_setinfo("output.power.minimum.percent", "%u", minpowerperc);
	}

	dstate_setinfo("output.voltage", "%0.2f", lastpktdata.vacoutrms);
	dstate_setinfo("input.voltage", "%0.2f", lastpktdata.vacinrms);
	dstate_setinfo("input.voltage.maximum", "%0.2f", lastpktdata.vacinrmsmin);
	dstate_setinfo("input.voltage.minimum", "%0.2f", lastpktdata.vacinrmsmax);

	if (got_hwinfo) {
		dstate_setinfo("ups.beeper.status", "%d", !lastpkthwinfo.c_buzzer_disable);

		vin_underv = lastpkthwinfo.s_220V_in ? lastpkthwinfo.undervoltagein220V : lastpkthwinfo.undervoltagein120V;
		vin_overv = lastpkthwinfo.s_220V_in ? lastpkthwinfo.overvoltagein220V : lastpkthwinfo.overvoltagein120V;
		perc = f_equal(get_vin_perc("vin_low_warn_perc"), get_vin_perc("vin_low_crit_perc")) ?  2 : 1;
		vin_low_warn = vin_underv + (vin_underv * ((get_vin_perc("vin_low_warn_perc") * perc) / 100.0));
		dstate_setinfo("input.voltage.low.warning", "%0.2f", vin_low_warn);
		vin_low_crit = vin_underv + (vin_underv * (get_vin_perc("vin_low_crit_perc") / 100.0));
		dstate_setinfo("input.voltage.low.critical", "%0.2f", vin_low_crit);
		vin_high_warn = vin_overv + (vin_overv * ((get_vin_perc("vin_high_warn_perc") * perc) / 100.0));
		dstate_setinfo("input.voltage.high.warning", "%0.2f", vin_high_warn);
		vin_high_crit = vin_overv + (vin_overv * (get_vin_perc("vin_high_crit_perc") / 100.0));
		dstate_setinfo("input.voltage.high.critical", "%0.2f", vin_high_crit);

		dstate_setinfo("input.transfer.low", "%u", lastpkthwinfo.s_220V_in ? lastpkthwinfo.undervoltagein220V : lastpkthwinfo.undervoltagein120V);
		dstate_setinfo("input.transfer.high", "%u", lastpkthwinfo.s_220V_in ? lastpkthwinfo.overvoltagein220V : lastpkthwinfo.overvoltagein120V);

		/* FIXME: Move nominal settings to upsdrv_initinfo()
		 *  or at worst to interpret_pkt_hwinfo() */
		vin = lastpkthwinfo.s_220V_in ? lastpkthwinfo.tensionout220V : lastpkthwinfo.tensionout120V;
		dstate_setinfo("input.voltage.nominal", "%u", vin);
		vout = lastpkthwinfo.s_220V_out ? lastpkthwinfo.tensionout220V : lastpkthwinfo.tensionout120V;
		dstate_setinfo("output.voltage.nominal", "%u", vout);
	}

	/* Battery electric info */
	bcharge = lrint(round((lastpktdata.vdcmed_real * 100) / vbat));
	if (bcharge > 100)
		bcharge = 100;
	dstate_setinfo("battery.charge", "%ld", bcharge);
	dstate_setinfo("battery.voltage", "%0.2f", lastpktdata.vdcmed_real);
	dstate_setinfo("battery.current", "%0.2f", abat);
	dstate_setinfo("battery.current.total", "%0.2f", (float)abat * numbat);
	dstate_setinfo("battery.temperature", "%ld", lrint(round(lastpktdata.tempmed_real)));

	/* FIXME: Move nominal and other static settings to upsdrv_initinfo()
	 *  or at worst to interpret_pkt_hwinfo() */
	dstate_setinfo("battery.packs", "%u", numbat);
	dstate_setinfo("battery.voltage.nominal", "%u", vbat);
	dstate_setinfo("battery.capacity", "%u", ah);
	dstate_setinfo("battery.capacity.nominal", "%0.2f", (float)ah * pf);
	dstate_setinfo("battery.runtime.low", "%d", 30);

	if (vpower > 0) {
		/* We will calculate autonomy in seconds
		 *   autonomy_secs = (ah / lastpktdata.vdcmed_real) * 3600;
		 * Maybe wrong, too.
		 * People say that the correct calculation is
		 *
		 *   Battery Amp-Hour / (Power in Watts / battery voltage)
		 *
		 * Is that correct? I don't know. I'll use it for now.
		 */

		/* That result is IN HOURS. We need to convert it to seconds */
		actual_current = vpower / vbat;	/* Current consumption in A*/
		autonomy_secs = (ah / actual_current) * 3600;

		dstate_setinfo("battery.runtime", "%u", autonomy_secs);
	}

	/* Battery charger status */
	if (lastpktdata.s_charger_on) {
		dstate_setinfo("battery.charger.status", "%s", "CHARGING");
	} else {
		if (lastpktdata.s_battery_mode)
			dstate_setinfo("battery.charger.status", "%s", "DISCHARGING");
		else
			dstate_setinfo("battery.charger.status", "%s", "RESTING");
	}

	if (debug_pkt_data) {
		unsigned int	i = 0;

		/* Now, creating a structure called NHS.DATA, for latest
		 * data packet contents and raw data points, including those
		 * that were sorted above into NUT standard variables -
		 * for debug.
		 */
		dstate_setinfo("experimental.nhs.data.header", "%u", lastpktdata.header);
		dstate_setinfo("experimental.nhs.data.length", "%u", lastpktdata.length);
		dstate_setinfo("experimental.nhs.data.packet_type", "%c", lastpktdata.packet_type);
		dstate_setinfo("experimental.nhs.data.vacinrms_high", "%u", lastpktdata.vacinrms_high);
		dstate_setinfo("experimental.nhs.data.vacinrms_low", "%u", lastpktdata.vacinrms_low);
		dstate_setinfo("experimental.nhs.data.vacinrms", "%0.2f", lastpktdata.vacinrms);
		dstate_setinfo("experimental.nhs.data.vdcmed_high", "%u", lastpktdata.vdcmed_high);
		dstate_setinfo("experimental.nhs.data.vdcmed_low", "%u", lastpktdata.vdcmed_low);
		dstate_setinfo("experimental.nhs.data.vdcmed", "%0.2f", lastpktdata.vdcmed);
		dstate_setinfo("experimental.nhs.data.vdcmed_real", "%0.2f", lastpktdata.vdcmed_real);
		dstate_setinfo("experimental.nhs.data.potrms", "%u", lastpktdata.potrms);
		dstate_setinfo("experimental.nhs.data.vacinrmsmin_high", "%u", lastpktdata.vacinrmsmin_high);
		dstate_setinfo("experimental.nhs.data.vacinrmsmin_low", "%u", lastpktdata.vacinrmsmin_low);
		dstate_setinfo("experimental.nhs.data.vacinrmsmin", "%0.2f", lastpktdata.vacinrmsmin);
		dstate_setinfo("experimental.nhs.data.vacinrmsmax_high", "%u", lastpktdata.vacinrmsmax_high);
		dstate_setinfo("experimental.nhs.data.vacinrmsmax_low", "%u", lastpktdata.vacinrmsmax_low);
		dstate_setinfo("experimental.nhs.data.vacinrmsmax", "%0.2f", lastpktdata.vacinrmsmax);
		dstate_setinfo("experimental.nhs.data.vacoutrms_high", "%u", lastpktdata.vacoutrms_high);
		dstate_setinfo("experimental.nhs.data.vacoutrms_low", "%u", lastpktdata.vacoutrms_low);
		dstate_setinfo("experimental.nhs.data.vacoutrms", "%0.2f", lastpktdata.vacoutrms);
		dstate_setinfo("experimental.nhs.data.tempmed_high", "%u", lastpktdata.tempmed_high);
		dstate_setinfo("experimental.nhs.data.tempmed_low", "%u", lastpktdata.tempmed_low);
		dstate_setinfo("experimental.nhs.data.tempmed", "%0.2f", lastpktdata.tempmed);
		dstate_setinfo("experimental.nhs.data.tempmed_real", "%0.2f", lastpktdata.tempmed_real);
		dstate_setinfo("experimental.nhs.data.icarregrms", "%u", lastpktdata.icarregrms);
		dstate_setinfo("experimental.nhs.data.icarregrms_real", "%u", lastpktdata.icarregrms_real);
		dstate_setinfo("experimental.nhs.data.battery_tension", "%0.2f", lastpktdata.battery_tension);
		dstate_setinfo("experimental.nhs.data.perc_output", "%u", lastpktdata.perc_output);
		dstate_setinfo("experimental.nhs.data.statusval", "%u", lastpktdata.statusval);
		for (i = 0; i < 8; i++) {
			/* Reusing variable */
			snprintf(data_scratch_buf, sizeof(data_scratch_buf), "experimental.nhs.data.status_p%u", i);
			dstate_setinfo(data_scratch_buf, "%u", lastpktdata.status[i]);
		}
		dstate_setinfo("experimental.nhs.data.nominaltension", "%u", lastpktdata.nominaltension);
		dstate_setinfo("experimental.nhs.data.timeremain", "%0.2f", lastpktdata.timeremain);
		dstate_setinfo("experimental.nhs.data.s_battery_mode", "%s", lastpktdata.s_battery_mode ? "true" : "false");
		dstate_setinfo("experimental.nhs.data.s_battery_low", "%s", lastpktdata.s_battery_low ? "true" : "false");
		dstate_setinfo("experimental.nhs.data.s_network_failure", "%s", lastpktdata.s_network_failure ? "true" : "false");
		dstate_setinfo("experimental.nhs.data.s_fast_network_failure", "%s", lastpktdata.s_fast_network_failure ? "true" : "false");
		dstate_setinfo("experimental.nhs.data.s_220_in", "%s", lastpktdata.s_220_in ? "true" : "false");
		dstate_setinfo("experimental.nhs.data.s_220_out", "%s", lastpktdata.s_220_out ? "true" : "false");
		dstate_setinfo("experimental.nhs.data.s_bypass_on", "%s", lastpktdata.s_bypass_on ? "true" : "false");
		dstate_setinfo("experimental.nhs.data.s_charger_on", "%s", lastpktdata.s_charger_on ? "true" : "false");
		dstate_setinfo("experimental.nhs.data.checksum", "%u", lastpktdata.checksum);
		dstate_setinfo("experimental.nhs.data.checksum_ok", "%s", lastpktdata.checksum_ok ? "true" : "false");
		dstate_setinfo("experimental.nhs.data.checksum_calc", "%u", lastpktdata.checksum_calc);
		dstate_setinfo("experimental.nhs.data.end_marker", "%u", lastpktdata.end_marker);
		dstate_setinfo("experimental.nhs.param.va", "%u", va);
		dstate_setinfo("experimental.nhs.param.pf", "%0.2f", pf);
		dstate_setinfo("experimental.nhs.param.ah", "%u", ah);
		dstate_setinfo("experimental.nhs.param.vin_low_warn_perc", "%0.2f", get_vin_perc("vin_low_warn_perc"));
		dstate_setinfo("experimental.nhs.param.vin_low_crit_perc", "%0.2f", get_vin_perc("vin_low_crit_perc"));
		dstate_setinfo("experimental.nhs.param.vin_high_warn_perc", "%0.2f", get_vin_perc("vin_high_warn_perc"));
		dstate_setinfo("experimental.nhs.param.vin_high_crit_perc", "%0.2f", get_vin_perc("vin_high_crit_perc"));
	}
}

void upsdrv_updateinfo(void) {
	double	tempodecorrido = 0.0;
	time_t	now;

	upsdebugx(3, "%s: starting...", __func__);

	/* If comms failed earlier, try to resuscitate */
	if (INVALID_FD_SER(reconnect_ups_if_needed()))
		return;

	chr = '\0';
	{
		ssize_t	read_result;

		/* ser_get_char accepts separate seconds and microseconds components. */
		time_t		timeout_sec = (time_t)(serial_read_timeout_ms / 1000);
		useconds_t	timeout_usec =
			(useconds_t)((serial_read_timeout_ms % 1000) * 1000);

		/*
		 * A positive result supplies one byte, zero is the configured normal
		 * timeout, and a negative result is handled as a communication error.
		 */
		while ((read_result = ser_get_char(
			serial_fd,
			&chr,
			timeout_sec,
			timeout_usec
		)) > 0) {
			if (chr == 0xFF) {	/* DataPacket start */
				datapacketstart = true;
				memset(datapacket, 0, sizeof(datapacket));
				datapacket_index = 0;
			}
			if (datapacketstart) {
				datapacket[datapacket_index] = chr;
				datapacket_index++;
				if (chr == 0xFE) {	/* DataPacket */
					break;
				}
				if (datapacket_index >= sizeof(datapacket)) {
					upslogx(LOG_WARNING, "Incoming packet does not seem to end, discarding!");
					datapacketstart = false;
					break;
				}
			}
		}

		if (read_result < 0) {
			upsdebug_with_errno(1, "%s: Serial read failed on %s",
				__func__, porta);
			close_serial_port();
			dstate_datastale();
			return;
		}
	}

	if (chr != 0xFE || !datapacketstart) {
		upsdebugx(2, "%s: packet reading did not finish, not interpreting yet", __func__);
		return;
	}

	/* Interpret the just-finished packet buffer */
	now = time(NULL);
	upsdebugx(4, "DATAPACKET INDEX IS %" PRIuSIZE, datapacket_index);
	if (lastdp != 0) {
		tempodecorrido = difftime(now, lastdp);
	}
	lastdp = now;

	/* Parse the bytes into a structure to handle below:
	 * If size is 18 or 50, may be an answer packet.
	 * Then check if doesn't have already a packet processed.
	 * We don't need to read all times these information.
	 * Can be a corrupted packet too.
	 */
	if (((datapacket_index == 18) || (datapacket_index == 50)) && (!lastpkthwinfo.checksum_ok)) {
		/* Re-read HW info only if the old one is broken */
		lastpkthwinfo = mount_hwinfo(datapacket, datapacket_index);
		if (lastpkthwinfo.checksum_ok) {
			interpret_pkt_hwinfo();
			/* Refresh the healthy timer */
			dstate_dataok();
		}
	}	/* end if */
	else if (datapacket_index == 21) {
		lastpktdata = mount_datapacket(datapacket, datapacket_index, tempodecorrido, lastpkthwinfo);
		if (lastpktdata.checksum_ok) {
			interpret_pkt_data();
			/* Refresh the healthy timer */
			dstate_dataok();
		}
	}	/* end else-if */
	else {
		upslogx(LOG_WARNING, "Incoming packet size not recognized, discarding!");
	}	/* end else */

	/* Clean datapacket structure to avoid problems for next parse */
	datapacket_index = 0;
	memset(datapacket, 0, sizeof(datapacket));
	datapacketstart = false;

	/* Now the nobreak read buffer is empty.
	 * We need a hw info packet to discover several variables,
	 * like number of batteries, to calculate some data
	 * FIXME: move (semi)static info discovery to upsdrv_initinfo() or so
	 */
	if (!lastpkthwinfo.checksum_ok) {
		upsdebugx(4, "pkt_hwinfo loss -- Requesting");
		/* If size == 0, packet maybe not initizated,
		 * then send an initialization packet to obtain data.
		 * Send two times the extended initialization string,
		 * but, on fail, try randomly send extended or normal.
		 */
		if (send_extended < 6) {
			upsdebugx(4, "Sending extended initialization packet. Try %u", send_extended+1);
			bwritten = write_serial_int(serial_fd, string_initialization_long, 9);
			send_extended++;
		}	/* end if */
		else {
			/* randomly send */
			if (rand() % 2 == 0) {
				upsdebugx(4, "Sending long initialization packet");
				bwritten = write_serial_int(serial_fd, string_initialization_long, 9);
			}	/* end if */
			else {
				upsdebugx(4, "Sending short initialization packet");
				bwritten = write_serial_int(serial_fd, string_initialization_short, 9);
			}	/* end else */
		}	/* end else */
		if (bwritten < 0) {
			upsdebugx(1, "%s: Problem to write data to %s", __func__, porta);
			if (bwritten == -1) {
				upsdebugx(1, "%s: Data problem", __func__);
			}
			close_serial_port();
		}	/* end if */
		else {
			if (checktime > max_checktime)
				checktime = max_checktime;
			else {
				upsdebugx(3, "Increase checktime to %u", checktime + 100000);
				checktime = checktime + 100000;
			}
			usleep(checktime);
		}	/* end else */
	}	/* end if lastpkthwinfo good/bad checksum */

	upsdebugx(3, "%s: finished", __func__);
}

void upsdrv_shutdown(void) {
	upsdebugx(3, "%s: starting...", __func__);

	/* replace with a proper shutdown function */
	upslogx(LOG_ERR, "shutdown not supported");
	set_exit_flag(-1);

	upsdebugx(1, "Driver shutdown");
}

void upsdrv_cleanup(void) {
	upsdebugx(3, "%s: starting...", __func__);

	close_serial_port();

	upsdebugx(3, "%s: finished", __func__);
}

void upsdrv_initups(void) {
	/* From docs/new-drivers.txt:
	 * Open the port (`device_path`) and do any low-level
	 * things that it may need to start using that port.
	 * If you have to set DTR or RTS on a serial port,
	 * do it here.
	 * Don't do any sort of hardware detection here, since
	 * you may be quickly going into upsdrv_shutdown next.
	 */

	upsdebugx(3, "%s: starting...", __func__);

	/* Process optional configuration flags that may
	 * impact HW init methods (debug them or not)
	 */
	if (getval("debug_pkt_raw"))
		debug_pkt_raw = 1;
	if (getval("debug_pkt_data"))
		debug_pkt_data = 1;
	if (getval("debug_pkt_hwinfo"))
		debug_pkt_hwinfo = 1;

	/* Validate the complete serial configuration before accessing the device. */
	parse_serial_options();

	upsdebugx(1, "%s: Port is %s and baud_rate is %d",
		__func__, device_path, baudrate);

	if (device_path) {
		if (strcasecmp(device_path, "auto") == 0)
			strncpy(porta, DEFAULTPORT, sizeof(porta) - 1);
		else
			strncpy(porta, device_path, sizeof(porta) - 1);
		serial_fd = openfd(porta, baudrate);
		if (INVALID_FD_SER(serial_fd))
			fatalx(EXIT_FAILURE, "Unable to open port %s with baud %d",
				porta, baudrate);
		else {
			upsdebugx(1, "%s: Communication started on port %s, baud rate %d",
				__func__, porta, baudrate);
		}
	}
	else
		fatalx(EXIT_FAILURE, "Unable to define port and baud");

	/* If we got here, the port is opened with desired baud rate.
	 * If not shutting down ASAP, soon we will call upsdrv_initinfo()
	 * and "infinitely" loop calling upsdrv_updateinfo() afterwards.
	 */
	upsdebugx(3, "%s: finished", __func__);
}

void upsdrv_makevartable(void) {
	char	help[4096];

	/* Standard variable in main.c */
	/* //addvar(VAR_VALUE, "port", "Port to communication"); */

	/* Expose only the seven conventional serial settings supported above. */
	addvar(VAR_VALUE, "baud", "Serial baud rate from the rates compiled into this driver (default: 2400)");

	addvar(VAR_VALUE, "serial_data_bits", "Serial data bits: 5, 6, 7 or 8 (default: 8)");

	addvar(VAR_VALUE, "serial_parity", "Serial parity: none, even or odd (default: none)");

	addvar(VAR_VALUE, "serial_stop_bits", "Serial stop bits: 1 or 2 (default: 1)");

	addvar(VAR_VALUE, "serial_flow_control", "Serial flow control: none, hardware, software or both (default: hardware)");

	addvar(VAR_VALUE, "serial_read_timeout_ms", "Timeout per serial byte: 0 to 60000 ms (default: 100)");

	addvar(VAR_VALUE, "serial_send_pace_us", "Delay between transmitted bytes: 0 to 999999 us (default: 0)");

	addvar(VAR_VALUE, "ah", "Battery discharge capacity in Ampere/hour");

	addvar(VAR_VALUE, "va", "Nobreak NOMINAL POWER in VA");

	snprintf(help, sizeof(help), "Power Factor to use in calculations of battery time. Default is %0.2f", DEFAULTPF);
	addvar(VAR_VALUE, "pf", help);

	snprintf(help, sizeof(help), "Voltage In Percentage to calculate warning low level. Default is %0.2f", DEFAULTPERC);
	addvar(VAR_VALUE, "vin_low_warn_perc", help);

	snprintf(help, sizeof(help), "Voltage In Percentage to calculate critical low level. Default is %0.2f", DEFAULTPERC);
	addvar(VAR_VALUE, "vin_low_crit_perc", help);

	snprintf(help, sizeof(help), "Voltage In Percentage to calculate warning high level. Default is %0.2f", DEFAULTPERC);
	addvar(VAR_VALUE, "vin_high_warn_perc", help);

	snprintf(help, sizeof(help), "Voltage In Percentage to calculate critical high level. Default is %0.2f", DEFAULTPERC);
	addvar(VAR_VALUE, "vin_high_crit_perc", help);

	snprintf(help, sizeof(help), "Num Batteries (override value from nobreak)");
	addvar(VAR_VALUE, "numbatteries", help);

	snprintf(help, sizeof(help), "Battery Voltage (override default value). Default is %0.2f", DEFAULTBATV);
	addvar(VAR_VALUE, "vbat", help);

	addvar(VAR_FLAG, "debug_pkt_raw", "Enable debug logging of packet bytes");

	addvar(VAR_FLAG, "debug_pkt_data", "Enable debug logging of data packet decoding");

	addvar(VAR_FLAG, "debug_pkt_hwinfo", "Enable debug logging of hwinfo packet decoding");
}

void upsdrv_help(void) {
}

/* optionally tweak prognames[] entries */
void upsdrv_tweak_prognames(void)
{
}
