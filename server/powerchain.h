/*  powerchain.h - internal PowerChain tracking structure details
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
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#ifndef POWERCHAIN_H_SEEN
#define POWERCHAIN_H_SEEN 1

#include "parseconf.h"

#define MAIN_POWER	"Main"

/* Each Powerchain is stored separatly, ie:
 * parent -> child
 * Main -> ups1 -> pdu1.outlet.1 -> psu1
 * Main -> ups1 -> outlet1:pdu1 ->
 * Main -> UPS2 -> PDU1
 * ... */
 
typedef struct powerchain_link_s {
	char						*name;
	struct powerchain_link_s	*parent;
	int							 parent_outlet_num;
	struct powerchain_link_s	*child;
} powerlink_t;

/* structure for the linked list of each UPS that we track */
typedef struct powerchain_s {
	/* char						*name;
	 * or
	 * int						num; */
	powerlink_t					*nodes;
	struct powerchain_s			*next;
} powerchain_t;


extern powerchain_t	*powerchains;

/* Add a new link to a powerchain ; create a new powerchain if needed,
 * or insert the node into an existing one otherwise.
 * Child default to $hostname, when no powerchain already exist. */
void powerchain_add(const char *link_name, const char *parent);
powerlink_t *powerchain_get(const char *link_name);

#endif	/* POWERCHAIN_H_SEEN */
