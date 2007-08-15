/*!
 * @file libhid.c
 * @brief HID Library - User API (Generic HID Access using MGE HIDParser)
 *
 * @author Copyright (C) 2003 - 2005
 *	Arnaud Quette <arnaud.quette@free.fr> && <arnaud.quette@mgeups.com>
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
#include "common.h" /* for xmalloc, upsdebugx prototypes */

/* Communication layers and drivers (USB and MGE SHUT) */
#ifdef SHUT_MODE
	#include "libshut.h"
	communication_subdriver_t *comm_driver = &shut_subdriver;
#else
	#include "libusb.h"
	communication_subdriver_t *comm_driver = &usb_subdriver;
#endif

#include <errno.h>

/* support functions */
static float logical_to_physical(HIDData_t *Data, long logical);
static long physical_to_logical(HIDData_t *Data, float physical);
static const char *hid_lookup_path(unsigned int usage, usage_tables_t *utab);
static int hid_lookup_usage(char *name, usage_tables_t *utab);
static int string_to_path(char *HIDpath, HIDPath_t *path, usage_tables_t *utab);
static int path_to_string(char *HIDpath, HIDPath_t *path, usage_tables_t *utab);
static long get_unit_expo(long UnitType);
static float expo(int a, int b);

/* report buffer structure: holds data about most recent report for
   each given report id */
struct reportbuf_s {
       time_t ts[256];           /* timestamp when report was retrieved */
       int len[256];             /* size of report data */
       unsigned char *data[256]; /* report data (allocated) */
};
typedef struct reportbuf_s reportbuf_t;

/* global variables */

static HIDDesc_t         *pDesc = NULL; /* parsed Report Descriptor */
static reportbuf_t      *rbuf = NULL;  /* buffer for most recent reports */

#define min(x,y) ((x)>(y) ? (y) : (x))

/* ---------------------------------------------------------------------- */
/* report buffering system */

/* HID data items are retrieved via "reports". Each report is
   identified by a report ID, which is an integer in the range
   0-255. Each report can hold several items. To avoid retrieving a
   given report multiple times in short succession, we use a data
   structure called a "report buffer". The functions in this group
   operate on entire *reports*, not individual data items. */

/* allocate a new report buffer. Return pointer on success, else NULL
   with errno set. The returned data structure must later be freed
   with free_report_buffer(). */
reportbuf_t *new_report_buffer(void) {
	return calloc(1, sizeof(reportbuf_t));
}

void free_report_buffer(reportbuf_t *rbuf) {
	int i;

	if (!rbuf)
		return;

	for (i=0; i<256; i++) {
		free(rbuf->data[i]);
	}
	free(rbuf);
}

/* refresh the report with the given id in the report buffer rbuf.  If
   the report is not yet in the buffer, or if it is older than "age"
   seconds, then the report is freshly read from the USB
   device. Otherwise, it is unchanged. Return 0 on success, -1 on
   error with errno set. */
int refresh_report_buffer(reportbuf_t *rbuf, int id, int age, HIDDesc_t *pDesc, hid_dev_handle_t *udev) {
	int len = pDesc->replen[id]; /* length of report */
	unsigned char *data;
	int r;

	if (rbuf->data[id] != NULL && rbuf->ts[id] + age > time(0)) {
		/* buffered report is still good; nothing to do */
		return 0;
	}

	data = calloc(1, len+1); /* first byte holds report id */
	if (!data) {
		return -1;
	}
	r = comm_driver->get_report(udev, id, data, len+1);
	if (r <= 0) {
		return -1;
	}
	/* have valid report */
	free(rbuf->data[id]);
	rbuf->data[id] = data;
	rbuf->ts[id] = time(0);
	rbuf->len[id] = r;  /* normally equal to len+1, but could be less? */

	upsdebug_hex (3, "Report[r]", rbuf->data[id], rbuf->len[id]);
	
	return 0;
}

/* send the report with the given id to the HID device. Return 0 on
 * success, or -1 on failure with errno set. */
int set_report_from_buffer(reportbuf_t *rbuf, int id, hid_dev_handle_t *udev) {
	int r;

	r = comm_driver->set_report(udev, id, rbuf->data[id], rbuf->len[id]);
	if (r <= 0) {
		return -1;
	}
	return 0;
}	

/* file a given report in the report buffer. This is used when the
   report has been obtained without having been explicitly requested,
   e.g., it arrived through an interrupt transfer. Returns 0 on
   success, -1 on error with errno set. Note: len >= 1, and the first
   byte holds the report id. */
