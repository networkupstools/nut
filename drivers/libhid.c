/*!
 * @file libhid.c
 * @brief HID Library - User API (Generic HID Access using MGE HIDParser)
 *
 * @author Copyright (C) 2003 - 2005
 *	Arnaud Quette <arnaud.quette@free.fr> && <arnaud.quette@mgeups.com>
 *	Philippe Marzouk <philm@users.sourceforge.net> (dump_hex())
 *	John Stamp <kinsayder@hotmail.com>
 *	
 * This program is sponsored by MGE UPS SYSTEMS - opensource.mgeups.com
 *
 *      The logic of this file is ripped from mge-shut driver (also from
 *      Arnaud Quette), which is a "HID over serial link" UPS driver for
 *      Network UPS Tools <http://www.networkupstools.org/>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * -------------------------------------------------------------------------- */

#include <stdio.h>
#include <string.h>
/* #include <math.h> */
#include "hidparser.h"
#include "hidtypes.h"
#include "libhid.h"

#include "hid-usb.h"

#include <errno.h>
extern int errno;

HIDDevice curDevice;

static HIDData   	hData;
static HIDParser 	hParser;

unsigned char raw_buf[100];
unsigned char **report_buf;
int replen; /* size of the last report retrieved */
static int prev_report; /* previously retrieved report ID */
static time_t prev_report_ts = 0; /* timestamp of the previously retrieved report */
unsigned char ReportDesc[4096];

#define MAX_REPORT_SIZE         0x1800

/* TODO: rework all that */
extern void upsdebugx(int level, const char *fmt, ...);
#define TRACE upsdebugx

/* Units and exponents table (HID PDC, 3.2.3) */
#define NB_HID_UNITS 10
const long HIDUnits[NB_HID_UNITS][2]=
{
	{0x00000000,0}, /* None */
	{0x00F0D121,7}, /* Voltage */
	{0x00100001,0}, /* Ampere */
	{0x0000D121,7}, /* VA */
	{0x0000D121,7}, /* Watts */
	{0x00001001,0}, /* second */
	{0x00010001,0}, /* K */
	{0x00000000,0}, /* percent */
	{0x0000F001,0}, /* Hertz */
	{0x00101001,0}, /* As */
};

/* support functions */
void logical_to_physical(HIDData *Data);
void physical_to_logical(HIDData *Data);
const char *hid_lookup_path(int usage);
int hid_lookup_usage(char *name);
ushort lookup_path(char *HIDpath, HIDData *data);
void dump_hex (const char *msg, const unsigned char *buf, int len);
long get_unit_expo(long UnitType);
float expo(int a, int b);


void HIDDumpTree(HIDDevice *hd)
{
	int i;
	char str[128];

	while (HIDParse(&hParser, &hData) != FALSE)
	{
		str[0] = '\0';
		for (i = 0; i < hData.Path.Size; i++)
		{
		  strcat(str, hid_lookup_path((hData.Path.Node[i].UPage * 0x10000) + hData.Path.Node[i].Usage));
			if (i < (hData.Path.Size - 1))
				strcat (str, ".");
		}
		TRACE(1, "Path: %s", str);
	}
}
						
HIDDevice *HIDOpenDevice(const char *port, MatchFlags *flg, int mode)
{
	int ReportSize;

	if ( mode == MODE_OPEN )
	{
		/* Init structure */
		curDevice.Name = NULL;
		curDevice.Vendor = NULL;
		curDevice.VendorID = -1;
		curDevice.Product = NULL;
		curDevice.ProductID = -1;
		curDevice.Serial = NULL;
		curDevice.Application = -1;
		curDevice.fd = -1;
	}
	else
	{
		TRACE(2, "Reopening device");
	}

	/* get and parse descriptors (dev, cfg and report) */
	ReportSize = libusb_open(&curDevice, flg, ReportDesc, mode);

	if (ReportSize == -1)
		return NULL;
	else
	{
		if ( mode == MODE_REOPEN )
		{
			TRACE(2, "Device reopened successfully");
			return &curDevice;
		}
	
		TRACE(2, "Report Descriptor size = %d", ReportSize);
		dump_hex ("Report Descriptor", ReportDesc, 200);

		/* HID Parser Init */
		ResetParser(&hParser);
		hParser.ReportDescSize = ReportSize;
		memcpy(hParser.ReportDesc, ReportDesc, ReportSize);
		HIDParse(&hParser, &hData);
	}
	return &curDevice;
}

