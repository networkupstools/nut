/* mge-utalk.c 

   Driver for MGE UPS SYSTEMS units with serial UTalk protocol

   This driver is a collaborative effort by (Copyright (C) 2002 - 2004):

     Hans Ekkehard Plesser <hans.plesser@itf.nlh.no>
     Arnaud Quette <arnaud.quette@mgeups.com>
     Nicholas Reilly <nreilly@magma.ca>
     Dave Abbott <d.abbott@dcs.shef.ac.uk>
     Marek Kralewski <marek@mercy49.de>

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

#include <ctype.h>
#include <sys/ioctl.h>
#include "timehead.h"
#include "main.h"
#include "serial.h"
#include "mge-utalk.h"

/* --------------------------------------------------------------- */
/*                  Define "technical" constants                   */
/* --------------------------------------------------------------- */

#define DRIVER_NAME    "MGE UPS SYSTEMS/U-Talk driver"
#define DRIVER_VERSION "0.81.0"

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

int sdtype = SD_RETURN;

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
static void extract_info(const char *buf, const mge_info_item *mge, 
			 char *infostr, int infolen);
static const char *info_variable_cmd(const char *type);
static bool info_variable_ok(const char *type);
static int  get_ups_status(void);
static int mge_command(char *reply, int replylen, const char *fmt, ...);
static void format_model_name(char *model);

/* --------------------------------------------------------------- */
/*                    UPS Driver Functions                         */
/* --------------------------------------------------------------- */

void upsdrv_banner(void)
{
	printf("Network UPS Tools - %s %s (%s)\n", 
		DRIVER_NAME, DRIVER_VERSION, UPS_VERSION);
}

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
	int  bytes_rcvd;
	
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B2400);

	/* read command line/conf variables */
	if (getval ("LowBatt"))
		mge_ups.LowBatt = atoi (getval ("LowBatt"));

	if (getval ("OnDelay"))
		mge_ups.OnDelay = atoi (getval ("OnDelay"));

	if (getval ("OffDelay"))
		mge_ups.OffDelay = atoi (getval ("OffDelay"));

	if (testvar ("oldmac"))
		RTS = ~TIOCM_RTS;
	
	/* Init serial line */
	ioctl(upsfd, TIOCMBIC, &RTS);
	enable_ups_comm();

	/* Get and set values (if exists) given on the command line */
	bytes_rcvd = mge_command(buf, sizeof(buf), "Bl ?");
	if(bytes_rcvd > 0 && buf[0] != '?') {
		mge_command(buf, sizeof(buf), "Bl %d",  mge_ups.LowBatt);
		if(strcmp(buf, "OK"))
			upsdebugx(1, "UPS response to %d%% Low Batt Level was %s",
				mge_ups.LowBatt, buf);
	} else
		upsdebugx(1, "initups: LowBatt unavailable");

	bytes_rcvd = mge_command(buf, sizeof(buf), "Sm ?");
	if(bytes_rcvd > 0 && buf[0] != '?') {
		mge_command(buf, sizeof(buf), "Sm %d",  mge_ups.OnDelay);
		if(strcmp(buf, "OK"))
			upsdebugx(1, "UPS response to %d min ON delay was %s",
				mge_ups.OnDelay, buf);
	} else
		upsdebugx(1, "initups: OnDelay unavailable");

	bytes_rcvd = mge_command(buf, sizeof(buf), "Sn ?");
	if(bytes_rcvd > 0 && buf[0] != '?') {
		mge_command(buf, sizeof(buf), "Sn %d",  mge_ups.OffDelay);
		if(strcmp(buf, "OK"))
			upsdebugx(1, "UPS response to %d min OFF delay was %s",
				mge_ups.OffDelay, buf);
	} else
		upsdebugx(1, "initups: OffDelay unavailable");

}

/* --------------------------------------------------------------- */

