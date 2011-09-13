/* scan_avahi.c: detect NUT avahi services
 * 
 *  Copyright (C) 2011 - Frederic Bohe <fredericbohe@eaton.com>
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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_AVAHI_CLIENT_CLIENT_H
#include "nut-scan.h"
#include "common.h"

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>

#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

static AvahiSimplePoll *simple_poll = NULL;
static nutscan_device_t * dev_ret = NULL;
static long avahi_usec_timeout = 0;

static void update_device(const char * host_name,const char *ip, uint16_t port,char * text, int proto)
{
	nutscan_device_t * dev = NULL;

	char * t = NULL;
	char * t_saveptr = NULL;
	char * phrase = NULL;
	char * phrase_saveptr = NULL;
	char * word = NULL;
	char * value = NULL;
	char * device = NULL;
	char * device_saveptr = NULL;
	int device_found = 0;
	char buf[6];
	int buf_size;

	if( text == NULL ) {
		return;
	}

	t = strdup(text);
	phrase = strtok_r(t,"\"",&t_saveptr);
	while(phrase != NULL ) {
		word = strtok_r(phrase,"=",&phrase_saveptr);
		if( word == NULL ) {
			phrase = strtok_r(NULL,"\"",&t_saveptr);
			continue;
		}
		value = strtok_r(NULL,"=",&phrase_saveptr);
		if( value == NULL ) {
			phrase = strtok_r(NULL,"\"",&t_saveptr);
			continue;
		}

		if( strcmp(word,"device_list") != 0 ) {
			phrase = strtok_r(NULL,"\"",&t_saveptr);
			continue;
		}

		device = strtok_r(value,";",&device_saveptr);
		while( device != NULL ) {
			device_found = 1;
			dev = nutscan_new_device();
			dev->type = TYPE_NUT;
			dev->driver = strdup("nutclient");
			if( proto == AVAHI_PROTO_INET) {
				nutscan_add_option_to_device(dev,"desc","IPv4");
			}
			if( proto == AVAHI_PROTO_INET6 ) {
				nutscan_add_option_to_device(dev,"desc","IPv6");
			}

			if( port != PORT) {
				/* +5+1+1+1 is for : 
				 - port number (max 65535 so 5 characters),
				 - '@' and ':' characters
				 - terminating 0 */
				buf_size = strlen(device)+strlen(host_name)+
						5+1+1+1;
				dev->port=malloc(buf_size);
				if(dev->port) {
					snprintf(dev->port,buf_size,"%s@%s:%u",
							device,host_name,port);
				}
			}
			else {
				/*+1+1 is for '@' character and terminating 0 */
				buf_size = strlen(device)+strlen(host_name)+1+1;
				dev->port=malloc(buf_size);
				if(dev->port) {
					snprintf(dev->port,buf_size,"%s@%s",
							device,host_name);
				}
			}
			if( dev->port ) {
				dev_ret = nutscan_add_device_to_device(dev_ret,dev);
			}
			else {
				nutscan_free_device(dev);
			}
			device = strtok_r(NULL,";",&device_saveptr);
		};

		phrase = strtok_r(NULL,"\"",&t_saveptr);
	};
	free(t);

	/* If no device published in avahi data, try to get the device by
	connecting directly to upsd */
	if( !device_found) {
		snprintf(buf,sizeof(buf),"%u",port);
		dev = nutscan_scan_nut(ip,ip,buf,avahi_usec_timeout);
		if(dev) {
			dev_ret = nutscan_add_device_to_device(dev_ret,dev);
		}
		/* add an upsd entry without associated device */
		else {
			dev = nutscan_new_device();
			dev->type = TYPE_NUT;
			dev->driver = strdup("nutclient");
			if( proto == AVAHI_PROTO_INET) {
				nutscan_add_option_to_device(dev,"desc","IPv4");
			}
			if( proto == AVAHI_PROTO_INET6 ) {
				nutscan_add_option_to_device(dev,"desc","IPv6");
			}
			if( port != PORT) {
				/*+1+1 is for ':' character and terminating 0 */
				/*buf is the string containing the port number*/
				buf_size = strlen(host_name)+strlen(buf)+1+1;
				dev->port=malloc(buf_size);
				if(dev->port) {
					snprintf(dev->port,buf_size,"%s:%s",
							host_name,buf);
				}
			}
			else {
				dev->port=strdup(host_name);
			}
			if( dev->port ) {
				dev_ret = nutscan_add_device_to_device(dev_ret,dev);
			}
			else {
				nutscan_free_device(dev);
			}
		}
	}
}

