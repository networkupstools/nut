/*  generic_gpio_libgpiod.c - gpiod based NUT driver code for GPIO attached UPS devices
 *
 *  Copyright (C)
 *	2023 - 2025		Modris Berzonis <modrisb@apollo.lv>
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

#include "config.h"
#include "main.h"
#include "attribute.h"
#include "generic_gpio_common.h"
#include "generic_gpio_libgpiod.h"

#if !(defined WITH_LIBGPIO_VERSION) || !(defined WITH_LIBGPIO_VERSION_STR) || (WITH_LIBGPIO_VERSION == 0)
# error "This driver can not be built, requires a known API WITH_LIBGPIO_VERSION to build against"
#endif

#define DRIVER_NAME	"GPIO UPS driver (API " WITH_LIBGPIO_VERSION_STR ")"
#define DRIVER_VERSION	"1.04"

/* driver description structure */
upsdrv_info_t upsdrv_info = {
	DRIVER_NAME,
	DRIVER_VERSION,
	"ModrisB <modrisb@apollo.lv>",
	DRV_STABLE,
	{ NULL }
};

static void reserve_lines_libgpiod(struct gpioups_t *gpioupsfd, int inner);

/*	CyberPower 12V open collector state definitions
	0 ON BATTERY			Low when operating from utility line
							Open when operating from battery
	1 REPLACE BATTERY		Low when battery is charged
							Open when battery fails the Self Test
	6 BATTERY MISSING		Low when battery is present
							Open when battery is missing
	3 LOW BATTERY			Low when battery is near full charge capacity
							Open when operating from a battery with < 20% capacity
	NUT supported states
	OL      On line (mains is present)
	OB      On battery (mains is not present)
	LB      Low battery
	HB      High battery
	RB      The battery needs to be replaced
	CHRG    The battery is charging
	DISCHRG The battery is discharging (inverter is providing load power)
	BYPASS  UPS bypass circuit is active -- no battery protection is available
	CAL     UPS is currently performing runtime calibration (on battery)
	OFF     UPS is offline and is not supplying power to the load
	OVER    UPS is overloaded
	TRIM    UPS is trimming incoming voltage (called "buck" in some hardware)
	BOOST   UPS is boosting incoming voltage
	FSD     Forced Shutdown (restricted use, see the note below)
	CyberPower rules setting
	OL=^0;OB=0;LB=3;HB=^3;RB=1;DISCHRG=0&^3;BYPASS=6;
*/

/*
 * reserve GPIO lines as per run options and inner parameter: do reservation once
 * or per each status read
 */
static void reserve_lines_libgpiod(struct gpioups_t *gpioupsfdlocal, int inner) {
	struct libgpiod_data_t *libgpiod_data = (struct libgpiod_data_t *)(gpioupsfdlocal->lib_data);
	upsdebugx(5, "reserve_lines_libgpiod runOptions 0x%x, inner %d",
		(unsigned int)gpioupsfdlocal->runOptions, inner);

	if(((gpioupsfdlocal->runOptions&ROPT_REQRES) != 0) == inner) {
#if WITH_LIBGPIO_VERSION < 0x00020000
		struct gpiod_line_request_config config;
		int gpioRc;
		config.consumer=upsdrv_info.name;
		if(gpioupsfdlocal->runOptions&ROPT_EVMODE) {
			config.request_type = GPIOD_LINE_REQUEST_EVENT_BOTH_EDGES;
			upsdebugx(5, "reserve_lines_libgpiod GPIOD_LINE_REQUEST_EVENT_BOTH_EDGES");
		} else {
			config.request_type = GPIOD_LINE_REQUEST_DIRECTION_INPUT;
			upsdebugx(5, "reserve_lines_libgpiod GPIOD_LINE_REQUEST_DIRECTION_INPUT");
		}
		config.flags = 0;	/*	GPIOD_LINE_REQUEST_FLAG_OPEN_DRAIN;	*/
		gpioRc = gpiod_line_request_bulk(&libgpiod_data->gpioLines, &config, NULL);
		if(gpioRc)
			fatal_with_errno(
				LOG_ERR,
				"GPIO gpiod_line_request_bulk call failed, check for other applications that may have reserved GPIO lines"
			);
		upsdebugx(5,
			"GPIO gpiod_line_request_bulk with type %d return code %d",
			config.request_type,
			gpioRc
		);
#else	/* #if WITH_LIBGPIO_VERSION >= 0x00020000 */
		if(!libgpiod_data->request) {
			libgpiod_data->request = gpiod_chip_request_lines(libgpiod_data->gpioChipHandle, libgpiod_data->config, libgpiod_data->lineConfig);
		}
		if(!libgpiod_data->request) {
			fatalx(LOG_ERR,	"Failed to create line request");
		}
#endif	/* WITH_LIBGPIO_VERSION */
	}
}

