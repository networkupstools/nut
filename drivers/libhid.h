/*!
 * @file libhid.h
 * @brief HID Library - User API
 *
 * @author Copyright (C) 2003 - 2007
 *      Arnaud Quette <arnaud.quette@free.fr> && <arnaud.quette@mgeups.com>
 *      Charles Lepple <clepple@ghz.cc>
 *      Peter Selinger <selinger@users.sourceforge.net>
 *      Arjen de Korte <adkorte-guest@alioth.debian.org>
 *
 * This program is sponsored by MGE UPS SYSTEMS - opensource.mgeups.com
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

#ifndef _LIBHID_H
#define _LIBHID_H

#include "config.h"

#include <sys/types.h>
#include "hidtypes.h"

#include "timehead.h"
#ifdef SHUT_MODE
	#include "libshut.h"
	typedef SHUTDevice_t                   HIDDevice_t;
	typedef char                           HIDDeviceMatcher_t;
	typedef int                            hid_dev_handle_t;
	typedef shut_communication_subdriver_t communication_subdriver_t;
#else
	#include "nut_libusb.h"
	typedef USBDevice_t                   HIDDevice_t;
	typedef USBDeviceMatcher_t            HIDDeviceMatcher_t;
	typedef libusb_device_handle *        hid_dev_handle_t;
	typedef usb_communication_subdriver_t communication_subdriver_t;
#endif

/* use explicit booleans */
#ifndef FALSE
typedef enum ebool { FALSE, TRUE } bool_t;
#else
typedef int bool_t;
#endif

/* Device open modes */
#define MODE_OPEN	0	/* open a HID device for the first time */
#define MODE_REOPEN	1	/* reopen a HID device that was opened before */

#define MAX_TS		2	/* validity period of a gotten report (2 sec) */

/* ---------------------------------------------------------------------- */

/* structure to describe an item in a usage table */
typedef struct {
	const char	*usage_name;
	const HIDNode_t	usage_code;
} usage_lkp_t;

extern usage_lkp_t hid_usage_lkp[];

/* an object of type usage_tables_t is a NULL-terminated array of
 * pointers to individual usage tables. */
typedef usage_lkp_t *usage_tables_t;

extern communication_subdriver_t *comm_driver;
extern HIDDesc_t	*pDesc;	/* parsed Report Descriptor */

/* report buffer structure: holds data about most recent report for
   each given report id */
typedef struct reportbuf_s {
       time_t	ts[256];			/* timestamp when report was retrieved */
       int	len[256];			/* size of report data */
       unsigned char	*data[256];		/* report data (allocated) */
} reportbuf_t;

extern reportbuf_t	*reportbuf;	/* buffer for most recent reports */

extern int interrupt_only;
extern unsigned int interrupt_size;

/* ---------------------------------------------------------------------- */

/*
 * HIDGetItemValue
 * -------------------------------------------------------------------------- */
int HIDGetItemValue(hid_dev_handle_t udev, const char *hidpath, double *Value, usage_tables_t *utab);

/*
 * HIDGetItemString
 * -------------------------------------------------------------------------- */
char *HIDGetItemString(hid_dev_handle_t udev, const char *hidpath, char *buf, size_t buflen, usage_tables_t *utab);

/*
 * HIDSetItemValue
 * -------------------------------------------------------------------------- */
bool_t HIDSetItemValue(hid_dev_handle_t udev, const char *hidpath, double value, usage_tables_t *utab);

/*
 * GetItemData
 * -------------------------------------------------------------------------- */
HIDData_t *HIDGetItemData(const char *hidpath, usage_tables_t *utab);

/*
 * GetDataItem
 * -------------------------------------------------------------------------- */
char *HIDGetDataItem(const HIDData_t *hiddata, usage_tables_t *utab);

/*
 * HIDGetDataValue
 * -------------------------------------------------------------------------- */
int HIDGetDataValue(hid_dev_handle_t udev, HIDData_t *hiddata, double *Value, int age);

/*
 * HIDSetDataValue
 * -------------------------------------------------------------------------- */
int HIDSetDataValue(hid_dev_handle_t udev, HIDData_t *hiddata, double Value);

/*
 * HIDGetIndexString
 * -------------------------------------------------------------------------- */
char *HIDGetIndexString(hid_dev_handle_t udev, int Index, char *buf, size_t buflen);

/*
 * HIDGetEvents
 * -------------------------------------------------------------------------- */
int HIDGetEvents(hid_dev_handle_t udev, HIDData_t **event, int eventlen);

/*
 * Support functions
 * -------------------------------------------------------------------------- */
void HIDDumpTree(hid_dev_handle_t udev, HIDDevice_t *hd, usage_tables_t *utab);
const char *HIDDataType(const HIDData_t *hiddata);

void free_report_buffer(reportbuf_t *rbuf);
reportbuf_t *new_report_buffer(HIDDesc_t *pDesc);

#endif /* _LIBHID_H */
