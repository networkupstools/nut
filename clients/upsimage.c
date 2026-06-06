/* upsimage - cgi program to create graphical ups information reports

   Status:
     20020814 - Simon Rozman
       - redesigned the meters
     20020823 - Simon Rozman
       - added support for width, height and scale_height parameters
       - added support for outvolt
       - noimage now writes out a clue, why upsimage failed
     20020902 - Simon Rozman
       - background now transparent by default
       - added support for colorization parameters
       - removed linear antialiasing of the scale, until I come up with a better algorithm
     20020913 - Simon Rozman
       - added width, height and scale_height to imgarg table
     20020928 - Simon Rozman
       - added imgvar table to hold description, how to draw each UPS variable supported
       - added support for ACFREQ, OUT_FREQ and UPSTEMP

   Copyrights:
     (C) 1998  Russell Kroll <rkroll@exploits.org>
     (C) 2002  Simon Rozman <simon@rozman.net>
     (C) 2020-2026 Jim Klimov <jimklimov+nut@gmail.com>

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

#include "common.h"
#include "nut_stdint.h"
#include "upsclient.h"
#include "cgilib.h"
#include <stdlib.h>
#include <math.h>
#include <gd.h>
#include <gdfontmb.h>

#include "upsimagearg.h"

#define MAX_CGI_STRLEN 64

/* name-swap in libupsclient consumer to simplify the look of code base */
#define builtin_setproctag(x)	setproctag(x)
#define setproctag(x)	do { builtin_setproctag(x); upscli_upslog_setproctag(x, nut_common_cookie()); } while(0)

/* network timeout for initial connection, in seconds */
#define UPSCLI_DEFAULT_CONNECT_TIMEOUT	"10"

static	char	*monhost = NULL, *cmd = NULL;

static	uint16_t	port;
static	char	*upsname, *hostname;
static	UPSCONN_t	ups;

#define RED(x)		((x >> 16) & 0xff)
#define GREEN(x)	((x >> 8)  & 0xff)
#define BLUE(x)		(x & 0xff)


void parsearg(char *var, char *value)
{
	long long	i, v;	/* Be big enough to fit all expected inputs; truncate later */

	/* avoid bogus junk from evil people */
	if ((strlen(var) > MAX_CGI_STRLEN) || (strlen(value) > MAX_CGI_STRLEN))
		return;

	if (!strcmp(var, "host")) {
		free(monhost);
		monhost = xstrdup(value);
		return;
	}

	if (!strcmp(var, "display")) {
		free(cmd);
		cmd = xstrdup(value);
		return;
	}

	/* see if this is one of the shared (upsimagearg.h) variables */
	for (i = 0; imgarg[i].name != NULL; i++) {
		if (!strcmp(imgarg[i].name, var)) {
			if (!strncmp(value, "0x", 2))
				v = (long long)strtoul(value + 2, (char **)NULL, 16);
			else
				v = (long long)atoi(value);

			/* avoid false numbers from bad people */
			if (v < imgarg[i].min)
				imgarg[i].val = imgarg[i].min;
			else if (v > imgarg[i].max)
				imgarg[i].val = imgarg[i].max;
			else {
				assert (v < INT_MAX);
				assert (v > INT_MIN);
				imgarg[i].val = (int)v;
			}
			return;
		}
	}
}

/* return the value from the URL or the default if it wasn't set */
static int get_imgarg(const char *name)
{
	int	i;

	for (i = 0; imgarg[i].name != NULL; i++)
		if (!strcmp(imgarg[i].name, name))
			return imgarg[i].val;

	return -1;
}

/* write the HTML header then have gd dump the image */
static void drawimage(gdImagePtr im)
	__attribute__((noreturn));

static void drawimage(gdImagePtr im)
{
	printf("Pragma: no-cache\n");
	printf("Content-type: image/png\n\n");

	gdImagePng(im, stdout);
	gdImageDestroy(im);

	upscli_disconnect(&ups);

	exit(EXIT_SUCCESS);
}

/* helper function to allocate color in the image */
static int color_alloc(gdImagePtr im, int rgb)
{
	return gdImageColorAllocate(im, RED(rgb), GREEN(rgb), BLUE(rgb));
}

