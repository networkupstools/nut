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
#           using different shells (per $SHELL_PROGS) and CLI requests
#           for regression and compatibility tests as well as for TDD
#           fueled by pre-decided expected outcomes.

### Note: These are relative to where the selftest script lives,
### not the NUT top_srcdir etc. They can be exported by a Makefile.
[ -n "${BUILDDIR-}" ] || BUILDDIR="`dirname $0`"
[ -n "${SRCDIR-}" ] || SRCDIR="`dirname $0`"
[ -n "${SHELL_PROGS-}" ] || SHELL_PROGS="/bin/sh"
case "${DEBUG-}" in
    [Yy]|[Yy][Ee][Ss]) DEBUG=yes ;;
    [Tt][Rr][Aa][Cc][Ee]) DEBUG=trace ;;
    *) DEBUG="" ;;
esac

SYSTEMD_CONFPATH="${BUILDDIR}/selftest-rw/systemd-units"
export SYSTEMD_CONFPATH

NUT_CONFPATH="${BUILDDIR}/selftest-rw/nut"
export NUT_CONFPATH

[ -n "${UPSCONF-}" ] || UPSCONF="${SRCDIR}/nut-driver-enumerator-test--ups.conf"
[ ! -s "${UPSCONF-}" ] && echo "FATAL : testing ups.conf not found as '$UPSCONF'" >&2 && exit 1
export UPSCONF

if [ ! -n "${NDE-}" ] ; then
    for NDE in \
        "${BUILDDIR}/../scripts/upsdrvsvcctl/nut-driver-enumerator.sh" \
        "${SRCDIR}/../scripts/upsdrvsvcctl/nut-driver-enumerator.sh.in" \
    ; do [ -s "$NDE" ] && break ; done
fi
[ ! -s "${NDE-}" ] && echo "FATAL : testing nut-driver-enumerator.sh implementation not found as '$NDE'" >&2 && exit 1

# TODO : Add tests that generate configuration files for units
#mkdir -p "${NUT_CONFPATH}" "${SYSTEMD_CONFPATH}" || exit

FAIL_COUNT=0
GOOD_COUNT=0
callNDE() {
    case "$DEBUG" in
        yes)   time $USE_SHELL $NDE "$@" ;;
        trace) time $USE_SHELL -x $NDE "$@" ;;
        *)     $USE_SHELL $NDE "$@" 2>/dev/null ;;
    esac
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
    if [ "$RES" != 0 ] || [ -n "$DEBUG" ] ; then echo "" ; fi
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
dummy-proxy-localhost
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
'maxstartdelay=180
[dummy1]
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
driver="dummy-ups  "
port=remoteUPS@RemoteHost.local
[dummy-proxy-localhost]
driver='"'dummy-ups  '"'
port=localUPS@127.0.0.1
[valueHasEquals]
driver=dummy=ups
port=file1.dev # key = val, right?
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
    run_testcase "List decided MEDIA and config checksums for all devices" 0 \
"INST: 010cf0aed6dd49865bb49b70267946f5~[dummy-proxy]: DRV='dummy-ups  ' PORT='remoteUPS@RemoteHost.local' MEDIA='network' SECTIONMD5='aff543fc07d7fbf83e81001b181c8b97'
INST: 1ea79c6eea3681ba73cc695f3253e605~[dummy-proxy-localhost]: DRV='dummy-ups  ' PORT='localUPS@127.0.0.1' MEDIA='network-localhost' SECTIONMD5='73e6b7e3e3b73558dc15253d8cca51b2'
INST: 76b645e28b0b53122b4428f4ab9eb4b9~[dummy1]: DRV='dummy-ups' PORT='file1.dev' MEDIA='' SECTIONMD5='9e0a326b67e00d455494f8b4258a01f1'
INST: a293d65e62e89d6cc3ac6cb88bc312b8~[epdu-2]: DRV='netxml-ups' PORT='http://172.16.1.2' MEDIA='network' SECTIONMD5='0d9a0147dcf87c7c720e341170f69ed4'
INST: 9a5561464ff8c78dd7cb544740ce2adc~[epdu-2-snmp]: DRV='snmp-ups' PORT='172.16.1.2' MEDIA='network' SECTIONMD5='2631b6c21140cea0dd30bb88b942ce3f'
INST: efdb1b4698215fdca36b9bc06d24661d~[serial.4]: DRV='serial-ups' PORT='/dev/ttyS1 # some path' MEDIA='' SECTIONMD5='b9433819b80ffa3f723ca9109fa82276'
INST: f4a1c33db201c2ca897a3337993c10fc~[usb_3]: DRV='usbhid-ups' PORT='auto' MEDIA='usb' SECTIONMD5='1f6a24becde9bd31c9852610658ef84a'
INST: 8e5686f92a5ba11901996c813e7bb23d~[valueHasEquals]: DRV='dummy=ups' PORT='file1.dev # key = val, right?' MEDIA='' SECTIONMD5='2f04d65da53e3b13771bb65422f0f4c0'
INST: 99da99b1e301e84f34f349443aac545b~[valueHasHashtag]: DRV='dummy-ups' PORT='file#1.dev' MEDIA='' SECTIONMD5='6029bda216de0cf1e81bd55ebd4a0fff'
INST: d50c3281f9b68a94bf9df72a115fbb5c~[valueHasQuotedHashtag]: DRV='dummy-ups' PORT='file#1.dev' MEDIA='' SECTIONMD5='af59c3c0caaa68dcd796d7145ae403ee'" \
        upslist_debug

    # FIXME : in [valueHasEquals] and [serial.4] the PORT value is quite bogus
    # with its embedded comments. Check vs. binary config parser, whether in
    # unquoted case only first token is the valid value, and how comments are
    # handled in general?
    # FIXME : in [valueHasHashtag] the line after "#" should likely be dropped
    # (check in binary config parser first) while in [valueHasQuotedHashtag]
    # it should stay.
}

testcase_getValue() {
    run_testcase "Query a configuration key (SDP)" 0 \
        "file1.dev" \
        --show-device-config-value dummy1 port

    run_testcase "Query a configuration key (other)" 0 \
        "yes" \
        --show-device-config-value epdu-2 synchronous

    run_testcase "Query a configuration key (originally quoted)" 0 \
        'This is ups-1' \
        --show-device-config-value dummy1 desc
}

testcase_globalSection() {
    run_testcase "Display global config" 0 \
        "maxstartdelay=180" \
        --show-config ''

    run_testcase "Query a configuration key (global)" 0 \
        "180" \
        --show-config-value '' maxstartdelay
}


# Combine the cases above into a stack
testsuite() {
    testcase_bogus_args
    testcase_list_all_devices
    testcase_show_all_configs
    testcase_getValue
    testcase_globalSection
    # This one can take a while, put it last
    testcase_upslist_debug
}

# If no args...
for USE_SHELL in $SHELL_PROGS ; do
    testsuite
done
# End of loop over shells

echo "Test suite for nut-driver-enumerator has completed with $FAIL_COUNT failed cases and $GOOD_COUNT good cases" >&2

[ "$FAIL_COUNT" = 0 ] || exit 1
