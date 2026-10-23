/* nutscan-selection-test - exercise the production initialisation path
 *
 * Copyright (C) 2026 Network UPS Tools project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * No device scans, real semaphores or mutexes are used by this fixture.
 */
#include "config.h"

#ifndef NUTSCAN_SELECTION_VARIANT
# define NUTSCAN_SELECTION_VARIANT 0
#endif

#if NUTSCAN_SELECTION_VARIANT > 0
# undef WITH_USB
# define WITH_USB 1
# undef WITH_SNMP
# define WITH_SNMP 1
# undef WITH_NEON
# define WITH_NEON 1
# undef WITH_AVAHI
# define WITH_AVAHI 1
# undef WITH_UPOWER
# define WITH_UPOWER 1
# undef WITH_FREEIPMI
# define WITH_FREEIPMI 1
# undef WITH_IPMI
# define WITH_IPMI 0
# undef SONAME_LIBUSB1
# undef SOFILE_LIBUSB1
# undef SOPATH_LIBUSB1
# define SOFILE_LIBUSB1 "fixture-LIBUSB1-file"
# define SOPATH_LIBUSB1 "fixture-LIBUSB1-path"
# if NUTSCAN_SELECTION_VARIANT != 3 && NUTSCAN_SELECTION_VARIANT != 4
#  define SONAME_LIBUSB1 "fixture-LIBUSB1-soname"
# endif
# if NUTSCAN_SELECTION_VARIANT == 2
#  undef SOFILE_LIBUSB1
#  define SOFILE_LIBUSB1 SONAME_LIBUSB1
# elif NUTSCAN_SELECTION_VARIANT == 4
#  undef SOFILE_LIBUSB1
# endif
# undef SONAME_LIBUSB0
# undef SOFILE_LIBUSB0
# undef SOPATH_LIBUSB0
# define SOFILE_LIBUSB0 "fixture-LIBUSB0-file"
# define SOPATH_LIBUSB0 "fixture-LIBUSB0-path"
# if NUTSCAN_SELECTION_VARIANT != 3 && NUTSCAN_SELECTION_VARIANT != 4
#  define SONAME_LIBUSB0 "fixture-LIBUSB0-soname"
# endif
# if NUTSCAN_SELECTION_VARIANT == 2
#  undef SOFILE_LIBUSB0
#  define SOFILE_LIBUSB0 SONAME_LIBUSB0
# elif NUTSCAN_SELECTION_VARIANT == 4
#  undef SOFILE_LIBUSB0
# endif
# undef SONAME_LIBNETSNMP
# undef SOFILE_LIBNETSNMP
# undef SOPATH_LIBNETSNMP
# define SOFILE_LIBNETSNMP "fixture-LIBNETSNMP-file"
# define SOPATH_LIBNETSNMP "fixture-LIBNETSNMP-path"
# if NUTSCAN_SELECTION_VARIANT != 3 && NUTSCAN_SELECTION_VARIANT != 4
#  define SONAME_LIBNETSNMP "fixture-LIBNETSNMP-soname"
# endif
# if NUTSCAN_SELECTION_VARIANT == 2
#  undef SOFILE_LIBNETSNMP
#  define SOFILE_LIBNETSNMP SONAME_LIBNETSNMP
# elif NUTSCAN_SELECTION_VARIANT == 4
#  undef SOFILE_LIBNETSNMP
# endif
# undef SONAME_LIBNEON
# undef SOFILE_LIBNEON
# undef SOPATH_LIBNEON
# define SOFILE_LIBNEON "fixture-LIBNEON-file"
# define SOPATH_LIBNEON "fixture-LIBNEON-path"
# if NUTSCAN_SELECTION_VARIANT != 3 && NUTSCAN_SELECTION_VARIANT != 4
#  define SONAME_LIBNEON "fixture-LIBNEON-soname"
# endif
# if NUTSCAN_SELECTION_VARIANT == 2
#  undef SOFILE_LIBNEON
#  define SOFILE_LIBNEON SONAME_LIBNEON
# elif NUTSCAN_SELECTION_VARIANT == 4
#  undef SOFILE_LIBNEON
# endif
# undef SONAME_LIBAVAHI
# undef SOFILE_LIBAVAHI
# undef SOPATH_LIBAVAHI
# define SOFILE_LIBAVAHI "fixture-LIBAVAHI-file"
# define SOPATH_LIBAVAHI "fixture-LIBAVAHI-path"
# if NUTSCAN_SELECTION_VARIANT != 3 && NUTSCAN_SELECTION_VARIANT != 4
#  define SONAME_LIBAVAHI "fixture-LIBAVAHI-soname"
# endif
# if NUTSCAN_SELECTION_VARIANT == 2
#  undef SOFILE_LIBAVAHI
#  define SOFILE_LIBAVAHI SONAME_LIBAVAHI
# elif NUTSCAN_SELECTION_VARIANT == 4
#  undef SOFILE_LIBAVAHI
# endif
# undef SONAME_LIBGIO
# undef SOFILE_LIBGIO
# undef SOPATH_LIBGIO
# define SOFILE_LIBGIO "fixture-LIBGIO-file"
# define SOPATH_LIBGIO "fixture-LIBGIO-path"
# if NUTSCAN_SELECTION_VARIANT != 3 && NUTSCAN_SELECTION_VARIANT != 4
#  define SONAME_LIBGIO "fixture-LIBGIO-soname"
# endif
# if NUTSCAN_SELECTION_VARIANT == 2
#  undef SOFILE_LIBGIO
#  define SOFILE_LIBGIO SONAME_LIBGIO
# elif NUTSCAN_SELECTION_VARIANT == 4
#  undef SOFILE_LIBGIO
# endif
# undef SONAME_LIBFREEIPMI
# undef SOFILE_LIBFREEIPMI
# undef SOPATH_LIBFREEIPMI
# define SOFILE_LIBFREEIPMI "fixture-LIBFREEIPMI-file"
# define SOPATH_LIBFREEIPMI "fixture-LIBFREEIPMI-path"
# if NUTSCAN_SELECTION_VARIANT != 3 && NUTSCAN_SELECTION_VARIANT != 4
#  define SONAME_LIBFREEIPMI "fixture-LIBFREEIPMI-soname"
# endif
# if NUTSCAN_SELECTION_VARIANT == 2
#  undef SOFILE_LIBFREEIPMI
#  define SOFILE_LIBFREEIPMI SONAME_LIBFREEIPMI
# elif NUTSCAN_SELECTION_VARIANT == 4
#  undef SOFILE_LIBFREEIPMI
# endif
# if NUTSCAN_SELECTION_VARIANT == 5
#  undef SOPATH_LIBUSB1
#  define SOPATH_LIBUSB1 "libusb-1.0" SOEXT
#  undef SOPATH_LIBUSB0
#  define SOPATH_LIBUSB0 "libusb-0.1" SOEXT
#  undef SOPATH_LIBNETSNMP
#  define SOPATH_LIBNETSNMP "libnetsnmp" SOEXT
#  undef SOPATH_LIBNEON
#  define SOPATH_LIBNEON "libneon" SOEXT
#  undef SOPATH_LIBAVAHI
#  define SOPATH_LIBAVAHI "libavahi-client" SOEXT
#  undef SOPATH_LIBGIO
#  define SOPATH_LIBGIO "libgio-2.0" SOEXT
#  undef SOPATH_LIBFREEIPMI
#  define SOPATH_LIBFREEIPMI "libfreeipmi" SOEXT
# endif
# if NUTSCAN_SELECTION_VARIANT == 6
#  undef WITH_LIBUSB_1_0
#  define WITH_LIBUSB_1_0 0
# endif
# if NUTSCAN_SELECTION_VARIANT == 7
#  undef WITH_SNMP_STATIC
#  define WITH_SNMP_STATIC 1
# endif
# if NUTSCAN_SELECTION_VARIANT == 8
#  undef WITH_USB
#  define WITH_USB 0
#  undef WITH_SNMP
#  define WITH_SNMP 0
#  undef WITH_NEON
#  define WITH_NEON 0
#  undef WITH_AVAHI
#  define WITH_AVAHI 0
#  undef WITH_UPOWER
#  define WITH_UPOWER 0
#  undef WITH_FREEIPMI
#  define WITH_FREEIPMI 0
# endif
#endif

