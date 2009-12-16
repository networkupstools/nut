dnl Check for LIBSSL compiler flags. On success, set nut_have_libssl="yes"
dnl and set LIBSSL_CFLAGS and LIBSSL_LDFLAGS. On failure, set
dnl nut_have_libssl="no". This macro can be run multiple times, but will
dnl do the checking only once. 

AC_DEFUN([NUT_CHECK_LIBSSL], 
[
if test -z "${nut_have_libssl_seen}"; then
	nut_have_libssl_seen=yes

	dnl save CFLAGS and LDFLAGS
	CFLAGS_ORIG="${CFLAGS}"
	LDFLAGS_ORIG="${LDFLAGS}"

	AC_MSG_CHECKING(for openssl version via pkg-config)
	OPENSSL_VERSION=`pkg-config --silence-errors --modversion openssl`
	if (test "$?" != "0"); then
		nut_have_libssl=no
	else
		nut_have_libssl=yes
	fi

	AC_MSG_RESULT(${OPENSSL_VERSION} found)

	AC_MSG_CHECKING(for openssl cflags via pkg-config)
	CFLAGS=`pkg-config --silence-errors --cflags openssl`
	if (test "$?" != "0"); then
		AC_MSG_RESULT(not found)
		nut_have_libssl=no
	else
		AC_MSG_RESULT(${CFLAGS})
	fi

	AC_MSG_CHECKING(for openssl ldflags via pkg-config)
	LDFLAGS=`pkg-config --silence-errors --libs openssl`
	if (test "$?" != "0"); then
		AC_MSG_RESULT(not found)
		nut_have_libssl=no
	else
		AC_MSG_RESULT(${LDFLAGS})
	fi

	if test "${nut_have_libssl}" = "yes"; then
		AC_DEFINE(HAVE_SSL, 1, [Define to enable SSL development code])
		LIBSSL_CFLAGS="${CFLAGS}"
		LIBSSL_LDFLAGS="${LDFLAGS}"
	fi

	dnl restore original CFLAGS and LDFLAGS
	CFLAGS="${CFLAGS_ORIG}"
	LDFLAGS="${LDFLAGS_ORIG}"
fi
])
