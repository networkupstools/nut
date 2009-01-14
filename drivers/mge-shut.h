/*  mge-shut.h - monitor MGE UPS for NUT with SHUT protocol
 * 
 *  Copyright (C) 2002 - 2005
 *     Arnaud Quette <arnaud.quette@free.fr> & <arnaud.quette@mgeups.com>
 *     Philippe Marzouk <philm@users.sourceforge.net>
 *
 *  Sponsored by MGE UPS SYSTEMS <http://opensource.mgeups.com/>
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
 *
 */

#include "hidparser.h"
#include "hidtypes.h"

#define DEFAULT_TIMEOUT 	3000
#define MAX_STRING      	64

#define DEFAULT_LOWBATT		30	/* low battery level, in %          */
#define DEFAULT_ONDELAY		3	/* delay between return of utility power
					   and powering up of load, in 10 seconds units */
#define DEFAULT_OFFDELAY	20	/* delay befor power off, in seconds */ 
#define OFF_NOTIFICATION	1	/* notification off */

#define LIGHT_NOTIFICATION	2	/* light notification */
#define COMPLETE_NOTIFICATION	3	/* complete notification for UPSs which do
					 * not support disabling it like some early
					 * Ellipse models */
#define DEFAULT_NOTIFICATION COMPLETE_NOTIFICATION

/* HID definitions */

#define HID_REPORT_TYPE_INPUT	0x01
#define HID_REPORT_TYPE_OUTPUT	0x02
#define HID_REPORT_TYPE_FEATURE	0x03

#define REQUEST_TYPE_USB        0x80
#define REQUEST_TYPE_HID        0x81
#define REQUEST_TYPE_GET_REPORT 0xa1
#define REQUEST_TYPE_SET_REPORT 0x21

#define DEVICE_DESCRIPTOR       0x0001
#define CONFIG_DESCRIPTOR       0x0002
#define STRING_DESCRIPTOR       0x0003
#define INTERFACE_DESCRIPTOR    0x0004
#define ENDPOINT_DESCRIPTOR     0x0005
#define HID_DESCRIPTOR          0x0021
#define REPORT_DESCRIPTOR       0x0022

#define MAX_REPORT_SIZE         0x1800

/* SHUT definitions - From Simplified SHUT spec */

#define SHUT_TYPE_REQUEST       0x01
#define SHUT_TYPE_RESPONSE      0x04
#define SHUT_TYPE_NOTIFY        0x05
#define SHUT_OK                 0x06
#define SHUT_NOK                0x15
#define SHUT_SYNC               0x16 /* complete notifications - not yet managed
				      but needed for some early Ellipse models */
#define SHUT_SYNC_LIGHT         0x17 /* partial notifications */
#define SHUT_SYNC_OFF           0x18 /* disable notifications - only do polling */
#define SHUT_PKT_LAST           0x80

/* From SHUT specifications */

typedef struct hid_packet {
  unsigned char  bmRequestType;
  unsigned char  bRequest;
  unsigned short wValue;
  unsigned short wIndex;
  unsigned short wLength;
/*   unsigned char  padding[8]; for use with shut_set_report */
} hid_packet_t;

typedef union hid_data_u {
  hid_packet_t  hid_pkt;
  unsigned char raw_pkt[8]; /* max report lengh, was 8 */
} hid_data_t;

typedef struct shut_packet {
  unsigned char bType;
  unsigned char bLength;
  hid_data_t    data;
  unsigned char bChecksum;
} shut_packet_t;

typedef union shut_data_u {
  shut_packet_t shut_pkt;
  unsigned char raw_pkt[11];
} shut_data_t;

/* From USB/HID specifications */

typedef struct hid_descriptor {
  unsigned char  bLength;
  unsigned char  bDescriptorType;
  unsigned short bcdHID;
  unsigned char  bCountryCode;
  unsigned char  bNumDescriptors;
  unsigned char  bReportDescriptorType;
  unsigned short wDescriptorLength;
} hid_descriptor_t;

typedef union hid_desc_data_u {
  hid_descriptor_t hid_desc;
  unsigned char    raw_desc[9]; /* max report lengh, aws 9 */
} hid_desc_data_t;

