/* mge-utalk.c - monitor MGE UPS for NUT with UTalk protocol
 *
 *  Copyright (C) 2002 - 2005
 *     Arnaud Quette <arnaud.quette@gmail.com>
 *     Hans Ekkehard Plesser <hans.plesser@itf.nlh.no>
 *     Martin Loyer <martin@degraaf.fr>
 *     Patrick Agrain <patrick.agrain@alcatel.fr>
 *     Nicholas Reilly <nreilly@magma.ca>
 *     Dave Abbott <d.abbott@dcs.shef.ac.uk>
 *     Marek Kralewski <marek@mercy49.de>
 *
 *  This driver is a collaborative effort by the above people,
 *  Sponsored by MGE UPS SYSTEMS
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
 *
 */

/*
 * IMPLEMENTATION DETAILS
 *
 * Not all UTalk models provide all possible information, settings and commands.
 * mge-utalk checks on startup which variables and commands are available from
 * the UPS, and re-reads these regularly. Thus, startup is a bit slow, but this
 * should not matter much.
 *
 * mge-utalk.h defines a struct array that tells the driver how to read
 * variables from the UPS and publish them as NUT data.
 *
 * "ups.status" variable is not included in this array, since it contains
 * information that requires several calls to the UPS and more advanced analysis
 * of the reponses. The function get_ups_status does this job.
 *
 * Note that MGE enumerates the status "bits" from right to left,
 * i.e., if buf[] contains the reponse to command "Ss" (read system status),
 * then buf[0] contains "bit" Ss.1.7 (General alarm), while buf[7] contains
 * "bit" Ss.1.0 (Load unprotected).
 *
 * enable_ups_comm() is called before each attempt to read/write data
 * from/to the UPS to re synchronise the communication.
 */

#include <ctype.h>
#include <sys/ioctl.h>
#include "timehead.h"
#include "main.h"
#include "serial.h"
#include "mge-utalk.h"

/* --------------------------------------------------------------- */
/*                  Define "technical" constants                   */
/* --------------------------------------------------------------- */

#define DRIVER_NAME	"MGE UPS SYSTEMS/U-Talk driver"
#define DRIVER_VERSION	"0.93"


/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Arnaud Quette <ArnaudQuette@gmail.com>\n" \
	"Hans Ekkehard Plesser <hans.plesser@itf.nlh.no>\n" \
	"Martin Loyer <martin@degraaf.fr>\n" \
	"Patrick Agrain <patrick.agrain@alcatel.fr>\n" \
	"Nicholas Reilly <nreilly@magma.ca>\n" \
	"Dave Abbott <d.abbott@dcs.shef.ac.uk>\n" \
	"Marek Kralewski <marek@mercy49.de>",
	DRV_STABLE,
	{ NULL }
};

/* delay after sending each char to UPS (in MICROSECONDS) */
#define MGE_CHAR_DELAY 0

/* delay after command, before reading UPS reply (in MICROSECONDS) */
#define MGE_REPLY_DELAY 1000

/* delay after enable_ups_comm    */
#define MGE_CONNECT_DELAY 500000

#define MGE_COMMAND_ENDCHAR "\r\n"   /* some UPS need \r and \n */
#define MGE_REPLY_ENDCHAR   '\r'
#define MGE_REPLY_IGNCHAR   "\r\n"

#define MAXTRIES    10 /* max number of connect tries              */
#define BUFFLEN    256

#define SD_RETURN	0
#define SD_STAYOFF	1

static int sdtype = SD_RETURN;
static time_t lastpoll; /* Timestamp the last polling */

/* --------------------------------------------------------------- */
/*             Structure with information about UPS                */
/* --------------------------------------------------------------- */

static struct {
	int MultTab;
	int LowBatt;  /* percent */
	int OnDelay;  /* minutes */
	int OffDelay; /* seconds */
} mge_ups = { 0, DEFAULT_LOWBATT, DEFAULT_ONDELAY, DEFAULT_OFFDELAY };


/* --------------------------------------------------------------- */
/*             Declaration of internal functions                   */
/* --------------------------------------------------------------- */

static int instcmd(const char *cmdname, const char *extra);
static int setvar(const char *varname, const char *val);
static void enable_ups_comm(void);
static void disable_ups_comm(void);
static void extract_info(const char *buf, const mge_info_item_t *mge,
			 char *infostr, int infolen);
