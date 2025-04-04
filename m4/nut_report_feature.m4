dnl automated feature report at the end of configure script.
dnl it also AC_DEFINE() and AM_CONDITIONAL the matching variable.
dnl for example, "usb" (--with-usb) will give
dnl nut_with_usb and WITH_USB (both macros, and
dnl AM_CONDITIONAL)

AC_DEFUN([NUT_REPORT_FILE],
[
    dnl arg#1 = description (summary)
    dnl arg#2 = value
    dnl arg#3 = file tag (e.g. number)
    dnl arg#4 = file title (e.g. "NUT Configuration summary:")
    AS_IF([test x"${nut_report_feature_flag$3}" = x], [
        nut_report_feature_flag$3="1"
        ac_clean_files="${ac_clean_files} config.nut_report_feature.log.$3"
        case x"$3" in
        x1a)
            echo "$4"
            echo "$4" | sed 's/./=/g'
            echo ""
            ;;
        x1*) ;;
        *)
            echo ""
            echo "$4"
            echo "$4" | sed 's/./-/g'
            echo ""
            ;;
        esac > "config.nut_report_feature.log.$3"
    ])
    printf "* %s:\t%s\n" "$1" "$2" >> "config.nut_report_feature.log.$3"
])

AC_DEFUN([NUT_REPORT],
[
    dnl arg#1 = description (summary)
    dnl arg#2 = value
    NUT_REPORT_FILE([$1], [$2], [1a], "NUT Configuration summary:")
])

AC_DEFUN([NUT_REPORT_DRV],
[
    dnl arg#1 = description (summary)
    dnl arg#2 = value
    dnl Title irrelevant here, should not show
    NUT_REPORT_FILE([$1], [$2], [1b], "NUT Configuration summary:")
])

AC_DEFUN([NUT_REPORT_PRG],
[
    dnl arg#1 = description (summary)
    dnl arg#2 = value
    dnl Title irrelevant here, should not show
    NUT_REPORT_FILE([$1], [$2], [1c], "NUT Configuration summary:")
])

AC_DEFUN([NUT_REPORT_PATH],
[
    dnl arg#1 = description (summary)
    dnl arg#2 = value
    NUT_REPORT_FILE([$1], [$2], [2], "NUT Paths:")
])

AC_DEFUN([NUT_REPORT_PATH_INTEGRATIONS],
[
    dnl arg#1 = description (summary)
    dnl arg#2 = value
    NUT_REPORT_FILE([$1], [$2], [3], "NUT Paths for third-party integrations:")
])

AC_DEFUN([NUT_REPORT_DRIVER],
[
    dnl NOTE: Same as feature, just grouped into "1b" file to display after features
    dnl arg#1 = summary/config.log description
    dnl arg#2 = test flag ("yes" or not)
    dnl arg#3 = value
    dnl arg#4 = autoconf varname
    dnl arg#5 = longer description (autoconf comment)
    AC_MSG_CHECKING([whether to $1])
    AC_MSG_RESULT([$2 $3])
    NUT_REPORT_DRV([$1], [$2 $3])

    AM_CONDITIONAL([$4], test "$2" = "yes")
    AS_IF([test x"$2" = x"yes"], [
        AC_DEFINE_UNQUOTED($4, 1, $5)
    ])
])

AC_DEFUN([NUT_REPORT_PROGRAM],
[
    dnl NOTE: Same as feature, just grouped into "1c" file to display last
    dnl arg#1 = summary/config.log description
    dnl arg#2 = test flag ("yes" or not)
    dnl arg#3 = value
    dnl arg#4 = autoconf varname
    dnl arg#5 = longer description (autoconf comment)
    AC_MSG_CHECKING([whether to $1])
    AC_MSG_RESULT([$2 $3])
    NUT_REPORT_PRG([$1], [$2 $3])

    AM_CONDITIONAL([$4], test "$2" = "yes")
    AS_IF([test x"$2" = x"yes"], [
        AC_DEFINE_UNQUOTED($4, 1, $5)
    ])
])

AC_DEFUN([NUT_REPORT_FEATURE],
[
    dnl arg#1 = summary/config.log description
    dnl arg#2 = test flag ("yes" or not)
    dnl arg#3 = value
    dnl arg#4 = autoconf varname
    dnl arg#5 = longer description (autoconf comment)
    AC_MSG_CHECKING([whether to $1])
    AC_MSG_RESULT([$2 $3])
    NUT_REPORT([$1], [$2 $3])

    AM_CONDITIONAL([$4], test "$2" = "yes")
    AS_IF([test x"$2" = x"yes"], [
        AC_DEFINE_UNQUOTED($4, 1, $5)
    ])
])

