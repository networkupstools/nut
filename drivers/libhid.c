/*!
 * @file libhid.c
 * @brief HID Library - User API (Generic HID Access using MGE HIDParser)
 *
 * @author Copyright (C) 2003 - 2007
 *	Arnaud Quette <arnaud.quette@free.fr> && <arnaud.quette@mgeups.com>
 *	John Stamp <kinsayder@hotmail.com>
 *      Peter Selinger <selinger@users.sourceforge.net>
 *      Arjen de Korte <adkorte-guest@alioth.debian.org>
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
#include "libhid.h"
#include "hidparser.h"
#include "common.h" /* for xmalloc, upsdebugx prototypes */

/* Communication layers and drivers (USB and MGE SHUT) */
#ifdef SHUT_MODE
	#include "libshut.h"
	communication_subdriver_t *comm_driver = &shut_subdriver;
#else
	#include "nut_libusb.h"
	communication_subdriver_t *comm_driver = &usb_subdriver;
#endif

/* support functions */
static double logical_to_physical(HIDData_t *Data, long logical);
static long physical_to_logical(HIDData_t *Data, double physical);
static const char *hid_lookup_path(const HIDNode_t usage, usage_tables_t *utab);
static long hid_lookup_usage(const char *name, usage_tables_t *utab);
static int string_to_path(const char *string, HIDPath_t *path, usage_tables_t *utab);
static int path_to_string(char *string, size_t size, const HIDPath_t *path, usage_tables_t *utab);
static int8_t get_unit_expo(const HIDData_t *hiddata);
static double exponent(double a, int8_t b);

/* Tweak flag for APC Back-UPS */
size_t max_report_size = 0;

/* Tweaks for Powercom, at least */
int interrupt_only = 0;
size_t interrupt_size = 0;

/* ---------------------------------------------------------------------- */
/* report buffering system */

/* HID data items are retrieved via "reports". Each report is
   identified by a report ID, which is an integer in the range
   0-255. Each report can hold several items. To avoid retrieving a
   given report multiple times in short succession, we use a data
   structure called a "report buffer". The functions in this group
   operate on entire *reports*, not individual data items. */

void free_report_buffer(reportbuf_t *rbuf)
{
	int i;

	if (!rbuf)
		return;

	for (i=0; i<256; i++) {
		free(rbuf->data[i]);
	}

	free(rbuf);
}

/* allocate a new report buffer. Return pointer on success, else NULL
   with errno set. The returned data structure must later be freed
   with free_report_buffer(). */
reportbuf_t *new_report_buffer(HIDDesc_t *arg_pDesc)
{
	HIDData_t	*pData;
	reportbuf_t	*rbuf;
	int		id;
	size_t	i;

	if (!arg_pDesc)
		return NULL;

	rbuf = calloc(1, sizeof(*rbuf));
	if (!rbuf) {
		return NULL;
	}

	/* now go through all items that are part of this report */
	for (i=0; i<arg_pDesc->nitems; i++) {

		pData = &arg_pDesc->item[i];

		id = pData->ReportID;

		/* skip reports that already have been allocated */
		if (rbuf->data[id])
			continue;

		/* first byte holds id */
		rbuf->len[id] = arg_pDesc->replen[id] + 1;

		/* skip zero length reports */
		if (rbuf->len[id] < 1) {
			continue;
		}

		rbuf->data[id] = calloc(rbuf->len[id], sizeof(*(rbuf->data[id])));
		if (rbuf->data[id])
			continue;

		/* on failure, give up what we got so far */
		free_report_buffer(rbuf);
		return NULL;
	}

	return rbuf;
}

/* ---------------------------------------------------------------------- */
/* the functions in this next group operate on buffered reports, but
   operate on individual items, not whole reports. */

/* refresh the report with the given id in the report buffer rbuf.  If
   the report is not yet in the buffer, or if it is older than "age"
   seconds, then the report is freshly read from the USB
   device. Otherwise, it is unchanged.
   Return 0 on success, -1 on error with errno set. */
