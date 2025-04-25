/*
 *  Copyright (C) 2011 - 2024  Arnaud Quette <arnaud.quette@free.fr>
 *  Copyright (C) 2016 Michal Vyskocil <MichalVyskocil@eaton.com>
 *  Copyright (C) 2016 - 2021 Jim Klimov <EvgenyKlimov@eaton.com>
 *  Copyright (C) 2022 - 2024 Jim Klimov <jimklimov+nut@gmail.com>
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

/*! \file nut-scanner.c
    \brief A tool to detect NUT supported devices
    \author Arnaud Quette <arnaud.quette@free.fr>
    \author Michal Vyskocil <MichalVyskocil@eaton.com>
    \author Jim Klimov <EvgenyKlimov@eaton.com>
    \author Jim Klimov <jimklimov+nut@gmail.com>
*/

#include "common.h"	/* Must be first include to pull "config.h" */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

/* Headers related to getifaddrs() for `-m auto` on different platforms */
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#include <sys/types.h>
#ifndef WIN32
# include <arpa/inet.h>
# include <netinet/in.h>
# ifdef HAVE_IFADDRS_H
#  include <ifaddrs.h>
# endif
# include <netdb.h>
# include <sys/ioctl.h>
# include <net/if.h>
#else	/* WIN32 */
# if defined HAVE_WINSOCK2_H && HAVE_WINSOCK2_H
#  include <winsock2.h>
# endif
# if defined HAVE_IPHLPAPI_H && HAVE_IPHLPAPI_H
#  include <iphlpapi.h>
# endif
# include <ws2tcpip.h>
# include <wspiapi.h>
# ifndef AI_NUMERICSERV
#  define AI_NUMERICSERV NI_NUMERICSERV
# endif
# include "wincompat.h"
#endif	/* WIN32 */

#include "nut_stdint.h"

#ifdef HAVE_PTHREAD
# include <pthread.h>
# if (defined HAVE_SEMAPHORE_UNNAMED) || (defined HAVE_SEMAPHORE_NAMED)
#  include <semaphore.h>
# endif
# if (defined HAVE_PTHREAD_TRYJOIN) || (defined HAVE_SEMAPHORE_UNNAMED) || (defined HAVE_SEMAPHORE_NAMED)
#  ifdef HAVE_SYS_RESOURCE_H
#   include <sys/resource.h> /* for getrlimit() and struct rlimit */
#   include <errno.h>

/* 3 is reserved for known overhead (for NetXML at least)
 * following practical investigation summarized at
 *   https://github.com/networkupstools/nut/pull/1158
 * and probably means the usual stdin/stdout/stderr triplet
 * Another +1 is for NetSNMP which wants to open MIB files,
 * potential per-host configuration files, etc.
 */
#   define RESERVE_FD_COUNT 4
#  endif /* HAVE_SYS_RESOURCE_H */
# endif  /* HAVE_PTHREAD_TRYJOIN || HAVE_SEMAPHORE_UNNAMED || HAVE_SEMAPHORE_NAMED */
#endif   /* HAVE_PTHREAD */

#include "nut-scan.h"

#define ERR_BAD_OPTION	(-1)

static const char optstring[] = "?ht:T:s:e:E:c:l:u:W:X:w:x:p:b:B:d:L:CUSMOAm:QnNPqIVaD";

#ifdef HAVE_GETOPT_LONG
static const struct option longopts[] = {
	{ "timeout", required_argument, NULL, 't' },
	{ "thread", required_argument, NULL, 'T' },
	{ "start_ip", required_argument, NULL, 's' },
	{ "end_ip", required_argument, NULL, 'e' },
	{ "eaton_serial", required_argument, NULL, 'E' },
	{ "mask_cidr", required_argument, NULL, 'm' },
	{ "community", required_argument, NULL, 'c' },
	{ "secLevel", required_argument, NULL, 'l' },
	{ "secName", required_argument, NULL, 'u' },
	{ "authPassword", required_argument, NULL, 'W' },
	{ "privPassword", required_argument, NULL, 'X' },
	{ "authProtocol", required_argument, NULL, 'w' },
	{ "privProtocol", required_argument, NULL, 'x' },
	{ "username", required_argument, NULL, 'b' },
	{ "password", required_argument, NULL, 'B' },
	{ "authType", required_argument, NULL, 'd' },
	{ "cipher_suite_id", required_argument, NULL, 'L' },
	{ "port", required_argument, NULL, 'p' },
	{ "complete_scan", no_argument, NULL, 'C' },
	{ "usb_scan", no_argument, NULL, 'U' },
	{ "snmp_scan", no_argument, NULL, 'S' },
	{ "xml_scan", no_argument, NULL, 'M' },
	{ "oldnut_scan", no_argument, NULL, 'O' },	/* "old" NUT libupsclient.so scan */
	{ "avahi_scan", no_argument, NULL, 'A' },	/* "new" NUT scan where deployed */
	{ "nut_simulation_scan", no_argument, NULL, 'n' },
	{ "ipmi_scan", no_argument, NULL, 'I' },
	{ "disp_nut_conf_with_sanity_check", no_argument, NULL, 'Q' },
	{ "disp_nut_conf", no_argument, NULL, 'N' },
	{ "disp_parsable", no_argument, NULL, 'P' },
	{ "quiet", no_argument, NULL, 'q' },
	{ "help", no_argument, NULL, 'h' },
	{ "version", no_argument, NULL, 'V' },
	{ "available", no_argument, NULL, 'a' },
	{ "nut_debug_level", no_argument, NULL, 'D' },
	{ NULL, 0, NULL, 0 }
};
#else
#define getopt_long(a,b,c,d,e)	getopt(a,b,c)
#endif /* HAVE_GETOPT_LONG */

static nutscan_device_t *dev[TYPE_END];

static useconds_t timeout = DEFAULT_NETWORK_TIMEOUT * 1000 * 1000; /* in usec */
static char * port = NULL;
static char * serial_ports = NULL;
static int cli_link_detail_level = -1;

/* Track requested IP ranges (from CLI or auto-discovery) */
static nutscan_ip_range_list_t ip_ranges_list;

#ifdef HAVE_PTHREAD
static pthread_t thread[TYPE_END];

static void * run_usb(void *arg)
{
	nutscan_usb_t scanopts, *scanopts_ptr = &scanopts;

	if (!arg) {
		/* null => use library defaults; should not happen here anyway */
		scanopts_ptr = NULL;
	} else {
		/* 0: do not report bus/device/busport details
		 * 1: report bus and busport, if available
		 * 2: report bus/device/busport details
		 * 3: like (2) and report bcdDevice (limited use and benefit)
		 */
		int link_detail_level = *((int*)arg);

		switch (link_detail_level) {
			case 0:
				scanopts.report_bus = 0;
				scanopts.report_busport = 0;
				scanopts.report_device = 0;
				scanopts.report_bcdDevice = 0;
				break;

			case 1:
				scanopts.report_bus = 1;
				scanopts.report_busport = 1;
				scanopts.report_device = 0;
				scanopts.report_bcdDevice = 0;
				break;

			case 2:
				scanopts.report_bus = 1;
				scanopts.report_busport = 1;
				scanopts.report_device = 1;
				scanopts.report_bcdDevice = 0;
				break;

			case 3:
				scanopts.report_bus = 1;
				scanopts.report_busport = 1;
				scanopts.report_device = 1;
				scanopts.report_bcdDevice = 1;
				break;

			default:
				upsdebugx(1, "%s: using library default link_detail_level settings", __func__);
				scanopts_ptr = NULL;
		}
	}

	dev[TYPE_USB] = nutscan_scan_usb(scanopts_ptr);
	return NULL;
}
#endif	/* HAVE_PTHREAD */

static void * run_snmp(void * arg)
{
	nutscan_snmp_t * sec = (nutscan_snmp_t *)arg;

	upsdebugx(2, "Entering %s for %" PRIuSIZE " IP address range(s)",
		__func__, ip_ranges_list.ip_ranges_count);

	dev[TYPE_SNMP] = nutscan_scan_ip_range_snmp(&ip_ranges_list, timeout, sec);

	upsdebugx(2, "Finished %s loop", __func__);
	return NULL;
}

static void * run_xml(void * arg)
{
	nutscan_xml_t * sec = (nutscan_xml_t *)arg;

	upsdebugx(2, "Entering %s for %" PRIuSIZE " IP address range(s)",
		__func__, ip_ranges_list.ip_ranges_count);

	dev[TYPE_XML] = nutscan_scan_ip_range_xml_http(&ip_ranges_list, timeout, sec);

	upsdebugx(2, "Finished %s loop", __func__);
	return NULL;
}

static void * run_nut_old(void *arg)
{
	NUT_UNUSED_VARIABLE(arg);

	upsdebugx(2, "Entering %s for %" PRIuSIZE " IP address range(s)",
		__func__, ip_ranges_list.ip_ranges_count);

	dev[TYPE_NUT] = nutscan_scan_ip_range_nut(&ip_ranges_list, port, timeout);

	upsdebugx(2, "Finished %s loop", __func__);
	return NULL;
}

static void * run_nut_simulation(void *arg)
{
	NUT_UNUSED_VARIABLE(arg);

	dev[TYPE_NUT_SIMULATION] = nutscan_scan_nut_simulation();
	return NULL;
}

static void * run_avahi(void *arg)
{
	NUT_UNUSED_VARIABLE(arg);

	dev[TYPE_AVAHI] = nutscan_scan_avahi(timeout);
	return NULL;
}

static void * run_ipmi(void * arg)
{
	nutscan_ipmi_t * sec = (nutscan_ipmi_t *)arg;
	
	upsdebugx(2, "Entering %s for %" PRIuSIZE " IP address range(s)",
		__func__, ip_ranges_list.ip_ranges_count);

	dev[TYPE_IPMI] = nutscan_scan_ip_range_ipmi(&ip_ranges_list, sec);

	upsdebugx(2, "Finished %s loop", __func__);
	return NULL;
}

static void * run_eaton_serial(void *arg)
{
	char * arg_serial_ports = (char *)arg;

	dev[TYPE_EATON_SERIAL] = nutscan_scan_eaton_serial(arg_serial_ports);
	return NULL;
}

