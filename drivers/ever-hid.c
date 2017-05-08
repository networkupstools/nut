/* ever-hid.c - subdriver to monitor EVER USB/HID devices with NUT
 *
 *  Copyright (C)
 *  2003 - 2012	Arnaud Quette <ArnaudQuette@Eaton.com>
 *  2005 - 2006	Peter Selinger <selinger@users.sourceforge.net>
 *  2008 - 2009	Arjen de Korte <adkorte-guest@alioth.debian.org>
 *  2013 Charles Lepple <clepple+nut@gmail.com>
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

#include "usbhid-ups.h"
#include "ever-hid.h"
#include "main.h"	/* for getval() */
#include "usb-common.h"

#define EVER_HID_VERSION	"Ever HID 0.1"
/* FIXME: experimental flag to be put in upsdrv_info */

/* Ever */
#define EVER_VENDORID	0x2E51

/* USB IDs device table */
static usb_device_id_t ever_usb_device_table[] = {

	{ USB_DEVICE(0x0483, 0xa113), NULL },
	{ USB_DEVICE(EVER_VENDORID, 0xffff), NULL},


	/* Terminating entry */
	{ -1, -1, NULL }
};

/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

static const char *ever_format_hardware_fun(double value)
{
	/*TODO - add exception handling for v1.0b0B */
	const char* hard_rev[27] = {"0", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"};	
	static char model[10];
	snprintf(model, sizeof(model), "rev.%sv%02d", (&hard_rev[ ((unsigned int)value & 0xFF00)>>8 ])[0], (unsigned int)value & 0xFF );  
	return model;
}

static const char *ever_format_version_fun(double value)
{
	/*upsdebugx(1, "UPS ups_firmware_conversion_fun VALUE: %d", (long)value  ); */
	static char model[10];
	snprintf(model, sizeof(model), "v%X.%Xb%02d", ((unsigned int)value & 0xF000)>>12, ((unsigned int)value & 0xF00)>>8, ((int)value & 0xFF) );
	return model;
}

static const char *ever_mac_address_fun(double value)
{

	int mac_adress_report_id = 210;
	int len = reportbuf->len[mac_adress_report_id];
	const void *buf = reportbuf->data[mac_adress_report_id];

	static char line[100];
	line[0] = '\0';
	int n = 0;	/* number of characters currently in line */
	int i;	/* number of bytes output from buffer */

	/* skip first elemnt which is a report id */
	for (i = 1; i < len; i++) {
		n = snprintfcat(line, sizeof(line), n ? ":%02x" : "%02x",
			((unsigned char *)buf)[i]);
	}

	return line;
}

static const char *ever_ip_address_fun(double value)
{
	static int report_counter = 1;
	int report_id = 211;

	if(report_counter == 1) 
		report_id = 211; /* notification dest ip */
	else if(report_counter == 2)
		report_id = 230; /* ip address */
	else if(report_counter == 3)
		report_id = 231;	/* network mask */
	else if(report_counter == 4)
		report_id = 232;	/* default gateway */

	report_counter== 4 ? report_counter=1 : report_counter++; 

	int len = reportbuf->len[report_id];
	const void *buf = reportbuf->data[report_id];

	static char line[100];
	line[0] = '\0';
	int n = 0;	/* number of characters currently in line */
	int i;	/* number of bytes output from buffer */

	/*skip first element which is a report id */
	for (i = 1; i < len; i++) 
	{
		n = snprintfcat(line, sizeof(line), n ? ".%d" : "%d", ((unsigned char *)buf)[i]);
	}

	return line;
}

static const char *ever_packets_fun(double value)
{
	static int report_counter = 1;
	int report_id = 215;

	if(report_counter == 1 )
		report_id = 215;
	else if(report_counter == 2 )
		report_id = 216;
	else if(report_counter == 3 )
		report_id = 217;
	else if(report_counter == 4 )
		report_id = 218;

	report_counter== 4 ? report_counter=1 : report_counter++; 

	int len = reportbuf->len[report_id];
	const unsigned char *buf = reportbuf->data[report_id];

	static char line[100];
	line[0] = '\0';

	/*skip first elemnt which is a report id */

	if(len < 5)
		return "";

	int res = (int)buf[1];
	res |= (int)buf[2] << 8;
	res |= (int)buf[3] << 16;
	res |= (int)buf[4] << 24;

    snprintf(line, sizeof(line), "%d", res);
	return line;

}

static const char* ever_workmode_fun(double value)
{
	int workmode_report_id = 74;
	const unsigned char *buf = reportbuf->data[workmode_report_id];

	static char line[100];
	line[0] = '\0';

	/*skip first element which is a report id */
	snprintfcat(line, sizeof(line), "%d", buf[1]);
	


	int workmode = atoi(line);

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
	int messages_report_id = 75;
	const unsigned char *buf = reportbuf->data[messages_report_id];

	static char line[200];
	line[0] = '\0';

	/*skip first element which is a report id */
	int messages = (int)buf[1];
	messages |= (int)buf[2] << 8;

	int n = 0;	/* number of characters currently in line */

	if(messages & 0x01)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "OVERLOAD");
	if(messages & 0x02)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "BATTERY_LOW");
	if(messages & 0x04)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "BOOST");
	if(messages & 0x08)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "TRIM");
	if(messages & 0x10)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "BOOST_BLOCKED");
	if(messages & 0x20)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "TRIM_BLOCKED");
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
	int alarms_report_id = 76;

	const unsigned char *buf = reportbuf->data[alarms_report_id];

	static char line[200];
	line[0] = '\0';

	/*skip first element which is a report id */
	int alarms = (int)buf[1];
	alarms |= (int)buf[2] << 8;

	int n = 0;	/* number of characters currently in line */

	if(alarms & 0x01)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "OVERLOAD");
	if(alarms & 0x02)
		n = snprintfcat(line, sizeof(line), n ? " %s" : "%s", "SHORTCUT");
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
	int workmode_report_id = 74;
	const unsigned char *buf = reportbuf->data[workmode_report_id];

	static char line[100];
	line[0] = '\0';

	/*skip first element which is a report id */
	snprintfcat(line, sizeof(line), "%d", buf[1]);

	int workmode = atoi(line);	

	if(workmode != 0x04 && workmode != 0x08) 
		return "off";

	return "!off";
}