/* because buggy firmwares from APC return wrong report size, we either
   ask the report with the found report size or with the whole buffer size
   depending on the max_report_size flag */
static int refresh_report_buffer(reportbuf_t *rbuf, hid_dev_handle_t udev, HIDData_t *pData, time_t age)
{
	int	id = pData->ReportID;
	int	ret;
	size_t	r;

	if (interrupt_only || rbuf->ts[id] + age > time(NULL)) {
		/* buffered report is still good; nothing to do */
		upsdebug_hex(3, "Report[buf]", rbuf->data[id], rbuf->len[id]);
		return 0;
	}

	ret = comm_driver->get_report(udev, id, rbuf->data[id],
		max_report_size ? sizeof(rbuf->data[id]) : rbuf->len[id]);

	if (ret <= 0) {
		return -1;
	}
	r = (size_t)ret;

	if (rbuf->len[id] != r) {
		upsdebugx(2,
			"%s: expected %zu bytes, but got %zu instead",
			__func__, rbuf->len[id], r);
		upsdebug_hex(3, "Report[err]", rbuf->data[id], r);
	} else {
		upsdebug_hex(3, "Report[get]", rbuf->data[id], rbuf->len[id]);
	}

	/* have (valid) report */
	time(&rbuf->ts[id]);

	return 0;
}

/* read the logical value for the given pData. No logical to physical
   conversion is performed. If age>0, the read operation is buffered
   if the item's age is less than "age". On success, return 0 and
   store the answer in *value. On failure, return -1 and set errno. */
static int get_item_buffered(reportbuf_t *rbuf, hid_dev_handle_t udev, HIDData_t *pData, long *Value, time_t age)
{
	int id = pData->ReportID;
	int r;

	r = refresh_report_buffer(rbuf, udev, pData, age);
	if (r<0) {
		return -1;
	}

	GetValue(rbuf->data[id], pData, Value);

	return 0;
}

/* set the logical value for the given pData. No physical to logical
   conversion is performed. On success, return 0, and failure, return
   -1 and set errno. The updated value is sent to the device. */
static int set_item_buffered(reportbuf_t *rbuf, hid_dev_handle_t udev, HIDData_t *pData, long Value)
{
	int id = pData->ReportID;
	int r;

	SetValue(pData, rbuf->data[id], Value);

	r = comm_driver->set_report(udev, id, rbuf->data[id], rbuf->len[id]);
	if (r <= 0) {
		return -1;
	}

	upsdebug_hex(3, "Report[set]", rbuf->data[id], rbuf->len[id]);

	/* expire report */
	rbuf->ts[id] = 0;

	return 0;
}

/* file a given report in the report buffer. This is used when the
   report has been obtained without having been explicitly requested,
   e.g., it arrived through an interrupt transfer. Returns 0 on
   success, -1 on error with errno set. */
static int file_report_buffer(reportbuf_t *rbuf, unsigned char *buf, size_t buflen)
{
	int id = buf[0];

	/* broken report descriptors are common, so store whatever we can */
	memcpy(rbuf->data[id], buf, (buflen < rbuf->len[id]) ? buflen : rbuf->len[id]);

	if (rbuf->len[id] != buflen) {
		upsdebugx(2,
			"%s: expected %zu bytes, but got %zu instead",
			__func__, rbuf->len[id], buflen);
		upsdebug_hex(3, "Report[err]", buf, buflen);
	} else {
		upsdebug_hex(3, "Report[int]", rbuf->data[id], rbuf->len[id]);
	}

	/* have (valid) report */
	time(&rbuf->ts[id]);

	return 0;
}

/* ---------------------------------------------------------------------- */

/* Units and exponents table (HID PDC, 3.2.3) */
#define NB_HID_UNITS 10
static struct {
	const long	Type;
	const int8_t	Expo;
} HIDUnits[NB_HID_UNITS] = {
	{ 0x00000000, 0 },	/* None */
	{ 0x00F0D121, 7 },	/* Voltage */
	{ 0x00100001, 0 },	/* Ampere */
	{ 0x0000D121, 7 },	/* VA */
	{ 0x0000D121, 7 },	/* Watts */
	{ 0x00001001, 0 },	/* second */
	{ 0x00010001, 0 },	/* K */
	{ 0x00000000, 0 },	/* percent */
	{ 0x0000F001, 0 },	/* Hertz */
	{ 0x00101001, 0 },	/* As */
};

