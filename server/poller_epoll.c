/* poller_epoll.c - epoll implementation of poller for Linux

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

#include <sys/epoll.h>

#include "stype.h"
#include "sstate.h"

typedef struct {
	handler_type_t	type;
	void		*data;
} handler_t;

	/* epollfd */
	static int			epoll_fd;
	static struct epoll_event	*events = NULL;
	static handler_t		*handlers = NULL;
	static int			num_fds;

void poller_register_fd(int fd, handler_type_t type, void *data)
{
	struct epoll_event ev;

	if (fd < maxconn) {
		ev.events = EPOLLIN;
		ev.data.fd = fd;
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
			upsdebugx(2, "%s: failed to register %s fd %d, call to epoll_ctl() failed", __func__, handler_type_string[type], fd);
			return;
		}

		handlers[fd].type = type;
		handlers[fd].data = data;

		num_fds++;

		upsdebugx(3, "%s: registered %s fd %d", __func__, handler_type_string[type], fd);
	}
	else {
		upsdebugx(2, "%s: failed to register %s fd %d, out of free slots", __func__, handler_type_string[type], fd);
	}
}

void poller_unregister_fd(int fd)
{
	if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) == -1) {
		upsdebugx(2, "%s: call to epoll_ctl() failed", __func__);
	}
	handlers[fd].type = HANDLER_INVALID;
	upsdebugx(3, "%s: unregistered %s fd %d", __func__, handler_type_string[handlers[fd].type], fd);
	num_fds--;
}

void poller_cleanup(void)
{
	free(events);
	free(handlers);
	close(epoll_fd);
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

	if (!epoll_fd) {
		epoll_fd = epoll_create1(0);
		if (epoll_fd == -1) {
			fatalx(EXIT_FAILURE, "Couldn't allocate epoll file descriptor, can't proceed without one.");
		}
	}
	handlers = xrealloc(handlers, maxconn * sizeof(*handlers));
	events = xrealloc(events, maxconn * sizeof(*events));

	upsdebugx(3, "%s: set fd limit to %d", __func__, maxconn);
}

/* service requests and check on new data */
void poller_mainloop(void)
{
	int			i, ret;
	struct epoll_event	*event;
	handler_t		*handler;
	poller_event_t		poller_event;

	poller_tick();

	upsdebugx(2, "%s: epolling %d file descriptors", __func__, num_fds);
	ret = epoll_wait(epoll_fd, events, maxconn, 2000);

	if (ret == 0) {
		upsdebugx(2, "%s: no data available", __func__);
		return;
	}
	else if (ret < 0) {
		upslog_with_errno(LOG_ERR, "%s", __func__);
		return;
	}

	for (i = 0; i < ret; i++) {
		event = &events[i];
		handler = &handlers[event->data.fd];

		if (event->events & (EPOLLHUP|EPOLLERR)) {
			poller_event = POLLER_EVENT_ERROR;
		}
		else if (event->events & EPOLLIN) {
			poller_event = POLLER_EVENT_READY;
		}
		else {
			continue;
		}

		poller_callback(handler->type, handler->data, poller_event, event->data.fd);
	}
}
