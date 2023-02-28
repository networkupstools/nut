dnl Resolve given path (if we can), for filenames that may be symlinks,
dnl or in a relative directory, or where directories are symlinks...
dnl Copyright (C) 2023 by Jim Klimov <jimklimov+nut@gmail.com>
dnl Licensed under the terms of GPLv2 or newer.

dnl Calling script is welcome to pre-detect external REALPATH implementation:
dnl AC_CHECK_PROGS([REALPATH], [realpath], [])
AC_DEFUN([AX_REALPATH],
[
    dnl # resolve links - #1 value (not quoted by caller)
    dnl # or its directory may be a softlink
    dnl # save into varname #2
    AC_MSG_CHECKING([for "real path" of '$1'])

    REALPRG=""
    AS_IF([test -n "$REALPATH"], [
        REALPRG="`${REALPATH} "$1"`"
    ])

    AS_IF([test -z "$REALPRG"], [
        dnl # Code below was adapted from Apache Tomcat startup.sh
        PRG="$1"
        RESOLVE_ERROR=0

        if test \! -e "$PRG" ; then
            AC_MSG_WARN([Path name '$PRG' not resolved (absent or access to ancestor directotries denied)])
            RESOLVE_ERROR=1
        fi

        while test -h "$PRG" ; do
            LS_OUT="`ls -ld "$PRG"`" || { RESOLVE_ERROR=$? ; break ; }
            LINK="`expr "$LS_OUT" : '.*-> \(.*\)$'`" || { RESOLVE_ERROR=$? ; break ; }
            if expr "$LINK" : '/.*' > /dev/null; then
                PRG="$LINK"
            else
                PRG="`dirname "$PRG"`/$LINK"
            fi
        done

        if test "$RESOLVE_ERROR" = 0 ; then
            PRGDIR="`dirname "$PRG"`" && \
            PRGDIR="`cd "$PRGDIR" && pwd`" || {
                PRGDIR="`dirname "$PRG"`" || \
                RESOLVE_ERROR=$? ; }

            if test "$RESOLVE_ERROR" = 0 ; then
                while test -h "$PRGDIR" ; do
                    LS_OUT="`ls -ld "$PRGDIR"`" || { RESOLVE_ERROR=$? ; break ; }
                    LINK="`expr "$LS_OUT" : '.*-> \(.*\)$'`" || { RESOLVE_ERROR=$? ; break ; }
                    if expr "$LINK" : '/.*' > /dev/null; then
                        PRGDIR="$LINK"
                    else
                        PARENTDIR="`dirname "$PRGDIR"`"
                        case "$PARENTDIR" in
                            /) PRGDIR="/$LINK" ; break ;;
                            *) PRGDIR="$PARENTDIR/$LINK" ;;
                        esac
                    fi
                done
            fi
        fi

        if test "$RESOLVE_ERROR" = 0 ; then
            REALPRG="$PRGDIR/`basename "$PRG"`"
        fi
        unset PRG PRGDIR PARENTDIR LS_OUT LINK
    ])

    AS_IF([test -n "$REALPRG"], [
        AC_MSG_RESULT([$REALPRG])
        $2="$REALPRG"
    ], [
        dnl Indent due to newline from warning and/or tool errors above
        AC_MSG_RESULT([...failed to resolve, keeping original: $1])
        $2="$1"
    ])

    unset REALPRG
])

AC_DEFUN([UNITTEST_AX_REALPATH_EXPECT],
[
    AX_REALPATH([$1], [TMPNAME])
    AS_IF([test x"$TMPNAME" != x"$2"], [AC_MSG_WARN([>>> Got: $TMPNAME (should be $2)])])
])

AC_DEFUN([UNITTEST_AX_REALPATH],
[
    AC_MSG_NOTICE([======= starting UNITTEST for REALPATH macro])
    AC_MSG_NOTICE([=== Testing macro for realpath; .../q/x are directories, qwe is a file inside, and .../Q is a symlink to .../q])
    TESTDIR="`mktemp -d`" && test -d "$TESTDIR" && test -w "$TESTDIR" || TESTDIR="/tmp"
    rm -rf "$TESTDIR"/q ; mkdir -p "$TESTDIR"/q/x ; echo qwe > "$TESTDIR"/q/x/qwe ; ln -fs q "$TESTDIR"/Q
    dnl Do not quote TESTDIR in macro calls below, shell quotes are added in implem
    AC_MSG_NOTICE([=======])
    UNITTEST_AX_REALPATH_EXPECT([$TESTDIR/q/x], [$TESTDIR/q/x])
    UNITTEST_AX_REALPATH_EXPECT([$TESTDIR/Q/x], [$TESTDIR/q/x])
    UNITTEST_AX_REALPATH_EXPECT([$TESTDIR/Q/x/qwe], [$TESTDIR/q/x/qwe])
    AC_MSG_NOTICE([=======])
    AC_MSG_NOTICE([=== Should not have access to next file (does not exist) in a readable dir])
    UNITTEST_AX_REALPATH_EXPECT([$TESTDIR/q/x/absent], [$TESTDIR/q/x/absent])
    AC_MSG_NOTICE([=======])
    AC_MSG_NOTICE([=== Should not have access to next file (does not exist) in a SYMLINK to readable dir])
    UNITTEST_AX_REALPATH_EXPECT([$TESTDIR/Q/x/absent], [$TESTDIR/q/x/absent])
    AC_MSG_NOTICE([=======])
    AC_MSG_NOTICE([=== Present and visible or not, unless this is (behind) a symlink it should remain the same path])
    UNITTEST_AX_REALPATH_EXPECT([/etc/nut/ups.conf], [/etc/nut/ups.conf])
    AC_MSG_NOTICE([=======])
    AC_MSG_NOTICE([=== Should be a shell interpreter here (if procfs is supported on the platform)])
    AX_REALPATH([/proc/$$/exe], [TMPNAME])
    AC_MSG_NOTICE([>>> Got: $TMPNAME])
    AC_MSG_NOTICE([=======])
    AC_MSG_NOTICE([=== Should be an stdin socket here (if procfs is supported on the platform)])
    AX_REALPATH([/proc/$$/fd/1], [TMPNAME])
    AC_MSG_NOTICE([>>> Got: $TMPNAME])
    AC_MSG_NOTICE([=======])
    AC_MSG_NOTICE([=== Should not have access to next SYMLINK (reading link content is forbidden, if procfs is supported on the platform and if not root)])
    UNITTEST_AX_REALPATH_EXPECT([/proc/1/exe], [/proc/1/exe])
    AC_MSG_NOTICE([=======])
    AC_MSG_NOTICE([=== Should not have access to next dir (don't know if file exists, if procfs is supported on the platform and if not root)])
    UNITTEST_AX_REALPATH_EXPECT([/proc/1/fd/1], [/proc/1/fd/1])
    AC_MSG_NOTICE([======= end of UNITTEST for REALPATH macro])
    AS_IF([test x"$TESTDIR" = x"/tmp"], [rm -rf "$TESTDIR/q" "$TESTDIR/Q"], [rm -rf "$TESTDIR"])
])
