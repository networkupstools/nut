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

/* ACL structure */
struct acl_t {
	char	*name;
	unsigned int	addr;
	unsigned int	mask;
	void	*next;
};

/* ACCESS structure */
struct access_t {
	int	action;
	char	*aclname;
	void	*next;
};

int acl_check(const char *aclname, const struct sockaddr_in *addr);
int access_check(const struct sockaddr_in *addr);
void acl_add(const char *aclname, char *ipblock);
void access_add(int type, int numargs, const char **arg);
void acl_free(void);
void access_free(void);

#endif	/* ACCESS_H_SEEN */
