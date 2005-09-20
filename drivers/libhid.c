/*!
 * @file libhid.c
 * @brief HID Library - User API (Generic HID Access using MGE HIDParser)
 *
 * @author Copyright (C) 2003 - 2005
 *	Arnaud Quette <arnaud.quette@free.fr> && <arnaud.quette@mgeups.com>
 *	Philippe Marzouk <philm@users.sourceforge.net> (dump_hex())
 *	John Stamp <kinsayder@hotmail.com>
 *      2005 Peter Selinger <selinger@users.sourceforge.net>
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

#include "libusb.h"

#include <errno.h>

static HIDDevice curDevice;

static HIDData   	hData;
static HIDParser 	hParser;

static unsigned char raw_buf[100];
static int replen; /* size of the last report retrieved */
static int prev_report; /* previously retrieved report ID */
static time_t prev_report_ts = 0; /* timestamp of the previously retrieved report */
static unsigned char ReportDesc[4096];

/* FIXME: we currently "hard-wire" the report buffer size in the calls
   to libusb_get_report() below to 8 bytes. This is not really a great
   idea, but it is necessary because Belkin models will crash,
   sometimes with permanent firmware damage, if called with a larger
   buffer size (never mind the USB specification). Let's hope for now
   that no other UPS needs a buffer greater than 8. Ideally, the
   libhid library should calculate the *exact* size of the required
   report buffer from the report descriptor. */
#define REPORT_SIZE 8

/* TODO: rework all that */
void upsdebugx(int level, const char *fmt, ...);
#define TRACE upsdebugx

/* Units and exponents table (HID PDC, 3.2.3) */
#define NB_HID_UNITS 10
static const long HIDUnits[NB_HID_UNITS][2]=
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
static void logical_to_physical(HIDData *Data);
static void physical_to_logical(HIDData *Data);
static const char *hid_lookup_path(unsigned int usage);
static int hid_lookup_usage(char *name);
static ushort lookup_path(char *HIDpath, HIDData *data);
static void dump_hex (const char *msg, const unsigned char *buf, int len);
static long get_unit_expo(long UnitType);
static float expo(int a, int b);

/* ---------------------------------------------------------------------- */
/* matchers */

/* helper function: version of strcmp that tolerates NULL
 * pointers. NULL is considered to come before all other strings
 * alphabetically. */
static inline int strcmp_null(char *s1, char *s2) {
	if (s1 == NULL && s2 == NULL) {
		return 0;
	}
	if (s1 == NULL) {
		return -1;
	}
	if (s2 == NULL) {
		return 1;
	}
	return strcmp(s1, s2);
}

/* private callback function for exact matches */
static int match_function_exact(HIDDevice *d, void *privdata) {
	HIDDevice *data = (HIDDevice *)privdata;
	
	if (d->VendorID != data->VendorID) {
		return 0;
	}
	if (d->ProductID != data->ProductID) {
		return 0;
	}
	if (strcmp_null(d->Vendor, data->Vendor) != 0) {
		return 0;
	}
	if (strcmp_null(d->Product, data->Product) != 0) {
		return 0;
	}
	if (strcmp_null(d->Serial, data->Serial) != 0) {
		return 0;
	}
	/* note: the exact matcher ignores the "Bus" field, because
	   it can change during a reconnect. */
	return 1;
}

/* constructor: return a new matcher that matches the exact HIDDevice
 * d. Return NULL with errno set on error. */
HIDDeviceMatcher_t *new_exact_matcher(HIDDevice *d) {
	HIDDeviceMatcher_t *m;
	HIDDevice *data;

	m = (HIDDeviceMatcher_t *)malloc(sizeof(HIDDeviceMatcher_t));
	if (!m) {
		return NULL;
	}
	data = (HIDDevice *)malloc(sizeof(HIDDevice));
	if (!data) {
		free(m);
		return NULL;
	}
	data->VendorID = d->VendorID;
	data->ProductID = d->ProductID;
	data->Vendor = d->Vendor ? strdup(d->Vendor) : NULL;
	data->Product = d->Product ? strdup(d->Product) : NULL;
	data->Serial = d->Serial ? strdup(d->Serial) : NULL;

	m->match_function = &match_function_exact;
	m->privdata = (void *)data;
	m->next = NULL;
	return m;
}

