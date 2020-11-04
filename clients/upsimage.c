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
#include "upsclient.h"
#include "cgilib.h"
#include <stdlib.h>
#include <gd.h>
#include <gdfontmb.h>

#include "upsimagearg.h"

#define MAX_CGI_STRLEN 64

static	char	*monhost = NULL, *cmd = NULL;

static	int	port;
static	char	*upsname, *hostname;
static	UPSCONN_t	ups;

#define RED(x)		((x >> 16) & 0xff)
#define GREEN(x)	((x >> 8)  & 0xff)
#define BLUE(x)		(x & 0xff)


void parsearg(char *var, char *value)
{
	int	i, v;

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
				v = strtoul(value + 2, (char **)NULL, 16);
			else
				v = atoi(value);

			/* avoid false numbers from bad people */
			if (v < imgarg[i].min)
				imgarg[i].val = imgarg[i].min;
			else if (v > imgarg[i].max)
				imgarg[i].val = imgarg[i].max;
			else
				imgarg[i].val = v;
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
	int grnlo, int grnhi)			/* green zone start and end */
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
			snprintf(lbltxt, sizeof(lbltxt), "%d", level);
			gdImageString(im, gdFontMediumBold, width - strlen(lbltxt)*gdFontMediumBold->w, y,
				(unsigned char *) lbltxt, scale_num_color);
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
	double value, 				/* UPS variable value to draw */
	const char *format			/* printf style format to be used when rendering summary text */
)
{
	gdImagePtr	im;
	int		bar_color, summary_color;
	char		text[SMALLBUF];
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
		redlo2, redhi2, grnlo, grnhi);

	/* allocate colors for the bar and summary text */
	bar_color	= color_alloc(im, get_imgarg("bar_col"));
	summary_color	= color_alloc(im, get_imgarg("summary_col"));

	/* rescale UPS value to fit in the scale */
	bar_y = (1 - (value - lvllo) / (lvlhi - lvllo)) * scale_height;

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

	/* stick the text version of the value at the bottom center */
	snprintf(text, sizeof(text), format, value);
	gdImageString(im, gdFontMediumBold,
		(width - strlen(text)*gdFontMediumBold->w)/2,
		height - gdFontMediumBold->h,
		(unsigned char *) text, summary_color);

	drawimage(im);

	/* NOTREACHED */
}

/* draws the error image */
static void noimage(const char *fmt, ...)
{
	gdImagePtr	im;
	int		back_color, summary_color;
	int		width, height;
	char		msg[SMALLBUF];
	va_list		ap;

	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
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
			(width - strlen(msg)*gdFontMediumBold->w)/2,
			(height - gdFontMediumBold->h)/2,
			(unsigned char *) msg, 	summary_color);
	else
		gdImageStringUp(im, gdFontMediumBold,
			(width - gdFontMediumBold->h)/2,
			(height + strlen(msg)*gdFontMediumBold->w)/2,
			(unsigned char *) msg, summary_color);

	drawimage(im);

	/* NOTREACHED */
}

/* draws bar indicator when minimum, nominal or maximum values for the given
   UPS variable can be determined.
   deviation < 0 means that values below nom should be grey instead of
   green */
static void drawgeneralbar(double var, int min, int nom, int max,
		int deviation, 	const char *format)
{
	int	hi, lo, step1, step5, step10, graybelownom=0;

	if(deviation < 0) {
		deviation=-deviation;
		graybelownom=1;
	}

	if ((nom == -1) && ((min == -1) || (max == -1)))
		noimage("Can't determine range");

	/* if min, max and nom are mixed up, arrange them appropriately */
	if (nom != -1) {
		if (min == -1)
			min = nom - 3*deviation;

		if (max == -1)
			max = nom + 3*deviation;
	} else {
		/* if nominal value isn't available, assume, it's the
		   average between min and max */
		nom = (min + max) / 2;
	}

	/* draw scale in the background */
	if ((max - min) <= 50) {
		/* the scale is sparse enough to draw finer scale */
		step1 = 1;
		step5 = 5;
		step10 = 10;
	} else if((max - min) <= 100) {
		step1 = 2;
		step5 = 10;
		step10 = 20;
	} else {
		step1 = 5;
		step5 = 20;
		step10 = 40;
	}

	/* round min and max points to get high and low numbers for graph */
	lo = ((min - deviation) / step10) * step10;
	hi = ((max + deviation + step10/2) / step10) * step10;

	if(!graybelownom) {
		drawbar(lo, hi, step1, step5, step10, max, hi, lo, min,
				nom - deviation, nom + deviation, var, format);
	}
	else {
		drawbar(lo, hi, step1, step5, step10, 0, min, max, hi,
				nom, max, var, format);
	}

	/* NOTREACHED */
}

