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

case "$BUILD_TYPE" in
default|default-alldrv|default-spellcheck|default-nodoc|default-withdoc|"default-tgt:"*)
    LANG=C
    LC_ALL=C
    export LANG LC_ALL

    if [ -d "./tmp" ]; then
        rm -rf ./tmp
    fi
    if [ -d "./.inst" ]; then
        rm -rf ./.inst
    fi
    mkdir -p tmp .inst
    BUILD_PREFIX=$PWD/tmp
    INST_PREFIX=$PWD/.inst

    PATH="`echo "$PATH" | sed -e 's,^/usr/lib/ccache/?:,,' -e 's,:/usr/lib/ccache/?:,,' -e 's,:/usr/lib/ccache/?$,,' -e 's,^/usr/lib/ccache/?$,,'2`"
    CCACHE_PATH="$PATH"
    CCACHE_DIR="${HOME}/.ccache"
    export CCACHE_PATH CCACHE_DIR PATH
    HAVE_CCACHE=no
    if which ccache && ls -la /usr/lib/ccache ; then
        HAVE_CCACHE=yes
    fi

    if [ "$HAVE_CCACHE" = yes ] && [ -d "$CCACHE_DIR" ]; then
        echo "CCache stats before build:"
        ccache -s || true
    fi
    mkdir -p "${HOME}/.ccache"

    CONFIG_OPTS=()
    COMMON_CFLAGS=""
    EXTRA_CFLAGS=""
    EXTRA_CPPFLAGS=""
    EXTRA_CXXFLAGS=""

    is_gnucc() {
        if [ -n "$1" ] && "$1" --version 2>&1 | grep 'Free Software Foundation' > /dev/null ; then true ; else false ; fi
    }

    COMPILER_FAMILY=""
    if [ -n "$CC" -a -n "$CXX" ]; then
        if is_gnucc "$CC" && is_gnucc "$CXX" ; then
            COMPILER_FAMILY="GCC"
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
        fi
    fi

    if [ -n "$CPP" ] ; then
        [ -x "$CPP" ] && export CPP
    else
        if is_gnucc "cpp" ; then
            CPP=cpp && export CPP
        fi
    fi

    CONFIG_OPTS+=("CFLAGS=-I${BUILD_PREFIX}/include")
    CONFIG_OPTS+=("CPPFLAGS=-I${BUILD_PREFIX}/include")
    CONFIG_OPTS+=("CXXFLAGS=-I${BUILD_PREFIX}/include")
    CONFIG_OPTS+=("LDFLAGS=-L${BUILD_PREFIX}/lib")
    CONFIG_OPTS+=("PKG_CONFIG_PATH=${BUILD_PREFIX}/lib/pkgconfig")
    CONFIG_OPTS+=("--prefix=${BUILD_PREFIX}")
    CONFIG_OPTS+=("--sysconfdir=${BUILD_PREFIX}/etc/nut")
    CONFIG_OPTS+=("--with-udev-dir=${BUILD_PREFIX}/etc/udev")
    CONFIG_OPTS+=("--with-devd-dir=${BUILD_PREFIX}/etc/devd")
    CONFIG_OPTS+=("--with-hotplug-dir=${BUILD_PREFIX}/etc/hotplug")

    DO_DISTCHECK=yes
    case "$BUILD_TYPE" in
        "default-nodoc")
            CONFIG_OPTS+=("--with-doc=no")
            DO_DISTCHECK=no
            ;;
        "default-spellcheck")
            CONFIG_OPTS+=("--with-all=no")
            CONFIG_OPTS+=("--with-libltdl=no")
            CONFIG_OPTS+=("--with-doc=man=skip")
            DO_DISTCHECK=no
            ;;
        "default-withdoc")
            CONFIG_OPTS+=("--with-doc=yes")
            ;;
        "default-alldrv")
            # Do not build the docs and make possible a distcheck below
            CONFIG_OPTS+=("--with-doc=skip")
            CONFIG_OPTS+=("--with-all=yes")
            ;;
        "default"|*)
            # Do not build the docs and tell distcheck it is okay
            CONFIG_OPTS+=("--with-doc=skip")
            ;;
    esac

    if [ "$HAVE_CCACHE" = yes ] && [ "${COMPILER_FAMILY}" = GCC ]; then
        PATH="/usr/lib/ccache:$PATH"
        export PATH
        if [ -n "$CC" ] && [ -x "/usr/lib/ccache/`basename "$CC"`" ]; then
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
            : # CC="ccache $CC"
        fi
        if [ -n "$CXX" ] && [ -x "/usr/lib/ccache/`basename "$CXX"`" ]; then
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
            : # CXX="ccache $CXX"
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

        CONFIG_OPTS+=("CC=${CC}")
        CONFIG_OPTS+=("CXX=${CXX}")
        CONFIG_OPTS+=("CPP=${CPP}")
    fi

    # Build and check this project; note that zprojects always have an autogen.sh
    [ -z "$CI_TIME" ] || echo "`date`: Starting build of currently tested project..."
    CCACHE_BASEDIR=${PWD}
    export CCACHE_BASEDIR

    # Note: modern auto(re)conf requires pkg-config to generate the configure
    # script, so to stage the situation of building without one (as if on an
    # older system) we have to remove it when we already have the script.
    # This matches the use-case of distro-building from release tarballs that
    # include all needed pre-generated files to rely less on OS facilities.
    $CI_TIME ./autogen.sh 2> /dev/null
    if [ "$NO_PKG_CONFIG" == "true" ] ; then
        echo "NO_PKG_CONFIG==true : BUTCHER pkg-config for this test case" >&2
        sudo dpkg -r --force all pkg-config
    fi

    $CI_TIME ./configure "${CONFIG_OPTS[@]}"

    case "$BUILD_TYPE" in
        default-tgt:*) # Hook for matrix of custom distchecks primarily
            BUILD_TGT="`echo "$BUILD_TYPE" | sed 's,^default-tgt:,,'`"
            echo "`date`: Starting the sequential build attempt for singular target $BUILD_TGT..."
            export DISTCHECK_CONFIGURE_FLAGS="${CONFIG_OPTS[@]}"
            $CI_TIME make VERBOSE=1 DISTCHECK_CONFIGURE_FLAGS="$DISTCHECK_CONFIGURE_FLAGS" "$BUILD_TGT"
            echo "=== Are GitIgnores good after 'make $BUILD_TGT'? (should have no output below)"
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
            echo "=== Exiting after the custom-build target 'make $BUILD_TGT' succeeded OK"
            exit 0
            ;;
        "default-spellcheck")
            [ -z "$CI_TIME" ] || echo "`date`: Trying to spellcheck documentation of the currently tested project..."
            # Note: use the root Makefile's spellcheck recipe which goes into
            # sub-Makefiles known to check corresponding directory's doc files.
            ( $CI_TIME make VERBOSE=1 SPELLCHECK_ERROR_FATAL=yes spellcheck )
            exit 0
            ;;
    esac

    ( echo "`date`: Starting the parallel build attempt..."; \
      $CI_TIME make VERBOSE=1 -k -j8 all; ) || \
    ( echo "`date`: Starting the sequential build attempt..."; \
      $CI_TIME make VERBOSE=1 all )

    echo "=== Are GitIgnores good after 'make all'? (should have no output below)"
    git status -s || true
    echo "==="

    [ -z "$CI_TIME" ] || echo "`date`: Trying to install the currently tested project into the custom DESTDIR..."
    $CI_TIME make VERBOSE=1 DESTDIR="$INST_PREFIX" install
    [ -n "$CI_TIME" ] && echo "`date`: listing files installed into the custom DESTDIR..." && \
        find "$INST_PREFIX" -ls || true

    if [ "$DO_DISTCHECK" == "no" ] ; then
        echo "Skipping distcheck (doc generation is disabled, it would fail)"
    else
        [ -z "$CI_TIME" ] || echo "`date`: Starting distcheck of currently tested project..."
        (
        export DISTCHECK_CONFIGURE_FLAGS="${CONFIG_OPTS[@]}"
        $CI_TIME make VERBOSE=1 DISTCHECK_CONFIGURE_FLAGS="$DISTCHECK_CONFIGURE_FLAGS" distcheck

        echo "=== Are GitIgnores good after 'make distcheck'? (should have no output below)"
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
*)
    pushd "./builds/${BUILD_TYPE}" && REPO_DIR="$(dirs -l +1)" ./ci_build.sh
    ;;
esac