/* draws the scale behind the bar indicator */
static void drawscale(
	gdImagePtr im,				/* image where we would like to draw scale */
	int lvllo, int lvlhi,			/* min and max numbers on the scale */
	int step, int step5, int step10,	/* steps for minor, submajor and major dashes */
	int redlo1, int redhi1,			/* first red zone start and end */
	int redlo2, int redhi2,			/* second red zone start and end */
	int grnlo, int grnhi,			/* green zone start and end */
	int scale)				/* scaling factor for decimal precision */
{
	int	col1, col2, back_color, scale_num_color, ok_zone_maj_color,
		ok_zone_min_color, neutral_zone_maj_color,
		neutral_zone_min_color, warn_zone_maj_color,
		warn_zone_min_color;
	char		lbltxt[SMALLBUF];
	int		y, level, range;
	int		width, height, scale_height;

	back_color		= color_alloc(im, get_imgarg("back_col"));
	scale_num_color		= color_alloc(im, get_imgarg("scale_num_col"));
	ok_zone_maj_color	= color_alloc(im, get_imgarg("ok_zone_maj_col"));
	ok_zone_min_color	= color_alloc(im, get_imgarg("ok_zone_min_col"));
	neutral_zone_maj_color	= color_alloc(im, get_imgarg("neutral_zone_maj_col"));
	neutral_zone_min_color	= color_alloc(im, get_imgarg("neutral_zone_min_col"));
	warn_zone_maj_color	= color_alloc(im, get_imgarg("warn_zone_maj_col"));
	warn_zone_min_color	= color_alloc(im, get_imgarg("warn_zone_min_col"));

	width = get_imgarg("width");
	height = get_imgarg("height");
	scale_height = get_imgarg("scale_height");

	/* start out with a background color and make it transparent */
	gdImageFilledRectangle(im, 0, 0, width, height, back_color);
	gdImageColorTransparent(im, back_color);

	range = lvlhi - lvllo;
	if (range <= 0) {
		/* Prevent division by zero on collapsed ranges */
		lvlhi = lvllo + step10;
		range = step10;
	}

	/* draw scale to correspond with the values */
	for (level = lvlhi; level >= lvllo; level -= step) {
		/* select dash RGB color according to the level */
		if (((redlo1 <= level) && (level <=redhi1)) ||
			((redlo2 <= level) && (level <=redhi2))) {
			col1 = warn_zone_maj_color;
			col2 = warn_zone_min_color;
		} else if ((grnlo <= level) && (level <= grnhi)) {
			col1 = ok_zone_maj_color;
			col2 = ok_zone_min_color;
		} else {
			col1 = neutral_zone_maj_color;
			col2 = neutral_zone_min_color;
		}

		/* calculate integer value for y */
		y = scale_height * (lvlhi - level) / range;

		/* draw major, semimajor or minor dash accordingly */
		if (level % step10 == 0) {
			gdImageLine(im, 0, y, width, y, col1);
		} else {
			if (level % step5 == 0)
				gdImageLine(im, 5, y, width - 5, y, col2);
			else
				gdImageLine(im, 10, y, width - 10, y, col2);
		}
	}

	/* put the values on the scale */
	for (level = lvlhi; level >= lvllo; level -= step) {
		if (level % step10 == 0) {
			y = scale_height * (lvlhi - level) / range;
			if (scale == 1) {
				/* Whole-number scale */
				snprintf(lbltxt, sizeof(lbltxt), "%d", level);
			} else {
				/* How many digits after the decimal point?.. */
				int precision = 0, s;
				for (s = scale; s > 1; s /= 10)
					precision++;
				snprintf(lbltxt, sizeof(lbltxt), "%.*f",
					precision, level / (double) scale);
			}
			gdImageString(im, gdFontMediumBold,
				width - (int)(strlen(lbltxt)) * gdFontMediumBold->w,
				y, (unsigned char *) lbltxt, scale_num_color);
		}
	}
}

