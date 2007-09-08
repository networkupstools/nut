/* dstate-hal.c - Network UPS Tools driver-side state management
   This is a compatibility interface that encapsulate the HAL bridge
   into the NUT dstate API for NUT drivers

   Copyright (C) 2006-2007  Arnaud Quette <aquette.dev@gmail.com>

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

#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "common.h"
#include "dstate-hal.h"
#include "extstate.h"
/* #include "state.h"
#include "parseconf.h" */

#include <hal/libhal.h>

/* FIXME: export command and RW variables (using the HAL example: addon-cpufreq and macbook addon) */ 
/* beeper.on, beeper.off => SetBeeper(bool)
   beeper.toggle => ToggleBeeper(void)

org.freedesktop.Hal.Device.UPS.SetSounder (bool)


Shutdown() or ShutOff() 
	shutdown.return
	shutdown.stayoff
	shutdown.reboot
	shutdown.reboot.graceful
	
#define UPS_ERROR_GENERAL                   "GeneralError"	
#define UPS_ERROR_UNSUPPORTED_FEATURE       "FeatureNotSupported"	
#define UPS_ERROR_PERMISSION_DENIED         "PermissionDenied"
  
****** implementation *******

#define DBUS_INTERFACE "org.freedesktop.Hal.Device.UPS"

if (!libhal_device_claim_interface(halctx, udi, DBUS_INTERFACE, 

	"    <method name=\"Shutdown\">\n"
	"      <arg name=\"shutdown_type\" direction=\"in\" type=\"s\"/>\n"
	"      <arg name=\"return_code\" direction=\"out\" type=\"i\"/>\n"
	"    </method>\n"

	&dbus_dbus_error)) {
		fprintf(stderr, "Cannot claim interface: %s", dbus_dbus_error.message);
		goto Error;
	}	


*/

/*
 *	static	int	sockfd = -1, stale = 1, alarm_active = 0;
 *	static	struct	st_tree_t	*dtree_root = NULL;
 *	static	struct	conn_t	*connhead = NULL;
 *	static	struct	cmdlist_t *cmdhead = NULL;
 *	static	char	*sockfn = NULL;
 *	static	char	status_buf[ST_MAX_VALUE_LEN], 
 *			alarm_buf[ST_MAX_VALUE_LEN];
 */
 
struct	ups_handler	upsh;

LibHalContext *halctx = NULL;
char *udi;

int ac_present = 0; /* 0 = false ; 1 = true */
static void* runtime_handler(LibHalChangeSet *cs, char* runtime);

/* Structure to lookup between NUT and HAL */
typedef struct {
	char	*nut_name;				/* NUT variable name */
	char	*hal_name;				/* HAL variable name */
	int		hal_type;				/* HAL variable type */
	void    *(*fun)(LibHalChangeSet *cs, 
					char *value);	/* conversion function. */
} info_lkp_t;

enum hal_type_t
{
	NONE = 0,
	HAL_TYPE_INT,
	HAL_TYPE_BOOL,
	HAL_TYPE_STRING
};

/* Data to lookup between NUT and HAL for dstate_setinfo() */
static info_lkp_t nut2hal[] =
{
	/* ups.status is handled by status_set() calls */
	{ "battery.charge.low", "battery.charge_level.low", HAL_TYPE_INT, NULL },
	{ "battery.charge.low", "battery.reporting.low", HAL_TYPE_INT, NULL },
	{ "battery.charge", "battery.charge_level.current", HAL_TYPE_INT, NULL },
	{ "battery.charge", "battery.charge_level.percentage", HAL_TYPE_INT, NULL },
	{ "battery.charge", "battery.reporting.current", HAL_TYPE_INT, NULL },
	{ "battery.charge", "battery.reporting.percentage", HAL_TYPE_INT, NULL },
	{ "battery.runtime", "battery.remaining_time", HAL_TYPE_INT, *runtime_handler },
	{ "battery.type", "battery.technology", HAL_TYPE_STRING, NULL },
	{ "battery.type", "battery.reporting.technology", HAL_TYPE_STRING, NULL },
	
	/* AQ note: Not sure it fits!	*/
	/* HAL marked as mandatory! */
	{ "battery.voltage", "battery.voltage.current", HAL_TYPE_INT, NULL },
	{ "battery.voltage.nominal", "battery.voltage.design", HAL_TYPE_INT, NULL },

	{ "ups.mfr", "battery.vendor", HAL_TYPE_STRING, NULL },
	{ "ups.model", "battery.model", HAL_TYPE_STRING, NULL },
	{ "ups.serial", "battery.serial", HAL_TYPE_STRING, NULL },

	/* Terminating element */
	{ NULL, NULL, NONE, NULL }
};