typedef struct device_descriptor {
  unsigned char  bLength;
  unsigned char  bDescriptorType;
  unsigned short bcdUSB;
  unsigned char  bDeviceClass;
  unsigned char  bDeviceSubClass;
  unsigned char  bDeviceProtocol;
  unsigned char  bMaxPacketSize0;
  unsigned short idVendor;
  unsigned short idProduct;
  unsigned short bcdDevice;
  unsigned char  iManufacturer;
  unsigned char  iProduct;
  unsigned char  iSerialNumber;
  unsigned char  bNumConfigurations;
} device_descriptor_t;

typedef union device_desc_data_u {
  device_descriptor_t dev_desc;
  unsigned char       raw_desc[18];
} device_desc_data_t;

/* --------------------------------------------------------------- */
/*                       Explicit Booleans                         */
/* --------------------------------------------------------------- */

#define SHUT_FLAG_OK	 (1 << 0)	/* show element to upsd. */
#define SHUT_FLAG_STATIC (1 << 1)	/* retrieve info only once. */
#define SHUT_FLAG_ABSENT (1 << 2)	/* data is absent in the device,
					   use default value. */
#define SHUT_FLAG_STALE	 (1 << 3)	/* data stale, don't try too often. */
#define SHUT_FLAG_DELAY	 (1 << 4)	/* delay type value: formated differently. */

/* --------------------------------------------------------------- */
/*      Model Name formating entries                               */
/* --------------------------------------------------------------- */
typedef struct {
  char	*iProduct;
  char	*iModel;
  char	*finalname;
} models_name_t;

