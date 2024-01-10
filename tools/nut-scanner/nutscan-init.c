/*
 *  Copyright (C) 2011 - 2024 Arnaud Quette (Design and part of implementation)
 *  Copyright (C) 2011-2021 - EATON
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
*/

#include "common.h"
#include "nutscan-init.h"
#include <ltdl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "nut-scan.h"

#ifndef WIN32
#define SOEXT ".so"
#else
#define SOEXT ".dll"
#endif

int nutscan_avail_avahi = 0;
int nutscan_avail_ipmi = 0;
int nutscan_avail_nut = 0;
int nutscan_avail_nut_simulation = 1;
#ifdef WITH_SNMP_STATIC
int nutscan_avail_snmp = 1;
#else
int nutscan_avail_snmp = 0;
#endif
int nutscan_avail_usb = 0;
int nutscan_avail_xml_http = 0;

int nutscan_load_usb_library(const char *libname_path);
int nutscan_load_snmp_library(const char *libname_path);
int nutscan_load_neon_library(const char *libname_path);
int nutscan_load_avahi_library(const char *libname_path);
int nutscan_load_ipmi_library(const char *libname_path);
int nutscan_load_upsclient_library(const char *libname_path);

#ifdef HAVE_PTHREAD
# ifdef HAVE_SEMAPHORE
/* Shared by library consumers, exposed by nutscan_semaphore() below */
static sem_t semaphore;

sem_t * nutscan_semaphore(void)
{
	return &semaphore;
}
# endif /* HAVE_SEMAPHORE */

# ifdef HAVE_PTHREAD_TRYJOIN
pthread_mutex_t threadcount_mutex;
# endif

# if (defined HAVE_PTHREAD_TRYJOIN) || (defined HAVE_SEMAPHORE)
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

# endif  /* HAVE_PTHREAD_TRYJOIN || HAVE_SEMAPHORE */

#endif /* HAVE_PTHREAD */

#ifdef WIN32
/* Stub for libupsclient */
void do_upsconf_args(char *confupsname, char *var, char *val) {
	NUT_UNUSED_VARIABLE(confupsname);
	NUT_UNUSED_VARIABLE(var);
	NUT_UNUSED_VARIABLE(val);
}
#endif

void nutscan_init(void)
{
	char *libname = NULL;

	/* Optional filter to not walk things twice */
	nut_prepare_search_paths();

	/* Report library paths we would search, at given debug verbosity level */
	upsdebugx_report_search_paths(1, 1);

#ifdef HAVE_PTHREAD
/* TOTHINK: Should semaphores to limit thread count
 * and the more naive but portable methods be an
 * if-else proposition? At least when initializing?
 */
# ifdef HAVE_SEMAPHORE
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
			"WARNING: %s: Limiting max_threads to range acceptable for sem_init()",
			__func__);
		max_threads = UINT_MAX - 1;
	}
	sem_init(&semaphore, 0, (unsigned int)max_threads);
# endif

# ifdef HAVE_PTHREAD_TRYJOIN
	pthread_mutex_init(&threadcount_mutex, NULL);
# endif
#endif	/* HAVE_PTHREAD */

#ifdef WITH_USB
 #if WITH_LIBUSB_1_0
	libname = get_libname("libusb-1.0" SOEXT);
 #else
	libname = get_libname("libusb-0.1" SOEXT);
  #ifdef WIN32
	/* TODO: Detect DLL name at build time, or rename it at install time? */
	/* libusb-compat built for mingw per NUT instructions */
	if (!libname) {
		libname = get_libname("libusb-0-1-4" SOEXT);
	}
  #endif
 #endif
	if (!libname) {
		/* We can also use libusb-compat from newer libusb-1.0 releases */
		libname = get_libname("libusb" SOEXT);
	}
	if (libname) {
		upsdebugx(1, "%s: get_libname() resolved '%s' for %s, loading it",
			__func__, libname, "LibUSB");
		nutscan_avail_usb = nutscan_load_usb_library(libname);
		free(libname);
	} else {
		/* let libtool (lt_dlopen) do its default magic maybe better */
		upsdebugx(1, "%s: get_libname() did not resolve libname for %s, "
			"trying to load it with libtool default resolver",
			__func__, "LibUSB");
 #if WITH_LIBUSB_1_0
		nutscan_avail_usb = nutscan_load_usb_library("libusb-1.0" SOEXT);
 #else
		nutscan_avail_usb = nutscan_load_usb_library("libusb-0.1" SOEXT);
  #ifdef WIN32
		if (!nutscan_avail_usb) {
			nutscan_avail_usb = nutscan_load_usb_library("libusb-0-1-4" SOEXT);
		}
  #endif
 #endif
		if (!nutscan_avail_usb) {
			nutscan_avail_usb = nutscan_load_usb_library("libusb" SOEXT);
		}
	}
	upsdebugx(1, "%s: %s to load the library for %s",
		__func__, nutscan_avail_usb ? "succeeded" : "failed", "LibUSB");
