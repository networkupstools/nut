/*  nut-libfreeipmi.c - NUT IPMI backend, using FreeIPMI
 *
 *  Copyright (C)
 *    2011 - 2012  Arnaud Quette <arnaud.quette@free.fr>
 *    2011 - Albert Chu <chu11@llnl.gov>
 *
 *  Based on the sample codes 'ipmi-fru-example.c', 'frulib.c' and
 *    'ipmimonitoring-sensors.c', from FreeIPMI
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
 *
 TODO:
 * add power control support (ipmipower): seems OOB only!
  -n, --on                   Power on the target hosts.
  -f, --off                  Power off the target hosts.
  -c, --cycle                Power cycle the target hosts.
  -r, --reset                Reset the target hosts.
  -s, --stat                 Get power status of the target hosts.
      --pulse                Send power diagnostic interrupt to target hosts.
      --soft                 Initiate a soft-shutdown of the OS via ACPI.
      --on-if-off            Issue a power on command instead of a power cycle
                             or hard reset command if the remote machine's
                             power is currently off.
      --wait-until-off       Regularly query the remote BMC and return only
                             after the machine has powered off.
      --wait-until-on        Regularly query the remote BMC and return only

 */

#include <stdlib.h>
#include <string.h>
#include "timehead.h"
#include "common.h"
#include <freeipmi/freeipmi.h>
#include <ipmi_monitoring.h>
#if HAVE_FREEIPMI_MONITORING
#include <ipmi_monitoring_bitmasks.h>
#endif
#include "nut-ipmi.h"
#include "dstate.h"

/* FreeIPMI defines */
#define IPMI_FRU_STR_BUFLEN    1024
/* haven't seen a motherboard with more than 2-3 so far,
 * 64 should be more than enough */
#define IPMI_FRU_CUSTOM_FIELDS 64

/* FreeIPMI contexts and configuration*/
static ipmi_ctx_t ipmi_ctx = NULL;
static ipmi_monitoring_ctx_t mon_ctx = NULL;
static struct ipmi_monitoring_ipmi_config ipmi_config;

/* SDR management API has changed with 1.1.X and later */
#ifdef HAVE_FREEIPMI_11X_12X
  static ipmi_sdr_ctx_t sdr_ctx = NULL;
  static ipmi_fru_ctx_t fru_ctx = NULL;
  #define SDR_PARSE_CTX sdr_ctx
  #define NUT_IPMI_SDR_CACHE_DEFAULTS                              IPMI_SDR_CACHE_CREATE_FLAGS_DEFAULT
#else
  /* NOTE: Maybe declare the vars in lines below also as static? */
  static ipmi_sdr_cache_ctx_t sdr_ctx = NULL;
  ipmi_sdr_parse_ctx_t sdr_parse_ctx = NULL;
  #define SDR_PARSE_CTX sdr_parse_ctx
  ipmi_fru_parse_ctx_t fru_ctx = NULL;
  /* Functions remapping */
  #define ipmi_sdr_ctx_create                           ipmi_sdr_cache_ctx_create
  #define ipmi_sdr_ctx_destroy                          ipmi_sdr_cache_ctx_destroy
  #define ipmi_sdr_ctx_errnum                           ipmi_sdr_cache_ctx_errnum
  #define ipmi_sdr_ctx_errormsg                         ipmi_sdr_cache_ctx_errormsg
  #define ipmi_fru_ctx_create                           ipmi_fru_parse_ctx_create
  #define ipmi_fru_ctx_destroy                          ipmi_fru_parse_ctx_destroy
  #define ipmi_fru_ctx_set_flags                        ipmi_fru_parse_ctx_set_flags
  #define ipmi_fru_ctx_strerror                         ipmi_fru_parse_ctx_strerror
  #define ipmi_fru_ctx_errnum                           ipmi_fru_parse_ctx_errnum
  #define ipmi_fru_open_device_id                       ipmi_fru_parse_open_device_id
  #define ipmi_fru_close_device_id                      ipmi_fru_parse_close_device_id
  #define ipmi_fru_ctx_errormsg                         ipmi_fru_parse_ctx_errormsg
  #define ipmi_fru_read_data_area                       ipmi_fru_parse_read_data_area
  #define ipmi_fru_next                                 ipmi_fru_parse_next
  #define ipmi_fru_type_length_field_to_string          ipmi_fru_parse_type_length_field_to_string
  #define ipmi_fru_multirecord_power_supply_information ipmi_fru_parse_multirecord_power_supply_information
  #define ipmi_fru_board_info_area                      ipmi_fru_parse_board_info_area
  #define ipmi_fru_field_t                              ipmi_fru_parse_field_t
  /* Constants */
  #define IPMI_SDR_MAX_RECORD_LENGTH                               IPMI_SDR_CACHE_MAX_SDR_RECORD_LENGTH
  #define IPMI_SDR_ERR_CACHE_READ_CACHE_DOES_NOT_EXIST             IPMI_SDR_CACHE_ERR_CACHE_READ_CACHE_DOES_NOT_EXIST
  #define IPMI_FRU_AREA_SIZE_MAX                                   IPMI_FRU_PARSE_AREA_SIZE_MAX
  #define IPMI_FRU_FLAGS_SKIP_CHECKSUM_CHECKS                      IPMI_FRU_PARSE_FLAGS_SKIP_CHECKSUM_CHECKS
  #define IPMI_FRU_AREA_TYPE_BOARD_INFO_AREA                       IPMI_FRU_PARSE_AREA_TYPE_BOARD_INFO_AREA
  #define IPMI_FRU_AREA_TYPE_MULTIRECORD_POWER_SUPPLY_INFORMATION  IPMI_FRU_PARSE_AREA_TYPE_MULTIRECORD_POWER_SUPPLY_INFORMATION
  #define IPMI_FRU_AREA_STRING_MAX                                 IPMI_FRU_PARSE_AREA_STRING_MAX
  #define NUT_IPMI_SDR_CACHE_DEFAULTS                              IPMI_SDR_CACHE_CREATE_FLAGS_DEFAULT, IPMI_SDR_CACHE_VALIDATION_FLAGS_DEFAULT
#endif /* HAVE_FREEIPMI_11X_12X */

/* FIXME: freeipmi auto selects a cache based on the hostname you are
 * connecting too, but this is probably fine for you
 */
#define CACHE_LOCATION "/tmp/sdrcache"

/* Support functions */
static const char* libfreeipmi_getfield (uint8_t language_code,
	ipmi_fru_field_t *field);

static void libfreeipmi_cleanup(void);

static int libfreeipmi_get_psu_info (const void *areabuf,
	uint8_t area_length, IPMIDevice_t *ipmi_dev);

static int libfreeipmi_get_board_info (const void *areabuf,
	uint8_t area_length, IPMIDevice_t *ipmi_dev);

static int libfreeipmi_get_sensors_info (IPMIDevice_t *ipmi_dev);


/*******************************************************************************
 * Implementation
 ******************************************************************************/
int nut_ipmi_open(int ipmi_id, IPMIDevice_t *ipmi_dev)
{
	int ret = -1;
	uint8_t areabuf[IPMI_FRU_AREA_SIZE_MAX+1];
	unsigned int area_type = 0;
	unsigned int area_length = 0;

	upsdebugx(1, "nut-libfreeipmi: nutipmi_open()...");

	/* Initialize the FreeIPMI library. */
	if (!(ipmi_ctx = ipmi_ctx_create ()))
	{
		/* we have to force cleanup, since exit handler is not yet installed */
		libfreeipmi_cleanup();
		fatal_with_errno(EXIT_FAILURE, "ipmi_ctx_create");
	}

	if ((ret = ipmi_ctx_find_inband (ipmi_ctx,
				NULL,
				0, /* don't disable auto-probe */
				0,
				0,
				NULL,
				0, /* workaround flags, none by default */
				0  /* flags */
				)) < 0)
	{
		libfreeipmi_cleanup();
		fatalx(EXIT_FAILURE, "ipmi_ctx_find_inband: %s",
			ipmi_ctx_errormsg (ipmi_ctx));
	}
	if (!ret)
	{
		libfreeipmi_cleanup();
		fatalx(EXIT_FAILURE, "could not find inband device");
	}

	upsdebugx(1, "FreeIPMI initialized...");

	/* Parse FRU information */
	if (!(fru_ctx = ipmi_fru_ctx_create (ipmi_ctx)))
	{
		libfreeipmi_cleanup();
		fatal_with_errno(EXIT_FAILURE, "ipmi_fru_ctx_create()");
	}

	/* lots of motherboards calculate checksums incorrectly */
	if (ipmi_fru_ctx_set_flags (fru_ctx, IPMI_FRU_FLAGS_SKIP_CHECKSUM_CHECKS) < 0)
	{
		libfreeipmi_cleanup();
		fatalx(EXIT_FAILURE, "ipmi_fru_ctx_set_flags: %s\n",
			ipmi_fru_ctx_strerror (ipmi_fru_ctx_errnum (fru_ctx)));
	}

	/* Now open the requested (local) PSU */
	if (ipmi_fru_open_device_id (fru_ctx, ipmi_id) < 0)
	{
		libfreeipmi_cleanup();
		fatalx(EXIT_FAILURE, "ipmi_fru_open_device_id: %s\n",
			ipmi_fru_ctx_errormsg (fru_ctx));
	}

	/* Set IPMI identifier */
	ipmi_dev->ipmi_id = ipmi_id;

	do
	{
		/* clear fields */
		area_type = 0;
		area_length = 0;
		memset (areabuf, '\0', IPMI_FRU_AREA_SIZE_MAX + 1);

		/* parse FRU buffer */
		if (ipmi_fru_read_data_area (fru_ctx,
											&area_type,
											&area_length,
											areabuf,
											IPMI_FRU_AREA_SIZE_MAX) < 0)
		{
			libfreeipmi_cleanup();
			fatal_with_errno(EXIT_FAILURE,
				"ipmi_fru_read_data_area: %s\n",
				ipmi_fru_ctx_errormsg (fru_ctx));
		}

		if (area_length)
		{
			switch (area_type)
			{
				/* get generic board information */
				case IPMI_FRU_AREA_TYPE_BOARD_INFO_AREA:

					if(libfreeipmi_get_board_info (areabuf, area_length,
						ipmi_dev) < 0)
					{
						upsdebugx(1, "Can't retrieve board information");
					}
					break;
				/* get specific PSU information */
				case IPMI_FRU_AREA_TYPE_MULTIRECORD_POWER_SUPPLY_INFORMATION:

					if(libfreeipmi_get_psu_info (areabuf, area_length, ipmi_dev) < 0)
					{
						upsdebugx(1, "Can't retrieve PSU information");
					}
					break;
				default:
					upsdebugx (5, "FRU: discarding FRU Area Type Read: %02Xh", area_type);
					break;
			}
		}
	} while ((ret = ipmi_fru_next (fru_ctx)) == 1);

	/* check for errors */
	if (ret < 0) {
		libfreeipmi_cleanup();
		fatal_with_errno(EXIT_FAILURE, "ipmi_fru_next: %s",
			ipmi_fru_ctx_errormsg (fru_ctx));
	}
	else {
		/* Get all related sensors information */
		libfreeipmi_get_sensors_info (ipmi_dev);
	}

	/* cleanup context */
	libfreeipmi_cleanup();

	return (0);
}

void nut_ipmi_close(void)
{
	upsdebugx(1, "nutipmi_close...");

	libfreeipmi_cleanup();
}

static const char* libfreeipmi_getfield (uint8_t language_code,
									ipmi_fru_field_t *field)
{
	static char strbuf[IPMI_FRU_AREA_STRING_MAX + 1];
	unsigned int strbuflen = IPMI_FRU_AREA_STRING_MAX;

	if (!field->type_length_field_length)
		return NULL;

	memset (strbuf, '\0', IPMI_FRU_AREA_STRING_MAX + 1);

	if (ipmi_fru_type_length_field_to_string (fru_ctx,
													field->type_length_field,
													field->type_length_field_length,
													language_code,
													strbuf,
													&strbuflen) < 0)
		{
			upsdebugx (2, "ipmi_fru_type_length_field_to_string: %s",
				ipmi_fru_ctx_errormsg (fru_ctx));
			return NULL;
		}

	if (strbuflen)
		return strbuf;

  return NULL;
}

/* Get voltage value from the IPMI voltage code */
static float libfreeipmi_get_voltage (uint8_t voltage_code)
{
  if (voltage_code == IPMI_FRU_VOLTAGE_12V)
    return 12;
  else if (voltage_code == IPMI_FRU_VOLTAGE_MINUS12V)
    return -12;
  else if (voltage_code == IPMI_FRU_VOLTAGE_5V)
    return 5;
  else if (voltage_code == IPMI_FRU_VOLTAGE_3_3V)
    return 3.3;
  else
    return 0;
}

/* Cleanup IPMI contexts */
static void libfreeipmi_cleanup()
{
	/* cleanup */
	if (fru_ctx) {
		ipmi_fru_close_device_id (fru_ctx);
		ipmi_fru_ctx_destroy (fru_ctx);
	}

	if (sdr_ctx) {
		ipmi_sdr_ctx_destroy (sdr_ctx);
	}

#ifndef HAVE_FREEIPMI_11X_12X
	if (sdr_parse_ctx) {
		ipmi_sdr_parse_ctx_destroy (sdr_parse_ctx);
	}
#endif

	if (ipmi_ctx) {
		ipmi_ctx_close (ipmi_ctx);
		ipmi_ctx_destroy (ipmi_ctx);
	}

	if (mon_ctx) {
		ipmi_monitoring_ctx_destroy (mon_ctx);
	}
}

/* Get generic board information (manufacturer and model names, serial, ...)
 * from IPMI FRU */
static int libfreeipmi_get_psu_info (const void *areabuf,
										uint8_t area_length,
										IPMIDevice_t *ipmi_dev)
{
	/* FIXME: directly use ipmi_dev fields */
	unsigned int overall_capacity;
	int low_end_input_voltage_range_1;
	int high_end_input_voltage_range_1;
	unsigned int low_end_input_frequency_range;
	unsigned int high_end_input_frequency_range;
	unsigned int voltage_1;

	/* FIXME: check for the interest and capability to use these data */
	unsigned int peak_va;
	unsigned int inrush_current;
	unsigned int inrush_interval;
	int low_end_input_voltage_range_2;
	int high_end_input_voltage_range_2;
	unsigned int ac_dropout_tolerance;
	unsigned int predictive_fail_support;
	unsigned int power_factor_correction;
	unsigned int autoswitch;
	unsigned int hot_swap_support;
	unsigned int tachometer_pulses_per_rotation_predictive_fail_polarity;
	unsigned int peak_capacity;
	unsigned int hold_up_time;
	unsigned int voltage_2;
	unsigned int total_combined_wattage;
	unsigned int predictive_fail_tachometer_lower_threshold;

	upsdebugx(1, "entering libfreeipmi_get_psu_info()");

	if (ipmi_fru_multirecord_power_supply_information (fru_ctx,
			areabuf,
			area_length,
			&overall_capacity,
			&peak_va,
			&inrush_current,
			&inrush_interval,
			&low_end_input_voltage_range_1,
			&high_end_input_voltage_range_1,
			&low_end_input_voltage_range_2,
			&high_end_input_voltage_range_2,
			&low_end_input_frequency_range,
			&high_end_input_frequency_range,
			&ac_dropout_tolerance,
			&predictive_fail_support,
			&power_factor_correction,
			&autoswitch,
			&hot_swap_support,
			&tachometer_pulses_per_rotation_predictive_fail_polarity,
			&peak_capacity,
			&hold_up_time,
			&voltage_1,
			&voltage_2,
			&total_combined_wattage,
			&predictive_fail_tachometer_lower_threshold) < 0)
	{
		fatalx(EXIT_FAILURE, "ipmi_fru_multirecord_power_supply_information: %s",
			ipmi_fru_ctx_errormsg (fru_ctx));
	}

	ipmi_dev->overall_capacity = overall_capacity;

	/* Voltages are in mV! */
	ipmi_dev->input_minvoltage = low_end_input_voltage_range_1 / 1000;
	ipmi_dev->input_maxvoltage = high_end_input_voltage_range_1 / 1000;

	ipmi_dev->input_minfreq = low_end_input_frequency_range;
	ipmi_dev->input_maxfreq = high_end_input_frequency_range;

	ipmi_dev->voltage = libfreeipmi_get_voltage(voltage_1);

	upsdebugx(1, "libfreeipmi_get_psu_info() retrieved successfully");

	return (0);
}

/* Get specific PSU information from IPMI FRU */
static int libfreeipmi_get_board_info (const void *areabuf,
	uint8_t area_length, IPMIDevice_t *ipmi_dev)
{
	uint8_t language_code;
	uint32_t mfg_date_time;
	ipmi_fru_field_t board_manufacturer;
	ipmi_fru_field_t board_product_name;
	ipmi_fru_field_t board_serial_number;
	ipmi_fru_field_t board_part_number;
	ipmi_fru_field_t board_fru_file_id;
	ipmi_fru_field_t board_custom_fields[IPMI_FRU_CUSTOM_FIELDS];
	const char *string = NULL;
	time_t timetmp;
	struct tm mfg_date_time_tm;
	char mfg_date_time_buf[IPMI_FRU_STR_BUFLEN + 1];

	upsdebugx(1, "entering libfreeipmi_get_board_info()");

	/* clear fields */
	memset (&board_manufacturer, '\0', sizeof (ipmi_fru_field_t));
	memset (&board_product_name, '\0', sizeof (ipmi_fru_field_t));
	memset (&board_serial_number, '\0', sizeof (ipmi_fru_field_t));
	memset (&board_fru_file_id, '\0', sizeof (ipmi_fru_field_t));
	memset (&board_custom_fields[0], '\0',
			sizeof (ipmi_fru_field_t) * IPMI_FRU_CUSTOM_FIELDS);

	/* parse FRU buffer */
	if (ipmi_fru_board_info_area (fru_ctx,
			areabuf,
			area_length,
			&language_code,
			&mfg_date_time,
			&board_manufacturer,
			&board_product_name,
			&board_serial_number,
			&board_part_number,
			&board_fru_file_id,
			board_custom_fields,
			IPMI_FRU_CUSTOM_FIELDS) < 0)
	{
		libfreeipmi_cleanup();
		fatalx(EXIT_FAILURE, "ipmi_fru_board_info_area: %s",
			ipmi_fru_ctx_errormsg (fru_ctx));
	}


	if (IPMI_FRU_LANGUAGE_CODE_VALID (language_code)) {
		upsdebugx (5, "FRU Board Language: %s", ipmi_fru_language_codes[language_code]);
	}
	else {
		upsdebugx (5, "FRU Board Language Code: %02Xh", language_code);
	}

	/* Posix says individual calls need not clear/set all portions of
	 * 'struct tm', thus passing 'struct tm' between functions could
	 * have issues.  So we need to memset */
	memset (&mfg_date_time_tm, '\0', sizeof (struct tm));
	timetmp = mfg_date_time;
	localtime_r (&timetmp, &mfg_date_time_tm);
	memset (mfg_date_time_buf, '\0', IPMI_FRU_STR_BUFLEN + 1);
	strftime (mfg_date_time_buf, IPMI_FRU_STR_BUFLEN, "%D - %T", &mfg_date_time_tm);

	/* Store values */
	ipmi_dev->date = xstrdup(mfg_date_time_buf);
	upsdebugx(2, "FRU Board Manufacturing Date/Time: %s", ipmi_dev->date);

	if ((string = libfreeipmi_getfield (language_code, &board_manufacturer)) != NULL)
		ipmi_dev->manufacturer = xstrdup(string);
	else
		ipmi_dev->manufacturer = xstrdup("Generic IPMI manufacturer");

	if ((string = libfreeipmi_getfield (language_code, &board_product_name)) != NULL)
		ipmi_dev->product = xstrdup(string);
	else
		ipmi_dev->product = xstrdup("Generic PSU");

	if ((string = libfreeipmi_getfield (language_code, &board_serial_number)) != NULL)
		ipmi_dev->serial = xstrdup(string);
	else
		ipmi_dev->serial = NULL;

	if ((string = libfreeipmi_getfield (language_code, &board_part_number)) != NULL)
		ipmi_dev->part = xstrdup(string);
	else
		ipmi_dev->part = NULL;

	return (0);
}


/* Get the sensors list & values, specific to the given FRU ID
 * Return -1 on error, or the number of sensors found otherwise */
