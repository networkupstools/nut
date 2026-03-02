/*! 
 * @file libwinhid.c
 * @brief Generic USB communication backend (Windows HID API, phase 1)
 *
 * Notes for phase-1 implementation:
 * - Uses only SetupAPI/hid.dll/kernel32 APIs mirrored in gethidwindows.py.
 * - Avoids IOCTL/DeviceIoControl usage.
 * - Keeps matcher flow used by libusb backends (VID/PID and regex chain).
 * - Builds a synthetic report descriptor from HidP caps as a best-effort bridge
 *   to existing NUT HID parser path.
 */

#include "config.h"

#include "common.h"
#include "usb-common.h"
#include "nut_libusb.h"
#include "nut_stdint.h"
#include "libwinhid.h"

#ifdef WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WINHID_DRIVER_NAME           "USB communication driver (Windows HID API)"
#define WINHID_DRIVER_VERSION        "0.10"
#define WINHID_MAX_REPORT_SIZE       0x1800
#define WINHID_HIDP_STATUS_SUCCESS   0x00110000UL

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

typedef HDEVINFO (WINAPI *pSetupDiGetClassDevsW)(
	const GUID *ClassGuid,
	PCWSTR Enumerator,
	HWND hwndParent,
	DWORD Flags);

typedef BOOL (WINAPI *pSetupDiEnumDeviceInterfaces)(
	HDEVINFO DeviceInfoSet,
	win_sp_devinfo_data_t *DeviceInfoData,
	const GUID *InterfaceClassGuid,
	DWORD MemberIndex,
	win_sp_device_interface_data_t *DeviceInterfaceData);

typedef BOOL (WINAPI *pSetupDiGetDeviceInterfaceDetailW)(
	HDEVINFO DeviceInfoSet,
	win_sp_device_interface_data_t *DeviceInterfaceData,
	PVOID DeviceInterfaceDetailData,
	DWORD DeviceInterfaceDetailDataSize,
	DWORD *RequiredSize,
	win_sp_devinfo_data_t *DeviceInfoData);

typedef BOOL (WINAPI *pSetupDiDestroyDeviceInfoList)(
	HDEVINFO DeviceInfoSet);

typedef BOOLEAN (WINAPI *pHidD_GetPreparsedData)(
	HANDLE HidDeviceObject,
	win_phidp_preparsed_data_t *PreparsedData);

typedef BOOLEAN (WINAPI *pHidD_FreePreparsedData)(
	win_phidp_preparsed_data_t PreparsedData);

typedef BOOLEAN (WINAPI *pHidD_GetInputReport)(
	HANDLE HidDeviceObject,
	PVOID ReportBuffer,
	ULONG ReportBufferLength);

typedef BOOLEAN (WINAPI *pHidD_GetFeature)(
	HANDLE HidDeviceObject,
	PVOID ReportBuffer,
	ULONG ReportBufferLength);

typedef ULONG (WINAPI *pHidP_GetCaps)(
	win_phidp_preparsed_data_t PreparsedData,
	win_hidp_caps_t *Capabilities);

typedef ULONG (WINAPI *pHidP_GetValueCaps)(
	ULONG ReportType,
	win_hidp_value_caps_t *ValueCaps,
	USHORT *ValueCapsLength,
	win_phidp_preparsed_data_t PreparsedData);

typedef ULONG (WINAPI *pHidP_GetButtonCaps)(
	ULONG ReportType,
	win_hidp_button_caps_t *ButtonCaps,
	USHORT *ButtonCapsLength,
	win_phidp_preparsed_data_t PreparsedData);

typedef HANDLE (WINAPI *pCreateFileW_t)(
	LPCWSTR lpFileName,
	DWORD dwDesiredAccess,
	DWORD dwShareMode,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	DWORD dwCreationDisposition,
	DWORD dwFlagsAndAttributes,
	HANDLE hTemplateFile);

typedef BOOL (WINAPI *pReadFile_t)(
	HANDLE hFile,
	LPVOID lpBuffer,
	DWORD nNumberOfBytesToRead,
	LPDWORD lpNumberOfBytesRead,
	LPOVERLAPPED lpOverlapped);

typedef BOOL (WINAPI *pCloseHandle_t)(HANDLE hObject);

typedef HANDLE (WINAPI *pCreateEventW_t)(
	LPSECURITY_ATTRIBUTES lpEventAttributes,
	BOOL bManualReset,
	BOOL bInitialState,
	LPCWSTR lpName);

typedef BOOL (WINAPI *pResetEvent_t)(HANDLE hEvent);

typedef DWORD (WINAPI *pWaitForSingleObject_t)(HANDLE hHandle, DWORD dwMilliseconds);

typedef BOOL (WINAPI *pGetOverlappedResult_t)(
	HANDLE hFile,
	LPOVERLAPPED lpOverlapped,
	LPDWORD lpNumberOfBytesTransferred,
	BOOL bWait);

typedef struct winhid_api_s {
	HMODULE setupapi_mod;
	HMODULE hid_mod;
	HMODULE kernel32_mod;

	pSetupDiGetClassDevsW SetupDiGetClassDevsW;
	pSetupDiEnumDeviceInterfaces SetupDiEnumDeviceInterfaces;
	pSetupDiGetDeviceInterfaceDetailW SetupDiGetDeviceInterfaceDetailW;
	pSetupDiDestroyDeviceInfoList SetupDiDestroyDeviceInfoList;

	pHidD_GetPreparsedData HidD_GetPreparsedData;
	pHidD_FreePreparsedData HidD_FreePreparsedData;
	pHidD_GetInputReport HidD_GetInputReport;
	pHidD_GetFeature HidD_GetFeature;
	pHidP_GetCaps HidP_GetCaps;
	pHidP_GetValueCaps HidP_GetValueCaps;
	pHidP_GetButtonCaps HidP_GetButtonCaps;

	pCreateFileW_t CreateFileW;
	pReadFile_t ReadFile;
	pCloseHandle_t CloseHandle;
	pCreateEventW_t CreateEventW;
	pResetEvent_t ResetEvent;
	pWaitForSingleObject_t WaitForSingleObject;
	pGetOverlappedResult_t GetOverlappedResult;

	int loaded;
} winhid_api_t;

static winhid_api_t g_winhid_api;

enum {
	WINHID_REPORT_INPUT = 0,
	WINHID_REPORT_OUTPUT = 1,
	WINHID_REPORT_FEATURE = 2
};

typedef struct winhid_dev_ctx_s {
	HANDLE handle;
	HANDLE event;
	char *device_path;
	int use_overlapped_io;
	OVERLAPPED read_ov;
	unsigned char *read_buf;
	size_t read_buf_size;
	int read_pending;
	size_t input_report_len;
	size_t feature_report_len;
	unsigned char input_report_ids[256];
	unsigned char has_input_report_id[256];
	unsigned int input_report_id_count;
	unsigned int input_report_id_cursor;
	int input_report_id_zero_seen;
} winhid_dev_ctx_t;

typedef struct winhid_descbuf_s {
	unsigned char *data;
	size_t len;
	size_t cap;
} winhid_descbuf_t;

#define WINHID_TYPE_MAIN   0x0
#define WINHID_TYPE_GLOBAL 0x1
#define WINHID_TYPE_LOCAL  0x2

#define WINHID_MAIN_INPUT         0x8
#define WINHID_MAIN_OUTPUT        0x9
#define WINHID_MAIN_COLLECTION    0xA
#define WINHID_MAIN_FEATURE       0xB
#define WINHID_MAIN_ENDCOLLECTION 0xC

