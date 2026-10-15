/*
 *  Copyright (C) 2011 - 2024 Arnaud Quette (Design and part of implementation)
 *  Copyright (C) 2011 - 2021 EATON
 *  Copyright (C) 2016 - 2021 Jim Klimov <EvgenyKlimov@eaton.com>
 *  Copyright (C) 2021 - 2026 Jim Klimov <jimklimov+nut@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/*! \file nutscan-init.c
    \brief init functions for nut scanner library
    \author Frederic Bohe <fredericbohe@eaton.com>
    \author Arnaud Quette <ArnaudQuette@Eaton.com>
    \author Arnaud Quette <arnaudquette@free.fr>
    \author Jim Klimov <jimklimov+nut@gmail.com>
*/

#include "common.h"
#include "nutscan-init.h"
#include <ltdl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "nut-scan.h"
#include "nut_platform.h"
#include "nut_stdint.h"

/* Note: generated during build in $(top_builddir)/clients/
 * and should be ensured to be here at the right moment by
 * the nut-scanner Makefile. */
#include "libupsclient-version.h"

#ifdef WIN32
# if defined HAVE_WINSOCK2_H && HAVE_WINSOCK2_H
#  include <winsock2.h>
# endif
#endif	/* WIN32 */

/* Flags for code paths we can support in this run (libs available or not
 * needed). For consistency, only set non-zero values via nutscan_init() call.
 */
int nutscan_avail_avahi = 0;
int nutscan_avail_ipmi = 0;
int nutscan_avail_nut = 0;	/* aka oldnut detection via libupsclient compared to avahi as newnut */
int nutscan_avail_nut_simulation = 0;
int nutscan_avail_snmp = 0;
int nutscan_avail_usb = 0;
int nutscan_avail_xml_http = 0;
int nutscan_avail_upower = 0;

/* Methods defined in scan_*.c source files */
int nutscan_load_usb_library(const char *libname_path);
int nutscan_unload_usb_library(void);
int nutscan_load_snmp_library(const char *libname_path);
int nutscan_unload_snmp_library(void);
int nutscan_load_neon_library(const char *libname_path);
int nutscan_unload_neon_library(void);
int nutscan_load_avahi_library(const char *libname_path);
int nutscan_unload_avahi_library(void);
int nutscan_load_ipmi_library(const char *libname_path);
int nutscan_unload_ipmi_library(void);
int nutscan_load_upsclient_library(const char *libname_path);
int nutscan_unload_upsclient_library(void);
void nutscan_upscli_set_debug_level(int level, const void *cookie);
void nutscan_upscli_setprocname(const char *pn, const void *cookie);
void nutscan_upscli_setproctag(const char *tag, const void *cookie);
struct timeval *nutscan_upscli_upslog_start_sync(struct timeval *tv, const void *cookie);
int nutscan_load_upower_library(const char *libname_path);
int nutscan_unload_upower_library(void);

/* privately exported from common.c for internal libs */
const char *setproctag_lib_once(const char *val);

#ifdef HAVE_PTHREAD
# ifdef HAVE_SEMAPHORE_UNNAMED
/* Shared by library consumers, exposed by nutscan_semaphore() below */
static sem_t semaphore;

sem_t * nutscan_semaphore(void)
{
	return &semaphore;
}

void nutscan_semaphore_set(sem_t *s)
{
	NUT_UNUSED_VARIABLE(s);
}
# elif defined HAVE_SEMAPHORE_NAMED
/* Shared by library consumers, exposed by nutscan_semaphore() below.
 * Methods like sem_open() return the pointer and sem_close() frees its data.
 */
static sem_t *semaphore = NULL;	/* TOTHINK: maybe SEM_FAILED? */

sem_t * nutscan_semaphore(void)
{
	return semaphore;
}

void nutscan_semaphore_set(sem_t *s)
{
	semaphore = s;
}
# endif /* HAVE_SEMAPHORE_UNNAMED || HAVE_SEMAPHORE_NAMED */