#define nut_prepare_search_paths fixture_prepare_paths
#define upsdebugx_report_search_paths fixture_report_paths
#define get_libname fixture_resolve
#define s_upsdebugx fixture_debug
#define sem_init fixture_sem_init
#define sem_destroy fixture_sem_destroy
#define sem_open fixture_sem_open
#define sem_close fixture_sem_close
#define sem_unlink fixture_sem_unlink
#define pthread_mutex_init fixture_mutex_init
#define pthread_mutex_destroy fixture_mutex_destroy
#define lt_dlclose fixture_dlclose
#define lt_dlexit fixture_dlexit
#define free fixture_free
#ifndef NUTSCAN_SELECTION_SOURCE
# define NUTSCAN_SELECTION_SOURCE "nutscan-init.c"
#endif
#include NUTSCAN_SELECTION_SOURCE
#undef free
extern void free(void *ptr);

#define ADAPTERS 7
#define MAX_NAMES 10
#define MAX_EVENTS 64
static int failures = 0, cases = 0, active = 0, resolver_hit = -1;
static int load_success = -1, resolve_calls = 0, load_calls = 0;
static int unload_calls = 0, close_calls = 0, exit_calls = 0;
static int resolver_frees = 0, trace = 0;
static void *resolved_pointer = NULL;
static lt_dlhandle handles[ADAPTERS];
static int tokens[ADAPTERS];
static char *saved[ADAPTERS];
static int *availability[ADAPTERS] = {
	&nutscan_avail_usb, &nutscan_avail_snmp, &nutscan_avail_xml_http,
	&nutscan_avail_avahi, &nutscan_avail_upower, &nutscan_avail_ipmi,
	&nutscan_avail_nut
};
static const char *labels[ADAPTERS] = {
	"LibUSB", "LibSNMP", "LibNeon", "LibAvahi", "LibGIO",
	"LibFreeIPMI", "NUT Client library"
};
static struct {
	const char *resolve[MAX_NAMES];
	const char *fallback[MAX_NAMES];
	int reset[MAX_NAMES];
	int nr, nf, enabled, is_static;
} spec[ADAPTERS];
static const char *seen_resolve[MAX_EVENTS], *seen_load[MAX_EVENTS];
static int seen_reset[MAX_EVENTS], seen_result[MAX_EVENTS];
static int reset_pending, script_calls, saved_allocs, saved_frees;
static int diagnostic_count, diagnostic_resolved, diagnostic_fallback, diagnostic_verdict;
static int mutex_inits, mutex_destroys, semaphore_inits, semaphore_destroys;
static int semaphore_opens, semaphore_closes, semaphore_unlinks;

