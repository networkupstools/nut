/* upshandler.h - function callbacks used by the drivers

   Copyright (C) 2003  Russell Kroll <rkroll@exploits.org>
   Copyright (C) 2025  Jim Klimov <jimklimov+nut@gmail.com>

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
	STAT_INSTCMD_HANDLED = 0,		/* completed successfully */
	STAT_INSTCMD_UNKNOWN,			/* unspecified error */
	STAT_INSTCMD_INVALID,			/* invalid command */
	STAT_INSTCMD_FAILED,			/* command failed */
	STAT_INSTCMD_CONVERSION_FAILED	/* could not convert value */
};

/* return values for setvar */
enum {
	STAT_SET_HANDLED = 0,			/* completed successfully */
	STAT_SET_UNKNOWN,				/* unspecified error */
	STAT_SET_INVALID,				/* not writeable */
	STAT_SET_FAILED,				/* writing failed */
	STAT_SET_CONVERSION_FAILED		/* could not convert value from string */
};

/* structure for funcs that get called by msg parse routine */
struct ups_handler
{
	int	(*setvar)(const char *, const char *);
	int	(*instcmd)(const char *, const char *);
};

/* Log levels; packagers might want to fiddle with NOTICE vs. WARN or ERR,
 * maybe even CRIT for shutdown commands?.. or we might eventually tie this
 * into nut_debug_level run-time verbosity.
 * Up to NUT v2.8.3 everything was LOG_NOTICE except in a few drivers.
 * Conversion to these macros allows all drivers to behave consistently.
 */
#ifndef UPSHANDLER_LOGLEVEL_DIFFERENTIATE
# define UPSHANDLER_LOGLEVEL_DIFFERENTIATE	1 /* 0 to LOG_NOTICE everything by default */
#endif

#ifndef LOG_INSTCMD_UNKNOWN
# if UPSHANDLER_LOGLEVEL_DIFFERENTIATE
#  define	LOG_INSTCMD_UNKNOWN	LOG_WARNING
# else
#  define	LOG_INSTCMD_UNKNOWN	LOG_NOTICE
# endif
#endif

#ifndef LOG_INSTCMD_INVALID
# if UPSHANDLER_LOGLEVEL_DIFFERENTIATE
#  define	LOG_INSTCMD_INVALID	LOG_WARNING
# else
#  define	LOG_INSTCMD_INVALID	LOG_NOTICE
# endif
#endif

#ifndef LOG_INSTCMD_FAILED
# if UPSHANDLER_LOGLEVEL_DIFFERENTIATE
#  define	LOG_INSTCMD_FAILED	LOG_ERR
# else
#  define	LOG_INSTCMD_FAILED	LOG_NOTICE
# endif
#endif

#ifndef LOG_INSTCMD_CONVERSION_FAILED
# if UPSHANDLER_LOGLEVEL_DIFFERENTIATE
#  define	LOG_INSTCMD_CONVERSION_FAILED	LOG_ERR
# else
#  define	LOG_INSTCMD_CONVERSION_FAILED	LOG_WARNING
# endif
#endif

/* FIXME: Define here and apply in drivers log levels for mere HANDLED
 *  states (INFO), and for instcmd about shutdowns (CRIT) in particular;
 *  see bestfortress.c for some practical inspiration.
 */

#ifndef LOG_SET_UNKNOWN
# if UPSHANDLER_LOGLEVEL_DIFFERENTIATE
#  define	LOG_SET_UNKNOWN	LOG_WARNING
# else
#  define	LOG_SET_UNKNOWN	LOG_NOTICE
# endif
#endif

#ifndef LOG_SET_INVALID
# if UPSHANDLER_LOGLEVEL_DIFFERENTIATE
#  define	LOG_SET_INVALID	LOG_WARNING
# else
#  define	LOG_SET_INVALID	LOG_NOTICE
# endif
#endif

#ifndef LOG_SET_FAILED
# if UPSHANDLER_LOGLEVEL_DIFFERENTIATE
#  define	LOG_SET_FAILED	LOG_ERR
# else
#  define	LOG_SET_FAILED	LOG_NOTICE
# endif
#endif

#ifndef LOG_SET_CONVERSION_FAILED
# if UPSHANDLER_LOGLEVEL_DIFFERENTIATE
#  define	LOG_SET_CONVERSION_FAILED	LOG_ERR
# else
#  define	LOG_SET_CONVERSION_FAILED	LOG_WARNING
# endif
#endif

/* Call upslogx() with certain log level and wording/markup, so different
 * driver programs present a consistent picture to their end-users.
 */
/* instcmd(), upscmd() et al -- note an argument is optional: */
#define upsdebug_INSTCMD_STARTING(cmdname, extra)	do {		\
	upsdebugx(1, "Starting %s::%s('%s', '%s')",		\
		__FILE__, __func__, NUT_STRARG(cmdname), NUT_STRARG(extra));	\
	} while(0)

#define upslog_INSTCMD_UNKNOWN(cmdname, extra)	do {		\
	upslogx(LOG_INSTCMD_UNKNOWN, "%s: unknown command [%s] [%s]",	\
		__func__, NUT_STRARG(cmdname), NUT_STRARG(extra));	\
	} while(0)

#define upslog_INSTCMD_INVALID(cmdname, extra)	do {		\
	upslogx(LOG_INSTCMD_INVALID, "%s: parameter [%s] is invalid for command [%s]",	\
		__func__, NUT_STRARG(extra), NUT_STRARG(cmdname));	\
	} while(0)

#define upslog_INSTCMD_FAILED(cmdname, extra)	do {		\
	upslogx(LOG_INSTCMD_FAILED, "%s: FAILED command [%s] [%s]",	\
		__func__, NUT_STRARG(cmdname), NUT_STRARG(extra));	\
	} while(0)

/* setvar(), setcmd() et al -- note they MUST have an argument: */
#define upsdebug_SET_STARTING(varname, value)	do {		\
	upsdebugx(1, "Starting %s::%s('%s', '%s')",		\
		__FILE__, __func__, NUT_STRARG(varname), NUT_STRARG(value));	\
	} while(0)

#define upslog_SET_UNKNOWN(varname, value)	do {		\
	upslogx(LOG_SET_UNKNOWN, "%s: unknown variable [%s] [%s]",	\
		__func__, NUT_STRARG(varname), NUT_STRARG(value));	\
	} while(0)

#define upslog_SET_INVALID(varname, value)	do {		\
	upslogx(LOG_SET_INVALID, "%s: value [%s] is invalid for variable [%s]",	\
		__func__, NUT_STRARG(value), NUT_STRARG(varname));	\
	} while(0)

#define upslog_SET_FAILED(varname, value)	do {		\
	upslogx(LOG_SET_FAILED, "%s: FAILED to set variable [%s] to value [%s]",	\
		__func__, NUT_STRARG(varname), NUT_STRARG(value));	\
	} while(0)

#endif /* NUT_UPSHANDLER_H */
