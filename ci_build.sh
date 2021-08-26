#!/usr/bin/env bash

################################################################################
# This file is based on a template used by zproject, but isn't auto-generated. #
################################################################################

set -e

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

[ -n "$MAKE" ] || MAKE=make
[ -n "$GGREP" ] || GGREP=grep

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
    case "$OS_FAMILY-$OS_DISTRO" in
        *freebsd*)
            CI_OS_NAME="freebsd" ;;
        *debian*|*linux*)
            CI_OS_NAME="debian" ;;
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
        "-") ;;
        *)  echo "WARNING: Could not recognize CI_OS_NAME from '$OS_FAMILY'-'$OS_DISTRO', update './ci_build.sh' if needed" >&2 ;;
    esac
fi

# CI builds on Travis
[ -n "$CI_OS_NAME" ] || CI_OS_NAME="$TRAVIS_OS_NAME"

# Analyze some environmental choices
if [ -z "${CANBUILD_LIBGD_CGI-}" ]; then
    # No prereq dll and headers on win so far
    [[ "$CI_OS_NAME" = "windows" ]] && CANBUILD_LIBGD_CGI=no

    # NUT CI farm with Jenkins can build it; Travis could not
    #[[ "$CI_OS_NAME" = "freebsd" ]] && CANBUILD_LIBGD_CGI=no
    [[ "$TRAVIS_OS_NAME" = "freebsd" ]] && CANBUILD_LIBGD_CGI=no
fi

configure_nut() {
    local CONFIGURE_SCRIPT=./configure
    if [[ "$CI_OS_NAME" == "windows" ]] ; then
        find . -ls
        CONFIGURE_SCRIPT=./configure.bat
    fi

    # Help copy-pasting build setups from CI logs to terminal:
    local CONFIG_OPTS_STR="`for F in "${CONFIG_OPTS[@]}" ; do echo "'$F' " ; done`" ### | tr '\n' ' '`"
    echo "=== CONFIGURING NUT: $CONFIGURE_SCRIPT ${CONFIG_OPTS_STR}"
    echo "=== CC='$CC' CXX='$CXX' CPP='$CPP'"
    $CI_TIME $CONFIGURE_SCRIPT "${CONFIG_OPTS[@]}" \
    || { RES=$?
        echo "FAILED ($RES) to configure nut, will dump config.log in a second to help troubleshoot CI" >&2
        echo "    (or press Ctrl+C to abort now if running interactively)" >&2
        sleep 5
        echo "=========== DUMPING config.log :"
        $GGREP -B 100 -A 1 'Cache variables' config.log 2>/dev/null \
        || cat config.log || true
        echo "=========== END OF config.log"
        echo "FATAL: FAILED ($RES) to ./configure ${CONFIG_OPTS[*]}" >&2
        exit $RES
       }
}

build_to_only_catch_errors() {
    ( echo "`date`: Starting the parallel build attempt (quietly to build what we can)..."; \
      $CI_TIME $MAKE VERBOSE=0 -k -j8 all >/dev/null 2>&1 && echo "`date`: SUCCESS" ; ) || \
    ( echo "`date`: Starting the sequential build attempt (to list remaining files with errors considered fatal for this build configuration)..."; \
      $CI_TIME $MAKE VERBOSE=1 all -k ) || return $?

    echo "`date`: Starting a '$MAKE check' for quick sanity test of the products built with the current compiler and standards"
    $CI_TIME $MAKE VERBOSE=0 check \
    && echo "`date`: SUCCESS" \
    || return $?

    return 0
}