/* Functions to lookup between NUT and HAL */
static info_lkp_t *find_nut_info(const char *nut_varname, info_lkp_t *prev_info_item);

/* HAL accessors wrappers */
void hal_set_string(LibHalChangeSet *cs, const char *key, const char *value);
void hal_set_int(LibHalChangeSet *cs, const char *key, const int value);
void hal_set_bool(LibHalChangeSet *cs, const char *key, const dbus_bool_t value);

/* Handle runtime exposition according to the AC status */
static void* runtime_handler(LibHalChangeSet *cs, char* runtime)
{
	if (ac_present == 0) {
		/* unSet the runtime auto computation and rely upon NUT.battery.runtime*/
		hal_set_bool (cs, "battery.remaining_time.calculate_per_time", FALSE);

		/* Set the runtime value */
		hal_set_int (cs, "battery.remaining_time", atoi(runtime));
	}
	else {
		/* Set the runtime auto computation */
		hal_set_bool (cs, "battery.remaining_time.calculate_per_time", TRUE);

		/* Set the runtime value */
		hal_set_int (cs, "battery.remaining_time", 0);
	}
	return NULL; /* Nothing to return */
}


/********************************************************************
 * dstate compatibility interface
 *******************************************************************/
void dstate_init(const char *prog, const char *port)
{
	DBusError dbus_error;
	LibHalChangeSet *cs;

	dbus_error_init (&dbus_error);

	cs = libhal_device_new_changeset (udi);
	if (cs == NULL) {
		fatalx (EXIT_FAILURE, "Cannot initialize changeset");
	}

	/* UPS always report charge as percent */
	hal_set_string (cs, "battery.charge_level.unit", "percent");
	hal_set_string (cs, "battery.reporting.unit", "percent");

	/* Various UPSs assumptions */
	/****************************/
	/* UPS are always rechargeable! */
	/* FIXME: Check for NUT extension however: ie HID->UPS.PowerSummary.Rechargeable
	 * into battery.rechargeable
	 * or always expose it?
	 */
	hal_set_bool (cs, "battery.is_rechargeable", TRUE);

	/* UPS always has a max battery charge of 100 % */
	hal_set_int (cs, "battery.charge_level.design", 100);
	hal_set_int (cs, "battery.charge_level.last_full", 100);
	hal_set_int (cs, "battery.reporting.design", 100);
	hal_set_int (cs, "battery.reporting.last_full", 100);

	/* NUT always express Voltage in Volts "V" */
	/* But not all UPSs provide the data
	 * battery.voltage.{design,current} */	
	hal_set_string (cs, "battery.voltage.unit", "V");

	/* UPS always have a battery! */
	/* Note(AQU): wrong with some solar panel usage, where the UPS */
	/* is just an energy source switch! */
	/* FIXME: to be processed (need possible NUT extension) */
	hal_set_bool (cs, "battery.present", TRUE);

	/* FIXME: can be improved?! (implies "info.recall.vendor") */
	hal_set_bool (cs, "info.is_recalled", FALSE);

	/* Set generic properties */
	hal_set_string (cs, "battery.type", "ups");
	libhal_device_add_capability (halctx, udi, "battery", &dbus_error);
	libhal_device_add_capability (halctx, udi, "ac_adaptor", &dbus_error);
	
	/* FIXME: what's that? (from addon-hidups) */
	/* UPS_DEVICENAME
	libhal_changeset_set_property_string (
	cs, "foo", ups_get_string (fd, uref.value)); */

	dbus_error_init (&dbus_error);
	/* NOTE: commit_changeset won't do IPC if set is empty */
	libhal_device_commit_changeset (halctx, cs, &dbus_error);
	libhal_device_free_changeset (cs);
	
	if (dbus_error_is_set (&dbus_error))
		dbus_error_free (&dbus_error);
}


const char *dstate_getinfo(const char *var)
{
	/* FIXME: use revert lookup from nut2hal_info or
	from raw NUT info? */
	return NULL;
}

