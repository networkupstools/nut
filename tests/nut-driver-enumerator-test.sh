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

### Use a standard locale setup so sorting in expected results is not confused
LANG=C
LC_ALL=C
TZ=UTC
export LANG LC_ALL TZ

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
        # Give a nice output to help track the problem:
        ( rm -f "/tmp/.nde.text.expected.$$" "/tmp/.nde.text.actual.$$" \
            && echo "$EXPECT_TEXT" > "/tmp/.nde.text.expected.$$" \
            && echo "$OUT" > "/tmp/.nde.text.actual.$$" \
            && diff -u "/tmp/.nde.text.expected.$$" "/tmp/.nde.text.actual.$$" ) 2>/dev/null || true
        rm -f "/tmp/.nde.text.expected.$$" "/tmp/.nde.text.actual.$$"
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
qx-serial
qx-usb1
qx-usb2
sectionWithComment
sectionWithCommentWhitespace
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
globalflag
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
driverflag
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
port=file#1.dev
[qx-serial]
driver=nutdrv_qx
port=/dev/ttyb
[qx-usb1]
driver=nutdrv_qx
port=auto
[qx-usb2]
driver=nutdrv_qx
port=/dev/usb/8
[sectionWithComment]
driver=nutdrv_qx#comment
port=/dev/usb/8
desc="value with [brackets]"
[brackets with spaces are not sections] # but rather an invalid mess as binary parser may think
[sectionWithCommentWhitespace]
driver=nutdrv_qx	# comment
port=/dev/usb/8 #	comment
commentedDriverFlag # This flag gotta mean something' \
        --show-all-configs
}

testcase_upslist_debug() {
    # We expect a list of names, ports and decided MEDIA type (for dependencies)
    run_testcase "List decided MEDIA and config checksums for all devices" 0 \
"INST: 68b329da9893e34099c7d8ad5cb9c940~[]: DRV='' PORT='' MEDIA='' SECTIONMD5='9a1f372a850f1ee3ab1fc08b185783e0'
INST: 010cf0aed6dd49865bb49b70267946f5~[dummy-proxy]: DRV='dummy-ups  ' PORT='remoteUPS@RemoteHost.local' MEDIA='network' SECTIONMD5='aff543fc07d7fbf83e81001b181c8b97'
INST: 1ea79c6eea3681ba73cc695f3253e605~[dummy-proxy-localhost]: DRV='dummy-ups  ' PORT='localUPS@127.0.0.1' MEDIA='network-localhost' SECTIONMD5='73e6b7e3e3b73558dc15253d8cca51b2'
INST: 76b645e28b0b53122b4428f4ab9eb4b9~[dummy1]: DRV='dummy-ups' PORT='file1.dev' MEDIA='' SECTIONMD5='9e0a326b67e00d455494f8b4258a01f1'
INST: a293d65e62e89d6cc3ac6cb88bc312b8~[epdu-2]: DRV='netxml-ups' PORT='http://172.16.1.2' MEDIA='network' SECTIONMD5='0d9a0147dcf87c7c720e341170f69ed4'
INST: 9a5561464ff8c78dd7cb544740ce2adc~[epdu-2-snmp]: DRV='snmp-ups' PORT='172.16.1.2' MEDIA='network' SECTIONMD5='2631b6c21140cea0dd30bb88b942ce3f'
INST: 16adbdafb22d9fdff1d09038520eb32e~[qx-serial]: DRV='nutdrv_qx' PORT='/dev/ttyb' MEDIA='serial' SECTIONMD5='e3e6e586fbe5b3c0a89432f4b993f4ad'
INST: a21bd2b786228b9619f6adba6db8fa83~[qx-usb1]: DRV='nutdrv_qx' PORT='auto' MEDIA='usb' SECTIONMD5='a6139c5da35bef89dc5b96e2296f5369'
INST: 0066605e07c66043a17eccecbeea1ac5~[qx-usb2]: DRV='nutdrv_qx' PORT='/dev/usb/8' MEDIA='usb' SECTIONMD5='5722dd9c21d07a1f5bcb516dbc458deb'
INST: 1280a731e03116f77290e51dd2a2f37e~[sectionWithComment]: DRV='nutdrv_qx#comment' PORT='/dev/usb/8' MEDIA='' SECTIONMD5='be30e15e17d0579c85eecaf176b4a064'
INST: 770abd5659061a29ed3ae4f7c0b00915~[sectionWithCommentWhitespace]: DRV='nutdrv_qx	# comment' PORT='/dev/usb/8 #	comment' MEDIA='' SECTIONMD5='c757822a331521cdc97310d0241eba28'
INST: efdb1b4698215fdca36b9bc06d24661d~[serial.4]: DRV='serial-ups' PORT='/dev/ttyS1 # some path' MEDIA='' SECTIONMD5='9c485f733aa6d6c85c1724f162929443'
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

    run_testcase "Query a configuration flag (driver)" 0 \
        "driverflag" \
        --show-config-value 'serial.4' driverflag

    run_testcase "Query a missing configuration flag (driver)" 1 \
        "" \
        --show-config-value 'valueHasQuotedHashtag' nosuchflag

    run_testcase "Query multiple configuration keys (originally quoted)" 0 \
        'This is ups-1
file1.dev' \
        --show-device-config-value dummy1 desc port

    run_testcase "Query multiple configuration keys with some missing (originally quoted)" 1 \
        'This is ups-1

file1.dev' \
        --show-device-config-value dummy1 desc unknownkey port
}

testcase_globalSection() {
    run_testcase "Display global config" 0 \
        "maxstartdelay=180
globalflag" \
        --show-config ''

    run_testcase "Query a configuration key (global)" 0 \
        "180" \
        --show-config-value '' maxstartdelay

    run_testcase "Query a configuration flag (global)" 0 \
        "globalflag" \
        --show-config-value '' globalflag

    run_testcase "Query a missing configuration flag (global)" 1 \
        "" \
        --show-config-value '' nosuchflag
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
    case "$USE_SHELL" in
        busybox|busybox_sh) USE_SHELL="busybox sh" ;;
    esac
    testsuite
done
# End of loop over shells

echo "Test suite for nut-driver-enumerator has completed with $FAIL_COUNT failed cases and $GOOD_COUNT good cases" >&2

[ "$FAIL_COUNT" = 0 ] || { echo "As a developer, you may want to export DEBUG=trace or export DEBUG=yes and re-run the test; also make sure you meant the nut-driver-enumerator.sh implementation as NDE='$NDE'" >&2 ; exit 1; }
