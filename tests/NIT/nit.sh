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
#	2022-2023 Jim Klimov <jimklimov+nut@gmail.com>
#
# License: GPLv2+

TZ=UTC
LANG=C
LC_ALL=C
export TZ LANG LC_ALL

NUT_QUIET_INIT_SSL="true"
export NUT_QUIET_INIT_SSL

NUT_QUIET_INIT_UPSNOTIFY="true"
export NUT_QUIET_INIT_UPSNOTIFY

NUT_DEBUG_PID="true"
export NUT_DEBUG_PID

log_separator() {
    echo "" >&2
    echo "================================" >&2
}

shouldDebug() {
    [ -n "$DEBUG" ] || [ -n "$DEBUG_SLEEP" ]
}

log_debug() {
    if shouldDebug ; then
        echo "`TZ=UTC LANG=C date` [DEBUG] $@" >&2
    fi
    return 0
}

log_info() {
    echo "`TZ=UTC LANG=C date` [INFO] $@" >&2
}

log_warn() {
    echo "`TZ=UTC LANG=C date` [WARNING] $@" >&2
}

log_error() {
    echo "`TZ=UTC LANG=C date` [ERROR] $@" >&2
}

report_NUT_PORT() {
    # Try to say which processes deal with current NUT_PORT
    [ -n "${NUT_PORT}" ] || return

    log_info "Trying to report users of NUT_PORT=${NUT_PORT}"
    # Note: on Solarish systems, `netstat -anp` does not report PID info
    (netstat -an ; netstat -anp || sockstat -l) 2>/dev/null | grep -w "${NUT_PORT}" \
    || (lsof -i :"${NUT_PORT}") 2>/dev/null \
    || true

    [ -z "${PID_UPSD}" ] || log_info "UPSD was last known to start as PID ${PID_UPSD}"
}

isBusy_NUT_PORT() {
    # Try to say if current NUT_PORT is busy (0 = true)
    # or available (non-0 = false)
    [ -n "${NUT_PORT}" ] || return

    log_debug "isBusy_NUT_PORT() Trying to report if NUT_PORT=${NUT_PORT} is used"
    if [ -e /proc/net/tcp ] || [ -e /proc/net/tcp6 ]; then
        # Assume Linux - hex-encoded
        # IPv4:
        #   sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode
        #   0: 0100007F:EE48 00000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 48881 1 00000000ec238d02 100 0 0 10 0
        #   ^^^ 1.0.0.127 - note reversed byte order!
        # IPv6:
        #   sl  local_address                         remote_address                        st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode
        #   0: 00000000000000000000000000000000:1F46 00000000000000000000000000000000:0000 0A 00000000:00000000 00:00000000 00000000    33        0 37451 1 00000000fa3c0c15 100 0 0 10 0
        NUT_PORT_HEX="`printf '%04X' "${NUT_PORT}"`"
        NUT_PORT_HITS="`cat /proc/net/tcp /proc/net/tcp6 2>/dev/null | awk '{print $2}' | grep -E ":${NUT_PORT_HEX}\$"`" \
        && [ -n "$NUT_PORT_HITS" ] \
        && log_debug "isBusy_NUT_PORT() found that NUT_PORT=${NUT_PORT} is busy per /proc/net/tcp*" \
        && return 0

        # We had a way to check, and the way said port is available
        log_debug "isBusy_NUT_PORT() found that NUT_PORT=${NUT_PORT} is not busy per /proc/net/tcp*"
        return 1
    fi

    (netstat -an || sockstat -l || ss -tn || ss -n) 2>/dev/null | grep -E "[:.]${NUT_PORT}(\t| |\$)" > /dev/null \
    && log_debug "isBusy_NUT_PORT() found that NUT_PORT=${NUT_PORT} is busy per netstat, sockstat or ss" \
    && return

    (lsof -i :"${NUT_PORT}") 2>/dev/null \
    && log_debug "isBusy_NUT_PORT() found that NUT_PORT=${NUT_PORT} is busy per lsof" \
    && return

    # Not busy... or no tools to confirm?
    if (command -v netstat || command -v sockstat || command -v ss || command -v lsof) 2>/dev/null >/dev/null ; then
        # at least one tool is present, so not busy
        log_debug "isBusy_NUT_PORT() found that NUT_PORT=${NUT_PORT} is not busy per netstat, sockstat, ss or lsof"
        return 1
    fi

    # If the current shell interpreter is bash, it can do a bit of networking:
    if [ -n "${BASH_VERSION-}" ]; then
        # NOTE: Probing host names we use in upsd.conf
        # See generatecfg_upsd_trivial() for a more carefully made list
        (   # Hide this looped noise:
            # ./nit.sh: connect: Connection refused
            # ./nit.sh: line 112: /dev/tcp/localhost/35050: Connection refused
            if shouldDebug ; then : ; else
                exec 2>/dev/null
            fi
            for H in "localhost" "127.0.0.1" "::1"; do
                if echo "HELP" > "/dev/tcp/${H}/${NUT_PORT}" ; then
                    # Successfully connected - port isBusy
                    return 0
                fi
            done
            return 1
        ) && return 0
        log_warn "isBusy_NUT_PORT() tried with BASH-specific query, and port does not seem busy (or something else errored out)"
    fi

    # Assume not busy to not preclude testing in 100% of the cases
    log_warn "isBusy_NUT_PORT() can not say definitively, tools for checking NUT_PORT=$NUT_PORT are not available"
    return 1
}

die() {
    echo "[FATAL] $@" >&2
    exit 1
}

