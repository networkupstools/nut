/* ever-hid.c - subdriver to monitor EVER USB/HID devices with NUT
 *
 *  Copyright (C)
 *  2003 - 2012	Arnaud Quette <ArnaudQuette@Eaton.com>
 *  2005 - 2006	Peter Selinger <selinger@users.sourceforge.net>
 *  2008 - 2009	Arjen de Korte <adkorte-guest@alioth.debian.org>
 *  2013 Charles Lepple <clepple+nut@gmail.com>
 *  2017 EVER Power Systems [https://ever.eu/]
 *  2020 - 2022	Jim Klimov <jimklimov+nut@gmail.com>
 *
 *  Note: this subdriver was initially generated as a "stub" by the
 *  gen-usbhid-subdriver script. It must be customized.
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "config.h"	/* must be first */

#include "usbhid-ups.h"
#include "ever-hid.h"
#include "main.h"	/* for getval() */
#include "usb-common.h"

#define EVER_HID_VERSION	"Ever HID 0.10"
/* FIXME: experimental flag to be put in upsdrv_info */

/* Ever */
#define EVER_VENDORID	0x2e51

/* ST Microelectronics */
#define STMICRO_VENDORID	0x0483
/* Please note that USB vendor ID 0x0483 is from ST Microelectronics -
 * with actual product IDs delegated to different OEMs.
 * Devices handled in this driver are marketed under Ever brand.
 */

/* USB IDs device table */
static usb_device_id_t ever_usb_device_table[] = {

	{ USB_DEVICE(STMICRO_VENDORID, 0xa113), NULL },
	{ USB_DEVICE(EVER_VENDORID, 0xffff), NULL},
	{ USB_DEVICE(EVER_VENDORID, 0x0000), NULL},

	/* Terminating entry */
	{ 0, 0, NULL }
};

/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

static const char *ever_format_hardware_fun(double value)
{
	/* TODO - add exception handling for v1.0b0B */
	const char* hard_rev[27] = {"0", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"};
	static char model[10];
	snprintf(model, sizeof(model), "rev.%sv%02u",
		(&hard_rev[ ((unsigned int)value & 0xFF00)>>8 ])[0],
		(unsigned int)value & 0xFF );
	return model;
}

static const char *ever_format_version_fun(double value)
{
	static char model[10];

	/*upsdebugx(1, "UPS ups_firmware_conversion_fun VALUE: %d", (long)value);*/
	snprintf(model, sizeof(model), "v%X.%Xb%02d",
		((unsigned int)value & 0xF000)>>12,
		((unsigned int)value & 0xF00)>>8,
		((int)value & 0xFF) );
	return model;
}

static const char *ever_mac_address_fun(double value)
{
	int	mac_adress_report_id = 210;
	int	len = reportbuf->len[mac_adress_report_id];
	int	n = 0;	/* number of characters currently in line */
	int	i;	/* number of bytes output from buffer */
	const void	*buf = reportbuf->data[mac_adress_report_id];
	static char	line[100];
	NUT_UNUSED_VARIABLE(value);

	line[0] = '\0';

	/* skip first element which is a report id */
	for (i = 1; i < len; i++) {
		n = snprintfcat(line, sizeof(line), n ? ":%02x" : "%02x",
			((unsigned char *)buf)[i]);
	}

	return line;
}

static const char *ever_ip_address_fun(double value)
{
	static int	report_counter = 1;
	int	report_id = 211, len;
	int	n = 0;	/* number of characters currently in line */
	int	i;	/* number of bytes output from buffer */
	const void	*buf;
	static char	line[100];
	NUT_UNUSED_VARIABLE(value);

	if(report_counter == 1)
		report_id = 211; /* notification dest ip */
	else if(report_counter == 2)
		report_id = 230; /* ip address */
	else if(report_counter == 3)
		report_id = 231;	/* network mask */
	else if(report_counter == 4)
		report_id = 232;	/* default gateway */

	report_counter== 4 ? report_counter=1 : report_counter++;

	len = reportbuf->len[report_id];
	buf = reportbuf->data[report_id];

	line[0] = '\0';

	/* skip first element which is a report id */
	for (i = 1; i < len; i++)
	{
		n = snprintfcat(line, sizeof(line), n ? ".%d" : "%d",
			((unsigned char *)buf)[i]);
	}

	return line;
}

static const char *ever_packets_fun(double value)
{
	static int	report_counter = 1;
	int	report_id = 215, len, res;
	const unsigned char	*buf;
	static char	line[200];
	NUT_UNUSED_VARIABLE(value);

	if(report_counter == 1 )
		report_id = 215;
	else if(report_counter == 2 )
		report_id = 216;
	else if(report_counter == 3 )
		report_id = 217;
	else if(report_counter == 4 )
		report_id = 218;

	report_counter== 4 ? report_counter=1 : report_counter++;

	len = reportbuf->len[report_id];
	buf = reportbuf->data[report_id];

	line[0] = '\0';

	/* skip first element which is a report id */

	if(len < 5)
		return "";

	res  = (int)buf[1];
	res |= (int)buf[2] << 8;
	res |= (int)buf[3] << 16;
	res |= (int)buf[4] << 24;

    snprintf(line, sizeof(line), "%d", res);
	return line;

}

static const char* ever_workmode_fun(double value)
{
	int	workmode_report_id = 74;
	int	workmode = -1;
	const unsigned char	*buf = reportbuf->data[workmode_report_id];
	static char	line[200];
	NUT_UNUSED_VARIABLE(value);

	line[0] = '\0';

	/* skip first element which is a report id */
	snprintfcat(line, sizeof(line), "%d", buf[1]);

	workmode = atoi(line);

	switch(workmode)
	{
		case 1:
			return "UNKNOWN";

		case 2:
			return "STOP";

		case 4:
			return "ONLINE";

		case 8:
			return "ONBATTERY";

		case 16:
			return "WATCH";

		case 32:
			return "WAITING";

		case 64:
			return "EMERGENCY";

		default:
			return "UNKNOWN";
	}

}

static const char* ever_messages_fun(double value)
{
	int	messages_report_id = 75, messages;
	int	n = 0;	/* number of characters currently in line */
	const unsigned char	*buf = reportbuf->data[messages_report_id];
	static char	line[200];
	NUT_UNUSED_VARIABLE(value);

	line[0] = '\0';

	/* skip first element which is a report id */
	messages  = (int)buf[1];
	messages |= (int)buf[2] << 8;

	/* duplicate of ups.status: OB LB */
	/*
	if(messages & 0x01)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "OVERLOAD");
	if(messages & 0x02)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "BATTERY_LOW");
	*/
	if(messages & 0x04)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "BOOST");
	if(messages & 0x08)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "BUCK");
	if(messages & 0x10)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "BOOST_BLOCKED");
	if(messages & 0x20)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "BUCK_BLOCKED");
	if(messages & 0x40)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "CHARGING");
	if(messages & 0x80)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "FAN_ON");
	if(messages & 0x100)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "EPO_BLOCKED");
	if(messages & 0x200)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "NEED_REPLACMENT");
	if(messages & 0x400 || messages & 0x800)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "OVERHEAT");
	if(messages & 0x1000)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "WAITING_FOR_MIN_CHARGE");
	if(messages & 0x2000)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "MAINS_OUT_OF_RANGE");
	return line;
}

