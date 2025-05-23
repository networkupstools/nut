/* failover.c - UPS Failover Driver

   Copyright (C)
       2025 - Sebastian Kuttnig <sebastian.kuttnig@gmail.com>

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

#include "config.h"
#include "main.h"
#include "failover.h"
#include "nut_stdint.h"
#include "parseconf.h"
#include "timehead.h"
#include "upsdrvquery.h"

#define DRIVER_NAME      "UPS Failover Driver"
#define DRIVER_VERSION   "0.01"

upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Sebastian Kuttnig <sebastian.kuttnig@gmail.com>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

static status_filters_t arg_status_filters;

static int arg_init_timeout        = DEFAULT_INIT_TIMEOUT;
static int arg_dead_timeout        = DEFAULT_DEAD_TIMEOUT;
static int arg_relog_timeout       = DEFAULT_RELOG_TIMEOUT;
static int arg_noprimary_timeout   = DEFAULT_NO_PRIMARY_TIMEOUT;
static int arg_maxconnfails        = DEFAULT_MAX_CONNECT_FAILS;
static int arg_coolofftimeout      = DEFAULT_CONNECTION_COOLOFF;
static int arg_fsdmode             = DEFAULT_FSD_MODE;
static int arg_strict_filtering    = DEFAULT_STRICT_FILTERING;

static int init_time_elapsed;
static int primaries_gone;

static time_t drv_startup_time;
static time_t primaries_gone_time;

static ups_device_t **ups_list;
static ups_device_t *primary_ups;
static ups_device_t *last_primary_ups;

static size_t ups_count;
static size_t ups_alive_count;
static size_t ups_online_count;
static size_t ups_primary_count;

static int instcmd(const char *cmdname, const char *extra);
static int setvar(const char *varname, const char *val);

static void handle_arguments(void);
static void parse_port_argument(void);
static void parse_status_filters(void);
static void handle_connections(void);
static void export_driver_state(void);

static void handle_no_primaries(void);
static int handle_init_time(const ups_device_t *primary_candidate);

static int ups_connect(ups_device_t *ups);
static int ups_read_data(ups_device_t *ups);
static void ups_disconnect(ups_device_t *ups);
static int ups_parse_protocol(ups_device_t *ups, size_t numargs, char **arg);

static int is_ups_alive(ups_device_t *ups);
static void ups_is_alive(ups_device_t *ups);
static void ups_is_dead(ups_device_t *ups);
static void ups_is_online(ups_device_t *ups);
static void ups_is_offline(ups_device_t *ups);

static ups_device_t *get_primary_candidate(void);
static int ups_passes_status_filters(const ups_device_t *ups);
static void ups_promote_primary(ups_device_t *ups);
static void ups_demote_primary(ups_device_t *ups);
static void ups_export_dstate(ups_device_t *ups);
static void ups_clean_dstate(const ups_device_t *ups);

static int ups_get_cmd_pos(const ups_device_t *ups, const char *cmd);
static int ups_add_cmd(ups_device_t *ups, const char *val);
static int ups_del_cmd(ups_device_t *ups, const char *val);

static int ups_get_var_pos(const ups_device_t *ups, const char *key);
static int ups_set_var(ups_device_t *ups, const char *key, const char *value);
static int ups_del_var(ups_device_t *ups, const char *key);
static int ups_set_var_flags(ups_device_t *ups, const char *key, const int flag);
static int ups_set_var_aux(ups_device_t *ups, const char *key, const long aux);
static int ups_add_range(ups_device_t *ups, const char *varkey, const int min, const int max);
static int ups_del_range(ups_device_t *ups, const char *varkey, const int min, const int max);
static int ups_add_enum(ups_device_t *ups, const char *varkey, const char *enumval);
static int ups_del_enum(ups_device_t *ups, const char *varkey, const char *enumval);

static void free_status_filters(void);
static void ups_free_ups_state(ups_device_t *ups);
static void ups_free_var_state(ups_var_t *var);
static const char *rewrite_driver_prefix(const char *in, char *out, size_t outlen);
static int split_socket_name(const char *input, char **driver, char **ups);
static void csv_arg_to_array(const char *argname, const char *argcsv, char ***array, size_t *countvar);

static inline void ups_set_flag(ups_device_t *ups, ups_flags_t flag);
static inline void ups_clear_flag(ups_device_t *ups, ups_flags_t flag);
static inline int ups_has_flag(const ups_device_t *ups, ups_flags_t flag);

void upsdrv_initups(void)
{
	handle_arguments();
}

void upsdrv_initinfo(void)
{
	char buf[SMALLBUF];
	size_t i = 0;
	int required = -1;

	for (i = 0; i < ups_count; ++i) {
		ups_device_t *ups = ups_list[i];

		ups_connect(ups);

		required = snprintf(buf, sizeof(buf), "%s.force.ignore", ups->socketname);
		dstate_addcmd(buf);

		if ((size_t)required >= sizeof(buf)) {
			upslogx(LOG_WARNING, "%s: truncated administrative command size "
				"[%" PRIuSIZE "] exceeds buffer of size [%" PRIuSIZE "]: %s",
				__func__, (size_t)required, sizeof(buf), buf);
		}

		required = snprintf(buf, sizeof(buf), "%s.force.primary", ups->socketname);
		dstate_addcmd(buf);

		if ((size_t)required >= sizeof(buf)) {
			upslogx(LOG_WARNING, "%s: truncated administrative command size "
				"[%" PRIuSIZE "] exceeds buffer of size [%" PRIuSIZE "]: %s",
				__func__, (size_t)required, sizeof(buf), buf);
		}
	}

	if (!ups_alive_count) {
		upslogx(LOG_WARNING, "%s: none of the tracked UPS drivers were connectable",
			__func__);
	}

	status_init();
	status_set("WAIT");
	status_commit();

	time(&drv_startup_time);

	upsh.instcmd = instcmd;
	upsh.setvar = setvar;

	dstate_dataok();
}

void upsdrv_updateinfo(void)
{
	ups_device_t *primary_candidate = NULL;

	handle_connections();

	primary_candidate = get_primary_candidate();

	export_driver_state();

	if (handle_init_time(primary_candidate)) {
		return;
	}

	if (!primary_candidate) {
		handle_no_primaries();

		return;
	}

	if (primaries_gone) {
		if (primary_candidate == primary_ups) {
			/* Special handling for fsdmode 0 where primary was never demoted */
			upslogx(LOG_NOTICE, "%s: [%s] was declared to be a suitable primary (again)",
				__func__, primary_candidate->socketname);
			ups_clean_dstate(primary_candidate);
			primary_candidate->force_dstate_export = 1;
		}
		primaries_gone = 0;
		primaries_gone_time = 0;
	}

	if (primary_ups != primary_candidate) {
		ups_promote_primary(primary_candidate);
	} else {
		ups_export_dstate(primary_ups);
	}

	if(!ups_has_flag(primary_candidate, UPS_FLAG_DATA_OK)) {
		dstate_datastale();

		return;
	}

	dstate_dataok();
}

void upsdrv_shutdown(void)
{
	upslogx(LOG_ERR, "%s: %s: Shutdown is not supported by this proxying driver. "
		"Upstream drivers may implement their own shutdown handling, which would be "
		"called directly or by upsdrvctl to shut down any specific upstream driver.",
		progname, __func__);

	if (handling_upsdrv_shutdown > 0) {
		set_exit_flag(EF_EXIT_FAILURE);
	}
}

void upsdrv_help(void)
{

}

void upsdrv_makevartable(void)
{
	char buf[SMALLBUF];

	snprintf(buf, sizeof(buf),
		"Grace period in seconds during which no primaries found are "
		"acceptable (for driver startup) (default: %d)",
		arg_init_timeout);
	addvar(VAR_VALUE, "inittime", buf);

	snprintf(buf, sizeof(buf),
		"Grace period in seconds after which a non-responsive UPS "
		"driver is considered dead (default: %d)",
		arg_dead_timeout);
	addvar(VAR_VALUE, "deadtime", buf);

	snprintf(buf, sizeof(buf),
		"Grace period in seconds until connection failures are logged "
		"again (to reduce spamming logs) (default: %d)",
		arg_relog_timeout);
	addvar(VAR_VALUE, "relogtime", buf);

	snprintf(buf, sizeof(buf),
		"Grace period in seconds until 'fsdmode' is entered into after "
		"not finding any primaries (default: %d)",
		arg_noprimary_timeout);
	addvar(VAR_VALUE, "noprimarytime", buf);

	snprintf(buf, sizeof(buf),
		"Maximum amount of failures connecting to a driver until "
		"'coolofftime' is entered into (default: %d)",
		arg_maxconnfails);
	addvar(VAR_VALUE, "maxconnfails", buf);

	snprintf(buf, sizeof(buf),
		"Period in seconds during which driver connections are not "
		"retried after exceeding 'maxconnfails' (default: %d)",
		arg_coolofftimeout);
	addvar(VAR_VALUE, "coolofftime", buf);

	snprintf(buf, sizeof(buf),
		"Sets no primary behavior (0: last primary data + stale, 1: no "
		"data + alarm + stale, 2: no data + fsd + alarm) (default: %d)",
		arg_fsdmode);
	addvar(VAR_VALUE, "fsdmode", buf);

	snprintf(buf, sizeof(buf),
		"Sets if only the given status filters should be considered for "
		"UPS driver to be electable as primary (default: %d)",
		arg_strict_filtering);
	addvar(VAR_VALUE, "strictfiltering", buf);

	addvar(VAR_VALUE, "status_have_any",
		"Comma separated list of status tokens, any present qualifies "
		"the UPS driver for primary (default: unset)");
	addvar(VAR_VALUE, "status_have_all",
		"Comma separated list of status tokens, only all present "
		"qualifies the UPS driver for primary (default: unset)");
	addvar(VAR_VALUE, "status_nothave_any",
		"Comma separated list of status tokens, any present disqualifies "
		"the UPS driver for primary (default: unset)");
	addvar(VAR_VALUE, "status_nothave_all",
		"Comma separated list of status tokens, only all present "
		"disqualifies the UPS driver for primary (default: unset)");
}

