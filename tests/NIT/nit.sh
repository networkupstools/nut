#!/bin/sh

# NUT Integration Testing suite, assumes the codebase was built and
# arranges running of the binaries to test driver-client-server
# ability to start and their interactions.
#
# Note: currently it is a PoC-quality mess that gets the job done
# but could be refactored for better maintainability and generic
# approach. Part of the goal was to let this script set up the
# sandbox to run tests which could be defined in other files.
# To speed up this practical developer-testing aspect, you can just
# `make check-NIT-sandbox{,-devel}` (optionally with custom DEBUG_SLEEP).
#
# WARNING: Current working directory when starting the script should be
# the location where it may create temporary data (e.g. the BUILDDIR).
# Caller can export envvars to impact the script behavior, e.g.:
#	DEBUG_NIT=true	to print debug messages, running processes, etc.
#	DEBUG_SLEEP=60	to sleep after tests, with driver+server running
#	NUT_DEBUG_MIN=3	to set (minimum) debug level for drivers, upsd...
#	NUT_DEBUG_LEVEL_UPSSCHED=3	to set debug level for particular
#			NUT daemons or tools
#	NUT_PORT=12345	custom port for upsd to listen and clients to query
#	NUT_FOREGROUND_WITH_PID=true	default foregrounding is without
#			PID files, this option tells daemons to save them
#	TESTDIR=/tmp/nut-NIT	to propose a location for "etc" and "run"
#			mock locations (under some circumstances, the
#			script can anyway choose to `mktemp` another)
#	NUT_DEBUG_UPSMON_NOTIFY_SYSLOG=false	To *disable* upsmon
#			notifications spilling to the script's console.
#
# Common sandbox run for testing goes from NUT root build directory like:
#	DEBUG_SLEEP=600 NUT_PORT=12345 NIT_CASE=testcase_sandbox_start_drivers_after_upsd NUT_FOREGROUND_WITH_PID=true make check-NIT &
#
# NOTE: For systems where executables need an extension, like Windows,
# you may require an EXEEXT variable to be exported (e.g. by a Makefile);
# also on Windows the DLL shared libraries may have to be present in same
# directory as the executable "module" (or in the current working directory).
# Ability to run in cross-builds (e.g. NUT for Windows on Linux) may depend
# on binary support deployed in the run-time system ( Wine, WSL... on Linux).
#
# Design note: written with dumbed-down POSIX shell syntax, to
# properly work in whatever different OSes have (bash, dash,
# ksh, busybox sh...)
#
# Special considerations for starting the tests as "root":
# * See I_AM_ROOT evaluation and conditional uses in code below for the
#   most authoritative reference.
# * More liberal permissions to TESTDIR, STATEPATH and configuration files
#   than when a non-root user started the test suite right away and could
#   only constrain access to allow itself.
# * BUILTIN_RUN_AS_USER and BUILTIN_RUN_AS_GROUP envvars would be consulted
#   and if those accounts do not exist (e.g. in a packaging build root), a
#   different value like "nobody" or "nogroup" would be defaulted for test.
#
# Copyright
#	2022-2026 Jim Klimov <jimklimov+nut@gmail.com>
#
# License: GPLv2+

if [ -z "${BASH_VERSION-}" ] \
&& (command -v bash) \
&& [ x"${DEBUG_NONBASH-}" != xtrue ] \
; then
    # FIXME: detect and pass -x/-v options?
    echo "WARNING: Re-execing in BASH (export DEBUG_NONBASH=true to avoid)" >&2
    exec bash $0 "$@"
fi

if [ -n "${BASH_VERSION-}" ]; then
    eval `echo "set -o pipefail"`
fi

TZ=UTC
LANG=C
LC_ALL=C
export TZ LANG LC_ALL

NUT_QUIET_INIT_SSL="true"
export NUT_QUIET_INIT_SSL

NUT_QUIET_INIT_UPSNOTIFY="true"
export NUT_QUIET_INIT_UPSNOTIFY

# Avoid noise in syslog and OS console
NUT_DEBUG_SYSLOG="stderr"
export NUT_DEBUG_SYSLOG

# Stash the originally requested value so we can fiddle with envvars (if set)
# per daemon, like NUT_DEBUG_LEVEL_UPSD, NUT_DEBUG_LEVEL_UPSMON,
# NUT_DEBUG_LEVEL_UPSLOG, NUT_DEBUG_LEVEL_UPSSCHED, NUT_DEBUG_LEVEL_DRIVERS,
# or per tool, like NUT_DEBUG_LEVEL_UPSC (not in all cases),
# NUT_DEBUG_LEVEL_NUT_SCANNER
NUT_DEBUG_LEVEL_ORIG="${NUT_DEBUG_LEVEL-}"
# Whether from env or set in script per prog, let it be known to children:
export NUT_DEBUG_LEVEL

NUT_DEBUG_PID="true"
export NUT_DEBUG_PID

# Just keep upsdrvctl quiet if used in test builds or with the sandbox
NUT_QUIET_INIT_NDE_WARNING="true"
export NUT_QUIET_INIT_NDE_WARNING

ARG_FG="-F"
if [ x"${NUT_FOREGROUND_WITH_PID-}" = xtrue ] ; then ARG_FG="-FF" ; fi

# tools
[ -n "${GREP}" ] || { GREP="`command -v grep`" && [ x"${GREP}" != x ] || { echo "$0: FAILED to locate GREP tool" >&2 ; exit 1 ; } ; }
[ -n "${EGREP}" ] || { if ( [ x"`echo a | $GREP -E '(a|b)'`" = xa ] ) 2>/dev/null ; then EGREP="$GREP -E" ; else EGREP="`command -v egrep`" ; fi && [ x"${EGREP}" != x ] || { echo "$0: FAILED to locate EGREP tool" >&2 ; exit 1 ; } ; }

TABCHAR="`printf '\t'`"

log_separator() {
    echo "" >&2
    echo "================================" >&2
}

shouldDebug() {
    [ -n "$DEBUG" ] || [ -n "$DEBUG_NIT" ] || [ -n "$DEBUG_SLEEP" ]
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
    (netstat -an ; netstat -anp || sockstat -l) 2>/dev/null | ${GREP} -w "${NUT_PORT}" \
    || (lsof -i :"${NUT_PORT}") 2>/dev/null \
    || true

    [ -z "${PID_UPSD}" ] || log_info "UPSD was last known to start as PID ${PID_UPSD}"
}

isBusy_NUT_PORT() {
    # Try to say if current NUT_PORT is busy (0 = true)
    # or available (non-0 = false)
    [ -n "${NUT_PORT}" ] || return

    log_debug "isBusy_NUT_PORT() Trying to report if NUT_PORT=${NUT_PORT} is used"
    if [ -r /proc/net/tcp ] || [ -r /proc/net/tcp6 ]; then
        # Assume Linux - hex-encoded
        # IPv4:
        #   sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode
        #   0: 0100007F:EE48 00000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 48881 1 00000000ec238d02 100 0 0 10 0
        #   ^^^ 1.0.0.127 - note reversed byte order!
        # IPv6:
        #   sl  local_address                         remote_address                        st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode
        #   0: 00000000000000000000000000000000:1F46 00000000000000000000000000000000:0000 0A 00000000:00000000 00:00000000 00000000    33        0 37451 1 00000000fa3c0c15 100 0 0 10 0
        NUT_PORT_HEX="`printf '%04X' \"${NUT_PORT}\"`"
        NUT_PORT_HITS="`cat /proc/net/tcp /proc/net/tcp6 2>/dev/null | awk '{print $2}' | ${EGREP} \":${NUT_PORT_HEX}\$\"`" \
        && [ -n "$NUT_PORT_HITS" ] \
        && log_debug "isBusy_NUT_PORT() found that NUT_PORT=${NUT_PORT} is busy per /proc/net/tcp*" \
        && return 0

        # We had a way to check, and the way said port is available
        log_debug "isBusy_NUT_PORT() found that NUT_PORT=${NUT_PORT} is not busy per /proc/net/tcp*"
        return 1
    fi

    (netstat -an || sockstat -l || ss -tn || ss -n) 2>/dev/null | ${EGREP} "[:.]${NUT_PORT}(${TABCHAR}| |\$)" > /dev/null \
    && log_debug "isBusy_NUT_PORT() found that NUT_PORT=${NUT_PORT} is busy per netstat, sockstat or ss" \
    && return

    (lsof -i :"${NUT_PORT}") 2>/dev/null \
    && log_debug "isBusy_NUT_PORT() found that NUT_PORT=${NUT_PORT} is busy per lsof" \
    && return

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

    # Not busy... or no tools to confirm? (or no perms, or no lsof plugin)?
    if (command -v netstat || command -v sockstat || command -v ss || command -v lsof) 2>/dev/null >/dev/null ; then
        # at least one tool is present, so not busy
        log_debug "isBusy_NUT_PORT() found that NUT_PORT=${NUT_PORT} is not busy per netstat, sockstat, ss or lsof"
        return 1
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
execcmd() {
    # Help set up EXEEXT and logging, but allow use for backgrounded runs.
    # WARNING: uses `exec` and overrides NUT_DEBUG_LEVEL, so must be
    # called as sub-shelled (by pipe, amperesand, backticks, etc.)!
    # Do not "fix" this method to round parentheses, because the way
    # this is works just right for remembering CHILDPID="$!" and later
    # killing off the daemons.
    log_debug "execcmd: asked for: $@"
    CMDPROG=""
    case "$1" in
        upsc|*/upsc|upsc"${EXEEXT-}"|*/upsc"${EXEEXT-}")
            if [ -n "${NUT_DEBUG_LEVEL_UPSC-}" ]; then
                NUT_DEBUG_LEVEL="${NUT_DEBUG_LEVEL_UPSC}"
            fi
            ;;
        nut-scanner|*/nut-scanner|nut-scanner"${EXEEXT-}"|*/nut-scanner"${EXEEXT-}")
            if [ -n "${NUT_DEBUG_LEVEL_NUT_SCANNER-}" ]; then
                NUT_DEBUG_LEVEL="${NUT_DEBUG_LEVEL_NUT_SCANNER}"
            fi
            ;;
    esac

    case "$1" in
        *"${EXEEXT-}") CMDPROG="$1" ;;
        *)  if [ -x "$1" ] || (command -v "$1") >/dev/null 2>/dev/null ; then
                CMDPROG="$1"
            else
                if [ x"${EXEEXT-}" = x ] ; then
                    log_warn "Did not find '$1' via 'command -v', the call below may fail"
                    CMDPROG="$1"
                else
                    if [ -x "$1${EXEEXT-}" ] || (command -v "$1${EXEEXT-}") >/dev/null 2>/dev/null ; then
                        CMDPROG="$1${EXEEXT}"
                    else
                        log_warn "Did not find '$1' nor '$1${EXEEXT-}' via 'command -v', the call below may fail"
                        CMDPROG="$1"
                    fi
                fi
            fi
            ;;
    esac
    shift

    log_debug "execcmd: running:   ${CMDPROG} $@"
    exec "${CMDPROG}" "$@"
}

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

    (execcmd "$@" > "${NUT_STATEPATH}/runcmd.out" 2>"${NUT_STATEPATH}/runcmd.err") || CMDRES=$?
    NUT_DEBUG_LEVEL="${NUT_DEBUG_LEVEL_ORIG}"
    CMDOUT="`cat \"${NUT_STATEPATH}/runcmd.out\"`"
    CMDERR="`cat \"${NUT_STATEPATH}/runcmd.err\"`"

    [ "$RUNCMD_QUIET_OUT" = true ] || { [ -z "$CMDOUT" ] || echo "$CMDOUT" ; }
    [ "$RUNCMD_QUIET_ERR" = true ] || { [ -z "$CMDERR" ] || echo "$CMDERR" >&2 ; }

    return $CMDRES
}