echo "Processing BUILD_TYPE='${BUILD_TYPE}' ..."
case "$BUILD_TYPE" in
default|default-alldrv|default-all-errors|default-spellcheck|default-shellcheck|default-nodoc|default-withdoc|default-withdoc:man|"default-tgt:"*)
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
    if which ccache && ls -la /usr/lib/ccache ; then
        HAVE_CCACHE=yes
    fi
    mkdir -p "${CCACHE_DIR}"/ || HAVE_CCACHE=no

    if [ "$HAVE_CCACHE" = yes ] && [ -d "$CCACHE_DIR" ]; then
        echo "CCache stats before build:"
        ccache -s || true
    fi

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

    # Note: Potentially there can be spaces in entries for multiple
    # *FLAGS here; this should be okay as long as entry expands to
    # one token when calling shell (may not be the case for distcheck)
    CONFIG_OPTS+=("CFLAGS=-I${BUILD_PREFIX}/include ${CFLAGS}")
    CONFIG_OPTS+=("CPPFLAGS=-I${BUILD_PREFIX}/include ${CPPFLAGS}")
    CONFIG_OPTS+=("CXXFLAGS=-I${BUILD_PREFIX}/include ${CXXFLAGS}")
    CONFIG_OPTS+=("LDFLAGS=-L${BUILD_PREFIX}/lib")
    if [ -n "$PKG_CONFIG_PATH" ] ; then
        CONFIG_OPTS+=("PKG_CONFIG_PATH=${BUILD_PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH}")
    else
        CONFIG_OPTS+=("PKG_CONFIG_PATH=${BUILD_PREFIX}/lib/pkgconfig")
    fi
    CONFIG_OPTS+=("--prefix=${BUILD_PREFIX}")
    CONFIG_OPTS+=("--sysconfdir=${BUILD_PREFIX}/etc/nut")
    CONFIG_OPTS+=("--with-udev-dir=${BUILD_PREFIX}/etc/udev")
    CONFIG_OPTS+=("--with-devd-dir=${BUILD_PREFIX}/etc/devd")
    CONFIG_OPTS+=("--with-hotplug-dir=${BUILD_PREFIX}/etc/hotplug")

    # Some OSes have broken cppunit support, it crashes either build/link
    # or at run-time. While distros take time to figure out fixes, we can
    # skip the case...
    if [ "${CANBUILD_CPPUNIT_TESTS-}" = no ] \
    || ( [ "${CANBUILD_CPPUNIT_TESTS-}" = "no-gcc" ] && [ "$COMPILER_FAMILY" = "GCC" ] ) \
    || ( [ "${CANBUILD_CPPUNIT_TESTS-}" = "no-clang" ] && [ "$COMPILER_FAMILY" = "CLANG" ] ) \
    ; then
        echo "WARNING: Build agent says it can't build or run libcppunit tests, adding configure option to skip them" >&2
        CONFIG_OPTS+=("--with-cppunit=no")
    fi

    DO_DISTCHECK=yes
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
            ;;
        "default-all-errors")
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
        "default-alldrv")
            # Do not build the docs and make possible a distcheck below
            CONFIG_OPTS+=("--with-doc=skip")
            if [ "${CANBUILD_DRIVERS_ALL-}" = no ]; then
                echo "WARNING: Build agent says it can't build 'all' driver types; will ask for what we can build" >&2
                CONFIG_OPTS+=("--with-all=auto")
            else
                CONFIG_OPTS+=("--with-all=yes")
            fi
            ;;
        "default"|*)
            # Do not build the docs and tell distcheck it is okay
            CONFIG_OPTS+=("--with-doc=skip")
            ;;
    esac

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

    if [ -n "${BUILD_WARNOPT-}" ]; then
        CONFIG_OPTS+=("--enable-warnings=${BUILD_WARNOPT}")
    fi

    if [ -n "${BUILD_WARNFATAL-}" ]; then
        CONFIG_OPTS+=("--enable-Werror=${BUILD_WARNFATAL}")
    fi

    # Note: modern auto(re)conf requires pkg-config to generate the configure
    # script, so to stage the situation of building without one (as if on an
    # older system) we have to remove it when we already have the script.
    # This matches the use-case of distro-building from release tarballs that
    # include all needed pre-generated files to rely less on OS facilities.
    if [ "$CI_OS_NAME" = "windows" ] ; then
        $CI_TIME ./autogen.sh || true
    else
        $CI_TIME ./autogen.sh ### 2>/dev/null
    fi
    if [ "$NO_PKG_CONFIG" == "true" ] && [ "$CI_OS_NAME" = "linux" ] ; then
        echo "NO_PKG_CONFIG==true : BUTCHER pkg-config for this test case" >&2
        sudo dpkg -r --force all pkg-config
    fi

    if [ "$BUILD_TYPE" != "default-all-errors" ] ; then
        configure_nut
    fi

    case "$BUILD_TYPE" in
        "default-tgt:"*) # Hook for matrix of custom distchecks primarily
            BUILD_TGT="`echo "$BUILD_TYPE" | sed 's,^default-tgt:,,'`"
            echo "`date`: Starting the sequential build attempt for singular target $BUILD_TGT..."

            # Note: Makefile.am already sets some default DISTCHECK_CONFIGURE_FLAGS
            # that include DISTCHECK_FLAGS if provided
            DISTCHECK_FLAGS="`for F in "${CONFIG_OPTS[@]}" ; do echo "'$F' " ; done | tr '\n' ' '`"
            export DISTCHECK_FLAGS
            $CI_TIME $MAKE VERBOSE=1 DISTCHECK_FLAGS="$DISTCHECK_FLAGS" "$BUILD_TGT"

            echo "=== Are GitIgnores good after '$MAKE $BUILD_TGT'? (should have no output below)"
            git status -s || true
            echo "==="
            if git status -s | egrep '\.dmf$' ; then
                echo "FATAL: There are changes in DMF files listed above - tracked sources should be updated!" >&2
                exit 1
            fi
            if [ "$HAVE_CCACHE" = yes ]; then
                echo "CCache stats after build:"
                ccache -s
            fi
            echo "=== Exiting after the custom-build target '$MAKE $BUILD_TGT' succeeded OK"
            exit 0
            ;;
        "default-spellcheck")
            [ -z "$CI_TIME" ] || echo "`date`: Trying to spellcheck documentation of the currently tested project..."
            # Note: use the root Makefile's spellcheck recipe which goes into
            # sub-Makefiles known to check corresponding directory's doc files.
            ( $CI_TIME $MAKE VERBOSE=1 SPELLCHECK_ERROR_FATAL=yes spellcheck )
            exit 0
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
            ( $CI_TIME $MAKE VERBOSE=1 shellcheck check-scripts-syntax )
            exit $?
            ;;
        "default-all-errors")
            RES=0
            if pkg-config --exists nss && pkg-config --exists openssl && [ "${BUILD_SSL_ONCE-}" != "true" ] ; then
                # Try builds for both cases as they are ifdef-ed

                echo "=== Building with SSL=openssl..."
                ( CONFIG_OPTS+=("--with-openssl")
                  configure_nut
                  build_to_only_catch_errors ) || RES=$?

                echo "=== Clean the sandbox..."
                $MAKE distclean -k || true

                echo "=== Building with SSL=nss..."
                ( CONFIG_OPTS+=("--with-nss")
                  configure_nut
                  build_to_only_catch_errors ) || RES=$?
            else
                # Build what we can configure
                configure_nut
                build_to_only_catch_errors || RES=$?
            fi
            exit $RES
            ;;
    esac

    ( echo "`date`: Starting the parallel build attempt..."; \
      $CI_TIME $MAKE VERBOSE=1 -k -j8 all; ) || \
    ( echo "`date`: Starting the sequential build attempt..."; \
      $CI_TIME $MAKE VERBOSE=1 all )

    echo "=== Are GitIgnores good after '$MAKE all'? (should have no output below)"
    git status -s || true
    echo "==="
    if [ -n "`git status -s`" ]; then
        echo "FATAL: There are changes in some files listed above - tracked sources should be updated in the PR, and build products should be added to a .gitignore file!" >&2
        git diff || true
        echo "==="
        exit 1
    fi

    [ -z "$CI_TIME" ] || echo "`date`: Trying to install the currently tested project into the custom DESTDIR..."
    $CI_TIME $MAKE VERBOSE=1 DESTDIR="$INST_PREFIX" install
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
        $CI_TIME $MAKE VERBOSE=1 DISTCHECK_FLAGS="$DISTCHECK_FLAGS" distcheck

        echo "=== Are GitIgnores good after '$MAKE distcheck'? (should have no output below)"
        git status -s || true
        echo "==="
        )
    fi

    if [ "$HAVE_CCACHE" = yes ]; then
        echo "CCache stats after build:"
        ccache -s
    fi
    ;;
bindings)
    pushd "./bindings/${BINDING}" && ./ci_build.sh
    ;;
"")
    echo "ERROR: No BUILD_TYPE was specified, doing a minimal default ritual"
    ./autogen.sh
    ./configure
    $MAKE all && $MAKE check
    ;;
*)
    pushd "./builds/${BUILD_TYPE}" && REPO_DIR="$(dirs -l +1)" ./ci_build.sh
    ;;
esac