#define WINHID_GLOBAL_USAGE_PAGE  0x0
#define WINHID_GLOBAL_LOGICAL_MIN 0x1
#define WINHID_GLOBAL_LOGICAL_MAX 0x2
#define WINHID_GLOBAL_PHYSICAL_MIN 0x3
#define WINHID_GLOBAL_PHYSICAL_MAX 0x4
#define WINHID_GLOBAL_UNIT_EXP    0x5
#define WINHID_GLOBAL_UNIT        0x6
#define WINHID_GLOBAL_REPORT_SIZE 0x7
#define WINHID_GLOBAL_REPORT_ID   0x8
#define WINHID_GLOBAL_REPORT_COUNT 0x9

#define WINHID_LOCAL_USAGE        0x0
#define WINHID_LOCAL_USAGE_MIN    0x1
#define WINHID_LOCAL_USAGE_MAX    0x2

#define WINHID_COLLECTION_PHYSICAL   0x00
#define WINHID_COLLECTION_APPLICATION 0x01

#define WINHID_USAGE_PAGE_POWER_DEVICE   0x0084U
#define WINHID_USAGE_PAGE_BATTERY_SYSTEM 0x0085U
#define WINHID_USAGE_POWER_SUMMARY       0x0024U
#define WINHID_USAGE_PRESENT_STATUS      0x0002U

static const GUID g_hid_interface_guid = {
	0x4D1E55B2,
	0xF16F,
	0x11CF,
	{ 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 }
};

#define WINHID_RESOLVE(dst, mod, sym) do { \
	FARPROC _fp = GetProcAddress((mod), (sym)); \
	if (!_fp) { \
		upsdebugx(1, "%s: failed to resolve %s", __func__, (sym)); \
		goto fail; \
	} \
	memcpy(&(dst), &_fp, sizeof(_fp)); \
} while (0)

static int winhid_load_apis(void)
{
	if (g_winhid_api.loaded) {
		return 1;
	}

	memset(&g_winhid_api, 0, sizeof(g_winhid_api));

	g_winhid_api.kernel32_mod = LoadLibraryA("kernel32.dll");
	g_winhid_api.setupapi_mod = LoadLibraryA("setupapi.dll");
	g_winhid_api.hid_mod = LoadLibraryA("hid.dll");

	if (!g_winhid_api.kernel32_mod || !g_winhid_api.setupapi_mod || !g_winhid_api.hid_mod) {
		upsdebugx(1, "%s: could not load required Windows DLLs", __func__);
		goto fail;
	}

	WINHID_RESOLVE(g_winhid_api.SetupDiGetClassDevsW, g_winhid_api.setupapi_mod, "SetupDiGetClassDevsW");
	WINHID_RESOLVE(g_winhid_api.SetupDiEnumDeviceInterfaces, g_winhid_api.setupapi_mod, "SetupDiEnumDeviceInterfaces");
	WINHID_RESOLVE(g_winhid_api.SetupDiGetDeviceInterfaceDetailW, g_winhid_api.setupapi_mod, "SetupDiGetDeviceInterfaceDetailW");
	WINHID_RESOLVE(g_winhid_api.SetupDiDestroyDeviceInfoList, g_winhid_api.setupapi_mod, "SetupDiDestroyDeviceInfoList");

	WINHID_RESOLVE(g_winhid_api.HidD_GetPreparsedData, g_winhid_api.hid_mod, "HidD_GetPreparsedData");
	WINHID_RESOLVE(g_winhid_api.HidD_FreePreparsedData, g_winhid_api.hid_mod, "HidD_FreePreparsedData");
	WINHID_RESOLVE(g_winhid_api.HidD_GetInputReport, g_winhid_api.hid_mod, "HidD_GetInputReport");
	WINHID_RESOLVE(g_winhid_api.HidD_GetFeature, g_winhid_api.hid_mod, "HidD_GetFeature");
	WINHID_RESOLVE(g_winhid_api.HidP_GetCaps, g_winhid_api.hid_mod, "HidP_GetCaps");
	WINHID_RESOLVE(g_winhid_api.HidP_GetValueCaps, g_winhid_api.hid_mod, "HidP_GetValueCaps");
	WINHID_RESOLVE(g_winhid_api.HidP_GetButtonCaps, g_winhid_api.hid_mod, "HidP_GetButtonCaps");

	WINHID_RESOLVE(g_winhid_api.CreateFileW, g_winhid_api.kernel32_mod, "CreateFileW");
	WINHID_RESOLVE(g_winhid_api.ReadFile, g_winhid_api.kernel32_mod, "ReadFile");
	WINHID_RESOLVE(g_winhid_api.CloseHandle, g_winhid_api.kernel32_mod, "CloseHandle");
	WINHID_RESOLVE(g_winhid_api.CreateEventW, g_winhid_api.kernel32_mod, "CreateEventW");
	WINHID_RESOLVE(g_winhid_api.ResetEvent, g_winhid_api.kernel32_mod, "ResetEvent");
	WINHID_RESOLVE(g_winhid_api.WaitForSingleObject, g_winhid_api.kernel32_mod, "WaitForSingleObject");
	WINHID_RESOLVE(g_winhid_api.GetOverlappedResult, g_winhid_api.kernel32_mod, "GetOverlappedResult");

	g_winhid_api.loaded = 1;
	return 1;

fail:
	if (g_winhid_api.setupapi_mod) {
		FreeLibrary(g_winhid_api.setupapi_mod);
	}
	if (g_winhid_api.hid_mod) {
		FreeLibrary(g_winhid_api.hid_mod);
	}
	if (g_winhid_api.kernel32_mod) {
		FreeLibrary(g_winhid_api.kernel32_mod);
	}
	memset(&g_winhid_api, 0, sizeof(g_winhid_api));
	return 0;
}

static int winhid_map_winerr_to_libusb(const DWORD err)
{
	switch (err) {
	case ERROR_SUCCESS:
		return 0;
	case ERROR_ACCESS_DENIED:
		return LIBUSB_ERROR_ACCESS;
	case ERROR_SHARING_VIOLATION:
		return LIBUSB_ERROR_BUSY;
	case ERROR_NOT_ENOUGH_MEMORY:
	case ERROR_OUTOFMEMORY:
		return LIBUSB_ERROR_NO_MEM;
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
		return LIBUSB_ERROR_NOT_FOUND;
	case ERROR_DEVICE_NOT_CONNECTED:
	case ERROR_INVALID_HANDLE:
		return LIBUSB_ERROR_NO_DEVICE;
	case ERROR_SEM_TIMEOUT:
		return LIBUSB_ERROR_TIMEOUT;
	case ERROR_INVALID_FUNCTION:
		return LIBUSB_ERROR_NOT_SUPPORTED;
	default:
		return LIBUSB_ERROR_IO;
	}
}

static inline int matches(USBDeviceMatcher_t *matcher, USBDevice_t *device)
{
	if (!matcher) {
		return 1;
	}
	return matcher->match_function(device, matcher->privdata);
}

static void winhid_reset_curdevice(USBDevice_t *curDevice)
{
	if (!curDevice) {
		return;
	}

	free(curDevice->Vendor);
	free(curDevice->Product);
	free(curDevice->Serial);
	free(curDevice->Bus);
	free(curDevice->Device);
#if (defined WITH_USB_BUSPORT) && (WITH_USB_BUSPORT)
	free(curDevice->BusPort);
#endif
	memset(curDevice, 0, sizeof(*curDevice));
}

static char *winhid_strdup_range(const char *s, size_t len)
{
	char *ret;

	ret = (char *)malloc(len + 1);
	if (!ret) {
		return NULL;
	}
	if (len) {
		memcpy(ret, s, len);
	}
	ret[len] = '\0';
	return ret;
}