/*
 * allocate memeory for libary, open gpiochip
 * and check lines numbers validity - consistency with h/w chip
 */
void gpio_open(struct gpioups_t *gpioupsfdlocal) {
	struct libgpiod_data_t *libgpiod_data = xcalloc(1, sizeof(struct libgpiod_data_t));
	gpioupsfdlocal->lib_data = libgpiod_data;

#if WITH_LIBGPIO_VERSION < 0x00020000
	libgpiod_data->gpioChipHandle = gpiod_chip_open_by_name(gpioupsfdlocal->chipName);
#else	/* #if WITH_LIBGPIO_VERSION >= 0x00020000 */
	if(!strchr(gpioupsfdlocal->chipName, '/')) {
		char *pathName = xcalloc(strlen(gpioupsfdlocal->chipName)+6, sizeof(char));
		strcpy(pathName, "/dev/");
		strcat(pathName, gpioupsfdlocal->chipName);
		libgpiod_data->gpioChipHandle = gpiod_chip_open(pathName);
		free(pathName);
	} else {
		libgpiod_data->gpioChipHandle = gpiod_chip_open(gpioupsfdlocal->chipName);
	}
#endif	/* WITH_LIBGPIO_VERSION */

	if(!libgpiod_data->gpioChipHandle)
		fatal_with_errno(
			LOG_ERR,
			"Could not open GPIO chip [%s], check chips presence and/or access rights",
			gpioupsfdlocal->chipName
		);
	else {
		int gpioRc;

#if WITH_LIBGPIO_VERSION < 0x00020000
		upslogx(LOG_NOTICE, "GPIO chip [%s] opened", gpioupsfdlocal->chipName);
		gpioupsfdlocal->chipLinesCount = gpiod_chip_num_lines(libgpiod_data->gpioChipHandle);
#else	/* #if WITH_LIBGPIO_VERSION >= 0x00020000 */
		struct gpiod_chip_info *chipInfo;
		struct gpiod_line_settings *lineSettings;

		upslogx(LOG_NOTICE, "GPIO chip [%s] opened, api version 2", gpioupsfdlocal->chipName);
		chipInfo = gpiod_chip_get_info(libgpiod_data->gpioChipHandle);
		gpioupsfdlocal->chipLinesCount = gpiod_chip_info_get_num_lines(chipInfo);
		gpiod_chip_info_free(chipInfo);
#endif	/* WITH_LIBGPIO_VERSION */

		upslogx(LOG_NOTICE, "Find %d lines on GPIO chip [%s]", gpioupsfdlocal->chipLinesCount, gpioupsfdlocal->chipName);
		if(gpioupsfdlocal->chipLinesCount<gpioupsfdlocal->upsMaxLine) {
			gpiod_chip_close(libgpiod_data->gpioChipHandle);
			fatalx(
				LOG_ERR,
				"GPIO chip lines count %d smaller than UPS line number used (%d)",
				gpioupsfdlocal->chipLinesCount,
				gpioupsfdlocal->upsMaxLine
			);
		}

#if WITH_LIBGPIO_VERSION < 0x00020000
		gpiod_line_bulk_init(&libgpiod_data->gpioLines);
		gpiod_line_bulk_init(&libgpiod_data->gpioEventLines);
		gpioRc = gpiod_chip_get_lines(
			libgpiod_data->gpioChipHandle,
			(unsigned int *)gpioupsfdlocal->upsLines,
			gpioupsfdlocal->upsLinesCount,
			&libgpiod_data->gpioLines
		);
#else	/* #if WITH_LIBGPIO_VERSION >= 0x00020000 */
		libgpiod_data->values = xcalloc(gpioupsfdlocal->upsLinesCount, sizeof(*libgpiod_data->values));
		lineSettings = gpiod_line_settings_new();
		libgpiod_data->lineConfig = gpiod_line_config_new();
		libgpiod_data->config = gpiod_request_config_new();
		if(!lineSettings || !libgpiod_data->lineConfig || !libgpiod_data->config) {
			fatalx(LOG_ERR,	"Failed to allocate configuration structures - out of memory");
		}
		gpioRc = gpiod_line_settings_set_direction(lineSettings, GPIOD_LINE_DIRECTION_INPUT);
		if(gpioRc) {
			fatal_with_errno(LOG_ERR, "Failed to set lines to GPIOD_LINE_DIRECTION_INPUT");
		}
		gpioRc = gpiod_line_settings_set_edge_detection(lineSettings, GPIOD_LINE_EDGE_BOTH);
		if(gpioRc) {
			fatal_with_errno(LOG_ERR, "Failed to set lines to GPIOD_LINE_EDGE_BOTH");
		}
		gpioRc = gpiod_line_config_add_line_settings(libgpiod_data->lineConfig, (unsigned int *)gpioupsfdlocal->upsLines,
							  gpioupsfdlocal->upsLinesCount, lineSettings);
		if(gpioRc) {
			fatalx(LOG_ERR,	"Failed to attach line settings to line configuration");
		}
		gpiod_request_config_set_consumer(libgpiod_data->config, upsdrv_info.name);
		gpiod_line_settings_free(lineSettings);
#endif
		if(gpioRc)
			fatal_with_errno(
				LOG_ERR,
				"GPIO line reservation (gpiod_chip_get_lines call) failed with code %d, check for possible issue in rules parameter",
				gpioRc
			);
		upsdebugx(5, "GPIO gpiod_chip_get_lines return code %d", gpioRc);
		reserve_lines_libgpiod(gpioupsfdlocal, 0);

#if WITH_LIBGPIO_VERSION >= 0x00020000
	gpioRc=gpiod_line_request_get_values(libgpiod_data->request, gpioupsfdlocal->upsLinesStates);
	if(gpioRc==0) {
		int i;
		for(i=0; i < gpioupsfdlocal->upsLinesCount; i++) {
			gpioupsfdlocal->upsLinesStates[i] = libgpiod_data->values[i];
		}
	}
#endif	/* WITH_LIBGPIO_VERSION */
	}
}