# By default, keep stdout hidden but report the errors:
[ -n "$RUNCMD_QUIET_OUT" ] || RUNCMD_QUIET_OUT=true
[ -n "$RUNCMD_QUIET_ERR" ] || RUNCMD_QUIET_ERR=false
runcmd() {
    # Re-uses a couple of files in test scratch area NUT_STATEPATH
    # to store the stderr and stdout of the launched program.
    # Prints the captured output back and returns the exit code.

    [ -n "${NUT_STATEPATH}" -a -d "${NUT_STATEPATH}" -a -w "${NUT_STATEPATH}" ] \
    || die "runcmd() called when NUT_STATEPATH was not yet set up"

    # Values from variables below may be used until next runcmd():
    CMDRES=0
    CMDOUT=""
    CMDERR=""

    "$@" > "${NUT_STATEPATH}/runcmd.out" 2>"${NUT_STATEPATH}/runcmd.err" || CMDRES=$?
    CMDOUT="`cat "${NUT_STATEPATH}/runcmd.out"`"
    CMDERR="`cat "${NUT_STATEPATH}/runcmd.err"`"

    [ "$RUNCMD_QUIET_OUT" = true ] || { [ -z "$CMDOUT" ] || echo "$CMDOUT" ; }
    [ "$RUNCMD_QUIET_ERR" = true ] || { [ -z "$CMDERR" ] || echo "$CMDERR" >&2 ; }

    return $CMDRES
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
if test ! -w "${BUILDDIR}" ; then
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

# No fuss about LD_LIBRARY_PATH: for most of the (client) binaries that
# need it, the PATH entries below would contain libtool wrapper scripts;
# for other builds we use system default or caller's env. One exception
# so far is nut-scanner that needs to know where libupsclient.so is...
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

LD_LIBRARY_PATH_ORIG="${LD_LIBRARY_PATH-}"
LD_LIBRARY_PATH_CLIENT=""
if [ x"${TOP_BUILDDIR}" != x ]; then
    LD_LIBRARY_PATH_CLIENT="${TOP_BUILDDIR}/clients:${TOP_BUILDDIR}/clients/.libs"
fi

if [ x"${LD_LIBRARY_PATH_CLIENT}" != x ]; then
    if [ -n "${LD_LIBRARY_PATH_ORIG-}" ]; then
        LD_LIBRARY_PATH_CLIENT="${LD_LIBRARY_PATH_CLIENT}:${LD_LIBRARY_PATH_ORIG}"
    fi
else
    LD_LIBRARY_PATH_CLIENT="${LD_LIBRARY_PATH_ORIG}"
fi

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
    if ( [ -n "${TMPDIR-}" ] && [ -d "${TMPDIR-}" ] && [ -w "${TMPDIR-}" ] ) ; then
        :
    else
        if [ -d /dev/shm ] && [ -w /dev/shm ]; then TMPDIR=/dev/shm ; else TMPDIR=/tmp ; fi
    fi
    TESTDIR="`mktemp -d "${TMPDIR}/nit-tmp.$$.XXXXXX"`" || die "Failed to mktemp"
else
    rm -rf "${TESTDIR}" || true
fi
log_info "Using '$TESTDIR' for generated configs and state files"

mkdir -p "${TESTDIR}/etc" "${TESTDIR}/run" && chmod 750 "${TESTDIR}/run" \
|| die "Failed to create temporary FS structure for the NIT"

if [ "`id -u`" = 0 ]; then
    log_info "Test script was started by 'root' - expanding permissions for '${TESTDIR}/run' so unprivileged daemons may create pipes and PID files there"
    chmod 777 "${TESTDIR}/run"
fi

stop_daemons() {
    if [ -n "$PID_UPSD$PID_DUMMYUPS$PID_DUMMYUPS1$PID_DUMMYUPS2" ] ; then
        log_info "Stopping test daemons"
        kill -15 $PID_UPSD $PID_DUMMYUPS $PID_DUMMYUPS1 $PID_DUMMYUPS2 2>/dev/null || return 0
        wait $PID_UPSD $PID_DUMMYUPS $PID_DUMMYUPS1 $PID_DUMMYUPS2 || true
    fi

    PID_UPSD=""
    PID_DUMMYUPS=""
    PID_DUMMYUPS1=""
    PID_DUMMYUPS2=""
}

trap 'RES=$?; stop_daemons; if [ "${TESTDIR}" != "${BUILDDIR}/tmp" ] ; then rm -rf "${TESTDIR}" ; fi; exit $RES;' 0 1 2 3 15

NUT_STATEPATH="${TESTDIR}/run"
NUT_ALTPIDPATH="${TESTDIR}/run"
NUT_CONFPATH="${TESTDIR}/etc"
export NUT_STATEPATH NUT_ALTPIDPATH NUT_CONFPATH

# TODO: Find a portable way to (check and) grab a random unprivileged port?
if [ -n "${NUT_PORT-}" ] && [ "$NUT_PORT" -gt 0 ] && [ "$NUT_PORT" -lt 65536 ] ; then
    if isBusy_NUT_PORT ; then
        log_warn "NUT_PORT=$NUT_PORT requested by caller seems occupied; tests may fail below"
    fi
else
    COUNTDOWN=60
    while [ "$COUNTDOWN" -gt 0 ] ; do
        DELTA1="`date +%S`" || DELTA1=0
        DELTA2="`expr $$ % 99`" || DELTA2=0

        NUT_PORT="`expr 34931 + $DELTA1 + $DELTA2`" \
        && [ "$NUT_PORT" -gt 0 ] && [ "$NUT_PORT" -lt 65536 ] \
        || NUT_PORT=34931

        if isBusy_NUT_PORT ; then : ; else
            break
        fi

        log_warn "Selected NUT_PORT=$NUT_PORT seems occupied; will try another in a few seconds"
        COUNTDOWN="`expr "$COUNTDOWN" - 1`"

        [ "$COUNTDOWN" = 0 ] || sleep 2
    done

    if [ "$COUNTDOWN" = 0 ] ; then
        COUNTDOWN=60
        DELTA1=1025
        while [ "$COUNTDOWN" -gt 0 ] ; do
            DELTA2="`expr $RANDOM % 64000`" \
            && [ "$DELTA2" -ge 0 ] || die "Can not pick random port"

            NUT_PORT="`expr $DELTA1 + $DELTA2`"
            if isBusy_NUT_PORT ; then : ; else
                break
            fi

            # Loop quickly, no sleep here
            COUNTDOWN="`expr "$COUNTDOWN" - 1`"
        done

        if [ "$COUNTDOWN" = 0 ] ; then
            die "Can not pick random port"
        fi
    fi
fi
export NUT_PORT
# Help track collisions in log, if someone else starts a test in same directory
log_info "Using NUT_PORT=${NUT_PORT} for this test run"

### upsd.conf: ##################################################

generatecfg_upsd_trivial() {
    # Populate the configs for the run
    cat > "$NUT_CONFPATH/upsd.conf" << EOF
STATEPATH "$NUT_STATEPATH"
LISTEN localhost $NUT_PORT
EOF
    [ $? = 0 ] || die "Failed to populate temporary FS structure for the NIT: upsd.conf"

    if [ "`id -u`" = 0 ]; then
        log_info "Test script was started by 'root' - expanding permissions for '$NUT_CONFPATH/upsd.conf' so unprivileged daemons (after de-elevation) may read it"
        chmod 644 "$NUT_CONFPATH/upsd.conf"
    else
        chmod 640 "$NUT_CONFPATH/upsd.conf"
    fi

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

    if [ "`id -u`" = 0 ]; then
        log_info "Test script was started by 'root' - expanding permissions for '$NUT_CONFPATH/upsd.users' so unprivileged daemons (after de-elevation) may read it"
        chmod 644 "$NUT_CONFPATH/upsd.users"
    else
        chmod 640 "$NUT_CONFPATH/upsd.users"
    fi
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

    if [ "`id -u`" = 0 ]; then
        log_info "Test script was started by 'root' - expanding permissions for '$NUT_CONFPATH/upsmon.conf' so unprivileged daemons (after de-elevation) may read it"
        chmod 644 "$NUT_CONFPATH/upsmon.conf"
    else
        chmod 640 "$NUT_CONFPATH/upsmon.conf"
    fi
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
            echo "driverpath = \"${TOP_BUILDDIR}/drivers\"" >> "$NUT_CONFPATH/ups.conf" || exit
        fi
        if [ -n "${NUT_DEBUG_MIN-}" ] ; then
            echo "debug_min = ${NUT_DEBUG_MIN}" >> "$NUT_CONFPATH/ups.conf" || exit
        fi
    ) || die "Failed to populate temporary FS structure for the NIT: ups.conf"

    if [ "`id -u`" = 0 ]; then
        log_info "Test script was started by 'root' - expanding permissions for '$NUT_CONFPATH/ups.conf' so unprivileged daemons (after de-elevation) may read it"
        chmod 644 "$NUT_CONFPATH/ups.conf"
    else
        chmod 640 "$NUT_CONFPATH/ups.conf"
    fi
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
        # Avoid "sed -i", it behaves differently on some platforms
        # and is completely absent on others [#1736 and earlier]
        for F in "$NUT_CONFPATH/"*.dev "$NUT_CONFPATH/"*.seq ; do
            sed -e 's,^ups.status: *$,ups.status: OL BOOST,' "$F" > "$F.bak"
            mv -f "$F.bak" "$F"
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
FAILED_FUNCS=""
PASSED=0

testcase_upsd_no_configs_at_all() {
    log_separator
    log_info "[testcase_upsd_no_configs_at_all] Test UPSD without configs at all"
    upsd -F
    if [ "$?" = 0 ]; then
        log_error "[testcase_upsd_no_configs_at_all] upsd should fail without configs"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_upsd_no_configs_at_all"
    else
        log_info "[testcase_upsd_no_configs_at_all] PASSED: upsd failed to start in wrong conditions"
        PASSED="`expr $PASSED + 1`"
    fi
}

testcase_upsd_no_configs_driver_file() {
    log_separator
    log_info "[testcase_upsd_no_configs_driver_file] Test UPSD without driver config file"
    generatecfg_upsd_trivial
    upsd -F
    if [ "$?" = 0 ]; then
        log_error "[testcase_upsd_no_configs_driver_file] upsd should fail without driver config file"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_upsd_no_configs_driver_file"
    else
        log_info "[testcase_upsd_no_configs_driver_file] PASSED: upsd failed to start in wrong conditions"
        PASSED="`expr $PASSED + 1`"
    fi
}

testcase_upsd_no_configs_in_driver_file() {
    log_separator
    log_info "[testcase_upsd_no_configs_in_driver_file] Test UPSD without drivers defined in config file"
    generatecfg_upsd_trivial
    generatecfg_ups_trivial
    upsd -F
    if [ "$?" = 0 ]; then
        log_error "[testcase_upsd_no_configs_in_driver_file] upsd should fail without drivers defined in config file"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_upsd_no_configs_in_driver_file"
    else
        log_info "[testcase_upsd_no_configs_in_driver_file] PASSED: upsd failed to start in wrong conditions"
        PASSED="`expr $PASSED + 1`"
    fi
}

upsd_start_loop() {
    TESTCASE="${1-upsd_start_loop}"

    if isPidAlive "$PID_UPSD" ; then
        return 0
    fi

    upsd -F &
    PID_UPSD="$!"
    log_debug "[${TESTCASE}] Tried to start UPSD as PID $PID_UPSD"
    sleep 2
    # Due to a busy port, server could have died by now

    COUNTDOWN=60
    while [ "$COUNTDOWN" -gt 0 ]; do
        sleep 1
        COUNTDOWN="`expr $COUNTDOWN - 1`"

        # Is our server alive AND occupying the port?
        PID_OK=true
        isPidAlive "$PID_UPSD" || PID_OK=false # not running
        PORT_OK=true
        isBusy_NUT_PORT 2>/dev/null >/dev/null || PORT_OK=false # not busy
        if "${PID_OK}" ; then
            if "${PORT_OK}" ; then break ; fi
            continue
        fi

        # FIXME: If we are here, even once, then PID_UPSD which we
        # knew has already disappeared... wait() for its exit-code?
        # Give some time for ports to time out, if busy and that is
        # why the server died.
        PORT_WORD=""
        ${PORT_OK} || PORT_WORD="not "
        log_warn "[${TESTCASE}] Port ${NUT_PORT} is ${PORT_WORD}listening and UPSD PID $PID_UPSD is not alive, will sleep and retry"

        sleep 10
        upsd -F &
        PID_UPSD="$!"
        log_warn "[${TESTCASE}] Tried to start UPSD again, now as PID $PID_UPSD"
        sleep 5
    done

    if [ "$COUNTDOWN" -le 50 ] ; then
        # Should not get to this, except on very laggy systems maybe
        log_warn "[${TESTCASE}] Had to wait a few retries for the UPSD process to appear"
    fi

    # Return code is 0/OK if the server is alive AND occupying the port
    if isPidAlive "$PID_UPSD" && isBusy_NUT_PORT 2>/dev/null >/dev/null ; then
        log_debug "[${TESTCASE}] Port ${NUT_PORT} is listening and UPSD PID $PID_UPSD is alive"
        return
    fi

    log_error "[${TESTCASE}] Port ${NUT_PORT} is not listening and/or UPSD PID $PID_UPSD is not alive"
    return 1
}

testcase_upsd_allow_no_device() {
    log_separator
    log_info "[testcase_upsd_allow_no_device] Test UPSD allowed to run without driver configs"
    generatecfg_upsd_nodev
    generatecfg_upsdusers_trivial
    generatecfg_ups_trivial
    if shouldDebug ; then
        ls -la "$NUT_CONFPATH/" || true
    fi

    upsd_start_loop "testcase_upsd_allow_no_device"

    res_testcase_upsd_allow_no_device=0
    if [ "$COUNTDOWN" -gt 0 ] \
    && isPidAlive "$PID_UPSD" \
    ; then
        log_info "[testcase_upsd_allow_no_device] OK, upsd is running"
        PASSED="`expr $PASSED + 1`"

        log_separator
        log_info "[testcase_upsd_allow_no_device] Query listing from UPSD by UPSC (no devices configured yet) to test that UPSD responds to UPSC"
        if runcmd upsc -l localhost:$NUT_PORT ; then
            :
        else
            # Note: avoid exact matching for stderr, because it can have Init SSL messages etc.
            if echo "$CMDERR" | grep "Error: Server disconnected" >/dev/null ; then
                log_warn "[testcase_upsd_allow_no_device] Retry once to rule out laggy systems"
                sleep 3
                runcmd upsc -l localhost:$NUT_PORT
            fi
            if echo "$CMDERR" | grep "Error: Server disconnected" >/dev/null ; then
                log_warn "[testcase_upsd_allow_no_device] Retry once more to rule out very laggy systems"
                sleep 15
                runcmd upsc -l localhost:$NUT_PORT
            fi
            [ "$CMDRES" = 0 ] || die "[testcase_upsd_allow_no_device] upsd does not respond on port ${NUT_PORT} ($?): $CMDOUT"
        fi
        if [ -n "$CMDOUT" ] ; then
            log_error "[testcase_upsd_allow_no_device] got reply for upsc listing when none was expected: $CMDOUT"
            FAILED="`expr $FAILED + 1`"
            FAILED_FUNCS="$FAILED_FUNCS testcase_upsd_allow_no_device"
            res_testcase_upsd_allow_no_device=1
        else
            log_info "[testcase_upsd_allow_no_device] OK, empty response as expected"
            PASSED="`expr $PASSED + 1`"
        fi
    else
        log_error "[testcase_upsd_allow_no_device] upsd was expected to be running although no devices are defined; is ups.conf populated?"
        ls -la "$NUT_CONFPATH/" || true
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_upsd_allow_no_device"
        res_testcase_upsd_allow_no_device=1
        report_NUT_PORT
    fi

    log_info "[testcase_upsd_allow_no_device] stopping upsd: $PID_UPSD"
    UPSD_RES=0
    kill -15 $PID_UPSD
    wait $PID_UPSD || UPSD_RES=$?
    if [ "$res_testcase_upsd_allow_no_device" = 0 ] ; then
        log_info "[testcase_upsd_allow_no_device] upsd exit-code was: $UPSD_RES"
    else
        log_error "[testcase_upsd_allow_no_device] upsd exit-code was: $UPSD_RES"
    fi
    if [ "$UPSD_RES" != 0 ]; then
        return $UPSD_RES
    fi
    return $res_testcase_upsd_allow_no_device
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

    upsd_start_loop "sandbox"
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
    log_debug "Tried to start dummy-ups driver for 'dummy' as PID $PID_DUMMYUPS"

    if [ x"${TOP_SRCDIR}" != x ]; then
        dummy-ups -a UPS1 -F &
        PID_DUMMYUPS1="$!"
        log_debug "Tried to start dummy-ups driver for 'UPS1' as PID $PID_DUMMYUPS1"

        dummy-ups -a UPS2 -F &
        PID_DUMMYUPS2="$!"
        log_debug "Tried to start dummy-ups driver for 'UPS2' as PID $PID_DUMMYUPS2"
    fi

    sleep 5

    if shouldDebug ; then
        (ps -ef || ps -xawwu) 2>/dev/null | grep -E '(ups|nut|dummy|'"`basename "$0"`"')' | grep -vE '(ssh|startups|grep)' || true
    fi

    if isPidAlive "$PID_DUMMYUPS" \
    && { [ x"${TOP_SRCDIR}" != x ] && isPidAlive "$PID_DUMMYUPS1" && isPidAlive "$PID_DUMMYUPS2" \
         || [ x"${TOP_SRCDIR}" = x ] ; } \
    ; then
        # All drivers expected for this environment are already running
        log_info "Starting dummy-ups driver(s) for sandbox - all expected processes are running"
        return 0
    else
        log_error "Starting dummy-ups driver(s) for sandbox - finished, but something seems to not be running"
        return 1
    fi
}

testcase_sandbox_start_upsd_alone() {
    log_separator
    log_info "[testcase_sandbox_start_upsd_alone] Test starting UPSD but not a driver before it"
    sandbox_start_upsd

    EXPECTED_UPSLIST='dummy'
    if [ x"${TOP_SRCDIR}" != x ]; then
        EXPECTED_UPSLIST="$EXPECTED_UPSLIST
UPS1
UPS2"
        # For windows runners (strip CR if any):
        EXPECTED_UPSLIST="`echo "$EXPECTED_UPSLIST" | tr -d '\r'`"
    fi

    log_info "[testcase_sandbox_start_upsd_alone] Query listing from UPSD by UPSC (driver not running yet)"
    res_testcase_sandbox_start_upsd_alone=0
    runcmd upsc -l localhost:$NUT_PORT || die "[testcase_sandbox_start_upsd_alone] upsd does not respond on port ${NUT_PORT} ($?): $CMDOUT"
    # For windows runners (printf can do wonders, so strip CR if any):
    if [ x"${TOP_SRCDIR}" != x ]; then
        CMDOUT="`echo "$CMDOUT" | tr -d '\r'`"
    fi
    if [ x"$CMDOUT" != x"$EXPECTED_UPSLIST" ] ; then
        log_error "[testcase_sandbox_start_upsd_alone] got this reply for upsc listing when '$EXPECTED_UPSLIST' was expected: '$CMDOUT'"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_sandbox_start_upsd_alone"
        res_testcase_sandbox_start_upsd_alone=1
    else
        PASSED="`expr $PASSED + 1`"
    fi

    log_info "[testcase_sandbox_start_upsd_alone] Query driver state from UPSD by UPSC (driver not running yet)"
    runcmd upsc dummy@localhost:$NUT_PORT && {
        log_error "upsc was supposed to answer with error exit code: $CMDOUT"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_sandbox_start_upsd_alone"
        res_testcase_sandbox_start_upsd_alone=1
    }
    # Note: avoid exact matching for stderr, because it can have Init SSL messages etc.
    if echo "$CMDERR" | grep 'Error: Driver not connected' >/dev/null ; then
        PASSED="`expr $PASSED + 1`"
    else
        log_error "[testcase_sandbox_start_upsd_alone] got some other reply for upsc query when 'Error: Driver not connected' was expected on stderr: '$CMDOUT'"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_sandbox_start_upsd_alone"
        res_testcase_sandbox_start_upsd_alone=1
    fi

    if [ "$res_testcase_sandbox_start_upsd_alone" = 0 ]; then
        log_info "[testcase_sandbox_start_upsd_alone] PASSED: got just the failures expected for data server alone (driver not running yet)"
    else
        log_error "[testcase_sandbox_start_upsd_alone] got some unexpected failures, see above"
    fi

    return $res_testcase_sandbox_start_upsd_alone
}

testcase_sandbox_start_upsd_after_drivers() {
    # Historically this is a fallback from testcase_sandbox_start_drivers_after_upsd
    log_info "[testcase_sandbox_start_upsd_after_drivers] Test starting UPSD after drivers"
    kill -15 $PID_UPSD 2>/dev/null
    wait $PID_UPSD

    # Not calling upsd_start_loop() here, before drivers
    # If the server starts, fine; if not - we retry below
    upsd -F &
    PID_UPSD="$!"
    log_debug "[testcase_sandbox_start_upsd_after_drivers] Tried to start UPSD as PID $PID_UPSD"

    sandbox_start_drivers
    sandbox_start_upsd

    sleep 5

    COUNTDOWN=90
    while [ "$COUNTDOWN" -gt 0 ]; do
        # For query errors or known wait, keep looping
        runcmd upsc dummy@localhost:$NUT_PORT \
        && case "$CMDOUT" in
            *"ups.status: WAIT"*) ;;
            *) log_info "Got output:" ; echo "$CMDOUT" ; break ;;
        esac
        sleep 1
        COUNTDOWN="`expr $COUNTDOWN - 1`"
    done

    if [ "$COUNTDOWN" -le 88 ] ; then
        log_warn "[testcase_sandbox_start_upsd_after_drivers] Had to wait a few retries for the dummy driver to connect"
    fi

    if [ "$COUNTDOWN" -le 1 ] ; then
        report_NUT_PORT
        die "[testcase_sandbox_start_upsd_after_drivers] upsd does not respond on port ${NUT_PORT} ($?)"
    fi

    log_info "[testcase_sandbox_start_upsd_after_drivers] PASSED: upsd responds on port ${NUT_PORT}"
}