/* int HIDGetItem(hid_info_t *ItemInfo, HIDItem *item) */
HIDItem *HIDGetItem(const char *ItemPath)
{
	/* was for libhid, not useful in our scope! */
	return NULL;
}

/* return 1 if OK, 0 on fail, <= -1 otherwise (ie disconnect) */
float HIDGetItemValue(char *path, float *Value)
{
	int i, retcode;
	float tmpValue;

	/* Prepare path of HID object */
	hData.Type = ITEM_FEATURE;
	hData.ReportID = 0;
	hData.Path.Size = 0;

	if((retcode = lookup_path(path, &hData)) > 0)
	{
		TRACE(4, "Path depth = %i", retcode);

		for (i = 0; i<retcode; i++)
			TRACE(4, "%i: UPage(%x), Usage(%x)", i,
				hData.Path.Node[i].UPage,
				hData.Path.Node[i].Usage);

		hData.Path.Size = retcode;

		/* Get info on object (reportID, offset and size) */
		if (FindObject(&hParser, &hData) == 1)
		{
			/* Get report with data */
			/* if ((replen=libusb_get_report(hData.ReportID,
			raw_buf, MAX_REPORT_SIZE)) > 0) { => doesn't work! */
			/* Bufferize at least the last report */
			if ( ( (prev_report == hData.ReportID) && (time(NULL) <= (prev_report_ts + MAX_TS)) )
				|| ((replen=libusb_get_report(hData.ReportID, raw_buf, 10)) > 0) )
			{
				/* Extract the data value */
				GetValue((const unsigned char *) raw_buf, &hData);

				TRACE(4, "=>> Before exponent: %ld, %i/%i)", hData.Value,
					(int)hData.UnitExp, (int)get_unit_expo(hData.Unit) );

				/* Convert Logical Min, Max and Value in Physical */
				/* logical_to_physical(&hData); */

				tmpValue = hData.Value;

				/* Process exponents */
				/* Value*=(float) pow(10,(int)hData.UnitExp - get_unit_expo(hData.Unit)); */
				tmpValue*=(float) expo(10,(int)hData.UnitExp - get_unit_expo(hData.Unit));
				hData.Value = (long) tmpValue;

				/* Convert Logical Min, Max and Value into Physical */
				logical_to_physical(&hData);

				TRACE(4, "=>> After conversion: %ld, %i/%i)", hData.Value,
					(int)hData.UnitExp, (int)get_unit_expo(hData.Unit) );

				dump_hex ("Report ", raw_buf, replen);

				*Value = hData.Value;
				prev_report = hData.ReportID;
				prev_report_ts = time(NULL);
				return 1;
			}
			else
			{
				TRACE(2, "Can't retrieve Report %i (%i/%i)", hData.ReportID, replen, errno);
				return -errno;
			}
		}
		else
			TRACE(2, "Can't find object %s", path);
	}
	return 0; /* TODO: should be checked */
}

char *HIDGetItemString(char *path)
{
  int i, retcode;
  
  /* Prepare path of HID object */
  hData.Type = ITEM_FEATURE;
  hData.ReportID = 0;
  hData.Path.Size = 0;
  
  if((retcode = lookup_path(path, &hData)) > 0) {
    TRACE(4, "Path depth = %i", retcode);
    
    for (i = 0; i<retcode; i++)
      TRACE(4, "%i: UPage(%x), Usage(%x)", i,
		hData.Path.Node[i].UPage,
		hData.Path.Node[i].Usage);
    
    hData.Path.Size = retcode;
    
    /* Get info on object (reportID, offset and size) */
    if (FindObject(&hParser,&hData) == 1) {
      if (libusb_get_report(hData.ReportID, raw_buf, 8) > 0) { /* MAX_REPORT_SIZE) > 0) { */
	GetValue((const unsigned char *) raw_buf, &hData);

	/* now get string */
	libusb_get_string(hData.Value, raw_buf);
	return raw_buf;
      }
      else
	TRACE(2, "Can't retrieve Report %i", hData.ReportID);
    }
    else
      TRACE(2, "Can't find object %s", path);

    return NULL;
  }
  return NULL;
}
 