# Note: current directory is assumed to be writeable for temporary
# data, e.g. the $(builddir) from the Makefile. Static resources
# from the source codebase are where the script resides, e.g.
# the $(srcdir) from the Makefile. If we are not in the source
# tree, tests would use binaries in PATH (e.g. packaged install).
# Note that assumptions about relative paths below may NOT FIT
# out-of-tree builds, like NUT for Windows under Linux, so we'd
# better trust envvars from `make`, if exported and available to us!
BUILDDIR=""
log_debug "Current working directory: '`pwd`'"
if [ x"${abs_builddir-}" != x ] ; then
    BUILDDIR="${abs_builddir-}"
    log_debug "Trying BUILDDIR='${BUILDDIR}' from make vars"
else
    if [ x"${builddir-}" != x ] ; then
        log_debug "Trying BUILDDIR='${builddir}' from make vars"
        BUILDDIR="`cd \"${builddir}\" && pwd`"
    fi
fi
if [ x"${BUILDDIR}" = x ] || [ ! -d "${BUILDDIR}" ] ; then
    BUILDDIR="`pwd`"
    log_info "Guessing BUILDDIR='${BUILDDIR}' from script location..."
else
    log_info "Using BUILDDIR='${BUILDDIR}' from make vars"
fi

TOP_BUILDDIR=""
if [ x"${abs_top_builddir-}" != x ] ; then
    TOP_BUILDDIR="${abs_top_builddir}"
    log_debug "Trying TOP_BUILDDIR='${TOP_BUILDDIR}' from make vars"
else
    if [ x"${top_builddir-}" != x ] ; then
        log_debug "Trying TOP_BUILDDIR='${top_builddir}' from make vars"
        TOP_BUILDDIR="`cd \"${top_builddir}\" && pwd`"
    fi
fi
if [ x"${TOP_BUILDDIR}" = x ] || [ ! -d "${TOP_BUILDDIR}" ] ; then
    case "${BUILDDIR}" in
        */tests/NIT)
            TOP_BUILDDIR="`cd \"${BUILDDIR}\"/../.. && pwd`" ;;
        *) log_info "Current directory '${BUILDDIR}' is not a .../tests/NIT" ;;
    esac
    log_info "Guessing TOP_BUILDDIR='${TOP_BUILDDIR}' from script location and/or BUILDDIR value..."
else
    log_info "Using TOP_BUILDDIR='${TOP_BUILDDIR}' from make vars"
fi
if test ! -w "${BUILDDIR}" ; then
    log_error "BUILDDIR='${BUILDDIR}' is not writeable, tests may fail below"
fi

SRCDIR=""
if [ x"${abs_srcdir-}" != x ] ; then
    SRCDIR="${abs_srcdir}"
    log_debug "Trying SRCDIR='${SRCDIR}' from make vars"
else
    if [ x"${srcdir-}" != x ] ; then
        log_debug "Trying SRCDIR='${srcdir}' from make vars"
        SRCDIR="`cd \"${srcdir}\" && pwd`"
    fi
fi
if [ x"${SRCDIR}" = x ] || [ ! -d "${SRCDIR}" ] ; then
    SRCDIR="`dirname \"$0\"`"
    SRCDIR="`cd \"$SRCDIR\" && pwd`"
    log_info "Guessing SRCDIR='${SRCDIR}' from script location..."
else
    log_info "Using SRCDIR='${SRCDIR}' from make vars"
fi

TOP_SRCDIR=""
if [ x"${abs_top_srcdir-}" != x ] ; then
    TOP_SRCDIR="${abs_top_srcdir}"
    log_debug "Trying TOP_SRCDIR='${TOP_SRCDIR}' from make vars"
else
    if [ x"${top_srcdir-}" != x ] ; then
        log_debug "Trying TOP_SRCDIR='${top_srcdir}' from make vars"
        TOP_SRCDIR="`cd \"${top_srcdir}\" && pwd`"
    fi
fi
if [ x"${TOP_SRCDIR}" = x ] || [ ! -d "${TOP_SRCDIR}" ] ; then
    case "${SRCDIR}" in
        */tests/NIT)
            TOP_SRCDIR="`cd \"${SRCDIR}\"/../.. && pwd`" ;;
        *) log_info "Script source directory '${SRCDIR}' is not a .../tests/NIT" ;;
    esac
    log_info "Guessing TOP_SRCDIR='${TOP_SRCDIR}' from script location and/or SRCDIR value..."
else
    log_info "Using TOP_SRCDIR='${TOP_SRCDIR}' from make vars"
fi

# Make these paths known to e.g. upsmon/upssched and handler scripts they call
export BUILDDIR TOP_BUILDDIR SRCDIR TOP_SRCDIR

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

log_info "Locating NUT programs to test:"
for PROG in upsd upsc dummy-ups upsdrvctl upsmon upslog upssched ; do
    (command -v ${PROG}) || (command -v ${PROG}${EXEEXT-}) || die "Useless setup: ${PROG} not found in PATH: ${PATH}"
done

PID_UPSD=""
PID_UPSMON=""
PID_UPSSCHED=""
PID_DUMMYUPS=""
PID_DUMMYUPS1=""
PID_DUMMYUPS2=""

WITH_SSL_CLIENT="`upsmon -Dh 2>&1 | grep 'Using NUT libupsclient library'`" || WITH_SSL_CLIENT="none"
# NOTE: Currently OpenSSL/NSS builds and codepaths are exclusive of each other!
# Interesting idea: build and test server with one and clients with the other...
# SIDE NOTE: As of NUT v2.8.5, it seems that only upsmon client cares about SSL!
case "${WITH_SSL_CLIENT}" in
    *"without SSL"*|none|"") WITH_SSL_CLIENT="none" ;;
    *OpenSSL*) WITH_SSL_CLIENT="OpenSSL" ;;
    *NSS*) WITH_SSL_CLIENT="NSS" ;;
    *) log_warn "Unexpected client SSL support reported, ignoring: ${WITH_SSL_CLIENT}" ; WITH_SSL_CLIENT="none" ;;
esac
log_info "Tested client binaries SSL support: ${WITH_SSL_CLIENT}"

WITH_SSL_SERVER="`upsd -Dh 2>&1 | grep 'NUT data server was built with'`" || WITH_SSL_SERVER="none"
WITH_SSL_SERVER_CLIVAL="none"
case "${WITH_SSL_SERVER}" in
    *"without client certificate validation"*) WITH_SSL_SERVER_CLIVAL="false" ;;
    *"with client certificate validation"*) WITH_SSL_SERVER_CLIVAL="true" ;;
esac
case "${WITH_SSL_SERVER}" in
    *"without SSL"*|none|"") WITH_SSL_SERVER="none" ;;
    *OpenSSL*) WITH_SSL_SERVER="OpenSSL" ;;
    *NSS*) WITH_SSL_SERVER="NSS" ;;
    *) log_warn "Unexpected server SSL support reported, ignoring: ${WITH_SSL_SERVER}" ; WITH_SSL_SERVER="none" ;;
esac
log_info "Tested server binaries SSL support: ${WITH_SSL_SERVER}"
log_info "Tested server binaries client certificate validation: ${WITH_SSL_SERVER_CLIVAL}"

case "${WITH_SSL_CLIENT}${WITH_SSL_SERVER}" in
    *NSS*)
        (command -v certutil) || {
            log_warn "NUT can use NSS, but needed third-party tooling was not found to produce the crypto credential stores"
            if [ x"${WITH_SSL_CLIENT}" = xNSS ] ; then WITH_SSL_CLIENT="none" ; fi
            if [ x"${WITH_SSL_SERVER}" = xNSS ] ; then WITH_SSL_SERVER="none" ; fi
        }
        ;;
esac

case "${WITH_SSL_CLIENT}${WITH_SSL_SERVER}" in
    *OpenSSL*)
        (command -v openssl) || {
            log_warn "NUT can use OpenSSL, but needed third-party tooling was not found to produce the crypto credential stores"
            if [ x"${WITH_SSL_CLIENT}" = xOpenSSL ] ; then WITH_SSL_CLIENT="none" ; fi
            if [ x"${WITH_SSL_SERVER}" = xOpenSSL ] ; then WITH_SSL_SERVER="none" ; fi
        }
        ;;
esac

TESTCERT_ROOTCA_NAME="NUT Mock Root CA"
TESTCERT_ROOTCA_PASS="VeryS@cur@1337"
TESTCERT_CLIENT_NAME="NIT upsmon"
TESTCERT_CLIENT_PASS="MyPasSw0rD"
TESTCERT_SERVER_NAME="NIT data server"
TESTCERT_SERVER_PASS="TestS@rv!"
# Continued below regarding setup of crypto material data files
# (and possibly skipping SSL tests if we fail this setup).

# Platforms vary in abilities to report this...
I_AM_NAME=""
get_my_user_name() {
    if [ x"${I_AM_NAME}" != x ]; then
        # Use cache
        echo "${I_AM_NAME}"
        return
    fi

    _ME="`whoami 2>/dev/null`" && [ x"${_ME}" != x ] \
    || { _ME="`ps -ef 2>/dev/null | grep -v grep | grep $$`" && [ x"${_ME}" != x ] ; } \
    || { _ME="`ps -xawwu 2>/dev/null | grep -v grep | grep $$`" && [ x"${_ME}" != x ] ; } \
    || _ME=""

    if [ x"${_ME}" != x ] ; then
        echo "${_ME}"
        return
    fi

    # TOTHINK: Fallback to get "my current user": touch a file and see who owns it?

    # Non-numeric (empty) stdout; non-successful exit code
    log_warn "Current user account name was not found"
    return 1
}

get_user_id() {
    if _ID="`id -u ${1-} 2>/dev/null`" \
        && [ "${_ID}" -ge 0 ] 2>/dev/null ; then echo "${_ID}"; return ; fi
    if _ID="`id ${1-} 2>/dev/null | sed -e 's,^.*uid=,,' -e 's,(.*$,,'`" \
        && [ "${_ID}" -ge 0 ] 2>/dev/null ; then echo "${_ID}"; return ; fi
    if [ x"${1-}" != x ] 2>/dev/null && _ID="`getent passwd \"$1\" 2>/dev/null | awk -F: '{print $3}'`" \
        && [ "${_ID}" -ge 0 ] 2>/dev/null ; then echo "${_ID}"; return ; fi

    # Fallback
    if [ x"${1-}" = x ] 2>/dev/null ; then
        if _ME="`get_my_user_name`" 2>/dev/null && [ x"${_ME}" != x ] ; then
            _RES=0
            get_user_id "${_ME}" || _RES=$?
            return ${_RES}
        fi
    fi

    # Non-numeric (empty) stdout; non-successful exit code
    log_warn "User ID for name '${1-}' was not found"
    return 1
}

get_group_id() {
    if _ID="`id -g ${1-} 2>/dev/null`" \
        && [ "${_ID}" -ge 0 ] 2>/dev/null ; then echo "${_ID}"; return ; fi
    if _ID="`id ${1-} 2>/dev/null | sed -e 's,^.*gid=,,' -e 's,(.*$,,'`" \
        && [ "${_ID}" -ge 0 ] 2>/dev/null ; then echo "${_ID}"; return ; fi
    if [ x"${1-}" != x ] 2>/dev/null && _ID="`getent group \"$1\" 2>/dev/null | awk -F: '{print $3}'`" \
        && [ "${_ID}" -ge 0 ] 2>/dev/null ; then echo "${_ID}"; return ; fi

    # TOTHINK: Fallback to get "my current group": touch a file and see who owns it?

    # Non-numeric (empty) stdout; non-successful exit code
    case "`uname -a | tr 'A-Z' 'a-z'`" in
        *mingw*|*msys*|*win*)
            # Windows? Avoid "not a number" error messages,
            # common Unix groups are unlikely here...
            echo "-1"
            ;;
    esac

    log_warn "Group ID for name '${1-}' was not found"
    return 1
}

I_AM_NAME="`get_my_user_name`"
I_AM_NAME_REPORT="'${I_AM_NAME}'"
I_AM_ROOT=false
if [ "`get_user_id`" = 0 ] ; then
    if [ x"${I_AM_NAME}" = x ]; then
        log_warn "Seems we have started as UID 0, but could not detect user name: assuming 'root'"
    else if [ x"${I_AM_NAME}" != xroot ]; then
        log_warn "Seems we have started as UID 0, but detected user name was not 'root': '${I_AM_NAME}'"
        I_AM_NAME_REPORT="'${I_AM_NAME}' (root?)"
    fi; fi
    I_AM_ROOT=true
