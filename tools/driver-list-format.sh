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

echo "$0: $ACTION whether driver.list[.in] are well formatted"
for drvfile in driver.list.in driver.list
do
	if [ -f "${DRVLIST_PATH}/data/${drvfile}" ]; then
		# For every non-comment line:
		# * replace quote-spaces-quote with quote-TAB-quote
		# * strip trailing blank characters at the end of line
		# * if there is a trailing comment, make sure it is
		#   also TAB-separated (from the presumed sixth field)
		sed \
			-e '/^#/!s/\" \+\"/\"\t\"/g' \
			-e '/^#/!s/[[:blank:]]*$//' \
			-e '/^#/!s/\" \+\#/\"\t\#/' \
		< "${DRVLIST_PATH}/data/${drvfile}" \
		> "${DRVLIST_PATH}/data/${drvfile}.tabbed" \
		&& \
		if [ x"${ACTION}" = xEnsuring ] ; then
			mv -f "${DRVLIST_PATH}/data/${drvfile}.tabbed" "${DRVLIST_PATH}/data/${drvfile}"
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
