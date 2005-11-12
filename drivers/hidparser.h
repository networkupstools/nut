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

#ifndef HIDPARS_H
#define HIDPARS_H


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "hidtypes.h"

/*
 * Parse_ReportDesc
 * -------------------------------------------------------------------------- */
int Parse_ReportDesc(u_char *ReportDesc, int n, HIDDesc *pDesc);

/*
 * FindObject
 * -------------------------------------------------------------------------- */
int FindObject(HIDDesc *pDesc, HIDData* pData);

/*
 * GetValue
 * -------------------------------------------------------------------------- */
void GetValue(const u_char* Buf, HIDData* pData);

/*
 * SetValue
 * -------------------------------------------------------------------------- */
void SetValue(const HIDData* pData, u_char* Buf);


#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif
