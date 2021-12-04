/* optiups.c - OptiSafe UPS (very loosely based on the nut 0.45.5 driver)

   Copyright (C) 1999  Russell Kroll <rkroll@exploits.org>
   Copyright (C) 2006  Scott Heavner [Use my alioth acct: sheavner]

   Support for Zinto D from ONLINE USV (only minor differences to OptiSafe UPS)
   added by Matthias Goebl <matthias.goebl@goebl.net>

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

   ==================================================================================
*/

#include "main.h"
#include "serial.h"

#define DRIVER_NAME	"Opti-UPS driver"
#define DRIVER_VERSION "1.01"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Russell Kroll <rkroll@exploits.org>\n" \
	"Scott Heavner <debian@scottheavner.com>\n" \
	"Matthias Goebl <matthias.goebl@goebl.net>",
	DRV_STABLE,
	{ NULL }
};

#define HELP                                                                    "\n" \
   "**********************************************************"                 "\n" \
   "This driver has been tested only with the OPTI SAFE 420E using"             "\n" \
   "the custom cable described in the NUT Opti-UPS protocol page."              "\n" \
   "Seriously, it does _NOT_ work with the [windows] cable provided with"       "\n" \
   "the UPS or a standard serial cable. (I don't know if OPTI makes other"      "\n" \
   "UPS models that will work with another cable?)  Standard linux serial"      "\n" \
   "port drivers do not support the required DTR handshaking."                  "\n" \
   ""                                                                           "\n" \
   "     UPS 6 -> PC 3          This 3 wire cable pinout maps DTR to CTS."      "\n" \
   "     UPS 9 -> PC 2"                                                         "\n" \
   "     UPS 4 -> PC 5"                                                         "\n" \
   ""                                                                           "\n" \
   "This driver has also been tested with a Zinto D from Online-USV AG,"        "\n" \
   "using their special cable:"                                                 "\n" \
   "     UPS 6 -> PC 3"                                                         "\n" \
   "     UPS 9 -> PC 2"                                                         "\n" \
   "     UPS 7 -> PC 5"                                                         "\n" \
   "It works even with a pl2303 usb-serial converter."                          "\n" \
   "**********************************************************"                 "\n"

/* See http://www.networkupstools.org/protocols/optiups.html and the end of this
 * file for more information on the cable and the OPTI-UPS serial protocol used on
 * at least the older OPTI UPS models (420E, 820ES).
 */

#define ENDCHAR		'\n'
#define IGNCHARS	"\r"

/* Our custom options available with -x */
#define OPTI_MINPOLL		"status_only"
#define OPTI_FAKELOW		"fake_lowbatt"
#define OPTI_NOWARN_NOIMP	"nowarn_noimp"
#define OPTI_POWERUP		"powerup"

/* All serial commands put their response in the same buffer space */
static char _buf[256];

/* Model */
static int optimodel = 0;
enum {
	OPTIMODEL_DEFAULT = 0,
	OPTIMODEL_ZINTO =1
};


/* Status bits returned by the "AG" command */
enum {
	OPTISBIT_NOOUTPUT = 2,
	OPTISBIT_OVERLOAD = 8,
	OPTISBIT_REPLACE_BATTERY = 16,
	OPTISBIT_ON_BATTERY_POWER = 32,
	OPTISBIT_LOW_BATTERY = 64
};

/* Helper struct for the optifill() function */
typedef struct ezfill_s {
	const char *cmd;
	const char *var;
	const float scale;  /*  if 0, no conversion is done and the string
	                        is passed to dstate as is, otherwise a float
	                        conversion with single decimal is applied */
} ezfill_t;

/* These can be polled right into a string usable by NUT.
 * Others such as "AG" and "BV" require some transformation of the return value */
static ezfill_t _pollv[] = {
	{ "NV", "input.voltage", 0 },
	{ "OL", "ups.load", 1.0 },
	{ "OV", "output.voltage", 0 },
	{ "FF", "input.frequency", 0.1 },
	{ "BT", "ups.temperature", 0 },
};
static ezfill_t _pollv_zinto[] = {
	{ "NV", "input.voltage", 2.0 },
	{ "OL", "ups.load", 1.0 },
	{ "OV", "output.voltage", 2.0 },
	{ "OF", "output.frequency", 0.1 },
	{ "NF", "input.frequency", 0.1 },
	{ "BT", "ups.temperature", 0 },
};