testcase_sandbox_start_drivers_after_upsd() {
    #sandbox_start_upsd
    testcase_sandbox_start_upsd_alone
    sandbox_start_drivers

    log_info "[testcase_sandbox_start_drivers_after_upsd] Query driver state from UPSD by UPSC after driver startup"
    # Timing issues: upsd starts, we wait 10 sec, drivers start and init,
    # at 20 sec upsd does not see them yet, at 30 sec the sockets connect
    # but info does not come yet => may be "driver stale", finally at
    # 40+(drv)/50+(upsd) sec a DUMPALL is processed (regular 30-sec loop?) -
    # so tightly near a minute until we have sturdy replies.
    COUNTDOWN=90
    while [ "$COUNTDOWN" -gt 0 ]; do
        # For query errors or known wait, keep looping. May get:
        #   driver.state: updateinfo
        #   ups.status: WAIT
        runcmd upsc dummy@localhost:$NUT_PORT \
        && case "$CMDOUT" in
            *"ups.status: WAIT"*) ;;
            *) log_info "[testcase_sandbox_start_drivers_after_upsd] Got output:" ; echo "$CMDOUT" ; break ;;
        esac
        sleep 1
        COUNTDOWN="`expr $COUNTDOWN - 1`"
    done

    if [ "$COUNTDOWN" -le 88 ] ; then
        log_warn "[testcase_sandbox_start_drivers_after_upsd] Had to wait a few retries for the dummy driver to connect"
    fi

    if [ "$COUNTDOWN" -le 1 ] ; then
        # Should not get to this, except on very laggy systems maybe
        log_error "[testcase_sandbox_start_drivers_after_upsd] Query failed, retrying with UPSD started after drivers"
        testcase_sandbox_start_upsd_after_drivers
    fi

    if [ x"${TOP_SRCDIR}" != x ]; then
        log_info "[testcase_sandbox_start_drivers_after_upsd] Wait for dummy UPSes with larger data sets to initialize"
        for U in UPS1 UPS2 ; do
            COUNTDOWN=90
            # TODO: Convert to runcmd()?
            OUT=""
            while [ x"$OUT" = x"ups.status: WAIT" ] ; do
                OUT="`upsc $U@localhost:$NUT_PORT ups.status`" || break
                [ x"$OUT" = x"ups.status: WAIT" ] || { log_info "[testcase_sandbox_start_drivers_after_upsd] Got output:"; echo "$OUT"; break; }
                sleep 1
                COUNTDOWN="`expr $COUNTDOWN - 1`"
                # Systemic error, e.g. could not create socket file?
                [ "$COUNTDOWN" -lt 1 ] && die "[testcase_sandbox_start_drivers_after_upsd] Dummy driver did not start or respond in time"
            done
            if [ "$COUNTDOWN" -le 88 ] ; then
                log_warn "[testcase_sandbox_start_drivers_after_upsd] Had to wait a few retries for the $U driver to connect"
            fi
        done
    fi

    log_info "[testcase_sandbox_start_drivers_after_upsd] PASSED: Expected drivers are now responding via UPSD"
}