static void check(int condition, const char *what)
{
	if (!condition) {
		fprintf(stderr, "FAIL adapter=%d case=%d: %s\n", active, cases, what);
		failures++;
	}
}

void fixture_prepare_paths(void)
{
}

void fixture_report_paths(int level, int builtin)
{
	NUT_UNUSED_VARIABLE(level);
	NUT_UNUSED_VARIABLE(builtin);
}

void fixture_free(void *ptr)
{
	if (ptr && ptr == resolved_pointer) {
		resolver_frees++;
		resolved_pointer = NULL;
	}
	{
		int i;
		for (i = 0; ptr && i < ADAPTERS; i++) {
			if (ptr == saved[i]) {
				saved_frees++;
			}
		}
	}
	free(ptr);
}

void fixture_debug(int level, const char *fmt, ...)
{
	char text[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(text, sizeof(text), fmt, ap);
	va_end(ap);
	if (strstr(text, labels[active])) {
		diagnostic_count++;
		if (strstr(text, "get_libname() resolved")) {
			diagnostic_resolved++;
			check(!strncmp(text, "nutscan_init: ", 14) && level == 1, "resolved diagnostic caller/level");
		} else if (strstr(text, "default resolver")) {
			diagnostic_fallback++;
			check(!strncmp(text, "nutscan_init: ", 14) && level == 1, "fallback diagnostic caller/level");
		} else if (strstr(text, "succeeded to load")) {
			diagnostic_verdict++;
			check(!strncmp(text, "nutscan_init: ", 14) && level == 1, "success diagnostic caller/level");
		} else if (strstr(text, "Cannot load")) {
			diagnostic_verdict++;
			check(level == 0, "failure diagnostic level");
		}
	}
	if (trace && strstr(text, labels[active])
	&& (strstr(text, "nutscan_init:") || strstr(text, "Cannot load"))
	) {
		printf("D%d:%s\n", level, text);
	}
}

char *fixture_resolve(const char *name)
{
	int id, i, known = 0, relevant = 0;
	for (id = 0; id < ADAPTERS; id++) {
		for (i = 0; i < spec[id].nr; i++) {
			if (!strcmp(name, spec[id].resolve[i])) {
				known = 1;
				if (id == active) {
					relevant = 1;
				}
			}
		}
	}
	check(known, "resolver name belongs to an enabled adapter");
	if (!relevant) {
		return NULL;
	}
	check(resolve_calls < MAX_EVENTS, "resolver event capacity");
	seen_resolve[resolve_calls] = name;
	if (trace) {
		printf("R:%s\n", name);
	}
	if (resolve_calls++ == resolver_hit) {
		resolved_pointer = xstrdup("fixture-resolved-path");
		return resolved_pointer;
	}
	return NULL;
}

static int fixture_load(int id, const char *name)
{
	int result;
	if (id != active) {
		return 0;
	}
	if (handles[id] == (lt_dlhandle)1) {
		result = 0;
	} else if (handles[id]) {
		result = 1;
	} else if (script_calls++ == load_success) {
		result = 7;
		handles[id] = (lt_dlhandle)&tokens[id];
		saved[id] = xstrdup(name);
		saved_allocs++;
	} else {
		result = 0;
		handles[id] = (lt_dlhandle)1;
		fixture_dlexit();
	}
	check(load_calls < MAX_EVENTS, "loader event capacity");
	seen_load[load_calls] = name == resolved_pointer ? "fixture-resolved-path" : name;
	seen_reset[load_calls] = reset_pending;
	seen_result[load_calls] = result;
	reset_pending = 0;
	load_calls++;
	if (trace) {
		printf("L:%s:%d\n", name, result);
	}
	return result;
}

static int fixture_unload(int id)
{
	if (id == active) {
		unload_calls++;
		reset_pending = 1;
		if (trace) {
			printf("U\n");
		}
	}
	return nutscan_unload_library(availability[id], &handles[id], &saved[id]);
}

int fixture_dlclose(lt_dlhandle handle)
{
	check(handle == (lt_dlhandle)&tokens[active], "close fixture handle only");
	close_calls++;
	return 0;
}

int fixture_dlexit(void)
{
	exit_calls++;
	return 0;
}

#ifdef HAVE_PTHREAD
int fixture_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *attr)
{
	mutex_inits++;
	NUT_UNUSED_VARIABLE(m);
	NUT_UNUSED_VARIABLE(attr);
	return 0;
}

