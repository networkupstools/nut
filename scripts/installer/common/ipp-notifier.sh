#!/bin/sh
#	ipp-notifier
#
#	Copyright (c) 2013-2017, by Eaton (R) Corporation. All rights reserved.
#
#	A shell script to send e-mail from IPP - Unix (NUT)
#	Sends an email messages to one user to notify of a power event.
#
# Requires configuration from ipp.conf, or otherwise default to the values below
#
# MC2 Sync:
#	mc2/scripts/lang/eng.lbl (msg)
#	mc2/scripts/libs/labels.js (ldate, nodeName, Default{Subject,Message}
#
# TODO per IPSSUNIX-36:
#	* There is no error-checking, e.g. a `mailx` command can break due to
#	  unknown parameters (say, `smtp-use-starttls` is not known in Solaris)
#	* The two codepaths for email are a) local delivery with no "@" in
#	  recipient, or b) Full delivery using a remote relay, with account
#	  and password, etc. - assuming all variables for this are configured.
#	  There is no codepath for delivery to remote account via local relay.

NUT_DIR="/usr/local/ups"
NUT_CFG_DIR=""
for D in "$NUT_DIR/etc" "/etc/nut" "/etc/ups" ; do
	if [ -d "$D" ] && [ -f "$D/ups.conf" ] && [ -f "$D/ipp.conf" ] ; then
		NUT_CFG_DIR="$D"
		break
	fi
done
unset D
CONFIG_IPP="$NUT_CFG_DIR/ipp.conf"

PATH="$PATH:/usr/local/bin:/usr/local/sbin:/usr/bin:/usr/sbin:/bin:/sbin:/usr/ucb:/usr/ccs/bin:/usr/xpg4/bin:/usr/xpg6/bin"
# Note: $NUT_DIR/xbin holds the wrappers to run NUT binaries with co-bundled
# third party libs and hopefully without conflicts induced for the OS binaries
# Use them first!
PATH="$NUT_DIR/xbin:$NUT_DIR/sbin:$NUT_DIR/bin:$PATH"
export PATH

# Search for binaries under current PATH normally, no hardcoding. Scripts:
NUT_IPP_OS_SHUTDOWN="ipp-os-shutdown"
NUT_IPPSTATUS="ipp-status"
#NUT_IPP_OS_SHUTDOWN="$NUT_DIR/sbin/ipp-os-shutdown"
#NUT_IPPSTATUS="$NUT_DIR/bin/ipp-status"

# Set default values
# Notification configuration
MAIL_NOTIF=1
CONSOLE_NOTIF=1
SYSLOG_NOTIF=1
# Mail configuration (simple (Lansafe) version)
To_User="root"

# NOTE: for systems with role-based access and equivalents, or
# group-suid for /sbin/shutdown, etc. it may also be possible
# to allow "ipp" directly to use the command - in this case
# set SHUTDOWN_NEEDS_ROOT=0 in the $CONFIG_IPP file.
SHUTDOWN_NEEDS_ROOT=1
SHUTDOWN_TIMER=-1

# Commands
CMD_WALL="wall"
CMD_MAIL="mailx -n"
CMD_SYSLOG="logger"

# Create the message parts
ldate="`date +\"%y-%m-%d - %H:%M:%S\"`"
nodeName="$UPSNAME@`hostname`"
DefaultSubject="Intelligent Power Protector (IPP) Alarms"
DefaultMessage="Alarm from ${nodeName}:\n\n\t${ldate} - $*"
NeedRootMessage="WARNING: ipp-notifier.sh must run as root to request/cancel OS shutdown\nConsider changing the NUT init script to start 'upsmon -p' so you can use SHUTDOWN_TIMER>=0"
#...or change upsmon.conf to contain "RUN_AS_USER root"

# Include IPP ipp.conf (may overwrite the above default values!)
if [ -f "$CONFIG_IPP" ] ; then
	. "$CONFIG_IPP"
fi

if [ "$SHUTDOWN_NEEDS_ROOT" = 1 ] && [ "$SHUTDOWN_TIMER" -gt -1 ] 2>/dev/null ;
then
	who="`whoami 2>/dev/null`"
	if [ "$who" != 'root' ]; then
