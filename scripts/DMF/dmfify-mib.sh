#!/usr/bin/env bash

# This wrapper around Python scripts uses them to generate XML DMF files
# from existing NUT *-mib.c sources with legacy mapping structures.
# It expects to be located in (executed from) $NUT_SOURCEDIR/scripts/DMF
#
#   Copyright (C) 2016 Michal Vyskocil <MichalVyskocil@eaton.com>
#   Copyright (C) 2016 - 2021 Jim Klimov <EvgenyKlimov@eaton.com>
#   Copyright (C) 2022 - 2024 Jim Klimov <jimklimov+nut@gmail.com>
#

# A bashism, important for us here
set -o pipefail

# Strings must verbatim match the XSD (no trailing slash etc.)
XSD_DMFSNMP_VERSION='1.0.0'
XSD_DMFSNMP_XMLNS='http://www.networkupstools.org/dmf/snmp/snmp-ups'

# Where to look for python scripts - same dir as this shell script
_SCRIPT_DIR="`cd $(dirname "$0") && pwd`" || \
    _SCRIPT_DIR="./" # Fallback can fail
[ -n "${ABS_OUTDIR-}" ] || ABS_OUTDIR="`pwd`" || exit
[ -n "${ABS_BUILDDIR-}" ] || ABS_BUILDDIR="${ABS_OUTDIR}"

# Integrate with "make V=1"
[ -n "${DEBUG-}" ] || DEBUG="${V-}"

logmsg_info() {
    if [ x"${DEBUG-}" = x1 -o x"${DEBUG-}" = xyes -o x"${DEBUG-}" = x ] ; then
        echo "INFO: $*" >&2
    fi
    return 0
}

logmsg_debug() {
    if [ x"${DEBUG-}" = x1 -o x"${DEBUG-}" = xyes ] ; then
        echo "DEBUG-INFO: $*" >&2
    fi
    return 0
}

logmsg_error() {
#    if [ x"${DEBUG-}" = x1 -o x"${DEBUG-}" = xyes ] ; then
        echo "ERROR: $*" >&2
#    fi
    return 0
}

die() {
    code=$?
    if [ "$1" -ge 0 ] 2>/dev/null ; then
        code="$1"
        shift
    fi

    logmsg_error "$@"
    exit $code
}

if [ -n "${PYTHON-}" ] ; then
    # May be a name/path of binary, or one with args - check both
    (command -v "$PYTHON") \
    || $PYTHON -c "import re,glob,codecs" \
    || {
        echo "----------------------------------------------------------------------"
        echo "WARNING: Caller-specified PYTHON='$PYTHON' is not available."
        echo "----------------------------------------------------------------------"
        # Do not die just here, we may not need the interpreter
    }
else
    PYTHON=""
    for P in python python3 python2.7 python2 ; do
        if (command -v "$P" >/dev/null) && $P -c "import re,glob,codecs" ; then
            PYTHON="$P"
            break
        fi
    done
fi

# TODO: The PYTHON and CC variables currently assume pathnames (no args)
# NOTE: Here unquoted(!) `command -v $PYTHON` allows to expand e.g. a
# "/usr/bin/env python" value into a real path; ignoring args to python
# itself (if any).
[ -n "${PYTHON}" ] && PYTHON="`command -v $PYTHON | tail -1`" && [ -x "$PYTHON" ] \
|| die 2 "Can not find Python 2.7+: '$PYTHON'"

