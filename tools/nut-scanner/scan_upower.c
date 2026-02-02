/*
 *  Copyright (C) 2026 Tim Niemueller <tim@niemueller.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/*! \file scan_upower.c
    \brief detect NUT supported devices via UPower (GDBus/GIO)
    \author Tim Niemueller <tim@niemueller.de>
*/

#include "common.h"
#include "nut-scan.h"

/* externally visible to nutscan-init */
int nutscan_unload_upower_library(void);
/* externally visible to nut-scanner */
nutscan_device_t * nutscan_scan_upower(void);

#if (defined WITH_UPOWER) && WITH_UPOWER

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <gio/gio.h>
#include <ltdl.h>

/* dynamic link library stuff */
static lt_dlhandle dl_handle = NULL;
static const char *dl_error = NULL;
static char *dl_saved_libname = NULL;

/* Function pointers */
static GDBusConnection * (*nut_g_bus_get_sync)(GBusType bus_type, GCancellable *cancellable, GError **error);
static GDBusProxy * (*nut_g_dbus_proxy_new_sync)(GDBusConnection *connection, GDBusProxyFlags flags, GDBusInterfaceInfo *info, const gchar *name, const gchar *object_path, const gchar *interface_name, GCancellable *cancellable, GError **error);
static GVariant * (*nut_g_dbus_proxy_call_sync)(GDBusProxy *proxy, const gchar *method_name, GVariant *parameters, GDBusCallFlags flags, gint timeout_msec, GCancellable *cancellable, GError **error);
static GVariant * (*nut_g_dbus_proxy_get_cached_property)(GDBusProxy *proxy, const gchar *property_name);
static void (*nut_g_variant_get)(GVariant *value, const gchar *format_string, ...);
static void (*nut_g_variant_unref)(GVariant *value);
static void (*nut_g_object_unref)(gpointer object);
static void (*nut_g_error_free)(GError *error);
static const gchar * (*nut_g_variant_get_string)(GVariant *value, gsize *length);
static gboolean (*nut_g_variant_iter_next)(GVariantIter *iter, const gchar *format_string, ...);
static gsize (*nut_g_variant_iter_init)(GVariantIter *iter, GVariant *value);
static guint32 (*nut_g_variant_get_uint32)(GVariant *value);
static GVariant * (*nut_g_variant_get_child_value)(GVariant *value, gsize index_);

/* Return 0 on success, -1 on error e.g. "was not loaded";
 * other values may be possible if lt_dlclose() errors set them;
 * visible externally */
int nutscan_unload_library(int *avail, lt_dlhandle *pdl_handle, char **libpath);
int nutscan_unload_upower_library(void)
{
	return nutscan_unload_library(&nutscan_avail_upower, &dl_handle, &dl_saved_libname);
}

