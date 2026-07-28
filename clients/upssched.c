/* upssched.c - upsmon's scheduling helper for offset timers

   Copyright (C)
	2000       Russell Kroll <rkroll@exploits.org>
	2005-2012  Arnaud Quette <arnaud.quette@free.fr>
	2006       Charles Lepple <clepple+nut@gmail.com>
	2006-2019  Arjen de Korte <adkorte-guest@alioth.debian.org>
	2006-2007  Peter Selinger <selinger@users.sourceforge.net>
	2010-2012  Frederic BOHE <fredericbohe@eaton.com>
	2020-2026  Jim Klimov <jimklimov+nut@gmail.com>
	2022       Dimitris Economou <dimitris.s.economou@gmail.com>

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

/* design notes for the curious:
 *
 * 1. we get called with a ups_name and notify_type from upsmon
 *    (and notify_msg via first non-option argv[] element if
 *    present and not trivial)
 * 2. the config file is searched for an AT condition that matches
 * 3. the conditions on any matching lines are parsed
 *
 * starting a timer: the timer is added to the daemon's timer queue
 * cancelling a timer: the timer is removed from that queue
 * execute a command: the command is passed straight to the cmdscript
 *
 * if the daemon is not already running and is required (to start a timer)
 * it will be started automatically
 *
 * when the time arrives, the command associated with a timer will be
 * executed by the daemon (via the cmdscript)
 *
 * timers can be cancelled at any time before they trigger
 *
 * the daemon will shut down automatically when no more timers are active
 *
 */

#include "common.h"

#include <sys/types.h>
#ifndef WIN32
# include <sys/wait.h>
# include <sys/socket.h>
# include <sys/un.h>
# include <netinet/in.h>
# include <unistd.h>
# include <fcntl.h>
# include <poll.h>
#else	/* WIN32 */
# include "wincompat.h"
# include <winsock2.h>
# include <ws2tcpip.h>
#endif	/* WIN32 */

#include "upssched.h"
#include "timehead.h"
#include "nut_stdint.h"

typedef struct ttype_s {
	char	*name;
	time_t	etime;
	char	**upsnames;		/* List of unique UPSNAME values that commanded to start this timer name */
	char	**notifytypes;	/* List of unique NOTIFYTYPE values that commanded to start this timer name */
	char	**notifymsgs;	/* List of unique NOTIFYMSG values that commanded to start this timer name */
	struct ttype_s	*next;
} ttype_t;

static ttype_t	*thead = NULL;
static conn_t	*connhead = NULL;
static char	*pipefn = NULL, *lockfn = NULL;
/* Argument array for respective program, where [0] is the program name,
 * followed by (argc-1) possible command-line argument tokens, with a
 * NULL value in the end as the [argc]'th entry. Overall (argc+1) items
 * if at all populated, minimum 2 for the program name and NULL sentinel.
 * Concatenated value is also stored to ease debug logging, but is not
 * used directly for program calls. */
static char	**cmdscript_argv = NULL, *cmdscript_concat = NULL;
static size_t	cmdscript_argc = 0;
static int	nut_debug_level_args = 0, nut_debug_level_env = 0, nut_debug_level_conf = 0;
static int	list_timers = 0;

#ifdef WIN32
/* WIN32-only: set when THIS process instance is itself the freshly
 * spawned timer-daemon child (see WIN32_DAEMON_MARKER below), as
 * opposed to being the short-lived CLI process invoked by upsmon.
 *
 * This flag makes conf_arg() treat the config file the same way it
 * already does for the "-l" (list_timers) CLI switch: parse the
 * global upssched.conf directives (PIPEFN, LOCKFN, CMDSCRIPT,
 * DEBUG_MIN), but skip acting on "AT" lines (i.e. skip parse_at()
 * and, transitively, sendcmd()) -- a daemon child must never re-send
 * the notification that originally caused upsmon to invoke this
 * program; that is precisely the bug this patch addresses. */
static int	is_win32_timer_daemon = 0;

/* Hidden/internal argv[1] marker, used only for the WIN32 replacement
 * of fork(). CreateProcess() cannot split one already-running process
 * into a "parent continues here" / "child takes over from here" pair
 * the way fork() does on POSIX (see the "if (pid != 0) { ...; return; }"
 * branch in start_daemon() below) -- it always starts a brand-new
 * process at main(). We therefore spawn a new process and tell it,
 * via this marker (checked at the very top of main(), before normal
 * option/config parsing), that it must go straight to being the timer
 * daemon instead of re-running the normal CLI codepath (which would
 * re-parse the config and potentially re-send the very notification
 * that caused upsmon to invoke this program -- the original cause of
 * the runaway process cascade this patch fixes). */
#define WIN32_DAEMON_MARKER	"--nut-upssched-win32-timer-daemon"

/* HANDLE of a manual-reset Win32 event object, inherited from the
 * spawning process, used as a one-shot "I am listening on the named
 * pipe now" readiness signal from the daemon child back to the
 * process that spawned it (see start_daemon() and
 * win32_run_timer_daemon() below). This is the WIN32 counterpart of
 * us_serialize(SERIALIZE_WAIT/SET), which is implemented below with a
 * plain POSIX pipe(2) and has no WIN32 implementation.
 * Populated from argv[2] in main() when is_win32_timer_daemon is set;
 * remains NULL otherwise. */
static HANDLE	win32_daemon_ready_event = NULL;
#endif	/* WIN32 */

/* ups name and notify type (string) as received from upsmon */
static const	char	*ups_name = NULL, *notify_type = NULL, *notify_msg = NULL, *prog = NULL;

#ifdef WIN32
static OVERLAPPED connect_overlapped;
# define BUF_LEN 512
#endif	/* WIN32 */

#define PARENT_STARTED		-2
#define PARENT_UNNECESSARY	-3
#define MAX_TRIES 		30
#define EMPTY_WAIT		15	/* min passes with no timers to exit */
#define US_LISTEN_BACKLOG	16
#define US_SOCK_BUF_LEN		256
#define US_MAX_READ		128

/* --- server functions --- */

static void exec_cmd(const char *cmd)
{
#ifndef WIN32
	int	waitstatus = 0;
	pid_t	pid, waitret;
#else
	int	err = 0;
#endif
	char	**argv = NULL;

	if (cmdscript_argc == 0) {
		upslogx(LOG_ERR, "No CMDSCRIPT defined, cannot execute command: %s", NUT_STRARG(cmd));
		return;
	}

	/* For logging note that cmdscript_concat is quoted as appropriate */
	upsdebugx(4, "%s: calling: %s \"%s\"", __func__, NUT_STRARG(cmdscript_concat), NUT_STRARG(cmd));

#ifndef WIN32
	pid = fork();
	if (pid < 0) {
		upslog_with_errno(LOG_ERR, "fork() failed in exec_cmd");
		return;
	}

	if (pid > 0) {
		/* parent process - wait for child */
		waitret = waitpid(pid, &waitstatus, 0);
		if (waitret < 0) {
			upslog_with_errno(LOG_ERR, "waitpid(%d) failed", (int)pid);
			return;
		}

		if (WIFEXITED(waitstatus)) {
			if (WEXITSTATUS(waitstatus)) {
				upslogx(LOG_INFO, "exec_cmd(%s '%s') returned %d",
					NUT_STRARG(cmdscript_concat), NUT_STRARG(cmd), WEXITSTATUS(waitstatus));
			}
		} else {
			if (WIFSIGNALED(waitstatus)) {
				upslogx(LOG_WARNING, "exec_cmd(%s '%s') terminated with signal %d",
					NUT_STRARG(cmdscript_concat), NUT_STRARG(cmd), WTERMSIG(waitstatus));
			} else {
				upslogx(LOG_ERR, "Execute command failure: %s '%s'",
					NUT_STRARG(cmdscript_concat), NUT_STRARG(cmd));
			}
		}

		upsdebugx(3, "%s: returned status %d", __func__, waitstatus);
		return;
	}
#endif	/* !WIN32 */

	/* Only deal with argv for child or mono-process */
	/* We will add one argument, plus one NULL sentinel, after argc original entries */
	argv = (char**)xcalloc(cmdscript_argc + 2, sizeof(char *));
	/* Copy entries [0]..[argc-1] */
	memcpy(argv, cmdscript_argv, cmdscript_argc * sizeof(char *));
	argv[cmdscript_argc] = (char *)cmd;
	argv[cmdscript_argc + 1] = NULL;

#ifndef WIN32
	/* child process */
	execvp(cmdscript_argv[0], argv);
	/* execvp() only returns on error */
	upslog_with_errno(LOG_ERR, "execvp(%s) failed", NUT_STRARG(cmdscript_concat));
	free(argv);
	exit(EXIT_FAILURE);
#else	/* WIN32 */
	/* Use _spawnvp for Windows */
	err = _spawnvp(_P_WAIT, cmdscript_argv[0], (const char * const *)argv);
	if (err != -1) {
		upslogx(LOG_INFO, "Execute command %s \"%s\" OK", NUT_STRARG(cmdscript_concat), NUT_STRARG(cmd));
		upsdebugx(3, "%s: returned status %d", __func__, err);
	} else {
		upslog_with_errno(LOG_ERR, "Execute command %s \"%s\" failure", NUT_STRARG(cmdscript_concat), NUT_STRARG(cmd));
	}
	free(argv);
#endif	/* WIN32 */
}

/* Collect the list of strings into a "sep" (e.g. comma) separated string.
 * If the return value is not NULL, caller should free() this.
 * TODO: Generalize and move to common as some snprintfcatext()?
 */
static char* collect_string(char **string_arr, char *logtag, char *sep, size_t *pBufsize, size_t *pCount)
{
	size_t	bufsize = SMALLBUF, prevlen = 0, count = 0;
	char	*buf = NULL, **ptr, *s;
	int	ret_printf;

	/* Do we have at least one non-trivial string there? */
	if (!string_arr || !(*string_arr) || !(**string_arr))
		return NULL;

	buf = (char *)xcalloc(bufsize, sizeof(char));
	if (!buf) {
		upsdebugx(1, "%s: failed to allocate buffer, will not report any %s values", __func__, logtag);
		return NULL;
	}

	for (ptr = string_arr; ptr != NULL; ptr++) {
		int	retry;

		s = *ptr;
		upsdebugx(5, "%s: popped '%s'", __func__, NUT_STRARG(s));

		if (s == NULL || *s == '\0')
			break; /*continue?*/

		upsdebugx(4, "%s: appending '%s' to buffer %" PRIuSIZE "/%" PRIuSIZE " full",
			__func__, s, prevlen, bufsize);
		do {
			retry = 0;
			ret_printf = snprintf(
				buf + prevlen,
				bufsize - prevlen - 1,
				"%s%s",
				count ? (sep ? sep : ",") : "",
				s);
			upsdebugx(4, "%s: got %d after adding into buffer %" PRIuSIZE "/%" PRIuSIZE " full",
				__func__, ret_printf, prevlen, bufsize);
			buf[bufsize - 1] = '\0';

			if (ret_printf < 0) {
				upsdebugx(1, "%s: error collecting names, might not report all %s values", __func__, logtag);
				buf[prevlen] = '\0';
			} else if ((size_t)ret_printf + prevlen >= bufsize) {
				if (bufsize < SIZE_MAX - LARGEBUF) {
					bufsize += LARGEBUF;
					upsdebugx(1, "%s: buffer overflowed, trying to re-allocate as %" PRIuSIZE, __func__, bufsize);
					buf = (char *)realloc(buf, bufsize);

					if (!buf) {
						upsdebugx(1, "%s: buffer overflowed and failed to re-allocate, will not report any %s values", __func__, logtag);
						return NULL;
					} else {
						upsdebugx(5, "%s: buffer overflowed, but re-allocated successfully - retrying", __func__);
						/* Retry this loop */
						retry = 1;
					}
				} else {
					upsdebugx(1, "%s: buffer overflowed, might not report all %s values", __func__, logtag);
					buf[prevlen] = '\0';
				}
			} else {
				prevlen += (size_t)ret_printf;
			}
		} while (retry);

		count++;
	}

	upsdebugx(3, "%s: collected %" PRIuSIZE " items into %" PRIuSIZE " bytes: %s",
		__func__, count, bufsize, buf);

	if (pBufsize)
		*pBufsize = bufsize;

	if (pCount)
		*pCount = count;

	upsdebugx(5, "%s: returning", __func__);
	return buf;
}