static const char *info_variable_cmd(const char *type);
static bool_t info_variable_ok(const char *type);
static int  get_ups_status(void);
static int mge_command(char *reply, int replylen, const char *fmt, ...);

/* --------------------------------------------------------------- */
/*                    UPS Driver Functions                         */
/* --------------------------------------------------------------- */

void upsdrv_makevartable(void)
{
	char temp[BUFFLEN];

	snprintf(temp, sizeof(temp),
		"Low battery level, in %%              (default = %3d)",
		DEFAULT_LOWBATT);
	addvar (VAR_VALUE, "LowBatt", temp);

	snprintf(temp, sizeof(temp),
		"Delay before startup, in minutes     (default = %3d)",
		DEFAULT_ONDELAY);
	addvar (VAR_VALUE, "OnDelay", temp);

	snprintf(temp, sizeof(temp),
		"Delay before shutdown, in seconds    (default = %3d)",
		DEFAULT_OFFDELAY);
	addvar (VAR_VALUE, "OffDelay", temp);

	addvar(VAR_FLAG, "oldmac", "Enable Oldworld Apple Macintosh support");
}

/* --------------------------------------------------------------- */

void upsdrv_initups(void)
{
	char buf[BUFFLEN];
	int RTS = TIOCM_RTS;

	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B2400);

	/* read command line/conf variable that affect comm. */
	if (testvar ("oldmac"))
		RTS = ~TIOCM_RTS;

	/* Init serial line */
	ioctl(upsfd, TIOCMBIC, &RTS);
	enable_ups_comm();

	/* Try to set "Low Battery Level" (if supported and given) */
	if (getval ("lowbatt"))
	{
		mge_ups.LowBatt = atoi (getval ("lowbatt"));
		/* Set the value in the UPS */
		mge_command(buf, sizeof(buf), "Bl %d",  mge_ups.LowBatt);
		if(!strcmp(buf, "OK"))
			upsdebugx(1, "Low Battery Level set to %d%%", mge_ups.LowBatt);
		else
			upsdebugx(1, "initups: Low Battery Level cannot be set");
	}

        /* Try to set "ON delay" (if supported and given) */
	if (getval ("ondelay"))
	{
		mge_ups.OnDelay = atoi (getval ("ondelay"));
		/* Set the value in the UPS */
		mge_command(buf, sizeof(buf), "Sm %d",  mge_ups.OnDelay);
		if(!strcmp(buf, "OK"))
			upsdebugx(1, "ON delay set to %d min", mge_ups.OnDelay);
		else
			upsdebugx(1, "initups: OnDelay unavailable");
	}

        /* Try to set "OFF delay" (if supported and given) */
	if (getval ("offdelay"))
	{
		mge_ups.OffDelay = atoi (getval ("offdelay"));
		/* Set the value in the UPS */
		mge_command(buf, sizeof(buf), "Sn %d",  mge_ups.OffDelay);
		if(!strcmp(buf, "OK"))
			upsdebugx(1, "OFF delay set to %d sec", mge_ups.OffDelay);
		else
			upsdebugx(1, "initups: OffDelay unavailable");
	}
}

/* --------------------------------------------------------------- */

