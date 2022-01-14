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

CONFIG=/usr/local/ups/etc/ipp.conf
UPSC="/usr/local/ups/bin/upsc"

. $CONFIG

for dev in `$UPSC -l 2>/dev/null`; do
        shutdown=`$UPSC $dev@localhost ups.timer.shutdown 2>/dev/null`
        reboot=`$UPSC $dev@localhost ups.timer.reboot 2>/dev/null`
done    

