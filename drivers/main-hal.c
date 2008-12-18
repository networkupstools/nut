/* main-hal.c - Network UPS Tools driver core for HAL

   Copyright (C) 2006  Arnaud Quette <aquette.dev@gmail.com>
   Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>

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

/* TODO list:
 * -more cleanup
 * - dstate-hal: expose all data on org.freedesktop.NUT to prepare
 * a full HAL/DBus enabled nut (remove the need of {d,s}state layer
 * - use HAL logging functions
 */

#include "main-hal.h"
#include "dstate-hal.h"
#include <hal/libhal.h>

#ifdef HAVE_POLKIT
#include <libpolkit.h>
#endif

	/* HAL specific */
	extern LibHalContext *halctx;
	extern char *udi;

	/* data which may be useful to the drivers */	
	int	upsfd = -1;
	char	*device_path = NULL;
	const	char	*progname = NULL, *upsname = NULL, 
			*device_name = NULL;

	/* may be set by the driver to wake up while in dstate_poll_fds */
	int	extrafd = -1;

	/* for ser_open */
	int	do_lock_port = 1;

	/* set by the drivers */
	int	experimental_driver = 0;
	int	broken_driver = 0;

	static vartab_t	*vartab_h = NULL;

	/* variables possibly set by the global part of ups.conf */
	unsigned int	poll_interval = 2;
	static char	*chroot_path = NULL;

	/* signal handling */
	int	exit_flag = 0;

	/* everything else */
	static	char	*pidfn = NULL;
	GMainLoop *gmain;
	char *dbus_methods_introspection;

/* retrieve the value of variable <var> if possible */
char *getval(const char *var)
{
	vartab_t	*tmp = vartab_h;

	while (tmp) {
		if (!strcasecmp(tmp->var, var))
			return(tmp->val);
		tmp = tmp->next;
	}

	return NULL;
}

/* see if <var> has been defined, even if no value has been given to it */
int testvar(const char *var)
{
	vartab_t	*tmp = vartab_h;

	while (tmp) {
		if (!strcasecmp(tmp->var, var))
			return tmp->found;
		tmp = tmp->next;
	}

	return 0;	/* not found */
}

/* callback from driver - create the table for -x/conf entries */
void addvar(int vartype, const char *name, const char *desc)
{
	vartab_t	*tmp, *last;

	tmp = last = vartab_h;

	while (tmp) {
		last = tmp;
		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(vartab_t));

	tmp->vartype = vartype;
	tmp->var = xstrdup(name);
	tmp->val = NULL;
	tmp->desc = xstrdup(desc);
	tmp->found = 0;
	tmp->next = NULL;

	if (last)
		last->next = tmp;
	else
		vartab_h = tmp;
}

static void vartab_free(void)
{
	vartab_t	*tmp, *next;

	tmp = vartab_h;

	while (tmp) {
		next = tmp->next;

		free(tmp->var);
		free(tmp->val);
		free(tmp->desc);
		free(tmp);

		tmp = next;
	}
}

static void exit_cleanup(int sig)
{
	upsdebugx(2, "exit_cleanup(%i", sig);

	exit_flag = sig;

	upsdrv_cleanup();

	free(chroot_path);
	free(device_path);

	if (pidfn) {
		unlink(pidfn);
		free(pidfn);
	}

	dstate_free();
	vartab_free();
	
	/* break the main loop */
	g_main_loop_quit(gmain);
}

static void setup_signals(void)
{
	struct sigaction	sa;

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

	sa.sa_handler = exit_cleanup;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);

/*	sa.sa_handler = SIG_IGN;
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGPIPE, &sa, NULL);*/
}

/* (*GSourceFunc) wrapper */
static gboolean update_data (gpointer data)
{
	upsdrv_updateinfo();
	return (exit_flag == 0);
}

