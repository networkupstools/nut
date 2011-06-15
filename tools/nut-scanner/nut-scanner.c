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

/* TODO list:
 * - network iterator (IPv4 and v6) for connect style scan
 * - handle XML/HTTP and SNMP answers (need thread?)
 * - Avahi support for NUT instances discovery
 * (...)
 * https://alioth.debian.org/pm/task.php?func=detailtask&project_task_id=477&group_id=30602&group_project_id=42
 */

#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include <unistd.h>
#include <getopt.h>
#include <string.h>

#include "nut-scan.h"

#define DEFAULT_TIMEOUT 1

const char optstring[] = "?ht:s:e:c:";
const struct option longopts[] =
	{{ "timeout",required_argument,NULL,'t' },
	{ "start_ip",required_argument,NULL,'s' },
	{ "end_ip",required_argument,NULL,'e' },
	{ "community",required_argument,NULL,'c' },
	{ "help",no_argument,NULL,'h' },
	{NULL,0,NULL,0}};


int main(int argc, char *argv[])
{
	device_t * dev;
	snmp_security_t sec;
	long timeout = DEFAULT_TIMEOUT*1000*1000; /* in usec */
	int opt_ret;
	char *	start_ip = NULL;
	char *	end_ip = NULL;

	memset(&sec,0,sizeof(sec));

	while((opt_ret = getopt_long(argc, argv,optstring,longopts,NULL))!=-1) {

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
			case 'c':
				sec.community = strdup(optarg);
				break;
			case 'h':
			case '?':
			default:
				puts("nut-scanner : detecting available UPS.\n");
				puts("Options list:");
				printf("\t-t, --timeout\t<timeout in seconds>: network operation timeout (default %d).\n\n",DEFAULT_TIMEOUT);
				printf("\t-s, --start_ip\t<IP address>: First IP address to scan.\n\n");
				printf("\t-e, --end_ip\t<IP address>: Last IP address to scan.\n\n");
				printf("SNMP specific options :\n");
				printf("\t-c, --community\t<SNMP community name>: Set SNMP v1 community name. (default = public)\n\n");
				return 0;
		}

	}


#ifdef HAVE_USB_H
	printf("Scanning USB bus:\n");
	dev = scan_usb();
	display_ups_conf(dev);
	free_device(dev);
#endif /* HAVE_USB_H */

#ifdef HAVE_NET_SNMP_NET_SNMP_CONFIG_H
	printf("Scanning SNMP bus:\n");
	dev = scan_snmp(start_ip,end_ip,timeout,&sec);
	display_ups_conf(dev);
        free_device(dev);

#endif /* HAVE_NET_SNMP_NET_SNMP_CONFIG_H */

	printf("Scanning XML/HTTP bus:\n");
	dev = scan_xml_http(timeout);
	display_ups_conf(dev);
	free_device(dev);

/*TODO*/
	printf("Scanning NUT bus (old connect method):\n");
	scan_nut();

/*TODO*/
	printf("Scanning NUT bus (via avahi):\n");
	scan_avahi();

/*TODO*/
	printf("Scanning IPMI bus:\n");
	scan_ipmi();

	return EXIT_SUCCESS;
}

int
network_iterator (int argc, char *argv[])
{
    unsigned int iterator;
    int ipStart[]={192,168,0,100};
    int ipEnd[] = {192,168,10,100};

    unsigned int startIP= (
        ipStart[0] << 24 |
        ipStart[1] << 16 |
        ipStart[2] << 8 |
        ipStart[3]);
    unsigned int endIP= (
        ipEnd[0] << 24 |
        ipEnd[1] << 16 |
        ipEnd[2] << 8 |
        ipEnd[3]);

    for (iterator=startIP; iterator < endIP; iterator++)
    {
        printf (" %d.%d.%d.%d\n",
            (iterator & 0xFF000000)>>24,
            (iterator & 0x00FF0000)>>16,
            (iterator & 0x0000FF00)>>8,
            (iterator & 0x000000FF)
        );
    }

    return 0;
}