/* ---------------------------------------------------------------------- */

/* CAUTION: be careful when modifying the output format of this function,
 * since it's used to produce sub-drivers "stub" using
 * scripts/subdriver/gen-usbhid-subdriver.sh
 */
void HIDDumpTree(hid_dev_handle_t udev, HIDDevice_t *hd, usage_tables_t *utab)
{
	size_t	i;
#ifdef SHUT_MODE
	NUT_UNUSED_VARIABLE(hd);
#else
	/* extract the VendorId for further testing */
	int vendorID = hd->VendorID;
	int productID = hd->ProductID;
#endif

	/* Do not go further if we already know nothing will be displayed.
	 * Some reports take a while before they timeout, so if these are
	 * not used in the driver, they should only be fetched when we're
	 * in debug mode
	 */
	if (nut_debug_level < 1) {
		return;
	}

	upsdebugx(1, "%zu HID objects found", pDesc->nitems);

	for (i = 0; i < pDesc->nitems; i++)
	{
		double		value;
		HIDData_t	*pData = &pDesc->item[i];

		/* skip reports 254/255 for Eaton / MGE / Dell due to special handling needs */
#ifdef SHUT_MODE
		if ((pData->ReportID == 254) || (pData->ReportID == 255)) {
			continue;
		}
#else
		if ((vendorID == 0x0463) || (vendorID == 0x047c)) {
			if ((pData->ReportID == 254) || (pData->ReportID == 255)) {
				continue;
			}
		}

		/* skip report 0x54 for Tripplite SU3000LCD2UHV due to firmware bug */
		if ((vendorID == 0x09ae) && (productID == 0x1330)) {
			if (pData->ReportID == 0x54) {
				continue;
			}
		}
#endif

		/* Get data value */
		if (HIDGetDataValue(udev, pData, &value, MAX_TS) == 1) {
			upsdebugx(1, "Path: %s, Type: %s, ReportID: 0x%02x, Offset: %i, Size: %i, Value: %g",
				HIDGetDataItem(pData, utab), HIDDataType(pData), pData->ReportID, pData->Offset, pData->Size, value);
			continue;
		}

		upsdebugx(1, "Path: %s, Type: %s, ReportID: 0x%02x, Offset: %i, Size: %i",
			HIDGetDataItem(pData, utab), HIDDataType(pData), pData->ReportID, pData->Offset, pData->Size);
	}

	fflush(stdout);
}

/* Returns text string which can be used to display type of data
 */
const char *HIDDataType(const HIDData_t *hiddata)
{
	switch (hiddata->Type)
	{
	case ITEM_FEATURE:
		return "Feature";
	case ITEM_INPUT:
		return "Input";
	case ITEM_OUTPUT:
		return "Output";
	default:
		return "Unknown";
	}
}

/* Returns pointer to the corresponding HIDData_t item
 * or NULL if path is not found in report descriptor
 */
HIDData_t *HIDGetItemData(const char *hidpath, usage_tables_t *utab)
{
	int	r;
	HIDPath_t Path;

	r = string_to_path(hidpath, &Path, utab);
	if (r <= 0) {
		return NULL;
	}

	/* Get info on object (reportID, offset and size) */
	return FindObject_with_Path(pDesc, &Path, interrupt_only ? ITEM_INPUT:ITEM_FEATURE);
}

char *HIDGetDataItem(const HIDData_t *hiddata, usage_tables_t *utab)
{
	/* TODO: not thread safe! */
	static char itemPath[128];

	path_to_string(itemPath, sizeof(itemPath), &hiddata->Path, utab);

	return itemPath;
}

/* Return the physical value associated with the given HIDData path.
 * return 1 if OK, 0 on fail, -errno otherwise (ie disconnect).
 */