testcase_sandbox_upsc_query_model() {
    log_info "[testcase_sandbox_upsc_query_model] Query model from dummy device"
    runcmd upsc dummy@localhost:$NUT_PORT device.model || die "[testcase_sandbox_upsc_query_model] upsd does not respond on port ${NUT_PORT} ($?): $CMDOUT"
    if [ x"$CMDOUT" != x"Dummy UPS" ] ; then
        log_error "[testcase_sandbox_upsc_query_model] got this reply for upsc query when 'Dummy UPS' was expected: $CMDOUT"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_sandbox_upsc_query_model"
    else
        PASSED="`expr $PASSED + 1`"
        log_info "[testcase_sandbox_upsc_query_model] PASSED: got expected model from dummy device: $CMDOUT"
    fi
}

testcase_sandbox_upsc_query_bogus() {
    log_info "[testcase_sandbox_upsc_query_bogus] Query driver state from UPSD by UPSC for bogus info"
    runcmd upsc dummy@localhost:$NUT_PORT ups.bogus.value && {
        log_error "[testcase_sandbox_upsc_query_bogus] upsc was supposed to answer with error exit code: $CMDOUT"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_sandbox_upsc_query_bogus"
    }
    # Note: avoid exact matching for stderr, because it can have Init SSL messages etc.
    if echo "$CMDERR" | grep 'Error: Variable not supported by UPS' >/dev/null ; then
        PASSED="`expr $PASSED + 1`"
        log_info "[testcase_sandbox_upsc_query_bogus] PASSED: got expected reply to bogus query"
    else
        log_error "[testcase_sandbox_upsc_query_bogus] got some other reply for upsc query when 'Error: Variable not supported by UPS' was expected on stderr: stderr:'$CMDERR' / stdout:'$CMDOUT'"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_sandbox_upsc_query_bogus"
    fi
}

