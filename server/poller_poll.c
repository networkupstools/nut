/* poller_poll.c - poll implementation of poller for Unixes

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

#include "upsd.h"
#include "upstype.h"
#include "conf.h"

#include "upsconf.h"

#include <poll.h>

#include "stype.h"
#include "sstate.h"

typedef struct {
	handler_type_t	type;
	void		*data;
} handler_t;

	/* pollfd */
	static struct pollfd	*events = NULL;
	static handler_t	*handlers = NULL;
	static int		num_fds;

void poller_register_fd(int fd, handler_type_t type, void *data)
{
	if (num_fds < maxconn) {
		events[num_fds].fd = fd;
		events[num_fds].events = POLLIN;

		handlers[num_fds].type = type;
		handlers[num_fds].data = data;

		num_fds++;

		upsdebugx(3, "%s: registered %s fd %d", __func__, handler_type_string[type], fd);
	}
	else {
		upsdebugx(2, "%s: failed to register %s fd %d, out of free slots", __func__, handler_type_string[type], fd);
	}
}

void poller_unregister_fd(int fd)
{
	int	i;

	for (i = 0; i < num_fds; i++) {
		if (events[i].fd == fd) {
			upsdebugx(3, "%s: unregistered %s fd %d", __func__, handler_type_string[handlers[i].type], fd);
			memmove(&events[i], &events[i+1], sizeof(*events) * (num_fds - i - 1));
			memmove(&handlers[i], &handlers[i+1], sizeof(*handlers) * (num_fds - i - 1));
			num_fds--;
			return;
		}
	}

	upsdebugx(2, "%s: tried to unregister unknown fd %d", __func__, fd);
}

void poller_cleanup(void)
{
	free(events);
	free(handlers);
}

void poller_reload(void)
{
	int	ret;

	ret = sysconf(_SC_OPEN_MAX);

	if (ret < maxconn) {
		fatalx(EXIT_FAILURE,
			"Your system limits the maximum number of connections to %d\n"
			"but you requested %d. The server won't start until this\n"
			"problem is resolved.\n", ret, maxconn);
	}

	events = xrealloc(events, maxconn * sizeof(*events));
	handlers = xrealloc(handlers, maxconn * sizeof(*handlers));

	upsdebugx(3, "%s: set fd limit to %d", __func__, maxconn);
}

/* service requests and check on new data */
void poller_mainloop(void)
{
	int		i, ret;
	poller_event_t	poller_event;

	poller_tick();

	upsdebugx(2, "%s: polling %d file descriptors", __func__, num_fds);

	ret = poll(events, num_fds, 2000);

	if (ret == 0) {
		upsdebugx(2, "%s: no data available", __func__);
		return;
	}
	else if (ret < 0) {
		upslog_with_errno(LOG_ERR, "%s", __func__);
		return;
	}

	for (i = 0; i < num_fds; i++) {
		if (events[i].revents & (POLLHUP|POLLERR|POLLNVAL)) {
			poller_event = POLLER_EVENT_ERROR;
		}
		else if (events[i].revents & POLLIN) {
			poller_event = POLLER_EVENT_READY;
		}
		else {
			continue;
		}

		poller_callback(handlers[i].type, handlers[i].data, poller_event, events[i].fd);
	}
}
