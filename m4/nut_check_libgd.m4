dnl Check for LIBGD compiler flags. On success, set nut_have_libgd="yes"
dnl and set LIBGD_CFLAGS and LIBGD_LDFLAGS. On failure, set
dnl nut_have_libgd="no". This macro can be run multiple times, but will
dnl do the checking only once. 

AC_DEFUN([NUT_CHECK_LIBGD], 
[
if test -z "${nut_have_libgd_seen}"; then
   nut_have_libgd_seen=yes

   CFLAGS_ORIG="${CFLAGS}"
   CPPFLAGS_ORIG="${CPPFLAGS}"
   LDFLAGS_ORIG="${LDFLAGS}"

   AC_MSG_CHECKING(for gd version via gdlib-config)

   dnl Initial defaults. These are only used if gdlib-config is
   dnl unusable and the user fails to pass better values in --with
   dnl arguments

   CFLAGS=""
   LDFLAGS="-L/usr/X11R6/lib -lgd -lpng -lz -ljpeg -lfreetype -lm -lXpm -lX11"

   GD_VERSION=`gdlib-config --version 2>/dev/null`
   if (test "$?" != "0")
   then
	GD_VERSION="unknown"
	AC_MSG_RESULT(not found)
   else
	AC_MSG_RESULT(${GD_VERSION})
   fi

   case "${GD_VERSION}" in
	unknown)
		;;

	2.0.5 | 2.0.6 | 2.0.7)
		AC_MSG_WARN([[gd ${GD_VERSION} detected, unable to use gdlib-config script]])
		AC_MSG_WARN([[If gd detection fails, upgrade gd or use --with-gd-libs]])
		;;

	*)
		LDFLAGS="`gdlib-config --ldflags` `gdlib-config --libs` -lgd"
		CFLAGS="`gdlib-config --includes`"
		;;
   esac

   dnl Now allow overriding gd settings if the user knows best

   AC_MSG_CHECKING(for gd include flags)
   AC_ARG_WITH(gd-includes,
   AC_HELP_STRING([--with-gd-includes=FLAGS], [include flags for the gd library]),
   [	case "${withval}" in
	yes|no)
		;;
	*)
		CFLAGS="${withval}"
		;;
	esac],
   )
   AC_MSG_RESULT([${CFLAGS}])

   AC_MSG_CHECKING(for gd library flags)
   AC_ARG_WITH(gd-libs,
   AC_HELP_STRING([--with-gd-libs=FLAGS], [linker flags for the gd library]),
   [	case "${withval}" in
	yes|no)
		;;
	*)
		LDFLAGS="${withval}"
		;;
	esac],
   )
   AC_MSG_RESULT([${LDFLAGS}])

   dnl check if gd is usable

		CPPFLAGS="${CFLAGS}"

		AC_CHECK_HEADERS(gd.h)
		AC_CHECK_LIB(gd, gdImagePng, 
		[ nut_have_libgd=yes
		  AC_DEFINE(HAVE_LIBGD, 1, 
			[Define if you have Boutell's libgd installed])
		],
                [ nut_have_libgd=no ],
		${LDFLAGS})

   if test "${nut_have_libgd}" = "yes"; then
        LIBGD_CFLAGS="${CFLAGS}"
        LIBGD_LDFLAGS="${LDFLAGS}"
   fi

   dnl put back the original versions
   CFLAGS="${CFLAGS_ORIG}"
   CPPFLAGS="${CPPFLAGS_ORIG}"
   LDFLAGS="${LDFLAGS_ORIG}"
fi
])
