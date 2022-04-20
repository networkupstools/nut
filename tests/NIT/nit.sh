#!/bin/sh

# NUT Integration Testing suite, assumes the codebase was built and
# arranges running of the binaries to test driver-client-server
# ability to start and their interactions.
#
# Note: currently it is a PoC-quality mess that gets the job done
# but could be refactored for better maintainability and generic
# approach. Part of the goal was to let this script set up the
# sandbox to run tests which could be defined in other files.
#
# WARNING: Current working directory when starting the script should be
# the location where it may create temporary data (e.g. the BUILDDIR).
# Caller can export envvars to impact the script behavior, e.g.:
#	DEBUG=true	to print debug messages, running processes, etc.
#	DEBUG_SLEEP=60	to sleep after tests, with driver+server running
#	NUT_PORT=12345	custom port for upsd to listen and clients to query
#
# Design note: written with dumbed-down POSIX shell syntax, to
# properly work in whatever different OSes have (bash, dash,
# ksh, busybox sh...)
#
# Copyright
#	2022 Jim Klimov <jimklimov+nut@gmail.com>
#
# License: GPLv2+

TZ=UTC
LANG=C
LC_ALL=C
export TZ LANG LC_ALL

log_separator() {
    echo "" >&2
    echo "================================" >&2
}

shouldDebug() {
    [ -n "$DEBUG" ] || [ -n "$DEBUG_SLEEP" ]
}

log_debug() {
    if shouldDebug ; then
        echo "[DEBUG] $@" >&2
    fi
}

log_info() {
    echo "[INFO] $@" >&2
}

log_error() {
    echo "[ERROR] $@" >&2
}

die() {
    echo "[FATAL] $@" >&2
    exit 1
}

# Note: current directory is assumed to be writeable for temporary
# data, e.g. the $(builddir) from the Makefile. Static resources
# from the source codebase are where the script resides, e.g.
# the $(srcdir) from the Makefile. If we are not in the source
# tree, tests would use binaries in PATH (e.g. packaged install).
BUILDDIR="`pwd`"
TOP_BUILDDIR=""
case "${BUILDDIR}" in
    */tests/NIT)
        TOP_BUILDDIR="`cd "${BUILDDIR}"/../.. && pwd`" ;;
    *) log_info "Current directory is not a .../tests/NIT" ;;
esac
if ! test -w "${BUILDDIR}" ; then
    log_error "BUILDDIR='${BUILDDIR}' is not writeable, tests may fail below"
fi

SRCDIR="`dirname "$0"`"
SRCDIR="`cd "$SRCDIR" && pwd`"
TOP_SRCDIR=""
case "${SRCDIR}" in
    */tests/NIT)
        TOP_SRCDIR="`cd "${SRCDIR}"/../.. && pwd`" ;;
esac

# No fuss about LD_LIBRARY_PATH: for binaries that need it,
# PATH entries below would contain libtool wrapper scripts;
# for other builds we use system default or caller's env.
PATH_ADD="${BUILDDIR}"
if [ x"${SRCDIR}" != x"${BUILDDIR}" ]; then
    PATH_ADD="${PATH_ADD}:${SRCDIR}"
fi

if [ x"${TOP_BUILDDIR}" != x ]; then
    PATH_ADD="${PATH_ADD}:${TOP_BUILDDIR}/clients:${TOP_BUILDDIR}/drivers:${TOP_BUILDDIR}/server:${TOP_BUILDDIR}/tools:${TOP_BUILDDIR}/tools/nut-scanner"
fi

if [ x"${TOP_SRCDIR}" != x ]; then
    PATH_ADD="${PATH_ADD}:${TOP_SRCDIR}/clients:${TOP_SRCDIR}/drivers:${TOP_SRCDIR}/server:${TOP_SRCDIR}/tools:${TOP_SRCDIR}/tools/nut-scanner"
fi

PATH="${PATH_ADD}:${PATH}"
export PATH
unset PATH_ADD

log_debug "Using PATH='$PATH'"

