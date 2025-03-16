#!/usr/bin/env bash

################################################################################
# This file is based on a template used by zproject, but isn't auto-generated. #
# Its primary use is to automate a number of BUILD_TYPE scenarios for the NUT  #
# CI farm, but for the same reason it can also be useful to reduce typing for  #
# reproducible build attempts with NUT development and refactoring workflows.  #
# Note that it is driven by enviroment variables rather than CLI arguments --  #
# this approach better suits the practicalities of CI build farm technologies. #
################################################################################

set -e
SCRIPTDIR="`dirname "$0"`"
SCRIPTDIR="`cd "$SCRIPTDIR" && pwd`"

SCRIPT_PATH="${SCRIPTDIR}/`basename $0`"
SCRIPT_ARGS=("$@")

# Quick hijack for interactive development like this:
#   BUILD_TYPE=fightwarn-clang ./ci_build.sh
# or to quickly hit the first-found errors in a larger matrix
# (and then easily `make` to iterate fixes), like this:
#   CI_REQUIRE_GOOD_GITIGNORE="false" CI_FAILFAST=true DO_CLEAN_CHECK=no BUILD_TYPE=fightwarn ./ci_build.sh
#
# For in-place build configurations you can pass `INPLACE_RUNTIME=true`
# (for common BUILD_TYPE's) or call `./ci_build.sh inplace`
#
# For out-of-tree builds you can specify a CI_BUILDDIR (absolute or relative
# to SCRIPTDIR - not current path), or just call .../ci_build.sh while being
# in a different directory and then it would be used with a warning. This may
# require that you `make distclean` the original source checkout first:
#   CI_BUILDDIR=obj BUILD_TYPE=default-all-errors ./ci_build.sh
case "$BUILD_TYPE" in
    fightwarn) ;; # for default compiler
    fightwarn-all)
        # This recipe allows to test with different (default-named)
        # compiler suites if available. Primary goal is to see whether
        # everything is building ok on a given platform, with one shot.
        TRIED_BUILD=false
        if (command -v gcc) >/dev/null ; then
            TRIED_BUILD=true
            BUILD_TYPE=fightwarn-gcc "$0" || exit
        else
            echo "SKIPPING BUILD_TYPE=fightwarn-gcc: compiler not found" >&2
        fi
        if (command -v clang) >/dev/null ; then
            TRIED_BUILD=true
            BUILD_TYPE=fightwarn-clang "$0" || exit
        else
            echo "SKIPPING BUILD_TYPE=fightwarn-clang: compiler not found" >&2
        fi
        if ! $TRIED_BUILD ; then
            echo "FAILED to run: no default-named compilers were found" >&2
            exit 1
        fi
        exit 0
        ;;
    fightwarn-gcc)
        CC="gcc"
        CXX="g++"
        # Avoid "cpp" directly as it may be too "traditional"
        #CPP="cpp"
        CPP="gcc -E"
        BUILD_TYPE=fightwarn
        ;;
    fightwarn-clang)
        CC="clang"
        CXX="clang++"
        if (command -v clang-cpp) >/dev/null 2>/dev/null ; then
            CPP="clang-cpp"
        else
            CPP="clang -E"
        fi
        BUILD_TYPE=fightwarn
        ;;
esac

if [ "$BUILD_TYPE" = fightwarn ]; then
    # For CFLAGS/CXXFLAGS keep caller or compiler defaults
    # (including C/C++ revision)
    BUILD_TYPE=default-all-errors
    #BUILD_WARNFATAL=yes
    #   configure => "yes" except for antique compilers
    BUILD_WARNFATAL=auto

    # Current fightwarn goal is to have no warnings at preset level below,
    # or at the level defaulted with configure.ac (perhaps considering the
    # compiler version, etc.):
    #[ -n "$BUILD_WARNOPT" ] || BUILD_WARNOPT=hard
    #[ -n "$BUILD_WARNOPT" ] || BUILD_WARNOPT=medium
    #   configure => default to medium, detect by compiler type
    [ -n "$BUILD_WARNOPT" ] || BUILD_WARNOPT=auto

    # Eventually this constraint would be removed to check all present
    # SSL implementations since their ifdef-driven codebases differ and
    # emit varied warnings. But so far would be nice to get the majority
    # of shared codebase clean first:
    #[ -n "$NUT_SSL_VARIANTS" ] || NUT_SSL_VARIANTS=auto

    # Similarly for libusb implementations with varying support
    #[ -n "$NUT_USB_VARIANTS" ] || NUT_USB_VARIANTS=auto
fi

# configure default is "no"; an "auto" value is "yes unless CFLAGS say something"
[ -n "${BUILD_DEBUGINFO-}" ] || BUILD_DEBUGINFO=""

# Set this to enable verbose profiling
[ -n "${CI_TIME-}" ] || CI_TIME=""
case "$CI_TIME" in
    [Yy][Ee][Ss]|[Oo][Nn]|[Tt][Rr][Uu][Ee])
        CI_TIME="time -p " ;;
    [Nn][Oo]|[Oo][Ff][Ff]|[Ff][Aa][Ll][Ss][Ee])
        CI_TIME="" ;;
esac

# Set this to enable verbose tracing
[ -n "${CI_TRACE-}" ] || CI_TRACE="no"
case "$CI_TRACE" in
    [Nn][Oo]|[Oo][Ff][Ff]|[Ff][Aa][Ll][Ss][Ee])
        set +x ;;
    [Yy][Ee][Ss]|[Oo][Nn]|[Tt][Rr][Uu][Ee])
        set -x ;;
esac

[ -n "${CI_REQUIRE_GOOD_GITIGNORE-}" ] || CI_REQUIRE_GOOD_GITIGNORE="true"
case "$CI_REQUIRE_GOOD_GITIGNORE" in
    [Nn][Oo]|[Oo][Ff][Ff]|[Ff][Aa][Ll][Ss][Ee])
        CI_REQUIRE_GOOD_GITIGNORE="false" ;;
    [Yy][Ee][Ss]|[Oo][Nn]|[Tt][Rr][Uu][Ee])
        CI_REQUIRE_GOOD_GITIGNORE="true" ;;
esac

# Abort loops like BUILD_TYPE=default-all-errors as soon as we have a problem
# (allowing to rebuild interactively and investigate that set-up)?
[ -n "${CI_FAILFAST-}" ] || CI_FAILFAST=false

# We allow some CI setups to CI_SKIP_CHECK (avoiding it during single-process
# scripted build), so tests can be done as a visibly separate stage.
# This does not really apply to some build scenarios whose purpose is to
# loop and check many build scenarios (e.g. BUILD_TYPE="default-all-errors"
# and "fightwarn*" family), but it is up to caller when and why to set it.
# It is also a concern of the caller (for now) if actually passing the check
# relies on something this script does (set envvars, change paths...)
[ -n "${CI_SKIP_CHECK-}" ] || CI_SKIP_CHECK=false

# By default we configure and build in the same directory as source;
# and a `make distcheck` handles how we build from a tarball.
# However there are also cases where source is prepared (autogen) once,
# but is built in various directories with different configurations.
# This is something to test via CI, that recipes are not broken for
# such use-case. Note the path should be in .gitignore, e.g. equal to
# or under ./tmp/ or ./obj/ for the CI_REQUIRE_GOOD_GITIGNORE sanity
# checks to pass.
case "${CI_BUILDDIR-}" in
    "") # Not set, likeliest case
        CI_BUILDDIR="`pwd`"
        if [ x"${SCRIPTDIR}" = x"${CI_BUILDDIR}" ] ; then
            CI_BUILDDIR="."
        else
            echo "=== WARNING: This build will use '${CI_BUILDDIR}'"
            echo "=== for an out-of-tree build of NUT with sources located"
            echo "=== in '${SCRIPTDIR}'"
            echo "=== PRESS CTRL+C NOW if you did not mean this! (Sleeping 5 sec)"

            sleep 5
        fi
        ;;
    ".") ;; # Is SCRIPTDIR, in-tree build
    /*)  ;; # Absolute path located somewhere else
    *) # Non-trivial, relative to SCRIPTDIR, may not exist yet
        CI_BUILDDIR="${SCRIPTDIR}/${CI_BUILDDIR}"
        ;;
esac

# Just in case we get blanks from CI - consider them as not-set:
if [ -z "`echo "${MAKE-}" | tr -d ' '`" ] ; then
    if [ "$1" = spellcheck -o "$1" = spellcheck-interactive ] \
    && (command -v gmake) >/dev/null 2>/dev/null \
    ; then
        # GNU make processes quiet mode better, which helps with spellcheck use-case
        MAKE=gmake
    else
        # Use system default, there should be one (or fail eventually if not)
        MAKE=make
    fi
    export MAKE
fi

[ -n "$GGREP" ] || GGREP=grep

[ -n "$MAKE_FLAGS_QUIET" ] || MAKE_FLAGS_QUIET="VERBOSE=0 V=0 -s"
[ -n "$MAKE_FLAGS_VERBOSE" ] || MAKE_FLAGS_VERBOSE="VERBOSE=1 V=1 -s"
[ -n "$MAKE_FLAGS_CLEAN" ] || MAKE_FLAGS_CLEAN="${MAKE_FLAGS_QUIET}"

normalize_path() {
    # STDIN->STDOUT: strip duplicate "/" and extra ":" if present,
    # leave first copy of duplicates in (preferred) place
    sed -e 's,:::*,:,g' -e 's,^:*,,' -e 's,:*$,,' -e 's,///*,/,g' \
    | tr ':' '\n' \
    | ( P=""
        while IFS='' read D ; do
            case "${D}" in
                "") continue ;;
                /)  ;;
                */) D="`echo "${D}" | sed 's,/*$,,'`" ;;
            esac
            case "${P}" in
                "${D}"|*":${D}"|"${D}:"*|*":${D}:"*) ;;
                "") P="${D}" ;;
                *) P="${P}:${D}" ;;
            esac
        done
        echo "${P}"
      )
}

propose_CI_CCACHE_SYMLINKDIR() {
    # This is where many symlinks like "gcc -> ../bin/ccache" reside:
    echo \
        "/usr/lib/ccache" \
        "/mingw64/lib/ccache/bin" \
        "/mingw32/lib/ccache/bin" \
        "/usr/lib64/ccache" \
        "/usr/libexec/ccache" \
        "/usr/lib/ccache/bin" \
        "/usr/local/lib/ccache"

    if [ -n "${HOMEBREW_PREFIX-}" ]; then
        echo "${HOMEBREW_PREFIX}/opt/ccache/libexec"
    fi
}

ensure_CI_CCACHE_SYMLINKDIR_envvar() {
    # Populate CI_CCACHE_SYMLINKDIR envvar (if not yet populated e.g. by caller)
    # with location of symlinks like "gcc -> ../bin/ccache" to use
    # NOTE: a caller-provided value of "-" requests the script to NOT use
    # a CI_CCACHE_SYMLINKDIR; however ccache may still be used via prefixing
    # to compiler command, if the tool is found in the PATH, e.g. by calling
    # optional_ensure_ccache(), unless you export CI_CCACHE_USE=no also.

    if [ -z "${CI_CCACHE_SYMLINKDIR-}" ] ; then
        for D in `propose_CI_CCACHE_SYMLINKDIR` ; do
            if [ -d "$D" ] ; then
                if ( ls -la "$D" | grep -e ' -> .*ccache' >/dev/null) \
                || ( test -n "`find "$D" -maxdepth 1 -type f -exec grep -li ccache '{}' \;`" ) \
                ; then
                    CI_CCACHE_SYMLINKDIR="$D" && break
                else
                    echo "WARNING: Found potential CI_CCACHE_SYMLINKDIR='$D' but it did not host expected symlink patterns, skipped" >&2
                fi
            fi
        done

        if [ -n "${CI_CCACHE_SYMLINKDIR-}" ] ; then
            echo "INFO: Detected CI_CCACHE_SYMLINKDIR='$CI_CCACHE_SYMLINKDIR'; specify another explicitly if desired" >&2
        else
            echo "WARNING: Did not find any CI_CCACHE_SYMLINKDIR; specify one explicitly if desired" >&2
        fi
    else
        if [ x"${CI_CCACHE_SYMLINKDIR-}" = x- ] ; then
            echo "INFO: Empty CI_CCACHE_SYMLINKDIR was explicitly requested" >&2
            CI_CCACHE_SYMLINKDIR=""
        fi
    fi
}

optional_prepare_ccache() {
    # Prepare CCACHE_* variables and directories, determine if we HAVE_CCACHE
    # See also optional_ensure_ccache(), optional_prepare_compiler_family(),
    # ensure_CI_CCACHE_SYMLINKDIR_envvar()
    echo "PATH='$PATH' before possibly applying CCACHE into the mix"
    ( echo "$PATH" | grep ccache ) >/dev/null && echo "WARNING: ccache is already in PATH"
    if [ -n "$CC" ]; then
        echo "CC='$CC' before possibly applying CCACHE into the mix"
        $CC --version $CFLAGS || \
        $CC --version || true
    fi

    if [ -n "$CXX" ]; then
        echo "CXX='$CXX' before possibly applying CCACHE into the mix"
        $CXX --version $CXXFLAGS || \
        $CXX --version || true
    fi

    if [ x"${CI_CCACHE_USE-}" = xno ]; then
        HAVE_CCACHE=no
        CI_CCACHE_SYMLINKDIR=""
        echo "WARNING: Caller required to not use ccache even if available" >&2
    else
        if [ -n "${CI_CCACHE_SYMLINKDIR}" ]; then
            # Tell ccache the PATH without itself in it, to avoid loops processing
            PATH="`echo "$PATH" | sed -e 's,^'"${CI_CCACHE_SYMLINKDIR}"'/?:,,' -e 's,:'"${CI_CCACHE_SYMLINKDIR}"'/?:,,' -e 's,:'"${CI_CCACHE_SYMLINKDIR}"'/?$,,' -e 's,^'"${CI_CCACHE_SYMLINKDIR}"'/?$,,'`"
        fi
        CCACHE_PATH="$PATH"
        CCACHE_DIR="${HOME}/.ccache"
        export CCACHE_PATH CCACHE_DIR PATH
        HAVE_CCACHE=no
        if (command -v ccache || which ccache) \
        && ( [ -z "${CI_CCACHE_SYMLINKDIR}" ] || ls -la "${CI_CCACHE_SYMLINKDIR}" ) \
        ; then
            HAVE_CCACHE=yes
        fi
        mkdir -p "${CCACHE_DIR}"/ || HAVE_CCACHE=no
    fi
}

is_gnucc() {
    if [ -n "$1" ] && LANG=C "$1" --version 2>&1 | grep 'Free Software Foundation' > /dev/null ; then true ; else false ; fi
}

is_clang() {
    if [ -n "$1" ] && LANG=C "$1" --version 2>&1 | grep 'clang version' > /dev/null ; then true ; else false ; fi
}

filter_version() {
    # Starting with number like "6.0.0" or "7.5.0-il-0" is fair game,
    # but a "gcc-4.4.4-il-4" (starting with "gcc") is not
    sed -e 's,^.* \([0-9][0-9]*\.[0-9][^ ),]*\).*$,\1,' -e 's, .*$,,' | grep -E '^[0-9]' | head -1
}

ver_gnucc() {
    [ -n "$1" ] && LANG=C "$1" --version 2>&1 | grep -i gcc | filter_version
}

ver_clang() {
    [ -n "$1" ] && LANG=C "$1" --version 2>&1 | grep -i 'clang' | filter_version
}