static const char* ever_alarms_fun(double value)
{
	int	alarms_report_id = 76, alarms;
	int	n = 0;	/* number of characters currently in line */
	const unsigned char	*buf = reportbuf->data[alarms_report_id];
	static char	line[200];
	NUT_UNUSED_VARIABLE(value);

	line[0] = '\0';

	/* skip first element which is a report id */
	alarms  = (int)buf[1];
	alarms |= (int)buf[2] << 8;

	if(alarms & 0x01)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "OVERLOAD");
	if(alarms & 0x02)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "SHORT-CIRCUIT");
	if(alarms & 0x04 || alarms & 0x08)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "OVERHEAT");
	if(alarms & 0x10)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "EPO");
	if(alarms & 0x20)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "INERNAL_ERROR");
	if(alarms & 0x40)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "REVERSE_POWER_SUPPLY");
	if(alarms & 0x80)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "NO_INETERNAL_COMM");
	if(alarms & 0x100)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "CRITICAL_BATT_VOLTAGE");

	return line;
}

static const char* ever_on_off_fun(double value)
{
	int	workmode_report_id = 74;
	int	workmode = -1;
	const unsigned char	*buf = reportbuf->data[workmode_report_id];
	static char	line[200];
	NUT_UNUSED_VARIABLE(value);

	line[0] = '\0';

	/* skip first element which is a report id */
	snprintfcat(line, sizeof(line), "%d", buf[1]);

	workmode = atoi(line);

	if(workmode != 0x04 && workmode != 0x08)
		return "off";

	return "!off";
}