for PROG in upsd upsc dummy-ups upsmon ; do
    (command -v ${PROG}) || die "Useless setup: ${PROG} not found in PATH"
done

PID_UPSD=""
PID_DUMMYUPS=""
PID_DUMMYUPS1=""
PID_DUMMYUPS2=""

rm -rf "$BUILDDIR/tmp" || true

mkdir -p "$BUILDDIR/tmp/etc" "$BUILDDIR/tmp/run" && chmod 750 "$BUILDDIR/tmp/run" \
|| die "Failed to create temporary FS structure for the NIT"

trap 'RES=$?; if [ -n "$PID_UPSD$PID_DUMMYUPS$PID_DUMMYUPS1$PID_DUMMYUPS2" ] ; then kill -15 $PID_UPSD $PID_DUMMYUPS $PID_DUMMYUPS1 $PID_DUMMYUPS2 ; fi; exit $RES;' 0 1 2 3 15

NUT_STATEPATH="$BUILDDIR/tmp/run"
NUT_ALTPIDPATH="$BUILDDIR/tmp/run"
NUT_CONFPATH="$BUILDDIR/tmp/etc"
export NUT_STATEPATH NUT_ALTPIDPATH NUT_CONFPATH

# TODO: Find a portable way to (check and) grab a random unprivileged port?
[ -n "${NUT_PORT-}" ] && [ "$NUT_PORT" -gt 0 ] && [ "$NUT_PORT" -lt 65536 ] \
|| {
    DELTA1="`date +%S`" || DELTA1=0
    DELTA2="`expr $$ % 99`" || DELTA2=0

    NUT_PORT="`expr 34931 + $DELTA1 + $DELTA2`" \
    && [ "$NUT_PORT" -gt 0 ] && [ "$NUT_PORT" -lt 65536 ] \
    || NUT_PORT=34931
}

### upsd.conf: ##################################################

generatecfg_upsd_trivial() {
    # Populate the configs for the run
    cat > "$NUT_CONFPATH/upsd.conf" << EOF
STATEPATH "$NUT_STATEPATH"
LISTEN localhost $NUT_PORT
EOF
    [ $? = 0 ] || die "Failed to populate temporary FS structure for the NIT: upsd.conf"
    chmod 640 "$NUT_CONFPATH/upsd.conf"
}

generatecfg_upsd_nodev() {
    generatecfg_upsd_trivial
    echo "ALLOW_NO_DEVICE true" >> "$NUT_CONFPATH/upsd.conf" \
    || die "Failed to populate temporary FS structure for the NIT: upsd.conf"
}

### upsd.users: ##################################################

generatecfg_upsdusers_trivial() {
    cat > "$NUT_CONFPATH/upsd.users" << EOF
[admin]
    password = mypass
    actions = SET
    instcmds = ALL

[tester]
    password = "pass words"
    instcmds = test.battery.start
    instcmds = test.battery.stop

[dummy-admin-m]
    password = 'P@ssW0rdAdm'
    upsmon master

[dummy-admin]
    password = 'P@ssW0rdAdm'
    upsmon primary

[dummy-user-s]
    password = 'P@ssW0rd'
    upsmon slave

[dummy-user]
    password = 'P@ssW0rd'
    upsmon secondary
EOF
    [ $? = 0 ] || die "Failed to populate temporary FS structure for the NIT: upsd.users"
    chmod 640 "$NUT_CONFPATH/upsd.users"
}

### upsmon.conf: ##################################################

generatecfg_upsmon_trivial() {
    # Populate the configs for the run
    (  echo 'MINSUPPLIES 0' > "$NUT_CONFPATH/upsmon.conf" || exit
       echo 'SHUTDOWNCMD "echo TESTING_DUMMY_SHUTDOWN_NOW"' >> "$NUT_CONFPATH/upsmon.conf" || exit
    ) || die "Failed to populate temporary FS structure for the NIT: upsmon.conf"
    chmod 640 "$NUT_CONFPATH/upsmon.conf"
}

