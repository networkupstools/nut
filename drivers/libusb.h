/*!
 * @file libusb.h
 * @brief HID Library - Generic USB backend for Generic HID Access (using MGE HIDParser)
 *
 * @author Copyright (C)
 *	2003 - 2006 Arnaud Quette <aquette.dev@gmail.com>
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

extern communication_subdriver_t usb_subdriver;

#endif /* LIBUSB_H */