int HIDGetDataValue(hid_dev_handle_t udev, HIDData_t *hiddata, double *Value, time_t age)
{
	int	r;
	long	hValue;

	if (hiddata == NULL) {
		return 0;
	}

	r = get_item_buffered(reportbuf, udev, hiddata, &hValue, age);
	if (r<0) {
		upsdebug_with_errno(1, "Can't retrieve Report %02x", hiddata->ReportID);
		return -errno;
	}

	*Value = hValue;

	/* Convert Logical Min, Max and Value into Physical */
	*Value = logical_to_physical(hiddata, hValue);

	/* Process exponents and units */
	*Value *= exponent(10, get_unit_expo(hiddata));

	return 1;
}

/* Return the physical value associated with the given path.
 * return 1 if OK, 0 on fail, -errno otherwise (ie disconnect).
 */
int HIDGetItemValue(hid_dev_handle_t udev, const char *hidpath, double *Value, usage_tables_t *utab)
{
	return HIDGetDataValue(udev, HIDGetItemData(hidpath, utab), Value, MAX_TS);
}

/* Return pointer to indexed string (empty if not found)
 */
char *HIDGetIndexString(hid_dev_handle_t udev, const int Index, char *buf, size_t buflen)
{
	if (comm_driver->get_string(udev, Index, buf, buflen) < 1)
		buf[0] = '\0';

	return str_rtrim(buf, '\n');
}

/* Return pointer to indexed string from HID path (empty if not found)
 */
char *HIDGetItemString(hid_dev_handle_t udev, const char *hidpath, char *buf, size_t buflen, usage_tables_t *utab)
{
	double	Index;

	if (HIDGetDataValue(udev, HIDGetItemData(hidpath, utab), &Index, MAX_TS) != 1) {
		buf[0] = '\0';
		return buf;
	}

	return HIDGetIndexString(udev, Index, buf, buflen);
}

/* Set the given physical value for the variable associated with
 * path. return 1 if OK, 0 on fail, -errno otherwise (ie disconnect).
 */
int HIDSetDataValue(hid_dev_handle_t udev, HIDData_t *hiddata, double Value)
{
	int	r;
	long	hValue;

	if (hiddata == NULL) {
		return 0;
	}

	/* Test if Item is settable */
	/* FIXME: not constant == volatile, but
	* it doesn't imply that it's RW! */
	if (hiddata->Attribute == ATTR_DATA_CST) {
		return 0;
	}

	/* Process exponents and units */
	Value /= exponent(10, get_unit_expo(hiddata));

	/* Convert Physical Min, Max and Value into Logical */
	hValue = physical_to_logical(hiddata, Value);

	r = set_item_buffered(reportbuf, udev, hiddata, hValue);
	if (r<0) {
		upsdebug_with_errno(1, "Can't set Report %02x", hiddata->ReportID);
		return -errno;
	}

	/* flush the report buffer (data may have changed) */
	memset(reportbuf->ts, 0, sizeof(reportbuf->ts));

	upsdebugx(4, "Set report succeeded");
	return 1;
}

bool_t HIDSetItemValue(hid_dev_handle_t udev, const char *hidpath, double Value, usage_tables_t *utab)
{
	if (HIDSetDataValue(udev, HIDGetItemData(hidpath, utab), Value) != 1)
		return FALSE;

	return TRUE;
}

/* On success, return item count >0. When no notifications are available,
 * return 'error' or 'no event' code.
 */