void upsdrv_cleanup(void)
{
	size_t i = 0;

	for (i = 0; i < ups_count; ++i) {
		ups_device_t *ups = ups_list[i];

		if (ups) {
			if (primary_ups == ups) {
				primary_ups = NULL;
			}

			if (last_primary_ups == ups) {
				last_primary_ups = NULL;
			}

			ups_disconnect(ups); /* free conn + ctx */

			ups_free_ups_state(ups); /* free status, vars, subvars + cmds */

			if (ups->name) {
				free(ups->name);
				ups->name = NULL;
			}

			if (ups->drivername) {
				free(ups->drivername);
				ups->drivername = NULL;
			}

			if (ups->socketname) {
				free(ups->socketname);
				ups->socketname = NULL;
			}

			free(ups);
			ups_list[i] = NULL;
		}
	}

	if (ups_list) {
		free(ups_list);
		ups_list = NULL;
	}

	free_status_filters(); /* free status filters */
}

static int instcmd(const char *cmdname, const char *extra)
{
	size_t i = 0;

	upsdebug_INSTCMD_STARTING(cmdname, extra);

	for (i = 0; i < ups_count; ++i) {
		ups_device_t *ups = ups_list[i];
		size_t len = strlen(ups->socketname);

		if (!strncmp(cmdname, ups->socketname, len)) {
			const char *subcmd = cmdname + len;

			if (!strcmp(subcmd, ".force.ignore")) {
				time_t now;

				time(&now);

				ups->force_ignore = extra ? atoi(extra) : 0;
				ups->force_ignore_time = ups->force_ignore ? now : 0;

				upslogx(LOG_NOTICE, "%s: set [force_ignore] to [%d] on [%s]",
					__func__, ups->force_ignore, ups->socketname);

				return STAT_INSTCMD_HANDLED;
			}

			if (!strcmp(subcmd, ".force.primary")) {
				time_t now;

				time(&now);

				ups->force_primary = extra ? atoi(extra) : 0;
				ups->force_primary_time = ups->force_primary ? now : 0;

				upslogx(LOG_NOTICE, "%s: set [force_primary] to [%d] on [%s]",
					__func__, ups->force_primary, ups->socketname);

				return STAT_INSTCMD_HANDLED;
			}
		}
	}

	if (!primary_ups) {
		upslogx(LOG_INSTCMD_FAILED, "%s: received [%s] [%s], but"
			"there is currently no elected primary able to handle it",
			__func__, cmdname, NUT_STRARG(extra));

		return STAT_INSTCMD_FAILED;
	}

	if(ups_get_cmd_pos(primary_ups, cmdname) >= 0) {
		const char *cmd = NULL;
		char msgbuf[SMALLBUF];
		struct timeval tv;
		ssize_t	cmdret = -1;
		int required = -1;

		if (!strncmp(cmdname, "upstream.", 9)) {
			cmd = cmdname + 9;
			upsdebugx(3, "%s: rewriting from [%s] to [%s] for upstream driver",
				__func__, cmdname, cmd);
		} else {
			cmd = cmdname;
		}

		if (extra) {
			required = snprintf(msgbuf, sizeof(msgbuf), "INSTCMD %s %s\n", cmd, extra);
		} else {
			required = snprintf(msgbuf, sizeof(msgbuf), "INSTCMD %s\n", cmd);
		}

		if ((size_t)required >= sizeof(msgbuf)) {
			upslogx(LOG_WARNING, "%s: truncated INSTCMD command size "
				"[%" PRIuSIZE "] exceeds buffer of size [%" PRIuSIZE "]: %s",
				__func__, (size_t)required, sizeof(msgbuf), msgbuf);
		}

		tv.tv_sec = CONN_CMD_TIMEOUT;
		tv.tv_usec = 0;

		cmdret = upsdrvquery_oneshot(primary_ups->drivername, primary_ups->name,
			msgbuf, NULL, 0, &tv);

		if (cmdret >= 0) {
			upslogx(LOG_NOTICE, "%s: sent [%s] [%s], "
				"received response code: [%" PRIiSIZE "]",
				__func__, cmdname, NUT_STRARG(extra), cmdret);

			return cmdret;
		} else {
			upslog_with_errno(LOG_INSTCMD_FAILED, "%s: sent [%s] [%s], "
				"received no response code due to socket failure",
				__func__, cmdname, NUT_STRARG(extra));

			return STAT_INSTCMD_FAILED;
		}
	}

	upslogx(LOG_INSTCMD_UNKNOWN, "%s: received [%s] [%s], "
		"but it is not among the primary's supported commands",
		__func__, cmdname, NUT_STRARG(extra));

	return STAT_INSTCMD_UNKNOWN;
}

static int setvar(const char *varname, const char *val)
{
	upsdebug_SET_STARTING(varname, val);

	if (!primary_ups) {
		upslogx(LOG_SET_FAILED, "%s: received [%s] [%s], but "
			"there is currently no elected primary able to handle it",
			__func__, varname, val);

		return STAT_SET_FAILED;
	}

	if(ups_get_var_pos(primary_ups, varname) >= 0) {
		const char *var = NULL;
		char msgbuf[SMALLBUF];
		struct timeval tv;
		ssize_t	cmdret = -1;
		int required = -1;

		if (!strncmp(varname, "upstream.", 9)) {
			var = varname + 9;
			upsdebugx(3, "%s: rewriting from [%s] to [%s] for upstream driver",
				__func__, varname, var);
		} else {
			var = varname;
		}

		required = snprintf(msgbuf, sizeof(msgbuf), "SET %s \"%s\"\n", var, val);

		if ((size_t)required >= sizeof(msgbuf)) {
			upslogx(LOG_WARNING, "%s: truncated SET command size "
				"[%" PRIuSIZE "] exceeds buffer of size [%" PRIuSIZE "]: %s",
				__func__, (size_t)required, sizeof(msgbuf), msgbuf);
		}

		tv.tv_sec = CONN_CMD_TIMEOUT;
		tv.tv_usec = 0;

		cmdret = upsdrvquery_oneshot(primary_ups->drivername, primary_ups->name,
			msgbuf, NULL, 0, &tv);

		if (cmdret >= 0) {
			upslogx(LOG_NOTICE, "%s: sent [%s] [%s], "
				"received response code: [%" PRIiSIZE "]",
				__func__, varname, val, cmdret);

			return cmdret;
		} else {
			upslog_with_errno(LOG_SET_FAILED, "%s: sent [%s] [%s], "
				"received no response code due to socket failure",
				__func__, varname, val);

			return STAT_SET_FAILED;
		}
	}

	upslogx(LOG_SET_UNKNOWN, "%s: received [%s] [%s], "
		"but it is not among the primary's supported variables",
		__func__, varname, val);

	return STAT_SET_UNKNOWN;
}

static void handle_arguments(void)
{
	const char *val = NULL;

	parse_port_argument();
	parse_status_filters();

	val = getval("inittime");
	if (val) {
		arg_init_timeout = atoi(val);
		upsdebugx(1, "%s: set 'inittime' to [%d] from configuration",
			__func__, arg_init_timeout);
	}

	val = getval("deadtime");
	if (val) {
		arg_dead_timeout = atoi(val);
		upsdebugx(1, "%s: set 'deadtime' to [%d] from configuration",
			__func__, arg_dead_timeout);
	}

	val = getval("relogtime");
	if (val) {
		arg_relog_timeout = atoi(val);
		upsdebugx(1, "%s: set 'relogtime' to [%d] from configuration",
			__func__, arg_relog_timeout);
	}

	val = getval("noprimarytime");
	if (val) {
		arg_noprimary_timeout = atoi(val);
		upsdebugx(1, "%s: set 'noprimarytime' to [%d] from configuration",
			__func__, arg_noprimary_timeout);
	}

	val = getval("maxconnfails");
	if (val) {
		arg_maxconnfails = atoi(val);
		upsdebugx(1, "%s: set 'maxconnfails' to [%d] from configuration",
			__func__, arg_maxconnfails);
	}

	val = getval("coolofftime");
	if (val) {
		arg_coolofftimeout = atoi(val);
		upsdebugx(1, "%s: set 'coolofftime' to [%d] from configuration",
			__func__, arg_coolofftimeout);
	}

	val = getval("fsdmode");
	if (val) {
		arg_fsdmode = atoi(val);
		if (arg_fsdmode >= 0 && arg_fsdmode <= 2) {
			upsdebugx(1, "%s: set 'fsdmode' to [%d] from configuration",
				__func__, arg_fsdmode);
		} else {
			upslogx(LOG_ERR, "%s: invalid 'fsdmode' of [%d] from configuration, "
				"set to the default 'fsdmode' value of [%d] instead",
				__func__, arg_fsdmode, DEFAULT_FSD_MODE);
			arg_fsdmode = DEFAULT_FSD_MODE;
		}
	}

	val = getval("strictfiltering");
	if (val) {
		arg_strict_filtering = atoi(val);
		upsdebugx(1, "%s: set 'strictfiltering' to [%d] from configuration",
			__func__, arg_strict_filtering);
	}
}