static info_lkp_t ever_format_hardware[] = {
	{ 0, NULL, ever_format_hardware_fun, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t ever_format_version[] = {
	{ 0, NULL, ever_format_version_fun, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t ever_mac_address[] = {
	{ 0, NULL, ever_mac_address_fun, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t ever_ip_address[] = {
	{ 0, NULL, ever_ip_address_fun, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t ever_packets[] = {
	{ 0, NULL, ever_packets_fun, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t ever_workmode[] = {
	{ 0, NULL, ever_workmode_fun, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t ever_messages[] = {
	{ 0, NULL, ever_messages_fun, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t ever_alarms[] = {
	{ 0, NULL, ever_alarms_fun, NULL },
	{ 0, NULL, NULL, NULL }
};

static info_lkp_t ever_on_off_info[] = {
	{ 0, NULL, ever_on_off_fun, NULL },
	{ 0, NULL, NULL, NULL }
};


/* EVER usage table */
static usage_lkp_t ever_usage_lkp[] = {
	{ "EVER1",	0x00000000 },
	{ "EVER2",	0xff000000 },
	{ "EVER3",	0xff000001 },
	{ "EVER4",	0xff000002 },
	{ "EVER5",	0xff000003 },
	{ "EVER6",	0xff000004 },
	{ "EVER7",	0xff000005 },
	{ "EVER8",	0xff000006 },
	{ "EVER9",	0xff000007 },
	{ "EVER10",	0xff000008 },
	{ "EVER11",	0xff000009 },
	{ "EVER12",	0xff000010 },
	{ "EVER13",	0xff000011 },
	{ "EVER14",	0xff000012 },
	{ "EVER15",	0xff000013 },
	{ "EVER16",	0xff000014 },
	{ "EVER17",	0xff000015 },
	{ "EVER18",	0xff000016 },
	{ "EVER19",	0xff000017 },
	{ "EVER20",	0xff000018 },
	{ "EVER21",	0xff000019 },
	{ "EVER22",	0xff00001a },
	{ "EVER23",	0xff00001b },
	{ "EVER24",	0xff00001c },
	{ "EVER25",	0xff00001d },
	{ "EVER26",	0xff00001e },
	{ "EVER27",	0xff00001f },
	{ "EVER28",	0xff000020 },
	{ "EVER29",	0xff000021 },
	{ "EVER30",	0xff000022 },
	{ "EVER31",	0xff000023 },
	{ "EVER32",	0xff000030 },
	{ "EVER33",	0xff000031 },
	{ "EVER34",	0xff000032 },
	{ "EVER35",	0xff000033 },
	{ "EVER36",	0xff000034 },
	{ "EVER37",	0xff000035 },
	{ "EVER38",	0xff000036 },
	{ "EVER39",	0xff000037 },
	{ "EVER40",	0xff000038 },
	{ "EVER41",	0xff000039 },
	{ "EVER42",	0xff000040 },
	{ "EVER43",	0xff000041 },
	{ "EVER44",	0xff000042 },
	{ "EVER45",	0xff000043 },
	{ "EVER46",	0xff000044 },
	{ "EVER47",	0xff000045 },
	{ "EVER48",	0xff000046 },
	{ "EVER49",	0xff000050 },
	{ "EVER50",	0xff000051 },
	{ "EVER51",	0xff000052 },
	{ "EVER52",	0xff000053 },
	{ "EVER53",	0xff000054 },
	{ "EVER54",	0xff000060 },
	{ "EVER55",	0xff000061 },
	{ "EVER56",	0xff000062 },
	{ "EVER57",	0xff000063 },
	{ "EVER58",	0xff000064 },
	{ "EVER59",	0xff000066 },
	{ "EVER60",	0xff000067 },
	{ "EVER61",	0xff000068 },
	{ "EVER62",	0xff000069 },
	{ "EVER63",	0xff00006a },
	{ "EVER64",	0xff00006b },
	{ "EVER65",	0xff00006c },
	{ "EVER66",	0xff00006d },
	{ "EVER67",	0xff00006e },
	{ "EVER68",	0xff00006f },
	{ "EVER69",	0xff000070 },
	{ "EVER70",	0xff000071 },
	{ "EVER71",	0xff000072 },
	{ "EVER72",	0xff000073 },
	{ "EVER73",	0xff000074 },
	{ "EVER74",	0xff000075 },
	{ "EVER75",	0xff000076 },
	{ "EVER76",	0xff000077 },
	{ "EVER77",	0xff000078 },
	{ "EVER78",	0xff000079 },
	{ "EVER79",	0xff00007a },
	{ "EVER80",	0xff00007b },
	{ "EVER81",	0xff00007c },
	{ "EVER82",	0xff00007d },
	{ "EVER83",	0xff00007e },
	{ "EVER84",	0xff00007f },
	{ "EVER85",	0xff000080 },
	{ "EVER86",	0xff000081 },
	{ "EVER87",	0xff000082 },
	{ "EVER88",	0xff000083 },
	{ "EVER89",	0xff000084 },
	{ "EVER90",	0xff000085 },
	{ "EVER91",	0xff000086 },
	{ "EVER92",	0xff000087 },
	{ "EVER93",	0xff000088 },
	{ "EVER94",	0xff000089 },
	{ "EVER95",	0xff00008a },
	{ "EVER96",	0xff00008b },
	{ "EVER97",	0xff000090 },
	{ "EVER98",	0xff000091 },
	{ "EVER99",	0xff000092 },
	{ "EVER100",	0xff000093 },
	{ "EVER101",	0xff000094 },
	{ "EVER102",	0xff000095 },
	{ "EVER103",	0xff000096 },
	{ "EVER104",	0xff000097 },
	{  NULL, 0 }
};

static usage_tables_t ever_utab[] = {
	ever_usage_lkp,
	hid_usage_lkp,
	NULL,
};


/* --------------------------------------------------------------- */
/* HID2NUT lookup table                                            */
/* --------------------------------------------------------------- */

static hid_info_t ever_hid2nut[] = {

  /* Note: fields marked with "experimental." prefix were proposed without
   * an exact match vs. docs/nut-names.txt definitions. PRs are welcome to
   * analyze and map those values into standard NUT variable names, or to
   * raise discussion on mailing lists and define new names via consensus.
   * There is a lot of interesting info listed below.
   *
   * Note: mappings that were applied below (as committed 2022-02-09) may
   * be wrong and are based mostly on cursory reading of original names.
   * In particular, not sure if the skipped "id.*" fields were identifiers
   * or some "internal device" etc.
   */

  /* experimental: "NUT variable names" do not currently have
   * any battery.*id data points: */
  { "experimental.battery.batteryid", 0, 0, "UPS.BatterySystem.Battery.BatteryID", NULL, "%.0f", 0, NULL },
  { "experimental.battery.systemid", 0, 0, "UPS.BatterySystem.BatterySystemID", NULL, "%.0f", 0, NULL },
  { "experimental.battery.chargerid", 0, 0, "UPS.BatterySystem.Charger.ChargerID", NULL, "%.0f", 0, NULL },
  { "experimental.battery.input_flowid", 0, 0, "UPS.BatterySystem.Input.FlowID", NULL, "%.0f", 0, NULL },
  { "experimental.battery.input_id", 0, 0, "UPS.BatterySystem.Input.InputID", NULL, "%.0f", 0, NULL },
  { "experimental.battery.output_flowid", 0, 0, "UPS.BatterySystem.Output.FlowID", NULL, "%.0f", 0, NULL },
  { "experimental.battery.output_id", 0, 0, "UPS.BatterySystem.Output.OutputID", NULL, "%.0f", 0, NULL },

  /* experimental: "NUT variable names" do not currently have
   * any id (nor version) data points for FW/HW of components: */
  /* not implemented*/
  /* { "experimental.id.ups_type", 0, 0, "UPS.EVER1.EVER12", NULL, "%s", 0, ever_format_model }, */
  { "experimental.id.firmware_version_inverter", 0, 0, "UPS.EVER1.EVER13", NULL, "%s", 0, ever_format_version },
  { "experimental.id.firmware_version_interfaces", 0, 0, "UPS.EVER1.EVER14", NULL, "%s", 0, ever_format_version },
  { "experimental.id.hardware_version", 0, 0, "UPS.EVER1.EVER15", NULL, "%s", 0, ever_format_hardware },
  { "experimental.id.protocol_version_inverter", 0, 0, "UPS.EVER1.EVER16", NULL, "%s", 0, ever_format_version },
  { "experimental.id.protocol_version_interfaces", 0, 0, "UPS.EVER1.EVER17", NULL, "%s", 0, ever_format_version },

  /* WAS: "experimental.inverter_info.heatsink_temperature" */
  { "ups.temperature", 0, 0, "UPS.EVER1.EVER42", NULL, "%s", 0, kelvin_celsius_conversion },
  /* WAS: "experimental.inverter_info.battery_temperature" */
  { "battery.temperature", 0, 0, "UPS.EVER1.EVER43", NULL, "%s", 0, kelvin_celsius_conversion },
  /* WAS: "experimental.ups_info.output_powerfactor" */
  { "output.powerfactor", 0, 0, "UPS.EVER1.EVER44", NULL, "%.0f", 0, NULL },

  /* experimental: Should these be HU_TYPE_CMD entries?
   * Or are they really settings? */
  { "experimental.control.ups_on", ST_FLAG_RW | ST_FLAG_NUMBER, 0, "UPS.EVER1.EVER45.EVER46", NULL, "%.0f", 0, NULL },
  { "experimental.control.clear_fault", ST_FLAG_RW | ST_FLAG_NUMBER, 0, "UPS.EVER1.EVER45.EVER47", NULL, "%.0f", 0, NULL },
  { "experimental.control.clear_battery_fault", ST_FLAG_RW | ST_FLAG_NUMBER, 0, "UPS.EVER1.EVER45.EVER48", NULL, "%.0f", 0, NULL },
  { "experimental.control.epo_blocked", ST_FLAG_RW | ST_FLAG_NUMBER, 0, "UPS.EVER1.EVER49.EVER50", NULL, "%.0f", 0, NULL },
  { "experimental.control.green_mode", ST_FLAG_RW | ST_FLAG_NUMBER, 0, "UPS.EVER1.EVER49.EVER51", NULL, "%.0f", 0, NULL },
  { "experimental.control.button_sound", ST_FLAG_RW | ST_FLAG_NUMBER, 0, "UPS.EVER1.EVER49.EVER52", NULL, "%.0f", 0, NULL },
  { "experimental.control.audible_alarm", ST_FLAG_RW | ST_FLAG_NUMBER, 0, "UPS.EVER1.EVER49.EVER53", NULL, "%.0f", 0, NULL },

  /* WAS: "experimental.config.output_voltage" */
  { "output.voltage", ST_FLAG_RW | ST_FLAG_STRING, 3, "UPS.EVER1.EVER54", NULL, "%.0f", 0, NULL },

  /* not implemented*/
  /*
  { "experimental.config.min_output_voltage", ST_FLAG_RW | ST_FLAG_STRING, 3, "UPS.EVER1.EVER55", NULL, "%.0f", 0, NULL },
  { "experimental.config.max_output_voltage", ST_FLAG_RW | ST_FLAG_STRING, 3, "UPS.EVER1.EVER56", NULL, "%.0f", 0, NULL },
  { "experimental.config.min_output_frequency", ST_FLAG_RW | ST_FLAG_NUMBER, 0, "UPS.EVER1.EVER57", NULL, "%.1f", 0, NULL },
  { "experimental.config.max_output_frequency", ST_FLAG_RW | ST_FLAG_NUMBER, 0, "UPS.EVER1.EVER58", NULL, "%.1f", 0, NULL },
  */
  /* experimental: what units is this counted in?
   * is "ups.load.high" a suitable mapping here? or "battery.voltage.high"?
   */
  { "experimental.config.overload_clearance_threshold", ST_FLAG_RW | ST_FLAG_STRING, 2, "UPS.EVER1.EVER59", NULL, "%.0f", 0, NULL },
  { "experimental.config.stb_charge", ST_FLAG_RW | ST_FLAG_STRING, 2, "UPS.EVER1.EVER60", NULL, "%.0f", 0, NULL },
  /* WAS: "experimental.config.number_of_ebms"
   * Should this be a string? rw?
   */
  { "battery.packs.external", ST_FLAG_RW | ST_FLAG_STRING, 1, "UPS.EVER1.EVER61", NULL, "%.0f", 0, NULL },

  { "experimental.statistics.mains_loss_counter", 0, 0, "UPS.EVER1.EVER62", NULL, "%.0f", 0, NULL },
  { "experimental.statistics.lowering_AVR_trigger_counter", 0, 0, "UPS.EVER1.EVER63", NULL, "%.0f", 0, NULL },
  { "experimental.statistics.rising_AVR_trigger_counter", 0, 0, "UPS.EVER1.EVER64", NULL, "%.0f", 0, NULL },
  { "experimental.statistics.overload_counter", 0, 0, "UPS.EVER1.EVER65", NULL, "%.0f", 0, NULL },
  { "experimental.statistics.short_circuit_counter", 0, 0, "UPS.EVER1.EVER66", NULL, "%.0f", 0, NULL },
  { "experimental.statistics.discharge_counter", 0, 0, "UPS.EVER1.EVER67", NULL, "%.0f", 0, NULL },
  { "experimental.statistics.overheat_counter", 0, 0, "UPS.EVER1.EVER68", NULL, "%.0f", 0, NULL },
  { "experimental.statistics.mains_operation_time", 0, 0, "UPS.EVER1.EVER69", NULL, "%.0f", 0, NULL },
  { "experimental.statistics.autonomous_operation_time", 0, 0, "UPS.EVER1.EVER70", NULL, "%.0f", 0, NULL },
  { "experimental.statistics.overload_operation_time", 0, 0, "UPS.EVER1.EVER71", NULL, "%.0f", 0, NULL },

  { "experimental.networkcard.mac_address", 0, 0, "UPS.EVER1.EVER72", NULL, "%s", 0, ever_mac_address },
  { "experimental.networkcard.notification_destination_ip", 0, 0, "UPS.EVER1.EVER73", NULL, "%s", 0, ever_ip_address },
  { "experimental.networkcard.send_packets", 0, 0, "UPS.EVER1.EVER77", NULL, "%s", 0, ever_packets },
  { "experimental.networkcard.received_packets", 0, 0, "UPS.EVER1.EVER78", NULL, "%s", 0, ever_packets },
  { "experimental.networkcard.send_packets_err", 0, 0, "UPS.EVER1.EVER79", NULL, "%s", 0, ever_packets },
  { "experimental.networkcard.received_packets_err", 0, 0, "UPS.EVER1.EVER80", NULL, "%s", 0, ever_packets },
  { "experimental.networkcard.config_dhcp_enabled", 0, 0, "UPS.EVER1.EVER85.EVER86", NULL, "%.0f", 0, NULL },
  { "experimental.networkcard.config_ethernet_enabled", 0, 0, "UPS.EVER1.EVER85.EVER87", NULL, "%.0f", 0, NULL },
  { "experimental.networkcard.config_http_enabled", 0, 0, "UPS.EVER1.EVER85.EVER88", NULL, "%.0f", 0, NULL },
  { "experimental.networkcard.config_snmp_enabled", 0, 0, "UPS.EVER1.EVER85.EVER89", NULL, "%.0f", 0, NULL },
  { "experimental.networkcard.config_snmp_trap_enabled", 0, 0, "UPS.EVER1.EVER85.EVER90", NULL, "%.0f", 0, NULL },
  { "experimental.networkcard.config_readonly", 0, 0, "UPS.EVER1.EVER85.EVER91", NULL, "%.0f", 0, NULL },
  { "experimental.networkcard.config_restart_eth", 0, 0, "UPS.EVER1.EVER85.EVER96", NULL, "%.0f", 0, NULL },
  { "experimental.networkcard.ip_address", 0, 0, "UPS.EVER1.EVER93", NULL, "%s", 0, ever_ip_address },
  { "experimental.networkcard.network_mask", 0, 0, "UPS.EVER1.EVER94", NULL, "%s", 0, ever_ip_address },
  { "experimental.networkcard.default_gateway", 0, 0, "UPS.EVER1.EVER95", NULL, "%s", 0, ever_ip_address },

  /* WAS: "experimental.id.config_active_power" */
  { "ups.realpower.nominal", 0, 0, "UPS.Flow.ConfigActivePower", NULL, "%.0f", 0, NULL },
  /* WAS: "experimental.id.config_apparent_power"
   * Other HID subdrivers use "ups.power.nominal" mostly (often HU_FLAG_STATIC);
   * once of each: "ups.realpower.nominal", "ups.realpower".
   * Is this even a run-time value or a hardware property?
   */
  { "ups.power.nominal", 0, 0, "UPS.Flow.ConfigApparentPower", NULL, "%.0f", 0, NULL },

  /* WAS: "experimental.ups.config_frequency"
   * Here and next: is this about input or output?..
   * Other drivers have "input.frequency.nominal" on numbered Flows
   * As a "nominal", should it be HU_FLAG_SEMI_STATIC or HU_FLAG_STATIC maybe?
   * Note there are non-nominal values in "powerconverter" below,
   * so the questions here may be somewhat irrelevant...
   */
  { "output.frequency.nominal", 0, 0, "UPS.Flow.ConfigFrequency", NULL, "%.0f", 0, NULL },
  /* WAS: "experimental.ups.config_voltage" */
  { "output.voltage.nominal", 0, 0, "UPS.Flow.ConfigVoltage", NULL, "%.0f", 0, NULL },
  { "experimental.ups.flow_id", 0, 0, "UPS.Flow.FlowID", NULL, "%.0f", 0, NULL },

  /* NOTE: NUT variable names define "outlet.n.*" names for numbering all
   * separately manageable outlets; the numberless value (or outlet.0.*)
   * is reserved to represent common properties of all outlets, if there
   * are more than one outlet (group).
   * Mapping below arbitrarily assigns n=1 but really this should be tied
   * to actual outlet counts (see %i mappings in other drivers).
   */
  /* WAS: experimental.outlet.outlet_id */
  { "outlet.1.id", 0, 0, "UPS.OutletSystem.Outlet.OutletID", NULL, "%.0f", 0, NULL },
  /* WAS:  */
  { "experimental.outlet.1.present", 0, 0, "UPS.OutletSystem.Outlet.PresentStatus.Present", NULL, "%.0f", 0, yes_no_info },
  { "outlet.1.switchable", 0, 0, "UPS.OutletSystem.Outlet.PresentStatus.Switchable", NULL, "%.0f", 0, yes_no_info },
  /* WAS: experimental.outlet.switch_on_off  */
  { "outlet.1.status", 0, 0, "UPS.OutletSystem.Outlet.PresentStatus.SwitchOn/Off", NULL, "%.0f", 0, NULL },
  { "experimental.outlet.1.undefined", 0, 0, "UPS.OutletSystem.Outlet.PresentStatus.Undefined", NULL, "%.0f", 0, NULL },
  { "experimental.outlet.1.system_id", 0, 0, "UPS.OutletSystem.OutletSystemID", NULL, "%.0f", 0, NULL },
  /* experimental: Should these be HU_TYPE_CMD entries?
   * Or are they really settings? */
  { "experimental.outlet.1.switch_off_control", ST_FLAG_RW | ST_FLAG_NUMBER, 0, "UPS.OutletSystem.Outlet.SwitchOffControl", NULL, "%.0f", 0, NULL },
  { "experimental.outlet.1.switch_on_control", ST_FLAG_RW | ST_FLAG_NUMBER, 0, "UPS.OutletSystem.Outlet.SwitchOnControl", NULL, "%.0f", 0, NULL },

  { "experimental.powerconverter.input_flow_id", 0, 0, "UPS.PowerConverter.Input.FlowID", NULL, "%.0f", 0, NULL },
  /* WAS: experimental.powerconverter.input_frequency */
  { "input.frequency", 0, 0, "UPS.PowerConverter.Input.Frequency", NULL, "%.0f", 0, NULL },
  { "experimental.powerconverter.input_id", 0, 0, "UPS.PowerConverter.Input.InputID", NULL, "%.0f", 0, NULL },
  /* WAS: experimental.powerconverter.input_voltage */
  { "input.voltage", 0, 0, "UPS.PowerConverter.Input.Voltage", NULL, "%.0f", 0, NULL },
  /* WAS: experimental.powerconverter.output_active_power */
  { "ups.realpower", 0, 0, "UPS.PowerConverter.Output.ActivePower", NULL, "%.0f", 0, NULL },
  /* WAS: experimental.powerconverter.output_apparent_power */
  { "ups.power", 0, 0, "UPS.PowerConverter.Output.ApparentPower", NULL, "%.0f", 0, NULL },
  /* WAS: experimental.powerconverter.output_current */
  { "output.current", 0, 0, "UPS.PowerConverter.Output.Current", NULL, "%.0f", 0, NULL },
  { "experimental.powerconverter.output_flowid", 0, 0, "UPS.PowerConverter.Output.FlowID", NULL, "%.0f", 0, NULL },
  /* WAS: experimental.powerconverter.output_frequency */
  { "output.frequency", 0, 0, "UPS.PowerConverter.Output.Frequency", NULL, "%.0f", 0, NULL },
  { "experimental.powerconverter.output_id", 0, 0, "UPS.PowerConverter.Output.OutputID", NULL, "%.0f", 0, NULL },
  /* WAS: experimental.powerconverter.output_percent_load
   * Note: several original readings map into "ups.load", first served wins
   */
  { "ups.load", 0, 0, "UPS.PowerConverter.Output.PercentLoad", NULL, "%.0f", 0, NULL },
  /* WAS: experimental.powerconverter.output_voltage */
  { "output.voltage", 0, 0, "UPS.PowerConverter.Output.Voltage", NULL, "%.0f", 0, NULL },
  { "experimental.powerconverter.powerconverterid", 0, 0, "UPS.PowerConverter.PowerConverterID", NULL, "%.0f", 0, NULL },
  { "experimental.powersummary.capacity_granularity_1", 0, 0, "UPS.PowerSummary.CapacityGranularity1", NULL, "%.0f", 0, NULL },
  { "experimental.powersummary.capacity_granularity_2", 0, 0, "UPS.PowerSummary.CapacityGranularity2", NULL, "%.0f", 0, NULL },
  /* WAS:  */
  { "experimental.powersummary.capacity_mode", 0, 0, "UPS.PowerSummary.CapacityMode", NULL, "%.0f", 0, NULL },
  /* WAS: experimental.powersummary.delay_before_shutdown */
  { "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.PowerSummary.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_FLAG_ABSENT, NULL },
  /* WAS: experimental.powersummary.design_capacity */
  { "battery.capacity", 0, 0, "UPS.PowerSummary.DesignCapacity", NULL, "%.0f", 0, NULL },
  { "experimental.powersummary.flow_id", 0, 0, "UPS.PowerSummary.FlowID", NULL, "%.0f", 0, NULL },
  /* WAS: experimental.powersummary.full_charge_capacity */
  { "battery.capacity", 0, 0, "UPS.PowerSummary.FullChargeCapacity", NULL, "%.0f", 0, NULL },
  /* WAS: experimental.powersummary.idevice_chemistry */
  { "battery.type", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%.0f", 0, NULL },
  /* WAS: experimental.powersummary.percent_load
   * Note: several original readings map into "ups.load", first served wins
   */
  { "ups.load", 0, 0, "UPS.PowerSummary.PercentLoad", NULL, "%.0f", 0, NULL },
  { "experimental.powersummary.rechargeable", 0, 0, "UPS.PowerSummary.Rechargeable", NULL, "%.0f", 0, NULL },
  /* WAS: experimental.powersummary.remaining_capacity */
  { "battery.charge", 0, 0, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", 0, NULL },
  /* WAS: experimental.powersummary.remaining_time_limit */
  { "battery.runtime.low", ST_FLAG_RW | ST_FLAG_NUMBER, 0, "UPS.PowerSummary.RemainingTimeLimit", NULL, "%.0f", 0, NULL },
  /* WAS: experimental.powersummary.run_time_to_empty */
  { "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", 0, NULL },
  /* WAS: experimental.powersummary.voltage */
  { "battery.voltage", 0, 0, "UPS.PowerSummary.Voltage", NULL, "%.0f", 0, NULL },
  /* WAS: experimental.powersummary.delay_before_shutdown */
  { "ups.timer.shutdown", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL },

#if WITH_UNMAPPED_DATA_POINTS
  /* not implemented*/
  { "unmapped.ups.powersummary.powersummaryid", 0, 0, "UPS.PowerSummary.PowerSummaryID", NULL, "%.0f", 0, NULL },
#endif	/* if WITH_UNMAPPED_DATA_POINTS */
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, NULL, HU_FLAG_QUICK_POLL, online_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.AwaitingPower", NULL, NULL, HU_FLAG_QUICK_POLL, awaitingpower_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.BatteryPresent", NULL, NULL, 0, nobattery_info },

  /* not implemented*/
  /* { "experimental.ups.presentstatus.belowremainingcapacitylimit", 0, 0, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL, "%.0f", 0, NULL }, */
  /* { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Boost", NULL, NULL, 0, boost_info }, */
  /* { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Buck", NULL, NULL, 0, trim_info }, */
  /* { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Charging",  NULL, NULL, HU_FLAG_QUICK_POLL, charging_info }, */
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.CommunicationLost", NULL, NULL, 0, commfault_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Discharging", NULL, NULL, HU_FLAG_QUICK_POLL, discharging_info },
  /* not implemented*/
  /* { "experimental.ups.powersummary.presentstatus.good", 0, 0, "UPS.PowerSummary.PresentStatus.Good", NULL, "%.0f", 0, NULL }, */
  /* { "experimental.ups.powersummary.presentstatus.internalfailure", 0, 0, "UPS.PowerSummary.PresentStatus.InternalFailure", NULL, "%.0f", 0, NULL }, */
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.NeedReplacement", NULL, NULL, 0, replacebatt_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Overload", NULL, NULL, HU_FLAG_QUICK_POLL, overload_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.OverTemperature", NULL, NULL, 0, overheat_info },
  /* not implemented*/
  /* { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.RemainingTimeLimitExpired", NULL, NULL, 0, lowbatt_info }, */
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ShutdownImminent", NULL, NULL, 0, shutdownimm_info },

  { "BOOL", 0, 0, "UPS.EVER1.EVER18.EVER19", NULL, NULL, 0, overload_info },
  { "BOOL", 0, 0, "UPS.EVER1.EVER18.EVER20", NULL, NULL, 0, lowbatt_info },
  { "BOOL", 0, 0, "UPS.EVER1.EVER18.EVER21", NULL, NULL, 0, boost_info },
  { "BOOL", 0, 0, "UPS.EVER1.EVER18.EVER22", NULL, NULL, 0, trim_info },
  { "BOOL", 0, 0, "UPS.EVER1.EVER18.EVER25", NULL, NULL, 0, charging_info },
  { "BOOL", 0, 0, "UPS.EVER1.EVER18.EVER28", NULL, NULL, 0, replacebatt_info },
  { "BOOL", 0, 0, "UPS.EVER1.EVER32.EVER33", NULL, NULL, 0, overload_info },
  { "BOOL", 0, 0, "UPS.EVER1.EVER32.EVER40", NULL, "%.0f", 0, commfault_info },
  { "BOOL", 0, 0, "UPS.EVER1.EVER97.EVER102", NULL, "%s", 0, ever_on_off_info },

  /* ever workmodes, messages & alarms */
  { "experimental.status.workmode", 0, 0, "UPS.EVER1.EVER97.EVER98", NULL, "%s", 0, ever_workmode },
  { "experimental.status.messages", 0, 0, "UPS.EVER1.EVER18.EVER28", NULL, NULL, 0, ever_messages },
  { "experimental.status.alarms", 0, 0, "UPS.EVER1.EVER32.EVER33", NULL, NULL, 0, ever_alarms },

  /* instant commands */
  /* experimental: With the same fields here, are the commands different?
   * Per NUT command names, should be: documented load.off stays off, like
   * shutdown.stayoff, but shutdown.return may return if wall power comes back!
   * In many drivers, similar command with "-1" instead of DEFAULT_OFFDELAY
   * serves as a shutdown.stop (to abort a pending shutdown).
   */
  { "load.off.delay", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_TYPE_CMD, NULL },
  { "shutdown.return", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_TYPE_CMD, NULL },

  /* end of structure. */
  { NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

static const char *ever_format_model(HIDDevice_t *hd) {
	return hd->Product;
}

static const char *ever_format_mfr(HIDDevice_t *hd) {
	return hd->Vendor ? hd->Vendor : "Ever";
}

static const char *ever_format_serial(HIDDevice_t *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if the device is supported by this subdriver, else 0. */
static int ever_claim(HIDDevice_t *hd)
{
	int status = is_usb_device_supported(ever_usb_device_table, hd);

	switch (status)
	{
		case POSSIBLY_SUPPORTED:
			/* by default, reject, unless the productid option is given */
			if (getval("productid")) {
				return 1;
			}
			possibly_supported("Ever", hd);
			return 0;

		case SUPPORTED:
			return 1;

		case NOT_SUPPORTED:
		default:
			return 0;
	}
}

subdriver_t ever_subdriver = {
	EVER_HID_VERSION,
	ever_claim,
	ever_utab,
	ever_hid2nut,
	ever_format_model,
	ever_format_mfr,
	ever_format_serial,
	fix_report_desc,
};