int fixture_mutex_destroy(pthread_mutex_t *m)
{
	mutex_destroys++;
	NUT_UNUSED_VARIABLE(m);
	return 0;
}
# if defined HAVE_SEMAPHORE_NAMED || defined HAVE_SEMAPHORE_UNNAMED
int fixture_sem_init(sem_t *sem, int shared, unsigned int value)
{
	semaphore_inits++;
	NUT_UNUSED_VARIABLE(sem);
	NUT_UNUSED_VARIABLE(shared);
	NUT_UNUSED_VARIABLE(value);
	return 0;
}

int fixture_sem_destroy(sem_t *sem)
{
	semaphore_destroys++;
	NUT_UNUSED_VARIABLE(sem);
	return 0;
}
#ifdef WIN32
sem_t *fixture_sem_open(const char *name, int flags, mode_t mode, unsigned int value)
#else
sem_t *fixture_sem_open(const char *name, int flags, ...)
#endif
{
	static sem_t object;
#ifdef WIN32
	NUT_UNUSED_VARIABLE(mode);
	NUT_UNUSED_VARIABLE(value);
#endif
	semaphore_opens++;
	NUT_UNUSED_VARIABLE(name);
	NUT_UNUSED_VARIABLE(flags);
	return &object;
}

int fixture_sem_close(sem_t *sem)
{
	semaphore_closes++;
	NUT_UNUSED_VARIABLE(sem);
	return 0;
}

int fixture_sem_unlink(const char *name)
{
	semaphore_unlinks++;
	NUT_UNUSED_VARIABLE(name);
	return 0;
}
# endif
#endif

