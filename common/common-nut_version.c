/* nut_version.c - data and functions regarding just NUT version reporting
                   (extracted from common.c to minimize the compilation unit
                   impacted by git metadata changes during development)

   Copyright (C) 2021-2025  Jim Klimov <jimklimov+nut@gmail.com>

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

#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#ifndef WIN32
# include <syslog.h>
#else	/* WIN32 */
# include "wincompat.h"
#endif	/* WIN32 */

/* the reason we define UPS_VERSION as a static string, rather than a macro,
 * is to make dependency tracking easier (only common-nut_version.o file
 * depends on nut_version.h), and also to prevent all sources from having
 * to be recompiled each time the version changes (they only need to be
 * re-linked). Similarly for other variables below in the code.
 */
#include "nut_version.h"
const char *UPS_VERSION = NUT_VERSION_MACRO;

/* Know which bitness we were built for, to adjust the report */
#include "nut_stdint.h"
#if defined(UINTPTR_MAX) && (UINTPTR_MAX + 0) == 0xffffffffffffffffULL
# define BUILD_64   1
#else
# ifdef BUILD_64
#  undef BUILD_64
# endif
#endif

#if defined(UINTPTR_MAX) && (UINTPTR_MAX + 0) == 0x00000000ffffffffULL
# define BUILD_32   1
#else
# ifdef BUILD_32
#  undef BUILD_32
# endif
#endif

/* privately declare a few things implemented in common.c (this code used
 * to be there) */
extern struct timeval	upslog_start;
extern int	upslog_flags;
int xbit_test(int val, int flag);

int banner_is_disabled(void)
{
	static int	value = -1;

	if (value < 0) {
		char *s = getenv("NUT_QUIET_INIT_BANNER");
		/* Envvar present and empty or true-ish means NUT tool name+version
		 * banners disabled by the setting: default is enabled (inversed per
		 * method name) */
		value = 0;
		if (s) {
			if (*s == '\0' || !strcasecmp(s, "true") || strcmp(s, "1")) {
				value = 1;
			}
		}
	}

	return value;
}

const char *describe_NUT_VERSION_once(void)
{
	static char	buf[LARGEBUF];
	static const char	*printed = NULL;

	if (printed)
		return printed;

	memset(buf, 0, sizeof(buf));

#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
#pragma GCC diagnostic ignored "-Wunreachable-code"
#endif
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunreachable-code"
#endif
	/* NOTE: Some compilers deduce that macro-based decisions about
	 * NUT_VERSION_IS_RELEASE make one of codepaths unreachable in
	 * a particular build. So we pragmatically handwave this away.
	 */
	if (1 < snprintf(buf, sizeof(buf),
		"%s %s%s%s",
		NUT_VERSION_MACRO,
		NUT_VERSION_IS_RELEASE ? "release" :
			(NUT_VERSION_IS_PRERELEASE
			 ? "(pre-release iteration of "
			 : "(development iteration after "),
		NUT_VERSION_IS_RELEASE ? "" : NUT_VERSION_SEMVER_MACRO,
		NUT_VERSION_IS_RELEASE ? "" : ")"
	)) {
		printed = buf;
	} else {
		upslogx(LOG_WARNING, "%s: failed to report detailed NUT version", __func__);
		printed = UPS_VERSION;
	}
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
#pragma GCC diagnostic pop
#endif

	return printed;
}

int print_banner_once(const char *prog, int even_if_disabled)
{
	static int	printed = 0;
	static int	ret = -1;

	if (printed)
		return ret;

	if (!banner_is_disabled() || even_if_disabled) {
		ret = printf("Network UPS Tools %s %s%s\n",
			prog, describe_NUT_VERSION_once(),
			even_if_disabled == 2 ? "\n" : "");
		fflush(stdout);
		if (ret > 0)
			printed = 1;
	}

	return ret;
}