optional_prepare_compiler_family() {
    # Populate CC, CXX, CPP envvars according to actual tools used/requested
    # Remember them as COMPILER_FAMILY
    # See also: optional_prepare_ccache(), ensure_CI_CCACHE_SYMLINKDIR_envvar(),
    # optional_prepare_compiler_family()
    COMPILER_FAMILY=""
    if [ -n "$CC" -a -n "$CXX" ]; then
        if is_gnucc "$CC" && is_gnucc "$CXX" ; then
            COMPILER_FAMILY="GCC"
            export CC CXX
        elif is_clang "$CC" && is_clang "$CXX" ; then
            COMPILER_FAMILY="CLANG"
            export CC CXX
        fi
    else
        # Generally we prefer GCC unless it is very old so we can't impact
        # its warnings and complaints.
        if is_gnucc "gcc" && is_gnucc "g++" ; then
            # Autoconf would pick this by default
            COMPILER_FAMILY="GCC"
            [ -n "$CC" ] || CC=gcc
            [ -n "$CXX" ] || CXX=g++
            export CC CXX
        elif is_gnucc "cc" && is_gnucc "c++" ; then
            COMPILER_FAMILY="GCC"
            [ -n "$CC" ] || CC=cc
            [ -n "$CXX" ] || CXX=c++
            export CC CXX
        fi

        if ( [ "$COMPILER_FAMILY" = "GCC" ] && \
            case "`ver_gnucc "$CC"`" in
                [123].*) true ;;
                4.[0123][.,-]*) true ;;
                4.[0123]) true ;;
                *) false ;;
            esac && \
            case "`ver_gnucc "$CXX"`" in
                [123].*) true ;;
                4.[0123][.,-]*) true ;;
                4.[0123]) true ;;
                *) false ;;
            esac
        ) ; then
            echo "NOTE: default GCC here is very old, do we have a CLANG instead?.." >&2
            COMPILER_FAMILY="GCC_OLD"
        fi

        if [ -z "$COMPILER_FAMILY" ] || [ "$COMPILER_FAMILY" = "GCC_OLD" ]; then
            if is_clang "clang" && is_clang "clang++" ; then
                # Autoconf would pick this by default
                [ "$COMPILER_FAMILY" = "GCC_OLD" ] && CC="" && CXX=""
                COMPILER_FAMILY="CLANG"
                [ -n "$CC" ]  || CC=clang
                [ -n "$CXX" ] || CXX=clang++
                export CC CXX
            elif is_clang "cc" && is_clang "c++" ; then
                [ "$COMPILER_FAMILY" = "GCC_OLD" ] && CC="" && CXX=""
                COMPILER_FAMILY="CLANG"
                [ -n "$CC" ]  || CC=cc
                [ -n "$CXX" ] || CXX=c++
                export CC CXX
            fi
        fi

        if [ "$COMPILER_FAMILY" = "GCC_OLD" ]; then
            COMPILER_FAMILY="GCC"
        fi
    fi

    if [ -n "$CPP" ] ; then
        # Note: can be a multi-token name like "clang -E" or just not a full pathname
        ( [ -x "$CPP" ] || $CPP --help >/dev/null 2>/dev/null ) && export CPP
    else
        # Avoid "cpp" directly as it may be too "traditional"
        case "$COMPILER_FAMILY" in
            CLANG*|GCC*) CPP="$CC -E" && export CPP ;;
            *) if is_gnucc "cpp" ; then
                CPP=cpp && export CPP
               fi ;;
        esac
    fi
}