models_name_t models_names [] =
  {
	/* Ellipse models */
	{ "ELLIPSE", "300", "ellipse 300" },
	{ "ELLIPSE", "500", "ellipse 500" },
	{ "ELLIPSE", "650", "ellipse 650" },
	{ "ELLIPSE", "800", "ellipse 800" },
	{ "ELLIPSE", "1200", "ellipse 1200" },
	/* Ellipse Premium models */
	{ "ellipse", "PR500", "ellipse premium 500" },
	{ "ellipse", "PR650", "ellipse premium 650" },
	{ "ellipse", "PR800", "ellipse premium 800" },
	{ "ellipse", "PR1200", "ellipse premium 1200" },
	/* Ellipse "Pro" */
	{ "ELLIPSE", "600", "Ellipse 600" },
	{ "ELLIPSE", "750", "Ellipse 750" },
	{ "ELLIPSE", "1000", "Ellipse 1000" },
	{ "ELLIPSE", "1500", "Ellipse 1500" },
	/* Ellipse "MAX" */
	{ "Ellipse MAX", "600", "Ellipse MAX 600" },
	{ "Ellipse MAX", "850", "Ellipse MAX 850" },
	{ "Ellipse MAX", "1100", "Ellipse MAX 1100" },
	{ "Ellipse MAX", "1500", "Ellipse MAX 1500" },
	/* Protection Center */
	{ "PROTECTIONCENTER", "420", "Protection Center 420" },
	{ "PROTECTIONCENTER", "500", "Protection Center 500" },
	{ "PROTECTIONCENTER", "675", "Protection Center 675" },
	/* Pulsar Evolution models */
	{ "Evolution", "500", "Pulsar Evolution 500" },
	{ "Evolution", "800", "Pulsar Evolution 800" },
	{ "Evolution", "1100", "Pulsar Evolution 1100" },
	{ "Evolution", "1500", "Pulsar Evolution 1500" },
	{ "Evolution", "2200", "Pulsar Evolution 2200" },
	{ "Evolution", "3000", "Pulsar Evolution 3000" },
	{ "Evolution", "3000XL", "Pulsar Evolution 3000 XL" },
	/* Newer Evolution models */
	{ "Evolution", "650", "Evolution 650" },
	{ "Evolution", "850", "Evolution 850" },
	{ "Evolution", "1150", "Evolution 1150" },
	{ "Evolution", "S 1250", "Evolution S 1250" },
	{ "Evolution", "1550", "Evolution 1550" },
	{ "Evolution", "S 1750", "Evolution S 1750" },
	{ "Evolution", "2000", "Evolution 2000" },
	{ "Evolution", "S 2500", "Evolution S 2500" },
	{ "Evolution", "S 3000", "Evolution S 3000" },
	/* Pulsar M models */
	{ "PULSAR M", "2200", "Pulsar M 2200" },
	{ "PULSAR M", "3000", "Pulsar M 3000" },
	{ "PULSAR M", "3000 XL", "Pulsar M 3000 XL" },
	/* Eaton'ified names */
	{ "EX", "2200", "EX 2200" },
	{ "EX", "3000", "EX 3000" },
	{ "EX", "3000 XL", "EX 3000 XL" },
	/* Pulsar models */
	{ "Pulsar", "700", "Pulsar 700" },
	{ "Pulsar", "1000", "Pulsar 1000" },
	{ "Pulsar", "1500", "Pulsar 1500" },
	{ "Pulsar", "1000 RT2U", "Pulsar 1000 RT2U" },
	{ "Pulsar", "1500 RT2U", "Pulsar 1500 RT2U" },
	/* Eaton'ified names */
	{ "EX", "700", "EX 700" },
	{ "EX", "1000", "EX 1000" },
	{ "EX", "1500", "EX 1500" },
	{ "EX", "1000 RT2U", "EX 1000 RT2U" },
	{ "EX", "1500 RT2U", "EX 1500 RT2U" },
	/* Pulsar MX models */
	{ "PULSAR", "MX4000", "Pulsar MX 4000 RT" },
	{ "PULSAR", "MX5000", "Pulsar MX 5000 RT" },
	/* NOVA models */	
	{ "NOVA AVR", "600", "NOVA 600 AVR" },
	{ "NOVA AVR", "625", "Nova AVR 625" },
	{ "NOVA AVR", "1100", "NOVA 1100 AVR" },
	{ "NOVA AVR", "1250", "Nova AVR 1250" },
	/* EXtreme C (EMEA) */
	{ "EXtreme", "700C", "Pulsar EXtreme 700C" },
	{ "EXtreme", "1000C", "Pulsar EXtreme 1000C" },
	{ "EXtreme", "1500C", "Pulsar EXtreme 1500C" },
	{ "EXtreme", "1500CCLA", "Pulsar EXtreme 1500C CLA" },
	{ "EXtreme", "2200C", "Pulsar EXtreme 2200C" },
	{ "EXtreme", "3200C", "Pulsar EXtreme 3200C" },
	/* EXtreme C (USA, aka "EX RT") */
	{ "EX", "700RT", "Pulsar EX 700 RT" },
	{ "EX", "1000RT", "Pulsar EX 1000 RT" },
	{ "EX", "1500RT", "Pulsar EX 1500 RT" },
	{ "EX", "2200RT", "Pulsar EX 2200 RT" },
	{ "EX", "3200RT", "Pulsar EX 3200 RT" },
	/* Comet EX RT three phased */
	{ "EX", "5RT31", "EX 5 RT 3:1" },
	{ "EX", "7RT31", "EX 7 RT 3:1" },
	{ "EX", "11RT31", "EX 11 RT 3:1" },
	/* Comet EX RT single phased */
	{ "EX", "5RT", "EX 5 RT" },
	{ "EX", "7RT", "EX 7 RT" },
	{ "EX", "11RT", "EX 11 RT" },
	/* Galaxy 3000 */
	{ "GALAXY", "3000_10", "Galaxy 3000 10 kVA" },
	{ "GALAXY", "3000_15", "Galaxy 3000 15 kVA" },
	{ "GALAXY", "3000_20", "Galaxy 3000 20 kVA" },
	{ "GALAXY", "3000_30", "Galaxy 3000 30 kVA" },

	/* FIXME: To be completed (Comet, Galaxy, Esprit, ...) */

	/* end of structure. */
	{ NULL, NULL, "Generic SHUT model" }
};
/* for lookup between HID values and NUT values*/
typedef struct {
	long	hid_value;	/* HID value */
	char	*nut_value;	/* NUT value */
} info_lkp_t;

/* Actual value lookup tables => should be fine for all Mfrs (TODO: validate it!) */
/* --------------------------------------------------------------- */
/*      Lookup values between NUT and HID                          */
/* --------------------------------------------------------------- */
info_lkp_t onbatt_info[] = {
  { 0, "OB" },
  { 1, "OL" },
  { 0, "NULL" }
};
info_lkp_t discharging_info[] = {
  { 1, "DISCHRG" },
  { 0, "NULL" }
};
info_lkp_t charging_info[] = {
  { 1, "CHRG" },
  { 0, "NULL" }
};
info_lkp_t lowbatt_info[] = {
  { 1, "LB" },
  { 0, "NULL" }
};
info_lkp_t overbatt_info[] = {
  { 1, "OVER" },
  { 0, "NULL" }
};
info_lkp_t replacebatt_info[] = {
  { 1, "RB" },
  { 0, "NULL" }
};
info_lkp_t shutdownimm_info[] = {
  { 1, "LB" },
  { 0, "NULL" }
};
info_lkp_t trim_info[] = {
  { 1, "TRIM" },
  { 0, "NULL" }
};
info_lkp_t boost_info[] = {
  { 1, "BOOST" },
  { 0, "NULL" }
};
/* TODO: add BYPASS, OFF, CAL */

