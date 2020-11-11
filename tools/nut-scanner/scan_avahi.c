/*
 *  Copyright (C) 2011 - EATON
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

/*! \file scan_avahi.c
    \brief detect NUT through Avahi mDNS / DNS-SD services
    \author Frederic Bohe <fredericbohe@eaton.com>
*/

#include "common.h"
#include "nut-scan.h"

#ifdef WITH_AVAHI

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "timehead.h"

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>

#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

#include <ltdl.h>

/* dynamic link library stuff */
static lt_dlhandle dl_handle = NULL;
static const char *dl_error = NULL;

static AvahiClient* (*nut_avahi_service_browser_get_client)(AvahiServiceBrowser *);
static int (*nut_avahi_simple_poll_loop)(AvahiSimplePoll *s);
static void (*nut_avahi_client_free)(AvahiClient *client);
static int (*nut_avahi_client_errno)(AvahiClient*);
static void (*nut_avahi_free)(void *p);
static void (*nut_avahi_simple_poll_quit)(AvahiSimplePoll *s);
static AvahiClient* (*nut_avahi_client_new)(
		const AvahiPoll *poll_api,
		AvahiClientFlags flags,
		AvahiClientCallback callback,
		void *userdata,
		int *error);
static void (*nut_avahi_simple_poll_free)(AvahiSimplePoll *s);
static AvahiServiceResolver * (*nut_avahi_service_resolver_new)(
		AvahiClient *client,
		AvahiIfIndex interface,
		AvahiProtocol protocol,
		const char *name,
		const char *type,
		const char *domain,
		AvahiProtocol aprotocol,
		AvahiLookupFlags flags,
		AvahiServiceResolverCallback callback,
		void *userdata);
static const char * (*nut_avahi_strerror)(int error);
static AvahiClient* (*nut_avahi_service_resolver_get_client)(AvahiServiceResolver *);
static AvahiServiceBrowser* (*nut_avahi_service_browser_new)(
		AvahiClient *client,
		AvahiIfIndex interface,
		AvahiProtocol protocol,
		const char *type,
		const char *domain,
		AvahiLookupFlags flags,
		AvahiServiceBrowserCallback callback,
		void *userdata);
static int (*nut_avahi_service_resolver_free)(AvahiServiceResolver *r);
static AvahiSimplePoll *(*nut_avahi_simple_poll_new)(void);
static char* (*nut_avahi_string_list_to_string)(AvahiStringList *l);
static int (*nut_avahi_service_browser_free)(AvahiServiceBrowser *);
static char * (*nut_avahi_address_snprint)(char *ret_s, size_t length, const AvahiAddress *a);
static const AvahiPoll* (*nut_avahi_simple_poll_get)(AvahiSimplePoll *s);

/* return 0 on error */
int nutscan_load_avahi_library(const char *libname_path)
{
	if( dl_handle != NULL ) {
		/* if previous init failed */
		if( dl_handle == (void *)1 ) {
				return 0;
		}
		/* init has already been done */
		return 1;
	}

	if (libname_path == NULL) {
		fprintf(stderr, "AVAHI client library not found. AVAHI search disabled.\n");
		return 0;
	}

	if( lt_dlinit() != 0 ) {
		fprintf(stderr, "Error initializing lt_init\n");
		return 0;
	}

	dl_handle = lt_dlopen(libname_path);
	if (!dl_handle) {
		dl_error = lt_dlerror();
		goto err;
	}
	lt_dlerror();      /* Clear any existing error */
	*(void **) (&nut_avahi_service_browser_get_client) = lt_dlsym(dl_handle, "avahi_service_browser_get_client");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_avahi_simple_poll_loop) = lt_dlsym(dl_handle, "avahi_simple_poll_loop");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_avahi_client_free) = lt_dlsym(dl_handle, "avahi_client_free");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_avahi_client_errno) = lt_dlsym(dl_handle, "avahi_client_errno");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_avahi_free) = lt_dlsym(dl_handle, "avahi_free");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_avahi_simple_poll_quit) = lt_dlsym(dl_handle, "avahi_simple_poll_quit");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_avahi_client_new) = lt_dlsym(dl_handle, "avahi_client_new");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_avahi_simple_poll_free) = lt_dlsym(dl_handle, "avahi_simple_poll_free");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_avahi_service_resolver_new) = lt_dlsym(dl_handle, "avahi_service_resolver_new");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_avahi_strerror) = lt_dlsym(dl_handle, "avahi_strerror");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_avahi_service_resolver_get_client) = lt_dlsym(dl_handle, "avahi_service_resolver_get_client");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_avahi_service_browser_new) = lt_dlsym(dl_handle, "avahi_service_browser_new");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_avahi_service_resolver_free) = lt_dlsym(dl_handle, "avahi_service_resolver_free");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_avahi_simple_poll_new) = lt_dlsym(dl_handle, "avahi_simple_poll_new");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_avahi_string_list_to_string) = lt_dlsym(dl_handle, "avahi_string_list_to_string");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_avahi_service_browser_free) = lt_dlsym(dl_handle, "avahi_service_browser_free");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_avahi_address_snprint) = lt_dlsym(dl_handle, "avahi_address_snprint");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	*(void **) (&nut_avahi_simple_poll_get) = lt_dlsym(dl_handle, "avahi_simple_poll_get");
	if ((dl_error = lt_dlerror()) != NULL)  {
		goto err;
	}

	return 1;
