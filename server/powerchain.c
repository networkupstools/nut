/*  powerchain.c - internal PowerChain tracking structure details
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

/* TODO
 * - process and store outlet
 * - process multi parent
 * - handle app type (ie, level below $hostname)
 */

#include <unistd.h>
#include "common.h"
#include "powerchain.h"


static powerchain_t *powerchain_create(powerlink_t *root_link)
{
	powerchain_t	*temp;
	powerchain_t	*pchains = powerchains;

	temp = xcalloc(1, sizeof(*temp));
	temp->nodes = root_link;
	temp->next = NULL;

	if (pchains == NULL) {
		powerchains = temp;
	}
	else {
		/* Search for the last node */
		while (pchains->next) {
			pchains = pchains->next;
		}
		pchains->next = temp;
	}

	return temp;
}

static powerlink_t* powerlink_create(const char* name,
	powerlink_t *parent, powerlink_t *child)
{
	powerlink_t	*temp;

	upsdebugx(3, "powerlink_create(%s)", name);

	temp = xcalloc(1, sizeof(*temp));
	temp->name = xstrdup(name);
	temp->parent_outlet_num = -1;

	temp->parent = parent;
	if (parent != NULL) {
		/* Inherit parent->child, to shift automatically default hostname
		 * to the end of the list */
		temp->child = temp->parent->child;
		temp->parent->child = temp;
	}

	if (child != NULL) {
		/* Shift previous child */
		if (temp->child != NULL) {
			temp->child->child = temp->child;
		}
		temp->child = child;
	}

	return temp;
}

/* add a new link to a powerchain ; create a new powerchain if needed,
 * or insert the node into an existing one otherwise */
void powerchain_add(const char *link_name, const char *parent)
{
	powerchain_t	*temp_powerchain;
	powerchain_t	*temp_powerchain_next;
	powerlink_t		*temp_link = powerchain_get(link_name);
	powerlink_t		*temp_parent_link;
	powerlink_t		*temp_child_link;
	const char		*parent_name;
	char			*hostname;
	int				outlet_index = -1;

	upsdebugx(3, "powerchain_add(%s, %s)", link_name, parent);

	if (link_name == NULL) {
		upsdebugx(3, "powerchain_add error: link_name is NULL");
		return;
	}

	/* Check for special parent names */
	/* FIXME: need completion */
	if (strchr(parent, ':') != NULL) {
		outlet_index = atoi(parent);
		
		/* parent_outlet_num
			$hostname
			MAIN_POWER
		*/
	}

	/* Acquire parent link */
	if ((parent != NULL) && ((parent_name = strchr(parent, ':')) != NULL)) {
		outlet_index = atoi(parent);
		parent_name++;
	}
	else {
		parent_name = parent;
	}

	temp_parent_link = powerchain_get(parent_name);
	/* FIXME: process multi parent (sts, app, ...)
	 * while (temp_parent_link != NULL)
	 * temp_parent_link = powerchain_get_next(parent_name); */

	/* Parent defaults to 'Main', if not otherwise specified */
	if ((parent_name == NULL) || (temp_parent_link == NULL)) {
		temp_parent_link = powerlink_create(MAIN_POWER, NULL, NULL);
		temp_powerchain = powerchain_create(temp_parent_link);
	}

	/* Create the new Powerlink */
	temp_link = powerlink_create(link_name, temp_parent_link, NULL);

	/* And update reference */
	temp_parent_link->child = temp_link;

	/* Acquire child */
	if (temp_link->child == NULL)
	{
		/* Defaults to $hostname */
		hostname = xcalloc(1, SMALLBUF);
		if (gethostname(hostname, SMALLBUF) < 0) {
			free(hostname);
			hostname = xstrdup("Host");
		}
		/* Check that it's not our parent (device.type=app is the only case
		 * and does not have a child) */
		if (strncmp(temp_link->parent->name, hostname, strlen(hostname))) {
			/* Actually create the child link */
			temp_child_link = powerlink_create(hostname, temp_link, NULL);

			/* And update reference */
			temp_link->child = temp_child_link;
		}
		free(hostname);
	}
	/* FIXME: else, if == "$hostname", replace with $hostname */
}

/* called upon reload */
void powerchain_update(const char *link_name, const char *parent)
{
	powerlink_t		*temp_link = powerchain_get(link_name);
	powerlink_t		*parent_link = powerchain_get(parent);

	/* FIXME: what to do?
	Main ; ups1 ; hostname
	Main ; psu1 ; hostname

	Main ; ups1 ; psu1 ; hostname */

	upsdebugx(3, "powerchain_update: updating %s, with parent = %s, in PowerChain",
				link_name, (parent == NULL)? MAIN_POWER : parent);

	/* Acquire link */
	if (temp_link != NULL) {
		/* Compare parent and child names */
		/* If different, relink and free... */

		/* Check if an update is needed */
		if (strcmp(temp_link->parent->name, parent)) {

			/* free old parent and child links */
			free(temp_link->child);
			free(temp_link->parent);

			/* update link references */
			temp_link->child = parent_link->child;
			parent_link->child = temp_link;
			temp_link->parent = parent_link;
		}
	}
	else {
		/* Create a new link */
		powerchain_add(link_name, parent);
	}
}

				
/* Get the next Powerlink named 'link_name',
 * starting from the Powerchain 'pcref' */
powerlink_t *powerchain_get_next(const char *link_name, powerchain_t *pcref)
{
	powerlink_t		*plink;
	powerchain_t	*ptmp;

	upsdebugx(1, "powerchain_get(%s)", link_name);

	if (link_name == NULL)
		return NULL;

	if (pcref != NULL)
		ptmp = pcref;
	else
		ptmp = powerchains;

	while (ptmp) {
		plink = ptmp->nodes;

		/* Loop on Powerlinks */
		while (plink) {
			upsdebugx(1, "powerchain_get: checking %s", link_name);
			if (!strncmp(plink->name, link_name, strlen(link_name))) {
				upsdebugx(1, "powerchain_get: link found");
				return plink;
			}
			plink = plink->child;
		}

		/* Switch to the next Powerchain */
		ptmp = ptmp->next;
		if (pcref != NULL)
			pcref = ptmp;
	}
	return NULL;
}

/* Get the first Powerlink named 'link_name' */
powerlink_t *powerchain_get(const char *link_name)
{
	return powerchain_get_next(link_name, NULL);
}
