#!/bin/sh

# Copyright (C) 2020,2022 by Jim Klimov <jimklimov+nut@gmail.com>
# Licensed according to GPLv2+ for the NUT project

# If your USB connection on Solaris/illumos platform gets lost regularly
# and might benefit from a harsh reconnection, consider customizing this
# script (or its envvar args) to your deployment and adding to crontab:
#   0,5,10,15,20,25,30,35,40,45,50,55 * * * * MODE=optional /etc/nut/reset-ups-usb-solaris.sh

# Comment this away in your deployment after customizing defaults;
# see NUT source docs/solaris-usb.txt for details:
echo "WARNING: Script $0 was not yet tailored to this deployment!" >&2 ; exit

# TODO: Parse CLI args?
[ -n "$MODE" ] || MODE='always'
# Defaults below come from documentation example:
[ -n "$DEVICE" ] || DEVICE='innotech'
[ -n "$CFGADM_APID" ] || CFGADM_APID="usb10/1"
# Can specify '-' to not reload OS driver:
[ -n "$UGEN_DRV_ID" ] || UGEN_DRV_ID='"usb665,5161.2"'

if [ "$MODE" = optional ]; then
    if upsc "$DEVICE" 2>&1 | grep -i 'Data stale' ; then : ; else exit 0 ; fi
fi

# Sanity-checks
command -v svcs && command -v svcadm || { echo "ERROR: This system does not have SMF tools?" >&2 ; exit 1; }
command -v cfgadm || { echo "ERROR: This system does not have cfgadm?" >&2 ; exit 1; }
# upsc and upsdrvctl below are rather informational
command -v upsc && command -v upsdrvctl || echo "WARNING: This system does not have NUT tools?" >&2

date
svcs -p "$DEVICE" ; upsc "$DEVICE"

DO_SVC=false
if [ "`svcs -Hostate "$DEVICE"`" = "online" ]; then
    DO_SVC=true
    svcadm disable -ts "$DEVICE"
fi
upsdrvctl stop "$DEVICE" || true

echo "Soft-resetting connection of '${CFGADM_APID}':"
cfgadm -lv "${CFGADM_APID}"

cfgadm -c disconnect -y "${CFGADM_APID}"
if [ "$UGEN_DRV_ID" != '-' ] ; then
    rem_drv ugen ; sleep 3
fi
cfgadm -c configure -y "${CFGADM_APID}"; sleep 3
if [ "$UGEN_DRV_ID" != '-' ] ; then
    add_drv -i "$UGEN_DRV_ID" -m '* 0666 root sys' ugen
fi

sleep 3
cfgadm -lv "${CFGADM_APID}"

if $DO_SVC ; then svcadm enable "$DEVICE" ; fi
svcadm clear "$DEVICE" 2>/dev/null

dmesg | tail -n 20
date
svcs -p "$DEVICE" ; upsc "$DEVICE" || { \
    COUNT=60
    while [ "$COUNT" -gt 0 ] ; do
        COUNT="`expr $COUNT - 1`"
        if upsc "$DEVICE" 2>&1 | grep -Ei '^ups\.status:' >/dev/null ; then break ; fi
        sleep 1
    done
    svcs -p "$DEVICE" ; upsc "$DEVICE"
}