int nutscan_load_usb_library(const char *name)
{
	return fixture_load(0, name);
}

int nutscan_unload_usb_library(void)
{
	return fixture_unload(0);
}

int nutscan_load_snmp_library(const char *name)
{
	return fixture_load(1, name);
}

int nutscan_unload_snmp_library(void)
{
	return fixture_unload(1);
}

int nutscan_load_neon_library(const char *name)
{
	return fixture_load(2, name);
}

int nutscan_unload_neon_library(void)
{
	return fixture_unload(2);
}

int nutscan_load_avahi_library(const char *name)
{
	return fixture_load(3, name);
}

int nutscan_unload_avahi_library(void)
{
	return fixture_unload(3);
}

int nutscan_load_upower_library(const char *name)
{
	return fixture_load(4, name);
}

int nutscan_unload_upower_library(void)
{
	return fixture_unload(4);
}

int nutscan_load_ipmi_library(const char *name)
{
	return fixture_load(5, name);
}

int nutscan_unload_ipmi_library(void)
{
	return fixture_unload(5);
}

int nutscan_load_upsclient_library(const char *name)
{
	return fixture_load(6, name);
}

int nutscan_unload_upsclient_library(void)
{
	return fixture_unload(6);
}

void nutscan_upscli_set_debug_level(int level, const void *cookie)
{
	NUT_UNUSED_VARIABLE(level);
	NUT_UNUSED_VARIABLE(cookie);
}

void nutscan_upscli_setprocname(const char *name, const void *cookie)
{
	NUT_UNUSED_VARIABLE(name);
	NUT_UNUSED_VARIABLE(cookie);
}

void nutscan_upscli_setproctag(const char *tag, const void *cookie)
{
	NUT_UNUSED_VARIABLE(tag);
	NUT_UNUSED_VARIABLE(cookie);
}

struct timeval *nutscan_upscli_upslog_start_sync(struct timeval *tv, const void *cookie)
{
	NUT_UNUSED_VARIABLE(cookie);
	return tv;
}

/* Independent specification table: never inspect production candidate arrays. */
static void expected(int id, const char *name, int reset)
{
	int n = spec[id].nr++, f = spec[id].nf++;
	spec[id].resolve[n] = name;
	spec[id].fallback[f] = name;
	spec[id].reset[f] = reset;
}