info_lkp_t ever_format_hardware[] = {
	{ 0, NULL, ever_format_hardware_fun}
};

info_lkp_t ever_format_version[] = {
	{ 0, NULL, ever_format_version_fun}
};

info_lkp_t ever_mac_address[] = {
	{ 0, NULL, ever_mac_address_fun}
};

info_lkp_t ever_ip_address[] = {
	{ 0, NULL, ever_ip_address_fun}
};

info_lkp_t ever_packets[] = {
	{ 0, NULL, ever_packets_fun}
};

info_lkp_t ever_workmode[] = {
	{ 0, NULL, ever_workmode_fun}
};

info_lkp_t ever_messages[] = {
	{ 0, NULL, ever_messages_fun}
};

info_lkp_t ever_alarms[] = {
	{ 0, NULL, ever_alarms_fun}
};

info_lkp_t ever_on_off_info[] = {
	{ 0, NULL, ever_on_off_fun}
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

  { "ups.batterysystem.battery.batteryid", 0, 0, "UPS.BatterySystem.Battery.BatteryID", NULL, "%.0f", 0, NULL },
  { "ups.batterysystem.batterysystemid", 0, 0, "UPS.BatterySystem.BatterySystemID", NULL, "%.0f", 0, NULL },
  { "ups.batterysystem.charger.chargerid", 0, 0, "UPS.BatterySystem.Charger.ChargerID", NULL, "%.0f", 0, NULL },
  { "ups.batterysystem.input.flowid", 0, 0, "UPS.BatterySystem.Input.FlowID", NULL, "%.0f", 0, NULL },
  { "ups.batterysystem.input.inputid", 0, 0, "UPS.BatterySystem.Input.InputID", NULL, "%.0f", 0, NULL },
  { "ups.batterysystem.output.flowid", 0, 0, "UPS.BatterySystem.Output.FlowID", NULL, "%.0f", 0, NULL },
  { "ups.batterysystem.output.outputid", 0, 0, "UPS.BatterySystem.Output.OutputID", NULL, "%.0f", 0, NULL },
  /*{ "ever.identyfication.upstype", 0, 0, "UPS.EVER1.EVER12", NULL, "%s", 0, ever_format_model },*/
  { "ever.identyfication.firmware.inverter.version", 0, 0, "UPS.EVER1.EVER13", NULL, "%s", 0, ever_format_version },
  { "ever.identyfication.firmware.interfaces.version", 0, 0, "UPS.EVER1.EVER14", NULL, "%s", 0, ever_format_version },
  { "ever.identyfication.hardware.version", 0, 0, "UPS.EVER1.EVER15", NULL, "%s", 0, ever_format_hardware },
  { "ever.identyfication.protocol.inverter.version", 0, 0, "UPS.EVER1.EVER16", NULL, "%s", 0, ever_format_version },
  { "ever.identyfication.protocol.interfaces.version", 0, 0, "UPS.EVER1.EVER17", NULL, "%s", 0, ever_format_version },
  { "ever.hitsinktemperature", 0, 0, "UPS.EVER1.EVER42", NULL, "%s", 0, kelvin_celsius_conversion },
  { "ever.batterytemperature", 0, 0, "UPS.EVER1.EVER43", NULL, "%s", 0, kelvin_celsius_conversion },
  { "ever.outputpowerfactor", 0, 0, "UPS.EVER1.EVER44", NULL, "%.0f", 0, NULL },
  { "ever.ups.control.upson", ST_FLAG_RW | ST_FLAG_NUMBER, 0, "UPS.EVER1.EVER45.EVER46", NULL, "%.0f", 0, NULL },
  { "ever.ups.control.clearfault", ST_FLAG_RW | ST_FLAG_NUMBER, 0, "UPS.EVER1.EVER45.EVER47", NULL, "%.0f", 0, NULL },
  { "ever.ups.control.clearbatteryfault", ST_FLAG_RW | ST_FLAG_NUMBER, 0, "UPS.EVER1.EVER45.EVER48", NULL, "%.0f", 0, NULL },
  { "ever.ups.control.epoblocked", ST_FLAG_RW | ST_FLAG_NUMBER, 0, "UPS.EVER1.EVER49.EVER50", NULL, "%.0f", 0, NULL },
  { "ever.ups.control.greenmode", ST_FLAG_RW | ST_FLAG_NUMBER, 0, "UPS.EVER1.EVER49.EVER51", NULL, "%.0f", 0, NULL },
  { "ever.ups.control.buttonsound", ST_FLAG_RW | ST_FLAG_NUMBER, 0, "UPS.EVER1.EVER49.EVER52", NULL, "%.0f", 0, NULL },
  { "ever.ups.control.audiblealarm", ST_FLAG_RW | ST_FLAG_NUMBER, 0, "UPS.EVER1.EVER49.EVER53", NULL, "%.0f", 0, NULL },
  { "ever.ups.config.outputvoltage", ST_FLAG_RW | ST_FLAG_STRING, 3, "UPS.EVER1.EVER54", NULL, "%.0f", 0, NULL },
  /*{ "ever.ups.config.minoutputvoltage", ST_FLAG_RW | ST_FLAG_STRING, 3, "UPS.EVER1.EVER55", NULL, "%.0f", 0, NULL },
  { "ever.ups.config.maxoutputvoltage", ST_FLAG_RW | ST_FLAG_STRING, 3, "UPS.EVER1.EVER56", NULL, "%.0f", 0, NULL },
  { "ever.ups.config.minoutputfrequency", ST_FLAG_RW | ST_FLAG_NUMBER, 0, "UPS.EVER1.EVER57", NULL, "%.1f", 0, NULL },
  { "ever.ups.config.maxoutputfrequency", ST_FLAG_RW | ST_FLAG_NUMBER, 0, "UPS.EVER1.EVER58", NULL, "%.1f", 0, NULL },*/
  { "ever.ups.config.overloadclearancethreshold", ST_FLAG_RW | ST_FLAG_STRING, 2, "UPS.EVER1.EVER59", NULL, "%.0f", 0, NULL },
  { "ever.ups.config.stbcharge", ST_FLAG_RW | ST_FLAG_STRING, 2, "UPS.EVER1.EVER60", NULL, "%.0f", 0, NULL },
  { "ever.ups.config.numberofebms", ST_FLAG_RW | ST_FLAG_STRING, 1, "UPS.EVER1.EVER61", NULL, "%.0f", 0, NULL },
  { "ever.statistics.mainslosscounter", 0, 0, "UPS.EVER1.EVER62", NULL, "%.0f", 0, NULL },
  { "ever.statistics.loweringAVRtriggercounter", 0, 0, "UPS.EVER1.EVER63", NULL, "%.0f", 0, NULL },
  { "ever.statistics.risingAVRtriggercounter", 0, 0, "UPS.EVER1.EVER64", NULL, "%.0f", 0, NULL },
  { "ever.statistics.overloadcounter", 0, 0, "UPS.EVER1.EVER65", NULL, "%.0f", 0, NULL },
  { "ever.statistics.shortcircuitcounter", 0, 0, "UPS.EVER1.EVER66", NULL, "%.0f", 0, NULL },
  { "ever.statistics.dischargecounter", 0, 0, "UPS.EVER1.EVER67", NULL, "%.0f", 0, NULL },
  { "ever.statistics.overheatcounter", 0, 0, "UPS.EVER1.EVER68", NULL, "%.0f", 0, NULL },
  { "ever.statistics.mainsoperationtime", 0, 0, "UPS.EVER1.EVER69", NULL, "%.0f", 0, NULL },
  { "ever.statistics.autonomousoperationtime", 0, 0, "UPS.EVER1.EVER70", NULL, "%.0f", 0, NULL },
  { "ever.statistics.overloadoperationtime", 0, 0, "UPS.EVER1.EVER71", NULL, "%.0f", 0, NULL },
  { "ever.networkcard.macaddress", 0, 0, "UPS.EVER1.EVER72", NULL, "%s", 0, ever_mac_address },
  { "ever.networkcard.notificationdestinationip", 0, 0, "UPS.EVER1.EVER73", NULL, "%s", 0, ever_ip_address },
  { "ever.networkcard.statistics.sendpackets", 0, 0, "UPS.EVER1.EVER77", NULL, "%s", 0, ever_packets },
  { "ever.networkcard.statistics.receivedpackets", 0, 0, "UPS.EVER1.EVER78", NULL, "%s", 0, ever_packets },
  { "ever.networkcard.statistics.sendpacketserrors", 0, 0, "UPS.EVER1.EVER79", NULL, "%s", 0, ever_packets },
  { "ever.networkcard.statistics.receivedpacketserrors", 0, 0, "UPS.EVER1.EVER80", NULL, "%s", 0, ever_packets },
  { "ever.networkcard.config.dhcpenabled", 0, 0, "UPS.EVER1.EVER85.EVER86", NULL, "%.0f", 0, NULL },
  { "ever.networkcard.config.ethernetenabled", 0, 0, "UPS.EVER1.EVER85.EVER87", NULL, "%.0f", 0, NULL },
  { "ever.networkcard.config.httpenabled", 0, 0, "UPS.EVER1.EVER85.EVER88", NULL, "%.0f", 0, NULL },
  { "ever.networkcard.config.snmpenabled", 0, 0, "UPS.EVER1.EVER85.EVER89", NULL, "%.0f", 0, NULL },
  { "ever.networkcard.config.snmptrapenabled", 0, 0, "UPS.EVER1.EVER85.EVER90", NULL, "%.0f", 0, NULL },
  { "ever.networkcard.config.readonly", 0, 0, "UPS.EVER1.EVER85.EVER91", NULL, "%.0f", 0, NULL },
  { "ever.networkcard.config.restarteth", 0, 0, "UPS.EVER1.EVER85.EVER96", NULL, "%.0f", 0, NULL },
  { "ever.networkcard.ipaddress", 0, 0, "UPS.EVER1.EVER93", NULL, "%s", 0, ever_ip_address },
  { "ever.networkcard.networkmask", 0, 0, "UPS.EVER1.EVER94", NULL, "%s", 0, ever_ip_address },
  { "ever.networkcard.defaultgateway", 0, 0, "UPS.EVER1.EVER95", NULL, "%s", 0, ever_ip_address },
  { "ups.flow.configactivepower", 0, 0, "UPS.Flow.ConfigActivePower", NULL, "%.0f", 0, NULL },
  { "ups.flow.configapparentpower", 0, 0, "UPS.Flow.ConfigApparentPower", NULL, "%.0f", 0, NULL },
  { "ups.flow.configfrequency", 0, 0, "UPS.Flow.ConfigFrequency", NULL, "%.0f", 0, NULL },
  { "ups.flow.configvoltage", 0, 0, "UPS.Flow.ConfigVoltage", NULL, "%.0f", 0, NULL },
  { "ups.flow.flowid", 0, 0, "UPS.Flow.FlowID", NULL, "%.0f", 0, NULL },
  { "ups.outletsystem.outlet.outletid", 0, 0, "UPS.OutletSystem.Outlet.OutletID", NULL, "%.0f", 0, NULL },
  { "ups.outletsystem.outlet.presentstatus.present", 0, 0, "UPS.OutletSystem.Outlet.PresentStatus.Present", NULL, "%.0f", 0, yes_no_info },
  { "ups.outletsystem.outlet.presentstatus.switchable", 0, 0, "UPS.OutletSystem.Outlet.PresentStatus.Switchable", NULL, "%.0f", 0, yes_no_info },
  { "ups.outletsystem.outlet.presentstatus.switchon/off", 0, 0, "UPS.OutletSystem.Outlet.PresentStatus.SwitchOn/Off", NULL, "%.0f", 0, NULL },
  { "ups.outletsystem.outlet.presentstatus.undefined", 0, 0, "UPS.OutletSystem.Outlet.PresentStatus.Undefined", NULL, "%.0f", 0, NULL },
  { "ups.outletsystem.outlet.switchoffcontrol", ST_FLAG_RW | ST_FLAG_NUMBER, 0, "UPS.OutletSystem.Outlet.SwitchOffControl", NULL, "%.0f", 0, NULL },
  { "ups.outletsystem.outlet.switchoncontrol", ST_FLAG_RW | ST_FLAG_NUMBER, 0, "UPS.OutletSystem.Outlet.SwitchOnControl", NULL, "%.0f", 0, NULL },
  { "ups.outletsystem.outletsystemid", 0, 0, "UPS.OutletSystem.OutletSystemID", NULL, "%.0f", 0, NULL },
  { "ups.powerconverter.input.flowid", 0, 0, "UPS.PowerConverter.Input.FlowID", NULL, "%.0f", 0, NULL },
  { "ups.powerconverter.input.frequency", 0, 0, "UPS.PowerConverter.Input.Frequency", NULL, "%.0f", 0, NULL },
  { "ups.powerconverter.input.inputid", 0, 0, "UPS.PowerConverter.Input.InputID", NULL, "%.0f", 0, NULL },
  { "ups.powerconverter.input.voltage", 0, 0, "UPS.PowerConverter.Input.Voltage", NULL, "%.0f", 0, NULL },
  { "ups.powerconverter.output.activepower", 0, 0, "UPS.PowerConverter.Output.ActivePower", NULL, "%.0f", 0, NULL },
  { "ups.powerconverter.output.apparentpower", 0, 0, "UPS.PowerConverter.Output.ApparentPower", NULL, "%.0f", 0, NULL },
  { "ups.powerconverter.output.current", 0, 0, "UPS.PowerConverter.Output.Current", NULL, "%.0f", 0, NULL },
  { "ups.powerconverter.output.flowid", 0, 0, "UPS.PowerConverter.Output.FlowID", NULL, "%.0f", 0, NULL },
  { "ups.powerconverter.output.frequency", 0, 0, "UPS.PowerConverter.Output.Frequency", NULL, "%.0f", 0, NULL },
  { "ups.powerconverter.output.outputid", 0, 0, "UPS.PowerConverter.Output.OutputID", NULL, "%.0f", 0, NULL },
  { "ups.powerconverter.output.percentload", 0, 0, "UPS.PowerConverter.Output.PercentLoad", NULL, "%.0f", 0, NULL },
  { "ups.powerconverter.output.voltage", 0, 0, "UPS.PowerConverter.Output.Voltage", NULL, "%.0f", 0, NULL },
  { "ups.powerconverter.powerconverterid", 0, 0, "UPS.PowerConverter.PowerConverterID", NULL, "%.0f", 0, NULL },
  { "ups.powersummary.capacitygranularity1", 0, 0, "UPS.PowerSummary.CapacityGranularity1", NULL, "%.0f", 0, NULL },
  { "ups.powersummary.capacitygranularity2", 0, 0, "UPS.PowerSummary.CapacityGranularity2", NULL, "%.0f", 0, NULL },
  { "ups.powersummary.capacitymode", 0, 0, "UPS.PowerSummary.CapacityMode", NULL, "%.0f", 0, NULL },
  { "ups.powersummary.delaybeforeshutdown", ST_FLAG_RW | ST_FLAG_STRING, 10, "UPS.PowerSummary.DelayBeforeShutdown", NULL, DEFAULT_OFFDELAY, HU_FLAG_ABSENT, NULL },
  { "ups.powersummary.designcapacity", 0, 0, "UPS.PowerSummary.DesignCapacity", NULL, "%.0f", 0, NULL },
  { "ups.powersummary.flowid", 0, 0, "UPS.PowerSummary.FlowID", NULL, "%.0f", 0, NULL },
  { "ups.powersummary.fullchargecapacity", 0, 0, "UPS.PowerSummary.FullChargeCapacity", NULL, "%.0f", 0, NULL },
  { "ups.powersummary.idevicechemistry", 0, 0, "UPS.PowerSummary.iDeviceChemistry", NULL, "%.0f", 0, NULL },
  { "ups.powersummary.percentload", 0, 0, "UPS.PowerSummary.PercentLoad", NULL, "%.0f", 0, NULL },
  { "ups.powersummary.rechargeable", 0, 0, "UPS.PowerSummary.Rechargeable", NULL, "%.0f", 0, NULL },
  { "ups.powersummary.remainingcapacity", 0, 0, "UPS.PowerSummary.RemainingCapacity", NULL, "%.0f", 0, NULL },
  { "ups.powersummary.remainingtimelimit", ST_FLAG_RW | ST_FLAG_NUMBER, 0, "UPS.PowerSummary.RemainingTimeLimit", NULL, "%.0f", 0, NULL },
  { "ups.powersummary.runtimetoempty", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", NULL, "%.0f", 0, NULL },
  { "ups.powersummary.voltage", 0, 0, "UPS.PowerSummary.Voltage", NULL, "%.0f", 0, NULL },
  { "ups.timer.shutdown", 0, 0, "UPS.PowerSummary.DelayBeforeShutdown", NULL, "%.0f", HU_FLAG_QUICK_POLL, NULL },
  /*{ "unmapped.ups.powersummary.powersummaryid", 0, 0, "UPS.PowerSummary.PowerSummaryID", NULL, "%.0f", 0, NULL },*/
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.ACPresent", NULL, NULL, HU_FLAG_QUICK_POLL, online_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.AwaitingPower", NULL, NULL, HU_FLAG_QUICK_POLL, awaitingpower_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.BatteryPresent", NULL, NULL, 0, nobattery_info },
  /* { "ups.presentstatus.belowremainingcapacitylimit", 0, 0, "UPS.PowerSummary.PresentStatus.BelowRemainingCapacityLimit", NULL, "%.0f", 0, NULL }, */
  /*{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Boost", NULL, NULL, 0, boost_info }, */
  /*{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Buck", NULL, NULL, 0, trim_info }, */
  /*{ "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Charging",  NULL, NULL, HU_FLAG_QUICK_POLL, charging_info },*/
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.CommunicationLost", NULL, NULL, 0, commfault_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Discharging", NULL, NULL, HU_FLAG_QUICK_POLL, discharging_info },
  /* { "ups.powersummary.presentstatus.good", 0, 0, "UPS.PowerSummary.PresentStatus.Good", NULL, "%.0f", 0, NULL }, */
  /* { "ups.powersummary.presentstatus.internalfailure", 0, 0, "UPS.PowerSummary.PresentStatus.InternalFailure", NULL, "%.0f", 0, NULL }, */
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.NeedReplacement", NULL, NULL, 0, replacebatt_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.Overload", NULL, NULL, HU_FLAG_QUICK_POLL, overload_info },
  { "BOOL", 0, 0, "UPS.PowerSummary.PresentStatus.OverTemperature", NULL, NULL, 0, overheat_info },
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
  { "ever.ups.status.workmode", 0, 0, "UPS.EVER1.EVER97.EVER98", NULL, "%s", 0, ever_workmode },
  { "ever.ups.status.messages", 0, 0, "UPS.EVER1.EVER18.EVER28", NULL, NULL, 0, ever_messages }, 
  { "ever.ups.status.alarms", 0, 0, "UPS.EVER1.EVER32.EVER33", NULL, NULL, 0, ever_alarms }, 

  /* instant commands */
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
};