static char *winhid_wstr_to_ascii(const WCHAR *wstr)
{
	size_t len, i;
	char *ret;

	if (!wstr) {
		return NULL;
	}

	len = wcslen(wstr);
	ret = (char *)malloc(len + 1);
	if (!ret) {
		return NULL;
	}

	for (i = 0; i < len; i++) {
		WCHAR wc = wstr[i];
		ret[i] = (wc <= 0x7fU) ? (char)wc : '?';
	}
	ret[len] = '\0';
	return ret;
}

static int winhid_is_sane_device_path(const WCHAR *path)
{
	if (!path) {
		return 0;
	}

	if (path[0] != L'\\' || path[1] != L'\\') {
		return 0;
	}

	if ((path[2] != L'?' && path[2] != L'.') || path[3] != L'\\') {
		return 0;
	}

	return 1;
}

static int winhid_parse_hex4(const char *p, uint16_t *out)
{
	unsigned int i;
	unsigned int v = 0;

	if (!p || !out) {
		return 0;
	}

	for (i = 0; i < 4; i++) {
		int c = p[i];
		int n;

		if (!isxdigit((unsigned char)c)) {
			return 0;
		}

		if (c >= '0' && c <= '9') {
			n = c - '0';
		} else {
			c = tolower((unsigned char)c);
			n = 10 + c - 'a';
		}

		v = (v << 4) | (unsigned int)n;
	}

	*out = (uint16_t)v;
	return 1;
}

static char *winhid_extract_instance_segment(const char *path)
{
	const char *h1, *h2, *h3;

	if (!path) {
		return NULL;
	}

	h1 = strchr(path, '#');
	if (!h1) {
		return NULL;
	}
	h2 = strchr(h1 + 1, '#');
	if (!h2) {
		return NULL;
	}
	h3 = strchr(h2 + 1, '#');
	if (!h3 || h3 <= h2 + 1) {
		return NULL;
	}

	return winhid_strdup_range(h2 + 1, (size_t)(h3 - (h2 + 1)));
}

static void winhid_fill_device_identity(USBDevice_t *curDevice, const char *path, const size_t enum_index)
{
	size_t i;
	char *lower;
	const char *vidp, *pidp;
	char tmp[8];

	if (!curDevice || !path) {
		return;
	}

	lower = strdup(path);
	if (!lower) {
		fatal_with_errno(EXIT_FAILURE, "%s: strdup()", __func__);
	}

	for (i = 0; lower[i] != '\0'; i++) {
		lower[i] = (char)tolower((unsigned char)lower[i]);
	}

	vidp = strstr(lower, "vid_");
	pidp = strstr(lower, "pid_");

	if (vidp) {
		uint16_t vid = 0;
		if (winhid_parse_hex4(vidp + 4, &vid)) {
			curDevice->VendorID = vid;
		}
	}
	if (pidp) {
		uint16_t pid = 0;
		if (winhid_parse_hex4(pidp + 4, &pid)) {
			curDevice->ProductID = pid;
		}
	}

	curDevice->Serial = winhid_extract_instance_segment(path);
	curDevice->bcdDevice = 0;

	snprintf(tmp, sizeof(tmp), "%03u", 0U);
	curDevice->Bus = strdup(tmp);
	if (!curDevice->Bus) {
		free(lower);
		fatal_with_errno(EXIT_FAILURE, "%s: strdup(bus)", __func__);
	}

	if (enum_index < 999U) {
		snprintf(tmp, sizeof(tmp), "%03u", (unsigned int)(enum_index + 1));
		curDevice->Device = strdup(tmp);
	}

	if (!curDevice->Device) {
		curDevice->Device = strdup("000");
		if (!curDevice->Device) {
			free(lower);
			fatal_with_errno(EXIT_FAILURE, "%s: strdup(device)", __func__);
		}
	}

	free(lower);
}