static int libfreeipmi_get_sensors_info (IPMIDevice_t *ipmi_dev)
{
	uint8_t sdr_record[IPMI_SDR_MAX_RECORD_LENGTH];
	uint8_t record_type, logical_physical_fru_device, logical_fru_device_device_slave_address;
	uint8_t tmp_entity_id, tmp_entity_instance;
	int sdr_record_len;
	uint16_t record_count;
	int found_device_id = 0;
	uint16_t record_id;
	uint8_t entity_id, entity_instance;
	int i;

	if (ipmi_ctx == NULL)
		return (-1);

	/* Clear the sensors list */
	ipmi_dev->sensors_count = 0;
	memset(ipmi_dev->sensors_id_list, 0, sizeof(ipmi_dev->sensors_id_list));

	if (!(sdr_ctx = ipmi_sdr_ctx_create ()))
	{
		libfreeipmi_cleanup();
		fatal_with_errno(EXIT_FAILURE, "ipmi_sdr_ctx_create()");
	}

#ifndef HAVE_FREEIPMI_11X_12X
	if (!(sdr_parse_ctx = ipmi_sdr_parse_ctx_create ()))
	{
		libfreeipmi_cleanup();
		fatal_with_errno(EXIT_FAILURE, "ipmi_sdr_parse_ctx_create()");
	}
#endif

	if (ipmi_sdr_cache_open (sdr_ctx, ipmi_ctx, CACHE_LOCATION) < 0)
	{
		if (ipmi_sdr_ctx_errnum (sdr_ctx) != IPMI_SDR_ERR_CACHE_READ_CACHE_DOES_NOT_EXIST)
		{
			libfreeipmi_cleanup();
			fatal_with_errno(EXIT_FAILURE, "ipmi_sdr_cache_open: %s",
				ipmi_sdr_ctx_errormsg (sdr_ctx));
		}
	}

	if (ipmi_sdr_ctx_errnum (sdr_ctx) == IPMI_SDR_ERR_CACHE_READ_CACHE_DOES_NOT_EXIST)
	{
		if (ipmi_sdr_cache_create (sdr_ctx,
				 ipmi_ctx, CACHE_LOCATION,
				 NUT_IPMI_SDR_CACHE_DEFAULTS,
				 NULL, NULL) < 0)
		{
			libfreeipmi_cleanup();
			fatal_with_errno(EXIT_FAILURE, "ipmi_sdr_cache_create: %s",
				ipmi_sdr_ctx_errormsg (sdr_ctx));
		}
		if (ipmi_sdr_cache_open (sdr_ctx, ipmi_ctx, CACHE_LOCATION) < 0)
		{
			if (ipmi_sdr_ctx_errnum (sdr_ctx) != IPMI_SDR_ERR_CACHE_READ_CACHE_DOES_NOT_EXIST)
			{
				libfreeipmi_cleanup();
				fatal_with_errno(EXIT_FAILURE, "ipmi_sdr_cache_open: %s",
					ipmi_sdr_ctx_errormsg (sdr_ctx));
			}
		}
	}

	if (ipmi_sdr_cache_record_count (sdr_ctx, &record_count) < 0) {
		fprintf (stderr,
			"ipmi_sdr_cache_record_count: %s\n",
			ipmi_sdr_ctx_errormsg (sdr_ctx));
		goto cleanup;
	}

	upsdebugx(3, "Found %i records in SDR cache", record_count);

	for (i = 0; i < record_count; i++, ipmi_sdr_cache_next (sdr_ctx))
	{
		memset (sdr_record, '\0', IPMI_SDR_MAX_RECORD_LENGTH);

		if ((sdr_record_len = ipmi_sdr_cache_record_read (sdr_ctx,
				sdr_record,
				IPMI_SDR_MAX_RECORD_LENGTH)) < 0)
		{
			fprintf (stderr, "ipmi_sdr_cache_record_read: %s\n",
				ipmi_sdr_ctx_errormsg (sdr_ctx));
			goto cleanup;
		}
		if (ipmi_sdr_parse_record_id_and_type (SDR_PARSE_CTX,
				sdr_record,
				sdr_record_len,
				NULL,
				&record_type) < 0)
		{
			fprintf (stderr, "ipmi_sdr_parse_record_id_and_type: %s\n",
				ipmi_sdr_ctx_errormsg (sdr_ctx));
			goto cleanup;
		}

		upsdebugx (5, "Checking record %i (/%i)", i, record_count);

		if (record_type != IPMI_SDR_FORMAT_FRU_DEVICE_LOCATOR_RECORD) {
			upsdebugx(1, "=======> not device locator (%i)!!", record_type);
			continue;
		}

		if (ipmi_sdr_parse_fru_device_locator_parameters (SDR_PARSE_CTX,
				sdr_record,
				sdr_record_len,
				NULL,
				&logical_fru_device_device_slave_address,
				NULL,
				NULL,
				&logical_physical_fru_device,
				NULL) < 0)
		{
			fprintf (stderr, "ipmi_sdr_parse_fru_device_locator_parameters: %s\n",
				ipmi_sdr_ctx_errormsg (sdr_ctx));
			goto cleanup;
		}

		upsdebugx(2, "Checking device %i/%i", logical_physical_fru_device,
					logical_fru_device_device_slave_address);

		if (logical_physical_fru_device
			&& logical_fru_device_device_slave_address == ipmi_dev->ipmi_id)
		{
			found_device_id++;

			if (ipmi_sdr_parse_fru_entity_id_and_instance (SDR_PARSE_CTX,
					sdr_record,
					sdr_record_len,
					&entity_id,
					&entity_instance) < 0)
			{
				fprintf (stderr,
					"ipmi_sdr_parse_fru_entity_id_and_instance: %s\n",
					ipmi_sdr_ctx_errormsg (sdr_ctx));
				goto cleanup;
			}
			break;
		}
	}

	if (!found_device_id)
	{
		fprintf (stderr, "Couldn't find device id %d\n", ipmi_dev->ipmi_id);
		goto cleanup;
	}
	else
		upsdebugx(1, "Found device id %d", ipmi_dev->ipmi_id);

	if (ipmi_sdr_cache_first (sdr_ctx) < 0)
	{
		fprintf (stderr, "ipmi_sdr_cache_first: %s\n",
			ipmi_sdr_ctx_errormsg (sdr_ctx));
		goto cleanup;
	}

	for (i = 0; i < record_count; i++, ipmi_sdr_cache_next (sdr_ctx))
	{
		/* uint8_t sdr_record[IPMI_SDR_CACHE_MAX_SDR_RECORD_LENGTH];
		uint8_t record_type, tmp_entity_id, tmp_entity_instance;
		int sdr_record_len; */

		memset (sdr_record, '\0', IPMI_SDR_MAX_RECORD_LENGTH);
		if ((sdr_record_len = ipmi_sdr_cache_record_read (sdr_ctx,
				sdr_record,
				IPMI_SDR_MAX_RECORD_LENGTH)) < 0)
		{
			fprintf (stderr, "ipmi_sdr_cache_record_read: %s\n",
				ipmi_sdr_ctx_errormsg (sdr_ctx));
			goto cleanup;
		}

		if (ipmi_sdr_parse_record_id_and_type (SDR_PARSE_CTX,
				sdr_record,
				sdr_record_len,
				&record_id,
				&record_type) < 0)
		{
			fprintf (stderr, "ipmi_sdr_parse_record_id_and_type: %s\n",
				ipmi_sdr_ctx_errormsg (sdr_ctx));
			goto cleanup;
		}

		upsdebugx (5, "Checking record %i (/%i)", record_id, record_count);

		if (record_type != IPMI_SDR_FORMAT_FULL_SENSOR_RECORD
			&& record_type != IPMI_SDR_FORMAT_COMPACT_SENSOR_RECORD
			&& record_type != IPMI_SDR_FORMAT_EVENT_ONLY_RECORD) {
			continue;
		}

		if (ipmi_sdr_parse_entity_id_instance_type (SDR_PARSE_CTX,
				sdr_record,
				sdr_record_len,
				&tmp_entity_id,
				&tmp_entity_instance,
				NULL) < 0)
		{
			fprintf (stderr, "ipmi_sdr_parse_entity_instance_type: %s\n",
				ipmi_sdr_ctx_errormsg (sdr_ctx));
			goto cleanup;
		}

		if (tmp_entity_id == entity_id
			&& tmp_entity_instance == entity_instance)
		{
			upsdebugx (1, "Found record id = %u for device id %u",
				record_id, ipmi_dev->ipmi_id);

			/* Add it to the tracked list */
			ipmi_dev->sensors_id_list[ipmi_dev->sensors_count] = record_id;
			ipmi_dev->sensors_count++;
		}
	}


cleanup:
	/* Cleanup */
	if (sdr_ctx) {
		ipmi_sdr_ctx_destroy (sdr_ctx);
	}

#ifndef HAVE_FREEIPMI_11X_12X
	if (sdr_parse_ctx) {
		ipmi_sdr_parse_ctx_destroy (sdr_parse_ctx);
	}
#endif /* HAVE_FREEIPMI_11X_12X */

	return ipmi_dev->sensors_count;
}


