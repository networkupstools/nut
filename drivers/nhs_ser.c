/*
 * nhs_ser.c - NUT support for NHS Nobreaks, senoidal line
 *
 *
 * Copyright (C) 2024   Lucas Willian Bocchi <lucas@lucas.inf.br>
 *     Initial Release (as nhs-nut.c)
 * Copyright (C) 2024 - 2026 Jim Klimov <jimklimov+nut@gmail.com>
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
	/* Protocol generation associated with the model; currently informational. */
	unsigned int	pversion;
	/* Set to 1 to publish the received bypass bit as a NUT alarm. */
	unsigned int	bypassasalarm;
} upsinfo;

static const unsigned int	string_initialization_long[9] = {0xFF, 0x09, 0x53, 0x83, 0x00, 0x00, 0x00, 0xDF, 0xFE};
static const unsigned int	string_initialization_short[9] = {0xFF, 0x09, 0x53, 0x03, 0x00, 0x00, 0x00, 0x5F, 0xFE};
static const unsigned int	string_initialization_comptmode[5] = {0xFF, 0x05, 0x01, 0x06, 0xFE};

static int		debug_pkt_data = 0, debug_pkt_hwinfo = 0, debug_pkt_raw = 0;

static TYPE_FD_SER	serial_fd = ERROR_FD_SER;
static unsigned char	chr;
static size_t		datapacket_index = 0;
static unsigned int	datapacketsize = 0;
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
static TYPE_FD_SER reconnect_ups_if_needed(void);
#if 0
static int write_serial(int fd, const char * dados, int size);
#endif
static int write_serial_int(TYPE_FD_SER fd, const unsigned int *data, size_t size);
static bool send_initialization_packet(const unsigned int *data, size_t size, TYPE_FD_SER fd);

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

	if (bit_position >= size * 8)
		return -3;	/* Invalid Position */

	if (invertorder == 0)
		retval = (byte_ptr[byte_index] >> (7 - bit_index)) & 1 ? 1 : 0;
	else
		retval = (byte_ptr[byte_index] >> (7 - bit_index)) & 1 ? 0 : 1;
	return retval;
}

static void print_pkt_hwinfo(pkt_hwinfo data) {
	int	i = 0, retorno;

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
		retorno = get_bit_in_position(&data.configuration, sizeof(data.configuration), i, 0);
		upsdebugx(5, "Binary value is %d", retorno);
		upsdebugx(5, "%u ", data.configuration_array[i]);
	}	/* end for */
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
	}	/* end for */
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
	}	/* end for */
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
	snprintf(serial_flow_control, sizeof(serial_flow_control), "%s", DEFAULT_SERIAL_FLOW_CONTROL);
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
		if (errno != 0 || endptr == value || *endptr != '\0' || number <= 0)
			fatalx(EXIT_FAILURE, "Invalid baud value '%s': expected a supported baud rate", value);

		supported = 0;
		for (i = 0; i < NUM_BAUD_RATES && !supported; i++)
			if (number == baud_rates[i].speed)
				supported = 1;
		if (!supported)
			fatalx(EXIT_FAILURE, "Invalid baud value '%s': rate is not available in this build", value);
		baudrate = (int)number;
	}	/* end if */

	value = getval("serial_data_bits");
	if (value) {
		/* Reject partial numbers and anything outside the termios CS5..CS8 set. */
		errno = 0;
		endptr = NULL;
		number = strtol(value, &endptr, 10);
		if (errno != 0 || endptr == value || *endptr != '\0' || (number != 5 && number != 6 && number != 7 && number != 8))
			fatalx(EXIT_FAILURE, "Invalid serial_data_bits value '%s': expected 5, 6, 7 or 8", value);
		serial_data_bits = (unsigned int)number;
	}	/* end if */

	value = getval("serial_parity");
	if (value) {
		if (strcasecmp(value, "none") != 0 && strcasecmp(value, "even") != 0 && strcasecmp(value, "odd") != 0)
			fatalx(EXIT_FAILURE, "Invalid serial_parity value '%s': expected none, even or odd", value);
		snprintf(serial_parity, sizeof(serial_parity), "%s", value);
	}	/* end if */

	value = getval("serial_stop_bits");
	if (value) {
		errno = 0;
		endptr = NULL;
		number = strtol(value, &endptr, 10);
		if (errno != 0 || endptr == value || *endptr != '\0' || (number != 1 && number != 2))
			fatalx(EXIT_FAILURE, "Invalid serial_stop_bits value '%s': expected 1 or 2", value);
		serial_stop_bits = (unsigned int)number;
	}	/* end if */

	value = getval("serial_flow_control");
	if (value) {
		if (strcasecmp(value, "none") != 0 && strcasecmp(value, "hardware") != 0 && strcasecmp(value, "software") != 0 && strcasecmp(value, "both") != 0)
			fatalx(EXIT_FAILURE, "Invalid serial_flow_control value '%s': expected none, hardware, software or both", value);
		snprintf(serial_flow_control, sizeof(serial_flow_control), "%s", value);
	}	/* end if */

	value = getval("serial_read_timeout_ms");
	if (value) {
		/* A zero timeout requests a non-blocking poll from ser_get_char. */
		errno = 0;
		endptr = NULL;
		number = strtol(value, &endptr, 10);
		if (errno != 0 || endptr == value || *endptr != '\0' || number < 0 || number > MAX_SERIAL_READ_TIMEOUT_MS)
			fatalx(EXIT_FAILURE, "Invalid serial_read_timeout_ms value '%s': expected 0 to %d", value, MAX_SERIAL_READ_TIMEOUT_MS);
		serial_read_timeout_ms = (unsigned int)number;
	}	/* end if */

	value = getval("serial_send_pace_us");
	if (value) {
		/* Zero selects an unpaced send; positive values delay each byte. */
		errno = 0;
		endptr = NULL;
		number = strtol(value, &endptr, 10);
		if (errno != 0 || endptr == value || *endptr != '\0' || number < 0 || number > MAX_SERIAL_SEND_PACE_US)
			fatalx(EXIT_FAILURE, "Invalid serial_send_pace_us value '%s': expected 0 to %d", value, MAX_SERIAL_SEND_PACE_US);
		serial_send_pace_us = (unsigned int)number;
	}	/* end if */
}

