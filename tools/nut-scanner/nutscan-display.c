/*
 *  Copyright (C) 2011 - EATON
 *  2023 - Jim Klimov <jimklimov+nut@gmail.com>
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

/*! \file nutscan-display.c
    \brief format and display scanned devices
    \author Frederic Bohe <fredericbohe@eaton.com>
*/

#include "common.h"
#include <stdio.h>
#include "nutscan-device.h"
#include "nut-scan.h"
#include "nut_stdint.h"

static char * nutscan_device_type_string[TYPE_END] = {
	"NONE",
	"USB",
	"SNMP",
	"XML",
	"NUT",
	"IPMI",
	"AVAHI",
	"EATON_SERIAL"
};

void nutscan_display_ups_conf_with_sanity_check(nutscan_device_t * device)
{
	upsdebugx(2, "%s: %s", __func__, device
		? (device->type < TYPE_END ? nutscan_device_type_string[device->type] : "<UNKNOWN>")
		: "<NULL>");
	nutscan_display_ups_conf(device);
	nutscan_display_sanity_check(device);
}

void nutscan_display_ups_conf(nutscan_device_t * device)
{
	nutscan_device_t * current_dev = device;
	nutscan_options_t * opt;
	static int nutdev_num = 1;

	upsdebugx(2, "%s: %s", __func__, device
		? (device->type < TYPE_END ? nutscan_device_type_string[device->type] : "<UNKNOWN>")
		: "<NULL>");

	if (device == NULL) {
		return;
	}

	/* Find start of the list */
	while (current_dev->prev != NULL) {
		current_dev = current_dev->prev;
	}

	/* Display each device */
	do {
		printf("[nutdev%i]\n\tdriver = \"%s\"\n\tport = \"%s\"\n",
				nutdev_num, current_dev->driver,
				current_dev->port);

		opt = current_dev->opt;

		while (NULL != opt) {
			if (opt->option != NULL) {
				printf("\t%s", opt->option);
				if (opt->value != NULL) {
					printf(" = \"%s\"", opt->value);
				}
				printf("\n");
			}
			opt = opt->next;
		}

		nutdev_num++;

		current_dev = current_dev->next;
	}
	while (current_dev != NULL);
}

void nutscan_display_parsable(nutscan_device_t * device)
{
	nutscan_device_t * current_dev = device;
	nutscan_options_t * opt;

	upsdebugx(2, "%s: %s", __func__, device
		? (device->type < TYPE_END ? nutscan_device_type_string[device->type] : "<UNKNOWN>")
		: "<NULL>");

	if (device == NULL) {
		return;
	}

	/* Find start of the list */
	while (current_dev->prev != NULL) {
		current_dev = current_dev->prev;
	}

	/* Display each device */
	do {
		/* Do not separate by whitespace, in case someone already parses this */
		printf("%s:driver=\"%s\",port=\"%s\"",
			nutscan_device_type_string[current_dev->type],
			current_dev->driver,
			current_dev->port);

		opt = current_dev->opt;

		while (NULL != opt) {
			if (opt->option != NULL) {
				/* Do not separate by whitespace, in case someone already parses this */
				printf(",%s", opt->option);
				if (opt->value != NULL) {
					printf("=\"%s\"", opt->value);
				}
			}
			opt = opt->next;
		}

		printf("\n");

		current_dev = current_dev->next;
	}
	while (current_dev != NULL);
}

/* TODO: If this is ever a memory-pressure problem,
 * e.g. if preparing to monitor hundreds of devices,
 * can convert to dynamically allocated (and freed)
 * strings. For now go for speed with static arrays.
 */
typedef struct keyval_strings {
	char key[SMALLBUF];
	char val[LARGEBUF];
} keyval_strings_t;

