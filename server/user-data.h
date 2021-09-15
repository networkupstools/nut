/* user-data.h - structures for user.c

   Copyright (C) 2001  Russell Kroll <rkroll@exploits.org>
	2005	Arnaud Quette <arnaud.quette@free.fr>
	2007	Peter Selinger <selinger@users.sourceforge.net>
	2008	Arjen de Korte <adkorte-guest@alioth.debian.org>
	2013	Emilien Kia <kiae.dev@gmail.com>
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

#ifndef NUT_USERDATA_H_SEEN
#define NUT_USERDATA_H_SEEN 1

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

typedef struct {
	char	*cmd;
	void	*next;
} instcmdlist_t;

typedef struct {
	char	*action;
	void	*next;
} actionlist_t;

typedef struct {
	char	*username;
	char	*password;
	instcmdlist_t *firstcmd;
	actionlist_t  *firstaction;
	void	*next;
} ulist_t;

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* NUT_USERDATA_H_SEEN */