/* draws the bar style indicator */
static void drawbar(
	int lvllo, int lvlhi,			/* min and max numbers on the scale */
	int step, int step5, int step10,	/* steps for minor, submajor and major dashes */
	int redlo1, int redhi1,			/* first red zone start and end */
	int redlo2, int redhi2,			/* second red zone start and end */
	int grnlo, int grnhi,			/* green zone start and end */
	int scale,					/* scaling factor for decimal precision */
	double value, 				/* scaled UPS variable value to draw */
	double display_value, 		/* unscaled value for text output */
	const char *format			/* printf style format to be used when rendering summary text */
)
	__attribute__((noreturn));

static void drawbar(
	int lvllo, int lvlhi,			/* min and max numbers on the scale */
	int step, int step5, int step10,	/* steps for minor, submajor and major dashes */
	int redlo1, int redhi1,			/* first red zone start and end */
	int redlo2, int redhi2,			/* second red zone start and end */
	int grnlo, int grnhi,			/* green zone start and end */
	int scale,					/* scaling factor for decimal precision */
	double value, 				/* UPS variable value to draw */
	double display_value, 		/* unscaled value for text output */
	const char *format			/* printf style format to be used when rendering summary text */
)
{
	gdImagePtr	im;
	int		bar_color, summary_color;
	char		text[SMALLBUF];
	double	value_range;
	int		bar_y;
	int		width, height, scale_height;

	/* get the dimension parameters */
	width = get_imgarg("width");
	height = get_imgarg("height");
	scale_height = get_imgarg("scale_height");

	/* create the image */
	im = gdImageCreate(width, height);

	/* draw the scale */
	drawscale(im, lvllo, lvlhi, step, step5, step10, redlo1, redhi1,
		redlo2, redhi2, grnlo, grnhi, scale);

	/* allocate colors for the bar and summary text */
	bar_color	= color_alloc(im, get_imgarg("bar_col"));
	summary_color	= color_alloc(im, get_imgarg("summary_col"));

	/* rescale UPS value to fit in the scale */
	value_range = (double)(lvlhi - lvllo);
	if (value_range == 0.0)
		value_range = 1.0;
	bar_y = (int)((1.0 - (value - lvllo) / value_range) * scale_height);

	/* sanity checks: */

	/* 1: if value is above maximum, then bar_y goes negative */
	if (bar_y < 0)
		bar_y = 0;

	/* 2: if value is below minimum, bar_y goes off the scale */
	if (bar_y > scale_height)
		bar_y = scale_height;

	/* draw it */
	gdImageFilledRectangle(im, 25, bar_y, width - 25, scale_height,
		bar_color);

	/* stick the text version of the value at the bottom center
	 * expected format is one of imgvar[] entries for "double value"
	 */
	snprintf_dynamic(text, sizeof(text), format, "%f", display_value);
	gdImageString(im, gdFontMediumBold,
		(width - (int)(strlen(text))*gdFontMediumBold->w)/2,
		height - gdFontMediumBold->h,
		(unsigned char *) text, summary_color);

	drawimage(im);

	/* NOTREACHED */
}

/* draws the error image */
static void noimage(const char *fmt, ...)
	__attribute__((noreturn));

static void noimage(const char *fmt, ...)
{
	gdImagePtr	im;
	int		back_color, summary_color;
	int		width, height;
	char		msg[SMALLBUF];
	va_list		ap;

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
	/* Note: Not converting to hardened NUT methods with dynamic
	 * format string checking, this one is used locally with
	 * fixed strings (and args) */
	/* FIXME: Actually, almost only fixed strings, no formatting
	 * needed here: one use-case of having a format, and another
	 * with externally prepared snprintf(). */
	vsnprintf(msg, sizeof(msg), fmt, ap);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
	va_end(ap);

	width = get_imgarg("width");
	height = get_imgarg("height");

	im = gdImageCreate(width, height);
	back_color = color_alloc(im, get_imgarg("back_col"));
	summary_color = color_alloc(im, get_imgarg("summary_col"));

	gdImageFilledRectangle(im, 0, 0, width, height, back_color);
	gdImageColorTransparent(im, back_color);

	if (width > height)
		gdImageString(im, gdFontMediumBold,
			(width - (int)(strlen(msg))*gdFontMediumBold->w)/2,
			(height - gdFontMediumBold->h)/2,
			(unsigned char *) msg, 	summary_color);
	else
		gdImageStringUp(im, gdFontMediumBold,
			(width - gdFontMediumBold->h)/2,
			(height + (int)(strlen(msg))*gdFontMediumBold->w)/2,
			(unsigned char *) msg, summary_color);

	drawimage(im);

	/* NOTE: Earlier code called noimage() and then exit(EXIT_FAILURE);
	 * to signal an error via process exit code. Now that drawimage()
	 * always ends with exit(EXIT_SUCCESS) - which might make webserver
	 * feel good - the command-line use if any suffers no error returns.
	 */

	/* NOTREACHED */
}