static void specification(void)
{
#if defined WITH_USB && WITH_USB
	spec[0].enabled = 1;
#if WITH_LIBUSB_1_0
#ifdef SONAME_LIBUSB1
	expected(0, SONAME_LIBUSB1, 0);
#endif
#ifdef SOFILE_LIBUSB1
# ifdef SONAME_LIBUSB1
#include "nut-pragmas-unreachable-code.h"
	if (strcmp(SONAME_LIBUSB1, SOFILE_LIBUSB1)) {
		expected(0, SOFILE_LIBUSB1, 1);
	}
#include "nut-pragmas-unreachable-code-end.h"
# else
	expected(0, SOFILE_LIBUSB1, 0);
# endif
#endif
	expected(0, "libusb-1.0" SOEXT, 1);
#ifdef SOPATH_LIBUSB1
	expected(0, SOPATH_LIBUSB1, 1);
#endif
	expected(0, "libusb" SOEXT, 1);
#else
#ifdef SONAME_LIBUSB0
	expected(0, SONAME_LIBUSB0, 0);
#endif
#ifdef SOFILE_LIBUSB0
# ifdef SONAME_LIBUSB0
#include "nut-pragmas-unreachable-code.h"
	if (strcmp(SONAME_LIBUSB0, SOFILE_LIBUSB0)) {
		expected(0, SOFILE_LIBUSB0, 1);
	}
#include "nut-pragmas-unreachable-code-end.h"
# else
	expected(0, SOFILE_LIBUSB0, 0);
# endif
#endif
	expected(0, "libusb-0.1" SOEXT, 1);
#ifdef WIN32
	expected(0, "libusb-0-1-4" SOEXT, 1);
#endif
#ifdef SOPATH_LIBUSB0
	expected(0, SOPATH_LIBUSB0, 1);
#endif
	expected(0, "libusb" SOEXT, 1);
#endif
#endif
#if defined WITH_SNMP && WITH_SNMP
	spec[1].enabled = 1;
# if defined WITH_SNMP_STATIC && WITH_SNMP_STATIC
	spec[1].is_static = 1;
# else
#ifdef SONAME_LIBNETSNMP
	expected(1, SONAME_LIBNETSNMP, 0);
#endif
#ifdef SOFILE_LIBNETSNMP
# ifdef SONAME_LIBNETSNMP
#include "nut-pragmas-unreachable-code.h"
	if (strcmp(SONAME_LIBNETSNMP, SOFILE_LIBNETSNMP)) {
		expected(1, SOFILE_LIBNETSNMP, 1);
	}
#include "nut-pragmas-unreachable-code-end.h"
# else
	expected(1, SOFILE_LIBNETSNMP, 0);
# endif
#endif
	expected(1, "libnetsnmp" SOEXT, 1);
#ifdef WIN32
	expected(1, "libnetsnmp-40" SOEXT, 1);
#endif
#ifdef SOPATH_LIBNETSNMP
	expected(1, SOPATH_LIBNETSNMP, 1);
#endif
# endif
#endif
#if defined WITH_NEON && WITH_NEON
	spec[2].enabled = 1;
#ifdef SONAME_LIBNEON
	expected(2, SONAME_LIBNEON, 0);
#endif
#ifdef SOFILE_LIBNEON
# ifdef SONAME_LIBNEON
#include "nut-pragmas-unreachable-code.h"
	if (strcmp(SONAME_LIBNEON, SOFILE_LIBNEON)) {
		expected(2, SOFILE_LIBNEON, 1);
	}
#include "nut-pragmas-unreachable-code-end.h"
# else
	expected(2, SOFILE_LIBNEON, 0);
# endif
#endif
	expected(2, "libneon" SOEXT, 1);
	expected(2, "libneon-gnutls" SOEXT, 1);
#ifdef WIN32
	expected(2, "libneon-27" SOEXT, 1);
	expected(2, "libneon-gnutls-27" SOEXT, 1);
#endif
#ifdef SOPATH_LIBNEON
	expected(2, SOPATH_LIBNEON, 1);
#endif
#endif
#if defined WITH_AVAHI && WITH_AVAHI
	spec[3].enabled = 1;
#ifdef SONAME_LIBAVAHI
	expected(3, SONAME_LIBAVAHI, 0);
#endif
#ifdef SOFILE_LIBAVAHI
# ifdef SONAME_LIBAVAHI
#include "nut-pragmas-unreachable-code.h"
	if (strcmp(SONAME_LIBAVAHI, SOFILE_LIBAVAHI)) {
		expected(3, SOFILE_LIBAVAHI, 1);
	}
#include "nut-pragmas-unreachable-code-end.h"
# else
	expected(3, SOFILE_LIBAVAHI, 0);
# endif
#endif
	expected(3, "libavahi-client" SOEXT, 1);
#ifdef SOPATH_LIBAVAHI
	expected(3, SOPATH_LIBAVAHI, 1);
#endif
#endif
#if defined WITH_UPOWER && WITH_UPOWER
	spec[4].enabled = 1;
#ifdef SONAME_LIBGIO
	expected(4, SONAME_LIBGIO, 0);
#endif
#ifdef SOFILE_LIBGIO
# ifdef SONAME_LIBGIO
#include "nut-pragmas-unreachable-code.h"
	if (strcmp(SONAME_LIBGIO, SOFILE_LIBGIO)) {
		expected(4, SOFILE_LIBGIO, 1);
	}
#include "nut-pragmas-unreachable-code-end.h"
# else
	expected(4, SOFILE_LIBGIO, 0);
# endif
#endif
	expected(4, "libgio-2.0" SOEXT, 1);
#ifdef SOPATH_LIBGIO
	expected(4, SOPATH_LIBGIO, 1);
#endif
#endif
#if defined WITH_FREEIPMI && WITH_FREEIPMI
	spec[5].enabled = 1;
#ifdef SONAME_LIBFREEIPMI
	expected(5, SONAME_LIBFREEIPMI, 0);
#endif
#ifdef SOFILE_LIBFREEIPMI
# ifdef SONAME_LIBFREEIPMI
#include "nut-pragmas-unreachable-code.h"
	if (strcmp(SONAME_LIBFREEIPMI, SOFILE_LIBFREEIPMI)) {
		expected(5, SOFILE_LIBFREEIPMI, 1);
	}
#include "nut-pragmas-unreachable-code-end.h"
# else
	expected(5, SOFILE_LIBFREEIPMI, 0);
# endif
#endif
	expected(5, "libfreeipmi" SOEXT, 1);
#ifdef SOPATH_LIBFREEIPMI
	expected(5, SOPATH_LIBFREEIPMI, 1);
#endif
#endif

	spec[6].enabled = 1;
#ifdef SOFILE_LIBUPSCLIENT
	expected(6, SOFILE_LIBUPSCLIENT, 0);
#endif
	expected(6, "libupsclient" SOEXT, 1);
#ifdef SOPATH_LIBUPSCLIENT
	expected(6, SOPATH_LIBUPSCLIENT, 1);
#endif
#ifdef WIN32
	expected(6, "libupsclient-6" SOEXT, 1);
	expected(6, "libupsclient-3" SOEXT, 1);
# ifdef SOPATH_LIBUPSCLIENT
	/* Resolver has P before W; fallback has P after W. */
	spec[6].fallback[spec[6].nf - 3] = "libupsclient-6" SOEXT;
	spec[6].fallback[spec[6].nf - 2] = "libupsclient-3" SOEXT;
	spec[6].fallback[spec[6].nf - 1] = SOPATH_LIBUPSCLIENT;
# endif
#endif
}