void upsdrv_initinfo(void)
{
	char buf[BUFFLEN];
	const char *model = NULL;
	char *firmware = NULL;
	char *p;
	char *v = NULL;  /* for parsing Si output, get Version ID */
	int  table;
	int  tries;
	int  status_ok = 0;
	int  bytes_rcvd;
	int  si_data1 = 0;
	int  si_data2 = 0;
	mge_info_item_t *item;
	models_name_t *model_info;
	mge_model_info_t *legacy_model;
	char infostr[32];
	int  chars_rcvd;

	/* manufacturer -------------------------------------------- */
	dstate_setinfo("ups.mfr", "MGE UPS SYSTEMS");

	/* loop until we have at status */
	tries = 0;
	do {
	    printf(".");

		/* get model information in ASCII string form: <Family> <Model> <Firmware> */
		bytes_rcvd = mge_command(buf, sizeof(buf), "Si 1");

		if(bytes_rcvd > 0 && buf[0] != '?') {
			dstate_setinfo("ups.id", "%s", buf); /* raw id */

			model = buf;

			p = strrchr(buf, ' ');
			if ( p != NULL ) {
				*p = '\0';
				firmware = p+1;
			}

			if( firmware && strlen(firmware) < 1 )
				firmware = NULL;   /* no firmware information */

			/* Parsing model names table */
			for ( model_info = Si1_models_names ; model_info->basename != NULL ; model_info++ ) {
				if(!strcasecmp(model_info->basename, model))
				{
					model = model_info->finalname;
					upsdebugx(1, "initinfo: UPS model == >%s<", model);
					break;
				}
			}
		}
		else
		  {
			upsdebugx(1, "initinfo: 'Si 1' unavailable, switching to 'Si' command");

			/* get model information, numbered form, : <Model ID> <Version ID> <Firmware> */
			bytes_rcvd = mge_command(buf, sizeof(buf), "Si");

			if(bytes_rcvd > 0 && buf[0] != '?') {
			  upsdebugx(1, "initinfo: Si == >%s<", buf);

			  printf("\nCAUTION : This is an older model. It may not support too much polling.\nPlease read man mge-utalk and use pollinterval\n");

			  p = strchr(buf, ' ');

			  if ( p != NULL ) {
				*p = '\0';
				si_data1 = atoi(buf);
				v = p+1;
			  	p = strchr(v, ' ');
			  }

			  if ( p != NULL ) {
				*p = '\0';
				si_data2 = atoi(v);
			  }

				/* Parsing legacy model table in order to find it */
				for ( legacy_model = mge_model ; legacy_model->name != NULL ; legacy_model++ ) {
					if(legacy_model->Data1 == si_data1 && legacy_model->Data2 == si_data2){
						model = legacy_model->name;
						upsdebugx(1, "initinfo: UPS model == >%s<", model);
						break;
				}
			  }

			  if( model == NULL )
				printf("No model found by that model and version ID\nPlease contact us with UPS model, name and reminder info\nReminder info : Data1=%i , Data2=%i\n", si_data1, si_data2);

			}
		  }

		if ( model ) {
			upsdebugx(2, "Got model name: %s", model);

			/* deal with truncated model names */
			if (!strncmp(model, "Evolutio", 8)) {
				dstate_setinfo("ups.model", "Evolution %i", atoi(strchr(model, ' ')));
			} else {
				dstate_setinfo("ups.model", "%s", model);
			}
		}

		if ( firmware && strcmp(firmware, ""))
			dstate_setinfo("ups.firmware", "%s", firmware);
		else
			dstate_setinfo("ups.firmware", "unknown");

		/* multiplier table */
		/* <protocol level> <multiplier table> */
		bytes_rcvd = mge_command(buf, sizeof(buf), "Ai");

		if (bytes_rcvd > 0 && buf[0] != '?') {
			p = strchr(buf, ' ');
			if ( p != NULL ) {
				table = atoi(p + 1);
				if ( 0 < table && table < 4 )
				mge_ups.MultTab = table;
			}
		}

		/* status --- try only system status, to get the really important
		 * information (OL, OB, LB); all else is added later by updateinfo */
		status_ok = get_ups_status();

	} while ( (!status_ok) && (tries++ < MAXTRIES) && (exit_flag != 1) );

	if ( tries == MAXTRIES && !status_ok )
		fatalx(EXIT_FAILURE, "Could not get status from UPS.");

	if ( mge_ups.MultTab == 0 )
		upslogx(LOG_WARNING, "Could not get multiplier table: using raw readings.");

	/* all other variables ------------------------------------ */
	for ( item = mge_info ; item->type != NULL ; item++ ) {

		/* Check if we are asked to stop (reactivity++) */
		if (exit_flag != 0)
			return;

		/* send request, read answer */
		chars_rcvd = mge_command(buf, sizeof(buf), item->cmd);

		if ( chars_rcvd < 1 || buf[0] == '?' ) {
			item->ok = FALSE;
			upsdebugx(1, "initinfo: %s unavailable", item->type);
		} else {
			item->ok = TRUE;
			extract_info(buf, item, infostr, sizeof(infostr));
			dstate_setinfo(item->type, "%s", infostr);
			dstate_setflags(item->type, item->flags);
			upsdebugx(1, "initinfo: %s == >%s<", item->type, infostr);

			/* Set max length for strings */
			if (item->flags & ST_FLAG_STRING)
				dstate_setaux(item->type, item->length);
		}
	} /* for item */

	/* store timestamp */
	lastpoll = time(NULL);

	/* commands ----------------------------------------------- */
	/* FIXME: check if available before adding! */
	dstate_addcmd("load.off");
	dstate_addcmd("load.on");
	dstate_addcmd("shutdown.return");
	dstate_addcmd("shutdown.stayoff");
	dstate_addcmd("test.panel.start");
	dstate_addcmd("test.battery.start");
	dstate_addcmd("bypass.start");
	dstate_addcmd("bypass.stop");

	/* install handlers */
	upsh.setvar = setvar;
	upsh.instcmd = instcmd;

	printf("Detected %s on %s\n", dstate_getinfo("ups.model"), device_path);
}