static void exec_cmd_timer(ttype_t *item)
{
	char	*upsnames = NULL, *notifytypes = NULL, *notifymsgs = NULL;
	size_t	upsnames_count = 0, notifytypes_count = 0, notifymsgs_count = 0;

	if (!item || !item->name || !(*(item->name))) {
		upsdebugx(1, "%s: SKIP bad call with null arg or its command name", __func__);
		return;
	}

	/* Do we have at least one non-trivial string there? */
	if (item->upsnames && *(item->upsnames) && **(item->upsnames)) {
		upsnames = collect_string(item->upsnames, "UPSNAME", ",", NULL, &upsnames_count);
	}

	if (item->notifytypes && *(item->notifytypes) && **(item->notifytypes)) {
		notifytypes = collect_string(item->notifytypes, "NOTIFYTYPE", ",", NULL, &notifytypes_count);
	}

	if (item->notifymsgs && *(item->notifymsgs) && **(item->notifymsgs)) {
		notifymsgs = collect_string(item->notifymsgs, "NOTIFYMSG", ".\t", NULL, &notifymsgs_count);
	}

	if (upsnames)
		setenv("UPSNAME", upsnames, 1);

	if (notifytypes)
		setenv("NOTIFYTYPE", notifytypes, 1);

	if (notifymsgs)
		setenv("NOTIFYMSG", notifymsgs, 1);

	if (nut_debug_level)
		upslogx(LOG_INFO, "Executing command by timer: %s\t[%s]\t[%s]\t[%s]",
			item->name, NUT_STRARG(notifytypes), NUT_STRARG(upsnames), NUT_STRARG(notifymsgs));
	exec_cmd(item->name);
	upsdebugx(3, "%s: returned from exec_cmd()", __func__);

	/* Timer process should not retain random envvars */
	if (upsnames) {
		unsetenv("UPSNAME");
		free(upsnames);
	}

	if (notifytypes) {
		unsetenv("NOTIFYTYPE");
		free(notifytypes);
	}

	if (notifymsgs) {
		unsetenv("NOTIFYMSG");
		free(notifymsgs);
	}

	upsdebugx(3, "%s: done", __func__);
}

static void removetimer(ttype_t *tfind)
{
	ttype_t	*tmp, *last;

	last = NULL;
	tmp = thead;

	while (tmp) {
		if (tmp == tfind) {	/* found it */
			upsdebugx(5, "%s: found %s", __func__, NUT_STRARG(tmp->name));
			if (last == NULL)	/* deleting first */
				thead = tmp->next;
			else
				last->next = tmp->next;

			if (tmp->upsnames) {
				char **ps;
				for (ps = tmp->upsnames; *ps != NULL; ps++) {
					free(*ps);
				}
				free(tmp->upsnames);
			}

			if (tmp->notifytypes) {
				char **ps;
				for (ps = tmp->notifytypes; *ps != NULL; ps++) {
					free(*ps);
				}
				free(tmp->notifytypes);
			}

			if (tmp->notifymsgs) {
				char **ps;
				for (ps = tmp->notifymsgs; *ps != NULL; ps++) {
					free(*ps);
				}
				free(tmp->notifymsgs);
			}

			upsdebugx(3, "%s: forgetting %s", __func__, tmp->name);
			free(tmp->name);
			free(tmp);
			return;
		}

		last = tmp;
		tmp = tmp->next;
	}

	/* this one should never happen */

	upslogx(LOG_ERR, "removetimer: failed to locate target at %p", (void *)tfind);
}

static void checktimers(void)
{
	ttype_t	*tmp, *tmpnext;
	time_t	now;
	static	int	emptyctr = 0;

	upsdebugx(3, "%s: starting", __func__);

	/* if the queue is empty we might be ready to exit */
	if (!thead) {

		emptyctr++;

		/* wait a little while in case someone wants us again */
		if (emptyctr < EMPTY_WAIT)
			return;

		if (nut_debug_level)
			upslogx(LOG_INFO, "Timer queue empty, exiting");

#ifdef UPSSCHED_RACE_TEST
		upslogx(LOG_INFO, "triggering race: sleeping 15 sec before exit");
		sleep(15);
#endif

		upsdebugx(1, "Timer queue empty, closing pipe and exiting upssched daemon");
		unlink(pipefn);
		exit(EXIT_SUCCESS);
	}

	emptyctr = 0;

	/* flip through LL, look for activity */
	tmp = thead;

	time(&now);
	while (tmp) {
		tmpnext = tmp->next;

		if (now >= tmp->etime) {
			if (nut_debug_level)
				upslogx(LOG_INFO, "Event: %s ", tmp->name);

			exec_cmd_timer(tmp);

			/* delete from queue */
			upsdebugx(5, "%s: removing timer for the event just handled", __func__);
			removetimer(tmp);
			upsdebugx(5, "%s: removed timer for the event just handled", __func__);
		}

		tmp = tmpnext;
	}

	upsdebugx(3, "%s: done", __func__);
}

static void start_timer(const char *name, const char *ofsstr, const char *notifytype, const char *upsname, const char *notifymsg, int shared_timer)
{
	time_t	now;
	long	ofs;
	ttype_t	*tmp, *last = NULL;

	/* get the time */
	time(&now);

	/* add an event for <now> + <time> */
	ofs = strtol(ofsstr, (char **) NULL, 10);

	if (ofs < 0) {
		upslogx(LOG_INFO, "bogus offset for timer, ignoring");
		return;
	}

	if (shared_timer) {
		/* See if there is an older entry to attach to,
		 * otherwise fall through to creating a new one */
		tmp = last = thead;
		upsdebugx(3, "%s: searching for existing timer named '%s' to share", __func__, name);

		while (tmp) {
			if (tmp->name && !strcmp(tmp->name, name)) {
				if (nut_debug_level)
					upslogx(LOG_INFO, "Append data to shared timer: %s\t[%s]\t[%s]\t[%s]\t(will elapse in %g seconds)",
						name, NUT_STRARG(notifytype), NUT_STRARG(upsname), NUT_STRARG(notifymsg),
						difftime(tmp->etime, now));

				/* FIXME? Consider only the first hit as the shared timer?
				 *  Or check if there is already a copy with same name elsewhere?
				 */
				if (notifytype && *notifytype) {
					if (tmp->notifytypes) {
						char	**ps = NULL;
						size_t	count = 0;	/* amount of non-NULL entries, if we get to the end */

						for (ps = tmp->notifytypes; *ps != NULL ; ps++) {
							count++;
							if (!strcmp(*ps, notifytype))
								break;
						}

						if (*ps == NULL) {
							tmp->notifytypes = (char **)xrealloc(tmp->notifytypes, count + 2);
							tmp->notifytypes[count] = xstrdup(notifytype);
							tmp->notifytypes[count + 1] = NULL;
						}
					} else {
						tmp->notifytypes = (char **)xcalloc(2, sizeof(char*));
						tmp->notifytypes[0] = xstrdup(notifytype);
						tmp->notifytypes[1] = NULL;
					}
				}

				if (notifymsg && *notifymsg) {
					if (tmp->notifymsgs) {
						char	**ps = NULL;
						size_t	count = 0;	/* amount of non-NULL entries, if we get to the end */

						for (ps = tmp->notifymsgs; *ps != NULL ; ps++) {
							count++;
							if (!strcmp(*ps, notifymsg))
								break;
						}

						if (*ps == NULL) {
							tmp->notifymsgs = (char **)xrealloc(tmp->notifymsgs, count + 2);
							tmp->notifymsgs[count] = xstrdup(notifymsg);
							tmp->notifymsgs[count + 1] = NULL;
						}
					} else {
						tmp->notifymsgs = (char **)xcalloc(2, sizeof(char*));
						tmp->notifymsgs[0] = xstrdup(notifymsg);
						tmp->notifymsgs[1] = NULL;
					}
				}

				if (upsname && *upsname) {
					if (tmp->upsnames) {
						char	**ps = NULL;
						size_t	count = 0;	/* amount of non-NULL entries, if we get to the end */

						for (ps = tmp->upsnames; *ps != NULL ; ps++) {
							count++;
							if (!strcmp(*ps, upsname))
								break;
						}

						if (*ps == NULL) {
							tmp->upsnames = (char **)xrealloc(tmp->upsnames, count + 2);
							tmp->upsnames[count] = xstrdup(upsname);
							tmp->upsnames[count + 1] = NULL;
						}
					} else {
						tmp->upsnames = (char **)xcalloc(2, sizeof(char*));
						tmp->upsnames[0] = xstrdup(upsname);
						tmp->upsnames[1] = NULL;
					}
				}

				return;
			}

			/* Looping anyway, prepare for fall-through if we get to it */
			last = tmp;
			tmp = tmp->next;
		}
	}

	if (nut_debug_level)
		upslogx(LOG_INFO, "New timer: %s\t[%s]\t[%s]\t[%s]\t(will elapse in %ld seconds)",
			name, NUT_STRARG(notifytype), NUT_STRARG(upsname), NUT_STRARG(notifymsg), ofs);

	/* now add to the queue */
	if (!shared_timer) {
		tmp = last = thead;

		while (tmp) {
			last = tmp;
			tmp = tmp->next;
		}
	}	/* else we already know */

	tmp = (ttype_t *)xmalloc(sizeof(ttype_t));
	tmp->name = xstrdup(name);
	tmp->etime = now + ofs;
	tmp->notifytypes = NULL;
	tmp->notifymsgs = NULL;
	tmp->upsnames = NULL;
	tmp->next = NULL;

	if (notifytype && *notifytype) {
		tmp->notifytypes = (char **)xcalloc(2, sizeof(char*));
		tmp->notifytypes[0] = xstrdup(notifytype);
		tmp->notifytypes[1] = NULL;
	}

	if (notifymsg && *notifymsg) {
		tmp->notifymsgs = (char **)xcalloc(2, sizeof(char*));
		tmp->notifymsgs[0] = xstrdup(notifymsg);
		tmp->notifymsgs[1] = NULL;
	}

	if (upsname && *upsname) {
		tmp->upsnames = (char **)xcalloc(2, sizeof(char*));
		tmp->upsnames[0] = xstrdup(upsname);
		tmp->upsnames[1] = NULL;
	}

	if (last)
		last->next = tmp;
	else
		thead = tmp;
}

static void cancel_timer(const char *name, const char *cname, const char *notifytype, const char *upsname, const char *notifymsg, int do_cancel_matched)
{
	ttype_t	*tmp;
	size_t	removed = 0;

	/* TOTHINK: Only cancel events associated with a particular UPS and/or type? */
	NUT_UNUSED_VARIABLE(notifytype);
	NUT_UNUSED_VARIABLE(upsname);

	for (tmp = thead; tmp != NULL; tmp = tmp->next) {
		if (!strcmp(tmp->name, name)) {		/* match */
			/* Note we do not match "notifymsg" as it likely differs */
			if (!do_cancel_matched
			||  (   (!notifytype || !(*notifytype))
				 && (!upsname || !(*upsname)) )
			) {
				upsdebugx(2, "%s: cancelling of timer %s not constrained by caller's NOTIFYTYPE nor UPSNAME", __func__, name);
			} else {
				char	**ps = NULL;
				int	matched = 0;

				/* FIXME: Do not remove whole timer, just the respective strings?
				 *  (Do drop the timer only if none remain) */
				if (notifytype) {
					matched = 0;
					if (!(tmp->notifytypes)) {
						upsdebugx(2, "%s: do not cancel timer %s due to lack of NOTIFYTYPE in it", __func__, name);
						continue;
					}
					for (ps = tmp->notifytypes; *ps != NULL ; ps++) {
						if (!strcmp(*ps, notifytype)) {
							matched = 1;
							break;
						}
					}
					if (!matched) {
						upsdebugx(2, "%s: do not cancel timer %s due to mismatch vs. caller's NOTIFYTYPE", __func__, name);
						continue;
					}
				}

				if (upsname) {
					matched = 0;
					if (!(tmp->upsnames)) {
						upsdebugx(2, "%s: do not cancel timer %s due to lack of UPSNAME in it", __func__, name);
						continue;
					}
					for (ps = tmp->upsnames; *ps != NULL ; ps++) {
						if (!strcmp(*ps, upsname)) {
							matched = 1;
							break;
						}
					}
					if (!matched) {
						upsdebugx(2, "%s: do not cancel timer %s due to mismatch vs. caller's UPSNAME", __func__, name);
						continue;
					}
				}
			}

			if (nut_debug_level) {
				if (notifymsg && *notifymsg) {
					upslogx(LOG_INFO, "Cancelling timer: %s: %s", name, notifymsg);
				} else {
					upslogx(LOG_INFO, "Cancelling timer: %s", name);
				}
			}
			removetimer(tmp);
			removed++;

			/* Go on, we want to continue and cancel possibly many
			 * timers with this name (modulo constraints by envvars) */
		}
	}
	if (removed > 0)
		return;

	/* this is not necessarily an error: per docs,
	 * if the timer has passed then pass the optional argument cmd to CMDSCRIPT.
	 */
	if (cname && cname[0]) {
		if (nut_debug_level)
			upslogx(LOG_INFO, "Cancel %s, event: %s", name, cname);

		exec_cmd(cname);
	}
}