generatecfg_upsmon_master() {
    generatecfg_upsmon_trivial
    echo "MONITOR 'dummy@localhost:$NUT_PORT' 0 'dummy-admin-m' 'P@ssW0rdAdm' master" >> "$NUT_CONFPATH/upsmon.conf" \
    || die "Failed to populate temporary FS structure for the NIT: upsmon.conf"
}

generatecfg_upsmon_primary() {
    generatecfg_upsmon_trivial
    echo "MONITOR 'dummy@localhost:$NUT_PORT' 0 'dummy-admin' 'P@ssW0rdAdm' primary" >> "$NUT_CONFPATH/upsmon.conf" \
    || die "Failed to populate temporary FS structure for the NIT: upsmon.conf"
}

generatecfg_upsmon_slave() {
    generatecfg_upsmon_trivial
    echo "MONITOR 'dummy@localhost:$NUT_PORT' 0 'dummy-user-s' 'P@ssW0rd' slave" >> "$NUT_CONFPATH/upsmon.conf" \
    || die "Failed to populate temporary FS structure for the NIT: upsmon.conf"
}

generatecfg_upsmon_secondary() {
    generatecfg_upsmon_trivial
    echo "MONITOR 'dummy@localhost:$NUT_PORT' 0 'dummy-user' 'P@ssW0rd' secondary" >> "$NUT_CONFPATH/upsmon.conf" \
    || die "Failed to populate temporary FS structure for the NIT: upsmon.conf"
}

### ups.conf: ##################################################

generatecfg_ups_trivial() {
    # Populate the configs for the run
    (   echo 'maxretry = 3' > "$NUT_CONFPATH/ups.conf" || exit
        if [ x"${TOP_BUILDDIR}" != x ]; then
            echo "driverpath = '${TOP_BUILDDIR}/drivers'" >> "$NUT_CONFPATH/ups.conf" || exit
        fi
    ) || die "Failed to populate temporary FS structure for the NIT: ups.conf"
    chmod 640 "$NUT_CONFPATH/ups.conf"
}

generatecfg_ups_dummy() {
    generatecfg_ups_trivial

    cat > "$NUT_CONFPATH/dummy.dev" << EOF
ups.status: OB
TIMER 5
ups.status: OL
TIMER 5
EOF
    [ $? = 0 ] || die "Failed to populate temporary FS structure for the NIT: dummy.dev"

    cat >> "$NUT_CONFPATH/ups.conf" << EOF
[dummy]
    driver = dummy-ups
    desc = "Crash Dummy"
    port = dummy.dev
EOF
    [ $? = 0 ] || die "Failed to populate temporary FS structure for the NIT: ups.conf"

    if [ x"${TOP_SRCDIR}" != x ]; then
        cp "${TOP_SRCDIR}/data/evolution500.seq" "${TOP_SRCDIR}/data/epdu-managed.dev" "$NUT_CONFPATH/"

        cat >> "$NUT_CONFPATH/ups.conf" << EOF
[UPS1]
    driver = dummy-ups
    desc = "Example event sequence"
    port = evolution500.seq

[UPS2]
    driver = dummy-ups
    desc = "Example ePDU data dump"
    port = epdu-managed.dev
EOF
        [ $? = 0 ] || die "Failed to populate temporary FS structure for the NIT: ups.conf"
    fi

}

#####################################################

FAILED=0
PASSED=0

log_separator
log_info "Test UPSD without configs at all"
upsd -F
if [ "$?" = 0 ]; then
    log_error "upsd should fail without configs"
    FAILED="`expr $FAILED + 1`"
else
    log_info "OK, upsd failed to start in wrong conditions"
    PASSED="`expr $PASSED + 1`"
fi

log_separator
log_info "Test UPSD without driver config file"
generatecfg_upsd_trivial
upsd -F
if [ "$?" = 0 ]; then
    log_error "upsd should fail without driver config file"
    FAILED="`expr $FAILED + 1`"