testcase_sandbox_upsc_query_timer() {
    log_separator
    log_info "[testcase_sandbox_upsc_query_timer] Test that dummy-ups TIMER action changes the reported state"
    # Driver is set up to flip ups.status every 5 sec, so check every 3
    # TODO: Any need to convert to runcmd()?
    OUT1="`upsc dummy@localhost:$NUT_PORT ups.status`" || die "[testcase_sandbox_upsc_query_timer] upsd does not respond on port ${NUT_PORT} ($?): $OUT1" ; sleep 3
    OUT2="`upsc dummy@localhost:$NUT_PORT ups.status`" || die "[testcase_sandbox_upsc_query_timer] upsd does not respond on port ${NUT_PORT} ($?): $OUT2"
    OUT3=""
    OUT4=""
    if [ x"$OUT1" = x"$OUT2" ]; then
        sleep 3
        OUT3="`upsc dummy@localhost:$NUT_PORT ups.status`" || die "[testcase_sandbox_upsc_query_timer] upsd does not respond on port ${NUT_PORT} ($?): $OUT3"
        if [ x"$OUT2" = x"$OUT3" ]; then
            sleep 3
            OUT4="`upsc dummy@localhost:$NUT_PORT ups.status`" || die "[testcase_sandbox_upsc_query_timer] upsd does not respond on port ${NUT_PORT} ($?): $OUT4"
        fi
    fi
    if echo "$OUT1$OUT2$OUT3$OUT4" | grep "OB" && echo "$OUT1$OUT2$OUT3$OUT4" | grep "OL" ; then
        log_info "[testcase_sandbox_upsc_query_timer] PASSED: ups.status flips over time"
        PASSED="`expr $PASSED + 1`"
    else
        log_error "[testcase_sandbox_upsc_query_timer] ups.status did not flip over time"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_sandbox_upsc_query_timer"
    fi
}