int file_report_buffer(reportbuf_t *rbuf, u_char *report, int len) {
	unsigned char *data;
	int id;

	id = report[0];
	data = malloc(len);
	if (!data) {
		return -1;
	}
	memcpy(data, report, len);

	/* have valid report */
	free(rbuf->data[id]);
	rbuf->data[id] = data;
	rbuf->ts[id] = time(0);
	rbuf->len[id] = len;

	upsdebug_hex (3, "Report[i]", rbuf->data[id], rbuf->len[id]);
	
	return 0;
}

/* ---------------------------------------------------------------------- */
/* the functions in this next group operate on buffered reports, but
   operate on individual items, not whole reports. */

/* read the logical value for the given pData. No logical to physical
   conversion is performed. If age>0, the read operation is buffered
   if the item's age is less than "age". On success, return 0 and
   store the answer in *value. On failure, return -1 and set errno. */
int get_item_buffered(reportbuf_t *rbuf, HIDData_t *pData, int age, HIDDesc_t *pDesc, hid_dev_handle_t *udev, long *Value) {
	int r;
	int id;

	id = pData->ReportID;

	r = refresh_report_buffer(rbuf, id, age, pDesc, udev);
	if (r<0) {
		return -1;
	}
	GetValue(rbuf->data[id], pData, Value);
	return 0;
}

/* set the logical value for the given pData. No physical to logical
   conversion is performed. On success, return 0, and failure, return
   -1 and set errno. The updated value is sent to the device, and also
   stored in the local buffer. */
int set_item_buffered(reportbuf_t *rbuf, HIDData_t *pData, HIDDesc_t *pDesc, hid_dev_handle_t *udev, long Value) {
	int id, r;

	id = pData->ReportID;

	SetValue(pData, rbuf->data[id], Value);
	r = set_report_from_buffer(rbuf, id, udev);
	return r;
}

/* ---------------------------------------------------------------------- */

/* FIXME: we currently "hard-wire" the report buffer size in the calls
   to libxxx_get_report() below to 8 bytes. This is not really a great
   idea, but it is necessary because Belkin models will crash,
   sometimes with permanent firmware damage, if called with a larger
   buffer size (never mind the USB specification). Let's hope for now
   that no other UPS needs a buffer greater than 8. Ideally, the
   libhid library should calculate the *exact* size of the required
   report buffer from the report descriptor. */
