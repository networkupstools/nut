/* netcmds.h - upsd support structure details

   Copyright (C) 2001  Russell Kroll <rkroll@exploits.org>
	2005	Arnaud Quette <arnaud.quette@free.fr>
	2007	Peter Selinger <selinger@users.sourceforge.net>
	2010	Arjen de Korte <adkorte-guest@alioth.debian.org>
	2012	Emilien Kia <kiae.dev@gmail.com>
	2020	Jim Klimov <jimklimov@gmail.com>

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

#ifndef NUT_NETCMDS_H_SEEN
#define NUT_NETCMDS_H_SEEN 1

#include "nut_ctype.h"

#include "netssl.h"
#include "netget.h"
#include "netset.h"
#include "netlist.h"
#include "netmisc.h"
#include "netuser.h"
#include "netinstcmd.h"

#define FLAG_USER	0x0001		/* username and password must be set */

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

static struct {
	const	char	*name;
	void	(*func)(nut_ctype_t *client, int numargs, const char **arg);
	int	flags;
} netcmds[] = {
	{ "VER",	net_ver,	0		},
	{ "NETVER",	net_netver,	0		},
	{ "HELP",	net_help,	0		},
	{ "STARTTLS",	net_starttls,	0		},

	{ "GET",	net_get,	0		},
	{ "LIST",	net_list,	0		},

	{ "USERNAME",	net_username,	0		},
	{ "PASSWORD",	net_password,	0		},

	{ "LOGIN",	net_login,	FLAG_USER	},
	{ "LOGOUT", 	net_logout,	0		},
	{ "MASTER",	net_master,	FLAG_USER	},

	{ "FSD",	net_fsd,	FLAG_USER	},

	{ "SET",	net_set,	FLAG_USER	},
	{ "INSTCMD",	net_instcmd,	FLAG_USER	},

	{ NULL,		(void(*)(void))(NULL), 0		}
};

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* NUT_NETCMDS_H_SEEN */