#ifndef WIN32
static void us_serialize(int op)
{
	static	int	pipefd[2];
	ssize_t	ret;
	char	ch;

	switch(op) {
		case SERIALIZE_INIT:
			ret = pipe(pipefd);

			if (ret != 0)
				fatal_with_errno(EXIT_FAILURE, "serialize: pipe");

			break;

		case SERIALIZE_SET:
			close(pipefd[0]);
			close(pipefd[1]);
			break;

		case SERIALIZE_WAIT:
			close(pipefd[1]);
			ret = read(pipefd[0], &ch, 1);
			close(pipefd[0]);
			break;

		default:
			break;
	}
}
#endif	/* !WIN32 */

static TYPE_FD open_sock(void)
{
	TYPE_FD fd;

#ifndef WIN32
	int	ret;
	struct	sockaddr_un	ssaddr;

	check_unix_socket_filename(pipefn);
	fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (INVALID_FD(fd))
		fatal_with_errno(EXIT_FAILURE, "Can't create a unix domain socket");

	ssaddr.sun_family = AF_UNIX;
	snprintf(ssaddr.sun_path, sizeof(ssaddr.sun_path), "%s", pipefn);

	unlink(pipefn);

	umask(0007);

	ret = bind(fd, (struct sockaddr *) &ssaddr, sizeof ssaddr);

	if (ret < 0)
		fatal_with_errno(EXIT_FAILURE, "bind %s failed", pipefn);

	ret = chmod(pipefn, 0660);

	if (ret < 0)
		fatal_with_errno(EXIT_FAILURE, "chmod(%s, 0660) failed", pipefn);

	ret = listen(fd, US_LISTEN_BACKLOG);

	if (ret < 0)
		fatal_with_errno(EXIT_FAILURE, "listen(%d, %d) failed", fd, US_LISTEN_BACKLOG);

	/* don't leak socket to CMDSCRIPT */
	set_close_on_exec(fd);

#else /* WIN32 */
	SECURITY_ATTRIBUTES	pipe_sa;
	SECURITY_DESCRIPTOR	pipe_sd;

	init_pipe_security(&pipe_sa, &pipe_sd);

	fd = CreateNamedPipe(
			pipefn,		/* pipe name */
			PIPE_ACCESS_DUPLEX	/* read/write access */
			| FILE_FLAG_OVERLAPPED,	/* async IO */
			PIPE_TYPE_BYTE
			| PIPE_READMODE_BYTE
			| PIPE_REJECT_REMOTE_CLIENTS	/* local host only */
			| PIPE_WAIT,
			PIPE_UNLIMITED_INSTANCES,	/* max. instances */
			BUF_LEN,	/* output buffer size */
			BUF_LEN,	/* input buffer size */
			0,		/* client time-out */
			&pipe_sa);	/* default security attributes */

	if (INVALID_FD(fd)) {
		fatal_with_errno(EXIT_FAILURE,
			"Can't create a state socket (windows named pipe)");
	}

	/* Prepare an async wait on a connection on the pipe */
	memset(&connect_overlapped,0,sizeof(connect_overlapped));
	connect_overlapped.hEvent = CreateEvent(NULL, /*Security*/
			FALSE, /* auto-reset*/
			FALSE, /* inital state = non signaled*/
			NULL /* no name*/);
	if (connect_overlapped.hEvent == NULL) {
		fatal_with_errno(EXIT_FAILURE, "Can't create event");
	}

	/* Wait for a connection */
	ConnectNamedPipe(fd,&connect_overlapped);
#endif /* WIN32 */

	return fd;
}

static void conn_del(conn_t *target)
{
	conn_t	*tmp, *last = NULL;

	if (!target)
		return;

#ifndef WIN32
	upsdebugx(3, "%s: closing connection %d", __func__, target->fd);
#endif
	tmp = connhead;

	while (tmp) {
		if (tmp == target) {

			if (last)
				last->next = tmp->next;
			else
				connhead = tmp->next;

			pconf_finish(&tmp->ctx);

			free(tmp);
			return;
		}

		last = tmp;
		tmp = tmp->next;
	}

	upslogx(LOG_ERR, "Tried to delete a bogus state connection");
}

static int send_to_one(conn_t *conn, const char *fmt, ...)
{
	ssize_t	ret;
	size_t	buflen;
	va_list	ap;
	char	buf[US_SOCK_BUF_LEN];

	va_start(ap, fmt);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic push
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_PRAGMA_GCC_DIAGNOSTIC_IGNORED_FORMAT_SECURITY
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
	/* Note: Not converting to hardened NUT methods with dynamic
	 * format string checking, this one is used locally with
	 * fixed strings (and args) */
	/* FIXME: Actually, only fixed strings, no formatting here. */
	vsnprintf(buf, sizeof(buf), fmt, ap);
#ifdef HAVE_PRAGMAS_FOR_GCC_DIAGNOSTIC_IGNORED_FORMAT_NONLITERAL
#pragma GCC diagnostic pop
#endif
	va_end(ap);

	buflen = strlen(buf);
	upsdebugx(5, "%s: sending %" PRIuSIZE " bytes:", __func__, buflen);
	upsdebug_ascii_compact(5, "send_to_one buffer content: ", buf, buflen);

	if (buflen >= SSIZE_MAX) {
		/* Can't compare buflen to ret */
		upsdebugx(2, "send_to_one(): buffered message too large");

		if (VALID_FD(conn->fd)) {
#ifndef WIN32
			close(conn->fd);
#else	/* WIN32 */
			FlushFileBuffers(conn->fd);
			CloseHandle(conn->fd);
#endif	/* WIN32 */
			conn->fd = ERROR_FD;
		}

		conn_del(conn);

		return 0;	/* failed */
	}

#ifndef WIN32
	ret = write(conn->fd, buf, buflen);

	if ((ret < 1) || (ret != (ssize_t) buflen)) {
		upsdebugx(2, "write failed on socket %d, disconnecting", conn->fd);

		close(conn->fd);
		conn_del(conn);

		return 0;	/* failed */
	}
#else	/* WIN32 */
	DWORD bytesWritten = 0;
	BOOL  result = FALSE;

	result = WriteFile (conn->fd, buf, buflen, &bytesWritten, NULL);
	if (result == 0) {
		upsdebugx(2, "write failed on handle %p, disconnecting", conn->fd);

		/* FIXME not sure this is the right way to close a connection */
		if( conn->read_overlapped.hEvent != INVALID_HANDLE_VALUE) {
			CloseHandle(conn->read_overlapped.hEvent);
			conn->read_overlapped.hEvent = INVALID_HANDLE_VALUE;
		}
		DisconnectNamedPipe(conn->fd);
		CloseHandle(conn->fd);
		conn_del(conn);

		return 0;
	}
	else {
		ret = (ssize_t)bytesWritten;
	}

	if ((ret < 1) || (ret != (ssize_t)buflen)) {
		upsdebugx(2, "write to fd %p failed", conn->fd);
		/* FIXME not sure this is the right way to close a connection */
		if (conn->read_overlapped.hEvent != INVALID_HANDLE_VALUE) {
			CloseHandle(conn->read_overlapped.hEvent);
			conn->read_overlapped.hEvent = INVALID_HANDLE_VALUE;
		}
		DisconnectNamedPipe(conn->fd);
		CloseHandle(conn->fd);

		return 0;	/* failed */
	}
#endif /* WIN32 */

	return 1;	/* OK */
}

static TYPE_FD conn_add(TYPE_FD sockfd)
{
	TYPE_FD	acc;

#ifndef WIN32
	int	ret;
	conn_t	*tmp, *last;
	struct	sockaddr_un	saddr;
# if defined(__hpux) && !defined(_XOPEN_SOURCE_EXTENDED)
	int			salen;
# else
	socklen_t	salen;
# endif

	salen = sizeof(saddr);
	acc = accept(sockfd, (struct sockaddr *) &saddr, &salen);

	if (INVALID_FD(acc)) {
		upslog_with_errno(LOG_ERR, "accept on unix fd failed");
		return ERROR_FD;
	}

	/* don't leak connection to CMDSCRIPT */
	set_close_on_exec(acc);

	/* enable nonblocking I/O */

	ret = fcntl(acc, F_GETFL, 0);

	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "fcntl get on unix fd failed");
		close(acc);
		return ERROR_FD;
	}

	ret = fcntl(acc, F_SETFL, ret | O_NDELAY);

	if (ret < 0) {
		upslog_with_errno(LOG_ERR, "fcntl set O_NDELAY on unix fd failed");
		close(acc);
		return ERROR_FD;
	}

	tmp = last = connhead;

	while (tmp) {
		last = tmp;
		tmp = tmp->next;
	}

	tmp = (conn_t *)xmalloc(sizeof(conn_t));
	tmp->fd = acc;
	tmp->next = NULL;

	if (last)
		last->next = tmp;
	else
		connhead = tmp;

	upsdebugx(3, "new connection on fd %d", acc);

	pconf_init(&tmp->ctx, NULL);

#else /* WIN32 */

	conn_t	*conn, *tmp, *last;
	SECURITY_ATTRIBUTES	pipe_sa;
	SECURITY_DESCRIPTOR	pipe_sd;

	/* We have detected a connection on the opened pipe. So we start
	 * by saving its handle and creating a new pipe for future connection */
	conn = xcalloc(1, sizeof(*conn));
	conn->fd = sockfd;

	init_pipe_security(&pipe_sa, &pipe_sd);

	/* sock is the handle of the connection pending pipe */
	acc = CreateNamedPipe(
			pipefn,		/* pipe name */
			PIPE_ACCESS_DUPLEX	/* read/write access */
			| FILE_FLAG_OVERLAPPED,	/* async IO */
			PIPE_TYPE_BYTE
			| PIPE_READMODE_BYTE
			| PIPE_REJECT_REMOTE_CLIENTS	/* local host only */
			| PIPE_WAIT,
			PIPE_UNLIMITED_INSTANCES,	/* max. instances */
			BUF_LEN,	/* output buffer size */
			BUF_LEN,	/* input buffer size */
			0,		/* client time-out */
			&pipe_sa);	/* default security attribute */

	if (INVALID_FD(acc)) {
		fatal_with_errno(EXIT_FAILURE,
			"Can't create a state socket (windows named pipe)");
	}

	/* Prepare a new async wait for a connection on the pipe */
	CloseHandle(connect_overlapped.hEvent);
	memset(&connect_overlapped,0,sizeof(connect_overlapped));
	connect_overlapped.hEvent = CreateEvent(NULL, /*Security*/
			FALSE, /* auto-reset*/
			FALSE, /* inital state = non signaled*/
			NULL /* no name*/);
	if (connect_overlapped.hEvent == NULL) {
		fatal_with_errno(EXIT_FAILURE, "Can't create event");
	}

	/* Wait for a connection */
	ConnectNamedPipe(acc,&connect_overlapped);

	/* A new pipe waiting for new client connection has been created.
	 * We could manage the current connection now.
	 */

	/* Start a read operation on the newly connected pipe so we could
	 * wait on the event associated to this IO */
	memset(&conn->read_overlapped,0,sizeof(conn->read_overlapped));
	memset(conn->buf,0,sizeof(conn->buf));
	conn->read_overlapped.hEvent = CreateEvent(NULL, /*Security*/
			FALSE, /* auto-reset*/
			FALSE, /* inital state = non signaled*/
			NULL /* no name*/);
	if (conn->read_overlapped.hEvent == NULL) {
		fatal_with_errno(EXIT_FAILURE, "Can't create event");
	}

	ReadFile (conn->fd,conn->buf,1,NULL,&(conn->read_overlapped));

	conn->next = NULL;

	tmp = last = connhead;

	while (tmp) {
		last = tmp;
		tmp = tmp->next;
	}

	if (last)
		last->next = conn;
	else
		connhead = conn;

	upsdebugx(3, "new connection on handle %p", acc);

	pconf_init(&conn->ctx, NULL);
#endif /* WIN32 */

	return acc;
}