static void close_serial_port(void)
{
	/* Use NUT's portable descriptor checks and always invalidate after closing. */
	if (VALID_FD_SER(serial_fd)) {
		if (ser_close(serial_fd, porta) != 0)
			upsdebug_with_errno(1, "%s: Error closing serial port %s", __func__, porta);
		serial_fd = ERROR_FD_SER;
	}	/* end if */
}

static TYPE_FD_SER openfd(const char *portarg, int requested_baudrate)
{
	TYPE_FD_SER	fd;
	struct termios	tty;
	speed_t		rate = 0;
	size_t		i;

	/* Translate the numeric ups.conf value to the matching termios constant. */
	for (i = 0; i < NUM_BAUD_RATES && rate == 0; i++) {
		if (baud_rates[i].speed == requested_baudrate) {
			rate = baud_rates[i].rate;
			upsdebugx(1, "%s: Selected baud rate %d -- %s", __func__, baud_rates[i].speed, baud_rates[i].description);
		}	/* end if */
	}	/* end for */

	if (rate == 0) {
		upslogx(LOG_ERR, "%s: Unsupported baud rate %d", __func__, requested_baudrate);
		return ERROR_FD_SER;
	}	/* end if */

	/*
	 * Establish NUT's standard raw serial baseline first, then customize only
	 * data bits, parity, stop bits and flow control below.
	 */
	fd = ser_open_nf(portarg);
	if (INVALID_FD_SER(fd)) {
		upsdebug_with_errno(1, "%s: Error opening %s", __func__, portarg);
		return ERROR_FD_SER;
	}	/* end if */

	if (ser_set_speed_nf(fd, portarg, rate) != 0) {
		upsdebug_with_errno(1, "%s: Error setting baud rate on %s", __func__, portarg);
		ser_close(fd, portarg);
		return ERROR_FD_SER;
	}	/* end if */

	if (tcgetattr(fd, &tty) != 0) {
		upsdebug_with_errno(1, "%s: Error reading serial settings from %s", __func__, portarg);
		ser_close(fd, portarg);
		return ERROR_FD_SER;
	}	/* end if */

	/* Replace the baseline character size with the validated selection. */
	tty.c_cflag &= ~CSIZE;
	switch (serial_data_bits) {
		case 5:
			tty.c_cflag |= CS5;
			break;
		case 6:
			tty.c_cflag |= CS6;
			break;
		case 7:
			tty.c_cflag |= CS7;
			break;
		default:
			tty.c_cflag |= CS8;
			break;
	}	/* end switch */

	/*
	 * Configure parity as a complete unit. Parity errors are ignored for
	 * "none", but checked for the even and odd modes.
	 */
	tty.c_cflag &= ~(PARENB | PARODD);
	tty.c_iflag &= ~INPCK;
	if (strcasecmp(serial_parity, "none") == 0)
		tty.c_iflag |= IGNPAR;
	else {
		if (strcasecmp(serial_parity, "even") == 0) {
			tty.c_iflag &= ~IGNPAR;
			tty.c_cflag |= PARENB;
			tty.c_iflag |= INPCK;
		}	/* end if */
		else {
			tty.c_iflag &= ~IGNPAR;
			tty.c_cflag |= PARENB | PARODD;
			tty.c_iflag |= INPCK;
		}	/* end else */
	}	/* end else */

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
	}	/* end if */
	else {
		if (strcasecmp(serial_flow_control, "hardware") == 0)
			tty.c_cflag |= CRTSCTS;
		else {
			if (strcasecmp(serial_flow_control, "software") == 0)
				tty.c_iflag |= IXON | IXOFF;
			else {
				if (strcasecmp(serial_flow_control, "both") == 0) {
					tty.c_cflag |= CRTSCTS;
					tty.c_iflag |= IXON | IXOFF;
				}	/* end if */
			}	/* end else */
		}	/* end else */
	}	/* end else */

	/* Apply the four conventional settings together. */
	if (tcsetattr(fd, TCSANOW, &tty) != 0) {
		upsdebug_with_errno(1, "%s: Error applying serial settings to %s", __func__, portarg);
		ser_close(fd, portarg);
		return ERROR_FD_SER;
	}	/* end if */

	/* Discard bytes queued under any previous port configuration. */
	if (ser_flush_io(fd) != 0) {
		upsdebug_with_errno(1, "%s: Error flushing serial port %s", __func__, portarg);
		ser_close(fd, portarg);
		return ERROR_FD_SER;
	}	/* end if */

	upsdebugx(1, "%s: Serial settings: %d baud, %u data bits, %s parity, %u stop bit(s), %s flow control, read timeout %u ms, send pace %u us", __func__, requested_baudrate, serial_data_bits, serial_parity, serial_stop_bits, serial_flow_control, serial_read_timeout_ms, serial_send_pace_us);

	return fd;
}