static void handle_arg_cidr(const char *arg_addr, int *auto_nets_ptr)
{
	char	*start_ip = NULL, *end_ip = NULL;
	/* Scanning mode: IPv4, IPv6 or both */
	int	auto_nets = -1;
	/* Bit-length limit for *address part* of subnets to consider;
	 * e.g. if your LAN's network range is 10.2.3.0/24 the address
	 * part is (32-24)=8. Larger subnets e.g. 10.0.0.0/8 would be
	 * ignored to avoid billions of scan requests. Note that while
	 * this is applied to IPv6 also, their typical /64 subnets are
	 * not likely to have a NUT/SNMP/NetXML/... server *that* close
	 * nearby in addressing terms, for a tight filter to find them.
	 */
	long	masklen_hosts_limit = 8;
	char	*s = NULL;
	int	errno_saved;
	size_t	auto_subnets_found = 0;

#ifdef HAVE_GETIFADDRS
	/* NOTE: this ifdef is more precise than ifdef WIN32; assuming
	 * its implementation of getifaddrs() would actually be functional
	 * if it appears in the OS or mingw/cygwin/... shims eventually.
	 * If it would be *not* functional, may have to revert to checking
	 * (also?) for WIN32 here and below. */
	/* Inspired by https://stackoverflow.com/a/63789267/4715872 */
	struct ifaddrs	*ifap = NULL, *ifa = NULL;
#elif defined WIN32 && defined HAVE_GETADAPTERSADDRESSES
	/* TODO: The two WIN32 approaches overlap quite a bit, deduplicate!
	 * First shot comes straight from examples as a starting point, but... */
	/* For Windows newer than Vista. Here and below, inspired by
	 * https://learn.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-getadaptersaddresses
	 * https://stackoverflow.com/questions/122208/how-can-i-get-the-ip-address-of-a-local-computer
	 * https://stackoverflow.com/questions/41139561/find-ip-address-of-the-machine-in-c/41151132#41151132
	 */

#	define WIN32_GAA_WORKING_BUFFER_SIZE	15000
#	define WIN32_GAA_MAX_TRIES	3

	DWORD	dwRetVal = 0;

	/* Set the flags to pass to GetAdaptersAddresses */
	ULONG	flags = GAA_FLAG_INCLUDE_PREFIX;

	/* default to unspecified address family (both IPv4 and IPv6) */
	ULONG	family = AF_UNSPEC;

	PIP_ADAPTER_ADDRESSES		pAddresses = NULL;
	ULONG	outBufLen = 0;
	ULONG	Iterations = 0;

	PIP_ADAPTER_ADDRESSES		pCurrAddresses = NULL;
	PIP_ADAPTER_UNICAST_ADDRESS	pUnicast = NULL;
#elif defined WIN32 && defined HAVE_GETADAPTERSINFO
	/* For Windows older than XP (present but not recommended
	 * in later releases). Here and below, inspired by
	 * https://learn.microsoft.com/en-us/windows/win32/api/iphlpapi/nf-iphlpapi-getadaptersinfo
	 */

	PIP_ADAPTER_INFO	pAdapterInfo;
	PIP_ADAPTER_INFO	pAdapter = NULL;
	DWORD	dwRetVal = 0;

	ULONG	ulOutBufLen = sizeof (IP_ADAPTER_INFO);
#endif

	upsdebugx(3, "Entering %s('%s')", __func__, arg_addr);

#if defined HAVE_GETIFADDRS || (defined WIN32 && (defined HAVE_GETADAPTERSINFO || defined HAVE_GETADAPTERSADDRESSES))
	/* Is this a `-m auto<something_optional>` mode? */
	if (!strncmp(arg_addr, "auto", 4)) {
		/* TODO: Maybe split later, to allow separate
		 *  `-m auto4/X` and `-m auto6/Y` requests?
		 */
		if (auto_nets_ptr && *auto_nets_ptr) {
			upsdebugx(0, "Duplicate request for connected subnet scan ignored");
			return;
		}

		/* Not very efficient to stack strcmp's, but
		 * also not a hot codepath to care much, either.
		 */
		if (!strcmp(arg_addr, "auto")) {
			auto_nets = 46;
		} else if (!strcmp(arg_addr, "auto4")) {
			auto_nets = 4;
		} else if (!strcmp(arg_addr, "auto6")) {
			auto_nets = 6;
		} else if (!strncmp(arg_addr, "auto/", 5)) {
			auto_nets = 46;
			errno = 0;
			masklen_hosts_limit = strtol(arg_addr + 5, &s, 10);
			errno_saved = errno;
			upsdebugx(6, "errno=%d s='%s'(%p) input='%s'(%p) output=%ld",
				errno_saved, NUT_STRARG(s), (void *)s,
				arg_addr + 5, (void *)(arg_addr + 5),
				masklen_hosts_limit);
			if (errno_saved || (s && *s != '\0') || masklen_hosts_limit < 0 || masklen_hosts_limit > 128) {
				fatalx(EXIT_FAILURE,
					"Invalid auto-net limit value, should be an integer [0..128]: %s",
					arg_addr);
			}
		} else if (!strncmp(arg_addr, "auto4/", 6)) {
			auto_nets = 4;
			errno = 0;
			masklen_hosts_limit = strtol(arg_addr + 6, &s, 10);
			errno_saved = errno;
			upsdebugx(6, "errno=%d s='%s'(%p) input='%s'(%p) output=%ld",
				errno_saved, NUT_STRARG(s), (void *)s,
				arg_addr + 6, (void *)(arg_addr + 6),
				masklen_hosts_limit);
			if (errno_saved || (s && *s != '\0') || masklen_hosts_limit < 0 || masklen_hosts_limit > 32) {
				fatalx(EXIT_FAILURE,
					"Invalid auto-net limit value, should be an integer [0..32]: %s",
					arg_addr);
			}
		} else if (!strncmp(arg_addr, "auto6/", 6)) {
			auto_nets = 6;
			errno = 0;
			masklen_hosts_limit = strtol(arg_addr + 6, &s, 10);
			errno_saved = errno;
			upsdebugx(6, "errno=%d s='%s'(%p) input='%s'(%p) output=%ld",
				errno_saved, NUT_STRARG(s), (void *)s,
				arg_addr + 6, (void *)(arg_addr + 6),
				masklen_hosts_limit);
			if (errno_saved || (s && *s != '\0') || masklen_hosts_limit < 0 || masklen_hosts_limit > 128) {
				fatalx(EXIT_FAILURE,
					"Invalid auto-net limit value, should be an integer [0..128]: %s",
					arg_addr);
			}
		} else {
			/* TODO: maybe fail right away?
			 *  Or claim a simple auto46 mode? */
			upsdebugx(0,
				"Got a '-m auto*' CLI option with unsupported "
				"keyword pattern; assuming a CIDR, "
				"likely to fail: %s", arg_addr);
		}

		/* Let the caller know, to allow for run-once support */
		if (auto_nets_ptr) {
			*auto_nets_ptr = auto_nets;
		}
	}
#endif	/* HAVE_GETIFADDRS || HAVE_GETADAPTERSINFO || HAVE_GETADAPTERSADDRESSES */

	if (auto_nets < 0) {
		/* not a supported `-m auto*` pattern => is `-m cidr` */
		upsdebugx(5, "Processing CIDR net/mask: %s", arg_addr);
		nutscan_cidr_to_ip(arg_addr, &start_ip, &end_ip);
		upsdebugx(5, "Extracted IP address range from CIDR net/mask: %s => %s", start_ip, end_ip);

		nutscan_add_ip_range(&ip_ranges_list, start_ip, end_ip);
		start_ip = NULL;
		end_ip = NULL;
		return;
	}

#if defined HAVE_GETIFADDRS || (defined WIN32 && (defined HAVE_GETADAPTERSINFO || defined HAVE_GETADAPTERSADDRESSES))
	/* Handle `-m auto*` modes below */
#ifdef HAVE_GETIFADDRS
	upsdebugx(4, "%s: using getifaddrs()", __func__);

	if (getifaddrs(&ifap) < 0 || !ifap) {
		if (ifap)
			freeifaddrs(ifap);
		fatal_with_errno(EXIT_FAILURE,
			"Failed to getifaddrs() for connected subnet scan");
		/* TOTHINK: Non-fatal, just return / goto finish?
		 * Either way, do not proceed with code below! */
	}
#elif defined WIN32 && defined HAVE_GETADAPTERSADDRESSES
	upsdebugx(4, "%s: using GetAdaptersAddresses()", __func__);

	switch (auto_nets) {
	case 4:
		family = AF_INET;
		break;

	case 6:
		family = AF_INET6;
		break;

	case 46:
	default:
		family = AF_UNSPEC;
		break;
	}

	/* Allocate a 15 KB buffer to start with; we will be told
	 * if more is needed (hence the loop below) */
	outBufLen = WIN32_GAA_WORKING_BUFFER_SIZE;

	do {
		pAddresses = (IP_ADAPTER_ADDRESSES *) xcalloc(1, outBufLen);

		dwRetVal =
		    GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);

		if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
		    free(pAddresses);
		    pAddresses = NULL;
		} else {
		    break;
		}

		Iterations++;
	} while (
		(dwRetVal == ERROR_BUFFER_OVERFLOW)
		&& (Iterations < WIN32_GAA_MAX_TRIES)
		);

	if (dwRetVal != NO_ERROR) {
		char	msgPrefix[LARGEBUF];

		snprintf(msgPrefix, sizeof(msgPrefix),
			"Failed to GetAdaptersAddresses() for "
			"connected subnet scan (%" PRIiMAX ")",
			(intmax_t)dwRetVal);

		if (pAddresses) {
			free(pAddresses);
			pAddresses = NULL;
		}

		if (dwRetVal == ERROR_NO_DATA) {
			fatalx(EXIT_FAILURE, "%s: %s",
				msgPrefix,
				"No addresses were found for the requested parameters");
		} else {
			LPVOID	lpMsgBuf = NULL;

			if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
				FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
				NULL, dwRetVal, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),   
				// Default language
				(LPTSTR) & lpMsgBuf, 0, NULL) && lpMsgBuf
			) {
				fatalx(EXIT_FAILURE, "%s: %s",
					msgPrefix,
					(LPTSTR)lpMsgBuf);
			}

			if (lpMsgBuf)
				LocalFree(lpMsgBuf);
			fatalx(EXIT_FAILURE, "%s", msgPrefix);
		}

		/* TOTHINK: Non-fatal, just return / goto finish?
		 * Either way, do not proceed with code below! */
	}
