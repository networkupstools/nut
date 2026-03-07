/*!
 * @file libwinhid.h
 * @brief NUT USB communication backend using native Windows HID API
 *
 * This backend implements the usb_communication_subdriver_t interface using
 * only the native Windows HID API (SetupAPI, hid.dll, kernel32).  It provides
 * an alternative to the libusb-based backends for Windows builds, avoiding the
 * need for third-party USB filter drivers (e.g. Zadig/WinUSB).
 *
 * The backend dynamically loads all required Windows APIs at runtime and
 * reconstructs a synthetic HID report descriptor from the parsed HidP caps
 * so that the existing NUT hidparser/libhid pipeline can operate unchanged.
 *
 * @author Copyright (C)
 * 2026 Owen Li <geek@geeking.moe>
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

#ifndef NUT_LIBWINHID_H_SEEN
#define NUT_LIBWINHID_H_SEEN 1

#include "nut_libusb.h"
#include "hidtypes.h"

#ifdef WIN32

#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>

/* ---------------------------------------------------------------------- */
/* Backend identity and limits                                            */
/* ---------------------------------------------------------------------- */

#define WINHID_DRIVER_NAME         "USB communication driver (Windows HID API)"
#define WINHID_DRIVER_VERSION      "0.11"
#define WINHID_MAX_REPORT_SIZE     0x1800
#define WINHID_HIDP_STATUS_SUCCESS 0x00110000UL

/* ---------------------------------------------------------------------- */
/* Platform fallback defines (may be missing in some MinGW variants)      */
/* ---------------------------------------------------------------------- */

#ifndef ERROR_NO_MORE_ITEMS
# define ERROR_NO_MORE_ITEMS 259
#endif

#ifndef DIGCF_PRESENT
# define DIGCF_PRESENT 0x00000002
#endif

#ifndef DIGCF_DEVICEINTERFACE
# define DIGCF_DEVICEINTERFACE 0x00000010
#endif

/* ---------------------------------------------------------------------- */
/* SetupAPI / HID struct mirrors                                          */
/*                                                                        */
/* These definitions mirror the native Windows SDK types so that we can   */
/* build without requiring the Windows Driver Kit headers.  The layout    */
/* MUST remain binary-compatible with the native definitions.             */
/* ---------------------------------------------------------------------- */

typedef void *HDEVINFO;

/*! @brief SP_DEVINFO_DATA mirror for SetupAPI device enumeration */
typedef struct win_sp_devinfo_data_s {
	DWORD cbSize;
	GUID ClassGuid;
	DWORD DevInst;
	ULONG_PTR Reserved;
} win_sp_devinfo_data_t;

/*! @brief SP_DEVICE_INTERFACE_DATA mirror for SetupAPI interface enum */
typedef struct win_sp_device_interface_data_s {
	DWORD cbSize;
	GUID InterfaceClassGuid;
	DWORD Flags;
	ULONG_PTR Reserved;
} win_sp_device_interface_data_t;

/*! @brief Opaque handle to preparsed HID data from HidD_GetPreparsedData */
typedef PVOID win_phidp_preparsed_data_t;

/* ---------------------------------------------------------------------- */
/* HIDP_CAPS mirror                                                       */
/* ---------------------------------------------------------------------- */

/*! @brief HIDP_CAPS mirror — top-level device capabilities summary */
typedef struct win_hidp_caps_s {
	USHORT Usage;
	USHORT UsagePage;
	USHORT InputReportByteLength;
	USHORT OutputReportByteLength;
	USHORT FeatureReportByteLength;
	USHORT Reserved[17];
	USHORT NumberLinkCollectionNodes;
	USHORT NumberInputButtonCaps;
	USHORT NumberInputValueCaps;
	USHORT NumberInputDataIndices;
	USHORT NumberOutputButtonCaps;
	USHORT NumberOutputValueCaps;
	USHORT NumberOutputDataIndices;
	USHORT NumberFeatureButtonCaps;
	USHORT NumberFeatureValueCaps;
	USHORT NumberFeatureDataIndices;
} win_hidp_caps_t;

/* ---------------------------------------------------------------------- */
/* HIDP value/button caps mirrors and their range unions                  */
/* ---------------------------------------------------------------------- */