bool HIDSetItemValue(char *path, float value)
{
	float Value;
	int retcode;
	
	/* Begin by a standard Get to fill in com structures ... */
	retcode = HIDGetItemValue(path, &Value);
	
	/* ... And play with global vars */
	if (retcode == 1) /* Get succeed */
	{
		TRACE(2, "=>> SET: Before set: %.2f (%ld)", Value, (long)value);
		
		/* Test if Item is settable */
		if (hData.Attribute != ATTR_DATA_CST)
		{
			/* Set new value for this item */
			/* And Process exponents restoration */
			Value = value * expo(10, get_unit_expo(hData.Unit) - (int)hData.UnitExp);
			
			hData.Value=(long) Value;
			
			TRACE(2, "=>> SET: after exp: %ld/%.2f (exp = %.2f)", hData.Value, Value,
				expo(10, (int)get_unit_expo(hData.Unit) - (int)hData.UnitExp));

			/* Convert Physical Min, Max and Value in Logical */
			physical_to_logical(&hData);
			TRACE(2, "=>> SET: after PL: %ld", hData.Value);
			
			SetValue(&hData, raw_buf);
			
			dump_hex ("==> Report after setvalue", raw_buf, replen);
			
			if (libusb_set_report(hData.ReportID, raw_buf, replen) > 0)
			{
				TRACE(2, "Set report succeeded");
				return TRUE;
			}
			else
			{
				TRACE(2, "Set report failed");
				return FALSE;
			}
			/* check if set succeed! => doesn't work on *Delay (decremented!) */
			/*      Value = HIDGetItemValue(path);
			
			TRACE(2, "=>> SET: new value = %.2f (was set to %.2f)\n", 
			Value, (float) value);
			return TRUE;*/ /* (Value == value); */
		}
	}
	return FALSE;
}

int HIDGetEvents(HIDDevice *dev, HIDItem **eventsList)
{
	unsigned char buf[20];
	char itemPath[128];
	int size, offset = 0, itemCount = 0;
	
	upsdebugx(1, "Waiting for notifications...");
	
	/* needs libusb-0.1.8 to work => use ifdef and autoconf */
	if ((size = libusb_get_interrupt(&buf[0], 20, 5000)) > -1)
	{
		dump_hex ("Notification", buf, size);
		
		/* Convert report size in bits */
		size = (size - 1) * 8;
		
		/* Parse response Report and Set correspondant Django values */
		hData.ReportID = buf[0];
		hData.Type = ITEM_INPUT;
		
		while(offset < size)
		{
			/* Set Offset */
			hData.Offset = offset;
	
			/* Reset HID Path but keep Report ID */
			memset(&hData.Path, '\0', sizeof(HIDPath));
	
			/* Get HID Object characteristics */
			if(FindObject(&hParser, &hData))
			{
				/* Get HID Object value from report */
				GetValue(buf, &hData);
				memset(&itemPath, 0, sizeof(128));
				lookup_path(&itemPath[0], &hData);
	
				upsdebugx(3, "Object: %s = %ld", itemPath, hData.Value);
	
				/* FIXME: enhance this or fix/change the HID parser (see libhid project) */
				/* if ( strstr(itemPath, "000000") == NULL) */
				if (strcmp(itemPath, "UPS.PowerSummary.PresentStatus.") > 0)
				{
					eventsList[itemCount] = (HIDItem *)malloc(sizeof (HIDItem));
					eventsList[itemCount]->Path = strdup(itemPath);
					eventsList[itemCount]->Value = hData.Value;
					itemCount++;
				}
			}
			offset += hData.Size;
		}
	}
	else
		itemCount = size; /* propagate error code */

	return itemCount;
}

void HIDCloseDevice(HIDDevice *dev)
{
	TRACE(2, "Closing device");
	libusb_close(&curDevice);
}


/*******************************************************
 * Support functions
 *******************************************************/

#define MAX_STRING      		64

void logical_to_physical(HIDData *Data)
{
	if(Data->PhyMax - Data->PhyMin > 0)
	{
		float Factor = (float)(Data->PhyMax - Data->PhyMin) / (Data->LogMax - Data->LogMin);
		/* Convert Value */
		Data->Value=(long)((Data->Value - Data->LogMin) * Factor) + Data->PhyMin;
			
		if(Data->Value > Data->PhyMax)
			Data->Value |= ~Data->PhyMax;
	}
	else /* => nothing to do!? */
	{
		/* Value.m_Value=(long)(pConvPrm->HValue); */
		if(Data->Value > Data->LogMax)
			Data->Value |= ~Data->LogMax;
	}
	
	/* if(Data->Value > Data->Value.m_Max)
		Value.m_Value |= ~Value.m_Max;
	*/
}

