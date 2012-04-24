/* scan_ipmi.c: detect NUT supported Power Supply Units
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
#include "common.h"
#include "nut-scan.h"

#ifdef WITH_IPMI
#include "upsclient.h"
#include <freeipmi/freeipmi.h>
#include <stdio.h>
#include <string.h>
#include <ltdl.h>

#define NUT_IPMI_DRV_NAME	"nut-ipmipsu"

/* dynamic link library stuff */
static char * libname = "libfreeipmi";
static lt_dlhandle dl_handle = NULL;
static const char *dl_error = NULL;

static int (*nut_ipmi_fru_parse_close_device_id) (ipmi_fru_parse_ctx_t ctx);
static void (*nut_ipmi_fru_parse_ctx_destroy) (ipmi_fru_parse_ctx_t ctx);
static void (*nut_ipmi_sdr_cache_ctx_destroy) (ipmi_sdr_cache_ctx_t ctx);
static void (*nut_ipmi_sdr_parse_ctx_destroy) (ipmi_sdr_parse_ctx_t ctx);
static ipmi_fru_parse_ctx_t (*nut_ipmi_fru_parse_ctx_create) (ipmi_ctx_t ipmi_ctx);
static int (*nut_ipmi_fru_parse_ctx_set_flags) (ipmi_fru_parse_ctx_t ctx, unsigned int flags);
static int (*nut_ipmi_fru_parse_open_device_id) (ipmi_fru_parse_ctx_t ctx, uint8_t fru_device_id);
static char * (*nut_ipmi_fru_parse_ctx_errormsg) (ipmi_fru_parse_ctx_t ctx);
static int (*nut_ipmi_fru_parse_read_data_area) (ipmi_fru_parse_ctx_t ctx,
                                   unsigned int *area_type,
                                   unsigned int *area_length,
                                   void *areabuf,
                                   unsigned int areabuflen);
static int (*nut_ipmi_fru_parse_next) (ipmi_fru_parse_ctx_t ctx);
static ipmi_ctx_t (*nut_ipmi_ctx_create) (void);
static int (*nut_ipmi_ctx_find_inband) (ipmi_ctx_t ctx,
                          ipmi_driver_type_t *driver_type,
                          int disable_auto_probe,
                          uint16_t driver_address,
                          uint8_t register_spacing,
                          const char *driver_device,
                          unsigned int workaround_flags,
                          unsigned int flags);
static char * (*nut_ipmi_ctx_errormsg) (ipmi_ctx_t ctx);
static int (*nut_ipmi_ctx_close) (ipmi_ctx_t ctx);
static void (*nut_ipmi_ctx_destroy) (ipmi_ctx_t ctx);


