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

#ifdef	HAVE_IPV6
/*
 *  Stolen from privoxy code :]
 */
static int mask_cmp (const struct sockaddr_storage* ip_addr, unsigned int prefix, const struct sockaddr_storage* net_addr) {
	switch (ip_addr->ss_family) {
	case AF_INET:
		return((((struct sockaddr_in*)ip_addr)->sin_addr.s_addr & htonl(prefix)) == ((struct sockaddr_in*)net_addr)->sin_addr.s_addr);
		break;
	case AF_INET6:
		if (AF_INET6 == net_addr->ss_family) {
			struct in6_addr ip, net;
			register unsigned char i;
			
			memcpy (&ip, &((struct sockaddr_in6 *)ip_addr)->sin6_addr, sizeof (struct in6_addr));
			memcpy (&net, &((struct sockaddr_in6 *)net_addr)->sin6_addr, sizeof (struct in6_addr));
			
			i = prefix/8;
			if (prefix%8)
				ip.s6_addr[i++] &= 0xff<<(8-(prefix%8));
			for (; i < sizeof ip.s6_addr; i++)
				ip.s6_addr[i] = 0;
			
			return (memcmp (ip.s6_addr, net.s6_addr, sizeof ip.s6_addr)==0);
		}
		else if (AF_INET == net_addr->ss_family) { /* IPv4 mapped IPv6 */
			struct in6_addr *ip6 = &((struct sockaddr_in6 *)ip_addr)->sin6_addr;
			struct in_addr *net = &((struct sockaddr_in *)net_addr)->sin_addr;
			
			if ((ip6->s6_addr32[3] & (u_int32_t)prefix) == net->s_addr &&
#if BYTE_ORDER == LITTLE_ENDIAN
					(ip6->s6_addr32[2] == (u_int32_t)0xffff0000) &&
#else
					(ip6->s6_addr32[2] == (u_int32_t)0x0000ffff) &&
#endif
					(ip6->s6_addr32[1] == 0) && (ip6->s6_addr32[0] == 0))
				return(1);
			else
				return(0); 
		}
	default:
		fatal_with_errno("mask_cmp: Unknown address family");
		return(0);
	}
}
#endif

/* see if <addr> matches the acl <aclname> */
#ifndef	HAVE_IPV6
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
#else
int acl_check(const char *aclname, const struct sockaddr_storage *addr)
{
	struct	acl_t	*tmp;

	tmp = acl_head;
	while (tmp != NULL) {
		if (!strcmp(tmp->name, aclname))
			if (mask_cmp (addr, tmp->mask, &tmp->addr))
				return 1;
		tmp = tmp->next;
	}
	
	return 0;	/* not found */
}
#endif

/* return ACCEPT/REJECT based on source address */
#ifndef	HAVE_IPV6
int access_check(const struct sockaddr_in *addr)
#else
int access_check(const struct sockaddr_storage *addr)
#endif
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

#ifndef	HAVE_IPV6
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
#else
	/* memset (&saddr, 0, sizeof (struct sockaddr_storage)); */
	tmp = xmalloc(sizeof(struct acl_t));
	memset (tmp, 0, sizeof (struct acl_t));
	tmp->name = xstrdup(aclname);
	tmp->next = NULL;

	if (*addr == '[') { 
		struct sockaddr_in6 s6;
		char *stmp;

		stmp = strchr (addr, ']');
		if (stmp == NULL) {
			free (tmp);
			fatal_with_errno("Expecting \']\' in \"%s\"", addr);
		}

		*stmp = '\0';
		addr++;
		
		memset (&s6, 0, sizeof (struct sockaddr_in6));
		s6.sin6_family = AF_INET6;

		if (inet_pton (AF_INET6, addr, &s6.sin6_addr) < 1) {
			free (tmp);
			fatal_with_errno("Invalid IPv6 address: \"%s\"", addr);
		}

		/* prefix */
		tmp->mask = strtol (mask, NULL, 10);

		if (tmp->mask < 0 || tmp->mask > 128) {
			free (tmp);
			fatal_with_errno("Invalid IPv6 prefix");
		}

		{ register unsigned char i;
			i = (tmp->mask)/8;
			if ((tmp->mask)%8)
				s6.sin6_addr.s6_addr[i++] &= 0xff<<(8-((tmp->mask)%8));
			for (; i < sizeof s6.sin6_addr.s6_addr; i++)
				s6.sin6_addr.s6_addr[i] = 0;
		}

		memcpy (&(tmp->addr), &s6, sizeof (struct sockaddr_in6));
		/* tmp->addr.ss_len = sizeof (struct sockaddr_in6); */
		tmp->addr.ss_family = AF_INET6;
	} else {
		struct sockaddr_in s4;

		/* mask */
		if (inet_pton (AF_INET, mask, &s4.sin_addr) < 1) {
			/* must be a /nn CIDR type block */
			tmp->mask = strtol (mask, NULL, 10);

			if (tmp->mask < 0 || tmp->mask > 32) {
				free (tmp);
				fatal_with_errno("Invalid CIDR type block: Must be > 0 && < 32");
			}
			tmp->mask = 0xffffffff << (32 - tmp->mask);
		} else {
			tmp->mask = ntohl (s4.sin_addr.s_addr);
		}

		memset (&s4, 0, sizeof (struct sockaddr_in));
		s4.sin_family = AF_INET;

		if (inet_pton (AF_INET, addr, &s4.sin_addr) < 1) {
			free (tmp);
			fatal_with_errno("Invalid IPv4 address: \"%s\"", addr);
		}

		s4.sin_addr.s_addr &= htonl (tmp->mask);

		memcpy (&(tmp->addr), &s4, sizeof (struct sockaddr_in));
		/* tmp->addr.ss_len = sizeof (struct sockaddr_in); */
		tmp->addr.ss_family = AF_INET;
	}
#endif

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