/*
=> Nominal conditions


Record ID, Sensor Name, Sensor Number, Sensor Type, Sensor State, Sensor Reading, Sensor Units, Sensor Event/Reading Type Code, Sensor Event Bitmask, Sensor Event String
52, Presence, 84, Entity Presence, Nominal, N/A, N/A, 6Fh, 1h, 'Entity Present'
57, Status, 100, Power Supply, Nominal, N/A, N/A, 6Fh, 1h, 'Presence detected'
116, Current, 148, Current, Nominal, 0.20, A, 1h, C0h, 'OK'
118, Voltage, 150, Voltage, Nominal, 236.00, V, 1h, C0h, 'OK'

=> Power failure conditions

Record ID, Sensor Name, Sensor Number, Sensor Type, Sensor State, Sensor Reading, Sensor Units, Sensor Event/Reading Type Code, Sensor Event Bitmask, Sensor Event String
52, Presence, 84, Entity Presence, Nominal, N/A, N/A, 6Fh, 1h, 'Entity Present'
57, Status, 100, Power Supply, Critical, N/A, N/A, 6Fh, 9h, 'Presence detected' 'Power Supply input lost (AC/DC)'

=> PSU removed

Record ID, Sensor Name, Sensor Number, Sensor Type, Sensor State, Sensor Reading, Sensor Units, Sensor Event/Reading Type Code, Sensor Event Bitmask, Sensor Event String
52, Presence, 84, Entity Presence, Critical, N/A, N/A, 6Fh, 2h, 'Entity Absent'
57, Status, 100, Power Supply, Critical, N/A, N/A, 6Fh, 8h, 'Power Supply input lost (AC/DC)'

*/