/* destructor: free matcher previously created with new_exact_matcher */
void free_exact_matcher(HIDDeviceMatcher_t *matcher) {
	HIDDevice *data;

	if (matcher) {
		data = (HIDDevice *)matcher->privdata;
		
		free(data->Vendor);
		free(data->Product);
		free(data->Serial);
		free(data);
		free(matcher);
	}
}

/* Private function for compiling a regular expression. On success,
   store the compiled regular expression (or NULL) in *compiled, and
   return 0. On error with errno set, return -1. If the supplied
   regular expression is unparseable, return -2 (an error message can
   then be retrieved with regerror(3)). Note that *compiled will be an
   allocated value, and must be freed with regfree(), then free(), see
   regex(3). As a special case, if regex==NULL, then set
   *compiled=NULL (regular expression NULL is intended to match
   anything). */
static inline int compile_regex(regex_t **compiled, char *regex, int cflags) {
	int r;
	regex_t *preg;

	if (regex == NULL) {
		*compiled = NULL;
		return 0;
	}
	preg = (regex_t *)malloc(sizeof(regex_t));
	if (!preg) {
		return -1;
	}

	r = regcomp(preg, regex, cflags);
	if (r) {
		return -2;
	}
	*compiled = preg;
	return 0;
}

/* Private function for regular expression matching. Check if the
   entire string str (minus any initial and trailing whitespace)
   matches the compiled regular expression preg. Return 1 if it
   matches, 0 if not. Return -1 on error with errno set. Special
   cases: if preg==NULL, it matches everything (no contraint).  If
   str==NULL, then it is treated as "". */
static int match_regex(regex_t *preg, char *str) {
  int r;
  regmatch_t pmatch[1];
  char *p, *q;
  int len;

  if (preg == NULL) {
    return 1;
  }
  if (str == NULL) {
    str = "";
  }

  /* make a copy of str with whitespace stripped */
  for (q=str; *q==' ' || *q=='\t' || *q=='\n'; q++) {
    /* empty */
  }
  len = strlen(q);
  p = (char *)malloc(len+1);
  if (!p) {
	  return -1;
  }
  memcpy(p, q, len+1);
  while (len>0 && (p[len-1]==' ' || p[len-1]=='\t' || p[len-1]=='\n')) {
    len--;
  }
  p[len] = 0;

  /* test the regular expression */
  r = regexec(preg, p, 1, pmatch, 0);
  free(p);
  if (r) {
    return 0;
  }
  /* check that the match is the entire string */
  if (pmatch[0].rm_so != 0 || pmatch[0].rm_eo != len) {
    return 0;
  }
  return 1;
}

/* Private function, similar to match_regex, but the argument being
 * matched is a (hexadecimal) number, rather than a string. It is
 * converted to a 4-digit hexadecimal string. */
static inline int match_regex_hex(regex_t *preg, int n) {
	char buf[10];
	sprintf(buf, "%04x", n);
	return match_regex(preg, buf);
}

/* private data type: hold a set of compiled regular expressions. */
struct regex_matcher_data_s {
	regex_t *regex[6];
};
typedef struct regex_matcher_data_s regex_matcher_data_t;