static int sock_arg(conn_t *conn)
{
	/* "Server-side" listener for the timer daemon */
	if (conn->ctx.numargs < 1)
		return 0;

	/* LIST-TIMERS (no args expected now)
	 * returns a list with tab-separated values for:
	 * NAME TO_ABS TO_REL NOTIFYTYPES UPSNAMES NOTIFYMSGS_TABSEP
	 */
	if (!strcmp(conn->ctx.arglist[0], "LIST-TIMERS")) {
		ttype_t	*item = thead;
		char	*s = NULL;
		time_t	now;

		send_to_one(conn, "BEGIN LIST TIMERS\n");
		time(&now);

		while (item) {
			if (item->name) {
				send_to_one(conn, "%s\t%ld\t%g\t",
					item->name, (long)item->etime, difftime(item->etime, now));

				s = NULL;
				if (item->notifytypes && *(item->notifytypes) && **(item->notifytypes)) {
					s = collect_string(item->notifytypes, "NOTIFYTYPE", ",", NULL, NULL);
				}

				if (s && *s) {
					send_to_one(conn, "%s\t", s);
				} else {
					send_to_one(conn, "\"\"\t");
				}
				if (s) {
					free(s);
				}

				s = NULL;
				if (item->upsnames && *(item->upsnames) && **(item->upsnames)) {
					s = collect_string(item->upsnames, "UPSNAME", ",", NULL, NULL);
				}

				if (s && *s) {
					send_to_one(conn, "%s\t", s);
				} else {
					send_to_one(conn, "\"\"\t");
				}
				if (s) {
					free(s);
				}

				s = NULL;
				if (item->notifymsgs && *(item->notifymsgs) && **(item->notifymsgs)) {
					s = collect_string(item->notifymsgs, "NOTIFYMSG", ".\t", NULL, NULL);
				}

				if (s && *s) {
					send_to_one(conn, "%s\n", s);
				} else {
					send_to_one(conn, "\"\"\n");
				}
				if (s) {
					free(s);
				}
			}
			item = item->next;
		}

		send_to_one(conn, "END LIST TIMERS\n");
		send_to_one(conn, "OK\n\0");
		return 1;
	}

	/* CANCEL <name> [<cmd>] [<NOTIFYTYPE> <UPSNAME> <NOTIFYMSG_FOR_LOGINFO>] */
	{ /* scoping */
		int	do_cancel = !strcmp(conn->ctx.arglist[0], "CANCEL"),
			do_cancel_matched = !strcmp(conn->ctx.arglist[0], "CANCEL-MATCHED");

		if (do_cancel || do_cancel_matched) {
			/* "cmd" may be present and empty, this is handled in the method */
			if (conn->ctx.numargs < 3)
				cancel_timer(conn->ctx.arglist[1], NULL,
					NULL, NULL, NULL, do_cancel_matched);
			else
			if (conn->ctx.numargs < 6)
				cancel_timer(conn->ctx.arglist[1], conn->ctx.arglist[2],
					NULL, NULL, NULL, do_cancel_matched);
			else
				cancel_timer(conn->ctx.arglist[1], conn->ctx.arglist[2],
					conn->ctx.arglist[3], conn->ctx.arglist[4],
					conn->ctx.arglist[5], do_cancel_matched);

			send_to_one(conn, "OK\n");
			return 1;
		}
	}

	if (conn->ctx.numargs < 6)
		return 0;

	/* START <name> <length> <NOTIFYTYPE> <UPSNAME> <NOTIFYMSG> */
	if (!strcmp(conn->ctx.arglist[0], "START")) {
		start_timer(conn->ctx.arglist[1], conn->ctx.arglist[2],
			conn->ctx.arglist[3], conn->ctx.arglist[4],
			conn->ctx.arglist[5], 0);
		send_to_one(conn, "OK\n");
		return 1;
	}

	/* START-SHARED <name> <length> <NOTIFYTYPE> <UPSNAME> <NOTIFYMSG> */
	if (!strcmp(conn->ctx.arglist[0], "START-SHARED")) {
		start_timer(conn->ctx.arglist[1], conn->ctx.arglist[2],
			conn->ctx.arglist[3], conn->ctx.arglist[4],
			conn->ctx.arglist[5], 1);
		send_to_one(conn, "OK\n");
		return 1;
	}

	/* unknown */
	return 0;
}

static void log_unknown(size_t numarg, char **arg)
{
	size_t	i;

	upslogx(LOG_INFO, "Unknown command on socket: ");

	for (i = 0; i < numarg; i++)
		upslogx(LOG_INFO, "arg %" PRIuSIZE ": %s", i, arg[i]);
}

static int sock_read(conn_t *conn)
{
	int	i;
	ssize_t	ret;
	char	ch;

	upsdebugx(6, "Starting sock_read()");
	for (i = 0; i < US_MAX_READ; i++) {
		/* NOTE: This does not imply that each command line must
		 * fit in the US_MAX_READ length limit - at worst we would
		 * "return 0", and continue with pconf_char() next round.
		 */
		size_t numarg;
#ifndef WIN32
		errno = 0;
		ret = read(conn->fd, &ch, 1);

		if (ret > 0)
			upsdebug_with_errno(7, "read() from fd %d returned %" PRIiSIZE " (bytes): '%c'",
				conn->fd, ret, ch);

		if (ret < 1) {

			/* short read = no parsing, come back later */
			if ((ret == -1) && (errno == EAGAIN)) {
				upsdebugx(6, "Ending sock_read(): short read");
				return 0;
			}

			/* O_NDELAY with zero bytes means nothing to read but
			 * since read() follows a successful select() with
			 * ready file descriptor, ret shouldn't be 0.
			 * This may also mean that the counterpart has exited
			 * and the file descriptor should be reaped.
			 */
			if (ret == 0) {
				struct pollfd pfd;
				pfd.fd = conn->fd;
				pfd.events = 0;
				pfd.revents = 0;
				/* Note: we check errno twice, since it could
				 * have been set by read() above or by one
				 * of the probing routines below
				 */
				if (errno
				|| (fcntl(conn->fd, F_GETFD) < 0)
				|| (poll(&pfd, 1, 0) <= 0)
				||  errno
				) {
					upsdebug_with_errno(4, "read() from fd %d returned 0", conn->fd);
					return -1;	/* connection closed, probably */
				}
				if (i == (US_MAX_READ - 1)) {
					upsdebug_with_errno(4, "read() from fd %d returned 0 "
						"too many times in a row, aborting "
						"sock_read()", conn->fd);
					return -1;	/* connection closed, probably */
				}
				continue;
			}

			/* some other problem */
			upsdebugx(6, "Ending sock_read(): some other problem");
			return -1;	/* error */
		}
#else	/* WIN32 */
		DWORD bytesRead;
		GetOverlappedResult(conn->fd, &conn->read_overlapped, &bytesRead,FALSE);
		if( bytesRead < 1 ) {
			/* Restart async read */
			memset(conn->buf,0,sizeof(conn->buf));
			ReadFile(conn->fd,conn->buf,1,NULL,&(conn->read_overlapped));
			return 0;
		}

		ch = conn->buf[0];

		/* Restart async read */
		memset(conn->buf,0,sizeof(conn->buf));
		ReadFile(conn->fd,conn->buf,1,NULL,&(conn->read_overlapped));
#endif /* WIN32 */

		ret = pconf_char(&conn->ctx, ch);

		if (ret == 0)	/* nothing to parse yet */
			continue;

		if (ret == -1) {
			upslogx(LOG_NOTICE, "Parse error on sock: %s",
				conn->ctx.errmsg);

			upsdebugx(6, "Ending sock_read(): parse error");
			return 0;	/* nothing parsed */
		}

		/* try to use it, and complain about unknown commands */
		upsdebugx(3, "Ending sock_read() on a good note: try to use command:");
		for (numarg = 0; numarg < conn->ctx.numargs; numarg++)
			upsdebugx(3, "\targ %" PRIuSIZE ": %s", numarg, conn->ctx.arglist[numarg]);
		if (!sock_arg(conn)) {
			log_unknown(conn->ctx.numargs, conn->ctx.arglist);
			send_to_one(conn, "ERR UNKNOWN\n");
		}

		return 1;	/* we did some work */
	}

	upsdebug_with_errno(6, "sock_read() from fd %d returned nothing "
		"(maybe still collecting the command line); ", conn->fd);

	return 0;	/* fell out without parsing anything */
}