int main(int argc, char **argv)
{
	DBusError dbus_error;
	DBusConnection	*dbus_connection;
	struct	passwd	*new_uid = NULL;
	char *hal_debug_level;
/*	int	i, do_forceshutdown = 0; */

	if (experimental_driver) {
		printf("Warning: This is an experimental driver.\n");
		printf("Some features may not function correctly.\n\n");
	}

	progname = xbasename(argv[0]);
	open_syslog(progname);

	dbus_methods_introspection = xmalloc(1024);

	/* Register the basic supported method (Shutdown)
	 * Leave other methods registration to driver core calls to
	 * dstate_addcmd(), at initinfo() time
	 */
	sprintf(dbus_methods_introspection, "%s",
		"    <method name=\"Shutdown\">\n"
/*		"      <arg name=\"shutdown_type\" direction=\"in\" type=\"s\"/>\n" */
		"      <arg name=\"return_code\" direction=\"out\" type=\"i\"/>\n"
		"    </method>\n");

	/* initialise HAL and DBus interface*/
	halctx = NULL;
	udi = getenv ("UDI");
	if (udi == NULL) {
		fprintf(stderr, "Error: UDI is null.\n");
		exit(EXIT_FAILURE);
	}

	dbus_error_init (&dbus_error);
	if ((halctx = libhal_ctx_init_direct (&dbus_error)) == NULL) {
		fprintf(stderr, "Error: can't initialise libhal.\n");
		exit(EXIT_FAILURE);
	}
	if (dbus_error_is_set (&dbus_error))
	{
		fatalx(EXIT_FAILURE, "Error in context creation: %s\n", dbus_error.message);
		dbus_error_free (&dbus_error);
	}
	
	if ((dbus_connection = libhal_ctx_get_dbus_connection(halctx)) == NULL) {
		fprintf(stderr, "Error: can't get DBus connection.\n");
		exit(EXIT_FAILURE);
	}

	/* FIXME: rework HAL param interface! or get path/regex from UDI
	 * Example:
	 * /org/freedesktop/Hal/devices/usb_device_463_ffff_1H2E300AH
	 * => linux.device_file = /dev/bus/usb/002/065
	 */
	/* FIXME: the naming should be abstracted to os.device_file! */
	device_path = xstrdup("auto");
	
	/* FIXME: bridge debug/warning on HAL equivalent (need them
	 * to externalize these in a lib... */
	hal_debug_level = getenv ("NUT_HAL_DEBUG");
	if (hal_debug_level == NULL)
		nut_debug_level = 0;
	else
		nut_debug_level = atoi(hal_debug_level);

	/* Sleep 2 seconds to be able to view the device through usbfs! */
	sleep(2);

	/* build the driver's extra (-x) variable table */
	upsdrv_makevartable();

	/* Switch to the HAL user */
	new_uid = get_user_pwent(HAL_USER);
	become_user(new_uid);

	/* Only switch to statepath if we're not powering off */
	/* This avoid case where ie /var is umounted */
/*	?! Not needed for HAL !?
	if (!do_forceshutdown)
		if (chdir(dflt_statepath()))
			fatal_with_errno(EXIT_FAILURE, "Can't chdir to %s", dflt_statepath());
*/
	setup_signals();

	/* clear out callback handler data */
	memset(&upsh, '\0', sizeof(upsh));

	upsdrv_initups();

#if 0
	if (do_forceshutdown)
		forceshutdown();
#endif

	/* get the supported data and commands before allowing connections */
 	upsdrv_initinfo();
	upsdrv_updateinfo();

	/* now we can start servicing requests */
	dstate_init(NULL, NULL);

	/* Commit DBus methods */
	if (!libhal_device_claim_interface(halctx, udi, DBUS_INTERFACE, 
		dbus_methods_introspection,	&dbus_error)) {
			fprintf(stderr, "Cannot claim interface: %s\n", dbus_error.message);
	}
	else
		fprintf(stdout, "Claimed the following DBus interfaces on %s:\n%s",
			DBUS_INTERFACE, dbus_methods_introspection);

	/* Complete DBus binding */
	dbus_connection_setup_with_g_main(dbus_connection, NULL);
	dbus_connection_add_filter(dbus_connection, dbus_filter_function, NULL, NULL);
	dbus_connection_set_exit_on_disconnect(dbus_connection, 0);

	dbus_init_local();
	/* end of HAL init */

#if 0
	/* publish the top-level data: version number, driver name */
	dstate_setinfo("driver.version", "%s", UPS_VERSION);
	dstate_setinfo("driver.name", "%s", progname);

	/* The poll_interval may have been changed from the default */
	dstate_setinfo("driver.parameter.pollinterval", "%d", poll_interval);

/* FIXME: needed? */
	if (nut_debug_level == 0) {
		background();
		writepid(pidfn);
	}
#endif
	/* End HAL init */
	dbus_error_init (&dbus_error);
	if (!libhal_device_addon_is_ready (halctx, udi, &dbus_error)) {
		fprintf(stderr, "Error (libhal): device addon is not ready\n");
		exit(EXIT_FAILURE);
	}

	/* add a timer for data update */
#ifdef HAVE_GLIB_2_14
	g_timeout_add_seconds (poll_interval,
#else
	g_timeout_add (1000 * poll_interval,				/* seconds */
#endif
							(GSourceFunc)update_data,
							NULL);

	/* setup and run the main loop */
	gmain = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(gmain);

	/* reached upon addon exit */
	upslogx(LOG_INFO, "Signal %d: exiting", exit_flag);
	exit(EXIT_SUCCESS);
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

#ifdef HAVE_POLKIT
/** 
 * dbus_is_privileged:
 * @connection:		connection to D-Bus
 * @message:		Message
 * @error:		the error
 *
 * Returns: 		TRUE if the caller is privileged
 *
 * checks if caller of message possesses the CPUFREQ_POLKIT_PRIVILGE 
 */
static gboolean 
dbus_is_privileged (DBusConnection *connection, DBusMessage *message, DBusError *error)
{
        gboolean ret;
        char *polkit_result;
        const char *invoked_by_syscon_name;

        ret = FALSE;
        polkit_result = NULL;
/* FIXME: CPUFREQ_POLKIT_PRIVILEGE, CPUFREQ_ERROR_GENERAL */
        invoked_by_syscon_name = dbus_message_get_sender (message);
        
        polkit_result = libhal_device_is_caller_privileged (halctx,
                                                            udi,
                                                            CPUFREQ_POLKIT_PRIVILEGE,
                                                            invoked_by_syscon_name,
                                                            error);
        if (polkit_result == NULL) {
			dbus_raise_error (connection, message, CPUFREQ_ERROR_GENERAL,
                                  "Cannot determine if caller is privileged");
        }
        else {
			if (strcmp (polkit_result, "yes") != 0) {

				dbus_raise_error (connection, message, 
									  "org.freedesktop.Hal.Device.PermissionDeniedByPolicy",
									  "%s %s <-- (action, result)",
									  CPUFREQ_POLKIT_PRIVILEGE, polkit_result);
			}
			else
				ret = TRUE;
		}

        if (polkit_result != NULL)
                libhal_free_string (polkit_result);
        return ret;
}
#endif

/** 
 * dbus_send_reply:
 * @connection:		connection to D-Bus
 * @message:		Message
 * @type:               the type of data param
 * @data:		data to send
 *
 * Returns: 		TRUE/FALSE
 *
 * sends a reply to message with the given data and its dbus_type 
 */
static gboolean dbus_send_reply(DBusConnection *connection, DBusMessage *message,
				int dbus_type, void *data)
{
	DBusMessage *reply;

	if ((reply = dbus_message_new_method_return(message)) == NULL) {
		upslogx(LOG_WARNING, "Could not allocate memory for the DBus reply");
		return FALSE;
	}

	if (data != NULL)
		dbus_message_append_args(reply, dbus_type, data, DBUS_TYPE_INVALID);

	if (!dbus_connection_send(connection, reply, NULL)) {
		upslogx(LOG_WARNING, "Could not sent reply");
		return FALSE;
	}
	dbus_connection_flush(connection);
	dbus_message_unref(reply);
	
	return TRUE;
}

/** 
 * dbus_get_argument:
 * @connection:		connection to D-Bus
 * @message:		Message
 * @dbus_error:         the D-Bus error
 * @type:               the type of arg param
 * @arg:		the value to get from the message
 *
 * Returns: 		TRUE/FALSE
 *
 * gets one argument from message with the given dbus_type and stores it in arg
 */
static gboolean dbus_get_argument(DBusConnection *connection, DBusMessage *message,
				  DBusError *dbus_error, int dbus_type, void *arg)
{
	dbus_message_get_args(message, dbus_error, dbus_type, arg,
			      DBUS_TYPE_INVALID);
	if (dbus_error_is_set(dbus_error)) {
		upslogx(LOG_WARNING, "Could not get argument of DBus message: %s",
			     dbus_error->message);
		dbus_error_free(dbus_error);
		return FALSE;
	}
	return TRUE;
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
	int retcode = -1;
	const char	*member		= dbus_message_get_member(message);
	const char	*path		= dbus_message_get_path(message);
/*	int ret = DBUS_HANDLER_RESULT_NOT_YET_HANDLED; */

	/* upsdebugx(2, "Received DBus message with member %s path %s", member, path); */
	fprintf(stdout, "Received DBus message with member %s on path %s\n", member, path);

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

		if (dbus_message_is_method_call(message, DBUS_INTERFACE, "Shutdown")) {
			
	 		fprintf(stdout, "executing Shutdown\n");
			upsdrv_shutdown();
			dbus_send_reply(connection, message, DBUS_TYPE_INVALID, NULL);

		} else if (dbus_message_is_method_call(message,
												DBUS_INTERFACE, "SetBeeper")) {
	 		fprintf(stdout, "executing SetBeeper\n");
			gboolean b_enable;
			if (!dbus_get_argument(connection, message, &dbus_error,
					       DBUS_TYPE_BOOLEAN, &b_enable)) {
	 			fprintf(stderr, "Error receiving boolean argument\n");
				return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
			}
	 		fprintf(stdout, "Received argument: %s\n", (b_enable==TRUE)?"true":"false");
			
			if (b_enable==TRUE) {
				if (upsh.instcmd("beeper.enable", NULL) != STAT_INSTCMD_HANDLED) {
					dbus_send_reply(connection, message, DBUS_TYPE_INT32, &retcode);
					return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
				}
			}
			else {
				if (upsh.instcmd("beeper.disable", NULL) != STAT_INSTCMD_HANDLED) {
					dbus_send_reply(connection, message, DBUS_TYPE_INT32, &retcode);
					return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
				}
			}
		} else {
			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}
	}
	dbus_send_reply(connection, message, DBUS_TYPE_INVALID, NULL);
	return DBUS_HANDLER_RESULT_HANDLED;
}