/*! @brief Range sub-struct for HIDP value/button caps (IsRange == TRUE) */
typedef struct win_hidp_range_s {
	USHORT UsageMin;
	USHORT UsageMax;
	USHORT StringMin;
	USHORT StringMax;
	USHORT DesignatorMin;
	USHORT DesignatorMax;
	USHORT DataIndexMin;
	USHORT DataIndexMax;
} win_hidp_range_t;

/*! @brief NotRange sub-struct for HIDP value/button caps (IsRange == FALSE) */
typedef struct win_hidp_notrange_s {
	USHORT Usage;
	USHORT Reserved1;
	USHORT StringIndex;
	USHORT Reserved2;
	USHORT DesignatorIndex;
	USHORT Reserved3;
	USHORT DataIndex;
	USHORT Reserved4;
} win_hidp_notrange_t;

/*! @brief Union that covers both Range and NotRange variants */
typedef union win_hidp_union_range_u {
	win_hidp_range_t Range;
	win_hidp_notrange_t NotRange;
} win_hidp_union_range_t;

/*! @brief HIDP_VALUE_CAPS mirror — describes one value-type HID data item */
typedef struct win_hidp_value_caps_s {
	USHORT UsagePage;
	UCHAR ReportID;
	BOOLEAN IsAlias;
	USHORT BitField;
	USHORT LinkCollection;
	USHORT LinkUsage;
	USHORT LinkUsagePage;
	BOOLEAN IsRange;
	BOOLEAN IsStringRange;
	BOOLEAN IsDesignatorRange;
	BOOLEAN IsAbsolute;
	BOOLEAN HasNull;
	UCHAR Reserved;
	USHORT BitSize;
	USHORT ReportCount;
	USHORT Reserved2[5];
	ULONG UnitsExp;
	ULONG Units;
	LONG LogicalMin;
	LONG LogicalMax;
	LONG PhysicalMin;
	LONG PhysicalMax;
	win_hidp_union_range_t u;
} win_hidp_value_caps_t;

/*! @brief HIDP_BUTTON_CAPS mirror — describes one button-type HID data item */
typedef struct win_hidp_button_caps_s {
	USHORT UsagePage;
	UCHAR ReportID;
	BOOLEAN IsAlias;
	USHORT BitField;
	USHORT LinkCollection;
	USHORT LinkUsage;
	USHORT LinkUsagePage;
	BOOLEAN IsRange;
	BOOLEAN IsStringRange;
	BOOLEAN IsDesignatorRange;
	BOOLEAN IsAbsolute;
	DWORD Reserved[10];
	win_hidp_union_range_t u;
} win_hidp_button_caps_t;

/*! @brief HIDP_LINK_COLLECTION_NODE mirror — one node in the collection tree */
typedef struct win_hidp_link_collection_node_s {
	USHORT LinkUsage;
	USHORT LinkUsagePage;
	USHORT Parent;
	USHORT NumberOfChildren;
	USHORT NextSibling;
	USHORT FirstChild;
	ULONG Flags;
	PVOID UserContext;
} win_hidp_link_collection_node_t;

/* ---------------------------------------------------------------------- */
/* Public API                                                             */
/* ---------------------------------------------------------------------- */

/*! @brief The winhid communication subdriver instance (registered at build time) */
extern usb_communication_subdriver_t winhid_subdriver;

/*!
 * @brief Canonicalize a parsed HID report descriptor for winhid compatibility.
 *
 * After Parse_ReportDesc() produces a HIDDesc_t from the synthetic descriptor
 * built by the winhid backend, several fixups are needed so that existing NUT
 * subdriver mapping tables can resolve paths correctly:
 *
 *   - Insert PowerSummary collection nodes where expected by subdriver tables
 *   - Re-map 0x84xx (Power Device page) status leaf usages to their canonical
 *     0x85xx (Battery System page) equivalents
 *   - Flatten intermediate collection hierarchy (e.g. PowerConverter.Input →
 *     Input) and rewrite collection ID nodes to base form
 *   - Generate alias HIDData items for paths that differ only by the above
 *     transformations, so both old and new path forms resolve
 *
 * @param desc  The parsed descriptor to canonicalize (modified in place).
 * @return      Number of changes made, or 0 if nothing was modified.
 *
 * @note It is only meaningful on WIN32 builds using the winhid backend.
 */
int winhid_canonicalize_parsed_report_desc(HIDDesc_t *desc);

#endif /* WIN32 */

#endif /* NUT_LIBWINHID_H_SEEN */