/* --------------------------------------------------------------- */

void upsdrv_updateinfo(void)
{
	char buf[BUFFLEN];
	char infostr[32];
	int status_ok;
	int bytes_rcvd;
	mge_info_item_t *item;

	/* make sure that communication is enabled */
	enable_ups_comm();

	/* update status */
	status_ok = get_ups_status();  /* only sys status is critical */
	if ( !status_ok )
	{
		upslogx(LOG_NOTICE, "updateinfo: Cannot update system status");
		/* try to re enable communication */
		disable_ups_comm();
		enable_ups_comm();
	}
	else
	{
		dstate_dataok();
	}

	/* Don't overload old units (at startup) */
	if ( (unsigned int)time(NULL) <= (unsigned int)(lastpoll + poll_interval) )
		return;

	/* update all other ok variables */
	for ( item = mge_info ; item->type != NULL ; item++ ) {
		/* Check if we are asked to stop (reactivity++) */
		if (exit_flag != 0)
			return;

		if ( item->ok ) {
			/* send request, read answer */
			bytes_rcvd = mge_command(buf, sizeof(buf), item->cmd);

			if ( bytes_rcvd > 0 && buf[0] != '?' )  {
				extract_info(buf, item, infostr, sizeof(infostr));
				dstate_setinfo(item->type, "%s", infostr);
				upsdebugx(2, "updateinfo: %s == >%s<", item->type, infostr);
				dstate_dataok();
			} else
			{
			  upslogx(LOG_NOTICE, "updateinfo: Cannot update %s", item->type);
			  /* try to re enable communication */
			  disable_ups_comm();
			  enable_ups_comm();
			}
		} /* if item->ok */
	}

	/* store timestamp */
	lastpoll = time(NULL);
}

/* --------------------------------------------------------------- */

void upsdrv_shutdown(void)
{
	char buf[BUFFLEN];
	/*  static time_t lastcmd = 0; */
	memset(buf, 0, sizeof(buf));

	if (sdtype == SD_RETURN) {
		/* enable automatic restart */
		mge_command(buf, sizeof(buf), "Sx 5");

		upslogx(LOG_INFO, "UPS response to Automatic Restart was %s", buf);
	}

	/* Only call the effective shutoff if restart is ok */
	/* or if we need only a stayoff... */
	if (!strcmp(buf, "OK") || (sdtype == SD_STAYOFF)) {
		/* shutdown UPS */
		mge_command(buf, sizeof(buf), "Sx 0");

		upslogx(LOG_INFO, "UPS response to Shutdown was %s", buf);
	}
/*	if(strcmp(buf, "OK")) */

	/* call the cleanup to disable/close the comm link */
	upsdrv_cleanup();
}

/* --------------------------------------------------------------- */

void upsdrv_help(void)
{
}

/* --------------------------------------------------------------- */
/*                      Internal Functions                         */
/* --------------------------------------------------------------- */

