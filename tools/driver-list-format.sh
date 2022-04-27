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

echo "Checking whether driver.list[.in] are well formatted"
for drvfile in driver.list.in driver.list
do
	if [ -f "${DRVLIST_PATH}/data/${drvfile}" ]; then
		sed -e '/^#/!s/\" \+\"/\"\t\"/g' -e "/^#/!s/[[:blank:]]*$//" < "${DRVLIST_PATH}/data/${drvfile}" > "${DRVLIST_PATH}/data/${drvfile}.tabbed"
		mv -f "${DRVLIST_PATH}/data/${drvfile}.tabbed" "${DRVLIST_PATH}/data/${drvfile}"
		echo "Processed ${DRVLIST_PATH}/data/${drvfile}"
	else
		echo "Skipping ${drvfile} as it is missing..."
	fi
done
echo "done"