/* private callback function for regex matches */
static int match_function_regex(HIDDevice *d, void *privdata) {
	regex_matcher_data_t *data = (regex_matcher_data_t *)privdata;
	int r;
	
	r = match_regex_hex(data->regex[0], d->VendorID);
	if (r != 1) {
		return r;
	}
	r = match_regex_hex(data->regex[1], d->ProductID);
	if (r != 1) {
		return r;
	}
	r = match_regex(data->regex[2], d->Vendor);
	if (r != 1) {
		return r;
	}
	r = match_regex(data->regex[3], d->Product);
	if (r != 1) {
		return r;
	}
	r = match_regex(data->regex[4], d->Serial);
	if (r != 1) {
		return r;
	}
	r = match_regex(data->regex[5], d->Bus);
	if (r != 1) {
		return r;
	}
	return 1;
}

/* constructor: create a regular expression matcher. This matcher is
   based on six regular expression strings in regex_array[0..5],
   corresponding to: vendorid, productid, vendor, product, serial,
   bus. Any of these strings can be NULL, which matches
   everything. Cflags are as in regcomp(3). Typical values for cflags
   are REG_ICASE (case insensitive matching) and REG_EXTENDED (use
   extended regular expressions).  On success, return 0 and store the
   matcher in *matcher. On error, return -1 with errno set, or return
   i=1--5 to indicate that the regular expression regex_array[i] was
   ill-formed (an error message can then be retrieved with
   regerror(3)). */
int new_regex_matcher(HIDDeviceMatcher_t **matcher, char *regex_array[6], int cflags) {
	HIDDeviceMatcher_t *m = NULL;
	regex_matcher_data_t *data = NULL;
	int r, i;

	m = (HIDDeviceMatcher_t *)malloc(sizeof(HIDDeviceMatcher_t));
	if (!m) {
		return -1;
	}
	data = (regex_matcher_data_t *)malloc(sizeof(regex_matcher_data_t));
	if (!data) {
		free(m);
		return -1;
	}
	for (i=0; i<6; i++) {
		r = compile_regex(&data->regex[i], regex_array[i], cflags);
		if (r==-2) {
			r = i;
		}
		if (r) {
			free(m);
			free(data);
			return r;
		}
	}

	m->match_function = &match_function_regex;
	m->privdata = (void *)data;
	m->next = NULL;
	*matcher = m;
	return 0;
}

void free_regex_matcher(HIDDeviceMatcher_t *matcher) {
	int i;
	regex_matcher_data_t *data;
	
	if (matcher) {
		data = (regex_matcher_data_t *)matcher->privdata;
		for (i=0; i<6; i++) {
			if (data->regex[i]) {
				regfree(data->regex[i]);
				free(data->regex[i]);
			}
		}
		free(data);
		free(matcher);
	}
}

/* ---------------------------------------------------------------------- */


void HIDDumpTree(HIDDevice *hd)
{
	int 		i;
	char 		path[128], type[10];
	float		value;
	HIDData 	tmpData;
	HIDParser 	tmpParser;

	while (HIDParse(&hParser, &tmpData) != FALSE)
	{
		/* Build the path */
		path[0] = '\0';
		for (i = 0; i < tmpData.Path.Size; i++)
		{
		  strcat(path, hid_lookup_path((tmpData.Path.Node[i].UPage * 0x10000) + tmpData.Path.Node[i].Usage));
			if (i < (tmpData.Path.Size - 1))
				strcat (path, ".");
		}

		/* Get data type */
		type[0] = '\0';
		switch (tmpData.Type)
		{
			case ITEM_FEATURE:
				strcat(type, "Feature");
				break;
			case ITEM_INPUT:
				strcat(type, "Input");
				break;
			case ITEM_OUTPUT:
				strcat(type, "Output");
				break;
			default:
				strcat(type, "Unknown");
				break;
		}

		/* FIXME: enhance this or fix/change the HID parser (see libhid project) */
		if ( strstr(path, "000000") == NULL) {
			/* Backup shared data */
			memcpy(&tmpData, &hData, sizeof (hData));
			memcpy(&tmpParser, &hParser, sizeof (hParser));

			/* Get data value */
			if (HIDGetItemValue(path, &value) > 0)
				TRACE(1, "Path: %s, Type: %s, Value: %f", path, type, value);
			
			else
				TRACE(1, "Path: %s, Type: %s", path, type);

			/* Restore shared data */
			memcpy(&hData, &tmpData, sizeof (tmpData));
			memcpy(&hParser, &tmpParser, sizeof (tmpParser));
		}
	}
}