static unsigned char calculate_checksum(unsigned char *pacote, int inicio, int fim) {
	int	soma = 0, i = 0;

	for (i = inicio; i <= fim; i++)
		soma += pacote[i];
	return (soma & 0xFF);
}

static void pdatapacket(unsigned char *datapkt, size_t size) {
	size_t	i = 0;

	if (!debug_pkt_raw)
		return;

	if (datapkt != NULL) {
		/* FIXME: convert to upsdebug_hex()? */
		upsdebugx(1, "%s: logging received data packet bytes at debug verbosity 5 or more", __func__);

		for (i = 0; i < size; i++)
			upsdebugx(5, "\tPosition %" PRIuSIZE " -- 0x%02X -- Decimal %d -- Char %c", i, datapkt[i], datapkt[i], datapkt[i]);
	}	/* end if */
}

static float createfloat(int integer, int decimal) {
	char	flt[1024];
	snprintf(flt, sizeof(flt), "%d.%d", integer, decimal);
	return atof(flt);
}

static unsigned int get_vbat(void) {
	char	*v = getval("vbat");
	if (v)
		return atoi(v);
	else
		return DEFAULTBATV;
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
	}	/* end if */

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
	}	/* end else */
	pkthwinfo.checksum_calc = checksum;
	if (pkthwinfo.checksum == checksum)
		pkthwinfo.checksum_ok = true;

	if (debug_pkt_hwinfo) {
		pdatapacket(datapkt, size);
		print_pkt_hwinfo(pkthwinfo);
	}	/* end if */

	return pkthwinfo;
}

