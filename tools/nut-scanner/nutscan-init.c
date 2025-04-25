/*
 *  Copyright (C) 2011 - 2024 Arnaud Quette (Design and part of implementation)
 *  Copyright (C) 2011 - 2021 EATON
 *  Copyright (C) 2016 - 2021 Jim Klimov <EvgenyKlimov@eaton.com>
 *  Copyright (C) 2021 - 2024 Jim Klimov <jimklimov+nut@gmail.com>
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
/* Stub for libupsclient */
void do_upsconf_args(char *confupsname, char *var, char *val) {
	NUT_UNUSED_VARIABLE(confupsname);
	NUT_UNUSED_VARIABLE(var);
	NUT_UNUSED_VARIABLE(val);
}
#endif	/* WIN32 */

void nutscan_init(void)
{
	char *libname = NULL;

#ifdef WIN32
	/* Required ritual before calling any socket functions */
	WSADATA WSAdata;
	WSAStartup(2,&WSAdata);
	atexit((void(*)(void))WSACleanup);
#endif	/* WIN32 */

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
	/* Different platforms, different sizes, none fits all... */
	if (SIZE_MAX > UINT_MAX && max_threads > UINT_MAX) {
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_UNREACHABLE_CODE
#pragma GCC diagnostic pop
#endif
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
	 * embedded version identifiers).
	 *
	 * Historically we allow run-time environments to override the
	 * library search paths (e.g. for bundled NUT installers that
	 * may be incompatible with OS library builds), so we use full
	 * SOPATH_LIB<X> as the last option, but prefer a presumably
	 * known-compatible SOFILE_LIB<X> first.
	 */

#ifdef WITH_USB
# if WITH_LIBUSB_1_0

#  ifdef SOFILE_LIBUSB1
	if (!libname) {
		libname = get_libname(SOFILE_LIBUSB1);
	}
#  endif	/* SOFILE_LIBUSB1 */
	if (!libname) {
		libname = get_libname("libusb-1.0" SOEXT);
	}
#  ifdef SOPATH_LIBUSB1
	if (!libname) {
		libname = get_libname(SOPATH_LIBUSB1);
	}
#  endif	/* SOPATH_LIBUSB1 */

# else	/* not WITH_LIBUSB_1_0 => WITH_LIBUSB_0_1 */

#  ifdef SOFILE_LIBUSB0
	if (!libname) {
		libname = get_libname(SOFILE_LIBUSB0);
	}
#  endif	/* SOFILE_LIBUSB0 */
	if (!libname) {
		libname = get_libname("libusb-0.1" SOEXT);
	}
#  ifdef WIN32
	/* TODO: Detect DLL name at build time, or rename it at install time? */
	/* libusb-compat built for mingw per NUT instructions */
	if (!libname) {
		libname = get_libname("libusb-0-1-4" SOEXT);
	}
#  endif	/* WIN32 */
#  ifdef SOPATH_LIBUSB0
	if (!libname) {
		libname = get_libname(SOPATH_LIBUSB0);
	}
#  endif	/* SOPATH_LIBUSB0 */
# endif	/* WITH_LIBUSB_X_Y */

	if (!libname) {
		/* We can also use libusb-compat from newer libusb-1.0 releases */
		libname = get_libname("libusb" SOEXT);
	}

	if (libname) {
		upsdebugx(1, "%s: get_libname() resolved '%s' for %s, loading it",
			__func__, libname, "LibUSB");
		nutscan_avail_usb = nutscan_load_usb_library(libname);
		free(libname);
		libname = NULL;
	} else {
		/* let libtool (lt_dlopen) do its default magic maybe better */
		upsdebugx(1, "%s: get_libname() did not resolve libname for %s, "
			"trying to load it with libtool default resolver",
			__func__, "LibUSB");

# if WITH_LIBUSB_1_0

#  ifdef SOFILE_LIBUSB1
		if (!nutscan_avail_usb) {
			nutscan_avail_usb = nutscan_load_usb_library(SOFILE_LIBUSB1);
		}
#  endif	/* SOFILE_LIBUSB1 */
		if (!nutscan_avail_usb) {
			nutscan_avail_usb = nutscan_load_usb_library("libusb-1.0" SOEXT);
		}
#  ifdef SOPATH_LIBUSB1
		if (!nutscan_avail_usb) {
			nutscan_avail_usb = nutscan_load_usb_library(SOPATH_LIBUSB1);
		}
#  endif	/* SOPATH_LIBUSB1 */

# else	/* not WITH_LIBUSB_1_0 => WITH_LIBUSB_0_1 */

#  ifdef SOFILE_LIBUSB0
		if (!nutscan_avail_usb) {
			nutscan_avail_usb = nutscan_load_usb_library(SOFILE_LIBUSB0);
		}
#  endif	/* SOFILE_LIBUSB0 */
		if (!nutscan_avail_usb) {
			nutscan_avail_usb = nutscan_load_usb_library("libusb-0.1" SOEXT);
		}
#  ifdef WIN32
		if (!nutscan_avail_usb) {
			nutscan_avail_usb = nutscan_load_usb_library("libusb-0-1-4" SOEXT);
		}
#  endif	/* WIN32 */
#  ifdef SOPATH_LIBUSB0
		if (!nutscan_avail_usb) {
			nutscan_avail_usb = nutscan_load_usb_library(SOPATH_LIBUSB0);
		}
#  endif	/* SOPATH_LIBUSB0 */
# endif	/* WITH_LIBUSB_X_Y */

		if (!nutscan_avail_usb) {
			/* We can also use libusb-compat from newer libusb-1.0 releases */
			nutscan_avail_usb = nutscan_load_usb_library("libusb" SOEXT);
		}
	}
	upsdebugx(1, "%s: %s to load the library for %s",
		__func__, nutscan_avail_usb ? "succeeded" : "failed", "LibUSB");
#else	/* not WITH_USB */
	upsdebugx(1, "%s: skipped loading the library for %s: was absent during NUT build",
		__func__, "LibUSB");
#endif	/* WITH_USB */

#ifdef WITH_SNMP
# ifdef WITH_SNMP_STATIC
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
#  ifdef SOFILE_LIBNETSNMP
	if (!libname) {
		libname = get_libname(SOFILE_LIBNETSNMP);
	}
#  endif	/* SOFILE_LIBNETSNMP */
	if (!libname) {
		libname = get_libname("libnetsnmp" SOEXT);
	}
#  ifdef WIN32
	if (!libname) {
		libname = get_libname("libnetsnmp-40" SOEXT);
	}
#  endif	/* WIN32 */
#  ifdef SOPATH_LIBNETSNMP
	if (!libname) {
		libname = get_libname(SOPATH_LIBNETSNMP);
	}
#  endif	/* SOPATH_LIBNETSNMP */

	if (libname) {
		upsdebugx(1, "%s: get_libname() resolved '%s' for %s, loading it",
			__func__, libname, "LibSNMP");
		nutscan_avail_snmp = nutscan_load_snmp_library(libname);
		free(libname);
		libname = NULL;
	} else {
		/* let libtool (lt_dlopen) do its default magic maybe better */
		upsdebugx(1, "%s: get_libname() did not resolve libname for %s, "
			"trying to load it with libtool default resolver",
			__func__, "LibSNMP");
#  ifdef SOFILE_LIBNETSNMP
		if (!nutscan_avail_snmp) {
			nutscan_avail_snmp = nutscan_load_snmp_library(SOFILE_LIBNETSNMP);
		}
#  endif	/* SOFILE_LIBNETSNMP */
		if (!nutscan_avail_snmp) {
			nutscan_avail_snmp = nutscan_load_snmp_library("libnetsnmp" SOEXT);
		}
#  ifdef WIN32
		if (!nutscan_avail_snmp) {
			nutscan_avail_snmp = nutscan_load_snmp_library("libnetsnmp-40" SOEXT);
		}
#  endif	/* WIN32 */
#  ifdef SOPATH_LIBNETSNMP
		if (!nutscan_avail_snmp) {
			nutscan_avail_snmp = nutscan_load_snmp_library(SOPATH_LIBNETSNMP);
		}
#  endif	/* SOPATH_LIBNETSNMP */
	}
	upsdebugx(1, "%s: %s to load the library for %s",
		__func__, nutscan_avail_snmp ? "succeeded" : "failed", "LibSNMP");
# endif	/* WITH_SNMP_STATIC */
#else	/* not WITH_SNMP */
	upsdebugx(1, "%s: skipped loading the library for %s: was absent during NUT build",
		__func__, "LibSNMP");
#endif	/* WITH_SNMP */

#ifdef WITH_NEON
# ifdef SOFILE_LIBNEON
	if (!libname) {
		libname = get_libname(SOFILE_LIBNEON);
	}
# endif	/* SOFILE_LIBNEON */
	if (!libname) {
		libname = get_libname("libneon" SOEXT);
	}
	if (!libname) {
		libname = get_libname("libneon-gnutls" SOEXT);
	}
# ifdef WIN32
	if (!libname) {
		libname = get_libname("libneon-27" SOEXT);
	}
	if (!libname) {
		libname = get_libname("libneon-gnutls-27" SOEXT);
	}
# endif	/* WIN32 */
# ifdef SOPATH_LIBNEON
	if (!libname) {
		libname = get_libname(SOPATH_LIBNEON);
	}
# endif	/* SOPATH_LIBNEON */

	if (libname) {
		upsdebugx(1, "%s: get_libname() resolved '%s' for %s, loading it",
			__func__, libname, "LibNeon");
		nutscan_avail_xml_http = nutscan_load_neon_library(libname);
		free(libname);
		libname = NULL;
	} else {
		/* let libtool (lt_dlopen) do its default magic maybe better */
		upsdebugx(1, "%s: get_libname() did not resolve libname for %s, "
			"trying to load it with libtool default resolver",
			__func__, "LibNeon");
# ifdef SOFILE_LIBNEON
		if (!nutscan_avail_xml_http) {
			nutscan_avail_xml_http = nutscan_load_neon_library(SOFILE_LIBNEON);
		}
# endif	/* SOFILE_LIBNEON */
		if (!nutscan_avail_xml_http) {
			nutscan_avail_xml_http = nutscan_load_neon_library("libneon" SOEXT);
		}
		if (!nutscan_avail_xml_http) {
			nutscan_avail_xml_http = nutscan_load_neon_library("libneon-gnutls" SOEXT);
		}
# ifdef WIN32
		if (!nutscan_avail_xml_http) {
			nutscan_avail_xml_http = nutscan_load_neon_library("libneon-27" SOEXT);
		}
		if (!nutscan_avail_xml_http) {
			nutscan_avail_xml_http = nutscan_load_neon_library("libneon-gnutls-27" SOEXT);
		}
# endif	/* WIN32 */
# ifdef SOPATH_LIBNEON
		if (!nutscan_avail_xml_http) {
			nutscan_avail_xml_http = nutscan_load_neon_library(SOPATH_LIBNEON);
		}
# endif	/* SOPATH_LIBNEON */
	}
	upsdebugx(1, "%s: %s to load the library for %s",
		__func__, nutscan_avail_xml_http ? "succeeded" : "failed", "LibNeon");
#else	/* not WITH_NEON */
	upsdebugx(1, "%s: skipped loading the library for %s: was absent during NUT build",
		__func__, "LibNeon");
#endif	/* WITH_NEON */

#ifdef WITH_AVAHI
# ifdef SOFILE_LIBAVAHI
	if (!libname) {
		libname = get_libname(SOFILE_LIBAVAHI);
	}
# endif	/* SOFILE_LIBAVAHI */
	if (!libname) {
		libname = get_libname("libavahi-client" SOEXT);
	}
# ifdef SOPATH_LIBAVAHI
	if (!libname) {
		libname = get_libname(SOPATH_LIBAVAHI);
	}
# endif	/* SOPATH_LIBAVAHI */

	if (libname) {
		upsdebugx(1, "%s: get_libname() resolved '%s' for %s, loading it",
			__func__, libname, "LibAvahi");
		nutscan_avail_avahi = nutscan_load_avahi_library(libname);
		free(libname);
		libname = NULL;
	} else {
		/* let libtool (lt_dlopen) do its default magic maybe better */
		upsdebugx(1, "%s: get_libname() did not resolve libname for %s, "
			"trying to load it with libtool default resolver",
			__func__, "LibAvahi");
# ifdef SOFILE_LIBAVAHI
		if (!nutscan_avail_avahi) {
			nutscan_avail_avahi = nutscan_load_avahi_library(SOFILE_LIBAVAHI);
		}
# endif	/* SOFILE_LIBAVAHI */
		if (!nutscan_avail_avahi) {
			nutscan_avail_avahi = nutscan_load_avahi_library("libavahi-client" SOEXT);
# ifdef SOPATH_LIBAVAHI
		if (!nutscan_avail_avahi) {
			nutscan_avail_avahi = nutscan_load_avahi_library(SOPATH_LIBAVAHI);
		}
# endif	/* SOPATH_LIBAVAHI */
		}
	}
	upsdebugx(1, "%s: %s to load the library for %s",
		__func__, nutscan_avail_avahi ? "succeeded" : "failed", "LibAvahi");
#else	/* not WITH_AVAHI */
	upsdebugx(1, "%s: skipped loading the library for %s: was absent during NUT build",
		__func__, "LibAvahi");
#endif	/* WITH_AVAHI */

#ifdef WITH_FREEIPMI
# ifdef SOFILE_LIBFREEIPMI
	if (!libname) {
		libname = get_libname(SOFILE_LIBFREEIPMI);
	}
# endif	/* SOFILE_LIBFREEIPMI */
	if (!libname) {
		libname = get_libname("libfreeipmi" SOEXT);
	}
# ifdef SOPATH_LIBFREEIPMI
	if (!libname) {
		libname = get_libname(SOPATH_LIBFREEIPMI);
	}
# endif	/* SOPATH_LIBAVAHI */
	if (libname) {
		upsdebugx(1, "%s: get_libname() resolved '%s' for %s, loading it",
			__func__, libname, "LibFreeIPMI");
		nutscan_avail_ipmi = nutscan_load_ipmi_library(libname);
		free(libname);
		libname = NULL;
	} else {
		/* let libtool (lt_dlopen) do its default magic maybe better */
		upsdebugx(1, "%s: get_libname() did not resolve libname for %s, "
			"trying to load it with libtool default resolver",
			__func__, "LibFreeIPMI");
# ifdef SOFILE_LIBFREEIPMI
		if (!nutscan_avail_ipmi) {
			nutscan_avail_ipmi = nutscan_load_ipmi_library(SOFILE_LIBFREEIPMI);
		}
# endif	/* SOFILE_LIBFREEIPMI */
		if (!nutscan_avail_ipmi) {
			nutscan_avail_ipmi = nutscan_load_ipmi_library("libfreeipmi" SOEXT);
		}
# ifdef SOPATH_LIBFREEIPMI
		if (!nutscan_avail_ipmi) {
			nutscan_avail_ipmi = nutscan_load_ipmi_library(SOPATH_LIBFREEIPMI);
		}
# endif	/* SOPATH_LIBFREEIPMI */
	}
	upsdebugx(1, "%s: %s to load the library for %s",
		__func__, nutscan_avail_ipmi ? "succeeded" : "failed", "LibFreeIPMI");
#else	/* not WITH_FREEIPMI */
	upsdebugx(1, "%s: skipped loading the library for %s: was absent during NUT build",
		__func__, "LibFreeIPMI");
#endif	/* WITH_FREEIPMI */

/* start of libupsclient for "old NUT" (vs. Avahi) protocol - unconditional */
#ifdef SOFILE_LIBUPSCLIENT
	if (!libname) {
		libname = get_libname(SOFILE_LIBUPSCLIENT);
	}
#endif	/* SOFILE_LIBUPSCLIENT */
	if (!libname) {
		libname = get_libname("libupsclient" SOEXT);
	}
#ifdef SOPATH_LIBUPSCLIENT
	if (!libname) {
		libname = get_libname(SOPATH_LIBUPSCLIENT);
	}
#endif	/* SOPATH_LIBUPSCLIENT */
#ifdef WIN32
	/* NOTE: Normally we should detect DLL name at build time,
	 * see clients/Makefile.am for libupsclient-version.h */
	if (!libname) {
		libname = get_libname("libupsclient-6" SOEXT);
	}
	if (!libname) {
		libname = get_libname("libupsclient-3" SOEXT);
	}
#endif	/* WIN32 */
	if (libname) {
		upsdebugx(1, "%s: get_libname() resolved '%s' for %s, loading it",
			__func__, libname, "NUT Client library");
		nutscan_avail_nut = nutscan_load_upsclient_library(libname);
		free(libname);
		libname = NULL;
	} else {
		/* let libtool (lt_dlopen) do its default magic maybe better */
		upsdebugx(1, "%s: get_libname() did not resolve libname for %s, "
			"trying to load it with libtool default resolver",
			__func__, "NUT Client library");
#ifdef SOFILE_LIBUPSCLIENT
		if (!nutscan_avail_nut) {
			nutscan_avail_xml_http = nutscan_load_upsclient_library(SOFILE_LIBUPSCLIENT);
		}
#endif	/* SOFILE_LIBUPSCLIENT */
		nutscan_avail_nut = nutscan_load_upsclient_library("libupsclient" SOEXT);
#ifdef WIN32
		if (!nutscan_avail_nut) {
			nutscan_avail_nut = nutscan_load_upsclient_library("libupsclient-6" SOEXT);
		}
		if (!nutscan_avail_nut) {
			nutscan_avail_nut = nutscan_load_upsclient_library("libupsclient-3" SOEXT);
		}
#endif	/* WIN32 */
#ifdef SOPATH_LIBUPSCLIENT
		if (!nutscan_avail_nut) {
			nutscan_avail_xml_http = nutscan_load_upsclient_library(SOPATH_LIBUPSCLIENT);
		}
#endif	/* SOFILE_LIBUPSCLIENT */
	}
	upsdebugx(1, "%s: %s to load the library for %s",
		__func__, nutscan_avail_nut ? "succeeded" : "failed", "NUT Client library");
/* end of libupsclient for "old NUT" (vs. Avahi) protocol */


/* start of "NUT Simulation" - unconditional */
/* no need for additional library */
	nutscan_avail_nut_simulation = 1;
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
	if (*pdl_handle == (void *)1) {
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
	nutscan_unload_usb_library();
	nutscan_unload_snmp_library();
	nutscan_unload_neon_library();
	nutscan_unload_avahi_library();
	nutscan_unload_ipmi_library();
	nutscan_unload_upsclient_library();

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

}