# ifdef HAVE_PTHREAD_TRYJOIN
pthread_mutex_t threadcount_mutex;
# endif

# if (defined HAVE_PTHREAD_TRYJOIN) || (defined HAVE_SEMAPHORE_UNNAMED) || (defined HAVE_SEMAPHORE_NAMED)
/* We have 3 networked scan types: nut, snmp, xml,
 * and users typically give their /24 subnet as "-m" arg.
 * With some systems having a 1024 default (u)limit to
 * file descriptors, this should fit if those are involved.
 * On some systems tested, a large amount of not-joined
 * pthreads did cause various crashes; also RAM is limited.
 * Note that each scan may be time consuming to query an
 * IP address and wait for (no) reply, so while these threads
 * are usually not resource-intensive (nor computationally),
 * they spend much wallclock time each so parallelism helps.
 */
size_t max_threads = DEFAULT_THREAD;
size_t curr_threads = 0;

size_t max_threads_netxml = 1021; /* experimental finding, see PR#1158 */
size_t max_threads_oldnut = 1021;
size_t max_threads_netsnmp = 0; /* 10240; */
	/* per reports in PR#1158, some versions of net-snmp could be limited
	 * to 1024 threads in the past; this was not found in practice.
	 * Still, some practical limit can be useful (configurable?)
	 * Here 0 means to not apply any special limit (beside max_threads).
	 */
size_t max_threads_ipmi = 0;	/* limits not yet known */

# endif  /* HAVE_PTHREAD_TRYJOIN || HAVE_SEMAPHORE_UNNAMED || HAVE_SEMAPHORE_NAMED */

#endif /* HAVE_PTHREAD */

#ifdef WIN32
/* Stub for libupsclient, no need to register a callback for ENABLE_SHARED_PRIVATE_LIBS builds */
void do_upsconf_args(char *confupsname, char *var, char *val) {
	NUT_UNUSED_VARIABLE(confupsname);
	NUT_UNUSED_VARIABLE(var);
	NUT_UNUSED_VARIABLE(val);
}
#endif	/* WIN32 */

const void *nutscan_upslog_cookie(void)
{
	return nut_common_cookie();
}

void nutscan_upslog_set_debug_level(int level, const void *cookie) {
	nut_debug_level = level;

	if (cookie != nutscan_upslog_cookie())
		setproctag_lib_once("libnutscan");

	/* Succeeds after init and if the library is loaded, else no-op */
	nutscan_upscli_set_debug_level(level, cookie ? cookie : nutscan_upslog_cookie());

}

int nutscan_upslog_get_debug_level(void)
{
	return nut_debug_level;
}

/* Avoid re-querying /proc or equivalent and logging about it,
 * if the caller is a NUT program that already knows its name:
 * see getmyprocname() in NUT common library */
void nutscan_upslog_setprocname(const char *pn, const void *cookie)
{
	if (cookie != nutscan_upslog_cookie())
		setproctag_lib_once("libnutscan");

	setmyprocname(pn);
	/* Succeeds after init and if the library is loaded, else no-op */
	nutscan_upscli_setprocname(pn, cookie ? cookie : nutscan_upslog_cookie());
}

void nutscan_upslog_setproctag(const char *tag, const void *cookie)
{
	if (cookie != nutscan_upslog_cookie())
		setproctag_lib_once("libnutscan");

	setproctag(tag);
	/* Succeeds after init and if the library is loaded, else no-op */
	nutscan_upscli_setproctag(tag, cookie ? cookie : nutscan_upslog_cookie());
}

const char *nutscan_upslog_getproctag(void)
{
	return getproctag();
}

struct timeval *nutscan_upslog_start_sync(struct timeval *tv, const void *cookie)
{
	/* No-op if internal tv equals passed tv */
	struct timeval *nstv = upslog_start_sync(tv);

	if (cookie != nutscan_upslog_cookie())
		setproctag_lib_once("libnutscan");

