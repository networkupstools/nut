/*
 *  Copyright (C) 2011 - 2012  Arnaud Quette <arnaud.quette@free.fr>
 *  Copyright (C) 2016 Michal Vyskocil <MichalVyskocil@eaton.com>
 *  Copyright (C) 2016 Jim Klimov <EvgenyKlimov@eaton.com>
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
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "common.h"
#include "nut_version.h"
#include <unistd.h>
#include <string.h>
#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif

#include "nut-scan.h"

#define ERR_BAD_OPTION	(-1)

static const char optstring[] = "?ht:s:e:E:c:l:u:W:X:w:x:p:b:B:d:L:CUSMOAm:NPqIVaD";

#ifdef HAVE_GETOPT_LONG
static const struct option longopts[] = {
	{ "timeout", required_argument, NULL, 't' },
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
	{ "oldnut_scan", no_argument, NULL, 'O' },
	{ "avahi_scan", no_argument, NULL, 'A' },
	{ "ipmi_scan", no_argument, NULL, 'I' },
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

static long timeout = DEFAULT_NETWORK_TIMEOUT * 1000 * 1000; /* in usec */
static char * start_ip = NULL;
static char * end_ip = NULL;
static char * port = NULL;
static char * serial_ports = NULL;

#ifdef HAVE_PTHREAD
static pthread_t thread[TYPE_END];

static void * run_usb(void *arg)
{
	NUT_UNUSED_VARIABLE(arg);

	dev[TYPE_USB] = nutscan_scan_usb();
	return NULL;
}

static void * run_snmp(void * arg)
{
	nutscan_snmp_t * sec = (nutscan_snmp_t *)arg;

	dev[TYPE_SNMP] = nutscan_scan_snmp(start_ip, end_ip, timeout, sec);
	return NULL;
}

static void * run_xml(void * arg)
{
	nutscan_xml_t * sec = (nutscan_xml_t *)arg;

	dev[TYPE_XML] = nutscan_scan_xml_http_range(start_ip, end_ip, timeout, sec);
	return NULL;
}

static void * run_nut_old(void *arg)
{
	NUT_UNUSED_VARIABLE(arg);

	dev[TYPE_NUT] = nutscan_scan_nut(start_ip, end_ip, port, timeout);
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

	dev[TYPE_IPMI] = nutscan_scan_ipmi(start_ip, end_ip, sec);
	return NULL;
}

static void * run_eaton_serial(void *arg)
{
	NUT_UNUSED_VARIABLE(arg);

	dev[TYPE_EATON_SERIAL] = nutscan_scan_eaton_serial(serial_ports);
	return NULL;
}

#endif /* HAVE_PTHREAD */