static void start_daemon(TYPE_FD lockfd)
{
#ifndef WIN32
	int	maxfd = 0;	/* Unidiomatic use vs. "pipefd" below, which is "int" on non-WIN32 */
	TYPE_FD pipefd;
	struct	timeval	tv;
	conn_t	*tmp;
	int	pid, ret;
	fd_set	rfds;
	conn_t	*tmpnext;

	us_serialize(SERIALIZE_INIT);

	if ((pid = fork()) < 0)
		fatal_with_errno(EXIT_FAILURE, "Unable to enter background");

	if (pid != 0) {		/* parent */

		/* wait for child to set up the listener */
		us_serialize(SERIALIZE_WAIT);

		return;
	}

	/* child */
	setproctag("timer");

	/* make fds 0-2 (typically) point somewhere defined */
# ifdef HAVE_DUP2
	/* system can close (if needed) and (re-)open a specific FD number */
	if (1) { /* scoping */
		TYPE_FD devnull = open("/dev/null", O_RDWR);
		if (devnull < 0)
			fatal_with_errno(EXIT_FAILURE, "open /dev/null");

		if (dup2(devnull, STDIN_FILENO) != STDIN_FILENO)
			fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDIN");
		if (dup2(devnull, STDOUT_FILENO) != STDOUT_FILENO)
			fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDOUT");

		if (nut_debug_level) {
			upsdebugx(1, "Keeping stderr open due to debug verbosity %d", nut_debug_level);
		} else {
			if (dup2(devnull, STDERR_FILENO) != STDERR_FILENO)
				fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDERR");
		}

		close(devnull);
	}
# else /* not HAVE_DUP2 */
#  ifdef HAVE_DUP
	/* opportunistically duplicate to the "lowest-available" FD number */
	close(STDIN_FILENO);
	if (open("/dev/null", O_RDWR) != STDIN_FILENO)
		fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDIN");

	close(STDOUT_FILENO);
	if (dup(STDIN_FILENO) != STDOUT_FILENO)
		fatal_with_errno(EXIT_FAILURE, "dup /dev/null as STDOUT");

	if (nut_debug_level) {
		upsdebugx(1, "Keeping stderr open due to debug verbosity %d", nut_debug_level);
	} else {
		close(STDERR_FILENO);
		if (dup(STDIN_FILENO) != STDERR_FILENO)
			fatal_with_errno(EXIT_FAILURE, "dup /dev/null as STDERR");
	}
#  else /* not HAVE_DUP */
	close(STDIN_FILENO);
	if (open("/dev/null", O_RDWR) != STDIN_FILENO)
		fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDIN");

	close(STDOUT_FILENO);
	if (open("/dev/null", O_RDWR) != STDOUT_FILENO)
		fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDOUT");

	if (nut_debug_level) {
		upsdebugx(1, "Keeping stderr open due to debug verbosity %d", nut_debug_level);
	} else {
		close(STDERR_FILENO);
		if (open("/dev/null", O_RDWR) != STDERR_FILENO)
			fatal_with_errno(EXIT_FAILURE, "re-open /dev/null as STDERR");
	}
#  endif /* not HAVE_DUP */
# endif /* not HAVE_DUP2 */

	/* Still in child, non-WIN32 - work as timer daemon (infinite loop) */
	pipefd = open_sock();

	if (nut_debug_level)
		upslogx(LOG_INFO, "Timer daemon started");

	/* release the parent */
	us_serialize(SERIALIZE_SET);

	/* drop the lock now that the background is running */
	unlink(lockfn);
	close(lockfd);
	writepid(prog);

	/* Whatever upsmon envvars were set when this daemon started, would be
	 * irrelevant and only confusing at the moment a particular timer causes
	 * CMDSCRIPT to run */
	unsetenv("NOTIFYTYPE");
	unsetenv("UPSNAME");
	unsetenv("NOTIFYMSG");

	/* now watch for activity */
	upsdebugx(2, "Timer daemon waiting for connections on pipefd %d",
		pipefd);

	for (;;) {
		int	zero_reads = 0, total_reads = 0;
		struct timeval	start, now;

		gettimeofday(&start, NULL);

		/* wait at most 1s so we can check our timers regularly */
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		FD_ZERO(&rfds);
		FD_SET(pipefd, &rfds);

		maxfd = pipefd;

		for (tmp = connhead; tmp != NULL; tmp = tmp->next) {
			FD_SET(tmp->fd, &rfds);

			if (tmp->fd > maxfd)
				maxfd = tmp->fd;
		}

		ret = select(maxfd + 1, &rfds, NULL, NULL, &tv);

		if (ret > 0) {
			if (FD_ISSET(pipefd, &rfds))
				conn_add(pipefd);

			tmp = connhead;

			while (tmp) {
				tmpnext = tmp->next;

				if (FD_ISSET(tmp->fd, &rfds)) {
					total_reads++;
					ret = sock_read(tmp);
					if (ret < 0) {
						upsdebugx(3, "closing connection on fd %d", tmp->fd);
						close(tmp->fd);
						conn_del(tmp);
					}
					if (ret == 0)
						zero_reads++;
				}

				tmp = tmpnext;
			}
		}

		checktimers();

		/* upsdebugx(6, "zero_reads=%d total_reads=%d", zero_reads, total_reads); */
		if (zero_reads && zero_reads == total_reads) {
			/* Catch run-away loops - that is, consider
			 * throttling the cycle as to not hog CPU:
			 * did select() spend its second to reply,
			 * or had something to say immediately?
			 * Note that while select() may have changed
			 * "tv" to deduct the time waited, our further
			 * processing loops could eat some more time.
			 * So we just check the difference of "start"
			 * and "now". If we did spend a substantial
			 * part of the second, do not delay further.
			 */
			double d;
			gettimeofday(&now, NULL);
			d = difftimeval(now, start);
			upsdebugx(6, "difftimeval() => %f sec", d);
			if (d > 0 && d < 0.2) {
				d = (1.0 - d) * 1000000.0;
				upsdebugx(5, "Enforcing a throttling sleep: %f usec", d);
				usleep((useconds_t)d);
			}
		}
	}

#else /* WIN32 */

	/* NOTE ON THIS REWRITE:
	 *
	 * This function is only ever called by the short-lived CLI process
	 * (the one invoked by upsmon with UPSNAME/NOTIFYTYPE set) when it
	 * could not connect to an already-running timer daemon.
	 *
	 * Historically (see version control history for the code this
	 * replaces), this WIN32 branch did NOT fork(): it called
	 * CreateProcess(module, NULL, ...) -- i.e. spawned a brand-new,
	 * unmarked copy of this very executable, which (since
	 * lpEnvironment==NULL) inherited this process' current
	 * environment, INCLUDING UPSNAME/NOTIFYTYPE -- and then THIS
	 * process itself (not the spawned one!) called open_sock() and
	 * ran the server for(;;) loop forever, i.e. becoming the daemon.
	 *
	 * That spawned copy had no way of knowing it was supposed to
	 * "become the daemon": unlike fork(), CreateProcess() cannot
	 * split this already-initialized process into a parent/child
	 * pair -- it always starts a brand-new process at main(). So the
	 * spawned copy simply re-ran the entire normal CLI codepath
	 * (re-parsing upssched.conf, calling parse_at() and sendcmd()
	 * again) as if upsmon had invoked upssched a second time for the
	 * very same event. Combined with a lock-file race in
	 * check_parent() (see the unconditional DeleteFile(lockfn) a few
	 * lines above in that function), this could make the spawned
	 * copy conclude that IT also needed to start a daemon, spawning
	 * yet another copy, and so on -- a runaway cascade of processes
	 * instead of the intended single client + single daemon pair.
	 *
	 * The rewrite below fixes this by:
	 *   1. Explicitly marking the spawned process (via
	 *      WIN32_DAEMON_MARKER on its command line, checked at the
	 *      very top of main()) so it goes straight to
	 *      win32_run_timer_daemon() instead of the normal CLI path.
	 *   2. Clearing UPSNAME/NOTIFYTYPE/NOTIFYMSG from our own
	 *      environment before spawning, since CreateProcess() with
	 *      lpEnvironment==NULL takes a snapshot of it for the child.
	 *   3. Adding an explicit readiness handshake (a manual-reset
	 *      Win32 event, inherited by the child) so THIS process waits
	 *      until the child is actually listening on the named pipe,
	 *      then RETURNS -- exactly like the non-WIN32 parent branch
	 *      above returns after us_serialize(SERIALIZE_WAIT). This
	 *      process is NOT the daemon: control goes back to
	 *      check_parent(), which retries try_connect() and delivers
	 *      the ORIGINAL command to the now-running daemon, and this
	 *      process then exits normally, same as on non-WIN32.
	 */

	char module[NUT_PATH_MAX + 1];
	char cmdline[NUT_PATH_MAX + 64];
	char ready_event_str[32];
	STARTUPINFO sinfo;
	PROCESS_INFORMATION pinfo;
	SECURITY_ATTRIBUTES sa;
	HANDLE hReady;
	DWORD wait_ret;

	if (!GetModuleFileName(NULL, module, sizeof(module))) {
		fatal_with_errno(EXIT_FAILURE, "Can't retrieve module name");
	}

	/* Manual-reset event, created inheritable, so the about-to-be
	 * spawned child can SetEvent() it once its named pipe is up. This
	 * is the WIN32 counterpart of us_serialize(SERIALIZE_INIT), which
	 * is implemented above with a plain POSIX pipe(2) and has no
	 * WIN32 equivalent. */
	memset(&sa, 0, sizeof(sa));
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;

	hReady = CreateEvent(&sa, TRUE /* manual-reset */, FALSE /* initially non-signaled */, NULL);
	if (hReady == NULL) {
		fatal_with_errno(EXIT_FAILURE, "Can't create timer daemon readiness event");
	}

	/* Hand the readiness-event HANDLE down to the child as a hex
	 * string on its command line, alongside WIN32_DAEMON_MARKER (see
	 * main() for the receiving side). This relies on the handle
	 * actually being inherited by the child process, which requires
	 * BOTH bInheritHandle=TRUE on the handle itself (set above) AND
	 * bInheritHandles=TRUE in the CreateProcess() call below.
	 *
	 * NOTE for the developers reading this years down the road:
	 * passing a raw HANDLE value across a process boundary via argv,
	 * encoded as text (with hex value), is somewhat unusual (because a
	 * named/well-known event, or the newer PROC_THREAD_ATTRIBUTE_HANDLE_LIST
	 * mechanism, are the more modern alternatives), but it is simple,
	 * requires no extra Windows SDK version checks, and is a
	 * well-documented pattern for inherited handle hand-off predating
	 * Vista-era API additions.
	 * strtoull()/"%llx" are used here for portability across the
	 * MinGW-w64 and MSVC toolchains this project supports; please
	 * double check this against the actual minimum supported
	 * toolchain versions in this codebase's build files, should
	 * any target OS level ever get officially deprecated by the
	 * NUT project. */
	snprintf(ready_event_str, sizeof(ready_event_str), "%llx",
		(unsigned long long)(uintptr_t)hReady);
	snprintf(cmdline, sizeof(cmdline), "\"%s\" %s %s",
		module, WIN32_DAEMON_MARKER, ready_event_str);

	/* Whatever upsmon envvars are currently set (UPSNAME, NOTIFYTYPE,
	 * NOTIFYMSG) must NOT leak into the spawned child: unlike fork(),
	 * CreateProcess() with lpEnvironment==NULL makes the child inherit
	 * a snapshot of THIS process' current environment block -- and it
	 * was exactly that inheritance which historically let the (back
	 * then, unmarked) spawned copy believe it was handling a fresh
	 * notification from upsmon. The child is not expected to consult
	 * these variables at all now (is_win32_timer_daemon short-circuits
	 * that in main()/conf_arg()), but we clear them here too, out of
	 * caution. */
	unsetenv("UPSNAME");
	unsetenv("NOTIFYTYPE");
	unsetenv("NOTIFYMSG");

	memset(&sinfo, 0, sizeof(sinfo));
	sinfo.cb = sizeof(sinfo);

	/* bInheritHandles=TRUE is required for the child to receive a
	 * usable duplicate of hReady. */
	if (!CreateProcess(module, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &sinfo, &pinfo)) {
		CloseHandle(hReady);
		fatal_with_errno(EXIT_FAILURE, "Can't create timer daemon child process");
	}

	/* We don't need to track or wait on the child process/thread
	 * handles beyond this point -- only the readiness event matters
	 * here. (The code this replaces never closed these two handles,
	 * leaking a HANDLE pair on every daemon start; fixed here.) */
	CloseHandle(pinfo.hThread);
	CloseHandle(pinfo.hProcess);

	/* Wait for the child to open its named pipe and call SetEvent();
	 * this is the WIN32 equivalent of us_serialize(SERIALIZE_WAIT).
	 * A finite timeout avoids hanging forever if the child fails to
	 * start for some unrelated reason -- we fail loudly instead of
	 * silently proceeding as if the daemon were already up. */
	wait_ret = WaitForSingleObject(hReady, 10000 /* ms; NOTE: adjust if too tight/loose for the target environment */);
	CloseHandle(hReady);

	if (wait_ret != WAIT_OBJECT_0) {
		fatal_with_errno(EXIT_FAILURE,
			"Timer daemon child did not signal readiness in time");
	}

	if (nut_debug_level)
		upslogx(LOG_INFO, "Timer daemon child confirmed ready");

	/* We (the CLI process) are NOT the daemon. Release our lock and
	 * return to the caller (check_parent()), which will retry
	 * try_connect() and deliver the ORIGINAL command via the named
	 * pipe, then exit -- mirroring the non-WIN32 parent branch above. */
	CloseHandle(lockfd);
	DeleteFile(lockfn);

	return;
#endif /* WIN32 */

	/* Should not get here */
/*
	if (nut_debug_level)
		upslogx(LOG_INFO, "Timer daemon ending");
*/
}

#ifdef WIN32
/* WIN32-only: body of the persistent timer-daemon process.
 *
 * Entered directly from main() when WIN32_DAEMON_MARKER is found on
 * the command line (see main() below), i.e. this is the freshly
 * spawned child created by start_daemon() above via CreateProcess().
 * This process never goes through the normal CLI codepath: by the
 * time main() calls us, checkconf() has already run (gated by
 * is_win32_timer_daemon so that only PIPEFN/LOCKFN/CMDSCRIPT/
 * DEBUG_MIN are parsed and no "AT" line is acted upon -- see
 * conf_arg()).
 *
 * This is the WIN32 counterpart of the "child" branch of the
 * non-WIN32 start_daemon() above (the code path taken when
 * "pid != 0" is false, i.e. after the "if (pid != 0) { ...; return; }"
 * block) -- except it necessarily lives in its own function, since
 * unlike fork() it cannot simply "fall through" from inside
 * start_daemon(): it is a distinct OS process running its own,
 * separate main().
 *
 * Below this point, down to the closing brace, the loop body is
 * relocated VERBATIM from the previous (buggy) WIN32 branch of
 * start_daemon() -- no functional change was made to the connection/
 * timer-handling logic itself, only to how and by which process this
 * code gets reached. */