#elif defined WIN32 && defined HAVE_GETADAPTERSINFO
	upsdebugx(4, "%s: using GetAdaptersInfo()", __func__);

	/* NOTE: IPv4 only! */
	if (auto_nets == 6) {
		fatalx(EXIT_FAILURE,
			"Requested explicitly to query only IPv6 addresses, "
			"but current system libraries support only IPv4."
			);

		/* TOTHINK: Non-fatal, just return / goto finish?
		 * Either way, do not proceed with code below! */
	}

	pAdapterInfo = (IP_ADAPTER_INFO *) xcalloc(1, sizeof (IP_ADAPTER_INFO));

	/* Make an initial call to GetAdaptersInfo to get
	 * the necessary size into the ulOutBufLen variable */
	if (GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW) {
		free(pAdapterInfo);
		pAdapterInfo = (IP_ADAPTER_INFO *) xcalloc(1, ulOutBufLen);
	}

	if ((dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen)) != NO_ERROR) {
		if (pAdapterInfo) {
			free(pAdapterInfo);
			pAdapterInfo = NULL;
		}

		fatalx(EXIT_FAILURE,
			"Failed to GetAdaptersAddresses() for "
			"connected subnet scan (%" PRIiMAX ")",
			(intmax_t)dwRetVal);

		/* TOTHINK: Non-fatal, just return / goto finish?
		 * Either way, do not proceed with code below! */
	}
#else
	fatalx(EXIT_FAILURE,
		"Have no way to query local interface addresses on this "
		"platform, please run without the '-m auto*' options!");
