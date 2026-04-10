/*!
 * @file libwinhid.c
 * @brief NUT USB communication backend using native Windows HID API
 *
 * This backend implements the usb_communication_subdriver_t interface using
 * only the native Windows HID API (SetupAPI, hid.dll, kernel32).  It provides
 * an alternative to the libusb-based backends for Windows builds, avoiding the
 * need for third-party USB filter drivers (e.g. Zadig/WinUSB).
 *
 * Implementation notes:
 *   - All Windows APIs are dynamically loaded at runtime via LoadLibrary/
 *     GetProcAddress so that the build does not require Windows SDK headers.
 *   - A synthetic HID report descriptor is reconstructed from the parsed
 *     HidP caps data, then fed into the existing NUT hidparser/libhid
 *     pipeline unchanged.
 *   - Matcher flow mirrors the libusb backends (VID/PID and regex chain).
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

#include "config.h" /* must be the first header */

#include "common.h"
#include "usb-common.h"
#include "nut_libusb.h"
#include "nut_stdint.h"
#include "libwinhid.h"
#include "hidtypes.h"

#ifdef WIN32

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef BOOLEAN (WINAPI *pHidD_GetIndexedString)(
	HANDLE HidDeviceObject,
	ULONG StringIndex,
	PVOID Buffer,
	ULONG BufferLength);

typedef BOOLEAN (WINAPI *pHidD_GetManufacturerString)(
	HANDLE HidDeviceObject,
	PVOID Buffer,
	ULONG BufferLength);

typedef BOOLEAN (WINAPI *pHidD_GetProductString)(
	HANDLE HidDeviceObject,
	PVOID Buffer,
	ULONG BufferLength);

typedef BOOLEAN (WINAPI *pHidD_GetSerialNumberString)(
	HANDLE HidDeviceObject,
	PVOID Buffer,
	ULONG BufferLength);

typedef BOOLEAN (WINAPI *pHidD_GetStringFn)(
	HANDLE HidDeviceObject,
	PVOID Buffer,
	ULONG BufferLength);

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