else
    log_info "OK, upsd failed to start in wrong conditions"
    PASSED="`expr $PASSED + 1`"
fi

log_separator
log_info "Test UPSD without drivers defined in config file"
generatecfg_upsd_trivial
generatecfg_ups_trivial
upsd -F
if [ "$?" = 0 ]; then
    log_error "upsd should fail without drivers defined in config file"
    FAILED="`expr $FAILED + 1`"
else
    log_info "OK, upsd failed to start in wrong conditions"
    PASSED="`expr $PASSED + 1`"
fi

log_separator
log_info "Test UPSD allowed to run without driver configs"
generatecfg_upsd_nodev
generatecfg_upsdusers_trivial
generatecfg_ups_trivial
upsd -F &
PID_UPSD="$!"
sleep 2
if [ -d "/proc/$PID_UPSD" ] || kill -0 "$PID_UPSD"; then
    log_info "OK, upsd is running"
    PASSED="`expr $PASSED + 1`"

    log_separator
    log_info "Test that UPSD responds to UPSC"
    OUT="`upsc -l localhost:$NUT_PORT`" || die "upsd does not respond: $OUT"
    if [ -n "$OUT" ] ; then
        log_error "got reply for upsc listing when none was expected: $OUT"
        FAILED="`expr $FAILED + 1`"
    else
        log_info "OK, empty response as expected"
        PASSED="`expr $PASSED + 1`"
    fi
else
    log_error "upsd was expected to be running although no devices are defined"
    FAILED="`expr $FAILED + 1`"
fi
kill -15 $PID_UPSD
wait $PID_UPSD

log_separator
log_info "Test starting UPSD and a driver"
generatecfg_upsd_nodev
generatecfg_upsdusers_trivial
generatecfg_ups_dummy

log_info "Starting UPSD alone first"
upsd -F &
PID_UPSD="$!"
sleep 5

EXPECTED_UPSLIST='dummy'
if [ x"${TOP_SRCDIR}" != x ]; then
    EXPECTED_UPSLIST="$EXPECTED_UPSLIST
UPS1
UPS2"
fi

log_info "Query listing from UPSD by UPSC (driver not running yet)"
OUT="`upsc -l localhost:$NUT_PORT`" || die "upsd does not respond: $OUT"
if [ x"$OUT" != x"$EXPECTED_UPSLIST" ] ; then
    log_error "got this reply for upsc listing when '$EXPECTED_UPSLIST' was expected: $OUT"
    FAILED="`expr $FAILED + 1`"
else
    PASSED="`expr $PASSED + 1`"
fi

log_info "Query driver state from UPSD by UPSC (driver not running yet)"
OUT="`upsc dummy@localhost:$NUT_PORT 2>&1`" && {
    log_error "upsc was supposed to answer with error exit code: $OUT"
    FAILED="`expr $FAILED + 1`"
}
if [ x"$OUT" != x'Error: Driver not connected' ] ; then
    log_error "got reply for upsc query when 'Error: Driver not connected' was expected: $OUT"
    FAILED="`expr $FAILED + 1`"
else
    PASSED="`expr $PASSED + 1`"
fi

log_info "Starting dummy-ups driver(s) now"
#upsdrvctl -F start dummy &
dummy-ups -a dummy -F &
PID_DUMMYUPS="$!"

if [ x"${TOP_SRCDIR}" != x ]; then
    dummy-ups -a UPS1 -F &
    PID_DUMMYUPS1="$!"

    dummy-ups -a UPS2 -F &
    PID_DUMMYUPS2="$!"
fi

sleep 5

if shouldDebug ; then
    (ps -ef || ps -xawwu) 2>/dev/null | grep -E '(ups|nut|dummy)' || true
fi

log_info "Query driver state from UPSD by UPSC after driver startup"
upsc dummy@localhost:$NUT_PORT || die "upsd does not respond"

