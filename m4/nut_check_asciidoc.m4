dnl Check for LIBUSB compiler flags. On success, set nut_have_libusb="yes"
dnl and set LIBUSB_CFLAGS and LIBUSB_LDFLAGS. On failure, set
dnl nut_have_libusb="no". This macro can be run multiple times, but will
dnl do the checking only once. 

AC_DEFUN([NUT_CHECK_ASCIIDOC], 
[
if test -z "${nut_have_asciidoc_seen}"; then
	nut_have_asciidoc_seen=yes

	AC_PATH_PROGS([ASCIIDOC], [asciidoc])
	if test -n "${ASCIIDOC}"; then
		AC_MSG_CHECKING([for asciiDoc version])
		ASCIIDOC_VERSION="`${ASCIIDOC} --version 2>/dev/null`"
		dnl strip 'asciidoc ' from version string
		ASCIIDOC_VERSION="${ASCIIDOC_VERSION##* }"
		AC_MSG_RESULT(${ASCIIDOC_VERSION} found)
	fi

	AC_PATH_PROGS([A2X], [a2x])
	if test -n "${A2X}"; then
		AC_MSG_CHECKING([for a2x version])
		A2X_VERSION="`${A2X} --version 2>/dev/null`"
		dnl strip 'a2x ' from version string
		A2X_VERSION="${A2X_VERSION##* }"
		AC_MSG_RESULT(${A2X_VERSION} found)
	fi

	AC_PATH_PROGS([DBLATEX], [dblatex])
	if test -n "${DBLATEX}"; then
		AC_MSG_CHECKING([for dblatex version])
		DBLATEX_VERSION="`${DBLATEX} --version 2>/dev/null`"
		dnl strip 'dblatex version ' from version string
		DBLATEX_VERSION="${DBLATEX_VERSION##* }"
		AC_MSG_RESULT(${DBLATEX_VERSION} found)
	fi

	dnl FIXME check for xsltproc, xmlllint, etc for chunked HTML and man pages
fi
])
