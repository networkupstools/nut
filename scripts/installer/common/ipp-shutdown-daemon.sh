#THIS IS WORK IN PROGRESS, THIS IS NOT FUNCTIONAL.
exit

#!/bin/sh
#       ipp-shutdown-daemon
#
#       Copyright (c) 2013, by Eaton (R) Corporation. All rights reserved.
#
#       A shell script to detect shutdown from NMC cards IPP - Unix (NUT)
#       It needs ipp.conf file.
#

NUT_DIR="/usr/local/ups"
CONFIG="$NUT_DIR/etc/ipp.conf"
NUT_UPSC="$NUT_DIR/bin/upsc"

. "$CONFIG"

# Convert to parsing of "ipp-status -p" which reports all needed details
for dev in `$NUT_UPSC -l 2>/dev/null`; do
        shutdown="`$NUT_UPSC "$dev"@localhost ups.timer.shutdown 2>/dev/null`"
        reboot="`$NUT_UPSC "$dev"@localhost ups.timer.reboot 2>/dev/null`"
done

