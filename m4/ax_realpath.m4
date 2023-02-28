dnl Resolve given path (if we can), for filenames that may be symlinks,
dnl or in a relative directory, or where directories are symlinks...
dnl Copyright (C) 2023 by Jim Klimov <jimklimov+nut@gmail.com>
dnl Licensed under the terms of GPLv2 or newer.

dnl Calling script is welcome to pre-detect external REALPATH implementation,
dnl otherwise shell implementation would be used (hopefully capable enough):
dnl AC_CHECK_PROGS([REALPATH], [realpath], [])

AC_DEFUN([AX_REALPATH_SHELL_ONELEVEL],
[
    dnl # resolve links - #1 value (not quoted by caller) or some its
    dnl # ancestor directory may be a symlink; save into varname #2.
    dnl # In case of problems return #1 and set non-zero RESOLVE_ERROR.
    dnl # We recurse backwards from original name to root "/"
    dnl # (or abort mid-way).

    AS_IF([test x"$1" = x], [AC_MSG_ERROR([Bad call to REALPATH_SHELL_ONELEVEL macro (arg1)])])
    AS_IF([test x"$2" = x], [AC_MSG_ERROR([Bad call to REALPATH_SHELL_ONELEVEL macro (arg2)])])
    AS_IF([test x"$RESOLVE_ERROR" = x], [RESOLVE_ERROR=0])

    AS_IF([test x"$RESOLVE_ERROR" != x0 || test x"$1" = x/], [dnl Quick bail out
        $2="$1"
    ], [
        dnl # Code below was adapted from Apache Tomcat startup.sh
        TGT="$1"

        while test -h "$TGT" ; do
            LS_OUT="`ls -ld "$TGT"`" || { RESOLVE_ERROR=$? ; break ; }
            LINK="`expr "$LS_OUT" : '.*-> \(.*\)$'`" || { RESOLVE_ERROR=$? ; break ; }
            if expr "$LINK" : '/.*' > /dev/null; then
                TGT="$LINK"
            else
                TGT="`dirname "$TGT"`/$LINK"
            fi
        done

        if test "$RESOLVE_ERROR" = 0 ; then
            TGTDIR="`dirname "$TGT"`" && \
            TGTDIR="`cd "$TGTDIR" && pwd`" || {
                TGTDIR="`dirname "$TGT"`" || \
                RESOLVE_ERROR=$? ; }

            if test "$RESOLVE_ERROR" = 0 ; then
                while test -h "$TGTDIR" ; do
                    LS_OUT="`ls -ld "$TGTDIR"`" || { RESOLVE_ERROR=$? ; break ; }
                    LINK="`expr "$LS_OUT" : '.*-> \(.*\)$'`" || { RESOLVE_ERROR=$? ; break ; }
                    if expr "$LINK" : '/.*' > /dev/null; then
                        TGTDIR="$LINK"
                    else
                        PARENTDIR="`dirname "$TGTDIR"`"
                        case "$PARENTDIR" in
                            /) TGTDIR="/$LINK" ; break ;;
                            *) TGTDIR="$PARENTDIR/$LINK" ;;
                        esac
                    fi
                done
            fi
        fi

        if test "$RESOLVE_ERROR" = 0 ; then
            $2="$TGTDIR/`basename "$TGT"`"
        else
            $2="$1"
        fi

        unset TGT TGTDIR PARENTDIR
        unset LS_OUT LINK
    ])
])

AC_DEFUN([AX_REALPATH_SHELL_RECURSIVE],
[
    dnl Autoconf/m4 is not really friendly for recursive functions
    dnl so we have to do it like a loop. Sanity-checking is in the
    dnl helper method above and will abort the script upon problems.
    dnl The RESOLVE_ERROR is provided and unset by caller.

    RESOLVE_PREFIX="$1"
    RESOLVE_SUFFIX=""

    while \
           test x"$RESOLVE_PREFIX" != x \
        && test x"$RESOLVE_PREFIX" != x/ \
        && test x"$RESOLVE_ERROR" = x0 \
    ; do
        dnl In case of non-fatal resolve error, value in RESOLVE_PREFIX
        dnl should remain unchanged, and a RESOLVE_ERROR flag raised.
        dnl Note that the recursion would technically re-check the last
        dnl seen object (the parent directory), but should quickly move
        dnl on since it is not a symlink anymore. So not too ineffecient.
        AX_REALPATH_SHELL_ONELEVEL([$RESOLVE_PREFIX], [RESOLVE_PREFIX])
        if test x"$RESOLVE_ERROR" = x0 ; then
            dnl Recurse to check the (grand)parent dir (if any)
            if test -n "$RESOLVE_SUFFIX" ; then
                RESOLVE_SUFFIX="`basename "$RESOLVE_PREFIX"`/$RESOLVE_SUFFIX"
            else
                RESOLVE_SUFFIX="`basename "$RESOLVE_PREFIX"`"
            fi
            RESOLVE_PREFIX="`dirname "$RESOLVE_PREFIX"`"
        else
            dnl Bail out, keep latest answer
            break
        fi
    done

    if test -n "$RESOLVE_SUFFIX" ; then
        if test x"$RESOLVE_PREFIX" = x ; then
            RESOLVE_PREFIX="/"
        fi
        if test x"$RESOLVE_PREFIX" = x/ ; then
            $2="/$RESOLVE_SUFFIX"
        else
            $2="$RESOLVE_PREFIX/$RESOLVE_SUFFIX"
        fi
    else
        $2="$RESOLVE_PREFIX"
    fi

    unset RESOLVE_PREFIX RESOLVE_SUFFIX
])

