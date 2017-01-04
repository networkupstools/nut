dnl Check for tools used in generation of documentation final-format files
dnl such as the basically required MAN, and optional HTML (single or chunked).
dnl On success, set nut_have_asciidoc="yes" (meaning we can do at least some
dnl documentation generation) and lots of automake macros and configure vars.
dnl On failure, set nut_have_asciidoc="no" (meaning we can't generate even a
dnl manpage, which is a requirement for proceeding to the other formats).
dnl This macro can be run multiple times, but will do the checking only once.
dnl Also note that this routine currently only checks the basic presence (and
dnl versions) of the required software; the configure script has additional
dnl functional checks (ability to build doc files) based on --with-doc request
dnl so as to not waste time on doc-types not requested by maintainer, and done
dnl in ways more intimate to NUT (using its tracked docs).

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
	AM_CONDITIONAL([MANUALUPDATE], [test -n "${ASCIIDOC}"])

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

	AC_PATH_PROGS([XSLTPROC], [xsltproc])
	if test -n "${XSLTPROC}"; then
		AC_MSG_CHECKING([for xsltproc version])
		XSLTPROC_VERSION="`${XSLTPROC} --version 2>/dev/null`"
		dnl strip 'xsltproc version ' from version string
		XSLTPROC_VERSION="${XSLTPROC_VERSION##* }"
		AC_MSG_RESULT(${XSLTPROC_VERSION} found)
	fi

	AC_PATH_PROGS([XMLLINT], [xmllint])
	if test -n "${XMLLINT}"; then
		AC_MSG_CHECKING([for xmllint version])
		XMLLINT_VERSION="`${XMLLINT} --version 2>/dev/null`"
		dnl strip 'xmllint version ' from version string
		XMLLINT_VERSION="${XMLLINT_VERSION##* }"
		if test -z "${XMLLINT_VERSION}" ; then
			dnl Some releases also report what flags they were compiled with as
			dnl part of the version info, so the last-line match finds nothing.
			dnl Also some builds return version data to stderr.
			XMLLINT_VERSION="`${XMLLINT} --version 2>&1 | grep version`"
			XMLLINT_VERSION="${XMLLINT_VERSION##* }"
		fi
		AC_MSG_RESULT(${XMLLINT_VERSION} found)
	fi

	AC_PATH_PROGS([SOURCE_HIGHLIGHT], [source-highlight])
	AM_CONDITIONAL([HAVE_SOURCE_HIGHLIGHT], [test -n "$SOURCE_HIGHLIGHT"])

	dnl check for spell checking deps
	AC_PATH_PROGS([ASPELL], [aspell])
	AM_CONDITIONAL([HAVE_ASPELL], [test -n "$ASPELL"])

	dnl Note that a common "nut_have_asciidoc" variable is in fact a flag
	dnl that we have several tools needed for the documentation generation
	dnl TODO? Rename the script variable and makefile flags to reflect this?
	AC_MSG_CHECKING([if asciidoc version can build manpages (minimum required 8.6.3)])
	AX_COMPARE_VERSION([${ASCIIDOC_VERSION}], [ge], [8.6.3], [
		AC_MSG_RESULT(yes)
		nut_have_asciidoc="yes"
	], [
		AC_MSG_RESULT(no)
		nut_have_asciidoc="no"
	])

	AC_MSG_CHECKING([if a2x version can build manpages (minimum required 8.6.3)])
	AX_COMPARE_VERSION([${A2X_VERSION}], [ge], [8.6.3], [
		AC_MSG_RESULT(yes)
	], [
		AC_MSG_RESULT(no)
		nut_have_asciidoc="no"
	])

	dnl TODO: test for docbook-xsl files (maybe build a test man page?)
	dnl https://github.com/networkupstools/nut/issues/162
	AC_MSG_CHECKING([if xsltproc is present (mandatory for man page regeneration)])
	if test -n "${XSLTPROC}"; then
		AC_MSG_RESULT(yes)
	else
		AC_MSG_RESULT(no)
		nut_have_asciidoc="no"
	fi

	AC_MSG_CHECKING([if xmllint is present (mandatory for man page regeneration)])
	if test -n "${XMLLINT}"; then
		AC_MSG_RESULT(yes)
	else
		AC_MSG_RESULT(no)
		nut_have_asciidoc="no"
	fi

	dnl Notes: we also keep HAVE_ASCIIDOC for implicit targets, such as manpage
	dnl building
	AM_CONDITIONAL([HAVE_ASCIIDOC], [test "${nut_have_asciidoc}" = "yes"])

	AC_MSG_CHECKING([if we have all the tools mandatory for man page regeneration])
	AC_MSG_RESULT([${nut_have_asciidoc}])

	AC_MSG_CHECKING([if source-highlight is present (preferable for documentation generation)])
	if test -n "${SOURCE_HIGHLIGHT}"; then
		AC_MSG_RESULT(yes)
	else
		AC_MSG_RESULT(no)
	fi

fi
])
