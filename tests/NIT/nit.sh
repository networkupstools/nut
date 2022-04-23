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
#	NUT_DEBUG_MIN=3	to set (minimum) debug level for drivers, upsd...
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
    *) log_info "Current directory '${BUILDDIR}' is not a .../tests/NIT" ;;
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
    *) log_info "Script source directory '${SRCDIR}' is not a .../tests/NIT" ;;
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
    (command -v ${PROG}) || die "Useless setup: ${PROG} not found in PATH: ${PATH}"
done

PID_UPSD=""
PID_DUMMYUPS=""
PID_DUMMYUPS1=""
PID_DUMMYUPS2=""

TESTDIR="$BUILDDIR/tmp"
# Technically the limit is sizeof(sockaddr.sun_path) for complete socket
# pathname, which varies 104-108 chars max on systems seen in CI farm;
# we reserve 17 chars for "/dummy-ups-dummy" longest filename.
if [ `echo "$TESTDIR" | wc -c` -gt 80 ]; then
    log_info "'$TESTDIR' is too long to store AF_UNIX socket files, will mktemp"
    if ! ( [ -n "${TMPDIR-}" ] && [ -d "${TMPDIR-}" ] && [ -w "${TMPDIR-}" ] ) ; then
        if [ -d /dev/shm ] && [ -w /dev/shm ]; then TMPDIR=/dev/shm ; else TMPDIR=/tmp ; fi
    fi
    TESTDIR="`mktemp -d "${TMPDIR}/nit-tmp.$$.XXXXXX"`" || die "Failed to mktemp"
else
    rm -rf "${TESTDIR}" || true
fi
log_info "Using '$TESTDIR' for generated configs and state files"

mkdir -p "${TESTDIR}/etc" "${TESTDIR}/run" && chmod 750 "${TESTDIR}/run" \
|| die "Failed to create temporary FS structure for the NIT"

stop_daemons() {
    if [ -n "$PID_UPSD$PID_DUMMYUPS$PID_DUMMYUPS1$PID_DUMMYUPS2" ] ; then
        log_info "Stopping test daemons"
        kill -15 $PID_UPSD $PID_DUMMYUPS $PID_DUMMYUPS1 $PID_DUMMYUPS2 2>/dev/null
    fi
}

trap 'RES=$?; stop_daemons; exit $RES;' 0 1 2 3 15

NUT_STATEPATH="${TESTDIR}/run"
NUT_ALTPIDPATH="${TESTDIR}/run"
NUT_CONFPATH="${TESTDIR}/etc"
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
export NUT_PORT

### upsd.conf: ##################################################

generatecfg_upsd_trivial() {
    # Populate the configs for the run
    cat > "$NUT_CONFPATH/upsd.conf" << EOF
STATEPATH "$NUT_STATEPATH"
LISTEN localhost $NUT_PORT
EOF
    [ $? = 0 ] || die "Failed to populate temporary FS structure for the NIT: upsd.conf"
    chmod 640 "$NUT_CONFPATH/upsd.conf"

    # Some systems listining on symbolic "localhost" actually
    # only bind to IPv6, and Python telnetlib resolves IPv4
    # and fails its connection tests. Others fare well with
    # both addresses in one command.
    for LH in 127.0.0.1 '::1' ; do
        if (
           ( cat /etc/hosts || getent hosts ) | grep "$LH" \
             || ping -c 1 "$LH"
        ) 2>/dev/null >/dev/null ; then
            echo "LISTEN $LH $NUT_PORT" >> "$NUT_CONFPATH/upsd.conf"
        fi
    done

    if [ -n "${NUT_DEBUG_MIN-}" ] ; then
        echo "DEBUG_MIN ${NUT_DEBUG_MIN}" >> "$NUT_CONFPATH/upsd.conf" || exit
    fi
}

