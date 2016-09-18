/* alist.c - "auxiliary list" to store complex structures
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

#include <errno.h>
#include <dirent.h>
#include <assert.h>

#include "common.h"
#include "alist.h"

/*
 *
 *  C FILE
 *
 */

/*New generic list element (can be the root element)*/
alist_t *
alist_new ( const char *name,
	void (*destroy)(void **self_p),
	void (*new_element)(void) )
{
	alist_t *self = (alist_t*) calloc (1, sizeof (alist_t));
	assert (self);
	self->size = 0;
	self->capacity = DEFAULT_CAPACITY;
	self->values = (void**) calloc (self->capacity, sizeof (void*));
	assert (self->values);
	self->destroy = destroy;
	self->new_element = new_element;
	if(name)
		self->name = strdup(name);
	else
		self->name = NULL;
	return self;
}

/*Destroy full array of generic list elements*/
void
alist_destroy (alist_t **self_p)
{
	if (*self_p)
	{
		alist_t *self = *self_p;
		for (;self->size > 0; self->size--)
		{
			if (self->destroy)
				self->destroy(& self->values [self->size-1]);
			else
				free(self->values[self->size-1]);
		}
		if (self->name)
			free(self->name);
		free (self->values);
		free (self);
		*self_p = NULL;
	}
}

/*Add a generic element at the end of the list*/
void
alist_append (alist_t *self, void *element)
{
	if (self->size + 1 == self->capacity)
	{
		self->capacity += DEFAULT_CAPACITY;
		self->values = (void**) realloc (
			self->values,
			self->capacity * sizeof(void*) );
	}
	self->values[self->size] = element;
	self->size++;
	self->values[self->size] = NULL;
}

/*Return the last element of the list*/
alist_t *
alist_get_last_element (alist_t *self)
{
	if(self)
		return (alist_t*)self->values[self->size-1];
	return NULL;
}

alist_t *
alist_get_element_by_name (alist_t *self, char *name)
{
	int i;
	if (self)
		for (i = 0; i < self->size; i++)
			if ( ((alist_t*)self->values[i])->name )
				if (strcmp(((alist_t*)self->values[i])->name, name) == 0)
					return (alist_t*)self->values[i];
	return NULL;
}

