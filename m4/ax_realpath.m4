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

        while test -h "$PRG" ; do
            LS_OUT="`ls -ld "$PRG"`"
            LINK="`expr "$LS_OUT" : '.*-> \(.*\)$'`"
            if expr "$LINK" : '/.*' > /dev/null; then
                PRG="$LINK"
            else
                PRG="`dirname "$PRG"`/$LINK"
            fi
        done

        PRGDIR="`dirname "$PRG"`"
        PRGDIR="`cd "$PRGDIR" && pwd`"

        while test -h "$PRGDIR" ; do
            LS_OUT="`ls -ld "$PRGDIR"`"
            LINK="`expr "$LS_OUT" : '.*-> \(.*\)$'`"
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

        REALPRG="$PRGDIR/`basename "$PRG"`"
        unset PRG PRGDIR PARENTDIR LS_OUT LINK
    ])

    AC_MSG_RESULT([$REALPRG])
    $2="$REALPRG"

    unset REALPRG
])
