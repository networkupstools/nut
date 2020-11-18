/* upsimagearg.h - arguments passed between upsstats and upsimage

   Copyright (C) 2002  Russell Kroll <rkroll@exploits.org>

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

#ifndef NUT_UPSIMAGEARG_H_SEEN
#define NUT_UPSIMAGEARG_H_SEEN 1

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

struct {
	char	*name;
	int	val;		/* hex digits, ala HTML */
	int	min;		/* minimum reasonable value */
	int	max;		/* maximum reasonable value */
}	imgarg[] =
{
	{ "width",                           100,       50,      200 },
	{ "height",                          350,      100,      500 },
	{ "scale_height",                    300,      100,      500 },
	{ "back_col",			0x000000, 0x000000, 0xffffff },
	{ "scale_num_col",		0xffff00, 0x000000, 0xffffff },
	{ "summary_col",		0xffff00, 0x000000, 0xffffff },
	{ "ok_zone_maj_col",		0x00ff00, 0x000000, 0xffffff },
	{ "ok_zone_min_col",		0x007800, 0x000000, 0xffffff },
	{ "neutral_zone_maj_col",	0xffffff, 0x000000, 0xffffff },
	{ "neutral_zone_min_col",	0x646464, 0x000000, 0xffffff },
	{ "warn_zone_maj_col",		0xff0000, 0x000000, 0xffffff },
	{ "warn_zone_min_col",		0x960000, 0x000000, 0xffffff },
	{ "bar_col",			0x00ff00, 0x000000, 0xffffff },
	{ "tempmin",                           0,     -100,      150 },
	{ "tempmax",                          40,     -100,      150 },
	{ "nom_in_freq",                      50,        0,      100 },
	{ "nom_out_freq",                     50,        0,      100 },
	{ NULL, 0, 0, 0 }
};

typedef struct {
	char	*name;		/* name of the UPS variable                 */
	char	*minimum;	/* name of minimum value UPS variable
	                       or variable in imgarg table              */
	char	*nominal;	/* as above, only for nominal value         */
	char	*maximum;	/* as above, only for maximum value         */
	int	deviation;	/* variable deviation - width of green zone */
	char	*format;	/* format string to generate summary text   */

				/* pointer to drawing function              */
	void	(*drawfunc)(double, int, int, int, int, const char*);
} imgvar_t;

extern imgvar_t imgvar[];

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif	/* NUT_UPSIMAGEARG_H_SEEN */
