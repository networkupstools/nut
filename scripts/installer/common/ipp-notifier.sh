#!/bin/sh
#	ipp-notifier
#
#	Copyright (c) 2013, by Eaton (R) Corporation. All rights reserved.
#
#	A shell script to send e-mail from IPP - Unix (NUT)
#	Sends an email messages to one user to notify of a power event.
#
# Requires configuration from ipp.conf, or otherwise default to the values below
#
# MC2 Sync:
#	mc2/scripts/lang/eng.lbl (msg)
#	mc2/scripts/libs/labels.js (ldate, nodeName, Default{Subject,Message}

CONFIG=/usr/local/ups/etc/ipp.conf

# Set default values
# Notification configuration
MAIL_NOTIF=1
CONSOLE_NOTIF=1
SYSLOG_NOTIF=1
# Mail configuration (simple (Lansafe) version)
To_User="root"

# Commands
CMD_WALL="wall"
CMD_MAIL="mailx -n"
CMD_SYSLOG="logger"

# Create the message parts
ldate="`date +\"%y-%m-%d - %H:%M:%S\"`"
nodeName="$UPSNAME@`hostname`"
DefaultSubject="Intelligent Power Protector (IPP) Alarms"
DefaultMessage="Alarm from ${nodeName}:\n\n\t${ldate} - $*"

# Include IPP ipp.conf (may overwrite the above default values!)
if [ -f $CONFIG ] ; then
	. $CONFIG
fi

# Console notification
if [ $CONSOLE_NOTIF -eq 1 ];
then
	echo "$DefaultSubject\n\n$DefaultMessage" | $CMD_WALL 
fi

# Mail notification
OTHER_OPT=""

if [ $MAIL_NOTIF -eq 1 ];
then
	echo $To_User | grep "@"
	if [ $? -eq 1 ]
	then
		# Simple (Lansafe / local) version
		env MAILRC=/dev/null echo "$DefaultMessage" | $CMD_MAIL -s $DefaultSubject $To_User
	else
		# GMail need starttls
		echo $SMTP_Server | grep "gmail.com"
		if [ $? -eq 0 ]; then
			OTHER_OPT="-S smtp-use-starttls"
		fi

		env MAILRC=/dev/null echo -e "$DefaultMessage" | $CMD_MAIL -s "$DefaultSubject" \
			-S from=$SMTP_User \
			-S smtp="smtp://$SMTP_Server" \
			-S smtp-auth-user=$SMTP_User \
			-S smtp-auth-password=$SMTP_Password \
			-S smtp-auth=login \
			-S ssl-verify=ignore $OTHER_OPT \
			$To_User
	fi
fi 

# System log notification
if [ $SYSLOG_NOTIF -eq 1 ];
then
	$CMD_SYSLOG -t eaton-ipp "$*"
fi

