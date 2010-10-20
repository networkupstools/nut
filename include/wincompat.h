#ifndef NUT_WINCOMPAT_H
#define NUT_WINCOMPAT_H

/* 
   Copyright (C) 2001 Andrew Delpha (delpha@computer.org)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, WRITE to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.              
*/


/*
This header is provided to make the map the windows system calls to be compatable with
the NUT code.  Windows defines a lot of system functions with a _(name) like snprintf
Also, the error codes from the networking code are not put into errno as nut expects.
For convience I've provided wrapper functions which do this.
*/

/* This is conditional because read/write are generic in unix, and this will make them network specific */
#ifdef W32_NETWORK_CALL_OVERRIDE 
#define close(h) sktclose(h)
#define read(h,b,s) sktread(h,b,s)
#define write(h,b,s) sktwrite( h ,(char *) b , s )
#define connect(h,n,l) sktconnect(h,n,l)
#endif

#define strcasecmp(a,b) _stricmp(a,b)
#define snprintf _snprintf
#define vsnprintf _vsnprintf

int sktconnect(int fh, struct sockaddr * name, int len);
int sktread(int fh, char *buf, int size);
int sktwrite(int fh, char *buf, int size);
int sktclose(int fh);

#endif
