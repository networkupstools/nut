/*
 * hidparser.h: HID Parser header file
 *
 * This file is part of the MGE UPS SYSTEMS HID Parser.
 *
 * Copyright (C) 1998-2003 MGE UPS SYSTEMS,
 *		Written by Luc Descotils.
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

#ifndef NUT_HID_PARSER_H_SEEN
#define NUT_HID_PARSER_H_SEEN


#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif /* __cplusplus */

#include "config.h"
#include "hidtypes.h"

/* Include "usb-common.h" or "libshut.h" as appropriate, to define the 
 * usb_ctrl_* types used below according to the backend USB API version
 */
#ifdef SHUT_MODE
# include "libshut.h"
#else
# include "usb-common.h"
#endif

/*
 * Parse_ReportDesc
 * -------------------------------------------------------------------------- */
HIDDesc_t *Parse_ReportDesc(const usb_ctrl_charbuf ReportDesc, const usb_ctrl_charbufsize n);

/*
 * Free_ReportDesc
 * -------------------------------------------------------------------------- */
void Free_ReportDesc(HIDDesc_t *pDesc);

/*
 * FindObject
 * -------------------------------------------------------------------------- */
int FindObject(HIDDesc_t *pDesc, HIDData_t *pData);

HIDData_t *FindObject_with_Path(HIDDesc_t *pDesc, HIDPath_t *Path, uint8_t Type);

HIDData_t *FindObject_with_ID(HIDDesc_t *pDesc, uint8_t ReportID, uint8_t Offset, uint8_t Type);

/*
 * GetValue
 * -------------------------------------------------------------------------- */
void GetValue(const unsigned char *Buf, HIDData_t *pData, long *pValue);

/*
 * SetValue
 * -------------------------------------------------------------------------- */
void SetValue(const HIDData_t *pData, unsigned char *Buf, long Value);


#ifdef __cplusplus
/* *INDENT-OFF* */
} /* extern "C" */
/* *INDENT-ON* */
#endif /* __cplusplus */

#endif	/* NUT_HID_PARSER_H_SEEN */
