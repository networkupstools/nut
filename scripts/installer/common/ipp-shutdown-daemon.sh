#THIS IS WORK IN PROGRESS, THIS IS NOT FUNCTIONAL.
exit

#!/bin/sh
#       ipp-shutdown-daemon
#
#       Copyright (c) 2013-2017, by Eaton (R) Corporation. All rights reserved.
#
#       A shell script to detect shutdown from NMC cards IPP - Unix (NUT)
#       It needs ipp.conf file.
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

# Search for binaries under current PATH normally, no hardcoding
NUT_UPSC="upsc"
#NUT_UPSC="$NUT_DIR/xbin/upsc"

if [ -f "$CONFIG_IPP" ] ; then
	. "$CONFIG_IPP"
fi

# Convert to parsing of "ipp-status -p" which reports all needed details
for dev in `$NUT_UPSC -l 2>/dev/null`; do
	shutdown="`$NUT_UPSC "$dev"@localhost ups.timer.shutdown 2>/dev/null`"
	reboot="`$NUT_UPSC "$dev"@localhost ups.timer.reboot 2>/dev/null`"
done