/* model "IO" is parsed differently in upsdrv_initinfo() */
static ezfill_t _initv[] = {
	{ "IM", "ups.mfr", 0 },
	{ "IZ", "ups.serial", 0 },
	{ "IS", "ups.firmware", 0 },
};

/* All serial reads of the OPTI-UPS go through here.  We always expect a CR/LF terminated
 *   response.  Unknown/Unimplemented commands return ^U (0x15).  Actions that complete
 *   successfully return ^F (0x06). */
static inline ssize_t optireadline(void)
{
	ssize_t r;
	usleep(150000);
	r = ser_get_line(upsfd, _buf, sizeof(_buf), ENDCHAR, IGNCHARS, 0, 500000 );
	_buf[sizeof(_buf)-1] = 0;
	if ( r > 0 )
	{
		if ( r < (int)sizeof(_buf) )
			_buf[r] = 0;
		if ( _buf[0] == 0x15 )
		{
			r=-2;
			upsdebugx(1, "READ: <unsupported command>");
		}
		if ( _buf[0] == 0x06 )
		{
			upsdebugx(2, "READ: <command done>");
		}
		else
		{
			upsdebugx(2, "READ: \"%s\"", _buf );
		}
	}
	else
		upsdebugx(1, "READ ERROR: %zd", r );
	return r;
}

/* Send a command and read the response.  Command response is in global _buf.
 *   Return
 *      > 0 implies success.
 *      -1  serial timeout
 *      -2  unknown/unimplemented command
 */
static inline ssize_t optiquery( const char *cmd )
{
	upsdebugx(2, "SEND: \"%s\"", cmd );
	ser_send( upsfd, "%s", cmd );
	if ( optimodel == OPTIMODEL_ZINTO )
		ser_send( upsfd, "\r\n" );
	return optireadline();
}

/* Uses the ezfill_t structure to map UPS commands to the NUT variable destinations */
static void optifill( ezfill_t *a, size_t len )
{
	size_t i;
	ssize_t r;

	/* Some things are easy to poll and store */
	for ( i=0; i<len; ++i )
	{
		if ( ( a[i].cmd == NULL ) || ( a[i].cmd[0] == 0 ) )
			continue;

		r = optiquery( a[i].cmd );
		if ( r < 1 )
		{
			if ( r == -2 )
			{
				if ( ! testvar(OPTI_NOWARN_NOIMP) )
					upslogx( LOG_NOTICE,
						"disabling poll of unsupported var %s",
						a[i].var );
				dstate_setinfo( a[i].var, "N/A" );
				a[i].cmd = NULL;
			}
			else
				upslogx( LOG_WARNING, "can't retrieve %s", a[i].var );
			continue;
		}
		if ( a[i].scale > 1e-20 )
		{
			float f = strtol( _buf, NULL, 10 ) * a[i].scale;
			dstate_setinfo( a[i].var, "%.1f", f );
		}
		else
		{
			dstate_setinfo( a[i].var, "%s", _buf);
		}
	}
}

