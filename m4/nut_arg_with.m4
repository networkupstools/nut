dnl Simplified declaration of some feature options
dnl Copyright (C)
dnl     2006       Peter Selinger <selinger@users.sourceforge.net>
dnl     2020-2025  Jim Klimov <jimklimov+nut@gmail.com>
dnl Common arguments:
dnl     $1 option name (part after `--with-`)
dnl     $2 help bit in option name (except legacy short funcs)
dnl     $2 or $3 help text (descriptive part)
dnl     $3 or $4 default value
dnl     $4 or $5 how to represent default in help text (for *_CUSTOM_DEFAULT_HELP)
dnl Note that for some reason use of dollar-number gets expanded below,
dnl but any other variable (e.g. $conftemp straight in help string) is not.
dnl In fact, the generated configure script includes the help wall of text
dnl much earlier than it includes lines that manipulate conftemp (unless it
dnl gets somehow escaped to happen earlier), due to m4 diverts in autoconf code.
dnl So we can use the "EXPAND" methods to convert static text, but not to display
dnl any values determined and changed during `configure` shell script run-time.
dnl See research in https://github.com/networkupstools/nut/issues/3049 for more.

dnl Default expanded once to substitute current variables,
dnl or remove backslashing of verbatim dollars, etc.
dnl NOTE: Not sure if the fuss with uniquely named conftemp is worth it,
dnl or is even remembered between calls. There seems to be 3 calls per name.
AC_DEFUN([NUT_ARG_EXPAND],
[[]m4_esyscmd_s(
    [nut_conftemp_]m4_translit($1, [-], [_])="$2" ;
    eval [nut_conftemp_]m4_translit($1, [-], [_])=\"\${[nut_conftemp_]m4_translit($1, [-], [_])}\" ;
    echo "${[nut_conftemp_]m4_translit($1, [-], [_])}" ;
    dnl # RUNS 3 TIMES # echo "$1 | $2 | [nut_conftemp_]m4_translit($1, [-], [_])=${[nut_conftemp_]m4_translit($1, [-], [_])}" >> arg.log ;
)[]])

dnl Working With External Software (might name a variant or other contextual arg)
dnl https://www.gnu.org/software/autoconf/manual/autoconf-2.66/html_node/External-Software.html#External-Software
dnl including concepts of the OS as external software (account names, paths...)

dnl     $1 option name (part after `--with-`)
dnl     $2 optional "=VALUE" for help tag (after $1)
dnl     $3 help text (descriptive part)
dnl     $4 default value
dnl     $5 how to represent default in help text (for *_CUSTOM_DEFAULT_HELP)
AC_DEFUN([NUT_ARG_WITH_CUSTOM_DEFAULT_HELP],
[   AC_ARG_WITH($1,
        m4_ifval([$2],
            [m4_ifval([$5],
                [AS_HELP_STRING([--with-$1=$2], [$3 (default: $5)])],
                [AS_HELP_STRING([--with-$1=$2], [$3 (default: $4)])])],
            [m4_ifval([$5],
                [AS_HELP_STRING([--with-$1], [$3 (default: $5)])],
                [AS_HELP_STRING([--with-$1], [$3 (default: $4)])])]),
        [[nut_with_]m4_translit($1, [-], [_])="${withval}"],
        [[nut_with_]m4_translit($1, [-], [_])="$4"]
    )
])

AC_DEFUN([NUT_ARG_WITH_EXPAND_DEFAULT_HELP],
[
    dnl Note: only 4 args expected
    dnl NUT_ARG_EXPAND($1, $3)
    NUT_ARG_WITH_CUSTOM_DEFAULT_HELP([$1], [$2], [$3], [$4], NUT_ARG_EXPAND(with_$1, $4))
])

AC_DEFUN([NUT_ARG_WITH_EXPAND_DEFAULT_HELP_SINGLEQUOTE],
[
    dnl Variant for paths (likely using backslash-dollar in $4)
    dnl Note: only 4 args expected
    dnl Note: "resolved from" must be not bracketed
    NUT_ARG_WITH_CUSTOM_DEFAULT_HELP([$1], [$2], [$3], [$4], resolved from NUT_ARG_EXPAND(with_$1, '$4'))
])

