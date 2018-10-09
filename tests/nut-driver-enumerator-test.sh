#!/bin/sh

# Copyright (C) 2018 Eaton
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
#! \file    nut-driver-enumerator-test.sh
#  \author  Jim Klimov <EvgenyKlimov@eaton.com>
#  \brief   Self-test for nut-driver-enumerator.sh utility
#  \details Automated sanity test for nut-driver-enumerator.sh(.in)
#           using different shells (per $USE_SHELLS) and CLI requests
#           for regression and compatibility tests as well as for TDD
#           fueled by pre-decided expected outcomes.

[ -n "${BUILDDIR-}" ] || BUILDDIR="`dirname $0`"
[ -n "${SRCDIR-}" ] || SRCDIR="`dirname $0`"
[ -n "${USE_SHELLS-}" ] || USE_SHELLS="/bin/sh"
case "${DEBUG-}" in
    [Yy]|[Yy][Ee][Ss]) DEBUG=yes ;;
    *) DEBUG="" ;;
esac

SYSTEMD_CONFPATH="${BUILDDIR}/selftest-rw/systemd-units"
export SYSTEMD_CONFPATH

NUT_CONFPATH="${BUILDDIR}/selftest-rw/nut"
export NUT_CONFPATH

[ -n "${UPSCONF-}" ] || UPSCONF="${SRCDIR}/nut-driver-enumerator-test--ups.conf"
export UPSCONF

[ -n "${NDE-}" ] || NDE="${SRCDIR}/../scripts/upsdrvsvcctl/nut-driver-enumerator.sh.in"

# TODO : Add tests that generate configuration files for units
#mkdir -p "${NUT_CONFPATH}" "${SYSTEMD_CONFPATH}" || exit

FAIL_COUNT=0
GOOD_COUNT=0
callNDE() {
    if [ "$DEBUG" = yes ]; then
        time $USE_SHELL $NDE "$@"
    else
        $USE_SHELL $NDE "$@" 2>/dev/null
    fi
}

run_testcase() {
    # First 3 args are required as defined below; the rest are
    # CLI arg(s) to nut-driver-enumerator.sh
    CASE_DESCR="$1"
    EXPECT_CODE="$2"
    EXPECT_TEXT="$3"
    shift 3

    printf "Testing : SHELL='%s'\tCASE='%s'\t" "$USE_SHELL" "$CASE_DESCR"
    OUT="`callNDE "$@"`" ; RESCODE=$?
    printf "Got : RESCODE='%s'\t" "$RESCODE"

    RES=0
    if [ "$RESCODE" = "$EXPECT_CODE" ]; then
        printf "STATUS_CODE='MATCHED'\t"
        GOOD_COUNT="`expr $GOOD_COUNT + 1`"
    else
        printf "STATUS_CODE='MISMATCH' expect_code=%s received_code=%s\t" "$EXPECT_CODE" "$RESCODE" >&2
        FAIL_COUNT="`expr $FAIL_COUNT + 1`"
        RES="`expr $RES + 1`"
    fi

    if [ "$OUT" = "$EXPECT_TEXT" ]; then
        printf "STATUS_TEXT='MATCHED'\n"
        GOOD_COUNT="`expr $GOOD_COUNT + 1`"
    else
        printf "STATUS_TEXT='MISMATCH'\n"
        printf '\t--- expected ---\n%s\n\t--- received ---\n%s\n\t--- MISMATCH ABOVE\n\n' "$EXPECT_TEXT" "$OUT" >&2
        FAIL_COUNT="`expr $FAIL_COUNT + 1`"
        RES="`expr $RES + 2`"
    fi
    if [ "$RES" != 0 ] || [ "$DEBUG" = yes ] ; then echo "" ; fi
    return $RES
}

##################################################################
# Note: expectations in test cases below are tightly connected   #
# to both the current code in the script and content of the test #
# configuration file.                                            #
##################################################################

testcase_bogus_args() {
    run_testcase "Reject unknown args" 1 "" \
        --some-bogus-arg
}

testcase_list_all_devices() {
    # We expect a list of unbracketed names from the device sections
    # Note: unlike other outputs, this list is alphabetically sorted
    run_testcase "List all device names from sections" 0 \
"dummy-proxy
dummy1
epdu-2
epdu-2-snmp
serial.4
usb_3
valueHasEquals
valueHasHashtag
valueHasQuotedHashtag" \
        --list-devices
}

testcase_show_all_configs() {
    # We expect whitespace trimmed, comment-only lines removed
    run_testcase "Show all configs" 0 \
'[dummy1]
driver=dummy-ups
port=file1.dev
desc="This is ups-1"
[epdu-2]
driver=netxml-ups
port=http://172.16.1.2
synchronous=yes
[epdu-2-snmp]
driver=snmp-ups
port=172.16.1.2
synchronous=no
[usb_3]
driver=usbhid-ups
port=auto
[serial.4]
driver=serial-ups
port=/dev/ttyS1 # some path
[dummy-proxy]
driver=dummy-ups
port=remoteUPS@RemoteHost.local
[valueHasEquals]
driver=dummy=ups
port=file1.dev # key=val, right?
[valueHasHashtag]
driver=dummy-ups
port=file#1.dev
[valueHasQuotedHashtag]
driver=dummy-ups
port=file#1.dev' \
        --show-all-configs
}

testcase_upslist_debug() {
    # We expect a list of names, ports and decided MEDIA type (for dependencies)
    run_testcase "List decided MEDIA for all devices" 0 \
"INST: [dummy-proxy]: DRV='dummy-ups' PORT='remoteUPS@RemoteHost.local' MEDIA='network'
INST: [dummy1]: DRV='dummy-ups' PORT='file1.dev' MEDIA=''
INST: [epdu-2]: DRV='netxml-ups' PORT='http://172.16.1.2' MEDIA='network'
INST: [epdu-2-snmp]: DRV='snmp-ups' PORT='172.16.1.2' MEDIA='network'
INST: [serial.4]: DRV='serial-ups' PORT='/dev/ttyS1 # some path' MEDIA=''
INST: [usb_3]: DRV='usbhid-ups' PORT='auto' MEDIA='usb'
INST: [valueHasEquals]: DRV='dummy=ups' PORT='file1.dev # key=val, right?' MEDIA=''
INST: [valueHasHashtag]: DRV='dummy-ups' PORT='file#1.dev' MEDIA=''
INST: [valueHasQuotedHashtag]: DRV='dummy-ups' PORT='file#1.dev' MEDIA=''" \
        upslist_debug
}

testcase_getValue() {
    run_testcase "Query a configuration key (SDP)" 0 \
        "file1.dev" \
        --show-device-config-value dummy1 port

    run_testcase "Query a configuration key (other)" 0 \
        "yes" \
        --show-device-config-value epdu-2 synchronous
}

# Combine the cases above into a stack
testsuite() {
    testcase_bogus_args
    testcase_list_all_devices
    testcase_show_all_configs
    testcase_upslist_debug
    testcase_getValue
}

# If no args...
for USE_SHELL in $USE_SHELLS ; do
    testsuite
done
# End of loop over shells

echo "Test suite for nut-driver-enumerator has completed with $FAIL_COUNT failed cases and $GOOD_COUNT good cases" >&2

[ "$FAIL_COUNT" = 0 ] || exit 1
