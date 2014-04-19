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

#include <sys/ioctl.h>

#include "main.h"
#include "serial.h"
#include "genericups.h"

#define DRIVER_NAME	"Generic contact-closure UPS driver"
#define DRIVER_VERSION	"1.36"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"Russell Kroll <rkroll@exploits.org>",
	DRV_STABLE,
	{ NULL }
};

	static	int	upstype = -1;

static void parse_output_signals(const char *value, int *line)
{
	/* parse signals the serial port can output */

	*line = 0;

	if (strstr(value, "DTR") && !strstr(value, "-DTR")) {
		*line |= TIOCM_DTR;
	}

	if (strstr(value, "RTS") && !strstr(value, "-RTS")) {
		*line |= TIOCM_RTS;
	}

	if (strstr(value, "ST")) {
		*line |= TIOCM_ST;
	}

	if (strstr(value, "CTS")) {
		fatalx(EXIT_FAILURE, "Can't override output with CTS (not an output)");
	}

	if (strstr(value, "DCD")) {
		fatalx(EXIT_FAILURE, "Can't override output with DCD (not an output)");
	}

	if (strstr(value, "RNG")) {
		fatalx(EXIT_FAILURE, "Can't override output with RNG (not an output)");
	}

	if (strstr(value, "DSR")) {
		fatalx(EXIT_FAILURE, "Can't override output with DSR (not an output)");
	}
}

static void parse_input_signals(const char *value, int *line, int *val)
{
	/* parse signals the serial port can input */

	*line = 0;
	*val = 0;

	if (strstr(value, "CTS")) {
		*line |= TIOCM_CTS;

		if (!strstr(value, "-CTS")) {
			*val |= TIOCM_CTS;
		}
	}

	if (strstr(value, "DCD")) {
		*line |= TIOCM_CD;

		if (!strstr(value, "-DCD")) {
			*val |= TIOCM_CD;
		}
	}

	if (strstr(value, "RNG")) {
		*line |= TIOCM_RNG;

		if (!strstr(value, "-RNG")) {
			*val |= TIOCM_RNG;
		}
	}

	if (strstr(value, "DSR")) {
		*line |= TIOCM_DSR;

		if (!strstr(value, "-DSR")) {
			*val |= TIOCM_DSR;
		}
	}

	if (strstr(value, "DTR")) {
		fatalx(EXIT_FAILURE, "Can't override input with DTR (not an input)");
	}

	if (strstr(value, "RTS")) {
		fatalx(EXIT_FAILURE, "Can't override input with RTS (not an input)");
	}

	if (strstr(value, "ST")) {
		fatalx(EXIT_FAILURE, "Can't override input with ST (not an input)");
	}
}

void upsdrv_initinfo(void)
{
	char	*v;

	/* setup the basics */

	dstate_setinfo("ups.mfr", "%s", ((v = getval("mfr")) != NULL) ? v : upstab[upstype].mfr);
	dstate_setinfo("ups.model", "%s", ((v = getval("model")) != NULL) ? v : upstab[upstype].model);

	if ((v = getval("serial")) != NULL) {
		dstate_setinfo("ups.serial", "%s", v);
	}

	/*
	 User wants to override the input signal definitions. See also upsdrv_initups().
	 */
	if ((v = getval("OL")) != NULL) {
		parse_input_signals(v, &upstab[upstype].line_ol, &upstab[upstype].val_ol);
		upsdebugx(2, "parse_input_signals: OL overridden with %s\n", v);
	}

	if ((v = getval("LB")) != NULL) {
		parse_input_signals(v, &upstab[upstype].line_bl, &upstab[upstype].val_bl);
		upsdebugx(2, "parse_input_signals: LB overridden with %s\n", v);
	}
}