int nut_ipmi_monitoring_init()
{
	int errnum;

	if (ipmi_monitoring_init (0, &errnum) < 0) {
		upsdebugx (1, "ipmi_monitoring_init() error: %s", ipmi_monitoring_ctx_strerror (errnum));
		return -1;
	}

	if (!(mon_ctx = ipmi_monitoring_ctx_create ())) {
		upsdebugx (1, "ipmi_monitoring_ctx_create() failed");
		return -1;
	}

#if HAVE_FREEIPMI_MONITORING
	/* FIXME: replace "/tmp" by a proper place, using mkdtemp() or similar */
	if (ipmi_monitoring_ctx_sdr_cache_directory (mon_ctx, "/tmp") < 0) {
		upsdebugx (1, "ipmi_monitoring_ctx_sdr_cache_directory() error: %s",
					ipmi_monitoring_ctx_errormsg (mon_ctx));
		return -1;
	}

	if (ipmi_monitoring_ctx_sensor_config_file (mon_ctx, NULL) < 0) {
		upsdebugx (1, "ipmi_monitoring_ctx_sensor_config_file() error: %s",
					ipmi_monitoring_ctx_errormsg (mon_ctx));
		return -1;
	}
#endif /* HAVE_FREEIPMI_MONITORING */

	return 0;
}