static void parse_port_argument(void)
{
	char *tmp = NULL;
	char *token = NULL;
	const char *str = device_path;

	tmp = xstrdup(str);

	token = strtok(tmp, ",");
	while (token) {
		ups_device_t *new_ups = NULL;

		str_trim_space(token);

		new_ups = xcalloc(1, sizeof(**ups_list));
		new_ups->socketname = xstrdup(token);

		if (!split_socket_name(new_ups->socketname, &new_ups->drivername, &new_ups->name)) {
			char buf[SMALLBUF];
			snprintf(buf, sizeof(buf), "%s", token); /* for fatalx */

			free(new_ups->socketname);
			free(new_ups);
			free(tmp);

			fatalx(EXIT_FAILURE, "%s: %s: the 'port' argument has an invalid format, "
				"[%s] is not a valid splittable socket name, please correct the argument",
				progname, __func__, buf);
		} else {
			upsdebugx(3, "%s: [%s] was parsed into UPS driver [%s] and UPS [%s]",
				__func__, new_ups->socketname, new_ups->drivername, new_ups->name);
		}

		ups_list = xrealloc(ups_list, sizeof(*ups_list) * (ups_count + 1));
		ups_list[ups_count] = new_ups;
		ups_count++;

		upsdebugx(1, "%s: [%s]: was added to the list of tracked UPS drivers",
			__func__, new_ups->socketname);

		token = strtok(NULL, ",");
	}

	free(tmp);
}

static void parse_status_filters(void)
{
	csv_arg_to_array("status_have_any",
		getval("status_have_any"),
		&arg_status_filters.have_any,
		&arg_status_filters.have_any_count);

	csv_arg_to_array("status_have_all",
		getval("status_have_all"),
		&arg_status_filters.have_all,
		&arg_status_filters.have_all_count);

	csv_arg_to_array("status_nothave_any",
		getval("status_nothave_any"),
		&arg_status_filters.nothave_any,
		&arg_status_filters.nothave_any_count);

	csv_arg_to_array("status_nothave_all",
		getval("status_nothave_all"),
		&arg_status_filters.nothave_all,
		&arg_status_filters.nothave_all_count);
}

static void handle_connections(void)
{
	size_t i = 0;

	for (i = 0; i < ups_count; ++i) {
		ups_device_t *ups = ups_list[i];

		if (!ups_has_flag(ups, UPS_FLAG_ALIVE) && !ups_connect(ups)) {
			/* Reconnecting a dead UPS has failed... skip it */

			continue;
		}
		else if (!is_ups_alive(ups)) {
			/* UPS is dead long enough... disconnect it */
			upslogx(LOG_WARNING, "%s: [%s]: connection to UPS driver was lost (declared dead)",
				__func__, ups->socketname);
			ups_disconnect(ups);

			continue;
		}

		if (ups_read_data(ups) < 0 ) {
			/* Socket failure... warrants immediate disconnect */
			upslog_with_errno(LOG_ERR, "%s: [%s]: connection to UPS driver was lost (socket failure)",
				__func__, ups->socketname);
			ups_disconnect(ups);
		}
	}
}

static void export_driver_state(void)
{
	dstate_setinfo("driver.stats.alive_drivers", "%" PRIuSIZE, ups_alive_count);
	dstate_setinfo("driver.stats.online_drivers", "%" PRIuSIZE, ups_online_count);
	dstate_setinfo("driver.stats.primary_drivers", "%" PRIuSIZE, ups_primary_count);
	dstate_setinfo("driver.stats.total_drivers", "%" PRIuSIZE, ups_count);

	if (primary_ups) {
		dstate_setinfo("driver.primary.upsname", "%s", primary_ups->name);
		dstate_setinfo("driver.primary.drvname", "%s", primary_ups->drivername);
		dstate_setinfo("driver.primary.sockname", "%s", primary_ups->socketname);
		dstate_setinfo("driver.primary.priority", "%d", primary_ups->priority);
		dstate_setinfo("driver.primary.stats.cmds", "%" PRIuSIZE, primary_ups->cmd_count);
		dstate_setinfo("driver.primary.stats.vars", "%" PRIuSIZE, primary_ups->var_count);
	} else {
		dstate_delinfo("driver.primary.upsname");
		dstate_delinfo("driver.primary.drvname");
		dstate_delinfo("driver.primary.sockname");
		dstate_delinfo("driver.primary.priority");
		dstate_delinfo("driver.primary.stats.cmds");
		dstate_delinfo("driver.primary.stats.vars");
	}

	upsdebugx(4, "%s: exported internal driver state to dstate",
		__func__);
}

static void handle_no_primaries(void)
{
	time_t now;
	double elapsed;

	if (!primaries_gone_time) {
		time(&primaries_gone_time);
	}

	if (primary_ups && arg_fsdmode > 0) {
		ups_demote_primary(primary_ups);
	}

	time(&now);

	elapsed = difftime(now, primaries_gone_time);

	if (elapsed > arg_noprimary_timeout) {
		if (!primaries_gone) {
			upslogx(LOG_WARNING, "%s: none of the tracked UPS drivers are suitable primaries",
				__func__);
		}

		switch (arg_fsdmode) {
			case 0:
				if (!primaries_gone) {
					upslogx(LOG_WARNING, "%s: 'fsdmode' is [0]: "
						"keeping last primary and declaring data stale immediately",
						__func__);
				}

				dstate_datastale();
				break;

			case 1:
				if (!primaries_gone) {
					upslogx(LOG_WARNING, "%s: 'fsdmode' is [1]: "
						"demoting last primary, raising alarm, and declaring data stale "
						"after another %d seconds elapse to ensure full ALARM propagation",
						__func__, ALARM_PROPAG_TIME);
				}

				/* dstate is already clean at this point, hence no _init() calls */
				alarm_set("No suitable primaries for failover");
				alarm_commit();
				status_commit(); /* publish ALARM */

				if (elapsed < arg_noprimary_timeout + ALARM_PROPAG_TIME) {
					/* make sure ALARM propagates to all clients first... */
					dstate_dataok();
				} else {
					/* ... and then eventually declare the data as stale */
					dstate_datastale();
				}
				break;

			case 2:
				if (!primaries_gone) {
					upslogx(LOG_WARNING, "%s: 'fsdmode' is [2]: "
						"demoting last primary, raising alarm, and setting FSD",
						__func__);
				}

				/* dstate is already clean at this point, hence no _init() calls */
				status_set("FSD");
				alarm_set("No suitable primaries for failover");
				alarm_commit();
				status_commit(); /* publish ALARM + FSD */

				dstate_dataok();
				break;

			default:
				/* Should never happen, as we validate the argument */
				if (!primaries_gone) {
					upslogx(LOG_WARNING, "%s: 'fsdmode' has unknown value [%d]: "
						"keeping last primary and declaring data stale immediately",
						__func__, arg_fsdmode);
				}

				dstate_datastale();
				break;
		}
		primaries_gone = 1;
	} else {
		upslogx(LOG_WARNING, "%s: No suitable primaries, "
			"waiting for one to emerge... (%.0fs of %ds max)",
			__func__, elapsed, arg_noprimary_timeout);

		dstate_dataok();
	}
}

static int handle_init_time(const ups_device_t *primary_candidate)
{
	if (!init_time_elapsed) {
		time_t now;
		double elapsed;

		time(&now);
		elapsed = difftime(now, drv_startup_time);

		if (!primary_candidate && elapsed <= arg_init_timeout) {
			upslogx(LOG_NOTICE, "%s: still waiting for "
				"first primary to emerge... (%.0fs of %ds max), if this was "
				"too short for drivers to start, consider increasing 'inittime'",
				__func__, elapsed, arg_init_timeout);

			dstate_dataok();

			return 1;
		}

		init_time_elapsed = 1;
	}

	return 0;
}

