/*  tests/generic_gpio_libglocal.c - gpio device emulation library for GPIO attached UPS devices
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

#include "common.h"
#include <unistd.h>
#include <errno.h>
#include "generic_gpio_utest.h"

#if !(defined WITH_LIBGPIO_VERSION) || !(defined WITH_LIBGPIO_VERSION_STR) || (WITH_LIBGPIO_VERSION == 0)
# error "This driver can not be built, requires a known API WITH_LIBGPIO_VERSION to build against"
#endif

static char  chipName[NUT_GPIO_CHIPNAMEBUF];
static unsigned int num_lines=0;

static int gStatus = 0;
static int errReqFor_line_get_value_bulk=0;

void setNextLinesReadToFail(void) {
	errReqFor_line_get_value_bulk=1;
}

void gpiod_chip_close(struct gpiod_chip *chip) {
	NUT_UNUSED_VARIABLE(chip);
}

#if WITH_LIBGPIO_VERSION < 0x00020000

struct gpiod_chip *gpiod_chip_open_by_name(const char *name) {
	strcpy(chipName, name);
	if(strcmp(name, "gpiochip1"))
		return (struct gpiod_chip *)1;
	else {
		errno = EACCES;
		return NULL;
	}
}

unsigned int gpiod_chip_num_lines(struct gpiod_chip *chip) {
	NUT_UNUSED_VARIABLE(chip);
	if(!strcmp(chipName, "gpiochip2"))
		return 2;
	return 32;
}

int gpiod_chip_get_lines(struct gpiod_chip *chip,
			 unsigned int *offsets, unsigned int num_offsets,
			 struct gpiod_line_bulk *bulk) {
	NUT_UNUSED_VARIABLE(chip);
	NUT_UNUSED_VARIABLE(offsets);
	NUT_UNUSED_VARIABLE(bulk);
	num_lines=num_offsets;
	return 0;
}

int gpiod_line_request_bulk(struct gpiod_line_bulk *bulk,
			    const struct gpiod_line_request_config *config,
			    const int *default_vals)
{
	NUT_UNUSED_VARIABLE(bulk);
	NUT_UNUSED_VARIABLE(config);
	NUT_UNUSED_VARIABLE(default_vals);
	return 0;
}

int gpiod_line_get_value_bulk(struct gpiod_line_bulk *bulk,
			      int *values)
{
	unsigned int	i;
	int	pinPos = 1;
	NUT_UNUSED_VARIABLE(bulk);

	if(errReqFor_line_get_value_bulk) {
		errReqFor_line_get_value_bulk=0;
		errno = EPERM;
		return -1;
	}
	for(i=0; i<num_lines; i++) {
		values[i]=(gStatus&pinPos)!=0;
		pinPos=pinPos<<1;
	}

	gStatus++;
	return 0;
}

int gpiod_line_event_wait_bulk(struct gpiod_line_bulk *bulk,
			       const struct timespec *timeout,
			       struct gpiod_line_bulk *event_bulk)
{
	NUT_UNUSED_VARIABLE(bulk);
	NUT_UNUSED_VARIABLE(timeout);
	NUT_UNUSED_VARIABLE(event_bulk);
	switch(gStatus%3) {
		case 0:
			sleep(2);
			break;

		case 1:
			sleep(4);
			break;

		case 2:
			sleep(6);
			break;

		/* Static analysis wants this, we should never get here though, not with %3 above */
		default:
			fatalx(EXIT_FAILURE, "%s: Hit impossible default case", __func__);
	}
	return 0;
}

int gpiod_line_event_read(struct gpiod_line *line,
			  struct gpiod_line_event *event) {
	NUT_UNUSED_VARIABLE(line);
	NUT_UNUSED_VARIABLE(event);
	return 0;
}

unsigned int gpiod_line_offset(struct gpiod_line *line) {
	NUT_UNUSED_VARIABLE(line);
	return 0;
}

void gpiod_line_release_bulk(struct gpiod_line_bulk *bulk) {
	NUT_UNUSED_VARIABLE(bulk);
}

#else	/* #if WITH_LIBGPIO_VERSION >= 0x00020000 */

