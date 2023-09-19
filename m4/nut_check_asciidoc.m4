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
	# Note: this is for both asciidoc and a2x at this time
	ASCIIDOC_MIN_VERSION="8.6.3"
	# Note: this is checked in the configure script if PDF is of interest at all
	DBLATEX_MIN_VERSION="0.2.5"

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

	dnl Some builds of aspell (e.g. in mingw) claim they do not know mode "tex"
	dnl even though they can list it as a built-in filter and files exist.
	dnl It seems that specifying the path helps in those cases.
	ASPELL_FILTER_PATH="none"
	dnl Location of "tex.amf" may be shifted, especially if binary filters
	dnl are involved (happens in some platform packages but not others).
	ASPELL_FILTER_TEX_PATH="none"
	if test -n "${ASPELL}" ; then
		dnl # e.g.: @(#) International Ispell Version 3.1.20 (but really Aspell 0.60.8)
		AC_MSG_CHECKING([for aspell version])
		ASPELL_VERSION="`LANG=C LC_ALL=C ${ASPELL} --version 2>/dev/null | sed -e 's,^.*@<:@Aa@:>@spell \(@<:@0-9.@:>@*\),\1,' -e 's,@<:@^0-9.@:>@.*,,'`" || ASPELL_VERSION="none"
		AC_MSG_RESULT([${ASPELL_VERSION}])

		ASPELL_VERSION_MINMAJ="`echo "${ASPELL_VERSION}" | sed 's,\.@<:@0-9@:>@@<:@0-9@:>@*$,,'`"

		AC_MSG_CHECKING([for aspell filtering resources directory])
		ASPELL_BINDIR="`dirname "$ASPELL"`"
		if test -d "${ASPELL_BINDIR}/../lib" ; then
			if test x"${ASPELL_VERSION}" != x"none" && test -d "${ASPELL_BINDIR}/../lib/aspell-${ASPELL_VERSION}" ; then
				ASPELL_FILTER_PATH="`cd "${ASPELL_BINDIR}/../lib/aspell-${ASPELL_VERSION}" && pwd`" \
				|| ASPELL_FILTER_PATH="${ASPELL_BINDIR}/../lib/aspell-${ASPELL_VERSION}"
			else
				if test x"${ASPELL_VERSION_MINMAJ}" != x"none" && test -d "${ASPELL_BINDIR}/../lib/aspell-${ASPELL_VERSION_MINMAJ}" ; then
					ASPELL_FILTER_PATH="`cd "${ASPELL_BINDIR}/../lib/aspell-${ASPELL_VERSION_MINMAJ}" && pwd`" \
					|| ASPELL_FILTER_PATH="${ASPELL_BINDIR}/../lib/aspell-${ASPELL_VERSION_MINMAJ}"
				else
					if test -d "${ASPELL_BINDIR}/../lib/aspell" ; then
						ASPELL_FILTER_PATH="`cd "${ASPELL_BINDIR}/../lib/aspell" && pwd`" \
						|| ASPELL_FILTER_PATH="${ASPELL_BINDIR}/../lib/aspell"
					fi
				fi
			fi
		fi
		AC_MSG_RESULT([${ASPELL_FILTER_PATH}])

		AC_MSG_CHECKING([for aspell "tex" filtering resources directory])
		if test -d "${ASPELL_FILTER_PATH}" ; then
			ASPELL_FILTER_TEX_PATH="`find "${ASPELL_FILTER_PATH}" -name "tex.amf"`" \
			&& ASPELL_FILTER_TEX_PATH="`dirname "${ASPELL_FILTER_TEX_PATH}"`" \
			&& test -d "${ASPELL_FILTER_TEX_PATH}" \
			|| ASPELL_FILTER_TEX_PATH="none"
		fi
		AC_MSG_RESULT([${ASPELL_FILTER_TEX_PATH}])
	fi
	AM_CONDITIONAL([HAVE_ASPELL_FILTER_PATH], [test -d "$ASPELL_FILTER_PATH"])
	AC_SUBST(ASPELL_FILTER_PATH)
	AM_CONDITIONAL([HAVE_ASPELL_FILTER_TEX_PATH], [test -d "$ASPELL_FILTER_TEX_PATH"])
	AC_SUBST(ASPELL_FILTER_TEX_PATH)

	dnl Note that a common "nut_have_asciidoc" variable is in fact a flag
	dnl that we have several tools needed for the documentation generation
	dnl TODO? Rename the script variable and makefile flags to reflect this?
	AC_MSG_CHECKING([if asciidoc version can build manpages (minimum required ${ASCIIDOC_MIN_VERSION})])
	AX_COMPARE_VERSION([${ASCIIDOC_VERSION}], [ge], [${ASCIIDOC_MIN_VERSION}], [
		AC_MSG_RESULT(yes)
		nut_have_asciidoc="yes"
	], [
		AC_MSG_RESULT(no)
		nut_have_asciidoc="no"
	])

	AC_MSG_CHECKING([if a2x version can build manpages (minimum required ${ASCIIDOC_MIN_VERSION})])
	AX_COMPARE_VERSION([${A2X_VERSION}], [ge], [${ASCIIDOC_MIN_VERSION}], [
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
