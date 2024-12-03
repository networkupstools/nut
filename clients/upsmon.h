/* upsmon.h - headers and other useful things for upsmon.h

   Copyright (C)
     2000  Russell Kroll <rkroll@exploits.org>
     2012  Arnaud Quette <arnaud.quette.free.fr>
     2017  Eaton (author: Arnaud Quette <ArnaudQuette@Eaton.com>)
     2020-2024  Jim Klimov <jimklimov+nut@gmail.com>

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

#ifndef NUT_UPSMON_H_SEEN
#define NUT_UPSMON_H_SEEN 1

#include "state.h"

/* flags for ups->status */

#define ST_ONLINE      (1 << 0)       /* UPS is on line (OL)                      */
#define ST_ONBATT      (1 << 1)       /* UPS is on battery (OB)                   */
#define ST_LOWBATT     (1 << 2)       /* UPS has a low battery (LB)               */
#define ST_FSD         (1 << 3)       /* primary has set forced shutdown flag     */
#define ST_PRIMARY     (1 << 4)       /* we are the primary (manager) of this UPS */
#define ST_MASTER      ST_PRIMARY     /* legacy alias                             */
#define ST_LOGIN       (1 << 5)       /* we are logged into this UPS              */
#define ST_CLICONNECTED (1 << 6)      /* upscli_connect returned OK               */
#define ST_CAL         (1 << 7)       /* UPS calibration in progress (CAL)        */
#define ST_OFF         (1 << 8)       /* UPS is administratively off or asleep (OFF) */
#define ST_BYPASS      (1 << 9)       /* UPS is on bypass so not protecting       */
#define ST_ECO         (1 << 10)      /* UPS is in ECO (High Efficiency) mode or similar tweak, e.g. Energy Saver System mode */
#define ST_ALARM       (1 << 11)      /* UPS has at least one active alarm        */
#define ST_OTHER       (1 << 12)      /* UPS has at least one unclassified status token */

/* required contents of flag file */
#define SDMAGIC "upsmon-shutdown-file"

