/* powerman-pdu.c - Powerman PDU client driver
 *
 * Copyright (C) 2008 Arnaud Quette <aquette.dev@gmail.com>
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
 *
 */

#include "main.h"

#include <libpowerman.h>

#define DRIVER_NAME	"Powerman PDU client driver"
#define DRIVER_VERSION	"0.11"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Arnaud Quette <aquette.dev@gmail.com>",
	DRV_EXPERIMENTAL,
	{ NULL }
};

/* Powerman functions and variables */
static pm_err_t query_one(pm_handle_t pm, char *s, int mode);
static pm_err_t query_all(pm_handle_t pm, int mode);

pm_handle_t pm;
char ebuf[64];

/* modes to snmp_ups_walk. */
#define WALKMODE_INIT	0
#define WALKMODE_UPDATE	1

static int reconnect_ups(void);

static int instcmd(const char *cmdname, const char *extra)
{
	pm_err_t rv = -1;
	char *cmdsuffix = NULL;
	char *cmdindex = NULL;
	char outletname[SMALLBUF];

	upsdebugx(1, "entering instcmd (%s)", cmdname);

	/* only consider the end of the command */
	if ( (cmdsuffix = strrchr(cmdname, '.')) == NULL )
		return STAT_INSTCMD_UNKNOWN;
	else
		cmdsuffix++;

	/* get the outlet name */
	if ( (cmdindex = strchr(cmdname, '.')) == NULL )
		return STAT_INSTCMD_UNKNOWN;
	else {
		char	buf[32];
		cmdindex++;
		snprintf(buf, sizeof(buf), "outlet.%i.desc", atoi(cmdindex));
		snprintf(outletname, sizeof(outletname), "%s", dstate_getinfo(buf));
	}

	/* Power on the outlet */
	if (!strcasecmp(cmdsuffix, "on")) {
		rv = pm_node_on(pm, outletname);
		return (rv==PM_ESUCCESS)?STAT_INSTCMD_HANDLED:STAT_SET_INVALID;
	}

	/* Power off the outlet */
	if (!strcasecmp(cmdsuffix, "off")) {
		rv = pm_node_off(pm, outletname);
		return (rv==PM_ESUCCESS)?STAT_INSTCMD_HANDLED:STAT_SET_INVALID;
	}

	/* Cycle the outlet */
	if (!strcasecmp(cmdsuffix, "cycle")) {
		rv = pm_node_cycle(pm, outletname);
		return (rv==PM_ESUCCESS)?STAT_INSTCMD_HANDLED:STAT_SET_INVALID;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s] [%s]", cmdname, extra);
	return STAT_INSTCMD_UNKNOWN;
}

void upsdrv_updateinfo(void)
{
	pm_err_t rv = PM_ESUCCESS;

	if ( (rv = query_all(pm, WALKMODE_UPDATE)) != PM_ESUCCESS) {
		upslogx(2, "Error: %s (%i)\n", pm_strerror(rv, ebuf, sizeof(ebuf)), errno);
		/* FIXME: try to reconnect?
		 *	dstate_datastale();
		 */
		reconnect_ups();
	}
}

void upsdrv_initinfo(void)
{
	pm_err_t rv = PM_ESUCCESS;

	/* try to detect the PDU here - call fatal_with_errno(EXIT_FAILURE, ) if it fails */

	/* FIXME: can we report something useful? */
	dstate_setinfo("ups.mfr", "Powerman");
	dstate_setinfo("device.mfr", "Powerman");
	dstate_setinfo("ups.model", "unknown PDU");
	dstate_setinfo("device.model", "unknown PDU");
	dstate_setinfo("device.type", "pdu");

	/* Now walk the data tree */
	if ( (rv = query_all(pm, WALKMODE_INIT)) != PM_ESUCCESS) {
		upslogx(2, "Error: %s\n", pm_strerror(rv, ebuf, sizeof(ebuf)));
		/* FIXME: try to reconnect?
		 *	dstate_datastale();
		 */
		reconnect_ups();

	}
	upsh.instcmd = instcmd;
	/* FIXME: no need for setvar (ex for outlet.n.delay.*)!? */
}

void upsdrv_shutdown(void)
{
	/* FIXME: shutdown all outlets? */
	fatalx(EXIT_FAILURE, "shutdown not supported");

	/* OL: this must power cycle the load if possible */
	/* OB: the load must remain off until the power returns */
}

