/* alist.h - Header for alist.c - the alist structure as used in DMF.
 *
 * Copyright (C) 2016 Carlos Dominguez <CarlosDominguez@eaton.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef ALIST_H
/* Note: we #define ALIST_H only in the end of file */

/* THE BIG THEORY COMMENT (portion of dmfsnmp.h big theory)
 *
 * The dynamic DMF supports adds a way to load and populate at run-time the
 * C structures with OIDs, flags and other information that was previously
 * statically compiled into "snmp-ups" driver and "nut-scanner" application.
 *
 * For that, first you dynamically instantiate the auxiliary list:
 * `alist_t * list = alist_new(NULL,(void (*)(void **))alist_destroy, NULL );`
 * This list hides the complexity of a dynamically allocated array of arrays,
 * ultimately storing the lookup tables, alarms, etc.
 *
 * You can also search for entries in the hierarchy behind "list" with
 * `alist_get_element_by_name()`, and dump contents with debug methods, e.g.:
 *      print_mib2nut_memory_struct((mib2nut_info_t *)
 *              alist_get_element_by_name(list, "powerware")->values[0]);
 *
 * See dmf-test.c for some example usage.
 */

#include <stdbool.h>
#include "extstate.h"

/*
 *      HEADER FILE
 *
 */

#define DEFAULT_CAPACITY 16

/* "Auxiliary list" structure to store hierarchies
 * of lists with bits of data */
typedef struct {
	void **values;
	int size;
	int capacity;
	char *name;
	void (*destroy)(void **self_p);
	void (*new_element)(void);
} alist_t;

/* Create new instance of alist_t with LOOKUP type,
 * for storage a list of collections
 *alist_t *
 *	alist_new ();
 * New generic list element (can be the root element) */
alist_t *
	alist_new (
		const char *name,
		void (*destroy)(void **self_p),
		void (*new_element)(void)
	);

/* Destroy full array of generic list elements */
void
	alist_destroy (alist_t **self_p);

/* Add a generic element at the end of the list */
void
	alist_append (alist_t *self, void *element);

/* Return the last element of the list */
alist_t *
	alist_get_last_element (alist_t *self);

/* Return the element which has `char* name` equal to the requested one */
alist_t *
	alist_get_element_by_name (alist_t *self, char *name);

#define ALIST_H
#endif /* ALIST_H */