void nutscan_display_sanity_check_serial(nutscan_device_t * device)
{
	/* Some devices have useless serial numbers
	 * (empty strings, all-zeroes, all-spaces etc.)
	 * and others have identical serial numbers on
	 * physically different hardware units.
	 * Warn about these as a possible problem e.g.
	 * for matching and discerning devices generally.
	 * Note that we may also have multiple data paths
	 * to the same device (e.g. monitored over USB
	 * and SNMP, so the situation is not necessarily
	 * a problem).
	 * Also note that not all devices may have/report
	 * a serial at all (option will be missing).
	 */
	nutscan_device_t * current_dev = device;
	nutscan_options_t * opt;
	int nutdev_num = 1;
	size_t listlen = 0, count = 0, i;
	keyval_strings_t *map = NULL, *entry = NULL;

	upsdebugx(2, "%s: %s", __func__, device
		? (device->type < TYPE_END ? nutscan_device_type_string[device->type] : "<UNKNOWN>")
		: "<NULL>");

	if (device == NULL) {
		return;
	}

	/* Find end of the list */
	while (current_dev->next != NULL) {
		current_dev = current_dev->next;
	}

	/* Find start of the list and count its size */
	while (current_dev->prev != NULL) {
		current_dev = current_dev->prev;
		listlen++;
	}

	/* Empty? No bogus data or conflicts at least... */
	if (!listlen) {
		return;
	}

	/* Process each device:
	 * Build a map of "serial"=>"nutdevX[,...,nutdevZ]"
	 * and warn if there are bogus "serial" keys or if
	 * there are several nutdev's (a comma in value).
	 */

	/* Reserve enough slots for all-unique serials */
	map = calloc(sizeof(keyval_strings_t), listlen);
	if (map == NULL) {
		fprintf(stderr, "%s: Memory allocation error, skipped\n", __func__);
		return;
	}

	upsdebugx(3, "%s: checking serial numbers for %" PRIuSIZE " device configuration(s)",
		__func__, listlen);

	do {
		/* Look for serial option in current device */
		opt = current_dev->opt;

		while (NULL != opt) {
			if (opt->option != NULL && !strcmp(opt->option, "serial")) {
				/* This nutdevX has a serial; is it in map already? */
				char keytmp[SMALLBUF];
				snprintf(keytmp, sizeof(keytmp), "%s",
					opt->value ? (opt->value[0] ? opt->value : "<empty>") : "<null>");

				for (i = 0, entry = NULL; i < listlen && map[i].key[0] != '\0'; i++) {
					if (!strncmp(map[i].key, keytmp, sizeof(map[i].key))) {
						entry = &(map[i]);
						break;
					}
				}

				if (entry) {
					/* Got a hit => append value */
					upsdebugx(3, "%s: duplicate entry for serial '%s'",
						__func__, keytmp);

					/* TODO: If changing from preallocated LARGEBUF to
					 * dynamic allocation, malloc data for larger "val".
					 */
					snprintfcat(entry->val, sizeof(entry->val),
						",nutdev%i", nutdev_num);
				} else {
					/* No hit => new key */
					upsdebugx(3, "%s: new entry for serial '%s'",
						__func__, keytmp);

					/* TODO: If changing from preallocated LARGEBUF to
					 * dynamic allocation, malloc data for new "entry"
					 * and its key/val fields.
					 */
					entry = &(map[i]);

					count++;
					if (count != i || count > listlen) {
						/* Should never get here, but just in case... */
						fprintf(stderr, "%s: Loop overflow, skipped\n", __func__);
						upsdebugx(3, "%s: count=%" PRIuSIZE " i=%" PRIuSIZE " listlen%" PRIuSIZE,
							__func__, count, i, listlen);
						goto exit;
					}

					snprintf(entry->key, sizeof(entry->key),
						"%s", keytmp);
					snprintf(entry->val, sizeof(entry->val),
						"nutdev%i", nutdev_num);
				}

				/* Abort the opt-searching loop for this device */
				goto next;
			}
			opt = opt->next;
		}

next:
		nutdev_num++;

		current_dev = current_dev->next;
	}
	while (current_dev != NULL);

	if (!count) {
		/* No serials in found devices? Oh well */
		goto exit;
	}

	/* Now look for red flags in the map */
	/* FIXME: Weed out special chars to avoid breaking comment-line markup?
	 * Thinking of ASCII control codes < 32 including CR/LF, and codes 128+... */
	for (i = 0; i < count; i++) {
		size_t j;
		entry = &(map[i]);

		/* NULL or empty serials */
		if (!strcmp(entry->key, "<null>") || !strcmp(entry->key, "<empty>")) {
			printf("\n# WARNING: %s \"serial\" reported in some devices: %s\n",
				entry->key, entry->val);
			continue;
		}

		/* All chars in "serial" are same (zero, space, etc.) */
		for (j = 0; entry->key[j] != '\0' && entry->key[j] == entry->key[0]; j++);
		if (j > 0 && entry->key[j] == '\0') {
			printf("\n# WARNING: all-same character \"serial\" "
				"with %" PRIuSIZE " copies of '%c' (0x%02X) "
				"reported in some devices: %s\n",
				j, entry->key[0], entry->key[0], entry->val);
			continue;
		}

		/* Duplicates (maybe same device, maybe not) */
		for (j = 0; entry->val[j] != '\0' && entry->val[j] != ','; j++);
		if (j > 0 && entry->key[j] != '\0') {
			printf("\n# WARNING: same \"serial\" value \"%s\" "
				"reported in several device configurations "
				"(maybe okay if multiple drivers for same device, "
				"likely a vendor bug if reported by same driver "
				"for many devices): %s\n",
				entry->key, entry->val);
			continue;
		}
	}

exit:
	free (map);
}

void nutscan_display_sanity_check(nutscan_device_t * device)
{
	upsdebugx(2, "%s: %s", __func__, device
		? (device->type < TYPE_END ? nutscan_device_type_string[device->type] : "<UNKNOWN>")
		: "<NULL>");

	/* Extend the list later as more sanity-checking appears */
	nutscan_display_sanity_check_serial(device);
}