/*
static int setvar(const char *varname, const char *val)
{
	if (!strcasecmp(varname, "outlet.n.delay.*")) {
		...
		return STAT_SET_HANDLED;
	}

	upslogx(LOG_NOTICE, "setvar: unknown variable [%s]", varname);
	return STAT_SET_UNKNOWN;
}
*/

void upsdrv_help(void)
{
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	/* FIXME: anything useful to be put here? */
}

void upsdrv_initups(void)
{
	pm_err_t rv = PM_ESUCCESS;

	/* Connect to the PowerMan daemon */
	if ((rv = pm_connect(device_path, NULL, &pm, 0)) != PM_ESUCCESS) {
		fatalx(EXIT_FAILURE, "Can't connect to %s: %s\n", device_path,
			pm_strerror(rv, ebuf, sizeof(ebuf)));
	}

	/* FIXME: suitable?
	 * poll_interval = 30; */
}

void upsdrv_cleanup(void)
{
	pm_disconnect(pm);
}

static int reconnect_ups(void)
{
	pm_err_t rv;

	upsdebugx(4, "===================================================");
	upsdebugx(4, "= connection lost with Powerman, try to reconnect =");
	upsdebugx(4, "===================================================");

	/* clear the situation */
	pm_disconnect(pm);

	/* Connect to the PowerMan daemon */
	if ((rv = pm_connect(device_path, NULL, &pm, 0)) != PM_ESUCCESS)
		return 0;
	else {
		upsdebugx(4, "connection restored with Powerman");
		return 1;
	}
}

/*
 * powerman support functions
 ****************************/

static pm_err_t query_one(pm_handle_t pm, char *s, int outletnum)
{
	pm_err_t rv;
	pm_node_state_t ns;
	char outlet_prop[64];

	upsdebugx(1, "entering query_one (%s)", s);

	rv = pm_node_status(pm, s, &ns);
	if (rv == PM_ESUCCESS) {

		upsdebugx(3, "updating status");

		snprintf(outlet_prop, sizeof(outlet_prop), "outlet.%i.status", outletnum);
		dstate_setinfo(outlet_prop, "%s", ns == PM_ON ? "on" :
						ns == PM_OFF ? "off" : "unknown");
		dstate_dataok();
	}
	return rv;
}

static pm_err_t query_all(pm_handle_t pm, int mode)
{
	pm_err_t rv;
	pm_node_iterator_t itr;
	char outlet_prop[64];
	char *s;
	int outletnum = 1;

	upsdebugx(1, "entering query_all ()");

	rv = pm_node_iterator_create(pm, &itr);
	if (rv != PM_ESUCCESS)
		return rv;

	while ((s = pm_node_next(itr))) {

		/* in WALKMODE_UPDATE, we always call this one for the
		 * status update... */
		if ((rv = query_one(pm, s, outletnum)) != PM_ESUCCESS)
			break;
		else  {
			/* set the initial generic properties (ie except status)
			 * but only if the status query succeeded */
			if (mode == WALKMODE_INIT) {
				snprintf(outlet_prop, sizeof(outlet_prop), "outlet.%i.id", outletnum);
				dstate_setinfo(outlet_prop, "%i", outletnum);

				snprintf(outlet_prop, sizeof(outlet_prop), "outlet.%i.desc", outletnum);
				dstate_setinfo(outlet_prop, "%s", s);

				/* we assume it's always true! */
				snprintf(outlet_prop, sizeof(outlet_prop), "outlet.%i.switchable", outletnum);
				dstate_setinfo(outlet_prop, "yes");

				/* add instant commands */
				snprintf(outlet_prop, sizeof(outlet_prop), "outlet.%i.load.on", outletnum);
				dstate_addcmd(outlet_prop);
				snprintf(outlet_prop, sizeof(outlet_prop), "outlet.%i.load.off", outletnum);
				dstate_addcmd(outlet_prop);
				snprintf(outlet_prop, sizeof(outlet_prop), "outlet.%i.load.cycle", outletnum);
				dstate_addcmd(outlet_prop);
			}
		}
		outletnum++;
	}
	pm_node_iterator_destroy(itr);
	return rv;
}
