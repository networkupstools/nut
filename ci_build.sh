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

# Quick hijack for interactive development like this:
#   BUILD_TYPE=fightwarn-clang ./ci_build.sh
# or to quickly hit the first-found errors in a larger matrix
# (and then easily `make` to iterate fixes), like this:
#   CI_REQUIRE_GOOD_GITIGNORE="false" CI_FAILFAST=true DO_CLEAN_CHECK=no BUILD_TYPE=fightwarn ./ci_build.sh
case "$BUILD_TYPE" in
    fightwarn) ;; # for default compiler
    fightwarn-gcc)
        CC="gcc"
        CXX="g++"
        CPP="cpp"
        BUILD_TYPE=fightwarn
        ;;
    fightwarn-clang)
        CC="clang"
        CXX="clang++"
        CPP="clang-cpp"
        BUILD_TYPE=fightwarn
        ;;
esac

if [ "$BUILD_TYPE" = fightwarn ]; then
    # For CFLAGS/CXXFLAGS keep caller or compiler defaults
    # (including C/C++ revision)
    BUILD_TYPE=default-all-errors
    BUILD_WARNFATAL=yes

    # Current fightwarn goal is to have no warnings at preset level below:
    #[ -n "$BUILD_WARNOPT" ] || BUILD_WARNOPT=hard
    [ -n "$BUILD_WARNOPT" ] || BUILD_WARNOPT=medium

    # Eventually this constraint would be removed to check all present
    # SSL implementations since their ifdef-driven codebases differ and
    # emit varied warnings. But so far would be nice to get the majority
    # of shared codebase clean first:
    [ -n "$NUT_SSL_VARIANTS" ] || NUT_SSL_VARIANTS=auto

    # Similarly for libusb implementations with varying support
    [ -n "$NUT_USB_VARIANTS" ] || NUT_USB_VARIANTS=auto
fi

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

[ -n "$MAKE" ] || MAKE=make
[ -n "$GGREP" ] || GGREP=grep

[ -n "$MAKE_FLAGS_QUIET" ] || MAKE_FLAGS_QUIET="VERBOSE=0 V=0 -s"
[ -n "$MAKE_FLAGS_VERBOSE" ] || MAKE_FLAGS_VERBOSE="VERBOSE=1 -s"

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
    if LANG=C LC_ALL=C "$MAKE" --version 2>&1 | egrep 'GNU Make|Free Software Foundation' > /dev/null ; then
        PARMAKE_FLAGS="$PARMAKE_FLAGS -l $PARMAKE_LA_LIMIT"
        echo "Parallel builds would spawn up to $NPARMAKES jobs (detected $NCPUS CPUs), or peak out at $PARMAKE_LA_LIMIT system load average" >&2
    else
        echo "Parallel builds would spawn up to $NPARMAKES jobs (detected $NCPUS CPUs)" >&2
    fi
fi

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

        # Some (QEMU) builders have issues running valgrind as a tool
        "NUT_BUILD_CAPS=valgrind=no")
            [ -n "$CANBUILD_VALGRIND_TESTS" ] || CANBUILD_VALGRIND_TESTS=no ;;
        "NUT_BUILD_CAPS=valgrind"|"NUT_BUILD_CAPS=valgrind=yes")
            [ -n "$CANBUILD_VALGRIND_TESTS" ] || CANBUILD_VALGRIND_TESTS=yes ;;

        "NUT_BUILD_CAPS=cppcheck=no")
            [ -n "$CANBUILD_CPPCHECK_TESTS" ] || CANBUILD_CPPCHECK_TESTS=no ;;
        "NUT_BUILD_CAPS=cppcheck"|"NUT_BUILD_CAPS=cppcheck=yes")
            [ -n "$CANBUILD_CPPCHECK_TESTS" ] || CANBUILD_CPPCHECK_TESTS=yes ;;

        "NUT_BUILD_CAPS=docs:man=no")
            [ -n "$CANBUILD_DOCS_MAN" ] || CANBUILD_DOCS_MAN=no ;;
        "NUT_BUILD_CAPS=docs:man"|"NUT_BUILD_CAPS=docs:man=yes")
            [ -n "$CANBUILD_DOCS_MAN" ] || CANBUILD_DOCS_MAN=yes ;;

        "NUT_BUILD_CAPS=docs:all=no")
            [ -n "$CANBUILD_DOCS_ALL" ] || CANBUILD_DOCS_ALL=no ;;
        "NUT_BUILD_CAPS=docs:all"|"NUT_BUILD_CAPS=docs:all=yes")
            [ -n "$CANBUILD_DOCS_ALL" ] || CANBUILD_DOCS_ALL=yes ;;

        "NUT_BUILD_CAPS=drivers:all=no")
            [ -n "$CANBUILD_DRIVERS_ALL" ] || CANBUILD_DRIVERS_ALL=no ;;
        "NUT_BUILD_CAPS=drivers:all"|"NUT_BUILD_CAPS=drivers:all=yes")
            [ -n "$CANBUILD_DRIVERS_ALL" ] || CANBUILD_DRIVERS_ALL=yes ;;

        "NUT_BUILD_CAPS=cgi=no")
            [ -n "$CANBUILD_LIBGD_CGI" ] || CANBUILD_LIBGD_CGI=no ;;
        "NUT_BUILD_CAPS=cgi"|"NUT_BUILD_CAPS=cgi=yes")
            [ -n "$CANBUILD_LIBGD_CGI" ] || CANBUILD_LIBGD_CGI=yes ;;
    esac