int nut_ipmi_get_sensors_status(IPMIDevice_t *ipmi_dev)
{
	int retval = -1;

#if HAVE_FREEIPMI_MONITORING
	/* It seems we don't need more! */
	unsigned int sensor_reading_flags = IPMI_MONITORING_SENSOR_READING_FLAGS_IGNORE_NON_INTERPRETABLE_SENSORS;
	int sensor_count, i, str_count;
	int psu_status = PSU_STATUS_UNKNOWN;

	if (mon_ctx == NULL) {
		upsdebugx (1, "Monitoring context not initialized!");
		return -1;
	}

	/* Monitor only the list of sensors found previously */
	if ((sensor_count = ipmi_monitoring_sensor_readings_by_record_id (mon_ctx,
																		NULL, /* hostname is NULL for In-band communication */
																		NULL, /* FIXME: needed? ipmi_config */
																		sensor_reading_flags,
																		ipmi_dev->sensors_id_list,
																		ipmi_dev->sensors_count,
																		NULL,
																		NULL)) < 0)
	{
		upsdebugx (1, "ipmi_monitoring_sensor_readings_by_record_id() error: %s",
					ipmi_monitoring_ctx_errormsg (mon_ctx));
		return -1;
	}

	upsdebugx (1, "nut_ipmi_get_sensors_status: %i sensors to check", sensor_count);

	for (i = 0; i < sensor_count; i++, ipmi_monitoring_sensor_iterator_next (mon_ctx))
	{
		int record_id, sensor_type;
		int sensor_bitmask_type = -1;
		/* int sensor_reading_type, sensor_state; */
		char **sensor_bitmask_strings = NULL;
		void *sensor_reading = NULL;

		if ((record_id = ipmi_monitoring_sensor_read_record_id (mon_ctx)) < 0)
		{
			upsdebugx (1, "ipmi_monitoring_sensor_read_record_id() error: %s",
						ipmi_monitoring_ctx_errormsg (mon_ctx));
			continue;
		}

		if ((sensor_type = ipmi_monitoring_sensor_read_sensor_type (mon_ctx)) < 0)
		{
			upsdebugx (1, "ipmi_monitoring_sensor_read_sensor_type() error: %s",
						ipmi_monitoring_ctx_errormsg (mon_ctx));
			continue;
		}

		upsdebugx (1, "checking sensor #%i, type %i", record_id, sensor_type);

		/* should we consider this for ALARM?
		 * IPMI_MONITORING_STATE_NOMINAL
		 * IPMI_MONITORING_STATE_WARNING
		 * IPMI_MONITORING_STATE_CRITICAL
		 * if ((sensor_state = ipmi_monitoring_sensor_read_sensor_state (mon_ctx)) < 0)
		 * ... */

		if ((sensor_reading = ipmi_monitoring_sensor_read_sensor_reading (mon_ctx)) == NULL)
		{
			upsdebugx (1, "ipmi_monitoring_sensor_read_sensor_reading() error: %s",
						ipmi_monitoring_ctx_errormsg (mon_ctx));
		}

		/* This can be needed to interpret sensor_reading format!
		if ((sensor_reading_type = ipmi_monitoring_sensor_read_sensor_reading_type (ctx)) < 0)
		{
			upsdebugx (1, "ipmi_monitoring_sensor_read_sensor_reading_type() error: %s",
						ipmi_monitoring_ctx_errormsg (mon_ctx));
		} */

		if ((sensor_bitmask_type = ipmi_monitoring_sensor_read_sensor_bitmask_type (mon_ctx)) < 0)
		{
			upsdebugx (1, "ipmi_monitoring_sensor_read_sensor_bitmask_type() error: %s",
						ipmi_monitoring_ctx_errormsg (mon_ctx));
			continue;
		}

		if ((sensor_bitmask_strings = ipmi_monitoring_sensor_read_sensor_bitmask_strings (mon_ctx)) == NULL)
		{
			upsdebugx (1, "ipmi_monitoring_sensor_read_sensor_bitmask_strings() error: %s",
						ipmi_monitoring_ctx_errormsg (mon_ctx));
			continue;
		}

		/* Only the few possibly interesting sensors are considered */
		switch (sensor_type)
		{
			case IPMI_MONITORING_SENSOR_TYPE_TEMPERATURE:
				ipmi_dev->temperature = *((double *)sensor_reading);
				upsdebugx (3, "Temperature: %.2f", *((double *)sensor_reading));
				dstate_setinfo("ambient.temperature", "%.2f", *((double *)sensor_reading));
				retval = 0;
				break;
			case IPMI_MONITORING_SENSOR_TYPE_VOLTAGE:
				ipmi_dev->voltage =  *((double *)sensor_reading);
				upsdebugx (3, "Voltage: %.2f", *((double *)sensor_reading));
				dstate_setinfo("input.voltage", "%.2f", *((double *)sensor_reading));
				retval = 0;
				break;
			case IPMI_MONITORING_SENSOR_TYPE_CURRENT:
				ipmi_dev->input_current = *((double *)sensor_reading);
				upsdebugx (3, "Current: %.2f", *((double *)sensor_reading));
				dstate_setinfo("input.current", "%.2f", *((double *)sensor_reading));
				retval = 0;
				break;

			case IPMI_MONITORING_SENSOR_TYPE_POWER_SUPPLY:
				/* Possible values:
				 * 'Presence detected'
				 * 'Power Supply input lost (AC/DC)' => maps to status:OFF */
				upsdebugx (3, "Power Supply: status string");
				if (sensor_bitmask_type == IPMI_MONITORING_SENSOR_BITMASK_TYPE_UNKNOWN) {
					upsdebugx(3, "No status string");
				}
				str_count = 0;
				while (sensor_bitmask_strings[str_count])
				{
					upsdebugx (3, "\t'%s'", sensor_bitmask_strings[str_count]);
					if (!strncmp("Power Supply input lost (AC/DC)",
							sensor_bitmask_strings[str_count],
							strlen("Power Supply input lost (AC/DC)"))) {
								/* Don't override PSU absence! */
								if (psu_status != PSU_ABSENT) {
									psu_status = PSU_POWER_FAILURE;	/* = status OFF */
								}
					}
					str_count++;
				}
				break;
			case IPMI_MONITORING_SENSOR_TYPE_ENTITY_PRESENCE:
				/* Possible values:
				 * 'Entity Present' => maps to status:OL
				 * 'Entity Absent' (PSU has been removed!) => declare staleness */
				upsdebugx (3, "Entity Presence: status string");
				if (sensor_bitmask_type == IPMI_MONITORING_SENSOR_BITMASK_TYPE_UNKNOWN) {
					upsdebugx(3, "No status string");
				}
				str_count = 0;
				while (sensor_bitmask_strings[str_count])
				{
					upsdebugx (3, "\t'%s'", sensor_bitmask_strings[str_count]);
					if (!strncmp("Entity Present",
							sensor_bitmask_strings[str_count],
							strlen("Entity Present"))) {
								psu_status = PSU_PRESENT;
					}
					else if (!strncmp("Entity Absent",
							sensor_bitmask_strings[str_count],
							strlen("Entity Absent"))) {
								psu_status = PSU_ABSENT;
					}
					str_count++;
				}
				break;

			/* Not sure of the values of these, so get as much as possible... */
			case IPMI_MONITORING_SENSOR_TYPE_POWER_UNIT:
				upsdebugx (3, "Power Unit: status string");
				str_count = 0;
				while (sensor_bitmask_strings[str_count])
				{
					upsdebugx (3, "\t'%s'", sensor_bitmask_strings[str_count]);
					str_count++;
				}
				break;
			case IPMI_MONITORING_SENSOR_TYPE_SYSTEM_ACPI_POWER_STATE:
				upsdebugx (3, "System ACPI Power State: status string");
				str_count = 0;
				while (sensor_bitmask_strings[str_count])
				{
					upsdebugx (3, "\t'%s'", sensor_bitmask_strings[str_count]);
					str_count++;
				}
				break;
			case IPMI_MONITORING_SENSOR_TYPE_BATTERY:
				upsdebugx (3, "Battery: status string");
				str_count = 0;
				while (sensor_bitmask_strings[str_count])
				{
					upsdebugx (3, "\t'%s'", sensor_bitmask_strings[str_count]);
					str_count++;
				}
				break;
		}
	}

	/* Process status if needed */
	if (psu_status != PSU_STATUS_UNKNOWN) {

		status_init();

		switch (psu_status)
		{
			case PSU_PRESENT:
				status_set("OL");
				retval = 0;
				break;
			case PSU_ABSENT:
				status_set("OFF");
				/* Declare stale */
				retval = -1;
				break;
			case PSU_POWER_FAILURE:
				status_set("OFF");
				retval = 0;
				break;
		}

		status_commit();
	}
#endif /* HAVE_FREEIPMI_MONITORING */

	return retval;
}

/*
--chassis-control=CONTROL
              Control the chassis. This command provides power-up, power-down, and reset control. Supported values: POWER-DOWN, POWER-UP, POWER-CYCLE, HARD-RESET, DIAGNOS‐
              TIC-INTERRUPT, SOFT-SHUTDOWN.
*/
