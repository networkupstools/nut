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
 * - split scan method into separate .ch files (1 per bus / method)
 * - compile as a lib, with an executable for command line options
 * - network iterator (IPv4 and v6) for connect style scan
 * - handle XML/HTTP and SNMP answers
 * - Avahi support
 * (...)
 * https://alioth.debian.org/pm/task.php?func=detailtask&project_task_id=477&group_id=30602&group_project_id=42
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "upsclient.h"

int verbose = 0;
int nutdev_num = 1;

#ifdef HAVE_USB_H
#include "nutscan-usb.h"

char* is_usb_device_supported(usb_device_id_t *usb_device_id_list, int dev_VendorID, int dev_ProductID)
{
	usb_device_id_t *usbdev;

	for (usbdev = usb_device_id_list; usbdev->driver_name != NULL; usbdev++) {

		if ( (usbdev->vendorID == dev_VendorID)
			&& (usbdev->productID == dev_ProductID) ) {

			return usbdev->driver_name;
		}
	}
	
	return NULL;
}

void scan_usb()
{
	int ret;
	char string[256];
	char *driver_name = NULL;
	char *serialnumber = NULL;
	char *device_name = NULL;
	struct usb_device *dev;
	struct usb_bus *bus;
	usb_dev_handle *udev;

	printf("Scanning USB bus:\n");

	/* libusb base init */
	usb_init();
	usb_find_busses();
	usb_find_devices();

	for (bus = usb_busses; bus; bus = bus->next) {
		for (dev = bus->devices; dev; dev = dev->next) {

			/*printf("Checking USB device %04x:%04x (Bus: %s, Device: %s)\n",
				dev->descriptor.idVendor, dev->descriptor.idProduct,
				bus->dirname, dev->filename);*/

			if ((driver_name = is_usb_device_supported(usb_device_table, 
				dev->descriptor.idVendor, dev->descriptor.idProduct)) != NULL) {

				/*printf("=== supported...\n");*/

				/* open the device */
				udev = usb_open(dev);
				if (!udev) {
					printf("Failed to open device, skipping. (%s)\n", usb_strerror());
					continue;
				}

				/* need to get serial number */
				if (dev->descriptor.iSerialNumber) {
					ret = usb_get_string_simple(udev, dev->descriptor.iSerialNumber,
						string, sizeof(string));
					if (ret > 0) {
						serialnumber = strdup(string);
					}
				}
				/* need to get product name */
				if (dev->descriptor.iProduct) {
					ret = usb_get_string_simple(udev, dev->descriptor.iProduct,
						string, sizeof(string));
					if (ret > 0) {
						device_name = strdup(string);
					}
				}
				/* FIXME
				 * if (serialnumber == NULL)
				 * 		store vendorid, productid, product and ?bus?, Ie
				 * vendorid=dev->descriptor.idVendor
				 * productid=dev->descriptor.idProduct
				 * product=device_name
				 * bus=bus->dirname */

				memset (string, 0, 256);
				
				/* Format for ups.conf */
				printf("[nutdev%i]\n\tdriver=%s\n\tport=auto\n",
					nutdev_num, driver_name);

				if (serialnumber != NULL)
				{
					printf("\tserial=%s\n", serialnumber);
					free(serialnumber);
				}
				/* FIXME: else, we need to provide ways to find uniquely the device
				 * vendorid=dev->descriptor.idVendor
				 * productid=dev->descriptor.idProduct
				 * product=device_name
				 * bus=bus->dirname */

				nutdev_num++;

				usb_close(udev);

			}
		}
	}
}
#endif /* HAVE_USB_H */


#ifdef HAVE_NET_SNMP_NET_SNMP_CONFIG_H
void scan_snmp()
{
	
}
#endif /* HAVE_NET_SNMP_NET_SNMP_CONFIG_H */