#else
	upsdebugx(1, "%s: skipped loading the library for %s: was absent during NUT build",
		__func__, "LibUSB");
#endif	/* WITH_USB */

#ifdef WITH_SNMP
	libname = get_libname("libnetsnmp" SOEXT);
 #ifdef WIN32
	if (!libname) {
		libname = get_libname("libnetsnmp-40" SOEXT);
	}
 #endif
	if (libname) {
		upsdebugx(1, "%s: get_libname() resolved '%s' for %s, loading it",
			__func__, libname, "LibSNMP");
		nutscan_avail_snmp = nutscan_load_snmp_library(libname);
		free(libname);
	} else {
		/* let libtool (lt_dlopen) do its default magic maybe better */
		upsdebugx(1, "%s: get_libname() did not resolve libname for %s, "
			"trying to load it with libtool default resolver",
			__func__, "LibSNMP");
		nutscan_avail_snmp = nutscan_load_snmp_library("libnetsnmp" SOEXT);
#ifdef WIN32
		if (!nutscan_avail_snmp) {
			nutscan_avail_snmp = nutscan_load_snmp_library("libnetsnmp-40" SOEXT);
		}
#endif
	}
	upsdebugx(1, "%s: %s to load the library for %s",
		__func__, nutscan_avail_snmp ? "succeeded" : "failed", "LibSNMP");
#else
	upsdebugx(1, "%s: skipped loading the library for %s: was absent during NUT build",
		__func__, "LibSNMP");
#endif	/* WITH_SNMP */

#ifdef WITH_NEON
	libname = get_libname("libneon" SOEXT);
	if (!libname) {
		libname = get_libname("libneon-gnutls" SOEXT);
	}
 #ifdef WIN32
	if (!libname) {
		libname = get_libname("libneon-27" SOEXT);
	}
	if (!libname) {
		libname = get_libname("libneon-gnutls-27" SOEXT);
	}
 #endif
	if (libname) {
		upsdebugx(1, "%s: get_libname() resolved '%s' for %s, loading it",
			__func__, libname, "LibNeon");
		nutscan_avail_xml_http = nutscan_load_neon_library(libname);
		free(libname);
	} else {
		/* let libtool (lt_dlopen) do its default magic maybe better */
		upsdebugx(1, "%s: get_libname() did not resolve libname for %s, "
			"trying to load it with libtool default resolver",
			__func__, "LibNeon");
		nutscan_avail_xml_http = nutscan_load_neon_library("libneon" SOEXT);
		if (!nutscan_avail_xml_http) {
			nutscan_avail_xml_http = nutscan_load_neon_library("libneon-gnutls" SOEXT);
		}
#ifdef WIN32
		if (!nutscan_avail_xml_http) {
			nutscan_avail_xml_http = nutscan_load_neon_library("libneon-27" SOEXT);
		}
		if (!nutscan_avail_xml_http) {
			nutscan_avail_xml_http = nutscan_load_neon_library("libneon-gnutls-27" SOEXT);
		}
#endif
	}
	upsdebugx(1, "%s: %s to load the library for %s",
		__func__, nutscan_avail_xml_http ? "succeeded" : "failed", "LibNeon");
#else
	upsdebugx(1, "%s: skipped loading the library for %s: was absent during NUT build",
		__func__, "LibNeon");
#endif	/* WITH_NEON */