/* draws bar indicator when minimum, nominal or maximum values for the given
 * UPS variable can be determined.
 * deviation < 0 means that values below nom should be grey instead of green
 */
static void drawgeneralbar(double var, double min, double nom, double max,
		int deviation, 	const char *format)
	__attribute__((noreturn));

static void drawgeneralbar(double var, double min, double nom, double max,
		int deviation, 	const char *format)
{
	int	hi, lo, step1, step5, step10, graybelownom=0;
	double range, scale;

	if(deviation < 0) {
		deviation=-deviation;
		graybelownom=1;
	}

	if (((int)nom == -1) && (((int)min == -1) || ((int)max == -1)))
		noimage("Can't determine range");

	/* if min, max and nom are mixed up, arrange them appropriately */
	if ((int)nom != -1) {
		if ((int)min == -1)
			min = nom - 3*deviation;

		if ((int)max == -1)
			max = nom + 3*deviation;
	} else {
		/* if nominal value isn't available, assume, it's the
		 * average between min and max */
		nom = (min + max) / 2;
	}

	/* draw scale in the background */
	range = max - min;
	scale = 1.0;
	if (range < 10.0) {
		/* Use decimal precision for small ranges */
		scale = 10.0;
	}

	if (range <= 50.0) {
		/* the scale is sparse enough to draw finer scale */
		step1 = 1;
		step5 = 5;
		step10 = 10;
	} else if((max - min) <= 100.0) {
		step1 = 2;
		step5 = 10;
		step10 = 20;
	} else {
		step1 = 5;
		step5 = 20;
		step10 = 40;
	}

	/* round min and max points to get high and low numbers for graph */
	lo = (int)((floor((min - deviation) * scale / step10)) * step10);
	hi = (int)((ceil((max + deviation + step10/2) * scale / step10)) * step10);

	if(!graybelownom) {
		drawbar(lo, hi, step1, step5, step10, max * scale, hi, lo, min * scale,
				nom * scale - deviation * scale, nom * scale + deviation * scale,
				(int)scale, var * scale, var, format);
	}
	else {
		drawbar(lo, hi, step1, step5, step10, 0, min * scale, max * scale, hi,
				nom * scale, max * scale, (int)scale, var * scale, var, format);
	}

	/* NOTREACHED */
}

/* draws input and output voltage bar style indicators */
static void draw_utility(double var, double min, double nom, double max,
		int deviation, const char *format)
	__attribute__((noreturn));

static void draw_utility(double var, double min, double nom, double max,
		int deviation, const char *format)
{
	/* hack: deal with hardware that doesn't have known transfer points */
	if ((int)min == -1) {
		if(var < 200) {
			min = 90;
		}
		else if(var < 300) {
			min = 200;
		}
		else {
			min = 340;
		}
	}

	/* somewhere between 220 and 230 V, to keep everybody satisfied */
	if ((int)nom == -1) {
		if(var < 200) {
			nom = 110;
		}
		else if(var < 300) {
			nom = 225;
		}
		else {
			nom = 400;
		}
	}

	/* symmetrical around nom */
	if ((int)max == -1)
		max = nom+(nom-min);

	/* Acceptable range of voltage is 85%-110% of nominal voltage
	 * in EU at least. Be conservative and say +-10% */
	deviation = (int)(nom * 0.1);

	drawgeneralbar(var, min, nom, max, deviation, format);

	/* NOTREACHED */
}