isTestablePython() {
    # We optionally make python module (if interpreter is found):
    if [ x"${TOP_BUILDDIR}" = x ] \
    || [ ! -x "${TOP_BUILDDIR}/scripts/python/module/test_nutclient.py" ] \
    ; then
        return 1
    fi
    PY_SHEBANG="`head -1 "${TOP_BUILDDIR}/scripts/python/module/test_nutclient.py"`"
    if [ x"${PY_SHEBANG}" = x"#!no" ] ; then
        return 1
    fi
    return 0
}

testcase_sandbox_python_without_credentials() {
    isTestablePython || return 0
    log_separator
    log_info "[testcase_sandbox_python_without_credentials] Call Python module test suite: PyNUT (NUT Python bindings) without login credentials"
    if ( unset NUT_USER || true
         unset NUT_PASS || true
        "${TOP_BUILDDIR}/scripts/python/module/test_nutclient.py"
    ) ; then
        log_info "[testcase_sandbox_python_without_credentials] PASSED: PyNUT did not complain"
        PASSED="`expr $PASSED + 1`"
    else
        log_error "[testcase_sandbox_python_without_credentials] PyNUT complained, check above"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_sandbox_python_without_credentials"
    fi
}

testcase_sandbox_python_with_credentials() {
    isTestablePython || return 0

    # That script says it expects data/evolution500.seq (as the UPS1 dummy)
    # but the dummy data does not currently let issue the commands and
    # setvars tested from python script.
    log_separator
    log_info "[testcase_sandbox_python_with_credentials] Call Python module test suite: PyNUT (NUT Python bindings) with login credentials"
    if (
        NUT_USER='admin'
        NUT_PASS="${TESTPASS_ADMIN}"
        export NUT_USER NUT_PASS
        "${TOP_BUILDDIR}/scripts/python/module/test_nutclient.py"
    ) ; then
        log_info "[testcase_sandbox_python_with_credentials] PASSED: PyNUT did not complain"
        PASSED="`expr $PASSED + 1`"
    else
        log_error "[testcase_sandbox_python_with_credentials] PyNUT complained, check above"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_sandbox_python_with_credentials"
    fi
}