static void exercise(int state, int hit, int success)
{
	int i, expected_loads = 0, expected_result = *availability[active];
	int model = state, expected_unloads = 0, expected_scripts = 0;
	int expected_exits = 0;
	int old_mutex = mutex_inits, old_sem = semaphore_inits, old_open = semaphore_opens;

	resolver_hit = hit;
	load_success = success;
	resolve_calls = load_calls = unload_calls = close_calls = exit_calls = 0;
	resolver_frees = reset_pending = script_calls = 0;
	resolved_pointer = NULL;
	diagnostic_count = diagnostic_resolved = diagnostic_fallback = diagnostic_verdict = 0;
	cases++;
	if (trace) {
		printf("CASE:%d:%d:%d:%d:%d\n", active, state, hit, success, nut_debug_level);
	}
	nutscan_init();
	check(resolve_calls == (hit < 0 ? spec[active].nr : hit + 1), "resolver count");
	for (i = 0; i < resolve_calls; i++) {
		check(!strcmp(seen_resolve[i], spec[active].resolve[i]), "resolver order");
	}
	if (!spec[active].enabled || spec[active].is_static) {
		expected_result = spec[active].is_static ? 1 : expected_result;
	} else if (hit >= 0) {
		expected_loads = 1;
		expected_result = state == 2 ? 1 : (state == 1 ? 0 : (success == 0 ? 7 : 0));
		expected_scripts = state == 0 ? 1 : 0;
		expected_exits = state == 0 && expected_result == 0 ? 1 : 0;
		check(!strcmp(seen_load[0], "fixture-resolved-path"), "resolved path load");
		check(seen_reset[0] == 0, "no reset on resolved path");
	} else if (state != 2) {
		for (i = 0; i < spec[active].nf; i++) {
			int result;
			if (spec[active].reset[i]) {
				model = 0;
				expected_unloads++;
			}
			result = 0;
			if (model == 0) {
				result = expected_scripts++ == success ? 7 : 0;
				if (!result) {
					expected_exits++;
				}
			}
			model = result ? 2 : 1;
			check(i < load_calls, "fallback attempt exists");
			if (i < load_calls) {
				check(!strcmp(seen_load[i], spec[active].fallback[i]), "fallback order");
				check(seen_reset[i] == spec[active].reset[i], "reset marker");
				check(seen_result[i] == result, "sticky failure result");
			}
			expected_loads++;
			expected_result = result;
			if (result) {
				break;
			}
		}
	}
	if (spec[active].enabled && !spec[active].is_static) {
		check(diagnostic_verdict == ((!expected_result || nut_debug_level) ? 1 : 0), "one final verdict at correct level");
		check(diagnostic_count == (nut_debug_level ? 2 : (!expected_result ? 1 : 0)), "diagnostic count");
		check(diagnostic_resolved == (nut_debug_level && hit >= 0 ? 1 : 0), "resolved diagnostic");
		check(diagnostic_fallback == (nut_debug_level && hit < 0 ? 1 : 0), "fallback diagnostic");
	} else {
		check(diagnostic_count == (nut_debug_level ? 1 : 0) && !diagnostic_verdict, "disabled/static diagnostic");
	}
	check(load_calls == expected_loads, "loader count / stop on success");
	check(script_calls == expected_scripts, "script consumed only by fresh loader");
	check(unload_calls == expected_unloads, "unload count");
	check(close_calls == 0 && exit_calls == expected_exits, "selection ltdl lifecycle");
	check(*availability[active] == expected_result, "exact availability");
	check(resolver_frees == (hit >= 0 ? 1 : 0), "resolver allocation freed once");
	check((saved[active] != NULL) == (handles[active] && handles[active] != (lt_dlhandle)1), "saved path ownership");
#ifdef HAVE_PTHREAD
# ifdef HAVE_PTHREAD_TRYJOIN
	check(mutex_inits == old_mutex + 1, "mutex init intercepted");
# else
	check(mutex_inits == old_mutex, "no unexpected mutex init");
# endif
# ifdef HAVE_SEMAPHORE_UNNAMED
	check(semaphore_inits == old_sem + 1 && semaphore_opens == old_open, "unnamed semaphore intercepted");
# elif defined HAVE_SEMAPHORE_NAMED
	check(semaphore_opens == old_open + 1 && semaphore_inits == old_sem, "named semaphore intercepted");
# else
	check(semaphore_inits == old_sem && semaphore_opens == old_open, "no unexpected semaphore init");
# endif
#else
	check(mutex_inits == old_mutex && semaphore_inits == old_sem && semaphore_opens == old_open, "threading disabled");
#endif
	if (trace) {
		printf("RESULT:%d:%d:%d:%d:%d\n", *availability[active], unload_calls, close_calls, exit_calls, script_calls);
	}
}