/* Matcher is a linked list of matchers (see libhid.h), and the opened
    device must match all of them. */
HIDDevice *HIDOpenDevice(HIDDeviceMatcher_t *matcher, int mode)
{
	int ReportSize;

	if ( mode == MODE_REOPEN )
	{
		TRACE(2, "Reopening device");
	}

	/* get and parse descriptors (dev, cfg and report) */
	ReportSize = libusb_open(&curDevice, matcher, ReportDesc, mode);

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
		/* don't throw away first item */
		//HIDParse(&hParser, &hData);
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
			/* Bufferize at least the last report */
			if ( ( (prev_report == hData.ReportID) && (time(NULL) <= (prev_report_ts + MAX_TS)) )
				|| ((replen=libusb_get_report(hData.ReportID, raw_buf, REPORT_SIZE)) > 0) )
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
				TRACE(2, "Can't retrieve Report %i (%i/%i): %s", hData.ReportID, replen, errno, strerror(errno));
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
      if (libusb_get_report(hData.ReportID, raw_buf, REPORT_SIZE) > 0) { 
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
	
	upsdebugx(2, "Waiting for notifications...");
	
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

void HIDCloseDevice()
{
	TRACE(2, "Closing device");
	libusb_close();
}


/*******************************************************
 * Support functions
 *******************************************************/

#define MAX_STRING      		64

static void logical_to_physical(HIDData *Data)
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

static void physical_to_logical(HIDData *Data)
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

static long get_unit_expo(long UnitType)
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
static float expo(int a, int b)
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
static ushort lookup_path(char *HIDpath, HIDData *data)
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
	  for (i = 1; i < hData.Path.Size; i++)
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
/* FIXME: put as external data, like in usb.ids (or use
 * this last?) */
typedef struct {
	const char *usage_name;
	unsigned int usage_code;
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
	{  "ShutdownRequested",			0x00840068 },
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
	{ "VoltageNotRegulated",		0x008500db },
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
	{ "APCStatusFlag",			0xff860060 },
	{ "APCPanelTest",			0xff860072 }, /* FIXME: need to be exploited */
	{ "APCShutdownAfterDelay",		0xff860076 }, /* FIXME: need to be exploited */
	{ "APC_USB_FirmwareRevision",		0xff860079 }, /* FIXME: need to be exploited */
	{ "APCForceShutdown",			0xff86007c },
	{ "APCDelayBeforeShutdown",		0xff86007d },
	{ "APCDelayBeforeStartup",		0xff86007e }, /* FIXME: need to be exploited */

	/* FIXME: The below one seems to have been wrongly encoded by Belkin */
	/* Pages 84 to 88 are reserved for official HID definition! */

	{ "BELKINConfig",			0x00860026 },
	{ "BELKINConfigVoltage",		0x00860040 }, /* (V) */
	{ "BELKINConfigFrequency",		0x00860042 }, /* (Hz) */
	{ "BELKINConfigApparentPower",		0x00860043 }, /* (VA) */
	{ "BELKINConfigBatteryVoltage",		0x00860044 }, /* (V) */
	{ "BELKINConfigOverloadTransfer",	0x00860045 }, /* (%) */
	{ "BELKINLowVoltageTransfer",		0x00860053 }, /* R/W (V) */
	{ "BELKINHighVoltageTransfer",		0x00860054 }, /* R/W (V)*/
	{ "BELKINLowVoltageTransferMax",	0x0086005b }, /* (V) */
	{ "BELKINLowVoltageTransferMin",	0x0086005c }, /* (V) */
	{ "BELKINHighVoltageTransferMax",	0x0086005d }, /* (V) */
	{ "BELKINHighVoltageTransferMin",	0x0086005e }, /* (V) */

	{ "BELKINControls",			0x00860027 },
	{ "BELKINLoadOn",			0x00860050 }, /* R/W: write: 1=do action. Read: 0=none, 1=started, 2=in progress, 3=complete */
	{ "BELKINLoadOff",			0x00860051 }, /* R/W: ditto */
	{ "BELKINLoadToggle",			0x00860052 }, /* R/W: ditto */
	{ "BELKINDefaultShutdown",		0x00860055 }, /* R/W: write: 0=start shutdown using default delay. */
	{ "BELKINDelayBeforeStartup",		0x00860056 }, /* R/W (minutes) */
	{ "BELKINDelayBeforeShutdown",		0x00860057 }, /* R/W (seconds) */
	{ "BELKINTest",				0x00860058 }, /* R/W: write: 0=no test, 1=quick test, 2=deep test, 3=abort test. Read: 0=no test, 1=passed, 2=warning, 3=error, 4=abort, 5=in progress */
	{ "BELKINAudibleAlarmControl",		0x0086005a }, /* R/W: 1=disabled, 2=enabled, 3=muted */
	
	{ "BELKINDevice",			0x00860029 },
	{ "BELKINVoltageSensitivity",		0x00860074 }, /* R/W: 0=normal, 1=reduced, 2=low */
	{ "BELKINModelString",			0x00860075 },
	{ "BELKINModelStringOffset",		0x00860076 }, /* offset of Model name in Model String */
	{ "BELKINUPSType",			0x0086007c }, /* high nibble: firmware version. Low nibble: 0=online, 1=offline, 2=line-interactive, 3=simple online, 4=simple offline, 5=simple line-interactive */

	{ "BELKINPowerState",			0x0086002a },
	{ "BELKINInput",			0x0086001a },
	{ "BELKINOutput",			0x0086001c },
	{ "BELKINBatterySystem",		0x00860010 },
	{ "BELKINVoltage",			0x00860030 }, /* (0.1 Volt) */
	{ "BELKINFrequency",			0x00860032 }, /* (0.1 Hz) */
	{ "BELKINPower",			0x00860034 }, /* (Watt) */
	{ "BELKINPercentLoad",			0x00860035 }, /* (%) */
	{ "BELKINTemperature",			0x00860036 }, /* (Celsius) */
	{ "BELKINCharge",			0x00860039 }, /* (%) */
	{ "BELKINRunTimeToEmpty",		0x0086006c }, /* (minutes) */

	{ "BELKINStatus",			0x00860028 },
	{ "BELKINBatteryStatus",		0x00860022 }, /* 1 byte: bit2=low battery, bit4=charging, bit5=discharging, bit6=battery empty, bit7=replace battery */
	{ "BELKINPowerStatus",			0x00860021 }, /* 2 bytes: bit0=ac failure, bit4=overload, bit5=load is off, bit6=overheat, bit7=UPS fault, bit13=awaiting power, bit15=alarm status */

	/* FIXME: The below one seems to have been wrongly encoded by APC */
	/* FIXME: This also overlaps with Belkin */
	/* FIXME: what is BUP anyway? */
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

static const char *hid_lookup_path(unsigned int usage)
{
	int i;
	static char raw_usage[10];
	
	TRACE(3, "Looking up %08x", usage);

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

static int hid_lookup_usage(char *name)
{
	int i;
	int value;
	char buf[20];
	
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
	/* finally, translate unnamed path components such as
	   "ff860024" */
	value = strtoul(name, NULL, 16);
	sprintf(buf, "%08x", value);
	if (strcasecmp(buf, name) != 0) {
	  return -1;
	}
	return value;
}

int get_current_data_attribute()
{
	return hData.Attribute;
}
#define NIBBLE(_i)    (((_i) < 10) ? '0' + (_i) : 'A' + (_i) - 10)

static void dump_hex (const char *msg, const unsigned char *buf, int len)
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