# The pycparser uses GCC-compatible flags
[ -n "${CC-}" ] || CC="`command -v gcc`"
CC_ENV=""
CC_WITH_ARGS=false
if [ -n "${CC-}" ] ; then
    case "$CC" in
        *" "*|*\t*) # args inside? prefixed envvars?
            STAGE=env
            for TOKEN in $CC ; do
                case "$TOKEN" in
                    *=*)
                        case "$STAGE" in
                            env) CC_ENV="$CC_ENV $TOKEN" ;;
                            arg) CFLAGS="$CFLAGS $TOKEN" ;;
                        esac ;;
                    *)
                        case "$STAGE" in
                            env) STAGE=bin ; CC="$TOKEN" ;;
                            bin) STAGE=arg ; CFLAGS="$CFLAGS $TOKEN" ;;
                            arg) CFLAGS="$CFLAGS $TOKEN" ;;
                        esac ;;
                esac
            done
            ;;
    esac
    case "$CC" in
        /*) ;;
        *)  CC="`command -v "$CC"`" ;;
    esac
    case "$CC" in
        *" "*|*"	"*) $CC --help >/dev/null 2>/dev/null && CC_WITH_ARGS=true;;
    esac
fi
[ -n "${CC}" ] && ( [ -x "$CC" ] || $CC_WITH_ARGS ) || die 2 "Can not find (G)CC: '$CC'"
export CC CFLAGS CC_ENV

# Avoid "cpp" directly as it may be too "traditional"
[ -n "${CPP-}" ] || CPP="$CC -E" ### CPP="`command -v cpp`"
CPP_ENV=""
CPP_WITH_ARGS=false
if [ -n "${CPP-}" ] ; then
    case "$CPP" in
        "$CC -E") # exact hit
            CPP_ENV="$CC_ENV"
            ;;
        *" "*|*\t*) # args inside? prefixed envvars?
            STAGE=env
            for TOKEN in $CPP ; do
                case "$TOKEN" in
                    *=*)
                        case "$STAGE" in
                            env) CPP_ENV="$CPP_ENV $TOKEN" ;;
                            arg) CPPFLAGS="$CPPFLAGS $TOKEN" ;;
                        esac ;;
                    *)
                        case "$STAGE" in
                            env) STAGE=bin ; CPP="$TOKEN" ;;
                            bin) STAGE=arg ; CPPFLAGS="$CPPFLAGS $TOKEN" ;;
                            arg) CPPFLAGS="$CPPFLAGS $TOKEN" ;;
                        esac ;;
                esac
            done
            ;;
    esac
    case "$CPP" in
        /*) ;;
        *)  CPP="`command -v "$CPP"`" ;;
    esac
    case "$CPP" in
        # FIXME: Make CPP single-token and prepend args to CPPFLAGS (if any)
        # Issue: some `/usr/bin/env gcc -E` would make "CPP=/usr/bin/env"...
        *" "*|*"	"*) $CPP --help >/dev/null 2>/dev/null && CPP_WITH_ARGS=true;;
    esac
fi
[ -n "${CPP}" ] && ( [ -x "$CPP" ] || $CPP_WITH_ARGS ) || die 2 "Can not find a C preprocessor: '$CPP' (for CC='$CC')"
export CPP CPPFLAGS CPP_ENV

if [ "$1" == "--skip-sanity-check" ]; then
    shift 1
else
    # Here we only check basic prerequisites (a module provided with Python 2.7
    # and an extra module that end-user is expected to pre-install per README).
    # Other included modules will be checked when scripts are executed.
    logmsg_info "Validating some basics about your Python installation"
    for PYMOD in argparse pycparser json; do
        "${PYTHON}" -c "import $PYMOD; print ($PYMOD);" || \
            die 2 "Can not use Python module '$PYMOD'"
    done

    eval $CPP_ENV "$CPP" $CPPFLAGS --help > /dev/null || die 2 "Can not find a C preprocessor: '$CPP'"
    eval $CC_ENV  "$CC"  $CFLAGS   --help > /dev/null || die 2 "Can not find a C compiler: '$CC'"

    if [ "$1" = "--sanity-check" ]; then
        # We are alive by now, so checks above have succeeded
        exit 0
    fi
fi

dmfify_c_file() {
    # One reqiured argument: path to a `*-mib.c` filename
    # Optional second one names the output file (and temporary files)
    # Optional third (and beyond) name additional files to look into
    # (e.g. the snmp-ups-helpers.c for shared tables and routines)
    #logmsg_debug "dmfify_c_file: '$1' '$2' '$3' ..." >&2
    local cmib="$1"
    local mib="$2"
    shift
    shift || true # $2+ may be missing

    if [ -z "${mib}" ] ; then
        mib="$(basename "${cmib}" .c)"
    else
        # Note/FIXME: dirname is dropped, files land into current dir
        # as prepared by Makefile
        mib="$(basename "${mib}" .dmf)"
    fi

    [ -n "${cmib}" ] && [ -s "${cmib}" ] || \
        { logmsg_error "dmfify_c_file() can not process argument '${cmib}'!"
          return 2; }

    logmsg_info "Parsing '${cmib}' into '${mib}.dmf'; do not worry if 'missing setvar' warnings pop up..."
    if [ $# -gt 0 ]; then logmsg_info "Additionally parsing resources from: $*" ; fi

    # Code below assumes that the *.py.in only template the shebang line
    JNAME=""
    for F in "${_SCRIPT_DIR}"/jsonify-mib.py.in "${_SCRIPT_DIR}"/jsonify-mib.py ; do
        [ -s "$F" ] && JNAME="$F" && break
    done
    XNAME=""
    for F in "${_SCRIPT_DIR}"/xmlify-mib.py.in "${_SCRIPT_DIR}"/xmlify-mib.py ; do
        [ -s "$F" ] && XNAME="$F" && break
    done
    logmsg_debug "PWD: `pwd`"
    ( "${PYTHON}" "${JNAME}" --test "${cmib}" "$@" > "${ABS_BUILDDIR}/${mib}.json.tmp" && \
      "${PYTHON}" "${XNAME}" < "${ABS_BUILDDIR}/${mib}.json.tmp" > "${ABS_BUILDDIR}/${mib}.dmf.tmp" ) \
    && [ -s "${ABS_BUILDDIR}/${mib}.dmf.tmp" ] \
    || { ERRCODE=$?
        if [ $# -gt 0 ]; then
            logmsg_error "Could not parse '${cmib}' + $* into '${ABS_BUILDDIR}/${mib}.dmf'"
        else
            logmsg_error "Could not parse '${cmib}' into '${ABS_BUILDDIR}/${mib}.dmf'"
        fi
        logmsg_error "You can inspect a copy of the intermediate result in '${mib}.json.tmp', '${mib}.dmf.tmp' and '${mib}_TEST.c' located in '${ABS_BUILDDIR}/', '${_SCRIPT_DIR}' and/or '`pwd`'"
        logmsg_error "To troubleshoot 'nut_cpp' errors, export DEBUG_NUT_CPP=true and check resulting 'temp-cpp-filt.tmp' (see README-NUT-DMF.txt for more details)"
        return $ERRCODE; }

    sed 's,^<nut>,\<nut version="'"${XSD_DMFSNMP_VERSION}"'" xmlns="'"${XSD_DMFSNMP_XMLNS}"'"\>,' < "${ABS_BUILDDIR}/${mib}.dmf.tmp" > "${ABS_BUILDDIR}/${mib}.dmf" \
    || { ERRCODE=$?
        logmsg_error "Could not fix headers of '${ABS_BUILDDIR}/${mib}.dmf'"
        logmsg_error "You can inspect a copy of the intermediate result in '${mib}.json.tmp', '${mib}.dmf.tmp' and '${mib}_TEST.c located in '${ABS_BUILDDIR}/', '${_SCRIPT_DIR}' and/or '`pwd`''"
        return $ERRCODE; }

#    mv -f "${ABS_BUILDDIR}/${mib}.dmf.tmp" "${ABS_BUILDDIR}/${mib}.dmf" \
#    && rm -f "${ABS_BUILDDIR}/${mib}_TEST"{.c,.exe} "${ABS_BUILDDIR}/${mib}.json.tmp"

    if [ -n "${ABS_OUTDIR-}" -a "${ABS_OUTDIR-}" != "${ABS_BUILDDIR}" ] ; then
        mv -f "${ABS_BUILDDIR}/${mib}.dmf" "${ABS_OUTDIR}/" || return
    fi
}

list_shared_sources() {
    # Current codebase provides additional shared mappings to
    # conversion functions in snmp-ups -- and that is available
    # to all drivers, both DMF and non-DMF. Required built-in
    # to each DMF file because of `extern` in snmp-ups.h
    # TODO: Detect this sort of resources (presence, location)
    # somehow? Support a nested loop and separate storage var
    # to find many such files?
    SNAME=""

    for F in \
        ../../../drivers/snmp-ups-helpers.c \
        ../../drivers/snmp-ups-helpers.c \
        "${_SCRIPT_DIR}"/../../../drivers/snmp-ups-helpers.c \
        "${_SCRIPT_DIR}"/../../drivers/snmp-ups-helpers.c \
    ; do
        [ -s "$F" ] && SNAME="$F" && break
    done

    for F in \
        ../../../drivers/eaton-pdu-marlin-helpers.c \
        ../../drivers/eaton-pdu-marlin-helpers.c \
        "${_SCRIPT_DIR}"/../../../drivers/eaton-pdu-marlin-helpers.c \
        "${_SCRIPT_DIR}"/../../drivers/eaton-pdu-marlin-helpers.c \
    ; do
        [ -s "$F" ] && SNAME="$SNAME $F" && break
    done

    # echo the return value (string)
    echo "$SNAME"
}

dmfify_NUT_drivers() {
    local i=0
    # TODO? Use LEGACY_NUT_C_MIBS instead of filesystem query?
    # Got to know abs_srcdir to use it well then :)
    cd "${_SCRIPT_DIR}" || exit

    # TODO: It may be not too efficient to re-parse SNAME from C
    # to JSON and XML about 30 times for each mapping. Better
    # make some way to parse it once and add that JSON as the
    # helper for quick inclusion to jsonify and beyond.
    # TODO: Conflicts resolution (if several files define same token?)
    SNAME="`list_shared_sources`"
    for cmib in ../../../drivers/*-mib.c ../../drivers/*-mib.c; do
        [ -s "${cmib}" ] || \
            { logmsg_error "File not found or is empty: '${cmib}'" ; continue; }
        # Note: helper sources must be arg 3+ below;
        # arg2 may be empty but must then be present
        dmfify_c_file "${cmib}" "" $SNAME || return
        i=$(($i+1))
    done
    [ "$i" = 0 ] && logmsg_error "No files processed" && return 2
    logmsg_info "Processed $i files OK"
    return 0
}

if [[ "$#" -gt 0 ]]; then
    logmsg_info "Got some arguments, assuming they are NUT filenames for parsing"
    SNAME="`list_shared_sources`"
    # Note: helper sources must be arg 3+ below;
    # arg2 may be empty but must then be present
    while [[ "$#" -gt 0 ]]; do
        case "${2-}" in
            *.dmf)
                dmfify_c_file "$1" "$2" $SNAME || exit
                shift
                ;;
            *)
                dmfify_c_file "$1" "" $SNAME || exit
                ;;
        esac
        shift
    done
else
    logmsg_info "No arguments provided, will try to parse all NUT drivers"
    dmfify_NUT_drivers || exit
fi

logmsg_info "OK - All done"
