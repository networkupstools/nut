/* genericups.c - support for generic contact-closure UPS models

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

#define DRV_VERSION "1.31"

#include <sys/ioctl.h>
#include <sys/termios.h>

#include "main.h"
#include "serial.h"

#include "genericups.h"

	static	int	upstype = -1;

static void parse_output_signals(const char *value, int *line) 
{ 
	/* parse signals the serial port can output */ 
 
	*line = 0; 
 
	if (strstr(value, "DTR")) 
		if (!strstr(value, "-DTR")) 
			*line |= TIOCM_DTR; 
 
	if (strstr(value, "RTS")) 
		if (!strstr(value, "-RTS")) 
			*line |= TIOCM_RTS; 
 
	if (strstr(value, "ST")) 
		*line |= TIOCM_ST; 
} 
 
static void parse_input_signals(const char *value, int *line, int *val) 
{ 
	/* parse signals the serial port can input */ 
 
	*line = 0; 
	*val = 0; 
 
	if (strstr(value, "CTS")) 
		*line |= TIOCM_CTS; 
	if (!strstr(value, "-CTS")) 
		*val |= TIOCM_CTS; 
 
	if (strstr(value, "DCD")) 
		*line |= TIOCM_CD; 
	if (!strstr(value, "-DCD")) 
		*val |= TIOCM_CD; 
 
	if (strstr(value, "RNG")) 
		*line |= TIOCM_RNG; 
	if (!strstr(value, "-RNG")) 
		*val |= TIOCM_RNG; 
}

void upsdrv_initinfo(void)
{
	/* setup the basics */

	if (getval("mfr"))
		dstate_setinfo("ups.mfr", "%s", getval("mfr"));
	else
		dstate_setinfo("ups.mfr", "%s", upstab[upstype].mfr);

	if (getval("model"))
		dstate_setinfo("ups.model", "%s", getval("model"));
	else
		dstate_setinfo("ups.model", "%s", upstab[upstype].model);

	if (getval("serial"))
		dstate_setinfo("ups.serial", "%s", getval("serial"));

	dstate_setinfo("driver.version.internal", "%s", DRV_VERSION);

	/* see if the user wants to override the signal definitions */

	if (getval("CP")) {
		parse_output_signals(getval("CP"), &upstab[upstype].line_norm); 
		upsdebugx(2, "parse_output_signals: CP overriden with %s\n",
			getval("CP"));	
	}
	
	if (getval("OL")) {
		parse_input_signals(getval("OL"), &upstab[upstype].line_ol, 
			&upstab[upstype].val_ol); 
		upsdebugx(2, "parse_input_signals: OL overriden with %s\n",
			getval("OL"));	
	}
	
	if (getval("LB")) {
		parse_input_signals(getval("LB"), &upstab[upstype].line_bl, 
			&upstab[upstype].val_bl); 
 		upsdebugx(2, "parse_input_signals: LB overriden with %s\n",
			getval("LB"));
	}
	
	if (getval("SD")) {
		parse_output_signals(getval("SD"), &upstab[upstype].line_sd); 
		upsdebugx(2, "parse_output_signals: SD overriden with %s\n",
			getval("SD"));
	}
	
	dstate_dataok();
}

/* normal idle loop - keep up with the current state of the UPS */
void upsdrv_updateinfo(void)
{
	int	flags, ol, bl, ret;

	ret = ioctl(upsfd, TIOCMGET, &flags);

	if (ret != 0) {
		upslog(LOG_INFO, "ioctl failed");
		dstate_datastale();
		return;
	}

	ol = ((flags & upstab[upstype].line_ol) == upstab[upstype].val_ol);
	bl = ((flags & upstab[upstype].line_bl) == upstab[upstype].val_bl);

	status_init();

	if (bl)
		status_set("LB");	/* low battery */

	if (ol)
		status_set("OL");	/* on line */
	else
		status_set("OB");	/* on battery */

	status_commit();
	dstate_dataok();
}

/* show all possible UPS types */
static void listtypes(void)
{
	int	i;

	printf("Valid UPS types:\n\n");

	for (i = 0; upstab[i].mfr != NULL; i++)
		printf("%i: %s\n", i, upstab[i].desc);
}

/* set the flags for this UPS type */
static void set_ups_type(void)
{
	int	i;

	if (!getval("upstype"))
		fatalx("No upstype set - see help text / man page!\n");
	
	upstype = atoi(getval("upstype"));

	for (i = 0; upstab[i].mfr != NULL; i++) {
		if (upstype == i) {
			printf("UPS type: %s\n", upstab[i].desc);
			return;
		}
	}

	printf("\nFatal error: unknown UPS type number\n");
	listtypes();

	exit(EXIT_FAILURE);
}			

/* power down the attached load immediately */
void upsdrv_shutdown(void)
{
	int	flags, ret;;

	if (upstype == -1)
		fatalx("No upstype set - see help text / man page!");

	flags = upstab[upstype].line_sd;

	if (flags == -1)
		fatalx("No shutdown command defined for this model!");

	if (flags == TIOCM_ST) {

#ifndef HAVE_TCSENDBREAK
		fatalx("Need to send a BREAK, but don't have tcsendbreak!");
#endif

		ret = tcsendbreak(upsfd, 4901);

		if (ret != 0)
			fatal("tcsendbreak");

		return;
	}

	ret = ioctl(upsfd, TIOCMSET, &flags);
	if (ret != 0)
		fatal("ioctl TIOCMSET");

	if (getval("sdtime")) {
		int	sdtime;

		sdtime = strtol(getval("sdtime"), (char **) NULL, 10);

		upslogx(LOG_INFO, "Holding shutdown signal for %d seconds...\n",
			sdtime);

		sleep(sdtime);
	}
}

void upsdrv_help(void)
{
	listtypes();
}

void upsdrv_banner(void)
{
	printf("Network UPS Tools - Generic UPS driver %s (%s)\n", 
		DRV_VERSION, UPS_VERSION);
}

void upsdrv_makevartable(void)
{
	addvar(VAR_VALUE, "upstype", "Set UPS type (required)");
	addvar(VAR_VALUE, "mfr", "Override manufacturer name");
	addvar(VAR_VALUE, "model", "Override model name");
	addvar(VAR_VALUE, "serial", "Specify the serial number");
	addvar(VAR_VALUE, "CP", "Override cable power setting"); 
	addvar(VAR_VALUE, "OL", "Override on line signal"); 
	addvar(VAR_VALUE, "LB", "Override low battery signal"); 
	addvar(VAR_VALUE, "SD", "Override shutdown setting"); 
	addvar(VAR_VALUE, "sdtime", "Hold time for shutdown value (seconds)");
}

void upsdrv_initups(void)
{
	set_ups_type();

	upsfd = ser_open(device_path);

	if (ioctl(upsfd, TIOCMSET, &upstab[upstype].line_norm))
		fatal("ioctl TIOCMSET");
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}