/* Return 0 on error */
int nutscan_load_ipmi_library()
{
	if( dl_handle != NULL ) {
		/* if previous init failed */
		if( dl_handle == (void *)1 ) {
			return 0;
		}
		/* init has already been done */
		return 1;
	}

	if( lt_dlinit() != 0 ) {
		fprintf(stderr, "Error initializing lt_init\n");
		return 0;
	}

	dl_handle = lt_dlopenext(libname);
	if (!dl_handle) {
		dl_error = lt_dlerror();
		goto err;
	}

	/* Clear any existing error */
	lt_dlerror();

	*(void **) (&nut_ipmi_fru_parse_close_device_id) = lt_dlsym(dl_handle, "ipmi_fru_parse_close_device_id");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_ipmi_fru_parse_ctx_destroy) = lt_dlsym(dl_handle, "ipmi_fru_parse_ctx_destroy");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_ipmi_sdr_cache_ctx_destroy) = lt_dlsym(dl_handle, "ipmi_sdr_cache_ctx_destroy");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_ipmi_sdr_parse_ctx_destroy) = lt_dlsym(dl_handle, "ipmi_sdr_parse_ctx_destroy");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_ipmi_fru_parse_ctx_create) = lt_dlsym(dl_handle, "ipmi_fru_parse_ctx_create");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_ipmi_fru_parse_ctx_set_flags) = lt_dlsym(dl_handle, "ipmi_fru_parse_ctx_set_flags");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_ipmi_fru_parse_open_device_id) = lt_dlsym(dl_handle, "ipmi_fru_parse_open_device_id");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_ipmi_fru_parse_ctx_errormsg) = lt_dlsym(dl_handle, "ipmi_fru_parse_ctx_errormsg");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_ipmi_fru_parse_read_data_area) = lt_dlsym(dl_handle, "ipmi_fru_parse_read_data_area");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_ipmi_fru_parse_next) = lt_dlsym(dl_handle, "ipmi_fru_parse_next");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_ipmi_ctx_create) = lt_dlsym(dl_handle, "ipmi_ctx_create");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_ipmi_ctx_find_inband) = lt_dlsym(dl_handle, "ipmi_ctx_find_inband");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_ipmi_ctx_errormsg) = lt_dlsym(dl_handle, "ipmi_ctx_errormsg");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_ipmi_ctx_close) = lt_dlsym(dl_handle, "ipmi_ctx_close");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	*(void **) (&nut_ipmi_ctx_destroy) = lt_dlsym(dl_handle, "ipmi_ctx_destroy");
	if ((dl_error = lt_dlerror()) != NULL)  {
			goto err;
	}

	return 1;
err:
        fprintf(stderr, "Cannot load IPMI library (%s) : %s. IPMI search disabled.\n", libname, dl_error);
	dl_handle = (void *)1;
	lt_dlexit();
	return 0;
}
/* end of dynamic link library stuff */

/* Cleanup IPMI contexts */
static void nut_freeipmi_cleanup(ipmi_fru_parse_ctx_t fru_parse_ctx,
								 ipmi_sdr_cache_ctx_t sdr_cache_ctx,
								 ipmi_sdr_parse_ctx_t sdr_parse_ctx)
{
	if (fru_parse_ctx) {
		(*nut_ipmi_fru_parse_close_device_id) (fru_parse_ctx);
		(*nut_ipmi_fru_parse_ctx_destroy) (fru_parse_ctx);
	}

	if (sdr_cache_ctx) {
		(*nut_ipmi_sdr_cache_ctx_destroy) (sdr_cache_ctx);
	}

	if (sdr_parse_ctx) {
		(*nut_ipmi_sdr_parse_ctx_destroy) (sdr_parse_ctx);
	}
}

/* Return 1 if supported, 0 otherwise */
int is_ipmi_device_supported(ipmi_ctx_t ipmi_ctx, int ipmi_id)
{
	int ret = -1;
	unsigned int area_type = 0;
	unsigned int area_length = 0;
	uint8_t areabuf[IPMI_FRU_PARSE_AREA_SIZE_MAX+1];
	ipmi_fru_parse_ctx_t fru_parse_ctx = NULL;
	ipmi_sdr_cache_ctx_t sdr_cache_ctx = NULL;
	ipmi_sdr_parse_ctx_t sdr_parse_ctx = NULL;

	/* Parse FRU information */
	if (!(fru_parse_ctx = (*nut_ipmi_fru_parse_ctx_create) (ipmi_ctx)))
	{
		fprintf(stderr, "ipmi_fru_parse_ctx_create()\n");
		return 0;
	}
	  
	/* lots of motherboards calculate checksums incorrectly */
	if ((*nut_ipmi_fru_parse_ctx_set_flags) (fru_parse_ctx, IPMI_FRU_PARSE_FLAGS_SKIP_CHECKSUM_CHECKS) < 0)
	{
		nut_freeipmi_cleanup(fru_parse_ctx, sdr_cache_ctx, sdr_parse_ctx);
		return 0;
	}

	if ((*nut_ipmi_fru_parse_open_device_id) (fru_parse_ctx, ipmi_id) < 0)
	{
		nut_freeipmi_cleanup(fru_parse_ctx, sdr_cache_ctx, sdr_parse_ctx);
		return 0;
	}

	do
	{
		/* clear fields */
		area_type = 0;
		area_length = 0;
		memset (areabuf, '\0', IPMI_FRU_PARSE_AREA_SIZE_MAX + 1);

		/* parse FRU buffer */
		if ((*nut_ipmi_fru_parse_read_data_area) (fru_parse_ctx,
											&area_type,
											&area_length,
											areabuf,
											IPMI_FRU_PARSE_AREA_SIZE_MAX) < 0)
		{
			nut_freeipmi_cleanup(fru_parse_ctx, sdr_cache_ctx, sdr_parse_ctx);
			return 0;
		}

		if (area_length)
		{
			if (area_type == IPMI_FRU_PARSE_AREA_TYPE_MULTIRECORD_POWER_SUPPLY_INFORMATION)
			{
				/* Found a POWER_SUPPLY record */
				nut_freeipmi_cleanup(fru_parse_ctx, sdr_cache_ctx, sdr_parse_ctx);
				return 1;
			}
		}
	} while ((ret = (*nut_ipmi_fru_parse_next) (fru_parse_ctx)) == 1);

	/* No need for further errors checking */
	nut_freeipmi_cleanup(fru_parse_ctx, sdr_cache_ctx, sdr_parse_ctx);
	return 0;
}