optional_ensure_ccache() {
    # Prepare PATH, CC, CXX envvars to use ccache (if enabled, applicable and available)
    # See also optional_prepare_ccache()
    if [ "$HAVE_CCACHE" = yes ] && [ "${COMPILER_FAMILY}" = GCC -o "${COMPILER_FAMILY}" = CLANG ]; then
        if [ -n "${CI_CCACHE_SYMLINKDIR}" ]; then
            echo "INFO: Using ccache via PATH preferring tool names in ${CI_CCACHE_SYMLINKDIR}" >&2
            PATH="${CI_CCACHE_SYMLINKDIR}:$PATH"
            export PATH
        else
            case "$CC" in
                "") ;; # skip
                *ccache*) ;; # already requested to use ccache
                *) CC="ccache $CC" ;;
            esac
            case "$CXX" in
                "") ;; # skip
                *ccache*) ;; # already requested to use ccache
                *) CXX="ccache $CXX" ;;
            esac
            # No-op for CPP currently
        fi
        if [ -n "$CC" ] && [ -n "${CI_CCACHE_SYMLINKDIR}" ]; then
          if [ -x "${CI_CCACHE_SYMLINKDIR}/`basename "$CC"`" ]; then
            case "$CC" in
                *ccache*) ;;
                */*) DIR_CC="`dirname "$CC"`" && [ -n "$DIR_CC" ] && DIR_CC="`cd "$DIR_CC" && pwd `" && [ -n "$DIR_CC" ] && [ -d "$DIR_CC" ] || DIR_CC=""
                    [ -z "$CCACHE_PATH" ] && CCACHE_PATH="$DIR_CC" || \
                    if echo "$CCACHE_PATH" | grep -E '(^'"$DIR_CC"':.*|^'"$DIR_CC"'$|:'"$DIR_CC"':|:'"$DIR_CC"'$)' ; then
                        CCACHE_PATH="$DIR_CC:$CCACHE_PATH"
                    fi
                    ;;
            esac
            CC="${CI_CCACHE_SYMLINKDIR}/`basename "$CC"`"
          else
            CC="ccache $CC"
          fi
        fi
        if [ -n "$CXX" ] && [ -n "${CI_CCACHE_SYMLINKDIR}" ]; then
          if [ -x "${CI_CCACHE_SYMLINKDIR}/`basename "$CXX"`" ]; then
            case "$CXX" in
                *ccache*) ;;
                */*) DIR_CXX="`dirname "$CXX"`" && [ -n "$DIR_CXX" ] && DIR_CXX="`cd "$DIR_CXX" && pwd `" && [ -n "$DIR_CXX" ] && [ -d "$DIR_CXX" ] || DIR_CXX=""
                    [ -z "$CCACHE_PATH" ] && CCACHE_PATH="$DIR_CXX" || \
                    if echo "$CCACHE_PATH" | grep -E '(^'"$DIR_CXX"':.*|^'"$DIR_CXX"'$|:'"$DIR_CXX"':|:'"$DIR_CXX"'$)' ; then
                        CCACHE_PATH="$DIR_CXX:$CCACHE_PATH"
                    fi
                    ;;
            esac
            CXX="${CI_CCACHE_SYMLINKDIR}/`basename "$CXX"`"
          else
            CXX="ccache $CXX"
          fi
        fi
        if [ -n "$CPP" ] && [ -n "${CI_CCACHE_SYMLINKDIR}" ] \
        && [ -x "${CI_CCACHE_SYMLINKDIR}/`basename "$CPP"`" ]; then
            case "$CPP" in
                *ccache*) ;;
                */*) DIR_CPP="`dirname "$CPP"`" && [ -n "$DIR_CPP" ] && DIR_CPP="`cd "$DIR_CPP" && pwd `" && [ -n "$DIR_CPP" ] && [ -d "$DIR_CPP" ] || DIR_CPP=""
                    [ -z "$CCACHE_PATH" ] && CCACHE_PATH="$DIR_CPP" || \
                    if echo "$CCACHE_PATH" | grep -E '(^'"$DIR_CPP"':.*|^'"$DIR_CPP"'$|:'"$DIR_CPP"':|:'"$DIR_CPP"'$)' ; then
                        CCACHE_PATH="$DIR_CPP:$CCACHE_PATH"
                    fi
                    ;;
            esac
            CPP="${CI_CCACHE_SYMLINKDIR}/`basename "$CPP"`"
        else
            : # CPP="ccache $CPP"
        fi

        CCACHE_BASEDIR="${PWD}"
        export CCACHE_BASEDIR

        return 0
    fi

    # Not enabled or incompatible compiler
    # Still, if ccache somehow gets used, let it
    # know the correct BASEDIR of the built project
    CCACHE_BASEDIR="${PWD}"
    export CCACHE_BASEDIR

    return 1
}

if [ -z "$TMPDIR" ]; then
    echo "WARNING: TMPDIR not set, trying to guess"
    if [ -d /tmp -a -w /tmp ] ; then
        TMPDIR=/tmp
        export TMPDIR
    fi
fi

if [ -z "$TMPDIR" ]; then
    echo "WARNING: TMPDIR still not set, some tools (notably clang) can fail"
fi

# For two-phase builds (quick parallel make first, sequential retry if failed)
# how verbose should that first phase be? Nothing, automake list of ops, CLIs?
# See build_to_only_catch_errors_target() for a consumer of this setting.
case "${CI_PARMAKE_VERBOSITY-}" in
    silent|quiet|verbose|default) ;;
    *) CI_PARMAKE_VERBOSITY=silent ;;
esac

# Set up the parallel make with reasonable limits, using several ways to
# gather and calculate this information. Note that "psrinfo" count is not
# an honest approach (there may be limits of current CPU set etc.) but is
# a better upper bound than nothing...
[ -n "$NCPUS" ] || { \
    NCPUS="`/usr/bin/getconf _NPROCESSORS_ONLN`" || \
    NCPUS="`/usr/bin/getconf NPROCESSORS_ONLN`" || \
    NCPUS="`cat /proc/cpuinfo | grep -wc processor`" || \
    { [ -x /usr/sbin/psrinfo ] && NCPUS="`/usr/sbin/psrinfo | wc -l`"; } \
    || NCPUS=1; } 2>/dev/null
[ x"$NCPUS" != x -a "$NCPUS" -ge 1 ] || NCPUS=1

[ x"$NPARMAKES" = x ] && { NPARMAKES="`expr "$NCPUS" '*' 2`" || NPARMAKES=2; }
[ x"$NPARMAKES" != x -a "$NPARMAKES" -ge 1 ] || NPARMAKES=2
[ x"$MAXPARMAKES" != x ] && [ "$MAXPARMAKES" -ge 1 ] && \
    [ "$NPARMAKES" -gt "$MAXPARMAKES" ] && \
    echo "INFO: Detected or requested NPARMAKES=$NPARMAKES," \
        "however a limit of MAXPARMAKES=$MAXPARMAKES was configured" && \
    NPARMAKES="$MAXPARMAKES"

# GNU make allows to limit spawning of jobs by load average of the host,
# where LA is (roughly) the average amount over the last {timeframe} of
# queued processes that are ready to compute but must wait for CPU.
# The rough estimate for VM builders however seems that they always have
# some non-trivial LA, so we set the default limit per CPU relatively high.
[ x"$PARMAKE_LA_LIMIT" = x ] && PARMAKE_LA_LIMIT="`expr $NCPUS '*' 8`".0

# After all the tunable options above, this is the one which takes effect
# for actual builds with parallel phases. Specify a whitespace to neuter.
if [ -z "$PARMAKE_FLAGS" ]; then
    PARMAKE_FLAGS="-j $NPARMAKES"
    if LANG=C LC_ALL=C "$MAKE" --version 2>&1 | grep -E 'GNU Make|Free Software Foundation' > /dev/null ; then
        PARMAKE_FLAGS="$PARMAKE_FLAGS -l $PARMAKE_LA_LIMIT"
        echo "Parallel builds would spawn up to $NPARMAKES jobs (detected $NCPUS CPUs), or peak out at $PARMAKE_LA_LIMIT system load average" >&2
    else
        echo "Parallel builds would spawn up to $NPARMAKES jobs (detected $NCPUS CPUs)" >&2
    fi
fi

# Stash the value provided by caller, if any
ORIG_DISTCHECK_TGT="${DISTCHECK_TGT-}"

# CI builds on Jenkins
[ -z "$NODE_LABELS" ] || \
for L in $NODE_LABELS ; do
    case "$L" in
        "NUT_BUILD_CAPS=cppunit=no")
            [ -n "$CANBUILD_CPPUNIT_TESTS" ] || CANBUILD_CPPUNIT_TESTS=no ;;
        "NUT_BUILD_CAPS=cppunit=no-gcc")
            [ -n "$CANBUILD_CPPUNIT_TESTS" ] || CANBUILD_CPPUNIT_TESTS=no-gcc ;;
        "NUT_BUILD_CAPS=cppunit=no-clang")
            [ -n "$CANBUILD_CPPUNIT_TESTS" ] || CANBUILD_CPPUNIT_TESTS=no-clang ;;
        "NUT_BUILD_CAPS=cppunit"|"NUT_BUILD_CAPS=cppunit=yes")
            [ -n "$CANBUILD_CPPUNIT_TESTS" ] || CANBUILD_CPPUNIT_TESTS=yes ;;

        # This should cover both the --with-nutconf tool setting
        # and the cppunit tests for it (if active per above).
        # By default we would nowadays guess (requires C++11).
        "NUT_BUILD_CAPS=nutconf=no")
            [ -n "$CANBUILD_NUTCONF" ] || CANBUILD_NUTCONF=no ;;
        "NUT_BUILD_CAPS=nutconf=no-gcc")
            [ -n "$CANBUILD_NUTCONF" ] || CANBUILD_NUTCONF=no-gcc ;;
        "NUT_BUILD_CAPS=nutconf=no-clang")
            [ -n "$CANBUILD_NUTCONF" ] || CANBUILD_NUTCONF=no-clang ;;
        "NUT_BUILD_CAPS=nutconf"|"NUT_BUILD_CAPS=nutconf=yes")
            [ -n "$CANBUILD_NUTCONF" ] || CANBUILD_NUTCONF=yes ;;

        # Some (QEMU) builders have issues running valgrind as a tool
        "NUT_BUILD_CAPS=valgrind=no")
            [ -n "$CANBUILD_VALGRIND_TESTS" ] || CANBUILD_VALGRIND_TESTS=no ;;
        "NUT_BUILD_CAPS=valgrind"|"NUT_BUILD_CAPS=valgrind=yes")
            [ -n "$CANBUILD_VALGRIND_TESTS" ] || CANBUILD_VALGRIND_TESTS=yes ;;

        "NUT_BUILD_CAPS=cppcheck=no")
            [ -n "$CANBUILD_CPPCHECK_TESTS" ] || CANBUILD_CPPCHECK_TESTS=no ;;
        "NUT_BUILD_CAPS=cppcheck"|"NUT_BUILD_CAPS=cppcheck=yes")
            [ -n "$CANBUILD_CPPCHECK_TESTS" ] || CANBUILD_CPPCHECK_TESTS=yes ;;

        # Some workers (presumably where several executors or separate
        # Jenkins agents) are enabled randomly fail NIT tests, once in
        # a hundred runs or so. This option allows isolated workers to
        # proclaim they are safe places to "make check-NIT" (and we can
        # see if that is true, over time).
        "NUT_BUILD_CAPS=NIT=no")
            [ -n "$CANBUILD_NIT_TESTS" ] || CANBUILD_NIT_TESTS=no ;;
        "NUT_BUILD_CAPS=NIT"|"NUT_BUILD_CAPS=NIT=yes")
            [ -n "$CANBUILD_NIT_TESTS" ] || CANBUILD_NIT_TESTS=yes ;;

        "NUT_BUILD_CAPS=docs:man=no")
            [ -n "${DISTCHECK_TGT-}" ] || DISTCHECK_TGT="distcheck-ci"
            [ -n "$CANBUILD_DOCS_MAN" ] || CANBUILD_DOCS_MAN=no ;;
        "NUT_BUILD_CAPS=docs:man"|"NUT_BUILD_CAPS=docs:man=yes")
            [ -n "$CANBUILD_DOCS_MAN" ] || CANBUILD_DOCS_MAN=yes ;;

        "NUT_BUILD_CAPS=docs:all=no")
            [ -n "${DISTCHECK_TGT-}" ] || DISTCHECK_TGT="distcheck-ci"
            [ -n "$CANBUILD_DOCS_ALL" ] || CANBUILD_DOCS_ALL=no ;;
        "NUT_BUILD_CAPS=docs:all"|"NUT_BUILD_CAPS=docs:all=yes")
            [ -n "$CANBUILD_DOCS_ALL" ] || CANBUILD_DOCS_ALL=yes ;;

        "NUT_BUILD_CAPS=drivers:all=no")
            ( [ -n "${DISTCHECK_TGT-}" ] && [ x"${DISTCHECK_TGT-}" != x"distcheck-ci" ] ) || DISTCHECK_TGT="distcheck-light"
            [ -n "$CANBUILD_DRIVERS_ALL" ] || CANBUILD_DRIVERS_ALL=no ;;
        "NUT_BUILD_CAPS=drivers:all"|"NUT_BUILD_CAPS=drivers:all=yes")
            [ -n "$CANBUILD_DRIVERS_ALL" ] || CANBUILD_DRIVERS_ALL=yes ;;

        "NUT_BUILD_CAPS=cgi=no")
            ( [ -n "${DISTCHECK_TGT-}" ] && [ x"${DISTCHECK_TGT-}" != x"distcheck-ci" ] ) || DISTCHECK_TGT="distcheck-light"
            [ -n "$CANBUILD_LIBGD_CGI" ] || CANBUILD_LIBGD_CGI=no ;;
        "NUT_BUILD_CAPS=cgi"|"NUT_BUILD_CAPS=cgi=yes")
            [ -n "$CANBUILD_LIBGD_CGI" ] || CANBUILD_LIBGD_CGI=yes ;;

        # Currently for nut-scanner, might be more later - hence agnostic naming:
        "NUT_BUILD_CAPS=libltdl=no")
            ( [ -n "${DISTCHECK_TGT-}" ] && [ x"${DISTCHECK_TGT-}" != x"distcheck-ci" ] ) || DISTCHECK_TGT="distcheck-light"
            [ -n "$CANBUILD_WITH_LIBLTDL" ] || CANBUILD_WITH_LIBLTDL=no ;;
        "NUT_BUILD_CAPS=libltdl"|"NUT_BUILD_CAPS=libltdl=yes")
            [ -n "$CANBUILD_WITH_LIBLTDL" ] || CANBUILD_WITH_LIBLTDL=yes ;;
    esac
done

if [ -z "$CI_OS_NAME" ]; then
    # Check for dynaMatrix node labels support and map into a simple
    # classification styled after (compatible with) that in Travis CI
    for CI_OS_HINT in \
        "$OS_FAMILY-$OS_DISTRO" \
        "`grep = /etc/os-release 2>/dev/null`" \
        "`cat /etc/release 2>/dev/null`" \
        "`uname -o 2>/dev/null`" \
        "`uname -s -r -v 2>/dev/null`" \
        "`uname -a`" \
        "`uname`" \
    ; do
        [ -z "$CI_OS_HINT" -o "$CI_OS_HINT" = "-" ] || break
    done

    case "`echo "$CI_OS_HINT" | tr 'A-Z' 'a-z'`" in
        *freebsd*)
            CI_OS_NAME="freebsd" ;;
        *openbsd*)
            CI_OS_NAME="openbsd" ;;
        *netbsd*)
            CI_OS_NAME="netbsd" ;;
        *debian*|*ubuntu*)
            CI_OS_NAME="debian" ;;
        *centos*|*fedora*|*redhat*|*rhel*)
            CI_OS_NAME="centos" ;;
        *linux*)
            CI_OS_NAME="linux" ;;
        *msys2*)
            CI_OS_NAME="windows-msys2" ;;
        *mingw*64*)
            CI_OS_NAME="windows-mingw64" ;;
        *mingw*32*)
            CI_OS_NAME="windows-mingw32" ;;
        *windows*)
            CI_OS_NAME="windows" ;;
        *[Mm]ac*|*arwin*|*[Oo][Ss][Xx]*)
            CI_OS_NAME="osx" ;;
        *openindiana*)
            CI_OS_NAME="openindiana" ;;
        *omnios*)
            CI_OS_NAME="omnios" ;;
        *bsd*)
            CI_OS_NAME="bsd" ;;
        *illumos*)
            CI_OS_NAME="illumos" ;;
        *solaris*)
            CI_OS_NAME="solaris" ;;
        *sunos*)
            CI_OS_NAME="sunos" ;;
        "-") ;;
        *)  echo "WARNING: Could not recognize CI_OS_NAME from CI_OS_HINT='$CI_OS_HINT', update './ci_build.sh' if needed" >&2
            if [ "$OS_FAMILY-$OS_DISTRO" != "-" ]; then
                echo "WARNING: I was told that OS_FAMILY='$OS_FAMILY' and OS_DISTRO='$OS_DISTRO'" >&2
            fi
            ;;
    esac
    [ -z "$CI_OS_NAME" ] || echo "INFO: Detected CI_OS_NAME='$CI_OS_NAME'" >&2
fi

# CI builds on Travis
[ -n "$CI_OS_NAME" ] || CI_OS_NAME="$TRAVIS_OS_NAME"

case "${CI_OS_NAME}" in
    windows-msys2)
        # No-op: we seem to pass builds on MSYS2 anyway even without
        # these flags, and a populated CFLAGS happens to be toxic to
        # ccache builds in that distribution (as of 2022-2025 at least)
        ;;
    windows*)
        # At the moment WIN32 builds are quite particular in their
        # desires, for headers to declare what is needed, and yet
        # there is currently not much real variation in supportable
        # build environment (mingw variants). Lest we hardcode
        # stuff in configure script, define some here:
        case "$CFLAGS" in
            *-D_POSIX=*) ;;
            *) CFLAGS="$CFLAGS -D_POSIX=1" ;;
        esac
        case "$CFLAGS" in
            *-D_POSIX_C_SOURCE=*) ;;
            *) CFLAGS="$CFLAGS -D_POSIX_C_SOURCE=200112L" ;;
        esac
        case "$CFLAGS" in
            *-D_WIN32_WINNT=*) ;;
            *) CFLAGS="$CFLAGS -D_WIN32_WINNT=0xffff" ;;
        esac

        case "$CXXFLAGS" in
            *-D_POSIX=*) ;;
            *) CXXFLAGS="$CXXFLAGS -D_POSIX=1" ;;
        esac
        case "$CXXFLAGS" in
            *-D_POSIX_C_SOURCE=*) ;;
            *) CXXFLAGS="$CXXFLAGS -D_POSIX_C_SOURCE=200112L" ;;
        esac
        case "$CXXFLAGS" in
            *-D_WIN32_WINNT=*) ;;
            *) CXXFLAGS="$CXXFLAGS -D_WIN32_WINNT=0xffff" ;;
        esac
        ;;
esac

# Analyze some environmental choices
if [ -z "${CANBUILD_LIBGD_CGI-}" ]; then
    # No prereq dll and headers on win so far
    [[ "$CI_OS_NAME" = "windows" ]] && CANBUILD_LIBGD_CGI=no

    # NUT CI farm with Jenkins can build it; Travis could not
    [[ "$CI_OS_NAME" = "freebsd" ]] && CANBUILD_LIBGD_CGI=yes \
    || { [[ "$TRAVIS_OS_NAME" = "freebsd" ]] && CANBUILD_LIBGD_CGI=no ; }

    # See also below for some compiler-dependent decisions
fi

detect_platform_CANBUILD_LIBGD_CGI() {
    # Call after optional_prepare_compiler_family()!
    if [ -z "${CANBUILD_LIBGD_CGI-}" ]; then
        if [[ "$CI_OS_NAME" = "openindiana" ]] ; then
            # For some reason, here gcc-4.x (4.4.4, 4.9) have a problem with
            # configure-time checks of libgd; newer compilers fare okay.
            # Feel free to revise this if the distro packages are fixed
            # (or the way configure script and further build uses them).
            # UPDATE: Per https://github.com/networkupstools/nut/pull/1089
            # This is a systems issue (in current OpenIndiana 2021.04 built
            # with a newer GCC version, the older GCC is not ABI compatible
            # with the libgd shared object file). Maybe this warrants later
            # caring about not just the CI_OS_NAME but also CI_OS_RELEASE...
            if [[ "$COMPILER_FAMILY" = "GCC" ]]; then
                case "`LANG=C $CC --version | head -1`" in
                    *[\ -][01234].*)
                        echo "WARNING: Seems we are running with gcc-4.x or older on $CI_OS_NAME, which last had known issues with libgd; disabling CGI for this build"
                        CANBUILD_LIBGD_CGI=no
                        ;;
                    *)
                        case "${ARCH}${BITS}${ARCH_BITS}" in
                            *64*|*sparcv9*) ;;
                            *)
                                # GCC-7 (maybe other older compilers) could default
                                # to 32-bit builds, and the 32-bit libfontconfig.so
                                # and libfreetype.so are absent for some years now
                                # (while libgd.so still claims to exist).
                                echo "WARNING: Seems we are running with gcc on $CI_OS_NAME, which last had known issues with libgd on non-64-bit builds; making CGI optional for this build"
                                CANBUILD_LIBGD_CGI=auto
                                ;;
                        esac
                        ;;
                esac
            else
                case "${ARCH}${BITS}${ARCH_BITS}" in
                    *64*|*sparcv9*) ;;
                    *)
                        echo "WARNING: Seems we are running with $COMPILER_FAMILY on $CI_OS_NAME, which last had known issues with libgd on non-64-bit builds; making CGI optional for this build"
                        CANBUILD_LIBGD_CGI=auto
                        ;;
                esac
            fi
        fi
    fi
}

if [ -z "${PKG_CONFIG-}" ]; then
    # Default to using one from PATH, if any - mostly for config tuning done
    # below in this script
    # DO NOT "export" it here so configure script can find one for the build
    PKG_CONFIG="pkg-config"
fi

detect_platform_PKG_CONFIG_PATH_and_FLAGS() {
    # Some systems want a custom PKG_CONFIG_PATH which would be prepended
    # to whatever the callers might have provided as their PKG_CONFIG_PATH.
    # Optionally provided by some CI build scenarios: DEFAULT_PKG_CONFIG_PATH
    # On some platforms also barges in to CONFIG_OPTS[] (should exist!),
    # PATH, CFLAGS, CXXFLAGS, LDFLAGS, XML_CATALOG_FILES et al.
    #
    # Caller can override by OVERRIDE_PKG_CONFIG_PATH (ignore other values
    # then, including a PKG_CONFIG_PATH), where a "-" value leaves it empty.
    SYS_PKG_CONFIG_PATH="" # Let the OS guess... usually
    BUILTIN_PKG_CONFIG_PATH="`pkg-config --variable pc_path pkg-config`" || BUILTIN_PKG_CONFIG_PATH=""
    case "`echo "$CI_OS_NAME" | tr 'A-Z' 'a-z'`" in
        *openindiana*|*omnios*|*solaris*|*illumos*|*sunos*)
            _ARCHES="${ARCH-}"
            _BITS="${BITS-}"
            _ISA1=""

            [ -n "${_BITS}" ] || \
            case "${CC}${CXX}${CFLAGS}${CXXFLAGS}${LDFLAGS}" in
                *-m64*) _BITS=64 ;;
                *-m32*) _BITS=32 ;;
                *)  case "${ARCH-}${ARCH_BITS-}" in
                        *64*|*sparcv9*) _BITS=64 ;;
                        *32*|*86*|*sparcv7*|*sparc) _BITS=32 ;;
                        *)  _ISA1="`isainfo | awk '{print $1}'`"
                            case "${_ISA1}" in
                                *64*|*sparcv9*) _BITS=64 ;;
                                *32*|*86*|*sparcv7*|*sparc) _BITS=32 ;;
                            esac
                            ;;
                    esac
                    ;;
            esac

            # Consider also `gcc -v`/`clang -v`: their "Target:" line exposes the
            # triplet compiler was built to run on, e.g. "x86_64-pc-solaris2.11"
            case "${_ARCHES}${ARCH_BITS-}" in
                *amd64*|*x86_64*) _ARCHES="amd64" ;;
                *sparcv9*) _ARCHES="sparcv9" ;;
                *86*) _ARCHES="i86pc i386" ;;
                *sparcv7*|*sparc) _ARCHES="sparcv7 sparc" ;;
                *)  [ -n "${_ISA1}" ] || _ISA1="`isainfo | awk '{print $1}'`"
                    case "${_ISA1}" in
                        *amd64*|*x86_64*) _ARCHES="amd64" ;;
                        *sparcv9*) _ARCHES="sparcv9" ;;
                        *86*) _ARCHES="i86pc i386" ;;
                        *sparcv7*|*sparc) _ARCHES="sparcv7 sparc" ;;
                    esac
                    ;;
            esac

            # Pile it on, strip extra ":" and dedup entries later
            for D in \
                "/opt/ooce/lib" \
                "/usr/lib" \
            ; do
                if [ -d "$D" ] ; then
                    _ADDSHORT=false
                    if [ -n "${_BITS}" ] ; then
                        if [ -d "${D}/${_BITS}/pkgconfig" ] ; then
                            SYS_PKG_CONFIG_PATH="${SYS_PKG_CONFIG_PATH}:${D}/${_BITS}/pkgconfig"
                            # Here and below: hot-fix for https://github.com/networkupstools/nut/issues/2782
                            # situation on OmniOS with Extra repository,
                            # assumed only useful if we use it via pkgconfig
                            case "${D}" in
                                /usr/lib) ;;
                                *) LDFLAGS="${LDFLAGS} -R${D}/${_BITS}" ;;
                            esac
                        else
                            if [ -d "${D}/pkgconfig" ] ; then
                                case "`LANG=C LC_ALL=C file $(ls -1 $D/*.so | head -1)`" in
                                    *"ELF ${_BITS}-bit"*) _ADDSHORT=true ;;
                                esac
                            fi
                        fi
                    fi
                    for _ARCH in $_ARCHES ; do
                        if [ -d "${D}/${_ARCH}/pkgconfig" ] ; then
                            SYS_PKG_CONFIG_PATH="${SYS_PKG_CONFIG_PATH}:${D}/${_ARCH}/pkgconfig"
                            case "${D}" in
                                /usr/lib) ;;
                                *) LDFLAGS="${LDFLAGS} -R${D}/${_ARCH}" ;;
                            esac
                        else
                            if [ -d "${D}/pkgconfig" ] ; then
                                case "`LANG=C LC_ALL=C file $(ls -1 $D/*.so | head -1)`" in
                                    *"ELF 32-bit"*" SPARC "*)
                                        case "${_ARCH}" in
                                            sparc|sparcv7) _ADDSHORT=true ;;
                                        esac
                                        ;;
                                    *"ELF 64-bit"*" SPARCV9 "*)
                                        case "${_ARCH}" in
                                            sparcv9) _ADDSHORT=true ;;
                                        esac
                                        ;;
                                    *"ELF 32-bit"*" 80386 "*)
                                        case "${_ARCH}" in
                                            i386|i86pc) _ADDSHORT=true ;;
                                        esac
                                        ;;
                                    *"ELF 64-bit"*" AMD64 "*)
                                        case "${_ARCH}" in
                                            amd64) _ADDSHORT=true ;;
                                        esac
                                        ;;
                                esac
                            fi
                        fi
                    done
                    if [ "${_ADDSHORT}" = true ] ; then
                        SYS_PKG_CONFIG_PATH="${SYS_PKG_CONFIG_PATH}:${D}/pkgconfig"
                        case "${D}" in
                            /usr/lib) ;;
                            *) LDFLAGS="${LDFLAGS} -R${D}" ;;
                        esac
                    fi
                fi
            done

            # Last option if others are lacking
            if [ -d /usr/lib/pkgconfig ] ; then
                SYS_PKG_CONFIG_PATH="${SYS_PKG_CONFIG_PATH}:/usr/lib/pkgconfig"
            fi
            unset _ADDSHORT _BITS _ARCH _ARCHES D

            # OmniOS CE "Extra" repository
            case "$PATH" in
                /opt/ooce/bin|*:/opt/ooce/bin|/opt/ooce/bin:*|*:/opt/ooce/bin:*) ;;
                *) if [ -d "/opt/ooce/bin" ] ; then PATH="/opt/ooce/bin:${PATH}" ; fi ;;
            esac
            ;;
        *darwin*|*macos*|*osx*)
            # Architecture-dependent base dir, e.g.
            # * /usr/local on macos x86
            # * /opt/homebrew on macos Apple Silicon
            if [ -n "${HOMEBREW_PREFIX-}" -a -d "${HOMEBREW_PREFIX-}" ]; then
                echo "Homebrew: export general pkg-config location and C/C++/LD flags for the platform"
                SYS_PKG_CONFIG_PATH="${HOMEBREW_PREFIX}/lib/pkgconfig"
                CFLAGS="${CFLAGS-} -Wno-poison-system-directories -Wno-deprecated-declarations -isystem ${HOMEBREW_PREFIX}/include -I${HOMEBREW_PREFIX}/include"
                #CPPFLAGS="${CPPFLAGS-} -Wno-poison-system-directories -Wno-deprecated-declarations -isystem ${HOMEBREW_PREFIX}/include -I${HOMEBREW_PREFIX}/include"
                CXXFLAGS="${CXXFLAGS-} -Wno-poison-system-directories -isystem ${HOMEBREW_PREFIX}/include -I${HOMEBREW_PREFIX}/include"
                LDFLAGS="${LDFLAGS-} -L${HOMEBREW_PREFIX}/lib"

                # Net-SNMP "clashes" with system-provided tools (but no header/lib)
                # so explicit args are needed
                checkFSobj="${HOMEBREW_PREFIX}/opt/net-snmp/lib/pkgconfig"
                if [ -d "$checkFSobj" -a ! -e "${HOMEBREW_PREFIX}/lib/pkgconfig/netsnmp.pc" ] ; then
                    echo "Homebrew: export pkg-config location for Net-SNMP"
                    SYS_PKG_CONFIG_PATH="$SYS_PKG_CONFIG_PATH:$checkFSobj"
                    #echo "Homebrew: export flags for Net-SNMP"
                    #CONFIG_OPTS+=("--with-snmp-includes=-isystem ${HOMEBREW_PREFIX}/opt/net-snmp/include -I${HOMEBREW_PREFIX}/opt/net-snmp/include")
                    #CONFIG_OPTS+=("--with-snmp-libs=-L${HOMEBREW_PREFIX}/opt/net-snmp/lib")
                fi

                if [ -d "${HOMEBREW_PREFIX}/opt/net-snmp/include" -a -d "${HOMEBREW_PREFIX}/include/openssl" ]; then
                    # TODO? Check netsnmp.pc for Libs.private with
                    #   -L/opt/homebrew/opt/openssl@1.1/lib
                    # or
                    #   -L/usr/local/opt/openssl@3/lib
                    # among other options to derive the exact version
                    # it wants, and serve that include path here
                    echo "Homebrew: export configure options for Net-SNMP with default OpenSSL headers (too intimate on Homebrew)"
                    CONFIG_OPTS+=("--with-snmp-includes=-isystem ${HOMEBREW_PREFIX}/opt/net-snmp/include -I${HOMEBREW_PREFIX}/opt/net-snmp/include -isystem ${HOMEBREW_PREFIX}/include -I${HOMEBREW_PREFIX}/include")
                    CONFIG_OPTS+=("--with-snmp-libs=-L${HOMEBREW_PREFIX}/opt/net-snmp/lib -lnetsnmp")
                fi

                # A bit hackish to check this outside `configure`, but...
                if [ -s "${HOMEBREW_PREFIX-}/include/ltdl.h" ] ; then
                    echo "Homebrew: export flags for LibLTDL"
                    # The m4 script clear default CFLAGS/LIBS so benefit from new ones
                    CONFIG_OPTS+=("--with-libltdl-includes=-isystem ${HOMEBREW_PREFIX}/include -I${HOMEBREW_PREFIX}/include")
                    CONFIG_OPTS+=("--with-libltdl-libs=-L${HOMEBREW_PREFIX}/lib -lltdl")
                fi

                if [ -z "${XML_CATALOG_FILES-}" ] ; then
                    checkFSobj="${HOMEBREW_PREFIX}/etc/xml/catalog"
                    if [ -e "$checkFSobj" ] ; then
                        echo "Homebrew: export XML_CATALOG_FILES='$checkFSobj' for asciidoc et al"
                        XML_CATALOG_FILES="$checkFSobj"
                        export XML_CATALOG_FILES
                    fi
                fi
            else
                echo "WARNING: It seems you are building on MacOS, but HOMEBREW_PREFIX is not set or valid."
                echo 'If you do use this build system, try running   eval "$(brew shellenv)"'
                echo "in your terminal or shell profile, it can help with auto-detection of some features!"
            fi
            ;;
    esac

    if [ -n "${OVERRIDE_PKG_CONFIG_PATH-}" ] ; then
        if [ x"${OVERRIDE_PKG_CONFIG_PATH}" = x- ] ; then
            PKG_CONFIG_PATH=""
        else
            PKG_CONFIG_PATH="${OVERRIDE_PKG_CONFIG_PATH}"
        fi
        return
    fi

    # Do not check for existence of non-trivial values, we normalize the mess (if any)
    PKG_CONFIG_PATH="`echo "${DEFAULT_PKG_CONFIG_PATH-}:${SYS_PKG_CONFIG_PATH-}:${PKG_CONFIG_PATH-}:${BUILTIN_PKG_CONFIG_PATH-}" | normalize_path`"
}

# Would hold full path to the CONFIGURE_SCRIPT="${SCRIPTDIR}/${CONFIGURE_SCRIPT_FILENAME}"
CONFIGURE_SCRIPT=""
autogen_get_CONFIGURE_SCRIPT() {
    # Autogen once (delete the file if some scenario ever requires to re-autogen)
    if [ -n "${CONFIGURE_SCRIPT}" -a -s "${CONFIGURE_SCRIPT}" ] ; then return 0 ; fi

    pushd "${SCRIPTDIR}" || exit

    if [[ "$CI_OS_NAME" == "windows" ]] ; then
        # Debug once
        [ -n "$CONFIGURE_SCRIPT" ] || find . -ls
        CONFIGURE_SCRIPT="configure.bat"
    else
        CONFIGURE_SCRIPT="configure"
    fi

    if [ ! -s "./$CONFIGURE_SCRIPT" ]; then
        # Note: modern auto(re)conf requires pkg-config to generate the configure
        # script, so to stage the situation of building without one (as if on an
        # older system) we have to remove it when we already have the script.
        # This matches the use-case of distro-building from release tarballs that
        # include all needed pre-generated files to rely less on OS facilities.
        if [ "$CI_OS_NAME" = "windows" ] ; then
            $CI_TIME ./autogen.sh || true
        else
            $CI_TIME ./autogen.sh ### 2>/dev/null
        fi || exit
    fi

    # Retain the full path to configure script file
    CONFIGURE_SCRIPT="${SCRIPTDIR}/${CONFIGURE_SCRIPT}"

    popd || exit
}

configure_CI_BUILDDIR() {
    autogen_get_CONFIGURE_SCRIPT

    if [ "${CI_BUILDDIR}" != "." ]; then
        # Per above, we always start this routine in absolute $SCRIPTDIR
        echo "=== Running NUT build out-of-tree in ${CI_BUILDDIR}"
        mkdir -p "${CI_BUILDDIR}" && cd "${CI_BUILDDIR}" || exit
    fi
}

configure_nut() {
    configure_CI_BUILDDIR

    # Note: maintainer-clean checks remove this, and then some systems'
    # build toolchains noisily complain about missing LD path candidate
    if [ -n "$BUILD_PREFIX" ]; then
        # tmp/lib/
        mkdir -p "$BUILD_PREFIX"/lib
    fi
    if [ -n "$INST_PREFIX" ]; then
        # .inst/
        mkdir -p "$INST_PREFIX"
    fi

    # Help copy-pasting build setups from CI logs to terminal:
    local CONFIG_OPTS_STR="`for F in "${CONFIG_OPTS[@]}" ; do echo "'$F' " ; done`" ### | tr '\n' ' '`"
    while : ; do # Note the CI_SHELL_IS_FLAKY=true support below
      echo "=== CONFIGURING NUT: $CONFIGURE_SCRIPT ${CONFIG_OPTS_STR}"
      echo "=== CC='$CC' CXX='$CXX' CPP='$CPP'"
      [ -z "${CI_SHELL_IS_FLAKY-}" ] || echo "=== CI_SHELL_IS_FLAKY='$CI_SHELL_IS_FLAKY'"
      $CI_TIME $CONFIGURE_SCRIPT "${CONFIG_OPTS[@]}" \
      && echo "$0: configure phase complete (0)" >&2 \
      && return 0 \
      || { RES_CFG=$?
        echo "$0: configure phase complete ($RES_CFG)" >&2
        echo "FAILED ($RES_CFG) to configure nut, will dump config.log in a second to help troubleshoot CI" >&2
        echo "    (or press Ctrl+C to abort now if running interactively)" >&2
        sleep 5
        echo "=========== DUMPING config.log :"
        $GGREP -B 100 -A 1 'Cache variables' config.log 2>/dev/null \
        || cat config.log || true
        echo "=========== END OF config.log"

        if [ "${CI_SHELL_IS_FLAKY-}" = true ]; then
            # Real-life story from the trenches: there are weird systems
            # which fail ./configure in random spots not due to script's
            # quality. Then we'd just loop here.
            echo "WOULD BE FATAL: FAILED ($RES_CFG) to $CONFIGURE_SCRIPT ${CONFIG_OPTS[*]} -- but asked to loop trying" >&2
        else
            echo "FATAL: FAILED ($RES_CFG) to $CONFIGURE_SCRIPT ${CONFIG_OPTS[*]}" >&2
            echo "If you are sure this is not a fault of scripting or config option, try" >&2
            echo "    CI_SHELL_IS_FLAKY=true $0"
            exit $RES_CFG
        fi
       }
    done
}

build_to_only_catch_errors_target() {
    if [ $# = 0 ]; then
        # Re-enter with an arg list
        build_to_only_catch_errors_target all ; return $?
    fi

    # Sub-shells to avoid crashing with "unhandled" faults in "set -e" mode:
    ( echo "`date`: Starting the parallel build attempt (quietly to build what we can) for '$@' ..."; \
      if [ -n "$PARMAKE_FLAGS" ]; then
        echo "For parallel builds, '$PARMAKE_FLAGS' options would be used"
      fi
      if [ -n "$MAKEFLAGS" ]; then
        echo "Generally, MAKEFLAGS='$MAKEFLAGS' options would be passed"
      fi
      ( case "${CI_PARMAKE_VERBOSITY}" in
        silent)
          # Note: stderr would still expose errors and warnings (needed for
          # e.g. CI analysis of coding issues, even if not treated as fatal)
          $CI_TIME $MAKE $MAKE_FLAGS_QUIET -k $PARMAKE_FLAGS "$@" >/dev/null ;;
        quiet)
          $CI_TIME $MAKE $MAKE_FLAGS_QUIET -k $PARMAKE_FLAGS "$@" ;;
        silent)
          $CI_TIME $MAKE $MAKE_FLAGS_VERBOSE -k $PARMAKE_FLAGS "$@" ;;
        default)
          $CI_TIME $MAKE -k $PARMAKE_FLAGS "$@" ;;
      esac ) && echo "`date`: SUCCESS" ; ) || \
    ( RET=$?
      if [ "$CI_FAILFAST" = true ]; then
        echo "===== Aborting after parallel build attempt failure for '$*' because CI_FAILFAST=$CI_FAILFAST" >&2
        exit $RET
      fi
      echo "`date`: Starting the sequential build attempt (to list remaining files with errors considered fatal for this build configuration) for '$@'..."; \
      $CI_TIME $MAKE $MAKE_FLAGS_VERBOSE "$@" -k ) || return $?
    return 0
}

build_to_only_catch_errors_check() {
    # Specifically run (an optional) "make check"
    if [ "${CI_SKIP_CHECK}" = true ] ; then
        echo "`date`: SKIP: not starting a '$MAKE check' for quick sanity test of the products built with the current compiler and standards, because caller requested CI_SKIP_CHECK=true; plain build has just succeeded however"
        return 0
    fi

    echo "`date`: Starting a '$MAKE check' for quick sanity test of the products built with the current compiler and standards"
    $CI_TIME $MAKE $MAKE_FLAGS_QUIET check \
    && echo "`date`: SUCCESS" \
    || return $?

    return 0
}

build_to_only_catch_errors() {
    build_to_only_catch_errors_target all || return $?
    build_to_only_catch_errors_check || return $?
    return 0
}

ccache_stats() {
    local WHEN="$1"
    [ -n "$WHEN" ] || WHEN="some time around the"
    if [ "$HAVE_CCACHE" = yes ]; then
        if [ -d "$CCACHE_DIR" ]; then
            echo "CCache stats $WHEN build:"
            ccache -s || true
            # Some ccache versions support compression stats
            # This may take time on slower systems however
            # (and/or with larger cache contents) => off by default
            if [ x"${CI_CCACHE_STATS_COMPRESSION-}" = xtrue ]; then
                ccache -x 2>/dev/null || true
            fi
        else
            echo "WARNING: CCache stats $WHEN build: tool is enabled, but CCACHE_DIR='$CCACHE_DIR' was not found now" >&2
        fi
    fi
    return 0
}

check_gitignore() {
    # Optional envvars from caller: FILE_DESCR FILE_REGEX FILE_GLOB
    # and GIT_ARGS GIT_DIFF_SHOW
    local BUILT_TARGETS="$@"

    [ -n "${FILE_DESCR-}" ] || FILE_DESCR="some"
    # Note: regex actually used starts with catching Git markup, so
    # FILE_REGEX should not include that nor "^" line-start marker.
    # We also rule out files made by CI routines and this script.
    # NOTEL: In particular, we need build results of `make cppcheck`
    # later, so its recipe does not clean nor care for gitignore.
    [ -n "${FILE_REGEX-}" ] || FILE_REGEX='.*'
    # Shell-glob filename pattern for points of interest to git status
    # and git diff; note that filenames starting with a dot should be
    # reported by `git status -- '*'` and not hidden.
    [ -n "${FILE_GLOB-}" ] || FILE_GLOB="'*'"
    # Always filter these names away:
    FILE_GLOB_EXCLUDE="':!.ci*.log*' ':!VERSION_DEFAULT' ':!VERSION_FORCED*'"
    [ -n "${GIT_ARGS-}" ] || GIT_ARGS='' # e.g. GIT_ARGS="--ignored"
    # Display contents of the diff?
    # (Helps copy-paste from CI logs to source to amend quickly)
    [ -n "${GIT_DIFF_SHOW-}" ] || GIT_DIFF_SHOW=true
    [ -n "${BUILT_TARGETS-}" ] || BUILT_TARGETS="all? (usual default)"

    echo "=== Are GitIgnores good after '$MAKE $BUILT_TARGETS'? (should have no output below)"
    if [ ! -e .git ]; then
        echo "WARNING: Skipping the GitIgnores check after '$BUILT_TARGETS' because there is no `pwd`/.git anymore" >&2
        return 0
    fi

    # One invocation should report to log if there was any discrepancy
    # to report in the first place (GITOUT may be empty without error):
    GITOUT="`git status $GIT_ARGS -s -- ${FILE_GLOB} ${FILE_GLOB_EXCLUDE}`" \
    || { echo "WARNING: Could not query git repo while in `pwd`" >&2 ; GITOUT=""; }

    if [ -n "${GITOUT-}" ] ; then
        echo "$GITOUT" \
        | grep -E "${FILE_REGEX}"
    else
        echo "Got no output and no errors querying git repo while in `pwd`: seems clean" >&2
    fi
    echo "==="

    # Another invocation checks that there was nothing to complain about:
    if [ -n "`git status $GIT_ARGS -s ${FILE_GLOB} ${FILE_GLOB_EXCLUDE} | grep -E "^.. ${FILE_REGEX}"`" ] \
    && [ "$CI_REQUIRE_GOOD_GITIGNORE" != false ] \
    ; then
        echo "FATAL: There are changes in $FILE_DESCR files listed above - tracked sources should be updated in the PR (even if generated - not all builders can do so), and build products should be added to a .gitignore file, everything made should be cleaned and no tracked files should be removed! You can 'export CI_REQUIRE_GOOD_GITIGNORE=false' if appropriate." >&2
        if [ "$GIT_DIFF_SHOW" = true ]; then
            PAGER=cat git diff -- ${FILE_GLOB} ${FILE_GLOB_EXCLUDE} || true
        fi
        echo "==="
        return 1
    fi
    return 0
}

consider_cleanup_shortcut() {
    # Note: modern auto(re)conf requires pkg-config to generate the configure
    # script, so to stage the situation of building without one (as if on an
    # older system) we have to remove it when we already have the script.
    # This matches the use-case of distro-building from release tarballs that
    # include all needed pre-generated files to rely less on OS facilities.
    DO_REGENERATE=false
    if [ x"${CI_REGENERATE}" = xtrue ] ; then
        echo "=== Starting initial clean-up (from old build products): TAKING SHORTCUT because CI_REGENERATE='${CI_REGENERATE}'"
        DO_REGENERATE=true
    fi

    if [ -s Makefile ]; then
        if [ -n "`find "${SCRIPTDIR}" -name configure.ac -newer "${CI_BUILDDIR}"/configure`" ] \
        || [ -n "`find "${SCRIPTDIR}" -name '*.m4' -newer "${CI_BUILDDIR}"/configure`" ] \
        || [ -n "`find "${SCRIPTDIR}" -name Makefile.am -newer "${CI_BUILDDIR}"/Makefile`" ] \
        || [ -n "`find "${SCRIPTDIR}" -name Makefile.in -newer "${CI_BUILDDIR}"/Makefile`" ] \
        || [ -n "`find "${SCRIPTDIR}" -name Makefile.am -newer "${CI_BUILDDIR}"/Makefile.in`" ] \
        ; then
            # Avoid reconfiguring just for the sake of distclean
            echo "=== Starting initial clean-up (from old build products): TAKING SHORTCUT because recipes changed"
            DO_REGENERATE=true
        fi
    fi

    # When iterating configure.ac or m4 sources, we can end up with an
    # existing but useless script file - nuke it and restart from scratch!
    if [ -s "${CI_BUILDDIR}"/configure ] ; then
        if ! sh -n "${CI_BUILDDIR}"/configure 2>/dev/null ; then
            echo "=== Starting initial clean-up (from old build products): TAKING SHORTCUT because current configure script syntax is broken"
            DO_REGENERATE=true
        fi
    fi

    if $DO_REGENERATE ; then
        rm -f "${CI_BUILDDIR}"/Makefile "${CI_BUILDDIR}"/configure "${CI_BUILDDIR}"/include/config.h "${CI_BUILDDIR}"/include/config.h.in "${CI_BUILDDIR}"'/include/config.h.in~'
    fi
}

can_clean_check() {
    if [ "${DO_CLEAN_CHECK-}" = "no" ] ; then
        # NOTE: Not handling here particular DO_MAINTAINER_CLEAN_CHECK or DO_DIST_CLEAN_CHECK
        return 1
    fi
    if [ -s Makefile ] && [ -e .git ] ; then
        return 0
    fi
    return 1
}

optional_maintainer_clean_check() {
    if [ ! -e .git ]; then
        echo "Skipping maintainer-clean check because there is no .git" >&2
        return 0
    fi

    if [ ! -e Makefile ]; then
        echo "WARNING: Skipping maintainer-clean check because there is no Makefile (did we clean in a loop earlier?)" >&2
        return 0
    fi

    if [ "${DO_CLEAN_CHECK-}" = "no" ] || [ "${DO_MAINTAINER_CLEAN_CHECK-}" = "no" ] ; then
        echo "Skipping maintainer-clean check because recipe/developer said so"
    else
        [ -z "$CI_TIME" ] || echo "`date`: Starting maintainer-clean check of currently tested project..."

        # Note: currently Makefile.am has just a dummy "distcleancheck" rule
        case "$MAKE_FLAGS $DISTCHECK_FLAGS $PARMAKE_FLAGS $MAKE_FLAGS_CLEAN" in
        *V=0*)
            $CI_TIME $MAKE DISTCHECK_FLAGS="$DISTCHECK_FLAGS" $PARMAKE_FLAGS $MAKE_FLAGS_CLEAN maintainer-clean > /dev/null || return
            ;;
        *)
            $CI_TIME $MAKE DISTCHECK_FLAGS="$DISTCHECK_FLAGS" $PARMAKE_FLAGS $MAKE_FLAGS_CLEAN maintainer-clean || return
        esac

        GIT_ARGS="--ignored" check_gitignore "maintainer-clean" || return
    fi

    return 0
}

optional_dist_clean_check() {
    if [ ! -e .git ]; then
        echo "Skipping distclean check because there is no .git" >&2
        return 0
    fi

    if [ ! -e Makefile ]; then
        echo "WARNING: Skipping distclean check because there is no Makefile (did we clean in a loop earlier?)" >&2
        return 0
    fi

    if [ "${DO_CLEAN_CHECK-}" = "no" ] || [ "${DO_DIST_CLEAN_CHECK-}" = "no" ] ; then
        echo "Skipping distclean check because recipe/developer said so"
    else
        [ -z "$CI_TIME" ] || echo "`date`: Starting dist-clean check of currently tested project..."

        # Note: currently Makefile.am has just a dummy "distcleancheck" rule
        $CI_TIME $MAKE DISTCHECK_FLAGS="$DISTCHECK_FLAGS" $PARMAKE_FLAGS $MAKE_FLAGS_CLEAN distclean || return

        check_gitignore "distclean" || return
    fi
    return 0
}

# Link a few BUILD_TYPEs to command-line arguments
if [ -z "$BUILD_TYPE" ] ; then
    case "$1" in
        inplace)
            # Note: causes a developer-style build (not CI)
            shift
            BUILD_TYPE="inplace"
            ;;

        docs|docs=*|doc|doc=*)
            # Note: causes a developer-style build (not CI)
            # Arg will be passed to configure script as `--with-$1`
            BUILD_TYPE="$1"
            shift
            ;;

        win64|cross-windows-mingw64) BUILD_TYPE="cross-windows-mingw64" ; shift ;;

        win32|cross-windows-mingw32) BUILD_TYPE="cross-windows-mingw32" ; shift ;;

        win|windows|cross-windows-mingw) BUILD_TYPE="cross-windows-mingw" ; shift ;;

        spellcheck|spellcheck-interactive)
            # Note: this is a little hack to reduce typing
            # and scrolling in (docs) developer iterations.
            case "$CI_OS_NAME" in
                windows-msys2)
                    # https://github.com/msys2/MSYS2-packages/issues/2088
                    echo "=========================================================================="
                    echo "WARNING: some MSYS2 builds of aspell are broken with 'tex' support"
                    echo "Are you sure you run this in a functional build environment? Ctrl+C if not"
                    echo "=========================================================================="
                    sleep 5
                    ;;
                *)  if ! (command -v aspell) 2>/dev/null >/dev/null ; then
                        echo "=========================================================================="
                        echo "WARNING: Seems you do not have 'aspell' in PATH (but maybe NUT configure"
                        echo "script would find the spellchecking toolkit elsewhere)"
                        echo "Are you sure you run this in a functional build environment? Ctrl+C if not"
                        echo "=========================================================================="
                        sleep 5
                    fi
                    ;;
            esac >&2
            if [ -s Makefile ] && [ -s docs/Makefile ]; then
                echo "Processing quick and quiet spellcheck with already existing recipe files, will only report errors if any ..."
                build_to_only_catch_errors_target $1 ; exit
            else
                # TODO: Actually do it (default-spellcheck-interactive)?
                if [ "$1" = spellcheck-interactive ] ; then
                    echo "Only CI-building 'spellcheck', please do the interactive part manually if needed" >&2
                fi
                BUILD_TYPE="default-spellcheck"
                shift
            fi
            ;;

        *) echo "WARNING: Command-line argument '$1' wsa not recognized as a BUILD_TYPE alias" >&2 ;;
    esac
fi

# Default follows autotools:
[ -n "${DISTCHECK_TGT-}" ] || DISTCHECK_TGT="distcheck"

echo "Processing BUILD_TYPE='${BUILD_TYPE}' ..."

ensure_CI_CCACHE_SYMLINKDIR_envvar
echo "Build host settings:"
set | grep -E '^(PATH|[^ ]*CCACHE[^ ]*|CI_[^ ]*|OS_[^ ]*|CANBUILD_[^ ]*|NODE_LABELS|MAKE|C[^ ]*FLAGS|LDFLAGS|ARCH[^ ]*|BITS[^ ]*|CC|CXX|CPP|DO_[^ ]*|BUILD_[^ ]*|[^ ]*_TGT)=' || true
uname -a
echo "LONG_BIT:`getconf LONG_BIT` WORD_BIT:`getconf WORD_BIT`" || true
if command -v xxd >/dev/null ; then xxd -c 1 -l 6 | tail -1; else if command -v od >/dev/null; then od -N 1 -j 5 -b | head -1 ; else hexdump -s 5 -n 1 -C | head -1; fi; fi < /bin/ls 2>/dev/null | awk '($2 == 1){print "Endianness: LE"}; ($2 == 2){print "Endianness: BE"}' || true

case "$BUILD_TYPE" in
default|default-alldrv|default-alldrv:no-distcheck|default-all-errors|default-spellcheck|default-shellcheck|default-nodoc|default-withdoc|default-withdoc:man|"default-tgt:"*)
    LANG=C
    LC_ALL=C
    export LANG LC_ALL

    if [ -d "./tmp/" ]; then
        rm -rf ./tmp/
    fi
    if [ -d "./.inst/" ]; then
        rm -rf ./.inst/
    fi

    # Pre-create locations; tmp/lib in particular to avoid (on MacOS xcode):
    #   ld: warning: directory not found for option '-L/Users/distiller/project/tmp/lib'
    # Note that maintainer-clean checks can remove these directory trees,
    # so we re-create them just in case in the configure_nut() method too.
    mkdir -p tmp/lib .inst/
    BUILD_PREFIX="$PWD/tmp"
    INST_PREFIX="$PWD/.inst"

    optional_prepare_ccache
    ccache_stats "before"

    optional_prepare_compiler_family

    CONFIG_OPTS=()
    COMMON_CFLAGS=""
    EXTRA_CFLAGS=""
    EXTRA_CPPFLAGS=""
    EXTRA_CXXFLAGS=""

    detect_platform_CANBUILD_LIBGD_CGI

    # Prepend or use this build's preferred artifacts, if any
    DEFAULT_PKG_CONFIG_PATH="${BUILD_PREFIX}/lib/pkgconfig"
    detect_platform_PKG_CONFIG_PATH_and_FLAGS
    if [ -n "$PKG_CONFIG_PATH" ] ; then
        CONFIG_OPTS+=("PKG_CONFIG_PATH=${PKG_CONFIG_PATH}")
    fi

    PATH="`echo "${PATH}" | normalize_path`"
    CCACHE_PATH="`echo "${CCACHE_PATH}" | normalize_path`"

    # Note: Potentially there can be spaces in entries for multiple
    # *FLAGS here; this should be okay as long as entry expands to
    # one token when calling shell (may not be the case for distcheck)
    CONFIG_OPTS+=("CFLAGS=-I${BUILD_PREFIX}/include ${CFLAGS}")
    CONFIG_OPTS+=("CPPFLAGS=-I${BUILD_PREFIX}/include ${CPPFLAGS}")
    CONFIG_OPTS+=("CXXFLAGS=-I${BUILD_PREFIX}/include ${CXXFLAGS}")
    CONFIG_OPTS+=("LDFLAGS=-L${BUILD_PREFIX}/lib ${LDFLAGS}")

    CONFIG_OPTS+=("--enable-keep_nut_report_feature")
    CONFIG_OPTS+=("--prefix=${BUILD_PREFIX}")
    CONFIG_OPTS+=("--sysconfdir=${BUILD_PREFIX}/etc/nut")
    CONFIG_OPTS+=("--with-udev-dir=${BUILD_PREFIX}/etc/udev")
    CONFIG_OPTS+=("--with-devd-dir=${BUILD_PREFIX}/etc/devd")
    CONFIG_OPTS+=("--with-hotplug-dir=${BUILD_PREFIX}/etc/hotplug")

    if [ x"${INPLACE_RUNTIME-}" = xtrue ]; then
        CONFIG_OPTS+=("--enable-inplace-runtime")
    fi

    # TODO: Consider `--enable-maintainer-mode` to add recipes that
    # would quickly regenerate Makefile(.in) if you edit Makefile.am
    # TODO: Resolve port-collision reliably (for multi-executor agents)
    # and enable the test for CI runs. Bonus for making it quieter.
    if [ "${CANBUILD_NIT_TESTS-}" != no ] ; then
        CONFIG_OPTS+=("--enable-check-NIT")
    else
        echo "WARNING: Build agent does not say it can reliably 'make check-NIT'" >&2
        CONFIG_OPTS+=("--disable-check-NIT")
    fi

    if [ -n "${PYTHON-}" ]; then
        # WARNING: Watch out for whitespaces, not handled here!
        CONFIG_OPTS+=("--with-python=${PYTHON}")
    fi
    # Even in scenarios that request --with-all, we do not want
    # to choke on absence of desktop-related modules in Python.
    # Just make sure relevant install recipes are tested:
    CONFIG_OPTS+=("--with-nut_monitor=force")
    CONFIG_OPTS+=("--with-pynut=auto")

    # Primarily here to ensure libusb-1.0 use on MSYS2/mingw
    # when 0.1 is available too
    if [ "${CANBUILD_WITH_LIBMODBUS_USB-}" = yes ] ; then
        CONFIG_OPTS+=("--with-modbus+usb=yes")
    fi

    # Similarly for nut-scanner which requires libltdl which
    # is not ubiquitous on CI workers. So unless agent labels
    # declare it should be capable, err on the safe side:
    if [ "${CANBUILD_WITH_LIBLTDL-}" != yes ] ; then
        CONFIG_OPTS+=("--with-nut-scanner=auto")
    fi

    # Some OSes have broken cppunit support, it crashes either build/link
    # or at run-time. While distros take time to figure out fixes, we can
    # skip the case...
    if [ "${CANBUILD_CPPUNIT_TESTS-}" = no ] \
    || ( [ "${CANBUILD_CPPUNIT_TESTS-}" = "no-gcc" ] && [ "$COMPILER_FAMILY" = "GCC" ] ) \
    || ( [ "${CANBUILD_CPPUNIT_TESTS-}" = "no-clang" ] && [ "$COMPILER_FAMILY" = "CLANG" ] ) \
    ; then
        echo "WARNING: Build agent says it can't build or run libcppunit tests, adding configure option to skip them" >&2
        CONFIG_OPTS+=("--enable-cppunit=no")
    fi

    if ( [ "${CANBUILD_NUTCONF-}" = "no-gcc" ] && [ "$COMPILER_FAMILY" = "GCC" ] ) \
    || ( [ "${CANBUILD_NUTCONF-}" = "no-clang" ] && [ "$COMPILER_FAMILY" = "CLANG" ] ) \
    ; then
        CANBUILD_NUTCONF=no
    fi

    case "${CANBUILD_NUTCONF-}" in
        yes)
            # Depends on C++11 or newer, so let configure script try this tediously
            # unless we know we would not build for the too-old language revision
            case "${CXXFLAGS-}" in
                *-std=c++98*|*-std=gnu++98*|*-std=c++03*|*-std=gnu++03*)
                    echo "WARNING: Build agent says it can build nutconf, but requires a test with C++ revision too old - so not requiring the experimental feature (auto-try)" >&2
                    CONFIG_OPTS+=("--with-nutconf=auto")
                    ;;
                *-std=c++0x*|*-std=gnu++0x*|*-std=c++1*|*-std=gnu++1*|*-std=c++2*|*-std=gnu++2*)
                    echo "WARNING: Build agent says it can build nutconf, and requires a test with a sufficiently new C++ revision - so requiring the experimental feature" >&2
                    CONFIG_OPTS+=("--with-nutconf=yes")
                    ;;
                *)
                    echo "WARNING: Build agent says it can build nutconf, and does not specify a test with particular C++ revision - so not requiring the experimental feature (auto-try)" >&2
                    CONFIG_OPTS+=("--with-nutconf=auto")
                    ;;
            esac
            ;;
        no)
            echo "WARNING: Build agent says it can not build nutconf, disabling the feature (do not even try)" >&2
            CONFIG_OPTS+=("--with-nutconf=no")
            ;;
        "")
            CONFIG_OPTS+=("--with-nutconf=auto")
            ;;
    esac

    if [ "${CANBUILD_VALGRIND_TESTS-}" = no ] ; then
        echo "WARNING: Build agent says it has a broken valgrind, adding configure option to skip tests with it" >&2
        CONFIG_OPTS+=("--with-valgrind=no")
    fi

    if [ -n "${CI_CROSSBUILD_TARGET-}" ] || [ -n "${CI_CROSSBUILD_HOST-}" ] ; then
        # at least one is e.g. "arm-linux-gnueabihf"
        [ -z "${CI_CROSSBUILD_TARGET-}" ] && CI_CROSSBUILD_TARGET="${CI_CROSSBUILD_HOST}"
        [ -z "${CI_CROSSBUILD_HOST-}" ] && CI_CROSSBUILD_HOST="${CI_CROSSBUILD_TARGET}"
        echo "NOTE: Cross-build was requested, passing options to configure this for target '${CI_CROSSBUILD_TARGET}' host '${CI_CROSSBUILD_HOST}' (note you may need customized PKG_CONFIG_PATH)" >&2
        CONFIG_OPTS+=("--host=${CI_CROSSBUILD_HOST}")
        CONFIG_OPTS+=("--target=${CI_CROSSBUILD_TARGET}")
    fi

    # This flag is primarily linked with (lack of) docs generation enabled
    # (or not) in some BUILD_TYPE scenarios or workers. Initial value may
    # be set by caller, but codepaths below have the final word.
    [ "${DO_DISTCHECK-}" = no ] || DO_DISTCHECK=yes
    case "$BUILD_TYPE" in
        "default-nodoc")
            CONFIG_OPTS+=("--with-doc=no")
            CONFIG_OPTS+=("--disable-spellcheck")
            DO_DISTCHECK=no
            ;;
        "default-spellcheck"|"default-shellcheck")
            CONFIG_OPTS+=("--with-all=no")
            CONFIG_OPTS+=("--with-libltdl=no")
            CONFIG_OPTS+=("--with-doc=man=skip")
            CONFIG_OPTS+=("--enable-spellcheck")
            #TBD# CONFIG_OPTS+=("--with-shellcheck=yes")
            DO_DISTCHECK=no
            ;;
        "default-withdoc")
            # If the build agent says what it can not do, honor that
            # TOTHINK: Should this build scenario die with error/unstable instead?
            if [ "${CANBUILD_DOCS_ALL-}" = no ]; then
                if [ "${CANBUILD_DOCS_MAN-}" = no ]; then
                    # TBD: Also html? We'd have man then, and that is needed for distchecks at least
                    echo "WARNING: Build agent says it can build neither 'all' nor 'man' doc types; will ask for what we can build" >&2
                    #?#CONFIG_OPTS+=("--with-doc=no")
                    CONFIG_OPTS+=("--with-doc=auto")
                else
                    echo "WARNING: Build agent says it can't build 'all' doc types, but can build 'man' pages; will ask for what we can build" >&2
                    CONFIG_OPTS+=("--with-doc=auto")
                fi
            else
                if [ "${CANBUILD_DOCS_MAN-}" = no ]; then
                    # TBD: Also html? We'd have man then, and that is needed for distchecks at least
                    echo "WARNING: Build agent says it can't build 'man' pages and says nothing about 'all' doc types; will ask for what we can build" >&2
                    CONFIG_OPTS+=("--with-doc=auto")
                else
                    # Not a "no" in any category (yes or unspecified), request everything
                    CONFIG_OPTS+=("--with-doc=yes")
                fi
            fi
            if [ -z "${DO_CLEAN_CHECK-}" ]; then
                # This is one of recipes where we want to
                # keep the build products by default ;)
                DO_CLEAN_CHECK=no
            fi
            ;;
        "default-withdoc:man")
            # Some systems lack tools for HTML/PDF generation
            # but may still yield standard man pages
            if [ "${CANBUILD_DOCS_MAN-}" = no ]; then
                echo "WARNING: Build agent says it can't build man pages; will ask for what we can build" >&2
                CONFIG_OPTS+=("--with-doc=auto")
            else
                CONFIG_OPTS+=("--with-doc=man")
            fi
            if [ -z "${DO_CLEAN_CHECK-}" ]; then
                # This is one of recipes where we want to
                # keep the build products by default ;)
                DO_CLEAN_CHECK=no
            fi
            ;;
        "default-all-errors")
            # This mode aims to build as many codepaths (to collect warnings)
            # as it can, so help it enable (require) as many options as we can.

            # Do not build the docs as we are interested in binary code
            CONFIG_OPTS+=("--with-doc=skip")
            CONFIG_OPTS+=("--disable-spellcheck")
            # Enable as many binaries to build as current worker setup allows
            CONFIG_OPTS+=("--with-all=auto")

            # Use "distcheck-ci" if caller did not ask for any DISTCHECK_TGT
            # value, and we defaulted to strict "distcheck" above
            ( [ -n "${ORIG_DISTCHECK_TGT}" ] || [ x"${DISTCHECK_TGT}" != x"distcheck" ] ) || DISTCHECK_TGT="distcheck-ci"

            if [ "${CANBUILD_LIBGD_CGI-}" != "no" ] && [ "${BUILD_LIBGD_CGI-}" != "auto" ]  ; then
                # Currently --with-all implies this, but better be sure to
                # really build everything we can to be certain it builds:
                if [ "${CANBUILD_LIBGD_CGI-}" != "auto" ] && (
                   $PKG_CONFIG --exists libgd || $PKG_CONFIG --exists libgd2 || $PKG_CONFIG --exists libgd3 || $PKG_CONFIG --exists gdlib || $PKG_CONFIG --exists gd
                ) ; then
                    CONFIG_OPTS+=("--with-cgi=yes")
                else
                    # Note: CI-wise, our goal IS to test as much as we can
                    # with this build, so environments should be set up to
                    # facilitate that as much as feasible. But reality is...
                    echo "WARNING: Seems libgd{,2,3} is not present, CGI build may be skipped!" >&2
                    CONFIG_OPTS+=("--with-cgi=auto")
                fi
            else
                if [ "${CANBUILD_LIBGD_CGI-}" = "no" ] ; then
                    CONFIG_OPTS+=("--without-cgi")
                else
                    CONFIG_OPTS+=("--with-cgi=auto")
                fi
            fi
            ;;
        "default-alldrv:no-distcheck")
            DO_DISTCHECK=no
            ;& # fall through
        "default-alldrv")
            # Do not build the docs and make possible a distcheck below
            CONFIG_OPTS+=("--with-doc=skip")
            CONFIG_OPTS+=("--disable-spellcheck")
            if [ "${CANBUILD_DRIVERS_ALL-}" = no ]; then
                echo "WARNING: Build agent says it can't build 'all' driver types; will ask for what we can build" >&2
                if [ "$DO_DISTCHECK" != no ]; then
                    echo "WARNING: this is effectively default-tgt:distcheck-light then" >&2
                fi
                CONFIG_OPTS+=("--with-all=auto")
                # Use "distcheck-light" if caller did not ask for any DISTCHECK_TGT
                # value, and we defaulted to strict "distcheck" above
                ( [ -n "${ORIG_DISTCHECK_TGT}" ] || [ x"${DISTCHECK_TGT}" != x"distcheck" ] ) || DISTCHECK_TGT="distcheck-light"
            else
                CONFIG_OPTS+=("--with-all=yes")
                # Use "distcheck-ci" if caller did not ask for any DISTCHECK_TGT
                # value, and we defaulted to strict "distcheck" above
                ( [ -n "${ORIG_DISTCHECK_TGT}" ] || [ x"${DISTCHECK_TGT}" != x"distcheck" ] ) || DISTCHECK_TGT="distcheck-ci"
            fi
            ;;
        "default-tgt:cppcheck")
            if [ "${CANBUILD_CPPCHECK_TESTS-}" = no ] ; then
                echo "WARNING: Build agent says it has a broken cppcheck, but we requested a BUILD_TYPE='$BUILD_TYPE'" >&2
                exit 1
            fi
            if [ -z "${DO_CLEAN_CHECK-}" ]; then
                # This is one of recipes where we want to
                # keep the build products by default ;)
                DO_CLEAN_CHECK=no
            fi
            CONFIG_OPTS+=("--enable-cppcheck=yes")
            CONFIG_OPTS+=("--with-doc=skip")
            # Use "distcheck-ci" if caller did not ask for any DISTCHECK_TGT
            # value, and we defaulted to strict "distcheck" above
            ( [ -n "${ORIG_DISTCHECK_TGT}" ] || [ x"${DISTCHECK_TGT}" != x"distcheck" ] ) || DISTCHECK_TGT="distcheck-ci"
            ;;
        "default"|"default-tgt:"*|*)
            # Do not build the docs and tell distcheck it is okay
            CONFIG_OPTS+=("--with-doc=skip")
            CONFIG_OPTS+=("--disable-spellcheck")
            # Use "distcheck-ci" if caller did not ask for any DISTCHECK_TGT
            # value, and we defaulted to strict "distcheck" above
            ( [ -n "${ORIG_DISTCHECK_TGT}" ] || [ x"${DISTCHECK_TGT}" != x"distcheck" ] ) || DISTCHECK_TGT="distcheck-ci"
            ;;
    esac
    # NOTE: The case "$BUILD_TYPE" above was about setting CONFIG_OPTS.
    # There is another below for running actual scenarios.

    if optional_ensure_ccache ; then
        # Note: Potentially there can be spaces in entries for multiword
        # "ccache gcc" here; this should be okay as long as entry expands to
        # one token when calling shell (may not be the case for distcheck)
        CONFIG_OPTS+=("CC=${CC}")
        CONFIG_OPTS+=("CXX=${CXX}")
        CONFIG_OPTS+=("CPP=${CPP}")
    fi

    # Build and check this project; note that zprojects always have an autogen.sh
    [ -z "$CI_TIME" ] || echo "`date`: Starting build of currently tested project..."

    # Numerous per-compiler variants defined in configure.ac, including
    # aliases "minimal", "medium", "hard", "all"
    if [ -n "${BUILD_WARNOPT-}" ]; then
        CONFIG_OPTS+=("--enable-warnings=${BUILD_WARNOPT}")
    fi

    # Parse from strings that could be populated by a CI Boolean checkbox:
    case "${BUILD_WARNFATAL-}" in
        [Tt][Rr][Uu][Ee]) BUILD_WARNFATAL=yes;;
        [Ff][Aa][Ll][Ss][Ee]) BUILD_WARNFATAL=no;;
    esac
    if [ -n "${BUILD_WARNFATAL-}" ]; then
        CONFIG_OPTS+=("--enable-Werror=${BUILD_WARNFATAL}")
    fi

    # Tell interactive and CI builds to prefer colorized output so warnings
    # and errors are found more easily in a wall of text:
    CONFIG_OPTS+=("--enable-Wcolor")

    if [ -n "${BUILD_DEBUGINFO-}" ]; then
        CONFIG_OPTS+=("--with-debuginfo=${BUILD_DEBUGINFO}")
    fi

    consider_cleanup_shortcut

    if [ -s Makefile ]; then
        # Let initial clean-up be at default verbosity

        # Handle Ctrl+C with helpful suggestions:
        trap 'echo "!!! If clean-up looped remaking the configure script for maintainer-clean, try to:"; echo "    rm -f Makefile configure include/config.h* ; $0 $SCRIPT_ARGS"' 2

        echo "=== Starting initial clean-up (from old build products)"
        case "$MAKE_FLAGS $MAKE_FLAGS_CLEAN" in
        *V=0*)
            ${MAKE} maintainer-clean $MAKE_FLAGS_CLEAN -k > /dev/null \
            || ${MAKE} maintainer-clean $MAKE_FLAGS_CLEAN -k
            ;;
        *)
            ${MAKE} maintainer-clean $MAKE_FLAGS_CLEAN -k
        esac \
        || ${MAKE} distclean $MAKE_FLAGS_CLEAN -k \
        || true
        echo "=== Finished initial clean-up"

        trap - 2
    fi

    # Just prepare `configure` script; we run it at different points
    # below depending on scenario
    autogen_get_CONFIGURE_SCRIPT

    if [ "$NO_PKG_CONFIG" = "true" ] && [ "$CI_OS_NAME" = "linux" ] && (command -v dpkg) ; then
        # This should be done in scratch containers...
        echo "NO_PKG_CONFIG==true : BUTCHER pkg-config package for this test case" >&2
        sudo dpkg -r --force all pkg-config
    fi

    if [ "$BUILD_TYPE" != "default-all-errors" ] ; then
        configure_nut
    fi

    # NOTE: There is also a case "$BUILD_TYPE" above for setting CONFIG_OPTS
    # This case runs some specially handled BUILD_TYPEs and exists; support
    # for all other scenarios proceeds.below.
    case "$BUILD_TYPE" in
        "default-tgt:"*) # Hook for matrix of custom distchecks primarily
            # e.g. distcheck-ci, distcheck-light, distcheck-valgrind, cppcheck,
            # maybe others later, as defined in top-level Makefile.am:
            BUILD_TGT="`echo "$BUILD_TYPE" | sed 's,^default-tgt:,,'`"
            if [ -n "${PARMAKE_FLAGS}" ]; then
                echo "`date`: Starting the parallel build attempt for singular target $BUILD_TGT..."
            else
                echo "`date`: Starting the sequential build attempt for singular target $BUILD_TGT..."
            fi

            # Note: Makefile.am already sets some default DISTCHECK_CONFIGURE_FLAGS
            # that include DISTCHECK_FLAGS if provided
            DISTCHECK_FLAGS="`for F in "${CONFIG_OPTS[@]}" ; do echo "'$F' " ; done | tr '\n' ' '`"
            export DISTCHECK_FLAGS

            # Tell the sub-makes (likely distcheck*) to hush down
            # NOTE: Parameter pass-through was tested with:
            #   MAKEFLAGS="-j 12" BUILD_TYPE=default-tgt:distcheck-light ./ci_build.sh
            MAKEFLAGS="${MAKEFLAGS-} $MAKE_FLAGS_QUIET" \
            $CI_TIME $MAKE DISTCHECK_FLAGS="$DISTCHECK_FLAGS" $PARMAKE_FLAGS "$BUILD_TGT"

            # Can be noisy if regen is needed (DMF branch)
            #GIT_DIFF_SHOW=false \
            FILE_DESCR="DMF" FILE_REGEX='\.dmf$' FILE_GLOB='*.dmf' check_gitignore "$BUILD_TGT" || exit
            check_gitignore "$BUILD_TGT" || exit

            ccache_stats "after"

            optional_maintainer_clean_check || exit

            echo "=== Exiting after the custom-build target '$MAKE $BUILD_TGT' succeeded OK"
            exit 0
            ;;
        "default-spellcheck")
            [ -z "$CI_TIME" ] || echo "`date`: Trying to spellcheck documentation of the currently tested project..."
            # Note: use the root Makefile's spellcheck recipe which goes into
            # sub-Makefiles known to check corresponding directory's doc files.
            # Note: no PARMAKE_FLAGS here - better have this output readably
            # ordered in case of issues (in sequential replay below).
            ( echo "`date`: Starting the quiet build attempt for target $BUILD_TYPE..." >&2
              $CI_TIME $MAKE $MAKE_FLAGS_QUIET SPELLCHECK_ERROR_FATAL=yes -k $PARMAKE_FLAGS spellcheck >/dev/null 2>&1 \
              && echo "`date`: SUCCEEDED the spellcheck" >&2
            ) || \
            ( echo "`date`: FAILED something in spellcheck above; re-starting a verbose build attempt to give more context first:" >&2
              $CI_TIME $MAKE $MAKE_FLAGS_VERBOSE SPELLCHECK_ERROR_FATAL=yes spellcheck
              # Make end of log useful:
              echo "`date`: FAILED something in spellcheck above; re-starting a non-verbose build attempt to just summarize now:" >&2
              $CI_TIME $MAKE $MAKE_FLAGS_QUIET SPELLCHECK_ERROR_FATAL=yes spellcheck
            )
            exit $?
            ;;
        "default-shellcheck")
            [ -z "$CI_TIME" ] || echo "`date`: Trying to check shell script syntax validity of the currently tested project..."
            ### Note: currently, shellcheck target calls check-scripts-syntax
            ### so when both are invoked at once, in the end the check is only
            ### executed once. Later it is anticipated that shellcheck would
            ### be implemented by requiring, configuring and calling the tool
            ### named "shellcheck" for even more code inspection and details.
            ### Still, there remains value in also checking the script syntax
            ### by the very version of the shell interpreter that would run
            ### these scripts in production usage of the resulting packages.
            ### Note: no PARMAKE_FLAGS here - better have this output readably
            ### ordered in case of issues.
            ( $CI_TIME $MAKE $MAKE_FLAGS_VERBOSE shellcheck check-scripts-syntax )
            exit $?
            ;;
        "default-all-errors")
            # This mode aims to build as many codepaths (to collect warnings)
            # as it can, so help it enable (require) as many options as we can.

            # Try to run various build scenarios to collect build errors
            # (no checks here) as configured further by caller's choice
            # of BUILD_WARNFATAL and/or BUILD_WARNOPT envvars above.
            # Note this is one scenario where we did not configure_nut()
            # in advance.
            RES_ALLERRORS=0
            FAILED=""
            SUCCEEDED=""
            BUILDSTODO=0

            # Technically, let caller provide this setting explicitly
            if [ -z "$NUT_SSL_VARIANTS" ] ; then
                NUT_SSL_VARIANTS="auto"
                if $PKG_CONFIG --exists nss && $PKG_CONFIG --exists openssl && [ "${BUILD_SSL_ONCE-}" != "true" ] ; then
                    # Try builds for both cases as they are ifdef-ed
                    # TODO: Extend if we begin to care about different
                    # major versions of openssl (with their APIs), etc.
                    NUT_SSL_VARIANTS="openssl nss"
                else
                    if [ "${BUILD_SSL_ONCE-}" != "true" ]; then
                        $PKG_CONFIG --exists nss 2>/dev/null && NUT_SSL_VARIANTS="nss"
                        $PKG_CONFIG --exists openssl 2>/dev/null && NUT_SSL_VARIANTS="openssl"
                    fi  # else leave at "auto", if we skipped building
                        # two variants while having two possibilities
                fi

                # Consider also a build --without-ssl to test that codepath?
                if [ "$NUT_SSL_VARIANTS" != auto ] && [ "${BUILD_SSL_ONCE-}" != "true" ]; then
                    NUT_SSL_VARIANTS="$NUT_SSL_VARIANTS no"
                fi
            fi

            if [ -z "$NUT_USB_VARIANTS" ] ; then
                # Check preferred version first, in case BUILD_USB_ONCE==true
                if $PKG_CONFIG --exists libusb-1.0 ; then
                    NUT_USB_VARIANTS="1.0"
                fi

                # TODO: Is there anywhere a `pkg-config --exists libusb-0.1`?
                if $PKG_CONFIG --exists libusb || ( command -v libusb-config || which libusb-config ) 2>/dev/null >/dev/null ; then
                    if [ -z "$NUT_USB_VARIANTS" ] ; then
                        NUT_USB_VARIANTS="0.1"
                    else
                        if [ "${BUILD_USB_ONCE-}" != "true" ] ; then
                            NUT_USB_VARIANTS="$NUT_USB_VARIANTS 0.1"
                        fi
                    fi
                fi

                if [ -z "$NUT_USB_VARIANTS" ] ; then
                    # Nothing supported detected...
                    NUT_USB_VARIANTS="auto"
                fi

                # Consider also a build --without-usb to test that codepath?
                # (e.g. for nutdrv_qx that has both serial and USB parts)
                if [ "$NUT_USB_VARIANTS" != auto ] && [ "${BUILD_USB_ONCE-}" != "true" ]; then
                    NUT_USB_VARIANTS="$NUT_USB_VARIANTS no"
                fi
            fi

            # Count our expected build variants, so the last one gets the
            # "maintainer-clean" check and not a mere "distclean" check
            # NOTE: We count different dependency variations separately,
            # and analyze later, to avoid building same (auto+auto) twice
            BUILDSTODO_SSL=0
            for NUT_SSL_VARIANT in $NUT_SSL_VARIANTS ; do
                BUILDSTODO_SSL="`expr $BUILDSTODO_SSL + 1`"
            done

            BUILDSTODO_USB=0
            for NUT_USB_VARIANT in $NUT_USB_VARIANTS ; do
                BUILDSTODO_USB="`expr $BUILDSTODO_USB + 1`"
            done

            if [ "${BUILDSTODO_SSL}" -gt 1 ] \
            && [ "${BUILDSTODO_USB}" -gt 1 ] \
            ; then
                BUILDSTODO="`expr $BUILDSTODO_SSL + $BUILDSTODO_USB`"
            else
                ###BUILDSTODO=0
                ###if [ "${BUILDSTODO_SSL}" -gt "${BUILDSTODO}" ] ; then BUILDSTODO="${BUILDSTODO_SSL}" ; fi
                ###if [ "${BUILDSTODO_USB}" -gt "${BUILDSTODO}" ] ; then BUILDSTODO="${BUILDSTODO_USB}" ; fi

                # Use same logic as in actual loops below
                # It may be imperfect (WRT avoiding extra builds) -- and
                # that may be addressed separately, but counts should fit
                BUILDSTODO="${BUILDSTODO_SSL}"

                # Adding up only if we are building several USB variants
                # or a single non-default variant (maybe a "no" option),
                # so we should be trying both SSL's and that/those USB
                ###[ "$NUT_USB_VARIANTS" = "auto" ] || \
                ###{ [ "${BUILDSTODO_USB}" -le 1 ] && [ "$NUT_USB_VARIANTS" != "no" ] ; } || \
                if [ "${BUILDSTODO_USB}" -gt 1 ] \
                || [ "$NUT_USB_VARIANTS" != "auto" ] \
                ; then
                    BUILDSTODO="`expr $BUILDSTODO + $BUILDSTODO_USB`"
                fi

                if [ "$NUT_SSL_VARIANTS" = "auto" ] \
                && [ "${BUILDSTODO_USB}" -gt 0 ] \
                ; then
                    echo "=== Only build USB scenario(s) picking whatever SSL is found"
                    BUILDSTODO="${BUILDSTODO_USB}"
                fi
            fi

            BUILDSTODO_INITIAL="$BUILDSTODO"
            echo "=== Will loop now with $BUILDSTODO build variants: found ${BUILDSTODO_SSL} SSL ($NUT_SSL_VARIANTS) and ${BUILDSTODO_USB} USB ($NUT_USB_VARIANTS) variations..."
            # If we don't care about SSL implem and want to pick USB, go straight there
            ( [ "$NUT_SSL_VARIANTS" = "auto" ] && [ "${BUILDSTODO_USB}" -gt 0 ] ) || \
            for NUT_SSL_VARIANT in $NUT_SSL_VARIANTS ; do
                # NOTE: Do not repeat a distclean before the loop,
                # we have cleaned above before autogen, and here it
                # would just re-evaluate `configure` to update the
                # Makefile to remove it and other generated data.
                #echo "=== Clean the sandbox, $BUILDSTODO build variants remaining..."
                #$MAKE distclean $MAKE_FLAGS_CLEAN -k || true

                echo "=== Starting 'NUT_SSL_VARIANT=$NUT_SSL_VARIANT', $BUILDSTODO build variants remaining..."
                case "$NUT_SSL_VARIANT" in
                    ""|auto|default)
                        # Quietly build one scenario, whatever we can (or not)
                        # configure regarding SSL and other features
                        NUT_SSL_VARIANT=auto
                        configure_nut
                        ;;
                    no)
                        echo "=== Building without SSL support..."
                        ( CONFIG_OPTS+=("--without-ssl")
                          configure_nut
                        )
                        ;;
                    *)
                        echo "=== Building with 'NUT_SSL_VARIANT=${NUT_SSL_VARIANT}' ..."
                        ( CONFIG_OPTS+=("--with-${NUT_SSL_VARIANT}")
                          configure_nut
                        )
                        ;;
                esac || {
                    RES_ALLERRORS=$?
                    FAILED="${FAILED} NUT_SSL_VARIANT=${NUT_SSL_VARIANT}[configure]"
                    # TOTHINK: Do we want to try clean-up if we likely have no Makefile?
                    if [ "$CI_FAILFAST" = true ]; then
                        echo "===== Aborting because CI_FAILFAST=$CI_FAILFAST" >&2
                        break
                    fi
                    BUILDSTODO="`expr $BUILDSTODO - 1`" || [ "$BUILDSTODO" = "0" ] || break
                    continue
                }

                echo "=== Configured 'NUT_SSL_VARIANT=$NUT_SSL_VARIANT', $BUILDSTODO build variants (including this one) remaining to complete; trying to build..."
                cd "${CI_BUILDDIR}"
                # Use default target e.g. "all":
                build_to_only_catch_errors_target && {
                    SUCCEEDED="${SUCCEEDED} NUT_SSL_VARIANT=${NUT_SSL_VARIANT}[build]"
                } || {
                    RES_ALLERRORS=$?
                    FAILED="${FAILED} NUT_SSL_VARIANT=${NUT_SSL_VARIANT}[build]"
                    # Help find end of build (before cleanup noise) in logs:
                    echo "=== FAILED 'NUT_SSL_VARIANT=${NUT_SSL_VARIANT}' build"
                    if [ "$CI_FAILFAST" = true ]; then
                        echo "===== Aborting because CI_FAILFAST=$CI_FAILFAST" >&2
                        break
                    fi
                }

                build_to_only_catch_errors_check && {
                    SUCCEEDED="${SUCCEEDED} NUT_SSL_VARIANT=${NUT_SSL_VARIANT}[check]"
                } || {
                    RES_ALLERRORS=$?
                    FAILED="${FAILED} NUT_SSL_VARIANT=${NUT_SSL_VARIANT}[check]"
                    # Help find end of build (before cleanup noise) in logs:
                    echo "=== FAILED 'NUT_SSL_VARIANT=${NUT_SSL_VARIANT}' check"
                    if [ "$CI_FAILFAST" = true ]; then
                        echo "===== Aborting because CI_FAILFAST=$CI_FAILFAST" >&2
                        break
                    fi
                }

                # Note: when `expr` calculates a zero value below, it returns
                # an "erroneous" `1` as exit code. Why oh why?..
                # (UPDATE: because expr returns boolean, and calculated 0 is false;
                # so a `set -e` run aborts)
                BUILDSTODO="`expr $BUILDSTODO - 1`" || [ "$BUILDSTODO" = "0" ] || break

                if [ "$BUILDSTODO" -gt 0 ] && [ "${DO_CLEAN_CHECK-}" != no ]; then
                    # For last iteration with DO_CLEAN_CHECK=no,
                    # we would leave built products in place
                    echo "=== Clean the sandbox, $BUILDSTODO build variants remaining..."
                fi

                if can_clean_check ; then
                    if [ $BUILDSTODO -gt 0 ]; then
                        ### Avoid having to re-autogen in a loop:
                        optional_dist_clean_check && {
                            if [ "${DO_DIST_CLEAN_CHECK-}" != "no" ] ; then
                                SUCCEEDED="${SUCCEEDED} NUT_SSL_VARIANT=${NUT_SSL_VARIANT}[dist_clean]"
                            fi
                        } || {
                            RES_ALLERRORS=$?
                            FAILED="${FAILED} NUT_SSL_VARIANT=${NUT_SSL_VARIANT}[dist_clean]"
                        }
                    else
                        optional_maintainer_clean_check && {
                            if [ "${DO_MAINTAINER_CLEAN_CHECK-}" != no ] ; then
                                SUCCEEDED="${SUCCEEDED} NUT_SSL_VARIANT=${NUT_SSL_VARIANT}[maintainer_clean]"
                            fi
                        } || {
                            RES_ALLERRORS=$?
                            FAILED="${FAILED} NUT_SSL_VARIANT=${NUT_SSL_VARIANT}[maintainer_clean]"
                        }
                    fi
                    echo "=== Completed sandbox cleanup-check after NUT_SSL_VARIANT=${NUT_SSL_VARIANT}, $BUILDSTODO build variants remaining"
                else
                    if [ "$BUILDSTODO" -gt 0 ] && [ "${DO_CLEAN_CHECK-}" != no ]; then
                        $MAKE distclean $MAKE_FLAGS_CLEAN -k \
                        || echo "WARNING: 'make distclean' FAILED: $? ... proceeding" >&2
                        echo "=== Completed sandbox cleanup after NUT_SSL_VARIANT=${NUT_SSL_VARIANT}, $BUILDSTODO build variants remaining"
                    else
                        echo "=== SKIPPED sandbox cleanup because DO_CLEAN_CHECK=$DO_CLEAN_CHECK and $BUILDSTODO build variants remaining"
                    fi
                fi
            done

            # Effectively, whatever up to one version of LibUSB support
            # was detected (or not), was tested above among SSL builds.
            # Here we drill deeper for envs that have more than one LibUSB,
            # or when caller explicitly requested to only test without it,
            # and then we only attempt the serial and/or USB options while
            # disabling other drivers for faster turnaround.
            ###[ "$NUT_USB_VARIANTS" = "auto" ] || \
            ###( [ "${BUILDSTODO_USB}" -le 1 ] && [ "$NUT_USB_VARIANTS" != "no" ] ) || \
            ( ( [ "$NUT_SSL_VARIANTS" = "auto" ] && [ "${BUILDSTODO_USB}" -gt 0 ] ) \
             || [ "${BUILDSTODO_USB}" -gt 1 ] \
             || [ "$NUT_USB_VARIANTS" != "auto" ] \
            ) && \
            (   [ "$CI_FAILFAST" != "true" ] \
             || [ "$CI_FAILFAST" = "true" -a "$RES_ALLERRORS" = 0 ] \
            ) && \
            for NUT_USB_VARIANT in $NUT_USB_VARIANTS ; do
                echo "=== Starting 'NUT_USB_VARIANT=$NUT_USB_VARIANT', $BUILDSTODO build variants remaining..."
                case "$NUT_USB_VARIANT" in
                    ""|auto|default)
                        # Quietly build one scenario, whatever we can (or not)
                        # configure regarding USB and other features
                        NUT_USB_VARIANT=auto
                        ( if [ "$NUT_SSL_VARIANTS" != "auto" ] ; then
                              CONFIG_OPTS+=("--without-all")
                              CONFIG_OPTS+=("--without-ssl")
                          fi
                          CONFIG_OPTS+=("--with-serial=auto")
                          CONFIG_OPTS+=("--with-usb")
                          configure_nut
                        )
                        ;;
                    no)
                        echo "=== Building without USB support (check mixed drivers coded for Serial/USB support)..."
                        ( if [ "$NUT_SSL_VARIANTS" != "auto" ] ; then
                              CONFIG_OPTS+=("--without-all")
                              CONFIG_OPTS+=("--without-ssl")
                          fi
                          CONFIG_OPTS+=("--with-serial=auto")
                          CONFIG_OPTS+=("--without-usb")
                          configure_nut
                        )
                        ;;
                    libusb-*)
                        echo "=== Building with 'NUT_USB_VARIANT=${NUT_USB_VARIANT}' ..."
                        ( if [ "$NUT_SSL_VARIANTS" != "auto" ] ; then
                              CONFIG_OPTS+=("--without-all")
                              CONFIG_OPTS+=("--without-ssl")
                          fi
                          CONFIG_OPTS+=("--with-serial=auto")
                          CONFIG_OPTS+=("--with-usb=${NUT_USB_VARIANT}")
                          configure_nut
                        )
                        ;;
                    *)
                        echo "=== Building with 'NUT_USB_VARIANT=${NUT_USB_VARIANT}' ..."
                        ( if [ "$NUT_SSL_VARIANTS" != "auto" ] ; then
                              CONFIG_OPTS+=("--without-all")
                              CONFIG_OPTS+=("--without-ssl")
                          fi
                          CONFIG_OPTS+=("--with-serial=auto")
                          CONFIG_OPTS+=("--with-usb=libusb-${NUT_USB_VARIANT}")
                          configure_nut
                        )
                        ;;
                esac || {
                    RES_ALLERRORS=$?
                    FAILED="${FAILED} NUT_USB_VARIANT=${NUT_USB_VARIANT}[configure]"
                    # TOTHINK: Do we want to try clean-up if we likely have no Makefile?
                    if [ "$CI_FAILFAST" = true ]; then
                        echo "===== Aborting because CI_FAILFAST=$CI_FAILFAST" >&2
                        break
                    fi
                    BUILDSTODO="`expr $BUILDSTODO - 1`" || [ "$BUILDSTODO" = "0" ] || break
                    continue
                }

                echo "=== Configured 'NUT_USB_VARIANT=$NUT_USB_VARIANT', $BUILDSTODO build variants (including this one) remaining to complete; trying to build..."
                cd "${CI_BUILDDIR}"
                # Use default target e.g. "all":
                build_to_only_catch_errors_target && {
                    SUCCEEDED="${SUCCEEDED} NUT_USB_VARIANT=${NUT_USB_VARIANT}[build]"
                } || {
                    RES_ALLERRORS=$?
                    FAILED="${FAILED} NUT_USB_VARIANT=${NUT_USB_VARIANT}[build]"
                    # Help find end of build (before cleanup noise) in logs:
                    echo "=== FAILED 'NUT_USB_VARIANT=${NUT_USB_VARIANT}' build"
                    if [ "$CI_FAILFAST" = true ]; then
                        echo "===== Aborting because CI_FAILFAST=$CI_FAILFAST" >&2
                        break
                    fi
                }

                build_to_only_catch_errors_check && {
                    SUCCEEDED="${SUCCEEDED} NUT_USB_VARIANT=${NUT_USB_VARIANT}[check]"
                } || {
                    RES_ALLERRORS=$?
                    FAILED="${FAILED} NUT_USB_VARIANT=${NUT_USB_VARIANT}[check]"
                    # Help find end of build (before cleanup noise) in logs:
                    echo "=== FAILED 'NUT_USB_VARIANT=${NUT_USB_VARIANT}' check"
                    if [ "$CI_FAILFAST" = true ]; then
                        echo "===== Aborting because CI_FAILFAST=$CI_FAILFAST" >&2
                        break
                    fi
                }

                # Note: when `expr` calculates a zero value below, it returns
                # an "erroneous" `1` as exit code. Notes above.
                BUILDSTODO="`expr $BUILDSTODO - 1`" || [ "$BUILDSTODO" = "0" ] || break

                if [ "$BUILDSTODO" -gt 0 ] && [ "${DO_CLEAN_CHECK-}" != no ]; then
                    # For last iteration with DO_CLEAN_CHECK=no,
                    # we would leave built products in place
                    echo "=== Clean the sandbox, $BUILDSTODO build variants remaining..."
                fi

                if can_clean_check ; then
                    if [ $BUILDSTODO -gt 0 ]; then
                        ### Avoid having to re-autogen in a loop:
                        optional_dist_clean_check && {
                            if [ "${DO_DIST_CLEAN_CHECK-}" != "no" ] ; then
                                SUCCEEDED="${SUCCEEDED} NUT_USB_VARIANT=${NUT_USB_VARIANT}[dist_clean]"
                            fi
                        } || {
                            RES_ALLERRORS=$?
                            FAILED="${FAILED} NUT_USB_VARIANT=${NUT_USB_VARIANT}[dist_clean]"
                        }
                    else
                        optional_maintainer_clean_check && {
                            if [ "${DO_MAINTAINER_CLEAN_CHECK-}" != no ] ; then
                                SUCCEEDED="${SUCCEEDED} NUT_USB_VARIANT=${NUT_USB_VARIANT}[maintainer_clean]"
                            fi
                        } || {
                            RES_ALLERRORS=$?
                            FAILED="${FAILED} NUT_USB_VARIANT=${NUT_USB_VARIANT}[maintainer_clean]"
                        }
                    fi
                    echo "=== Completed sandbox cleanup-check after NUT_USB_VARIANT=${NUT_USB_VARIANT}, $BUILDSTODO build variants remaining"
                else
                    if [ "$BUILDSTODO" -gt 0 ] && [ "${DO_CLEAN_CHECK-}" != no ]; then
                        $MAKE distclean $MAKE_FLAGS_CLEAN -k \
                        || echo "WARNING: 'make distclean' FAILED: $? ... proceeding" >&2
                        echo "=== Completed sandbox cleanup after NUT_USB_VARIANT=${NUT_USB_VARIANT}, $BUILDSTODO build variants remaining"
                    else
                        echo "=== SKIPPED sandbox cleanup because DO_CLEAN_CHECK=$DO_CLEAN_CHECK and $BUILDSTODO build variants remaining"
                    fi
                fi
            done

            # TODO: Similar loops for other variations like TESTING,
            # MGE SHUT vs. other serial protocols...

            if can_clean_check ; then
                echo "=== One final try for optional_maintainer_clean_check:"
                optional_maintainer_clean_check && {
                    if [ "${DO_MAINTAINER_CLEAN_CHECK-}" != no ] ; then
                        SUCCEEDED="${SUCCEEDED} [final_maintainer_clean]"
                    fi
                } || {
                    RES_ALLERRORS=$?
                    FAILED="${FAILED} [final_maintainer_clean]"
                }
                echo "=== Completed sandbox maintainer-cleanup-check after all builds"
            fi

            if [ -n "$SUCCEEDED" ]; then
                echo "SUCCEEDED build(s) with:${SUCCEEDED}" >&2
            fi

            if [ "$RES_ALLERRORS" != 0 ]; then
                # Leading space is included in FAILED
                echo "FAILED build(s) with code ${RES_ALLERRORS}:${FAILED}" >&2
            else
                echo "(and no build scenarios had failed)" >&2
            fi

            echo "Initially estimated ${BUILDSTODO_INITIAL} variations for BUILD_TYPE='$BUILD_TYPE'" >&2
            if [ "$BUILDSTODO" -gt 0 ]; then
                echo "(and missed the mark: ${BUILDSTODO} variations remain - did anything crash early above?)" >&2
            fi

            exit $RES_ALLERRORS
            ;;
    esac

    # Quiet parallel make, redo loud sequential if that failed
    build_to_only_catch_errors_target all

    # Can be noisy if regen is needed (DMF branch with this or that BUILD_TGT)
    # Bail out due to DMF will (optionally) happen in the next check
    #GIT_DIFF_SHOW=false FILE_DESCR="DMF" FILE_REGEX='\.dmf$' FILE_GLOB='*.dmf' check_gitignore "$BUILD_TGT" || true

    # TODO (when merging DMF branch, not a problem before then):
    # this one check should not-list the "*.dmf" files even if
    # changed (listed as a special group above) but should still
    # fail due to them:
    check_gitignore "all" || exit

    if test -s "${SCRIPTDIR}/install-sh" \
    && grep -w MKDIRPROG "${SCRIPTDIR}/install-sh" >/dev/null \
    ; then
         if grep -v '#' "${SCRIPTDIR}/install-sh" | grep -E '\$mkdirprog.*-p' >/dev/null \
        ; then
            true
        else
            if [ -z "${MKDIRPROG-}" ] ; then
                echo "`date`: WARNING: setting MKDIRPROG to work around possible deficiencies of install-sh"
                MKDIRPROG="mkdir -p"
                export MKDIRPROG
            fi
        fi
    fi

    [ -z "$CI_TIME" ] || echo "`date`: Trying to install the currently tested project into the custom DESTDIR..."
    $CI_TIME $MAKE $MAKE_FLAGS_VERBOSE DESTDIR="$INST_PREFIX" install
    [ -n "$CI_TIME" ] && echo "`date`: listing files installed into the custom DESTDIR..." && \
        find "$INST_PREFIX" -ls || true

    if [ "$DO_DISTCHECK" = "no" ] ; then
        echo "Skipping distcheck (by caller request or BUILD_TYPE specifics)"
    else
        [ -z "$CI_TIME" ] || echo "`date`: Starting distcheck of currently tested project..."
        (
        # Note: Makefile.am already sets some default DISTCHECK_CONFIGURE_FLAGS
        # that include DISTCHECK_FLAGS if provided
        DISTCHECK_FLAGS="`for F in "${CONFIG_OPTS[@]}" ; do echo "'$F' " ; done | tr '\n' ' '`"
        export DISTCHECK_FLAGS

        # Tell the sub-makes (distcheck) to hush down
        MAKEFLAGS="${MAKEFLAGS-} $MAKE_FLAGS_QUIET" \
        $CI_TIME $MAKE DISTCHECK_FLAGS="$DISTCHECK_FLAGS" $PARMAKE_FLAGS ${DISTCHECK_TGT}

        #FILE_DESCR="DMF" FILE_REGEX='\.dmf$' FILE_GLOB='*.dmf' check_gitignore "$BUILD_TGT" || true
        check_gitignore "${DISTCHECK_TGT}" || exit
        )
    fi

    optional_maintainer_clean_check || exit

    ccache_stats "after"
    ;;
bindings)
    pushd "./bindings/${BINDING}" && ./ci_build.sh
    ;;
""|inplace|doc*)
    if [ x"${BUILD_TYPE}" = x ] ; then
        _msg="No BUILD_TYPE"
    else
        _msg="BUILD_TYPE='${BUILD_TYPE}'"
    fi
    echo "WARNING: ${_msg} was specified, doing a minimal default ritual without any *required* build products and with developer-oriented options" >&2
    if [ -n "${BUILD_WARNOPT}${BUILD_WARNFATAL}" ]; then
        echo "WARNING: BUILD_WARNOPT and BUILD_WARNFATAL settings are ignored in this mode (warnings are always enabled and fatal for these developer-oriented builds)" >&2
        sleep 5
    fi
    echo ""

    # NOTE: Alternative to optional_prepare_ccache()
    # FIXME: Can these be united and de-duplicated?
    if [ -n "${CI_CCACHE_SYMLINKDIR}" ] && [ -d "${CI_CCACHE_SYMLINKDIR}" ] ; then
        PATH="`echo "$PATH" | sed -e 's,^'"${CI_CCACHE_SYMLINKDIR}"'/?:,,' -e 's,:'"${CI_CCACHE_SYMLINKDIR}"'/?:,,' -e 's,:'"${CI_CCACHE_SYMLINKDIR}"'/?$,,' -e 's,^'"${CI_CCACHE_SYMLINKDIR}"'/?$,,'`"
        CCACHE_PATH="$PATH"
        CCACHE_DIR="${HOME}/.ccache"
        if (command -v ccache || which ccache) && ls -la "${CI_CCACHE_SYMLINKDIR}" && mkdir -p "${CCACHE_DIR}"/ ; then
            echo "INFO: Using ccache via PATH preferring tool names in ${CI_CCACHE_SYMLINKDIR}" >&2
            PATH="${CI_CCACHE_SYMLINKDIR}:$PATH"
            export CCACHE_PATH CCACHE_DIR PATH
            HAVE_CCACHE=yes
            mkdir -p "${CCACHE_DIR}"/ || HAVE_CCACHE=no
        else
            HAVE_CCACHE=no
        fi
    fi

    optional_prepare_compiler_family

    cd "${SCRIPTDIR}"

    consider_cleanup_shortcut

    if [ -s Makefile ]; then
        # Help developers debug:
        # Let initial clean-up be at default verbosity
        echo "=== Starting initial clean-up (from old build products)"
        ${MAKE} realclean -k || true
        echo "=== Finished initial clean-up"
    fi

    configure_CI_BUILDDIR

    # NOTE: Default NUT "configure" actually insists on some features,
    # like serial port support unless told otherwise, or docs if possible.
    # Below we aim for really fast iterations of C/C++ development so
    # enable whatever is auto-detectable (except docs), and highlight
    # any warnings if we can.
    CONFIG_OPTS=(--enable-Wcolor \
        --enable-warnings --enable-Werror \
        --enable-keep_nut_report_feature \
        --with-all=auto --with-cgi=auto --with-serial=auto \
        --with-dev=auto \
        --with-nut_monitor=auto --with-pynut=auto \
        --disable-force-nut-version-header \
        --enable-check-NIT --enable-maintainer-mode)

    case x"${BUILD_TYPE}" in
        xdoc*) CONFIG_OPTS+=("--with-${BUILD_TYPE}") ;;
        *) CONFIG_OPTS+=("--with-doc=skip") ;;
    esac

    detect_platform_PKG_CONFIG_PATH_and_FLAGS
    if [ -n "$PKG_CONFIG_PATH" ] ; then
        CONFIG_OPTS+=("PKG_CONFIG_PATH=${PKG_CONFIG_PATH}")
    fi

    # Primarily here to ensure libusb-1.0 use on MSYS2/mingw
    # when 0.1 is available too
    if [ "${CANBUILD_WITH_LIBMODBUS_USB-}" = yes ] ; then
        CONFIG_OPTS+=("--with-modbus+usb=yes")
    fi

    # Not default for parameter-less build, to prevent "make check-NIT"
    # from somehow interfering with the running daemons.
    if [ x"${INPLACE_RUNTIME-}" = xtrue ] || [ x"${BUILD_TYPE-}" = xinplace ] ; then
        CONFIG_OPTS+=("--enable-inplace-runtime")
    else
        # Help developers debug:
        CONFIG_OPTS+=("--disable-silent-rules")
    fi

    if [ -n "${BUILD_DEBUGINFO-}" ]; then
        CONFIG_OPTS+=("--with-debuginfo=${BUILD_DEBUGINFO}")
    else
        CONFIG_OPTS+=("--with-debuginfo=auto")
    fi

    if [ -n "${PYTHON-}" ]; then
        # WARNING: Watch out for whitespaces, not handled here!
        CONFIG_OPTS+=("--with-python=${PYTHON}")
    fi

    if optional_ensure_ccache ; then
        # Note: Potentially there can be spaces in entries for multiword
        # "ccache gcc" here; this should be okay as long as entry expands to
        # one token when calling shell (may not be the case for distcheck)
        CONFIG_OPTS+=("CC=${CC}")
        CONFIG_OPTS+=("CXX=${CXX}")
        CONFIG_OPTS+=("CPP=${CPP}")
    fi

    # If detect_platform_PKG_CONFIG_PATH_and_FLAGS() customized anything here,
    # let configure script know
    _EXPORT_FLAGS=true
    case "${CI_OS_NAME}" in
        windows-msys2)
            if [ "$HAVE_CCACHE" = yes ] ; then
                # NO-OP: Its ccache gets confused, apparently parsing the flags as one parameter
                #   configure:6064: checking whether the C compiler works
                #   configure:6086: /mingw64/lib/ccache/bin/gcc  -D_POSIX=1 -D_POSIX_C_SOURCE=200112L -D_WIN32_WINNT=0xffff   conftest.c  >&5
                #   Cannot create temporary file in C:\msys64\home\abuild\nut-win\ -D_POSIX=1 -D_POSIX_C_SOURCE=200112L -D_WIN32_WINNT=0xffff\: No such file or directory
                # FIXME: Detect better if this bites us on the current system?
                _EXPORT_FLAGS=false
            fi
            ;;
    esac

    if [ "${_EXPORT_FLAGS}" = true ] ; then
            [ -z "${CFLAGS}" ]   || export CFLAGS
            [ -z "${CXXFLAGS}" ] || export CXXFLAGS
            [ -z "${CPPFLAGS}" ] || export CPPFLAGS
            [ -z "${LDFLAGS}" ]  || export LDFLAGS
    else
            # NOTE: Passing via CONFIG_OPTS also fails
            [ -z "${CFLAGS}" ]   || echo "WARNING: SKIP: On '${CI_OS_NAME}' with ccache used, can not export CFLAGS='${CFLAGS}'" >&2
            [ -z "${CXXFLAGS}" ] || echo "WARNING: SKIP: On '${CI_OS_NAME}' with ccache used, can not export CXXFLAGS='${CXXFLAGS}'" >&2
            [ -z "${CPPFLAGS}" ] || echo "WARNING: SKIP: On '${CI_OS_NAME}' with ccache used, can not export CPPFLAGS='${CPPFLAGS}'" >&2
            [ -z "${LDFLAGS}" ]  || echo "WARNING: SKIP: On '${CI_OS_NAME}' with ccache used, can not export LDFLAGS='${LDFLAGS}'" >&2
    fi

    PATH="`echo "${PATH}" | normalize_path`"
    CCACHE_PATH="`echo "${CCACHE_PATH}" | normalize_path`"

    RES_CFG=0
    ${CONFIGURE_SCRIPT} "${CONFIG_OPTS[@]}" \
    || RES_CFG=$?
    echo "$0: configure phase complete ($RES_CFG)" >&2
    [ x"$RES_CFG" = x0 ] || exit $RES_CFG

    # NOTE: Currently parallel builds are expected to succeed (as far
    # as recipes are concerned), and the builds without a BUILD_TYPE
    # are aimed at developer iterations so not tweaking verbosity.
    echo "Configuration finished, starting make" >&2
    if [ -n "$PARMAKE_FLAGS" ]; then
        echo "For parallel builds, '$PARMAKE_FLAGS' options would be used" >&2
    fi
    if [ -n "$MAKEFLAGS" ]; then
        echo "Generally, MAKEFLAGS='$MAKEFLAGS' options would be passed" >&2
    fi

    #$MAKE all || \
    $MAKE $PARMAKE_FLAGS all || exit
    if [ "${CI_SKIP_CHECK}" != true ] ; then $MAKE check || exit ; fi

    case "$CI_OS_NAME" in
        windows*)
            echo "INFO: Build and tests succeeded. If you plan to install a NUT bundle now" >&2
            echo "for practical usage or testing on a native Windows system, consider calling" >&2
            echo "    make install-win-bundle DESTDIR=`pwd`/.inst/NUT4Win" >&2
            echo "(or some other valid DESTDIR) to co-bundle dependency FOSS DLL files there." >&2
            ;;
    esac

    if [ -s config.nut_report_feature.log ]; then
        cat config.nut_report_feature.log
    fi
    ;;

# These mingw modes below are currently experimental and not too integrated
# with this script per se; it is intended to run for NUT CI farm on prepared
# Linux+mingw worker nodes (see scripts/Windows/README.adoc) in an uniform
# manner, using mostly default settings (warnings in particular) and some
# values hardcoded in that script (ARCH, CFLAGS, ...).
# Note that semi-native builds with e.g. MSYS2 on Windows should "just work" as
# on any other supported platform (more details in docs/config-prereqs.txt).
cross-windows-mingw*)
    echo "INFO: When using build-mingw-nut.sh consider 'export INSTALL_WIN_BUNDLE=true' to use mainstream DLL co-bundling recipe" >&2

    if [ "$HAVE_CCACHE" = yes ] \
    && [ -n "${CI_CCACHE_SYMLINKDIR}" ] \
    && [ -d "${CI_CCACHE_SYMLINKDIR}" ] \
    ; then
        PATH="`echo "$PATH" | sed -e 's,^'"${CI_CCACHE_SYMLINKDIR}"'/?:,,' -e 's,:'"${CI_CCACHE_SYMLINKDIR}"'/?:,,' -e 's,:'"${CI_CCACHE_SYMLINKDIR}"'/?$,,' -e 's,^'"${CI_CCACHE_SYMLINKDIR}"'/?$,,'`"
        CCACHE_PATH="$PATH"
        CCACHE_DIR="${HOME}/.ccache"
        if (command -v ccache || which ccache) && ls -la "${CI_CCACHE_SYMLINKDIR}" && mkdir -p "${CCACHE_DIR}"/ ; then
            echo "INFO: Using ccache via PATH preferring tool names in ${CI_CCACHE_SYMLINKDIR}" >&2
            PATH="${CI_CCACHE_SYMLINKDIR}:$PATH"
            export CCACHE_PATH CCACHE_DIR PATH
        fi
    fi

    ./autogen.sh || exit
    cd scripts/Windows || exit

    cmd="" # default soup of the day, as defined in the called script
    case "$BUILD_TYPE" in
        cross-windows-mingw32|cross-windows-mingw-32) cmd="all32" ;;
        cross-windows-mingw64|cross-windows-mingw-64) cmd="all64" ;;
        cross-windows-mingw) # make a difficult guess
            case "${BITS-}" in
                32|64) cmd="all${BITS}"
                    ;;
                *)  # Use other clues
                    case "${CFLAGS-}${CXXFLAGS-}${LDFLAGS-}" in
                        *-m32*-m64*|*-m64*-m32*)
                            echo "FATAL: Mismatched bitness requested in *FLAGS" >&2
                            exit 1
                            ;;
                        *-m32*) cmd="all32" ;;
                        *-m64*) cmd="all64" ;;
                    esac
                    ;;
            esac
            ;;
    esac

    SOURCEMODE="out-of-tree" \
    MAKEFLAGS="$PARMAKE_FLAGS" \
    KEEP_NUT_REPORT_FEATURE="true" \
    ./build-mingw-nut.sh $cmd
    ;;

*)
    pushd "./builds/${BUILD_TYPE}" && REPO_DIR="$(dirs -l +1)" ./ci_build.sh
    ;;
esac
