#THIS IS WORK IN PROGRESS, THIS IS NOT FUNCTIONAL
exit

#!/bin/sh
#	ipp-event
#
#	Copyright (c) 2013-2017, by Eaton (R) Corporation. All rights reserved.
#
#	A shell script to manage event from IPP - Unix (NUT)
#	It needs ipp.conf file.
#

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

# Note: $NUT_DIR/xbin holds the wrappers to run NUT binaries with co-bundled
# third party libs and hopefully without conflicts induced for the OS binaries
PATH="$NUT_DIR/xbin:$NUT_DIR/sbin:$NUT_DIR/bin:$PATH"
export PATH

# Search for binaries under current PATH normally, no hardcoding. Scripts:
CMD_NOTIFIER="ipp-notifier.sh"
DAEMON="ipp-shutdown-daemon.sh"
#CMD_NOTIFIER="$NUT_DIR/bin/ipp-notifier.sh"
#DAEMON="$NUT_DIR/bin/ipp-shutdown-daemon.sh"

if [ -f "$CONFIG_IPP" ] ; then
	. "$CONFIG_IPP"
fi

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
