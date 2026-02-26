/* placeholder/mock method implementations to just directly use driver
 * source code as done for belkin-hid.c (and eventually similar code)
 * for almost-in-vivo testing (and minimal intrusion to that codebase).
 * See also: getexponenttest-belkin-hid.c
 *
 * Copyright (C)
 *      2024   Jim Klimov <jimklimov+nut@gmail.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "common.h"

#include "usb-common.h"
int is_usb_device_supported(usb_device_id_t *usb_device_id_list, USBDevice_t *device) {
	NUT_UNUSED_VARIABLE(usb_device_id_list);
	NUT_UNUSED_VARIABLE(device);
	return -1;
}

#include "usbhid-ups.h"
hid_dev_handle_t udev = HID_DEV_HANDLE_CLOSED;
info_lkp_t beeper_info[] = { { 0, NULL, NULL, NULL } };
info_lkp_t date_conversion[] = { { 0, NULL, NULL, NULL } };
info_lkp_t stringid_conversion[] = { { 0, NULL, NULL, NULL } };
info_lkp_t divide_by_10_conversion[] = { { 0, NULL, NULL, NULL } };
info_lkp_t divide_by_100_conversion[] = { { 0, NULL, NULL, NULL } };

void possibly_supported(const char *mfr, HIDDevice_t *hd) {
	NUT_UNUSED_VARIABLE(mfr);
	NUT_UNUSED_VARIABLE(hd);
}

int fix_report_desc(HIDDevice_t *arg_pDev, HIDDesc_t *arg_pDesc) {
	NUT_UNUSED_VARIABLE(arg_pDev);
	NUT_UNUSED_VARIABLE(arg_pDesc);
	return -1;
}

#include "libhid.h"
usage_lkp_t hid_usage_lkp[] = { {NULL, 0} };
char *HIDGetItemString(hid_dev_handle_t arg_udev, const char *hidpath, char *buf, size_t buflen, usage_tables_t *utab) {
	NUT_UNUSED_VARIABLE(arg_udev);
	NUT_UNUSED_VARIABLE(hidpath);
	NUT_UNUSED_VARIABLE(buf);
	NUT_UNUSED_VARIABLE(buflen);
	NUT_UNUSED_VARIABLE(utab);
	return NULL;
}

#include "main.h"
char *getval(const char *var) {
	NUT_UNUSED_VARIABLE(var);
	return NULL;
}