int dstate_setinfo(const char *var, const char *fmt, ...)
{
	va_list	ap;
	int	ret = 1;
	LibHalChangeSet *cs;
	DBusError dbus_error;
	char	value[ST_MAX_VALUE_LEN];
	info_lkp_t *nut2hal_info = NULL, *prev_nut2hal_info = NULL;

	va_start(ap, fmt);
	vsnprintf(value, sizeof(value), fmt, ap);
	va_end(ap);

	cs = libhal_device_new_changeset (udi);
	if (cs == NULL) {
		fatalx (EXIT_FAILURE, "Cannot initialize changeset");
	}

	/* Loop on getting HAL variable(s) matching this NUT variable */
	while ( (nut2hal_info = find_nut_info(var, prev_nut2hal_info)) != NULL)
	{
		upsdebugx(2, "dstate_setinfo: %s => %s (%s)\n", var, nut2hal_info->hal_name, value);

		if (nut2hal_info->fun != NULL)
			nut2hal_info->fun(cs, value);
		else {
			switch (nut2hal_info->hal_type)
			{
				case HAL_TYPE_INT:
					hal_set_int(cs, nut2hal_info->hal_name, atoi(value));
					break;
				case HAL_TYPE_BOOL:
					/* FIXME: howto lookup TRUE/FALSE? */
					hal_set_bool(cs, nut2hal_info->hal_name, TRUE);
					break;
				case HAL_TYPE_STRING:
					hal_set_string(cs, nut2hal_info->hal_name, value);
					break;
			}
		}
		prev_nut2hal_info = nut2hal_info;
	}

	dbus_error_init (&dbus_error);
	/* NOTE: commit_changeset won't do IPC if set is empty */
	libhal_device_commit_changeset(halctx,cs,&dbus_error);
	libhal_device_free_changeset (cs);

	if (dbus_error_is_set (&dbus_error))
		dbus_error_free (&dbus_error);

	return ret;
}

int dstate_delinfo(const char *var)
{
	return 0;
}

int dstate_delcmd(const char *var)
{
	return 0;
}

void dstate_setflags(const char *var, int flags)
{
	return;
}

void dstate_setaux(const char *var, int aux)
{
	return;
}

void dstate_dataok(void)
{
	return;
}

void dstate_datastale(void)
{
	return;
}

void dstate_free(void)
{
	return;
}

/* extrafd: provided for waking up based on the driver's UPS fd */
int dstate_poll_fds(int interval, int extrafd)
{
	/* FIXME: can it be enhanced? */
	sleep (interval);
	return 0;
}	

/* clean out the temp space for a new pass */
void status_init(void)
{
	/* Nothing to do */
	return;
}

/* ups.status element conversion */
void status_set(const char *buf)
{
	upsdebugx(2, "status_set: %s", buf);

	/* Note: only usbhid-ups supported devices expose [DIS]CHRG status */
	/* along with the standard OL (online) / OB (on battery) status! */
	if ( (strcmp(buf, "DISCHRG") == 0) || (strcmp(buf, "OB") == 0) )
	{
		ac_present = 0;
	}
	else if ( (strcmp(buf, "CHRG") == 0) || (strcmp(buf, "OL") == 0) )
	{
		ac_present = 1;
	}
	else 
		upsdebugx(2, "status_set: dropping status %s (not managed)\n", buf);

	return;
}

/* write the status_buf into the externally visible dstate storage */
void status_commit(void)
{
	LibHalChangeSet *cs;
	DBusError dbus_error;

	upsdebugx(2, "status_commit");

	cs = libhal_device_new_changeset (udi);
	if (cs == NULL) {
		fatalx (EXIT_FAILURE, "Cannot initialize changeset");
	}

	/* Set AC present status */
	/* Note: UPSs are also AC adaptors! */
	hal_set_bool (cs, "ac_adaptor.present", (ac_present == 0)?FALSE:TRUE);

	/* Set discharging status */
	hal_set_bool (cs, "battery.rechargeable.is_discharging", (ac_present == 0)?TRUE:FALSE);

	/* Set charging status */
	hal_set_bool (cs, "battery.rechargeable.is_charging", (ac_present == 0)?FALSE:TRUE);

	dbus_error_init (&dbus_error);
	/* NOTE: commit_changeset won't do IPC if set is empty */
	libhal_device_commit_changeset (halctx, cs, &dbus_error);
	libhal_device_free_changeset (cs);
	
	if (dbus_error_is_set (&dbus_error))
		dbus_error_free (&dbus_error);

	return;
}

