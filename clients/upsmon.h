/* upsmon.h - headers and other useful things for upsmon.h

   Copyright (C) 2000  Russell Kroll <rkroll@exploits.org>

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

/* flags for ups->status */

#define ST_ONLINE      (1 << 0)       /* UPS is on line (OL)                  */
#define ST_ONBATT      (1 << 1)       /* UPS is on battery (OB)               */
#define ST_LOWBATT     (1 << 2)       /* UPS has a low battery (LB)           */
#define ST_FSD         (1 << 3)       /* master has set forced shutdown flag  */
#define ST_MASTER      (1 << 4)       /* we are the master on this UPS        */
#define ST_LOGIN       (1 << 5)       /* we are logged into this UPS          */
#define ST_CONNECTED   (1 << 6)       /* upscli_connect returned OK           */
#define ST_CAL         (1 << 7)       /* UPS calibration in progress (CAL)    */

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
	int	port;			/* just the port		*/

	int	pv;			/* power value from conf	*/
	char	*un;			/* username (optional for now)	*/
	char	*pw;  			/* password from conf		*/
	int	status;			/* status (see flags above)	*/
	int	retain;			/* tracks deletions at reload	*/

	/* handle suppression of COMMOK and ONLINE at startup */
	int	commstate;		/* these start at -1, and only	*/
	int	linestate;		/* fire on a 0->1 transition	*/

	time_t	lastpoll;		/* time of last successful poll	*/
	time_t  lastnoncrit;		/* time of last non-crit poll	*/
	time_t	lastrbwarn;		/* time of last REPLBATT warning*/
	time_t	lastncwarn;		/* time of last NOCOMM warning	*/
	void	*next;
}	utype_t;

/* notify identifiers */

#define NOTIFY_ONLINE	0	/* UPS went on-line			*/
#define NOTIFY_ONBATT	1	/* UPS went on battery			*/
#define NOTIFY_LOWBATT	2	/* UPS went to low battery		*/
#define NOTIFY_FSD	3	/* Master upsmon set FSD flag		*/
#define NOTIFY_COMMOK	4	/* Communication established		*/
#define NOTIFY_COMMBAD	5	/* Communication lost			*/
#define NOTIFY_SHUTDOWN	6	/* System shutdown in progress		*/
#define NOTIFY_REPLBATT	7	/* UPS battery needs to be replaced	*/
#define NOTIFY_NOCOMM	8	/* UPS hasn't been contacted in awhile	*/
#define NOTIFY_NOPARENT	9	/* privileged parent process died	*/
#define NOTIFY_CAL		10	/* UPS is performing calibration   */

/* notify flag values */

#define NOTIFY_IGNORE  (1 << 0)        /* don't do anything                */
#define NOTIFY_SYSLOG  (1 << 1)        /* send the msg to the syslog       */
#define NOTIFY_WALL    (1 << 2)        /* send the msg to all users        */
#define NOTIFY_EXEC    (1 << 3)        /* send the msg to NOTIFYCMD script */

/* flags are set to NOTIFY_SYSLOG | NOTIFY_WALL at program init	*/
/* the user can override with NOTIFYFLAGS in the upsmon.conf	*/

/* This is only used in upsmon.c, but might it also have external consumers?..
 * To move or not to move?..
 */
static struct {
	int	type;
	const	char	*name;
	char	*msg;		/* NULL until overridden */
	const char	*stockmsg;
	int	flags;
}	notifylist[] =
{
	{ NOTIFY_ONLINE,   "ONLINE",   NULL, "UPS %s on line power", NOTIFY_SYSLOG | NOTIFY_WALL },
	{ NOTIFY_ONBATT,   "ONBATT",   NULL, "UPS %s on battery", NOTIFY_SYSLOG | NOTIFY_WALL },
	{ NOTIFY_LOWBATT,  "LOWBATT",  NULL, "UPS %s battery is low", NOTIFY_SYSLOG | NOTIFY_WALL },
	{ NOTIFY_FSD,	   "FSD",      NULL, "UPS %s: forced shutdown in progress", NOTIFY_SYSLOG | NOTIFY_WALL },
	{ NOTIFY_COMMOK,   "COMMOK",   NULL, "Communications with UPS %s established", NOTIFY_SYSLOG | NOTIFY_WALL },
	{ NOTIFY_COMMBAD,  "COMMBAD",  NULL, "Communications with UPS %s lost", NOTIFY_SYSLOG | NOTIFY_WALL },
	{ NOTIFY_SHUTDOWN, "SHUTDOWN", NULL, "Auto logout and shutdown proceeding", NOTIFY_SYSLOG | NOTIFY_WALL },
	{ NOTIFY_REPLBATT, "REPLBATT", NULL, "UPS %s battery needs to be replaced", NOTIFY_SYSLOG | NOTIFY_WALL },
	{ NOTIFY_NOCOMM,   "NOCOMM",   NULL, "UPS %s is unavailable", NOTIFY_SYSLOG | NOTIFY_WALL },
	{ NOTIFY_NOPARENT, "NOPARENT", NULL, "upsmon parent process died - shutdown impossible", NOTIFY_SYSLOG | NOTIFY_WALL },
	{ NOTIFY_CAL,      "CAL",      NULL, "UPS %s: calibration in progress", NOTIFY_SYSLOG },
	{ 0, NULL, NULL, NULL, 0 }
};

/* values for signals passed between processes */

#define SIGCMD_FSD	SIGUSR1
#define SIGCMD_STOP	SIGTERM
#define SIGCMD_RELOAD	SIGHUP

/* various constants */

#define NET_TIMEOUT 10		/* wait 10 seconds max for upsd to respond */

#ifdef __cplusplus
/* *INDENT-OFF* */
}
/* *INDENT-ON* */
#endif

#endif	/* NUT_UPSMON_H_SEEN */
