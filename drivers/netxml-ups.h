/* netxml-ups.h	Driver data/defines for network XML UPS units 

   Copyright (C)
	2008		Arjen de Korte <adkorte-guest@alioth.debian.org>

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

#ifndef NETXML_UPS_H
#define NETXML_UPS_H

struct subdriver_s {
	const char	*version;		/* name of this subdriver */
	char		*initups;
	char		*initinfo;
	char		*updateinfo;
	int		(*startelm_cb)(void *userdata, int parent, const char *nspace, const char *name, const char **atts);
	int		(*cdata_cb)(void *userdata, int state, const char *cdata, size_t len);
	int		(*endelm_cb)(void *userdata, int state, const char *nspace, const char *name);
};

typedef struct subdriver_s subdriver_t;

#endif /* NETXML_UPS_H */