#define REPORT_SIZE 8

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
static int match_function_exact(HIDDevice_t *d, void *privdata) {
	HIDDevice_t *data = (HIDDevice_t *)privdata;
	
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

/* constructor: return a new matcher that matches the exact HIDDevice_t
 * d. Return NULL with errno set on error. */
HIDDeviceMatcher_t *new_exact_matcher(HIDDevice_t *d) {
	HIDDeviceMatcher_t *m;
	HIDDevice_t *data;

	m = (HIDDeviceMatcher_t *)malloc(sizeof(HIDDeviceMatcher_t));
	if (!m) {
		return NULL;
	}
	data = (HIDDevice_t *)malloc(sizeof(HIDDevice_t));
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
	HIDDevice_t *data;

	if (matcher) {
		data = (HIDDevice_t *)matcher->privdata;
		
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
	snprintf(buf, sizeof(buf), "%04x", n);
	return match_regex(preg, buf);
}

/* private data type: hold a set of compiled regular expressions. */
struct regex_matcher_data_s {
	regex_t *regex[6];
};
typedef struct regex_matcher_data_s regex_matcher_data_t;

/* private callback function for regex matches */
static int match_function_regex(HIDDevice_t *d, void *privdata) {
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

/* CAUTION: be careful when modifying the output format of this function,
 * since it's used to produce sub-drivers "stub" using
 * scripts/subdriver/path-to-subdriver.sh
 */
void HIDDumpTree(hid_dev_handle_t *udev, usage_tables_t *utab)
{
	int 		j;
	char 		path[128], type[10];
	float		value;
	HIDData_t 	*pData;

	for (j=0; j<pDesc->nitems; j++)
	{
		pData = &pDesc->item[j];

		/* Build the path */
		path_to_string(path, &pData->Path, utab);

		/* Get data type */
		type[0] = '\0';
		switch (pData->Type)
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

			/* Get data value */
			if (HIDGetItemValue(udev, path, &value, utab) > 0)
				upsdebugx(1, "Path: %s, Type: %s, ReportID: 0x%02x, Offset: %i, Size: %i, Value: %f",
				  path, type, pData->ReportID, pData->Offset, pData->Size, value);

			else
				upsdebugx(1, "Path: %s, Type: %s, ReportID: 0x%02x, Offset: %i, Size: %i",
				  path, type, pData->ReportID, pData->Offset, pData->Size);
		}
	}
}

/* Matcher is a linked list of matchers (see libhid.h), and the opened
    device must match all of them. On success, set *udevp and *hd and
    return hd. On failure, return NULL. Mode is MODE_OPEN or MODE_REOPEN. */
HIDDevice_t *HIDOpenDevice(hid_dev_handle_t **udevp, HIDDevice_t *hd, HIDDeviceMatcher_t *matcher, int mode)
{
	int ReportSize;
	unsigned char ReportDesc[4096];

	if ( mode == MODE_REOPEN )
	{
		upsdebugx(2, "Reopening device");
	}

	/* get and parse descriptors (dev, cfg and report) */
	ReportSize = comm_driver->open(udevp, hd, matcher, ReportDesc, mode);

	if (ReportSize == -1)
		return NULL;
	else
	{
		if ( mode == MODE_REOPEN )
		{
			upsdebugx(2, "Device reopened successfully");
			return hd;
		}
	
		upsdebugx(2, "Report Descriptor size = %d", ReportSize);
		upsdebug_hex(3, "Report Descriptor", ReportDesc, ReportSize);

		/* Parse Report Descriptor */
		Free_ReportDesc(pDesc);
		pDesc = Parse_ReportDesc(ReportDesc, ReportSize);
		if (!pDesc) {
			upsdebugx(0, "Failed to parse report descriptor: %s", strerror(errno));
			HIDCloseDevice(*udevp);
			*udevp = NULL;
			return NULL;
		}

		/* prepare report buffer */
		free_report_buffer(rbuf);
		rbuf = new_report_buffer();
		if (!rbuf) {
			upsdebugx(0, "Failed to allocate report buffer: %s", strerror(errno));
			HIDCloseDevice(*udevp);
			*udevp = NULL;
			return NULL;
		}
	}
	return hd;
}

/* return 1 if OK, 0 on fail, <= -1 otherwise (ie disconnect). TODO:
   return value should be checked. Returns the logical value
   associated with the given path in *Value (i.e., don't do any
   logical->physical conversion. Also returns pointer to the
   corresponding HIDData_t item in *ppData, if ppData!=NULL. */
static int HIDGetItemLogical(hid_dev_handle_t *udev, char *path, usage_tables_t *utab, long *Value, HIDData_t **ppData)
{
	int i, r;
	long hValue;
	HIDData_t *pData;
	HIDPath_t Path;

	r = string_to_path(path, &Path, utab);
	if (r <= 0) {
		return 0;
	}

	upsdebugx(4, "Path depth = %i", Path.Size);
	
	for (i = 0; i<Path.Size; i++) {
		upsdebugx(4, "%i: Usage(%08x)", i, Path.Node[i]);
	} 

	/* Get info on object (reportID, offset and size) */
	pData = FindObject_with_Path(pDesc, &Path, ITEM_FEATURE);
	if (!pData) {
		upsdebugx(2, "Can't find object %s", path);
		return 0;
	} 
	r = get_item_buffered(rbuf, pData, MAX_TS, pDesc, udev, &hValue);
	if (r<0) {
		upsdebugx(2, "Can't retrieve Report %i (%i): %s", pData->ReportID, errno, strerror(errno));
		return -errno;
	}

	*Value = hValue;
	if (ppData != NULL) {
		*ppData = pData;
	}
	return 1;
}

/* return 1 if OK, 0 on fail, -errno otherwise (ie disconnect). TODO:
   return value should be checked. Return the physical value
   associated with the given path. */
int HIDGetItemValue(hid_dev_handle_t *udev, char *path, float *Value, usage_tables_t *utab)
{
	int r;
	float physical;
	long hValue;
	HIDData_t *pData;

	r = HIDGetItemLogical(udev, path, utab, &hValue, &pData);
	if (r <= 0) {
		return r;
	}
	
	upsdebugx(4, "=>> Before exponent: %ld, %i/%i)", hValue,
	      (int)pData->UnitExp, (int)get_unit_expo(pData->Unit) );
	
	/* Convert Logical Min, Max and Value into Physical */
	physical = logical_to_physical(pData, hValue);
	
	/* Process exponents and units */
	physical *= (float) expo(10,(int)pData->UnitExp - get_unit_expo(pData->Unit));
	hValue = (long) physical;

	upsdebugx(4, "=>> After conversion: %f (%ld), %i/%i)", physical,
	      hValue, (int)pData->UnitExp, (int)get_unit_expo(pData->Unit));
	
	*Value = physical;
	return 1;
}

/* rawbuf must point to a large enough buffer to hold the resulting
 * string. Return pointer to rawbuf on success, NULL on failure. */
char *HIDGetItemString(hid_dev_handle_t *udev, char *path, char *rawbuf, usage_tables_t *utab)
{
	int r;
	long hValue;  
	
	r = HIDGetItemLogical(udev, path, utab, &hValue, NULL);
	if (r <= 0) {
		return NULL;
	}
	
	/* now get string */
	comm_driver->get_string(udev, hValue, rawbuf);
	return rawbuf;
}

/* set the given physical value for the variable associated with
 * path. Return TRUE on success, FALSE on failure. */ 
bool_t HIDSetItemValue(hid_dev_handle_t *udev, char *path, float value, usage_tables_t *utab)
{
	float Value;
	int i, r;
	long hValue, oldValue, newValue;
	HIDData_t *pData;
	int id;

	r = HIDGetItemLogical(udev, path, utab, &oldValue, &pData);
	if (r <= 0) {
		return FALSE;
	}

	upsdebugx(4, "=>> SET: Before set: %ld (%ld)", oldValue, (long)value);
	
	id = pData->ReportID;

	/* Test if Item is settable */
        /* FIXME: not constant == volatile, but
         * it doesn't imply that it's RW! */
	if (pData->Attribute == ATTR_DATA_CST) {
		return FALSE;
	}

	/* Set new value for this item */
	/* And Process exponents restoration */
	Value = value * expo(10, get_unit_expo(pData->Unit) - (int)pData->UnitExp);
	
	upsdebugx(4, "=>> SET: after exp: %.2f (exp = %.2f)", Value,
	      expo(10, (int)get_unit_expo(pData->Unit) - (int)pData->UnitExp));
	
	/* convert physical value to logical */
	hValue = physical_to_logical(pData, Value);
	upsdebugx(4, "=>> SET: after PL: %ld", hValue);
	
	upsdebug_hex (4, "==> Report to set", rbuf->data[id], rbuf->len[id]);

	r = set_item_buffered(rbuf, pData, pDesc, udev, hValue);
	if (r<0) {
		upsdebugx(2, "Set report failed: (%d): %s", errno, strerror(errno));
		return FALSE;
	}

	/* flush the report buffer (data may have changed) */
	for (i=0; i<256; i++) {
		free(rbuf->data[i]);
		rbuf->data[i] = NULL;
		rbuf->ts[i] = 0;
		rbuf->len[i] = 0;
	}
	
	/* re-read report without buffering */
	r = get_item_buffered(rbuf, pData, 0, pDesc, udev, &newValue);
	if (r<0) {
		upsdebugx(2, "Warning: Re-read report failed: (%d): %s", errno, strerror(errno));
		return TRUE;
	}
	upsdebug_hex (4, "==> Report after set", rbuf->data[id], rbuf->len[id]);

	if (newValue != hValue) {
		/* this is normal; device will usually correct "out of
		   range" values */
		upsdebugx(4, "Wrote %ld, got %ld\n", hValue, newValue);
	}

	upsdebugx(4, "Set report succeeded");
	return TRUE;
}

void HIDFreeEvents(HIDEvent_t *events) {
	HIDEvent_t *p;

	while (events) {
		p = events->next; /* must copy this before free(events)! */
		free(events->Path);
		free(events);
		events = p;
	}
}

/* FIXME: change this so that we iterate through the report descriptor
   once, instead of once for every offset. Simply pick out the items
   with the correct ReportID and Type! On success, return item count
   >=0 and set *eventsListp. On error, return <0. Note: on success, an
   allocated events list will be returned in *eventsListp that must
   later be freed by the caller using HIDFreeEvents(). */
int HIDGetEvents(hid_dev_handle_t *udev, HIDDevice_t *dev, HIDEvent_t **eventsListp, usage_tables_t *utab)
{
	unsigned char buf[100];
	char itemPath[128];
	int size, itemCount;
	long hValue;	
	HIDData_t *pData;
	int id, r, i;
	HIDEvent_t *root = NULL;
	HIDEvent_t **hook = &root;
	HIDEvent_t *p;

	upsdebugx(2, "Waiting for notifications...");
	
	/* needs libusb-0.1.8 to work => use ifdef and autoconf */
	if ((size = comm_driver->get_interrupt(udev, &buf[0], 100, 5000)) <= 0)
	{
		return size; /* propagate "error" or "no event" code */
	}
	upsdebug_hex (3, "Notification", buf, size);

	r = file_report_buffer(rbuf, buf, size);
	if (r < 0) {
		upsdebugx(2, "Failed to buffer report: %s", strerror(errno));
		return -errno;
	}

	/* now read all items that are part of this report */
	id = buf[0];

	itemCount = 0;
	for (i=0; i<pDesc->nitems; i++) {
		pData = &pDesc->item[i];

		if (pData->Type != ITEM_INPUT || pData->ReportID != id) {
			continue;
		}
		path_to_string(itemPath, &pData->Path, utab);
		GetValue(buf, pData, &hValue);
		
		upsdebugx(3, "Object: %s = %ld", itemPath, hValue);

		/* FIXME: enhance this or fix/change the HID parser
		   (see libhid project) */
		p = (HIDEvent_t *)malloc(sizeof (HIDEvent_t));
		if (!p) {
			HIDFreeEvents(root);
			return -errno;
		}
		p->Path = strdup(itemPath);
		p->Value = hValue;
		p->next = NULL;
		*hook = p;
		hook = &p->next;
		itemCount++;
	}
	*eventsListp = root;
	return itemCount;
}

void HIDCloseDevice(hid_dev_handle_t *udev)
{
    if (udev != NULL)
    {
      upsdebugx(2, "Closing device");
      free_report_buffer(rbuf);
      comm_driver->close(udev);
    }
}


/*******************************************************
 * Support functions
 *******************************************************/

#define MAX_STRING      		64

static float logical_to_physical(HIDData_t *Data, long logical)
{
	float physical;
	float Factor;

	upsdebugx(4, "PhyMax = %ld, PhyMin = %ld, LogMax = %ld, LogMin = %ld",
		Data->PhyMax, Data->PhyMin, Data->LogMax, Data->LogMin);

	/* HID spec says that if one or both are undefined, or if they are
	 * both 0, then PhyMin = LogMin, PhyMax = LogMax. */
	if (!Data->have_PhyMax || !Data->have_PhyMin ||
		(Data->PhyMax == 0 && Data->PhyMin == 0))
	{
		return (float)logical;
	}

	/* Paranoia */
	if ((Data->PhyMax <= Data->PhyMin) || (Data->LogMax <= Data->LogMin))
	{
		/* this should not really happen */
		return (float)logical;
	}
	
	Factor = (float)(Data->PhyMax - Data->PhyMin) / (Data->LogMax - Data->LogMin);
	/* Convert Value */
	physical = (float)((logical - Data->LogMin) * Factor) + Data->PhyMin;

	if (physical > Data->PhyMax) {
		return Data->PhyMax;
	}

	if (physical < Data->PhyMin) {
		return Data->PhyMin;
	}

	return physical;
}

static long physical_to_logical(HIDData_t *Data, float physical)
{
	long logical;
	float Factor;

	upsdebugx(4, "PhyMax = %ld, PhyMin = %ld, LogMax = %ld, LogMin = %ld",
		Data->PhyMax, Data->PhyMin, Data->LogMax, Data->LogMin);

	/* HID spec says that if one or both are undefined, or if they are
	 * both 0, then PhyMin = LogMin, PhyMax = LogMax. */
	if (!Data->have_PhyMax || !Data->have_PhyMin ||
		(Data->PhyMax == 0 && Data->PhyMin == 0))
	{
		return (long)physical;
	}

	/* Paranoia */
	if ((Data->PhyMax <= Data->PhyMin) || (Data->LogMax <= Data->LogMin))
	{
		/* this should not really happen */
		return (long)physical;
	}
	
	Factor = (float)(Data->LogMax - Data->LogMin) / (Data->PhyMax - Data->PhyMin);
	/* Convert Value */
	logical = (long)((physical - Data->PhyMin) * Factor) + Data->LogMin;

	if (logical > Data->LogMax)
		return Data->LogMax;

	if (logical < Data->LogMin)
		return Data->LogMin;

	return logical;
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

/* translate HID string path to numeric path and return path depth */
/* TODO: use usbutils functions (need to be externalised!) */
static int string_to_path(char *HIDpath, HIDPath_t *path, usage_tables_t *utab)
{
	int i = 0, cond = 1;
	int cur_usage;
	char buf[MAX_STRING];
	char *start, *end; 
	
	upsdebugx(5, "entering string_to_path()");
	
	strncpy(buf, HIDpath, min(strlen(HIDpath)+1, MAX_STRING));
	buf[MAX_STRING-1] = '\0';
	start = end = buf;
	
	while (cond) {
		
		if ((end = strchr(start, '.')) == NULL) {
			cond = 0;			
		}
		else
			*end = '\0';
		
		upsdebugx(4, "parsing %s", start);
		
		/* lookup code */
		if ((cur_usage = hid_lookup_usage(start, utab)) == -1) {
			upsdebugx(4, "%s wasn't found", start);
			return 0;
		}
		else {
			path->Node[i] = cur_usage;
			i++;
		}
		
		if(cond)
			start = end +1 ;
	}
	path->Size = i;
	
	return i;
}

/* translate HID numeric path to string path and return path depth */
/* TODO: use usbutils functions (need to be externalised!) */
/* FIXME: char *HIDpath must be large enough; buffer overflow otherwise. */
static int path_to_string(char *HIDpath, HIDPath_t *path, usage_tables_t *utab)
{
	int i = 0;
	char buf[20];
	
	upsdebugx(5, "entering path_to_string()");
	
	HIDpath[0] = '\0';
	
	/* Numeric to String */
	for (i = 0; i < path->Size; i++)
	{
		/* Deal with ?bogus? 
		if (path->Node[i] == 0)
			continue;
		*/
		
		/* manage indexed collection */
		if ((path->Node[i] & 0xffff0000) == 0x00ff0000)
		{
			upsdebugx(5, "Got an indexed collection");
			snprintf(buf, sizeof(buf), "[%i]", path->Node[i] & 0x0000ffff);
			strcat (HIDpath, buf);
		}
		else
		{
			strcat(HIDpath, hid_lookup_path(path->Node[i], utab));
		}
		if (i < (path->Size - 1))
			strcat (HIDpath, ".");
	}

	return i;
}

/* Lookup this usage name to find its code (page + index) */
/* temporary usage code lookup */
/* FIXME: put as external data, like in usb.ids (or use
 * this last?) */

/* Global usage table (from USB HID class definition) */
usage_lkp_t hid_usage_lkp[] = {
	/* Power Device Page */
	{  "Undefined",				0x00840000 },
	{  "iName",				0x00840001 },
	{  "PresentStatus",			0x00840002 },
	{  "ChangedStatus",			0x00840003 },
	{  "UPS",				0x00840004 },
	{  "PowerSupply",			0x00840005 },
	/* 0x00840006-0x0084000f	=>	Reserved */
	{  "BatterySystem",			0x00840010 },
	{  "BatterySystemID",			0x00840011 },
	{  "Battery",				0x00840012 },
	{  "BatteryID",				0x00840013 },
	{  "Charger",				0x00840014 },
	{  "ChargerID",				0x00840015 },
	{  "PowerConverter",			0x00840016 },
	{  "PowerConverterID",			0X00840017 },
	{  "OutletSystem",			0x00840018 },
	{  "OutletSystemID",			0x00840019 },
	{  "Input",				0x0084001a },
	{  "InputID",				0x0084001b },
	{  "Output",				0x0084001c },
	{  "OutputID",				0x0084001d },
	{  "Flow",				0x0084001e },
	{  "FlowID",				0x0084001f },
	{  "Outlet",				0x00840020 },
	{  "OutletID",				0x00840021 },
	{  "Gang",				0x00840022 },
	{  "GangID",				0x00840023 },
	{  "PowerSummary",			0x00840024 },
	{  "PowerSummaryID",			0x00840025 },
	/* 0x00840026-0x0084002f	=>	Reserved */
	{  "Voltage",				0x00840030 },
	{  "Current",				0x00840031 },
	{  "Frequency",				0x00840032 },
	{  "ApparentPower",			0x00840033 },
	{  "ActivePower",			0x00840034 },
	{  "PercentLoad",			0x00840035 },
	{  "Temperature",			0x00840036 },
	{  "Humidity",				0x00840037 },
	{  "BadCount",				0x00840038 },
	/* 0x00840039-0x0084003f	=>	Reserved */
	{  "ConfigVoltage",			0x00840040 },
	{  "ConfigCurrent",			0x00840041 },
	{  "ConfigFrequency",			0x00840042 },
	{  "ConfigApparentPower",		0x00840043 },
	{  "ConfigActivePower",			0x00840044 },
	{  "ConfigPercentLoad",			0x00840045 },
	{  "ConfigTemperature",			0x00840046 },
	{  "ConfigHumidity",			0x00840047 },
	/* 0x00840048-0x0084004f	=>	Reserved */
	{  "SwitchOnControl",			0x00840050 },
	{  "SwitchOffControl",			0x00840051 },
	{  "ToggleControl",			0x00840052 },
	{  "LowVoltageTransfer",		0x00840053 },
	{  "HighVoltageTransfer",		0x00840054 },	
	{  "DelayBeforeReboot",			0x00840055 },
	{  "DelayBeforeStartup",		0x00840056 },
	{  "DelayBeforeShutdown",		0x00840057 },
	{  "Test",				0x00840058 },
	{  "ModuleReset",			0x00840059 },
	{  "AudibleAlarmControl",		0x0084005a },
	/* 0x0084005b-0x0084005f	=>	Reserved */
	{  "Present",				0x00840060 },
	{  "Good",				0x00840061 },
	{  "InternalFailure",			0x00840062 },
	{  "VoltageOutOfRange",			0x00840063 },
	{  "FrequencyOutOfRange",		0x00840064 },
	{  "Overload",				0x00840065 }, 
        /* Note: the correct spelling is "Overload", not "OverLoad",
	 * according to the official specification, "Universal Serial
	 * Bus Usage Tables for HID Power Devices", Release 1.0,
	 * November 1, 1997 */
	{  "OverCharged",			0x00840066 },
	{  "OverTemperature", 			0x00840067 },
	{  "ShutdownRequested",			0x00840068 },
	{  "ShutdownImminent",			0x00840069 },
	{  "SwitchOn/Off",			0x0084006b },
	{  "Switchable",			0x0084006c },
	{  "Used",				0x0084006d },
	{  "Boost",				0x0084006e },
	{  "Buck",				0x0084006f },
	{  "Initialized",			0x00840070 },
	{  "Tested",				0x00840071 },
	{  "AwaitingPower",			0x00840072 },
	{  "CommunicationLost",			0x00840073 },
	/* 0x00840074-0x008400fc	=>	Reserved */
	{  "iManufacturer",			0x008400fd },
	{  "iProduct",				0x008400fe },
	{  "iSerialNumber",			0x008400ff },

	/* Battery System Page */
	{ "Undefined",				0x00850000 },
	{ "SMBBatteryMode",			0x00850001 },
	{ "SMBBatteryStatus",			0x00850002 },
	{ "SMBAlarmWarning",			0x00850003 },
	{ "SMBChargerMode",			0x00850004 },
	{ "SMBChargerStatus",			0x00850005 },
	{ "SMBChargerSpecInfo",			0x00850006 },
	{ "SMBSelectorState",			0x00850007 },
	{ "SMBSelectorPresets",			0x00850008 },
	{ "SMBSelectorInfo",			0x00850009 },
	/* 0x0085000A-0x0085000f	=>	Reserved */
	{ "OptionalMfgFunction1",		0x00850010 },
	{ "OptionalMfgFunction2",		0x00850011 },
	{ "OptionalMfgFunction3",		0x00850012 },
	{ "OptionalMfgFunction4",		0x00850013 },
	{ "OptionalMfgFunction5",		0x00850014 },
	{ "ConnectionToSMBus",			0x00850015 },
	{ "OutputConnection",			0x00850016 },
	{ "ChargerConnection",			0x00850017 },
	{ "BatteryInsertion",			0x00850018 },
	{ "Usenext",				0x00850019 },
	{ "OKToUse",				0x0085001a },
	{ "BatterySupported",			0x0085001b },
	{ "SelectorRevision",			0x0085001c },
	{ "ChargingIndicator",			0x0085001d },
	/* 0x0085001e-0x00850027	=>	Reserved */
	{ "ManufacturerAccess",			0x00850028 },
	{ "RemainingCapacityLimit",		0x00850029 },
	{ "RemainingTimeLimit",			0x0085002a },
	{ "AtRate",				0x0085002b },
	{ "CapacityMode",			0x0085002c },
	{ "BroadcastToCharger",			0x0085002d },
	{ "PrimaryBattery",			0x0085002e },
	{ "ChargeController",			0x0085002f },
	/* 0x00850030-0x0085003f	=>	Reserved */
	{ "TerminateCharge",			0x00850040 },
	{ "TerminateDischarge",			0x00850041 },
	{ "BelowRemainingCapacityLimit",	0x00850042 },
	{ "RemainingTimeLimitExpired",		0x00850043 },
	{ "Charging",				0x00850044 },
	{ "Discharging",			0x00850045 },
	{ "FullyCharged",			0x00850046 },
	{ "FullyDischarged",			0x00850047 },
	{ "ConditioningFlag",			0x00850048 },
	{ "AtRateOK",				0x00850049 },
	{ "SMBErrorCode",			0x0085004a },
	{ "NeedReplacement",			0x0085004b },
	/* 0x0085004c-0x0085005f	=>	Reserved */
	{ "AtRateTimeToFull",			0x00850060 },
	{ "AtRateTimeToEmpty",			0x00850061 },
	{ "AverageCurrent",			0x00850062 },
	{ "Maxerror",				0x00850063 },
	{ "RelativeStateOfCharge",		0x00850064 },
	{ "AbsoluteStateOfCharge",		0x00850065 },
	{ "RemainingCapacity",			0x00850066 },
	{ "FullChargeCapacity",			0x00850067 },
	{ "RunTimeToEmpty",			0x00850068 },
	{ "AverageTimeToEmpty",			0x00850069 },
	{ "AverageTimeToFull",			0x0085006a },
	{ "CycleCount",				0x0085006b },
	/* 0x0085006c-0x0085007f	=>	Reserved */
	{ "BattPackModelLevel",			0x00850080 },
	{ "InternalChargeController",		0x00850081 },
	{ "PrimaryBatterySupport",		0x00850082 },
	{ "DesignCapacity",			0x00850083 },
	{ "SpecificationInfo",			0x00850084 },
	{ "ManufacturerDate",			0x00850085 },
	{ "SerialNumber",			0x00850086 },
	{ "iManufacturerName",			0x00850087 },
	{ "iDevicename",			0x00850088 }, /* sic! */
	{ "iDeviceChemistry",			0x00850089 }, /* misspelled as "iDeviceChemistery" in spec. */
	{ "ManufacturerData",			0x0085008a },
	{ "Rechargeable",			0x0085008b },
	{ "WarningCapacityLimit",		0x0085008c },
	{ "CapacityGranularity1",		0x0085008d },
	{ "CapacityGranularity2",		0x0085008e },
	{ "iOEMInformation",			0x0085008f },
	/* 0x00850090-0x008500bf	=>	Reserved */
	{ "InhibitCharge",			0x008500c0 },
	{ "EnablePolling",			0x008500c1 },
	{ "ResetToZero",			0x008500c2 },
	/* 0x008500c3-0x008500cf	=>	Reserved */
	{ "ACPresent",				0x008500d0 },
	{ "BatteryPresent",			0x008500d1 },
	{ "PowerFail",				0x008500d2 },
	{ "AlarmInhibited",			0x008500d3 },
	{ "ThermistorUnderRange",		0x008500d4 },
	{ "ThermistorHot",			0x008500d5 },
	{ "ThermistorCold",			0x008500d6 },
	{ "ThermistorOverRange",		0x008500d7 },
	{ "VoltageOutOfRange",			0x008500d8 },
	{ "CurrentOutOfRange",			0x008500d9 },
	{ "CurrentNotRegulated",		0x008500da },
	{ "VoltageNotRegulated",		0x008500db },
	{ "MasterMode",				0x008500dc },
	/* 0x008500dd-0x008500ef	=>	Reserved */
	{ "ChargerSelectorSupport",		0x008500f0 },
	{ "ChargerSpec",			0x008500f1 },
	{ "Level2",				0x008500f2 },
	{ "Level3",				0x008500f3 },
	/* 0x008500f4-0x008500ff	=>	Reserved */

	/* end of structure. */
	{  "\0",				0x00000000 }
};

/* usage conversion numeric -> string */
static const char *hid_lookup_path(unsigned int usage, usage_tables_t *utab)
{
	int i, j;
	static char raw_usage[10];
	usage_lkp_t *table;

	upsdebugx(5, "Looking up %08x", usage);

	for (j=0; utab[j] != NULL; j++) {
		table = utab[j];
		for (i = 0; (table[i].usage_name[0] != '\0'); i++)
		{
			if (table[i].usage_code == usage)
				return table[i].usage_name;
		}
	}

	/* if the corresponding path isn't found,
		return the numeric usage in string form */
	snprintf(raw_usage, sizeof(raw_usage), "%08x", usage); 
	return &raw_usage[0];
}

/* usage conversion string -> numeric */
static int hid_lookup_usage(char *name, usage_tables_t *utab)
{
	int i, j;
	int value;
	char buf[20];
	usage_lkp_t *table;

	upsdebugx(5, "Looking up %s", name);
	
	if (name[0] == '[') { /* manage indexed collection */
		return (0x00FF0000 + atoi(&name[1]));
	}
	for (j=0; utab[j] != NULL; j++) {
		table = utab[j];
		for (i = 0; (table[i].usage_code != 0x0); i++)
		{
			if (!strcmp(table[i].usage_name, name))
			{
				upsdebugx(4, "hid_lookup_usage: found %04x",
				      table[i].usage_code);
				
				return table[i].usage_code;
			}
		}
	}
	/* finally, translate unnamed path components such as
	   "ff860024" */
	value = strtoul(name, NULL, 16);
	snprintf(buf, sizeof(buf), "%08x", value);
	if (strcasecmp(buf, name) != 0) {
		return -1;
	}
	return value;
}