generatecfg_upsd_nodev() {
    generatecfg_upsd_trivial
    echo "ALLOW_NO_DEVICE true" >> "$NUT_CONFPATH/upsd.conf" \
    || die "Failed to populate temporary FS structure for the NIT: upsd.conf"
}

### upsd.users: ##################################################

TESTPASS_ADMIN='mypass'
TESTPASS_TESTER='pass words'
TESTPASS_UPSMON_PRIMARY='P@ssW0rdAdm'
TESTPASS_UPSMON_SECONDARY='P@ssW0rd'

generatecfg_upsdusers_trivial() {
    cat > "$NUT_CONFPATH/upsd.users" << EOF
[admin]
    password = $TESTPASS_ADMIN
    actions = SET
    instcmds = ALL

[tester]
    password = "${TESTPASS_TESTER}"
    instcmds = test.battery.start
    instcmds = test.battery.stop

[dummy-admin-m]
    password = "${TESTPASS_UPSMON_PRIMARY}"
    upsmon master

[dummy-admin]
    password = "${TESTPASS_UPSMON_PRIMARY}"
    upsmon primary

[dummy-user-s]
    password = "${TESTPASS_UPSMON_SECONDARY}"
    upsmon slave

[dummy-user]
    password = "${TESTPASS_UPSMON_SECONDARY}"
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

       if [ -n "${NUT_DEBUG_MIN-}" ] ; then
           echo "DEBUG_MIN ${NUT_DEBUG_MIN}" >> "$NUT_CONFPATH/upsmon.conf" || exit
       fi
    ) || die "Failed to populate temporary FS structure for the NIT: upsmon.conf"
    chmod 640 "$NUT_CONFPATH/upsmon.conf"
}

generatecfg_upsmon_master() {
    generatecfg_upsmon_trivial
    echo "MONITOR 'dummy@localhost:$NUT_PORT' 0 'dummy-admin-m' '${TESTPASS_UPSMON_PRIMARY}' master" >> "$NUT_CONFPATH/upsmon.conf" \
    || die "Failed to populate temporary FS structure for the NIT: upsmon.conf"
}

generatecfg_upsmon_primary() {
    generatecfg_upsmon_trivial
    echo "MONITOR 'dummy@localhost:$NUT_PORT' 0 'dummy-admin' '${TESTPASS_UPSMON_PRIMARY}' primary" >> "$NUT_CONFPATH/upsmon.conf" \
    || die "Failed to populate temporary FS structure for the NIT: upsmon.conf"
}

generatecfg_upsmon_slave() {
    generatecfg_upsmon_trivial
    echo "MONITOR 'dummy@localhost:$NUT_PORT' 0 'dummy-user-s' '${TESTPASS_UPSMON_SECONDARY}' slave" >> "$NUT_CONFPATH/upsmon.conf" \
    || die "Failed to populate temporary FS structure for the NIT: upsmon.conf"
}

generatecfg_upsmon_secondary() {
    generatecfg_upsmon_trivial
    echo "MONITOR 'dummy@localhost:$NUT_PORT' 0 'dummy-user' '${TESTPASS_UPSMON_SECONDARY}' secondary" >> "$NUT_CONFPATH/upsmon.conf" \
    || die "Failed to populate temporary FS structure for the NIT: upsmon.conf"
}

### ups.conf: ##################################################

generatecfg_ups_trivial() {
    # Populate the configs for the run
    (   echo 'maxretry = 3' > "$NUT_CONFPATH/ups.conf" || exit
        if [ x"${TOP_BUILDDIR}" != x ]; then
            echo "driverpath = '${TOP_BUILDDIR}/drivers'" >> "$NUT_CONFPATH/ups.conf" || exit
        fi
        if [ -n "${NUT_DEBUG_MIN-}" ] ; then
            echo "debug_min = ${NUT_DEBUG_MIN}" >> "$NUT_CONFPATH/ups.conf" || exit
        fi
    ) || die "Failed to populate temporary FS structure for the NIT: ups.conf"
    chmod 640 "$NUT_CONFPATH/ups.conf"

}

