/* drivers/nut-upower.c - Driver for UPower via D-Bus
 *
 * Copyright (C) 2026 Tim Niemueller <tim@niemueller.de>
 *
 * Links against standard NUT driver core.
 * Requires: glib-2.0, gio-2.0
 */

#include "main.h"
#include "dstate.h"

#include <gio/gio.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>

#define DRIVER_NAME	"UPower D-Bus Driver"
#define DRIVER_VERSION	"0.01"

/* UPower Constants */
#define UPOWER_BUS	"org.freedesktop.UPower"
#define UPOWER_PATH	"/org/freedesktop/UPower"
#define UPOWER_IFACE	"org.freedesktop.UPower"
#define DEVICE_IFACE	"org.freedesktop.UPower.Device"
#define PROPS_IFACE	"org.freedesktop.DBus.Properties"

/* Global DBus Connection Objects */
static GDBusConnection *connection = NULL;
static char *ups_object_path = NULL;
static double lowbatt_pct = 20.0;

/* NUT Driver Info Structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Tim Niemueller <tim@niemueller.de>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

/* -------------------------------------------------------------------------- */
/* Map UPower State to NUT Status                                             */
/* -------------------------------------------------------------------------- */
static void set_nut_status(guint state, gdouble percentage)
{
	/* Clear previous status */
	status_init();

	/* UPower States:
	 * 1: Charging
	 * 2: Discharging
	 * 3: Empty
	 * 4: Fully Charged
	 * 5: Pending Charge
	 * 6: Pending Discharge
	 */

	switch (state) {
		case 1: /* Charging */
			status_set("OL CHRG");
			break;
		case 2: /* Discharging */
			status_set("OB DISCHRG");
			break;
		case 4: /* Full */
			status_set("OL");
			break;
		case 3: /* Empty */
			status_set("OB LB");
			break;
		default: /* Unknown / Pending */
			status_set("OL"); /* Default to Online to prevent shutdowns */
			break;
	}

	/* Override for Low Battery based on percentage */
	if (percentage < lowbatt_pct) {
		status_set("LB");
	}

	status_commit();
}

/* -------------------------------------------------------------------------- */
/* Find the First UPS Device (auto) or a specific one (device_path not auto)  */
/* -------------------------------------------------------------------------- */
static int find_ups_device(void)
{
	GError *error = NULL;
	GVariant *result;
	GVariantIter iter;
	gchar *obj_path;
	int found = 0;

	/* Call EnumerateDevices on the main UPower object */
	result = g_dbus_connection_call_sync(
		connection,
		UPOWER_BUS, UPOWER_PATH, UPOWER_IFACE,
		"EnumerateDevices",
		NULL, /* No params */
		G_VARIANT_TYPE("(ao)"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		&error
	);

	if (error) {
		fatalx(EXIT_FAILURE, "DBus Error enumerating devices: %s", error->message);
	}

	/* Iterate the result array of object paths */
	g_variant_iter_init(&iter, g_variant_get_child_value(result, 0));

	while (g_variant_iter_loop(&iter, "o", &obj_path)) {
		upsdebugx(2, "Enumerated UPower device: %s", obj_path);

		/* Check if we are looking for a specific path */
		if (device_path && strcmp(device_path, "auto") != 0 && *device_path != '\0') {
			if (strstr(obj_path, device_path)) {
				upsdebugx(1, "Match found for specific path '%s': %s", device_path, obj_path);
				ups_object_path = strdup(obj_path);
				found = 1;
				break;
			}
		} else {
			/* Auto-detection: Check for substring "ups_" */
			if (strstr(obj_path, "ups_")) {
				upsdebugx(1, "Auto-detected UPS device: %s", obj_path);
				ups_object_path = strdup(obj_path);
				found = 1;
				break;
			}
		}
	}

	g_variant_unref(result);
	return found;
}
	
/* -------------------------------------------------------------------------- */
/* List Available UPower Devices                                              */
/* -------------------------------------------------------------------------- */
static void list_upower_devices(void)
{
	GError *error = NULL;
	GDBusConnection *conn;
	GVariant *result;
	GVariantIter iter;
	gchar *obj_path;
	
	/* Initialize GLib type system */
	#if !GLIB_CHECK_VERSION(2, 36, 0)
	g_type_init();
	#endif
	
	conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (!conn) {
		printf("Error: Failed to connect to System Bus: %s\n", error->message);
		g_error_free(error);
		return;
	}
	
	result = g_dbus_connection_call_sync(
		conn,
		UPOWER_BUS, UPOWER_PATH, UPOWER_IFACE,
		"EnumerateDevices",
		NULL,
		G_VARIANT_TYPE("(ao)"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		&error
	);
	
	if (error) {
		printf("Error enumerating devices: %s\n", error->message);
		g_error_free(error);
		g_object_unref(conn);
		return;
	}
	
	printf("\nAvailable UPower devices:\n");
	g_variant_iter_init(&iter, g_variant_get_child_value(result, 0));
	
	while (g_variant_iter_loop(&iter, "o", &obj_path)) {
		printf("  %s\n", obj_path);
	}

	g_variant_unref(result);
	g_object_unref(conn);
}
	
/* -------------------------------------------------------------------------- */
/* NUT Hook: Help / Usage                                                     */
/* -------------------------------------------------------------------------- */
void upsdrv_help(void)
{
	list_upower_devices();
}
/* -------------------------------------------------------------------------- */
/* NUT Hook: Initialize Command Line Args                                     */
/* -------------------------------------------------------------------------- */
void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "lowbatt", "Low battery threshold, in percent (default: 20)");
}