info_lkp_t test_write_info[] = {
  { 0, "No test" },
  { 1, "Quick test" },
  { 2, "Deep test" },
  { 3, "Abort test" },
  { 0, "NULL" }
};
info_lkp_t test_read_info[] = {
  { 1, "Done and passed" },
  { 2, "Done and warning" },
  { 3, "Done and error" },
  { 4, "Aborted" },
  { 5, "In progress" },
  { 6, "No test initiated" },
  { 0, "NULL" }
};

/* --------------------------------------------------------------- */
/*      Query Commands and their Mapping to INFO_ Variables        */
/* --------------------------------------------------------------- */

/* Structure defining how to query UPS for a variable and write
   information to INFO structure.
*/
typedef struct {
  const char	*type;		/* INFO_* element                        */
  int			flags;		/* INFO-element flags to set in addinfo  */
  int			length;		/* INFO-element length of strings        */  
  const char	*item_path;	/* HID object (fully qualified string path) */
  const char	fmt[6];		/* printf format string for INFO entry   */
  const char	*dfl;		/* default value */
  unsigned long	shut_flags;	/* specific SHUT flags */
  info_lkp_t	*hid2info;	/* lookup table between HID and NUT values */
} mge_info_item_t;

/* Array containing information to translate between UTalk and NUT info
 * NOTE: 
 *	- Array is terminated by element with type NULL.
 *	- Essential INFO items (ups.{mfr, model, firmware, status} are
 *		handled separately.
 *	- Array is NOT const, since "shut_flags" can be changed.
 */