testcase_sandbox_python_with_upsmon_credentials() {
    isTestablePython || return 0

    log_separator
    log_info "[testcase_sandbox_python_with_upsmon_credentials] Call Python module test suite: PyNUT (NUT Python bindings) with upsmon role login credentials"
    if (
        NUT_USER='dummy-admin'
        NUT_PASS="${TESTPASS_UPSMON_PRIMARY}"
        export NUT_USER NUT_PASS
        "${TOP_BUILDDIR}/scripts/python/module/test_nutclient.py"
    ) ; then
        log_info "[testcase_sandbox_python_with_upsmon_credentials] PASSED: PyNUT did not complain"
        PASSED="`expr $PASSED + 1`"
    else
        log_error "[testcase_sandbox_python_with_upsmon_credentials] PyNUT complained, check above"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_sandbox_python_with_upsmon_credentials"
    fi
}

testcases_sandbox_python() {
    isTestablePython || return 0
    testcase_sandbox_python_without_credentials
    testcase_sandbox_python_with_credentials
    testcase_sandbox_python_with_upsmon_credentials
}

####################################

isTestableCppNIT() {
    # We optionally make and here can run C++ client tests:
    if [ x"${TOP_BUILDDIR}" = x ] \
    || [ ! -x "${TOP_BUILDDIR}/tests/cppnit" ] \
    ; then
        log_warn "SKIP: ${TOP_BUILDDIR}/tests/cppnit: Not found"
        return 1
    fi
    return 0
}

testcase_sandbox_cppnit_without_creds() {
    isTestableCppNIT || return 0

    log_separator
    log_info "[testcase_sandbox_cppnit_without_creds] Call libnutclient test suite: cppnit without login credentials"
    if ( unset NUT_USER || true
         unset NUT_PASS || true
        "${TOP_BUILDDIR}/tests/cppnit"
    ) ; then
        log_info "[testcase_sandbox_cppnit_without_creds] PASSED: cppnit did not complain"
        PASSED="`expr $PASSED + 1`"
    else
        log_error "[testcase_sandbox_cppnit_without_creds] cppnit complained, check above"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_sandbox_cppnit_without_creds"
    fi
}

testcase_sandbox_cppnit_simple_admin() {
    isTestableCppNIT || return 0

    log_separator
    log_info "[testcase_sandbox_cppnit_simple_admin] Call libnutclient test suite: cppnit with login credentials: simple admin"
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
        log_info "[testcase_sandbox_cppnit_simple_admin] PASSED: cppnit did not complain"
        PASSED="`expr $PASSED + 1`"
    else
        log_error "[testcase_sandbox_cppnit_simple_admin] cppnit complained, check above"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_sandbox_cppnit_simple_admin"
    fi
}

testcase_sandbox_cppnit_upsmon_primary() {
    isTestableCppNIT || return 0

    log_separator
    log_info "[testcase_sandbox_cppnit_upsmon_primary] Call libnutclient test suite: cppnit with login credentials: upsmon-primary"
    if (
        NUT_USER='dummy-admin'
        NUT_PASS="${TESTPASS_UPSMON_PRIMARY}"
        NUT_PRIMARY_DEVICE='dummy'
        unset NUT_SETVAR_DEVICE
        export NUT_USER NUT_PASS NUT_PRIMARY_DEVICE
        "${TOP_BUILDDIR}/tests/cppnit"
    ) ; then
        log_info "[testcase_sandbox_cppnit_upsmon_primary] PASSED: cppnit did not complain"
        PASSED="`expr $PASSED + 1`"
    else
        log_error "[testcase_sandbox_cppnit_upsmon_primary] cppnit complained, check above"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_sandbox_cppnit_upsmon_primary"
    fi
}

testcase_sandbox_cppnit_upsmon_master() {
    isTestableCppNIT || return 0

    log_separator
    log_info "[testcase_sandbox_cppnit_upsmon_master] Call libnutclient test suite: cppnit with login credentials: upsmon-master"
    if (
        NUT_USER='dummy-admin-m'
        NUT_PASS="${TESTPASS_UPSMON_PRIMARY}"
        NUT_PRIMARY_DEVICE='dummy'
        unset NUT_SETVAR_DEVICE
        export NUT_USER NUT_PASS NUT_PRIMARY_DEVICE
        "${TOP_BUILDDIR}/tests/cppnit"
    ) ; then
        log_info "[testcase_sandbox_cppnit_upsmon_master] PASSED: cppnit did not complain"
        PASSED="`expr $PASSED + 1`"
    else
        log_error "[testcase_sandbox_cppnit_upsmon_master] cppnit complained, check above"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_sandbox_cppnit_upsmon_master"
    fi
}

testcases_sandbox_cppnit() {
    isTestableCppNIT || return 0
    testcase_sandbox_cppnit_without_creds
    testcase_sandbox_cppnit_upsmon_primary
    testcase_sandbox_cppnit_upsmon_master
    testcase_sandbox_cppnit_simple_admin
}

####################################

isTestableNutScanner() {
    # We optionally make and here can run nut-scanner (as NUT client)
    # tests, which tangentially tests the C client library:
    if [ x"${TOP_BUILDDIR}" = x ] \
    || [ ! -x "${TOP_BUILDDIR}/tools/nut-scanner/nut-scanner" ] \
    ; then
        log_warn "SKIP: ${TOP_BUILDDIR}/tools/nut-scanner/nut-scanner: Not found"
        return 1
    fi
    return 0
}