AC_DEFUN([NUT_REPORT_SETTING],
[
    dnl arg#1 = summary/config.log description
    dnl arg#2 = autoconf varname
    dnl arg#3 = value
    dnl arg#4 = longer description (autoconf comment)
    AC_MSG_CHECKING([setting for $1])
    AC_MSG_RESULT([$3])
    NUT_REPORT([$1], [$3])

    dnl Note: unlike features, settings do not imply an AutoMake toggle
    AC_DEFINE_UNQUOTED($2, $3, $4)
])

AC_DEFUN([NUT_REPORT_SETTING_PATH],
[
    dnl arg#1 = summary/config.log description
    dnl arg#2 = autoconf varname
    dnl arg#3 = value
    dnl arg#4 = longer description (autoconf comment)
    AC_MSG_CHECKING([setting for $1])
    AC_MSG_RESULT([$3])
    NUT_REPORT_PATH([$1], [$3])

    dnl Note: unlike features, settings do not imply an AutoMake toggle
    AC_DEFINE_UNQUOTED($2, $3, $4)
])

AC_DEFUN([NUT_REPORT_SETTING_PATH_INTEGRATIONS],
[
    dnl arg#1 = summary/config.log description
    dnl arg#2 = autoconf varname
    dnl arg#3 = value
    dnl arg#4 = longer description (autoconf comment)
    AC_MSG_CHECKING([setting for $1])
    AC_MSG_RESULT([$3])
    NUT_REPORT_PATH_INTEGRATIONS([$1], [$3])

    dnl Note: unlike features, settings do not imply an AutoMake toggle
    AC_DEFINE_UNQUOTED($2, $3, $4)
])

AC_DEFUN([NUT_REPORT_TARGET],
[
    dnl arg#1 = autoconf varname
    dnl arg#2 = value
    dnl arg#3 = summary/config.log/autoconf description
    AC_MSG_CHECKING([$3])
    AC_MSG_RESULT([$2])
    dnl FIXME: value here is already quoted by caller (for AC_DEFINE_UNQUOTED
    dnl with multi-token strings). Then quotes are added in NUT_REPORT_FILE()
    dnl and turn it into multiple single-token strings. So we neuter that here:
    NUT_REPORT_FILE([$3], ["$2"], [8], "NUT Build/Target system info:")

    dnl Note: unlike features, target info does not imply an AutoMake toggle
    AC_DEFINE_UNQUOTED($1, $2, $3)
])

AC_DEFUN([NUT_REPORT_COMPILERS],
[
    (echo ""
     echo "NUT Compiler settings:"
     echo "----------------------"
     echo ""
     if test x"${nut_with_debuginfo_C}" = x"yes" -o x"${nut_with_debuginfo_CXX}" = x"yes" ; then
        printf 'NOTE: Settings for '
        if test x"${nut_with_debuginfo_C}" = x"yes" ; then
            printf 'C '
        fi
        if test x"${nut_with_debuginfo_C}${nut_with_debuginfo_CXX}" = x"yesyes" ; then
            printf 'and '
        fi
        if test x"${nut_with_debuginfo_CXX}" = x"yes" ; then
            printf 'C++ '
        fi
        printf 'compiler'
        if test x"${nut_with_debuginfo_C}${nut_with_debuginfo_CXX}" = x"yesyes" ; then
            printf 's'
        fi
        printf ' are adjusted for debugging (and minimal optimizations)\n\n'
     fi
     printf '* CC      \t: %s\n' "$CC"
     printf '* CFLAGS  \t: %s\n' "$CFLAGS"
     printf '* CXX     \t: %s\n' "$CXX"
     printf '* CXXFLAGS\t: %s\n' "$CXXFLAGS"
     printf '* CPP     \t: %s\n' "$CPP"
     printf '* CPPFLAGS\t: %s\n' "$CPPFLAGS"
     printf '* LD      \t: %s\n' "$LD"
     printf '* LDFLAGS \t: %s\n' "$LDFLAGS"
     printf '* CONFIG_FLAGS\t: %s\n' "$CONFIG_FLAGS"
    ) > config.nut_report_feature.log.9
    ac_clean_files="${ac_clean_files} config.nut_report_feature.log.9"
])

AC_DEFUN([NUT_PRINT_FEATURE_REPORT],
[
    dnl By (legacy) default we remove this report file
    dnl For CI we want to publish its artifact
    dnl Manageable by "--enable-keep_nut_report_feature"
    echo ""
    AS_IF([test x"${nut_enable_keep_nut_report_feature-}" = xyes],
        [AC_MSG_NOTICE([Will keep config.nut_report_feature.log])
         cat config.nut_report_feature.log.* > config.nut_report_feature.log
         cat config.nut_report_feature.log
        ],
        [dnl Remove if exists from old builds
         ac_clean_files="${ac_clean_files} config.nut_report_feature.log"
         cat config.nut_report_feature.log.*
        ])
])