/* #ifdef nothing apart socket for detection! */ 
void scan_xml_http()
{
	char *scanMsg = "<SCAN_REQUEST/>";
	int port = 4679;
	int peerSocket;
	int sockopt_on = 1;
	struct sockaddr_in sockAddress;
	socklen_t sockAddressLength = sizeof(sockAddress);
	memset(&sockAddress, 0, sizeof(sockAddress));

	printf("Scanning XML/HTTP bus:\n");

	if((peerSocket = socket(AF_INET, SOCK_DGRAM, 0)) != -1)
	{
		/* Initialize socket */
		sockAddress.sin_family = AF_INET;
		sockAddress.sin_addr.s_addr = INADDR_BROADCAST;
		sockAddress.sin_port = htons(port);
		setsockopt(peerSocket, SOL_SOCKET, SO_BROADCAST, &sockopt_on, sizeof(sockopt_on));

		/* Send scan request */
		if(sendto(peerSocket, scanMsg, strlen(scanMsg), 0,
			(struct sockaddr *)&sockAddress, sockAddressLength) <= 0)
		{
			printf("Error sending Eaton <SCAN_REQUEST/>\n");
		}
		else
		{
			/*printf("Eaton <SCAN_REQUEST/> sent\n");*/
			/* FIXME: handle replies */
			;
		}
	}
	else
	{
		printf("Error creating socket\n");
	}
}


/* FIXME: SSL support */
static void list_nut_devices(char *target_hostname)
{
	int port;
	unsigned int numq, numa;
	const char *query[4];
	char **answer;
	char *hostname = NULL;
	UPSCONN_t *ups = malloc(sizeof(*ups));

	query[0] = "UPS";
	numq = 1;


	if (upscli_splitaddr(target_hostname ? target_hostname : "localhost", &hostname, &port) != 0) {
		return;
	}

	if (upscli_connect(ups, hostname, port, UPSCLI_CONN_TRYSSL) < 0) {

		printf("Error: %s\n", upscli_strerror(ups));
		return;
	}

	if(upscli_list_start(ups, numq, query) < 0) {

		printf("Error: %s\n", upscli_strerror(ups));
		return;
	}

	while (upscli_list_next(ups, numq, query, &numa, &answer) == 1) {

		/* UPS <upsname> <description> */
		if (numa < 3) {
			printf("Error: insufficient data (got %d args, need at least 3)\n", numa);
			return;
		}
		/* FIXME: check for duplication by getting driver.port and device.serial
		 * for comparison with other busses results */ 
		/* FIXME:
		 * - also print answer[2] if != "Unavailable"?
		 * - for upsmon.conf or ups.conf (using dummy-ups)? */
		printf("\t%s@%s\n", answer[1], hostname);
	}
}

/* #ifdef nothing apart libupsclient! */ 
void scan_nut()
{
	printf("Scanning NUT bus (old connect method):\n");

	/* try on localhost first */
	list_nut_devices(NULL);

	/* FIXME: network range iterator IPv4 and IPv6*/

}

/* #ifdef nothing apart libupsclient! */ 
void scan_nut_avahi()
{
	printf("Scanning NUT bus (DNS-SD method):\n");

	/* Check avahi-browse code:
	 * http://git.0pointer.de/?p=avahi.git;a=tree;f=avahi-utils;h=5655a104964258e7be32ada78794f73beb84e0dd;hb=HEAD
	 *
	 * Example service publication (counterpart of the above):
	 * $ avahi-publish -s nut _upsd._tcp 3493 txtvers=1 protovers=1.0.0 type=standalone
	 */
}

int main()
{
#ifdef HAVE_USB_H
	scan_usb();
#endif /* HAVE_USB_H */

#if 0
#ifdef HAVE_NET_SNMP_NET_SNMP_CONFIG_H
	scan_snmp(range);
#endif /* HAVE_NET_SNMP_NET_SNMP_CONFIG_H */
#endif /* 0 */

	scan_xml_http();

	scan_nut();

	return EXIT_SUCCESS;
}

/* Parseable output
 *  bus_type: driver=driver,port=port,[,serial,vendorid,productid,mibs,...]
 *  USB: driver=usbhid-ups,port=auto,serial=XXX,vendorid=0x0463,productid=0xffff,
 *  SNMP: driver=snmp-ups,port=ip_address,mibs=...
 */

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