OUT="`upsc dummy@localhost:$NUT_PORT device.model`" || die "upsd does not respond: $OUT"
if [ x"$OUT" != x"Dummy UPS" ] ; then
    log_error "got this reply for upsc query when 'Dummy UPS' was expected: $OUT"
    FAILED="`expr $FAILED + 1`"
else
    PASSED="`expr $PASSED + 1`"
fi

log_info "Query driver state from UPSD by UPSC for bogus info"
OUT="`upsc dummy@localhost:$NUT_PORT ups.bogus.value 2>&1`" && {
    log_error "upsc was supposed to answer with error exit code: $OUT"
    FAILED="`expr $FAILED + 1`"
}
if [ x"$OUT" != x'Error: Variable not supported by UPS' ] ; then
    log_error "got reply for upsc query when 'Error: Variable not supported by UPS' was expected: $OUT"
    FAILED="`expr $FAILED + 1`"
else
    PASSED="`expr $PASSED + 1`"
fi

log_separator
log_info "Test that dummy-ups TIMER action changes the reported state"
# Driver is set up to flip ups.status every 5 sec, so check every 3
OUT1="`upsc dummy@localhost:$NUT_PORT ups.status`" || die "upsd does not respond: $OUT1" ; sleep 3
OUT2="`upsc dummy@localhost:$NUT_PORT ups.status`" || die "upsd does not respond: $OUT2" ; sleep 3
OUT3="`upsc dummy@localhost:$NUT_PORT ups.status`" || die "upsd does not respond: $OUT3" ; sleep 3
if echo "$OUT1$OUT2$OUT3" | grep "OB" && echo "$OUT1$OUT2$OUT3" | grep "OL" ; then
    log_info "OK, ups.status flips over time"
    PASSED="`expr $PASSED + 1`"
else
    log_error "ups.status did not flip over time"
    FAILED="`expr $FAILED + 1`"
fi

# We optionally make python module (if interpreter is found):
if [ x"${TOP_BUILDDIR}" != x ] \
&& [ -x "${TOP_BUILDDIR}/scripts/python/module/test_nutclient.py" ] \
; then
    # That script says it expects data/evolution500.seq (as the UPS1 dummy)
    # but the dummy data does not currently let issue the commands and
    # setvars tested from python script.
    log_separator
    log_info "Call Python module test suite: PyNUT (NUT Python bindings) without login credentials"
    if ( unset NUT_USER || true
         unset NUT_PASS || true
        "${TOP_BUILDDIR}/scripts/python/module/test_nutclient.py"
    ) ; then
        log_info "OK, PyNUT did not complain"
        PASSED="`expr $PASSED + 1`"
    else
        log_error "PyNUT complained, check above"
        FAILED="`expr $FAILED + 1`"
    fi

    log_separator
    log_info "Call Python module test suite: PyNUT (NUT Python bindings) with login credentials"
    if (
        NUT_USER='admin'
        NUT_PASS='mypass'
        export NUT_USER NUT_PASS
        "${TOP_BUILDDIR}/scripts/python/module/test_nutclient.py"
    ) ; then
        log_info "OK, PyNUT did not complain"
        PASSED="`expr $PASSED + 1`"
    else
        log_error "PyNUT complained, check above"
        FAILED="`expr $FAILED + 1`"
    fi
fi

# TODO: Make and run C++ client tests

# TODO: Some upsmon tests?

# Allow to leave the sandbox daemons running for a while,
# to experiment with them interactively:
if [ -n "${DEBUG_SLEEP-}" ] ; then
    log_info "Sleeping now as asked, so you can play with the driver and server (port $NUT_PORT) running"
    if [ "${DEBUG_SLEEP-}" -gt 0 ] ; then
        sleep "${DEBUG_SLEEP}"
    else
        sleep 60
    fi
    log_info "Sleep finished"
fi

log_separator
log_info "OVERALL: PASSED=$PASSED FAILED=$FAILED"
if [ "$PASSED" = 0 ] || [ "$FAILED" != 0 ] ; then
    die "Some test scenarios failed!"
else
    log_info "SUCCESS"
fi