static void resolve_callback(
	AvahiServiceResolver *r,
	AVAHI_GCC_UNUSED AvahiIfIndex interface,
	AVAHI_GCC_UNUSED AvahiProtocol protocol,
	AvahiResolverEvent event,
	const char *name,
	const char *type,
	const char *domain,
	const char *host_name,
	const AvahiAddress *address,
	uint16_t port,
	AvahiStringList *txt,
	AvahiLookupResultFlags flags,
	AVAHI_GCC_UNUSED void* userdata) {

	assert(r);

	/* Called whenever a service has been resolved successfully or timed out */

	switch (event) {
		case AVAHI_RESOLVER_FAILURE:
			fprintf(stderr, "(Resolver) Failed to resolve service '%s' of type '%s' in domain '%s': %s\n", name, type, domain, avahi_strerror(avahi_client_errno(avahi_service_resolver_get_client(r))));
			break;

		case AVAHI_RESOLVER_FOUND: {
			char a[AVAHI_ADDRESS_STR_MAX], *t;

/*			fprintf(stderr, "Service '%s' of type '%s' in domain '%s':\n", name, type, domain); */

			avahi_address_snprint(a, sizeof(a), address);
			t = avahi_string_list_to_string(txt);
/*
			fprintf(stderr,
				"\t%s:%u (%s)\n"
				"\tTXT=%s\n"
				"\tcookie is %u\n"
				"\tis_local: %i\n"
				"\tour_own: %i\n"
				"\twide_area: %i\n"
				"\tmulticast: %i\n"
				"\tcached: %i\n",
				host_name, port, a,
				t,
				avahi_string_list_get_service_cookie(txt),
				!!(flags & AVAHI_LOOKUP_RESULT_LOCAL),
				!!(flags & AVAHI_LOOKUP_RESULT_OUR_OWN),
				!!(flags & AVAHI_LOOKUP_RESULT_WIDE_AREA),
				!!(flags & AVAHI_LOOKUP_RESULT_MULTICAST),
				!!(flags & AVAHI_LOOKUP_RESULT_CACHED));
*/
			update_device(host_name,a,port,t,address->proto);
			avahi_free(t);
		}
	}

	avahi_service_resolver_free(r);
}

static void browse_callback(
		AvahiServiceBrowser *b,
		AvahiIfIndex interface,
		AvahiProtocol protocol,
		AvahiBrowserEvent event,
		const char *name,
		const char *type,
		const char *domain,
		AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
		void* userdata) {

	AvahiClient *c = userdata;
	assert(b);

	/* Called whenever a new services becomes available on the LAN or is removed from the LAN */

	switch (event) {
		case AVAHI_BROWSER_FAILURE:

			fprintf(stderr, "(Browser) %s\n", avahi_strerror(avahi_client_errno(avahi_service_browser_get_client(b))));
			avahi_simple_poll_quit(simple_poll);
			return;

		case AVAHI_BROWSER_NEW:
/*			fprintf(stderr, "(Browser) NEW: service '%s' of type '%s' in domain '%s'\n", name, type, domain); */

			/* We ignore the returned resolver object. In the callback
			   function we free it. If the server is terminated before
			   the callback function is called the server will free
			   the resolver for us. */

			if (!(avahi_service_resolver_new(c, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, 0, resolve_callback, c)))
				fprintf(stderr, "Failed to resolve service '%s': %s\n", name, avahi_strerror(avahi_client_errno(c)));

			break;

		case AVAHI_BROWSER_REMOVE:
			fprintf(stderr, "(Browser) REMOVE: service '%s' of type '%s' in domain '%s'\n", name, type, domain);
			break;

		case AVAHI_BROWSER_ALL_FOR_NOW:
			avahi_simple_poll_quit(simple_poll);
		case AVAHI_BROWSER_CACHE_EXHAUSTED:
/*			fprintf(stderr, "(Browser) %s\n", event == AVAHI_BROWSER_CACHE_EXHAUSTED ? "CACHE_EXHAUSTED" : "ALL_FOR_NOW"); */
			break;
	}
}

static void client_callback(AvahiClient *c, AvahiClientState state, AVAHI_GCC_UNUSED void * userdata) {
	assert(c);

	/* Called whenever the client or server state changes */

	if (state == AVAHI_CLIENT_FAILURE) {
		fprintf(stderr, "Server connection failure: %s\n", avahi_strerror(avahi_client_errno(c)));
		avahi_simple_poll_quit(simple_poll);
	}
}

nutscan_device_t * nutscan_scan_avahi(long usec_timeout)
{
	/* Example service publication 
	 * $ avahi-publish -s nut _upsd._tcp 3493 txtvers=1 protovers=1.0.0 device_list="dev1;dev2"
	 */
	AvahiClient *client = NULL;
	AvahiServiceBrowser *sb = NULL;
	int error;
	int ret = 1;

	avahi_usec_timeout = usec_timeout;

	/* Allocate main loop object */
	if (!(simple_poll = avahi_simple_poll_new())) {
		fprintf(stderr, "Failed to create simple poll object.\n");
		goto fail;
	}

	/* Allocate a new client */
	client = avahi_client_new(avahi_simple_poll_get(simple_poll), 0, client_callback, NULL, &error);

	/* Check wether creating the client object succeeded */
	if (!client) {
		fprintf(stderr, "Failed to create client: %s\n", avahi_strerror(error));
		goto fail;
	}

	/* Create the service browser */
	if (!(sb = avahi_service_browser_new(client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, "_upsd._tcp", NULL, 0, browse_callback, client))) {
		fprintf(stderr, "Failed to create service browser: %s\n", avahi_strerror(avahi_client_errno(client)));
		goto fail;
	}

	/* Run the main loop */
	avahi_simple_poll_loop(simple_poll);

	ret = 0;

fail:

	/* Cleanup things */
	if (sb)
		avahi_service_browser_free(sb);

	if (client)
		avahi_client_free(client);

	if (simple_poll)
		avahi_simple_poll_free(simple_poll);

	return dev_ret;
}
#endif /* HAVE_AVAHI_CLIENT_CLIENT_H */