AC_DEFUN([NUT_ARG_WITH],
[
    dnl Note: historicaly only 3 args were expected
    dnl Now a second arg may be injected for optional "=VALUE" for help tag (after $1)
    dnl Legacy behavior (for static values): default value
    dnl and its help representation are the same (verbatim!)
    m4_ifval([$4],
        [NUT_ARG_WITH_CUSTOM_DEFAULT_HELP([$1], [$2], [$3], [$4], [$4])],
        [NUT_ARG_WITH_CUSTOM_DEFAULT_HELP([$1], [], [$2], [$3], [$3])])
])

dnl Special common case for *-includes and *-libs optional parameters
dnl (bracketed in help text to confer optionality)
AC_DEFUN([NUT_ARG_WITH_LIBOPTS],
[
    AC_ARG_WITH($1,
        [AS_HELP_STRING([@<:@--with-$1=$2@:>@], [$3 (default: $4)])],
        [AS_CASE([${withval}],
            [yes|no], [AC_MSG_ERROR([invalid option --with(out)-$1 - see docs/configure.txt])],
                [[nut_with_]m4_translit($1, [-], [_])="${withval}"]
        )],
        [[nut_with_]m4_translit($1, [-], [_])="$4"]
    )
])

dnl Just the (normal-cased) third-party project name is required
AC_DEFUN([NUT_ARG_WITH_LIBOPTS_INCLUDES],
[
    m4_ifval([$2],
        [NUT_ARG_WITH_LIBOPTS([m4_translit($1, 'A-Z', 'a-z')-includes], [CFLAGS], [include flags for the $1 library], [$2])],
        [NUT_ARG_WITH_LIBOPTS([m4_translit($1, 'A-Z', 'a-z')-includes], [CFLAGS], [include flags for the $1 library], [auto])])
])

AC_DEFUN([NUT_ARG_WITH_LIBOPTS_LIBS],
[
    m4_ifval([$2],
        [NUT_ARG_WITH_LIBOPTS([m4_translit($1, 'A-Z', 'a-z')-libs], [LIBS], [linker flags for the $1 library], [$2])],
        [NUT_ARG_WITH_LIBOPTS([m4_translit($1, 'A-Z', 'a-z')-libs], [LIBS], [linker flags for the $1 library], [auto])])
])

dnl Enable a package feature/ability (might name a variant, or yes/no)
dnl https://www.gnu.org/software/autoconf/manual/autoconf-2.66/html_node/Package-Options.html
AC_DEFUN([NUT_ARG_ENABLE_CUSTOM_DEFAULT_HELP],
[   AC_ARG_ENABLE($1,
        m4_ifval([$2],
            [m4_ifval([$5],
                [AS_HELP_STRING([--enable-$1=$2], [$3 (default: $5)])],
                [AS_HELP_STRING([--enable-$1=$2], [$3 (default: $4)])])],
            [m4_ifval([$5],
                [AS_HELP_STRING([--enable-$1], [$3 (default: $5)])],
                [AS_HELP_STRING([--enable-$1], [$3 (default: $4)])])]),
        [[nut_enable_]m4_translit($1, [-], [_])="${enableval}"],
        [[nut_enable_]m4_translit($1, [-], [_])="$4"]
    )
])

AC_DEFUN([NUT_ARG_ENABLE_EXPAND_DEFAULT_HELP],
[
    NUT_ARG_ENABLE_CUSTOM_DEFAULT_HELP([$1], [$2], [$3], [$4], NUT_ARG_EXPAND(enable_$1, $4))
])

AC_DEFUN([NUT_ARG_ENABLE_EXPAND_DEFAULT_HELP_SINGLEQUOTE],
[
    NUT_ARG_ENABLE_CUSTOM_DEFAULT_HELP([$1], [$2], [$3], [$4], resolved from NUT_ARG_EXPAND(enable_$1, '$4'))
])

AC_DEFUN([NUT_ARG_ENABLE],
[
    m4_ifval([$4],
        [NUT_ARG_ENABLE_CUSTOM_DEFAULT_HELP([$1], [$2], [$3], [$4], [$4])],
        [NUT_ARG_ENABLE_CUSTOM_DEFAULT_HELP([$1], [], [$2], [$3], [$3])])
])
