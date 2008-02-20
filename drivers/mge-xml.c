/* mge-xml.c		Model specific routines for MGE XML protocol UPSes 

   Copyright (C)
	2008		Arjen de Korte <adkorte-guest@alioth.debian.org>

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
#include <stdlib.h>
#include <string.h>

#include <ne_xml.h>

#include "common.h"
#include "dstate.h"

#include "netxml-ups.h"
#include "mge-xml.h"

#define MGE_XML_VERSION		"MGEXML/0.10"

static char	mge_xml_initups[] = "/";
static char	mge_xml_initinfo[] = "/mgeups/product.xml";
static char	mge_xml_updateinfo[32] = "";

static char	var[128];
static char	val[128];

typedef  enum {
	ROOTPARENT = NE_XML_STATEROOT,

	_UNEXPECTED,
	_PARSEERROR,

	PRODUCT_INFO = 100,	/* "/mgeups/product.xml" */

		PI_SUMMARY,
			PI_HTML_PROPERTIES_PAGE,
			PI_XML_SUMMARY_PAGE,
			PI_CENTRAL_CFG,
			PI_CSV_LOGS,
		/* /PI_SUMMARY */

		PI_ALARMS,
			PI_SUBSCRIPTION,
		/* /ALARMS */

		PI_MANAGEMENT,
			PI_MANAGEMENT_PAGE,
			PI_XML_MANAGEMENT_PAGE,
		/* /MANAGEMENT */

		PI_UPS_DATA,
			PI_GET_OBJECT,
			PI_SET_OBJECT,
		/* /UPS_DATA */

	/* /PRODUCT_INFO */

	SUMMARY = 200,		/* "/upsprop.xml" */
		SU_OBJECT,
	/* /SUMMARY */

	GET_OBJECT = 300,	/* "/getvalue.cgi" */
		GO_OBJECT,
	/* /GET_OBJECT */

	SET_OBJECT = 400,	/* "/setvalue.cgi" */
		SO_OBJECT
	/* /SET_OBJECT */

} mge_xml_state_t;