err:
	fprintf(stderr, "Cannot load AVAHI library (%s) : %s. AVAHI search disabled.\n", libname_path, dl_error);
	dl_handle = (void *)1;
	lt_dlexit();
	return 0;
}
/* end of dynamic link library stuff */

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
	AvahiIfIndex interface,
	AvahiProtocol protocol,
	AvahiResolverEvent event,
	const char *name,
	const char *type,
	const char *domain,
	const char *host_name,
	const AvahiAddress *address,
	uint16_t port,
	AvahiStringList *txt,
	AvahiLookupResultFlags flags,
	void* userdata)
{

	assert(r);

	NUT_UNUSED_VARIABLE(interface);
	NUT_UNUSED_VARIABLE(protocol);
	NUT_UNUSED_VARIABLE(userdata);

	/* Called whenever a service has been resolved successfully or timed out */

	switch (event) {
		case AVAHI_RESOLVER_FAILURE:
			fprintf(stderr, "(Resolver) Failed to resolve service '%s' of type '%s' in domain '%s': %s\n", name, type, domain, (*nut_avahi_strerror)((*nut_avahi_client_errno)((*nut_avahi_service_resolver_get_client)(r))));
			break;

		case AVAHI_RESOLVER_FOUND: {
			char a[AVAHI_ADDRESS_STR_MAX], *t;

/*			fprintf(stderr, "Service '%s' of type '%s' in domain '%s':\n", name, type, domain); */

			(*nut_avahi_address_snprint)(a, sizeof(a), address);
			t = (*nut_avahi_string_list_to_string)(txt);

			NUT_UNUSED_VARIABLE(flags);
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
			(*nut_avahi_free)(t);
		}
	}

	(*nut_avahi_service_resolver_free)(r);
}

static void browse_callback(
		AvahiServiceBrowser *b,
		AvahiIfIndex interface,
		AvahiProtocol protocol,
		AvahiBrowserEvent event,
		const char *name,
		const char *type,
		const char *domain,
		AvahiLookupResultFlags flags,
		void* userdata)
{

	AvahiClient *c = userdata;
	assert(b);

	NUT_UNUSED_VARIABLE(flags);

	/* Called whenever a new services becomes available on the LAN or is removed from the LAN */

	switch (event) {
		case AVAHI_BROWSER_FAILURE:

			fprintf(stderr, "(Browser) %s\n", (*nut_avahi_strerror)((*nut_avahi_client_errno)((*nut_avahi_service_browser_get_client)(b))));
			(*nut_avahi_simple_poll_quit)(simple_poll);
			return;

		case AVAHI_BROWSER_NEW:
/*			fprintf(stderr, "(Browser) NEW: service '%s' of type '%s' in domain '%s'\n", name, type, domain); */

			/* We ignore the returned resolver object. In the callback
			   function we free it. If the server is terminated before
			   the callback function is called the server will free
			   the resolver for us. */

			if (!((*nut_avahi_service_resolver_new)(c, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, 0, resolve_callback, c)))
				fprintf(stderr, "Failed to resolve service '%s': %s\n", name, (*nut_avahi_strerror)((*nut_avahi_client_errno)(c)));

			break;

		case AVAHI_BROWSER_REMOVE:
			fprintf(stderr, "(Browser) REMOVE: service '%s' of type '%s' in domain '%s'\n", name, type, domain);
			break;

		case AVAHI_BROWSER_ALL_FOR_NOW:
			(*nut_avahi_simple_poll_quit)(simple_poll);
		case AVAHI_BROWSER_CACHE_EXHAUSTED:
/*			fprintf(stderr, "(Browser) %s\n", event == AVAHI_BROWSER_CACHE_EXHAUSTED ? "CACHE_EXHAUSTED" : "ALL_FOR_NOW"); */
			break;
	}
}

static void client_callback(AvahiClient *c, AvahiClientState state, void * userdata) {
	assert(c);
	NUT_UNUSED_VARIABLE(userdata);

	/* Called whenever the client or server state changes */

	if (state == AVAHI_CLIENT_FAILURE) {
		fprintf(stderr, "Server connection failure: %s\n", (*nut_avahi_strerror)((*nut_avahi_client_errno)(c)));
		(*nut_avahi_simple_poll_quit)(simple_poll);
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

	if( !nutscan_avail_avahi ) {
		return NULL;
	}

	avahi_usec_timeout = usec_timeout;

	/* Allocate main loop object */
	if (!(simple_poll = (*nut_avahi_simple_poll_new)())) {
		fprintf(stderr, "Failed to create simple poll object.\n");
		goto fail;
	}

	/* Allocate a new client */
	client = (*nut_avahi_client_new)((*nut_avahi_simple_poll_get)(simple_poll), 0, client_callback, NULL, &error);

	/* Check wether creating the client object succeeded */
	if (!client) {
		fprintf(stderr, "Failed to create client: %s\n", (*nut_avahi_strerror)(error));
		goto fail;
	}

	/* Create the service browser */
	if (!(sb = (*nut_avahi_service_browser_new)(client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, "_upsd._tcp", NULL, 0, browse_callback, client))) {
		fprintf(stderr, "Failed to create service browser: %s\n", (*nut_avahi_strerror)((*nut_avahi_client_errno)(client)));
		goto fail;
	}

	/* Run the main loop */
	(*nut_avahi_simple_poll_loop)(simple_poll);

fail:

	/* Cleanup things */
	if (sb)
		(*nut_avahi_service_browser_free)(sb);

	if (client)
		(*nut_avahi_client_free)(client);

	if (simple_poll)
		(*nut_avahi_simple_poll_free)(simple_poll);

	return nutscan_rewind_device(dev_ret);
}
#else  /* WITH_AVAHI */
/* stub function */
nutscan_device_t * nutscan_scan_avahi(long usec_timeout)
{
	return NULL;
}
#endif /* WITH_AVAHI */