#if 0
static int write_serial(int fd, const char *dados, size_t size) {
	ssize_t	bytes_written;

	if (fd > 0) {
		bytes_written = write(fd, dados, size);
		if (bytes_written < 0)
			return -1;
		if (tcdrain(fd) != 0)
			return -2;
		return size;
	}	/* end if */
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
		sent = ser_send_buf_pace(fd, (useconds_t)serial_send_pace_us, message, size);

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

/*
 * Keep model metadata in an indexed table instead of a long switch to improve
 * visualization of protocol codes, descriptions and nominal VA values while
 * preserving direct lookup by the model code reported by the UPS.
 */
/*
 * Initialize behavior metadata explicitly for every known model. Keeping the
 * defaults beside the model data documents the resolved behavior and prevents
 * strict builds from treating omitted structure fields as compilation errors.
 */
#define UPS_INFO_ENTRY(code, description, nominal_va) \
	[code] = { code, description, nominal_va, 3, 1 }

static const upsinfo ups_info_table[114] = {
	UPS_INFO_ENTRY(1, "NHS COMPACT PLUS", 1000),
	UPS_INFO_ENTRY(2, "NHS COMPACT PLUS SENOIDAL", 1000),
	UPS_INFO_ENTRY(3, "NHS COMPACT PLUS RACK", 1000),
	UPS_INFO_ENTRY(4, "NHS PREMIUM PDV", 1500),
	UPS_INFO_ENTRY(5, "NHS PREMIUM PDV SENOIDAL", 1500),
	UPS_INFO_ENTRY(6, "NHS PREMIUM 1500VA", 1500),
	UPS_INFO_ENTRY(7, "NHS PREMIUM 2200VA", 2200),
	UPS_INFO_ENTRY(8, "NHS PREMIUM SENOIDAL", 1500),
	UPS_INFO_ENTRY(9, "NHS LASER 2600VA", 2600),
	UPS_INFO_ENTRY(10, "NHS LASER 3300VA", 3300),
	UPS_INFO_ENTRY(11, "NHS LASER 2600VA ISOLADOR", 2600),
	UPS_INFO_ENTRY(12, "NHS LASER SENOIDAL", 2600),
	UPS_INFO_ENTRY(13, "NHS LASER ON-LINE", 2600),
	UPS_INFO_ENTRY(15, "NHS COMPACT PLUS 2003", 1000),
	UPS_INFO_ENTRY(16, "COMPACT PLUS SENOIDAL 2003", 1000),
	UPS_INFO_ENTRY(17, "COMPACT PLUS RACK 2003", 1000),
	UPS_INFO_ENTRY(18, "PREMIUM PDV 2003", 1500),
	UPS_INFO_ENTRY(19, "PREMIUM PDV SENOIDAL 2003", 1500),
	UPS_INFO_ENTRY(20, "PREMIUM 1500VA 2003", 1500),
	UPS_INFO_ENTRY(21, "PREMIUM 2200VA 2003", 2200),
	UPS_INFO_ENTRY(22, "PREMIUM SENOIDAL 2003", 1500),
	UPS_INFO_ENTRY(23, "LASER 2600VA 2003", 2600),
	UPS_INFO_ENTRY(24, "LASER 3300VA 2003", 3300),
	UPS_INFO_ENTRY(25, "LASER 2600VA ISOLADOR 2003", 2600),
	UPS_INFO_ENTRY(26, "LASER SENOIDAL 2003", 2600),
	UPS_INFO_ENTRY(27, "PDV ONLINE 2003", 1500),
	UPS_INFO_ENTRY(28, "LASER ONLINE 2003", 3300),
	UPS_INFO_ENTRY(29, "EXPERT ONLINE 2003", 5000),
	UPS_INFO_ENTRY(30, "MINI 2", 500),
	UPS_INFO_ENTRY(31, "COMPACT PLUS 2", 1000),
	UPS_INFO_ENTRY(32, "LASER ON-LINE", 2600),
	UPS_INFO_ENTRY(33, "PDV SENOIDAL 1500VA", 1500),
	UPS_INFO_ENTRY(34, "PDV SENOIDAL 1000VA", 1000),
	UPS_INFO_ENTRY(36, "LASER ONLINE 3750VA", 3750),
	UPS_INFO_ENTRY(37, "LASER ONLINE 5000VA", 5000),
	UPS_INFO_ENTRY(38, "PREMIUM SENOIDAL 2000VA", 2000),
	UPS_INFO_ENTRY(39, "LASER SENOIDAL 3500VA", 3500),
	UPS_INFO_ENTRY(40, "PREMIUM PDV 1200VA", 1200),
	UPS_INFO_ENTRY(41, "PREMIUM 1500VA", 1500),
	UPS_INFO_ENTRY(42, "PREMIUM 2200VA", 2200),
	UPS_INFO_ENTRY(43, "LASER 2600VA", 2600),
	UPS_INFO_ENTRY(44, "LASER 3300VA", 3300),
	UPS_INFO_ENTRY(45, "COMPACT PLUS SENOIDAL 700VA", 700),
	UPS_INFO_ENTRY(46, "PREMIUM ONLINE 2000VA", 2000),
	UPS_INFO_ENTRY(47, "EXPERT ONLINE 10000VA", 10000),
	UPS_INFO_ENTRY(48, "LASER SENOIDAL 4200VA", 4200),
	UPS_INFO_ENTRY(49, "NHS COMPACT PLUS EXTENDIDO 1500VA", 1500),
	UPS_INFO_ENTRY(50, "LASER ONLINE 6000VA", 6000),
	UPS_INFO_ENTRY(51, "LASER EXT 3300VA", 3300),
	UPS_INFO_ENTRY(52, "NHS COMPACT PLUS 1200VA", 1200),
	UPS_INFO_ENTRY(53, "LASER SENOIDAL 3000VA GII", 3000),
	UPS_INFO_ENTRY(54, "LASER SENOIDAL 3500VA GII", 3500),
	UPS_INFO_ENTRY(55, "LASER SENOIDAL 4200VA GII", 4200),
	UPS_INFO_ENTRY(56, "LASER ONLINE 3000VA", 3000),
	UPS_INFO_ENTRY(57, "LASER ONLINE 3750VA", 3750),
	UPS_INFO_ENTRY(58, "LASER ONLINE 5000VA", 5000),
	UPS_INFO_ENTRY(59, "LASER ONLINE 6000VA", 6000),
	UPS_INFO_ENTRY(60, "PREMIUM ONLINE 2000VA", 2000),
	UPS_INFO_ENTRY(61, "PREMIUM ONLINE 1500VA", 1500),
	UPS_INFO_ENTRY(62, "PREMIUM ONLINE 1200VA", 1200),
	UPS_INFO_ENTRY(63, "COMPACT PLUS II MAX 1400VA", 1400),
	UPS_INFO_ENTRY(64, "PREMIUM PDV MAX 2200VA", 2200),
	UPS_INFO_ENTRY(65, "PREMIUM PDV 3000VA", 3000),
	UPS_INFO_ENTRY(66, "PREMIUM SENOIDAL 2200VA GII", 2200),
	UPS_INFO_ENTRY(67, "LASER PRIME SENOIDAL 3200VA GII", 3200),
	UPS_INFO_ENTRY(68, "PREMIUM RACK ONLINE 3000VA", 3000),
	UPS_INFO_ENTRY(69, "PREMIUM ONLINE 3000VA", 3000),
	UPS_INFO_ENTRY(70, "LASER ONLINE 4000VA", 4000),
	UPS_INFO_ENTRY(71, "LASER ONLINE 7500VA", 7500),
	UPS_INFO_ENTRY(72, "LASER ONLINE BIFASICO 5000VA", 5000),
	UPS_INFO_ENTRY(73, "LASER ONLINE BIFASICO 6000VA", 6000),
	UPS_INFO_ENTRY(74, "LASER ONLINE BIFASICO 7500VA", 7500),
	UPS_INFO_ENTRY(75, "NHS MINI ST", 500),
	UPS_INFO_ENTRY(76, "NHS MINI 120", 120),
	UPS_INFO_ENTRY(77, "NHS MINI BIVOLT", 500),
	UPS_INFO_ENTRY(78, "PDV 600", 600),
	UPS_INFO_ENTRY(79, "NHS MINI MAX", 500),
	UPS_INFO_ENTRY(80, "NHS MINI EXT", 500),
	UPS_INFO_ENTRY(81, "NHS AUTONOMY PDV 4T", 4000),
	UPS_INFO_ENTRY(82, "NHS AUTONOMY PDV 8T", 8000),
	UPS_INFO_ENTRY(83, "NHS COMPACT PLUS RACK 1200VA", 1200),
	UPS_INFO_ENTRY(84, "PDV SENOIDAL ISOLADOR 1500VA", 1500),
	UPS_INFO_ENTRY(85, "NHS PDV RACK 1500VA", 1500),
	UPS_INFO_ENTRY(86, "NHS PDV 1400VA S GII", 1400),
	UPS_INFO_ENTRY(87, "PDV SENOIDAL ISOLADOR 1500VA", 1500),
	UPS_INFO_ENTRY(88, "LASER PRIME SENOIDAL ISOLADOR 2000VA", 2000),
	UPS_INFO_ENTRY(89, "PREMIUM SENOIDAL 2400VA GII", 2400),
	UPS_INFO_ENTRY(90, "NHS PDV 1400VA S 8T GII", 1400),
	UPS_INFO_ENTRY(91, "PREMIUM ONLINE 2000VA", 2000),
	UPS_INFO_ENTRY(92, "LASER PRIME ONLINE 2200VA", 2200),
	UPS_INFO_ENTRY(93, "PREMIUM RACK ONLINE 2200VA", 2200),
	UPS_INFO_ENTRY(94, "PREMIUM SENOIDAL 2400VA GII", 2400),
	UPS_INFO_ENTRY(95, "LASER ONLINE 10000VA", 10000),
	UPS_INFO_ENTRY(96, "LASER ONLINE BIFASICO 10000VA", 10000),
	UPS_INFO_ENTRY(97, "LASER SENOIDAL 3300VA GII", 3300),
	UPS_INFO_ENTRY(98, "LASER SENOIDAL 2600VA GII", 2600),
	UPS_INFO_ENTRY(99, "PREMIUM SENOIDAL 3000VA GII", 3000),
	UPS_INFO_ENTRY(100, "PREMIUM SENOIDAL 2200VA GII", 2200),
	UPS_INFO_ENTRY(101, "LASER ONLINE BIFASICO 4000VA", 4000),
	UPS_INFO_ENTRY(102, "LASER ONLINE 12000VA", 12000),
	UPS_INFO_ENTRY(103, "LASER ONLINE 8000VA", 8000),
	UPS_INFO_ENTRY(104, "PDV SENOIDAL ISOLADOR 1000VA", 1000),
	UPS_INFO_ENTRY(105, "MINI SENOIDAL 500VA", 500),
	UPS_INFO_ENTRY(106, "LASER SENOIDAL 5000VA GII", 5000),
	UPS_INFO_ENTRY(107, "COMPACT PLUS SENOIDAL 1000VA", 1000),
	UPS_INFO_ENTRY(108, "QUAD_COM 80A", 0),
	UPS_INFO_ENTRY(109, "LASER ONLINE 5000VA", 5000),
	UPS_INFO_ENTRY(113, "PDV SENOIDAL ISOLADOR 700VA", 700),
};

#undef UPS_INFO_ENTRY

static upsinfo getupsinfo(unsigned int upscode) {
	upsinfo	data = { (unsigned int)-1, "NHS UNKNOWN", 0, 3, 1 };
	char	*overridemodel = getval("overridemodel");
	char	*protocolversion = getval("protocolversion");
	char	*bypassasalarm = getval("bypassasalarm");

	if (upscode > 0 && upscode < sizeof(ups_info_table) / sizeof(ups_info_table[0]) && ups_info_table[upscode].upscode == upscode) {
		data = ups_info_table[upscode];
	} /* end if */

	/*
	 * Protocol version 3 and bypass-as-alarm are initialized explicitly both in
	 * the model table and in the synthetic unknown-model entry. Values supplied
	 * in ups.conf customize only the returned copy, leaving the static table
	 * unchanged. Protocol version is retained as model metadata for now and does
	 * not select framing or initialization commands.
	 */
	if (overridemodel && overridemodel[0] != '\0') {
		data.upscode = upscode;
		snprintf(data.upsdesc, sizeof(data.upsdesc), "%s", overridemodel);
	} /* end if */
	if (protocolversion)
		data.pversion = (unsigned int)atoi(protocolversion);
	if (bypassasalarm)
		data.bypassasalarm = (unsigned int)atoi(bypassasalarm);

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
	}	/* end else */
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

/*
 * Send one hardware-discovery request only through a valid serial descriptor.
 * If the descriptor is already invalid, try the driver's bounded reopen path.
 * A failed or partial write closes and invalidates the descriptor so the next
 * update cycle knows that it must reopen the port before communicating again.
 */
static bool send_initialization_packet(const unsigned int *data, size_t size, TYPE_FD_SER fd) {
	int	written;

	if (INVALID_FD_SER(fd)) {
		upslogx(LOG_WARNING, "%s: serial port %s is not open before the initialization request; trying to reopen it", __func__, porta);
		fd = reconnect_ups_if_needed();
		if (INVALID_FD_SER(fd)) {
			upslogx(LOG_WARNING, "%s: unable to send the initialization request because serial port %s could not be reopened", __func__, porta);
			dstate_datastale();
			return false;
		}	/* end if */
	}	/* end if */

	upsdebugx(3, "%s: sending initialization request (%" PRIuSIZE " bytes) on %s", __func__, size, porta);
	errno = 0;
	written = write_serial_int(fd, data, size);
	if (written < 0 || (size_t)written != size) {
		if (errno != 0)
			upslog_with_errno(LOG_WARNING, "%s: failed to send the initialization request on %s", __func__, porta);
		else
			upslogx(LOG_WARNING, "%s: failed to send the complete initialization request on %s; wrote %d of %" PRIuSIZE " bytes", __func__, porta, written, size);

		/* Mark the connection unusable. upsdrv_updateinfo() will invoke the
		 * normal reopen procedure before its next read or retry.
		 */
		close_serial_port();
		dstate_datastale();
		return false;
	}	/* end if */

	upsdebugx(3, "%s: initialization request sent successfully on %s", __func__, porta);
	return true;
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
	if (!send_initialization_packet(string_initialization_long, 9, serial_fd))
		return;
	usleep(250000);
	if (!send_initialization_packet(string_initialization_short, 9, serial_fd))
		return;
	usleep(250000);
	if (!send_initialization_packet(string_initialization_comptmode, 5, serial_fd))
		return;

	upsdebugx(3, "%s: initialization commands sent", __func__);

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
	/* retries to open port until we declare "data stale" loudly */
	static unsigned int	retries = 0;
	bool	retry_limit_reached = false;

	/* If comms failed earlier, try to resuscitate */
	if (INVALID_FD_SER(serial_fd)) {
		upsdebugx(1, "%s: Serial port '%s' communications problem", __func__, porta);

		/* Uh oh, got to reconnect! */
		reconnect_trying(RECONNECT_TRYING);

		/* Close any surviving handle and mark it invalid before reopening. */
		close_serial_port();

		while (INVALID_FD_SER(serial_fd) && !retry_limit_reached) {
			upsdebugx(1, "%s: Trying to reopen serial...", __func__);
			serial_fd = openfd(porta, baudrate);
			retries++;
			if (retries >= MAXTRIES) {
				upsdebugx(1, "%s: serial port reopen failed", __func__);
				retry_limit_reached = true;
			}	/* end if */
			else
				usleep(checktime);
		}	/* end while */

		if (VALID_FD_SER(serial_fd)) {
			if (retries > MAXTRIES && may_log_reconnect_trying(1))
				upslogx(LOG_NOTICE, "Communications with UPS re-established");

			/* A reopened serial port may now be connected to a different UPS.
			 * Invalidate the cached discovery result and restart the initialization
			 * sequence so model-specific data is obtained without restarting NUT.
			 */
			lastpkthwinfo.checksum_ok = false;
			send_extended = 0;
			checktime = 2000000;
			retries = 0;
			reconnect_trying(RECONNECT_SUCCESS);
		}	/* end if */
		else {
			if (retries == MAXTRIES && may_log_reconnect_trying(1))
				upslogx(LOG_WARNING, "Communications with UPS lost: port reopen failed!");
			dstate_datastale();
		}	/* end else */
	}	/* end if */

	return serial_fd;
}

static void interpret_pkt_hwinfo(void) {
	/* TOTHINK: Consider passing in the packet struct as parameter? */
	upsinfo	ups;
	char	hw_scratch_buf[1024];	/* General-purpose string buffer */
	unsigned int	i = 0;

	/* @freechurros identified in issue #3592 that this function used to
	 * inspect lastpktdata here. Validate the HWINFO packet being interpreted,
	 * otherwise a valid HWINFO received before the first DATA packet is lost.
	 */
	if (!lastpkthwinfo.checksum_ok) {
		upslogx(LOG_WARNING, "%s: bad lastpkthwinfo.checksum", __func__);
		return;
	}	/* end if */

	if (lastpkthwinfo.size < 1) {
		upslogx(LOG_WARNING, "%s: Pkt HWINFO is not OK. See if will be requested next time!", __func__);
		return;
	}	/* end if */

	/* checksum is OK, then use it to set values */
	upsdebugx(4, "Pkt HWINFO is OK. Model code is %u, hwversion is %u and swversion is %u", lastpkthwinfo.model, lastpkthwinfo.hardwareversion, lastpkthwinfo.softwareversion);

	/* We need to set data on NUT with data
	 * that I believe that I can calculate.
	 * Now setting data on NUT
	 */
	ups = getupsinfo(lastpkthwinfo.model);
	upsdebugx(4, "UPS Struct data: Code %u Model %s VA %u Protocol version %u Bypass as alarm %u", ups.upscode, ups.upsdesc, ups.VA, ups.pversion, ups.bypassasalarm);
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
		}	/* end for */
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
		}	/* end for */
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
	}	/* end if */
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
	unsigned int	i = 0;
	upsinfo	ups;

	if (!lastpktdata.checksum_ok) {
		upslogx(LOG_WARNING, "%s: bad lastpktdata.checksum", __func__);
		return;
	}	/* end if */

	/* checksum is OK, then use it to set values */
	upsdebugx(4, "%s: Data Packet seems be OK", __func__);

	/* Not return, but we would miss some data points */
	if (!got_hwinfo)
		upsdebugx(2, "%s: Pkt HWINFO is not OK. See if will be requested next time. Some data points will not be set on this pass!", __func__);

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
	upsdebugx(4, "Set UPS status as OFF and start checking. s_battery_mode is %d", lastpktdata.s_battery_mode);

	if (got_hwinfo) {
		if (lastpkthwinfo.s_220V_in) {
			upsdebugx(4, "I'm on 220v IN!. My undervoltage is %u", lastpkthwinfo.undervoltagein220V);
			min_input_power = lastpkthwinfo.undervoltagein220V;
		}	/* end if */
		else {
			upsdebugx(4, "I'm on 120v IN!. My undervoltage is %u", lastpkthwinfo.undervoltagein120V);
			min_input_power = lastpkthwinfo.undervoltagein120V;
		}	/* end else */
	}	/* end if */
	else {
		if (!min_input_power) {
			min_input_power = 96;
			upsdebugx(4, "I'm on unknown input!. My undervoltage is default %d", min_input_power);
		}	/* end if */
	}	/* end else */

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
			upsdebugx(4, "UPS has power-in value %0.2f and min_input_power is %d, or network is in failure. Network failure is %d", lastpktdata.vacinrms, min_input_power, lastpktdata.s_network_failure);
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
				}	/* end if */
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
				else
					/* ...or network failure */
					status_set("OB");
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
	}	/* end if */

	/* No ups.alarm changes above this line */
	alarm_init();
	ups = getupsinfo(lastpkthwinfo.model);

	if (lastpktdata.s_battery_low)
		alarm_set("[LOW BATTERY]");
	/*
	 * Some line-interactive models keep the decoded bypass bit set during normal
	 * on-line operation. A zero bypassasalarm setting preserves the decoded bit
	 * for diagnostics without publishing a permanent [ON BYPASS] alarm.
	 */
	if (lastpktdata.s_bypass_on && ups.bypassasalarm == 1)
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
	}	/* end if */

	dstate_setinfo("output.voltage", "%0.2f", lastpktdata.vacoutrms);
	dstate_setinfo("input.voltage", "%0.2f", lastpktdata.vacinrms);
	/* Map the packet's observed minimum and maximum to their matching NUT names. */
	dstate_setinfo("input.voltage.maximum", "%0.2f", lastpktdata.vacinrmsmax);
	dstate_setinfo("input.voltage.minimum", "%0.2f", lastpktdata.vacinrmsmin);

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
	}	/* end if */

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
	}	/* end if */

	/* Battery charger status
	 * @freechurros reported in issue #3592 that Home Assistant rejects the
	 * former upper-case strings. Use the lower-case vocabulary documented in
	 * docs/nut-names.txt for interoperability with NUT clients.
	 */
	if (lastpktdata.s_charger_on)
		dstate_setinfo("battery.charger.status", "%s", "charging");
	else {
		if (lastpktdata.s_battery_mode)
			dstate_setinfo("battery.charger.status", "%s", "discharging");
		else
			dstate_setinfo("battery.charger.status", "%s", "resting");
	}	/* end else */

	if (debug_pkt_data) {
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
		}	/* end for */
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
	}	/* end if */
}