static int ups_connect(ups_device_t *ups)
{
	time_t now;
	double elapsed;
	int ret = 0;
	int report_failure = 1;
	udq_pipe_conn_t *conn = NULL;

	time(&now);

	elapsed = difftime(now, ups->last_failure_time);

	if (ups->failure_count > arg_maxconnfails && elapsed <= arg_coolofftimeout) {
		upsdebugx(4, "%s: [%s]: not retrying in cooloff phase (%.0fs < %ds max)",
			__func__, ups->socketname, elapsed, arg_coolofftimeout);

		return 0;
	}

	if (nut_debug_level < 1 && elapsed <= arg_relog_timeout) {
		report_failure = 0;
		nut_upsdrvquery_debug_level = 0;
	} else {
		report_failure = 1;
		nut_upsdrvquery_debug_level = NUT_UPSDRVQUERY_DEBUG_LEVEL_DEFAULT;
	}

	conn = upsdrvquery_connect(ups->socketname);

	if (conn) {
		pconf_init(&ups->parse_ctx, NULL);
		ups->conn = conn;

		upslogx(LOG_NOTICE, "%s: [%s]: connection is now established",
			__func__, ups->socketname);

		if (upsdrvquery_write(ups->conn, "DUMPALL\n") >= 0) {
			ups_free_ups_state(ups); /* free any previous state */
			ups->force_dstate_export = 1;

			ups_is_alive(ups);
			time(&ups->last_heard_time);

			ups->failure_count = 0;
			ups->last_failure_time = 0;

			upsdebugx(2, "%s: [%s]: requested first batch of data (DUMPALL)",
				__func__, ups->socketname);

			ret = 1;
		} else {
			if (report_failure) {
				upslog_with_errno(LOG_ERR, "%s: [%s]: communication failed "
					"sending DUMPALL, disconnecting and re-trying it later",
					__func__, ups->socketname);
			}
			ups_disconnect(ups);

			ups->failure_count++;
			ups->last_failure_time = now;

			ret = 0;
		}
	} else {
		if (report_failure) {
			upslog_with_errno(LOG_ERR, "%s: [%s]: failed to establish connection",
				__func__, ups->socketname);
		}

		ups->failure_count++;
		ups->last_failure_time = now;

		ret = 0;
	}

	nut_upsdrvquery_debug_level = NUT_UPSDRVQUERY_DEBUG_LEVEL_DEFAULT;

	return ret;
}

static int ups_read_data(ups_device_t *ups)
{
	int	i = 0;
	ssize_t	ret;
	struct timeval tv;

	tv.tv_sec = CONN_READ_TIMEOUT;
	tv.tv_usec = 0;

	ret = upsdrvquery_read_timeout(ups->conn, tv);

	if (ret == -1) {
		upsdebug_with_errno(2, "%s: [%s]: read from UPS driver has failed",
			__func__, ups->socketname);

		return ret;
	}

	if (ret == -2) {
		upsdebug_with_errno(2, "%s: [%s]: read from UPS driver has timed out",
			__func__, ups->socketname);

		return ret;
	}

	for (i = 0; i < ret; ++i) {
		switch (pconf_char(&ups->parse_ctx, ups->conn->buf[i]))
		{
			case 1:
				if (ups_parse_protocol(ups, ups->parse_ctx.numargs, ups->parse_ctx.arglist)) {
					time(&ups->last_heard_time);
				}
				continue;

			case 0:
				continue; /* no complete line yet */

			default:
				upsdebug_with_errno(2, "%s: [%s]: parse error on read data: %s",
					__func__, ups->socketname, ups->parse_ctx.errmsg);

				return -1;
		}
	}

	return ret;
}

static void ups_disconnect(ups_device_t *ups)
{
	ups_is_dead(ups);
	pconf_finish(&ups->parse_ctx);

	ups->flags = UPS_FLAG_NONE;

	if (ups->conn) {
		upsdrvquery_close(ups->conn);
		free(ups->conn);
		ups->conn = NULL;
	}

	upsdebugx(2, "%s: [%s]: connection was destroyed",
		__func__, ups->socketname);
}

static int ups_parse_protocol(ups_device_t *ups, size_t numargs, char **arg)
{
	char buf[SMALLBUF];
	const char *varptr = NULL;
	int required = -1;

	if (numargs < 1) {
		goto skip_out;
	}

	if (!strcasecmp(arg[0], "PONG")) {
		upsdebugx(6, "%s: [%s]: got PONG from UPS driver",
			__func__, ups->socketname);

		return 1;
	}

	if (!strcasecmp(arg[0], "DUMPDONE")) {
		upsdebugx(6, "%s: [%s]: got DUMPDONE from UPS driver",
			__func__, ups->socketname);

		ups_set_flag(ups, UPS_FLAG_DUMPED);

		return 1;
	}

	if (!strcasecmp(arg[0], "DATASTALE")) {
		upsdebugx(6, "%s: [%s]: got DATASTALE from UPS driver",
			__func__, ups->socketname);

		ups_clear_flag(ups, UPS_FLAG_DATA_OK);

		return 1;
	}

	if (!strcasecmp(arg[0], "DATAOK")) {
		upsdebugx(6, "%s: [%s]: got DATAOK from UPS driver",
			__func__, ups->socketname);

		ups_set_flag(ups, UPS_FLAG_DATA_OK);

		return 1;
	}

	if (numargs < 2) {
		goto skip_out;
	}

	/* DELCMD <cmdname> */
	if (!strcasecmp(arg[0], "DELCMD")) {
		upsdebugx(6, "%s: [%s]: got DELCMD [%s] from UPS driver",
			__func__, ups->socketname, arg[1]);

		required = snprintf(buf, sizeof(buf), "upstream.%s", arg[1]);

		if ((size_t)required >= sizeof(buf)) {
			upslogx(LOG_WARNING, "%s: truncated DELCMD command size "
				"[%" PRIuSIZE "] exceeds buffer of size [%" PRIuSIZE "]: %s",
				__func__, (size_t)required, sizeof(buf), buf);
		}

		ups_del_cmd(ups, buf);

		return 1;
	}

	/* ADDCMD <cmdname> */
	if (!strcasecmp(arg[0], "ADDCMD")) {
		upsdebugx(6, "%s: [%s]: got ADDCMD [%s] from UPS driver",
			__func__, ups->socketname, arg[1]);

		required = snprintf(buf, sizeof(buf), "upstream.%s", arg[1]);

		if ((size_t)required >= sizeof(buf)) {
			upslogx(LOG_WARNING, "%s: truncated ADDCMD command size "
				"[%" PRIuSIZE "] exceeds buffer of size [%" PRIuSIZE "]: %s",
				__func__, (size_t)required, sizeof(buf), buf);
		}

		ups_add_cmd(ups, buf);

		return 1;
	}

	/* DELINFO <var> */
	if (!strcasecmp(arg[0], "DELINFO")) {
		varptr = rewrite_driver_prefix(arg[1], buf, sizeof(buf));

		upsdebugx(6, "%s: [%s]: got DELINFO [%s] from UPS driver",
			__func__, ups->socketname, arg[1]);

		ups_del_var(ups, varptr);

		return 1;
	}

	if (numargs < 3) {
		goto skip_out;
	}

	/* SETFLAGS <varname> <flag>... */
	if (!strcasecmp(arg[0], "SETFLAGS")) {
		size_t i = 0;
		int varflags = 0;

		varptr = rewrite_driver_prefix(arg[1], buf, sizeof(buf));

		upsdebugx(6, "%s: [%s]: got SETFLAGS [%s] from UPS driver",
			__func__, ups->socketname, arg[1]);

		for (i = 2; i < numargs; ++i) {
			if (!strcasecmp(arg[i], "RW")) {
				varflags |= ST_FLAG_RW;
			}
			else if (!strcasecmp(arg[i], "STRING")) {
				varflags |= ST_FLAG_STRING;
			}
			else if (!strcasecmp(arg[i], "NUMBER")) {
				varflags |= ST_FLAG_NUMBER;
			}
			else {
				upsdebugx(6, "%s: [%s]: got unknown SETFLAGS [%s] from UPS driver",
					__func__, ups->socketname, arg[i]);
			}
		}

		ups_set_var_flags(ups, varptr, varflags);

		return 1;
	}

	/* SETAUX <varname> <numeric value> */
	if (!strcasecmp(arg[0], "SETAUX")) {
		varptr = rewrite_driver_prefix(arg[1], buf, sizeof(buf));

		upsdebugx(6, "%s: [%s]: got SETAUX [%s] from UPS driver",
			__func__, ups->socketname, arg[1]);

		ups_set_var_aux(ups, varptr, atol(arg[2]));

		return 1;
	}

	/* DELENUM <varname> <value> */
	if (!strcasecmp(arg[0], "DELENUM")) {
		varptr = rewrite_driver_prefix(arg[1], buf, sizeof(buf));

		upsdebugx(6, "%s: [%s]: got DELENUM [%s] from UPS driver",
			__func__, ups->socketname, arg[1]);

		ups_del_enum(ups, varptr, arg[2]);

		return 1;
	}

	/* ADDENUM <varname> <value> */
	if (!strcasecmp(arg[0], "ADDENUM")) {
		varptr = rewrite_driver_prefix(arg[1], buf, sizeof(buf));

		upsdebugx(6, "%s: [%s]: got ADDENUM [%s] from UPS driver",
			__func__, ups->socketname, arg[1]);

		ups_add_enum(ups, varptr, arg[2]);

		return 1;
	}

	/* SETINFO <varname> <value> */
	if (!strcasecmp(arg[0], "SETINFO")) {
		varptr = rewrite_driver_prefix(arg[1], buf, sizeof(buf));

		upsdebugx(6, "%s: [%s]: got SETINFO [%s] from UPS driver",
			__func__, ups->socketname, arg[1]);

		if (!strcmp(arg[1], "ups.status")) {
			if (ups->status) {
				free(ups->status);
				ups->status = NULL;
			}
			ups->status = xstrdup(arg[2]);

			if(str_contains_token(arg[2], "OL")) {
				ups_is_online(ups);
			} else {
				ups_is_offline(ups);
			}
		}

		ups_set_var(ups, varptr, arg[2]);

		return 1;
	}

	if (numargs < 4) {
		goto skip_out;
	}

	/* DELRANGE <varname> <minvalue> <maxvalue> */
	if (!strcasecmp(arg[0], "DELRANGE")) {
		varptr = rewrite_driver_prefix(arg[1], buf, sizeof(buf));

		upsdebugx(6, "%s: [%s]: got DELRANGE [%s] from UPS driver",
			__func__, ups->socketname, arg[1]);

		ups_del_range(ups, varptr, atoi(arg[2]), atoi(arg[3]));

		return 1;
	}

	/* ADDRANGE <varname> <minvalue> <maxvalue> */
	if (!strcasecmp(arg[0], "ADDRANGE")) {
		varptr = rewrite_driver_prefix(arg[1], buf, sizeof(buf));

		upsdebugx(6, "%s: [%s]: got ADDRANGE [%s] from UPS driver",
			__func__, ups->socketname, arg[1]);

		ups_add_range(ups, varptr, atoi(arg[2]), atoi(arg[3]));

		return 1;
	}

skip_out:
	if (nut_debug_level > 0) {
		char msgbuf[LARGEBUF];
		size_t i = 0;
		int	len = -1;

		memset(msgbuf, 0, sizeof(msgbuf));
		for (i = 0; i < numargs; ++i) {
			len = snprintfcat(msgbuf, sizeof(msgbuf), "[%s] ", arg[i]);
		}
		if (len > 0) {
			msgbuf[len - 1] = '\0';
		}
		if ((size_t)len >= sizeof(msgbuf)) {
			upsdebugx(6, "%s: truncated DBG output size "
				"[%" PRIuSIZE "] exceeds buffer of size [%" PRIuSIZE "]: %s",
				__func__, (size_t)len, sizeof(msgbuf), msgbuf);
		}

		upsdebugx(6, "%s: [%s]: ignored protocol line with %" PRIuSIZE " keyword(s): %s",
			__func__, ups->socketname, numargs, numargs < 1 ? "<empty>" : msgbuf);
	}

	return 0;
}

