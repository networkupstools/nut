/* status.h - translation of status abbreviations to descriptions

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

#ifndef NUT_STATUS_H_SEEN
#define NUT_STATUS_H_SEEN 1

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

struct {
	char	*name;
	char	*desc;
	int	severity;
}	stattab[] =
{
	{ "OFF",	"OFF",			1	},
	{ "OL",		"ONLINE",		0	},
	{ "OB",		"ON BATTERY",		2	},
	{ "LB",		"LOW BATTERY",		2	},
	{ "RB",		"REPLACE BATTERY",	2	},
	{ "OVER",	"OVERLOAD",		2	},
	{ "TRIM",	"VOLTAGE TRIM",		1	},
	{ "BOOST",	"VOLTAGE BOOST",	1	},
	{ "CAL",	"CALIBRATION",		1	},
	{ "BYPASS",	"BYPASS",		2	},
	{ NULL,		NULL,			0	}
};

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif	/* NUT_STATUS_H_SEEN */
