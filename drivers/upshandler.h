/* upshandler.h - function callbacks used by the drivers

   Copyright (C) 2003  Russell Kroll <rkroll@exploits.org>

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

#ifndef NUT_UPSHANDLER_H
#define NUT_UPSHANDLER_H

/* return values for instcmd */
enum {
	STAT_INSTCMD_HANDLED = 0,	/* completed successfully */
	STAT_INSTCMD_UNKNOWN,		/* unspecified error */
	STAT_INSTCMD_INVALID,		/* invalid command */
	STAT_INSTCMD_FAILED		/* command failed */
};

/* return values for setvar */
enum {
	STAT_SET_HANDLED = 0,	/* completed successfully */
	STAT_SET_UNKNOWN,	/* unspecified error */
	STAT_SET_INVALID,	/* not writeable */
	STAT_SET_FAILED		/* writing failed */
};

/* structure for funcs that get called by msg parse routine */
struct ups_handler
{
	int	(*setvar)(const char *, const char *);
	int	(*instcmd)(const char *, const char *);
};

#endif /* NUT_UPSHANDLER_H */