/* A start-element callback for element with given namespace/name. */
static int mge_xml_startelm_cb(void *userdata, int parent, const char *nspace, const char *name, const char **atts)
{
	int	state = _UNEXPECTED;

	switch(parent)
	{
	case ROOTPARENT:
		if (!strcasecmp(name, "PRODUCT_INFO")) {
			/* name="Network Management Card" type="Mosaic M" version="BA" */
			int	i;
			for (i = 0; atts[i] && atts[i+1]; i += 2) {
				if (i == 0) {
					snprintf(val, sizeof(val), "%s", atts[i+1]);
				} else {
					snprintf(val + strlen(val), sizeof(val) - strlen(val), "/%s", atts[i+1]);
				}
			}
			state = PRODUCT_INFO;
			break;
		}
		if (!strcasecmp(name, "SUMMARY")) {
			state = SUMMARY;
			break;
		}
		if (!strcasecmp(name, "GET_OBJECT")) {
			state = GET_OBJECT;
			break;
		}
		if (!strcasecmp(name, "SET_OBJECT")) {
			state = SET_OBJECT;
			break;
		}
		break;

	case PRODUCT_INFO:
		if (!strcasecmp(name, "SUMMARY")) {
			state = PI_SUMMARY;
			break;
		}

		if (!strcasecmp(name, "ALARMS")) {
			state = PI_ALARMS;
			break;
		}

		if (!strcasecmp(name, "MANAGEMENT")) {
			state = PI_MANAGEMENT;
			break;
		}

		if (!strcasecmp(name, "UPS_DATA")) {
			state = PI_UPS_DATA;
			break;
		}
		break;

	case PI_SUMMARY:
		if (!strcasecmp(name, "HTML_PROPERTIES_PAGE")) {
			/* url="mgeups/default.htm" */
			state = PI_HTML_PROPERTIES_PAGE;
			break;
		}
		if (!strcasecmp(name, "XML_SUMMARY_PAGE")) {
			/* url="upsprop.xml" */
			int	i;
			for (i = 0; atts[i] && atts[i+1]; i += 2) {
				if (!strcasecmp(atts[i], "url")) {
					snprintf(mge_xml_updateinfo, sizeof(mge_xml_updateinfo), "/%s", atts[i+1]);
				}
			}
			state = PI_XML_SUMMARY_PAGE;
			break;
		}
		if (!strcasecmp(name, "CENTRAL_CFG")) {
			/* url="config.xml" */
			state = PI_CENTRAL_CFG;
			break;
		}
		if (!strcasecmp(name, "CSV_LOGS")) {
			/* url="logevent.csv" dateRange="no" eventFiltering="no" */
			state = PI_CSV_LOGS;
			break;
		}
		break;

	case PI_ALARMS:
		if (!strcasecmp(name, "SUBSCRIPTION")) {
			/* url="subscribe.cgi" security="basic" */
			state = PI_SUBSCRIPTION;
			break;
		}
		break;

	case PI_MANAGEMENT:
		if (!strcasecmp(name, "MANAGEMENT_PAGE")) {
			/* name="Manager list" id="ManagerList" url="FS/FLASH0/TrapReceiverList.cfg" security="none" */
			/* name="Shutdown criteria settings" id="Shutdown" url="FS/FLASH0/ShutdownParameters.cfg" security="none" */
			/* name="Network settings" id="Network" url="FS/FLASH0/NetworkSettings.cfg" security="none" */
			/* name="Centralized configuration settings" id="ClientCfg" url="FS/FLASH0/CentralizedConfig.cfg" security="none" */
			state = PI_MANAGEMENT_PAGE;
			break;
		}
		if (!strcasecmp(name, "XML_MANAGEMENT_PAGE")) {
			/* name="Set Card Time" id="SetTime" url="management/set_time.xml" security="none" */
			state = PI_XML_MANAGEMENT_PAGE;
			break;
		}
		break;

	case PI_UPS_DATA:
		if (!strcasecmp(name, "GET_OBJECT")) {
			/* url="getvalue.cgi" security="none" */
			state = PI_GET_OBJECT;
			break;
		}
		if (!strcasecmp(name, "SET_OBJECT")) {
			/* url="setvalue.cgi" security="ssl" */
			state = PI_SET_OBJECT;
			break;
		}
		break;

	case SUMMARY:
		if (!strcasecmp(name, "OBJECT")) {
			/* name="UPS.PowerSummary.iProduct" */
			int	i;
			for (i = 0; atts[i] && atts[i+1]; i += 2) {
				if (!strcasecmp(atts[i], "name")) {
					snprintf(var, sizeof(var), "%s", atts[i+1]);
				}
			}
			state = SU_OBJECT;
			break;
		}
		break;
			
	case GET_OBJECT:
		if (!strcasecmp(name, "OBJECT")) {
			/* name="System.RunTimeToEmptyLimit" unit="s" access="RW" */
			int	i;
			for (i = 0; atts[i] && atts[i+1]; i += 2) {
				if (!strcasecmp(atts[i], "name")) {
					snprintf(var, sizeof(var), "%s", atts[i+1]);
				}
				if (!strcasecmp(atts[i], "access")) {
					/* do something with RO/RW access? */
				}
			}
			state = GO_OBJECT;
			break;
		}
		break;
	}

	upsdebugx(3, "%s: name <%s> (parent = %d, state = %d)\n", __func__, name, parent, state);
	return state;
}

/* Character data callback; may return non-zero to abort the parse. */
static int mge_xml_cdata_cb(void *userdata, int state, const char *cdata, size_t len)
{
	/* skip empty lines */
	if ((len == 1) && (cdata[0] == '\n')) {
		upsdebugx(3, "%s: cdata ignored (state = %d)\n", __func__, state);
		return 0;
	}

	switch(state)
	{
	case SU_OBJECT:
	case GO_OBJECT:
		snprintf(val, sizeof(val), "%.*s", len, cdata);
		break;
	}

	upsdebugx(3, "%s: cdata [%.*s] (state = %d)\n", __func__, len, cdata, state);
	return 0;
}

/* End element callback; may return non-zero to abort the parse. */
static int mge_xml_endelm_cb(void *userdata, int state, const char *nspace, const char *name)
{
	switch(state)
	{
	case PRODUCT_INFO:
		dstate_setinfo("ups.firmware.aux", val);
		break;

	case SU_OBJECT:
	case GO_OBJECT:
		upsdebugx(2, "dstate_setinfo(\"%s\", \"%s\");\n", var, val);
		break;
	}
	
	upsdebugx(3, "%s: name </%s> (state = %d)\n", __func__, name, state);
	return 0;
}

subdriver_t mge_xml_subdriver = {
	MGE_XML_VERSION,
	mge_xml_initups,
	mge_xml_initinfo,
	mge_xml_updateinfo,
	mge_xml_startelm_cb,
	mge_xml_cdata_cb,
	mge_xml_endelm_cb,
};