/* similar functions for ups.alarm */
void alarm_init(void)
{
	return;
}

void alarm_set(const char *buf)
{
	return;
}

void alarm_commit(void)
{
	return;
}

/* Register DBus methods */
void dstate_addcmd(const char *cmdname)
{
	DBusError dbus_error;
	dbus_error_init (&dbus_error);

	upsdebugx(2, "dstate_addcmd: %s\n", cmdname);

	/* beeper.{on,off} */
/*	if (!strncasecmp(cmdname, "beeper.o", 8))
	{
		if (!libhal_device_claim_interface(halctx, udi, DBUS_INTERFACE,
			"    <method name=\"SetBeeper\">\n"
			"      <arg name=\"beeper_mode\" direction=\"in\" type=\"b\"/>\n"
			"      <arg name=\"return_code\" direction=\"out\" type=\"i\"/>\n"
			"    </method>\n",
			&dbus_error)) {
				fprintf(stderr, "Error: can't claim DBus interface: %s", dbus_error.message);
				exit(EXIT_FAILURE);
		}
	}

	dbus_connection_setup_with_g_main(dbus_connection, NULL);
	dbus_connection_add_filter(dbus_connection, dbus_filter_function, NULL, NULL);
	dbus_connection_set_exit_on_disconnect(dbus_connection, 0);
*/
/*
	if (!libhal_device_claim_interface(halctx, udi, DBUS_INTERFACE,
		"    <method name=\"Shutdown\">\n"
		"      <arg name=\"shutdown_type\" direction=\"in\" type=\"s\"/>\n"
		"      <arg name=\"return_code\" direction=\"out\" type=\"i\"/>\n"
		"    </method>\n",
		&dbus_error)) {
			fprintf(stderr, "Error: can't claim DBus interface: %s", dbus_error.message);
			exit(EXIT_FAILURE);
	}
*/
	
/*	if (dbus_error_is_set (&dbus_error))
		dbus_error_free (&dbus_error); */
}

/********************************************************************
 * DBus interface functions and data
 *******************************************************************/


static DBusHandlerResult dbus_filter_function_local(DBusConnection *connection,
						    DBusMessage *message,
						    void *user_data)
{
	if (dbus_message_is_signal(message, DBUS_INTERFACE_LOCAL, "Disconnected")) {
		upsdebugx(1, "DBus daemon disconnected. Trying to reconnect...");
		dbus_connection_unref(connection);
		g_timeout_add(5000, (GSourceFunc)dbus_init_local, NULL);
	}
	return DBUS_HANDLER_RESULT_HANDLED;
}

/* returns FALSE on success because it's used as a callback */
gboolean dbus_init_local(void)
{
	DBusConnection	*dbus_connection;
	DBusError	dbus_error;

	dbus_error_init(&dbus_error);

	dbus_connection = dbus_bus_get(DBUS_BUS_SYSTEM, &dbus_error);
	if (dbus_error_is_set(&dbus_error)) {
		upsdebugx(1, "Cannot get D-Bus connection");
/*		dbus_error_free (&dbus_error); */
		return TRUE;
	}

	dbus_connection_setup_with_g_main(dbus_connection, NULL);
	dbus_connection_add_filter(dbus_connection, dbus_filter_function_local,
				   NULL, NULL);
	dbus_connection_set_exit_on_disconnect(dbus_connection, 0);
	return FALSE;
}


/** 
 * dbus_filter_function:
 * @connection:		connection to D-Bus
 * @message:		message
 * @user_data:  pointer to the data
 *
 * Returns: 		the result
 * 
 * @raises UnknownMethod
 *
 * D-Bus filter function
 */