/* handler for commands to be sent to UPS */
int instcmd(const char *cmdname, const char *extra)
{
	char temp[BUFFLEN];

	/* Start battery test */
	if (!strcasecmp(cmdname, "test.battery.start"))
	{
		mge_command(temp, sizeof(temp), "Bx 1");
		upsdebugx(2, "UPS response to %s was %s", cmdname, temp);

		if(strcmp(temp, "OK"))
			return STAT_INSTCMD_UNKNOWN;
		else
			return STAT_INSTCMD_HANDLED;
	}

	/* Start front panel test  */
	if (!strcasecmp(cmdname, "test.panel.start"))
	{
		mge_command(temp, sizeof(temp), "Sx 129");
		upsdebugx(2, "UPS response to %s was %s", cmdname, temp);

		if(strcmp(temp, "OK"))
			return STAT_INSTCMD_UNKNOWN;
		else
			return STAT_INSTCMD_HANDLED;
	}

	/* Shutdown UPS */
	if (!strcasecmp(cmdname, "shutdown.stayoff"))
	{
		sdtype = SD_STAYOFF;
		upsdrv_shutdown();
	}

	if (!strcasecmp(cmdname, "shutdown.return"))
	{
		sdtype = SD_RETURN;
		upsdrv_shutdown();
	}

	/* Power Off [all] plugs */
	if (!strcasecmp(cmdname, "load.off"))
	{
		/* TODO: Powershare (per plug) control */
		mge_command(temp, sizeof(temp), "Wy 65535");
		upsdebugx(2, "UPS response to Select All Plugs was %s", temp);

		if(strcmp(temp, "OK"))
			return STAT_INSTCMD_UNKNOWN;
		else
		{
			mge_command(temp, sizeof(temp), "Wx 0");
			upsdebugx(2, "UPS response to %s was %s", cmdname, temp);
			if(strcmp(temp, "OK"))
				return STAT_INSTCMD_UNKNOWN;
			else
				return STAT_INSTCMD_HANDLED;
		}
	}

	/* Power On all plugs */
	if (!strcasecmp(cmdname, "load.on"))
	{
		/* TODO: add per plug control */
		mge_command(temp, sizeof(temp), "Wy 65535");
		upsdebugx(2, "UPS response to Select All Plugs was %s", temp);

		if(strcmp(temp, "OK"))
			return STAT_INSTCMD_UNKNOWN;
		else
		{
			mge_command(temp, sizeof(temp), "Wx 1");
			upsdebugx(2, "UPS response to %s was %s", cmdname, temp);
			if(strcmp(temp, "OK"))
				return STAT_INSTCMD_UNKNOWN;
			else
				return STAT_INSTCMD_HANDLED;
		}
	}

	/* Switch on/off Maintenance Bypass */
	if ((!strcasecmp(cmdname, "bypass.start"))
		|| (!strcasecmp(cmdname, "bypass.stop")))
	{
		/* TODO: add control on bypass value */
		/* read maintenance bypass status */
		if(mge_command(temp, sizeof(temp), "Ps") > 0)
		{
			if (temp[0] == '1')
			{
				/* Disable Maintenance Bypass */
				mge_command(temp, sizeof(temp), "Px 2");
				upsdebugx(2, "UPS response to Select All Plugs was %s", temp);
			} else
			{
				/* Enable Maintenance Bypass */
				mge_command(temp, sizeof(temp), "Px 3");
			}

			upsdebugx(2, "UPS response to %s was %s", cmdname, temp);

			if(strcmp(temp, "OK"))
				return STAT_INSTCMD_UNKNOWN;
			else
				return STAT_INSTCMD_HANDLED;
		}
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s] [%s]", cmdname, extra);
	return STAT_INSTCMD_UNKNOWN;
}

/* --------------------------------------------------------------- */

/* handler for settable variables in UPS*/
int setvar(const char *varname, const char *val)
{
	char temp[BUFFLEN];
	char cmd[15];

	/* TODO : add some controls */

	if(info_variable_ok(varname))
	{
		/* format command */
		snprintf(cmd, sizeof(cmd), "%s", info_variable_cmd(varname));
		sprintf(strchr(cmd, '?'), "%s", val);

		/* Execute command */
		mge_command(temp, sizeof(temp), cmd);
		upslogx(LOG_INFO, "setvar: UPS response to Set %s to %s was %s", varname, val, temp);
	} else
		upsdebugx(1, "setvar: Variable %s not supported by UPS", varname);

	return STAT_SET_UNKNOWN;
}

/* --------------------------------------------------------------- */

/* disable communication with UPS to avoid interference with
 * kernel serial init at boot time (ie with V24 init) */
