/* neterr.h - network error definitions for NUT

   Copyright (C)
	2003	Russell Kroll <rkroll@exploits.org>
	2005	Arnaud Quette <arnaud.quette@free.fr>
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

/* network error definitions for consistency */

#define NUT_ERR_ACCESS_DENIED		"ACCESS-DENIED"
#define NUT_ERR_UNKNOWN_UPS		"UNKNOWN-UPS"
#define NUT_ERR_VAR_NOT_SUPPORTED	"VAR-NOT-SUPPORTED"
#define NUT_ERR_CMD_NOT_SUPPORTED	"CMD-NOT-SUPPORTED"
#define NUT_ERR_INVALID_ARGUMENT	"INVALID-ARGUMENT"
#define NUT_ERR_INSTCMD_FAILED		"INSTCMD-FAILED"
#define NUT_ERR_SET_FAILED		"SET-FAILED"
#define NUT_ERR_READONLY		"READONLY"
#define NUT_ERR_TOO_LONG		"TOO-LONG"
#define NUT_ERR_FEATURE_NOT_SUPPORTED	"FEATURE-NOT-SUPPORTED"
#define NUT_ERR_FEATURE_NOT_CONFIGURED	"FEATURE-NOT-CONFIGURED"
#define NUT_ERR_ALREADY_SSL_MODE	"ALREADY-SSL-MODE"

/* errors which are only used by top-level upsd functions */

#define NUT_ERR_DRIVER_NOT_CONNECTED	"DRIVER-NOT-CONNECTED"
#define NUT_ERR_DATA_STALE		"DATA-STALE"
#define NUT_ERR_ALREADY_LOGGED_IN	"ALREADY-LOGGED-IN"
#define NUT_ERR_INVALID_PASSWORD	"INVALID-PASSWORD"
#define NUT_ERR_ALREADY_SET_PASSWORD	"ALREADY-SET-PASSWORD"
#define NUT_ERR_INVALID_USERNAME	"INVALID-USERNAME"
#define NUT_ERR_ALREADY_SET_USERNAME	"ALREADY-SET-USERNAME"
#define NUT_ERR_USERNAME_REQUIRED	"USERNAME-REQUIRED"
#define NUT_ERR_PASSWORD_REQUIRED	"PASSWORD-REQUIRED"
#define NUT_ERR_UNKNOWN_COMMAND		"UNKNOWN-COMMAND"

/* errors which are only used with the old functions */

#define NUT_ERR_VAR_UNKNOWN		"VAR-UNKNOWN"
#define NUT_ERR_UNKNOWN_TYPE		"UNKNOWN-TYPE"
#define NUT_ERR_UNKNOWN_INSTCMD		"UNKNOWN-INSTCMD"
#define NUT_ERR_MISSING_ARGUMENT	"MISSING-ARGUMENT"
#define NUT_ERR_INVALID_VALUE		"INVALID-VALUE"