typedef ULONG (WINAPI *pHidP_GetLinkCollectionNodes)(
	win_hidp_link_collection_node_t *LinkCollectionNodes,
	ULONG *LinkCollectionNodesLength,
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
	pHidD_GetIndexedString HidD_GetIndexedString;
	pHidD_GetManufacturerString HidD_GetManufacturerString;
	pHidD_GetProductString HidD_GetProductString;
	pHidD_GetSerialNumberString HidD_GetSerialNumberString;
	pHidP_GetCaps HidP_GetCaps;
	pHidP_GetValueCaps HidP_GetValueCaps;
	pHidP_GetButtonCaps HidP_GetButtonCaps;
	pHidP_GetLinkCollectionNodes HidP_GetLinkCollectionNodes;

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
	{
		FARPROC fp = GetProcAddress(g_winhid_api.hid_mod, "HidD_GetIndexedString");
		if (!fp) {
			upsdebugx(2, "%s: optional API HidD_GetIndexedString not found", __func__);
		} else {
			memcpy(&g_winhid_api.HidD_GetIndexedString, &fp, sizeof(fp));
		}
	}
	{
		FARPROC fp = GetProcAddress(g_winhid_api.hid_mod, "HidD_GetManufacturerString");
		if (!fp) {
			upsdebugx(2, "%s: optional API HidD_GetManufacturerString not found", __func__);
		} else {
			memcpy(&g_winhid_api.HidD_GetManufacturerString, &fp, sizeof(fp));
		}
	}
	{
		FARPROC fp = GetProcAddress(g_winhid_api.hid_mod, "HidD_GetProductString");
		if (!fp) {
			upsdebugx(2, "%s: optional API HidD_GetProductString not found", __func__);
		} else {
			memcpy(&g_winhid_api.HidD_GetProductString, &fp, sizeof(fp));
		}
	}
	{
		FARPROC fp = GetProcAddress(g_winhid_api.hid_mod, "HidD_GetSerialNumberString");
		if (!fp) {
			upsdebugx(2, "%s: optional API HidD_GetSerialNumberString not found", __func__);
		} else {
			memcpy(&g_winhid_api.HidD_GetSerialNumberString, &fp, sizeof(fp));
		}
	}
	WINHID_RESOLVE(g_winhid_api.HidP_GetCaps, g_winhid_api.hid_mod, "HidP_GetCaps");
	WINHID_RESOLVE(g_winhid_api.HidP_GetValueCaps, g_winhid_api.hid_mod, "HidP_GetValueCaps");
	WINHID_RESOLVE(g_winhid_api.HidP_GetButtonCaps, g_winhid_api.hid_mod, "HidP_GetButtonCaps");
	{
		FARPROC fp = GetProcAddress(g_winhid_api.hid_mod, "HidP_GetLinkCollectionNodes");
		if (!fp) {
			upsdebugx(2, "%s: optional API HidP_GetLinkCollectionNodes not found", __func__);
		} else {
			memcpy(&g_winhid_api.HidP_GetLinkCollectionNodes, &fp, sizeof(fp));
		}
	}

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

static int winhid_copy_ascii_to_buf(
	const char *src,
	char *dst,
	usb_ctrl_charbufsize dstlen)
{
	size_t copy;

	if (!dst || dstlen < 1) {
		return -1;
	}

	if (!src || src[0] == '\0') {
		dst[0] = '\0';
		return 0;
	}

	copy = strlen(src);
	if (copy >= (size_t)dstlen) {
		copy = (size_t)dstlen - 1U;
	}

	memcpy(dst, src, copy);
	dst[copy] = '\0';
	return (int)copy;
}

static char *winhid_hidd_get_string(HANDLE handle, pHidD_GetStringFn get_string_fn)
{
	WCHAR wbuf[512];

	if (!get_string_fn || !handle || handle == INVALID_HANDLE_VALUE) {
		return NULL;
	}

	memset(wbuf, 0, sizeof(wbuf));
	if (!get_string_fn(handle, wbuf, (ULONG)sizeof(wbuf))) {
		return NULL;
	}
	wbuf[(sizeof(wbuf) / sizeof(wbuf[0])) - 1U] = L'\0';

	return winhid_wstr_to_ascii(wbuf);
}

static void winhid_replace_device_string(char **dst, char *src)
{
	if (!src || !dst) {
		free(src);
		return;
	}

	if (src[0] == '\0') {
		free(src);
		return;
	}

	free(*dst);
	*dst = src;
}

static void winhid_fill_device_strings(HANDLE handle, USBDevice_t *curDevice)
{
	char *s;

	if (!curDevice || !handle || handle == INVALID_HANDLE_VALUE) {
		return;
	}

	s = winhid_hidd_get_string(handle, g_winhid_api.HidD_GetManufacturerString);
	winhid_replace_device_string(&curDevice->Vendor, s);

	s = winhid_hidd_get_string(handle, g_winhid_api.HidD_GetProductString);
	winhid_replace_device_string(&curDevice->Product, s);

	s = winhid_hidd_get_string(handle, g_winhid_api.HidD_GetSerialNumberString);
	winhid_replace_device_string(&curDevice->Serial, s);
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

static size_t winhid_get_link_collection_nodes(
	win_phidp_preparsed_data_t preparsed,
	USHORT count,
	win_hidp_link_collection_node_t **out)
{
	ULONG len;
	ULONG status;
	win_hidp_link_collection_node_t *arr;

	if (!out) {
		return 0;
	}
	*out = NULL;

	if (!preparsed || count == 0) {
		return 0;
	}
	if (!g_winhid_api.HidP_GetLinkCollectionNodes) {
		return 0;
	}

	arr = (win_hidp_link_collection_node_t *)calloc((size_t)count, sizeof(*arr));
	if (!arr) {
		return 0;
	}

	len = (ULONG)count;
	status = g_winhid_api.HidP_GetLinkCollectionNodes(arr, &len, preparsed);
	if (status != WINHID_HIDP_STATUS_SUCCESS) {
		free(arr);
		return 0;
	}

	*out = arr;
	return (size_t)len;
}

static void winhid_debug_dump_feature_caps_summary(
	const win_hidp_value_caps_t *value_caps,
	size_t value_caps_n,
	const win_hidp_button_caps_t *button_caps,
	size_t button_caps_n)
{
	unsigned int i;
	unsigned int value_count_by_rid[256];
	unsigned int button_count_by_rid[256];
	unsigned int seen = 0U;
	char line[1024];
	int n;

	if (nut_debug_level < 3) {
		return;
	}

	memset(value_count_by_rid, 0, sizeof(value_count_by_rid));
	memset(button_count_by_rid, 0, sizeof(button_count_by_rid));

	if (value_caps && value_caps_n > 0U) {
		for (i = 0U; i < (unsigned int)value_caps_n; i++) {
			value_count_by_rid[(unsigned int)value_caps[i].ReportID]++;
		}
	}

	if (button_caps && button_caps_n > 0U) {
		for (i = 0U; i < (unsigned int)button_caps_n; i++) {
			button_count_by_rid[(unsigned int)button_caps[i].ReportID]++;
		}
	}

	n = snprintf(line, sizeof(line), "%s: Feature caps summary:", __func__);
	if (n < 0) {
		return;
	}

	for (i = 0U; i < 256U; i++) {
		unsigned int v = value_count_by_rid[i];
		unsigned int b = button_count_by_rid[i];
		int add;

		if (v == 0U && b == 0U) {
			continue;
		}
		seen = 1U;

		add = snprintfcat(
			line,
			sizeof(line),
			" RID=0x%02x(V=%u,B=%u)",
			i, v, b);
		if (add < 0) {
			return;
		}
		n = add;
	}

	if (!seen) {
		(void)snprintfcat(line, sizeof(line), " none");
	}

	upsdebugx(3, "%s", line);
}

static void winhid_debug_dump_feature_value_caps(
	const win_hidp_value_caps_t *caps,
	size_t caps_n)
{
	size_t i;

	if (!caps || caps_n == 0U || nut_debug_level < 3) {
		return;
	}

	upsdebugx(3, "%s: Feature ValueCaps count=%zu", __func__, caps_n);
	for (i = 0; i < caps_n; i++) {
		const win_hidp_value_caps_t *vc = &caps[i];

		if (vc->IsRange) {
			upsdebugx(3,
				"Feature ValueCaps[%zu]: ReportID=0x%02x BitField=%u DataIndex=%u-%u UsagePage=0x%04x Usage=0x%04x-0x%04x",
				i,
				(unsigned int)vc->ReportID,
				(unsigned int)vc->BitField,
				(unsigned int)vc->u.Range.DataIndexMin,
				(unsigned int)vc->u.Range.DataIndexMax,
				(unsigned int)vc->UsagePage,
				(unsigned int)vc->u.Range.UsageMin,
				(unsigned int)vc->u.Range.UsageMax);
		} else {
			upsdebugx(3,
				"Feature ValueCaps[%zu]: ReportID=0x%02x BitField=%u DataIndex=%u UsagePage=0x%04x Usage=0x%04x",
				i,
				(unsigned int)vc->ReportID,
				(unsigned int)vc->BitField,
				(unsigned int)vc->u.NotRange.DataIndex,
				(unsigned int)vc->UsagePage,
				(unsigned int)vc->u.NotRange.Usage);
		}
	}
}

static void winhid_debug_dump_feature_button_caps(
	const win_hidp_button_caps_t *caps,
	size_t caps_n)
{
	size_t i;

	if (!caps || caps_n == 0U || nut_debug_level < 3) {
		return;
	}

	upsdebugx(3, "%s: Feature ButtonCaps count=%zu", __func__, caps_n);
	for (i = 0; i < caps_n; i++) {
		const win_hidp_button_caps_t *bc = &caps[i];

		if (bc->IsRange) {
			upsdebugx(3,
				"Feature ButtonCaps[%zu]: ReportID=0x%02x BitField=%u DataIndex=%u-%u UsagePage=0x%04x Usage=0x%04x-0x%04x",
				i,
				(unsigned int)bc->ReportID,
				(unsigned int)bc->BitField,
				(unsigned int)bc->u.Range.DataIndexMin,
				(unsigned int)bc->u.Range.DataIndexMax,
				(unsigned int)bc->UsagePage,
				(unsigned int)bc->u.Range.UsageMin,
				(unsigned int)bc->u.Range.UsageMax);
		} else {
			upsdebugx(3,
				"Feature ButtonCaps[%zu]: ReportID=0x%02x BitField=%u DataIndex=%u UsagePage=0x%04x Usage=0x%04x",
				i,
				(unsigned int)bc->ReportID,
				(unsigned int)bc->BitField,
				(unsigned int)bc->u.NotRange.DataIndex,
				(unsigned int)bc->UsagePage,
				(unsigned int)bc->u.NotRange.Usage);
		}
	}
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

static uint8_t winhid_link_collection_type(const win_hidp_link_collection_node_t *node)
{
	if (!node) {
		return WINHID_COLLECTION_PHYSICAL;
	}
	return (uint8_t)(node->Flags & 0xFFU);
}

static int winhid_emit_link_collection_chain(
	winhid_descbuf_t *db,
	const win_hidp_link_collection_node_t *nodes,
	size_t nodes_n,
	USHORT link_collection,
	USHORT link_usage_page,
	USHORT link_usage,
	size_t *opened_count)
{
	size_t opened = 0;

	if (opened_count) {
		*opened_count = 0;
	}

	if (!db) {
		return 0;
	}

	/* Prefer explicit parent/child topology from link collection nodes.
	 * This retains nested collection hierarchy better than flat caps fields. */
	if (nodes && nodes_n > 0U && link_collection > 0U && (size_t)link_collection < nodes_n) {
		USHORT chain[64];
		size_t chain_len = 0;
		USHORT idx = link_collection;
		size_t guard;

		for (guard = 0; guard < (sizeof(chain) / sizeof(chain[0])); guard++) {
			const win_hidp_link_collection_node_t *node;
			USHORT parent;

			if (idx == 0U || (size_t)idx >= nodes_n) {
				break;
			}

			chain[chain_len++] = idx;
			node = &nodes[idx];
			parent = node->Parent;

			if (parent == idx || (size_t)parent >= nodes_n) {
				break;
			}

			idx = parent;
		}

		while (chain_len > 0U) {
			const win_hidp_link_collection_node_t *node;
			uint8_t ctype;

			node = &nodes[chain[--chain_len]];
			ctype = winhid_link_collection_type(node);

			if (node->LinkUsagePage == 0U && node->LinkUsage == 0U) {
				continue;
			}

			if (!winhid_desc_emit_usage_page(db, node->LinkUsagePage)
			 || !winhid_desc_emit_usage(db, node->LinkUsage)
			 || !winhid_desc_emit_collection(db, ctype)) {
				return 0;
			}

			opened++;
		}

		if (opened > 0U) {
			if (opened_count) {
				*opened_count = opened;
			}
			return 1;
		}
	}

	/* Fallback when no usable node topology is available. */
	if (link_usage_page != 0U && link_usage != 0U) {
		if (!winhid_desc_emit_usage_page(db, link_usage_page)
		 || !winhid_desc_emit_usage(db, link_usage)
		 || !winhid_desc_emit_collection(db, WINHID_COLLECTION_PHYSICAL)) {
			return 0;
		}
		opened = 1U;
	}

	if (opened_count) {
		*opened_count = opened;
	}

	return 1;
}

static int winhid_emit_one_value_cap(
	winhid_descbuf_t *db,
	const win_hidp_value_caps_t *vc,
	const win_hidp_link_collection_node_t *link_nodes,
	size_t link_nodes_n,
	unsigned int report_type,
	unsigned int *last_report_id)
{
	size_t opened_link = 0;
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

	if (!winhid_emit_link_collection_chain(
		db,
		link_nodes,
		link_nodes_n,
		vc->LinkCollection,
		vc->LinkUsagePage,
		vc->LinkUsage,
		&opened_link)) {
		return 0;
	}

	if (!winhid_desc_emit_usage_page(db, vc->UsagePage)) {
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

	while (opened_link > 0U) {
		if (!winhid_desc_emit_end_collection(db)) {
			return 0;
		}
		opened_link--;
	}

	return 1;
}

static int winhid_emit_one_button_cap(
	winhid_descbuf_t *db,
	const win_hidp_button_caps_t *bc,
	const win_hidp_link_collection_node_t *link_nodes,
	size_t link_nodes_n,
	unsigned int report_type,
	unsigned int *last_report_id)
{
	size_t opened_link = 0;
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

	if (!winhid_emit_link_collection_chain(
		db,
		link_nodes,
		link_nodes_n,
		bc->LinkCollection,
		bc->LinkUsagePage,
		bc->LinkUsage,
		&opened_link)) {
		return 0;
	}

	if (!winhid_desc_emit_usage_page(db, bc->UsagePage)) {
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
	 || !winhid_desc_emit_s(db, WINHID_TYPE_GLOBAL, WINHID_GLOBAL_PHYSICAL_MIN, 0)
	 || !winhid_desc_emit_s(db, WINHID_TYPE_GLOBAL, WINHID_GLOBAL_PHYSICAL_MAX, 0)
	 || !winhid_desc_emit_s(db, WINHID_TYPE_GLOBAL, WINHID_GLOBAL_UNIT_EXP, 0)
	 || !winhid_desc_emit_u(db, WINHID_TYPE_GLOBAL, WINHID_GLOBAL_UNIT, 0)
	 || !winhid_desc_emit_u(db, WINHID_TYPE_GLOBAL, WINHID_GLOBAL_REPORT_SIZE, 1)
	 || !winhid_desc_emit_u(db, WINHID_TYPE_GLOBAL, WINHID_GLOBAL_REPORT_COUNT, count)
	 || !winhid_desc_emit_main_data(db, report_type, 0x02)) {
		return 0;
	}

	while (opened_link > 0U) {
		if (!winhid_desc_emit_end_collection(db)) {
			return 0;
		}
		opened_link--;
	}

	return 1;
}

typedef struct winhid_ordered_cap_s {
	unsigned int kind;
	size_t index;
	UCHAR report_id;
	USHORT data_index;
	USHORT bit_field;
} winhid_ordered_cap_t;

#define WINHID_ORDERED_CAP_VALUE  1U
#define WINHID_ORDERED_CAP_BUTTON 2U

static USHORT winhid_value_cap_data_index(const win_hidp_value_caps_t *vc)
{
	if (!vc) {
		return 0U;
	}
	if (vc->IsRange) {
		return vc->u.Range.DataIndexMin;
	}
	return vc->u.NotRange.DataIndex;
}

static USHORT winhid_button_cap_data_index(const win_hidp_button_caps_t *bc)
{
	if (!bc) {
		return 0U;
	}
	if (bc->IsRange) {
		return bc->u.Range.DataIndexMin;
	}
	return bc->u.NotRange.DataIndex;
}

static int winhid_value_caps_signature_match(
	const win_hidp_value_caps_t *a,
	const win_hidp_value_caps_t *b)
{
	if (!a || !b) {
		return 0;
	}

	return a->IsRange == b->IsRange
		&& a->BitField == b->BitField
		&& a->LinkCollection == b->LinkCollection
		&& a->LinkUsage == b->LinkUsage
		&& a->LinkUsagePage == b->LinkUsagePage
		&& a->UsagePage == b->UsagePage
		&& a->BitSize == b->BitSize
		&& a->ReportCount == b->ReportCount
		&& a->UnitsExp == b->UnitsExp
		&& a->Units == b->Units
		&& a->LogicalMin == b->LogicalMin
		&& a->LogicalMax == b->LogicalMax
		&& a->PhysicalMin == b->PhysicalMin
		&& a->PhysicalMax == b->PhysicalMax;
}

static int winhid_button_caps_signature_match(
	const win_hidp_button_caps_t *a,
	const win_hidp_button_caps_t *b)
{
	if (!a || !b) {
		return 0;
	}

	return a->IsRange == b->IsRange
		&& a->BitField == b->BitField
		&& a->LinkCollection == b->LinkCollection
		&& a->LinkUsage == b->LinkUsage
		&& a->LinkUsagePage == b->LinkUsagePage
		&& a->UsagePage == b->UsagePage;
}

static int winhid_ordered_caps_match_run_signature(
	const winhid_ordered_cap_t *left,
	const winhid_ordered_cap_t *right,
	const win_hidp_value_caps_t *vals,
	const win_hidp_button_caps_t *btns)
{
	if (!left || !right || !vals || !btns) {
		return 0;
	}

	if (left->kind != right->kind || left->report_id != right->report_id) {
		return 0;
	}

	/* Only consider adjacent DataIndex controls for run reversal. */
	if ((unsigned int)right->data_index != (unsigned int)left->data_index + 1U) {
		return 0;
	}

	if (left->kind == WINHID_ORDERED_CAP_VALUE) {
		const win_hidp_value_caps_t *a = &vals[left->index];
		const win_hidp_value_caps_t *b = &vals[right->index];

		/* Range caps should preserve emitted order. */
		if (a->IsRange || b->IsRange) {
			return 0;
		}

		return winhid_value_caps_signature_match(a, b);
	}

	if (left->kind == WINHID_ORDERED_CAP_BUTTON) {
		const win_hidp_button_caps_t *a = &btns[left->index];
		const win_hidp_button_caps_t *b = &btns[right->index];

		/* Range caps should preserve emitted order. */
		if (a->IsRange || b->IsRange) {
			return 0;
		}

		return winhid_button_caps_signature_match(a, b);
	}

	return 0;
}

static void winhid_reverse_ordered_caps_range(
	winhid_ordered_cap_t *ordered,
	size_t start,
	size_t end)
{
	while (start < end) {
		winhid_ordered_cap_t tmp = ordered[start];
		ordered[start] = ordered[end];
		ordered[end] = tmp;
		start++;
		end--;
	}
}

static size_t winhid_reorder_multicontrol_runs(
	winhid_ordered_cap_t *ordered,
	size_t total,
	const win_hidp_value_caps_t *vals,
	const win_hidp_button_caps_t *btns)
{
	size_t i = 0U;
	size_t reversed_runs = 0U;

	if (!ordered || total < 2U || !vals || !btns) {
		return 0U;
	}

	while (i < total) {
		size_t j = i + 1U;

		while (j < total
			&& winhid_ordered_caps_match_run_signature(&ordered[j - 1U], &ordered[j], vals, btns)) {
			j++;
		}

		if (j > i + 1U) {
			winhid_reverse_ordered_caps_range(ordered, i, j - 1U);
			reversed_runs++;
		}

		i = j;
	}

	return reversed_runs;
}

static uint32_t winhid_value_cap_bit_count(const win_hidp_value_caps_t *vc)
{
	uint32_t bit_size;
	uint32_t count;

	if (!vc) {
		return 0U;
	}

	bit_size = vc->BitSize ? (uint32_t)vc->BitSize : 1U;
	count = vc->ReportCount ? (uint32_t)vc->ReportCount : 1U;
	return bit_size * count;
}

static uint32_t winhid_button_cap_count(const win_hidp_button_caps_t *bc)
{
	if (!bc) {
		return 0U;
	}

	if (bc->IsRange && bc->u.Range.UsageMax >= bc->u.Range.UsageMin) {
		return (uint32_t)(bc->u.Range.UsageMax - bc->u.Range.UsageMin + 1U);
	}

	return 1U;
}

static int winhid_emit_const_padding_bits(
	winhid_descbuf_t *db,
	unsigned int report_type,
	uint32_t bits)
{
	if (!db) {
		return 0;
	}
	if (bits == 0U) {
		return 1;
	}
	if (!winhid_desc_emit_u(db, WINHID_TYPE_GLOBAL, WINHID_GLOBAL_REPORT_SIZE, 1U)
	 || !winhid_desc_emit_u(db, WINHID_TYPE_GLOBAL, WINHID_GLOBAL_REPORT_COUNT, bits)
	 || !winhid_desc_emit_main_data(db, report_type, 0x03)) {
		return 0;
	}

	return 1;
}

static int winhid_cmp_ordered_caps(const void *pa, const void *pb)
{
	const winhid_ordered_cap_t *a = (const winhid_ordered_cap_t *)pa;
	const winhid_ordered_cap_t *b = (const winhid_ordered_cap_t *)pb;

	if (a->report_id < b->report_id) {
		return -1;
	}
	if (a->report_id > b->report_id) {
		return 1;
	}
	if (a->data_index < b->data_index) {
		return -1;
	}
	if (a->data_index > b->data_index) {
		return 1;
	}
	if (a->bit_field < b->bit_field) {
		return -1;
	}
	if (a->bit_field > b->bit_field) {
		return 1;
	}
	if (a->kind < b->kind) {
		return -1;
	}
	if (a->kind > b->kind) {
		return 1;
	}
	if (a->index < b->index) {
		return -1;
	}
	if (a->index > b->index) {
		return 1;
	}

	return 0;
}

static int winhid_emit_ordered_caps(
	winhid_descbuf_t *db,
	const win_hidp_value_caps_t *vals, size_t vals_n,
	const win_hidp_button_caps_t *btns, size_t btns_n,
	const win_hidp_link_collection_node_t *link_nodes, size_t link_nodes_n,
	unsigned int report_type)
{
	size_t total;
	winhid_ordered_cap_t *ordered;
	size_t i;
	size_t n = 0U;
	size_t reversed_runs;
	unsigned int last_report_id = UINT_MAX;
	UCHAR current_report_id = 0xFFU;
	uint32_t current_report_bitpos = 0U;
	unsigned int prev_kind = 0U;
	int prev_was_single_button = 0;
	USHORT prev_data_index = 0U;
	int prev_data_index_valid = 0;

	if (!db) {
		return 0;
	}

	total = vals_n + btns_n;
	if (total == 0U) {
		return 1;
	}

	ordered = (winhid_ordered_cap_t *)calloc(total, sizeof(*ordered));
	if (!ordered) {
		return 0;
	}

	for (i = 0U; i < vals_n; i++) {
		ordered[n].kind = WINHID_ORDERED_CAP_VALUE;
		ordered[n].index = i;
		ordered[n].report_id = vals[i].ReportID;
		ordered[n].data_index = winhid_value_cap_data_index(&vals[i]);
		ordered[n].bit_field = vals[i].BitField;
		n++;
	}
	for (i = 0U; i < btns_n; i++) {
		ordered[n].kind = WINHID_ORDERED_CAP_BUTTON;
		ordered[n].index = i;
		ordered[n].report_id = btns[i].ReportID;
		ordered[n].data_index = winhid_button_cap_data_index(&btns[i]);
		ordered[n].bit_field = btns[i].BitField;
		n++;
	}

	qsort(ordered, total, sizeof(*ordered), winhid_cmp_ordered_caps);
	reversed_runs = winhid_reorder_multicontrol_runs(ordered, total, vals, btns);
	if (nut_debug_level >= 3 && reversed_runs > 0U) {
		upsdebugx(3, "%s: reordered %zu contiguous cap run(s) to preserve usage declaration order",
			__func__, reversed_runs);
	}

	for (i = 0U; i < total; i++) {
		if (ordered[i].report_id != current_report_id) {
			if (current_report_id != 0xFFU && nut_debug_level >= 3 && report_type == WINHID_REPORT_FEATURE) {
				upsdebugx(3,
					"%s: synthesized Feature RID=0x%02x payload_bits=%u payload_bytes=%u",
					__func__,
					(unsigned int)current_report_id,
					(unsigned int)current_report_bitpos,
					(unsigned int)((current_report_bitpos + 7U) >> 3));
			}
			current_report_id = ordered[i].report_id;
			current_report_bitpos = 0U;
			prev_kind = 0U;
			prev_was_single_button = 0;
			prev_data_index_valid = 0;
		}

		if (report_type == WINHID_REPORT_FEATURE
		 && prev_kind == WINHID_ORDERED_CAP_BUTTON
		 && prev_was_single_button
		 && prev_data_index_valid
		 && (unsigned int)ordered[i].data_index > (unsigned int)prev_data_index + 1U
		 && ordered[i].kind == WINHID_ORDERED_CAP_VALUE) {
			const win_hidp_value_caps_t *vc = &vals[ordered[i].index];
			uint32_t bit_size = vc->BitSize ? (uint32_t)vc->BitSize : 1U;
			uint32_t misalign = current_report_bitpos & 7U;

			/* Heuristic: some UPS descriptors place a 7-bit const pad after one 1-bit flag.
			 * HidP caps do not expose that const field directly, but DataIndex usually skips. */
			if (!vc->IsRange && bit_size >= 8U && misalign == 1U) {
				uint32_t pad_bits = 8U - misalign;

				if (!winhid_emit_const_padding_bits(db, report_type, pad_bits)) {
					free(ordered);
					return 0;
				}
				current_report_bitpos += pad_bits;
				if (nut_debug_level >= 3) {
					upsdebugx(3,
						"%s: inserted %u bit constant padding before ReportID=0x%02x DataIndex=%u",
						__func__,
						(unsigned int)pad_bits,
						(unsigned int)ordered[i].report_id,
						(unsigned int)ordered[i].data_index);
				}
			}
		}

		if (ordered[i].kind == WINHID_ORDERED_CAP_VALUE) {
			const win_hidp_value_caps_t *vc = &vals[ordered[i].index];

			if (!winhid_emit_one_value_cap(
				db,
				vc,
				link_nodes, link_nodes_n,
				report_type,
				&last_report_id)) {
				free(ordered);
				return 0;
			}

			current_report_bitpos += winhid_value_cap_bit_count(vc);
			prev_kind = WINHID_ORDERED_CAP_VALUE;
			prev_was_single_button = 0;
			prev_data_index = ordered[i].data_index;
			prev_data_index_valid = 1;
		} else {
			const win_hidp_button_caps_t *bc = &btns[ordered[i].index];
			uint32_t bcount = winhid_button_cap_count(bc);

			if (!winhid_emit_one_button_cap(
				db,
				bc,
				link_nodes, link_nodes_n,
				report_type,
				&last_report_id)) {
				free(ordered);
				return 0;
			}

			current_report_bitpos += bcount;
			prev_kind = WINHID_ORDERED_CAP_BUTTON;
			prev_was_single_button = (!bc->IsRange && bcount == 1U);
			prev_data_index = ordered[i].data_index;
			prev_data_index_valid = 1;
		}
	}

	if (current_report_id != 0xFFU && nut_debug_level >= 3 && report_type == WINHID_REPORT_FEATURE) {
		upsdebugx(3,
			"%s: synthesized Feature RID=0x%02x payload_bits=%u payload_bytes=%u",
			__func__,
			(unsigned int)current_report_id,
			(unsigned int)current_report_bitpos,
			(unsigned int)((current_report_bitpos + 7U) >> 3));
	}

	free(ordered);
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
	const win_hidp_link_collection_node_t *link_nodes, size_t link_nodes_n,
	usb_ctrl_charbuf out_buf,
	size_t out_buf_size,
	usb_ctrl_charbufsize *out_len)
{
	winhid_descbuf_t db;

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

	if (!winhid_emit_ordered_caps(
		&db,
		in_vals, in_vals_n,
		in_btns, in_btns_n,
		link_nodes, link_nodes_n,
		WINHID_REPORT_INPUT)) {
		free(db.data);
		return 0;
	}

	if (!winhid_emit_ordered_caps(
		&db,
		out_vals, out_vals_n,
		out_btns, out_btns_n,
		link_nodes, link_nodes_n,
		WINHID_REPORT_OUTPUT)) {
		free(db.data);
		return 0;
	}

	if (!winhid_emit_ordered_caps(
		&db,
		feat_vals, feat_vals_n,
		feat_btns, feat_btns_n,
		link_nodes, link_nodes_n,
		WINHID_REPORT_FEATURE)) {
		free(db.data);
		return 0;
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
	usb_ctrl_charbuf rdbuf,
	size_t rdbuf_size,
	usb_ctrl_charbufsize *rdlen)
{
	win_hidp_caps_t caps;
	win_phidp_preparsed_data_t preparsed;
	win_hidp_value_caps_t *in_vals = NULL, *out_vals = NULL, *feat_vals = NULL;
	win_hidp_button_caps_t *in_btns = NULL, *out_btns = NULL, *feat_btns = NULL;
	win_hidp_link_collection_node_t *link_nodes = NULL;
	size_t in_vals_n = 0, out_vals_n = 0, feat_vals_n = 0;
	size_t in_btns_n = 0, out_btns_n = 0, feat_btns_n = 0;
	size_t link_nodes_n = 0;
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
	upsdebugx(3, "%s: HidP report lengths Input=%u Output=%u Feature=%u",
		__func__,
		(unsigned int)caps.InputReportByteLength,
		(unsigned int)caps.OutputReportByteLength,
		(unsigned int)caps.FeatureReportByteLength);

	in_vals_n = winhid_get_value_caps(preparsed, WINHID_REPORT_INPUT, caps.NumberInputValueCaps, &in_vals);
	out_vals_n = winhid_get_value_caps(preparsed, WINHID_REPORT_OUTPUT, caps.NumberOutputValueCaps, &out_vals);
	feat_vals_n = winhid_get_value_caps(preparsed, WINHID_REPORT_FEATURE, caps.NumberFeatureValueCaps, &feat_vals);

	in_btns_n = winhid_get_button_caps(preparsed, WINHID_REPORT_INPUT, caps.NumberInputButtonCaps, &in_btns);
	out_btns_n = winhid_get_button_caps(preparsed, WINHID_REPORT_OUTPUT, caps.NumberOutputButtonCaps, &out_btns);
	feat_btns_n = winhid_get_button_caps(preparsed, WINHID_REPORT_FEATURE, caps.NumberFeatureButtonCaps, &feat_btns);
	link_nodes_n = winhid_get_link_collection_nodes(preparsed, caps.NumberLinkCollectionNodes, &link_nodes);
	upsdebugx(3, "%s: collected %zu/%u link collection nodes",
		__func__, link_nodes_n, (unsigned int)caps.NumberLinkCollectionNodes);
	winhid_debug_dump_feature_caps_summary(feat_vals, feat_vals_n, feat_btns, feat_btns_n);
	winhid_debug_dump_feature_value_caps(feat_vals, feat_vals_n);
	winhid_debug_dump_feature_button_caps(feat_btns, feat_btns_n);

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
			link_nodes, link_nodes_n,
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
	free(link_nodes);

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
		winhid_fill_device_strings(handle, curDevice);

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
			usb_ctrl_char rdbuf[WINHID_MAX_REPORT_SIZE];
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
	upsdebugx(3, "%s: ReportID=0x%02x ReportSize=%u query_size=%zu feature_len=%zu",
		__func__,
		(unsigned int)(ReportId & 0xffU),
		(unsigned int)ReportSize,
		query_size,
		ctx->feature_report_len);

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
	if (query_size > (size_t)ReportSize) {
		upsdebugx(3,
			"%s: truncating ReportID=0x%02x from %zu to %u bytes to match synthesized descriptor",
			__func__,
			(unsigned int)(ReportId & 0xffU),
			query_size,
			(unsigned int)ReportSize);
	}
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
	winhid_dev_ctx_t *ctx;
	WCHAR wbuf[512];
	char *tmp;
	DWORD err;
	int ret;

	if (!sdev || !buf || buflen < 1) {
		return -1;
	}
	buf[0] = '\0';

	if (StringIdx < 1) {
		return -1;
	}

	ctx = (winhid_dev_ctx_t *)sdev;
	if (!ctx->handle || ctx->handle == INVALID_HANDLE_VALUE) {
		return LIBUSB_ERROR_NO_DEVICE;
	}
	if (!g_winhid_api.HidD_GetIndexedString) {
		return 0;
	}

	memset(wbuf, 0, sizeof(wbuf));
	if (!g_winhid_api.HidD_GetIndexedString(
		ctx->handle,
		(ULONG)StringIdx,
		wbuf,
		(ULONG)sizeof(wbuf))) {
		err = GetLastError();
		if (err == ERROR_INVALID_FUNCTION) {
			return 0;
		}
		return winhid_map_winerr_to_libusb(err);
	}
	wbuf[(sizeof(wbuf) / sizeof(wbuf[0])) - 1U] = L'\0';

	tmp = winhid_wstr_to_ascii(wbuf);
	if (!tmp) {
		return LIBUSB_ERROR_NO_MEM;
	}
	ret = winhid_copy_ascii_to_buf(tmp, buf, buflen);
	free(tmp);
	return ret;
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

/* ---------------------------------------------------------------------- */
/* Report descriptor canonicalization                                     */
/*                                                                        */
/* After Parse_ReportDesc() produces a HIDDesc_t from the synthetic       */
/* descriptor, these fixups align the parsed paths with what existing NUT  */
/* subdriver mapping tables expect.  Moved here from usbhid-ups.c so that */
/* all winhid-specific logic is contained in the backend module.          */
/* ---------------------------------------------------------------------- */

/*!
 * @brief Map 0x84xx (Power Device page) status leaf usages to canonical
 *        0x85xx (Battery System page) equivalents.
 *
 * The Windows HidP caps expose status booleans under the Power Device
 * usage page, while NUT subdriver tables expect the Battery System page.
 */
static int winhid_canon_presentstatus_leaf(HIDNode_t *leaf)
{
	if (!leaf) {
		return 0;
	}

	switch (*leaf) {
	case USAGE_POW_CONFIG_FREQUENCY:
		*leaf = USAGE_BAT_BELOW_REMAINING_CAPACITY_LIMIT;
		return 1;
	case USAGE_POW_CONFIG_APPARENT_POWER:
		*leaf = USAGE_BAT_REMAINING_TIME_LIMIT_EXPIRED;
		return 1;
	case USAGE_POW_CONFIG_ACTIVE_POWER:
		*leaf = USAGE_BAT_CHARGING;
		return 1;
	case USAGE_POW_CONFIG_PERCENT_LOAD:
		*leaf = USAGE_BAT_DISCHARGING;
		return 1;
	case USAGE_POW_CONFIG_TEMPERATURE:
		*leaf = USAGE_BAT_FULLY_CHARGED;
		return 1;
	case USAGE_POW_CONFIG_HUMIDITY:
		*leaf = USAGE_BAT_FULLY_DISCHARGED;
		return 1;
	case 0x0084004BU:
		*leaf = USAGE_BAT_NEED_REPLACEMENT;
		return 1;
	case 0x008400D0U:
		*leaf = USAGE_BAT_AC_PRESENT;
		return 1;
	case 0x008400D1U:
		*leaf = USAGE_BAT_BATTERY_PRESENT;
		return 1;
	default:
		return 0;
	}
}

/*!
 * @brief Map collection ID nodes (e.g. BatterySystemID) to their base
 *        collection form (e.g. BatterySystem).
 */
static HIDNode_t winhid_canon_collection_id_node(HIDNode_t node)
{
	switch (node) {
	case USAGE_POW_BATTERY_SYSTEM_ID:
		return USAGE_POW_BATTERY_SYSTEM;
	case USAGE_POW_BATTERY_ID:
		return USAGE_POW_BATTERY;
	case USAGE_POW_CHARGER_ID:
		return USAGE_POW_CHARGER;
	case USAGE_POW_POWER_CONVERTER_ID:
		return USAGE_POW_POWER_CONVERTER;
	case USAGE_POW_OUTLET_SYSTEM_ID:
		return USAGE_POW_OUTLET_SYSTEM;
	case USAGE_POW_INPUT_ID:
		return USAGE_POW_INPUT;
	case USAGE_POW_OUTPUT_ID:
		return USAGE_POW_OUTPUT;
	case USAGE_POW_FLOW_ID:
		return USAGE_POW_FLOW;
	case USAGE_POW_OUTLET_ID:
		return USAGE_POW_OUTLET;
	case USAGE_POW_GANG_ID:
		return USAGE_POW_GANG;
	case USAGE_POW_POWER_SUMMARY_ID:
		return USAGE_POW_POWER_SUMMARY;
	default:
		return node;
	}
}

/*! @brief Compare two HIDPath_t structs for exact equality. */
static int winhid_path_equals(const HIDPath_t *a, const HIDPath_t *b)
{
	if (!a || !b) {
		return 0;
	}
	if (a->Size != b->Size) {
		return 0;
	}
	if (a->Size == 0) {
		return 1;
	}
	return memcmp(a->Node, b->Node, (size_t)a->Size * sizeof(a->Node[0])) == 0;
}

/*!
 * @brief Append an alias HIDData item to the descriptor if no duplicate exists.
 *
 * @return 1 on success, 0 if duplicate found (skip), -1 on allocation failure.
 */
static int winhid_append_alias_item(
	HIDDesc_t *desc,
	const HIDData_t *src_item,
	const HIDPath_t *alias_path)
{
	size_t j;
	HIDData_t *nitems;

	if (!desc || !desc->item || !src_item || !alias_path) {
		return 0;
	}

	for (j = 0; j < desc->nitems; j++) {
		const HIDData_t *it = &desc->item[j];
		if (it->ReportID != src_item->ReportID
		 || it->Offset != src_item->Offset
		 || it->Size != src_item->Size
		 || it->Type != src_item->Type) {
			continue;
		}

		if (winhid_path_equals(&it->Path, alias_path)) {
			return 0;
		}
	}

	nitems = realloc(desc->item, (desc->nitems + 1U) * sizeof(*desc->item));
	if (!nitems) {
		upsdebugx(1, "%s: realloc() failed while appending winhid alias item", __func__);
		return -1;
	}
	desc->item = nitems;
	desc->item[desc->nitems] = *src_item;
	desc->item[desc->nitems].Path = *alias_path;
	desc->nitems++;

	return 1;
}

/*!
 * @brief Build an aliased path by rewriting collection ID nodes and
 *        optionally flattening intermediate collection hierarchy.
 *
 * @return Non-zero if the alias differs from the source path.
 */
static int winhid_build_path_aliases(
	const HIDPath_t *src_path,
	HIDPath_t *alias_path,
	int *id_nodes_rewritten,
	int *flattened)
{
	uint8_t p;
	int changed = 0;

	if (!src_path || !alias_path) {
		return 0;
	}

	*alias_path = *src_path;

	for (p = 0; p < alias_path->Size; p++) {
		HIDNode_t node = alias_path->Node[p];
		HIDNode_t canon = winhid_canon_collection_id_node(node);
		if (canon != node) {
			alias_path->Node[p] = canon;
			changed = 1;
			if (id_nodes_rewritten) {
				(*id_nodes_rewritten)++;
			}
		}
	}

	/* Some devices expose "UPS.PowerConverter.Input.*" (or similar nested
	 * forms) while current subdriver tables often expect "UPS.Input.*" and
	 * siblings.  Keep original path, but add a flattened alias branch. */
	if (alias_path->Size >= 3
	 && alias_path->Node[0] == USAGE_POW_UPS
	 && (alias_path->Node[1] == USAGE_POW_POWER_CONVERTER
	  || alias_path->Node[1] == USAGE_POW_BATTERY_SYSTEM)
	 && (alias_path->Node[2] == USAGE_POW_INPUT
	  || alias_path->Node[2] == USAGE_POW_OUTPUT
	  || alias_path->Node[2] == USAGE_POW_BATTERY)) {
		for (p = 1; (uint8_t)(p + 1) < alias_path->Size; p++) {
			alias_path->Node[p] = alias_path->Node[p + 1];
		}
		alias_path->Size--;
		changed = 1;
		if (flattened) {
			(*flattened)++;
		}
	}

	return changed;
}

/*!
 * @brief Canonicalize a parsed HID report descriptor for winhid compatibility.
 *
 * See the documentation in libwinhid.h for the full description of what
 * this function does and why it is needed.
 */
int winhid_canonicalize_parsed_report_desc(HIDDesc_t *desc)
{
	size_t i;
	size_t original_nitems;
	int changed = 0;
	int inserted = 0;
	int repaged = 0;
	int skipped_insert = 0;
	int alias_added = 0;
	int alias_id_nodes_rewritten = 0;
	int alias_flattened = 0;
	int alias_realloc_fail = 0;
	int alias_dup_skipped = 0;

	if (!desc || !desc->item) {
		return 0;
	}

	/* Alias generation below appends new items; iterate only over originals. */
	original_nitems = desc->nitems;
	for (i = 0; i < original_nitems; i++) {
		HIDPath_t *path = &desc->item[i].Path;
		uint8_t p = 0;
		int has_ups = 0;
		int ps_idx = -1;

		if (!path || path->Size < 2) {
			continue;
		}

		for (p = 0; p < path->Size; p++) {
			if (path->Node[p] == USAGE_POW_UPS) {
				has_ups = 1;
			}
			if (ps_idx < 0 && path->Node[p] == USAGE_POW_PRESENT_STATUS) {
				ps_idx = (int)p;
			}
		}

		if (ps_idx >= 0) {
			/* Expected by APC-like mappings:
			 * UPS.PowerSummary.PresentStatus.* */
			if (has_ups
			 && ps_idx > 0
			 && path->Node[ps_idx - 1] != USAGE_POW_POWER_SUMMARY
			) {
				if (path->Size >= PATH_SIZE) {
					skipped_insert++;
				} else {
					for (p = path->Size; p > (uint8_t)ps_idx; p--) {
						path->Node[p] = path->Node[p - 1];
					}
					path->Node[ps_idx] = USAGE_POW_POWER_SUMMARY;
					path->Size++;
					ps_idx++;
					inserted++;
					changed++;
				}
			}

			/* Map known status leaves from 0x84xx aliases
			 * to canonical 0x85xx. */
			if (winhid_canon_presentstatus_leaf(&path->Node[path->Size - 1])) {
				repaged++;
				changed++;
			}
		}

		{
			HIDPath_t alias_path;
			int alias_changed;
			int addres;

			alias_changed = winhid_build_path_aliases(
				path,
				&alias_path,
				&alias_id_nodes_rewritten,
				&alias_flattened);

			if (alias_changed && !winhid_path_equals(path, &alias_path)) {
				addres = winhid_append_alias_item(desc, &desc->item[i], &alias_path);
				if (addres > 0) {
					alias_added++;
					changed++;
				} else if (addres == 0) {
					alias_dup_skipped++;
				} else {
					alias_realloc_fail++;
				}
			}
		}
	}

	if (changed || skipped_insert || alias_dup_skipped || alias_realloc_fail) {
		upsdebugx(2,
			"%s: winhid parser fixups changed=%d inserted=%d repaged=%d "
			"alias_added=%d alias_id_nodes=%d alias_flattened=%d "
			"skipped_insert=%d alias_dups=%d alias_oom=%d",
			__func__, changed, inserted, repaged,
			alias_added, alias_id_nodes_rewritten, alias_flattened,
			skipped_insert, alias_dup_skipped, alias_realloc_fail);
	}

	return changed;
}

/* ---------------------------------------------------------------------- */
/* Subdriver interface registration                                       */
/* ---------------------------------------------------------------------- */

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