done

if [ -z "$CI_OS_NAME" ]; then
    # Check for dynaMatrix node labels support and map into a simple
    # classification styled after (compatible with) that in Travis CI
    for CI_OS_HINT in \
        "$OS_FAMILY-$OS_DISTRO" \
        "`grep = /etc/os-release 2>/dev/null`" \
        "`cat /etc/release 2>/dev/null`" \
        "`uname -o`" \
        "`uname -s -r -v`" \
        "`uname -a`" \
    ; do
        [ -z "$CI_OS_HINT" -o "$CI_OS_HINT" = "-" ] || break
    done

    case "`echo "$CI_OS_HINT" | tr 'A-Z' 'a-z'`" in
        *freebsd*)
            CI_OS_NAME="freebsd" ;;
        *debian*|*ubuntu*)
            CI_OS_NAME="debian" ;;
        *centos*|*fedora*|*redhat*|*rhel*)
            CI_OS_NAME="centos" ;;
        *linux*)
            CI_OS_NAME="linux" ;;
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

# Analyze some environmental choices
if [ -z "${CANBUILD_LIBGD_CGI-}" ]; then
    # No prereq dll and headers on win so far
    [[ "$CI_OS_NAME" = "windows" ]] && CANBUILD_LIBGD_CGI=no

    # NUT CI farm with Jenkins can build it; Travis could not
    [[ "$CI_OS_NAME" = "freebsd" ]] && CANBUILD_LIBGD_CGI=yes \
    || [[ "$TRAVIS_OS_NAME" = "freebsd" ]] && CANBUILD_LIBGD_CGI=no

    # See also below for some compiler-dependent decisions
fi

configure_nut() {
    local CONFIGURE_SCRIPT=./configure
    if [[ "$CI_OS_NAME" == "windows" ]] ; then
        find . -ls
        CONFIGURE_SCRIPT=./configure.bat
    fi

    if [ ! -s "$CONFIGURE_SCRIPT" ]; then
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

    # Help copy-pasting build setups from CI logs to terminal:
    local CONFIG_OPTS_STR="`for F in "${CONFIG_OPTS[@]}" ; do echo "'$F' " ; done`" ### | tr '\n' ' '`"
    while : ; do # Note the CI_SHELL_IS_FLAKY=true support below
      echo "=== CONFIGURING NUT: $CONFIGURE_SCRIPT ${CONFIG_OPTS_STR}"
      echo "=== CC='$CC' CXX='$CXX' CPP='$CPP'"
      [ -z "${CI_SHELL_IS_FLAKY-}" ] || echo "=== CI_SHELL_IS_FLAKY='$CI_SHELL_IS_FLAKY'"
      $CI_TIME $CONFIGURE_SCRIPT "${CONFIG_OPTS[@]}" \
      && return 0 \
      || { RES_CFG=$?
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
            echo "WOULD BE FATAL: FAILED ($RES_CFG) to ./configure ${CONFIG_OPTS[*]} -- but asked to loop trying" >&2
        else
            echo "FATAL: FAILED ($RES_CFG) to ./configure ${CONFIG_OPTS[*]}" >&2
            echo "If you are sure this is not a fault of scripting or config option, try" >&2
            echo "    CI_SHELL_IS_FLAKY=true $0"
            exit $RES_CFG
        fi
       }
    done
}