void upsdrv_updateinfo(void) {
	double	tempodecorrido = 0.0;
	time_t	now, timeout_sec;
	useconds_t	timeout_usec;
	ssize_t	read_result;
	int	randval = 0;
	pkt_hwinfo	received_hwinfo;

	upsdebugx(3, "%s: starting...", __func__);

	/* If comms failed earlier, try to resuscitate */
	if (INVALID_FD_SER(reconnect_ups_if_needed()))
		return;

	chr = '\0';

	/* ser_get_char accepts separate seconds and microseconds components. */
	timeout_sec = (time_t)(serial_read_timeout_ms / 1000);
	timeout_usec = (useconds_t)((serial_read_timeout_ms % 1000) * 1000);

	/*
	 * A positive result supplies one byte, zero is the configured normal
	 * timeout, and a negative result is handled as a communication error.
	 */
	read_result = ser_get_char(serial_fd, &chr, timeout_sec, timeout_usec);
	while (read_result > 0) {
		/* The length-aware framing and the need to preserve marker values inside
		 * packet payloads were demonstrated by raw captures contributed by
		 * @freechurros in issue #3592.
		 *
		 * A 0xFF byte starts a packet only while the reader is idle. After
		 * the packet starts, byte [1] declares its total size, including the
		 * initial 0xFF, checksum and final 0xFE. Any 0xFF or 0xFE received
		 * before the declared final position belongs to the packet contents.
		 * The packet is interpreted only when byte [size - 1] is 0xFE.
		 */
		if ((chr == 0xFF) && (!datapacketstart)) {
			datapacketstart = true;
			memset(datapacket, 0, sizeof(datapacket));
			datapacket_index = 0;
			datapacketsize = 0;
		}	/* end if */
		if (datapacketstart) {
			/* Assume that second position is PACKET SIZE. */
			if (datapacket_index == 1)
				datapacketsize = chr;
			datapacket[datapacket_index] = chr;
			if ((datapacket_index == 1) && ((datapacketsize < 18) || (datapacketsize > sizeof(datapacket)))) {
				upslogx(LOG_WARNING, "Incoming packet declares an invalid size, discarding!");
				datapacketstart = false;
			}	/* end if */
			else {
				if ((datapacketsize > 0) && (datapacket_index == (datapacketsize - 1))) {
					if (datapacket[datapacket_index] != 0xFE)
						upslogx(LOG_WARNING, "Incoming packet does not end with 0xFE, discarding!");
					else {
						now = time(NULL);
						upsdebugx(4, "DATAPACKET SIZE IS %u", datapacketsize);

						if (lastdp != 0)
							tempodecorrido = difftime(now, lastdp);

						lastdp = now;

						switch (datapacketsize) {
							case 18:
							case 50:
								received_hwinfo = mount_hwinfo(datapacket, datapacketsize);

								/* Always accept a new valid HWINFO packet. The UPS may be
								 * replaced while the driver remains active, so retaining only
								 * the first packet would leave model-specific data stale until
								 * NUT is restarted. Preserve the last valid data if the newly
								 * received packet has a bad checksum.
								 */
								if (received_hwinfo.checksum_ok) {
									lastpkthwinfo = received_hwinfo;
									interpret_pkt_hwinfo();
									dstate_dataok();
								}	/* end if */
								break;

							case 21:
								lastpktdata = mount_datapacket(datapacket, datapacketsize, tempodecorrido, lastpkthwinfo);

								if (lastpktdata.checksum_ok) {
									interpret_pkt_data();
									dstate_dataok();
								}	/* end if */
								break;

							default:
								upslogx(LOG_WARNING, "Incoming packet size not recognized, discarding!");
								/* Suggested by @freechurros in issue #3592: expose the
								 * rejected bytes when raw packet debugging is enabled so
								 * future framing variants can be diagnosed from the log.
								 */
								pdatapacket(datapacket, datapacketsize);
								break;
						}	/* end switch */
					}	/* end else */

					datapacketstart = false;
				}	/* end if */
			}	/* end else */
		}	/* end if */
		/* Advance only after consuming the current byte. Keeping both the
		 * index and declared size outside this function lets a packet resume
		 * at the correct position after a normal serial-read timeout.
		 */
		datapacket_index++;
		read_result = ser_get_char(serial_fd, &chr, timeout_sec, timeout_usec);
	}	/* end while */

	if (read_result < 0) {
		upsdebug_with_errno(1, "%s: Serial read failed on %s", __func__, porta);
		close_serial_port();
		dstate_datastale();
		return;
	}	/* end if */
	if (datapacketstart) {
		upsdebugx(2, "%s: packet reading did not finish, not interpreting yet", __func__);
		return;
	}	/* end if */

	/* Now the nobreak read buffer is empty.
	 * We need a hw info packet to discover several variables,
	 * like number of batteries, to calculate some data
	 * FIXME: move (semi)static info discovery to upsdrv_initinfo() or so
	 */
	if (!lastpkthwinfo.checksum_ok) {
		upsdebugx(4, "pkt_hwinfo loss -- Requesting");
		/* If size == 0, packet maybe not initizated,
		 * then send an initialization packet to obtain data.
		 * Send six times the extended initialization string,
		 * but, on fail, try randomly send extended, normal or compatibility.
		 */
		if (send_extended < 6) {
			upsdebugx(4, "Sending extended initialization packet. Try %u", send_extended+1);
			bwritten = write_serial_int(serial_fd, string_initialization_long, 9);
			send_extended++;
		}	/* end if */
		else {
			/* randomly send */
			randval = rand() % 3;
			switch (randval) {
				case 0:
					upsdebugx(4, "Sending long initialization packet (random)");
					bwritten = write_serial_int(serial_fd, string_initialization_long, 9);
					break;
				case 1:
					upsdebugx(4, "Sending short initialization packet (random)");
					bwritten = write_serial_int(serial_fd, string_initialization_short, 9);
					break;
				case 2:
					upsdebugx(4, "Sending compatibility initialization packet (random)");
					bwritten = write_serial_int(serial_fd, string_initialization_comptmode, 5);
					break;
				default:
					/* rand() % 3 currently limits this switch to cases 0..2.
					 * Keep a deterministic fallback so future changes to the
					 * selection range cannot leave bwritten with stale data.
					 */
					upsdebugx(4, "Initialization selection out of range; sending long packet");
					bwritten = write_serial_int(serial_fd, string_initialization_long, 9);
					break;
			}	/* end switch */
		}	/* end else */
		if (bwritten < 0) {
			upsdebugx(1, "%s: Problem to write data to %s", __func__, porta);
			if (bwritten == -1)
				upsdebugx(1, "%s: Data problem", __func__);
			close_serial_port();
		}	/* end if */
		else {
			if (checktime > max_checktime)
				checktime = max_checktime;
			else {
				upsdebugx(3, "Increase checktime to %u", checktime + 100000);
				checktime = checktime + 100000;
			}	/* end else */
			usleep(checktime);
		}	/* end else */
	}	/* end if lastpkthwinfo good/bad checksum */	/* end if */

	upsdebugx(3, "%s: finished", __func__);
}

