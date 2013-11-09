/* dstate-hal.h - Network UPS Tools driver-side state management

   Copyright (C) 2006  Arnaud Quette <aquette.dev@gmail.com>

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

#ifndef DSTATE_HAL_H_SEEN
#define DSTATE_HAL_H_SEEN 1

#include "state.h"
#include "attribute.h"

/*#include "parseconf.h"*/
#include "upshandler.h"

#include <glib.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <hal/libhal.h>

#define DS_LISTEN_BACKLOG 16
#define DS_MAX_READ 256		/* don't read forever from upsd */

/* HAL specific */
#define DBUS_INTERFACE "org.freedesktop.Hal.Device.UPS"

DBusHandlerResult dbus_filter_function(DBusConnection *connection,
					      DBusMessage *message,
					      void *user_data);

gboolean	dbus_init_local		(void);

#define HAL_WARNING 

/* track client connections */
/* typedef struct conn_s {
 *	int     fd;
 *	PCONF_CTX_t	ctx;
 *	struct conn_s *next;
 *} conn_t; 
 */
	extern	struct	ups_handler	upsh;

void dstate_init(const char *prog, const char *port);
int dstate_poll_fds(struct timeval timeout, int extrafd);
int dstate_setinfo(const char *var, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 2, 3)));
int dstate_addenum(const char *var, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 2, 3)));
int dstate_addrange(const char *var, const int min, const int max);
void dstate_setflags(const char *var, int flags);
void dstate_setaux(const char *var, int aux);
const char *dstate_getinfo(const char *var);
void dstate_addcmd(const char *cmdname);
int dstate_delinfo(const char *var);
int dstate_delenum(const char *var, const char *val);
int dstate_delcmd(const char *cmd);
void dstate_free(void);
const st_tree_t *dstate_getroot(void);
const cmdlist_t *dstate_getcmdlist(void);

void dstate_dataok(void);
void dstate_datastale(void);

int dstate_is_stale(void);

/* clean out the temp space for a new pass */
void status_init(void);

/* add a status element */
void status_set(const char *buf);

/* write the temporary status_buf into ups.status */
void status_commit(void);

/* similar functions for ups.alarm */
void alarm_init(void);
void alarm_set(const char *buf);
void alarm_commit(void);

#endif	/* DSTATE_HAL_H_SEEN */