static void win32_run_timer_daemon(void)
{
	int		maxfd = 0;
	TYPE_FD		pipefd;
	struct timeval	tv;
	conn_t		*tmp;
	DWORD		timeout_ms;
	HANDLE		rfds[32];

	pipefd = open_sock();

	if (nut_debug_level)
		upslogx(LOG_INFO, "Timer daemon started");

	/* Tell the process that spawned us (still waiting inside its own
	 * call to start_daemon(), see above) that our named pipe is now
	 * up and accepting connections. This is the WIN32 counterpart of
	 * us_serialize(SERIALIZE_SET). */
	if (win32_daemon_ready_event != NULL) {
		SetEvent(win32_daemon_ready_event);
		CloseHandle(win32_daemon_ready_event);
		win32_daemon_ready_event = NULL;
	} else {
		/* Defensive: should not happen in normal operation, since
		 * start_daemon() always passes a valid inherited handle via
		 * WIN32_DAEMON_MARKER's argv[2]. Log and carry on: the
		 * spawning process will simply hit its own
		 * WaitForSingleObject() timeout and report an error, rather
		 * than this (already perfectly usable) daemon exiting. */
		upslogx(LOG_WARNING,
			"%s: no readiness event handle was received from the spawning process",
			__func__);
	}

	writepid(prog);

	/* Whatever upsmon envvars were set for the CLI process that
	 * spawned us would be irrelevant and only confusing at the moment
	 * a particular timer causes CMDSCRIPT to run. Under normal
	 * operation none of these should be inherited any more anyway,
	 * since start_daemon() above clears them from its own environment
	 * before calling CreateProcess() -- this is kept as a defensive,
	 * now-redundant fallback matching the non-WIN32 daemon child. */
	unsetenv("NOTIFYTYPE");
	unsetenv("UPSNAME");
	unsetenv("NOTIFYMSG");

	/* now watch for activity */
	upsdebugx(2, "Timer daemon waiting for connections");

	for (;;) {
		/* wait at most 1s so we can check our timers regularly */
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		timeout_ms = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);

		maxfd = 0;

		/* Wait on the read IO of each connections */
		for (tmp = connhead; tmp != NULL; tmp = tmp->next) {
			rfds[maxfd] = tmp->read_overlapped.hEvent;
			maxfd++;
		}
		/* Add the connect event */
		rfds[maxfd] = connect_overlapped.hEvent;
		maxfd++;

		{
			DWORD ret_val;
			ret_val = WaitForMultipleObjects(
					maxfd,  /* number of objects in array */
					rfds,   /* array of objects */
					FALSE,  /* wait for any object */
					timeout_ms); /* timeout in millisecond */

			if (ret_val == WAIT_FAILED) {
				upslog_with_errno(LOG_ERR, "waitfor failed");
				return;
			}

			/* timer has not expired */
			if (ret_val != WAIT_TIMEOUT) {
				/* Retrieve the signaled connection */
				for (tmp = connhead; tmp != NULL; tmp = tmp->next) {
					if (tmp->read_overlapped.hEvent == rfds[ret_val - WAIT_OBJECT_0]) {
						break;
					}
				}

				/* the connection event handle has been signaled */
				if (rfds[ret_val] == connect_overlapped.hEvent) {
					pipefd = conn_add(pipefd);
				}
				/* one of the read event handle has been signaled */
				else {
					if (tmp != NULL) {
						if (sock_read(tmp) < 0) {
							upsdebugx(3, "closing connection on handle %p", tmp->fd);
							CloseHandle(tmp->fd);
							conn_del(tmp);
						}
					}
				}
			}
		}

		checktimers();
	}

	/* Should not get here (see the WAIT_FAILED "return" above) */
}
#endif	/* WIN32 */

/* --- 'client' functions --- */

static TYPE_FD try_connect(void)
{
	TYPE_FD pipefd;

#ifndef WIN32
	int	ret;
	struct	sockaddr_un saddr;

	check_unix_socket_filename(pipefn);

	memset(&saddr, '\0', sizeof(saddr));
	saddr.sun_family = AF_UNIX;
	snprintf(saddr.sun_path, sizeof(saddr.sun_path), "%s", pipefn);

	pipefd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (INVALID_FD(pipefd))
		fatal_with_errno(EXIT_FAILURE, "socket");

	ret = connect(pipefd, (const struct sockaddr *) &saddr, sizeof(saddr));

	if (ret != -1)
		return pipefd;

#else /* WIN32 */

	BOOL   result = FALSE;

	result = WaitNamedPipe(pipefn,NMPWAIT_USE_DEFAULT_WAIT);

	if (result == FALSE) {
		return ERROR_FD;
	}

	pipefd = CreateFile(
			pipefn,       /* pipe name */
			GENERIC_READ |  /* read and write access */
			GENERIC_WRITE,
			0,              /* no sharing */
			NULL,           /* default security attributes FIXME */
			OPEN_EXISTING,  /* opens existing pipe */
			FILE_FLAG_OVERLAPPED,   /*  enable async IO */
			NULL);          /* no template file */

	if (VALID_FD(pipefd))
		return pipefd;

#endif /* WIN32 */

	return ERROR_FD;
}