static int is_ups_alive(ups_device_t *ups)
{
	time_t now;
	double elapsed;

	if (!ups->conn || INVALID_FD(ups->conn->sockfd)) {
		upsdebugx(2, "%s: [%s]: socket connection lost - declaring it dead",
			__func__, ups->socketname);

		return 0;
	}

	time(&now);

	elapsed = difftime(now, ups->last_heard_time);

	if ((elapsed > (arg_dead_timeout / 3)) &&
		(difftime(now, ups->last_pinged_time) > (arg_dead_timeout / 3))) {

		nut_upsdrvquery_debug_level = 0;
		upsdrvquery_write(ups->conn, "PING\n");
		nut_upsdrvquery_debug_level = NUT_UPSDRVQUERY_DEBUG_LEVEL_DEFAULT;

		upsdebugx(3, "%s: [%s]: have not heard from driver, sent a PING to it",
			__func__, ups->socketname);

		ups->last_pinged_time = now;
	}

	if (elapsed > arg_dead_timeout) {
		upsdebugx(2, "%s: [%s]: did not hear from driver "
			"for at least %.0fs (of %ds max) - declaring it dead",
			__func__, ups->socketname, elapsed, arg_dead_timeout);

		return 0;
	}

	return 1;
}

static void ups_is_alive(ups_device_t *ups) {
	if (!ups_has_flag(ups, UPS_FLAG_ALIVE)) {
		ups_set_flag(ups, UPS_FLAG_ALIVE);
		ups_alive_count++;
		upsdebugx(2, "%s: [%s]: is now alive (alive devices: %" PRIuSIZE ")",
			__func__, ups->socketname, ups_alive_count);
	}
}

static void ups_is_dead(ups_device_t *ups) {
	if (ups_has_flag(ups, UPS_FLAG_ALIVE)) {
		ups_alive_count--;
		upsdebugx(2, "%s: [%s]: is now dead "
			"with (last known) status [%s] (alive devices: %" PRIuSIZE ")",
			__func__, ups->socketname, ups->status, ups_alive_count);
	}

	if (ups_has_flag(ups, UPS_FLAG_ONLINE)) {
		ups_online_count--;
		upsdebugx(3, "%s: [%s]: was online with (last known) "
			"status [%s] and is now dead (online devices: %" PRIuSIZE ")",
			__func__, ups->socketname, ups->status, ups_online_count);
	}
}

static void ups_is_online(ups_device_t *ups) {
	if (!ups_has_flag(ups, UPS_FLAG_ONLINE)) {
		ups_set_flag(ups, UPS_FLAG_ONLINE);
		ups_online_count++;
		upsdebugx(2, "%s: [%s]: is now online "
			"with status [%s] (online devices: %" PRIuSIZE ")",
			__func__, ups->socketname, ups->status, ups_online_count);
	}
}

static void ups_is_offline(ups_device_t *ups) {
	if (ups_has_flag(ups, UPS_FLAG_ONLINE)) {
		ups_clear_flag(ups, UPS_FLAG_ONLINE);
		ups_online_count--;
		upsdebugx(2, "%s: [%s]: is now offline "
			"with status [%s] (online devices: %" PRIuSIZE ")",
			__func__, ups->socketname, ups->status, ups_online_count);
	}
}

static ups_device_t *get_primary_candidate(void)
{
	time_t now;
	size_t i = 0;
	size_t primaries = 0;
	int best_priority = 100;
	ups_device_t *best_choice = NULL;

	time(&now);

	for (i = 0; i < ups_count; ++i) {
		int priority = PRIORITY_SKIPPED;
		ups_device_t *ups = ups_list[i];
		double elapsed_ignore = (ups->force_ignore > 0) ? difftime(now, ups->force_ignore_time) : 0;
		double elapsed_force = (ups->force_primary > 0) ? difftime(now, ups->force_primary_time) : 0;

		if (ups->force_primary > 0 &&
			(ups->force_primary_time == 0 || elapsed_force > ups->force_primary)) {
			ups->force_primary = 0;
			ups->force_primary_time = 0;
		}
		else if (ups->force_primary == 0 && ups->force_primary_time > 0) {
			ups->force_primary_time = 0;
		}

		if (ups->force_ignore > 0 &&
			(ups->force_ignore_time == 0 || elapsed_ignore > ups->force_ignore)) {
			ups->force_ignore = 0;
			ups->force_ignore_time = 0;
		}
		else if (ups->force_ignore == 0 && ups->force_ignore_time > 0) {
			ups->force_ignore_time = 0;
		}

		if (ups_has_flag(ups, (ups_flags_t)(UPS_FLAG_ALIVE | UPS_FLAG_DUMPED))) {
			if (ups->force_ignore < 0) {
				ups->priority = PRIORITY_SKIPPED;

				upsdebugx(4, "%s: [%s]: is permanently ignored and was not considered",
					__func__, ups->socketname);

				continue;
			}
			else if (ups->force_ignore > 0 && elapsed_ignore <= ups->force_ignore) {
				ups->priority = PRIORITY_SKIPPED;

				upsdebugx(4, "%s: [%s]: is currently ignored and not considered (%.0fs of %ds)",
					__func__, ups->socketname, elapsed_ignore, ups->force_ignore);

				continue;
			}
			else if (ups->force_primary < 0) {
				priority = PRIORITY_FORCED;

				upsdebugx(4, "%s: [%s]: is permanently forced to highest priority",
					__func__, ups->socketname);
			}
			else if (ups->force_primary > 0 && elapsed_force <= ups->force_primary) {
				priority = PRIORITY_FORCED;

				upsdebugx(4, "%s: [%s]: is currently forced to highest priority (%.0fs of %ds)",
					__func__, ups->socketname, elapsed_force, ups->force_primary);
			}
			else if (ups_passes_status_filters(ups)) {
				priority = PRIORITY_USERFILTERS;
			}
			else if (arg_strict_filtering) {
				upsdebugx(4, "%s: [%s]: 'strict_filtering' is enabled, considering "
					"only status filters, but not the default set of lower priorities",
					__func__, ups->socketname);
			}
			else if (ups_has_flag(ups, (ups_flags_t)(UPS_FLAG_DATA_OK | UPS_FLAG_ONLINE))) {
				priority = PRIORITY_GOOD;
			}
			else if (ups_has_flag(ups, UPS_FLAG_DATA_OK)) {
				priority = PRIORITY_WEAK;
			}
			else {
				priority = PRIORITY_LASTRESORT;
			}
		}

		if (priority >= 0) {
			primaries++;

			if (priority < best_priority) {
				best_choice = ups;
				best_priority = priority;
			}

			upsdebugx(4, "%s: [%s]: is a primary candidate with priority [%d]",
				__func__, ups->socketname, priority);
		}

		ups->priority = priority;
	}

	ups_primary_count = primaries;

	return best_choice;
}