/*
 * close gpiochip  and release allocated memory
 */
void gpio_close(struct gpioups_t *gpioupsfdlocal) {
	if(gpioupsfdlocal) {
		struct libgpiod_data_t *libgpiod_data = (struct libgpiod_data_t *)(gpioupsfdlocal->lib_data);
		if(libgpiod_data) {
#if WITH_LIBGPIO_VERSION >= 0x00020000
			if(libgpiod_data->values) {
				free(libgpiod_data->values);
			}
			gpiod_line_config_free(libgpiod_data->lineConfig);
			gpiod_request_config_free(libgpiod_data->config);
#endif	/* WITH_LIBGPIO_VERSION */
			if(libgpiod_data->gpioChipHandle) {
				gpiod_chip_close(libgpiod_data->gpioChipHandle);
			}
			free(gpioupsfdlocal->lib_data);
			gpioupsfdlocal->lib_data = NULL;
		}
	}
}

/*
 * get GPIO line states for all needed lines
 */
void gpio_get_lines_states(struct gpioups_t *gpioupsfdlocal) {
	int i;
	int gpioRc;
	struct libgpiod_data_t *libgpiod_data = (struct libgpiod_data_t *)(gpioupsfdlocal->lib_data);

	reserve_lines_libgpiod(gpioupsfdlocal, 1);

	if(gpioupsfdlocal->runOptions&ROPT_EVMODE) {
#if WITH_LIBGPIO_VERSION < 0x00020000
		struct timespec timeoutLong = {1,0};
		struct gpiod_line_event event;
		int monRes;
		upsdebugx(5,
			"gpio_get_lines_states_libgpiod initial %d, timeout %ld",
			gpioupsfdlocal->initial,
			timeoutLong.tv_sec
		);
		if(gpioupsfdlocal->initial) {
			timeoutLong.tv_sec = 35;
		} else {
			gpioupsfdlocal->initial = 1;
		}
		upsdebugx(5,
			"gpio_get_lines_states_libgpiod initial %d, timeout %ld",
			gpioupsfdlocal->initial,
			timeoutLong.tv_sec
		);
		gpiod_line_bulk_init(&libgpiod_data->gpioEventLines);
		monRes=gpiod_line_event_wait_bulk(
			&libgpiod_data->gpioLines,
			&timeoutLong,
			&libgpiod_data->gpioEventLines
		);
		upsdebugx(5,
			"gpiod_line_event_wait_bulk completed with %d return code and timeout %ld s",
			monRes,
			timeoutLong.tv_sec
		);
		if(monRes==1) {
			int num_lines_local = (int)gpiod_line_bulk_num_lines(&libgpiod_data->gpioEventLines);
			int j;
			for(j=0; j<num_lines_local; j++) {
				struct gpiod_line *eLine = gpiod_line_bulk_get_line(
					&libgpiod_data->gpioEventLines,
					j
				);
				int eventRc=gpiod_line_event_read(eLine, &event);
				unsigned int lineOffset = gpiod_line_offset(eLine);
				event.event_type=0;
				upsdebugx(5,
					"Event read return code %d and event type %d for line %d",
					eventRc,
					event.event_type,
					lineOffset
				);
			}
		}
#else	/* #if WITH_LIBGPIO_VERSION >= 0x00020000 */
		int64_t poll_timeout = 1000000000;	//	in nanoseconds
		if(gpioupsfdlocal->initial) {
			poll_timeout = 35000000000;
		} else {
			gpioupsfdlocal->initial = 1;
		}
		gpioRc = gpiod_line_request_wait_edge_events(libgpiod_data->request, poll_timeout);
#endif	/* WITH_LIBGPIO_VERSION */
	}
	for(i=0; i < gpioupsfdlocal->upsLinesCount; i++) {
		gpioupsfdlocal->upsLinesStates[i] = -1;
	}
#if WITH_LIBGPIO_VERSION < 0x00020000
	gpioRc=gpiod_line_get_value_bulk(
		&libgpiod_data->gpioLines,
		gpioupsfdlocal->upsLinesStates
	);
#else	/* #if WITH_LIBGPIO_VERSION >= 0x00020000 */
	gpioRc=gpiod_line_request_get_values(libgpiod_data->request, libgpiod_data->values);
	if(gpioRc==0) {
		for(i=0; i < gpioupsfdlocal->upsLinesCount; i++) {
			gpioupsfdlocal->upsLinesStates[i] = libgpiod_data->values[i];
		}
	}
#endif	/* WITH_LIBGPIO_VERSION */
	if (gpioRc < 0)
		fatal_with_errno(LOG_ERR, "GPIO line status read call failed");

	upsdebugx(5,
		"GPIO gpiod_line_get_value_bulk completed with %d return code, status values:",
		gpioRc
	);

	for(i=0; i < gpioupsfdlocal->upsLinesCount; i++) {
		upsdebugx(5,
			"Line%d state = %d",
			i,
			gpioupsfdlocal->upsLinesStates[i]
		);
	}

	if(gpioupsfdlocal->runOptions&ROPT_REQRES) {
#if WITH_LIBGPIO_VERSION < 0x00020000
		gpiod_line_release_bulk(&libgpiod_data->gpioLines);
#else	/* #if WITH_LIBGPIO_VERSION >= 0x00020000 */
		gpiod_line_request_release(libgpiod_data->request);
		libgpiod_data->request = NULL;
#endif	/* WITH_LIBGPIO_VERSION */
	}
}
