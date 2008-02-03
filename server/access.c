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

#include "nut_stdint.h" /* for uint32_t */

struct acl_t		*acl_head = NULL;
struct access_t	*access_head = NULL;

#ifndef	HAVE_IPV6
static int mask_cmp(const struct sockaddr_in *ip_addr,
		const struct sockaddr_in *net_addr, unsigned int prefix)
{
#else
static int mask_cmp (const struct sockaddr_storage *ip_addr,
		const struct sockaddr_storage *net_addr, unsigned int prefix)
{
	if (ip_addr->ss_family == AF_INET)
	{
#endif
		struct in_addr	*ip  = &((struct sockaddr_in *)ip_addr)->sin_addr;
		struct in_addr	*net = &((struct sockaddr_in *)net_addr)->sin_addr;

		return ((ip->s_addr & prefix) == net->s_addr);
#ifdef	HAVE_IPV6
	}

	if ((ip_addr->ss_family == AF_INET6) && (net_addr->ss_family == AF_INET6))
	{
		struct in6_addr	ip6;
		struct in6_addr	*net = &((struct sockaddr_in6 *)net_addr)->sin6_addr;
		unsigned char		i = (prefix >> 3);
		
		memcpy (&ip6, &((struct sockaddr_in6 *)ip_addr)->sin6_addr, sizeof (struct in6_addr));
			
		if (prefix % 8)
			ip6.s6_addr[i++] &= 0xff << (8 - (prefix % 8));

		while (i < sizeof(ip6.s6_addr))
			ip6.s6_addr[i++] = 0;

	
		return (memcmp(ip6.s6_addr, net->s6_addr, sizeof(ip6.s6_addr)) == 0);
	}

	if ((ip_addr->ss_family == AF_INET6) && (net_addr->ss_family == AF_INET))
	{
		struct in6_addr	*ip6 = &((struct sockaddr_in6 *)ip_addr)->sin6_addr;
		struct in_addr		*net = &((struct sockaddr_in *)net_addr)->sin_addr;
			
		return (IN6_IS_ADDR_V4MAPPED(ip6) &&
			((((const uint32_t *)ip6)[3] & prefix) == net->s_addr));
	}

	fatal_with_errno(EXIT_FAILURE, "mask_cmp: Unknown address family");
#endif
}

/* see if <addr> matches the acl <aclname> */
#ifndef	HAVE_IPV6
int acl_check(const char *aclname, const struct sockaddr_in *addr)
#else
int acl_check(const char *aclname, const struct sockaddr_storage *addr)
#endif
{
	struct acl_t	*tmp;

	for (tmp = acl_head; tmp != NULL; tmp = tmp->next)
	{
		if (strcmp(tmp->name, aclname))
			continue;

		if (mask_cmp(addr, &tmp->addr, tmp->mask))
			return 1;	/* match */
	}

	return 0;	/* not found */
}

/* return ACCEPT/REJECT based on source address */
#ifndef	HAVE_IPV6
int access_check(const struct sockaddr_in *addr)
#else
int access_check(const struct sockaddr_storage *addr)
#endif
{
	struct access_t	*tmp;
	int	ret;

	for (tmp = access_head; tmp != NULL; tmp = tmp->next)
	{
		ret = acl_check(tmp->aclname, addr);

		upsdebugx(3, "acl_check: %s: match %d", tmp->aclname, ret);

		if (ret == 1) {
			upsdebugx(1, "ACL [%s] matches, action=%d",
				tmp->aclname, tmp->action);
			return tmp->action;
		}
	}

	/* fail safe */
	return ACCESS_REJECT;
}

/* add to the master list of ACL names */
void acl_add(const char *aclname, char *ipblock)
{
	struct acl_t	*tmp, *last;
	char		*addr, *mask;

	/* are we sane? */
	if ((!aclname) || (!ipblock))
		return;

	/*
	 * 192.168.1.1/32			valid
	 * 192.168.1.1/255.255.255.255	valid
	 * 192.168.1.1			invalid
	 * ::FFFF:192.168.1.0/120		valid
	 * ::FFFF:192.168.1.1		invalid
	 * ::1/128				valid
	 * ::1					invalid
	 */
	if (((addr = strtok(ipblock, "/")) == NULL) || ((mask = strtok(NULL, "\0")) == NULL))
		fatalx(EXIT_FAILURE, "Can't parse ACL %s %s", aclname, ipblock);

	tmp = last = acl_head;

	while (tmp != NULL)
	{
		last = tmp;
		tmp = tmp->next;
	}

	/* memset (&saddr, 0, sizeof (struct sockaddr_storage)); */
	tmp = xmalloc(sizeof(struct acl_t));
	memset(tmp, 0, sizeof (struct acl_t));
	tmp->name = xstrdup(aclname);
	tmp->next = NULL;

#ifndef	HAVE_IPV6
	if (strstr(mask, ".") == NULL)
	{
		/* must be a /nn CIDR type block */
		tmp->mask = strtol(mask, NULL, 10);

		if (tmp->mask < 0 || tmp->mask > 32)
		{
			free (tmp);
			fatal_with_errno(EXIT_FAILURE, "Invalid CIDR type block: Must be > 0 && < 32");
		}

		if (tmp->mask != 32)
		{
			tmp->mask = htonl((uint32_t)((1 << tmp->mask) - 1) << (32 - tmp->mask));
		}
		else
		{
			tmp->mask = 0xffffffff;	/* avoid overflow from 2^32 */
		}
	}
	else
	{
		/* must be a n.n.n.n dotted quad block */
		tmp->mask = inet_addr(mask);
	}

	tmp->addr.sin_addr.s_addr = inet_addr(addr) & tmp->mask;
#else
	if (strstr(addr, ":") == NULL)
	{
		struct sockaddr_in	s4;	/* IPv4 address */

		/* mask */
		if (inet_pton(AF_INET, mask, &s4.sin_addr) < 1)
		{
			/* must be a /nn CIDR type block */
			tmp->mask = strtol(mask, NULL, 10);

			if (tmp->mask < 0 || tmp->mask > 32)
			{
				free (tmp);
				fatal_with_errno(EXIT_FAILURE, "Invalid CIDR type block: Must be > 0 && < 32");
			}

			if (tmp->mask != 32)
			{
				tmp->mask = htonl((uint32_t)((1 << tmp->mask) - 1) << (32 - tmp->mask));
			}
			else
			{
				tmp->mask = 0xffffffff;	/* avoid overflow from 2^32 */
			}
		}
		else
		{
			/* must be a n.n.n.n dotted quad block */
			tmp->mask = s4.sin_addr.s_addr;
		}

		/* address */
		memset(&s4, 0, sizeof (struct sockaddr_in));
		s4.sin_family = AF_INET;

		/* apply mask to address */
		if (inet_pton(AF_INET, addr, &s4.sin_addr) < 1)
		{
			free(tmp);
			fatal_with_errno(EXIT_FAILURE, "Invalid IPv4 address: \"%s\"", addr);
		}
		else
		{
			s4.sin_addr.s_addr &= tmp->mask;
		}

		memcpy(&(tmp->addr), &s4, sizeof(struct sockaddr_in));

		tmp->addr.ss_family = AF_INET;
	}
	else
	{
		struct sockaddr_in6	s6;	/* IPv6 address */

		/* prefix */
		tmp->mask = strtol(mask, NULL, 10);

		/* address */
		memset(&s6, 0, sizeof(struct sockaddr_in6));
		s6.sin6_family = AF_INET6;

		if (inet_pton(AF_INET6, addr, &s6.sin6_addr) < 1)
		{
			free(tmp);
			fatal_with_errno(EXIT_FAILURE, "Invalid IPv6 address: \"%s\"", addr);
		}

		/* apply prefix to address */
		if (tmp->mask < 0 || tmp->mask > 128)
		{
			free (tmp);
			fatal_with_errno(EXIT_FAILURE, "Invalid IPv6 prefix");
		}
		else
		{
			unsigned char	i = (tmp->mask >> 3);

			if ((tmp->mask) % 8)
				s6.sin6_addr.s6_addr[i++] &= 0xff << (8 - ((tmp->mask) % 8));

			while (i < sizeof(s6.sin6_addr.s6_addr))
				s6.sin6_addr.s6_addr[i++] = 0;
		}

		memcpy(&(tmp->addr), &s6, sizeof(struct sockaddr_in6));

		tmp->addr.ss_family = AF_INET6;
	}
#endif

	if (last == NULL)	/* first */
		acl_head = tmp;
	else
		last->next = tmp;
}

void acl_free(void)
{
	struct acl_t	*ptr, *next;

	ptr = acl_head;

	while (ptr)
	{
		next = ptr->next;
		
		free(ptr->name);
		free(ptr);

		ptr = next;
	}

	acl_head = NULL;
}

void access_free(void)
{
	struct access_t	*ptr, *next;

	ptr = access_head;

	while (ptr)
	{
		next = ptr->next;

		free(ptr->aclname);
		free(ptr);

		ptr = next;
	}

	access_head = NULL;
}	

static void access_append(int action, const char *aclname)
{
	struct access_t	*tmp, *last;

	tmp = last = access_head;

	while (tmp != NULL)
	{
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
