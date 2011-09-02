/*  nut-libfreeipmi.c - NUT IPMI backend, using FreeIPMI
 *
 *  Copyright (C) 2011 - Arnaud Quette <arnaud.quette@free.fr>
 *
 *  Based on the sample code 'ipmmi-fru-example.c', from FreeIPMI
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
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <freeipmi/freeipmi.h>
/* need libipmimonitoring (not yet available as package)
 * #include <ipmi_monitoring.h>
#include <ipmi_monitoring_bitmasks.h> */
#include "common.h"
#include "nut-ipmi.h"

/* FreeIPMI defines */
#define IPMI_FRU_STR_BUFLEN    1024
/* haven't seen a motherboard with more than 2-3 so far, 64 should be more than enough */
#define IPMI_FRU_CUSTOM_FIELDS 64

/* FreeIPMI contexts */
ipmi_ctx_t ipmi_ctx = NULL;
ipmi_fru_parse_ctx_t fru_parse_ctx = NULL;
/* ipmi_monitoring_ctx_t ctx = NULL; */

/* Support functions */
static const char* libfreeipmi_getfield (uint8_t language_code,
	ipmi_fru_parse_field_t *field);

static void libfreeipmi_cleanup();

static int libfreeipmi_get_psu_info (const void *areabuf,
	uint8_t area_length, IPMIDevice_t *ipmi_dev);

static int libfreeipmi_get_board_info (const void *areabuf,
	uint8_t area_length, IPMIDevice_t *ipmi_dev);


int nutipmi_open(int ipmi_id, IPMIDevice_t *ipmi_dev)
{
	int ret = -1;
	uint8_t areabuf[IPMI_FRU_PARSE_AREA_SIZE_MAX+1];
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
	if (!(fru_parse_ctx = ipmi_fru_parse_ctx_create (ipmi_ctx)))
	{
		libfreeipmi_cleanup();
		fatal_with_errno(EXIT_FAILURE, "ipmi_fru_parse_ctx_create()");
	}
      
	/* lots of motherboards calculate checksums incorrectly */
	if (ipmi_fru_parse_ctx_set_flags (fru_parse_ctx, IPMI_FRU_PARSE_FLAGS_SKIP_CHECKSUM_CHECKS) < 0)
	{
		libfreeipmi_cleanup();
		fatalx(EXIT_FAILURE, "ipmi_fru_parse_ctx_set_flags: %s\n",
			ipmi_fru_parse_ctx_strerror (ipmi_fru_parse_ctx_errnum (fru_parse_ctx)));
	}

	/* Now open the requested (local) PSU */
	if (ipmi_fru_parse_open_device_id (fru_parse_ctx, ipmi_id) < 0)
	{
		libfreeipmi_cleanup();
		fatalx(EXIT_FAILURE, "ipmi_fru_parse_open_device_id: %s\n",
			ipmi_fru_parse_ctx_errormsg (fru_parse_ctx));
	}

	/* Set IPMI identifier
	ipmi_dev->ipmi_id = ipmi_id; */

	do
	{
		/* clear fields */
		area_type = 0;
		area_length = 0;
		memset (areabuf, '\0', IPMI_FRU_PARSE_AREA_SIZE_MAX + 1);

		/* parse FRU buffer */
		if (ipmi_fru_parse_read_data_area (fru_parse_ctx,
											&area_type,
											&area_length,
											areabuf,
											IPMI_FRU_PARSE_AREA_SIZE_MAX) < 0)
		{
			libfreeipmi_cleanup();
			fatal_with_errno(EXIT_FAILURE, 
				"ipmi_fru_parse_open_device_id: %s\n",
				ipmi_fru_parse_ctx_errormsg (fru_parse_ctx));
		}

		if (area_length)
		{
			switch (area_type)
			{
				/* get generic board information */
				case IPMI_FRU_PARSE_AREA_TYPE_BOARD_INFO_AREA:

					if(libfreeipmi_get_board_info (areabuf, area_length,
						ipmi_dev) < 0)
					{
						upsdebugx(1, "Can't retrieve board information");
					}
					break;
				/* get specific PSU information */
				case IPMI_FRU_PARSE_AREA_TYPE_MULTIRECORD_POWER_SUPPLY_INFORMATION:

					if(libfreeipmi_get_psu_info (areabuf, area_length, ipmi_dev) < 0)
					{
						upsdebugx(1, "Can't retrieve PSU information");
					}
					break;
				default:
					upsdebugx (5, "FRU: discarding FRU Area Type Read: %02Xh\n", area_type);
					break;
			}
		}
	} while ((ret = ipmi_fru_parse_next (fru_parse_ctx)) == 1);

	/* check for errors */
	if (ret < 0)
	{
		libfreeipmi_cleanup();
		fatal_with_errno(EXIT_FAILURE, "ipmi_fru_parse_next: %s\n",
			ipmi_fru_parse_ctx_errormsg (fru_parse_ctx));
	}

	/* cleanup context */
	libfreeipmi_cleanup();

	return (0);
}

void nutipmi_close(void)
{
	upsdebugx(1, "nutipmi_close...");

	libfreeipmi_cleanup();
}