static int ups_passes_status_filters(const ups_device_t *ups)
{
	size_t i = 0;
	const char *status = NULL;

	if (!*ups->status) {
		return 0;
	}

	status = ups->status;

	if (arg_status_filters.have_any_count == 0 &&
		arg_status_filters.have_all_count == 0 &&
		arg_status_filters.nothave_any_count == 0 &&
		arg_status_filters.nothave_all_count == 0) {

		upsdebugx(4, "%s: [%s]: no status filters are set, disregarding filtering",
			__func__, ups->socketname);

		return 0;
	}

	for (i = 0; i < arg_status_filters.nothave_any_count; ++i) {
		if (str_contains_token(status, arg_status_filters.nothave_any[i])) {
			upsdebugx(4, "%s: [%s]: nothave_any: [%s] was found, excluded",
				__func__, ups->socketname, arg_status_filters.nothave_any[i]);

			return 0;
		}
	}

	for (i = 0; i < arg_status_filters.have_all_count; ++i) {
		if (!str_contains_token(status, arg_status_filters.have_all[i])) {
			upsdebugx(4, "%s: [%s]: have_all: [%s] not found, excluded",
				__func__, ups->socketname, arg_status_filters.have_all[i]);

			return 0;
		}
	}

	if (arg_status_filters.nothave_all_count > 0) {
		int all_found = 1;
		for (i = 0; i < arg_status_filters.nothave_all_count; ++i) {
			if (!str_contains_token(status, arg_status_filters.nothave_all[i])) {
				all_found = 0;
				break;
			}
		}
		if (all_found) {
			upsdebugx(4, "%s: [%s]: nothave_all: all were found, excluded",
				__func__, ups->socketname);

			return 0;
		}
	}

	if (arg_status_filters.have_any_count > 0) {
		int any_found = 0;
		for (i = 0; i < arg_status_filters.have_any_count; ++i) {
			if (str_contains_token(status, arg_status_filters.have_any[i])) {
				any_found = 1;
				break;
			}
		}
		if (!any_found) {
			upsdebugx(4, "%s: [%s]: have_any: none were found, excluded",
				__func__, ups->socketname);

			return 0;
		}
	}

	return 1;
}

/* Promote a UPS driver that is not NULL and not already the current primary */
static void ups_promote_primary(ups_device_t *ups)
{
	if (!ups || primary_ups == ups) {
		upslogx(LOG_WARNING, "%s: Unsupported function call, "
			"argument was either NULL or a UPS driver already declared as primary. "
			"Please notify the NUT developers (on GitHub) to check this driver's code.",
			__func__);

		return;
	}

	if (primary_ups) {
		ups_demote_primary(primary_ups);
	}

	primary_ups = ups;
	primary_ups->force_dstate_export = 1;

	ups_set_flag(ups, UPS_FLAG_PRIMARY);

	upslogx(LOG_NOTICE, "%s: [%s]: was promoted "
		"to primary with status [%s] and priority [%d]",
		__func__, primary_ups->socketname, primary_ups->status, primary_ups->priority);

	ups_export_dstate(primary_ups);
}

static void ups_demote_primary(ups_device_t *ups)
{
	last_primary_ups = ups;
	primary_ups = NULL;

	ups_clear_flag(last_primary_ups, UPS_FLAG_PRIMARY);

	upslogx(LOG_NOTICE, "%s: [%s]: is no longer "
		"primary with (last known) status [%s] and priority [%d]",
		__func__, last_primary_ups->socketname, last_primary_ups->status, last_primary_ups->priority);

	ups_clean_dstate(last_primary_ups);
}

static void ups_export_dstate(ups_device_t *ups)
{
	size_t i = 0;

	if (ups->force_dstate_export) {
		status_init();
		alarm_init();
	}

	for (i = 0; i < ups->cmd_count; ++i) {
		ups_cmd_t *cmd = ups->cmd_list[i];

		if (cmd->needs_export || ups->force_dstate_export) {
			dstate_addcmd(cmd->value);

			upsdebugx(5, "%s: [%s]: exported command to dstate: [%s]",
				__func__, ups->socketname, cmd->value);

			cmd->needs_export = 0;
		}
	}

	for (i = 0; i < ups->var_count; ++i) {
		ups_var_t *var = ups->var_list[i];

		if (var->needs_export || ups->force_dstate_export) {
			size_t j = 0;

			if (!strcmp(var->key, "ups.alarm")) {
				alarm_init();
				alarm_set(var->value);
				alarm_commit();
				status_commit(); /* publish ALARM */
				upsdebugx(5, "%s: [%s]: exported UPS alarm to dstate: [%s] : [%s]",
					__func__, ups->socketname, var->key, var->value);
			}
			else if (!strcmp(var->key, "ups.status")) {
				status_init();
				status_set(var->value);
				status_commit();
				upsdebugx(5, "%s: [%s]: exported UPS status to dstate: [%s] : [%s]",
					__func__, ups->socketname, var->key, var->value);
			}
			else {
				dstate_setinfo(var->key, "%s", var->value);
				upsdebugx(5, "%s: [%s]: exported variable to dstate: [%s] : [%s]",
					__func__, ups->socketname, var->key, var->value);
			}

			if (var->flags) {
				dstate_setflags(var->key, var->flags);
				upsdebugx(5, "%s: [%s]: exported variable flags to dstate: [%s] : [%d]",
					__func__, ups->socketname, var->key, var->flags);
			}

			if (var->aux) {
				dstate_setaux(var->key, var->aux);
				upsdebugx(5, "%s: [%s]: exported variable aux to dstate: [%s] : [%ld]",
					__func__, ups->socketname, var->key, var->aux);
			}

			for (j = 0; j < var->enum_count; ++j) {
				dstate_addenum(var->key, "%s", var->enum_list[j]);
				upsdebugx(5, "%s: [%s]: exported variable enum to dstate: [%s] : [%s]",
					__func__, ups->socketname, var->key, var->enum_list[j]);
			}

			for (j = 0; j < var->range_count; ++j) {
				dstate_addrange(var->key, var->range_list[j]->min, var->range_list[j]->max);
				upsdebugx(5, "%s: [%s]: exported variable range to dstate: [%s] : min=[%d] : max=[%d]",
					__func__, ups->socketname, var->key, var->range_list[j]->min, var->range_list[j]->max);
			}

			var->needs_export = 0;
		}
	}

	if (ups->force_dstate_export) {
		alarm_commit();
		status_commit();
	}

	ups->force_dstate_export = 0;
}

static void ups_clean_dstate(const ups_device_t *ups)
{
	size_t i = 0;

	status_init();
	alarm_init();

	for (i = 0; i < ups->cmd_count; ++i) {
		dstate_delcmd(ups->cmd_list[i]->value);
		upsdebugx(5, "%s: [%s]: removed command from dstate: [%s]",
			__func__, ups->socketname, ups->cmd_list[i]->value);
	}

	for (i = 0; i < ups->var_count; ++i) {
		dstate_delinfo(ups->var_list[i]->key);
		upsdebugx(5, "%s: [%s]: removed variable from dstate: [%s]",
			__func__, ups->socketname, ups->var_list[i]->key);
	}

	alarm_commit();
	status_commit();
}

static int ups_get_cmd_pos(const ups_device_t *ups, const char *cmd)
{
	size_t i = 0;

	for (i = 0; i < ups->cmd_count; ++i) {
		if (!strcmp(ups->cmd_list[i]->value, cmd)) {
			return i;
		}
	}

	return -1;
}

static int ups_add_cmd(ups_device_t *ups, const char *val)
{
	ups_cmd_t *new_cmd = NULL;

	if (ups_get_cmd_pos(ups, val) >= 0) {
		return 0;
	}

	if (ups->cmd_count >= ups->cmd_allocs) {
		ups->cmd_list = xrealloc(ups->cmd_list, sizeof(*ups->cmd_list) * (ups->cmd_allocs + CMD_ALLOC_BATCH));
		memset(ups->cmd_list + ups->cmd_allocs, 0, sizeof(*ups->cmd_list) * CMD_ALLOC_BATCH);
		ups->cmd_allocs = ups->cmd_allocs + CMD_ALLOC_BATCH;
	}

	new_cmd = xcalloc(1, sizeof(**ups->cmd_list));
	new_cmd->value = xstrdup(val);
	new_cmd->needs_export = 1;

	ups->cmd_list[ups->cmd_count] = new_cmd;
	ups->cmd_count++;

	upsdebugx(5, "%s: [%s]: added to ups->cmd_list: [%s]",
		__func__, ups->socketname, val);

	return 1;
}

