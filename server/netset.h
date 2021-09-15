/* netset.h - SET handler for upsd

   Copyright (C)
	2003	Russell Kroll <rkroll@exploits.org>
	2005	Arnaud Quette <arnaud.quette@free.fr>
	2007	Peter Selinger <selinger@users.sourceforge.net>
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

#ifndef NUT_NETSET_H_SEEN
#define NUT_NETSET_H_SEEN 1

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

typedef enum 
{
	SET_VAR_CHECK_VAL_OK = 0,
	SET_VAR_CHECK_VAL_VAR_NOT_SUPPORTED,
	SET_VAR_CHECK_VAL_READONLY,
	SET_VAR_CHECK_VAL_SET_FAILED,
	SET_VAR_CHECK_VAL_TOO_LONG,
	SET_VAR_CHECK_VAL_INVALID_VALUE
} set_var_check_val_t;

set_var_check_val_t set_var_check_val(upstype_t *ups, const char *var, const char *newval);
int do_set_var(upstype_t *ups, const char *var, const char *newval);

void net_set(nut_ctype_t *client, size_t numarg, const char **arg);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif /* NUT_NETSET_H_SEEN */