int HIDGetEvents(hid_dev_handle_t udev, HIDData_t **event, int eventsize)
{
	unsigned char	buf[SMALLBUF];
	int		itemCount = 0;
	int		buflen, r;
	size_t	i;
	HIDData_t	*pData;

	/* needs libusb-0.1.8 to work => use ifdef and autoconf */
	buflen = comm_driver->get_interrupt(udev, buf, interrupt_size ? interrupt_size : sizeof(buf), 250);
	if (buflen <= 0) {
		return buflen;	/* propagate "error" or "no event" code */
	}

	r = file_report_buffer(reportbuf, buf, (size_t)buflen);
	if (r < 0) {
		upsdebug_with_errno(1, "%s: failed to buffer report", __func__);
		return -errno;
	}

	/* now read all items that are part of this report */
	for (i=0; i<pDesc->nitems; i++) {

		pData = &pDesc->item[i];

		/* Variable not part of this report */
		if (pData->ReportID != buf[0])
			continue;

		/* Not an input report */
		if (pData->Type != ITEM_INPUT)
			continue;

		/* maximum number of events reached? */
		if (itemCount >= eventsize) {
			upsdebugx(1, "%s: too many events (truncated)", __func__);
			break;
		}

		event[itemCount++] = pData;
	}

	if (itemCount == 0) {
		upsdebugx(1, "%s: unexpected input report (ignored)", __func__);
	}

	return itemCount;
}


/*******************************************************
 * Support functions
 *******************************************************/

static double logical_to_physical(HIDData_t *Data, long logical)
{
	double physical;
	double Factor;

	upsdebugx(5, "PhyMax = %ld, PhyMin = %ld, LogMax = %ld, LogMin = %ld",
		Data->PhyMax, Data->PhyMin, Data->LogMax, Data->LogMin);

	/* HID spec says that if one or both are undefined, or if they are
	 * both 0, then PhyMin = LogMin, PhyMax = LogMax. */
	if (!Data->have_PhyMax || !Data->have_PhyMin ||
		(Data->PhyMax == 0 && Data->PhyMin == 0))
	{
		return (double)logical;
	}

	/* Paranoia */
	if ((Data->PhyMax <= Data->PhyMin) || (Data->LogMax <= Data->LogMin))
	{
		/* this should not really happen */
		return (double)logical;
	}

	Factor = (double)(Data->PhyMax - Data->PhyMin) / (Data->LogMax - Data->LogMin);
	/* Convert Value */
	physical = (double)((logical - Data->LogMin) * Factor) + Data->PhyMin;

	if (physical > Data->PhyMax) {
		return Data->PhyMax;
	}

	if (physical < Data->PhyMin) {
		return Data->PhyMin;
	}

	return physical;
}