	/* Succeeds after init and if the library is loaded, else no-op */
	nutscan_upscli_upslog_start_sync(nstv, cookie ? cookie : nutscan_upslog_cookie());

	return nstv;
}

/* Names are borrowed. unload_first preserves the loader's failed-attempt
 * sentinel: even the first compiled fallback can require a reset. */
typedef struct {
	const char *name;
	const char *skip_if_equal_to;
	int unload_first;
} nutscan_library_candidate_t;

static void nutscan_select_library(const char *caller, const char *label,
	const nutscan_library_candidate_t *resolve,
	const nutscan_library_candidate_t *fallback,
	int (*load)(const char *), int (*unload)(void), int *avail)
{
	const nutscan_library_candidate_t *candidate;
	char *libname = NULL;

	for (candidate = resolve; candidate->name; candidate++) {
		if (candidate->skip_if_equal_to
		&& !strcmp(candidate->name, candidate->skip_if_equal_to)
		) {
			continue;
		}
		libname = get_libname(candidate->name);
		if (libname) {
			break;
		}
	}

	if (libname) {
		upsdebugx(1, "%s: get_libname() resolved '%s' for %s, loading it",
			caller, libname, label);
		*avail = load(libname);
		free(libname);
		return;
	}

	/* let libtool (lt_dlopen) do its default magic maybe better.
	 * Note that each failed attempt is remembered in the module
	 * handle, so we unload it before trying the next candidate
	 * name (otherwise nutscan_load_*_library() short-circuits on
	 * that memory and the name is never really tried). */
	upsdebugx(1, "%s: get_libname() did not resolve libname for %s, "
		"trying to load it with libtool default resolver", caller, label);
	for (candidate = fallback; !*avail && candidate->name; candidate++) {
		if (candidate->skip_if_equal_to
		&& !strcmp(candidate->name, candidate->skip_if_equal_to)
		) {
			continue;
		}
		if (candidate->unload_first) {
			unload();
		}
		*avail = load(candidate->name);
	}
}

/* One verdict per library at the default debug level. The individual
 * nutscan_load_*_library() calls report each candidate name they try,
 * and why it failed, only at debug level 1: nutscan_init() may try
 * several names before one works, so a per-attempt message at level 0
 * would claim the search was disabled while the next name still loads.
 */
static void nutscan_report_load(const char *func, int avail,
	const char *libdesc, const char *scantype)
{
	if (avail) {
		upsdebugx(1, "%s: succeeded to load the library for %s",
			func, libdesc);
	} else {
		upsdebugx(0, "Cannot load %s, %s search disabled.",
			libdesc, scantype);
	}
}