static int ups_del_cmd(ups_device_t *ups, const char *val)
{
	int cmd_pos = ups_get_cmd_pos(ups, val);

	if (cmd_pos >= 0) {
		ups_cmd_t *cmd = ups->cmd_list[cmd_pos];
		size_t i = 0;

		if (primary_ups == ups) {
			dstate_delcmd(val);

			upsdebugx(5, "%s: [%s]: removed command from dstate: [%s]",
				__func__, ups->socketname, val);
		}

		free(cmd->value);
		free(cmd);

		for (i = cmd_pos; i < ups->cmd_count - 1; ++i) {
			ups->cmd_list[i] = ups->cmd_list[i + 1];
		}

		ups->cmd_list[ups->cmd_count - 1] = NULL;
		ups->cmd_count--;

		if (ups->cmd_count == 0) {
			free(ups->cmd_list);
			ups->cmd_list = NULL;
			ups->cmd_allocs = 0;
		}
		else if (ups->cmd_count % CMD_ALLOC_BATCH == 0) {
			ups->cmd_list = xrealloc(ups->cmd_list, sizeof(*ups->cmd_list) * ups->cmd_count);
			ups->cmd_allocs = ups->cmd_count;
		}

		upsdebugx(5, "%s: [%s]: removed from ups->cmd_list: [%s]",
			__func__, ups->socketname, val);

		return 1;
	}

	upsdebugx(6, "%s: [%s]: not found in ups->cmd_list: [%s]",
		__func__, ups->socketname, val);

	return 0;
}

static int ups_get_var_pos(const ups_device_t *ups, const char *key)
{
	size_t i = 0;

	for (i = 0; i < ups->var_count; ++i) {
		if (!strcmp(ups->var_list[i]->key, key)) {
			return i;
		}
	}

	return -1;
}

static int ups_set_var(ups_device_t *ups, const char *key, const char *value)
{
	ups_var_t *new_var = NULL;
	int var_pos = ups_get_var_pos(ups, key);

	if (var_pos >= 0) {
		ups_var_t *var = ups->var_list[var_pos];

		if (strcmp(var->value, value)) {
			free(var->value);
			var->value = xstrdup(value);
			var->needs_export = 1;

			upsdebugx(5, "%s: [%s]: updated in ups->var_list: [%s] : [%s]",
				__func__, ups->socketname, key, value);

			return 1;
		} else {
			upsdebugx(6, "%s: [%s]: unchanged in ups->var_list: [%s] : [%s]",
				__func__, ups->socketname, key, value);

			return 1;
		}
	}

	if (ups->var_count >= ups->var_allocs) {
		ups->var_list = xrealloc(ups->var_list, sizeof(*ups->var_list) * (ups->var_allocs + VAR_ALLOC_BATCH));
		memset(ups->var_list + ups->var_allocs, 0, sizeof(*ups->var_list) * VAR_ALLOC_BATCH);
		ups->var_allocs = ups->var_allocs + VAR_ALLOC_BATCH;
	}

	new_var = xcalloc(1, sizeof(**ups->var_list));
	new_var->key = xstrdup(key);
	new_var->value = xstrdup(value);
	new_var->needs_export = 1;

	ups->var_list[ups->var_count] = new_var;
	ups->var_count++;

	upsdebugx(5, "%s: [%s]: stored in ups->var_list: [%s] : [%s]",
		__func__, ups->socketname, key, value);

	return 1;
}

static int ups_del_var(ups_device_t *ups, const char *key)
{
	int var_pos = ups_get_var_pos(ups, key);

	if (var_pos >= 0) {
		ups_var_t *var = ups->var_list[var_pos];
		size_t i = 0;

		if (primary_ups == ups) {
			if (!strcmp(key, "ups.alarm")) {
				alarm_init();
				alarm_commit();
				status_commit(); /* clear ALARM */

				upsdebugx(5, "%s: [%s]: cleared UPS alarm from dstate: [%s]",
					__func__, ups->socketname, key);
			}
			if (!strcmp(key, "ups.status")) {
				status_init();
				status_commit(); /* clear STATUS */

				upsdebugx(5, "%s: [%s]: cleared UPS status from dstate: [%s]",
					__func__, ups->socketname, key);
			}

			dstate_delinfo(key);

			upsdebugx(5, "%s: [%s]: removed variable from dstate: [%s]",
				__func__, ups->socketname, key);
		}

		ups_free_var_state(var);
		free(var);

		for (i = var_pos; i < ups->var_count - 1; ++i) {
			ups->var_list[i] = ups->var_list[i + 1];
		}

		ups->var_list[ups->var_count - 1] = NULL;
		ups->var_count--;

		if (ups->var_count == 0) {
			free(ups->var_list);
			ups->var_list = NULL;
			ups->var_allocs = 0;
		}
		else if (ups->var_count % VAR_ALLOC_BATCH == 0) {
			ups->var_list = xrealloc(ups->var_list, sizeof(*ups->var_list) * ups->var_count);
			ups->var_allocs = ups->var_count;
		}

		upsdebugx(5, "%s: [%s]: removed from ups->var_list: [%s]",
			__func__, ups->socketname, key);

		return 1;
	}

	upsdebugx(6, "%s: [%s]: not found in ups->var_list: [%s]",
		__func__, ups->socketname, key);

	return 0;
}

static int ups_set_var_flags(ups_device_t *ups, const char *key, const int flags)
{
	int var_pos = ups_get_var_pos(ups, key);

	if (var_pos >= 0) {
		ups_var_t *var = ups->var_list[var_pos];

		if (var->flags == flags) {
			upsdebugx(6, "%s: [%s]: unchanged flags in ups->var_list: [%s] : [%d]",
				__func__, ups->socketname, key, flags);

			return 0;
		}

		var->flags = flags;
		var->needs_export = 1;

		upsdebugx(5, "%s: [%s]: stored flags in ups->var_list: [%s] : [%d]",
			__func__, ups->socketname, key, flags);

		return 1;
	}

	upsdebugx(6, "%s: [%s]: not found in ups->var_list: [%s]",
		__func__, ups->socketname, key);

	return 0;
}

static int ups_set_var_aux(ups_device_t *ups, const char *key, const long aux)
{
	int var_pos = ups_get_var_pos(ups, key);

	if (var_pos >= 0) {
		ups_var_t *var = ups->var_list[var_pos];

		if (var->aux == aux) {
			upsdebugx(6, "%s: [%s]: unchanged aux in ups->var_list: [%s] : [%ld]",
				__func__, ups->socketname, key, aux);

			return 0;
		}

		var->aux = aux;
		var->needs_export = 1;

		upsdebugx(5, "%s: [%s]: stored aux in ups->var_list: [%s] : [%ld]",
			__func__, ups->socketname, key, aux);

		return 1;
	}

	upsdebugx(6, "%s: [%s]: not found in ups->var_list: [%s]",
		__func__, ups->socketname, key);

	return 0;
}

static int ups_add_range(ups_device_t *ups, const char *key, const int min, const int max)
{
	int var_pos = ups_get_var_pos(ups, key);

	if (var_pos >= 0) {
		var_range_t *new_range = NULL;
		ups_var_t *var = ups->var_list[var_pos];
		size_t i = 0;

		for (i = 0; i < var->range_count; ++i) {
			if (var->range_list[i]->min == min && var->range_list[i]->max == max) {
				upsdebugx(6, "%s: [%s]: unchanged in ups->var_list->range_list: [%s] : min=[%d] : max=[%d]",
					__func__, ups->socketname, key, min, max);

				return 0;
			}
		}

		if (var->range_count >= var->range_allocs) {
			var->range_list = xrealloc(var->range_list, sizeof(*var->range_list) * (var->range_allocs + SUBVAR_ALLOC_BATCH));
			memset(var->range_list + var->range_allocs, 0, sizeof(*var->range_list) * SUBVAR_ALLOC_BATCH);
			var->range_allocs = var->range_allocs + SUBVAR_ALLOC_BATCH;
		}

		new_range = xcalloc(1, sizeof(**var->range_list));
		new_range->min = min;
		new_range->max = max;

		var->range_list[var->range_count] = new_range;
		var->range_count++;
		var->needs_export = 1;

		upsdebugx(5, "%s: [%s]: added to ups->var_list->range_list: [%s] : min=[%d] : max=[%d]",
			__func__, ups->socketname, key, min, max);

		return 1;
	}

	upsdebugx(6, "%s: [%s]: not found in ups->var_list: [%s]",
		__func__, ups->socketname, key);

	return 0;
}

static int ups_del_range(ups_device_t *ups, const char *key, const int min, const int max)
{
	int var_pos = ups_get_var_pos(ups, key);

	if (var_pos >= 0) {
		ups_var_t *var = ups->var_list[var_pos];
		size_t i = 0;

		for (i = 0; i < var->range_count; ++i) {
			if (var->range_list[i]->min == min && var->range_list[i]->max == max) {
				size_t j = 0;

				if (primary_ups == ups) {
					dstate_delrange(key, min, max);

					upsdebugx(5, "%s: [%s]: removed range from dstate: [%s] : min=[%d] : max=[%d]",
						__func__, ups->socketname, key, min, max);
				}

				free(var->range_list[i]);

				for (j = i; j < var->range_count - 1; ++j) {
					var->range_list[j] = var->range_list[j + 1];
				}

				var->range_list[var->range_count - 1] = NULL;
				var->range_count--;

				if (var->range_count == 0) {
					free(var->range_list);
					var->range_list = NULL;
					var->range_allocs = 0;
				}
				else if (var->range_count % SUBVAR_ALLOC_BATCH == 0) {
					var->range_list = xrealloc(var->range_list, sizeof(*var->range_list) * var->range_count);
					var->range_allocs = var->range_count;
				}

				upsdebugx(5, "%s: [%s]: deleted from ups->var_list->range_list: [%s] : min=[%d] : max=[%d]",
					__func__, ups->socketname, key, min, max);

				return 1;
			}
		}
	}

	upsdebugx(6, "%s: [%s]: not found in ups->var_list: [%s]",
		__func__, ups->socketname, key);

	return 0;
}

