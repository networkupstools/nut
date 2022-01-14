#THIS IS WORK IN PROGRESS, THIS IS NOT FUNCTIONAL
exit

#!/bin/sh
#	ipp-event
#
#	Copyright (c) 2013, by Eaton (R) Corporation. All rights reserved.
#
#	A shell script to manage event from IPP - Unix (NUT)
#	It needs ipp.conf file.
#

NUT_DIR="/usr/local/ups"
CMD_NOTIFIER="$NUT_DIR/bin/ipp-notifier.sh"
DAEMON="$NUT_DIR/bin/ipp-shutdown-daemon.sh"

#call notifier script
$CMD_NOTIFIER "$*" &

PROC="`ps -ef | grep "$DAEMON" | awk   -F" " '{print $2}'`"
case "$1" in
	ONBATT)
		if [ $PROC = "" ];then
			$DAEMON &
		fi
	;;
	ONLINE)
		kill -9 $PROC
	;;
esac