fi

# We have a certain problem when the requested user account does not exist:
# our `common::get_user_pwent()` aborts with `fatalx()` during such a check for
# validity of built-in or CLI-requested account (sometimes before even looking
# at any config files), and fails like "OS user upsd not found". Note we do not
# currently have similar troubles for groups (and interactions with customizing
# them seem limited to `chgrp()` of driver-upsd local socket).
# While this should not happen in real life, it can on test or packaging
# systems that did not provision the accounts (claimed by a NUT build) for
# the build/test run-time environment. In this case, tweak it into something
# we know exists (current non-root user explicitly, or a commonly available
# name if we are running as root).
TWEAK_RUN_AS_USER=""
TWEAK_RUN_AS_GROUP=""
ARG_USER=""
if [ x"${BUILTIN_RUN_AS_USER}" != x ] ; then
    if [ "`get_user_id \"${BUILTIN_RUN_AS_USER}\"`" -ge 0 ] 2>/dev/null; then
        # Do not bother to re-evaluate more IDs to rule out aliases - many names on same ID?
        if $I_AM_ROOT || [ x"${I_AM_NAME}" = x"${BUILTIN_RUN_AS_USER}" ] ; then
            log_info "Started as ${I_AM_NAME_REPORT}, and built-in RUN_AS_USER='${BUILTIN_RUN_AS_USER}' seems present on this system to run test daemons as"
        else
            log_info "Started as ${I_AM_NAME_REPORT}, and built-in RUN_AS_USER='${BUILTIN_RUN_AS_USER}' seems present on this system (but we would run test daemons as current unprivileged user)"
        fi
    else
        # The string allegedly built into NUT binaries is unknown to the
        # account identification databases of this runtime environment...
        if $I_AM_ROOT ; then
            for U in nobody daemon bin ; do
                if [ "`get_user_id \"${U}\"`" -ge 0 ] ; then
                    TWEAK_RUN_AS_USER="${U}"
                    break
                fi
            done
        else
            TWEAK_RUN_AS_USER="${I_AM_NAME}"
        fi

        if [ x"${TWEAK_RUN_AS_USER}" = x ] ; then
            log_warn "Started as ${I_AM_NAME_REPORT}, and built-in RUN_AS_USER='${BUILTIN_RUN_AS_USER}' seems absent on this system, and did not find a common alternative to run test daemons as; NIT suite can fail below!"
        else
            log_warn "Started as ${I_AM_NAME_REPORT}, and built-in RUN_AS_USER='${BUILTIN_RUN_AS_USER}' seems absent on this system, will run test daemons as '${TWEAK_RUN_AS_USER}'"
            # Needed e.g. for upsd, its config has no RUN_AS_USER setting
            ARG_USER="-u ${TWEAK_RUN_AS_USER}"
        fi
    fi
fi

if [ x"${BUILTIN_RUN_AS_GROUP}" != x ] ; then
    # Note: GID setting is not much used in NUT code at the moment:
    # * In `drivers/main.c` we can set FS access for the pipe to data server
    # * Otherwise in `common.c::become_user()` we try to assume the default
    #   GID of that user account we were asked to switch into.
    if [ "`get_group_id \"${BUILTIN_RUN_AS_GROUP}\"`" -ge 0 ] 2>/dev/null ; then
        # Do not bother to re-evaluate more IDs to rule out aliases - many names on same ID?
        # TOTHINK: Would need I_AM_GROUP first?..
        if $I_AM_ROOT ; then
            log_info "Started as ${I_AM_NAME_REPORT}, and built-in RUN_AS_GROUP='${BUILTIN_RUN_AS_GROUP}' seems present on this system to run test daemons as"
        else
            log_info "Started as ${I_AM_NAME_REPORT}, and built-in RUN_AS_GROUP='${BUILTIN_RUN_AS_GROUP}' seems present on this system (but we would run test daemons as current unprivileged user and untweaked group)"
        fi
    else
        # The string allegedly built into NUT binaries is unknown to the
        # account identification databases of this runtime environment...
        for G in nobody nogroup daemon bin ; do
            if [ "`get_group_id \"${G}\"`" -ge 0 ] ; then
                TWEAK_RUN_AS_GROUP="${G}"
                break
            fi
        done

        if [ x"${TWEAK_RUN_AS_GROUP}" = x ] ; then
            log_warn "Started as ${I_AM_NAME_REPORT}, and built-in RUN_AS_GROUP='${BUILTIN_RUN_AS_GROUP}' seems absent on this system, and did not find a common alternative to run test daemons as; NIT suite can fail below!"
        else
            log_warn "Started as ${I_AM_NAME_REPORT}, and built-in RUN_AS_GROUP='${BUILTIN_RUN_AS_GROUP}' seems absent on this system, will run test daemons as '${TWEAK_RUN_AS_GROUP}'"
        fi
    fi
fi

# Stash it for some later decisions
TESTDIR_CALLER="${TESTDIR-}"
[ -n "${TESTDIR-}" ] || TESTDIR="$BUILDDIR/tmp"
if [ `echo "${TESTDIR}" | wc -c` -gt 80 ]; then
    # Technically the limit is sizeof(sockaddr.sun_path) for complete socket
    # pathname, which varies 104-108 chars max on systems seen in CI farm;
    # we reserve 17 chars for "/dummy-ups-dummy" longest filename.
    log_info "'${TESTDIR}' is too long to store AF_UNIX socket files, will mktemp"
    TESTDIR=""
