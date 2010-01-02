dnl Check for LIBHAL compiler flags. On success, set nut_have_libhal="yes"
dnl and set LIBHAL_CFLAGS and LIBHAL_LDFLAGS. On failure, set
dnl nut_have_libhal="no". This macro can be run multiple times, but will
dnl do the checking only once. 
dnl NUT requires HAL version 0.5.8 at least

AC_DEFUN([NUT_CHECK_LIBHAL], 
[
if test -z "${nut_have_libhal_seen}"; then
	nut_have_libhal_seen=yes

	CFLAGS_ORIG="${CFLAGS}"
	LDFLAGS_ORIG="${LDFLAGS}"

	AC_MSG_CHECKING(for libhal version via pkg-config (0.5.8 minimum required))
	HAL_VERSION=`pkg-config --silence-errors --modversion hal`
	if test "$?" = "0"; then     
		if pkg-config --atleast-version=0.5.8 hal; then
			AC_MSG_RESULT(${HAL_VERSION} found)
			nut_have_libhal=yes

			dnl also get cflags from glib-2.0 to workaround a bug in dbus-glib
			AC_MSG_CHECKING(for libhal cflags via pkg-config)
			CFLAGS=`pkg-config --silence-errors --cflags hal dbus-glib-1`
			if test "$?" = "0"; then
				AC_MSG_RESULT(${CFLAGS})
			else
				AC_MSG_RESULT(not found)
				nut_have_libhal=no
			fi

			dnl also get libs from glib-2.0 to workaround a bug in dbus-glib
			AC_MSG_CHECKING(for libhal ldflags via pkg-config)
			LDFLAGS=`pkg-config --silence-errors --libs hal dbus-glib-1`
			if test "$?" = "0"; then
				AC_MSG_RESULT(${LDFLAGS})
			else
				AC_MSG_RESULT(not found)
				nut_have_libhal=no
			fi
		else
			AC_MSG_RESULT(${HAL_VERSION} is too old)
			nut_have_libhal=no
		fi
	else
		AC_MSG_RESULT(not found)
		nut_have_libhal=check
	fi

	dnl try again using defaults if pkg-config is not available
	if test "${nut_have_libhal}" = "check"; then
		CFLAGS="-DDBUS_API_SUBJECT_TO_CHANGE -I/usr/include/hal -I/usr/include/dbus-1.0 -I/usr/lib/dbus-1.0/include"
		LDFLAGS="-lhal -ldbus-1 -lpthread"

		AC_CHECK_HEADERS(libhal.h, [nut_have_libhal=yes], [nut_have_libhal=no], [AC_INCLUDES_DEFAULT])
		AC_CHECK_FUNCS(libhal_device_new_changeset, [], [nut_have_libhal=no])
	fi

	if test "${nut_have_libhal}" = "yes"; then
		LIBHAL_CFLAGS="${CFLAGS}"
		LIBHAL_LDFLAGS="${LDFLAGS}"

		dnl this will only work as of HAL 0.5.9
		AC_MSG_CHECKING(for libhal user via pkg-config)
		HAL_USER=`pkg-config --silence-errors --variable=haluser hal`
		if test -n "$HAL_USER"; then
			AC_MSG_RESULT(${HAL_USER})
		else
			HAL_USER="haldaemon"
			AC_MSG_RESULT(using default (${HAL_USER}))
		fi
		AC_DEFINE_UNQUOTED(HAL_USER, "${HAL_USER}", [HAL user])

		dnl the device match key changed with HAL 0.5.11
		AC_MSG_CHECKING(for hal-${HAL_VERSION} device match key)
		HAL_DEVICE_MATCH_KEY=`pkg-config --silence-errors --atleast-version=0.5.11 hal`
		if (test "$?" != "0"); then
			HAL_DEVICE_MATCH_KEY="info.bus"
		else
			HAL_DEVICE_MATCH_KEY="info.subsystem"
		fi
		AC_MSG_RESULT(${HAL_DEVICE_MATCH_KEY})
		AC_DEFINE_UNQUOTED(HAL_DEVICE_MATCH_KEY, "${HAL_DEVICE_MATCH_KEY}", [HAL device match key])

		dnl Determine installation paths for callout and .fdi
		dnl As per HAL spec, §5 Callouts and §2 Device Information Files
		dnl - addon install path: $libdir/hal
		AC_MSG_CHECKING(for libhal Callouts path)
		HAL_CALLOUTS_PATH=`pkg-config --silence-errors --variable=libexecdir hal`
		if (test -n "$HAL_CALLOUTS_PATH"); then
			AC_MSG_RESULT(${HAL_CALLOUTS_PATH})
		else
			# fallback to detecting the right path
			if (test -d "${libdir}/hal"); then
				# For Debian
				HAL_CALLOUTS_PATH="${libdir}/hal"
				AC_MSG_RESULT(${HAL_CALLOUTS_PATH})
			elif (test -d "/usr/libexec"); then
				# For RedHat
				HAL_CALLOUTS_PATH="${libexecdir}"
				AC_MSG_RESULT(${HAL_CALLOUTS_PATH})
			elif (test -d "/usr/lib/hal"); then
				# For OpenSUSE
				HAL_CALLOUTS_PATH="${libdir}/hal"
				AC_MSG_RESULT(${HAL_CALLOUTS_PATH})
			else
				# FIXME
				HAL_CALLOUTS_PATH="${libdir}/hal"
				AC_MSG_RESULT(using default (${HAL_CALLOUTS_PATH}))
			fi
		fi

		dnl - fdi install path: $datarootdir/hal/fdi/information/20thirdparty
		AC_MSG_CHECKING(for libhal Device Information path)
		HAL_FDI_PATH=`pkg-config --silence-errors --variable=hal_fdidir hal`
		if (test -n "$HAL_FDI_PATH"); then
			HAL_FDI_PATH="${HAL_FDI_PATH}/information/20thirdparty"
			AC_MSG_RESULT(${HAL_FDI_PATH})
		else
			# fallback to detecting the right path
			if (test -d "/usr/share/hal/fdi/information/20thirdparty" -o -d "/usr/local/share/hal/fdi/information/20thirdparty"); then
				# seems supported everywhere
				HAL_FDI_PATH="${datarootdir}/hal/fdi/information/20thirdparty"
				AC_MSG_RESULT(${HAL_FDI_PATH})
			else
				# FIXME
				HAL_FDI_PATH="/dev/null"
				AC_MSG_RESULT(not found)
			fi
		fi
	fi

	CFLAGS="${CFLAGS_ORIG}"
	LDFLAGS="${LDFLAGS_ORIG}"

	dnl - test for g_timeout_add_seconds availability
	AC_MSG_CHECKING([if GLib is version 2.14.0 or newer])
	if pkg-config --silence-errors --atleast-version=2.14.0 glib-2.0; then
		AC_DEFINE(HAVE_GLIB_2_14, 1, [Define to 1 if GLib is version 2.14 or newer])
		AC_MSG_RESULT(yes)
	else
		AC_MSG_RESULT(no)
	fi
fi
])