DBusHandlerResult dbus_filter_function(DBusConnection *connection,
					      DBusMessage *message,
					      void *user_data)
{
	DBusError dbus_error;
	DBusConnection	*dbus_connection;
	const char	*member		= dbus_message_get_member(message);
	const char	*path		= dbus_message_get_path(message);

	upsdebugx(2, "Received DBus message with\n\tmember %s\n\tpath %s", member, path);
exit(0);

	dbus_error_init(&dbus_error);
	if (dbus_error_is_set (&dbus_error))
   	{
		fprintf (stderr, "an error occurred: %s\n", dbus_error.message);
/*		dbus_error_free (&dbus_error); */
	}
	else
	{
#ifdef HAVE_POLKIT
		if (!dbus_is_privileged(connection, message, &dbus_error))
			return DBUS_HANDLER_RESULT_HANDLED;
#endif

		if (dbus_message_is_method_call(message, DBUS_INTERFACE,
						"Shutdown")) {
			char *arg;
/*
			if (!dbus_get_argument(connection, message, &dbus_error,
					       DBUS_TYPE_STRING, &arg)) {
				return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
			}
	 		HAL_DEBUG(("Received argument: %s", arg));
				
			if (set_governors(connection, message, arg))
				dbus_send_reply(connection, message, DBUS_TYPE_INVALID, NULL);

		}
*/
		} else {
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}
	}
	return DBUS_HANDLER_RESULT_HANDLED;
}

	
/*******************************************************************
 * internal functions
 *******************************************************************/

/****************
 * HAL wrappers *
 ****************/
/* Only update HAL string values if there are real changes */
void hal_set_string(LibHalChangeSet *cs, const char *key, const char *value)
{
	DBusError dbus_error;
	char *new_value = NULL;

	upsdebugx(2, "hal_set_string: %s => %s", key, value);

	dbus_error_init(&dbus_error);

	/* Check if the property already exists */
	if (libhal_device_property_exists (halctx, udi, key, &dbus_error) == TRUE) {
		
		new_value = libhal_device_get_property_string (halctx, udi,
				key, &dbus_error);

		/* Check if the value has really changed */
		if (strcmp(value, new_value))
			libhal_changeset_set_property_string (cs, key, value);
			
		/* Free the new_value string */
		if (new_value != NULL)
			libhal_free_string (new_value);
	}
	else {
		libhal_changeset_set_property_string (cs, key, value);
	}	
}

/* Only update HAL int values if there are real changes */
void hal_set_int(LibHalChangeSet *cs, const char *key, const int value)
{
	DBusError dbus_error;
	int new_value;

	upsdebugx(2, "hal_set_int: %s => %i", key, value);

	dbus_error_init(&dbus_error);

	/* Check if the property already exists */
	if (libhal_device_property_exists (halctx, udi, key, &dbus_error) == TRUE) {
		
		new_value = libhal_device_get_property_int (halctx, udi,
				key, &dbus_error);

		/* Check if the value has really changed */
		if (value != new_value)
			libhal_changeset_set_property_int (cs, key, value);
	}
	else {
		libhal_changeset_set_property_int (cs, key, value);
	}	
}

/* Only update HAL int values if there are real changes */
void hal_set_bool(LibHalChangeSet *cs, const char *key, const dbus_bool_t value)
{
	DBusError dbus_error;
	dbus_bool_t new_value;

	upsdebugx(2, "hal_set_bool: %s => %s", key, (value==TRUE)?"true":"false");
	
	dbus_error_init(&dbus_error);

	/* Check if the property already exists */
	if (libhal_device_property_exists (halctx, udi, key, &dbus_error) == TRUE) {
		
		new_value = libhal_device_get_property_bool (halctx, udi,
				key, &dbus_error);

		/* Check if the value has really changed */
		if (value != new_value)
			libhal_changeset_set_property_bool (cs, key, value);
	}
	else {
		libhal_changeset_set_property_bool (cs, key, value);
	}	
}

/* find the next info element definition in info array
 * that matches nut_varname, and that is after prev_info_item
 * if specified.
 * Note that 1 nut item can matches several HAL items
 */
static info_lkp_t *find_nut_info(const char *nut_varname, info_lkp_t *prev_info_item)
{
	info_lkp_t *info_item;

	upsdebugx(2, "find_nut_info: looking up => %s\n", nut_varname);
	
	if (prev_info_item != NULL) {
		/* Start from the item following prev_info_item */
		info_item = ++prev_info_item;
	}
	else {
		info_item = nut2hal;
	}

	for ( ; info_item != NULL && info_item->nut_name != NULL ; info_item++) {

		if (!strcasecmp(info_item->nut_name, nut_varname))
			return info_item;
	}

	return NULL;
}