generatecfg_ups_dummy() {
    generatecfg_ups_trivial

    cat > "$NUT_CONFPATH/dummy.seq" << EOF
ups.status: OB
TIMER 5
ups.status: OL
TIMER 5
EOF
    [ $? = 0 ] || die "Failed to populate temporary FS structure for the NIT: dummy.seq"

    cat >> "$NUT_CONFPATH/ups.conf" << EOF
[dummy]
    driver = dummy-ups
    desc = "Crash Dummy"
    port = dummy.seq
    #mode = dummy-loop
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
    mode = dummy-once
EOF
        [ $? = 0 ] || die "Failed to populate temporary FS structure for the NIT: ups.conf"

        # HACK: Avoid empty ups.status that may be present in example docs
        # FIXME: Might we actually want that value (un-)set for tests?..
        # TODO: Check if the problem was with dummy-ups looping? [#1385]
        for F in "$NUT_CONFPATH/"*.dev "$NUT_CONFPATH/"*.seq ; do
            sed -e 's,^ups.status: *$,ups.status: OL BOOST,' -i'.bak' "$F"
            grep -E '^ups.status:' "$F" >/dev/null || { echo "ups.status: OL BOOST" >> "$F"; }
        done
    fi

}

#####################################################

isPidAlive() {
    [ -n "$1" ] && [ "$1" -gt 0 ] || return
    [ -d "/proc/$1" ] || kill -0 "$1" 2>/dev/null
}

FAILED=0
PASSED=0

testcase_upsd_no_configs_at_all() {
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
}

testcase_upsd_no_configs_driver_file() {
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
}

testcase_upsd_no_configs_in_driver_file() {
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
}

testcase_upsd_allow_no_device() {
    log_separator
    log_info "Test UPSD allowed to run without driver configs"
    generatecfg_upsd_nodev
    generatecfg_upsdusers_trivial
    generatecfg_ups_trivial
    upsd -F &
    PID_UPSD="$!"
    sleep 2
    if isPidAlive "$PID_UPSD"; then
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
}

testgroup_upsd_invalid_configs() {
    testcase_upsd_no_configs_at_all
    testcase_upsd_no_configs_driver_file
    testcase_upsd_no_configs_in_driver_file
}

testgroup_upsd_questionable_configs() {
    testcase_upsd_allow_no_device
}

#########################################################
### Tests in a common sandbox with driver(s) + server ###
#########################################################

SANDBOX_CONFIG_GENERATED=false
sandbox_generate_configs() {
    if $SANDBOX_CONFIG_GENERATED ; then return ; fi

    log_info "Generating configs for sandbox"
    generatecfg_upsd_nodev
    generatecfg_upsdusers_trivial
    generatecfg_ups_dummy
    SANDBOX_CONFIG_GENERATED=true
}

sandbox_forget_configs() {
    SANDBOX_CONFIG_GENERATED=false
    if [ -z "${DEBUG_SLEEP-}" ] ; then
        stop_daemons
    fi
}

sandbox_start_upsd() {
    if isPidAlive "$PID_UPSD" ; then
        return 0
    fi

    sandbox_generate_configs

    log_info "Starting UPSD for sandbox"
    upsd -F &
    PID_UPSD="$!"
    sleep 5
}

sandbox_start_drivers() {
    if isPidAlive "$PID_DUMMYUPS" \
    && { [ x"${TOP_SRCDIR}" != x ] && isPidAlive "$PID_DUMMYUPS1" && isPidAlive "$PID_DUMMYUPS2" \
         || [ x"${TOP_SRCDIR}" = x ] ; } \
    ; then
        # All drivers expected for this environment are already running
        return 0
    fi

    sandbox_generate_configs

    log_info "Starting dummy-ups driver(s) for sandbox"
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
}

