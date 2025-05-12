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
	echo "Can't find driver.list in . or .."
	exit 1
fi

ACTION="Ensuring"
if [ x"${1-}" = x"--check" ] ; then
	ACTION="Checking"
fi

RES=0

echo "$ACTION whether driver.list[.in] are well formatted"
for drvfile in driver.list.in driver.list
do
	if [ -f "${DRVLIST_PATH}/data/${drvfile}" ]; then
		sed -e '/^#/!s/\" \+\"/\"\t\"/g' -e "/^#/!s/[[:blank:]]*$//" < "${DRVLIST_PATH}/data/${drvfile}" > "${DRVLIST_PATH}/data/${drvfile}.tabbed" && \
		if [ x"${ACTION}" = xEnsuring ] ; then
			mv -f "${DRVLIST_PATH}/data/${drvfile}.tabbed" "${DRVLIST_PATH}/data/${drvfile}"
		else # Checking
			diff "${DRVLIST_PATH}/data/${drvfile}.tabbed" "${DRVLIST_PATH}/data/${drvfile}" >/dev/null \
			|| { echo "File '${DRVLIST_PATH}/data/${drvfile}' markup needs to be fixed (run $0 and commit the git change, please)" >&2 ; RES=1 ; }
		fi || RES=$?
		echo "Processed ${DRVLIST_PATH}/data/${drvfile}"
	else
		echo "Skipping ${drvfile} as it is missing..."
	fi
done
echo "done"

exit $RES