/* Handle custom (but standardized) NUT commands */
static int instcmd(const char *cmdname, const char *extra)
{
	if (!strcasecmp(cmdname, "test.failure.start"))
	{
		optiquery( "Ts" );
		return STAT_INSTCMD_HANDLED;
	}
	else if (!strcasecmp(cmdname, "load.off"))
	{
		/* You do realize this will kill power to ourself.
		 * Would probably only be useful for killing power for
		 * a computer with upsmon in "secondary" mode */
		if ( optimodel == OPTIMODEL_ZINTO )
		{
			optiquery( "Ct1" );
			optiquery( "Cs0000000" );
			sleep(2);
			return STAT_INSTCMD_HANDLED;
		}
		optiquery( "Ct0" );
		optiquery( "Cs00000000" );
		return STAT_INSTCMD_HANDLED;
	}
	else if (!strcasecmp(cmdname, "load.on"))
	{
		if ( optimodel == OPTIMODEL_ZINTO )
		{
			optiquery( "Ct1" );
			optiquery( "Cu0000000" );
			sleep(2);
			return STAT_INSTCMD_HANDLED;
		}
		optiquery( "Ct0" );
		optiquery( "Cu00000000" );
		return STAT_INSTCMD_HANDLED;
	}
	else if (!strcasecmp(cmdname, "shutdown.return"))
	{
		/* This shuts down the UPS.  When the power returns to the UPS,
		 *   it will power back up in its default state. */
		if ( optimodel == OPTIMODEL_ZINTO )
		{
			optiquery( "Ct1" );
			optiquery( "Cu0000010" );
			optiquery( "Cs0000001" );
			return STAT_INSTCMD_HANDLED;
		}
		optiquery( "Ct1" );
		optiquery( "Cs00000010" );
		return STAT_INSTCMD_HANDLED;
	}
	else if (!strcasecmp(cmdname, "shutdown.stayoff"))
	{
		/* This actually stays off as long as the batteries hold,
		 *   if the line power comes back before the batteries die,
		 *   the UPS will never powerup its output stage!!! */
		if ( optimodel == OPTIMODEL_ZINTO )
		{
			optiquery( "Ct1" );
			optiquery( "Cs0000001" );
			return STAT_INSTCMD_HANDLED;
		}
		optiquery( "Ct0" );
		optiquery( "Cs00000010" );
		return STAT_INSTCMD_HANDLED;
	}
	else if (!strcasecmp(cmdname, "shutdown.stop"))
	{
		/* Aborts a shutdown that is couting down via the Cs command */
		optiquery( "Cs-0000001" );
		return STAT_INSTCMD_HANDLED;
	}

	upslogx(LOG_NOTICE, "instcmd: unknown command [%s] [%s]", cmdname, extra);
	return STAT_INSTCMD_UNKNOWN;
}

/* Handle variable setting */
static int setvar(const char *varname, const char *val)
{
	int status;

	if (sscanf(val, "%d", &status) != 1) {
		return STAT_SET_UNKNOWN;
	}

	if (strcasecmp(varname, "outlet.1.switch") == 0) {
		status = status==1 ? 1 : 0;
		dstate_setinfo( "outlet.1.switch", "%d", status);
		optiquery(status ? "Oi11" : "Oi10");
		dstate_dataok();
		return STAT_SET_HANDLED;
	}

	return STAT_SET_UNKNOWN;
}

void upsdrv_initinfo(void)
{
	ssize_t r;

	/* If an Zinto Online-USV is off, switch it on first. */
	/* It sends only "2" when off, without "\r\n", and doesn't */
	/* answer other commands. Therefore without power we'll be */
	/* unable to identify the ups. */
	if ( testvar(OPTI_POWERUP) && optiquery( "AG" ) < 1 )
	{
		ser_send( upsfd, "AG\r\n" );
		r = ser_get_char(upsfd, &_buf[0], 1, 0);
		if ( r == 1 && _buf[0] == '2' )
		{
			upslogx( LOG_WARNING, "ups was off, switching it on" );
			optiquery( "Ct1" );
			optiquery( "Cu0000000" );
			/* wait for power up */
			sleep(15);
		}
	}

	/* Autodetect an Online-USV (only Zinto D is known to work) */
	r = optiquery( "IM" );
	if ( r > 0 && !strcasecmp(_buf, "ONLINE") )
	{
		optimodel = OPTIMODEL_ZINTO;
		optiquery( "Om11" );
		optiquery( "Om21" );
		optiquery( "ON" );
	}

	optifill( _initv, sizeof(_initv)/sizeof(_initv[0]) );

	/* Parse out model into longer string -- is this really USEFUL??? */
	r = optiquery( "IO" );
	if ( r < 1 )
		fatal_with_errno(EXIT_FAILURE, "can't retrieve model" );
	else
	{
		switch ( _buf[r-1] )
		{
			case 'E':
			case 'P':
			case 'V':
				dstate_setinfo("ups.model", "Power%cS %s", _buf[r-1], _buf );
				break;
			default:
				dstate_setinfo("ups.model", "%s", _buf );
				break;
		}
	}

	/* Parse out model into longer string */
	r = optiquery( "IM" );
	if ( r > 0 && !strcasecmp(_buf, "ONLINE") )
	{
		dstate_setinfo("ups.mfr", "ONLINE USV-Systeme AG");
		r = optiquery( "IO" );
		if ( r < 1 )
			fatal_with_errno(EXIT_FAILURE, "can't retrieve model" );
		switch ( _buf[0] )
		{
			case 'D':
				dstate_setinfo("ups.model", "Zinto %s", _buf );
				break;
			default:
				dstate_setinfo("ups.model", "%s", _buf );
				break;
		}
	}

	dstate_addcmd("test.failure.start");
	dstate_addcmd("load.off");
	dstate_addcmd("load.on");
	if( optimodel != OPTIMODEL_ZINTO )
	dstate_addcmd("shutdown.stop");
	dstate_addcmd("shutdown.return");
	dstate_addcmd("shutdown.stayoff");
	upsh.instcmd = instcmd;

	if ( optimodel == OPTIMODEL_ZINTO )
	{
		dstate_setinfo("outlet.desc", "%s", "Main Outlet 1+2");
		dstate_setinfo("outlet.1.desc", "%s", "Switchable Outlet 3+4");
		dstate_setinfo("outlet.id", "%d", 1);
		dstate_setinfo("outlet.1.id", "%d", 2);
		dstate_setinfo("outlet.switchable", "%d", 0);
		dstate_setinfo("outlet.1.switchable", "%d", 1);
		dstate_setinfo("outlet.1.switch", "%d", 1);
		dstate_setflags("outlet.1.switch", ST_FLAG_RW | ST_FLAG_STRING);
		dstate_setaux("outlet.1.switch", 1);
		upsh.setvar = setvar;
	}
}