void upsdrv_initinfo(void)
{
	char buf[BUFFLEN];
	char *model = NULL;
	char *firmware = NULL;
	char *p;
	int  table;
	int  tries;
	int  status_ok;
	int  bytes_rcvd;
	mge_info_item *item;
	
	/* manufacturer -------------------------------------------- */
	dstate_setinfo("ups.mfr", "MGE UPS SYSTEMS");
	dstate_setinfo("driver.version.internal", "%s", DRIVER_VERSION);
	
	/* loop until we have at status */
	tries = 0;
	do {
	    printf(".");

		/* get model information: <Family> <Model> <Firmware> */
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
		}

		if ( model ) {
			format_model_name(model);
			dstate_setinfo("ups.model", "%s", model);
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
	
	} while ( !status_ok && tries++ < MAXTRIES );
  
	if ( tries == MAXTRIES && !status_ok )
		fatalx("Could not get status from UPS.");

	if ( mge_ups.MultTab == 0 )
		upslogx(LOG_WARNING, "Could not get multiplier table: using raw readings.");

	/* all other variables ------------------------------------ */
	for ( item = mge_info ; item->type != NULL ; item++ ) {

		char infostr[32];
		int  chars_rcvd;

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
}

/* --------------------------------------------------------------- */

void upsdrv_updateinfo(void)
{
	char buf[BUFFLEN];
	char infostr[32];
	int status_ok;
	int bytes_rcvd;
	mge_info_item *item;

	/* make sure that communication is enabled */
	enable_ups_comm();
   
	/* update status */
	status_ok = get_ups_status();  /* only sys status is critical */
	if ( !status_ok ) {
		dstate_datastale();
		upslogx(LOG_NOTICE, "updateinfo: Cannot update system status");
	} else {
		dstate_dataok();
	}

	/* update all other ok variables */
	for ( item = mge_info ; item->type != NULL ; item++ ) {
		if ( item->ok ) {
			/* send request, read answer */
			bytes_rcvd = mge_command(buf, sizeof(buf), item->cmd);
			
			if ( bytes_rcvd > 0 && buf[0] != '?' )  {
				extract_info(buf, item, infostr, sizeof(infostr));
				dstate_setinfo(item->type, infostr);
				upsdebugx(2, "updateinfo: %s == >%s<", item->type, infostr);
			} else {
				upslogx(LOG_NOTICE, "updateinfo: Cannot update %s", item->type);
			}
		} /* if item->ok */
	}
}

/* --------------------------------------------------------------- */

void upsdrv_shutdown(void)
{
	char buf[BUFFLEN];
	/*  static time_t lastcmd = 0; */
	
	if (sdtype == SD_RETURN) {
		/* enable automatic restart */
		mge_command(buf, sizeof(buf), "Sx 5");
		
		upslogx(LOG_INFO, "UPS response to Automatic Restart was %s", buf);
	}
	
	if (!strcmp(buf, "OK") || (sdtype == SD_STAYOFF)) {
		/* shutdown UPS */
		mge_command(buf, sizeof(buf), "Sx 0");

		upslogx(LOG_INFO, "UPS response to Shutdown was %s", buf);
	}
/*	if(strcmp(buf, "OK")) */
}

/* --------------------------------------------------------------- */

void upsdrv_help(void)
{
}

/* --------------------------------------------------------------- */
/*                      Internal Functions                         */
/* --------------------------------------------------------------- */

/* deal with truncated model names */
void format_model_name(char *model)
{
	if(!strncmp(model, "Evolutio", 8))
		sprintf(model, "Evolution %i", atoi(strchr(model, ' ')));
}

/* --------------------------------------------------------------- */

/* handler for commands to be sent to UPS */
int instcmd(const char *cmdname, const char *extra)
{
	char temp[BUFFLEN];
	
	/* Start battery test */
	if (!strcasecmp(cmdname, "test.battery.start")) {
		mge_command(temp, sizeof(temp), "Bx 1");
		upsdebugx(2, "UPS response to %s was %s", cmdname, temp);
		
		if(strcmp(temp, "OK"))
			return STAT_INSTCMD_UNKNOWN;
		else
			return STAT_INSTCMD_HANDLED;
	}

	/* Start front panel test  */
	if (!strcasecmp(cmdname, "test.panel.start")) {
		mge_command(temp, sizeof(temp), "Sx 129");
		upsdebugx(2, "UPS response to %s was %s", cmdname, temp);
		
		if(strcmp(temp, "OK"))
			return STAT_INSTCMD_UNKNOWN;
		else
			return STAT_INSTCMD_HANDLED;
	}

	/* Shutdown UPS */
	if (!strcasecmp(cmdname, "shutdown.stayoff")) {
		sdtype = SD_STAYOFF;
		upsdrv_shutdown();
	}
	
	if (!strcasecmp(cmdname, "shutdown.return")) {
		sdtype = SD_RETURN;
		upsdrv_shutdown();
	}
	
	/* Power Off [all] plugs */
	if (!strcasecmp(cmdname, "load.off")) {
    	/* TODO: Powershare (per plug) control */
		mge_command(temp, sizeof(temp), "Wy 65535");
		upsdebugx(2, "UPS response to Select All Plugs was %s", temp);

		if(strcmp(temp, "OK"))
			return STAT_INSTCMD_UNKNOWN;
		else {
			mge_command(temp, sizeof(temp), "Wx 0");
			upsdebugx(2, "UPS response to %s was %s", cmdname, temp);		
			if(strcmp(temp, "OK"))
				return STAT_INSTCMD_UNKNOWN;
			else 
				return STAT_INSTCMD_HANDLED;
		}
	}

	/* Power On all plugs */
	if (!strcasecmp(cmdname, "load.on")) {
    	/* TODO: add per plug control */
		mge_command(temp, sizeof(temp), "Wy 65535");
		upsdebugx(2, "UPS response to Select All Plugs was %s", temp);

		if(strcmp(temp, "OK"))
			return STAT_INSTCMD_UNKNOWN;
		else {
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
		|| (!strcasecmp(cmdname, "bypass.stop"))) {
		/* TODO: add control on bypass value */
    	/* read maintenance bypass status */
		if(mge_command(temp, sizeof(temp), "Ps") > 0) {
			if (temp[0] == '1') {
				/* Disable Maintenance Bypass */
				mge_command(temp, sizeof(temp), "Px 2");
				upsdebugx(2, "UPS response to Select All Plugs was %s", temp);
			} else {
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
	
	upslogx(LOG_NOTICE, "instcmd: unknown command [%s]", cmdname);
	return STAT_INSTCMD_UNKNOWN;
}

/* --------------------------------------------------------------- */

/* handler for settable variables in UPS*/
int setvar(const char *varname, const char *val)
{
	char temp[BUFFLEN];
	char cmd[15];
	
	/* TODO : add some controls */
	
	if(info_variable_ok(varname)) {
		/* format command */
		sprintf(cmd, "%s", info_variable_cmd(varname));
		sprintf(strchr(cmd, '?'), "%s", val);
    
		/* Execute command */
		mge_command(temp, sizeof(temp), cmd);
		upslogx(LOG_INFO, "setvar: UPS response to Set %s to %s was %s", varname, val, temp);
	} else
		upsdebugx(1, "setvar: Variable %s not supported by UPS", varname);
	
	return STAT_SET_UNKNOWN;
}
/* --------------------------------------------------------------- */

/* enable communication with UPS */
static void enable_ups_comm(void)
{
	mge_command(NULL, 0, "Z");   /* send Z twice --- speeds up re-connect */
	mge_command(NULL, 0, "Z");   
	mge_command(NULL, 0, "Ax 1");   
	usleep(MGE_CONNECT_DELAY);
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
static void extract_info(const char *buf, const mge_info_item *item, 
			 char *infostr, int infolen)
{
	/* initialize info string */
	infostr[0] = '\0';

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
}



/* --------------------------------------------------------------- */

/* get system status, at least: OB, OL, LB
   calls set_status appropriately
   tries MAXTRIES times
   returns non-nil if successful           

   NOTE: MGE counts bytes/chars the opposite way as C, 
         see mge-utalk.txt.  If status commands send two
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
		/* must clear status buffer before each round */
		status_init();

		/* system status */
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

			if (buf[0] == '1')
				status_set("ALARM");     /* self-invented */
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

			if ( !(buf[9] == '1') )
				status_set("OFF");
		} /* if strlen */  

		/* Bypass status */
		mge_command(buf, sizeof(buf), "Ps");
		upsdebugx(1, "Bypass Stat >%s<", buf);
		if ( strlen(buf) > 7 ) {
			if (buf[7] == '1')
				status_set("BYPASS");
		} /* if strlen */
	
	} while ( !ok && tries++ < MAXTRIES );

	status_commit();
	
	return ok;
}

/* --------------------------------------------------------------- */

/* return proper variable "ok" given INFO_ type */
   
static bool info_variable_ok(const char *type) 
{
	mge_info_item *item = mge_info ;
	
	while ( strcasecmp(item->type, type ))
		item++;
	
	return item->ok;
}

/* --------------------------------------------------------------- */

/* return proper variable "cmd" given INFO_ type */
   
static const char *info_variable_cmd(const char *type) 
{
	mge_info_item *item = mge_info ;
	
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

	ret = vsnprintf(command, sizeof(command), fmt, ap);

	if ((ret < 1) || (ret >= (int) sizeof(command)))
		upsdebugx(4, "mge_command: command truncated");
	
	va_end(ap);

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
	if (MGE_COMMAND_ENDCHAR) {
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
	}

	if ( !reply )
		return bytes_rcvd;
	else
		usleep(MGE_REPLY_DELAY);

	bytes_rcvd = ser_get_line(upsfd, reply, replylen,
		MGE_REPLY_ENDCHAR, MGE_REPLY_IGNCHAR, 3, 0);
	
	return bytes_rcvd;
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
