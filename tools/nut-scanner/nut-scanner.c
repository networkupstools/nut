/* nut-scanner.c: a tool to detect NUT supported devices
 * 
 *  Copyright (C) 2011 - Arnaud Quette <arnaud.quette@free.fr>
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

#define DEFAULT_TIMEOUT 5

#define ERR_BAD_OPTION	(-1)

const char optstring[] = "?ht:s:e:c:l:u:W:X:w:x:p:CUSMOAm:NPqIVa";

#ifdef HAVE_GETOPT_LONG
const struct option longopts[] =
	{{ "timeout",required_argument,NULL,'t' },
	{ "start_ip",required_argument,NULL,'s' },
	{ "end_ip",required_argument,NULL,'e' },
	{ "mask_cidr",required_argument,NULL,'m' },
	{ "community",required_argument,NULL,'c' },
	{ "secLevel",required_argument,NULL,'l' },
	{ "secName",required_argument,NULL,'u' },
	{ "authPassword",required_argument,NULL,'W' },
	{ "privPassword",required_argument,NULL,'X' },
	{ "authProtocol",required_argument,NULL,'w' },
	{ "privProtocol",required_argument,NULL,'x' },
	{ "port",required_argument,NULL,'p' },
	{ "complete_scan",no_argument,NULL,'C' },
	{ "usb_scan",no_argument,NULL,'U' },
	{ "snmp_scan",no_argument,NULL,'S' },
	{ "xml_scan",no_argument,NULL,'M' },
	{ "oldnut_scan",no_argument,NULL,'O' },
	{ "avahi_scan",no_argument,NULL,'A' },
	{ "ipmi_scan",no_argument,NULL,'I' },
	{ "disp_nut_conf",no_argument,NULL,'N' },
	{ "disp_parsable",no_argument,NULL,'P' },
	{ "quiet",no_argument,NULL,'q' },
	{ "help",no_argument,NULL,'h' },
	{ "version",no_argument,NULL,'V' },
	{ "available",no_argument,NULL,'a' },
	{NULL,0,NULL,0}};
#else
#define getopt_long(a,b,c,d,e)	getopt(a,b,c) 
#endif /* HAVE_GETOPT_LONG */

static nutscan_device_t *dev[TYPE_END];

static long timeout = DEFAULT_TIMEOUT*1000*1000; /* in usec */
static char *	start_ip = NULL;
static char *	end_ip = NULL;
static char * port = NULL;

#ifdef HAVE_PTHREAD
static pthread_t thread[TYPE_END];

static void * run_usb(void * arg)
{
	dev[TYPE_USB] = nutscan_scan_usb();
	return NULL;
}
static void * run_snmp(void * arg)
{
	nutscan_snmp_t * sec = (nutscan_snmp_t *)arg;

	dev[TYPE_SNMP] = nutscan_scan_snmp(start_ip,end_ip,timeout,sec);
	return NULL;
}
static void * run_xml(void * arg)
{
	dev[TYPE_XML] = nutscan_scan_xml_http(timeout);
	return NULL;
}

static void * run_nut_old(void * arg)
{
	dev[TYPE_NUT] = nutscan_scan_nut(start_ip,end_ip,port,timeout);
	return NULL;
}

static void * run_avahi(void * arg)
{
	dev[TYPE_AVAHI] = nutscan_scan_avahi(timeout);
	return NULL;
}
static void * run_ipmi(void * arg)
{
	dev[TYPE_IPMI] = nutscan_scan_ipmi();
	return NULL;
}
#endif /* HAVE_PTHREAD */
static int printq(int quiet,const char *fmt, ...)
{
	va_list ap;
	int ret;

	if(quiet) {
		return 0;
	}

        va_start(ap, fmt);
        ret = vprintf(fmt, ap);
        va_end(ap);

	return ret;
}