/* -------------------------------------------------------------------------- */
/* NUT Hook: Initialize UPS Connection                                        */
/* -------------------------------------------------------------------------- */
void upsdrv_initups(void)
{
	GError *error = NULL;
	const char *val;

	/* Initialize GLib type system */
	#if !GLIB_CHECK_VERSION(2, 36, 0)
	g_type_init();
	#endif

	/* Connect to System Bus */
	connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
	if (!connection) {
		fatalx(EXIT_FAILURE, "Failed to connect to System Bus: %s", error->message);
	}

	/* Find the UPS */
	if (!find_ups_device()) {
		fatalx(EXIT_FAILURE, "No UPower device found containing 'ups_' in path.");
	}

	upslogx(LOG_INFO, "Connected to UPower Device: %s", ups_object_path);
	dstate_setinfo("driver.parameter.port", "%s", ups_object_path);

	/* Handle low battery threshold */
	if ((val = getval("lowbatt"))) {
		lowbatt_pct = atof(val);
		upsdebugx(1, "Low battery threshold set to: %.1f%%", lowbatt_pct);
	}
}

/* -------------------------------------------------------------------------- */
/* NUT Hook: Update Info (The Main Loop)                                      */
/* -------------------------------------------------------------------------- */
void upsdrv_updateinfo(void)
{
	GError *error = NULL;
	GVariant *result, *props;
	GVariantIter iter;
	gchar *key;
	GVariant *value;
	gdouble voltage = 0.0, percentage = 0.0;
	gint64 time_empty = 0;
	guint state = 0;
	const gchar *model = "Unknown";
	const gchar *vendor = "Unknown";
	const gchar *serial = "Unknown";

	/* Fetch All Properties */
	result = g_dbus_connection_call_sync(
		connection,
		UPOWER_BUS, ups_object_path, PROPS_IFACE,
		"GetAll",
		g_variant_new("(s)", DEVICE_IFACE),
		G_VARIANT_TYPE("(a{sv})"),
		G_DBUS_CALL_FLAGS_NONE,
		-1,
		NULL,
		&error
	);

	if (error) {
		dstate_datastale();
		upslogx(LOG_WARNING, "Failed to get properties: %s", error->message);
		g_error_free(error);
		return;
	}

	/* Parse Dictionary */
	props = g_variant_get_child_value(result, 0);
	g_variant_iter_init(&iter, props);

	while (g_variant_iter_loop(&iter, "{sv}", &key, &value)) {
		if (g_strcmp0(key, "Voltage") == 0) {
			voltage = g_variant_get_double(value);
		} else if (g_strcmp0(key, "Percentage") == 0) {
			percentage = g_variant_get_double(value);
		} else if (g_strcmp0(key, "State") == 0) {
			state = g_variant_get_uint32(value);
		} else if (g_strcmp0(key, "TimeToEmpty") == 0) {
			time_empty = g_variant_get_int64(value);
		} else if (g_strcmp0(key, "Model") == 0) {
			model = g_variant_get_string(value, NULL);
		} else if (g_strcmp0(key, "Vendor") == 0) {
			vendor = g_variant_get_string(value, NULL);
		} else if (g_strcmp0(key, "Serial") == 0) {
			serial = g_variant_get_string(value, NULL);
		}
	}

	/* Update NUT Data Store (dstate) */
	dstate_setinfo("battery.charge", "%.1f", percentage);
	dstate_setinfo("battery.voltage", "%.1f", voltage);

	if (time_empty > 0) {
		dstate_setinfo("battery.runtime", "%lld", (long long)time_empty);
	}

	dstate_setinfo("device.mfr", "%s", vendor);
	dstate_setinfo("device.model", "%s", model);
	dstate_setinfo("device.serial", "%s", serial);

	/* Update Status Flags */
	set_nut_status(state, percentage);

	/* Commit data */
	dstate_dataok();

	g_variant_unref(props);
	g_variant_unref(result);
}

/* -------------------------------------------------------------------------- */
/* NUT Hook: Tweak Program Names                                              */
/* -------------------------------------------------------------------------- */
void upsdrv_tweak_prognames(void)
{
}

/* -------------------------------------------------------------------------- */
/* NUT Hook: Initialize Info                                                  */
/* -------------------------------------------------------------------------- */
void upsdrv_initinfo(void)
{
	upsdrv_updateinfo();
}

/* -------------------------------------------------------------------------- */
/* NUT Hook: Shutdown                                                         */
/* -------------------------------------------------------------------------- */
void upsdrv_shutdown(void)
{
	upslogx(LOG_ERR, "shutdown not supported");
}

/* -------------------------------------------------------------------------- */
/* NUT Hook: Cleanup                                                          */
/* -------------------------------------------------------------------------- */
void upsdrv_cleanup(void)
{
	if (ups_object_path) free(ups_object_path);
	if (connection) g_object_unref(connection);
}