testcase_sandbox_nutscanner_list() {
    isTestableNutScanner || return 0

    log_separator
    log_info "[testcase_sandbox_nutscanner_list] Call libupsclient test suite: nut-scanner on localhost:${NUT_PORT}"
    log_info "[testcase_sandbox_nutscanner_list] Preparing LD_LIBRARY_PATH='${LD_LIBRARY_PATH_CLIENT}'"

    # Note: for some reason `LD_LIBRARY_PATH=... runcmd ...` loses it :\
    LD_LIBRARY_PATH="${LD_LIBRARY_PATH_CLIENT}"
    export LD_LIBRARY_PATH

    # NOTE: Currently mask mode is IPv4 only
    runcmd "${TOP_BUILDDIR}/tools/nut-scanner/nut-scanner" -m 127.0.0.1/32 -O -p "${NUT_PORT}" \
    && test -n "$CMDOUT" \
    || runcmd "${TOP_BUILDDIR}/tools/nut-scanner/nut-scanner" -s localhost -O -p "${NUT_PORT}"

    LD_LIBRARY_PATH="${LD_LIBRARY_PATH_ORIG}"
    export LD_LIBRARY_PATH

    log_info "[testcase_sandbox_nutscanner_list] findings from nut-scanner:"
    echo "$CMDOUT"
    log_info "[testcase_sandbox_nutscanner_list] inspecting these findings from nut-scanner..."

    # Note: the reported "driver" string is not too helpful as a "nutclient".
    # In practice this could be a "dummy-ups" repeater or "clone" driver,
    # or some of the config elements needed for upsmon (lacking creds/role)
    # Also note that before PR #2247 nut-scanner returned "nutdev<NUM>"
    # section names, but now it returns "nutdev-<BUS><NUM>" to differentiate
    # the scanned buses (serial, snmp, usb, etc.)
    if (
        test -n "$CMDOUT" \
        && echo "$CMDOUT" | grep -E '^\[nutdev-nut1\]$' \
        && echo "$CMDOUT" | grep 'port = "dummy@' \
        || return

        if [ "${NUT_PORT}" = 3493 ] || [ x"$NUT_PORT" = x ]; then
            log_info "[testcase_sandbox_nutscanner_list] Note: not testing for suffixed port number" >&2
        else
            echo "$CMDOUT" | grep -E 'dummy@.*'":${NUT_PORT}" \
            || {
                log_error "[testcase_sandbox_nutscanner_list] dummy@... not found" >&2
                return 1
            }
        fi

        if [ x"${TOP_SRCDIR}" = x ]; then
            log_info "[testcase_sandbox_nutscanner_list] Note: only testing one dummy device" >&2
        else
            echo "$CMDOUT" | grep -E '^\[nutdev-nut2\]$' \
            && echo "$CMDOUT" | grep 'port = "UPS1@' \
            && echo "$CMDOUT" | grep -E '^\[nutdev-nut3\]$' \
            && echo "$CMDOUT" | grep 'port = "UPS2@' \
            || {
                log_error "[testcase_sandbox_nutscanner_list] something about UPS1/UPS2 not found" >&2
                return 1
            }
        fi

        if [ x"${TOP_SRCDIR}" = x ]; then
            PORTS_WANT=1
        else
            PORTS_WANT=3
        fi
        PORTS_SEEN="`echo "$CMDOUT" | grep -Ec 'port *='`"

        if [ "$PORTS_WANT" != "$PORTS_SEEN" ]; then
            log_error "[testcase_sandbox_nutscanner_list] Too many 'port=' lines: want $PORTS_WANT != seen $PORTS_SEEN" >&2
            return 1
        fi
    ) >/dev/null ; then
        log_info "[testcase_sandbox_nutscanner_list] PASSED: nut-scanner found all expected devices"
        PASSED="`expr $PASSED + 1`"
    else
        if ( echo "$CMDERR" | grep -E "Cannot load NUT library.*libupsclient.*found.*NUT search disabled" ) ; then
            log_warn "[testcase_sandbox_nutscanner_list] SKIP: ${TOP_BUILDDIR}/tools/nut-scanner/nut-scanner: $CMDERR"
        else
            log_error "[testcase_sandbox_nutscanner_list] nut-scanner complained or did not return all expected data, check above"
            FAILED="`expr $FAILED + 1`"
            FAILED_FUNCS="$FAILED_FUNCS testcase_sandbox_nutscanner_list"
        fi
    fi
}

testcases_sandbox_nutscanner() {
    isTestableNutScanner || return 0
    testcase_sandbox_nutscanner_list
}

####################################

# TODO: Some upsmon tests?

####################################

testgroup_sandbox() {
    testcase_sandbox_start_drivers_after_upsd
    testcase_sandbox_upsc_query_model
    testcase_sandbox_upsc_query_bogus
    testcase_sandbox_upsc_query_timer
    testcases_sandbox_python
    testcases_sandbox_cppnit
    testcases_sandbox_nutscanner

    log_separator
    sandbox_forget_configs
}

testgroup_sandbox_python() {
    # Arrange for quick test iterations
    testcase_sandbox_start_drivers_after_upsd
    testcases_sandbox_python

    log_separator
    sandbox_forget_configs
}

testgroup_sandbox_cppnit() {
    # Arrange for quick test iterations
    testcase_sandbox_start_drivers_after_upsd
    testcases_sandbox_cppnit

    log_separator
    sandbox_forget_configs
}

testgroup_sandbox_cppnit_simple_admin() {
    # Arrange for quick test iterations
    testcase_sandbox_start_drivers_after_upsd
    testcase_sandbox_cppnit_simple_admin

    log_separator
    sandbox_forget_configs
}

testgroup_sandbox_nutscanner() {
    # Arrange for quick test iterations
    testcase_sandbox_start_drivers_after_upsd
    testcases_sandbox_nutscanner

    log_separator
    sandbox_forget_configs
}

################################################################

case "${NIT_CASE}" in
    isBusy_NUT_PORT) DEBUG=yes isBusy_NUT_PORT ;;
    cppnit) testgroup_sandbox_cppnit ;;
    python) testgroup_sandbox_python ;;
    nutscanner|nut-scanner) testgroup_sandbox_nutscanner ;;
    testcase_*|testgroup_*|testcases_*|testgroups_*)
        log_warn "========================================================"
        log_warn "You asked to run just a specific testcase* or testgroup*"
        log_warn "Be sure to have previously run with DEBUG_SLEEP and"
        log_warn "   have exported the NUT_PORT upsd is listening on!"
        log_warn "========================================================"
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
if [ -n "$FAILED_FUNCS" ]; then
    for F in $FAILED_FUNCS ; do echo "$F" ; done | sort | uniq -c
fi

# Allow to leave the sandbox daemons running for a while,
# to experiment with them interactively:
if [ -n "${DEBUG_SLEEP-}" ] ; then
    if [ "${DEBUG_SLEEP-}" -gt 0 ] ; then : ; else
        DEBUG_SLEEP=60
    fi

    log_separator
    log_info "Sleeping now as asked (for ${DEBUG_SLEEP} seconds starting `date -u`), so you can play with the driver and server running; hint: export NUT_PORT=$NUT_PORT"
    log_separator

    sleep "${DEBUG_SLEEP}"
    log_info "Sleep finished"
    log_separator
fi

stop_daemons

if [ "$PASSED" = 0 ] || [ "$FAILED" != 0 ] ; then
    die "Some test scenarios failed!"
else
    log_info "SUCCESS"
fi
