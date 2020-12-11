/* dbus.h - dbus communication functions

   Copyright (C) 2018  Emilien Kia <emilien.kia+dev@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#ifndef DBUS_H_SEEN
#define DBUS_H_SEEN

#include "common.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

#define DBUS_INTERFACE_NUT_DEVICE "org.networkupstools.Device"
#define DBUS_NUT_UPSD_PATH "/org/networkupstools/Upsd"
#define DBUS_NUT_UPSD_NAME "org.networkupstools.Upsd"

typedef struct upstype_s upstype_t;

int dbus_init();
void dbus_cleanup();
void dbus_loop();

void dbus_notify_property_change(upstype_t* ups, const char* name, const char* value);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif	/* UPSD_H_SEEN */