testcase_sandbox_start_upsd_alone() {
    log_separator
    log_info "Test starting UPSD but not a driver before it"
    sandbox_start_upsd

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
    if ! echo "$OUT" | grep 'Error: Driver not connected' ; then
        log_error "got reply for upsc query when 'Error: Driver not connected' was expected: '$OUT'"
        FAILED="`expr $FAILED + 1`"
    else
        PASSED="`expr $PASSED + 1`"
    fi
}

testcase_sandbox_start_upsd_after_drivers() {
    # Historically this is a fallback from testcase_sandbox_start_drivers_after_upsd
    kill -15 $PID_UPSD 2>/dev/null
    wait $PID_UPSD
    upsd -F &
    PID_UPSD="$!"

    sandbox_start_drivers
    sandbox_start_upsd

    sleep 5
    upsc dummy@localhost:$NUT_PORT || die "upsd does not respond"
}

testcase_sandbox_start_drivers_after_upsd() {
    #sandbox_start_upsd
    testcase_sandbox_start_upsd_alone
    sandbox_start_drivers

    log_info "Query driver state from UPSD by UPSC after driver startup"
    upsc dummy@localhost:$NUT_PORT || {
        # Should not get to this, except on very laggy systems maybe
        log_error "Query failed, retrying with UPSD started after drivers"
        testcase_start_upsd_after_drivers
    }

    if [ x"${TOP_SRCDIR}" != x ]; then
        log_info "Wait for dummy UPSes with larger data sets to initialize"
        for U in UPS1 UPS2 ; do
            COUNTDOWN=60
            while ! upsc $U@localhost:$NUT_PORT ups.status ; do
                sleep 1
                COUNTDOWN="`expr $COUNTDOWN - 1`"
                # Systemic error, e.g. could not create socket file?
                [ "$COUNTDOWN" -lt 1 ] && die "Dummy driver did not start or respond in time"
            done
        done
    fi

    log_info "Expected drivers are now responding via UPSD"
}

testcase_sandbox_upsc_query_model() {
    OUT="`upsc dummy@localhost:$NUT_PORT device.model`" || die "upsd does not respond: $OUT"
    if [ x"$OUT" != x"Dummy UPS" ] ; then
        log_error "got this reply for upsc query when 'Dummy UPS' was expected: $OUT"
        FAILED="`expr $FAILED + 1`"
    else
        PASSED="`expr $PASSED + 1`"
    fi
}

testcase_sandbox_upsc_query_bogus() {
    log_info "Query driver state from UPSD by UPSC for bogus info"
    OUT="`upsc dummy@localhost:$NUT_PORT ups.bogus.value 2>&1`" && {
        log_error "upsc was supposed to answer with error exit code: $OUT"
        FAILED="`expr $FAILED + 1`"
    }
    if ! echo "$OUT" | grep 'Error: Variable not supported by UPS' ; then
        log_error "got reply for upsc query when 'Error: Variable not supported by UPS' was expected: $OUT"
        FAILED="`expr $FAILED + 1`"
    else
        PASSED="`expr $PASSED + 1`"
    fi
}

testcase_sandbox_upsc_query_timer() {
    log_separator
    log_info "Test that dummy-ups TIMER action changes the reported state"
    # Driver is set up to flip ups.status every 5 sec, so check every 3
    OUT1="`upsc dummy@localhost:$NUT_PORT ups.status`" || die "upsd does not respond: $OUT1" ; sleep 3
    OUT2="`upsc dummy@localhost:$NUT_PORT ups.status`" || die "upsd does not respond: $OUT2"
    OUT3=""
    OUT4=""
    if [ x"$OUT1" = x"$OUT2" ]; then
        sleep 3
        OUT3="`upsc dummy@localhost:$NUT_PORT ups.status`" || die "upsd does not respond: $OUT3"
        if [ x"$OUT2" = x"$OUT3" ]; then
            sleep 3
            OUT4="`upsc dummy@localhost:$NUT_PORT ups.status`" || die "upsd does not respond: $OUT4"
        fi
    fi
    if echo "$OUT1$OUT2$OUT3$OUT4" | grep "OB" && echo "$OUT1$OUT2$OUT3$OUT4" | grep "OL" ; then
        log_info "OK, ups.status flips over time"
        PASSED="`expr $PASSED + 1`"
    else
        log_error "ups.status did not flip over time"
        FAILED="`expr $FAILED + 1`"
    fi
}

