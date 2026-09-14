#!/bin/sh
################################################################################
#
# nut-recorder
#   An utility to record device running sequence (Ie power failures or any
#   other change) and dump it in a .seq format
#   The .seq file can then be used by the 'dummy-ups driver to replay the
#   sequence.
#
################################################################################
# FIXME:
#  - implement PAUSE / RESUME (do not increment TIMER) on pressing space (?)
#  - implement support for creating either .dev (static dump) or .seq
#  - implement dump of instcmd and upsrw
################################################################################

strUsage="Usage: nut-recorder <device-name> [output-file] [interval]"

# log data each 5 seconds
DEFAULT_INTERVAL=5

# temporary files location
TEMP_DIR="/tmp"

# output this file by default
DEFAULT_OUTPUT="dummy-device.seq"

# Process command line parameters
if [ -z "$1" ]; then
	echo "$strUsage"
	exit
else
	devName=$1
fi

if [ -z "$2" ]; then
	outFile=$DEFAULT_OUTPUT
else
	outFile=$2
fi

if [ -z "$3" ]; then
	pollInterval=$DEFAULT_INTERVAL
else
	pollInterval=$3
fi

# initialize TIMER value
curTimer=0

# Test communication with the device
testResult="`upsc $devName > /dev/null`"
if [ $? -gt 0 ]; then
	echo "$devName: $testResult"
	exit
fi

# initialize output file
echo "# dummy-ups sequence recorded with $0\n"> $outFile

# initialize data
upsc $devName >> $outFile
cp -f $outFile ${TEMP_DIR}/prevDump.tmp

echo "Initial data:\n"
cat $outFile

while (true)
do
	# rest a bit before getting fresh data
	sleep $pollInterval

	# update the TIMER value
	curTimer=`expr $curTimer + $pollInterval`

	# dump the current data
	testResult="`upsc $devName > ${TEMP_DIR}/curDump.tmp`"
	if [ $? -gt 0 ]; then
		echo "$devName: $testResult"
	#	FIXME: what to do (pause, exit)?
	fi

	# do the diff
	dataDiff="`diff --unchanged-line-format='' --old-line-format='' --new-line-format='%L' ${TEMP_DIR}/prevDump.tmp ${TEMP_DIR}/curDump.tmp`"

	# dump actual, if any
	if [ ! -z "${dataDiff}" ]; then
		# dump differences
		echo "TIMER $curTimer" >> $outFile
		echo "$dataDiff" >> $outFile
		# and echo out
		echo "TIMER $curTimer"
		echo "$dataDiff"
		curTimer=0
	fi

	# rotate dumps
	mv ${TEMP_DIR}/curDump.tmp ${TEMP_DIR}/prevDump.tmp

done