void nut_report_config_flags(void)
{
	/* Roughly similar to upslogx() but without the buffer-size limits and
	 * timestamp/debug-level prefixes. Only printed if debug (any) is on.
	 * Depending on amount of configuration tunables involved by a particular
	 * build of NUT, the string can be quite long (over 1KB).
	 */
#if 0
	const char	*acinit_ver = NULL;
#endif
	/* Pass these as variables to avoid warning about never reaching one
	 * of compiled codepaths: */
	const char	*compiler_ver = CC_VERSION;
	const char	*config_flags = CONFIG_FLAGS;
	const char	*BITNESS_STR = NULL;
	const char	*CPU_TYPE_STR = NULL;
	struct timeval		now;

	if (nut_debug_level < 1)
		return;

#ifdef BUILD_64
	BITNESS_STR = "64";
#else
# ifdef BUILD_32
	BITNESS_STR = "32";
# endif
#endif

#ifdef CPU_TYPE
	if (CPU_TYPE && *CPU_TYPE)
		CPU_TYPE_STR = CPU_TYPE;
#endif

#if 0
	/* Only report git revision if NUT_VERSION_MACRO in nut_version.h aka
	 * UPS_VERSION here is remarkably different from PACKAGE_VERSION from
	 * configure.ac AC_INIT() -- which may be e.g. "2.8.0.1" although some
	 * distros, especially embedders, tend to place their product IDs here).
	 * The macro may be that fixed version or refer to git source revision,
	 * as decided when generating nut_version.h (and if it was re-generated
	 * in case of rebuilds while developers are locally iterating -- this
	 * may be disabled for faster local iterations at a cost of a little lie).
	 */
	if (PACKAGE_VERSION && UPS_VERSION &&
		(strlen(UPS_VERSION) < 12 || !strstr(UPS_VERSION, PACKAGE_VERSION))
	) {
		/* If UPS_VERSION is too short (so likely a static string
		 * from configure.ac AC_INIT() -- although some distros,
		 * especially embedders, tend to place their product IDs here),
		 * or if PACKAGE_VERSION *is NOT* a substring of it: */
		acinit_ver = PACKAGE_VERSION;
/*
		// Triplet that was printed below:
		(acinit_ver ? " (release/snapshot of " : ""),
		(acinit_ver ? acinit_ver : ""),
		(acinit_ver ? ")" : ""),
*/
	}
#endif

	/* NOTE: If changing wording here, keep in sync with configure.ac logic
	 * looking for CONFIG_FLAGS_DEPLOYED via "configured with flags:" string!
	 */

	gettimeofday(&now, NULL);

	if (upslog_start.tv_sec == 0) {
		upslog_start = now;
	}

	if (upslog_start.tv_usec > now.tv_usec) {
		now.tv_usec += 1000000;
		now.tv_sec -= 1;
	}

	if (xbit_test(upslog_flags, UPSLOG_STDERR)) {
		fprintf(stderr, "%4.0f.%06ld\t[D1] Network UPS Tools version %s%s%s%s%s%s%s%s%s%s %s%s\n",
			difftime(now.tv_sec, upslog_start.tv_sec),
			(long)(now.tv_usec - upslog_start.tv_usec),
			describe_NUT_VERSION_once(),
			BITNESS_STR ? ", " : "",
			BITNESS_STR ? BITNESS_STR : "",
			BITNESS_STR ? "-bit build" : "",
			CPU_TYPE_STR ? " for " : "",
			CPU_TYPE_STR ? CPU_TYPE_STR : "",
			BITNESS_STR ? "," : "",
			(compiler_ver && *compiler_ver != '\0' ? " built with " : ""),
			(compiler_ver && *compiler_ver != '\0' ? compiler_ver : ""),
			(compiler_ver && *compiler_ver != '\0' ? " and" : ""),
			(config_flags && *config_flags != '\0' ? "configured with flags: " : "configured all by default guesswork"),
			(config_flags && *config_flags != '\0' ? config_flags : "")
		);
#ifdef WIN32
		fflush(stderr);
#endif	/* WIN32 */
	}

	/* NOTE: May be ignored or truncated by receiver if that syslog server
	 * (and/or OS sender) does not accept messages of such length */
	if (xbit_test(upslog_flags, UPSLOG_SYSLOG)) {
		syslog(LOG_DEBUG, "Network UPS Tools version %s%s%s%s%s%s%s%s%s%s %s%s",
			describe_NUT_VERSION_once(),
			BITNESS_STR ? ", " : "",
			BITNESS_STR ? BITNESS_STR : "",
			BITNESS_STR ? "-bit build" : "",
			CPU_TYPE_STR ? " for " : "",
			CPU_TYPE_STR ? CPU_TYPE_STR : "",
			BITNESS_STR ? "," : "",
			(compiler_ver && *compiler_ver != '\0' ? " built with " : ""),
			(compiler_ver && *compiler_ver != '\0' ? compiler_ver : ""),
			(compiler_ver && *compiler_ver != '\0' ? " and" : ""),
			(config_flags && *config_flags != '\0' ? "configured with flags: " : "configured all by default guesswork"),
			(config_flags && *config_flags != '\0' ? config_flags : "")
		);
	}
}

static const char *do_suggest_doc_links(const char *progname, const char *progconf, const char *mansection) {
	static char	buf[LARGEBUF];

	buf[0] = '\0';

	if (progname) {
		char	*s = NULL, *buf2 = xstrdup(xbasename(progname));
		size_t	i;

		for (i = 0; buf2[i]; i++) {
			buf2[i] = tolower((unsigned char)(buf2[i]));
		}

		if ((s = strstr(buf2, ".exe")) && strcmp(buf2, "nut.exe"))
			*s = '\0';

		snprintf(buf, sizeof(buf),
			"For more information please ");
#if defined(WITH_DOCS) && WITH_DOCS
		/* FIXME: Currently all NUT tools and drivers are in same
		 *  man page section for "System Management Programs".
		 *  If this ever changes (e.g. clients like `upsc` can be
		 *  a "User Program" just as well), we may need an extra
		    method argument here.
		 */
		snprintfcat(buf, sizeof(buf),
			"Read The Fine Manual ('man %s %s') and/or ",
			mansection, buf2);
#else
		NUT_UNUSED_VARIABLE(mansection);
#endif
		snprintfcat(buf, sizeof(buf),
			"see\n\t%s/docs/man/%s.html\n",
			NUT_WEBSITE_BASE, buf2);

		free(buf2);
	}

	if (progconf)
		snprintfcat(buf, sizeof(buf),
			"%s check documentation and samples of %s\n",
			progname ? "Also" : "Please",
			progconf);

	return buf;
}

const char *suggest_doc_links_CMD_SYS(const char *progname, const char *progconf) {
	return do_suggest_doc_links(progname, progconf, MAN_SECTION_CMD_SYS);
}

const char *suggest_doc_links_CMD_USR(const char *progname, const char *progconf) {
	return do_suggest_doc_links(progname, progconf, MAN_SECTION_CMD_USR);
}