isTestablePython() {
    # We optionally make python module (if interpreter is found):
    if [ x"${TOP_BUILDDIR}" = x ] \
    || [ ! -x "${TOP_BUILDDIR}/scripts/python/module/test_nutclient.py" ] \
    ; then
        return 1
    fi
    return 0
}

testcase_sandbox_python_without_credentials() {
    isTestablePython || return 0
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
}

testcase_sandbox_python_with_credentials() {
    isTestablePython || return 0

    # That script says it expects data/evolution500.seq (as the UPS1 dummy)
    # but the dummy data does not currently let issue the commands and
    # setvars tested from python script.
    log_separator
    log_info "Call Python module test suite: PyNUT (NUT Python bindings) with login credentials"
    if (
        NUT_USER='admin'
        NUT_PASS="${TESTPASS_ADMIN}"
        export NUT_USER NUT_PASS
        "${TOP_BUILDDIR}/scripts/python/module/test_nutclient.py"
    ) ; then
        log_info "OK, PyNUT did not complain"
        PASSED="`expr $PASSED + 1`"
    else
        log_error "PyNUT complained, check above"
        FAILED="`expr $FAILED + 1`"
    fi
}

testcases_sandbox_python() {
    isTestablePython || return 0
    testcase_sandbox_python_without_credentials
    testcase_sandbox_python_with_credentials
}

####################################

isTestableCppNIT() {
    # We optionally make and here can run C++ client tests:
    if [ x"${TOP_BUILDDIR}" = x ] \
    || [ ! -x "${TOP_BUILDDIR}/tests/cppnit" ] \
    ; then
        return 1
    fi
    return 0
}

testcase_sandbox_cppnit_without_creds() {
    isTestableCppNIT || return 0

    log_separator
    log_info "Call libnutclient test suite: cppnit without login credentials"
    if ( unset NUT_USER || true
         unset NUT_PASS || true
        "${TOP_BUILDDIR}/tests/cppnit"
    ) ; then
        log_info "OK, cppnit did not complain"
        PASSED="`expr $PASSED + 1`"
    else
        log_error "cppnit complained, check above"
        FAILED="`expr $FAILED + 1`"
    fi
}

testcase_sandbox_cppnit_simple_admin() {
    isTestableCppNIT || return 0

    log_separator
    log_info "Call libnutclient test suite: cppnit with login credentials: simple admin"
    if (
        NUT_USER='admin'
        NUT_PASS="${TESTPASS_ADMIN}"
        if [ x"${TOP_SRCDIR}" != x ]; then
            # Avoid dummies with TIMER flip-flops
            NUT_SETVAR_DEVICE='UPS2'
        else
            # Risks failure when lauching sub-test at the wrong second
            NUT_SETVAR_DEVICE='dummy'
        fi
        unset NUT_PRIMARY_DEVICE
        export NUT_USER NUT_PASS NUT_SETVAR_DEVICE
        "${TOP_BUILDDIR}/tests/cppnit"
    ) ; then
        log_info "OK, cppnit did not complain"
        PASSED="`expr $PASSED + 1`"
    else
        log_error "cppnit complained, check above"
        FAILED="`expr $FAILED + 1`"
    fi
}

