#!/bin/sh
################################################################################
#
# Ensure that driver.list and driver.list.in are properly formatted (with tabs)
#
################################################################################

# Adapt path for either dist target or manual call
CURRENT_PATH="`dirname $0`"
DRVLIST_PATH=""

if [ -f "${CURRENT_PATH}/data/driver.list.in" ]; then
	DRVLIST_PATH="${CURRENT_PATH}"
elif [ -f "${CURRENT_PATH}/../data/driver.list.in" ]; then
	DRVLIST_PATH="${CURRENT_PATH}/.."
else
	echo "$0: Can't find driver.list in . or .., aborting" >&2
	exit 1
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
	if [ -f "${DRVLIST_PATH}/data/${drvfile}" ]; then
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
		< "${DRVLIST_PATH}/data/${drvfile}" \
		> "${DRVLIST_PATH}/data/${drvfile}.tabbed" \
		&& \
		{
			# verify that lines are either empty, all-comments,
			# or have six quoted fields (and optional comment);
			# the fields may be empty (just two double-quotes).
			BADLINES="`grep -vE '(^$|^#|^"[^"]*"'"${TABCHAR}"'"[^"]*"'"${TABCHAR}"'"[^"]*"'"${TABCHAR}"'"[^"]*"'"${TABCHAR}"'"[^"]*"'"${TABCHAR}"'"[^"]*("|"'"${TABCHAR}"'#.*)$)' < "${DRVLIST_PATH}/data/${drvfile}.tabbed"`"
			if [ x"${BADLINES}" != x ] ; then
				echo "$0: ERROR: markup of '${DRVLIST_PATH}/data/${drvfile}' needs to be fixed: some lines are not exactly 6 fields (and optional comment)" >&2
				echo "$BADLINES" | head -5
				RES=1
				false
			fi
		} && \
		if [ x"${ACTION}" = xEnsuring ] ; then
			if diff "${DRVLIST_PATH}/data/${drvfile}.tabbed" "${DRVLIST_PATH}/data/${drvfile}" >/dev/null ; then
				# Same content
				rm -f "${DRVLIST_PATH}/data/${drvfile}.tabbed"
			else
				# Ensure new content is applied
				mv -f "${DRVLIST_PATH}/data/${drvfile}.tabbed" "${DRVLIST_PATH}/data/${drvfile}"
			fi
		else # Checking
			diff -u "${DRVLIST_PATH}/data/${drvfile}.tabbed" "${DRVLIST_PATH}/data/${drvfile}" \
			|| { GITACT=""
			     case "${drvfile}" in *.in) GITACT=" and commit the git change" ;; esac
			     echo "$0: ERROR: markup of '${DRVLIST_PATH}/data/${drvfile}' needs to be fixed: re-run this script without args${GITACT}, please" >&2
			     RES=1
			   }
		fi \
		|| RES=$?

		echo "$0: Processed ${DRVLIST_PATH}/data/${drvfile}"
	else
		echo "$0: Skipping ${drvfile} as it is missing..."
	fi
done
echo "$0: done ($RES)"

exit $RES