/* draws battery.percent bar style indicator */
static void draw_battpct(double var, double min, double nom,
		double max, int deviation, const char *format)
	__attribute__((noreturn));

static void draw_battpct(double var, double min, double nom,
		double max, int deviation, const char *format)
{
	NUT_UNUSED_VARIABLE(nom);
	NUT_UNUSED_VARIABLE(max);
	NUT_UNUSED_VARIABLE(deviation);

	if (min < 0) {
		min = 50;
	}

	drawbar(0, 100, 2, 10, 20, 0, min, -1, -1, 80, 100, 1, var, var, format);
}

/* draws battery.voltage bar style indicator */
static void draw_battvolt(double var, double min, double nom, double max,
		int deviation, const char *format)
	__attribute__((noreturn));

static void draw_battvolt(double var, double min, double nom, double max,
		int deviation, const char *format)
{
	if((int)nom == -1) {
		/* Use a fixed set of reasonable nominal voltages, seems to
		 * be the only way to get reasonable behaviour during
		 * discharge */

		if(var < 9)
			nom = 6;
		else if(var < 18)
			nom = 12;
		else if(var < 30)
			nom = 24;
		else if(var < 60)
			nom = 48;
		else if(var < 120)
			nom = 96;
		else if(var < 230)
			nom = 192;
		else
			nom = 384;

	}

	if((int)min == -1) {
		min = (int)(nom/2*1.6+1); /* Assume a 2V cell is dead at 1.6V */
	}

	if((int)max == -1) {
		max = (int)(nom/2*2.3+1); /* Assume 2.3V float charge voltage */
	}

	if (nom < min || nom > max)
		nom = -1;

	deviation = (int)(-nom*0.05); /* 5% deviation from nominal voltage */
	if(deviation==0) {
		deviation = -1;
	}

	drawgeneralbar(var, min, nom, max, deviation, format);
}

/* draws ups.load bar style indicator */
static void draw_upsload(double var, double min,
		double nom, double max,
		int deviation, const char *format)
	__attribute__((noreturn));

static void draw_upsload(double var, double min,
		double nom, double max,
		int deviation, const char *format)
{
	NUT_UNUSED_VARIABLE(min);
	NUT_UNUSED_VARIABLE(nom);
	NUT_UNUSED_VARIABLE(max);
	NUT_UNUSED_VARIABLE(deviation);

	drawbar(0, 125, 5, 5, 25, 100, 125, -1, -1, 0, 50, 1, var, var, format);
}

/* draws temperature bar style indicator */
static void draw_temperature(double var, double min, double nom, double max,
		int deviation, const char *format)
	__attribute__((noreturn));

static void draw_temperature(double var, double min, double nom, double max,
		int deviation, const char *format)
{
	int	hi = get_imgarg("tempmax");
	int	lo = get_imgarg("tempmin");
	NUT_UNUSED_VARIABLE(nom);
	NUT_UNUSED_VARIABLE(deviation);

	drawbar(lo, hi, 1, 5, 10, lo, min, max, hi, -1, -1, 1, var, var, format);
}

/* draws humidity bar style indicator */
static void draw_humidity(double var, double min, double nom, double max,
		int deviation, const char *format)
	__attribute__((noreturn));

static void draw_humidity(double var, double min, double nom, double max,
		int deviation, const char *format)
{
	NUT_UNUSED_VARIABLE(nom);
	NUT_UNUSED_VARIABLE(deviation);

	drawbar(0, 100, 2, 10, 20, 0, min, max, 100, -1, -1, 1, var, var, format);
}

/** Retrieves the value of a variable from the UPS into the string buf
 *  Returns 1 if the variable was found, 0 if not found or an error occurred.
 */
static int get_var(const char *var, char *buf, size_t buflen)
{
	int	ret;
	size_t	numq, numa;
	const	char	*query[4];
	char	**answer;

	query[0] = "VAR";
	query[1] = upsname;
	query[2] = var;

	numq = 3;

	ret = upscli_get(&ups, numq, query, &numa, &answer);

	if (ret < 0)
		return 0;

	if (numa < numq)
		return 0;

	snprintf(buf, buflen, "%s", answer[3]);
	return 1;
}

