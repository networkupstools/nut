/* access.c - functions for access control

   Copyright (C) 2003  Russell Kroll <rkroll@exploits.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "common.h"
#include "access.h"

	struct 	acl_t	*acl_head = NULL;
	struct	access_t	*access_head = NULL;

/* see if <addr> matches the acl <aclname> */
int acl_check(const char *aclname, const struct sockaddr_in *addr)
{
	struct	acl_t	*tmp;
	int	aclchk, addrchk;

	tmp = acl_head;
	while (tmp != NULL) {
		if (!strcmp(tmp->name, aclname)) {
			aclchk = tmp->addr & tmp->mask;
			addrchk = ntohl(addr->sin_addr.s_addr) & tmp->mask;

			if (aclchk == addrchk) 
				return 1;	/* match */
		}

		tmp = tmp->next;
	}

	return 0;	/* not found */
}

/* return ACCEPT/REJECT based on source address */
int access_check(const struct sockaddr_in *addr)
{
	struct	access_t	*tmp;
	int	ret;

	tmp = access_head;

	while (tmp != NULL) {
		ret = acl_check(tmp->aclname, addr);

		upsdebugx(3, "acl_check: %s: match %d", tmp->aclname, ret);

		if (ret == 1) {
			upsdebugx(1, "ACL [%s] matches, action=%d", 
				tmp->aclname, tmp->action);
			return tmp->action;
		}

		tmp = tmp->next;
	}

	/* fail safe */
	return ACCESS_REJECT;
}

/* add to the master list of ACL names */
void acl_add(const char *aclname, char *ipblock)
{
	struct	acl_t	*tmp, *last;
	char	*addr, *mask;

	/* are we sane? */
	if ((!aclname) || (!ipblock))
		return;

	/* ipblock must be in the form <addr>/<mask>	*/

	mask = strchr(ipblock, '/');

	/* 192.168.1.1/32: valid */
	/* 192.168.1.1/255.255.255.255: valid */
	/* 192.168.1.1: invalid */

	/* no slash = broken acl declaration */
	if (!mask) 	
		return;

	*mask++ = '\0';
	addr = ipblock;

	tmp = last = acl_head;

	while (tmp != NULL) {
		last = tmp;
		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(struct acl_t));
	tmp->name = xstrdup(aclname);
	tmp->addr = ntohl(inet_addr(addr));
	tmp->next = NULL;

	/* must be a /nn CIDR type block */
	if (strstr(mask, ".") == NULL) { 
		if (atoi(mask) != 32)
			tmp->mask = ((unsigned int) ((1 << atoi(mask)) - 1) << 
					(32 - atoi(mask)));
		else
			tmp->mask = 0xffffffff;	/* avoid overflow from 2^32 */
	}
	else
		tmp->mask = ntohl(inet_addr(mask));

	if (last == NULL)	/* first */
		acl_head = tmp;
	else
		last->next = tmp;
}

void acl_free(void)
{
	struct	acl_t	*ptr, *next;

	ptr = acl_head;

	while (ptr) {
		next = ptr->next;
		
		if (ptr->name)
			free(ptr->name);
		free(ptr);

		ptr = next;
	}

	acl_head = NULL;
}

void access_free(void)
{
	struct	access_t	*ptr, *next;

	ptr = access_head;

	while (ptr) {
		next = ptr->next;

		if (ptr->aclname)
			free(ptr->aclname);
		free(ptr);

		ptr = next;
	}

	access_head = NULL;
}	

static void access_append(int action, const char *aclname)
{
	struct	access_t	*tmp, *last;

	tmp = last = access_head;

	while (tmp != NULL) {
		last = tmp;
		tmp = tmp->next;
	}

	tmp = xmalloc(sizeof(struct access_t));

	tmp->action = action;
	tmp->aclname = xstrdup(aclname);

	tmp->next = NULL;

	if (last)
		last->next = tmp;
	else
		access_head = tmp;
}		

void access_add(int type, int numargs, const char **arg)
{
	int	i;

	for (i = 0; i < numargs; i++)
		access_append(type, arg[i]);
}