int main(int argc, char *argv[])
{
	nutscan_snmp_t sec;
	int opt_ret;
	char *	cidr = NULL;
	int allow_all = 0;
	int allow_usb = 0;
	int allow_snmp = 0;
	int allow_xml = 0;
	int allow_oldnut = 0;
	int allow_avahi = 0;
	int allow_ipmi = 0;
	int quiet = 0;
	void (*display_func)(nutscan_device_t * device);
	int ret_code = EXIT_SUCCESS;

	memset(&sec,0,sizeof(sec));

	nutscan_init();

	display_func = nutscan_display_ups_conf;

	while((opt_ret = getopt_long(argc, argv, optstring, longopts, NULL))!=-1) {

		switch(opt_ret) {
			case 't':
				timeout = atol(optarg)*1000*1000; /*in usec*/
				if( timeout == 0 ) {
					fprintf(stderr,"Illegal timeout value, using default %ds\n", DEFAULT_TIMEOUT);
					timeout = DEFAULT_TIMEOUT*1000*1000;
				}
				break;
			case 's':
				start_ip = strdup(optarg);
				end_ip = start_ip;
				break;
			case 'e':
				end_ip = strdup(optarg);
				break;
			case 'm':
				cidr = strdup(optarg);
				break;
			case 'c':
				if(!nutscan_avail_snmp) {
					goto display_help;
				}
				sec.community = strdup(optarg);
				break;
			case 'l':
				if(!nutscan_avail_snmp) {
					goto display_help;
				}
				sec.secLevel = strdup(optarg);
				break;
			case 'u':
				if(!nutscan_avail_snmp) {
					goto display_help;
				}
				sec.secName = strdup(optarg);
				break;
			case 'W':
				if(!nutscan_avail_snmp) {
					goto display_help;
				}
				sec.authPassword = strdup(optarg);
				break;
			case 'X':
				if(!nutscan_avail_snmp) {
					goto display_help;
				}
				sec.privPassword = strdup(optarg);
				break;
			case 'w':
				if(!nutscan_avail_snmp) {
					goto display_help;
				}
				sec.authProtocol = strdup(optarg);
				break;
			case 'x':
				if(!nutscan_avail_snmp) {
					goto display_help;
				}
				sec.privProtocol = strdup(optarg);
				break;
			case 'S':
				if(!nutscan_avail_snmp) {
					goto display_help;
				}
				allow_snmp = 1;
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
				exit(EXIT_SUCCESS);
			case '?':
				ret_code = ERR_BAD_OPTION;
			case 'h':
			default:
display_help:
				puts("nut-scanner : detecting available power devices.\n");
				puts("OPTIONS:");
				printf("  -C, --complete_scan: Scan all available devices (default).\n");
				if( nutscan_avail_usb ) {
					printf("  -U, --usb_scan: Scan USB devices.\n");
				}
				if( nutscan_avail_snmp ) {
					printf("  -S, --snmp_scan: Scan SNMP devices.\n");
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
				printf("  -t, --timeout <timeout in seconds>: network operation timeout (default %d).\n",DEFAULT_TIMEOUT);
				printf("  -s, --start_ip <IP address>: First IP address to scan.\n");
				printf("  -e, --end_ip <IP address>: Last IP address to scan.\n");
				printf("  -m, --mask_cidr <IP address/mask>: Give a range of IP using CIDR notation.\n");

				if( nutscan_avail_snmp ) {
					printf("\nSNMP v1 specific options:\n");
					printf("  -c, --community <community name>: Set SNMP v1 community name (default = public)\n");

					printf("\nSNMP v3 specific options:\n");
					printf("  -l, --secLevel <security level>: Set the securityLevel used for SNMPv3 messages (allowed values: noAuthNoPriv,authNoPriv,authPriv)\n");
					printf("  -u, --secName <security name>: Set the securityName used for authenticated SNMPv3 messages (mandatory if you set secLevel. No default)\n");
					printf("  -a, --authProtocol <authentication protocol>: Set the authentication protocol (MD5 or SHA) used for authenticated SNMPv3 messages (default=MD5)\n");
					printf("  -A, --authPassword <authentication pass phrase>: Set the authentication pass phrase used for authenticated SNMPv3 messages (mandatory if you set secLevel to authNoPriv or authPriv)\n");
					printf("  -x, --privProtocol <privacy protocol>: Set the privacy protocol (DES or AES) used for encrypted SNMPv3 messages (default=DES)\n");
					printf("  -X, --privPassword <privacy pass phrase>: Set the privacy pass phrase used for encrypted SNMPv3 messages (mandatory if you set secLevel to authPriv)\n");
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
				return ret_code;
		}

	}

	if( cidr ) {
		nutscan_cidr_to_ip(cidr, &start_ip, &end_ip);
	}

	if( !allow_usb && !allow_snmp && !allow_xml && !allow_oldnut &&
		!allow_avahi && !allow_ipmi ) {
		allow_all = 1;
	}

	if( allow_all ) {
		allow_usb = 1;
		allow_snmp = 1;
		allow_xml = 1;
		allow_oldnut = 1;
		allow_avahi = 1;
		allow_ipmi = 1;
	}

	if( allow_usb && nutscan_avail_usb ) {
		printq(quiet,"Scanning USB bus.\n");
#ifdef HAVE_PTHREAD
		if(pthread_create(&thread[TYPE_USB],NULL,run_usb,NULL)) {
			nutscan_avail_usb = 0;
		}
#else
		dev[TYPE_USB] = nutscan_scan_usb();
#endif /* HAVE_PTHREAD */
	}

	if( allow_snmp && nutscan_avail_snmp ) {
		if( start_ip == NULL ) {
			printq(quiet,"No start IP, skipping SNMP\n");
		}
		else {
			printq(quiet,"Scanning SNMP bus.\n");
#ifdef HAVE_PTHREAD
			if( pthread_create(&thread[TYPE_SNMP],NULL,run_snmp,&sec)) {
				nutscan_avail_snmp = 0;
			}
#else
			dev[TYPE_SNMP] = nutscan_scan_snmp(start_ip,end_ip,timeout,&sec);
#endif /* HAVE_PTHREAD */
		}
	}

	if( allow_xml && nutscan_avail_xml_http) {
		printq(quiet,"Scanning XML/HTTP bus.\n");
#ifdef HAVE_PTHREAD
		if(pthread_create(&thread[TYPE_XML],NULL,run_xml,NULL)) {
			nutscan_avail_xml_http = 0;
		}
#else
		dev[TYPE_XML] = nutscan_scan_xml_http(timeout);
#endif /* HAVE_PTHREAD */
	}

	if( allow_oldnut && nutscan_avail_nut) {
		if( start_ip == NULL ) {
			printq(quiet,"No start IP, skipping NUT bus (old connect method)\n");
		}
		else {
			printq(quiet,"Scanning NUT bus (old connect method).\n");
#ifdef HAVE_PTHREAD
			if(pthread_create(&thread[TYPE_NUT],NULL,run_nut_old,NULL)) {
				nutscan_avail_nut = 0;
			}
#else
			dev[TYPE_NUT] = nutscan_scan_nut(start_ip,end_ip,port,timeout);
#endif /* HAVE_PTHREAD */
		}
	}

	if( allow_avahi && nutscan_avail_avahi) {
		printq(quiet,"Scanning NUT bus (avahi method).\n");
#ifdef HAVE_PTHREAD
		if(pthread_create(&thread[TYPE_AVAHI],NULL,run_avahi,NULL)) {
			nutscan_avail_avahi = 0;
		}
#else
		dev[TYPE_AVAHI] = nutscan_scan_avahi();
#endif /* HAVE_PTHREAD */
	}

	if( allow_ipmi  && nutscan_avail_ipmi) {
		printq(quiet,"Scanning IPMI bus.\n");
#ifdef HAVE_PTHREAD
		if(pthread_create(&thread[TYPE_IPMI],NULL,run_ipmi,NULL)) {
			nutscan_avail_ipmi = 0;
		}
#else
		dev[TYPE_IPMI] = nutscan_scan_ipmi();
#endif /* HAVE_PTHREAD */
	}

#ifdef HAVE_PTHREAD
	if( allow_usb && nutscan_avail_usb ) {
		pthread_join(thread[TYPE_USB],NULL);
	}
	if( allow_snmp && nutscan_avail_snmp ) {
		pthread_join(thread[TYPE_SNMP],NULL);
	}
	if( allow_xml && nutscan_avail_xml_http ) {
		pthread_join(thread[TYPE_XML],NULL);
	}
	if( allow_oldnut && nutscan_avail_nut ) {
		pthread_join(thread[TYPE_NUT],NULL);
	}
	if( allow_avahi && nutscan_avail_avahi ) {
		pthread_join(thread[TYPE_AVAHI],NULL);
	}
	if( allow_ipmi && nutscan_avail_ipmi ) {
		pthread_join(thread[TYPE_IPMI],NULL);
	}
#endif /* HAVE_PTHREAD */

	display_func(dev[TYPE_USB]);
	nutscan_free_device(dev[TYPE_USB]);

	display_func(dev[TYPE_SNMP]);
	nutscan_free_device(dev[TYPE_SNMP]);

	display_func(dev[TYPE_XML]);
	nutscan_free_device(dev[TYPE_XML]);

	display_func(dev[TYPE_NUT]);
	nutscan_free_device(dev[TYPE_NUT]);

	display_func(dev[TYPE_AVAHI]);
	nutscan_free_device(dev[TYPE_AVAHI]);

	display_func(dev[TYPE_IPMI]);
	nutscan_free_device(dev[TYPE_IPMI]);
	return EXIT_SUCCESS;
}