static void clean_exit(void)
{
	/* Flush *our* output before possibly failing in third-party code
	 * (e.g. SSL libs), so client consumers have a chance to see it */
	fflush(stdout);
	fflush(stderr);

	upscli_cleanup();

	upsdebugx(1, "%s: finished, exiting", __func__);
}

int main(int argc, char **argv)
{
	char	str[SMALLBUF], *s, str_port[16];
	int	flags_ssl = UPSCLI_CONN_TRYSSL, i;
	upscli_authconf_t	*ac_conn = NULL;
	double	min, nom, max;
	double	var = 0;

#ifdef WIN32
	/* Required ritual before calling any socket functions */
	static WSADATA	WSAdata;
	static int	WSA_Started = 0;
	if (!WSA_Started) {
		WSAStartup(2, &WSAdata);
		atexit((void(*)(void))WSACleanup);
		WSA_Started = 1;
	}

	/* Avoid binary output conversions, e.g.
	 * mangling what looks like CRLF on WIN32 */
	setmode(STDOUT_FILENO, O_BINARY);
#endif

	upscli_upslog_start_sync(upslog_start_sync(NULL), nut_common_cookie());
	upscli_upslog_setprocname(xstrdup(getmyprocname()), nut_common_cookie());
	getprogname_argv0_default(argc > 0 ? argv[0] : NULL, "upsimage(CGI)");

	/* NOTE: Caller must `export NUT_DEBUG_LEVEL` to see debugs for upsc
	 * and NUT methods called from it. This line aims to just initialize
	 * the subsystem, and set initial timestamp. Debugging the client is
	 * primarily of use to developers, so is not exposed via `-D` args.
	 */
	s = getenv("NUT_DEBUG_LEVEL");
	if (s && str_to_int(s, &i, 10) && i > 0) {
		nut_debug_level = i;
		upscli_upslog_set_debug_level(nut_debug_level, nut_common_cookie());
	}

#ifdef NUT_CGI_DEBUG_UPSIMAGE
# if (NUT_CGI_DEBUG_UPSIMAGE - 0 < 1)
#  undef NUT_CGI_DEBUG_UPSIMAGE
#  define NUT_CGI_DEBUG_UPSIMAGE 6
# endif
	/* Un-comment via make flags when developer-troubleshooting: */
	nut_debug_level = NUT_CGI_DEBUG_UPSIMAGE;
	upscli_upslog_set_debug_level(nut_debug_level, nut_common_cookie());
#endif

	if (nut_debug_level > 0) {
		cgilogbit_set();
		printf("Content-type: text/html\n");
		printf("Pragma: no-cache\n");
		printf("\n");
		printf("<p>NUT CGI Debugging enabled, level: %d</p>\n\n", nut_debug_level);
	}

	extractcgiargs();

	upsdebugx(1, "Using best-effort auth config detection");
	upscli_read_authconf_file(NULL, 0);

	upscli_init_default_connect_timeout(NULL, NULL, UPSCLI_DEFAULT_CONNECT_TIMEOUT);
	atexit(clean_exit);

	ac_conn = upscli_get_authconf_item(NULL, hostname, snprintf(str_port, sizeof(str_port), "%" PRIu16, port) > 0 ? str_port : NULL, 1);
	if (ac_conn && upscli_init_authconf(ac_conn) > 0) {
		upscli_authconf_t	*ac_default = upscli_find_authconf_item(NULL, NULL, NULL);
		if (ac_default) {
			if (ac_default->certverify) {
				flags_ssl |= UPSCLI_CONN_CERTVERIF;
			}
			if (ac_default->forcessl) {
				flags_ssl ^= UPSCLI_CONN_TRYSSL;
				flags_ssl |= UPSCLI_CONN_REQSSL;
			}
		}
	}

	/* no 'host=' or 'display=' given */
	if ((!monhost) || (!cmd))
		noimage("No host or display");

	if (!checkhost(monhost, NULL))
		noimage("Access denied");

	upsname = hostname = NULL;

	if (upscli_splitname(monhost, &upsname, &hostname, &port) != 0) {
		noimage("Invalid UPS definition (upsname[@hostname[:port]])\n");
#ifndef HAVE___ATTRIBUTE__NORETURN
		exit(EXIT_FAILURE);	/* Should not get here in practice, but compiler is afraid we can fall through */
#endif
	}

	if (upscli_connect(&ups, hostname, port, flags_ssl) < 0) {
		noimage("Can't connect to server:\n%s\n",
			upscli_strerror(&ups));
#ifndef HAVE___ATTRIBUTE__NORETURN
		exit(EXIT_FAILURE);	/* Should not get here in practice, but compiler is afraid we can fall through */
#endif
	}

	/* TOTHINK #3411: Consider autologin via ac_conn->user/pass fields?
	 *  Probably no, not for a web client anyone can interact with...
	 *  This one is for a read-only listing, but could something be abused?
	 *  If it comes to that, better fall back to requiring query/form args
	 *  like in upsset.c
	 *  //upscli_authenticate_authconf(&ups, ac_conn);
	 */

	for (i = 0; imgvar[i].name; i++)
		if (!strcmp(cmd, imgvar[i].name)) {

			/* sanity check whether we have draw function
			 * registered with this variable */
			if (!imgvar[i].drawfunc) {
				noimage("Draw function N/A");
#ifndef HAVE___ATTRIBUTE__NORETURN
				exit(EXIT_FAILURE);	/* Should not get here in practice, but compiler is afraid we can fall through */
#endif
			}

			/* get the variable value */
			if (get_var(imgvar[i].name, str, sizeof(str)) == 1) {
				var = strtod(str, NULL);
			} else {
				/* no value, no fun */
				snprintf(str, sizeof(str), "%s N/A",
					imgvar[i].name);
				noimage(str);
#ifndef HAVE___ATTRIBUTE__NORETURN
				exit(EXIT_FAILURE);	/* Should not get here in practice, but compiler is afraid we can fall through */
#endif
			}

			/* when getting minimum, nominal and maximum values,
			 * we first look if the marginal value is supported
			 * by the UPS driver, if not, we look it up in the
			 * imgarg table under the SAME name
			 */

			/* get the minimum value */
			if (imgvar[i].minimum) {
				if (get_var(imgvar[i].minimum, str,
					sizeof(str)) == 1
				) {
					min = strtod(str, NULL);
				} else {
					min = get_imgarg(imgvar[i].minimum);
					/* min = -1; // AI? */
				}
			} else {
				min = -1;
			}

			/* get the nominal value */
			if (imgvar[i].nominal) {
				if (get_var(imgvar[i].nominal, str,
					sizeof(str)) == 1
				) {
					nom = strtod(str, NULL);
				} else {
					nom = get_imgarg(imgvar[i].nominal);
					/* nom = -1; // AI? */
				}
			} else {
				nom = -1;
			}

			/* get the maximum value */
			if (imgvar[i].maximum) {
				if (get_var(imgvar[i].maximum, str,
					sizeof(str)) == 1
				) {
					max = strtod(str, NULL);
				} else {
					max = get_imgarg(imgvar[i].maximum);
					/* max = -1; // AI? */
				}
			} else {
				max = -1;
			}

			imgvar[i].drawfunc(var, min, nom, max,
				imgvar[i].deviation, imgvar[i].format);
#ifndef HAVE___ATTRIBUTE__NORETURN
			exit(EXIT_SUCCESS);
#endif
		}

	noimage("Unknown display");
#ifndef HAVE___ATTRIBUTE__NORETURN
	exit(EXIT_FAILURE);
#endif
}