/* FIXME: should be shared with mgehid.h */
static mge_info_item_t mge_info[] = {
	/* Battery page */
	{ "battery.charge", 0, 0, "UPS.PowerSummary.RemainingCapacity", "%i", NULL, SHUT_FLAG_OK, NULL },
	{ "battery.charge.low", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerSummary.RemainingCapacityLimitSetting",
	  "%ld", NULL, SHUT_FLAG_OK, NULL }, /* RW, to be caught first if exists... */
	{ "battery.charge.low", ST_FLAG_STRING, 5, "UPS.PowerSummary.RemainingCapacityLimit",
	  "%ld", NULL, SHUT_FLAG_OK, NULL }, /* ... or Read only */
	{ "battery.runtime", 0, 0, "UPS.PowerSummary.RunTimeToEmpty", "%.0d", NULL, SHUT_FLAG_OK, NULL },
	/* UPS page */
	{ "ups.mfr", ST_FLAG_STRING, 20, NULL, "%s", "MGE UPS SYSTEMS", SHUT_FLAG_ABSENT | SHUT_FLAG_OK, NULL },
	{ "ups.load", 0, 0, "UPS.PowerSummary.PercentLoad", "%i", NULL, SHUT_FLAG_OK, NULL },
	{ "ups.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerSummary.DelayBeforeShutdown",
	  "%ld", NULL, SHUT_FLAG_OK | SHUT_FLAG_DELAY, NULL },
	{ "ups.delay.reboot", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerSummary.DelayBeforeReboot",
	  "%ld", NULL, SHUT_FLAG_OK | SHUT_FLAG_DELAY, NULL },
	{ "ups.delay.start", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.PowerSummary.DelayBeforeStartup",
	  "%ld", NULL, SHUT_FLAG_OK | SHUT_FLAG_DELAY, NULL },
	/* FIXME: miss ups.power */
	{ "ups.power.nominal", ST_FLAG_STRING, 5, "UPS.Flow.[4].ConfigApparentPower",
	  "%i", NULL, SHUT_FLAG_OK, NULL },
	{ "ups.test.interval", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.BatterySystem.Battery.TestPeriod",
	  "%i", NULL, SHUT_FLAG_OK, NULL },
	{ "ups.test.result", ST_FLAG_STRING, 5, "UPS.BatterySystem.Battery.Test",
	  "%i", NULL, SHUT_FLAG_OK, &test_read_info[0] },

	/* Output page */
	{ "output.voltage", 0, 0, "UPS.PowerConverter.Output.Voltage", "%i", NULL, SHUT_FLAG_OK, NULL },
	{ "output.voltage.nominal", 0, 0, "UPS.PowerSummary.ConfigVoltage", "%i", NULL, SHUT_FLAG_OK, NULL },
	{ "output.current", 0, 0, "UPS.PowerSummary.Output.Current", "%i", NULL, SHUT_FLAG_OK, NULL },
    { "output.frequency", 0, 0, "UPS.PowerConverter.Output.Frequency", "%i", NULL, SHUT_FLAG_OK, NULL },

	/* Outlet page (using MGE UPS SYSTEMS - PowerShare technology) */
	/* TODO: add an iterative semantic [%x] to factorise outlets */
	{ "outlet.id", 0, 0, "UPS.OutletSystem.Outlet.[1].OutletID", "%i", NULL, SHUT_FLAG_OK, NULL },
	{ "outlet.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, "UPS.OutletSystem.Outlet.[1].OutletID",
	  "%s", "Main Outlet", SHUT_FLAG_ABSENT | SHUT_FLAG_OK, NULL },
	{ "outlet.switchable", 0, 0, "UPS.OutletSystem.Outlet.[1].PresentStatus.Switchable",
	  "%i", NULL, SHUT_FLAG_OK, NULL },
	{ "outlet.1.id", 0, 0, "UPS.OutletSystem.Outlet.[2].OutletID", "%i", NULL, SHUT_FLAG_OK, NULL },	
	{ "outlet.1.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, "UPS.OutletSystem.Outlet.[2].OutletID",
	  "%s", "PowerShare Outlet 1", SHUT_FLAG_ABSENT | SHUT_FLAG_OK, NULL },
	{ "outlet.1.switchable", 0, 0, "UPS.OutletSystem.Outlet.[2].PresentStatus.Switchable",
	  "%i", NULL, SHUT_FLAG_OK, NULL },
	{ "outlet.1.switch", ST_FLAG_RW | ST_FLAG_STRING, 2, "UPS.OutletSystem.Outlet.[2].PresentStatus.SwitchOn/Off",
	  "%i", NULL, SHUT_FLAG_OK, NULL },
	{ "outlet.1.autoswitch.charge.low", ST_FLAG_RW | ST_FLAG_STRING, 3,
	  "UPS.OutletSystem.Outlet.[2].RemainingCapacityLimit", "%i", NULL, SHUT_FLAG_OK, NULL },
	{ "outlet.1.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 5,
	  "UPS.OutletSystem.Outlet.[2].DelayBeforeShutdown", "%i", NULL, SHUT_FLAG_OK, NULL },
	{ "outlet.1.delay.start", ST_FLAG_RW | ST_FLAG_STRING, 5, "UPS.OutletSystem.Outlet.[2].DelayBeforeStartup", "%i", NULL, SHUT_FLAG_OK, NULL },
	{ "outlet.2.id", 0, 0, "UPS.OutletSystem.Outlet.[3].OutletID", "%i", NULL, SHUT_FLAG_OK, NULL },	
	{ "outlet.2.desc", ST_FLAG_RW | ST_FLAG_STRING, 20, "UPS.OutletSystem.Outlet.[3].OutletID",
	  "%s", "PowerShare Outlet 2", SHUT_FLAG_ABSENT | SHUT_FLAG_OK, NULL },
	{ "outlet.2.switchable", 0, 0, "UPS.OutletSystem.Outlet.[3].PresentStatus.Switchable",
	  "%i", NULL, SHUT_FLAG_OK, NULL },
	{ "outlet.2.switch", ST_FLAG_RW | ST_FLAG_STRING, 2, "UPS.OutletSystem.Outlet.[3].PresentStatus.SwitchOn/Off",
	  "%i", NULL, SHUT_FLAG_OK, NULL },
	{ "outlet.2.autoswitch.charge.low", ST_FLAG_RW | ST_FLAG_STRING, 3,
	  "UPS.OutletSystem.Outlet.[3].RemainingCapacityLimit", "%i", NULL, SHUT_FLAG_OK, NULL },
	{ "outlet.2.delay.shutdown", ST_FLAG_RW | ST_FLAG_STRING, 5,
	  "UPS.OutletSystem.Outlet.[3].DelayBeforeShutdown", "%i", NULL, SHUT_FLAG_OK, NULL },
	{ "outlet.2.delay.start", ST_FLAG_RW | ST_FLAG_STRING, 5,
	  "UPS.OutletSystem.Outlet.[3].DelayBeforeStartup", "%i", NULL, SHUT_FLAG_OK, NULL },

	/* Input page */
	{ "input.voltage", 0, 0, "UPS.PowerConverter.Input.[1].Voltage", "%i", NULL, SHUT_FLAG_OK, NULL },
	{ "input.frequency", 0, 0, "UPS.PowerConverter.Input.[1].Frequency", "%i", NULL, SHUT_FLAG_OK, NULL },

	/* terminating element */
	{ NULL, 0, 0, "\0", "\0", NULL, 0, NULL } 
};