void upsdrv_shutdown(void) {
	upsdebugx(3, "%s: starting...", __func__);

	/* replace with a proper shutdown function */
	upslogx(LOG_ERR, "shutdown not supported");
	set_exit_flag(EF_EXIT_FAILURE);

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

	upsdebugx(1, "%s: Port is %s and baud_rate is %d", __func__, device_path, baudrate);

	if (device_path) {
		if (strcasecmp(device_path, "auto") == 0)
			strncpy(porta, DEFAULTPORT, sizeof(porta) - 1);
		else
			strncpy(porta, device_path, sizeof(porta) - 1);
		serial_fd = openfd(porta, baudrate);
		if (INVALID_FD_SER(serial_fd))
			fatalx(EXIT_FAILURE, "Unable to open port %s with baud %d", porta, baudrate);
		else
			upsdebugx(1, "%s: Communication started on port %s, baud rate %d", __func__, porta, baudrate);
	}	/* end if */
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

	addvar(VAR_VALUE, "overridemodel", "Override the UPS model name reported by the driver");

	addvar(VAR_VALUE, "protocolversion", "Override the NHS protocol version stored for the model (default: 3)");

	addvar(VAR_VALUE, "bypassasalarm", "Report the UPS bypass bit as an alarm: 0 or 1 (default: 1)");
}

void upsdrv_help(void) {
}

/* optionally tweak prognames[] entries */
void upsdrv_tweak_prognames(void)
{
}
