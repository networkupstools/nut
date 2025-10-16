dnl Simplified declaration of some feature options
dnl Copyright (C)
dnl     2006       Peter Selinger <selinger@users.sourceforge.net>
dnl     2020-2025  Jim Klimov <jimklimov+nut@gmail.com>
dnl Common arguments:
dnl     $1 option name (part after `--with-`)
dnl     $2 help text (descriptive part)
dnl     $3 default value
dnl     $4 how to represent default in help text (for *_CUSTOM_DEFAULT_HELP)
dnl Note that for some reason use of dollar-number gets expanded below,
dnl but any other variable (e.g. $conftemp straight in help string) is not.
dnl In fact, the generated configure script includes the help wall of text
dnl much earlier than it includes lines that manipulate conftemp (unless it
dnl gets somehow escaped to happen earlier), due to m4 diverts in autoconf code.

dnl Working With External Software (might name a variant or other contextual arg)
dnl https://www.gnu.org/software/autoconf/manual/autoconf-2.66/html_node/External-Software.html#External-Software
dnl including concepts of the OS as external software (account names, paths...)
AC_DEFUN([NUT_ARG_WITH_CUSTOM_DEFAULT_HELP],
[   AC_ARG_WITH($1,
        AS_HELP_STRING([--with-$1], [$2 ($4)]),
        [[nut_with_]m4_translit($1, [-], [_])="${withval}"],
        [[nut_with_]m4_translit($1, [-], [_])="$3"]
    )
])

AC_DEFUN([NUT_ARG_WITH_EXPAND_DEFAULT_HELP],
[
    dnl Note: only 3 args expected
    dnl Default expanded once to substitute current variables,
    dnl or remove backslashing of verbatim dollars, etc.
    conftemp="$3"
    eval conftemp=\"${conftemp}\"
    NUT_ARG_WITH_CUSTOM_DEFAULT_HELP([$1], [$2], [$3], [${conftemp}])
    unset conftemp
])

AC_DEFUN([NUT_ARG_WITH_EXPAND_DEFAULT_HELP_SINGLEQUOTE],
[
    dnl Variant for paths (likely using backslash-dollar in $3)
    dnl Note: only 3 args expected
    dnl Default expanded once to substitute current variables,
    dnl or remove backslashing of verbatim dollars, etc.
    conftemp="$3"
    eval conftemp=\"${conftemp}\"
    NUT_ARG_WITH_CUSTOM_DEFAULT_HELP([$1], [$2], [$3], ['${conftemp}'])
    unset conftemp
])

AC_DEFUN([NUT_ARG_WITH],
[
    dnl Note: only 3 args expected
    dnl Legacy behavior (for static values): default value
    dnl and its help representation are the same (verbatim!)
    NUT_ARG_WITH_CUSTOM_DEFAULT_HELP([$1], [$2], [$3], [$3])
])

dnl Enable a package feature/ability (might name a variant, or yes/no)
dnl https://www.gnu.org/software/autoconf/manual/autoconf-2.66/html_node/Package-Options.html
AC_DEFUN([NUT_ARG_ENABLE_CUSTOM_DEFAULT_HELP],
[   AC_ARG_ENABLE($1,
        AS_HELP_STRING([--enable-$1], [$2 ($4)]),
        [[nut_enable_]m4_translit($1, [-], [_])="${enableval}"],
        [[nut_enable_]m4_translit($1, [-], [_])="$3"]
    )
])

AC_DEFUN([NUT_ARG_ENABLE_EXPAND_DEFAULT_HELP],
[
    conftemp="$3"
    eval conftemp=\"${conftemp}\"
    NUT_ARG_ENABLE_CUSTOM_DEFAULT_HELP([$1], [$2], [$3], [${conftemp}])
    unset conftemp
])

AC_DEFUN([NUT_ARG_ENABLE_EXPAND_DEFAULT_HELP_SINGLEQUOTE],
[
    conftemp="$3"
    eval conftemp=\"${conftemp}\"
    NUT_ARG_ENABLE_CUSTOM_DEFAULT_HELP([$1], [$2], [$3], ['${conftemp}'])
    unset conftemp
])

AC_DEFUN([NUT_ARG_ENABLE],
[
    NUT_ARG_ENABLE_CUSTOM_DEFAULT_HELP([$1], [$2], [$3], [$3])
])
