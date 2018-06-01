dnl Check for LIBUSB 1.0 or 0.1 (and, if found, fill 'nut_usb_lib' with its
dnl approximate version) and its compiler flags.
dnl On success, set:
dnl - nut_have_libusb="yes"
dnl - LIBUSB_CFLAGS and LIBUSB_LIBS,
dnl - if libusb 0.1 is found, nut_cv_libusb0_needs_dl to "yes" or "no".
dnl On failure, set:
dnl - nut_have_libusb="no".
dnl This macro can be run multiple times, but will do the checking only once.
dnl By default, if both libusb 1.0 and libusb 0.1 are available and appear to be
dnl usable, libusb 1.0 takes precedence.
dnl An optional argument with value 'libusb-1.0' or 'libusb-0.1' can be used to
dnl restrict checks to a specific version.

AC_DEFUN([NUT_CHECK_LIBUSB],
[
if test -z "${nut_have_libusb_seen}"; then
	nut_have_libusb_seen=yes

	dnl save CFLAGS and LIBS
	CFLAGS_ORIG="${CFLAGS}"
	LIBS_ORIG="${LIBS}"

	nut_have_libusb=no
	nut_usb_lib="($1)"

	dnl check for both libusb 1.0 and libusb 0.1/libusb-compat, if not asked otherwise
	libusb1_VERSION="none"
	if test "${nut_usb_lib}" != "(libusb-0.1)"; then
		AC_MSG_CHECKING(for libusb 1.0 version via pkg-config)
		libusb1_VERSION="`pkg-config --silence-errors --modversion libusb-1.0 2>/dev/null`"
		if test "$?" = "0" -a -n "${libusb1_VERSION}"; then
			libusb1_CFLAGS="`pkg-config --silence-errors --cflags libusb-1.0 2>/dev/null`"
			libusb1_LIBS="`pkg-config --silence-errors --libs libusb-1.0 2>/dev/null`"
			nut_have_libusb=yes
		else
			libusb1_VERSION="none"
		fi
		AC_MSG_RESULT(${libusb1_VERSION} found)
	fi

	libusb0_VERSION="none"
	if test "${nut_usb_lib}" != "(libusb-1.0)"; then
		AC_MSG_CHECKING(for libusb 0.1 version via pkg-config)
		libusb0_VERSION="`pkg-config --silence-errors --modversion libusb 2>/dev/null`"
		if test "$?" = "0" -a -n "${libusb0_VERSION}"; then
			libusb0_CFLAGS="`pkg-config --silence-errors --cflags libusb 2>/dev/null`"
			libusb0_LIBS="`pkg-config --silence-errors --libs libusb 2>/dev/null`"
			nut_have_libusb=yes
		else
			libusb0_VERSION="`pkg-config --silence-errors --modversion libusb-0.1 2>/dev/null`"
			if test "$?" = "0" -a -n "${libusb0_VERSION}"; then
				libusb0_CFLAGS="`pkg-config --silence-errors --cflags libusb-0.1 2>/dev/null`"
				libusb0_LIBS="`pkg-config --silence-errors --libs libusb-0.1 2>/dev/null`"
				nut_have_libusb=yes
			else
				AC_MSG_CHECKING(via libusb-config)
				libusb0_VERSION="`libusb-config --version 2>/dev/null`"
				if test "$?" = "0" -a -n "${libusb0_VERSION}"; then
					libusb0_CFLAGS="`libusb-config --cflags 2>/dev/null`"
					libusb0_LIBS="`libusb-config --libs 2>/dev/null`"
					nut_have_libusb=yes
				else
					libusb0_VERSION="none"
				fi
			fi
		fi
		AC_MSG_RESULT(${libusb0_VERSION} found)
	fi

	dnl check optional user-provided values for cflags/ldflags and publish what we end up using
	AC_MSG_CHECKING(for libusb cflags)
	AC_ARG_WITH(usb-includes,
		AS_HELP_STRING([@<:@--with-usb-includes=CFLAGS@:>@], [include flags for the libusb library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-usb-includes - see docs/configure.txt)
			;;
		*)
			libusb1_CFLAGS="${withval}"
			libusb0_CFLAGS="${withval}"
			AC_MSG_RESULT([${withval}])
			;;
		esac
	], [
		if test "${nut_have_libusb}" != "yes"; then
			AC_MSG_RESULT([none])
		elif test "${libusb0_VERSION}" = "none"; then
			AC_MSG_RESULT([${libusb1_CFLAGS}])
		elif test "${libusb1_VERSION}" = "none"; then
			AC_MSG_RESULT([${libusb0_CFLAGS}])
		else
			AC_MSG_RESULT([libusb 1.0: ${libusb1_CFLAGS}; libusb 0.1: ${libusb0_CFLAGS}])
		fi
	])

	AC_MSG_CHECKING(for libusb ldflags)
	AC_ARG_WITH(usb-libs,
		AS_HELP_STRING([@<:@--with-usb-libs=LIBS@:>@], [linker flags for the libusb library]),
	[
		case "${withval}" in
		yes|no)
			AC_MSG_ERROR(invalid option --with(out)-usb-libs - see docs/configure.txt)
			;;
		*)
			libusb1_LIBS="${withval}"
			libusb0_LIBS="${withval}"
			AC_MSG_RESULT([${withval}])
			;;
		esac
	], [
		if test "${nut_have_libusb}" != "yes"; then
			AC_MSG_RESULT([none])
		elif test "${libusb0_VERSION}" = "none"; then
			AC_MSG_RESULT([${libusb1_LIBS}])
		elif test "${libusb1_VERSION}" = "none"; then
			AC_MSG_RESULT([${libusb0_LIBS}])
		else
			AC_MSG_RESULT([libusb 1.0: ${libusb1_LIBS}; libusb 0.1: ${libusb0_LIBS}])
		fi
	])

	dnl check if libusb is usable
	if test "${nut_have_libusb}" = "yes"; then
		dnl if not explicitly chosen, make libusb-1.0 take precedence over libusb-0.1,
		dnl if both are available: so, first check libusb 1.0, if available
		if test "${nut_usb_lib}" != "(libusb-0.1)"; then
			nut_usb_lib="(none)"
			pkg-config --silence-errors --atleast-version=1.0 libusb-1.0 2>/dev/null
			if test "$?" = "0"; then
				LIBS="${libusb1_LIBS}"
				CFLAGS="${libusb1_CFLAGS}"
				dnl libusb 1.0: libusb_set_auto_detach_kernel_driver
				AC_CHECK_HEADERS(libusb.h, [], [nut_have_libusb=no], [AC_INCLUDES_DEFAULT])
				AC_CHECK_FUNCS(libusb_init, [], [nut_have_libusb=no])
				AC_CHECK_FUNCS(libusb_strerror, [], [nut_have_libusb=no; nut_have_libusb_strerror=no])
				if test "${nut_have_libusb_strerror}" = "no"; then
					AC_MSG_WARN([libusb_strerror() not found; install libusbx to use libusb 1.0 API. See https://github.com/networkupstools/nut/issues/509])
				fi
				dnl This function is fairly old, but check for it anyway:
				AC_CHECK_FUNCS(libusb_kernel_driver_active)
				dnl Check for libusb "force driver unbind" availability
				AC_CHECK_FUNCS(libusb_set_auto_detach_kernel_driver)
				dnl libusb 1.0: libusb_detach_kernel_driver
				dnl FreeBSD 10.1-10.3 have this, but not libusb_set_auto_detach_kernel_driver
				AC_CHECK_FUNCS(libusb_detach_kernel_driver)
				if test "${nut_have_libusb}" = "yes"; then
					AC_DEFINE(WITH_LIBUSB_1_0, 1, [Define to 1 for version 1.0 of the libusb.])
					nut_usb_lib="(libusb-1.0)"
				fi
			fi
		fi
		dnl if libusb 1.0 is not available or usable, or if libusb 0.1/libusb-compat is explicitly chosen, try it
		if test	"${nut_usb_lib}" != "(libusb-1.0)"; then
			nut_usb_lib="(none)"
			LIBS="${libusb0_LIBS}"
			CFLAGS="${libusb0_CFLAGS}"
			AC_CHECK_HEADERS(usb.h, [nut_have_libusb=yes], [nut_have_libusb=no], [AC_INCLUDES_DEFAULT])
			AC_CHECK_FUNCS(usb_init, [], [nut_have_libusb=no])
			dnl Check for libusb "force driver unbind" availability
			dnl Also define 'HAVE_LIBUSB_DETACH_KERNEL_DRIVER' for libusb-compat-1.0
			AC_CHECK_FUNCS(usb_detach_kernel_driver_np, [AC_DEFINE(HAVE_LIBUSB_DETACH_KERNEL_DRIVER, HAVE_USB_DETACH_KERNEL_DRIVER_NP,)])
			if test "${nut_have_libusb}" = "yes"; then
				AC_DEFINE(WITH_LIBUSB_0_1, 1, [Define to 1 for version 0.1 of the libusb.])
				nut_usb_lib="(libusb-0.1)"
			fi
			dnl Check if libusb-0.1 is actually a libusb-compat-0.1 version implemented in such a way that would cause name clashes with our libusb-compat-1.0
			AC_CACHE_CHECK(
				[if libusb-0.1 needs to be dynamically loaded],
				[nut_cv_libusb0_needs_dl],
				[
					nut_cv_libusb0_needs_dl=yes
					AC_RUN_IFELSE(
						[AC_LANG_PROGRAM(
							[[
								/**
								 * @file
								 * @brief	Test whether libusb[-compat]-0.1 would clash with libusb-1.0.
								 * @copyright	@parblock
								 * Copyright (C):
								 * - 2018 -- Daniele Pezzini (<hyouko@gmail.com>)
								 *
								 * This program is free software: you can redistribute it and/or modify
								 * it under the terms of the GNU General Public License as published by
								 * the Free Software Foundation, either version 2 of the License, or
								 * (at your option) any later version.
								 *
								 * This program is distributed in the hope that it will be useful,
								 * but WITHOUT ANY WARRANTY; without even the implied warranty of
								 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
								 * GNU General Public License for more details.
								 *
								 * You should have received a copy of the GNU General Public License
								 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
								 *		@endparblock
								 *
								 * @details
								 * When run, this little thing will return:
								 * - 0, if libusb[-compat]-0.1 appears to happily coexist with libusb-1.0,
								 * - 1, if not.
								 */

								#include <usb.h>

								/** @brief Possible clever answers to our @ref would_libusb0_clash_with_libusb1 question. */
								enum would_clash {
									PROBABLY_NOT		=  0,	/**< We can't be sure, but the tea leaves seem to indicate so. */
									SO_IT_SEEMS		=  1	/**< The oh so mighty stars disagree, try coffee grounds the next time. */
								};

								/** @brief Our ultimate question. */
								static enum would_clash		would_libusb0_clash_with_libusb1 = PROBABLY_NOT;

								/** @brief Error codes returned by libusb functions on failure. */
								enum libusb_error {
									/* ==8<------------------------------------------ */
									LIBUSB_ERROR_OTHER	= -99	/**< Other error. */
								};

								/** @brief A libusb context... (ignored) */
								typedef struct libusb_context	libusb_context;

								/** @brief Set @ref would_libusb0_clash_with_libusb1 and error out.
								 * @note This function is here just to see if usb_init() calls it... so don't static it! (Monsieur de La Palice told me)
								 * @return @ref LIBUSB_ERROR_OTHER, as we don't want usb_init() to do anything else. */
								int	libusb_init(
									libusb_context	**ctx	/**< ignored */
								) {
									would_libusb0_clash_with_libusb1 = SO_IT_SEEMS;
									return LIBUSB_ERROR_OTHER;
								}
							]],
							[[
								/* Let's see if usb_init() attempts to call our libusb_init()... */
								usb_init();

								/* ...we got an answer, apparently... */
								if (would_libusb0_clash_with_libusb1 == SO_IT_SEEMS)
									return 1;
								return 0;
							]]
						)],
						[nut_cv_libusb0_needs_dl=no],
						[],
						[]
					)
				]
			)
			if test "${cross_compiling}" = "yes"; then
				AC_MSG_WARN([(using default value, since cross-compiling, if you want to override it, enable caching and edit the value of nut_cv_libusb0_needs_dl var)])
			fi
			if test "${nut_cv_libusb0_needs_dl}" = "yes"; then
				AC_DEFINE(DYNAMICALLY_LOAD_LIBUSB_0_1, 1, [Define to 1 to dynamically load libusb 0.1.])
			fi
		fi
	fi

	if test "${nut_have_libusb}" = "yes"; then
		LIBUSB_CFLAGS="${CFLAGS}"
		LIBUSB_LIBS="${LIBS}"
	fi

	dnl restore original CFLAGS and LIBS
	CFLAGS="${CFLAGS_ORIG}"
	LIBS="${LIBS_ORIG}"
fi
])
