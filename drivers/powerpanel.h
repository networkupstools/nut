/*
 * powerpanel.h - Model specific data/definitions for CyberPower text/binary
 *                protocol UPSes
 *
 * Copyright (C)
 *	2007        Doug Reynolds <mav@wastegate.net>
 *	2007-2008   Arjen de Korte <adkorte-guest@alioth.debian.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef POWERPANEL_H
#define POWERPANEL_H

#define ENDCHAR		'\r'
#define IGNCHAR		""
#define MAXTRIES	3
#define UPSDELAY	50000

#define SER_WAIT_SEC	0
#define SER_WAIT_USEC	250000

typedef struct {
	const char	*version;
	int	(*instcmd)(const char *cmdname, const char *extra);
	int	(*setvar)(const char *varname, const char *val);
	int	(*initups)(void);
	void	(*initinfo)(void);
	int	(*updateinfo)(void);
} subdriver_t;

#endif /* POWERPANEL_H */