void upsdrv_updateinfo(void)
{
	ssize_t r = optiquery( "AG" );

	/* Online-UPS send only "2" when off, without "\r\n" */
	if ( r < 1 && optimodel == OPTIMODEL_ZINTO )
	{
		ser_send( upsfd, "AG\r\n" );
		r = ser_get_char(upsfd, &_buf[0], 1, 0);
		if ( r == 1 && _buf[0] == '2' )
		{
			status_init();
			status_set("OFF");
			status_commit();
			return;
		}
	}

	if ( r < 1 )
	{
		upslogx(LOG_ERR, "can't retrieve ups status" );
		dstate_datastale();
	}
	else
	{
		int s = strtol( _buf, NULL, 16 );
		status_init();
		if ( s & OPTISBIT_OVERLOAD )
			status_set("OVER");
		if ( s & OPTISBIT_REPLACE_BATTERY )
			status_set("RB");
		if ( s & OPTISBIT_ON_BATTERY_POWER )
			status_set("OB");
		else
			status_set("OL");
		if ( s & OPTISBIT_NOOUTPUT )
			status_set("OFF");
		if ( s & OPTISBIT_LOW_BATTERY )
			status_set("LB");
		if ( testvar(OPTI_FAKELOW) )  /* FOR TESTING */
			status_set("LB");
		status_commit();
		dstate_dataok();
	}

	/* Get out of here now if minimum polling is desired */
	if ( testvar(OPTI_MINPOLL) )
		return;

	/* read some easy settings */
	if ( optimodel == OPTIMODEL_ZINTO )
		optifill( _pollv_zinto, sizeof(_pollv_zinto)/sizeof(_pollv_zinto[0]) );
	else
		optifill( _pollv, sizeof(_pollv)/sizeof(_pollv[0]) );

	/* Battery voltage is harder */
	r = optiquery( "BV" );
	if ( r < 1 )
		upslogx( LOG_WARNING, "cannot retrieve battery voltage" );
	else
	{
		float p, v = strtol( _buf, NULL, 10 ) / 10.0;
		dstate_setinfo("battery.voltage", "%.1f", v );

		/* battery voltage range: 10.4 - 13.0 VDC */
		p = ((v  - 10.4) / 2.6) * 100.0;
		if ( p > 100.0 )
			p = 100.0;
		dstate_setinfo("battery.charge", "%.1f", p );
	}
}