static void scenario(int state, int hit, int success)
{
	int i, loaded, old_exits;
	int wants_trace = trace;
	for (i = 0; i < ADAPTERS; i++) {
		handles[i] = NULL;
		saved[i] = NULL;
		*availability[i] = 0;
	}
	saved_allocs = saved_frees = 0;
	if (state == 1) {
		handles[active] = (lt_dlhandle)1;
	} else if (state == 2) {
		handles[active] = (lt_dlhandle)&tokens[active];
		saved[active] = xstrdup("fixture-entry-path");
		saved_allocs++;
		*availability[active] = 1;
	}
	exercise(state, hit, success);
	/* Actual re-entry: preserve handles, saved paths and raw availability. */
	state = handles[active] == (lt_dlhandle)1 ? 1 : (handles[active] ? 2 : 0);
	exercise(state, -1, -1);
	loaded = handles[active] && handles[active] != (lt_dlhandle)1;
	old_exits = exit_calls;
	trace = 0;
	for (i = 0; i < ADAPTERS; i++) {
		fixture_unload(i);
		check(!handles[i] && !saved[i] && !*availability[i], "complete teardown state");
	}
	check(close_calls == loaded && exit_calls == old_exits + loaded, "teardown closes only loaded handle");
	check(saved_allocs == saved_frees, "saved paths freed exactly once");
	check(mutex_destroys == 0 && semaphore_destroys == 0 && semaphore_closes == 0 && semaphore_unlinks == 0, "no live OS teardown");
	trace = wants_trace;
}

int main(int argc, char **argv)
{
	int level, state, hit, success, wants_trace = 0;
	if (argc > 1 && !strcmp(argv[1], "--trace")) {
		wants_trace = 1;
	}
	specification();
	for (active = 0; active < ADAPTERS; active++) {
		for (level = 0; level <= 1; level++) {
			nut_debug_level = level;
			for (state = 0; state < 3; state++) {
				for (hit = 0; hit < spec[active].nr; hit++) {
					trace = wants_trace;
					scenario(state, hit, -1);
					trace = wants_trace;
					scenario(state, hit, 0);
				}
				for (success = -1; success < spec[active].nf; success++) {
					trace = wants_trace;
					scenario(state, -1, success);
				}
			}
		}
	}
	printf("%d cases, %d failures, variant %d\n", cases, failures, NUTSCAN_SELECTION_VARIANT);
	return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