#endif

	/* Initial query did not fail, start a new scope
	 * for more variables, to be allocated just now */
	{
		char	msg[LARGEBUF];
		char	cidr[LARGEBUF];
		/* Note: INET6_ADDRSTRLEN is large enough for IPv4 too,
		 * and is smaller than LARGEBUF to avoid snprintf()
		 * warnings that the result might not fit. */
		char	addr[INET6_ADDRSTRLEN];
		char	mask[INET6_ADDRSTRLEN];
		int	masklen_subnet = 0;
		int	masklen_hosts = 0;
		uintmax_t	ifflags = 0, iftype = 0;

#ifdef HAVE_GETIFADDRS
		/* Every IP address (even if aliases on same interfaces)
		 * has its own "ifa" value.
		 */
		for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
			const char	*ifname = ifa->ifa_name;

			ifflags = (uintmax_t)ifa->ifa_flags;
			iftype  = ifflags;

			if (ifa->ifa_addr)
#elif defined WIN32 && defined HAVE_GETADAPTERSADDRESSES
		/* Nested structure can hold many addresses of different
		 * families assigned to the same physical interface.
		 * https://learn.microsoft.com/en-us/windows/win32/api/iptypes/ns-iptypes-ip_adapter_addresses_lh
		 * https://learn.microsoft.com/en-us/windows/win32/api/iptypes/ns-iptypes-ip_adapter_unicast_address_lh
		 */
		for (pCurrAddresses = pAddresses; pCurrAddresses; pCurrAddresses = pCurrAddresses->Next) {
			char	ifname[LARGEBUF];
			size_t	ifnamelen = 0;

			ifflags = (uintmax_t)pCurrAddresses->Flags;
			iftype  = (uintmax_t)pCurrAddresses->IfType;

			/* Convert some fields from wide chars */
			ifnamelen += snprintf(
				ifname, 
				sizeof(ifname),
				"%s (",
				pCurrAddresses->AdapterName);
			ifnamelen += wcstombs(
				ifname + ifnamelen,
				pCurrAddresses->FriendlyName,
				sizeof(ifname) - ifnamelen);
			ifnamelen += snprintf(
				ifname + ifnamelen,
				sizeof(ifname) - ifnamelen,
				"): ");
			ifnamelen += wcstombs(
				ifname + ifnamelen,
				pCurrAddresses->Description,
				sizeof(ifname) - ifnamelen);

			if ( (ifflags & IP_ADAPTER_IPV6_ENABLED)
			||   (ifflags & IP_ADAPTER_IPV4_ENABLED)
			) for (pUnicast = pCurrAddresses->FirstUnicastAddress; pUnicast; pUnicast = pUnicast->Next)
#elif defined WIN32 && defined HAVE_GETADAPTERSINFO
		/* NOTE: IPv4 only!
		 * https://learn.microsoft.com/en-us/windows/win32/api/iptypes/ns-iptypes-ip_adapter_info
		 * https://learn.microsoft.com/en-us/windows/win32/api/iptypes/ns-iptypes-ip_addr_string
		 * https://learn.microsoft.com/en-us/windows/win32/api/iptypes/ns-iptypes-ip_address_string
		 */
		for (pAdapter = pAdapterInfo; pAdapter; pAdapter = pAdapter->Next) {
			IP_ADDR_STRING	*pUnicast;
			char	ifname[LARGEBUF];
			size_t	ifnamelen = 0;

			iftype  = (uintmax_t)pAdapter->Type;
			ifflags = iftype; /* The nearest they have to flags */
			ifnamelen += snprintf(
				ifname,
				sizeof(ifname),
				"%s: %s",
				pAdapter->AdapterName,
				pAdapter->Description);

			for (pUnicast = &(pAdapter->IpAddressList); pUnicast; pUnicast = pUnicast->Next)
#endif
			{	/* Have some address to handle */
				memset(msg, 0, sizeof(msg));
				memset(addr, 0, sizeof(addr));
				memset(mask, 0, sizeof(mask));
				masklen_subnet = -1;

#if defined HAVE_GETIFADDRS || (defined WIN32 && defined HAVE_GETADAPTERSADDRESSES)
# ifdef HAVE_GETIFADDRS
				if (ifa->ifa_addr->sa_family == AF_INET6)
# elif defined WIN32 && defined HAVE_GETADAPTERSADDRESSES
				if (pUnicast->Address.lpSockaddr->sa_family == AF_INET6)
# elif defined WIN32 && defined HAVE_GETADAPTERSINFO
				if (0)	/* No IPv6 for this library call */
# endif
				{	/* IPv6 */
# ifdef HAVE_GETIFADDRS
					uint8_t	i, j;

					/* Ensure proper alignment */
					struct sockaddr_in6 sa, sm;
					memcpy (&sa, ifa->ifa_addr, sizeof(struct sockaddr_in6));
					memcpy (&sm, ifa->ifa_netmask, sizeof(struct sockaddr_in6));

					/* FIXME: Here and below, this code
					 * technically just counts set bits
					 * and we assume they are a single
					 * contiguous range in the address
					 * portion of the IP address for a
					 * netmask.
					 */
					masklen_subnet = 0;
					for (j = 0; j < 16; j++) {
						i = sm.sin6_addr.s6_addr[j];
						while (i) {
							masklen_subnet += i & 1;
							i >>= 1;
						}
					}
# elif defined WIN32 && defined HAVE_GETADAPTERSADDRESSES
					/* This structure member is only available on Windows Vista and later.
					 * If we need earlier versions built for, need to use common struct.
					 * https://learn.microsoft.com/en-us/windows/win32/api/iptypes/ns-iptypes-ip_adapter_unicast_address_lh
					 */
					masklen_subnet = pUnicast->OnLinkPrefixLength;
# elif defined WIN32 && defined HAVE_GETADAPTERSINFO
					masklen_subnet = 128;	/* no-op anyway */
# endif
					masklen_hosts = 128 - masklen_subnet;

# ifdef HAVE_GETIFADDRS
					getnameinfo((struct sockaddr *)&sa, sizeof(struct sockaddr_in6), addr, sizeof(addr), NULL, 0, NI_NUMERICHOST);
					getnameinfo((struct sockaddr *)&sm, sizeof(struct sockaddr_in6), mask, sizeof(mask), NULL, 0, NI_NUMERICHOST);
# elif defined WIN32 && defined HAVE_GETADAPTERSADDRESSES
					getnameinfo(pUnicast->Address.lpSockaddr, sizeof(struct sockaddr_in6), addr, sizeof(addr), NULL, 0, NI_NUMERICHOST);
					/* We have no real need for mask as an IP
					 * string except debug logs, so let it be */
					snprintf(mask, sizeof(mask), "/%i", masklen_subnet);
# elif defined WIN32 && defined HAVE_GETADAPTERSINFO
# endif

					snprintf(msg, sizeof(msg),
						"Interface: %s\tAddress: %s\tMask: %s (subnet: %i, hosts: %i)\tFlags: %08" PRIxMAX "\tType: %08" PRIxMAX,
						ifname, addr, mask,
						masklen_subnet, masklen_hosts,
						ifflags, iftype);
				}	/* IPv6 */
#endif	/* HAVE_GETIFADDRS || HAVE_GETADAPTERSADDRESSES */
				
#ifdef HAVE_GETIFADDRS
				else
				if (ifa->ifa_addr->sa_family == AF_INET)
#elif defined WIN32 && defined HAVE_GETADAPTERSADDRESSES
				else
				if (pUnicast->Address.lpSockaddr->sa_family == AF_INET)
#elif defined WIN32 && defined HAVE_GETADAPTERSINFO
				/* NOTE: No "else", no "if": IPv6 was skipped
				 * on this platform and IPv4 was the only option */
#endif
				{	/* IPv4 */
#ifdef HAVE_GETIFADDRS
					in_addr_t	i;

					/* Ensure proper alignment */
					struct sockaddr_in sa, sm;
					memcpy (&sa, ifa->ifa_addr, sizeof(struct sockaddr_in));
					memcpy (&sm, ifa->ifa_netmask, sizeof(struct sockaddr_in));

					snprintf(addr, sizeof(addr), "%s", inet_ntoa(sa.sin_addr));
					snprintf(mask, sizeof(mask), "%s", inet_ntoa(sm.sin_addr));

					i = sm.sin_addr.s_addr;
					masklen_subnet = 0;
					while (i) {
						masklen_subnet += i & 1;
						i >>= 1;
					}
#elif defined WIN32 && defined HAVE_GETADAPTERSADDRESSES
					masklen_subnet = pUnicast->OnLinkPrefixLength;

					getnameinfo(pUnicast->Address.lpSockaddr, sizeof(struct sockaddr_in), addr, sizeof(addr), NULL, 0, NI_NUMERICHOST);
					/* We have no real need for mask as an IP
					 * string except debug logs, so let it be */
					snprintf(mask, sizeof(mask), "/%i", masklen_subnet);
#elif defined WIN32 && defined HAVE_GETADAPTERSINFO
					struct sockaddr_in	sm;

					snprintf(addr, sizeof(addr) > 16 ? 16 : sizeof(addr), "%s", pUnicast->IpAddress.String);
					snprintf(mask, sizeof(mask) > 16 ? 16 : sizeof(mask), "%s", pUnicast->IpMask.String);

					masklen_subnet = 0;
					if (inet_pton(AF_INET, mask, &sm.sin_addr)) {
						uint32_t	i = sm.sin_addr.s_addr;
						while (i) {
							masklen_subnet += i & 1;
							i >>= 1;
						}
					}
#endif
					masklen_hosts = 32 - masklen_subnet;

					snprintf(msg, sizeof(msg),
						"Interface: %s\tAddress: %s\tMask: %s (subnet: %i, hosts: %i)\tFlags: %08" PRIxMAX "\tType: %08" PRIxMAX,
						ifname, addr, mask,
						masklen_subnet, masklen_hosts,
						ifflags, iftype);
				}	/* IPv4 */

#ifdef HAVE_GETIFADDRS
/*
				else {
					snprintf(msg, sizeof(msg), "Addr family: %" PRIuMAX, (intmax_t)ifa->ifa_addr->sa_family);
				}
*/
#endif

#ifdef HAVE_GETIFADDRS
				if (ifa->ifa_addr->sa_family == AF_INET6 || ifa->ifa_addr->sa_family == AF_INET) {
					if (iftype & IFF_LOOPBACK)
						snprintfcat(msg, sizeof(msg), " IFF_LOOPBACK");
					if (iftype & IFF_UP)
						snprintfcat(msg, sizeof(msg), " IFF_UP");
					if (iftype & IFF_RUNNING)
						snprintfcat(msg, sizeof(msg), " IFF_RUNNING");
					if (iftype & IFF_BROADCAST)
						snprintfcat(msg, sizeof(msg), " IFF_BROADCAST(is assigned)");
#elif defined WIN32 && defined HAVE_GETADAPTERSADDRESSES
				/* https://learn.microsoft.com/en-us/windows/win32/api/nldef/ne-nldef-nl_prefix_origin
				 * https://learn.microsoft.com/en-us/windows/win32/api/nldef/ne-nldef-nl_suffix_origin
				 */
				if (pUnicast->Address.lpSockaddr->sa_family == AF_INET
				||  pUnicast->Address.lpSockaddr->sa_family == AF_INET6
				) {
					if (iftype == IF_TYPE_OTHER)
						snprintfcat(msg, sizeof(msg), " IF_TYPE_OTHER");
					if (iftype == IF_TYPE_ETHERNET_CSMACD)
						snprintfcat(msg, sizeof(msg), " IF_TYPE_ETHERNET_CSMACD");
					if (iftype == IF_TYPE_ISO88025_TOKENRING)
						snprintfcat(msg, sizeof(msg), " IF_TYPE_ISO88025_TOKENRING");
					if (iftype == IF_TYPE_PPP)
						snprintfcat(msg, sizeof(msg), " IF_TYPE_PPP");
					if (iftype == IF_TYPE_SOFTWARE_LOOPBACK)
						snprintfcat(msg, sizeof(msg), " IF_TYPE_SOFTWARE_LOOPBACK");
					if (iftype == IF_TYPE_ATM)
						snprintfcat(msg, sizeof(msg), " IF_TYPE_ATM");
					if (iftype == IF_TYPE_IEEE80211)
						snprintfcat(msg, sizeof(msg), " IF_TYPE_IEEE80211");
					if (iftype == IF_TYPE_TUNNEL)
						snprintfcat(msg, sizeof(msg), " IF_TYPE_TUNNEL");
					if (iftype == IF_TYPE_IEEE1394)
						snprintfcat(msg, sizeof(msg), " IF_TYPE_IEEE1394");
#elif defined WIN32 && defined HAVE_GETADAPTERSINFO
				{	/* Just scoping for this platform */
					if (iftype == MIB_IF_TYPE_OTHER)
						snprintfcat(msg, sizeof(msg), " MIB_IF_TYPE_OTHER");
					if (iftype == MIB_IF_TYPE_ETHERNET)
						snprintfcat(msg, sizeof(msg), " MIB_IF_TYPE_ETHERNET");
					if (iftype == IF_TYPE_ISO88025_TOKENRING)
						snprintfcat(msg, sizeof(msg), " IF_TYPE_ISO88025_TOKENRING");
					if (iftype == MIB_IF_TYPE_PPP)
						snprintfcat(msg, sizeof(msg), " MIB_IF_TYPE_PPP");
					if (iftype == MIB_IF_TYPE_LOOPBACK)
						snprintfcat(msg, sizeof(msg), " MIB_IF_TYPE_LOOPBACK");
					if (iftype == MIB_IF_TYPE_SLIP)
						snprintfcat(msg, sizeof(msg), " MIB_IF_TYPE_SLIP");
					if (iftype == IF_TYPE_IEEE80211)
						snprintfcat(msg, sizeof(msg), " IF_TYPE_IEEE80211");
#endif

					upsdebugx(5, "Discovering getifaddrs(): %s", msg);

#ifdef HAVE_GETIFADDRS
					if (!(
						(auto_nets == 46
					     || (auto_nets == 4 && ifa->ifa_addr->sa_family == AF_INET)
					     || (auto_nets == 6 && ifa->ifa_addr->sa_family == AF_INET6) )
					)) {
						upsdebugx(6, "Subnet ignored: not of the requested address family");
						continue;
					}
#elif defined WIN32 && defined HAVE_GETADAPTERSADDRESSES
					if (!(
						(auto_nets == 46
					     || (auto_nets == 4 && pUnicast->Address.lpSockaddr->sa_family == AF_INET)
					     || (auto_nets == 6 && pUnicast->Address.lpSockaddr->sa_family == AF_INET6) )
					)) {
						upsdebugx(6, "Subnet ignored: not of the requested address family");
						continue;
					}
#elif defined WIN32 && defined HAVE_GETADAPTERSINFO
#endif

					if (
						!strcmp(addr, "0.0.0.0")
						|| !strcmp(addr, "*")
						|| !strcmp(addr, "::")
						|| !strcmp(addr, "0::")
					) {
						/* FIXME: IPv6 spellings? Search for hex/digits other than 0? */
						upsdebugx(6, "Subnet ignored: host address not assigned or mis-detected");
						continue;
					}

					if (masklen_hosts_limit < masklen_hosts) {
						/* NOTE: masklen_hosts==0 means
						 * an exact hit on one address,
						 * so an IPv4/32 or IPv6/128.
						 */
						upsdebugx(6, "Subnet ignored: address range too large: %ld bits allowed vs. %d bits per netmask",
							masklen_hosts_limit, masklen_hosts);
						continue;
					}

#ifdef HAVE_GETIFADDRS
					if (iftype & IFF_LOOPBACK)
#elif defined WIN32 && defined HAVE_GETADAPTERSADDRESSES
					if (iftype == IF_TYPE_SOFTWARE_LOOPBACK)
#elif defined WIN32 && defined HAVE_GETADAPTERSINFO
					if (iftype == MIB_IF_TYPE_LOOPBACK)
#endif
					{
						upsdebugx(6, "Subnet ignored: loopback");
						continue;
					}
					/* TODO? Filter other interface types? */

#ifdef HAVE_GETIFADDRS
					if (!(
						(ifflags & IFF_UP)
					   &&   (ifflags & IFF_RUNNING)
					   &&   (ifflags & IFF_BROADCAST)
					)) {
						upsdebugx(6, "Subnet ignored: not up and running, with a proper broadcast-able address");
						continue;
					}
#elif defined WIN32 && defined HAVE_GETADAPTERSADDRESSES
					if (pCurrAddresses->OperStatus != IfOperStatusUp) {
						upsdebugx(6, "Subnet ignored: not up and running");
						continue;
					}
#elif defined WIN32 && defined HAVE_GETADAPTERSINFO
#endif

					/* TODO: also rule out "link-local" address ranges
					 * so we do not issue billions of worthless scans.
					 * FIXME: IPv6 may also be a problem, see
					 * https://github.com/networkupstools/nut/issues/2512
					 */
					if (snprintf(cidr, sizeof(cidr), "%s/%i", addr, masklen_subnet) < 0) {
						fatalx(EXIT_FAILURE, "Could not construct a CIDR string from discovered address/mask");
					}

					upsdebugx(5, "Processing CIDR net/mask: %s", cidr);
					nutscan_cidr_to_ip(cidr, &start_ip, &end_ip);
					upsdebugx(5, "Extracted IP address range from CIDR net/mask: %s => %s", start_ip, end_ip);

					nutscan_add_ip_range(&ip_ranges_list, start_ip, end_ip);
					start_ip = NULL;
					end_ip = NULL;

					auto_subnets_found++;
				}	/* else AF_UNIX or a dozen other types we do not care about here */
			}
		}
	}

/*finish:*/
#ifdef HAVE_GETIFADDRS
	if (ifap) {
		freeifaddrs(ifap);
	}
#elif defined WIN32 && defined HAVE_GETADAPTERSADDRESSES
	if (pAddresses) {
		free(pAddresses);
	}
#elif defined WIN32 && defined HAVE_GETADAPTERSINFO
	if (pAdapterInfo) {
		free(pAdapterInfo);
	}
#endif

	if (!auto_subnets_found) {
		upsdebugx(0, "WARNING: A '-m auto*' request selected no subnets!\n"
			"Please check for reasons with higher debug verbosity (up to 6).");
	}
	upsdebugx(3, "Finished %s('%s'), selected %" PRIuSIZE " subnets automatically",
		__func__, arg_addr, auto_subnets_found);
#else	/* not (HAVE_GETIFADDRS || ( WIN32 && (HAVE_GETADAPTERSINFO || HAVE_GETADAPTERSADDRESSES))) */
	fatalx(EXIT_FAILURE,
		"Have no way to query local interface addresses on this "
		"platform, please run without the '-m auto*' options!");
#endif	/* HAVE_GETIFADDRS || ( WIN32 && (HAVE_GETADAPTERSINFO || HAVE_GETADAPTERSADDRESSES)) */
}

static void show_usage(const char *arg_progname)
{
/* NOTE: This code uses `nutscan_avail_*` global vars from nutscan-init.c */
	print_banner_once(arg_progname, 2);
	puts("NUT utility for detection of available power devices.\n");

	nut_report_config_flags();

	puts("OPTIONS:");
	printf("  -C, --complete_scan: Scan all available devices except serial ports (default).\n");
	if (nutscan_avail_usb) {
		printf("  -U, --usb_scan: Scan USB devices. Specify twice or more to report different\n"
			"                  detail levels of (change-prone) physical properties.\n"
			"                  This usage can be combined with '-C' or other scan types.\n");
	} else {
		printf("* Options for USB devices scan not enabled: library not detected.\n");
	}
	if (nutscan_avail_snmp) {
		printf("  -S, --snmp_scan: Scan SNMP devices using built-in mapping definitions.\n");
	} else {
		printf("* Options for SNMP devices scan not enabled: library not detected.\n");
	}
	if (nutscan_avail_xml_http) {
		printf("  -M, --xml_scan: Scan XML/HTTP devices.\n");
	} else {
		printf("* Options for XML/HTTP devices scan not enabled: library not detected.\n");
	}
	printf("  -O, --oldnut_scan: Scan NUT devices (old method via libupsclient).\n");
	if (nutscan_avail_avahi) {
		printf("  -A, --avahi_scan: Scan NUT devices (new avahi method).\n");
	} else {
		printf("* Options for NUT devices (new avahi method) scan not enabled: library not detected.\n");
	}
	printf("  -n, --nut_simulation_scan: Scan for NUT simulated devices (.dev files in $CONFPATH).\n");
	if (nutscan_avail_ipmi) {
		printf("  -I, --ipmi_scan: Scan IPMI devices.\n");
	} else {
		printf("* Options for IPMI devices scan not enabled: library not detected.\n");
	}

	printf("  -E, --eaton_serial <serial ports list>: Scan serial Eaton devices (XCP, SHUT and Q1).\n");

#if (defined HAVE_PTHREAD) && ( (defined HAVE_PTHREAD_TRYJOIN) || (defined HAVE_SEMAPHORE_UNNAMED) || (defined HAVE_SEMAPHORE_NAMED) )
	printf("  -T, --thread <max number of threads>: Limit the amount of scanning threads running simultaneously (default: %" PRIuSIZE ").\n", max_threads);
#else
	printf("  -T, --thread <max number of threads>: Limit the amount of scanning threads running simultaneously (not implemented in this build: no pthread support)\n");
#endif

	printf("\nNote: many scanning options depend on further loadable libraries.\n");
	/* Note: if debug is enabled, this is prefixed with timestamps */
	upsdebugx_report_search_paths(0, 0);

	printf("\nNetwork specific options:\n");
	printf("  -t, --timeout <timeout in seconds>: network operation timeout (default %d).\n", DEFAULT_NETWORK_TIMEOUT);
	printf("  -s, --start_ip <IP address>: First IP address to scan.\n");
	printf("  -e, --end_ip <IP address>: Last IP address to scan.\n");
	printf("  -m, --mask_cidr <IP address/mask>: Give a range of IP using CIDR notation.\n");
	printf("  -m, --mask_cidr auto: Detect local IP address(es) and scan corresponding subnet(s).\n");
#if !(defined HAVE_GETIFADDRS || (defined WIN32 && (defined HAVE_GETADAPTERSINFO || defined HAVE_GETADAPTERSADDRESSES)))
	printf("                        (Currently not implemented for this platform)\n");
#endif
	printf("  -m, --mask_cidr auto4/auto6: Likewise, limiting to IPv4 or IPv6 interfaces.\n");
	printf("                        Only the first auto* request would be honoured.\n");
	printf("NOTE: IP address range specifications can be repeated, to scan several.\n");
	printf("Specifying a single first or last address before starting another range\n");
	printf("leads to scanning just that one address as the range.\n");

	if (nutscan_avail_snmp) {
		printf("\nSNMP v1 specific options:\n");
		printf("  -c, --community <community name>: Set SNMP v1 community name (default = public)\n");

		printf("\nSNMP v3 specific options:\n");
		printf("  -l, --secLevel <security level>: Set the securityLevel used for SNMPv3 messages (allowed values: noAuthNoPriv, authNoPriv, authPriv)\n");
		printf("  -u, --secName <security name>: Set the securityName used for authenticated SNMPv3 messages (mandatory if you set secLevel. No default)\n");

		/* Construct help for AUTHPROTO */
		{ int comma = 0;
		NUT_UNUSED_VARIABLE(comma); /* potentially, if no protocols are available */
		printf("  -w, --authProtocol <authentication protocol>: Set the authentication protocol (");
#if (defined WITH_SNMP) && (defined NUT_HAVE_LIBNETSNMP_usmHMACMD5AuthProtocol)
/* Note: NUT_HAVE_LIBNETSNMP_* macros are not AC_DEFINE'd when libsnmp was
 * completely not detected at configure time, so "#if" is not a pedantically
 * correct test (unknown macro may default to "0" but is not guaranteed to).
 */
# if NUT_HAVE_LIBNETSNMP_usmHMACMD5AuthProtocol
		printf("%s%s",
			(comma++ ? ", " : ""),
			"MD5"
			);
# endif
# if NUT_HAVE_LIBNETSNMP_usmHMACSHA1AuthProtocol
		printf("%s%s",
			(comma++ ? ", " : ""),
			"SHA"
			);
# endif
# if NUT_HAVE_LIBNETSNMP_usmHMAC192SHA256AuthProtocol
		printf("%s%s",
			(comma++ ? ", " : ""),
			"SHA256"
			);
# endif
# if NUT_HAVE_LIBNETSNMP_usmHMAC256SHA384AuthProtocol
		printf("%s%s",
			(comma++ ? ", " : ""),
			"SHA384"
			);
# endif
# if NUT_HAVE_LIBNETSNMP_usmHMAC384SHA512AuthProtocol
		printf("%s%s",
			(comma++ ? ", " : ""),
			"SHA512"
			);
# endif
		printf("%s%s",
			(comma ? "" : "none supported"),
			") used for authenticated SNMPv3 messages (default=MD5 if available)\n"
			);
		} /* Construct help for AUTHPROTO */

		printf("  -W, --authPassword <authentication pass phrase>: Set the authentication pass phrase used for authenticated SNMPv3 messages (mandatory if you set secLevel to authNoPriv or authPriv)\n");

		/* Construct help for PRIVPROTO */
		{ int comma = 0;
		NUT_UNUSED_VARIABLE(comma); /* potentially, if no protocols are available */
		printf("  -x, --privProtocol <privacy protocol>: Set the privacy protocol (");
# if NUT_HAVE_LIBNETSNMP_usmDESPrivProtocol
		printf("%s%s",
			(comma++ ? ", " : ""),
			"DES"
			);
# endif
# if NUT_HAVE_LIBNETSNMP_usmAESPrivProtocol || NUT_HAVE_LIBNETSNMP_usmAES128PrivProtocol
		printf("%s%s",
			(comma++ ? ", " : ""),
			"AES"
			);
# endif
# if NUT_HAVE_LIBNETSNMP_DRAFT_BLUMENTHAL_AES_04
#  if NUT_HAVE_LIBNETSNMP_usmAES192PrivProtocol
		printf("%s%s",
			(comma++ ? ", " : ""),
			"AES192"
			);
#  endif
#  if NUT_HAVE_LIBNETSNMP_usmAES256PrivProtocol
		printf("%s%s",
			(comma++ ? ", " : ""),
			"AES256"
			);
#  endif
# endif /* NUT_HAVE_LIBNETSNMP_DRAFT_BLUMENTHAL_AES_04 */
#endif /* built WITH_SNMP */
		printf("%s%s",
			(comma ? "" : "none supported"),
			") used for encrypted SNMPv3 messages (default=DES if available)\n"
			);
		} /* Construct help for PRIVPROTO */

		printf("  -X, --privPassword <privacy pass phrase>: Set the privacy pass phrase used for encrypted SNMPv3 messages (mandatory if you set secLevel to authPriv)\n");
	}

	if (nutscan_avail_ipmi) {
		printf("\nIPMI over LAN specific options:\n");
		printf("  -b, --username <username>: Set the username used for authenticating IPMI over LAN connections (mandatory for IPMI over LAN. No default)\n");
		/* Specify  the  username  to  use  when authenticating with the remote host.  If not specified, a null (i.e. anonymous) username is assumed. The user must have
		 * at least ADMIN privileges in order for this tool to operate fully. */
		printf("  -B, --password <password>: Specify the password to use when authenticationg with the remote host (mandatory for IPMI over LAN. No default)\n");
		/* Specify the password to use when authenticationg with the remote host.  If not specified, a null password is assumed. Maximum password length is 16 for IPMI
		 * 1.5 and 20 for IPMI 2.0. */
		printf("  -d, --authType <authentication type>: Specify the IPMI 1.5 authentication type to use (NONE, STRAIGHT_PASSWORD_KEY, MD2, and MD5) with the remote host (default=MD5)\n");
		printf("  -L, --cipher_suite_id <cipher suite id>: Specify the IPMI 2.0 cipher suite ID to use, for authentication, integrity, and confidentiality (default=3)\n");
	}

	printf("\nNUT specific options:\n");
	printf("  -p, --port <port number>: Port number of remote NUT upsd\n");
	printf("\ndisplay specific options:\n");
	printf("  -Q, --disp_nut_conf_with_sanity_check: Display result in the ups.conf format with sanity-check warnings as comments (default)\n");
	printf("  -N, --disp_nut_conf: Display result in the ups.conf format\n");
	printf("  -P, --disp_parsable: Display result in a parsable format\n");
	printf("\nMiscellaneous options:\n");
	printf("  -h, --help: display this help text\n");
	printf("  -V, --version: Display NUT version\n");
	printf("  -a, --available: Display available bus that can be scanned\n");
	printf("  -q, --quiet: Display only scan result. No information on currently scanned bus is displayed.\n");
	printf("  -D, --nut_debug_level: Raise the debugging level.  Use this multiple times to see more details.\n");

	printf("\n%s", suggest_doc_links(arg_progname, "ups.conf"));
}

int main(int argc, char *argv[])
{
	const char	*progname = xbasename(argv[0]);
	nutscan_snmp_t snmp_sec;
	nutscan_ipmi_t ipmi_sec;
	nutscan_xml_t  xml_sec;
	int opt_ret;
	char *start_ip = NULL, *end_ip = NULL;
	int auto_nets = 0;
	int allow_all = 0;
	int allow_usb = 0;
	int allow_snmp = 0;
	int allow_xml = 0;
	int allow_oldnut = 0;
	int allow_nut_simulation = 0;
	int allow_avahi = 0;
	int allow_ipmi = 0;
	int allow_eaton_serial = 0; /* MUST be requested explicitly! */
	int quiet = 0; /* The debugging level for certain upsdebugx() progress messages; 0 = print always, quiet==1 is to require at least one -D */
	void (*display_func)(nutscan_device_t * device);
	int ret_code = EXIT_SUCCESS;
#ifdef HAVE_PTHREAD
# if (defined HAVE_SEMAPHORE_UNNAMED) || (defined HAVE_SEMAPHORE_NAMED)
	sem_t	*current_sem;
# endif
#endif
#if (defined HAVE_PTHREAD) && ( (defined HAVE_PTHREAD_TRYJOIN) || (defined HAVE_SEMAPHORE_UNNAMED) || (defined HAVE_SEMAPHORE_NAMED) ) && (defined HAVE_SYS_RESOURCE_H)
	struct rlimit nofile_limit;

	/* Limit the max scanning thread count by the amount of allowed open
	 * file descriptors (which caller can change with `ulimit -n NUM`),
	 * following practical investigation summarized at
	 *   https://github.com/networkupstools/nut/pull/1158
	 * Resource-Limit code inspired by example from:
	 *   https://stackoverflow.com/questions/4076848/how-to-do-the-equivalent-of-ulimit-n-400-from-within-c/4077000#4077000
	 */

	/* Get max number of files. */
	if (getrlimit(RLIMIT_NOFILE, &nofile_limit) != 0) {
		/* Report error, keep hardcoded default */
		upsdebug_with_errno(0, "getrlimit() failed, keeping default job limits");
		nofile_limit.rlim_cur = 0;
		nofile_limit.rlim_max = 0;
	} else {
		if (nofile_limit.rlim_cur > 0
		&&  nofile_limit.rlim_cur > RESERVE_FD_COUNT
		&&  (uintmax_t)max_threads > (uintmax_t)(nofile_limit.rlim_cur - RESERVE_FD_COUNT)
		&&  (uintmax_t)(nofile_limit.rlim_cur) < (uintmax_t)SIZE_MAX
		) {
			max_threads = (size_t)nofile_limit.rlim_cur;
			if (max_threads > (RESERVE_FD_COUNT + 1)) {
				max_threads -= RESERVE_FD_COUNT;
			}
		}
	}
#endif	/* HAVE_PTHREAD && ( HAVE_PTHREAD_TRYJOIN || HAVE_SEMAPHORE_UNNAMED || HAVE_SEMAPHORE_NAMED ) && HAVE_SYS_RESOURCE_H */

	memset(&snmp_sec, 0, sizeof(snmp_sec));
	memset(&ipmi_sec, 0, sizeof(ipmi_sec));
	memset(&xml_sec, 0, sizeof(xml_sec));

	/* Set the default values for IPMI */
	ipmi_sec.authentication_type = IPMI_AUTHENTICATION_TYPE_MD5;
	ipmi_sec.ipmi_version = IPMI_1_5; /* default to IPMI 1.5, if not otherwise specified */
	ipmi_sec.cipher_suite_id = 3; /* default to HMAC-SHA1; HMAC-SHA1-96; AES-CBC-128 */
	ipmi_sec.privilege_level = IPMI_PRIVILEGE_LEVEL_ADMIN; /* should be sufficient */
	ipmi_sec.peername = NULL;

	/* Set the default values for XML HTTP (run_xml()) */
	xml_sec.port_http = 80;
	xml_sec.port_udp = 4679;
	xml_sec.usec_timeout = 0; /* Override with the "timeout" common setting later */
	xml_sec.peername = NULL;

	/* Parse command line options -- First loop: only get debug level */
	/* Suppress error messages, for now -- leave them to the second loop. */
	opterr = 0;
	while ((opt_ret = getopt_long(argc, argv, optstring, longopts, NULL)) != -1) {
		if (opt_ret == 'D')
			nut_debug_level++;
	}

	nutscan_init_ip_ranges(&ip_ranges_list);
	nutscan_init();

	/* Default, see -Q/-N/-P below */
	display_func = nutscan_display_ups_conf_with_sanity_check;

	/* Parse command line options -- Second loop: everything else */
	/* Restore error messages... */
	opterr = 1;
	/* ...and index of the item to be processed by getopt(). */
	optind = 1;
	/* Note: the getopts print an error message about unknown arguments
	 * or arguments which need a second token and that is missing now */
	while ((opt_ret = getopt_long(argc, argv, optstring, longopts, NULL)) != -1) {

		switch(opt_ret) {
			case 't':
				{ // scoping
					long	l;
					char	*s = NULL;
					int	errno_saved;

					errno = 0;
					l = strtol(optarg, &s, 10);
					errno_saved = errno;
					upsdebugx(6, "errno=%d s='%s'(%p) input='%s'(%p) output=%ld",
						errno_saved, NUT_STRARG(s), (void *)s,
						optarg, (void *)(optarg), l);

					if (errno_saved || (s && *s != '\0') || l <= 0) {
						/* TODO: Any max limit? At least,
						 * max(useconds_t)/1000000 ? */
						upsdebugx(0,
							"Illegal timeout value, using default %ds",
							DEFAULT_NETWORK_TIMEOUT);
						timeout = DEFAULT_NETWORK_TIMEOUT * 1000 * 1000;
					} else {
						timeout = (useconds_t)l * 1000 * 1000; /*in usec*/
					}
				}
				break;
			case 's':
				if (start_ip) {
					/* Save whatever we have, either
					 * this one address or an earlier
					 * known range with its end */
					nutscan_add_ip_range(&ip_ranges_list, start_ip, end_ip);
					start_ip = NULL;
					end_ip = NULL;
				}

				if (optarg[0] == '[' && optarg[strlen(optarg) - 1] == ']') {
					start_ip = strdup(optarg + 1);
					start_ip[strlen(start_ip) - 1] = '\0';
				} else {
					start_ip = strdup(optarg);
				}

				if (end_ip != NULL) {
					/* Already we know two addresses, save them */
					nutscan_add_ip_range(&ip_ranges_list, start_ip, end_ip);
					start_ip = NULL;
					end_ip = NULL;
				}
				break;
			case 'e':
				if (end_ip) {
					/* Save whatever we have, either
					 * this one address or an earlier
					 * known range with its start */
					nutscan_add_ip_range(&ip_ranges_list, start_ip, end_ip);
					start_ip = NULL;
					end_ip = NULL;
				}

				if (optarg[0] == '[' && optarg[strlen(optarg) - 1] == ']') {
					end_ip = strdup(optarg + 1);
					end_ip[strlen(end_ip) - 1] = '\0';
				} else {
					end_ip = strdup(optarg);
				}

				if (start_ip != NULL) {
					/* Already we know two addresses, save them */
					nutscan_add_ip_range(&ip_ranges_list, start_ip, end_ip);
					start_ip = NULL;
					end_ip = NULL;
				}
				break;
			case 'E':
				serial_ports = strdup(optarg);
				allow_eaton_serial = 1;
				break;
			case 'm':
				if (start_ip || end_ip) {
					/* Save whatever we have, either
					 * this one address or an earlier
					 * known range with its start or end */
					nutscan_add_ip_range(&ip_ranges_list, start_ip, end_ip);
					start_ip = NULL;
					end_ip = NULL;
				}

				/* Large code block offloaded into a method */
				handle_arg_cidr(optarg, &auto_nets);

				break;
			case 'D':
				/* nothing to do, here */
				break;
			case 'c':
				if (!nutscan_avail_snmp) {
					goto display_help;
				}
				snmp_sec.community = strdup(optarg);
				break;
			case 'l':
				if (!nutscan_avail_snmp) {
					goto display_help;
				}
				snmp_sec.secLevel = strdup(optarg);
				break;
			case 'u':
				if (!nutscan_avail_snmp) {
					goto display_help;
				}
				snmp_sec.secName = strdup(optarg);
				break;
			case 'W':
				if (!nutscan_avail_snmp) {
					goto display_help;
				}
				snmp_sec.authPassword = strdup(optarg);
				break;
			case 'X':
				if (!nutscan_avail_snmp) {
					goto display_help;
				}
				snmp_sec.privPassword = strdup(optarg);
				break;
			case 'w':
				if (!nutscan_avail_snmp) {
					goto display_help;
				}
				snmp_sec.authProtocol = strdup(optarg);
				break;
			case 'x':
				if (!nutscan_avail_snmp) {
					goto display_help;
				}
				snmp_sec.privProtocol = strdup(optarg);
				break;
			case 'S':
				if (!nutscan_avail_snmp) {
					goto display_help;
				}
				allow_snmp = 1;
				break;
			case 'b':
				if (!nutscan_avail_ipmi) {
					goto display_help;
				}
				ipmi_sec.username = strdup(optarg);
				break;
			case 'B':
				if (!nutscan_avail_ipmi) {
					goto display_help;
				}
				ipmi_sec.password = strdup(optarg);
				break;
			case 'd':
				if (!nutscan_avail_ipmi) {
					goto display_help;
				}
				if (!strcmp(optarg, "NONE")) {
					ipmi_sec.authentication_type = IPMI_AUTHENTICATION_TYPE_NONE;
				}
				else if (!strcmp(optarg, "STRAIGHT_PASSWORD_KEY")) {
					ipmi_sec.authentication_type = IPMI_AUTHENTICATION_TYPE_STRAIGHT_PASSWORD_KEY;
				}
				else if (!strcmp(optarg, "MD2")) {
					ipmi_sec.authentication_type = IPMI_AUTHENTICATION_TYPE_MD2;
				}
				else if (!strcmp(optarg, "MD5")) {
					ipmi_sec.authentication_type = IPMI_AUTHENTICATION_TYPE_MD5;
				}
				else {
					upsdebugx(0,
						"Unknown authentication type (%s). Defaulting to MD5",
						optarg);
				}
				break;
			case 'L':
				if (!nutscan_avail_ipmi) {
					goto display_help;
				}
				ipmi_sec.cipher_suite_id = atoi(optarg);
				/* Force IPMI 2.0! */
				ipmi_sec.ipmi_version = IPMI_2_0;
				break;
			case 'p':
				port = strdup(optarg);
				break;
			case 'T': {
#if (defined HAVE_PTHREAD) && ( (defined HAVE_PTHREAD_TRYJOIN) || (defined HAVE_SEMAPHORE_UNNAMED) || (defined HAVE_SEMAPHORE_NAMED) )
				char* endptr;
				long val = strtol(optarg, &endptr, 10);
				/* With endptr we check that no chars were left in optarg
				 * (that is, pointed-to char -- if reported -- is '\0')
				 */
				if ((!endptr || !*endptr)
				&& val > 0
				&& (uintmax_t)val < (uintmax_t)SIZE_MAX
				) {
# ifdef HAVE_SYS_RESOURCE_H
					if (nofile_limit.rlim_cur > 0
					&&  nofile_limit.rlim_cur > RESERVE_FD_COUNT
					&& (uintmax_t)nofile_limit.rlim_cur < (uintmax_t)SIZE_MAX
					&& (uintmax_t)val > (uintmax_t)(nofile_limit.rlim_cur - RESERVE_FD_COUNT)
					) {
						upsdebugx(1, "Detected soft limit for "
							"file descriptor count is %" PRIuMAX,
							(uintmax_t)nofile_limit.rlim_cur);
						upsdebugx(1, "Detected hard limit for "
							"file descriptor count is %" PRIuMAX,
							(uintmax_t)nofile_limit.rlim_max);

						max_threads = (size_t)nofile_limit.rlim_cur;
						if (max_threads > (RESERVE_FD_COUNT + 1)) {
							max_threads -= RESERVE_FD_COUNT;
						}

						upsdebugx(0,
							"WARNING: Requested max scanning "
							"thread count %s (%ld) exceeds the "
							"current file descriptor count limit "
							"(minus reservation), constraining "
							"to %" PRIuSIZE,
							optarg, val, max_threads);
					} else
# endif /* HAVE_SYS_RESOURCE_H */
						max_threads = (size_t)val;
				} else {
					upsdebugx(0,
						"WARNING: Requested max scanning "
						"thread count %s (%ld) is out of range, "
						"using default %" PRIuSIZE,
						optarg, val, max_threads);
				}
#else
				upsdebugx(0,
					"WARNING: Max scanning thread count option "
					"is not supported in this build, ignored");
#endif /* HAVE_PTHREAD && ways to limit the thread count */
				}
				break;
			case 'C':
				allow_all = 1;
				break;
			case 'U':
				if (!nutscan_avail_usb) {
					goto display_help;
				}
				allow_usb = 1;
				/* NOTE: Starts as -1, so the first -U sets it to 0
				 * (minimal detail); further -U can bump it */
				if (cli_link_detail_level < 3)
					cli_link_detail_level++;
				break;
			case 'M':
				if (!nutscan_avail_xml_http) {
					goto display_help;
				}
				allow_xml = 1;
				break;
			case 'O':
				allow_oldnut = 1;
				break;
			case 'A':
				if (!nutscan_avail_avahi) {
					goto display_help;
				}
				allow_avahi = 1;
				break;
			case 'n':
				allow_nut_simulation = 1;
				break;
			case 'I':
				if (!nutscan_avail_ipmi) {
					goto display_help;
				}
				allow_ipmi = 1;
				break;
			case 'Q':
				display_func = nutscan_display_ups_conf_with_sanity_check;
				break;
			case 'N':
				display_func = nutscan_display_ups_conf;
				break;
			case 'P':
				display_func = nutscan_display_parsable;
				break;
			case 'q':
				quiet = 1;
				break;
			case 'V':
				/* just show the version and optional
				 * CONFIG_FLAGS banner if available */
				print_banner_once(progname, 1);
				nut_report_config_flags();
				exit(EXIT_SUCCESS);
			case 'a':
				printf("OLDNUT\n");
				if (nutscan_avail_usb) {
					printf("USB\n");
				}
				if (nutscan_avail_snmp) {
					printf("SNMP\n");
				}
				if (nutscan_avail_xml_http) {
					printf("XML\n");
				}
				if (nutscan_avail_avahi) {
					printf("AVAHI\n");
				}
				if (nutscan_avail_ipmi) {
					printf("IPMI\n");
				}
				printf("EATON_SERIAL\n");
				exit(EXIT_SUCCESS);
			case '?':
				ret_code = ERR_BAD_OPTION;
				goto display_help;
				/* Fall through to usage and error exit */
			case 'h':
			default:
display_help:
				show_usage(progname);
				if ((opt_ret != 'h') || (ret_code != EXIT_SUCCESS))
					fprintf(stderr, "\n\n"
						"WARNING: Some error has occurred while processing 'nut-scanner' command-line\n"
						"arguments, see more details above the usage help text.\n\n");
				return ret_code;
		}
	}

#ifdef HAVE_PTHREAD
	{	/* scoping for the string */
#  if defined HAVE_SEMAPHORE_UNNAMED
		char * semsuf = "_UNNAMED";
#  elif defined HAVE_SEMAPHORE_NAMED
		char * semsuf = "_NAMED";
#  else
		char * semsuf = "*";
#  endif
		NUT_UNUSED_VARIABLE(semsuf);	/* just in case */

# if (defined HAVE_PTHREAD_TRYJOIN) && ((defined HAVE_SEMAPHORE_UNNAMED) || (defined HAVE_SEMAPHORE_NAMED))
		upsdebugx(1, "Parallel scan support: HAVE_PTHREAD && HAVE_PTHREAD_TRYJOIN && HAVE_SEMAPHORE%s", semsuf);
# elif (defined HAVE_PTHREAD_TRYJOIN)
		upsdebugx(1, "Parallel scan support: HAVE_PTHREAD && HAVE_PTHREAD_TRYJOIN && !HAVE_SEMAPHORE*");
# elif (defined HAVE_SEMAPHORE_UNNAMED) || (defined HAVE_SEMAPHORE_NAMED)
		upsdebugx(1, "Parallel scan support: HAVE_PTHREAD && !HAVE_PTHREAD_TRYJOIN && HAVE_SEMAPHORE%s", semsuf);
# else
		upsdebugx(1, "Parallel scan support: HAVE_PTHREAD && !HAVE_PTHREAD_TRYJOIN && !HAVE_SEMAPHORE*");
# endif
	}

# if (defined HAVE_SEMAPHORE_UNNAMED) || (defined HAVE_SEMAPHORE_NAMED)
	/* FIXME: Currently sem_init already done on nutscan-init for lib need.
	   We need to destroy it before re-init. We currently can't change "sem value"
	   on lib (need to be thread safe). */
	current_sem = nutscan_semaphore();
#  ifdef HAVE_SEMAPHORE_UNNAMED
	sem_destroy(current_sem);
#  elif defined HAVE_SEMAPHORE_NAMED
	if (current_sem) {
		sem_unlink(SEMNAME_TOPLEVEL);
		sem_close(current_sem);
	}
#  endif
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
		fprintf(stderr, "\n\n"
			"WARNING: Limiting max_threads to range acceptable for "
			REPORT_SEM_INIT_METHOD "()\n\n");
		max_threads = UINT_MAX - 1;
	}

	upsdebugx(1, "Parallel scan support: max_threads=%" PRIuSIZE, max_threads);
#  ifdef HAVE_SEMAPHORE_UNNAMED
	if (sem_init(current_sem, 0, (unsigned int)max_threads)) {
		/* Show this one to end-users so they know */
		upsdebug_with_errno(0, "Parallel scan support: " REPORT_SEM_INIT_METHOD "() failed");
	}
#  elif defined HAVE_SEMAPHORE_NAMED
	/* FIXME: Do we need O_EXCL here? */
	if (SEM_FAILED == (current_sem = sem_open(SEMNAME_TOPLEVEL, O_CREAT, 0644, (unsigned int)max_threads))) {
		/* Show this one to end-users so they know */
		upsdebug_with_errno(0, "Parallel scan support: " REPORT_SEM_INIT_METHOD "() failed");
		current_sem = NULL;
	}
	nutscan_semaphore_set(current_sem);
#  endif

# endif
#else
	upsdebugx(1, "Parallel scan support: !HAVE_PTHREAD");
#endif /* HAVE_PTHREAD */

	if (start_ip != NULL || end_ip != NULL) {
		/* Something did not cancel out above */
		nutscan_add_ip_range(&ip_ranges_list, start_ip, end_ip);
		start_ip = NULL;
		end_ip = NULL;
	}

	if (!allow_usb && !allow_snmp && !allow_xml && !allow_oldnut && !allow_nut_simulation &&
		!allow_avahi && !allow_ipmi && !allow_eaton_serial
	) {
		allow_all = 1;
	}

	if (allow_all) {
		allow_usb = 1;
		/* NOTE: Starts as -1, so when we scan everything - set
		 * it to 0 (minimal detail); further -U can bump it */
		if (cli_link_detail_level < 0)
			cli_link_detail_level++;

		allow_snmp = 1;
		allow_xml = 1;
		allow_oldnut = 1;
		allow_nut_simulation = 1;
		allow_avahi = 1;
		allow_ipmi = 1;
		/* BEWARE: allow_all does not include allow_eaton_serial! */
	}

/* TODO/discuss : Should the #else...#endif code below for lack of pthreads
 * during build also serve as a fallback for pthread failure at runtime?
 */
	if (allow_usb && nutscan_avail_usb) {
		upsdebugx(quiet, "Scanning USB bus.");
#ifdef HAVE_PTHREAD
		if (pthread_create(&thread[TYPE_USB], NULL, run_usb, &cli_link_detail_level)) {
			upsdebugx(1, "pthread_create returned an error; disabling this scan mode");
			nutscan_avail_usb = 0;
		}
#else
		upsdebugx(1, "USB SCAN: no pthread support, starting nutscan_scan_usb...");
		/* Not calling run_usb() here, as it re-processes the arg */
		dev[TYPE_USB] = nutscan_scan_usb(&cli_link_detail_level);
#endif /* HAVE_PTHREAD */
	} else {
		upsdebugx(1, "USB SCAN: not requested or supported, SKIPPED");
	}

	if (allow_snmp && nutscan_avail_snmp) {
		if (!ip_ranges_list.ip_ranges_count) {
			upsdebugx(quiet, "No IP range(s) requested, skipping SNMP");
			nutscan_avail_snmp = 0;
		}
		else {
			upsdebugx(quiet, "Scanning SNMP bus.");
#ifdef HAVE_PTHREAD
			upsdebugx(1, "SNMP SCAN: starting pthread_create with run_snmp...");
			if (pthread_create(&thread[TYPE_SNMP], NULL, run_snmp, &snmp_sec)) {
				upsdebugx(1, "pthread_create returned an error; disabling this scan mode");
				nutscan_avail_snmp = 0;
			}
#else
			upsdebugx(1, "SNMP SCAN: no pthread support, starting nutscan_scan_snmp...");
			/* dev[TYPE_SNMP] = nutscan_scan_snmp(start_ip, end_ip, timeout, &snmp_sec); */
			run_snmp(&snmp_sec);
#endif /* HAVE_PTHREAD */
		}
	} else {
		upsdebugx(1, "SNMP SCAN: not requested or supported, SKIPPED");
	}

	if (allow_xml && nutscan_avail_xml_http) {
		/* NOTE: No check for ip_ranges_count,
		 * NetXML default scan is broadcast
		 * so it just runs (if requested and
		 * supported).
		 */
		upsdebugx(quiet, "Scanning XML/HTTP bus.");
		xml_sec.usec_timeout = timeout;
#ifdef HAVE_PTHREAD
		upsdebugx(1, "XML/HTTP SCAN: starting pthread_create with run_xml...");
		if (pthread_create(&thread[TYPE_XML], NULL, run_xml, &xml_sec)) {
			upsdebugx(1, "pthread_create returned an error; disabling this scan mode");
			nutscan_avail_xml_http = 0;
		}
#else
		upsdebugx(1, "XML/HTTP SCAN: no pthread support, starting nutscan_scan_xml_http_range()...");
		/* dev[TYPE_XML] = nutscan_scan_xml_http_range(start_ip, end_ip, timeout, &xml_sec); */
		run_xml(&xml_sec);
		}
#endif /* HAVE_PTHREAD */
	} else {
		upsdebugx(1, "XML/HTTP SCAN: not requested or supported, SKIPPED");
	}

	if (allow_oldnut && nutscan_avail_nut) {
		if (!ip_ranges_list.ip_ranges_count) {
			upsdebugx(quiet, "No IP range(s) requested, skipping NUT bus (old libupsclient connect method)");
			nutscan_avail_nut = 0;
		}
		else {
			upsdebugx(quiet, "Scanning NUT bus (old libupsclient connect method).");
#ifdef HAVE_PTHREAD
			upsdebugx(1, "NUT bus (old) SCAN: starting pthread_create with run_nut_old...");
			if (pthread_create(&thread[TYPE_NUT], NULL, run_nut_old, NULL)) {
				upsdebugx(1, "pthread_create returned an error; disabling this scan mode");
				nutscan_avail_nut = 0;
			}
#else
			upsdebugx(1, "NUT bus (old) SCAN: no pthread support, starting nutscan_scan_nut...");
			/*dev[TYPE_NUT] = nutscan_scan_nut(start_ip, end_ip, port, timeout);*/
			run_nut_old(NULL);
#endif /* HAVE_PTHREAD */
		}
	} else {
		upsdebugx(1, "NUT bus (old) SCAN: not requested or supported, SKIPPED");
	}

	if (allow_nut_simulation && nutscan_avail_nut_simulation) {
		upsdebugx(quiet, "Scanning NUT simulation devices.");
#ifdef HAVE_PTHREAD
		upsdebugx(1, "NUT simulation devices SCAN: starting pthread_create with run_nut_simulation...");
		if (pthread_create(&thread[TYPE_NUT_SIMULATION], NULL, run_nut_simulation, NULL)) {
			upsdebugx(1, "pthread_create returned an error; disabling this scan mode");
			nutscan_avail_nut_simulation = 0;
		}
#else
		upsdebugx(1, "NUT simulation devices SCAN: no pthread support, starting nutscan_scan_nut_simulation...");
		/* dev[TYPE_NUT_SIMULATION] = nutscan_scan_nut_simulation(); */
		run_nut_simulation(NULL);
#endif /* HAVE_PTHREAD */
	} else {
		upsdebugx(1, "NUT simulation devices SCAN: not requested or supported, SKIPPED");
	}

	if (allow_avahi && nutscan_avail_avahi) {
		upsdebugx(quiet, "Scanning NUT bus (avahi method).");
#ifdef HAVE_PTHREAD
		upsdebugx(1, "NUT bus (avahi) SCAN: starting pthread_create with run_avahi...");
		if (pthread_create(&thread[TYPE_AVAHI], NULL, run_avahi, NULL)) {
			upsdebugx(1, "pthread_create returned an error; disabling this scan mode");
			nutscan_avail_avahi = 0;
		}
#else
		upsdebugx(1, "NUT bus (avahi) SCAN: no pthread support, starting nutscan_scan_avahi...");
		/* dev[TYPE_AVAHI] = nutscan_scan_avahi(timeout); */
		run_avahi(NULL);
#endif /* HAVE_PTHREAD */
	} else {
		upsdebugx(1, "NUT bus (avahi) SCAN: not requested or supported, SKIPPED");
	}

	if (allow_ipmi && nutscan_avail_ipmi) {
		/* NOTE: No check for ip_ranges_count,
		 * IPMI default scan is local device
		 * so it just runs (if requested and
		 * supported).
		 */
		upsdebugx(quiet, "Scanning IPMI bus.");
#ifdef HAVE_PTHREAD
		upsdebugx(1, "IPMI SCAN: starting pthread_create with run_ipmi...");
		if (pthread_create(&thread[TYPE_IPMI], NULL, run_ipmi, &ipmi_sec)) {
			upsdebugx(1, "pthread_create returned an error; disabling this scan mode");
			nutscan_avail_ipmi = 0;
		}
#else
		upsdebugx(1, "IPMI SCAN: no pthread support, starting nutscan_scan_ipmi...");
		/* dev[TYPE_IPMI] = nutscan_scan_ipmi(start_ip, end_ip, &ipmi_sec); */
		run_ipmi(&ipmi_sec);
#endif /* HAVE_PTHREAD */
	} else {
		upsdebugx(1, "IPMI SCAN: not requested or supported, SKIPPED");
	}

	/* Eaton serial scan */
	if (allow_eaton_serial) {
		upsdebugx(quiet, "Scanning serial bus for Eaton devices.");
#ifdef HAVE_PTHREAD
		upsdebugx(1, "SERIAL SCAN: starting pthread_create with run_eaton_serial (return not checked!)...");
		pthread_create(&thread[TYPE_EATON_SERIAL], NULL, run_eaton_serial, serial_ports);
		/* FIXME: check return code */
		/* upsdebugx(1, "pthread_create returned an error; disabling this scan mode"); */
		/* nutscan_avail_eaton_serial(?) = 0; */
#else
		upsdebugx(1, "SERIAL SCAN: no pthread support, starting nutscan_scan_eaton_serial...");
		/* dev[TYPE_EATON_SERIAL] = nutscan_scan_eaton_serial (serial_ports); */
		run_eaton_serial(serial_ports);
#endif /* HAVE_PTHREAD */
	} else {
		upsdebugx(1, "SERIAL SCAN: not requested or supported, SKIPPED");
	}

#ifdef HAVE_PTHREAD
	if (allow_usb && nutscan_avail_usb && thread[TYPE_USB]) {
		upsdebugx(1, "USB SCAN: join back the pthread");
		pthread_join(thread[TYPE_USB], NULL);
	}
	if (allow_snmp && nutscan_avail_snmp && thread[TYPE_SNMP]) {
		upsdebugx(1, "SNMP SCAN: join back the pthread");
		pthread_join(thread[TYPE_SNMP], NULL);
	}
	if (allow_xml && nutscan_avail_xml_http && thread[TYPE_XML]) {
		upsdebugx(1, "XML/HTTP SCAN: join back the pthread");
		pthread_join(thread[TYPE_XML], NULL);
	}
	if (allow_oldnut && nutscan_avail_nut && thread[TYPE_NUT]) {
		upsdebugx(1, "NUT bus (old) SCAN: join back the pthread");
		pthread_join(thread[TYPE_NUT], NULL);
	}
	if (allow_nut_simulation && nutscan_avail_nut_simulation && thread[TYPE_NUT_SIMULATION]) {
		upsdebugx(1, "NUT simulation devices SCAN: join back the pthread");
		pthread_join(thread[TYPE_NUT_SIMULATION], NULL);
	}
	if (allow_avahi && nutscan_avail_avahi && thread[TYPE_AVAHI]) {
		upsdebugx(1, "NUT bus (avahi) SCAN: join back the pthread");
		pthread_join(thread[TYPE_AVAHI], NULL);
	}
	if (allow_ipmi && nutscan_avail_ipmi && thread[TYPE_IPMI]) {
		upsdebugx(1, "IPMI SCAN: join back the pthread");
		pthread_join(thread[TYPE_IPMI], NULL);
	}
	if (allow_eaton_serial && thread[TYPE_EATON_SERIAL]) {
		upsdebugx(1, "SERIAL SCAN: join back the pthread");
		pthread_join(thread[TYPE_EATON_SERIAL], NULL);
	}
#endif /* HAVE_PTHREAD */

	upsdebugx(1, "SCANS DONE: display results");

	upsdebugx(1, "SCANS DONE: display results: USB");
	display_func(dev[TYPE_USB]);
	upsdebugx(1, "SCANS DONE: free resources: USB");
	nutscan_free_device(dev[TYPE_USB]);

	upsdebugx(1, "SCANS DONE: display results: SNMP");
	display_func(dev[TYPE_SNMP]);
	upsdebugx(1, "SCANS DONE: free resources: SNMP");
	nutscan_free_device(dev[TYPE_SNMP]);

	upsdebugx(1, "SCANS DONE: display results: XML/HTTP");
	display_func(dev[TYPE_XML]);
	upsdebugx(1, "SCANS DONE: free resources: XML/HTTP");
	nutscan_free_device(dev[TYPE_XML]);

	upsdebugx(1, "SCANS DONE: display results: NUT bus (old)");
	display_func(dev[TYPE_NUT]);
	upsdebugx(1, "SCANS DONE: free resources: NUT bus (old)");
	nutscan_free_device(dev[TYPE_NUT]);

	upsdebugx(1, "SCANS DONE: display results: NUT simulation devices");
	display_func(dev[TYPE_NUT_SIMULATION]);
	upsdebugx(1, "SCANS DONE: free resources: NUT simulation devices");
	nutscan_free_device(dev[TYPE_NUT_SIMULATION]);

	upsdebugx(1, "SCANS DONE: display results: NUT bus (avahi)");
	display_func(dev[TYPE_AVAHI]);
	upsdebugx(1, "SCANS DONE: free resources: NUT bus (avahi)");
	nutscan_free_device(dev[TYPE_AVAHI]);

	upsdebugx(1, "SCANS DONE: display results: IPMI");
	display_func(dev[TYPE_IPMI]);
	upsdebugx(1, "SCANS DONE: free resources: IPMI");
	nutscan_free_device(dev[TYPE_IPMI]);

	upsdebugx(1, "SCANS DONE: display results: SERIAL");
	display_func(dev[TYPE_EATON_SERIAL]);
	upsdebugx(1, "SCANS DONE: free resources: SERIAL");
	nutscan_free_device(dev[TYPE_EATON_SERIAL]);

#ifdef HAVE_PTHREAD
# ifdef HAVE_SEMAPHORE_UNNAMED
	sem_destroy(nutscan_semaphore());
# elif defined HAVE_SEMAPHORE_NAMED
	if (nutscan_semaphore()) {
		sem_unlink(SEMNAME_TOPLEVEL);
		sem_close(nutscan_semaphore());
		nutscan_semaphore_set(NULL);
	}
# endif
#endif

	upsdebugx(1, "SCANS DONE: free common scanner resources");
	nutscan_free_ip_ranges(&ip_ranges_list);
	nutscan_free();

	upsdebugx(1, "SCANS DONE: EXIT_SUCCESS");
	return EXIT_SUCCESS;
}