/* return NULL on error */
nutscan_device_t *  nutscan_scan_ipmi()
{
	ipmi_ctx_t ipmi_ctx = NULL;
	nutscan_device_t * nut_dev = NULL;
	nutscan_device_t * current_nut_dev = NULL;
	int ret = -1;
	int ipmi_id = 0;
	char port_id[10];

	if( !nutscan_avail_ipmi ) {
		return NULL;
	}

	/* Initialize the FreeIPMI library. */
	if (!(ipmi_ctx = (*nut_ipmi_ctx_create) ()))
	{
		/* we have to force cleanup, since exit handler is not yet installed */
		fprintf(stderr, "ipmi_ctx_create\n");
		return NULL;
	}

	if ((ret = (*nut_ipmi_ctx_find_inband) (ipmi_ctx,
				NULL,
				0, /* don't disable auto-probe */
				0,
				0,
				NULL,
				0, /* workaround flags, none by default */
				0  /* flags */
				)) < 0)
	{
		fprintf(stderr, "ipmi_ctx_find_inband: %s\n",
			(*nut_ipmi_ctx_errormsg) (ipmi_ctx));
		return NULL;
	}
	if (!ret)
	{
		/* No local IPMI device detected */
		return NULL;
	}

	/* Loop through all possible devices */
	for (ipmi_id = 0 ; ipmi_id <= IPMI_FRU_DEVICE_ID_MAX ; ipmi_id++) {

		if (is_ipmi_device_supported(ipmi_ctx, ipmi_id)) {

			if ( (nut_dev = nutscan_new_device()) == NULL ) {
				fprintf(stderr,"Memory allocation error\n");
				nutscan_free_device(current_nut_dev);
				break;
			}

			/* Fill the device structure (sufficient with driver and port) */
			nut_dev->type = TYPE_IPMI;
			nut_dev->driver = strdup(NUT_IPMI_DRV_NAME);
			sprintf(port_id, "id%x", ipmi_id);
			nut_dev->port = strdup(port_id);

			current_nut_dev = nutscan_add_device_to_device(
							current_nut_dev,
							nut_dev);

			memset (port_id, 0, sizeof(port_id));
		}
	}

	/* Final cleanup */
	if (ipmi_ctx) {
		(*nut_ipmi_ctx_close) (ipmi_ctx);
		(*nut_ipmi_ctx_destroy) (ipmi_ctx);
	}

	return current_nut_dev;
}
#else /* WITH_IPMI */
/* stub function */
nutscan_device_t *  nutscan_scan_ipmi()
{
	return NULL;
}
#endif /* WITH_IPMI */