/* temporary usage code lookup */
typedef struct {
	const char *usage_name;
	int usage_code;
} usage_lkp_t;

/* FIXME: share this data structure with libhid.c */
static usage_lkp_t usage_lkp[] = {
	/* Power Device Page */
	{  "PresentStatus", 0x00840002 },
	{  "UPS", 0x00840004 },
	{  "BatterySystem", 0x00840010 },
	{  "Battery", 0x00840012 },
	{  "BatteryID", 0x00840013 },	
	{  "PowerConverter", 0x00840016 },
	{  "OutletSystem", 0x00840018 },
	{  "Input", 0x0084001a },
	{  "Output", 0x0084001c },
	{  "Outlet", 0x00840020 },
	{  "OutletID", 0x00840021 },
	{  "PowerSummary", 0x00840024 },
	{  "Voltage", 0x00840030 },
	{  "Current", 0x00840031 },
	{  "Frequency", 0x00840032 },
	{  "PercentLoad", 0x00840035 },
	{  "ConfigVoltage", 0x00840040 },
	{  "ConfigCurrent", 0x00840041 },
	{  "ConfigFrequency", 0x00840042 },
	{  "ConfigApparentPower", 0x00840043 },
	{  "LowVoltageTransfer", 0x00840053 },
	{  "HighVoltageTransfer", 0x00840054 },	
	{  "DelayBeforeReboot", 0x00840055 },	
	{  "DelayBeforeStartup", 0x00840056 },
	{  "DelayBeforeShutdown", 0x00840057 },
	{  "Test", 0x00840058 },
	{  "Good", 0x00840061 },
	{  "Overload", 0x00840065 }, /* sic */
	{  "SwitchOn/Off", 0x0084006b },	
	{  "Switchable", 0x0084006c },	
	{  "Used", 0x0084006d },
	{  "Flow", 0x0084001e },
	/* Battery System Page */
	{  "RemainingCapacityLimit", 0x00850029 },
	{  "BelowRemainingCapacityLimit", 0x00850042 },
	{  "RemainingCapacity", 0x00850066 },
	{  "RunTimeToEmpty", 0x00850068 },
	{  "ACPresent", 0x008500d0 },
	{  "Charging", 0x00850044 },
	{  "Discharging", 0x00850045 },
	{  "NeedReplacement", 0x0085004b },
	/* MGE UPS SYSTEMS Page */
	{  "iModel", 0xffff00f0 },
	{  "RemainingCapacityLimitSetting", 0xffff004d },
	{  "TestPeriod", 0xffff0001 },
	
	{  "\0", 0x0 }
};

/* SHUT / HID functions Prototypes */

int   shut_ups_start(void);
u_char shut_checksum(const u_char *buf, int bufsize);
int   shut_token_send(u_char token);
int   shut_packet_send (hid_data_t *hdata, int datalen, u_char token);
int   shut_packet_recv (u_char *Buf, int datalen);

int   shut_get_descriptor(int desctype, u_char *pkt, int reportlen);
int   shut_get_string(int strindex, char *string, int stringlen);
int   shut_get_report(int id, u_char *pkt, int reportlen);
int   shut_set_report(int id, u_char *pkt, int reportlen);
int   shut_identify_ups (void);
int   shut_wait_ack (void);
void  shut_ups_status(void);

int    hid_init_device(void);
char  *get_model_name(char *iProduct, char *iModel);
int    hid_lookup_usage(char *name);
int    hid_get_value(const char *item_path);
int    hid_set_value(const char *varname, const char *val);
u_short lookup_path(const char *HIDpath, HIDData_t *data);

int    instcmd(const char *cmdname, const char *extra);
void   setline(int set);
int    serial_read (int read_timeout, u_char *readbuf);
int    serial_send(u_char *buf, int len);

void   make_string(u_char *buf, int datalen, char *string);


mge_info_item_t *shut_find_info(const char *varname);