void nutscan_init(void)
{
#ifdef WIN32
	/* Required ritual before calling any socket functions */
	static WSADATA	WSAdata;
	static int	WSA_Started = 0;
	if (!WSA_Started) {
		WSAStartup(2, &WSAdata);
		atexit((void(*)(void))WSACleanup);
		WSA_Started = 1;
	}
#endif	/* WIN32 */

	upsdebugx(1, "%s: starting...", __func__);

	/* Optional filter to not walk things twice */
	nut_prepare_search_paths();

	/* Report library paths we would search, at given debug verbosity level */
	upsdebugx_report_search_paths(1, 1);

#ifdef HAVE_PTHREAD
/* TOTHINK: Should semaphores to limit thread count
 * and the more naive but portable methods be an
 * if-else proposition? At least when initializing?
 */
# if (defined HAVE_SEMAPHORE_UNNAMED) || (defined HAVE_SEMAPHORE_NAMED)
	/* NOTE: This semaphore may get re-initialized in nut-scanner program
	 * after parsing command-line arguments. It calls nutscan_init() before
	 * parsing CLI, to know about available libs and to set defaults below.
	 */
#include "nut-pragmas-unreachable-code.h"
	/* Different platforms, different sizes, none fits all... */
	if (SIZE_MAX > UINT_MAX && max_threads > UINT_MAX) {
#include "nut-pragmas-unreachable-code-end.h"
		upsdebugx(1,
			"WARNING: %s: Limiting max_threads to range acceptable for " REPORT_SEM_INIT_METHOD "()",
			__func__);
		max_threads = UINT_MAX - 1;
	}

	upsdebugx(1, "%s: Parallel scan support: max_threads=%" PRIuSIZE,
		__func__, max_threads);
#  ifdef HAVE_SEMAPHORE_UNNAMED
	if (sem_init(&semaphore, 0, (unsigned int)max_threads)) {
		upsdebug_with_errno(4, "%s: Parallel scan support: " REPORT_SEM_INIT_METHOD "() failed", __func__);
	}
#  elif defined HAVE_SEMAPHORE_NAMED
	/* FIXME: Do we need O_EXCL here? */
	if (SEM_FAILED == (semaphore = sem_open(SEMNAME_TOPLEVEL, O_CREAT, 0644, (unsigned int)max_threads))) {
		upsdebug_with_errno(4, "%s: Parallel scan support: " REPORT_SEM_INIT_METHOD "() failed", __func__);
		semaphore = NULL;
	}
#  endif
# endif

# ifdef HAVE_PTHREAD_TRYJOIN
	pthread_mutex_init(&threadcount_mutex, NULL);
# endif
#endif	/* HAVE_PTHREAD */

	/* Below we proceed to try loading dynamic libraries at run-time.
	 * This allows a pre-built `nut-scanner` binary to not require all
	 * of the possible NUT prerequisites to be installed on a system
	 * (needlessly expanding its footprint) and allows to package the
	 * tool with conservative formal dependencies.
	 *
	 * From the build time we can remember full SOPATH_LIB<X> and/or
	 * base SOFILE_LIB<X> with specific library file names available
	 * on the build system (with ".so.X.Y.Z" extensions or "libX-YZ.dll"
	 * embedded version identifiers), as well as SONAME_LIB<X> with the
	 * name the run-time linker itself would look for (e.g. "libX.so.N").
	 *
	 * The SONAME is tried first: an OS package update of the library
	 * which keeps its ABI (so the SONAME stays the same) usually renames
	 * the actual file (".so.N.Y.Z" -> ".so.N.Y+1.0"), and then a NUT
	 * binary that only knew the old file name would silently lose that
	 * scanning ability until it got rebuilt (issue #3474).
	 *
	 * Historically we allow run-time environments to override the
	 * library search paths (e.g. for bundled NUT installers that
	 * may be incompatible with OS library builds), so we use full
	 * SOPATH_LIB<X> as the last option, but prefer a presumably
	 * known-compatible SOFILE_LIB<X> before that.
	 *
	 * Worth noting that at least on macOS the base name of the
	 * install name (SOFILE_LIB<X>) already holds the version,
	 * e.g. `libusb-1.0.0.dylib` and would be same as SONAME_LIB<X>,
	 * so we check for duplicates below to avoid log clutter and
	 * time overheads to search for a failed filename again.
	 */

#if (defined WITH_USB) && WITH_USB
	{
		static const nutscan_library_candidate_t candidates[] = {
# if WITH_LIBUSB_1_0
#  ifdef SONAME_LIBUSB1
			{ SONAME_LIBUSB1, NULL, 0 },
#  endif	/* SONAME_LIBUSB1 */
#  ifdef SOFILE_LIBUSB1
#   ifdef SONAME_LIBUSB1
			{ SOFILE_LIBUSB1, SONAME_LIBUSB1, 1 },
#   else	/* not SONAME_LIBUSB1 */
			{ SOFILE_LIBUSB1, NULL, 0 },
#   endif	/* SONAME_LIBUSB1 */
#  endif	/* SOFILE_LIBUSB1 */
			{ "libusb-1.0" SOEXT, NULL, 1 },
#  ifdef SOPATH_LIBUSB1
			{ SOPATH_LIBUSB1, NULL, 1 },
#  endif	/* SOPATH_LIBUSB1 */
			/* We can also use libusb-compat from newer libusb-1.0 releases */
			{ "libusb" SOEXT, NULL, 1 },
# else	/* not WITH_LIBUSB_1_0 */
#  ifdef SONAME_LIBUSB0
			{ SONAME_LIBUSB0, NULL, 0 },
#  endif	/* SONAME_LIBUSB0 */
#  ifdef SOFILE_LIBUSB0
#   ifdef SONAME_LIBUSB0
			{ SOFILE_LIBUSB0, SONAME_LIBUSB0, 1 },
#   else	/* not SONAME_LIBUSB0 */
			{ SOFILE_LIBUSB0, NULL, 0 },
#   endif	/* SONAME_LIBUSB0 */
#  endif	/* SOFILE_LIBUSB0 */
			{ "libusb-0.1" SOEXT, NULL, 1 },
#  ifdef WIN32
			/* TODO: Detect DLL name at build time, or rename it at install time? */
			/* libusb-compat built for mingw per NUT instructions */
			{ "libusb-0-1-4" SOEXT, NULL, 1 },
#  endif	/* WIN32 */
#  ifdef SOPATH_LIBUSB0
			{ SOPATH_LIBUSB0, NULL, 1 },
#  endif	/* SOPATH_LIBUSB0 */
			/* We can also use libusb-compat from newer libusb-1.0 releases */
			{ "libusb" SOEXT, NULL, 1 },
# endif	/* WITH_LIBUSB_1_0 */
			{ NULL, NULL, 0 }
		};

		nutscan_select_library(__func__, "LibUSB", candidates, candidates,
			nutscan_load_usb_library, nutscan_unload_usb_library,
			&nutscan_avail_usb);
	}
	nutscan_report_load(__func__, nutscan_avail_usb, "LibUSB", "USB");
#else	/* not WITH_USB */
	upsdebugx(1, "%s: skipped loading the library for %s: was absent during NUT build",
		__func__, "LibUSB");
#endif	/* WITH_USB */

#if (defined WITH_SNMP) && WITH_SNMP
# if (defined WITH_SNMP_STATIC) && WITH_SNMP_STATIC
	/* This is a rare situation, reserved for platforms where libnetsnmp or
	 * equivalent (some other ucd-snmp descendants) was not packaged, and
	 * thus was custom-built for NUT (so linked statically to avoid potential
	 * conflicts with whatever else people may have practically deployed
	 * nearby).
	 */
	upsdebugx(1, "%s: skipped loading the library for %s: was linked statically during NUT build",
		__func__, "LibSNMP");
	nutscan_avail_snmp = 1;
# else	/* not WITH_SNMP_STATIC */
	{
		static const nutscan_library_candidate_t candidates[] = {
#  ifdef SONAME_LIBNETSNMP
			{ SONAME_LIBNETSNMP, NULL, 0 },
#  endif	/* SONAME_LIBNETSNMP */
#  ifdef SOFILE_LIBNETSNMP
#   ifdef SONAME_LIBNETSNMP
			{ SOFILE_LIBNETSNMP, SONAME_LIBNETSNMP, 1 },
#   else	/* not SONAME_LIBNETSNMP */
			{ SOFILE_LIBNETSNMP, NULL, 0 },
#   endif	/* SONAME_LIBNETSNMP */
#  endif	/* SOFILE_LIBNETSNMP */
			{ "libnetsnmp" SOEXT, NULL, 1 },
#  ifdef WIN32
			{ "libnetsnmp-40" SOEXT, NULL, 1 },
#  endif	/* WIN32 */
#  ifdef SOPATH_LIBNETSNMP
			{ SOPATH_LIBNETSNMP, NULL, 1 },
#  endif	/* SOPATH_LIBNETSNMP */
			{ NULL, NULL, 0 }
		};

		nutscan_select_library(__func__, "LibSNMP", candidates, candidates,
			nutscan_load_snmp_library, nutscan_unload_snmp_library,
			&nutscan_avail_snmp);
	}
	nutscan_report_load(__func__, nutscan_avail_snmp, "LibSNMP", "SNMP");
# endif	/* WITH_SNMP_STATIC */
#else	/* not WITH_SNMP */
	upsdebugx(1, "%s: skipped loading the library for %s: was absent during NUT build",
		__func__, "LibSNMP");
#endif	/* WITH_SNMP */

#if (defined WITH_NEON) && WITH_NEON
	{
		static const nutscan_library_candidate_t candidates[] = {
# ifdef SONAME_LIBNEON
			{ SONAME_LIBNEON, NULL, 0 },
# endif	/* SONAME_LIBNEON */
# ifdef SOFILE_LIBNEON
#  ifdef SONAME_LIBNEON
			{ SOFILE_LIBNEON, SONAME_LIBNEON, 1 },
#  else	/* not SONAME_LIBNEON */
			{ SOFILE_LIBNEON, NULL, 0 },
#  endif	/* SONAME_LIBNEON */
# endif	/* SOFILE_LIBNEON */
			{ "libneon" SOEXT, NULL, 1 },
			{ "libneon-gnutls" SOEXT, NULL, 1 },
# ifdef WIN32
			{ "libneon-27" SOEXT, NULL, 1 },
			{ "libneon-gnutls-27" SOEXT, NULL, 1 },
# endif	/* WIN32 */
# ifdef SOPATH_LIBNEON
			{ SOPATH_LIBNEON, NULL, 1 },
# endif	/* SOPATH_LIBNEON */
			{ NULL, NULL, 0 }
		};

		nutscan_select_library(__func__, "LibNeon", candidates, candidates,
			nutscan_load_neon_library, nutscan_unload_neon_library,
			&nutscan_avail_xml_http);
	}
	nutscan_report_load(__func__, nutscan_avail_xml_http, "LibNeon", "XML");
#else	/* not WITH_NEON */
	upsdebugx(1, "%s: skipped loading the library for %s: was absent during NUT build",
		__func__, "LibNeon");
#endif	/* WITH_NEON */

#if (defined WITH_AVAHI) && WITH_AVAHI
	{
		static const nutscan_library_candidate_t candidates[] = {
# ifdef SONAME_LIBAVAHI
			{ SONAME_LIBAVAHI, NULL, 0 },
# endif	/* SONAME_LIBAVAHI */
# ifdef SOFILE_LIBAVAHI
#  ifdef SONAME_LIBAVAHI
			{ SOFILE_LIBAVAHI, SONAME_LIBAVAHI, 1 },
#  else	/* not SONAME_LIBAVAHI */
			{ SOFILE_LIBAVAHI, NULL, 0 },
#  endif	/* SONAME_LIBAVAHI */
# endif	/* SOFILE_LIBAVAHI */
			{ "libavahi-client" SOEXT, NULL, 1 },
# ifdef SOPATH_LIBAVAHI
			{ SOPATH_LIBAVAHI, NULL, 1 },
# endif	/* SOPATH_LIBAVAHI */
			{ NULL, NULL, 0 }
		};

		nutscan_select_library(__func__, "LibAvahi", candidates, candidates,
			nutscan_load_avahi_library, nutscan_unload_avahi_library,
			&nutscan_avail_avahi);
	}
	nutscan_report_load(__func__, nutscan_avail_avahi, "LibAvahi", "AVAHI");
#else	/* not WITH_AVAHI */
	upsdebugx(1, "%s: skipped loading the library for %s: was absent during NUT build",
		__func__, "LibAvahi");
#endif	/* WITH_AVAHI */

#if (defined WITH_UPOWER) && WITH_UPOWER
	{
		/* NOTE: There may be a stack of libraries involved (libgio, libglib2,
		 * libmount...) in driver programs, but one entry point suffices
		 * (and/or dynamically pulls in the others) for just the scan itself */
		static const nutscan_library_candidate_t candidates[] = {
# ifdef SONAME_LIBGIO
			{ SONAME_LIBGIO, NULL, 0 },
# endif	/* SONAME_LIBGIO */
# ifdef SOFILE_LIBGIO
#  ifdef SONAME_LIBGIO
			{ SOFILE_LIBGIO, SONAME_LIBGIO, 1 },
#  else	/* not SONAME_LIBGIO */
			{ SOFILE_LIBGIO, NULL, 0 },
#  endif	/* SONAME_LIBGIO */
# endif	/* SOFILE_LIBGIO */
			{ "libgio-2.0" SOEXT, NULL, 1 },
# ifdef SOPATH_LIBGIO
			{ SOPATH_LIBGIO, NULL, 1 },
# endif	/* SOPATH_LIBGIO */
			{ NULL, NULL, 0 }
		};

		nutscan_select_library(__func__, "LibGIO", candidates, candidates,
			nutscan_load_upower_library, nutscan_unload_upower_library,
			&nutscan_avail_upower);
	}
	nutscan_report_load(__func__, nutscan_avail_upower, "LibGIO", "UPower");
#else	/* not WITH_UPOWER */
	upsdebugx(1, "%s: skipped loading the library for %s: was absent during NUT build",
		__func__, "LibGIO");
#endif	/* WITH_UPOWER */

#if (defined WITH_FREEIPMI) && WITH_FREEIPMI
	{
		static const nutscan_library_candidate_t candidates[] = {
# ifdef SONAME_LIBFREEIPMI
			{ SONAME_LIBFREEIPMI, NULL, 0 },
# endif	/* SONAME_LIBFREEIPMI */
# ifdef SOFILE_LIBFREEIPMI
#  ifdef SONAME_LIBFREEIPMI
			{ SOFILE_LIBFREEIPMI, SONAME_LIBFREEIPMI, 1 },
#  else	/* not SONAME_LIBFREEIPMI */
			{ SOFILE_LIBFREEIPMI, NULL, 0 },
#  endif	/* SONAME_LIBFREEIPMI */
# endif	/* SOFILE_LIBFREEIPMI */
			{ "libfreeipmi" SOEXT, NULL, 1 },
# ifdef SOPATH_LIBFREEIPMI
			{ SOPATH_LIBFREEIPMI, NULL, 1 },
# endif	/* SOPATH_LIBFREEIPMI */
			{ NULL, NULL, 0 }
		};

		nutscan_select_library(__func__, "LibFreeIPMI", candidates, candidates,
			nutscan_load_ipmi_library, nutscan_unload_ipmi_library,
			&nutscan_avail_ipmi);
	}
	nutscan_report_load(__func__, nutscan_avail_ipmi, "LibFreeIPMI", "IPMI");
#else	/* not WITH_FREEIPMI */
	upsdebugx(1, "%s: skipped loading the library for %s: was absent during NUT build",
		__func__, "LibFreeIPMI");
#endif	/* WITH_FREEIPMI */

/* start of libupsclient for "old NUT" (vs. Avahi) protocol - unconditional */
	{
		/* SOPATH precedes Windows aliases only in the resolver phase. */
		static const nutscan_library_candidate_t resolve[] = {
#ifdef SOFILE_LIBUPSCLIENT
			{ SOFILE_LIBUPSCLIENT, NULL, 0 },
#endif	/* SOFILE_LIBUPSCLIENT */
			{ "libupsclient" SOEXT, NULL, 1 },
#ifdef SOPATH_LIBUPSCLIENT
			{ SOPATH_LIBUPSCLIENT, NULL, 1 },
#endif	/* SOPATH_LIBUPSCLIENT */
#ifdef WIN32
			/* NOTE: Normally we should detect DLL name at build time,
			 * see clients/Makefile.am for libupsclient-version.h */
			{ "libupsclient-6" SOEXT, NULL, 1 },
			{ "libupsclient-3" SOEXT, NULL, 1 },
#endif	/* WIN32 */
			{ NULL, NULL, 0 }
		};
		static const nutscan_library_candidate_t fallback[] = {
#ifdef SOFILE_LIBUPSCLIENT
			{ SOFILE_LIBUPSCLIENT, NULL, 0 },
#endif	/* SOFILE_LIBUPSCLIENT */
			{ "libupsclient" SOEXT, NULL, 1 },
#ifdef WIN32
			/* NOTE: Normally we should detect DLL name at build time,
			 * see clients/Makefile.am for libupsclient-version.h */
			{ "libupsclient-6" SOEXT, NULL, 1 },
			{ "libupsclient-3" SOEXT, NULL, 1 },
#endif	/* WIN32 */
#ifdef SOPATH_LIBUPSCLIENT
			{ SOPATH_LIBUPSCLIENT, NULL, 1 },
#endif	/* SOPATH_LIBUPSCLIENT */
			{ NULL, NULL, 0 }
		};

		nutscan_select_library(__func__, "NUT Client library", resolve, fallback,
			nutscan_load_upsclient_library, nutscan_unload_upsclient_library,
			&nutscan_avail_nut);
	}
	nutscan_report_load(__func__, nutscan_avail_nut, "NUT Client library", "NUT");


/* end of libupsclient for "old NUT" (vs. Avahi) protocol */


/* start of "NUT Simulation" - unconditional */
/* no need for additional library */
	nutscan_avail_nut_simulation = 1;

	upsdebugx(1, "%s: done", __func__);
}