static long physical_to_logical(HIDData_t *Data, double physical)
{
	long logical;
	double Factor;

	upsdebugx(5, "PhyMax = %ld, PhyMin = %ld, LogMax = %ld, LogMin = %ld",
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

	Factor = (double)(Data->LogMax - Data->LogMin) / (Data->PhyMax - Data->PhyMin);
	/* Convert Value */
	logical = (long)((physical - Data->PhyMin) * Factor) + Data->LogMin;

	if (logical > Data->LogMax)
		return Data->LogMax;

	if (logical < Data->LogMin)
		return Data->LogMin;

	return logical;
}

static int8_t get_unit_expo(const HIDData_t *hiddata)
{
	int	i;
	int8_t	unit_expo = hiddata->UnitExp;

	upsdebugx(5, "Unit = %08x, UnitExp = %d", (uint32_t)(hiddata->Unit), hiddata->UnitExp);

	for (i = 0; i < NB_HID_UNITS; i++) {

		if (HIDUnits[i].Type == hiddata->Unit) {
			unit_expo -= HIDUnits[i].Expo;
			break;
		}
	}

	upsdebugx(5, "Exponent = %d", unit_expo);
	return unit_expo;
}

/* exponent function: return a^b */
static double exponent(double a, int8_t b)
{
	if (b>0)
		return (a * exponent(a, --b));		/* a * a ... */

	if (b<0)
		return ((1/a) * exponent(a, ++b));	/* (1/a) * (1/a) ... */

	return 1;
}

/* translate HID string path to numeric path and return path depth */
static int string_to_path(const char *string, HIDPath_t *path, usage_tables_t *utab)
{
	int	i = 0;
	long	usage;
	char	buf[SMALLBUF];
	char	*token, *last;

	snprintf(buf, sizeof(buf), "%s", string);

	for (token = strtok_r(buf, ".", &last); token != NULL; token = strtok_r(NULL, ".", &last))
	{
		/* lookup tables first (to override defaults) */
		if ((usage = hid_lookup_usage(token, utab)) >= 0)
		{
			path->Node[i++] = (HIDNode_t)usage;
			continue;
		}

		/* translate unnamed path components such as "ff860024" */
		if (strlen(token) == strspn(token, "1234567890abcdefABCDEF"))
		{
			long l = strtol(token, NULL, 16);
			/* Note: currently per hidtypes.h, HIDNode_t == uint32_t */
			if (l < 0 || (uintmax_t)l > (uintmax_t)UINT32_MAX) {
				goto badvalue;
			}
			path->Node[i++] = (HIDNode_t)l;
			continue;
		}

		/* indexed collection */
		if (strlen(token) == strspn(token, "[1234567890]"))
		{
			int l = atoi(token + 1); /* +1: skip the bracket */
			if (l < 0 || (uintmax_t)l > (uintmax_t)UINT32_MAX) {
				goto badvalue;
			}
			path->Node[i++] = 0x00ff0000 + (HIDNode_t)l;
			continue;
		}

badvalue:
		/* Uh oh, typo in usage table? */
		upsdebugx(1, "string_to_path: couldn't parse %s from %s", token, string);
	}

	if (i < 0 || i > (int)UINT8_MAX) {
		fatalx(EXIT_FAILURE, "Error: string_to_path(): length exceeded");
	}
	path->Size = (uint8_t)i; /* by construct, i>=0; but anyway checked above to be sure */

	upsdebugx(4, "string_to_path: depth = %d", path->Size);
	return i;
}

/* translate HID numeric path to string path and return path depth */
static int path_to_string(char *string, size_t size, const HIDPath_t *path, usage_tables_t *utab)
{
	int	i;
	const char	*p;

	snprintf(string, size, "%s", "");

	for (i = 0; i < path->Size; i++)
	{
		if (i > 0)
			snprintfcat(string, size, ".");

		/* lookup tables first (to override defaults) */
		if ((p = hid_lookup_path(path->Node[i], utab)) != NULL)
		{
			snprintfcat(string, size, "%s", p);
			continue;
		}

		/* indexed collection */
		if ((path->Node[i] & 0xffff0000) == 0x00ff0000)
		{
			snprintfcat(string, size, "[%i]", path->Node[i] & 0x0000ffff);
			continue;
		}

		/* unnamed path components such as "ff860024" */
		snprintfcat(string, size, "%08x", path->Node[i]);
	}

	return i;
}

/* usage conversion string -> numeric
 * Returns -1 for error, or a (HIDNode_t) ranged code value
 */
static long hid_lookup_usage(const char *name, usage_tables_t *utab)
{
	int i, j;

	for (i = 0; utab[i] != NULL; i++)
	{
		for (j = 0; utab[i][j].usage_name != NULL; j++)
		{
			if (strcasecmp(utab[i][j].usage_name, name))
				continue;

			/* Note: currently per hidtypes.h, HIDNode_t == uint32_t */
			upsdebugx(5, "hid_lookup_usage: %s -> %08x", name, (uint32_t)utab[i][j].usage_code);
			return (long)(utab[i][j].usage_code);
		}
	}

	upsdebugx(5, "hid_lookup_usage: %s -> not found in lookup table", name);
	return -1;
}

/* usage conversion numeric -> string */
static const char *hid_lookup_path(const HIDNode_t usage, usage_tables_t *utab)
{
	int i, j;

	for (i = 0; utab[i] != NULL; i++)
	{
		for (j = 0; utab[i][j].usage_name != NULL; j++)
		{
			if (utab[i][j].usage_code != usage)
				continue;

			upsdebugx(5, "hid_lookup_path: %08x -> %s", (unsigned int)usage, utab[i][j].usage_name);
			return utab[i][j].usage_name;
		}
	}

	upsdebugx(5, "hid_lookup_path: %08x -> not found in lookup table", (unsigned int)usage);
	return NULL;
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
	{ NULL, 0 }
};
