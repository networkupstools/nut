/*
 * types.h:		HID Parser types definitions
 *
 * This file is part of the MGE UPS SYSTEMS HID Parser
 *
 * Copyright (C)
 *	1998-2003	MGE UPS SYSTEMS, Luc Descotils
 *	2015		Eaton, Arnaud Quette (Update MAX_REPORT)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * -------------------------------------------------------------------------- */

#ifndef HIDTYPES_H
#define HIDTYPES_H

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif /* __cplusplus */

#include <sys/types.h>

#include "nut_stdint.h"

/*
 * Constants
 * -------------------------------------------------------------------------- */
#define PATH_SIZE         10   /* Deep max for Path                   */
#define USAGE_TAB_SIZE    50   /* Size of usage stack                 */
#define MAX_REPORT        500  /* Including FEATURE, INPUT and OUTPUT */
#define REPORT_DSC_SIZE   6144 /* Size max of Report Descriptor       */
#define MAX_REPORT_TS     3    /* Max time validity of a report       */

/*
 * Items
 * -------------------------------------------------------------------------- */
#define SIZE_0				0x00
#define SIZE_1				0x01
#define SIZE_2				0x02
#define SIZE_4				0x03
#define SIZE_MASK			0x03

#define TYPE_MAIN			0x00
#define TYPE_GLOBAL			0x04
#define TYPE_LOCAL			0x08
#define TYPE_MASK			0x0C

/* Main items */
#define ITEM_COLLECTION			0xA0
#define ITEM_END_COLLECTION		0xC0
#define ITEM_FEATURE			0xB0
#define ITEM_INPUT			0x80
#define ITEM_OUTPUT			0x90

/* Global items */
#define ITEM_UPAGE			0x04
#define ITEM_LOG_MIN			0x14
#define ITEM_LOG_MAX			0x24
#define ITEM_PHY_MIN			0x34
#define ITEM_PHY_MAX			0x44
#define ITEM_UNIT_EXP			0x54
#define ITEM_UNIT			0x64
#define ITEM_REP_SIZE			0x74
#define ITEM_REP_ID			0x84
#define ITEM_REP_COUNT			0x94

/* Local items */
#define ITEM_USAGE			0x08
#define ITEM_STRING			0x78

/* Long item */
#define ITEM_LONG			0xFC

#define ITEM_MASK			0xFC

/* Attribute Flags */
#define ATTR_DATA_CST			0x01
#define ATTR_NVOL_VOL			0x80

/*
 * HIDNode_t struct
 *
 * Describe a HID Path point: Usage = bits 0..15, UPage = bits 16..31
 * -------------------------------------------------------------------------- */
typedef uint32_t HIDNode_t;

/*
 * HIDPath struct
 *
 * Describe a HID Path
 * -------------------------------------------------------------------------- */
typedef struct {
	uint8_t		Size;				/* HID Path size			*/
	HIDNode_t	Node[PATH_SIZE];		/* HID Path				*/
} HIDPath_t;

/*
 * HIDData struct
 *
 * Describe a HID Data with its location in report
 * -------------------------------------------------------------------------- */
typedef struct {
	HIDPath_t	Path;				/* HID Path				*/

	uint8_t		ReportID;			/* Report ID				*/
	uint8_t		Offset;				/* Offset of data in report	*/
	uint8_t		Size;				/* Size of data in bit		*/

	uint8_t		Type;				/* Type : FEATURE / INPUT / OUTPUT */
	uint8_t		Attribute;			/* Report field attribute		*/

	long		Unit;				/* HID Unit				*/
	int8_t		UnitExp;			/* Unit exponent			*/

	long		LogMin;				/* Logical Min			*/
	long		LogMax;				/* Logical Max			*/
	long		PhyMin;				/* Physical Min			*/
	long		PhyMax;				/* Physical Max			*/
	int8_t		have_PhyMin;			/* Physical Min defined?		*/
	int8_t		have_PhyMax;			/* Physical Max defined?		*/
} HIDData_t;

/*
 * HIDDesc struct
 *
 * Holds a parsed report descriptor
 * -------------------------------------------------------------------------- */
typedef struct {
	int		nitems;				/* number of items in descriptor */
	HIDData_t	*item;				/* list of items			*/
	int		replen[256];			/* list of report lengths, in byte */
} HIDDesc_t;

#ifdef __cplusplus
/* *INDENT-OFF* */
} /* extern "C" */
/* *INDENT-ON* */
#endif /* __cplusplus */

#endif /* HIDTYPES_H */