struct gpiod_line_request *
gpiod_chip_request_lines(struct gpiod_chip *chip,
			 struct gpiod_request_config *req_cfg,
			 struct gpiod_line_config *line_cfg) {
	NUT_UNUSED_VARIABLE(chip);
	NUT_UNUSED_VARIABLE(req_cfg);
	NUT_UNUSED_VARIABLE(line_cfg);
	return (struct gpiod_line_request *)1;
}

struct gpiod_chip *gpiod_chip_open(const char *path) {
	strcpy(chipName, path);
	if(!strstr(path, "gpiochip1"))
		return (struct gpiod_chip *)1;
	else {
		errno = EACCES;
		return NULL;
	}
}

struct gpiod_chip_info *gpiod_chip_get_info(struct gpiod_chip *chip) {
	NUT_UNUSED_VARIABLE(chip);
	return (struct gpiod_chip_info *)1;
}

size_t gpiod_chip_info_get_num_lines(struct gpiod_chip_info *info) {
	NUT_UNUSED_VARIABLE(info);
	if(strstr(chipName, "gpiochip2"))
		return 2;
	return 32;
}

void gpiod_chip_info_free(struct gpiod_chip_info *info) {
	NUT_UNUSED_VARIABLE(info);
}

struct gpiod_line_settings *gpiod_line_settings_new(void) {
	return (struct gpiod_line_settings *)1;
}

int gpiod_line_settings_set_direction(struct gpiod_line_settings *settings,
				      enum gpiod_line_direction direction) {
	NUT_UNUSED_VARIABLE(settings);
	NUT_UNUSED_VARIABLE(direction);
	return 0;
}

int gpiod_line_settings_set_edge_detection(struct gpiod_line_settings *settings,
					   enum gpiod_line_edge edge) {
	NUT_UNUSED_VARIABLE(settings);
	NUT_UNUSED_VARIABLE(edge);
	return 0;
}

struct gpiod_line_config *gpiod_line_config_new(void) {
	return (struct gpiod_line_config *)1;
}

int gpiod_line_config_add_line_settings(struct gpiod_line_config *config,
					const unsigned int *offsets,
					size_t num_offsets,
					struct gpiod_line_settings *settings) {
	NUT_UNUSED_VARIABLE(config);
	NUT_UNUSED_VARIABLE(offsets);
	NUT_UNUSED_VARIABLE(num_offsets);
	NUT_UNUSED_VARIABLE(settings);
	return 0;
}

struct gpiod_request_config *gpiod_request_config_new(void) {
	return (struct gpiod_request_config *)1;
}

void gpiod_request_config_set_consumer(struct gpiod_request_config *config,
				       const char *consumer) {
	NUT_UNUSED_VARIABLE(config);
	NUT_UNUSED_VARIABLE(consumer);
}

int gpiod_line_request_get_values(struct gpiod_line_request *request,
				  enum gpiod_line_value *values) {
	NUT_UNUSED_VARIABLE(request);
	unsigned int	i;
	int	pinPos = 1;

	if(errReqFor_line_get_value_bulk) {
		errReqFor_line_get_value_bulk=0;
		errno = EPERM;
		return -1;
	}
	for(i=0; i<num_lines; i++) {
		values[i]=(gStatus&pinPos)!=0;
		pinPos=pinPos<<1;
	}

	gStatus++;
	return 0;
}

void gpiod_line_settings_free(struct gpiod_line_settings *settings) {
	NUT_UNUSED_VARIABLE(settings);
}

void gpiod_line_config_free(struct gpiod_line_config *config) {
	NUT_UNUSED_VARIABLE(config);
}

void gpiod_request_config_free(struct gpiod_request_config *config) {
	NUT_UNUSED_VARIABLE(config);
}

int gpiod_line_request_wait_edge_events(struct gpiod_line_request *request,
					int64_t timeout_ns) {
	NUT_UNUSED_VARIABLE(request);
	NUT_UNUSED_VARIABLE(timeout_ns);
	switch(gStatus%3) {
		case 0:
			sleep(2);
			break;

		case 1:
			sleep(4);
			break;

		case 2:
			sleep(6);
			break;

		/* Static analysis wants this, we should never get here though, not with %3 above */
		default:
			fatalx(EXIT_FAILURE, "%s: Hit impossible default case", __func__);
	}
	return 0;
}

void gpiod_line_request_release(struct gpiod_line_request *request) {
	NUT_UNUSED_VARIABLE(request);
}
#endif	/* WITH_LIBGPIO_VERSION */
