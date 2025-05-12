#!/bin/sh
################################################################################
#
# Ensure or read-only check that generated driver.list and source driver.list.in
# files are properly formatted (non-comment lines contain 6 fields separated
# with tabs, optionally with a trailing comment).
#
# Copyright (C)
#	2015-2016	Arnaud Quette <arnaud.quette@free.fr>
#	2025		Jim Klimov <jimklimov+nut@gmail.com>
#
################################################################################

# Adapt path for either dist target or manual call
CURRENT_PATH="`dirname $0`"
DRVLIST_PATH=""

# Integrate with Makefile recipes
if [ -n "${srcdir}" ] && [ -f "${srcdir}/driver.list.in" ] ; then
	DRVLIST_PATH="${srcdir}"
elif [ -n "${abs_srcdir}" ] && [ -f "${abs_srcdir}/driver.list.in" ] ; then
	DRVLIST_PATH="${abs_srcdir}"
elif [ -n "${top_srcdir}" ] && [ -f "${top_srcdir}/data/driver.list.in" ] ; then
	DRVLIST_PATH="${top_srcdir}/data"
elif [ -n "${abs_top_srcdir}" ] && [ -f "${abs_top_srcdir}/data/driver.list.in" ] ; then
	DRVLIST_PATH="${abs_top_srcdir}/data"
elif [ -f "${CURRENT_PATH}/data/driver.list.in" ]; then
	DRVLIST_PATH="${CURRENT_PATH}/data"
elif [ -f "${CURRENT_PATH}/../data/driver.list.in" ]; then
	DRVLIST_PATH="${CURRENT_PATH}/../data"
else
	echo "$0: ERROR: Can't find driver.list in . or .., aborting" >&2
	exit 1
fi

TMPBUILD_PATH=""
if [ -n "${builddir}" ] && [ -d "${builddir}" ] ; then
	TMPBUILD_PATH="${builddir}"
elif [ -n "${abs_builddir}" ] && [ -d "${abs_builddir}" ] ; then
	TMPBUILD_PATH="${abs_builddir}"
elif [ -n "${top_builddir}" ] && [ -d "${top_builddir}" ] ; then
	TMPBUILD_PATH="${top_builddir}/data"
elif [ -n "${abs_top_builddir}" ] && [ -d "${abs_top_builddir}" ] ; then
	TMPBUILD_PATH="${abs_top_builddir}/data"
else
	echo "$0: WARNING: no builddir was specified, using srcdir of the files for output" >&2
	TMPBUILD_PATH="${DRVLIST_PATH}"
fi

ACTION="Ensuring"
if [ x"${1-}" = x"--check" ] ; then
	ACTION="Checking"
fi

RES=0

# Some sed/grep implementations tend to have a problem with "\t"
# (treat it as escaped "t" character); substitutions are okay:
TABCHAR="`printf '\t'`"

echo "$0: $ACTION whether driver.list[.in] are well formatted"
for drvfile in driver.list.in driver.list
do
	if [ -f "${DRVLIST_PATH}/${drvfile}" ]; then
		# For every non-comment line:
		# * replace quote-spaces-quote (or technically any amount
		#   of blank space/tab characters) with quote-TAB-quote
		# * strip trailing blank characters at the end of line
		# * if there is a trailing comment, make sure it is
		#   also TAB-separated (from the presumed sixth field)
		sed \
			-e '/^#/!s/\"[[:blank:]]\+\"/\"\t\"/g' \
			-e '/^#/!s/[[:blank:]]*$//' \
			-e '/^#/!s/\" \+\#/\"\t\#/' \
		< "${DRVLIST_PATH}/${drvfile}" \
		> "${TMPBUILD_PATH}/${drvfile}.tabbed" \
		&& \
		{
			# verify that lines are either empty, all-comments,
			# or have six quoted fields (and optional comment);
			# the fields may be empty (just two double-quotes).
			BADLINES="`grep -vE '(^$|^#|^"[^"]*"'"${TABCHAR}"'"[^"]*"'"${TABCHAR}"'"[^"]*"'"${TABCHAR}"'"[^"]*"'"${TABCHAR}"'"[^"]*"'"${TABCHAR}"'"[^"]*("|"'"${TABCHAR}"'#.*)$)' < "${TMPBUILD_PATH}/${drvfile}.tabbed"`"
			if [ x"${BADLINES}" != x ] ; then
				echo "$0: ERROR: markup of '${DRVLIST_PATH}/${drvfile}' needs to be fixed: some lines are not exactly 6 fields (and optional comment)" >&2
				echo "$BADLINES" | head -5
				RES=1
				false
			fi
		} && \
		if [ x"${ACTION}" = xEnsuring ] ; then
			if diff "${TMPBUILD_PATH}/${drvfile}.tabbed" "${DRVLIST_PATH}/${drvfile}" >/dev/null ; then
				# Same content
				rm -f "${TMPBUILD_PATH}/${drvfile}.tabbed"
			else
				# Ensure new content is applied
				mv -f "${TMPBUILD_PATH}/${drvfile}.tabbed" "${DRVLIST_PATH}/${drvfile}"
			fi
		else # Checking
			diff -u "${TMPBUILD_PATH}/${drvfile}.tabbed" "${DRVLIST_PATH}/${drvfile}" \
			|| { GITACT=""
			     case "${drvfile}" in *.in) GITACT=" and commit the git change" ;; esac
			     echo "$0: ERROR: markup of '${DRVLIST_PATH}/${drvfile}' needs to be fixed: re-run this script without args${GITACT}, please" >&2
			     RES=1
			   }
		fi \
		|| RES=$?

		echo "$0: Processed ${DRVLIST_PATH}/${drvfile}"
	else
		echo "$0: Skipping ${drvfile} as it is missing..."
	fi
done
echo "$0: done ($RES)"

exit $RES
