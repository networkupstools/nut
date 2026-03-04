/*! 
 * @file libwinhid.h
 * @brief Generic USB communication backend (Windows HID API, phase 1)
 */

#ifndef NUT_LIBWINHID_H_SEEN
#define NUT_LIBWINHID_H_SEEN 1

#include "nut_libusb.h"

#ifdef WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>

#define WINHID_DRIVER_NAME         "USB communication driver (Windows HID API)"
#define WINHID_DRIVER_VERSION      "0.11"
#define WINHID_MAX_REPORT_SIZE     0x1800
#define WINHID_HIDP_STATUS_SUCCESS 0x00110000UL

#ifndef ERROR_NO_MORE_ITEMS
#define ERROR_NO_MORE_ITEMS 259
#endif

#ifndef DIGCF_PRESENT
#define DIGCF_PRESENT 0x00000002
#endif

#ifndef DIGCF_DEVICEINTERFACE
#define DIGCF_DEVICEINTERFACE 0x00000010
#endif

typedef void *HDEVINFO;

typedef struct win_sp_devinfo_data_s {
	DWORD cbSize;
	GUID ClassGuid;
	DWORD DevInst;
	ULONG_PTR Reserved;
} win_sp_devinfo_data_t;

typedef struct win_sp_device_interface_data_s {
	DWORD cbSize;
	GUID InterfaceClassGuid;
	DWORD Flags;
	ULONG_PTR Reserved;
} win_sp_device_interface_data_t;

typedef PVOID win_phidp_preparsed_data_t;

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

typedef union win_hidp_union_range_u {
	win_hidp_range_t Range;
	win_hidp_notrange_t NotRange;
} win_hidp_union_range_t;

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

extern usb_communication_subdriver_t winhid_subdriver;
#endif

#endif /* NUT_LIBWINHID_H_SEEN */