void physical_to_logical(HIDData *Data)
{
	TRACE(2, "PhyMax = %ld, PhyMin = %ld, LogMax = %ld, LogMin = %ld",
		Data->PhyMax, Data->PhyMin, Data->LogMax, Data->LogMin);
	
	if(Data->PhyMax - Data->PhyMin > 0)
	{
		float Factor=(float)(Data->LogMax - Data->LogMin) / (Data->PhyMax - Data->PhyMin);
		
		/* Convert Value */
		Data->Value=(long)((Data->Value - Data->PhyMin) * Factor) + Data->LogMin;
	}
	/* else => nothing to do!?
	{
	m_ConverterTab[iTab].HValue=m_ConverterTab[iTab].DValue;
	} */
}

long get_unit_expo(long UnitType)
{
	int i = 0, exp = -1;
	
	while (i < NB_HID_UNITS)
	{
		if (HIDUnits[i][0] == UnitType)
		{
			exp = HIDUnits[i][1];
			break;
		}
		i++;
	}
	return exp;
}

/* exponent function: return a^b */
/* FIXME: check if needed/possible to replace libmath->pow */
float expo(int a, int b)
{
	if (b==0)
		return (float) 1;
	if (b>0)
		return (float) a * expo(a,b-1);
	if (b<0)
		return (float)((float)(1/(float)a) * (float) expo(a,b+1));
	
	/* not reached */
	return -1;
}

/* translate HID string path from/to numeric path and return path depth */
/* TODO: use usbutils functions (need to be externalised!) */
ushort lookup_path(char *HIDpath, HIDData *data)
{
	ushort i = 0, cond = 1;
	int cur_usage;
	char buf[MAX_STRING];
	char *start, *end; 
	
	TRACE(3, "entering lookup_path()");
	
	/* Check the way we are called */
	if (data->Path.Size != 0)
	{
	  /* FIXME: another bug? */
	  strcat(HIDpath, "UPS.");

	  // Numeric to String
	  for (i = 1; i <= hData.Path.Size; i++)
		{
		  /* Deal with ?bogus? */
		  if ( ((hData.Path.Node[i].UPage * 0x10000) + hData.Path.Node[i].Usage) == 0)
			continue;

		  /* manage indexed collection */
		  if (hData.Path.Node[i].UPage == 0x00FF)
			{
			  TRACE(5, "Got an indexed collection");
			  sprintf(strrchr(HIDpath, '.'), "[%i]", hData.Path.Node[i].Usage);
			}
		  else
			strcat(HIDpath, hid_lookup_path((hData.Path.Node[i].UPage * 0x10000) + hData.Path.Node[i].Usage));
			
		  if (i < (hData.Path.Size - 1))
			strcat (HIDpath, ".");
		}
	}
  else
	{
	  // String to Numeric 
	  strncpy(buf, HIDpath, strlen(HIDpath));
	  buf[strlen(HIDpath)] = '\0';
	  start = end = buf;
  
	  while (cond) {
    
		if ((end = strchr(start, '.')) == NULL) {
		  cond = 0;			
		}
		else
		  *end = '\0';
    
		TRACE(4, "parsing %s", start);
    
		/* lookup code */
		if ((cur_usage = hid_lookup_usage(start)) == -1) {
		  TRACE(4, "%s wasn't found", start);
		  return 0;
		}
		else {
		  data->Path.Node[i].UPage = (cur_usage & 0xFFFF0000) / 0x10000;
		  data->Path.Node[i].Usage = cur_usage & 0x0000FFFF; 
		  i++; 
		}
    
		if(cond)
		  start = end +1 ;
	  }
	  data->Path.Size = i;
	}

  return i;
}

/* Lookup this usage name to find its code (page + index) */
/* temporary usage code lookup */
typedef struct {
	const char *usage_name;
	int usage_code;
} usage_lkp_t;