#work around in case no whoami is present
		who="`id -u 2>/dev/null`"
		if [ "$who" != '0' ]; then
			who="`id 2>/dev/null | grep -w '0' | grep -w root`"
			if [ -z "$who" ]; then
				DefaultMessage="$DefaultMessage\n$NeedRootMessage"
			fi
		fi
	fi
fi

if [ "$IPP_DEBUG" = yes ] ; then
	exec >> /var/tmp/ipp-notifier.log 2>&1
	echo "`date`: Started: $0 $*" >&2
	set >&2
	set -x
fi

# Console notification
if [ "$CONSOLE_NOTIF" -eq 1 ];
then
	echo "$DefaultSubject\n\n$DefaultMessage" | $CMD_WALL
fi

# Mail notification
OTHER_OPT=""

if [ "$MAIL_NOTIF" -eq 1 ];
then
	echo "$To_User" | grep "@"
	if [ $? -eq 1 ]
	then
		# Simple (Lansafe / local) version
		env MAILRC=/dev/null echo "$DefaultMessage" | $CMD_MAIL -s "$DefaultSubject" $To_User
	else
		# GMail need starttls
		echo "$SMTP_Server" | grep "gmail.com"
		if [ $? -eq 0 ]; then
			OTHER_OPT="-S smtp-use-starttls"
		fi

		env MAILRC=/dev/null echo -e "$DefaultMessage" | $CMD_MAIL -s "$DefaultSubject" \
			-S from="$SMTP_User" \
			-S smtp="smtp://$SMTP_Server" \
			-S smtp-auth-user="$SMTP_User" \
			-S smtp-auth-password="$SMTP_Password" \
			-S smtp-auth=login \
			-S ssl-verify=ignore $OTHER_OPT \
			$To_User
	fi
fi

# System log notification
if [ "$SYSLOG_NOTIF" -eq 1 ];
then
	$CMD_SYSLOG -t eaton-ipp "$*"
fi

# Scheduled shutdown - request or cancel based on this variable and on
# the UPS state (amount of ONLINE sources under the MINSUPPLIES or not?)
if [ "$SHUTDOWN_TIMER" -gt -1 ] 2>/dev/null; then
	# Note this status check clears any invalid PIDfiles
	SD_STATUS="`${NUT_IPP_OS_SHUTDOWN} status`"
	SD_STATE=$?
	${NUT_IPPSTATUS} -q
	if [ $? -eq 1 ] ; then
		if [ "$SD_STATE" = 0 ]; then
			${NUT_IPP_OS_SHUTDOWN} &
			sleep 5 # Let the script begin its work and PIDfiles
			${NUT_IPP_OS_SHUTDOWN} status
			SD_STATE=$?
			if [ "$SD_STATE" -gt 0  ]; then
				echo "Shutdown delayed by ${SHUTDOWN_TIMER}m was requested at ${ldate}"
				echo "${DefaultSubject}\nPowerfail-shutdown delayed by ${SHUTDOWN_TIMER}m was requested at ${ldate}" | $CMD_WALL 2>/dev/null
			else # Nothing pending nor irreversible
				echo "FAILED TO REQUEST a Shutdown delayed by ${SHUTDOWN_TIMER}m at ${ldate}"
				echo "${DefaultSubject}\nFAILED TO REQUEST a Powerfail-shutdown delayed by ${SHUTDOWN_TIMER}m at ${ldate}" | $CMD_WALL 2>/dev/null
			fi
		else
			echo "Shutdown request already running"
		fi
	else
		if [ "$SD_STATE" = 1 ]; then
			# Cancel any pending not-irreversible shutdown
			if ${NUT_IPP_OS_SHUTDOWN} -c ; then
				echo "Shutdown request canceled"
				echo "${DefaultSubject}\nPowerfail-shutdown request canceled at ${ldate}" | $CMD_WALL 2>/dev/null
			else
				echo "Could not cancel Shutdown request"
				echo "${DefaultSubject}\nPowerfail-shutdown request NOT CANCELED at ${ldate}" | $CMD_WALL 2>/dev/null
			fi
		fi
	fi
fi