build_to_only_catch_errors_target() {
    if [ $# = 0 ]; then
        build_to_only_catch_errors_target all ; return $?
    fi

    # Sub-shells to avoid crashing with "unhandled" faults in "set -e" mode:
    ( echo "`date`: Starting the parallel build attempt (quietly to build what we can) for '$@' ..."; \
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
    ( echo "`date`: Starting the sequential build attempt (to list remaining files with errors considered fatal for this build configuration) for '$@'..."; \
      $CI_TIME $MAKE $MAKE_FLAGS_VERBOSE "$@" -k ) || return $?
    return 0
}

build_to_only_catch_errors() {
    build_to_only_catch_errors_target all || return $?

    echo "`date`: Starting a '$MAKE check' for quick sanity test of the products built with the current compiler and standards"
    $CI_TIME $MAKE $MAKE_FLAGS_QUIET check \
    && echo "`date`: SUCCESS" \
    || return $?

    return 0
}

ccache_stats() {
    local WHEN="$1"
    [ -n "$WHEN" ] || WHEN="some time around the"
    if [ "$HAVE_CCACHE" = yes ]; then
        if [ -d "$CCACHE_DIR" ]; then
            echo "CCache stats $WHEN build:"
            ccache -s || true
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
    [ -n "${FILE_GLOB-}" ] || FILE_GLOB='*'
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

    # One invocation should report to log:
    git status $GIT_ARGS -s -- "${FILE_GLOB}" \
    | egrep -v '^.. \.ci.*\.log.*' \
    | egrep "${FILE_REGEX}" \
    || echo "WARNING: Could not query git repo while in `pwd`" >&2
    echo "==="

    # Another invocation checks that there was nothing to complain about:
    if [ -n "`git status $GIT_ARGS -s "${FILE_GLOB}" | egrep -v '^.. \.ci.*\.log.*' | egrep "^.. ${FILE_REGEX}"`" ] \
    && [ "$CI_REQUIRE_GOOD_GITIGNORE" != false ] \
    ; then
        echo "FATAL: There are changes in $FILE_DESCR files listed above - tracked sources should be updated in the PR (even if generated - not all builders can do so), and build products should be added to a .gitignore file, everything made should be cleaned and no tracked files should be removed!" >&2
        if [ "$GIT_DIFF_SHOW" = true ]; then
            git diff -- "${FILE_GLOB}" || true
        fi
        echo "==="
        return 1
    fi
    return 0
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
        $CI_TIME $MAKE DISTCHECK_FLAGS="$DISTCHECK_FLAGS" $PARMAKE_FLAGS maintainer-clean || return

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
        $CI_TIME $MAKE DISTCHECK_FLAGS="$DISTCHECK_FLAGS" $PARMAKE_FLAGS distclean || return

        check_gitignore "distclean" || return
    fi
    return 0
}

if [ "$1" = spellcheck ] && [ -z "$BUILD_TYPE" ] ; then
    # Note: this is a little hack to reduce typing
    # and scrolling in (docs) developer iterations.
    if [ -s Makefile ] && [ -s docs/Makefile ]; then
        echo "Processing quick and quiet spellcheck with already existing recipe files, will only report errors if any ..."
        build_to_only_catch_errors_target spellcheck ; exit
    else
        BUILD_TYPE="default-spellcheck"
        shift
    fi
fi

echo "Processing BUILD_TYPE='${BUILD_TYPE}' ..."

echo "Build host settings:"
set | egrep '^(CI_.*|OS_*|CANBUILD_.*|NODE_LABELS|MAKE|C.*FLAGS|LDFLAGS|CC|CXX|DO_.*|BUILD_.*)=' || true
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
    mkdir -p tmp/ .inst/
    BUILD_PREFIX="$PWD/tmp"
    INST_PREFIX="$PWD/.inst"

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

    PATH="`echo "$PATH" | sed -e 's,^/usr/lib/ccache/?:,,' -e 's,:/usr/lib/ccache/?:,,' -e 's,:/usr/lib/ccache/?$,,' -e 's,^/usr/lib/ccache/?$,,'`"
    CCACHE_PATH="$PATH"
    CCACHE_DIR="${HOME}/.ccache"
    export CCACHE_PATH CCACHE_DIR PATH
    HAVE_CCACHE=no
    if (command -v ccache || which ccache) && ls -la /usr/lib/ccache ; then
        HAVE_CCACHE=yes
    fi
    mkdir -p "${CCACHE_DIR}"/ || HAVE_CCACHE=no

    ccache_stats "before"

    CONFIG_OPTS=()
    COMMON_CFLAGS=""
    EXTRA_CFLAGS=""
    EXTRA_CPPFLAGS=""
    EXTRA_CXXFLAGS=""

    is_gnucc() {
        if [ -n "$1" ] && "$1" --version 2>&1 | grep 'Free Software Foundation' > /dev/null ; then true ; else false ; fi
    }

    is_clang() {
        if [ -n "$1" ] && "$1" --version 2>&1 | grep 'clang version' > /dev/null ; then true ; else false ; fi
    }

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
        elif is_clang "clang" && is_clang "clang++" ; then
            # Autoconf would pick this by default
            COMPILER_FAMILY="CLANG"
            [ -n "$CC" ] || CC=clang
            [ -n "$CXX" ] || CXX=clang++
            export CC CXX
        elif is_clang "cc" && is_clang "c++" ; then
            COMPILER_FAMILY="CLANG"
            [ -n "$CC" ] || CC=cc
            [ -n "$CXX" ] || CXX=c++
            export CC CXX
        fi
    fi

    if [ -n "$CPP" ] ; then
        [ -x "$CPP" ] && export CPP
    else
        if is_gnucc "cpp" ; then
            CPP=cpp && export CPP
        fi
    fi

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
                esac
            fi
        fi
    fi

    # Note: Potentially there can be spaces in entries for multiple
    # *FLAGS here; this should be okay as long as entry expands to
    # one token when calling shell (may not be the case for distcheck)
    CONFIG_OPTS+=("CFLAGS=-I${BUILD_PREFIX}/include ${CFLAGS}")
    CONFIG_OPTS+=("CPPFLAGS=-I${BUILD_PREFIX}/include ${CPPFLAGS}")
    CONFIG_OPTS+=("CXXFLAGS=-I${BUILD_PREFIX}/include ${CXXFLAGS}")
    CONFIG_OPTS+=("LDFLAGS=-L${BUILD_PREFIX}/lib ${LDFLAGS}")

    DEFAULT_PKG_CONFIG_PATH="${BUILD_PREFIX}/lib/pkgconfig"
    SYSPKG_CONFIG_PATH="" # Let the OS guess... usually
    case "`echo "$CI_OS_NAME" | tr 'A-Z' 'a-z'`" in
        *openindiana*|*omnios*|*solaris*|*illumos*|*sunos*)
            case "$CC$CXX$CFLAGS$CXXFLAGS$LDFLAGS" in
                *-m64*)
                    SYS_PKG_CONFIG_PATH="/usr/lib/64/pkgconfig:/usr/lib/amd64/pkgconfig:/usr/lib/sparcv9/pkgconfig:/usr/lib/pkgconfig"
                    ;;
                *-m32*)
                    SYS_PKG_CONFIG_PATH="/usr/lib/32/pkgconfig:/usr/lib/pkgconfig:/usr/lib/i86pc/pkgconfig:/usr/lib/i386/pkgconfig:/usr/lib/sparcv7/pkgconfig"
                    ;;
                *)
                    case "$ARCH$BITS" in
                        *64*)
                            SYS_PKG_CONFIG_PATH="/usr/lib/64/pkgconfig:/usr/lib/amd64/pkgconfig:/usr/lib/sparcv9/pkgconfig:/usr/lib/pkgconfig"
                            ;;
                        *32*)
                            SYS_PKG_CONFIG_PATH="/usr/lib/32/pkgconfig:/usr/lib/pkgconfig:/usr/lib/i86pc/pkgconfig:/usr/lib/i386/pkgconfig:/usr/lib/sparcv7/pkgconfig"
                            ;;
                    esac
                    ;;
            esac
            ;;
    esac
    if [ -n "$SYS_PKG_CONFIG_PATH" ] ; then
        if [ -n "$PKG_CONFIG_PATH" ] ; then
            PKG_CONFIG_PATH="$SYS_PKG_CONFIG_PATH:$PKG_CONFIG_PATH"
        else
            PKG_CONFIG_PATH="$SYS_PKG_CONFIG_PATH"
        fi
    fi
    if [ -n "$PKG_CONFIG_PATH" ] ; then
        CONFIG_OPTS+=("PKG_CONFIG_PATH=${DEFAULT_PKG_CONFIG_PATH}:${PKG_CONFIG_PATH}")
    else
        CONFIG_OPTS+=("PKG_CONFIG_PATH=${DEFAULT_PKG_CONFIG_PATH}")
    fi

    CONFIG_OPTS+=("--prefix=${BUILD_PREFIX}")
    CONFIG_OPTS+=("--sysconfdir=${BUILD_PREFIX}/etc/nut")
    CONFIG_OPTS+=("--with-udev-dir=${BUILD_PREFIX}/etc/udev")
    CONFIG_OPTS+=("--with-devd-dir=${BUILD_PREFIX}/etc/devd")
    CONFIG_OPTS+=("--with-hotplug-dir=${BUILD_PREFIX}/etc/hotplug")

    if [ -n "${PYTHON-}" ]; then
        # WARNING: Watch out for whitespaces, not handled here!
        CONFIG_OPTS+=("--with-python=${PYTHON}")
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

    if [ "${CANBUILD_VALGRIND_TESTS-}" = no ] ; then
        echo "WARNING: Build agent says it has a broken valgrind, adding configure option to skip tests with it" >&2
        CONFIG_OPTS+=("--with-valgrind=no")
    fi

    # This flag is primarily linked with (lack of) docs generation enabled
    # (or not) in some BUILD_TYPE scenarios or workers. Initial value may
    # be set by caller, but codepaths below have the final word.
    [ "${DO_DISTCHECK-}" = no ] || DO_DISTCHECK=yes
    case "$BUILD_TYPE" in
        "default-nodoc")
            CONFIG_OPTS+=("--with-doc=no")
            DO_DISTCHECK=no
            ;;
        "default-spellcheck"|"default-shellcheck")
            CONFIG_OPTS+=("--with-all=no")
            CONFIG_OPTS+=("--with-libltdl=no")
            CONFIG_OPTS+=("--with-doc=man=skip")
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
            # Enable as many binaries to build as current worker setup allows
            CONFIG_OPTS+=("--with-all=auto")

            if [ "${CANBUILD_LIBGD_CGI-}" != "no" ] && [ "${BUILD_LIBGD_CGI-}" != "auto" ]  ; then
                # Currently --with-all implies this, but better be sure to
                # really build everything we can to be certain it builds:
                if pkg-config --exists libgd || pkg-config --exists libgd2 || pkg-config --exists libgd3 || pkg-config --exists gdlib ; then
                    CONFIG_OPTS+=("--with-cgi=yes")
                else
                    # Note: CI-wise, our goal IS to test as much as we can
                    # with this build, so environments should be set up to
                    # facilitate that as much as feasible. But reality is...
                    echo "WARNING: Seems libgd{,2,3} is not present, CGI build may be skipped!" >&2
                    CONFIG_OPTS+=("--with-cgi=auto")
                fi
            else
                CONFIG_OPTS+=("--with-cgi=auto")
            fi
            ;;
        "default-alldrv:no-distcheck")
            DO_DISTCHECK=no
            ;& # fall through
        "default-alldrv")
            # Do not build the docs and make possible a distcheck below
            CONFIG_OPTS+=("--with-doc=skip")
            if [ "${CANBUILD_DRIVERS_ALL-}" = no ]; then
                echo "WARNING: Build agent says it can't build 'all' driver types; will ask for what we can build" >&2
                if [ "$DO_DISTCHECK" != no ]; then
                    echo "WARNING: this is effectively default-tgt:distcheck-light then" >&2
                fi
                CONFIG_OPTS+=("--with-all=auto")
            else
                CONFIG_OPTS+=("--with-all=yes")
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
            ;;
        "default"|"default-tgt:"*|*)
            # Do not build the docs and tell distcheck it is okay
            CONFIG_OPTS+=("--with-doc=skip")
            ;;
    esac
    # NOTE: The case "$BUILD_TYPE" above was about setting CONFIG_OPTS.
    # There is another below for running actual scenarios.

    if [ "$HAVE_CCACHE" = yes ] && [ "${COMPILER_FAMILY}" = GCC -o "${COMPILER_FAMILY}" = CLANG ]; then
        PATH="/usr/lib/ccache:$PATH"
        export PATH
        if [ -n "$CC" ]; then
          if [ -x "/usr/lib/ccache/`basename "$CC"`" ]; then
            case "$CC" in
                *ccache*) ;;
                */*) DIR_CC="`dirname "$CC"`" && [ -n "$DIR_CC" ] && DIR_CC="`cd "$DIR_CC" && pwd `" && [ -n "$DIR_CC" ] && [ -d "$DIR_CC" ] || DIR_CC=""
                    [ -z "$CCACHE_PATH" ] && CCACHE_PATH="$DIR_CC" || \
                    if echo "$CCACHE_PATH" | egrep '(^'"$DIR_CC"':.*|^'"$DIR_CC"'$|:'"$DIR_CC"':|:'"$DIR_CC"'$)' ; then
                        CCACHE_PATH="$DIR_CC:$CCACHE_PATH"
                    fi
                    ;;
            esac
            CC="/usr/lib/ccache/`basename "$CC"`"
          else
            CC="ccache $CC"
          fi
        fi
        if [ -n "$CXX" ]; then
          if [ -x "/usr/lib/ccache/`basename "$CXX"`" ]; then
            case "$CXX" in
                *ccache*) ;;
                */*) DIR_CXX="`dirname "$CXX"`" && [ -n "$DIR_CXX" ] && DIR_CXX="`cd "$DIR_CXX" && pwd `" && [ -n "$DIR_CXX" ] && [ -d "$DIR_CXX" ] || DIR_CXX=""
                    [ -z "$CCACHE_PATH" ] && CCACHE_PATH="$DIR_CXX" || \
                    if echo "$CCACHE_PATH" | egrep '(^'"$DIR_CXX"':.*|^'"$DIR_CXX"'$|:'"$DIR_CXX"':|:'"$DIR_CXX"'$)' ; then
                        CCACHE_PATH="$DIR_CXX:$CCACHE_PATH"
                    fi
                    ;;
            esac
            CXX="/usr/lib/ccache/`basename "$CXX"`"
          else
            CXX="ccache $CXX"
          fi
        fi
        if [ -n "$CPP" ] && [ -x "/usr/lib/ccache/`basename "$CPP"`" ]; then
            case "$CPP" in
                *ccache*) ;;
                */*) DIR_CPP="`dirname "$CPP"`" && [ -n "$DIR_CPP" ] && DIR_CPP="`cd "$DIR_CPP" && pwd `" && [ -n "$DIR_CPP" ] && [ -d "$DIR_CPP" ] || DIR_CPP=""
                    [ -z "$CCACHE_PATH" ] && CCACHE_PATH="$DIR_CPP" || \
                    if echo "$CCACHE_PATH" | egrep '(^'"$DIR_CPP"':.*|^'"$DIR_CPP"'$|:'"$DIR_CPP"':|:'"$DIR_CPP"'$)' ; then
                        CCACHE_PATH="$DIR_CPP:$CCACHE_PATH"
                    fi
                    ;;
            esac
            CPP="/usr/lib/ccache/`basename "$CPP"`"
        else
            : # CPP="ccache $CPP"
        fi

        # Note: Potentially there can be spaces in entries for multiword
        # "ccache gcc" here; this should be okay as long as entry expands to
        # one token when calling shell (may not be the case for distcheck)
        CONFIG_OPTS+=("CC=${CC}")
        CONFIG_OPTS+=("CXX=${CXX}")
        CONFIG_OPTS+=("CPP=${CPP}")
    fi

    # Build and check this project; note that zprojects always have an autogen.sh
    [ -z "$CI_TIME" ] || echo "`date`: Starting build of currently tested project..."
    CCACHE_BASEDIR="${PWD}"
    export CCACHE_BASEDIR

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

    # Note: modern auto(re)conf requires pkg-config to generate the configure
    # script, so to stage the situation of building without one (as if on an
    # older system) we have to remove it when we already have the script.
    # This matches the use-case of distro-building from release tarballs that
    # include all needed pre-generated files to rely less on OS facilities.
    if [ -s Makefile ]; then
        # Let initial clean-up be at default verbosity
        echo "=== Starting initial clean-up (from old build products)"
        ${MAKE} maintainer-clean -k || ${MAKE} distclean -k || true
        echo "=== Finished initial clean-up"
    fi

    if [ "$CI_OS_NAME" = "windows" ] ; then
        $CI_TIME ./autogen.sh || true
    else
        $CI_TIME ./autogen.sh ### 2>/dev/null
    fi

    if [ "$NO_PKG_CONFIG" == "true" ] && [ "$CI_OS_NAME" = "linux" ] && (command -v dpkg) ; then
        echo "NO_PKG_CONFIG==true : BUTCHER pkg-config for this test case" >&2
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
            # e.g. distcheck-light, distcheck-valgrind, cppcheck, maybe
            # others later, as defined in Makefile.am:
            BUILD_TGT="`echo "$BUILD_TYPE" | sed 's,^default-tgt:,,'`"
            echo "`date`: Starting the sequential build attempt for singular target $BUILD_TGT..."

            # Note: Makefile.am already sets some default DISTCHECK_CONFIGURE_FLAGS
            # that include DISTCHECK_FLAGS if provided
            DISTCHECK_FLAGS="`for F in "${CONFIG_OPTS[@]}" ; do echo "'$F' " ; done | tr '\n' ' '`"
            export DISTCHECK_FLAGS

            # Tell the sub-makes (distcheck) to hush down
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
                if pkg-config --exists nss && pkg-config --exists openssl && [ "${BUILD_SSL_ONCE-}" != "true" ] ; then
                    # Try builds for both cases as they are ifdef-ed
                    # TODO: Extend if we begin to care about different
                    # major versions of openssl (with their APIs), etc.
                    NUT_SSL_VARIANTS="openssl nss"
                else
                    if [ "${BUILD_SSL_ONCE-}" != "true" ]; then
                        pkg-config --exists nss 2>/dev/null && NUT_SSL_VARIANTS="nss"
                        pkg-config --exists openssl 2>/dev/null && NUT_SSL_VARIANTS="openssl"
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
                if pkg-config --exists libusb-1.0 ; then
                    NUT_USB_VARIANTS="1.0"
                fi

                # TODO: Is there anywhere a `pkg-config --exists libusb-0.1`?
                if pkg-config --exists libusb || ( command -v libusb-config || which libusb-config ) 2>/dev/null >/dev/null ; then
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
                #$MAKE distclean -k || true

                echo "=== Starting NUT_SSL_VARIANT='$NUT_SSL_VARIANT', $BUILDSTODO build variants remaining..."
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
                        echo "=== Building with NUT_SSL_VARIANT='${NUT_SSL_VARIANT}' ..."
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

                echo "=== Configured NUT_SSL_VARIANT='$NUT_SSL_VARIANT', $BUILDSTODO build variants (including this one) remaining to complete; trying to build..."
                build_to_only_catch_errors && {
                    SUCCEEDED="${SUCCEEDED} NUT_SSL_VARIANT=${NUT_SSL_VARIANT}[build]"
                } || {
                    RES_ALLERRORS=$?
                    FAILED="${FAILED} NUT_SSL_VARIANT=${NUT_SSL_VARIANT}[build]"
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
                        $MAKE distclean -k || echo "WARNING: 'make distclean' FAILED: $? ... proceeding" >&2
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
                echo "=== Starting NUT_USB_VARIANT='$NUT_USB_VARIANT', $BUILDSTODO build variants remaining..."
                case "$NUT_USB_VARIANT" in
                    ""|auto|default)
                        # Quietly build one scenario, whatever we can (or not)
                        # configure regarding USB and other features
                        NUT_USB_VARIANT=auto
                        ( CONFIG_OPTS+=("--without-all")
                          CONFIG_OPTS+=("--without-ssl")
                          CONFIG_OPTS+=("--with-serial=auto")
                          CONFIG_OPTS+=("--with-usb")
                          configure_nut
                        )
                        ;;
                    no)
                        echo "=== Building without USB support (check mixed drivers coded for Serial/USB support)..."
                        ( CONFIG_OPTS+=("--without-all")
                          CONFIG_OPTS+=("--without-ssl")
                          CONFIG_OPTS+=("--with-serial=auto")
                          CONFIG_OPTS+=("--without-usb")
                          configure_nut
                        )
                        ;;
                    libusb-*)
                        echo "=== Building with NUT_USB_VARIANT='${NUT_USB_VARIANT}' ..."
                        ( CONFIG_OPTS+=("--without-all")
                          CONFIG_OPTS+=("--without-ssl")
                          CONFIG_OPTS+=("--with-serial=auto")
                          CONFIG_OPTS+=("--with-usb=${NUT_USB_VARIANT}")
                          configure_nut
                        )
                        ;;
                    *)
                        echo "=== Building with NUT_USB_VARIANT='${NUT_USB_VARIANT}' ..."
                        ( CONFIG_OPTS+=("--without-all")
                          CONFIG_OPTS+=("--without-ssl")
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

                echo "=== Configured NUT_USB_VARIANT='$NUT_USB_VARIANT', $BUILDSTODO build variants (including this one) remaining to complete; trying to build..."
                build_to_only_catch_errors && {
                    SUCCEEDED="${SUCCEEDED} NUT_USB_VARIANT=${NUT_USB_VARIANT}[build]"
                } || {
                    RES_ALLERRORS=$?
                    FAILED="${FAILED} NUT_USB_VARIANT=${NUT_USB_VARIANT}[build]"
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
                        $MAKE distclean -k || echo "WARNING: 'make distclean' FAILED: $? ... proceeding" >&2
                        echo "=== Completed sandbox cleanup after NUT_USB_VARIANT=${NUT_USB_VARIANT}, $BUILDSTODO build variants remaining"
                    else
                        echo "=== SKIPPED sandbox cleanup because DO_CLEAN_CHECK=$DO_CLEAN_CHECK and $BUILDSTODO build variants remaining"
                    fi
                fi
            done

            # TODO: Similar loops for other variations like TESTING,
            # MGE SHUT vs other serial protocols...

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

    # Can be noisy if regen is needed (DMF branch)
    # Bail out due to DMF will (optionally) happen in the next check
    GIT_DIFF_SHOW=false FILE_DESCR="DMF" FILE_REGEX='\.dmf$' FILE_GLOB='*.dmf' check_gitignore "$BUILD_TGT" || true

    # TODO (when merging DMF branch, not a problem before then):
    # this one check should not-list the "*.dmf" files even if
    # changed (listed as a special group above) but should still
    # fail due to them:
    check_gitignore "all" || exit

    [ -z "$CI_TIME" ] || echo "`date`: Trying to install the currently tested project into the custom DESTDIR..."
    $CI_TIME $MAKE $MAKE_FLAGS_VERBOSE DESTDIR="$INST_PREFIX" install
    [ -n "$CI_TIME" ] && echo "`date`: listing files installed into the custom DESTDIR..." && \
        find "$INST_PREFIX" -ls || true

    if [ "$DO_DISTCHECK" == "no" ] ; then
        echo "Skipping distcheck (doc generation is disabled, it would fail)"
    else
        [ -z "$CI_TIME" ] || echo "`date`: Starting distcheck of currently tested project..."
        (
        # Note: Makefile.am already sets some default DISTCHECK_CONFIGURE_FLAGS
        # that include DISTCHECK_FLAGS if provided
        DISTCHECK_FLAGS="`for F in "${CONFIG_OPTS[@]}" ; do echo "'$F' " ; done | tr '\n' ' '`"
        export DISTCHECK_FLAGS

        # Tell the sub-makes (distcheck) to hush down
        MAKEFLAGS="${MAKEFLAGS-} $MAKE_FLAGS_QUIET" \
        $CI_TIME $MAKE DISTCHECK_FLAGS="$DISTCHECK_FLAGS" $PARMAKE_FLAGS distcheck

        FILE_DESCR="DMF" FILE_REGEX='\.dmf$' FILE_GLOB='*.dmf' check_gitignore "$BUILD_TGT" || true
        check_gitignore "distcheck" || exit
        )
    fi

    optional_maintainer_clean_check || exit

    ccache_stats "after"
    ;;
bindings)
    pushd "./bindings/${BINDING}" && ./ci_build.sh
    ;;
"")
    echo "ERROR: No BUILD_TYPE was specified, doing a minimal default ritual without any required options" >&2
    if [ -n "${BUILD_WARNOPT}${BUILD_WARNFATAL}" ]; then
        echo "WARNING: BUILD_WARNOPT and BUILD_WARNFATAL settings are ignored in this mode" >&2
        sleep 5
    fi
    echo ""
    if [ -s Makefile ]; then
        # Let initial clean-up be at default verbosity
        echo "=== Starting initial clean-up (from old build products)"
        ${MAKE} realclean -k || true
        echo "=== Finished initial clean-up"
    fi

    ./autogen.sh

    # NOTE: Default NUT "configure" actually insists on some features,
    # like serial port support unless told otherwise, or docs if possible.
    # Below we aim for really fast iterations of C/C++ development so
    # enable whatever is auto-detectable (except docs), and highlight
    # any warnings if we can:
    #./configure
    ./configure --enable-Wcolor --with-all=auto --with-cgi=auto --with-serial=auto --with-dev=auto --with-doc=skip

    # NOTE: Currently parallel builds are expected to succeed (as far
    # as recipes are concerned), and the builds without a BUILD_TYPE
    # are aimed at developer iterations so not tweaking verbosity.
    #$MAKE all && \
    $MAKE $PARMAKE_FLAGS all && \
    $MAKE check
    ;;
*)
    pushd "./builds/${BUILD_TYPE}" && REPO_DIR="$(dirs -l +1)" ./ci_build.sh
    ;;
esac