static usage_lkp_t usage_lkp[] = {
	/* Power Device Page */
	{  "Undefined",				0x00840000 },
	{  "PresentStatus",			0x00840002 },
	{  "UPS",				0x00840004 },
	{  "BatterySystem",			0x00840010 },
	{  "Battery",				0x00840012 },
	{  "BatteryID",				0x00840013 },
	{  "PowerConverter",			0x00840016 },
	{  "PowerConverterID",			0X00840017 },
	{  "OutletSystem",			0x00840018 },
	{  "OutletSystemID",			0x00840019 },
	{  "Input",				0x0084001a },
	{  "Output",				0x0084001c },
	{  "Flow",				0x0084001e },
	{  "FlowID",				0x0084001f },
	{  "Outlet",				0x00840020 },
	{  "OutletID",				0x00840021 },
	{  "PowerSummary",			0x00840024 },
	{  "PowerSummaryID",			0x00840025 },
	{  "Voltage",				0x00840030 },
	{  "Current",				0x00840031 },
	{  "Frequency",				0x00840032 },
	{  "PercentLoad",			0x00840035 },
	{  "Temperature",			0x00840036 },
	{  "ConfigVoltage",			0x00840040 },
	{  "ConfigCurrent",			0x00840041 },
	{  "ConfigFrequency",			0x00840042 },
	{  "ConfigApparentPower",		0x00840043 },
	{  "LowVoltageTransfer",		0x00840053 },
	{  "HighVoltageTransfer",		0x00840054 },	
	{  "DelayBeforeReboot",			0x00840055 },
	{  "DelayBeforeStartup",		0x00840056 },
	{  "DelayBeforeShutdown",		0x00840057 },
	{  "Test",				0x00840058 },
	{  "AudibleAlarmControl",		0x0084005a },
	{  "Good",				0x00840061 },
	{  "InternalFailure",			0x00840062 },
	{  "OverLoad",				0x00840065 }, /* mispelled in usb.ids */
	{  "OverTemperature", 			0x00840067 },
	{  "ShutdownImminent",			0x00840069 },
	{  "SwitchOn/Off",			0x0084006b },
	{  "Switchable",			0x0084006c },
	{  "Used",				0x0084006d },
	{  "Boost",				0x0084006e },
	{  "Buck",				0x0084006f },
	{  "CommunicationLost",			0x00840073 },
	{  "iManufacturer",			0x008400fd },
	{  "iProduct",				0x008400fe },
	{  "iSerialNumber",			0x008400ff },
	/* Battery System Page */
	{ "Undefined",				0x00850000 },
	{ "RemainingCapacityLimit",		0x00850029 },
	{ "RemainingTimeLimit",			0x0085002a },
	{ "CapacityMode",			0x0085002c },
	{ "BelowRemainingCapacityLimit",	0x00850042 },
	{ "RemainingTimeLimitExpired",		0x00850043 },
	{ "Charging",				0x00850044 },
	{ "Discharging",			0x00850045 },
	{ "NeedReplacement",			0x0085004b },
	{ "RemainingCapacity",			0x00850066 },
	{ "FullChargeCapacity",			0x00850067 },
	{ "RunTimeToEmpty",			0x00850068 },
	{ "ManufacturerDate",			0x00850085 },
	{ "Rechargeable",			0x0085008b },
	{ "WarningCapacityLimit",		0x0085008c },
	{ "CapacityGranularity1",		0x0085008d },
	{ "CapacityGranularity2",		0x0085008e },
	{ "iOEMInformation",			0x0085008f },
	{ "DesignCapacity",			0x00850083 },
	{ "iDeviceChemistry",			0x00850089 },
	{ "ACPresent",				0x008500d0 },
	{ "BatteryPresent",			0x008500d1 },
/* TODO: per MFR specific usages */
	/* MGE UPS SYSTEMS Page */
	{ "iModel",				0xffff00f0 },
	{ "RemainingCapacityLimitSetting",	0xffff004d },
	{ "TestPeriod",				0xffff0045 },
	{ "LowVoltageBoostTransfer",		0xffff0050 },
	{ "HighVoltageBoostTransfer",		0xffff0051 },
	{ "LowVoltageBuckTransfer",		0xffff0052 },
	{ "HighVoltageBuckTransfer",		0xffff0053 },
	/* APC Page */
	{ "APCGeneralCollection",		0xff860005 },
	{ "APCBattReplaceDate",			0xff860016 },
	{ "APCBattCapBeforeStartup",		0xFF860019 }, /* FIXME: need to be exploited */
	{ "APC_UPS_FirmwareRevision",		0xff860042 },
	{ "APC860052",				0xff860052 },  /* Needed for APCForceShutdown path */
	{ "APCStatusFlag",			0xff860060 },
	{ "APCPanelTest",			0xff860072 }, /* FIXME: need to be exploited */
	{ "APCShutdownAfterDelay",		0xff860076 }, /* FIXME: need to be exploited */
	{ "APC_USB_FirmwareRevision",		0xff860079 }, /* FIXME: need to be exploited */
	{ "APCForceShutdown",			0xff86007c },
	{ "APCDelayBeforeShutdown",		0xff86007d },
	{ "APCDelayBeforeStartup",		0xff86007e }, /* FIXME: need to be exploited */
	/* FIXME: The below one seems to have been wrongly encoded by APC */
	/* Pages 84 to 88 are reserved for official HID definition! */
	{ "BUPHibernate",			0x00850058 }, /* FIXME: need to be exploited */
	{ "BUPBattCapBeforeStartup",		0x00860012 }, /* FIXME: need to be exploited */
	{ "BUPDelayBeforeStartup",		0x00860076 }, /* FIXME: need to be exploited */
	{ "BUPSelfTest",			0x00860010 }, /* FIXME: need to be exploited */
/*
 * USB USAGE NOTES for APC (from Russell Kroll in the old hidups
 *
 * FIXME: read 0xff86.... instead of 0x(00)86....?
 *
 *  0x860013 == 44200155090 - capability again                   
 *           == locale 4, 4 choices, 2 bytes, 00, 15, 50, 90     
 *           == minimum charge to return online                  
 *
 *  0x860060 == "441HMLL" - looks like a 'capability' string     
 *           == locale 4, 4 choices, 1 byte each                 
 *           == line sensitivity (high, medium, low, low)        
 *  NOTE! the above does not seem to correspond to my info 
 *
 *  0x860062 == D43133136127130                                  
 *           == locale D, 4 choices, 3 bytes, 133, 136, 127, 130 
 *           == high transfer voltage                            
 *
 *  0x860064 == D43103100097106                                  
 *           == locale D, 4 choices, 3 bytes, 103, 100, 097, 106 
 *           == low transfer voltage                             
 *
 *  0x860066 == 441HMLL (see 860060)                                   
 *
 *  0x860074 == 4410TLN                                          
 *           == locale 4, 4 choices, 1 byte, 0, T, L, N          
 *           == alarm setting (5s, 30s, low battery, none)       
 *
 *  0x860077 == 443060180300600                                  
 *           == locale 4, 4 choices, 3 bytes, 060,180,300,600    
 *           == wake-up delay (after power returns)              
 */

	/* end of structure. */
	{  "\0", 0x0 }
};