/* Return 0 on success, -1 on error e.g. "was not loaded";
 * other values may be possible if lt_dlclose() errors set them;
 * visible externally to scan_* modules */
int nutscan_unload_library(int *avail, lt_dlhandle *pdl_handle, char **libpath);
int nutscan_unload_library(int *avail, lt_dlhandle *pdl_handle, char **libpath)
{
	int ret = -1;

	if (avail == NULL || pdl_handle == NULL) {
		upsdebugx(1, "%s: called with bad inputs, no-op", __func__);
		return -2;
	}

	/* never tried/already unloaded */
	if (*pdl_handle == NULL) {
		goto end;
	}

	/* if previous init failed */
	if (*pdl_handle == (lt_dlhandle)1) {
		goto end;
	}

	if (*avail == 0) {
		upsdebugx(1, "%s: Asked to unload a module %p "
			"for %s but our flag says it is not loaded",
			__func__, (void *)(*pdl_handle),
			(libpath && *libpath && **libpath)
			? *libpath
			: "<unidentified module>");
	}

	/* init has already been done */
	if (libpath && *libpath && **libpath) {
		upsdebugx(1, "%s: unloading module %s",
			__func__, *libpath);
	}
	ret = lt_dlclose(*pdl_handle);
	lt_dlexit();

end:
	*pdl_handle = NULL;
	*avail = 0;

	if (libpath && *libpath) {
		free(*libpath);
		*libpath = NULL;
	}

	return ret;
}

void nutscan_free(void)
{
	upsdebugx(1, "%s: starting...", __func__);

	nutscan_unload_usb_library();
	nutscan_unload_snmp_library();
	nutscan_unload_neon_library();
	nutscan_unload_avahi_library();
	nutscan_unload_ipmi_library();
	nutscan_unload_upsclient_library();
	nutscan_unload_upower_library();

#ifdef HAVE_PTHREAD
/* TOTHINK: See comments near mutex/semaphore init code above */
# ifdef HAVE_SEMAPHORE_UNNAMED
	sem_destroy(nutscan_semaphore());
# elif defined HAVE_SEMAPHORE_NAMED
	if (nutscan_semaphore()) {
		sem_unlink(SEMNAME_TOPLEVEL);
		sem_close(nutscan_semaphore());
		nutscan_semaphore_set(NULL);
	}
# endif

# ifdef HAVE_PTHREAD_TRYJOIN
	pthread_mutex_destroy(&threadcount_mutex);
# endif
#endif

	upsdebugx(1, "%s: done", __func__);
}