static void disable_ups_comm(void)
{
	upsdebugx(1, "disable_ups_comm()");
	ser_flush_in(upsfd, "?\r\n", 0);
	usleep(MGE_CONNECT_DELAY);
	mge_command(NULL, 0, "Ax 0");
}

/* enable communication with UPS */
static void enable_ups_comm(void)
{
	char buf[8];

	/* send Z twice --- speeds up re-connect */
	mge_command(NULL, 0, "Z");
	mge_command(NULL, 0, "Z");
	/* only enable communication if needed! */
	if ( mge_command(buf, 8, "Si") <= 0)
	{
		mge_command(NULL, 0, "Ax 1");
		usleep(MGE_CONNECT_DELAY);
	}

	ser_flush_in(upsfd, "?\r\n", nut_debug_level);
}

/* --------------------------------------------------------------- */

/* extract information from buffer
   in:   buf    : reply from UPS
         item   : INFO item queried
   out:  infostr: to be placed in INFO_ variable
   NOTE: buf="?" must be handled before calling extract_info
         buf is changed inspite of const !!!!!
*/
static void extract_info(const char *buf, const mge_info_item_t *item,
			 char *infostr, int infolen)
{
	/* initialize info string */
	infostr[0] = '\0';

#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	/* write into infostr with proper formatting */
	if ( strpbrk(item->fmt, "feEgG") ) {           /* float */
		snprintf(infostr, infolen, item->fmt,
			multiplier[mge_ups.MultTab][item->unit] * atof(buf));
	} else if ( strpbrk(item->fmt, "dioxXuc") ) {  /* int   */
		snprintf(infostr, infolen, item->fmt,
			(int) (multiplier[mge_ups.MultTab][item->unit] * atof(buf)));
	} else {
		snprintf(infostr, infolen, item->fmt, buf);
	}
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
}



/* --------------------------------------------------------------- */

/* get system status, at least: OB, OL, LB
   calls set_status appropriately
   tries MAXTRIES times
   returns non-nil if successful

   NOTE: MGE counts bytes/chars the opposite way as C,
         see mge-utalk manpage.  If status commands send two
         data items, these are separated by a space, so
	 the elements of the second item are in buf[16..9].
*/

static int get_ups_status(void)
{
	char buf[BUFFLEN];
	int rb_set= FALSE;  /* has RB flag been set ? */
	int over_set= FALSE;  /* has OVER flag been set ? */
	int tries = 0;
	int ok    = FALSE;
	int bytes_rcvd = 0;

	do {
		/* Check if we are asked to stop (reactivity++) */
		if (exit_flag != 0)
			return FALSE;

		/* must clear status buffer before each round */
		status_init();

		/* system status */
/* FIXME: some old units sometimes return "Syst Stat >1<"
   resulting in an temporary OB status */
		bytes_rcvd = mge_command(buf, sizeof(buf), "Ss");
		upsdebugx(1, "Syst Stat >%s<", buf);
		if ( bytes_rcvd > 0 && strlen(buf) > 7 ) {
			ok = TRUE;
			if (buf[6] == '1') {
				over_set = TRUE;
				status_set("OVER");
			}
			if (buf[5] == '1')
				status_set("OB");
			else
				status_set("OL");

			if (buf[4] == '1')
				status_set("LB");

			if (buf[3] == '1') {
				rb_set = TRUE;
				status_set("RB");
			}
			/* buf[2] not used */
			if (buf[1] == '1')
				status_set("COMMFAULT"); /* self-invented */
				/* FIXME: better to call datastale()?! */
			if (buf[0] == '1')
				status_set("ALARM");     /* self-invented */
				/* FIXME: better to use ups.alarm */
		}  /* if strlen */

		/* battery status */
		mge_command(buf, sizeof(buf), "Bs");
		upsdebugx(1, "Batt Stat >%s<", buf);
		if ( strlen(buf) > 7 ) {
			if ( !rb_set && ( buf[7] == '1' || buf[3] == '1' ) )
				status_set("RB");

			if (buf[1] == '1')
				status_set("CHRG");

			if (buf[0] == '1')
				status_set("DISCHRG");
		} /* if strlen */

		/* load status */
		mge_command(buf, sizeof(buf), "Ls");
		upsdebugx(1, "Load Stat >%s<", buf);
		if ( strlen(buf) > 7 ) {
			if (buf[4] == '1')
				status_set("BOOST");

			if ( !over_set && ( buf[3] == '1' ) )
				status_set("OVER");

			if (buf[2] == '1')
			status_set("TRIM");
		} /* if strlen */

		if ( strlen(buf) > 15 ) {   /* second "byte", skip <SP> */
			if (buf[16] == '1') {
				status_set("OB");
				status_set("LB");
			}

			/* FIXME: to be checked (MUST be buf[8]) !! */
			/* if ( !(buf[9] == '1') ) */
			/* This is not the OFF status!
			if ( !(buf[8] == '1') )
				status_set("OFF"); */
		} /* if strlen */

		/* Bypass status */
		mge_command(buf, sizeof(buf), "Ps");
		upsdebugx(1, "Bypass Stat >%s<", buf);
		if ( strlen(buf) > 7 ) {
		  /* FIXME: extend ups.status for BYPASS: */
		  /* Manual Bypass */
			if (buf[7] == '1')
				status_set("BYPASS");
		  /* Automatic Bypass */
			if (buf[6] == '1')
				status_set("BYPASS");
		} /* if strlen */

	} while ( !ok && tries++ < MAXTRIES );

	status_commit();

	return ok;
}