testcase_sandbox_cppnit_upsmon_primary() {
    isTestableCppNIT || return 0

    log_separator
    log_info "Call libnutclient test suite: cppnit with login credentials: upsmon-primary"
    if (
        NUT_USER='dummy-admin'
        NUT_PASS="${TESTPASS_UPSMON_PRIMARY}"
        NUT_PRIMARY_DEVICE='dummy'
        unset NUT_SETVAR_DEVICE
        export NUT_USER NUT_PASS NUT_PRIMARY_DEVICE
        "${TOP_BUILDDIR}/tests/cppnit"
    ) ; then
        log_info "OK, cppnit did not complain"
        PASSED="`expr $PASSED + 1`"
    else
        log_error "cppnit complained, check above"
        FAILED="`expr $FAILED + 1`"
    fi
}

testcase_sandbox_cppnit_upsmon_master() {
    isTestableCppNIT || return 0

    log_separator
    log_info "Call libnutclient test suite: cppnit with login credentials: upsmon-master"
    if (
        NUT_USER='dummy-admin-m'
        NUT_PASS="${TESTPASS_UPSMON_PRIMARY}"
        NUT_PRIMARY_DEVICE='dummy'
        unset NUT_SETVAR_DEVICE
        export NUT_USER NUT_PASS NUT_PRIMARY_DEVICE
        "${TOP_BUILDDIR}/tests/cppnit"
    ) ; then
        log_info "OK, cppnit did not complain"
        PASSED="`expr $PASSED + 1`"
    else
        log_error "cppnit complained, check above"
        FAILED="`expr $FAILED + 1`"
    fi
}

testcases_sandbox_cppnit() {
    isTestableCppNIT || return 0
    testcase_sandbox_cppnit_without_creds
    testcase_sandbox_cppnit_upsmon_primary
    testcase_sandbox_cppnit_upsmon_master
    testcase_sandbox_cppnit_simple_admin
}

# TODO: Some upsmon tests?

testgroup_sandbox() {
    testcase_sandbox_start_drivers_after_upsd
    testcase_sandbox_upsc_query_model
    testcase_sandbox_upsc_query_bogus
    testcase_sandbox_upsc_query_timer
    testcases_sandbox_python
    testcases_sandbox_cppnit

    sandbox_forget_configs
}

testgroup_sandbox_python() {
    # Arrange for quick test iterations
    testcase_sandbox_start_drivers_after_upsd
    testcases_sandbox_python
    sandbox_forget_configs
}

testgroup_sandbox_cppnit() {
    # Arrange for quick test iterations
    testcase_sandbox_start_drivers_after_upsd
    testcases_sandbox_cppnit
    sandbox_forget_configs
}

testgroup_sandbox_cppnit_simple_admin() {
    # Arrange for quick test iterations
    testcase_sandbox_start_drivers_after_upsd
    testcase_sandbox_cppnit_simple_admin
    sandbox_forget_configs
}

################################################################

case "${NIT_CASE}" in
    cppnit) testgroup_sandbox_cppnit ;;
    python) testgroup_sandbox_python ;;
    testcase_*|testgroup_*|testcases_*|testgroups_*)
        "${NIT_CASE}" ;;
    "") # Default test groups:
        testgroup_upsd_invalid_configs
        testgroup_upsd_questionable_configs
        testgroup_sandbox
        ;;
    *)  die "Unsupported NIT_CASE='$NIT_CASE' was requested" ;;
esac

log_separator
log_info "OVERALL: PASSED=$PASSED FAILED=$FAILED"

# Allow to leave the sandbox daemons running for a while,
# to experiment with them interactively:
if [ -n "${DEBUG_SLEEP-}" ] ; then
    log_separator
    log_info "Sleeping now as asked, so you can play with the driver and server running; hint: export NUT_PORT=$NUT_PORT"
    log_separator
    if [ "${DEBUG_SLEEP-}" -gt 0 ] ; then
        sleep "${DEBUG_SLEEP}"
    else
        sleep 60
    fi
    log_info "Sleep finished"
    log_separator
fi

if [ "$PASSED" = 0 ] || [ "$FAILED" != 0 ] ; then
    die "Some test scenarios failed!"
else
    log_info "SUCCESS"
fi
