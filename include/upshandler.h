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

/* structure for funcs that get called by msg parse routine */
struct ups_handler
{
	int	(*setvar)(const char *, const char *);
	int	(*instcmd)(const char *, const char *);
};

#endif /* NUT_UPSHANDLER_H */