#ifdef WITH_AVAHI
	libname = get_libname("libavahi-client" SOEXT);
	if (libname) {
		upsdebugx(1, "%s: get_libname() resolved '%s' for %s, loading it",
			__func__, libname, "LibAvahi");
		nutscan_avail_avahi = nutscan_load_avahi_library(libname);
		free(libname);
	} else {
		/* let libtool (lt_dlopen) do its default magic maybe better */
		upsdebugx(1, "%s: get_libname() did not resolve libname for %s, "
			"trying to load it with libtool default resolver",
			__func__, "LibAvahi");
		nutscan_avail_avahi = nutscan_load_avahi_library("libavahi-client" SOEXT);
	}
	upsdebugx(1, "%s: %s to load the library for %s",
		__func__, nutscan_avail_avahi ? "succeeded" : "failed", "LibAvahi");
#else
	upsdebugx(1, "%s: skipped loading the library for %s: was absent during NUT build",
		__func__, "LibAvahi");
#endif	/* WITH_AVAHI */

#ifdef WITH_FREEIPMI
	libname = get_libname("libfreeipmi" SOEXT);
	if (libname) {
		upsdebugx(1, "%s: get_libname() resolved '%s' for %s, loading it",
			__func__, libname, "LibFreeIPMI");
		nutscan_avail_ipmi = nutscan_load_ipmi_library(libname);
		free(libname);
	} else {
		/* let libtool (lt_dlopen) do its default magic maybe better */
		upsdebugx(1, "%s: get_libname() did not resolve libname for %s, "
			"trying to load it with libtool default resolver",
			__func__, "LibFreeIPMI");
		nutscan_avail_ipmi = nutscan_load_ipmi_library("libfreeipmi" SOEXT);
	}
	upsdebugx(1, "%s: %s to load the library for %s",
		__func__, nutscan_avail_ipmi ? "succeeded" : "failed", "LibFreeIPMI");
#else
	upsdebugx(1, "%s: skipped loading the library for %s: was absent during NUT build",
		__func__, "LibFreeIPMI");
#endif	/* WITH_FREEIPMI */

/* start of libupsclient for "old NUT" (vs. Avahi) protocol - unconditional */
	libname = get_libname("libupsclient" SOEXT);
#ifdef WIN32
	/* TODO: Detect DLL name at build time, or rename it at install time? */
	/* e.g. see clients/Makefile.am for version-info value */
	if (!libname) {
		libname = get_libname("libupsclient-6" SOEXT);
	}
	if (!libname) {
		libname = get_libname("libupsclient-3" SOEXT);
	}
#endif
	if (libname) {
		upsdebugx(1, "%s: get_libname() resolved '%s' for %s, loading it",
			__func__, libname, "NUT Client library");
		nutscan_avail_nut = nutscan_load_upsclient_library(libname);
		free(libname);
	} else {
		/* let libtool (lt_dlopen) do its default magic maybe better */
		upsdebugx(1, "%s: get_libname() did not resolve libname for %s, "
			"trying to load it with libtool default resolver",
			__func__, "NUT Client library");
		nutscan_avail_nut = nutscan_load_upsclient_library("libupsclient" SOEXT);
#ifdef WIN32
		if (!nutscan_avail_nut) {
			nutscan_avail_nut = nutscan_load_upsclient_library("libupsclient-6" SOEXT);
		}
		if (!nutscan_avail_nut) {
			nutscan_avail_nut = nutscan_load_upsclient_library("libupsclient-3" SOEXT);
		}
#endif
	}
	upsdebugx(1, "%s: %s to load the library for %s",
		__func__, nutscan_avail_nut ? "succeeded" : "failed", "NUT Client library");
/* end of libupsclient for "old NUT" (vs. Avahi) protocol */


/* start of "NUT Simulation" - unconditional */
/* no need for additional library */
	nutscan_avail_nut = 1;

}

void nutscan_free(void)
{
	if (nutscan_avail_usb) {
		lt_dlexit();
	}
	if (nutscan_avail_snmp) {
		lt_dlexit();
	}
	if (nutscan_avail_xml_http) {
		lt_dlexit();
	}
	if (nutscan_avail_avahi) {
		lt_dlexit();
	}
	if (nutscan_avail_ipmi) {
		lt_dlexit();
	}
	if (nutscan_avail_nut) {
		lt_dlexit();
	}

#ifdef HAVE_PTHREAD
/* TOTHINK: See comments near mutex/semaphore init code above */
# ifdef HAVE_SEMAPHORE
	sem_destroy(nutscan_semaphore());
# endif

# ifdef HAVE_PTHREAD_TRYJOIN
	pthread_mutex_destroy(&threadcount_mutex);
# endif
#endif

}