const char *hid_lookup_path(int usage)
{
	int i;
	static char raw_usage[10];
	
	TRACE(3, "Looking up %i", usage);

	for (i = 0; (usage_lkp[i].usage_name[0] != '\0'); i++)
	{
		if (usage_lkp[i].usage_code == usage)
			return usage_lkp[i].usage_name;
	}

	/* if the corresponding path isn't found,
		return the numeric usage in string form */
	sprintf (&raw_usage[0], "%08x", usage); 
	return &raw_usage[0];
}

int hid_lookup_usage(char *name)
{
	int i;
	
	TRACE(3, "Looking up %s", name);
	
	if (name[0] == '[') /* manage indexed collection */
		return (0x00FF0000 + atoi(&name[1]));
	else
	{
		for (i = 0; (usage_lkp[i].usage_code != 0x0); i++)
		{
			if (!strcmp(usage_lkp[i].usage_name, name))
			{
				TRACE(4, "hid_lookup_usage: found %04x",
					usage_lkp[i].usage_code);
	
				return usage_lkp[i].usage_code;
			}
		}
	}
	return -1;
}

#define NIBBLE(_i)    (((_i) < 10) ? '0' + (_i) : 'A' + (_i) - 10)

void dump_hex (const char *msg, const unsigned char *buf, int len)
{
	int i;
	int nlocal;
	const unsigned char *pc;
	char *out;
	const unsigned char *start;
	char c;
	char line[100];
 
	start = buf;
	out = line;
	
	for (i = 0, pc = buf, nlocal = len; i < 16; i++, pc++)
	{
		if (nlocal > 0)
		{
			c = *pc;

			*out++ = NIBBLE ((c >> 4) & 0xF);
			*out++ = NIBBLE (c & 0xF);

			nlocal--;
		}
		else
		{
			*out++ = ' ';
			*out++ = ' ';
		}
		*out++ = ' ';
	}
	*out++ = 0;

	TRACE(3, "%s: (%d bytes) => %s", msg, len, line);

	buf += 16;
	len -= 16;
}