static void show_usage()
{
/* NOTE: This code uses `nutscan_avail_*` global vars from nutscan-init.c */
	puts("nut-scanner : utility for detection of available power devices.\n");
	puts("OPTIONS:");
	printf("  -C, --complete_scan: Scan all available devices except serial ports (default).\n");
	if( nutscan_avail_usb ) {
		printf("  -U, --usb_scan: Scan USB devices.\n");
	}
	if( nutscan_avail_snmp ) {
		printf("  -S, --snmp_scan: Scan SNMP devices using built-in mapping definitions.\n");
	}
	if( nutscan_avail_xml_http ) {
		printf("  -M, --xml_scan: Scan XML/HTTP devices.\n");
	}
	printf("  -O, --oldnut_scan: Scan NUT devices (old method).\n");
	if( nutscan_avail_avahi ) {
		printf("  -A, --avahi_scan: Scan NUT devices (avahi method).\n");
	}
	if( nutscan_avail_ipmi ) {
		printf("  -I, --ipmi_scan: Scan IPMI devices.\n");
	}

	printf("  -E, --eaton_serial <serial ports list>: Scan serial Eaton devices (XCP, SHUT and Q1).\n");

	printf("\nNetwork specific options:\n");
	printf("  -t, --timeout <timeout in seconds>: network operation timeout (default %d).\n", DEFAULT_NETWORK_TIMEOUT);
	printf("  -s, --start_ip <IP address>: First IP address to scan.\n");
	printf("  -e, --end_ip <IP address>: Last IP address to scan.\n");
	printf("  -m, --mask_cidr <IP address/mask>: Give a range of IP using CIDR notation.\n");

	if( nutscan_avail_snmp ) {
		printf("\nSNMP v1 specific options:\n");
		printf("  -c, --community <community name>: Set SNMP v1 community name (default = public)\n");

		printf("\nSNMP v3 specific options:\n");
		printf("  -l, --secLevel <security level>: Set the securityLevel used for SNMPv3 messages (allowed values: noAuthNoPriv,authNoPriv,authPriv)\n");
		printf("  -u, --secName <security name>: Set the securityName used for authenticated SNMPv3 messages (mandatory if you set secLevel. No default)\n");
		printf("  -w, --authProtocol <authentication protocol>: Set the authentication protocol (MD5 or SHA) used for authenticated SNMPv3 messages (default=MD5)\n");
		printf("  -W, --authPassword <authentication pass phrase>: Set the authentication pass phrase used for authenticated SNMPv3 messages (mandatory if you set secLevel to authNoPriv or authPriv)\n");
		printf("  -x, --privProtocol <privacy protocol>: Set the privacy protocol (DES or AES) used for encrypted SNMPv3 messages (default=DES)\n");
		printf("  -X, --privPassword <privacy pass phrase>: Set the privacy pass phrase used for encrypted SNMPv3 messages (mandatory if you set secLevel to authPriv)\n");
	}

	if( nutscan_avail_ipmi ) {
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
	printf("  -N, --disp_nut_conf: Display result in the ups.conf format\n");
	printf("  -P, --disp_parsable: Display result in a parsable format\n");
	printf("\nMiscellaneous options:\n");
	printf("  -V, --version: Display NUT version\n");
	printf("  -a, --available: Display available bus that can be scanned\n");
	printf("  -q, --quiet: Display only scan result. No information on currently scanned bus is displayed.\n");
	printf("  -D, --nut_debug_level: Raise the debugging level.  Use this multiple times to see more details.\n");
}

int main(int argc, char *argv[])
{
	nutscan_snmp_t snmp_sec;
	nutscan_ipmi_t ipmi_sec;
	nutscan_xml_t  xml_sec;
	int opt_ret;
	char *	cidr = NULL;
	int allow_all = 0;
	int allow_usb = 0;
	int allow_snmp = 0;
	int allow_xml = 0;
	int allow_oldnut = 0;
	int allow_avahi = 0;
	int allow_ipmi = 0;
	int allow_eaton_serial = 0; /* MUST be requested explicitly! */
	int quiet = 0; /* The debugging level for certain upsdebugx() progress messages; 0 = print always, quiet==1 is to require at least one -D */
	void (*display_func)(nutscan_device_t * device);
	int ret_code = EXIT_SUCCESS;

	memset(&snmp_sec, 0, sizeof(snmp_sec));
	memset(&ipmi_sec, 0, sizeof(ipmi_sec));
	memset(&xml_sec, 0, sizeof(xml_sec));

	/* Set the default values for IPMI */
	ipmi_sec.authentication_type = IPMI_AUTHENTICATION_TYPE_MD5;
	ipmi_sec.ipmi_version = IPMI_1_5; /* default to IPMI 1.5, if not otherwise specified */
	ipmi_sec.cipher_suite_id = 3; /* default to HMAC-SHA1; HMAC-SHA1-96; AES-CBC-128 */
	ipmi_sec.privilege_level = IPMI_PRIVILEGE_LEVEL_ADMIN; /* should be sufficient */

	/* Set the default values for XML HTTP (run_xml()) */
	xml_sec.port_http = 80;
	xml_sec.port_udp = 4679;
	xml_sec.usec_timeout = -1; /* Override with the "timeout" common setting later */
	xml_sec.peername = NULL;

	/* Parse command line options -- First loop: only get debug level */
	/* Suppress error messages, for now -- leave them to the second loop. */
	opterr = 0;
	while((opt_ret = getopt_long(argc, argv, optstring, longopts, NULL)) != -1)
		if (opt_ret == 'D')
			nut_debug_level++;

	nutscan_init();

	display_func = nutscan_display_ups_conf;

	/* Parse command line options -- Second loop: everything else */
	/* Restore error messages... */
	opterr = 1;
	/* ...and index of the item to be processed by getopt(). */
	optind = 1;
	/* Note: the getopts print an error message about unknown arguments
	 * or arguments which need a second token and that is missing now */
	while((opt_ret = getopt_long(argc, argv, optstring, longopts, NULL))!=-1) {

		switch(opt_ret) {
			case 't':
				timeout = atol(optarg)*1000*1000; /*in usec*/
				if( timeout == 0 ) {
					fprintf(stderr,"Illegal timeout value, using default %ds\n", DEFAULT_NETWORK_TIMEOUT);
					timeout = DEFAULT_NETWORK_TIMEOUT * 1000 * 1000;
				}
				break;
			case 's':
				start_ip = strdup(optarg);
				if (end_ip == NULL)
					end_ip = start_ip;
				break;
			case 'e':
				end_ip = strdup(optarg);
				if (start_ip == NULL)
					start_ip = end_ip;
				break;
			case 'E':
				serial_ports = strdup(optarg);
				allow_eaton_serial = 1;
				break;
			case 'm':
				cidr = strdup(optarg);
				break;
			case 'D':
				/* nothing to do, here */
				break;
			case 'c':
				if(!nutscan_avail_snmp) {
					goto display_help;
				}
				snmp_sec.community = strdup(optarg);
				break;
			case 'l':
				if(!nutscan_avail_snmp) {
					goto display_help;
				}
				snmp_sec.secLevel = strdup(optarg);
				break;
			case 'u':
				if(!nutscan_avail_snmp) {
					goto display_help;
				}
				snmp_sec.secName = strdup(optarg);
				break;
			case 'W':
				if(!nutscan_avail_snmp) {
					goto display_help;
				}
				snmp_sec.authPassword = strdup(optarg);
				break;
			case 'X':
				if(!nutscan_avail_snmp) {
					goto display_help;
				}
				snmp_sec.privPassword = strdup(optarg);
				break;
			case 'w':
				if(!nutscan_avail_snmp) {
					goto display_help;
				}
				snmp_sec.authProtocol = strdup(optarg);
				break;
			case 'x':
				if(!nutscan_avail_snmp) {
					goto display_help;
				}
				snmp_sec.privProtocol = strdup(optarg);
				break;
			case 'S':
				if(!nutscan_avail_snmp) {
					goto display_help;
				}
				allow_snmp = 1;
				break;
			case 'b':
				if(!nutscan_avail_ipmi) {
					goto display_help;
				}
				ipmi_sec.username = strdup(optarg);
				break;
			case 'B':
				if(!nutscan_avail_ipmi) {
					goto display_help;
				}
				ipmi_sec.password = strdup(optarg);
				break;
			case 'd':
				if(!nutscan_avail_ipmi) {
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
					fprintf(stderr,"Unknown authentication type (%s). Defaulting to MD5\n", optarg);
				}
				break;
			case 'L':
				if(!nutscan_avail_ipmi) {
					goto display_help;
				}
				ipmi_sec.cipher_suite_id = atoi(optarg);
				/* Force IPMI 2.0! */
				ipmi_sec.ipmi_version = IPMI_2_0;
				break;
			case 'p':
				port = strdup(optarg);
				break;
			case 'C':
				allow_all = 1;
				break;
			case 'U':
				if(!nutscan_avail_usb) {
					goto display_help;
				}
				allow_usb = 1;
				break;
			case 'M':
				if(!nutscan_avail_xml_http) {
					goto display_help;
				}
				allow_xml = 1;
				break;
			case 'O':
				allow_oldnut = 1;
				break;
			case 'A':
				if(!nutscan_avail_avahi) {
					goto display_help;
				}
				allow_avahi = 1;
				break;
			case 'I':
				if(!nutscan_avail_ipmi) {
					goto display_help;
				}
				allow_ipmi = 1;
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
				printf("Network UPS Tools - %s\n", NUT_VERSION_MACRO);
				exit(EXIT_SUCCESS);
			case 'a':
				printf("OLDNUT\n");
				if(nutscan_avail_usb) {
					printf("USB\n");
				}
				if(nutscan_avail_snmp) {
					printf("SNMP\n");
				}
				if(nutscan_avail_xml_http) {
					printf("XML\n");
				}
				if(nutscan_avail_avahi) {
					printf("AVAHI\n");
				}
				if(nutscan_avail_ipmi) {
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
				show_usage();
				if ((opt_ret != 'h') || (ret_code != EXIT_SUCCESS))
					fprintf(stderr,"\n\n"
						"WARNING: Some error has occurred while processing 'nut-scanner' command-line\n"
						"arguments, see more details above the usage help text.\n\n");
				return ret_code;
		}
	}

	if( cidr ) {
		nutscan_cidr_to_ip(cidr, &start_ip, &end_ip);
	}

	if( !allow_usb && !allow_snmp && !allow_xml && !allow_oldnut &&
		!allow_avahi && !allow_ipmi && !allow_eaton_serial) {
		allow_all = 1;
	}

	if( allow_all ) {
		allow_usb = 1;
		allow_snmp = 1;
		allow_xml = 1;
		allow_oldnut = 1;
		allow_avahi = 1;
		allow_ipmi = 1;
		/* BEWARE: allow_all does not include allow_eaton_serial! */
	}

/* TODO/discuss : Should the #else...#endif code below for lack of pthreads
 * during build also serve as a fallback for pthread failure at runtime?
 */
	if( allow_usb && nutscan_avail_usb ) {
		upsdebugx(quiet,"Scanning USB bus.");
#ifdef HAVE_PTHREAD
		if(pthread_create(&thread[TYPE_USB],NULL,run_usb,NULL)) {
			upsdebugx(1,"pthread_create returned an error; disabling this scan mode");
			nutscan_avail_usb = 0;
		}
#else
		upsdebugx(1,"USB SCAN: no pthread support, starting nutscan_scan_usb...");
		dev[TYPE_USB] = nutscan_scan_usb();
#endif /* HAVE_PTHREAD */
	} else {
		upsdebugx(1,"USB SCAN: not requested, SKIPPED");
	}

	if( allow_snmp && nutscan_avail_snmp ) {
		if( start_ip == NULL ) {
			upsdebugx(quiet,"No start IP, skipping SNMP");
			nutscan_avail_snmp = 0;
		}
		else {
			upsdebugx(quiet,"Scanning SNMP bus.");
#ifdef HAVE_PTHREAD
			upsdebugx(1,"SNMP SCAN: starting pthread_create with run_snmp...");
			if( pthread_create(&thread[TYPE_SNMP],NULL,run_snmp,&snmp_sec)) {
				upsdebugx(1,"pthread_create returned an error; disabling this scan mode");
				nutscan_avail_snmp = 0;
			}
#else
			upsdebugx(1,"SNMP SCAN: no pthread support, starting nutscan_scan_snmp...");
			dev[TYPE_SNMP] = nutscan_scan_snmp(start_ip,end_ip,timeout,&snmp_sec);
#endif /* HAVE_PTHREAD */
		}
	} else {
		upsdebugx(1,"SNMP SCAN: not requested, SKIPPED");
	}

	if( allow_xml && nutscan_avail_xml_http) {
		upsdebugx(quiet,"Scanning XML/HTTP bus.");
		xml_sec.usec_timeout = timeout;
#ifdef HAVE_PTHREAD
		upsdebugx(1,"XML/HTTP SCAN: starting pthread_create with run_xml...");
		if(pthread_create(&thread[TYPE_XML],NULL,run_xml,&xml_sec)) {
			upsdebugx(1,"pthread_create returned an error; disabling this scan mode");
			nutscan_avail_xml_http = 0;
		}
#else
		upsdebugx(1,"XML/HTTP SCAN: no pthread support, starting nutscan_scan_xml_http_range()...");
		dev[TYPE_XML] = nutscan_scan_xml_http_range(start_ip, end_ip, timeout, &xml_sec);
#endif /* HAVE_PTHREAD */
	} else {
		upsdebugx(1,"XML/HTTP SCAN: not requested, SKIPPED");
	}

	if( allow_oldnut && nutscan_avail_nut) {
		if( start_ip == NULL ) {
			upsdebugx(quiet,"No start IP, skipping NUT bus (old connect method)");
			nutscan_avail_nut = 0;
		}
		else {
			upsdebugx(quiet,"Scanning NUT bus (old connect method).");
#ifdef HAVE_PTHREAD
			upsdebugx(1,"NUT bus (old) SCAN: starting pthread_create with run_nut_old...");
			if(pthread_create(&thread[TYPE_NUT],NULL,run_nut_old,NULL)) {
				upsdebugx(1,"pthread_create returned an error; disabling this scan mode");
				nutscan_avail_nut = 0;
			}
#else
			upsdebugx(1,"NUT bus (old) SCAN: no pthread support, starting nutscan_scan_nut...");
			dev[TYPE_NUT] = nutscan_scan_nut(start_ip,end_ip,port,timeout);
#endif /* HAVE_PTHREAD */
		}
	} else {
		upsdebugx(1,"NUT bus (old) SCAN: not requested, SKIPPED");
	}

	if( allow_avahi && nutscan_avail_avahi) {
		upsdebugx(quiet,"Scanning NUT bus (avahi method).");
#ifdef HAVE_PTHREAD
		upsdebugx(1,"NUT bus (avahi) SCAN: starting pthread_create with run_avahi...");
		if(pthread_create(&thread[TYPE_AVAHI],NULL,run_avahi,NULL)) {
			upsdebugx(1,"pthread_create returned an error; disabling this scan mode");
			nutscan_avail_avahi = 0;
		}
#else
		upsdebugx(1,"NUT bus (avahi) SCAN: no pthread support, starting nutscan_scan_avahi...");
		dev[TYPE_AVAHI] = nutscan_scan_avahi(timeout);
#endif /* HAVE_PTHREAD */
	} else {
		upsdebugx(1,"NUT bus (avahi) SCAN: not requested, SKIPPED");
	}

	if( allow_ipmi  && nutscan_avail_ipmi) {
		upsdebugx(quiet,"Scanning IPMI bus.");
#ifdef HAVE_PTHREAD
		upsdebugx(1,"IPMI SCAN: starting pthread_create with run_ipmi...");
		if(pthread_create(&thread[TYPE_IPMI],NULL,run_ipmi,&ipmi_sec)) {
			upsdebugx(1,"pthread_create returned an error; disabling this scan mode");
			nutscan_avail_ipmi = 0;
		}
#else
		upsdebugx(1,"IPMI SCAN: no pthread support, starting nutscan_scan_ipmi...");
		dev[TYPE_IPMI] = nutscan_scan_ipmi(start_ip,end_ip,&ipmi_sec);
#endif /* HAVE_PTHREAD */
	} else {
		upsdebugx(1,"IPMI SCAN: not requested, SKIPPED");
	}

	/* Eaton serial scan */
	if (allow_eaton_serial) {
		upsdebugx(quiet,"Scanning serial bus for Eaton devices.");
#ifdef HAVE_PTHREAD
		upsdebugx(1,"SERIAL SCAN: starting pthread_create with run_eaton_serial (return not checked!)...");
		pthread_create(&thread[TYPE_EATON_SERIAL], NULL, run_eaton_serial, serial_ports);
		/* FIXME: check return code */
		/* upsdebugx(1,"pthread_create returned an error; disabling this scan mode"); */
		/* nutscan_avail_eaton_serial(?) = 0; */
#else
		upsdebugx(1,"SERIAL SCAN: no pthread support, starting nutscan_scan_eaton_serial...");
		dev[TYPE_EATON_SERIAL] = nutscan_scan_eaton_serial (serial_ports);
#endif /* HAVE_PTHREAD */
	} else {
		upsdebugx(1,"SERIAL SCAN: not requested, SKIPPED");
	}

#ifdef HAVE_PTHREAD
	if( allow_usb && nutscan_avail_usb && thread[TYPE_USB]) {
		upsdebugx(1,"USB SCAN: join back the pthread");
		pthread_join(thread[TYPE_USB], NULL);
	}
	if( allow_snmp && nutscan_avail_snmp && thread[TYPE_SNMP]) {
		upsdebugx(1,"SNMP SCAN: join back the pthread");
		pthread_join(thread[TYPE_SNMP], NULL);
	}
	if( allow_xml && nutscan_avail_xml_http && thread[TYPE_XML]) {
		upsdebugx(1,"XML/HTTP SCAN: join back the pthread");
		pthread_join(thread[TYPE_XML], NULL);
	}
	if( allow_oldnut && nutscan_avail_nut && thread[TYPE_NUT]) {
		upsdebugx(1,"NUT bus (old) SCAN: join back the pthread");
		pthread_join(thread[TYPE_NUT], NULL);
	}
	if( allow_avahi && nutscan_avail_avahi && thread[TYPE_AVAHI]) {
		upsdebugx(1,"NUT bus (avahi) SCAN: join back the pthread");
		pthread_join(thread[TYPE_AVAHI], NULL);
	}
	if( allow_ipmi && nutscan_avail_ipmi && thread[TYPE_IPMI]) {
		upsdebugx(1,"IPMI SCAN: join back the pthread");
		pthread_join(thread[TYPE_IPMI], NULL);
	}
	if (allow_eaton_serial && thread[TYPE_EATON_SERIAL]) {
		upsdebugx(1,"SERIAL SCAN: join back the pthread");
		pthread_join(thread[TYPE_EATON_SERIAL], NULL);
	}
#endif /* HAVE_PTHREAD */

	upsdebugx(1,"SCANS DONE: display results");

	upsdebugx(1,"SCANS DONE: display results: USB");
	display_func(dev[TYPE_USB]);
	upsdebugx(1,"SCANS DONE: free resources: USB");
	nutscan_free_device(dev[TYPE_USB]);

	upsdebugx(1,"SCANS DONE: display results: SNMP");
	display_func(dev[TYPE_SNMP]);
	upsdebugx(1,"SCANS DONE: free resources: SNMP");
	nutscan_free_device(dev[TYPE_SNMP]);

	upsdebugx(1,"SCANS DONE: display results: XML/HTTP");
	display_func(dev[TYPE_XML]);
	upsdebugx(1,"SCANS DONE: free resources: XML/HTTP");
	nutscan_free_device(dev[TYPE_XML]);

	upsdebugx(1,"SCANS DONE: display results: NUT bus (old)");
	display_func(dev[TYPE_NUT]);
	upsdebugx(1,"SCANS DONE: free resources: NUT bus (old)");
	nutscan_free_device(dev[TYPE_NUT]);

	upsdebugx(1,"SCANS DONE: display results: NUT bus (avahi)");
	display_func(dev[TYPE_AVAHI]);
	upsdebugx(1,"SCANS DONE: free resources: NUT bus (avahi)");
	nutscan_free_device(dev[TYPE_AVAHI]);

	upsdebugx(1,"SCANS DONE: display results: IPMI");
	display_func(dev[TYPE_IPMI]);
	upsdebugx(1,"SCANS DONE: free resources: IPMI");
	nutscan_free_device(dev[TYPE_IPMI]);

	upsdebugx(1,"SCANS DONE: display results: SERIAL");
	display_func(dev[TYPE_EATON_SERIAL]);
	upsdebugx(1,"SCANS DONE: free resources: SERIAL");
	nutscan_free_device(dev[TYPE_EATON_SERIAL]);

	upsdebugx(1,"SCANS DONE: free common scanner resources");
	nutscan_free();

	upsdebugx(1,"SCANS DONE: EXIT_SUCCESS");
	return EXIT_SUCCESS;
}