/* normal idle loop - keep up with the current state of the UPS */
void upsdrv_updateinfo(void)
{
	int	flags, ol, bl, ret;

	ret = ioctl(upsfd, TIOCMGET, &flags);

	if (ret != 0) {
		upslog_with_errno(LOG_INFO, "ioctl failed");
		ser_comm_fail("Status read failed");
		dstate_datastale();
		return;
	}

	ol = ((flags & upstab[upstype].line_ol) == upstab[upstype].val_ol);
	bl = ((flags & upstab[upstype].line_bl) == upstab[upstype].val_bl);

	status_init();

	if (bl) {
		status_set("LB");	/* low battery */
	}

	if (ol) {
		status_set("OL");	/* on line */
	} else {
		status_set("OB");	/* on battery */
	}

	status_commit();

	upsdebugx(5, "ups.status: %s %s\n", ol ? "OL" : "OB", bl ? "BL" : "");

	ser_comm_good();
	dstate_dataok();
}

/* show all possible UPS types */
static void listtypes(void)
{
	int	i;

	printf("Valid UPS types:\n\n");

	for (i = 0; upstab[i].mfr != NULL; i++) {
		printf("%i: %s\n", i, upstab[i].desc);
	}
}

/* set the flags for this UPS type */
static void set_ups_type(void)
{
	int	i;

	if (!getval("upstype")) {
		fatalx(EXIT_FAILURE, "No upstype set - see help text / man page!");
	}

	upstype = atoi(getval("upstype"));

	for (i = 0; upstab[i].mfr != NULL; i++) {

		if (upstype == i) {
			upslogx(LOG_INFO, "UPS type: %s\n", upstab[i].desc);
			return;
		}
	}

	listtypes();

	fatalx(EXIT_FAILURE, "\nFatal error: unknown UPS type number");
}

/* power down the attached load immediately */
void upsdrv_shutdown(void)
{
	int	flags, ret;

	if (upstype == -1) {
		fatalx(EXIT_FAILURE, "No upstype set - see help text / man page!");
	}

	flags = upstab[upstype].line_sd;

	if (flags == -1) {
		fatalx(EXIT_FAILURE, "No shutdown command defined for this model!");
	}

	if (flags == TIOCM_ST) {

#ifndef HAVE_TCSENDBREAK
		fatalx(EXIT_FAILURE, "Need to send a BREAK, but don't have tcsendbreak!");
#endif

		ret = tcsendbreak(upsfd, 4901);

		if (ret != 0) {
			fatal_with_errno(EXIT_FAILURE, "tcsendbreak");
		}

		return;
	}

	ret = ioctl(upsfd, TIOCMSET, &flags);

	if (ret != 0) {
		fatal_with_errno(EXIT_FAILURE, "ioctl TIOCMSET");
	}

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

int upsdrv_initups(void)
{
	struct termios	tio;
	char	*v;

	set_ups_type();

	upsfd = ser_open(device_path);

	if (tcgetattr(upsfd, &tio)) {
		fatal_with_errno(EXIT_FAILURE, "tcgetattr");
	}

	/* don't hang up on last close */
	tio.c_cflag &= ~HUPCL;

	if (tcsetattr(upsfd, TCSANOW, &tio)) {
		fatal_with_errno(EXIT_FAILURE, "tcsetattr");
	}

	/*
	 See if the user wants to override the output signal definitions
	 this must be done here, since we might go to upsdrv_shutdown()
	 immediately. Input signal definition override is handled in
	 upsdrv_initinfo()
	 */
	if ((v = getval("CP")) != NULL) {
		parse_output_signals(v, &upstab[upstype].line_norm);
		upsdebugx(2, "parse_output_signals: CP overridden with %s\n", v);
	}

	if ((v = getval("SD")) != NULL) {
		parse_output_signals(v, &upstab[upstype].line_sd);
		upsdebugx(2, "parse_output_signals: SD overridden with %s\n", v);
	}

	if (ioctl(upsfd, TIOCMSET, &upstab[upstype].line_norm)) {
		fatal_with_errno(EXIT_FAILURE, "ioctl TIOCMSET");
	}

	return 1;
}

void upsdrv_cleanup(void)
{
	ser_close(upsfd, device_path);
}

