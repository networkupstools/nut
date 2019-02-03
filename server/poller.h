/* poller.h - generic poller interface for upsd

   Copyright (C) 2019  Jean-Baptiste Boric <JeanBaptisteBORIC@eaton.com>

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

#ifndef POLLER_H_SEEN
#define POLLER_H_SEEN

#include "attribute.h"

#include "common.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "timehead.h"

#include <sys/file.h>

#include "parseconf.h"
#include "nut_ctype.h"
#include "upstype.h"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

typedef enum {
	HANDLER_INVALID = 0,
	HANDLER_DRIVER,
	HANDLER_CLIENT,
	HANDLER_SERVER
} handler_type_t;

typedef enum {
	POLLER_EVENT_ERROR,
	POLLER_EVENT_READY
} poller_event_t;

extern const char* handler_type_string[];

void poller_register_fd(int fd, handler_type_t type, void *data);
void poller_unregister_fd(int fd);
void poller_cleanup(void);
void poller_reload(void);
void poller_mainloop(void);

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif	/* POLLER_H_SEEN */