void upsdrv_shutdown(void)
{
	/* OL: this must power cycle the load if possible */
	/* OB: the load must remain off until the power returns */

	/* If get no response, assume on battery & battery low */
	int s = OPTISBIT_ON_BATTERY_POWER | OPTISBIT_LOW_BATTERY;

	ssize_t r = optiquery( "AG" );
	if ( r < 1 )
	{
		upslogx(LOG_ERR, "can't retrieve ups status during shutdown" );
	}
	else
	{
		s = strtol( _buf, NULL, 16 );
	}

	/* Turn output stage back on if power returns - but really means
	 *   turn off ups if on battery */
	optiquery( "Ct1" );

	/* What happens, if the power comes back *after* reading the ups status and
	 * before the shutdown command? For "Online-UPS Zinto D" *always* asking for
	 * "shutdown shortly and power-up later" works perfectly, because it forces
	 * a power cycle, even for the named race condition.
	 * For Opti-UPS I have no information, so I wouldn't dare to change it.
	 * BTW, Zinto expects only 7 digits after Cu/Cs.
	 * (Matthias Goebl)
	 */
	if ( optimodel == OPTIMODEL_ZINTO )
	{
		/* On line power: Power up in 60 seconds (30 seconds after the following shutdown) */
		/* On battery: Power up when the line power returns */
		optiquery( "Cu0000060" );
		/* Shutdown in 30 seconds */
		optiquery( "Cs0000030" );
		return;
	}

	/* Just cycling power, schedule output stage to come back on in 60 seconds */
	if ( !(s&OPTISBIT_ON_BATTERY_POWER) )
		optiquery( "Cu00000600" );

	/* Shutdown in 8 seconds */
	optiquery( "Cs00000080" );
}

void upsdrv_help(void)
{
	printf(HELP);
}

/* list flags and values that you want to receive via -x */
void upsdrv_makevartable(void)
{
	addvar(VAR_FLAG, OPTI_MINPOLL, "Only poll for critical status variables");
	addvar(VAR_FLAG, OPTI_FAKELOW, "Fake a low battery status" );
	addvar(VAR_FLAG, OPTI_NOWARN_NOIMP, "Suppress warnings of unsupported commands");
	addvar(VAR_FLAG, OPTI_POWERUP, "(Zinto D) Power-up UPS at start (cannot identify a powered-down Zinto D)");
}

void upsdrv_initups(void)
{
	upsfd = ser_open(device_path);
	ser_set_speed(upsfd, device_path, B2400);

	/* NO PARSED COMMAND LINE VARIABLES */
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}

/*******************************************
 * COMMANDS THAT QUERY THE UPS
 *******************************************
 * IM - manufacturer <ViewSonic>
 * IO - model <UPS-420E>
 * IS - firmware version <V1.2B>
 * IZ - serial number <> (unsupported on 420E, returns ^U)
 *
 * BS - ??? returns <2>
 * BV - battery voltage (in deciVolts) <0140>
 * BT - battery temperature (deg C) <0033> (returns ^U on OptiUPS 420E)
 *
 * NF - input line frequency <600>
 * NV - input line voltage <116>
 *
 * OS - ?? return <3>
 * OF - output stage frequency (in 0.1 Hz) <600>
 * OV - output stage voltage <118>
 * OL - output stage load <027>
 *
 * FV - Input voltage <120>
 * FF - Input Frequency (0.1Hz) <600>
 * FO - Output volts <120>
 * FR - Output Frequency (0.1Hz) <600>
 * FA - Output VA <420>
 * FP - Ouptu power <252>
 * FU - ?? returns <2>
 * FB - ??
 * FH - High Transfer Point <144>
 * FL - Low Transfer Point <093>
 * FT - Transfer point? <121>
 *
 * AG - UPS status (bitmapped ASCII hex value) <00>
 *         bit 2: 1 = <set when output stage off?>
 *         bit 3: 1 = overload
 *         bit 4: 1 = replace battery
 *         bit 5: 1 = on battery, 0 = on line
 *         bit 6: 1 = low battery
 * TR - Test results <00>
 *         00 = Unknown
 *         01 = Passed
 *         02 = Warning
 *         03 = Error
 *         04 = Aborted
 *         05 = In Progress
 *         06 = No test init
 *
 *******************************************
 * ACTIONS
 *******************************************
 *
 * Ts - Start test
 * Ct0 - set power down mode (when running on battery)
 *         Ct0 = power down only ouput stage
 *         Ct1 = complete power down
 * Cs00000000 - power down after delay (count in 0.1s)
 *         Cs00000100 = power down in 10s
 *         Cs-0000001 = cancel power down request
 * Cu00000000 - power down after delay (count in 0.1s)
 *         Cu00000050 = power up in 5s
 *         Cu00000000 = power up now
 *
 * CT - returns last setting passed to Ct command <0>
 *
 *******************************************
 * UNKNOWN COMMANDS (on 420e anyways)
 *******************************************
 * Fu
 * Fv
 * Ff
 * Fo
 * Fr
 * Fb
 * Fl
 * Fh
 */