static TYPE_FD get_lock(const char *fn)
{
#ifndef WIN32
	return open(fn, O_RDONLY | O_CREAT | O_EXCL, 0);
#else	/* WIN32 */
	return CreateFile(fn,GENERIC_ALL,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
#endif	/* WIN32 */
}

/* try to connect to bg process, and start one if necessary */
static TYPE_FD check_parent(const char *cmd, const char *arg2)
{
	TYPE_FD	pipefd, lockfd;
	int	tries = 0;

	for (tries = 0; tries < MAX_TRIES; tries++) {
		pipefd = try_connect();

		if (VALID_FD(pipefd))
			return pipefd;

		/* timer daemon isn't running */

		/* it's not running, so there's nothing to cancel */
		if (!strcmp(cmd, "CANCEL") && (arg2 == NULL))
			return (TYPE_FD)PARENT_UNNECESSARY;

		/* arg2 non-NULL means there is a cancel action available */

		/* we need to start the daemon, so try to get the lock */

		lockfd = get_lock(lockfn);

		if (VALID_FD(lockfd)) {
			start_daemon(lockfd);
			return (TYPE_FD)PARENT_STARTED;	/* started successfully */
		}

		/* we didn't get the lock - must be two upsscheds running */

		/* blow this away in case we crashed before */
#ifndef WIN32
		unlink(lockfn);
#else	/* WIN32 */
		DeleteFile(lockfn);
#endif	/* WIN32 */

		/* give the other one a chance to start it, then try again */
		usleep(250000);
	}

	upslog_with_errno(LOG_ERR, "Failed to connect to parent and failed to create parent");
	exit(EXIT_FAILURE);
}

static void sendcmd(const char *cmd, const char *arg1, const char *arg2)
{
	int	i;
	ssize_t	ret;
	size_t	enclen, buflen;
	char buf[LARGEBUF], enc[LARGEBUF + 8];
#ifndef WIN32
	int	ret_s;
	struct	timeval tv;
	fd_set	fdread;
#else	/* WIN32 */
	DWORD bytesWritten = 0;
#endif	/* WIN32 */
	TYPE_FD pipefd;

	/* sanity-check */
	if (!arg1 && !list_timers)
		return;

	/* build the request
	 * note that in list-timers mode we may have no args to send, but
	 * otherwise we must have at least the timer name to start or cancel
	 */
	snprintf(buf, sizeof(buf), "%s \"%s\"",
		cmd, arg1 ? pconf_encode(arg1, enc, sizeof(enc)) : "");

	snprintfcat(buf, sizeof(buf), " \"%s\"",
		arg2 ? pconf_encode(arg2, enc, sizeof(enc)) : "");

	/* use envvars set by caller (upsmon) when launching this client process
	 * (C variables are known not-null courtesy of main() method... except
	 * when we are in the list-timers mode).
	 */
	snprintfcat(buf, sizeof(buf), " \"%s\"",
		notify_type ? pconf_encode(notify_type, enc, sizeof(enc)) : "");

	snprintfcat(buf, sizeof(buf), " \"%s\"",
		ups_name? pconf_encode(ups_name, enc, sizeof(enc)) : "");

	snprintfcat(buf, sizeof(buf), " \"%s\"",
		notify_msg ? pconf_encode(notify_msg, enc, sizeof(enc)) : "");

	snprintf(enc, sizeof(enc), "%s\n", buf);

	/* Sanity checks, for static analyzers to sleep well */
	enclen = strlen(enc);
	buflen = strlen(buf);
	if (enclen >= SSIZE_MAX || buflen >= SSIZE_MAX) {
		/* Can't compare enclen to ret below */
		fatalx(EXIT_FAILURE, "Unable to connect to daemon: buffered message too large");
	}

	for (i = 0; i < MAX_TRIES; i++) {

		if (list_timers) {
			pipefd = try_connect();

			if (INVALID_FD(pipefd)) {
				upsdebugx(1, "%s: failed to use PIPEFN='%s'", __func__, NUT_STRARG(pipefn));
				fatalx(EXIT_FAILURE, "upssched timer is not running");
			}
		} else {
			/* see if the parent needs to be started (and maybe start it) */
			pipefd = check_parent(cmd, arg2);

			if (pipefd == (TYPE_FD)PARENT_STARTED) {
				/* loop back and try to connect now */
				usleep(250000);
				continue;
			}

			/* special case for CANCEL when no parent is running */
			if (pipefd == (TYPE_FD)PARENT_UNNECESSARY)
				return;
		}

		/* we're connected now */
#ifndef WIN32
		ret = write(pipefd, enc, enclen);

		/* if we can't send the whole thing, loop back and try again */
		if ((ret < 1) || (ret != (ssize_t)enclen)) {
			if (list_timers) {
				upslogx(LOG_ERR, "write failed, daemon must have ended");
				close(pipefd);
				break;
			} else {
				upslogx(LOG_ERR, "write failed, trying again");
				close(pipefd);
				continue;
			}
		}

		/* select on child's pipe fd */
		do {
			/* set timeout every time before call select() */
			tv.tv_sec = 1;
			tv.tv_usec = 0;

			FD_ZERO(&fdread);
			FD_SET(pipefd, &fdread);

			ret_s = select(pipefd + 1, &fdread, NULL, NULL, &tv);
			upsdebugx(2, "%s: ret_s=%d", __func__, ret_s);
			switch(ret_s) {
				/* select error */
				case -1:
					upslog_with_errno(LOG_DEBUG, "parent select error");
					break;

				/* nothing to read */
				case 0:
					break;

				/* available data to read */
				default:
					memset (buf, 0, sizeof (buf));
					ret = read(pipefd, buf, sizeof(buf) - 1);
					upsdebugx(2, "%s: ret=%" PRIiSIZE ": [%s]", __func__, ret, buf);

					if (list_timers && ret > 0) {
						/* ASSUME we see whole starting and/or ending lines
						 * of the response within one read() operation
						 */
						char	*end = strstr(buf, "END LIST TIMERS\n"),
								*ok = strstr(buf, "OK\n");

						/* Require to continue the reading loop */
						ret_s = -2;

						if (end)
							*end = '\0';
						else {
							end = strstr(buf, "\nEND");
							if (end)
								*(end+1) = '\0';
						}

						if (ok)
							*ok = '\0';

						if (!strncmp(buf, "BEGIN LIST TIMERS\n", 18)) {
							printf("%s", buf + 18);
						} else {
							printf("%s", buf);
						}

						if (ok) {
							ret = snprintf(buf, sizeof(buf), "%s", "OK\n\0");
							ret_s = 1;
						} else {
							/* Let more of the response accumulate in the buffer */
							usleep(250000);
						}
					}
					break;
			}
		} while (ret_s <= 0);

		close(pipefd);

		/* same idea: no OK = go try it all again */
		if (ret < 2) {
			if (list_timers) {
				upslogx(LOG_ERR, "read confirmation failed, daemon must have ended");
				break;
			} else {
				upslogx(LOG_ERR, "read confirmation failed, trying again");
				continue;
			}
		}

#else /* WIN32 */
		ret = WriteFile(pipefd, enc, enclen, &bytesWritten, NULL);
		if (ret == 0 || bytesWritten != enclen) {
			if (list_timers) {
				upslogx(LOG_ERR, "write failed, daemon must have ended");
				CloseHandle(pipefd);
				break;
			} else {
				upslogx(LOG_ERR, "write failed, trying again");
				CloseHandle(pipefd);
				continue;
			}
		}

		/* NOTE ON THIS REWRITE:
		 *
		 * The code this replaces used the return value of ReadFile()
		 * itself as a "while (ReadFile(...))" loop condition. For a
		 * handle opened with FILE_FLAG_OVERLAPPED (see try_connect()),
		 * ReadFile() returning FALSE with GetLastError()==
		 * ERROR_IO_PENDING is the NORMAL, expected outcome while the
		 * read is still in flight -- it is NOT a failure, and it is
		 * by far the common case here, since the daemon practically
		 * never has its reply ready at the exact instant ReadFile()
		 * is called. That meant the loop's body (containing the
		 * WaitForSingleObject()/parsing logic) was, in practice,
		 * almost never entered, leaving "buf" at the all-zeroes state
		 * left by the memset() below -- which is the direct cause of
		 * the empty "read confirmation got []" messages observed in
		 * the logs.
		 *
		 * Separately, on the rare occasions that inner loop WAS
		 * entered, the "continue"/"break" statements inside it (meant,
		 * by the surrounding comments and by symmetry with the
		 * non-WIN32 branch above, to retry/give up on the OUTER
		 * "for (i = 0; i < MAX_TRIES; i++)" loop) actually only
		 * affected that inner while-loop -- and did so right after
		 * CloseHandle(pipefd) had already been called, meaning the
		 * loop's own condition would then call ReadFile() again on an
		 * already-closed HANDLE. This rewrite removes that inner loop
		 * entirely (folding its one legitimate use -- accumulating a
		 * possibly multi-chunk list_timers response -- into a small,
		 * clearly-scoped do/while used only for that purpose), and
		 * uses a "win32_outer_action" flag to route give-up/retry
		 * decisions to plain "break"/"continue" statements that sit
		 * directly in the outer for-loop's body, so they unambiguously
		 * target the outer loop, exactly like every other
		 * "break"/"continue" in this function already does.
		 *
		 * Also fixed in passing: pipefd (a HANDLE) was never closed
		 * on the success path (a leak on every successful sendcmd()
		 * call), and read_overlapped.hEvent was never closed at all
		 * (a leak on every single call, success or failure). Both are
		 * now closed exactly once, right before returning control to
		 * the outer loop.
		 */
		{
			OVERLAPPED	read_overlapped;
			DWORD		bytesRead = 0;
			int		win32_outer_action = 0;	/* 0=fall through and check buf; 1=continue; 2=break */
			int		keep_reading;

			memset(&read_overlapped, 0, sizeof(read_overlapped));
			memset(buf, 0, sizeof(buf));

			read_overlapped.hEvent = CreateEvent(NULL, /* Security */
					FALSE, /* auto-reset */
					FALSE, /* initial state = non-signaled */
					NULL /* no name */);
			if (read_overlapped.hEvent == NULL) {
				fatal_with_errno(EXIT_FAILURE, "Can't create event");
			}

			do {
				BOOL	read_ok;

				keep_reading = 0;

				read_ok = ReadFile(pipefd, buf, sizeof(buf) - 1, NULL, &read_overlapped);

				if (!read_ok && GetLastError() != ERROR_IO_PENDING) {
					/* A real, immediate failure -- as opposed to
					 * the read simply being queued (see the long
					 * comment above). */
					upslogx(LOG_ERR, "ReadFile failed (error %lu)",
						(unsigned long)GetLastError());
					win32_outer_action = list_timers ? 2 : 1;
					break;
				}

				{
					/* Whether read_ok==TRUE (completed synchronously
					 * -- rare but possible for a pipe) or the read
					 * was queued (ERROR_IO_PENDING), wait on the
					 * event to find out when it actually completes. */
					DWORD wait_ret = WaitForSingleObject(read_overlapped.hEvent, 2000);

					if (wait_ret != WAIT_OBJECT_0) {
						if (list_timers) {
							upslogx(LOG_ERR, "read confirmation failed, daemon must have ended");
							win32_outer_action = 2;
						} else {
							upslogx(LOG_ERR, "read confirmation failed, trying again");
							win32_outer_action = 1;
						}
						break;
					}
				}

				/* The code this replaces never called
				 * GetOverlappedResult() at all, and instead relied
				 * on the leftover zeroed tail of the memset() above
				 * to behave like a NUL terminator -- fragile, and
				 * unable to distinguish "0 bytes read" (e.g. a
				 * graceful pipe close by the peer) from "read
				 * failed". */
				if (!GetOverlappedResult(pipefd, &read_overlapped, &bytesRead, FALSE)) {
					upslogx(LOG_ERR, "GetOverlappedResult failed (error %lu)",
						(unsigned long)GetLastError());
					win32_outer_action = list_timers ? 2 : 1;
					break;
				}

				if (bytesRead >= sizeof(buf))
					bytesRead = sizeof(buf) - 1;
				buf[bytesRead] = '\0';

				if (bytesRead == 0)
					break;

				if (list_timers) {
					/* ASSUME we see whole starting and/or ending lines
					 * of the response within one read() operation
					 */
					char	*end = strstr(buf, "END LIST TIMERS\n"),
							*ok = strstr(buf, "OK\n");

					if (end)
						*end = '\0';

					if (ok)
						*ok = '\0';

					if (!strncmp(buf, "BEGIN LIST TIMERS\n", 18)) {
						printf("%s", buf + 18);
					} else {
						printf("%s", buf);
					}

					if (ok) {
						snprintf(buf, sizeof(buf), "OK\n");
					} else {
						/* More of the multi-line response may
						 * still be coming: read again. */
						keep_reading = 1;
					}
				}
			} while (keep_reading);

			CloseHandle(read_overlapped.hEvent);
			CloseHandle(pipefd);

			if (win32_outer_action == 2)
				break;
			if (win32_outer_action == 1)
				continue;
		}
#endif /* WIN32 */

		if (!strncmp(buf, "OK", 2))
			return;		/* success */

		upslogx(LOG_ERR, "read confirmation got [%s]", buf);

		if (list_timers)
			break;

		/* try again ... */
	}	/* loop until MAX_TRIES if no success above */

	if (list_timers)
		fatalx(EXIT_FAILURE, "Unable to connect to daemon or connection was broken during listing");

	fatalx(EXIT_FAILURE, "Unable to connect to daemon and unable to start daemon");
}

static void parse_at(const char *ntype, const char *un, const char *cmd,
		const char *ca1, const char *ca2)
{
	/* complain both ways in case we don't have a tty */

	if (!cmdscript_argc) {
		printf("CMDSCRIPT must be set before any ATs in the config file!\n");
		fatalx(EXIT_FAILURE, "CMDSCRIPT must be set before any ATs in the config file!");
	}

	if (!pipefn) {
		printf("PIPEFN must be set before any ATs in the config file!\n");
		fatalx(EXIT_FAILURE, "PIPEFN must be set before any ATs in the config file!");
	}

	if (!lockfn) {
		printf("LOCKFN must be set before any ATs in the config file!\n");
		fatalx(EXIT_FAILURE, "LOCKFN must be set before any ATs in the config file!");
	}

	/* check ups_name: does this apply to us? */
	upsdebugx(3, "%s: is '%s' in AT command the '%s' we were launched to process?",
		__func__, un, ups_name);
	if (strcmp(ups_name, un) != 0) {
		if (strcmp(un, "*") != 0) {
			upsdebugx(4, "%s: SKIP: '%s' in AT command "
				"did not match the '%s' UPSNAME "
				"we were launched to process",
				__func__, un, ups_name);
			return;		/* not for us, and not the wildcard */
		} else {
			upsdebugx(2, "%s: this AT command is for a wildcard: matched", __func__);
		}
	} else {
		upsdebugx(2, "%s: '%s' in AT command matched the '%s' "
			"UPSNAME we were launched to process",
			__func__, un, ups_name);
	}

	/* see if the current notify type matches the one from the .conf */
	if (strcasecmp(notify_type, ntype) != 0) {
		upsdebugx(4, "%s: SKIP: '%s' in AT command "
			"did not match the '%s' NOTIFYTYPE "
			"we were launched to process",
			__func__, ntype, notify_type);
		return;
	}

	/* if command is valid, send it to the daemon (which may start it) */

	if (!strcmp(cmd, "START-TIMER")) {
		upsdebugx(1, "%s: processing %s\t[%s]\t[%s]\t[%s]\t[%s]\t[%s]", __func__, cmd,
			NUT_STRARG(ca1), NUT_STRARG(ca2),
			NUT_STRARG(notify_type), NUT_STRARG(ups_name),
			NUT_STRARG(notify_msg));
		sendcmd("START", ca1, ca2);
		return;
	}

	if (!strcmp(cmd, "START-TIMER-SHARED")) {
		upsdebugx(1, "%s: processing %s\t[%s]\t[%s]\t[%s]\t[%s]\t[%s]", __func__, cmd,
			NUT_STRARG(ca1), NUT_STRARG(ca2),
			NUT_STRARG(notify_type), NUT_STRARG(ups_name),
			NUT_STRARG(notify_msg));
		sendcmd("START-SHARED", ca1, ca2);
		return;
	}

	if (!strcmp(cmd, "CANCEL-TIMER")) {
		upsdebugx(1, "%s: processing %s\t[%s]\t[%s]\t[%s]\t[%s]\t[%s]", __func__, cmd,
			NUT_STRARG(ca1), NUT_STRARG(ca2),
			NUT_STRARG(notify_type), NUT_STRARG(ups_name),
			NUT_STRARG(notify_msg));
		sendcmd("CANCEL", ca1, ca2);
		return;
	}

	if (!strcmp(cmd, "EXECUTE")) {
		upsdebugx(1, "%s: processing %s\t[%s]\t[%s]\t[%s]\t[%s]\t[%s]", __func__, cmd,
			NUT_STRARG(ca1), NUT_STRARG(ca2),
			NUT_STRARG(notify_type), NUT_STRARG(ups_name),
			NUT_STRARG(notify_msg));

		if (ca1[0] == '\0') {
			upslogx(LOG_ERR, "Empty EXECUTE command argument");
			return;
		}

		if (nut_debug_level)
			upslogx(LOG_INFO, "Executing command: %s", ca1);

		exec_cmd(ca1);
		return;
	}

	upslogx(LOG_ERR, "Invalid command: %s\t[%s]\t[%s]\t[%s]\t[%s]\t[%s]", cmd,
			NUT_STRARG(ca1), NUT_STRARG(ca2),
			NUT_STRARG(notify_type), NUT_STRARG(ups_name),
			NUT_STRARG(notify_msg));
}

static int conf_arg(size_t numargs, char **arg)
{
	if (numargs < 2)
		return 0;

	/* CMDSCRIPT <scriptname> [<arg1> [<arg2>...]] */
	if (!strcmp(arg[0], "CMDSCRIPT")) {
		size_t	i, l = 0;

		/* -1: the arg[0] is the configuration token */
		cmdscript_argc = numargs - 1;

		/* +1: the cmdscript_argv[cmdscript_argc] is the NULL sentinel */
		free(cmdscript_argv);
		cmdscript_argv = (char**)xcalloc(cmdscript_argc + 1, sizeof(char *));
		for (i = 1; i < numargs; i++) {
			/* +1: either a space follows, or '\0'
			 * +2: surrounding quotes
			 */
			l += strlen(arg[i]) + 3;
			cmdscript_argv[i - 1] = xstrdup(arg[i]);
			strcpy(cmdscript_argv[i - 1], arg[i]);
		}
		cmdscript_argv[cmdscript_argc] = NULL;

		free(cmdscript_concat);
		cmdscript_concat = (char*)xcalloc(l + 2, sizeof(char));
		for (i = 1; i < numargs; i++) {
			snprintfcat(cmdscript_concat, l + 1, "'%s'%s",
				arg[i], i == numargs - 1 ? "" : " ");
		}

		upsdebugx(1, "%s: collected %s with %" PRIuSIZE " tokens: %s",
			__func__, arg[0], cmdscript_argc, NUT_STRARG(cmdscript_concat));

		if (cmdscript_argc > 0 && strchr(cmdscript_argv[0], ' ')) {
			/* NOTE: this may also be a path with spaces, more prominent on Windows or MacOS, probably */
			upslogx(LOG_WARNING, "%s: command '%s' contains spaces, be sure to pass any arguments as separately quoted tokens!",
				arg[0], cmdscript_argv[0]);
		}

		/* Handled OK */
		return 1;
	}

	/* DEBUG_MIN <num> */
	if (!strcmp(arg[0], "DEBUG_MIN")) {
		if (str_to_int(arg[1], &nut_debug_level_conf, 10)) {
			if (nut_debug_level_conf > nut_debug_level) {
				nut_debug_level = nut_debug_level_conf;
				upsdebugx(1, "Applying debug_min=%d from upssched.conf", nut_debug_level);
			}
		}
		return 1;
	}

	/* PIPEFN <pipename> */
	if (!strcmp(arg[0], "PIPEFN")) {
#ifndef WIN32
		pipefn = xstrdup(arg[1]);
#else	/* WIN32 */
		if (arg[1] && strlen(arg[1]) > 9
		 && !strncmp(arg[1], "\\\\.\\pipe\\", 9)
		) {
			pipefn = xstrdup(arg[1]);
		} else {
			pipefn = xstrdup("\\\\.\\pipe\\upssched");
			upslogx(LOG_WARNING, "%s: Invalid PIPEFN '%s' provided: "
				"must start with '\\\\.\\pipe\\' for this platform; "
				"falling back to default '%s'",
				__func__, NUT_STRARG(arg[1]), pipefn);
		}
#endif	/* WIN32 */
		return 1;
	}

	/* LOCKFN <filename> */
	if (!strcmp(arg[0], "LOCKFN")) {
#ifndef WIN32
		lockfn = xstrdup(arg[1]);
#else	/* WIN32 */
		lockfn = filter_path(arg[1]);
#endif	/* WIN32 */
		return 1;
	}

	/* In list_timers mode ("-l") and, on WIN32, in the timer-daemon
	 * child process (is_win32_timer_daemon -- see WIN32_DAEMON_MARKER
	 * and win32_run_timer_daemon()), we only care about the global
	 * directives handled above (PIPEFN, LOCKFN, CMDSCRIPT, DEBUG_MIN).
	 * "AT" lines describe notification-triggered actions and must NOT
	 * be acted upon (parse_at() -> sendcmd()) by either of these two
	 * modes: list_timers is a read-only query, and the WIN32 daemon
	 * child is not forwarding any notification of its own -- doing so
	 * would resend a stale/foreign command and, on WIN32, is exactly
	 * what previously caused a runaway cascade of spawned processes. */
#ifdef WIN32
	if (list_timers || is_win32_timer_daemon)
		return 2;
#else
	if (list_timers)
		return 2;
#endif	/* WIN32 */

	if (numargs < 5)
		return 0;

	/* AT <notifytype> <upsname> <command> <cmdarg1> [<cmdarg2>] */
	if (!strcmp(arg[0], "AT")) {

		/* don't use arg[5] unless we have it... */
		if (numargs > 5)
			parse_at(arg[1], arg[2], arg[3], arg[4], arg[5]);
		else
			parse_at(arg[1], arg[2], arg[3], arg[4], NULL);

		return 1;
	}

	return 0;
}

/* called for fatal errors in parseconf like malloc failures */
static void upssched_err(const char *errmsg)
{
	upslogx(LOG_ERR, "Fatal error in parseconf(upssched.conf): %s", errmsg);
}

static void checkconf(void)
{
	char	fn[NUT_PATH_MAX + 1];
	PCONF_CTX_t	ctx;
	int	numerrors = 0;

	snprintf(fn, sizeof(fn), "%s/upssched.conf", confpath());

	pconf_init(&ctx, upssched_err);

	if (!pconf_file_begin(&ctx, fn)) {
		pconf_finish(&ctx);
		fatalx(EXIT_FAILURE, "%s", ctx.errmsg);
	}

	while (pconf_file_next(&ctx)) {
		if (pconf_parse_error(&ctx)) {
			upslogx(LOG_ERR, "Parse error: %s:%d: %s",
				fn, ctx.linenum, ctx.errmsg);
			numerrors++;
			continue;
		}

		if (ctx.numargs < 1)
			continue;

		/* Note: for list_timers mode this only parses some config values
		 * that are of interest for communications with the timer daemon
		 * (e.g. PIPEFN), but for normal mode this also calls parse_at()
		 * and does the actual work, as/when relevant, line by line.
		 */
		if (!conf_arg(ctx.numargs, ctx.arglist)) {
			unsigned int	i;
			char	errmsg[SMALLBUF];

			snprintf(errmsg, sizeof(errmsg),
				"upssched.conf: invalid directive");

			for (i = 0; i < ctx.numargs; i++)
				snprintfcat(errmsg, sizeof(errmsg), " %s",
					ctx.arglist[i]);

			numerrors++;
			upslogx(LOG_WARNING, "%s", errmsg);
		}
	}

	if (list_timers) {
		if (!pipefn || !(*pipefn)) {
			fatalx(EXIT_FAILURE, "upssched.conf: invalid configuration for timer listing: lacks PIPEFN");
		}

		upsdebugx(1, "%s: processing LIST-TIMERS", __func__);

		/* Here the send() also handles the response for this use-case */
		sendcmd("LIST-TIMERS", NULL, NULL);
	}

#ifdef WIN32
	if (is_win32_timer_daemon) {
		if (!pipefn || !(*pipefn)) {
			fatalx(EXIT_FAILURE, "upssched.conf: invalid configuration for WIN32 timer daemon: lacks PIPEFN");
		}
		if (!lockfn || !(*lockfn)) {
			fatalx(EXIT_FAILURE, "upssched.conf: invalid configuration for WIN32 timer daemon: lacks LOCKFN");
		}

		upsdebugx(1, "%s: config parsed for WIN32 timer daemon child", __func__);

		/* NOTE: unlike list_timers mode above, we do NOT sendcmd()
		 * here: this process IS the daemon being brought up, not a
		 * client asking an already-running daemon to list its timers.
		 * Control returns to main(), which calls
		 * win32_run_timer_daemon() next. */
	}
#endif	/* WIN32 */

	/* FIXME: Per legacy behavior, we silently went on.
	 *  Maybe should abort on unusable configs?
	 */
	if (numerrors) {
		upslogx(LOG_ERR, "Encountered %d config errors, those entries were ignored", numerrors);
	}

	pconf_finish(&ctx);
}

static void clean_exit(void)
{
	ttype_t	*tcurr, *tnext;
	conn_t	*ccurr, *cnext;
	size_t	i;

	/* Flush *our* output before possibly failing in third-party code
	 * (e.g. SSL libs), so client consumers have a chance to see it */
	fflush(stdout);
	fflush(stderr);

	upsdebugx(1, "%s: starting", __func__);

	/* Free timers */
	tcurr = thead;
	while (tcurr) {
		tnext = tcurr->next;

		free(tcurr->name);

		if (tcurr->upsnames) {
			for (i = 0; tcurr->upsnames[i]; i++) {
				free(tcurr->upsnames[i]);
			}
			free(tcurr->upsnames);
		}

		if (tcurr->notifytypes) {
			for (i = 0; tcurr->notifytypes[i]; i++) {
				free(tcurr->notifytypes[i]);
			}
			free(tcurr->notifytypes);
		}

		if (tcurr->notifymsgs) {
			for (i = 0; tcurr->notifymsgs[i]; i++) {
				free(tcurr->notifymsgs[i]);
			}
			free(tcurr->notifymsgs);
		}

		free(tcurr);
		tcurr = tnext;
	}
	thead = NULL;

	/* Free connections */
	ccurr = connhead;
	while (ccurr) {
		cnext = ccurr->next;
		pconf_finish(&ccurr->ctx);
		free(ccurr);
		ccurr = cnext;
	}
	connhead = NULL;

	/* Free strings and arrays */
	if (cmdscript_argv) {
		for (i = 0; i < cmdscript_argc; i++) {
			free(cmdscript_argv[i]);
		}
		free(cmdscript_argv);
		cmdscript_argv = NULL;
	}

	free(cmdscript_concat);
	cmdscript_concat = NULL;

	free(pipefn);
	pipefn = NULL;

	free(lockfn);
	lockfn = NULL;

	upsdebugx(1, "%s: finished, exiting", __func__);
}

static void help(const char *arg_progname)
	__attribute__((noreturn));

/* For getopt loops; should match usage documented below: */
static const char	optstring[] = "+DVhl";

static void help(const char *arg_progname)
{
	printf("upssched: upsmon's scheduling helper for offset timers\n");
	printf("Practical behavior is managed by UPSNAME and NOTIFYTYPE envvars\n");

	printf("\nUsage: %s [OPTIONS] [NOTIFYMSG]\n\n", arg_progname);
	printf("  -D		raise debugging level (NOTE: keeps reporting when daemonized)\n");
	printf("  -V		display the version of this software\n");
	printf("  -h		display this help\n");
	printf("  -l		display currently pending timers (if any)\n");

	nut_report_config_flags();

	printf("\n%s", suggest_doc_links(arg_progname, "upssched.conf"));

	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int	opt_ret;

	/* Here this is a global variable, used also in start_daemon() */
	prog = getprogname_argv0_default(argc > 0 ? argv[0] : NULL, "upssched");

#ifdef WIN32
	/* WIN32 replacement for fork(): see the rewritten WIN32 branch of
	 * start_daemon() for the process-spawning side of this handshake.
	 * CreateProcess() always starts a brand-new process at main() --
	 * there is no equivalent of the "if (pid == 0) { ...child
	 * continues here... }" branch fork() gives us on POSIX. So we
	 * recognize our own hidden marker in argv[1] here, at the very
	 * top of main() (before getopt() and before the UPSNAME/
	 * NOTIFYTYPE environment check further below), to take that
	 * "we are the daemon" role explicitly -- instead of falling
	 * through into the normal CLI path, which would re-parse the
	 * config and re-send the very notification that caused upsmon to
	 * invoke this program in the first place. That re-sending, plus
	 * a lock-file race further down the call chain, is what used to
	 * cause a runaway cascade of spawned processes; this patch fixes
	 * it by giving the spawned process an explicit, unambiguous
	 * signal of its role, mirroring fork()'s pid==0 branch.
	 *
	 * argv[2], if present, carries the hex-encoded value of a HANDLE
	 * to a manual-reset Win32 event object created by the spawning
	 * process (see start_daemon()) and marked inheritable; we
	 * SetEvent() it, in win32_run_timer_daemon(), once our named pipe
	 * is up and listening, so the spawning process (still waiting
	 * inside its own start_daemon() call) knows it is safe to
	 * proceed. */
	if (argc > 1 && !strcmp(argv[1], WIN32_DAEMON_MARKER)) {
		is_win32_timer_daemon = 1;

		if (argc > 2 && argv[2] && *argv[2]) {
			win32_daemon_ready_event = (HANDLE)(uintptr_t)strtoull(argv[2], NULL, 16);
		}

		/* Do NOT touch UPSNAME/NOTIFYTYPE/NOTIFYMSG here: this is
		 * not a CLI invocation forwarding an upsmon notification, so
		 * those variables -- even if somehow still present in our
		 * environment -- are simply irrelevant on this codepath.
		 * (start_daemon() already clears them from its own
		 * environment before spawning us, so in practice they won't
		 * be inherited at all any more.) */

		open_syslog(prog);
		syslogbit_set();
		atexit(clean_exit);
		setproctag("timer");

		upsdebugx(1, "%s: running as the WIN32 timer-daemon child process", __func__);

		/* Parses PIPEFN/LOCKFN/CMDSCRIPT/DEBUG_MIN from
		 * upssched.conf via conf_arg(), gated by
		 * is_win32_timer_daemon (see conf_arg() and checkconf()
		 * above) to skip acting on any "AT" line. */
		checkconf();

		win32_run_timer_daemon();

		/* win32_run_timer_daemon() only returns after a fatal wait
		 * error (see WAIT_FAILED handling within it); nothing
		 * sensible left to do here. */
		exit(EXIT_FAILURE);
	}
#endif	/* WIN32 */

	while ((opt_ret = getopt(argc, argv, optstring)) != -1) {
		switch (opt_ret) {
			case 'D':
				nut_debug_level_args++;
				break;

			case 'h':
				help(prog);
#ifndef HAVE___ATTRIBUTE__NORETURN
				break;
#endif

			case 'l':
				list_timers = 1;
				break;

			case 'V':
				/* just show the optional CONFIG_FLAGS banner */
				nut_report_config_flags();
				exit(EXIT_SUCCESS);

			default:
				fatalx(EXIT_FAILURE,
					"Error: unknown option -%c. Try -h for help.",
					(char)opt_ret);
		}
	}

	nut_debug_level = nut_debug_level_args;
	{ /* scoping */
		char *s = getenv("NUT_DEBUG_LEVEL");
		if (s && str_to_int(s, &nut_debug_level_env, 10)) {
			if (nut_debug_level_env > 0 && nut_debug_level_args < 1) {
				upslogx(LOG_INFO, "Defaulting debug verbosity to NUT_DEBUG_LEVEL=%d "
					"since none was requested by command-line options", nut_debug_level_env);
				nut_debug_level = nut_debug_level_env;
			}	/* else follow -D and/or upssched.conf DEBUG_MIN settings */
		}	/* else nothing to bother about */
	}

	/* normally we don't have stderr, so get this going to syslog early */
	open_syslog(prog);
	syslogbit_set();

	ups_name = getenv("UPSNAME");
	notify_type = getenv("NOTIFYTYPE");
	upsdebugx(2, "Handled optind=%d CLI tokens of argc=%d", optind - 1, argc);
	if (argc > optind && *argv[optind])
		notify_msg = argv[optind];

	if ((!list_timers) && ((!ups_name) || (!notify_type))) {
		printf("Error: environment variables UPSNAME and NOTIFYTYPE must be set.\n");
		printf("This program should only be run from upsmon(%s).\n", MAN_SECTION_CMD_SYS);
		exit(EXIT_FAILURE);
	}

	upsdebugx(1, "Handling NOTIFYTYPE='%s' for UPSNAME='%s'", notify_type, ups_name);
	if (notify_msg)
		upsdebugx(1, "Got a NOTIFYMSG from command line: %s", notify_msg);
	else
		upsdebugx(1, "Did not get any NOTIFYMSG from command line");

	/* Whenever a process exits, do carefully free any resources it
	 * has (maybe by parent, from before forking some notifier etc.) */
	atexit(clean_exit);

	setproctag("cli");

	/* See if this request matches anything in the config file */
	/* This is actually the processing loop:
	 * checkconf -> conf_arg -> parse_at -> sendcmd -> daemon if needed
	 *  -> start_daemon -> conn_add(pipefd) or sock_read(conn)
	 */
	checkconf();

	upsdebugx(1, "Exiting upssched (CLI process)");
	exit(EXIT_SUCCESS);
}
