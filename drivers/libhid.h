/*!
 * @file libhid.h
 * @brief HID Library - User API
 *
 * @author Copyright (C) 2003
 *      Arnaud Quette <arnaud.quette@free.fr> && <arnaud.quette@mgeups.com>
 *      Charles Lepple <clepple@ghz.cc>
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

#include <sys/types.h>
#include "timehead.h"
#include "hidtypes.h"

/* use explicit booleans */
#ifndef FALSE
typedef enum ebool { FALSE, TRUE } bool;
#else
typedef int bool;
#endif

/* Device open modes */
#define MODE_OPEN 0
#define MODE_REOPEN 1

#define MAX_TS			2		/* validity period of a gotten report (2 sec) */

/* ---------------------------------------------------------------------- */

/*!
 * Describe a HID device
 */
typedef struct
{
	char*     Name;		/*!< HID Device name */
	char*     Vendor; /*!< Device's Vendor Name */
	u_int16_t VendorID; /*!< Device's Vendor ID */
	char*     Product; /*!< Device's Product Name */
	u_int16_t ProductID; /*!< Device's Product ID */
	int       Application; /*!< match Usage for HIDOpenDevice(Usage)) */
	char*     Serial; /* Product serial number */
	int       fd; /* "internal" file descriptor */
} HIDDevice;

/*!
 * Describe a HID Item (a node in the HID tree)
 */
 typedef struct
{
	char*   Path;			/*!< HID Object's fully qualified HID path	*/
	long    Value;		/*!< HID Object Value					*/
	u_char   Attribute;	/*!< Report field attribute				*/
	u_long   Unit; 		/*!< HID Unit							*/
	char    UnitExp;		/*!< Unit exponent						*/
	long    LogMin;		/*!< Logical Min							*/
	long    LogMax;		/*!< Logical Max						*/
	long    PhyMin;		/*!< Physical Min						*/
	long    PhyMax;		/*!< Physical Max						*/	
} HIDItem;

/*!
 * Describe a set of values to match for finding a special HID device
 */
typedef struct
{
	int VendorID;
	int ProductID;
	int UsageCode;
	int Index;
} MatchFlags;

#define ANY	-1 /* 0xffff */ /* match any vendor or product */

 /* ---------------------------------------------------------------------- */

/*
 * HIDOpenDevice
 * -------------------------------------------------------------------------- */
HIDDevice *HIDOpenDevice(const char *port, MatchFlags *flg, int mode);

/*
 * HIDGetItem
 * -------------------------------------------------------------------------- */
HIDItem *HIDGetItem(const char *ItemPath);

/*
 * HIDGetItemValue
 * -------------------------------------------------------------------------- */
float HIDGetItemValue(char *path, float *Value);

/*
 * HIDGetItemString
 * -------------------------------------------------------------------------- */
char *HIDGetItemString(char *path);

/*
 * HIDSetItemValue
 * -------------------------------------------------------------------------- */
bool HIDSetItemValue(char *path, float value);

/*
 * HIDGetNextEvent
 * -------------------------------------------------------------------------- */
int HIDGetEvents(HIDDevice *dev, HIDItem **eventsList);

/*
 * HIDCloseDevice
 * -------------------------------------------------------------------------- */
void HIDCloseDevice(HIDDevice *dev);

/*
 * Support functions
 * -------------------------------------------------------------------------- */
int get_current_data_attribute();
void HIDDumpTree(HIDDevice *hd);

#endif /* _LIBHID_H */