static int ups_add_enum(ups_device_t *ups, const char *key, const char *val)
{
	int var_pos = ups_get_var_pos(ups, key);

	if (var_pos >= 0) {
		ups_var_t *var = ups->var_list[var_pos];
		size_t i = 0;

		for (i = 0; i < var->enum_count; ++i) {
			if (!strcmp(var->enum_list[i], val)) {
				return 0;
			}
		}

		if (var->enum_count >= var->enum_allocs) {
			var->enum_list = xrealloc(var->enum_list, sizeof(*var->enum_list) * (var->enum_allocs + SUBVAR_ALLOC_BATCH));
			memset(var->enum_list + var->enum_allocs, 0, sizeof(*var->enum_list) * SUBVAR_ALLOC_BATCH);
			var->enum_allocs = var->enum_allocs + SUBVAR_ALLOC_BATCH;
		}

		var->enum_list[var->enum_count] = xstrdup(val);

		var->enum_count++;
		var->needs_export = 1;

		upsdebugx(5, "%s: [%s]: added to ups->var_list->enum_list: [%s] : [%s]",
			__func__, ups->socketname, key, val);

		return 1;
	}

	upsdebugx(6, "%s: [%s]: not found in ups->var_list: [%s]",
		__func__, ups->socketname, key);

	return 0;
}

static int ups_del_enum(ups_device_t *ups, const char *key, const char *val)
{
	int var_pos = ups_get_var_pos(ups, key);

	if (var_pos >= 0) {
		ups_var_t *var = ups->var_list[var_pos];
		size_t i = 0;

		for (i = 0; i < var->enum_count; ++i) {
			if (!strcmp(var->enum_list[i], val)) {
				size_t j = 0;

				if (primary_ups == ups) {
					dstate_delenum(key, val);

					upsdebugx(5, "%s: [%s]: removed enum from dstate: [%s] : [%s]",
						__func__, ups->socketname, key, val);
				}

				free(var->enum_list[i]);

				for (j = i; j < var->enum_count - 1; ++j) {
					var->enum_list[j] = var->enum_list[j + 1];
				}

				var->enum_list[var->enum_count - 1] = NULL;
				var->enum_count--;

				if (var->enum_count == 0) {
					free(var->enum_list);
					var->enum_list = NULL;
					var->enum_allocs = 0;
				}
				else if (var->enum_count % SUBVAR_ALLOC_BATCH == 0) {
					var->enum_list = xrealloc(var->enum_list, sizeof(*var->enum_list) * var->enum_count);
					var->enum_allocs = var->enum_count;
				}

				upsdebugx(5, "%s: [%s]: deleted from ups->var_list->enum_list: [%s] : [%s]",
					__func__, ups->socketname, key, val);


				return 1;
			}
		}
	}

	upsdebugx(6, "%s: [%s]: not found in ups->var_list: [%s]",
		__func__, ups->socketname, key);

	return 0;
}

static void free_status_filters(void)
{
	size_t i = 0;

	if (arg_status_filters.have_any) {
		for (i = 0; i < arg_status_filters.have_any_count; ++i) {
			free(arg_status_filters.have_any[i]);
			arg_status_filters.have_any[i] = NULL;
		}
		free(arg_status_filters.have_any);
		arg_status_filters.have_any = NULL;
		arg_status_filters.have_any_count = 0;
	}

	if (arg_status_filters.have_all) {
		for (i = 0; i < arg_status_filters.have_all_count; ++i) {
			free(arg_status_filters.have_all[i]);
			arg_status_filters.have_all[i] = NULL;
		}
		free(arg_status_filters.have_all);
		arg_status_filters.have_all = NULL;
		arg_status_filters.have_all_count = 0;
	}

	if (arg_status_filters.nothave_any) {
		for (i = 0; i < arg_status_filters.nothave_any_count; ++i) {
			free(arg_status_filters.nothave_any[i]);
			arg_status_filters.nothave_any[i] = NULL;
		}
		free(arg_status_filters.nothave_any);
		arg_status_filters.nothave_any = NULL;
		arg_status_filters.nothave_any_count = 0;
	}

	if (arg_status_filters.nothave_all) {
		for (i = 0; i < arg_status_filters.nothave_all_count; ++i) {
			free(arg_status_filters.nothave_all[i]);
			arg_status_filters.nothave_all[i] = NULL;
		}
		free(arg_status_filters.nothave_all);
		arg_status_filters.nothave_all = NULL;
		arg_status_filters.nothave_all_count = 0;
	}
}

static void ups_free_ups_state(ups_device_t *ups)
{
	size_t i = 0;

	if (ups->var_list) {
		for (i = 0; i < ups->var_count; ++i) {
			if (ups->var_list[i]) {
				ups_free_var_state(ups->var_list[i]);
				free(ups->var_list[i]);
				ups->var_list[i] = NULL;
			}
		}

		free(ups->var_list);
		ups->var_list = NULL;
		ups->var_count = 0;
		ups->var_allocs = 0;
	}

	if (ups->cmd_list) {
		for (i = 0; i < ups->cmd_count; ++i) {
			if (ups->cmd_list[i]) {
				if (ups->cmd_list[i]->value) {
					free(ups->cmd_list[i]->value);
					ups->cmd_list[i]->value = NULL;
				}
				free(ups->cmd_list[i]);
				ups->cmd_list[i] = NULL;
			}
		}

		free(ups->cmd_list);
		ups->cmd_list = NULL;
		ups->cmd_count = 0;
		ups->cmd_allocs = 0;
	}

	if (ups->status) {
		free(ups->status);
		ups->status = NULL;
	}
}

static void ups_free_var_state(ups_var_t *var)
{
	size_t i = 0;

	if (var->key) {
		free(var->key);
		var->key = NULL;
	}

	if (var->value) {
		free(var->value);
		var->value = NULL;
	}

	if (var->enum_list) {
		for (i = 0; i < var->enum_count; ++i) {
			if (var->enum_list[i]) {
				free(var->enum_list[i]);
				var->enum_list[i] = NULL;
			}
		}
		free(var->enum_list);
		var->enum_list = NULL;
		var->enum_count = 0;
		var->enum_allocs = 0;
	}

	if (var->range_list) {
		for (i = 0; i < var->range_count; ++i) {
			if (var->range_list[i]) {
				free(var->range_list[i]);
				var->range_list[i] = NULL;
			}
		}
		free(var->range_list);
		var->range_list = NULL;
		var->range_count = 0;
		var->range_allocs = 0;
	}
}

static const char *rewrite_driver_prefix(const char *in, char *out, size_t outlen)
{
	int required = -1;

	if (!strncmp(in, "driver.", 7)) {
		required = snprintf(out, outlen, "upstream.%s", in);

		if ((size_t)required >= outlen) {
			upslogx(LOG_WARNING, "%s: truncated variable name size "
				"[%" PRIuSIZE "] exceeds buffer of size [%" PRIuSIZE "]: %s",
				__func__, (size_t)required, outlen, out);
		}

		return out;
	}

	return in;
}

static int split_socket_name(const char *input, char **driver, char **ups)
{
	size_t drv_len = 0;
	size_t ups_len = 0;
	const char *last_dash = strrchr(input, '-');

	if (!input || !last_dash || last_dash == input || *(last_dash + 1) == '\0') {
		*driver = NULL;
		*ups = NULL;

		return 0;
	}

	drv_len = last_dash - input;
	ups_len = strlen(last_dash + 1);

	*driver = xmalloc(drv_len + 1);
	*ups = xmalloc(ups_len + 1);

	snprintf(*driver, (drv_len + 1), "%.*s", (int)drv_len, input);
	snprintf(*ups, (ups_len + 1), "%s", last_dash + 1);

	str_trim_space(*driver);
	str_trim_space(*ups);

	return 1;
}

static void csv_arg_to_array(const char *argname, const char *argcsv, char ***array, size_t *countvar)
{
	char *tmp = NULL;
	char *token = NULL;
	char *str = NULL;

	if (!argcsv) {
		*array = NULL;
		*countvar = 0;

		return;
	}

	tmp = xstrdup(argcsv);

	token = strtok(tmp, ",");
	while (token) {
		str_trim_space(token);

		str = xstrdup(token);

		*array = xrealloc(*array, sizeof(**array) * (*countvar + 1));
		(*array)[*countvar] = str;
		(*countvar)++;

		upsdebugx(1, "%s: added [%s] to [%s] from configuration",
			__func__, str, argname);

		token = strtok(NULL, ",");
	}

	free(tmp);
	tmp = NULL;
}

static inline void ups_set_flag(ups_device_t *ups, ups_flags_t flag)
{
	ups->flags |= flag;
}

static inline void ups_clear_flag(ups_device_t *ups, ups_flags_t flag)
{
	ups->flags &= ~flag;
}

static inline int ups_has_flag(const ups_device_t *ups, ups_flags_t flags)
{
	return (ups->flags & flags) == flags;
}