static const char* libfreeipmi_getfield (uint8_t language_code,
									ipmi_fru_parse_field_t *field)
{
	static char strbuf[IPMI_FRU_PARSE_AREA_STRING_MAX + 1];
	unsigned int strbuflen = IPMI_FRU_PARSE_AREA_STRING_MAX;

	if (!field->type_length_field_length)
		return NULL;

	memset (strbuf, '\0', IPMI_FRU_PARSE_AREA_STRING_MAX + 1);

	if (ipmi_fru_parse_type_length_field_to_string (fru_parse_ctx,
													field->type_length_field,
													field->type_length_field_length,
													language_code,
													strbuf,
													&strbuflen) < 0)
		{
			upsdebugx (2, "ipmi_fru_parse_type_length_field_to_string: %s\n",
				ipmi_fru_parse_ctx_errormsg (fru_parse_ctx));
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
	if (fru_parse_ctx)
	{
		ipmi_fru_parse_close_device_id (fru_parse_ctx);
		ipmi_fru_parse_ctx_destroy (fru_parse_ctx);
	}

	if (ipmi_ctx)
	{
		ipmi_ctx_close (ipmi_ctx);
		ipmi_ctx_destroy (ipmi_ctx);
	}
}

/* Get generic board information (manufacturer and model names, serial, ...) */
static int libfreeipmi_get_psu_info (const void *areabuf,
										uint8_t area_length,
										IPMIDevice_t *ipmi_dev)
{
	/* FIXME: directly use ipmi_dev fields */
	unsigned int overall_capacity;
	unsigned int low_end_input_voltage_range_1;
	unsigned int high_end_input_voltage_range_1;
	unsigned int low_end_input_frequency_range;
	unsigned int high_end_input_frequency_range;
	unsigned int voltage_1;

	/* FIXME: check for the interest and capability to use these data */
	unsigned int peak_va;
	unsigned int inrush_current;
	unsigned int inrush_interval;
	unsigned int low_end_input_voltage_range_2;
	unsigned int high_end_input_voltage_range_2;
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

	if (ipmi_fru_parse_multirecord_power_supply_information (fru_parse_ctx,
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
		fatalx(EXIT_FAILURE, "ipmi_fru_parse_multirecord_power_supply_information: %s",
			ipmi_fru_parse_ctx_errormsg (fru_parse_ctx));
	}

	ipmi_dev->overall_capacity = overall_capacity;

	/* Voltages are in mV! */
	ipmi_dev->input_minvoltage = low_end_input_voltage_range_1 / 1000;
	ipmi_dev->input_maxvoltage = high_end_input_voltage_range_1 / 1000;

	ipmi_dev->input_minfreq = low_end_input_frequency_range;
	ipmi_dev->input_maxfreq = high_end_input_frequency_range;

	ipmi_dev->voltage = libfreeipmi_get_voltage(voltage_1);

	return (0);
}

/* Get specific PSU information */
static int libfreeipmi_get_board_info (const void *areabuf,
	uint8_t area_length, IPMIDevice_t *ipmi_dev)
{
	uint8_t language_code;
	uint32_t mfg_date_time;
	ipmi_fru_parse_field_t board_manufacturer;
	ipmi_fru_parse_field_t board_product_name;
	ipmi_fru_parse_field_t board_serial_number;
	ipmi_fru_parse_field_t board_part_number;
	ipmi_fru_parse_field_t board_fru_file_id;
	ipmi_fru_parse_field_t board_custom_fields[IPMI_FRU_CUSTOM_FIELDS];
	const char *string = NULL;
	time_t timetmp;
	struct tm mfg_date_time_tm;
	char mfg_date_time_buf[IPMI_FRU_STR_BUFLEN + 1];

	upsdebugx(1, "entering libfreeipmi_get_board_info()");

	/* clear fields */
	memset (&board_manufacturer, '\0', sizeof (ipmi_fru_parse_field_t));
	memset (&board_product_name, '\0', sizeof (ipmi_fru_parse_field_t));
	memset (&board_serial_number, '\0', sizeof (ipmi_fru_parse_field_t));
	memset (&board_fru_file_id, '\0', sizeof (ipmi_fru_parse_field_t));
	memset (&board_custom_fields[0], '\0',
			sizeof (ipmi_fru_parse_field_t) * IPMI_FRU_CUSTOM_FIELDS);

	/* parse FRU buffer */
	if (ipmi_fru_parse_board_info_area (fru_parse_ctx,
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
		fatalx(EXIT_FAILURE, "ipmi_fru_parse_board_info_area: %s\n",
			ipmi_fru_parse_ctx_errormsg (fru_parse_ctx));
	}


	if (IPMI_FRU_LANGUAGE_CODE_VALID (language_code))
		printf ("  FRU Board Language: %s\n", ipmi_fru_language_codes[language_code]);
	else
		printf ("  FRU Board Language Code: %02Xh\n", language_code);

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
	upsdebugx(2, "FRU Board Manufacturing Date/Time: %s\n", ipmi_dev->date);

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