/* Return 0 on error; visible externally */
int nutscan_load_upower_library(const char *libname_path);
int nutscan_load_upower_library(const char *libname_path)
{
	if (dl_handle != NULL) {
		/* if previous init failed */
		if (dl_handle == (lt_dlhandle)1) {
			return 0;
		}
		/* init has already been done */
		return 1;
	}

	if (libname_path == NULL) {
		upsdebugx(0, "GIO library not found. UPower search disabled.");
		return 0;
	}

	if (lt_dlinit() != 0) {
		upsdebugx(0, "%s: Error initializing lt_dlinit", __func__);
		return 0;
	}

	dl_handle = lt_dlopen(libname_path);
	if (!dl_handle) {
		dl_error = lt_dlerror();
		goto err;
	}

	/* Clear any existing error */
	lt_dlerror();

	*(void **) (&nut_g_bus_get_sync) = lt_dlsym(dl_handle, "g_bus_get_sync");
	if ((dl_error = lt_dlerror()) != NULL) goto err;

	*(void **) (&nut_g_dbus_proxy_new_sync) = lt_dlsym(dl_handle, "g_dbus_proxy_new_sync");
	if ((dl_error = lt_dlerror()) != NULL) goto err;

	*(void **) (&nut_g_dbus_proxy_call_sync) = lt_dlsym(dl_handle, "g_dbus_proxy_call_sync");
	if ((dl_error = lt_dlerror()) != NULL) goto err;

	*(void **) (&nut_g_dbus_proxy_get_cached_property) = lt_dlsym(dl_handle, "g_dbus_proxy_get_cached_property");
	if ((dl_error = lt_dlerror()) != NULL) goto err;

	*(void **) (&nut_g_variant_get) = lt_dlsym(dl_handle, "g_variant_get");
	if ((dl_error = lt_dlerror()) != NULL) goto err;

	*(void **) (&nut_g_variant_unref) = lt_dlsym(dl_handle, "g_variant_unref");
	if ((dl_error = lt_dlerror()) != NULL) goto err;

	*(void **) (&nut_g_object_unref) = lt_dlsym(dl_handle, "g_object_unref");
	if ((dl_error = lt_dlerror()) != NULL) goto err;

	*(void **) (&nut_g_error_free) = lt_dlsym(dl_handle, "g_error_free");
	if ((dl_error = lt_dlerror()) != NULL) goto err;

	*(void **) (&nut_g_variant_get_string) = lt_dlsym(dl_handle, "g_variant_get_string");
	if ((dl_error = lt_dlerror()) != NULL) goto err;

	*(void **) (&nut_g_variant_iter_next) = lt_dlsym(dl_handle, "g_variant_iter_next");
	if ((dl_error = lt_dlerror()) != NULL) goto err;

	*(void **) (&nut_g_variant_iter_init) = lt_dlsym(dl_handle, "g_variant_iter_init");
	if ((dl_error = lt_dlerror()) != NULL) goto err;

	*(void **) (&nut_g_variant_get_uint32) = lt_dlsym(dl_handle, "g_variant_get_uint32");
	if ((dl_error = lt_dlerror()) != NULL) goto err;

	*(void **) (&nut_g_variant_get_child_value) = lt_dlsym(dl_handle, "g_variant_get_child_value");
	if ((dl_error = lt_dlerror()) != NULL) goto err;

	if (dl_saved_libname)
		free(dl_saved_libname);
	dl_saved_libname = xstrdup(libname_path);

	return 1;

err:
	upsdebugx(0,
		"Cannot load GIO library (%s) : %s. UPower search disabled.",
		libname_path, dl_error);
	dl_handle = (lt_dlhandle)1;
	lt_dlexit();
	if (dl_saved_libname) {
		free(dl_saved_libname);
		dl_saved_libname = NULL;
	}
	return 0;
}

