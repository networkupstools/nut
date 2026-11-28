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
recovering=false

# Test communication with the device
testResult="`upsc \"$devName\" 2>&1 > /dev/null`"
if [ $? -gt 0 ]; then
	echo "$devName: $testResult"
	exit
fi

# initialize output file
echo "# dummy-ups sequence recorded with $0\n"> $outFile

# Preserve the full alarm description as one quoted sequence-file value.
quote_alarm() {
	sed '/^ups\.alarm: / {
		s/[\\"]/\\&/g
		s/^ups\.alarm: /ups.alarm: "/
		s/$/"/
	}'
}

# Keep the last successful upsc snapshot in its original format for diff.
upsc "$devName" > "${TEMP_DIR}/prevDump.tmp" || exit 1
quote_alarm < "${TEMP_DIR}/prevDump.tmp" >> $outFile

echo "Initial data:\n"
cat $outFile

while (true)
do
	# rest a bit before getting fresh data
	sleep $pollInterval

	# update the TIMER value
	curTimer=`expr $curTimer + $pollInterval`

	# dump the current data
	testResult="`upsc \"$devName\" 2>&1 > \"${TEMP_DIR}/curDump.tmp\"`"
	if [ $? -gt 0 ]; then
		echo "$devName: $testResult"
		recovering=true
		continue
	fi
	# On reconnect, upsd can report WAIT before the driver dump is
	# complete. A device can also report WAIT with real readings.
	if $recovering && grep -Fqx 'ups.status: WAIT' "${TEMP_DIR}/curDump.tmp"; then
		if ! grep -Eqv '^(ups\.status: WAIT|driver\.state: .*)$' "${TEMP_DIR}/curDump.tmp"; then
			continue
		fi
		# Keep affirmative changes. upsc has no dump-complete marker,
		# so defer a missing description until a subsequent non-WAIT
		# snapshot or an explicit alarm value resolves it.
		if ! grep -q '^ups\.alarm:' "${TEMP_DIR}/curDump.tmp"; then
			grep '^ups\.alarm:' "${TEMP_DIR}/prevDump.tmp" >> "${TEMP_DIR}/curDump.tmp"
		else
			recovering=false
		fi
	else
		recovering=false
	fi

	# Emit changed values. Repeat an unchanged description on a status change
	# so playback can distinguish this from a clear in an older recording.
	# Record a disappearing description explicitly.
	dataDiff=$(diff --unchanged-line-format=' %L' --old-line-format='-%L' --new-line-format='+%L' "${TEMP_DIR}/prevDump.tmp" "${TEMP_DIR}/curDump.tmp" |
		awk '
			/^ ups\.alarm:/ { retained = substr($0, 2) }
			/^-ups\.alarm:/ { removed = 1 }
			/^[+]ups\.alarm:/ { added = 1 }
			/^[+]ups\.status:/ { status_changed = 1 }
			/^[+]/ { print substr($0, 2) }
			END {
				if (status_changed && retained)
					print retained
				else if (removed && !added && !retained)
					print "ups.alarm:"
			}' | quote_alarm)

	# dump actual, if any
	if [ ! -z "${dataDiff}" ]; then
		# dump differences
		echo "TIMER $curTimer" >> $outFile
		printf '%s\n' "$dataDiff" >> $outFile
		# and echo out
		echo "TIMER $curTimer"
		printf '%s\n' "$dataDiff"
		curTimer=0
	fi

	# rotate dumps
	mv ${TEMP_DIR}/curDump.tmp ${TEMP_DIR}/prevDump.tmp

done