imgvar_t imgvar[] = {
	{ "input.voltage", "input.transfer.low", "input.voltage.nominal",
		"input.transfer.high", 0,
		"%.1f VAC", draw_utility				},

	{ "input.L1-N.voltage", "input.transfer.low", "input.voltage.nominal",
		"input.transfer.high", 0,
		"%.1f VAC", draw_utility				},

	{ "input.L2-N.voltage", "input.transfer.low", "input.voltage.nominal",
		"input.transfer.high", 0,
		"%.1f VAC", draw_utility				},

	{ "input.L3-N.voltage", "input.transfer.low", "input.voltage.nominal",
		"input.transfer.high", 0,
		"%.1f VAC", draw_utility				},

	{ "input.L1-L2.voltage", "input.transfer.low", "input.voltage.nominal",
		"input.transfer.high", 0,
		"%.1f VAC", draw_utility				},

	{ "input.L2-L3.voltage", "input.transfer.low", "input.voltage.nominal",
		"input.transfer.high", 0,
		"%.1f VAC", draw_utility				},

	{ "input.L3-L1.voltage", "input.transfer.low", "input.voltage.nominal",
		"input.transfer.high", 0,
		"%.1f VAC", draw_utility				},

	{ "battery.charge", "battery.charge.low", NULL, NULL, 0,
		"%.1f %%",	draw_battpct				},

	{ "battery.voltage", "battery.voltage.low", "battery.voltage.nominal",
		"battery.voltage.high", 0,
		"%.1f VDC",	draw_battvolt				},

	/* We use 'high' ASCII for the degrees symbol, since the gdImageString()
	 * function doesn't understand UTF-8 or HTML escape sequences. :-( */
	{ "ups.temperature", "ups.temperature.low", NULL,
		"ups.temperature.high", 0,
		"%.1f \260C",	draw_temperature			},

	/* Same here. */
	{ "ambient.temperature", "ambient.temperature.low", NULL,
		"ambient.temperature.high", 0,
		"%.1f \260C",	draw_temperature			},

	{ "ambient.humidity", "ambient.humidity.low", NULL,
		"ambient.humidity.high", 0,
		"%.1f %%",	draw_humidity				},

	{ "input.frequency", NULL, "input.frequency.nominal", NULL, 2,
		"%.1f Hz",	drawgeneralbar				},

	{ "ups.load", NULL, NULL, NULL, 0,
		"%.1f %%",	draw_upsload				},

	{ "output.L1.power.percent", NULL, NULL, NULL, 0,
		"%.1f %%",	draw_upsload				},

	{ "output.L2.power.percent", NULL, NULL, NULL, 0,
		"%.1f %%",	draw_upsload				},

	{ "output.L3.power.percent", NULL, NULL, NULL, 0,
		"%.1f %%",	draw_upsload				},

	{ "output.L1.realpower.percent", NULL, NULL, NULL, 0,
		"%.1f %%",	draw_upsload				},

	{ "output.L2.realpower.percent", NULL, NULL, NULL, 0,
		"%.1f %%",	draw_upsload				},

	{ "output.L3.realpower.percent", NULL, NULL, NULL, 0,
		"%.1f %%",	draw_upsload				},

	{ "output.voltage", "input.transfer.low", "output.voltage.nominal",
		"input.transfer.high", 0,
		"%.1f VAC",	draw_utility				},

	{ "output.L1-N.voltage", "input.transfer.low",
	 	"output.voltage.nominal", "input.transfer.high", 0,
		"%.1f VAC",	draw_utility				},

	{ "output.L2-N.voltage", "input.transfer.low",
	 	"output.voltage.nominal", "input.transfer.high", 0,
		"%.1f VAC",	draw_utility				},

	{ "output.L3-N.voltage", "input.transfer.low",
	 	"output.voltage.nominal", "input.transfer.high", 0,
		"%.1f VAC",	draw_utility				},

	{ "output.L1-L2.voltage", "input.transfer.low",
	 	"output.voltage.nominal", "input.transfer.high", 0,
		"%.1f VAC",	draw_utility				},

	{ "output.L2-L3.voltage", "input.transfer.low",
	 	"output.voltage.nominal", "input.transfer.high", 0,
		"%.1f VAC",	draw_utility				},

	{ "output.L3-L1.voltage", "input.transfer.low",
	 	"output.voltage.nominal", "input.transfer.high", 0,
		"%.1f VAC",	draw_utility				},

	{ "output.frequency", NULL, "output.frequency.nominal", NULL, 2,
		"%.1f Hz",	drawgeneralbar				},

	{ NULL,		NULL,		NULL,		NULL,		0,
		NULL,		NULL }
};