#ifdef __cplusplus
/* *INDENT-OFF* */
extern "C" {
/* *INDENT-ON* */
#endif

/* UPS tracking structure */

typedef struct {
	UPSCONN_t	conn;			/* upsclient state descriptor	*/

	char	*sys;			/* raw system name from .conf	*/
	char	*upsname;		/* just upsname			*/
	char	*hostname;		/* just hostname		*/
	uint16_t	port;			/* just the port		*/

	unsigned int	pv;			/* power value from conf	*/
	char	*un;			/* username (optional for now)	*/
	char	*pw;  			/* password from conf		*/
	int	status;			/* status (see flags above)	*/
	st_tree_t	*status_tokens;	/* parsed ups.status, mapping each token to whatever value if it is currently set (evicted when not) */
	int	retain;			/* tracks deletions at reload	*/

	/* handle suppression of COMMOK and ONLINE at startup */
	int	commstate;		/* these start at -1, and only	*/
	int	linestate;		/* fire on a 0->1 transition	*/
	int	offstate;		/* fire on a 0->1 transition, may	*/
					/* be delayed vs. seeing OFF state	*/
	int	bypassstate;		/* fire on a 0->1 transition;	*/
					/* delays not implemented now	*/
	int	ecostate;		/* fire on a 0->1 transition;	*/
					/* delays not implemented now	*/
	int	alarmstate;		/* fire on a 0->1 transition;	*/
					/* delays not implemented now	*/

	/* see detailed comment for pollfail_log_throttle_max in upsmon.c
	 * about handling of poll failure log throttling (syslog storage I/O)
	 */
	int	pollfail_log_throttle_state;	/* Last (error) state which we throttle */
	int	pollfail_log_throttle_count;	/* How many pollfreq loops this UPS was in this state since last logged report? */

	time_t	lastpoll;		/* time of last successful poll	*/
	time_t  lastnoncrit;		/* time of last non-crit poll	*/
	time_t	lastrbwarn;		/* time of last REPLBATT warning*/
	time_t	lastncwarn;		/* time of last NOCOMM warning	*/

	time_t	offsince;		/* time of recent entry into OFF state	*/
	time_t	oblbsince;		/* time of recent entry into OB LB state (normally this causes immediate shutdown alert, unless we are configured to delay it)	*/

	void	*next;
}	utype_t;

/* notify identifiers */

#define NOTIFY_ONLINE	0	/* UPS went on-line                     */
#define NOTIFY_ONBATT	1	/* UPS went on battery                  */
#define NOTIFY_LOWBATT	2	/* UPS went to low battery              */
#define NOTIFY_FSD	3	/* Primary upsmon set FSD flag          */
#define NOTIFY_COMMOK	4	/* Communication established            */
#define NOTIFY_COMMBAD	5	/* Communication lost                   */
#define NOTIFY_SHUTDOWN	6	/* System shutdown in progress          */
#define NOTIFY_REPLBATT	7	/* UPS battery needs to be replaced     */
#define NOTIFY_NOCOMM	8	/* UPS hasn't been contacted in a while	*/
#define NOTIFY_NOPARENT	9	/* privileged parent process died       */
#define NOTIFY_CAL	10	/* UPS is performing calibration        */
#define NOTIFY_NOTCAL	11	/* UPS is performing calibration        */
#define NOTIFY_OFF	12	/* UPS is administratively OFF or asleep*/
#define NOTIFY_NOTOFF	13	/* UPS is not anymore administratively OFF or asleep*/
#define NOTIFY_BYPASS	14	/* UPS is administratively on bypass    */
#define NOTIFY_NOTBYPASS	15	/* UPS is not anymore administratively on bypass    */
#define NOTIFY_ECO	16	/* UPS is in ECO mode or similar        */
#define NOTIFY_NOTECO	17	/* UPS is not anymore in ECO mode or similar */
#define NOTIFY_ALARM	18	/* UPS has at least one active alarm    */
#define NOTIFY_NOTALARM	19	/* UPS has no active alarms    */

/* Special handling below */
#define NOTIFY_OTHER	28	/* UPS has at least one unclassified status token */
#define NOTIFY_NOTOTHER	29	/* UPS has no unclassified status tokens anymore */

#define NOTIFY_SUSPEND_STARTING	30	/* OS is entering sleep/suspend/hibernate slumber mode, and we know it   */
#define NOTIFY_SUSPEND_FINISHED	31	/* OS just finished sleep/suspend/hibernate slumber mode, and we know it */

/* notify flag values */

#define NOTIFY_IGNORE  (1 << 0)        /* don't do anything                */
#define NOTIFY_SYSLOG  (1 << 1)        /* send the msg to the syslog       */
#define NOTIFY_WALL    (1 << 2)        /* send the msg to all users        */
#define NOTIFY_EXEC    (1 << 3)        /* send the msg to NOTIFYCMD script */

/* flags are set to NOTIFY_SYSLOG | NOTIFY_WALL at program init		*/
/* except under Windows where they are set to NOTIFY_SYSLOG only	*/
/* the user can override with NOTIFYFLAGS in the upsmon.conf		*/

#ifdef WIN32
#define NOTIFY_DEFAULT	NOTIFY_SYSLOG
#else
#define NOTIFY_DEFAULT	(NOTIFY_SYSLOG | NOTIFY_WALL)
#endif

/* This is only used in upsmon.c, but might it also have external consumers?..
 * To move or not to move?..
 */
static struct {
	int	type;
	const	char	*name;
	char	*msg;	/* NULL until overridden */
	const char	*stockmsg;
	int	flags;
}	notifylist[] =
{
	{ NOTIFY_ONLINE,   "ONLINE",   NULL, "UPS %s on line power", NOTIFY_DEFAULT },
	{ NOTIFY_ONBATT,   "ONBATT",   NULL, "UPS %s on battery", NOTIFY_DEFAULT },
	{ NOTIFY_LOWBATT,  "LOWBATT",  NULL, "UPS %s battery is low", NOTIFY_DEFAULT },
	{ NOTIFY_FSD,      "FSD",      NULL, "UPS %s: forced shutdown in progress", NOTIFY_DEFAULT },
	{ NOTIFY_COMMOK,   "COMMOK",   NULL, "Communications with UPS %s established", NOTIFY_DEFAULT },
	{ NOTIFY_COMMBAD,  "COMMBAD",  NULL, "Communications with UPS %s lost", NOTIFY_DEFAULT },
	{ NOTIFY_SHUTDOWN, "SHUTDOWN", NULL, "Auto logout and shutdown proceeding", NOTIFY_DEFAULT },
	{ NOTIFY_REPLBATT, "REPLBATT", NULL, "UPS %s battery needs to be replaced", NOTIFY_DEFAULT },
	{ NOTIFY_NOCOMM,   "NOCOMM",   NULL, "UPS %s is unavailable", NOTIFY_DEFAULT },
	{ NOTIFY_NOPARENT, "NOPARENT", NULL, "upsmon parent process died - shutdown impossible", NOTIFY_DEFAULT },
	{ NOTIFY_CAL,      "CAL",      NULL, "UPS %s: calibration in progress", NOTIFY_DEFAULT },
	{ NOTIFY_NOTCAL,   "NOTCAL",   NULL, "UPS %s: calibration finished", NOTIFY_DEFAULT },
	{ NOTIFY_OFF,      "OFF",      NULL, "UPS %s: administratively OFF or asleep", NOTIFY_DEFAULT },
	{ NOTIFY_NOTOFF,   "NOTOFF",   NULL, "UPS %s: no longer administratively OFF or asleep", NOTIFY_DEFAULT },
	{ NOTIFY_BYPASS,   "BYPASS",   NULL, "UPS %s: on bypass (powered, not protecting)", NOTIFY_DEFAULT },
	{ NOTIFY_NOTBYPASS,"NOTBYPASS",NULL, "UPS %s: no longer on bypass", NOTIFY_DEFAULT },
	{ NOTIFY_ECO,      "ECO",      NULL, "UPS %s: in ECO mode (as defined by vendor)", NOTIFY_DEFAULT },
	{ NOTIFY_NOTECO,   "NOTECO",   NULL, "UPS %s: no longer in ECO mode", NOTIFY_DEFAULT },

	/* NOTE: We remember the ups.alarm value and report it here,
	 * maybe optionally - e.g. check if the "ALARM" formatting
	 * string has actually one or two "%s" placeholders inside.
	 * Do issue a new notification if ups.alarm value changes.
	 */
	{ NOTIFY_ALARM,    "ALARM",    NULL, "UPS %s: one or more active alarms: [%s]", NOTIFY_DEFAULT },
	{ NOTIFY_NOTALARM, "NOTALARM", NULL, "UPS %s is no longer in an alarm state (no active alarms)", NOTIFY_DEFAULT },

	/* Special handling, two string placeholders!
	 * Reported when status_tokens tree changes (and is not empty in the end) */
	{ NOTIFY_OTHER,    "OTHER",    NULL, "UPS %s: has at least one unclassified status token: [%s]", NOTIFY_DEFAULT },
	/* Reported when status_tokens tree becomes empty */
	{ NOTIFY_NOTOTHER, "NOTOTHER", NULL, "UPS %s has no unclassified status tokens anymore", NOTIFY_DEFAULT },

	{ NOTIFY_SUSPEND_STARTING, "SUSPEND_STARTING", NULL, "OS is entering sleep/suspend/hibernate mode", NOTIFY_DEFAULT },
	{ NOTIFY_SUSPEND_FINISHED, "SUSPEND_FINISHED", NULL, "OS just finished sleep/suspend/hibernate mode, de-activating obsolete UPS readings to avoid an unfortunate shutdown", NOTIFY_DEFAULT },

	{ 0, NULL, NULL, NULL, 0 }
};

/* values for signals passed between processes */

#ifndef WIN32
#define SIGCMD_FSD	SIGUSR1
#define SIGCMD_STOP	SIGTERM
#define SIGCMD_RELOAD	SIGHUP
#else
#define SIGCMD_FSD	COMMAND_FSD
#define SIGCMD_STOP	COMMAND_STOP
#define SIGCMD_RELOAD	COMMAND_RELOAD
#endif

/* various constants */

#define NET_TIMEOUT 10		/* wait 10 seconds max for upsd to respond */

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif	/* NUT_UPSMON_H_SEEN */