nutscan_device_t * nutscan_scan_upower(void)
{
	GError *error = NULL;
	GDBusConnection *connection;
	GDBusProxy *proxy_upower = NULL;
	GVariant *result = NULL;
	GVariant *child = NULL;
	GVariantIter iter;
	gchar *object_path;
	nutscan_device_t * dev_ret = NULL;

	if (!nutscan_avail_upower) {
		return NULL;
	}

	connection = (*nut_g_bus_get_sync)(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (connection == NULL) {
		upsdebugx(1, "Error connecting to system bus: %s", error->message);
		(*nut_g_error_free)(error);
		return NULL;
	}

	proxy_upower = (*nut_g_dbus_proxy_new_sync)(connection,
						G_DBUS_PROXY_FLAGS_NONE,
						NULL,
						"org.freedesktop.UPower",
						"/org/freedesktop/UPower",
						"org.freedesktop.UPower",
						NULL,
						&error);

	if (proxy_upower == NULL) {
		upsdebugx(1, "Error creating UPower proxy: %s", error->message);
		(*nut_g_error_free)(error);
		(*nut_g_object_unref)(connection);
		return NULL;
	}

	result = (*nut_g_dbus_proxy_call_sync)(proxy_upower,
					"EnumerateDevices",
					NULL,
					G_DBUS_CALL_FLAGS_NONE,
					-1,
					NULL,
					&error);

	if (result == NULL) {
		upsdebugx(1, "Error enumerating devices: %s", error->message);
		(*nut_g_error_free)(error);
		(*nut_g_object_unref)(proxy_upower);
		(*nut_g_object_unref)(connection);
		return NULL;
	}

	/* Result is (ao) - array of object paths */
	child = (*nut_g_variant_get_child_value)(result, 0);
	(*nut_g_variant_iter_init)(&iter, child);

	while ((*nut_g_variant_iter_next)(&iter, "o", &object_path)) {
		GDBusProxy *proxy_device;
		GVariant *v_type, *v_model, *v_vendor, *v_serial, *v_native_path;
		nutscan_device_t * dev;

		proxy_device = (*nut_g_dbus_proxy_new_sync)(connection,
							G_DBUS_PROXY_FLAGS_NONE,
							NULL,
							"org.freedesktop.UPower",
							object_path,
							"org.freedesktop.UPower.Device",
							NULL,
							&error);

		if (proxy_device == NULL) {
			upsdebugx(1, "Error creating device proxy for %s: %s", object_path, error->message);
			(*nut_g_error_free)(error);
			free(object_path);
			continue;
		}

		/* Check Type property: 2 is Battery, 3 is UPS */
		v_type = (*nut_g_dbus_proxy_get_cached_property)(proxy_device, "Type");
		if (v_type) {
			guint32 type = (*nut_g_variant_get_uint32)(v_type);
			(*nut_g_variant_unref)(v_type);

			if (type == 2 || type == 3) { /* Battery or UPS */
				dev = nutscan_new_device();
				dev->type = TYPE_UPOWER;
				dev->driver = strdup("upower");
				dev->port = strdup(object_path);

				v_vendor = (*nut_g_dbus_proxy_get_cached_property)(proxy_device, "Vendor");
				if (v_vendor) {
					nutscan_add_option_to_device(dev, "vendor", (char*)(*nut_g_variant_get_string)(v_vendor, NULL));
					(*nut_g_variant_unref)(v_vendor);
				}

				v_model = (*nut_g_dbus_proxy_get_cached_property)(proxy_device, "Model");
				if (v_model) {
					nutscan_add_option_to_device(dev, "product", (char*)(*nut_g_variant_get_string)(v_model, NULL));
					(*nut_g_variant_unref)(v_model);
				}

				v_serial = (*nut_g_dbus_proxy_get_cached_property)(proxy_device, "Serial");
				if (v_serial) {
					nutscan_add_option_to_device(dev, "serial", (char*)(*nut_g_variant_get_string)(v_serial, NULL));
					(*nut_g_variant_unref)(v_serial);
				}

				v_native_path = (*nut_g_dbus_proxy_get_cached_property)(proxy_device, "NativePath");
				if (v_native_path) {
					nutscan_add_option_to_device(dev, "native_path", (char*)(*nut_g_variant_get_string)(v_native_path, NULL));
					(*nut_g_variant_unref)(v_native_path);
				}

				nutscan_add_option_to_device(dev, "bus", "upower");

				dev_ret = nutscan_add_device_to_device(dev_ret, dev);
			}
		}

		(*nut_g_object_unref)(proxy_device);
		free(object_path);
	}

	(*nut_g_variant_unref)(child);
	(*nut_g_variant_unref)(result);
	(*nut_g_object_unref)(proxy_upower);
	(*nut_g_object_unref)(connection);

	return nutscan_rewind_device(dev_ret);
}

#else /* not WITH_UPOWER */

nutscan_device_t * nutscan_scan_upower(void)
{
	return NULL;
}

int nutscan_unload_upower_library(void)
{
	return 0;
}

#endif /* WITH_UPOWER */