/* --------------------------------------------------------------- */

/* return proper variable "ok" given INFO_ type */

static bool_t info_variable_ok(const char *type)
{
	mge_info_item_t *item = mge_info ;

	while ( strcasecmp(item->type, type ))
		item++;

	return item->ok;
}

/* --------------------------------------------------------------- */

/* return proper variable "cmd" given INFO_ type */

static const char *info_variable_cmd(const char *type)
{
	mge_info_item_t *item = mge_info ;

	while ( strcasecmp(item->type, type ))
		item++;

	return item->cmd;
}

/* --------------------------------------------------------------- */

/* send command to UPS and read reply if requested

   reply   :  buffer for reply, NULL if no reply expected
   replylen:  length of buffer reply
   fmt     :  format string, followed by optional data for command

   returns :  no of chars received, -1 if error
*/
static int mge_command(char *reply, int replylen, const char *fmt, ...)
{
	const char *p;
	char command[BUFFLEN];
	int bytes_sent = 0;
	int bytes_rcvd = 0;
	int ret;
	va_list ap;

	/* build command string */
	va_start(ap, fmt);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	ret = vsnprintf(command, sizeof(command), fmt, ap);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif

	if ((ret < 1) || (ret >= (int) sizeof(command)))
		upsdebugx(4, "mge_command: command truncated");

	va_end(ap);

	/* Delay a bit to avoid overlap of a previous answer (500 ms), as per
	 * http://old.networkupstools.org/protocols/mge/9261zwfa.pdf § 6.1. Timings */
	usleep(500000);

	/* flush received, unread data */
	tcflush(upsfd, TCIFLUSH);

	/* send command */
	for (p = command; *p; p++) {
		if ( isprint(*p & 0xFF) )
			upsdebugx(4, "mge_command: sending [%c]", *p);
		else
			upsdebugx(4, "mge_command: sending [%02X]", *p);

		if (write(upsfd, p, 1) != 1)
			return -1;

		bytes_sent++;
		usleep(MGE_CHAR_DELAY);
	}

	/* send terminating string */
	for (p = MGE_COMMAND_ENDCHAR; *p; p++) {
		if ( isprint(*p & 0xFF) )
			upsdebugx(4, "mge_command: sending [%c]", *p);
		else
			upsdebugx(4, "mge_command: sending [%02X]", *p);

		if (write(upsfd, p, 1) != 1)
			return -1;

		bytes_sent++;
		usleep(MGE_CHAR_DELAY);
	}

	if ( !reply )
		return bytes_rcvd;
	else
		usleep(MGE_REPLY_DELAY);

	bytes_rcvd = ser_get_line(upsfd, reply, replylen,
		MGE_REPLY_ENDCHAR, MGE_REPLY_IGNCHAR, 3, 0);

	upsdebugx(4, "mge_command: received %d byte(s)", bytes_rcvd);

	return bytes_rcvd;
}

void upsdrv_cleanup(void)
{
	upsdebugx(1, "cleaning up");
	disable_ups_comm();
	ser_close(upsfd, device_path);
}
