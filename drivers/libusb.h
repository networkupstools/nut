/*!
 * @file libusb.h
 * @brief HID Library - Generic USB backend for Generic HID Access (using MGE HIDParser)
 *
 * @author Copyright (C) 2003
 *	Arnaud Quette <arnaud.quette@free.fr> && <arnaud.quette@mgeups.com>
 *	Philippe Marzouk <philm@users.sourceforge.net> (dump_hex())
 *      2005 Peter Selinger <selinger@users.sourceforge.net>
 *
 * This program is sponsored by MGE UPS SYSTEMS - opensource.mgeups.com
 *
 *      The logic of this file is ripped from mge-shut driver (also from
 *      Arnaud Quette), which is a "HID over serial link" UPS driver for
 *      Network UPS Tools <http://www.networkupstools.org/>
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

#ifndef LIBUSB_H
#define LIBUSB_H

#include <usb.h> /* libusb header file */
#include "libhid.h"

/* try to open the next available device matching the given MatchFlags
 * (in case mode=MODE_OPEN), or try to open the exact same device as
 * before (in case mode=MODE_REOPEN). */
int libusb_open(usb_dev_handle **udevp, HIDDevice *curDevice, HIDDeviceMatcher_t *matcher, unsigned char *ReportDesc, int mode);
void libusb_close(usb_dev_handle *udev);

//extern int usb_get_descriptor(int type, int len, char *report);
int libusb_get_report(usb_dev_handle *udev, int ReportId, unsigned char *raw_buf, int ReportSize );
int libusb_set_report(usb_dev_handle *udev, int ReportId, unsigned char *raw_buf, int ReportSize );
int libusb_get_string(usb_dev_handle *udev, int StringIdx, char *string);
int libusb_get_interrupt(usb_dev_handle *udev, unsigned char *buf, int bufsize, int timeout);

#endif /* LIBUSB_H */