AC_DEFUN([AX_REALPATH],
[
    dnl # resolve links - #1 value (not quoted by caller)
    dnl # or its directory may be a softlink
    dnl # save into varname #2
    AS_IF([test x"$1" = x], [AC_MSG_ERROR([Bad call to REALPATH macro (arg1)])])
    AS_IF([test x"$2" = x], [AC_MSG_ERROR([Bad call to REALPATH macro (arg2)])])

    AC_MSG_CHECKING([for "real path" of '$1'])

    REALPRG=""
    AS_IF([test -n "$REALPATH"], [
        REALPRG="`${REALPATH} "$1"`"
    ])

    AS_IF([test -z "$REALPRG"], [
        RESOLVE_ERROR=0

        dnl Note: not all "test" implementations have "-e", so got fallbacks:
        AS_IF([test -e "$1" || test -f "$1" || test -s "$1" || test -d "$1" || test -L "$1" || test -h "$1" || test -c "$1" || test -b "$1" || test -p "$1"],
            [], [
            AC_MSG_WARN([Path name '$1' not found (absent or access to ancestor directories denied)])
            dnl We can still try to resolve, e.g. to find
            dnl the real location an absent file would be in
            dnl RESOLVE_ERROR=1
        ])
        AX_REALPATH_SHELL_RECURSIVE([$1], [REALPRG])
    ])

    AS_IF([test -n "$REALPRG"], [
        AS_IF([test x"$REALPRG" = x"$1"],
            [AC_MSG_RESULT(['$REALPRG'])],
            [AC_MSG_RESULT(['$REALPRG' (differs from input)])]
        )
        $2="$REALPRG"
    ], [
        dnl Indent due to newline from warning and/or tool errors above
        AC_MSG_RESULT([...failed to resolve, keeping original: '$1'])
        $2="$1"
    ])

    unset REALPRG RESOLVE_ERROR
])

AC_DEFUN([UNITTEST_AX_REALPATH_EXPECT],
[
    AX_REALPATH([$1], [TMPNAME])
    AS_IF([test x"$TMPNAME" != x"$2"], [AC_MSG_WARN([>>> Got: '$TMPNAME' (should be '$2')])])
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
    AC_MSG_NOTICE([>>> Got: '$TMPNAME'])
    AC_MSG_NOTICE([=======])
    AC_MSG_NOTICE([=== Should be an stdin socket here (if procfs is supported on the platform)])
    AX_REALPATH([/proc/$$/fd/1], [TMPNAME])
    AC_MSG_NOTICE([>>> Got: '$TMPNAME'])
    AC_MSG_NOTICE([=======])
    AC_MSG_NOTICE([=== Should not have access to next SYMLINK (reading link content is forbidden, if procfs is supported on the platform and if not root)])
    UNITTEST_AX_REALPATH_EXPECT([/proc/1/exe], [/proc/1/exe])
    AC_MSG_NOTICE([=======])
    AC_MSG_NOTICE([=== Should not have access to next dir (don't know if file exists, if procfs is supported on the platform and if not root)])
    UNITTEST_AX_REALPATH_EXPECT([/proc/1/fd/1], [/proc/1/fd/1])
    AC_MSG_NOTICE([======= end of UNITTEST for REALPATH macro])
    AS_IF([test x"$TESTDIR" = x"/tmp"], [rm -rf "$TESTDIR/q" "$TESTDIR/Q"], [rm -rf "$TESTDIR"])
])