else
    if $I_AM_ROOT ; then
        case "${TESTDIR}" in
            "${HOME}"/*)
                log_info "Test script was started by ${I_AM_NAME_REPORT} and '${TESTDIR}' seems to be under its home, will mktemp so unprivileged daemons may access their configs, pipes and PID files"
                TESTDIR=""
                ;;
        esac
    else
        if mkdir -p "${TESTDIR}" && [ -w "${TESTDIR}" ] ; then
            true
        else
            log_info "'${TESTDIR}' could not be created/used, will mktemp"
            TESTDIR=""
        fi
    fi
fi

if [ x"${TESTDIR}" = x ] ; then
    if ( [ -n "${TMPDIR-}" ] && [ -d "${TMPDIR-}" ] && [ -w "${TMPDIR-}" ] ) ; then
        :
    else
        if [ -d /dev/shm ] && [ -w /dev/shm ]; then TMPDIR=/dev/shm ; else TMPDIR=/tmp ; fi
    fi
    log_warn "Will now mktemp a TESTDIR under '${TMPDIR}'. It will be wiped when the NIT script exits."
    log_warn "If you want a pre-determined location, pre-export a usable TESTDIR value."
    TESTDIR="`mktemp -d \"${TMPDIR}/nit-tmp.$$.XXXXXX\"`" || die "Failed to mktemp"
    if $I_AM_ROOT ; then
        # Cah be protected as 0700 by default
        chmod ugo+rx "${TESTDIR}"
    fi
else
    NUT_CONFPATH="${TESTDIR}/etc"
    if [ -f "${NUT_CONFPATH}/NIT.env-sandbox-ready" ] ; then
        log_warn "'${NUT_CONFPATH}/NIT.env-sandbox-ready' exists, do you have another instance of the script still running with same configs?"
        sleep 3
    fi

    # NOTE: Keep in sync with NIT_CASE handling and the exit-trap below
    case "${NIT_CASE}" in
        generatecfg_*|is*) ;;
        *)
            rm -rf "${TESTDIR}" || true
            ;;
    esac
fi
log_info "Using '$TESTDIR' for generated configs and state files"
if [ x"${TESTDIR_CALLER}" = x"${TESTDIR}" ] && [ x"${TESTDIR}" != x"$BUILDDIR/tmp" ] ; then
    log_warn "A caller-provided location is used, it would not be automatically removed after tests nor by 'make clean'"
fi

mkdir -p "${TESTDIR}/etc" "${TESTDIR}/run" && chmod 750 "${TESTDIR}/run" \
|| die "Failed to create temporary FS structure for the NIT"

if $I_AM_ROOT ; then
    log_info "Test script was started by ${I_AM_NAME_REPORT} - expanding permissions for '${TESTDIR}/run' so unprivileged daemons may create pipes and PID files there"
    chmod 777 "${TESTDIR}/run"
fi

stop_daemons() {
    if [ -n "$PID_UPSMON" ] ; then
        log_info "Stopping test daemons: upsmon via command"
        upsmon -c stop
    fi

    if [ -z "$PID_UPSSCHED" ] && [ -s "$NUT_PIDPATH/upssched.pid" ] ; then
        PID_UPSSCHED="`head -1 \"$NUT_PIDPATH/upssched.pid\"`"
    fi

    if [ -s "$NUT_PIDPATH/upssched.pid" ] ; then
        PID_UPSSCHED_NOW="`head -1 \"$NUT_PIDPATH/upssched.pid\"`"
    fi

    if [ -n "$PID_UPSD$PID_UPSMON$PID_DUMMYUPS$PID_DUMMYUPS1$PID_DUMMYUPS2$PID_UPSSCHED$PID_UPSSCHED_NOW" ] ; then
        log_info "Stopping test daemons"
        kill -15 $PID_UPSD $PID_UPSMON $PID_DUMMYUPS $PID_DUMMYUPS1 $PID_DUMMYUPS2 $PID_UPSSCHED $PID_UPSSCHED_NOW 2>/dev/null || return 0
        wait $PID_UPSD $PID_UPSMON $PID_DUMMYUPS $PID_DUMMYUPS1 $PID_DUMMYUPS2 $PID_UPSSCHED $PID_UPSSCHED_NOW || true
    fi

    PID_UPSD=""
    PID_UPSMON=""
    PID_UPSSCHED=""
    PID_DUMMYUPS=""
    PID_DUMMYUPS1=""
    PID_DUMMYUPS2=""

    unset PID_UPSSCHED_NOW
}

trap 'RES=$?; case "${NIT_CASE}" in generatecfg_*|is*) ;; *) stop_daemons; if [ x"${TESTDIR}" != x"${BUILDDIR}/tmp" ] && [ x"${TESTDIR}" != x"$TESTDIR_CALLER" ] ; then rm -rf "${TESTDIR}" ; else rm -f "${NUT_CONFPATH}/NIT.env-sandbox-ready" ; fi ;; esac; exit $RES;' 0 1 2 3 15

NUT_STATEPATH="${TESTDIR}/run"
NUT_PIDPATH="${TESTDIR}/run"
NUT_ALTPIDPATH="${TESTDIR}/run"
NUT_CONFPATH="${TESTDIR}/etc"
export NUT_STATEPATH NUT_PIDPATH NUT_ALTPIDPATH NUT_CONFPATH

if [ -f "${NUT_CONFPATH}/NIT.env-sandbox-ready" ] ; then
    log_warn "'${NUT_CONFPATH}/NIT.env-sandbox-ready' exists, do you have another instance of the script still running?"
    sleep 3
fi
rm -f "${NUT_CONFPATH}/NIT.env-sandbox-ready" || true

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
        COUNTDOWN="`expr \"$COUNTDOWN\" - 1`"

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
            COUNTDOWN="`expr \"$COUNTDOWN\" - 1`"
        done

        if [ "$COUNTDOWN" = 0 ] ; then
            die "Can not pick random port"
        fi
    fi
fi
export NUT_PORT
# Help track collisions in log, if someone else starts a test in same directory
log_info "Using NUT_PORT=${NUT_PORT} for this test run"

# SSL preparations: Can only do this after we learn NUT_CONFPATH
TESTCERT_PATH_ROOTCA="${NUT_CONFPATH}/cert/rootca"
TESTCERT_PATH_SERVER="${NUT_CONFPATH}/cert/upsd"
# TOTHINK: If other NUT clients get SSL support tested,
# should they use same or different cryptostore?..
TESTCERT_PATH_CLIENT="${NUT_CONFPATH}/cert/upsmon"

# Follow docs/security.txt points about setting up the crypto material
# stores and their contents (mock a self-signed CA here where appropriate)
case "${WITH_SSL_CLIENT}${WITH_SSL_SERVER}" in
    *OpenSSL*|*NSS*)
        ( # Sub-shelling here to keep soft failure cases handled once,
          # and changes of directory constrained without pushd/popd
          # (not in all shells) or remembering of `pwd` (clumsy-ish)
            log_info "Setting up crypto material storage for SSL capability tests..."

            if shouldDebug ; then
                set -x
            fi
            set -e

            mkdir -p "${TESTCERT_PATH_ROOTCA}"
            (   cd "${TESTCERT_PATH_ROOTCA}"
                log_info "SSL: Preparing test Root CA..."
                echo "${TESTCERT_ROOTCA_PASS}" > ".pwfile"
                case "${WITH_SSL_CLIENT}${WITH_SSL_SERVER}" in
                    *NSS*)
                        { [ -e /dev/urandom ] && \
                            dd if=/dev/urandom of=.random bs=16 count=1
                        } || {
                            [ -e /dev/random ] && \
                                dd if=/dev/random of=.random bs=16 count=1
                        } || date > .random
                        # Create the certificate database:
                        certutil -N -d . -f .pwfile
                        # Generate a certificate for CA:
                        # HACK NOTE: The first "yes" is for "Is this a CA certificate [y/N]?" question,
                        # others default (empty) for possible other questions, e.g.
                        #   Enter the path length constraint, enter to skip [<0 for unlimited path]: >
                        #   Is this a critical extension [y/N]? :
                        (echo y; yes "") | certutil -S -d . -f .pwfile -n "${TESTCERT_ROOTCA_NAME}" -s "CN=${TESTCERT_ROOTCA_NAME},OU=Test,O=NIT,ST=StateOfChaos,C=US" -t "CT,," -x -2 -z .random
                        # Extract the CA certificate to be able to use or import it later:
                        certutil -L -d . -f .pwfile -n "${TESTCERT_ROOTCA_NAME}" -a -o rootca.pem
                        # Use this later for signing, move on to server/client requests...

                        ls -l "${TESTCERT_PATH_ROOTCA}"/*.db "${TESTCERT_PATH_ROOTCA}"/*.txt
                        ;;
                esac

                [ -s rootca.pem ] || \
                case "${WITH_SSL_CLIENT}${WITH_SSL_SERVER}" in
                    *OpenSSL*)
                        # Generate an AES encrypted private key:
                        openssl genrsa -aes256 -out rootca.key -passout file:.pwfile 4096
                        # Generate a certificate for CA using that key:
                        openssl req -x509 -new -nodes -key rootca.key -passin file:.pwfile -sha256 -days 1826 -out rootca.pem -subj "/CN=${TESTCERT_ROOTCA_NAME}/OU=Test/O=NIT/ST=StateOfChaos/C=US"
                        ;;
                esac

                ls -l "${TESTCERT_PATH_ROOTCA}"/rootca.pem
            )

            mkdir -p "${TESTCERT_PATH_SERVER}"
            (   cd "${TESTCERT_PATH_SERVER}"
                log_info "SSL: Preparing test server certificate..."
                echo "${TESTCERT_SERVER_PASS}" > ".pwfile"
                case "${WITH_SSL_SERVER}" in
                    NSS)
                        # Create the certificate database:
                        certutil -N -d . -f .pwfile
                        # Import the CA certificate, so users of this DB trust it:
                        certutil -A -d . -f .pwfile -n "${TESTCERT_ROOTCA_NAME}" -t "TC,," -a -i "${TESTCERT_PATH_ROOTCA}"/rootca.pem
                        # Create a server certificate request:
                        # NOTE: IRL Each run should have a separate random seed; for tests we cut a few corners!
                        certutil -R -d . -f .pwfile -s "CN=${TESTCERT_SERVER_NAME},OU=Test,O=NIT,ST=StateOfChaos,C=US" -a -o server.req -z "${TESTCERT_PATH_ROOTCA}"/.random

                        # Sign a certificate request with the CA certificate:
                        # HACK NOTE: "No" for "Is this a CA certificate" question, defaults for others
                        (echo n; yes "") | certutil -C -d "${TESTCERT_PATH_ROOTCA}" -f "${TESTCERT_PATH_ROOTCA}"/.pwfile -c "${TESTCERT_ROOTCA_NAME}" -a -i server.req -o server.crt -2 --extKeyUsage "serverAuth" --nsCertType sslServer

                        # Import the signed certificate into server database:
                        certutil -A -d . -f .pwfile -n "${TESTCERT_SERVER_NAME}" -a -i server.crt -t ",,"

                        ls -l "${TESTCERT_PATH_SERVER}"/*.db "${TESTCERT_PATH_SERVER}"/*.txt
                        ;;
                    OpenSSL)
                        # Create a server certificate request:
                        openssl req -new -nodes -out server.req -newkey rsa:4096 -passout file:.pwfile -keyout server.key -subj "/CN=${TESTCERT_SERVER_NAME}/OU=Test/O=NIT/ST=StateOfChaos/C=US"
                        cat > server.v3.ext << EOF
authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage = digitalSignature, nonRepudiation, keyEncipherment, dataEncipherment
subjectAltName = @alt_names
[alt_names]
DNS.1 = localhost
DNS.2 = localhost6
IP.1 = 127.0.0.1
IP.2 = ::1
EOF
                        # Sign a certificate request with the CA certificate:
                        (   cd "${TESTCERT_PATH_ROOTCA}"
                            openssl x509 -req -in "${TESTCERT_PATH_SERVER}/server.req" -passin file:.pwfile -CA rootca.pem -CAkey rootca.key -CAcreateserial -out "${TESTCERT_PATH_SERVER}/server.crt" -days 730 -sha256 -extfile "${TESTCERT_PATH_SERVER}/server.v3.ext"
                        )
                        cat server.crt "${TESTCERT_PATH_ROOTCA}"/rootca.pem server.key > upsd.pem

                        ls -l "${TESTCERT_PATH_SERVER}"/upsd.pem
                        ;;
                esac
            )

            mkdir -p "${TESTCERT_PATH_CLIENT}"
            (   cd "${TESTCERT_PATH_CLIENT}"
                case "${WITH_SSL_CLIENT}" in
                    NSS)
                        log_info "SSL: Preparing test client certificate..."
                        # Also create 3-file database of client key+cert store
                        echo "${TESTCERT_CLIENT_PASS}" > ".pwfile"
                        # Create the certificate database:
                        certutil -N -d . -f .pwfile
                        # Import the CA certificate, so users of this DB trust it:
                        certutil -A -d . -f .pwfile -n "${TESTCERT_ROOTCA_NAME}" -t "TC,," -a -i "${TESTCERT_PATH_ROOTCA}"/rootca.pem

                        # Import server cert into client database so we can trust it (CERTHOST directive):
                        # NOTE: Seems we must do this before requesting or signing the client cert,
                        # otherwise (if importing server cert after doing everything about the
                        # client one) we get an error:
                        #  certutil: could not decode certificate: SEC_ERROR_REUSED_ISSUER_AND_SERIAL:
                        #    You are attempting to import a cert with the same issuer/serial
                        #    as an existing cert, but that is not the same cert.
                        certutil -A -d . -f .pwfile -n "${TESTCERT_SERVER_NAME}" -a -i "${TESTCERT_PATH_SERVER}/server.crt" -t ",,"

                        # Create a client certificate request:
                        # NOTE: IRL Each run should have a separate random seed; for tests we cut a few corners!
                        certutil -R -d . -f .pwfile -s "CN=${TESTCERT_CLIENT_NAME},OU=Test,O=NIT,ST=StateOfChaos,C=US" -a -o client.req -z "${TESTCERT_PATH_ROOTCA}"/.random

                        # Sign a certificate request with the CA certificate:
                        # HACK NOTE: "No" for "Is this a CA certificate" question, defaults for others
                        (echo n; yes "") | certutil -C -d "${TESTCERT_PATH_ROOTCA}" -f "${TESTCERT_PATH_ROOTCA}"/.pwfile -c "${TESTCERT_ROOTCA_NAME}" -a -i client.req -o client.crt -2 --extKeyUsage "clientAuth" --nsCertType sslClient

                        # Import the signed certificate into client database:
                        certutil -A -d . -f .pwfile -n "${TESTCERT_CLIENT_NAME}" -a -i client.crt -t ",,"

                        ls -l "${TESTCERT_PATH_CLIENT}"/*.db "${TESTCERT_PATH_CLIENT}"/*.txt
                        ;;
                    OpenSSL)
                        # NOTE: No special keys for an OpenSSL client so far,
                        # it only checks/trusts a server (public data in a PEM file)
                        log_info "SSL: Exporting public data of server certificate for client use..."
                        cat "${TESTCERT_PATH_SERVER}"/server.crt "${TESTCERT_PATH_ROOTCA}"/rootca.pem > upsd-public.pem

                        ls -l "${TESTCERT_PATH_CLIENT}/upsd-public.pem"
                        ;;
                esac
            )
        ) || {
            log_warn "Something failed about setup of crypto credential stores, will skip SSL tests"
            WITH_SSL_CLIENT="none"
            WITH_SSL_SERVER="none"
        }
        ;;
esac

# This file is not used by the test code, it is an
# aid for "DEBUG_SLEEP=X" mode so the caller can
# quickly source needed values into their shell.
#
# WARNING: Variables in this file are not guaranteed
# to impact subsequent runs of this script (nit.sh),
# e.g. NUT_PORT may be inherited, but paths would not be!
#
# NOTE: `set` typically wraps the values in quotes,
# but (maybe?) not all shell implementations do.
# Just in case, we have a fallback with single quotes.
# FIXME: We assume no extra quoting or escaping in
# the values when fallback is used. If this is a
# problem on any platform (Win/Mac and spaces in
# paths?) please investigate and fix accordingly.
set | ${EGREP} '^(NUT_|TESTDIR|TESTCERT|LD_LIBRARY_PATH|DEBUG|PATH).*=' \
| while IFS='=' read K V ; do
    case "$K" in
        LD_LIBRARY_PATH_CLIENT|LD_LIBRARY_PATH_ORIG|PATH_*|NUT_PORT_*|TESTDIR_*)
            continue
            ;;
        DEBUG_SLEEP|PATH|LD_LIBRARY_PATH*) printf '### ' ;;
    esac

    case "$V" in
        '"'*'"'|"'"*"'")
            printf "%s=%s\nexport %s\n\n" "$K" "$V" "$K" ;;
        *)
            printf "%s='%s'\nexport %s\n\n" "$K" "$V" "$K" ;;
    esac
done > "$NUT_CONFPATH/NIT.env"
log_info "Populated environment variables for this run into $NUT_CONFPATH/NIT.env"

### upsd.conf: ##################################################

generatecfg_upsd_trivial() {
    # Populate the configs for the run
    cat > "$NUT_CONFPATH/upsd.conf" << EOF
STATEPATH "$NUT_STATEPATH"
LISTEN localhost $NUT_PORT
EOF
    [ $? = 0 ] || die "Failed to populate temporary FS structure for the NIT: upsd.conf"

    if $I_AM_ROOT ; then
        log_info "Test script was started by ${I_AM_NAME_REPORT} - expanding permissions for '$NUT_CONFPATH/upsd.conf' so unprivileged daemons (after de-elevation) may read it"
        # NOTE: No RUN_AS_USER in upsd.conf currently; tweaking via CLI args if needed
        chmod 644 "$NUT_CONFPATH/upsd.conf"
    else
        chmod 640 "$NUT_CONFPATH/upsd.conf"
    fi

    # Some systems listening on symbolic "localhost" actually
    # only bind to IPv6, and some (Python) client might resolve
    # IPv4 and fail its connection tests. Others fare well with
    # both addresses in one command.
    for LH in 127.0.0.1 '::1' ; do
        if (
           ( cat /etc/hosts || getent hosts ) | ${GREP} "$LH" \
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

generatecfg_upsd_add_SSL() {
    # May first call one of the above consumers of generatecfg_upsd_trivial()
    if [ ! -s "$NUT_CONFPATH/upsd.conf" ] ; then
        generatecfg_upsd_trivial
    fi

    if grep CERT "$NUT_CONFPATH/upsd.conf" >/dev/null ; then
        # Already configured for SSL
        return 0
    fi

    case "${WITH_SSL_SERVER}" in
        none) return 0;;
        OpenSSL)
            { cat << EOF
# OpenSSL CERTFILE: PEM file with data server cert, possibly the
# intermediate and root CA's, and finally corresponding private key
CERTFILE "${TESTCERT_PATH_SERVER}/upsd.pem"
EOF
            } >> "$NUT_CONFPATH/upsd.conf" \
            || die "Failed to populate temporary FS structure for the NIT: upsd.conf"
            ;;
        NSS)
            { cat << EOF
# NSS CERTPATH: Directory with 3-file database of cert/key store
CERTPATH "${TESTCERT_PATH_SERVER}"
CERTIDENT "${TESTCERT_SERVER_NAME}" "${TESTCERT_SERVER_PASS}"
EOF

              if [ x"${WITH_SSL_SERVER_CLIVAL}" = xtrue -a x"${WITH_SSL_CLIENT}" = xNSS ]; then
                cat << EOF
#  - 0 to not request to clients to provide any certificate
#  - 1 to require to all clients a certificate
#  - 2 to require to all clients a valid certificate
CERTREQUEST 2
EOF
              fi
            } >> "$NUT_CONFPATH/upsd.conf" \
            || die "Failed to populate temporary FS structure for the NIT: upsd.conf"
            ;;
    esac

    # FIXME: Check for old/new OS and libs to toggle this?
    # echo "DISABLE_WEAK_SSL true" >> "$NUT_CONFPATH/upsd.conf" \
    # || die "Failed to populate temporary FS structure for the NIT: upsd.conf"

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

    if $I_AM_ROOT ; then
        log_info "Test script was started by ${I_AM_NAME_REPORT} - expanding permissions for '$NUT_CONFPATH/upsd.users' so unprivileged daemons (after de-elevation) may read it"
        chmod 644 "$NUT_CONFPATH/upsd.users"
    else
        chmod 640 "$NUT_CONFPATH/upsd.users"
    fi
}

### upsmon.conf and upssched.conf: ##################################################

updatecfg_upsmon_supplies() {
    NUMSUP="${1-}"
    [ -n "$NUMSUP" ] && [ "$NUMSUP" -ge 0 ] || NUMSUP=1

    [ -s "$NUT_CONFPATH/upsmon.conf" ] \
    || die "Failed to rewrite config - not populated yet: upsmon.conf"

    # NOTE: "sed -i file" is not ubiquitously portable
    # Using "cat" to retain FS permissions maybe set above
    sed -e 's,^\(MINSUPPLIES\) [0-9][0-9]*$,\1 '"$NUMSUP"',' \
        -e 's,^\(MONITOR .*\) [0-9][0-9]* \(.*\)$,\1 '"$NUMSUP"' \2,' \
        < "$NUT_CONFPATH/upsmon.conf" > "$NUT_CONFPATH/upsmon.conf.tmp" \
        && ( cat "$NUT_CONFPATH/upsmon.conf.tmp" > "$NUT_CONFPATH/upsmon.conf" ) \
        && rm -f "$NUT_CONFPATH/upsmon.conf.tmp"
}

generatecfg_upsmon_trivial() {
    # Populate the configs for the run
    rm -f "$NUT_STATEPATH/upsmon.sd.log"
    (   echo 'MINSUPPLIES 0' > "$NUT_CONFPATH/upsmon.conf" || exit
        echo 'SHUTDOWNCMD "echo \"`date`: TESTING_DUMMY_SHUTDOWN_NOW\" | tee \"'"$NUT_STATEPATH"'/upsmon.sd.log\" "' >> "$NUT_CONFPATH/upsmon.conf" || exit

        NOTIFYTGT=""
        if [ -x "${TOP_SRCDIR-}/scripts/misc/notifyme-debug" ] ; then
            echo "NOTIFYCMD \"TEMPDIR='${NUT_STATEPATH}' ${TOP_SRCDIR-}/scripts/misc/notifyme-debug\"" >> "$NUT_CONFPATH/upsmon.conf" || exit

            # NOTE: "SYSLOG" typically ends up in console log of the NIT run and
            # "EXEC" goes to a log file like tests/NIT/tmp/run/notifyme-399.log
            NOTIFYTGT="EXEC"
        fi
        if [ x"$NUT_DEBUG_UPSMON_NOTIFY_SYSLOG" != xfalse ] ; then
            [ -n "${NOTIFYTGT}" ] && NOTIFYTGT="${NOTIFYTGT}+SYSLOG" || NOTIFYTGT="SYSLOG"
        fi

        if [ -n "${NOTIFYTGT}" ]; then
            if [ -s "${TOP_SRCDIR-}/conf/upsmon.conf.sample.in" ] ; then
                ${EGREP} '# NOTIFYFLAG .*SYSLOG\+WALL$' \
                < "${TOP_SRCDIR-}/conf/upsmon.conf.sample.in" \
                | sed 's,^# \(NOTIFYFLAG[^A-Z_]*[A-Z_]*\)[^A-Z_]*SYSLOG.*$,\1\t'"${NOTIFYTGT}"',' \
                >> "$NUT_CONFPATH/upsmon.conf" || exit
            fi
        fi

        if [ -n "${NUT_DEBUG_MIN-}" ] ; then
            echo "DEBUG_MIN ${NUT_DEBUG_MIN}" >> "$NUT_CONFPATH/upsmon.conf" || exit
        fi
    ) || die "Failed to populate temporary FS structure for the NIT: upsmon.conf"

    sed \
        -e 's,[@]TOP_BUILDDIR[@],'"${TOP_BUILDDIR}"',g' \
        -e 's,[@]TOP_SRCDIR[@],'"${TOP_SRCDIR}"',g' \
        -e 's,[@]NUT_CONFPATH[@],'"${NUT_CONFPATH}"',g' \
        -e 's,[@]NUT_STATEPATH[@],'"${NUT_STATEPATH}"',g' \
    < "${TOP_SRCDIR-}/tests/NIT/upssched.conf.in" > "$NUT_CONFPATH/upssched.conf" \
    || die "Failed to populate temporary FS structure for the NIT: upssched.conf"

    if $I_AM_ROOT ; then
        log_info "Test script was started by ${I_AM_NAME_REPORT} - expanding permissions for '$NUT_CONFPATH/upsmon.conf' and '$NUT_CONFPATH/upssched.conf' so unprivileged daemons (after de-elevation) may read them"
        chmod 644 "$NUT_CONFPATH/upsmon.conf" "$NUT_CONFPATH/upssched.conf"
    else
        chmod 640 "$NUT_CONFPATH/upsmon.conf" "$NUT_CONFPATH/upssched.conf"
    fi

    if [ x"${TWEAK_RUN_AS_USER}" != x ] ; then
        echo "RUN_AS_USER ${TWEAK_RUN_AS_USER}" >> "$NUT_CONFPATH/upsmon.conf"
    fi

    if [ $# -gt 0 ] ; then
        updatecfg_upsmon_supplies "$1"
    fi
}

generatecfg_upsmon_master() {
    generatecfg_upsmon_trivial
    echo "MONITOR \"dummy@localhost:$NUT_PORT\" 0 \"dummy-admin-m\" \"${TESTPASS_UPSMON_PRIMARY}\" master" >> "$NUT_CONFPATH/upsmon.conf" \
    || die "Failed to populate temporary FS structure for the NIT: upsmon.conf"

    if [ $# -gt 0 ] ; then
        updatecfg_upsmon_supplies "$1"
    fi
}

generatecfg_upsmon_primary() {
    generatecfg_upsmon_trivial
    echo "MONITOR \"dummy@localhost:$NUT_PORT\" 0 \"dummy-admin\" \"${TESTPASS_UPSMON_PRIMARY}\" primary" >> "$NUT_CONFPATH/upsmon.conf" \
    || die "Failed to populate temporary FS structure for the NIT: upsmon.conf"

    if [ $# -gt 0 ] ; then
        updatecfg_upsmon_supplies "$1"
    fi
}

generatecfg_upsmon_slave() {
    generatecfg_upsmon_trivial
    echo "MONITOR \"dummy@localhost:$NUT_PORT\" 0 \"dummy-user-s\" \"${TESTPASS_UPSMON_SECONDARY}\" slave" >> "$NUT_CONFPATH/upsmon.conf" \
    || die "Failed to populate temporary FS structure for the NIT: upsmon.conf"

    if [ $# -gt 0 ] ; then
        updatecfg_upsmon_supplies "$1"
    fi
}

generatecfg_upsmon_secondary() {
    generatecfg_upsmon_trivial
    echo "MONITOR \"dummy@localhost:$NUT_PORT\" 0 \"dummy-user\" \"${TESTPASS_UPSMON_SECONDARY}\" secondary" >> "$NUT_CONFPATH/upsmon.conf" \
    || die "Failed to populate temporary FS structure for the NIT: upsmon.conf"

    if [ $# -gt 0 ] ; then
        updatecfg_upsmon_supplies "$1"
    fi
}

generatecfg_upsmon_add_SSL() {
    # May first call one of the above consumers of generatecfg_upsmon_trivial()
    if [ ! -s "$NUT_CONFPATH/upsmon.conf" ] ; then
        generatecfg_upsmon_trivial
    fi

    if grep CERTPATH "$NUT_CONFPATH/upsmon.conf" >/dev/null ; then
        # Already configured for SSL
        return 0
    fi

    case "${WITH_SSL_CLIENT}" in
        none) return 0;;
        OpenSSL)
            { cat << EOF
# OpenSSL CERTPATH: Directory with PEM file(s), looked up by the
#  CA subject name hash value (which must include our NUT server).
#  Here we just use the path for PEM file that should be populated
#  by the generatecfg_upsd_add_SSL() method.
# We only support CERTPATH (to recognize servers), FORCESSL and
# CERTVERIFY in OpenSSL builds.
CERTPATH "${TESTCERT_PATH_CLIENT}"
EOF

              if [ x"${WITH_SSL_SERVER}" != xnone ] ; then
                cat << EOF
# With OpenSSL this is the only way to configure these behaviors,
# no CERTHOST setting here so far:
FORCESSL 1
CERTVERIFY 1
EOF
              fi
            } >> "$NUT_CONFPATH/upsmon.conf" \
            || die "Failed to populate temporary FS structure for the NIT: upsmon.conf"
            ;;
        NSS)
            { cat << EOF
# NSS CERTPATH: Directory with 3-file database of cert/key store
CERTPATH "${TESTCERT_PATH_CLIENT}"
CERTIDENT "${TESTCERT_CLIENT_NAME}" "${TESTCERT_CLIENT_PASS}"
EOF

              case "${WITH_SSL_SERVER}" in
                none) ;;
                NSS)
                    cat << EOF
CERTHOST localhost "${TESTCERT_SERVER_NAME}" 1 1
EOF
                ;;
                *)  # OpenSSL
                    cat << EOF
CERTHOST localhost "${TESTCERT_SERVER_NAME}" 0 0
EOF
                ;;
              esac

              if [ x"${WITH_SSL_SERVER}" != xnone ] ; then
                cat << EOF
# Defaults that NSS CERTHOST may override per-server, but
# note that this impacts also the general upsmon behavior:
FORCESSL 1
CERTVERIFY 1
EOF
              fi
            } >> "$NUT_CONFPATH/upsmon.conf" \
            || die "Failed to populate temporary FS structure for the NIT: upsmon.conf"
            ;;
    esac
}

### ups.conf: ##################################################

generatecfg_ups_trivial() {
    # Populate the configs for the run
    (   echo 'maxretry = 3' > "$NUT_CONFPATH/ups.conf" || exit
        if [ x"${ABS_TOP_BUILDDIR}" != x ]; then
            # NOTE: Windows backslashes are pre-escaped in the configure-generated value
            case "${ABS_TOP_BUILDDIR}" in
                ?":\\"*) PATHSEP='\\' ;;
                *) PATHSEP="/" ;;
            esac
            echo "driverpath = \"${ABS_TOP_BUILDDIR}${PATHSEP}drivers\"" >> "$NUT_CONFPATH/ups.conf" || exit
        else
            # NOTE: Escaping presumed needed below, so for PATHSEP too
            if [ x"${TOP_BUILDDIR}" != x ]; then
                case "${TOP_BUILDDIR}" in
                    ?":\\"*) PATHSEP='\' ;;
                    *) PATHSEP="/" ;;
                esac
                echo "driverpath = \"${TOP_BUILDDIR}${PATHSEP}drivers\"" | sed 's,\\,\\\\,g' >> "$NUT_CONFPATH/ups.conf" || exit
            fi
        fi
        unset PATHSEP
        if [ -n "${NUT_DEBUG_MIN-}" ] ; then
            echo "debug_min = ${NUT_DEBUG_MIN}" >> "$NUT_CONFPATH/ups.conf" || exit
        fi
    ) || die "Failed to populate temporary FS structure for the NIT: ups.conf"

    if $I_AM_ROOT ; then
        log_info "Test script was started by ${I_AM_NAME_REPORT} - expanding permissions for '$NUT_CONFPATH/ups.conf' so unprivileged daemons (after de-elevation) may read it"
        chmod 644 "$NUT_CONFPATH/ups.conf"
    else
        chmod 640 "$NUT_CONFPATH/ups.conf"
    fi

    if [ x"${TWEAK_RUN_AS_USER}" != x ] ; then
        echo "user ${TWEAK_RUN_AS_USER}" >> "$NUT_CONFPATH/ups.conf"
    fi
    if [ x"${TWEAK_RUN_AS_GROUP}" != x ] ; then
        echo "group ${TWEAK_RUN_AS_GROUP}" >> "$NUT_CONFPATH/ups.conf"
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
            ${EGREP} '^ups.status:' "$F" >/dev/null || { echo "ups.status: OL BOOST" >> "$F"; }
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
    if [ -n "${NUT_DEBUG_LEVEL_UPSD-}" ]; then
        NUT_DEBUG_LEVEL="${NUT_DEBUG_LEVEL_UPSD}"
    fi
    (execcmd upsd ${ARG_FG} ${ARG_USER})
    if [ "$?" = 0 ]; then
        log_error "[testcase_upsd_no_configs_at_all] upsd should fail without configs"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_upsd_no_configs_at_all"
    else
        log_info "[testcase_upsd_no_configs_at_all] PASSED: upsd failed to start in wrong conditions"
        PASSED="`expr $PASSED + 1`"
    fi
    NUT_DEBUG_LEVEL="${NUT_DEBUG_LEVEL_ORIG}"
}

testcase_upsd_no_configs_driver_file() {
    log_separator
    log_info "[testcase_upsd_no_configs_driver_file] Test UPSD without driver config file"
    generatecfg_upsd_trivial
    if [ -n "${NUT_DEBUG_LEVEL_UPSD-}" ]; then
        NUT_DEBUG_LEVEL="${NUT_DEBUG_LEVEL_UPSD}"
    fi
    (execcmd upsd ${ARG_FG} ${ARG_USER})
    if [ "$?" = 0 ]; then
        log_error "[testcase_upsd_no_configs_driver_file] upsd should fail without driver config file"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_upsd_no_configs_driver_file"
    else
        log_info "[testcase_upsd_no_configs_driver_file] PASSED: upsd failed to start in wrong conditions"
        PASSED="`expr $PASSED + 1`"
    fi
    NUT_DEBUG_LEVEL="${NUT_DEBUG_LEVEL_ORIG}"
}

testcase_upsd_no_configs_in_driver_file() {
    log_separator
    log_info "[testcase_upsd_no_configs_in_driver_file] Test UPSD without drivers defined in config file"
    generatecfg_upsd_trivial
    generatecfg_ups_trivial
    if [ -n "${NUT_DEBUG_LEVEL_UPSD-}" ]; then
        NUT_DEBUG_LEVEL="${NUT_DEBUG_LEVEL_UPSD}"
    fi
    (execcmd upsd ${ARG_FG} ${ARG_USER})
    if [ "$?" = 0 ]; then
        log_error "[testcase_upsd_no_configs_in_driver_file] upsd should fail without drivers defined in config file"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_upsd_no_configs_in_driver_file"
    else
        log_info "[testcase_upsd_no_configs_in_driver_file] PASSED: upsd failed to start in wrong conditions"
        PASSED="`expr $PASSED + 1`"
    fi
    NUT_DEBUG_LEVEL="${NUT_DEBUG_LEVEL_ORIG}"
}

upsd_start_loop() {
    TESTCASE="${1-upsd_start_loop}"

    if isPidAlive "$PID_UPSD" ; then
        return 0
    fi

    if [ -n "${NUT_DEBUG_LEVEL_UPSD-}" ]; then
        NUT_DEBUG_LEVEL="${NUT_DEBUG_LEVEL_UPSD}"
    fi
    execcmd upsd ${ARG_FG} ${ARG_USER} &
    PID_UPSD="$!"
    NUT_DEBUG_LEVEL="${NUT_DEBUG_LEVEL_ORIG}"
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
        if [ -n "${NUT_DEBUG_LEVEL_UPSD-}" ]; then
            NUT_DEBUG_LEVEL="${NUT_DEBUG_LEVEL_UPSD}"
        fi
        execcmd upsd ${ARG_FG} ${ARG_USER} &
        PID_UPSD="$!"
        NUT_DEBUG_LEVEL="${NUT_DEBUG_LEVEL_ORIG}"
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
            if echo "$CMDERR" | ${GREP} "Error: Server disconnected" >/dev/null ; then
                log_warn "[testcase_upsd_allow_no_device] Retry once to rule out laggy systems"
                sleep 3
                runcmd upsc -l localhost:$NUT_PORT
            fi
            if echo "$CMDERR" | ${GREP} "Error: Server disconnected" >/dev/null ; then
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

        log_separator
        log_info "[testcase_upsd_allow_no_device] Query JSON listing from UPSD by UPSC (no devices configured yet) to test that UPSD responds to UPSC"
        if runcmd upsc -j -l localhost:$NUT_PORT && test x"${CMDOUT}" != x ; then
            log_debug "[testcase_upsd_allow_no_device] got a JSON reply:" "$CMDOUT"
            JSTRIP="`echo \"${CMDOUT}\" | tr -d ' ' | tr -d '\n' | tr -d '\r'`"
            if test x"${JSTRIP}" = x'[]' ; then
                log_info "[testcase_upsd_allow_no_device] OK, empty-list JSON response as expected"
                PASSED="`expr $PASSED + 1`"
            else
                log_error "[testcase_upsd_allow_no_device] got a reply for upsc JSON listing for empty but running server, but it was not expected (not an empty list):" "$CMDOUT" "$CMDERR"
                FAILED="`expr $FAILED + 1`"
                FAILED_FUNCS="$FAILED_FUNCS testcase_upsd_allow_no_device"
                res_testcase_upsd_allow_no_device=1
            fi
        else
            log_error "[testcase_upsd_allow_no_device] did not get a reply for upsc JSON listing for empty but running server:" "$CMDOUT" "$CMDERR"
            FAILED="`expr $FAILED + 1`"
            FAILED_FUNCS="$FAILED_FUNCS testcase_upsd_allow_no_device"
            res_testcase_upsd_allow_no_device=1
        fi
        log_separator
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
    if [ -n "${NUT_DEBUG_LEVEL_DRIVERS-}" ]; then
        NUT_DEBUG_LEVEL="${NUT_DEBUG_LEVEL_DRIVERS}"
    fi
    # Run one driver instance indirectly, to test the upsdrvctl tool too:
    execcmd upsdrvctl ${ARG_FG} ${ARG_USER} start dummy &
    #execcmd dummy-ups -a dummy ${ARG_USER} ${ARG_FG} &
    PID_DUMMYUPS="$!"
    log_debug "Tried to start dummy-ups driver for 'dummy' as PID $PID_DUMMYUPS"

    if [ x"${TOP_SRCDIR}" != x ]; then
        execcmd dummy-ups -a UPS1 ${ARG_USER} ${ARG_FG} &
        PID_DUMMYUPS1="$!"
        log_debug "Tried to start dummy-ups driver for 'UPS1' as PID $PID_DUMMYUPS1"

        execcmd dummy-ups -a UPS2 ${ARG_USER} ${ARG_FG} &
        PID_DUMMYUPS2="$!"
        log_debug "Tried to start dummy-ups driver for 'UPS2' as PID $PID_DUMMYUPS2"
    fi
    NUT_DEBUG_LEVEL="${NUT_DEBUG_LEVEL_ORIG}"

    sleep 5

    if shouldDebug ; then
        (ps -ef || ps -xawwu) 2>/dev/null | ${EGREP} '(ups|nut|dummy|'"`basename \"$0\"`"')' | ${EGREP} -v '(ssh|startups|grep)' || true
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
        EXPECTED_UPSLIST="`echo \"$EXPECTED_UPSLIST\" | tr -d '\r'`"
    fi

    EXPECTED_UPSLIST_JSON='[
  "dummy"'
    if [ x"${TOP_SRCDIR}" != x ]; then
        EXPECTED_UPSLIST_JSON="${EXPECTED_UPSLIST_JSON},"'
  "UPS1",
  "UPS2"'
    fi
    EXPECTED_UPSLIST_JSON="${EXPECTED_UPSLIST_JSON}"'
]'
    # For windows runners (strip CR if any):
    EXPECTED_UPSLIST_JSON="`echo \"$EXPECTED_UPSLIST_JSON\" | tr -d '\r'`"

    log_info "[testcase_sandbox_start_upsd_alone] Query listing from UPSD by UPSC (driver not running yet)"
    res_testcase_sandbox_start_upsd_alone=0
    runcmd upsc -l localhost:$NUT_PORT || die "[testcase_sandbox_start_upsd_alone] upsd does not respond on port ${NUT_PORT} ($?): $CMDOUT"
    # For windows runners (printf can do wonders, so strip CR if any):
    if [ x"${TOP_SRCDIR}" != x ]; then
        CMDOUT="`echo \"$CMDOUT\" | tr -d '\r'`"
    fi
    if [ x"$CMDOUT" != x"$EXPECTED_UPSLIST" ] ; then
        log_error "[testcase_sandbox_start_upsd_alone] got this reply for upsc listing when '$EXPECTED_UPSLIST' was expected: '$CMDOUT'"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_sandbox_start_upsd_alone"
        res_testcase_sandbox_start_upsd_alone=1
    else
        PASSED="`expr $PASSED + 1`"
    fi

    runcmd upsc -j -l localhost:$NUT_PORT || die "[testcase_sandbox_start_upsd_alone] upsd does not respond on port ${NUT_PORT} or JSON listing failed ($?): $CMDOUT"
    if [ x"${TOP_SRCDIR}" != x ]; then
        CMDOUT="`echo \"$CMDOUT\" | tr -d '\r'`"
    fi
    if [ x"$CMDOUT" != x"$EXPECTED_UPSLIST_JSON" ] ; then
        log_error "[testcase_sandbox_start_upsd_alone] got this reply for upsc JSON listing when '$EXPECTED_UPSLIST' was expected: '$CMDOUT'"
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
    if echo "$CMDERR" | ${GREP} 'Error: Driver not connected' >/dev/null ; then
        PASSED="`expr $PASSED + 1`"
    else
        log_error "[testcase_sandbox_start_upsd_alone] got some other reply for upsc query when 'Error: Driver not connected' was expected on stderr: '$CMDOUT'"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_sandbox_start_upsd_alone"
        res_testcase_sandbox_start_upsd_alone=1
    fi

    log_info "[testcase_sandbox_start_upsd_alone] Query driver state from UPSD by UPSC (driver not running yet) in JSON mode"
    runcmd upsc -j dummy@localhost:$NUT_PORT && {
        log_error "upsc was supposed to answer with error exit code: $CMDOUT"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_sandbox_start_upsd_alone"
        res_testcase_sandbox_start_upsd_alone=1
    }
    log_debug "[testcase_sandbox_start_upsd_alone] got a JSON reply:" "$CMDOUT"
    EXPECTED_UPSDATA_JSON='{
  "error": "Driver not connected"
}'
    # For windows runners (strip CR if any):
    EXPECTED_UPSDATA_JSON="`echo \"$EXPECTED_UPSDATA_JSON\" | tr -d '\r'`"
    CMDOUT="`echo \"$CMDOUT\" | tr -d '\r'`"
    # Note: avoid exact matching for stderr, because it can have Init SSL messages etc.
    if echo "$CMDERR" | ${GREP} 'Error: Driver not connected' >/dev/null && test x"${CMDOUT}" = x"${EXPECTED_UPSDATA_JSON}"; then
        PASSED="`expr $PASSED + 1`"
    else
        log_error "[testcase_sandbox_start_upsd_alone] got some other reply for upsc JSON query when 'Error: Driver not connected' was expected on stderr and similar in JSON object: '$CMDOUT'"
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

EXPECTED_UPSWAIT_JSON='{
  "ups.status": "WAIT"
}'
EXPECTED_UPSWAIT_JSON2='{
  "driver.state": "updateinfo",
  "ups.status": "WAIT"
}'
# For windows runners (strip CR if any):
EXPECTED_UPSWAIT_JSON="`echo \"$EXPECTED_UPSDATA_JSON\" | tr -d '\r'`"
EXPECTED_UPSWAIT_JSON2="`echo \"$EXPECTED_UPSDATA_JSON2\" | tr -d '\r'`"

testcase_sandbox_start_upsd_after_drivers() {
    # Historically this is a fallback from testcase_sandbox_start_drivers_after_upsd
    log_info "[testcase_sandbox_start_upsd_after_drivers] Test starting UPSD after drivers"
    kill -15 $PID_UPSD 2>/dev/null
    wait $PID_UPSD

    # Not calling upsd_start_loop() here, before drivers
    # If the server starts, fine; if not - we retry below
    if [ -n "${NUT_DEBUG_LEVEL_UPSD-}" ]; then
        NUT_DEBUG_LEVEL="${NUT_DEBUG_LEVEL_UPSD}"
    fi
    execcmd upsd ${ARG_FG} ${ARG_USER} &
    PID_UPSD="$!"
    NUT_DEBUG_LEVEL="${NUT_DEBUG_LEVEL_ORIG}"
    log_debug "[testcase_sandbox_start_upsd_after_drivers] Tried to start UPSD as PID $PID_UPSD"

    sandbox_start_drivers
    sandbox_start_upsd

    sleep 5

    COUNTDOWN=90
    GOT_REPLY=false
    while [ "$COUNTDOWN" -gt 0 ]; do
        # For query errors or known wait, keep looping
        runcmd upsc dummy@localhost:$NUT_PORT \
        && case "$CMDOUT" in
            *"ups.status: WAIT"*) ;;
            *) log_info "[testcase_sandbox_start_upsd_after_drivers] Got output:" ; echo "$CMDOUT" ; GOT_REPLY=true ;;
        esac

        runcmd upsc -j dummy@localhost:$NUT_PORT \
        && case "$CMDOUT" in
            "${EXPECTED_UPSWAIT_JSON}") ;;
            "${EXPECTED_UPSWAIT_JSON2}") ;;
            *) log_info "[testcase_sandbox_start_upsd_after_drivers] Got JSON output:" ; echo "$CMDOUT" ; GOT_REPLY=true ;;
        esac

        if $GOT_REPLY ; then break ; fi

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
    GOT_REPLY=false
    while [ "$COUNTDOWN" -gt 0 ]; do
        # For query errors or known wait, keep looping. May get:
        #   driver.state: updateinfo
        #   ups.status: WAIT
        runcmd upsc dummy@localhost:$NUT_PORT \
        && case "$CMDOUT" in
            *"ups.status: WAIT"*) ;;
            *) log_info "[testcase_sandbox_start_drivers_after_upsd] Got output:" ; echo "$CMDOUT" ; GOT_REPLY=true ;;
        esac

        runcmd upsc -j dummy@localhost:$NUT_PORT \
        && case "$CMDOUT" in
            "${EXPECTED_UPSWAIT_JSON}") ;;
            "${EXPECTED_UPSWAIT_JSON2}") ;;
            *) log_info "[testcase_sandbox_start_drivers_after_upsd] Got JSON output:" ; echo "$CMDOUT" ; GOT_REPLY=true ;;
        esac

        if $GOT_REPLY ; then break ; fi

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
            GOT_REPLY=false

            while [ "$COUNTDOWN" -gt 0 ]; do
                runcmd upsc $U@localhost:$NUT_PORT ups.status \
                && case "$CMDOUT" in
                    WAIT) ;;
                    *) log_info "[testcase_sandbox_start_drivers_after_upsd] Got output for $U:" ; echo "$CMDOUT" ; GOT_REPLY=true ;;
                esac

                runcmd upsc -j $U@localhost:$NUT_PORT ups.status \
                && case "$CMDOUT" in
                    '"WAIT"') ;; # JSON string is a valid document too
                    *) log_info "[testcase_sandbox_start_drivers_after_upsd] Got JSON output for $U:" ; echo "$CMDOUT" ; GOT_REPLY=true ;;
                esac

                if $GOT_REPLY ; then break ; fi

                sleep 1
                COUNTDOWN="`expr $COUNTDOWN - 1`"

                # Systemic error, e.g. could not create socket file?
                [ "$COUNTDOWN" -lt 1 ] && die "[testcase_sandbox_start_drivers_after_upsd] Dummy driver for $U did not start or respond in time"
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

    log_info "[testcase_sandbox_upsc_query_model] Query model from dummy device in JSON"
    runcmd upsc -j dummy@localhost:$NUT_PORT device.model || die "[testcase_sandbox_upsc_query_model] upsd does not respond on port ${NUT_PORT} or can not get JSON output ($?): $CMDOUT"
    log_debug "[testcase_sandbox_upsc_query_model] got a JSON reply:" "$CMDOUT"
    EXPECTED_UPSDATA_JSON='"Dummy UPS"'
    # For windows runners (strip CR if any):
    EXPECTED_UPSDATA_JSON="`echo \"$EXPECTED_UPSDATA_JSON\" | tr -d '\r'`"
    CMDOUT="`echo \"$CMDOUT\" | tr -d '\r'`"
    if [ x"$CMDOUT" != x"${EXPECTED_UPSDATA_JSON}" ] ; then
        log_error "[testcase_sandbox_upsc_query_model] got this reply for upsc JSON query when 'device.model: Dummy UPS' was expected: $CMDOUT"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_sandbox_upsc_query_model"
    else
        PASSED="`expr $PASSED + 1`"
        log_info "[testcase_sandbox_upsc_query_model] PASSED: got expected model from dummy device in JSON: $CMDOUT"
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
    if echo "$CMDERR" | ${GREP} 'Error: Variable not supported by UPS' >/dev/null ; then
        PASSED="`expr $PASSED + 1`"
        log_info "[testcase_sandbox_upsc_query_bogus] PASSED: got expected reply to bogus query"
    else
        log_error "[testcase_sandbox_upsc_query_bogus] got some other reply for upsc query when 'Error: Variable not supported by UPS' was expected on stderr: stderr:'$CMDERR' / stdout:'$CMDOUT'"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_sandbox_upsc_query_bogus"
    fi

    log_info "[testcase_sandbox_upsc_query_bogus] Query driver state from UPSD by UPSC for bogus info in JSON"
    runcmd upsc -j dummy@localhost:$NUT_PORT ups.bogus.value && {
        log_error "[testcase_sandbox_upsc_query_bogus] upsc was supposed to answer with error exit code: $CMDOUT"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_sandbox_upsc_query_bogus"
    }
    log_debug "[testcase_sandbox_upsc_query_bogus] got a JSON reply:" "$CMDOUT"
    EXPECTED_UPSDATA_JSON='{"error": "Variable not supported by UPS"}'
    EXPECTED_UPSDATA_JSON2='{
  "error": "Variable not supported by UPS"
}'
    # For windows runners (strip CR if any):
    EXPECTED_UPSDATA_JSON="`echo \"$EXPECTED_UPSDATA_JSON\" | tr -d '\r'`"
    EXPECTED_UPSDATA_JSON2="`echo \"$EXPECTED_UPSDATA_JSON2\" | tr -d '\r'`"
    CMDOUT="`echo \"$CMDOUT\" | tr -d '\r'`"
    # Note: avoid exact matching for stderr, because it can have Init SSL messages etc.
    if echo "$CMDERR" | ${GREP} 'Error: Variable not supported by UPS' >/dev/null && test x"${CMDOUT}" = x"${EXPECTED_UPSDATA_JSON}" -o x"${CMDOUT}" = x"${EXPECTED_UPSDATA_JSON2}" ; then
        PASSED="`expr $PASSED + 1`"
        log_info "[testcase_sandbox_upsc_query_bogus] PASSED: got expected reply to bogus query in JSON"
    else
        log_error "[testcase_sandbox_upsc_query_bogus] got some other reply for upsc JSON query when 'Error: Variable not supported by UPS' was expected on stderr: stderr:'$CMDERR' / stdout:'$CMDOUT'"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_sandbox_upsc_query_bogus"
    fi
}

testcase_sandbox_upsc_query_timer() {
    log_separator
    log_info "[testcase_sandbox_upsc_query_timer] Test that dummy-ups TIMER action changes the reported state"

    # Driver is set up to flip ups.status every 5 sec, so check every 3 sec
    # with upsc, but we can follow with upslog more intensively. New process
    # launches can lag a lot on very busy SUTs; hopefully still-running ones
    # are more responsive in this regard.
    log_info "Starting upslog daemon"
    rm -f "${NUT_STATEPATH}/upslog-dummy.log" || true
    # Start as foregrounded always, so we have a PID to kill easily:
    if [ -n "${NUT_DEBUG_LEVEL_UPSLOG-}" ]; then
        NUT_DEBUG_LEVEL="${NUT_DEBUG_LEVEL_UPSLOG}"
    fi
    execcmd upslog -F -i 1 -d 30 -m "dummy@localhost:${NUT_PORT},${NUT_STATEPATH}/upslog-dummy.log" &
    PID_UPSLOG="$!"
    NUT_DEBUG_LEVEL="${NUT_DEBUG_LEVEL_ORIG}"

    # TODO: Any need to convert to runcmd()?
    OUT1="`execcmd upsc dummy@localhost:$NUT_PORT ups.status`" || die "[testcase_sandbox_upsc_query_timer] upsd does not respond on port ${NUT_PORT} ($?): $OUT1" ; sleep 3
    OUT2="`execcmd upsc dummy@localhost:$NUT_PORT ups.status`" || die "[testcase_sandbox_upsc_query_timer] upsd does not respond on port ${NUT_PORT} ($?): $OUT2"
    OUT3=""
    OUT4=""
    OUT5=""

    # The SUT may be busy, or dummy-ups can have a loop delay
    # (pollfreq) after reading the file before wrapping around
    if [ x"$OUT1" = x"$OUT2" ]; then
        sleep 3
        OUT3="`execcmd upsc dummy@localhost:$NUT_PORT ups.status`" || die "[testcase_sandbox_upsc_query_timer] upsd does not respond on port ${NUT_PORT} ($?): $OUT3"
        if [ x"$OUT2" = x"$OUT3" ]; then
            sleep 3
            OUT4="`execcmd upsc dummy@localhost:$NUT_PORT ups.status`" || die "[testcase_sandbox_upsc_query_timer] upsd does not respond on port ${NUT_PORT} ($?): $OUT4"
            if [ x"$OUT3" = x"$OUT4" ]; then
                sleep 8
                OUT5="`execcmd upsc dummy@localhost:$NUT_PORT ups.status`" || die "[testcase_sandbox_upsc_query_timer] upsd does not respond on port ${NUT_PORT} ($?): $OUT4"
            fi
        fi
    fi

    log_info "Stopping upslog daemon"
    kill -15 $PID_UPSLOG 2>/dev/null || true
    wait $PID_UPSLOG || true

    if (${GREP} " [OB] " "${NUT_STATEPATH}/upslog-dummy.log" && ${GREP} " [OL] " "${NUT_STATEPATH}/upslog-dummy.log") \
    || (${GREP} " \[OB\] " "${NUT_STATEPATH}/upslog-dummy.log" && ${GREP} " \[OL\] " "${NUT_STATEPATH}/upslog-dummy.log") \
    || (echo "$OUT1$OUT2$OUT3$OUT4$OUT5" | ${GREP} "OB" && echo "$OUT1$OUT2$OUT3$OUT4$OUT5" | ${GREP} "OL") \
    ; then
        log_info "[testcase_sandbox_upsc_query_timer] PASSED: ups.status flips over time"
        PASSED="`expr $PASSED + 1`"
    else
        log_error "[testcase_sandbox_upsc_query_timer] ups.status did not flip over time"
        FAILED="`expr $FAILED + 1`"
        FAILED_FUNCS="$FAILED_FUNCS testcase_sandbox_upsc_query_timer"
    fi

    #rm -f "${NUT_STATEPATH}/upslog-dummy.log" || true
}

PY_SHEBANG=""
PY_RES=127
isTestablePython() {
    # We optionally make python module (if interpreter is found):
    case x"${PY_SHEBANG}" in
        x"") ;; # Fall through to detection
        *) return $PY_RES ;; # Probably resolved (if not a comment)?
    esac

    if [ x"${TOP_BUILDDIR}" = x ] \
    || [ ! -x "${TOP_BUILDDIR}/scripts/python/module/test_nutclient.py" ] \
    ; then
        return 1
    fi

    if [ x"${PYTHON}" != x ] ; then
        PY_SHEBANG="#!${PYTHON}"
        PY_RES=0
        return 0
    fi

    if [ x"${PYTHON_DEFAULT}" != x ] ; then
        PYTHON="${PYTHON_DEFAULT}"
        PY_SHEBANG="#!${PYTHON_DEFAULT}"
        PY_RES=0
        return 0
    fi

    PY_SHEBANG="`head -1 \"${TOP_BUILDDIR}/scripts/python/module/test_nutclient.py\"`"
    PY_RES=3
    case x"${PY_SHEBANG}" in
        x"#!"/*|x"#!"?":\\"*|x"#!"?":/"*) PY_RES=0 ;; # Seems like a full path
        x"#!no")   PY_RES=1 ;; # Explicitly skipped
        x"#!@")    PY_RES=2 ;; # Unresolved
        *)         PY_RES=3 ;; # Unexpected twist
    esac
    if [ x"${PY_RES}" = x0 ] ; then
        log_debug "=======\nDetected python shebang: '${PY_SHEBANG}' (result=${PY_RES})"
        PYTHON="`echo \"${PY_SHEBANG}\" | sed 's,^#!,,'`"
    else
        log_error "[isTestablePython] Detected python shebang: '${PY_SHEBANG}' (result=${PY_RES})"
    fi
    return $PY_RES
}

testcase_sandbox_python_without_credentials() {
    isTestablePython && [ -n "${PYTHON}" ] || return 0

    log_separator
    log_info "[testcase_sandbox_python_without_credentials] Call Python module test suite: PyNUT (NUT Python bindings) without login credentials"
    if ( unset NUT_USER || true
         unset NUT_PASS || true
        $PYTHON "${TOP_BUILDDIR}/scripts/python/module/test_nutclient.py"
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
    isTestablePython && [ -n "${PYTHON}" ] || return 0

    # That script says it expects data/evolution500.seq (as the UPS1 dummy)
    # but the dummy data does not currently let issue the commands and
    # setvars tested from python script.
    log_separator
    log_info "[testcase_sandbox_python_with_credentials] Call Python module test suite: PyNUT (NUT Python bindings) with login credentials"
    if (
        NUT_USER='admin'
        NUT_PASS="${TESTPASS_ADMIN}"
        export NUT_USER NUT_PASS
        $PYTHON "${TOP_BUILDDIR}/scripts/python/module/test_nutclient.py"
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
    isTestablePython && [ -n "${PYTHON}" ] || return 0

    log_separator
    log_info "[testcase_sandbox_python_with_upsmon_credentials] Call Python module test suite: PyNUT (NUT Python bindings) with upsmon role login credentials"
    if (
        NUT_USER='dummy-admin'
        NUT_PASS="${TESTPASS_UPSMON_PRIMARY}"
        export NUT_USER NUT_PASS
        $PYTHON "${TOP_BUILDDIR}/scripts/python/module/test_nutclient.py"
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
    isTestablePython && [ -n "${PYTHON}" ] || return 0

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
        && echo "$CMDOUT" | ${EGREP} '^\[nutdev-nut1\]$' \
        && echo "$CMDOUT" | ${GREP} 'port = "dummy@' \
        || return

        if [ "${NUT_PORT}" = 3493 ] || [ x"$NUT_PORT" = x ]; then
            log_info "[testcase_sandbox_nutscanner_list] Note: not testing for suffixed port number" >&2
        else
            echo "$CMDOUT" | ${EGREP} 'dummy@.*'":${NUT_PORT}" \
            || {
                log_error "[testcase_sandbox_nutscanner_list] dummy@... not found" >&2
                return 1
            }
        fi

        if [ x"${TOP_SRCDIR}" = x ]; then
            log_info "[testcase_sandbox_nutscanner_list] Note: only testing one dummy device" >&2
        else
            echo "$CMDOUT" | ${EGREP} '^\[nutdev-nut2\]$' \
            && echo "$CMDOUT" | ${GREP} 'port = "UPS1@' \
            && echo "$CMDOUT" | ${EGREP} '^\[nutdev-nut3\]$' \
            && echo "$CMDOUT" | ${GREP} 'port = "UPS2@' \
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
        PORTS_SEEN="`echo \"$CMDOUT\" | ${EGREP} -c 'port *='`"

        if [ "$PORTS_WANT" != "$PORTS_SEEN" ]; then
            log_error "[testcase_sandbox_nutscanner_list] Too many 'port=' lines: want $PORTS_WANT != seen $PORTS_SEEN" >&2
            return 1
        fi
    ) >/dev/null ; then
        log_info "[testcase_sandbox_nutscanner_list] PASSED: nut-scanner found all expected devices"
        PASSED="`expr $PASSED + 1`"
    else
        if ( echo "$CMDERR" | ${EGREP} "Cannot load NUT library.*libupsclient.*found.*NUT search disabled" ) ; then
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

upsmon_start_loop() {
    TESTCASE="${1-upsmon_start_loop}"

    if isPidAlive "$PID_UPSMON" ; then
        return 0
    fi

    if [ -n "${NUT_DEBUG_LEVEL_UPSMON-}" ]; then
        # Note: This is inherited into children of UPSMON process too,
        # but the sample script honours NUT_DEBUG_LEVEL_UPSSCHED if set
        NUT_DEBUG_LEVEL="${NUT_DEBUG_LEVEL_UPSMON}"
    fi
    execcmd upsmon ${ARG_FG} ${ARG_USER} &
    PID_UPSMON="$!"
    NUT_DEBUG_LEVEL="${NUT_DEBUG_LEVEL_ORIG}"
    log_debug "[${TESTCASE}] Tried to start UPSMON as PID $PID_UPSMON"
    sleep 2
    # Due to severe config errors, client could have died by now
    # TOTHINK: Loop like for upsd?

    # Return code is 0/OK if the client is alive
    if isPidAlive "$PID_UPSMON" ; then
        log_debug "[${TESTCASE}] UPSMON PID $PID_UPSMON is alive after a short while"
        return
    fi

    log_error "[${TESTCASE}] UPSMON PID $PID_UPSMON is not alive after a short while"
    return 1
}

sandbox_start_upsmon_master() {
    # Optional arg can specify amount of power sources to MONITOR and require
    if isPidAlive "$PID_UPSMON" ; then
        # FIXME: Kill and restart? Reconfigure and reload?
        return 0
    fi

    sandbox_generate_configs
    generatecfg_upsmon_master "$@"

    log_info "Starting UPSMON as master for sandbox"

    upsmon_start_loop "sandbox"
}

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

testgroup_sandbox_upsmon_master() {
    # Arrange for quick test iterations
    # Optional arg can specify amount of power sources to MONITOR and require
    testcase_sandbox_start_drivers_after_upsd
    sandbox_start_upsmon_master "$@"

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
        # NOTE: Not quoted, can have further arguments
        # e.g. NIT_CASE="testgroup_sandbox_upsmon_master 1"
        eval ${NIT_CASE}
        ;;
    generatecfg_*|is*)
        # NOTE: Keep in sync with TESTDIR cleanup above
        # (at start-up and at the exit-trap) and stop_daemons below
        log_warn "========================================================"
        log_warn "You asked to run just a specific helper method: '${NIT_CASE}'"
        log_warn "Populated environment variables for this run into a file so you can source them: . '$NUT_CONFPATH/NIT.env'"
        log_warn "Notably, NUT_CONFPATH='$NUT_CONFPATH' now"
        log_warn "========================================================"
        # NOTE: Not quoted, can have further arguments
        eval ${NIT_CASE}
        if [ $? = 0 ] ; then
            PASSED="`expr $PASSED + 1`"
        else
            FAILED="`expr $FAILED + 1`"
            FAILED_FUNCS="$FAILED_FUNCS $NIT_CASE"
        fi
        unset DEBUG_SLEEP
        ;;
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

    if [ -z "$PID_UPSSCHED" ] && [ -s "$NUT_PIDPATH/upssched.pid" ] ; then
        PID_UPSSCHED="`head -1 \"$NUT_PIDPATH/upssched.pid\"`"
    fi

    log_separator
    log_info "Sleeping now as asked (for ${DEBUG_SLEEP} seconds starting `date -u`), so you can play with the driver and server running"
    log_info "Populated environment variables for this run into a file so you can source them: . '$NUT_CONFPATH/NIT.env'"
    printf "PID_NIT_SCRIPT='%s'\nexport PID_NIT_SCRIPT\n" "$$" >> "$NUT_CONFPATH/NIT.env"
    set | ${EGREP} '^TESTPASS_|PID_[^ =]*='"'?[0-9][0-9]*'?$" | while IFS='=' read K V ; do
        V="`echo \"$V\" | tr -d \"'\"`"
        # Dummy comment to reset syntax highlighting due to ' quote above
        if [ -n "$V" ] ; then
            printf "%s='%s'\nexport %s\n" "$K" "$V" "$K"
        fi
    done >> "$NUT_CONFPATH/NIT.env"
    log_separator
    cat "$NUT_CONFPATH/NIT.env"
    log_separator
    log_info "See above about important variables from the test sandbox and a way to 'source' them into your shell, e.g.: . '$NUT_CONFPATH/NIT.env'"

    if [ x"${TESTDIR_CALLER}" != x"${TESTDIR}" ] && [ x"${TESTDIR}" != x"$BUILDDIR/tmp" ] ; then
        log_warn "A temporary random TESTDIR location is used, it would be automatically removed after this script exits!"
    fi

    log_info "You may want to press Ctrl+Z now and command 'bg' to the shell, if you did not start '$0 &' backgrounded already"
    log_info "To kill the script early, return it to foreground with 'fg' and press Ctrl+C, or 'kill -2 \$PID_NIT_SCRIPT' (kill -2 $$)"
    log_info "To trace built programs, check scripts/valgrind/README.adoc and LD_LIBRARY_PATH for this build"

    log_info "Remember that in shell/scripting you can probe for '${NUT_CONFPATH}/NIT.env-sandbox-ready' which only appears at this moment"
    touch "${NUT_CONFPATH}/NIT.env-sandbox-ready"

    sleep "${DEBUG_SLEEP}"
    log_info "Sleep finished"
    log_separator
fi

# We have a trap.... but for good measure, do some explicit clean-up:
case "${NIT_CASE}" in
    generatecfg_*|is*) ;;
    *) stop_daemons ;;
esac

if [ "$PASSED" = 0 ] || [ "$FAILED" != 0 ] ; then
    die "Some test scenarios failed!"
else
    log_info "SUCCESS"
fi