/* draws input and output voltage bar style indicators */
static void draw_utility(double var, int min, int nom, int max,
		int deviation, const char *format)
{
	/* hack: deal with hardware that doesn't have known transfer points */
	if (min == -1) {
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
	if (nom == -1) {
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
	if (max == -1)
		max = nom+(nom-min);

	/* Acceptable range of voltage is 85%-110% of nominal voltage
	 * in EU at least. Be conservative and say +-10% */
	deviation = nom*0.1;

	drawgeneralbar(var, min, nom, max, deviation, format);

	/* NOTREACHED */
}

/* draws battery.percent bar style indicator */
static void draw_battpct(double var, int min, int NUT_UNUSED(nom),
		int NUT_UNUSED(max), int NUT_UNUSED(deviation), const char *format)
{
	if (min < 0) {
		min = 50;
	}

	drawbar(0, 100, 2, 10, 20, 0, min, -1, -1, 80, 100, var, format);
}

/* draws battery.voltage bar style indicator */
static void draw_battvolt(double var, int min, int nom, int max,
		int deviation, const char *format)
{
	if(nom == -1) {
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

	if(min == -1) {
		min = nom/2*1.6+1; /* Assume a 2V cell is dead at 1.6V */
	}

	if(max == -1) {
		max = nom/2*2.3+1; /* Assume 2.3V float charge voltage */
	}

	if (nom < min || nom > max)
		nom = -1;


	deviation = -(nom*0.05); /* 5% deviation from nominal voltage */
	if(deviation==0) {
		deviation = -1;
	}

	drawgeneralbar(var, min, nom, max, deviation, format);
}

/* draws ups.load bar style indicator */
static void draw_upsload(double var, int NUT_UNUSED(min),
		int NUT_UNUSED(nom), int NUT_UNUSED(max),
		int NUT_UNUSED(deviation), const char *format)
{
	drawbar(0, 125, 5, 5, 25, 100, 125, -1, -1, 0, 50, var, format);
}

/* draws temperature bar style indicator */
static void draw_temperature(double var, int min, int NUT_UNUSED(nom), int max,
		int NUT_UNUSED(deviation), const char *format)
{
	int	hi = get_imgarg("tempmax");
	int	lo = get_imgarg("tempmin");

	drawbar(lo, hi, 1, 5, 10, lo, min, max, hi, -1, -1, var, format);
}

/* draws humidity bar style indicator */
static void draw_humidity(double var, int min, int NUT_UNUSED(nom), int max,
		int NUT_UNUSED(deviation), const char *format)
{
	drawbar(0, 100, 2, 10, 20, 0, min, max, 100, -1, -1, var, format);
}

static int get_var(const char *var, char *buf, size_t buflen)
{
	int	ret;
	unsigned int	numq, numa;
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

int main(int NUT_UNUSED(argc), char ** NUT_UNUSED(argv))
{
	char	str[SMALLBUF];
	int	i, min, nom, max;
	double	var = 0;

	extractcgiargs();

	/* no 'host=' or 'display=' given */
	if ((!monhost) || (!cmd))
		noimage("No host or display");

	if (!checkhost(monhost, NULL))
		noimage("Access denied");

	upsname = hostname = NULL;

	if (upscli_splitname(monhost, &upsname, &hostname, &port) != 0) {
		noimage("Invalid UPS definition (upsname[@hostname[:port]])\n");
		exit(EXIT_FAILURE);
	}

	if (upscli_connect(&ups, hostname, port, 0) < 0) {
		noimage("Can't connect to server:\n%s\n",
			upscli_strerror(&ups));
		exit(EXIT_FAILURE);
	}

	for (i = 0; imgvar[i].name; i++)
		if (!strcmp(cmd, imgvar[i].name)) {

			/* sanity check whether we have draw function
			   registered with this variable */
			if (!imgvar[i].drawfunc) {
				noimage("Draw function N/A");
				exit(EXIT_FAILURE);
			}

			/* get the variable value */
			if (get_var(imgvar[i].name, str, sizeof(str)) == 1) {
				var = strtod(str, NULL);
			} else {
				/* no value, no fun */
				snprintf(str, sizeof(str), "%s N/A",
					imgvar[i].name);
				noimage(str);
				exit(EXIT_FAILURE);
			}

			/* when getting minimum, nominal and maximum values,
			   we first look if the marginal value is supported
			   by the UPS driver, if not, we look it up in the
			   imgarg table under the SAME name */

			/* get the minimum value */
			if (imgvar[i].minimum) {
				if (get_var(imgvar[i].minimum, str,
					sizeof(str)) == 1) {
					min = atoi(str);
				} else {
					min = get_imgarg(imgvar[i].minimum);
				}

			} else {
				min = -1;
			}

			/* get the nominal value */
			if (imgvar[i].nominal) {
				if (get_var(imgvar[i].nominal, str,
					sizeof(str)) == 1) {
					nom = atoi(str);
				} else {
					nom = get_imgarg(imgvar[i].nominal);
				}

			} else {
				nom = -1;
			}

			/* get the maximum value */
			if (imgvar[i].maximum) {
				if (get_var(imgvar[i].maximum, str,
					sizeof(str)) == 1) {
					max = atoi(str);
				} else {
					max = get_imgarg(imgvar[i].maximum);
				}

			} else {
				max = -1;
			}

			imgvar[i].drawfunc(var, min, nom, max,
				imgvar[i].deviation, imgvar[i].format);
			exit(EXIT_SUCCESS);
		}

	noimage("Unknown display");
	exit(EXIT_FAILURE);
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