static HANDLE winhid_open_hid_path(const WCHAR *pathw, int *opened_overlapped)
{
	HANDLE handle;
	DWORD share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE;

	if (opened_overlapped) {
		*opened_overlapped = 0;
	}

	handle = g_winhid_api.CreateFileW(
		pathw,
		GENERIC_READ | GENERIC_WRITE,
		share_mode,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_OVERLAPPED,
		NULL);

	if (handle != INVALID_HANDLE_VALUE) {
		if (opened_overlapped) {
			*opened_overlapped = 1;
		}
		return handle;
	}

	handle = g_winhid_api.CreateFileW(
		pathw,
		GENERIC_READ | GENERIC_WRITE,
		share_mode,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	if (handle != INVALID_HANDLE_VALUE) {
		return handle;
	}

	handle = g_winhid_api.CreateFileW(
		pathw,
		GENERIC_READ,
		share_mode,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_OVERLAPPED,
		NULL);

	if (handle != INVALID_HANDLE_VALUE) {
		if (opened_overlapped) {
			*opened_overlapped = 1;
		}
		return handle;
	}

	handle = g_winhid_api.CreateFileW(
		pathw,
		GENERIC_READ,
		share_mode,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	return handle;
}

static int winhid_query_caps(HANDLE handle, win_hidp_caps_t *caps, win_phidp_preparsed_data_t *preparsed)
{
	ULONG status;

	if (!handle || handle == INVALID_HANDLE_VALUE || !caps || !preparsed) {
		return 0;
	}

	*preparsed = NULL;
	memset(caps, 0, sizeof(*caps));

	if (!g_winhid_api.HidD_GetPreparsedData(handle, preparsed)) {
		return 0;
	}

	status = g_winhid_api.HidP_GetCaps(*preparsed, caps);
	if (status != WINHID_HIDP_STATUS_SUCCESS) {
		g_winhid_api.HidD_FreePreparsedData(*preparsed);
		*preparsed = NULL;
		return 0;
	}

	return 1;
}

static size_t winhid_get_value_caps(
	win_phidp_preparsed_data_t preparsed,
	ULONG report_type,
	USHORT count,
	win_hidp_value_caps_t **out)
{
	USHORT len;
	ULONG status;
	win_hidp_value_caps_t *arr;

	if (!out) {
		return 0;
	}
	*out = NULL;

	if (!preparsed || count == 0) {
		return 0;
	}

	arr = (win_hidp_value_caps_t *)calloc((size_t)count, sizeof(*arr));
	if (!arr) {
		return 0;
	}

	len = count;
	status = g_winhid_api.HidP_GetValueCaps(report_type, arr, &len, preparsed);
	if (status != WINHID_HIDP_STATUS_SUCCESS) {
		free(arr);
		return 0;
	}

	*out = arr;
	return (size_t)len;
}

static size_t winhid_get_button_caps(
	win_phidp_preparsed_data_t preparsed,
	ULONG report_type,
	USHORT count,
	win_hidp_button_caps_t **out)
{
	USHORT len;
	ULONG status;
	win_hidp_button_caps_t *arr;

	if (!out) {
		return 0;
	}
	*out = NULL;

	if (!preparsed || count == 0) {
		return 0;
	}

	arr = (win_hidp_button_caps_t *)calloc((size_t)count, sizeof(*arr));
	if (!arr) {
		return 0;
	}

	len = count;
	status = g_winhid_api.HidP_GetButtonCaps(report_type, arr, &len, preparsed);
	if (status != WINHID_HIDP_STATUS_SUCCESS) {
		free(arr);
		return 0;
	}

	*out = arr;
	return (size_t)len;
}

static void winhid_track_input_report_id(winhid_dev_ctx_t *ctx, unsigned int report_id)
{
	if (!ctx || report_id > 0xFFU) {
		return;
	}

	if (report_id == 0U) {
		ctx->input_report_id_zero_seen = 1;
		return;
	}

	if (ctx->has_input_report_id[report_id]) {
		return;
	}

	ctx->has_input_report_id[report_id] = 1U;
	ctx->input_report_ids[ctx->input_report_id_count++] = (unsigned char)report_id;
}

static int winhid_desc_reserve(winhid_descbuf_t *db, size_t extra)
{
	size_t need;
	size_t ncap;
	unsigned char *ndata;

	if (!db) {
		return 0;
	}

	if (extra > (SIZE_MAX - db->len)) {
		return 0;
	}

	need = db->len + extra;
	if (need <= db->cap) {
		return 1;
	}

	ncap = db->cap ? db->cap : 512U;
	while (ncap < need) {
		if (ncap > (SIZE_MAX / 2U)) {
			ncap = need;
			break;
		}
		ncap *= 2U;
	}

	ndata = (unsigned char *)realloc(db->data, ncap);
	if (!ndata) {
		return 0;
	}

	db->data = ndata;
	db->cap = ncap;
	return 1;
}

static int winhid_desc_append(winhid_descbuf_t *db, const unsigned char *src, size_t srclen)
{
	if (!db || !src || srclen == 0) {
		return 0;
	}
	if (!winhid_desc_reserve(db, srclen)) {
		return 0;
	}
	memcpy(db->data + db->len, src, srclen);
	db->len += srclen;
	return 1;
}

static int winhid_desc_emit_item(winhid_descbuf_t *db, unsigned int type, unsigned int tag, unsigned int size, uint32_t value)
{
	unsigned char hdr;
	unsigned char payload[4];
	unsigned int i;
	unsigned int code;

	if (!db) {
		return 0;
	}

	switch (size) {
	case 0:
		code = 0;
		break;
	case 1:
		code = 1;
		break;
	case 2:
		code = 2;
		break;
	case 4:
		code = 3;
		break;
	default:
		return 0;
	}

	hdr = (unsigned char)(((tag & 0x0FU) << 4) | ((type & 0x03U) << 2) | (code & 0x03U));
	if (!winhid_desc_append(db, &hdr, 1)) {
		return 0;
	}

	for (i = 0; i < size; i++) {
		payload[i] = (unsigned char)((value >> (8U * i)) & 0xFFU);
	}

	if (size > 0U) {
		if (!winhid_desc_append(db, payload, size)) {
			return 0;
		}
	}

	return 1;
}

static int winhid_desc_emit_u(winhid_descbuf_t *db, unsigned int type, unsigned int tag, uint32_t value)
{
	unsigned int size;

	if (value <= 0xFFU) {
		size = 1U;
	} else if (value <= 0xFFFFU) {
		size = 2U;
	} else {
		size = 4U;
	}

	return winhid_desc_emit_item(db, type, tag, size, value);
}

static int winhid_desc_emit_s(winhid_descbuf_t *db, unsigned int type, unsigned int tag, int32_t value)
{
	unsigned int size;

	if (value >= -128 && value <= 127) {
		size = 1U;
	} else if (value >= -32768 && value <= 32767) {
		size = 2U;
	} else {
		size = 4U;
	}

	return winhid_desc_emit_item(db, type, tag, size, (uint32_t)value);
}

static int winhid_desc_emit_usage_page(winhid_descbuf_t *db, uint32_t v)
{
	return winhid_desc_emit_u(db, WINHID_TYPE_GLOBAL, WINHID_GLOBAL_USAGE_PAGE, v);
}

static int winhid_desc_emit_usage(winhid_descbuf_t *db, uint32_t v)
{
	return winhid_desc_emit_u(db, WINHID_TYPE_LOCAL, WINHID_LOCAL_USAGE, v);
}

static int winhid_desc_emit_usage_min(winhid_descbuf_t *db, uint32_t v)
{
	return winhid_desc_emit_u(db, WINHID_TYPE_LOCAL, WINHID_LOCAL_USAGE_MIN, v);
}

static int winhid_desc_emit_usage_max(winhid_descbuf_t *db, uint32_t v)
{
	return winhid_desc_emit_u(db, WINHID_TYPE_LOCAL, WINHID_LOCAL_USAGE_MAX, v);
}

static int winhid_desc_emit_collection(winhid_descbuf_t *db, uint8_t coll_type)
{
	return winhid_desc_emit_item(db, WINHID_TYPE_MAIN, WINHID_MAIN_COLLECTION, 1, coll_type);
}

static int winhid_desc_emit_end_collection(winhid_descbuf_t *db)
{
	return winhid_desc_emit_item(db, WINHID_TYPE_MAIN, WINHID_MAIN_ENDCOLLECTION, 0, 0);
}

static int winhid_desc_emit_main_data(winhid_descbuf_t *db, unsigned int report_type, uint8_t flags)
{
	unsigned int tag;

	switch (report_type) {
	case WINHID_REPORT_INPUT:
		tag = WINHID_MAIN_INPUT;
		break;
	case WINHID_REPORT_OUTPUT:
		tag = WINHID_MAIN_OUTPUT;
		break;
	case WINHID_REPORT_FEATURE:
		tag = WINHID_MAIN_FEATURE;
		break;
	default:
		return 0;
	}

	return winhid_desc_emit_item(db, WINHID_TYPE_MAIN, tag, 1, flags);
}

static int winhid_desc_emit_report_id(winhid_descbuf_t *db, unsigned int report_id)
{
	if (report_id == 0U) {
		return 1;
	}
	return winhid_desc_emit_u(db, WINHID_TYPE_GLOBAL, WINHID_GLOBAL_REPORT_ID, (uint32_t)report_id);
}

static int winhid_is_present_status_link(uint16_t link_usage_page, uint16_t link_usage)
{
	return (link_usage_page == WINHID_USAGE_PAGE_POWER_DEVICE
		&& link_usage == WINHID_USAGE_PRESENT_STATUS);
}

static int winhid_usage_is_battery_status_semantic(uint16_t usage)
{
	switch (usage) {
	case 0x0042U: /* BelowRemainingCapacityLimit */
	case 0x0043U: /* RemainingTimeLimitExpired */
	case 0x0044U: /* Charging */
	case 0x0045U: /* Discharging */
	case 0x0046U: /* FullyCharged */
	case 0x0047U: /* FullyDischarged */
	case 0x004BU: /* NeedReplacement */
	case 0x00D0U: /* ACPresent */
	case 0x00D1U: /* BatteryPresent */
		return 1;
	default:
		return 0;
	}
}

static int winhid_range_is_battery_status_semantic(uint16_t usage_min, uint16_t usage_max)
{
	uint16_t usage;

	if (usage_max < usage_min) {
		return 0;
	}

	for (usage = usage_min; usage <= usage_max; usage++) {
		if (!winhid_usage_is_battery_status_semantic(usage)) {
			return 0;
		}
		if (usage == UINT16_MAX) {
			break;
		}
	}

	return 1;
}

static int winhid_emit_one_value_cap(
	winhid_descbuf_t *db,
	const win_hidp_value_caps_t *vc,
	unsigned int report_type,
	unsigned int *last_report_id)
{
	int opened_link = 0;
	int status_link;
	uint16_t usage_page;
	uint8_t main_flags = 0x02;
	uint32_t usage = 0;

	if (!db || !vc || !last_report_id) {
		return 0;
	}

	if ((unsigned int)vc->ReportID != *last_report_id) {
		if (!winhid_desc_emit_report_id(db, (unsigned int)vc->ReportID)) {
			return 0;
		}
		*last_report_id = (unsigned int)vc->ReportID;
	}

	status_link = winhid_is_present_status_link(vc->LinkUsagePage, vc->LinkUsage);
	if (vc->LinkUsagePage != 0U && vc->LinkUsage != 0U) {
		if (status_link
		 && (!winhid_desc_emit_usage_page(db, WINHID_USAGE_PAGE_POWER_DEVICE)
		  || !winhid_desc_emit_usage(db, WINHID_USAGE_POWER_SUMMARY)
		  || !winhid_desc_emit_collection(db, WINHID_COLLECTION_PHYSICAL))) {
			return 0;
		}
		if (!winhid_desc_emit_usage_page(db, vc->LinkUsagePage)
		 || !winhid_desc_emit_usage(db, vc->LinkUsage)
		 || !winhid_desc_emit_collection(db, WINHID_COLLECTION_PHYSICAL)) {
			return 0;
		}
		opened_link = status_link ? 2 : 1;
	}

	usage_page = vc->UsagePage;
	if (status_link
	 && usage_page == WINHID_USAGE_PAGE_POWER_DEVICE
	) {
		if (vc->IsRange) {
			if (winhid_range_is_battery_status_semantic(vc->u.Range.UsageMin, vc->u.Range.UsageMax)) {
				usage_page = WINHID_USAGE_PAGE_BATTERY_SYSTEM;
			}
		} else {
			if (winhid_usage_is_battery_status_semantic(vc->u.NotRange.Usage)) {
				usage_page = WINHID_USAGE_PAGE_BATTERY_SYSTEM;
			}
		}
	}

	if (!winhid_desc_emit_usage_page(db, usage_page)) {
		return 0;
	}

	if (vc->IsRange) {
		if (!winhid_desc_emit_usage_min(db, vc->u.Range.UsageMin)
		 || !winhid_desc_emit_usage_max(db, vc->u.Range.UsageMax)) {
			return 0;
		}
	} else {
		usage = vc->u.NotRange.Usage;
		if (!winhid_desc_emit_usage(db, usage)) {
			return 0;
		}
	}

	if (!winhid_desc_emit_s(db, WINHID_TYPE_GLOBAL, WINHID_GLOBAL_LOGICAL_MIN, vc->LogicalMin)
	 || !winhid_desc_emit_s(db, WINHID_TYPE_GLOBAL, WINHID_GLOBAL_LOGICAL_MAX, vc->LogicalMax)
	 || !winhid_desc_emit_s(db, WINHID_TYPE_GLOBAL, WINHID_GLOBAL_PHYSICAL_MIN, vc->PhysicalMin)
	 || !winhid_desc_emit_s(db, WINHID_TYPE_GLOBAL, WINHID_GLOBAL_PHYSICAL_MAX, vc->PhysicalMax)
	 || !winhid_desc_emit_s(db, WINHID_TYPE_GLOBAL, WINHID_GLOBAL_UNIT_EXP, (int32_t)vc->UnitsExp)
	 || !winhid_desc_emit_u(db, WINHID_TYPE_GLOBAL, WINHID_GLOBAL_UNIT, vc->Units)
	 || !winhid_desc_emit_u(db, WINHID_TYPE_GLOBAL, WINHID_GLOBAL_REPORT_SIZE, vc->BitSize ? vc->BitSize : 1U)
	 || !winhid_desc_emit_u(db, WINHID_TYPE_GLOBAL, WINHID_GLOBAL_REPORT_COUNT, vc->ReportCount ? vc->ReportCount : 1U)) {
		return 0;
	}

	if (!vc->IsAbsolute) {
		main_flags |= 0x04;
	}

	if (!winhid_desc_emit_main_data(db, report_type, main_flags)) {
		return 0;
	}

	if (opened_link) {
		if (!winhid_desc_emit_end_collection(db)) {
			return 0;
		}
		if (opened_link > 1 && !winhid_desc_emit_end_collection(db)) {
			return 0;
		}
	}

	return 1;
}

static int winhid_emit_one_button_cap(
	winhid_descbuf_t *db,
	const win_hidp_button_caps_t *bc,
	unsigned int report_type,
	unsigned int *last_report_id)
{
	int opened_link = 0;
	int status_link;
	uint16_t usage_page;
	uint32_t count = 1U;

	if (!db || !bc || !last_report_id) {
		return 0;
	}

	if ((unsigned int)bc->ReportID != *last_report_id) {
		if (!winhid_desc_emit_report_id(db, (unsigned int)bc->ReportID)) {
			return 0;
		}
		*last_report_id = (unsigned int)bc->ReportID;
	}

	status_link = winhid_is_present_status_link(bc->LinkUsagePage, bc->LinkUsage);
	if (bc->LinkUsagePage != 0U && bc->LinkUsage != 0U) {
		if (status_link
		 && (!winhid_desc_emit_usage_page(db, WINHID_USAGE_PAGE_POWER_DEVICE)
		  || !winhid_desc_emit_usage(db, WINHID_USAGE_POWER_SUMMARY)
		  || !winhid_desc_emit_collection(db, WINHID_COLLECTION_PHYSICAL))) {
			return 0;
		}
		if (!winhid_desc_emit_usage_page(db, bc->LinkUsagePage)
		 || !winhid_desc_emit_usage(db, bc->LinkUsage)
		 || !winhid_desc_emit_collection(db, WINHID_COLLECTION_PHYSICAL)) {
			return 0;
		}
		opened_link = status_link ? 2 : 1;
	}

	usage_page = bc->UsagePage;
	if (status_link
	 && usage_page == WINHID_USAGE_PAGE_POWER_DEVICE
	) {
		if (bc->IsRange) {
			if (winhid_range_is_battery_status_semantic(bc->u.Range.UsageMin, bc->u.Range.UsageMax)) {
				usage_page = WINHID_USAGE_PAGE_BATTERY_SYSTEM;
			}
		} else {
			if (winhid_usage_is_battery_status_semantic(bc->u.NotRange.Usage)) {
				usage_page = WINHID_USAGE_PAGE_BATTERY_SYSTEM;
			}
		}
	}

	if (!winhid_desc_emit_usage_page(db, usage_page)) {
		return 0;
	}

	if (bc->IsRange) {
		if (!winhid_desc_emit_usage_min(db, bc->u.Range.UsageMin)
		 || !winhid_desc_emit_usage_max(db, bc->u.Range.UsageMax)) {
			return 0;
		}
		if (bc->u.Range.UsageMax >= bc->u.Range.UsageMin) {
			count = (uint32_t)(bc->u.Range.UsageMax - bc->u.Range.UsageMin + 1U);
		}
	} else {
		if (!winhid_desc_emit_usage(db, bc->u.NotRange.Usage)) {
			return 0;
		}
	}

	if (!winhid_desc_emit_s(db, WINHID_TYPE_GLOBAL, WINHID_GLOBAL_LOGICAL_MIN, 0)
	 || !winhid_desc_emit_s(db, WINHID_TYPE_GLOBAL, WINHID_GLOBAL_LOGICAL_MAX, 1)
	 || !winhid_desc_emit_u(db, WINHID_TYPE_GLOBAL, WINHID_GLOBAL_REPORT_SIZE, 1)
	 || !winhid_desc_emit_u(db, WINHID_TYPE_GLOBAL, WINHID_GLOBAL_REPORT_COUNT, count)
	 || !winhid_desc_emit_main_data(db, report_type, 0x02)) {
		return 0;
	}

	if (opened_link) {
		if (!winhid_desc_emit_end_collection(db)) {
			return 0;
		}
		if (opened_link > 1 && !winhid_desc_emit_end_collection(db)) {
			return 0;
		}
	}

	return 1;
}

static int winhid_build_descriptor_from_caps(
	const win_hidp_caps_t *caps,
	const win_hidp_value_caps_t *in_vals, size_t in_vals_n,
	const win_hidp_button_caps_t *in_btns, size_t in_btns_n,
	const win_hidp_value_caps_t *out_vals, size_t out_vals_n,
	const win_hidp_button_caps_t *out_btns, size_t out_btns_n,
	const win_hidp_value_caps_t *feat_vals, size_t feat_vals_n,
	const win_hidp_button_caps_t *feat_btns, size_t feat_btns_n,
	unsigned char *out_buf,
	size_t out_buf_size,
	usb_ctrl_charbufsize *out_len)
{
	winhid_descbuf_t db;
	size_t i;
	unsigned int last_report_id;

	if (!caps || !out_buf || !out_len) {
		return 0;
	}

	memset(&db, 0, sizeof(db));

	if (!winhid_desc_emit_usage_page(&db, caps->UsagePage)
	 || !winhid_desc_emit_usage(&db, caps->Usage)
	 || !winhid_desc_emit_collection(&db, WINHID_COLLECTION_APPLICATION)) {
		free(db.data);
		return 0;
	}

	last_report_id = UINT_MAX;
	for (i = 0; i < in_vals_n; i++) {
		if (!winhid_emit_one_value_cap(&db, &in_vals[i], WINHID_REPORT_INPUT, &last_report_id)) {
			free(db.data);
			return 0;
		}
	}
	for (i = 0; i < in_btns_n; i++) {
		if (!winhid_emit_one_button_cap(&db, &in_btns[i], WINHID_REPORT_INPUT, &last_report_id)) {
			free(db.data);
			return 0;
		}
	}

	last_report_id = UINT_MAX;
	for (i = 0; i < out_vals_n; i++) {
		if (!winhid_emit_one_value_cap(&db, &out_vals[i], WINHID_REPORT_OUTPUT, &last_report_id)) {
			free(db.data);
			return 0;
		}
	}
	for (i = 0; i < out_btns_n; i++) {
		if (!winhid_emit_one_button_cap(&db, &out_btns[i], WINHID_REPORT_OUTPUT, &last_report_id)) {
			free(db.data);
			return 0;
		}
	}

	last_report_id = UINT_MAX;
	for (i = 0; i < feat_vals_n; i++) {
		if (!winhid_emit_one_value_cap(&db, &feat_vals[i], WINHID_REPORT_FEATURE, &last_report_id)) {
			free(db.data);
			return 0;
		}
	}
	for (i = 0; i < feat_btns_n; i++) {
		if (!winhid_emit_one_button_cap(&db, &feat_btns[i], WINHID_REPORT_FEATURE, &last_report_id)) {
			free(db.data);
			return 0;
		}
	}

	if (!winhid_desc_emit_end_collection(&db)) {
		free(db.data);
		return 0;
	}

	if (db.len > (size_t)USB_CTRL_CHARBUFSIZE_MAX
	 || db.len > out_buf_size) {
		free(db.data);
		return 0;
	}

	memcpy(out_buf, db.data, db.len);
	*out_len = (usb_ctrl_charbufsize)db.len;
	free(db.data);
	return 1;
}

static int winhid_collect_caps_and_optional_descriptor(
	HANDLE handle,
	winhid_dev_ctx_t *ctx,
	unsigned char *rdbuf,
	size_t rdbuf_size,
	usb_ctrl_charbufsize *rdlen)
{
	win_hidp_caps_t caps;
	win_phidp_preparsed_data_t preparsed;
	win_hidp_value_caps_t *in_vals = NULL, *out_vals = NULL, *feat_vals = NULL;
	win_hidp_button_caps_t *in_btns = NULL, *out_btns = NULL, *feat_btns = NULL;
	size_t in_vals_n = 0, out_vals_n = 0, feat_vals_n = 0;
	size_t in_btns_n = 0, out_btns_n = 0, feat_btns_n = 0;
	size_t i;
	int ok = 0;

	if (!handle || handle == INVALID_HANDLE_VALUE || !ctx) {
		return 0;
	}

	preparsed = NULL;
	if (!winhid_query_caps(handle, &caps, &preparsed)) {
		return 0;
	}

	ctx->input_report_len = (size_t)caps.InputReportByteLength;
	ctx->feature_report_len = (size_t)caps.FeatureReportByteLength;

	in_vals_n = winhid_get_value_caps(preparsed, WINHID_REPORT_INPUT, caps.NumberInputValueCaps, &in_vals);
	out_vals_n = winhid_get_value_caps(preparsed, WINHID_REPORT_OUTPUT, caps.NumberOutputValueCaps, &out_vals);
	feat_vals_n = winhid_get_value_caps(preparsed, WINHID_REPORT_FEATURE, caps.NumberFeatureValueCaps, &feat_vals);

	in_btns_n = winhid_get_button_caps(preparsed, WINHID_REPORT_INPUT, caps.NumberInputButtonCaps, &in_btns);
	out_btns_n = winhid_get_button_caps(preparsed, WINHID_REPORT_OUTPUT, caps.NumberOutputButtonCaps, &out_btns);
	feat_btns_n = winhid_get_button_caps(preparsed, WINHID_REPORT_FEATURE, caps.NumberFeatureButtonCaps, &feat_btns);

	for (i = 0; i < in_vals_n; i++) {
		winhid_track_input_report_id(ctx, in_vals[i].ReportID);
	}
	for (i = 0; i < in_btns_n; i++) {
		winhid_track_input_report_id(ctx, in_btns[i].ReportID);
	}

	if (rdbuf && rdlen) {
		ok = winhid_build_descriptor_from_caps(
			&caps,
			in_vals, in_vals_n,
			in_btns, in_btns_n,
			out_vals, out_vals_n,
			out_btns, out_btns_n,
			feat_vals, feat_vals_n,
			feat_btns, feat_btns_n,
			rdbuf,
			rdbuf_size,
			rdlen);
	} else {
		ok = 1;
	}

	if (g_winhid_api.HidD_FreePreparsedData && preparsed) {
		g_winhid_api.HidD_FreePreparsedData(preparsed);
	}
	free(in_vals);
	free(out_vals);
	free(feat_vals);
	free(in_btns);
	free(out_btns);
	free(feat_btns);

	return ok;
}

static void winhid_free_ctx(winhid_dev_ctx_t *ctx)
{
	if (!ctx) {
		return;
	}

	if (ctx->event && ctx->event != INVALID_HANDLE_VALUE) {
		g_winhid_api.CloseHandle(ctx->event);
		ctx->event = NULL;
	}
	if (ctx->handle && ctx->handle != INVALID_HANDLE_VALUE) {
		g_winhid_api.CloseHandle(ctx->handle);
		ctx->handle = NULL;
	}

	free(ctx->read_buf);
	free(ctx->device_path);
	free(ctx);
}

static int nut_winhid_open(
	usb_dev_handle **udevp,
	USBDevice_t *curDevice,
	USBDeviceMatcher_t *matcher,
	int (*callback)(usb_dev_handle *udev, USBDevice_t *hd,
		usb_ctrl_charbuf rdbuf, usb_ctrl_charbufsize rdlen))
{
	HDEVINFO hinfo;
	DWORD index;
	int ret;
	int count_open_errors = 0;
	int count_open_attempts = 0;

	if (!udevp || !curDevice) {
		return -1;
	}
	*udevp = NULL;

	if (!winhid_load_apis()) {
		upslogx(LOG_WARNING, "%s: failed to initialize Windows HID API bindings", __func__);
		return -1;
	}

	hinfo = g_winhid_api.SetupDiGetClassDevsW(
		&g_hid_interface_guid,
		NULL,
		NULL,
		DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

	if (hinfo == INVALID_HANDLE_VALUE) {
		upsdebugx(1, "%s: SetupDiGetClassDevsW failed", __func__);
		return -1;
	}

	for (index = 0; ; index++) {
		win_sp_device_interface_data_t iface_data;
		DWORD required = 0;
		win_sp_devinfo_data_t devinfo;
		char *detail_buf = NULL;
		WCHAR *device_path_w = NULL;
		char *device_path = NULL;
		HANDLE handle = INVALID_HANDLE_VALUE;
		int opened_overlapped = 0;
		winhid_dev_ctx_t *ctx = NULL;
		USBDeviceMatcher_t *m;

		memset(&iface_data, 0, sizeof(iface_data));
		iface_data.cbSize = sizeof(iface_data);

		if (!g_winhid_api.SetupDiEnumDeviceInterfaces(
			hinfo,
			NULL,
			&g_hid_interface_guid,
			index,
			&iface_data)) {
			DWORD err = GetLastError();
			if (err == ERROR_NO_MORE_ITEMS) {
				break;
			}
			upsdebugx(1, "%s: SetupDiEnumDeviceInterfaces failed with %lu", __func__, (unsigned long)err);
			continue;
		}

		memset(&devinfo, 0, sizeof(devinfo));
		devinfo.cbSize = sizeof(devinfo);
		g_winhid_api.SetupDiGetDeviceInterfaceDetailW(
			hinfo,
			&iface_data,
			NULL,
			0,
			&required,
			&devinfo);

		if (required == 0) {
			continue;
		}

		detail_buf = (char *)calloc((size_t)required, 1);
		if (!detail_buf) {
			fatal_with_errno(EXIT_FAILURE, "%s: calloc(detail_buf)", __func__);
		}

		if (sizeof(void *) == 8U) {
			*((DWORD *)detail_buf) = 8U;
		} else {
			*((DWORD *)detail_buf) = 6U;
		}

		if (!g_winhid_api.SetupDiGetDeviceInterfaceDetailW(
			hinfo,
			&iface_data,
			detail_buf,
			required,
			&required,
			&devinfo)) {
			free(detail_buf);
			continue;
		}

		device_path_w = (WCHAR *)(void *)(detail_buf + 4);
		if (!winhid_is_sane_device_path(device_path_w)) {
			device_path_w = (WCHAR *)(void *)(detail_buf + 8);
		}
		if (!winhid_is_sane_device_path(device_path_w)) {
			free(detail_buf);
			continue;
		}

		device_path = winhid_wstr_to_ascii(device_path_w);
		if (!device_path) {
			free(detail_buf);
			fatal_with_errno(EXIT_FAILURE, "%s: winhid_wstr_to_ascii", __func__);
		}

		count_open_attempts++;
		handle = winhid_open_hid_path(device_path_w, &opened_overlapped);
		if (handle == INVALID_HANDLE_VALUE) {
			count_open_errors++;
			upsdebugx(2, "%s: could not open '%s'", __func__, device_path);
			free(device_path);
			free(detail_buf);
			continue;
		}

		winhid_reset_curdevice(curDevice);
		winhid_fill_device_identity(curDevice, device_path, (size_t)index);

		ret = 1;
		for (m = matcher; m != NULL; m = m->next) {
			ret = matches(m, curDevice);
			if (ret != 1) {
				break;
			}
		}

		if (ret != 1) {
			g_winhid_api.CloseHandle(handle);
			free(device_path);
			free(detail_buf);
			continue;
		}

		ctx = (winhid_dev_ctx_t *)calloc(1, sizeof(*ctx));
		if (!ctx) {
			g_winhid_api.CloseHandle(handle);
			free(device_path);
			free(detail_buf);
			fatal_with_errno(EXIT_FAILURE, "%s: calloc(ctx)", __func__);
		}

		ctx->handle = handle;
		ctx->device_path = device_path;
		ctx->use_overlapped_io = opened_overlapped;

		if (callback) {
			unsigned char rdbuf[WINHID_MAX_REPORT_SIZE];
			usb_ctrl_charbufsize rdlen = 0;
			int cbret;

			if (!winhid_collect_caps_and_optional_descriptor(handle, ctx, rdbuf, sizeof(rdbuf), &rdlen)) {
				upsdebugx(2, "%s: could not synthesize report descriptor for '%s'", __func__, device_path);
				winhid_free_ctx(ctx);
				free(detail_buf);
				continue;
			}

			cbret = callback((usb_dev_handle *)ctx, curDevice, rdbuf, rdlen);
			if (cbret < 1) {
				upsdebugx(2, "%s: callback rejected '%s'", __func__, device_path);
				winhid_free_ctx(ctx);
				free(detail_buf);
				continue;
			}
		} else {
			/* reconnect path: best-effort report metadata for runtime reads */
			(void)winhid_collect_caps_and_optional_descriptor(handle, ctx, NULL, 0, NULL);
		}

		upsdebugx(2, "%s: accepted HID device '%s' (%04x/%04x)",
			__func__, device_path,
			(unsigned int)curDevice->VendorID,
			(unsigned int)curDevice->ProductID);

		*udevp = (usb_dev_handle *)ctx;
		free(detail_buf);
		g_winhid_api.SetupDiDestroyDeviceInfoList(hinfo);
		return 1;
	}

	g_winhid_api.SetupDiDestroyDeviceInfoList(hinfo);
	upsdebugx(2, "%s: no appropriate HID device found (attempted=%d open_errors=%d)",
		__func__, count_open_attempts, count_open_errors);
	return -1;
}

static int nut_winhid_get_report(
	usb_dev_handle *sdev,
	usb_ctrl_repindex ReportId,
	usb_ctrl_charbuf raw_buf,
	usb_ctrl_charbufsize ReportSize)
{
	winhid_dev_ctx_t *ctx;
	size_t query_size;
	unsigned char *tmp;
	DWORD err;
	size_t copy_len;

	if (!sdev || !raw_buf || ReportSize < 1) {
		return 0;
	}

	ctx = (winhid_dev_ctx_t *)sdev;
	if (!ctx->handle || ctx->handle == INVALID_HANDLE_VALUE) {
		return LIBUSB_ERROR_NO_DEVICE;
	}

	query_size = (size_t)ReportSize;
	if (ctx->feature_report_len > query_size) {
		query_size = ctx->feature_report_len;
	}

	if (query_size < 1 || query_size > (size_t)USB_CTRL_CHARBUFSIZE_MAX) {
		return LIBUSB_ERROR_INVALID_PARAM;
	}

	tmp = (unsigned char *)calloc(query_size, 1);
	if (!tmp) {
		return LIBUSB_ERROR_NO_MEM;
	}

	tmp[0] = (unsigned char)(ReportId & 0xffU);
	if (!g_winhid_api.HidD_GetFeature(ctx->handle, tmp, (ULONG)query_size)) {
		err = GetLastError();
		free(tmp);
		if (err == ERROR_INVALID_FUNCTION) {
			return 0;
		}
		return winhid_map_winerr_to_libusb(err);
	}

	copy_len = ((size_t)ReportSize < query_size) ? (size_t)ReportSize : query_size;
	memcpy(raw_buf, tmp, copy_len);
	free(tmp);

	return (int)copy_len;
}

static int nut_winhid_set_report(
	usb_dev_handle *sdev,
	usb_ctrl_repindex ReportId,
	usb_ctrl_charbuf raw_buf,
	usb_ctrl_charbufsize ReportSize)
{
	NUT_UNUSED_VARIABLE(sdev);
	NUT_UNUSED_VARIABLE(ReportId);
	NUT_UNUSED_VARIABLE(raw_buf);
	NUT_UNUSED_VARIABLE(ReportSize);

	/* Phase-1 limitation: HidD_SetFeature was intentionally not used yet. */
	upsdebugx(2, "%s: not implemented in phase-1 backend", __func__);
	return LIBUSB_ERROR_NOT_SUPPORTED;
}

static int nut_winhid_get_string(
	usb_dev_handle *sdev,
	usb_ctrl_strindex StringIdx,
	char *buf,
	usb_ctrl_charbufsize buflen)
{
	NUT_UNUSED_VARIABLE(sdev);
	NUT_UNUSED_VARIABLE(StringIdx);

	if (buf && buflen > 0) {
		buf[0] = '\0';
	}

	return 0;
}

static int winhid_get_interrupt_overlapped(
	winhid_dev_ctx_t *ctx,
	usb_ctrl_charbuf buf,
	usb_ctrl_charbufsize bufsize,
	usb_ctrl_timeout_msec timeout)
{
	DWORD got = 0;
	DWORD waitres;
	DWORD err;
	size_t req_size;
	size_t copy_len;

	if (!ctx || !buf || bufsize < 1) {
		return LIBUSB_ERROR_INVALID_PARAM;
	}

	if (!ctx->event) {
		ctx->event = g_winhid_api.CreateEventW(NULL, TRUE, FALSE, NULL);
		if (!ctx->event) {
			return winhid_map_winerr_to_libusb(GetLastError());
		}
	}

	req_size = (size_t)bufsize;
	if (ctx->input_report_len > req_size) {
		req_size = ctx->input_report_len;
	}
	if (req_size < 1 || req_size > (size_t)USB_CTRL_CHARBUFSIZE_MAX) {
		return LIBUSB_ERROR_INVALID_PARAM;
	}
	if (ctx->read_buf_size < req_size) {
		unsigned char *nbuf = (unsigned char *)realloc(ctx->read_buf, req_size);
		if (!nbuf) {
			return LIBUSB_ERROR_NO_MEM;
		}
		ctx->read_buf = nbuf;
		ctx->read_buf_size = req_size;
	}

	if (!ctx->read_pending) {
		if (!g_winhid_api.ResetEvent(ctx->event)) {
			return winhid_map_winerr_to_libusb(GetLastError());
		}

		memset(&ctx->read_ov, 0, sizeof(ctx->read_ov));
		ctx->read_ov.hEvent = ctx->event;

		if (!g_winhid_api.ReadFile(ctx->handle, ctx->read_buf, (DWORD)ctx->read_buf_size, &got, &ctx->read_ov)) {
			err = GetLastError();
			if (err == ERROR_INVALID_FUNCTION) {
				return LIBUSB_ERROR_NOT_SUPPORTED;
			}
			if (err != ERROR_IO_PENDING) {
				return winhid_map_winerr_to_libusb(err);
			}

			ctx->read_pending = 1;
		} else {
			copy_len = (size_t)got;
			if (copy_len > (size_t)bufsize) {
				copy_len = (size_t)bufsize;
			}
			memcpy(buf, ctx->read_buf, copy_len);
			return (int)copy_len;
		}
	}

	waitres = g_winhid_api.WaitForSingleObject(ctx->event, (DWORD)timeout);
	if (waitres == WAIT_TIMEOUT) {
		return 0;
	}
	if (waitres != WAIT_OBJECT_0) {
		return winhid_map_winerr_to_libusb(GetLastError());
	}

	if (!g_winhid_api.GetOverlappedResult(ctx->handle, &ctx->read_ov, &got, FALSE)) {
		err = GetLastError();
		ctx->read_pending = 0;
		return winhid_map_winerr_to_libusb(err);
	}

	ctx->read_pending = 0;
	copy_len = (size_t)got;
	if (copy_len > (size_t)bufsize) {
		copy_len = (size_t)bufsize;
	}
	memcpy(buf, ctx->read_buf, copy_len);

	return (int)copy_len;
}

static int winhid_get_input_report_control(
	winhid_dev_ctx_t *ctx,
	usb_ctrl_charbuf buf,
	usb_ctrl_charbufsize bufsize)
{
	size_t query_size;
	unsigned char *tmp;
	unsigned int rid = 0U;
	size_t copy_len;
	DWORD err;

	if (!ctx || !buf || bufsize < 1) {
		return 0;
	}

	if (ctx->input_report_id_count > 0) {
		rid = (unsigned int)ctx->input_report_ids[
			ctx->input_report_id_cursor % ctx->input_report_id_count
		];
		ctx->input_report_id_cursor++;
	} else if (!ctx->input_report_id_zero_seen) {
		return 0;
	}

	query_size = (size_t)bufsize;
	if (ctx->input_report_len > query_size) {
		query_size = ctx->input_report_len;
	}
	if (query_size < 1 || query_size > (size_t)USB_CTRL_CHARBUFSIZE_MAX) {
		return 0;
	}

	tmp = (unsigned char *)calloc(query_size, 1);
	if (!tmp) {
		return LIBUSB_ERROR_NO_MEM;
	}
	tmp[0] = (unsigned char)(rid & 0xFFU);

	if (!g_winhid_api.HidD_GetInputReport(ctx->handle, tmp, (ULONG)query_size)) {
		err = GetLastError();
		free(tmp);
		if (err == ERROR_INVALID_FUNCTION) {
			return 0;
		}
		return winhid_map_winerr_to_libusb(err);
	}

	copy_len = ((size_t)bufsize < query_size) ? (size_t)bufsize : query_size;
	memcpy(buf, tmp, copy_len);
	free(tmp);

	return (int)copy_len;
}

static int nut_winhid_get_interrupt(
	usb_dev_handle *sdev,
	usb_ctrl_charbuf buf,
	usb_ctrl_charbufsize bufsize,
	usb_ctrl_timeout_msec timeout)
{
	winhid_dev_ctx_t *ctx;
	int ret;

	if (!sdev || !buf || bufsize < 1) {
		return 0;
	}

	ctx = (winhid_dev_ctx_t *)sdev;
	if (!ctx->handle || ctx->handle == INVALID_HANDLE_VALUE) {
		return LIBUSB_ERROR_NO_DEVICE;
	}

	if (ctx->use_overlapped_io) {
		ret = winhid_get_interrupt_overlapped(ctx, buf, bufsize, timeout);
		if (ret == LIBUSB_ERROR_NOT_SUPPORTED) {
			return winhid_get_input_report_control(ctx, buf, bufsize);
		}
		if (ret != 0) {
			return ret;
		}

		/* timeout in overlapped path: optionally try control report fallback */
		ret = winhid_get_input_report_control(ctx, buf, bufsize);
		if (ret != 0) {
			return ret;
		}
		return 0;
	}

	/* If we could not open with FILE_FLAG_OVERLAPPED, do not issue blocking
	 * synchronous reads in the daemon loop; rely on control-report fallback. */
	return winhid_get_input_report_control(ctx, buf, bufsize);
}

static void nut_winhid_close(usb_dev_handle *sdev)
{
	winhid_dev_ctx_t *ctx = (winhid_dev_ctx_t *)sdev;
	winhid_free_ctx(ctx);
}

usb_communication_subdriver_t winhid_subdriver = {
	WINHID_DRIVER_NAME,
	WINHID_DRIVER_VERSION,
	nut_winhid_open,
	nut_winhid_close,
	nut_winhid_get_report,
	nut_winhid_set_report,
	nut_winhid_get_string,
	nut_winhid_get_interrupt,
	LIBUSB_DEFAULT_CONF_INDEX,
	LIBUSB_DEFAULT_INTERFACE,
	LIBUSB_DEFAULT_DESC_INDEX,
	LIBUSB_DEFAULT_HID_EP_IN,
	LIBUSB_DEFAULT_HID_EP_OUT
};

#else  /* !WIN32 */

/* The source is only meant for WIN32 builds. */

#endif /* WIN32 */
