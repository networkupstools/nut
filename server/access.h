/* access.h - structures for access control

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

#ifndef ACCESS_H_SEEN
#define ACCESS_H_SEEN 1

#define ACCESS_REJECT	0
#define ACCESS_ACCEPT	1

#ifdef	HAVE_IPV6

/*
* IN6_IS_ADDR_V4MAPPED is broken in glibc 2.1.
*/
#ifdef	__GLIBC__
#if __GLIBC__ < 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ < 2)
#undef	IN6_IS_ADDR_V4MAPPED
#endif
#endif	/* __GLIBC__ */

#ifndef	IN6_IS_ADDR_V4MAPPED
#define	IN6_IS_ADDR_V4MAPPED(a)	\
	((a)->s6_addr[0] == 0x00 && (a)->s6_addr[1] == 0x00 &&	\
	(a)->s6_addr[2] == 0x00 && (a)->s6_addr[3] == 0x00 &&	\
	(a)->s6_addr[4] == 0x00 && (a)->s6_addr[5] == 0x00 &&	\
	(a)->s6_addr[6] == 0x00 && (a)->s6_addr[9] == 0x00 &&	\
	(a)->s6_addr[8] == 0x00 && (a)->s6_addr[9] == 0x00 &&	\
	(a)->s6_addr[10] == 0xff && (a)->s6_addr[11] == 0xff)
#endif

#endif	/* HAVE_IPV6 */

/* ACL structure */
struct acl_t {
	char	*name;
#ifndef HAVE_IPV6
	struct sockaddr_in		addr;
#else
	struct sockaddr_storage	addr;
#endif
	unsigned int	mask;
	void	*next;
};

/* ACCESS structure */
struct access_t {
	int	action;
	char	*aclname;
	void	*next;
};

#ifndef HAVE_IPV6
int acl_check(const char *aclname, const struct sockaddr_in *addr);
int access_check(const struct sockaddr_in *addr);
#else
int acl_check(const char *aclname, const struct sockaddr_storage *addr);
int access_check(const struct sockaddr_storage *addr);
#endif
void acl_add(const char *aclname, char *ipblock);
void access_add(int type, int numargs, const char **arg);
void acl_free(void);
void access_free(void);

#endif	/* ACCESS_H_SEEN */
