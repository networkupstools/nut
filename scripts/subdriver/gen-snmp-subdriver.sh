#!/bin/bash
#
# an auxiliary script to produce a "stub" snmp-ups subdriver from
# SNMP data from a real agent or from dump files
#
# Version: 0.11-dmf
#
# See also: docs/snmp-subdrivers.txt
#
# Copyright (C)
# 2011 - 2012 Arnaud Quette <arnaud.quette@free.fr>
# 2015 - 2019 Arnaud Quette <ArnaudQuette@Eaton.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
#
# TODO:
# - Prepend sysDescription (.1.3.6.1.2.1.1.1.0) to have some more visibility
# - extend to SNMP v3 (auth.)

usage() {
	echo "Usage: $0 [options] [file]"
	echo "Options:"
	echo " -d, --dmf           -- generate DMF formatted files instead of C-style"
	echo " -h, --help          -- show this message and quit"
	echo " -n name             -- subdriver name (use natural capitalization)"
	echo " -M DIRLIST          -- colon separated list of directories to also search for MIBs"
	echo " -k                  -- keep temporary files (for debugging)"
	echo ""
	echo "mode 1: get SNMP data from a real agent"
	echo " -H host_address     -- SNMP host IP address or name"
	echo " -c community        -- SNMP v1 community name (default: public)"
	echo " -s XXXX             -- override SNMP OID entry point (sysOID). Ex: '.1.3.6.1.4.1.534.10'"
	echo ""
	echo "mode 2: get data from files (snmpwalk dumps of 'sysOID' subtree)"
	echo " -s XXXX             -- SNMP OID entry point (sysOID). Ex: '.1.3.6.1.4.1.534.6.6.7'"
	echo " file1 file2         -- read from files instead of an host (using Net SNMP)"
	echo "                        file1: numeric SNMP walk (snmpwalk -On ... <sysOID>)"
	echo "                        file2: string SNMP walk (snmpwalk -Os ... <sysOID>)"
	echo ""
	echo "mode 3: get data from 1 file (numeric snmpwalk dump of the whole SNMP tree)"
	echo "        The sysOID is extracted from the dump, and only the pointed subtree is used"
	echo "        A MIB file MUST be provided, and is used to produce the string SNMP walk"
	echo " file1               -- read from file instead of an host (using Net SNMP)"
	echo "                        file1: numeric SNMP walk (snmpwalk -On ... <sysOID>)"
	echo ""

	echo "Notes:"
	echo " For both modes, prefer to copy the specific MIB file(s) for your device in the $0 script directory"
	echo " So that it is automatically taken into account for the string name resolution of OIDs"
	echo " Otherwise, use \"-M.\" option"
	echo ""
	echo "Example:"
	echo "mode 1: $0 -H 192.168.0.1 -n mibname -c mycommunity"
	echo "mode 2: (using sysOID .1.3.6.1.4.1.534.6.6.7)"
	echo " snmpwalk -On -v1 -c mycommunity 192.168.0.1 .1.3.6.1.4.1.534.6.6.7 2>/dev/null 1> numeric-walk-file"
	echo " snmpwalk -Os -v1 -m ALL -M+. -c mycommunity 192.168.0.1 .1.3.6.1.4.1.534.6.6.7 2>/dev/null 1> string-walk-file"
	echo " $0 -s .1.3.6.1.4.1.534.6.6.7 numeric-walk-file string-walk-file"
	echo "mode 3:"
	echo " snmpwalk -On -v1 -c mycommunity 192.168.0.1 .1 2>/dev/null 1> numeric-walk-file"
	echo " $0 numeric-walk-file"
	echo ""
	echo " You may alos need to install additional packages:"
	echo " - 'snmp' package (on Debian) for the base commands (snmpget, snmpwalk, snmptranslate)"
	echo " - 'snmp-mibs-downloader' package (on Debian) to get all standard MIBs"
}

# variables
DRIVER=""
KEEP=""
HOSTNAME=""
MIBS_DIRLIST="+."
COMMUNITY="public"
DEVICE_SYSOID=""
SYSOID=""
MODE=0
DMF=0

# constants
NAME=gen-snmp-subdriver
TMPDIR="${TEMPDIR:-/tmp}"
SYSOID_NUMBER=".1.3.6.1.2.1.1.2.0"
DEBUG=`mktemp "$TMPDIR/$NAME-DEBUG.XXXXXX"`
DFL_NUMWALKFILE=`mktemp "$TMPDIR/$NAME-NUMWALK.XXXXXX"`
DFL_STRWALKFILE=`mktemp "$TMPDIR/$NAME-STRWALK.XXXXXX"`
TMP_NUMWALKFILE=`mktemp "$TMPDIR/$NAME-TMP-NUMWALK.XXXXXX"`
TMP_STRWALKFILE=`mktemp "$TMPDIR/$NAME-TMP-STRWALK.XXXXXX"`

get_snmp_data() {
	# 1) get the sysOID (points the mfr specif MIB), apart if there's an override
	if [ -z "$SYSOID" ]
	then
		SYSOID=`snmpget -On -v1 -c $COMMUNITY -Ov $HOSTNAME $SYSOID_NUMBER | cut -d' ' -f2`
		echo "sysOID retrieved: ${SYSOID}"
	else
		echo "Using the provided sysOID override ($SYSOID)"
	fi
	DEVICE_SYSOID=$SYSOID

	OID_COUNT=0
	while (test $OID_COUNT -eq 0)
	do
		# 2) get the content of the mfr specif MIB
		echo "Retrieving SNMP information. This may take some time"
		snmpwalk -On -v1 -c $COMMUNITY $HOSTNAME $SYSOID 2>/dev/null 1> $DFL_NUMWALKFILE
		snmpwalk -Os -v1 -m ALL -M$MIBS_DIRLIST -c $COMMUNITY $HOSTNAME $SYSOID 2>/dev/null 1> $DFL_STRWALKFILE

		# 3) test return value of the walk, and possibly ramp-up the path to get something.
		# The sysOID mechanism only works if we're pointed somehow in the right direction
		# i.e. doesn't work if sysOID is .1.3.6.1.4.1.705.1 and data is at .1.3.6.1.4.1.534...
		# Ex: sysOID = ".1.X.Y.Z"
		# try with ".1.X.Y.Z", if fails try with .1.X.Y", if fails try with .1.X"...
		OID_COUNT="`cat $NUMWALKFILE | wc -l`"
		if [ $OID_COUNT -eq 0 ]; then
			# ramp-up the provided sysOID by removing the last .x part
			SYSOID=${SYSOID%.*}
			echo "Warning: sysOID provided no data! Trying with a level up using $SYSOID"
		fi
	done
	return $OID_COUNT
}

generate_C() {
	# create file names
	LDRIVER=`echo $DRIVER | tr A-Z a-z`
	UDRIVER=`echo $DRIVER | tr a-z A-Z`
	CFILE="$LDRIVER-mib.c"
	HFILE="$LDRIVER-mib.h"

	#FIXME: LDRIVER & UDRIVER => replace - by _

	# generate header file
	echo "Creating $HFILE"
	cat > "$HFILE" <<-EOF
	/* ${HFILE} - subdriver to monitor ${DRIVER} SNMP devices with NUT
	 *
	 *  Copyright (C)
	 *  2011 - 2016	Arnaud Quette <arnaud.quette@free.fr>
	 *
	 *  This program is free software; you can redistribute it and/or modify
	 *  it under the terms of the GNU General Public License as published by
	 *  the Free Software Foundation; either version 2 of the License, or
	 *  (at your option) any later version.
	 *
	 *  This program is distributed in the hope that it will be useful,
	 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
	 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	 *  GNU General Public License for more details.
	 *
	 *  You should have received a copy of the GNU General Public License
	 *  along with this program; if not, write to the Free Software
	 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
	 */

	#ifndef ${UDRIVER}_MIB_H
	#define ${UDRIVER}_MIB_H

	#include "main.h"
	#include "snmp-ups.h"

	extern mib2nut_info_t ${LDRIVER};

	#endif /* ${UDRIVER}_MIB_H */
	EOF

	# generate source file
	# create header
	echo "Creating $CFILE"
	cat > "$CFILE" <<-EOF
	/* ${CFILE} - subdriver to monitor ${DRIVER} SNMP devices with NUT
	 *
	 *  Copyright (C)
	 *  2011 - 2016	Arnaud Quette <arnaud.quette@free.fr>
	 *
	 *  Note: this subdriver was initially generated as a "stub" by the
	 *  gen-snmp-subdriver script. It must be customized!
	 *
	 *  This program is free software; you can redistribute it and/or modify
	 *  it under the terms of the GNU General Public License as published by
	 *  the Free Software Foundation; either version 2 of the License, or
	 *  (at your option) any later version.
	 *
	 *  This program is distributed in the hope that it will be useful,
	 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
	 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	 *  GNU General Public License for more details.
	 *
	 *  You should have received a copy of the GNU General Public License
	 *  along with this program; if not, write to the Free Software
	 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
	 */

	#include "${HFILE}"

	#define ${UDRIVER}_MIB_VERSION  "0.1"

	#define ${UDRIVER}_SYSOID       "${DEVICE_SYSOID}"

	/* To create a value lookup structure (as needed on the 2nd line of the example
	 * below), use the following kind of declaration, outside of the present snmp_info_t[]:
	 * static info_lkp_t onbatt_info[] = {
	 * 	{ 1, "OB" },
	 * 	{ 2, "OL" },
	 * 	{ 0, NULL }
	 * };
	 */

	/* ${UDRIVER} Snmp2NUT lookup table */
	static snmp_info_t ${LDRIVER}_mib[] = {

		/* Data format:
		 * { info_type, info_flags, info_len, OID, dfl, flags, oid2info },
		 *
		 *	info_type:	NUT INFO_ or CMD_ element name
		 *	info_flags:	flags to set in addinfo
		 *	info_len:	length of strings if ST_FLAG_STRING, multiplier otherwise
		 *	OID: SNMP OID or NULL
		 *	dfl: default value
		 *	flags: snmp-ups internal flags (FIXME: ...)
		 *	oid2info: lookup table between OID and NUT values
		 *
		 * Example:
		 * { "input.voltage", 0, 0.1, ".1.3.6.1.4.1.705.1.6.2.1.2.1", "", SU_INPUT_1, NULL },
		 * { "ups.status", ST_FLAG_STRING, SU_INFOSIZE, ".1.3.6.1.4.1.705.1.7.3.0", "", SU_FLAG_OK | SU_STATUS_BATT, onbatt_info },
		 *
		 * To create a value lookup structure (as needed on the 2nd line), use the
		 * following kind of declaration, outside of the present snmp_info_t[]:
		 * static info_lkp_t onbatt_info[] = {
		 * 	{ 1, "OB" },
		 * 	{ 2, "OL" },
		 * 	{ 0, NULL }
		 * };
		 */
	EOF

	# extract OID string paths, one by one
	LINENB="0"
	while IFS= read -r line; do
		LINENB="`expr $LINENB + 1`"
		FULL_STR_OID="$line"
		STR_OID="`echo $line | cut -d'.' -f1`"
		echo $line | grep STRING > /dev/null
		if [ $? -eq 0 ]; then
			ST_FLAG_TYPE="ST_FLAG_STRING"
			SU_INFOSIZE="SU_INFOSIZE"
		else
			ST_FLAG_TYPE="0"
			SU_INFOSIZE="1"
		fi
		# get the matching numeric OID
		NUM_OID="`sed -n ${LINENB}p ${NUMWALKFILE} | cut -d' ' -f1`"
		printf "\t/* ${FULL_STR_OID} */\n\t{ \"unmapped.${STR_OID}\", ${ST_FLAG_TYPE}, ${SU_INFOSIZE}, \"${NUM_OID}\", NULL, SU_FLAG_OK, NULL },\n"
	done < ${STRWALKFILE} >> ${CFILE}

	# append footer
	cat >> "$CFILE" <<-EOF

		/* end of structure. */
		{ NULL, 0, 0, NULL, NULL, 0, NULL }
	};

	mib2nut_info_t	${LDRIVER} = { "${LDRIVER}", ${UDRIVER}_MIB_VERSION, NULL, NULL, ${LDRIVER}_mib, ${UDRIVER}_DEVICE_SYSOID };
	EOF
}

generate_DMF() {

	# create file names
	LDRIVER=`echo $DRIVER | tr A-Z a-z`
	UDRIVER=`echo $DRIVER | tr a-z A-Z`
	DMFFILE="$LDRIVER-mib.dmf"

	#FIXME: LDRIVER & UDRIVER => replace - by _

	# generate DMF file
	echo "Creating $DMFFILE"
	printf "<!-- ${DMFFILE} - Data Mapping File to monitor ${DRIVER} SNMP devices with NUT -->\n" > ${DMFFILE}
	printf "<!-- this DMF was generated automatically. It must be customized! -->\n" >> ${DMFFILE}
	printf "<?xml version=\"1.0\" ?>\n<nut>\n\t<snmp name=\"${LDRIVER}_mib\">\n" >> ${DMFFILE}
	printf "\t\t<!-- Data format: -->\n" >> ${DMFFILE}
	printf "\t\t<!-- To create a value lookup structure (as needed on the 2nd line of the example" >> ${DMFFILE}
	printf "below), use the following kind of declaration:\n" >> ${DMFFILE}
	printf "\t\t<lookup name=\"onbatt_info\">\n" >> ${DMFFILE}
	printf "\t\t\t<lookup_info oid=\"1\" value=\"OB\"/>\n" >> ${DMFFILE}
	printf "\t\t\t<lookup_info oid=\"2\" value=\"OL\"/>\n" >> ${DMFFILE}
	printf "\t\t</lookup> -->\n\n" >> ${DMFFILE}
	printf "\t\t<!-- To create a variable mapping entry, use the following kind of declaration:\n" >> ${DMFFILE}
	printf "\t\t<snmp_info multiplier=\"...\" name=\"...\" oid=\"...\" power_status=\"...\" default=\"\" static=\"yes\" string=\"yes\"/>\n\n" >> ${DMFFILE}
	printf "\t\tPossible attributes:\n" >> ${DMFFILE}
	printf "\t\t* oid: numeric SNMP OID to get data from\n" >> ${DMFFILE}
	printf "\t\t* multiplier: if present, multiply the value by this\n" >> ${DMFFILE}
	printf "\t\t* power_status=\"yes\": if present and set to \"yes\", indicates power status element\n" >> ${DMFFILE}
	printf "\t\t* battery_status=\"yes\": if present and set to \"yes\", indicates battery status element\n" >> ${DMFFILE}
	printf "\t\t* calibration=\"yes\": if present and set to \"yes\", indicates calibration status element\n" >> ${DMFFILE}
	printf "\t\t* replace_battery=\"yes\": if present and set to \"yes\", indicates replace battery status element\n" >> ${DMFFILE}
	printf "\t\t* default: the default value, if we can't retrieve the OID value\n" >> ${DMFFILE}
	printf "\t\t* static: retrieve info only once\n" >> ${DMFFILE}
	printf "\t\t* string: the value of the OID is to be processed as a string\n" >> ${DMFFILE}
	printf "\t\t* absent: data is absent in the device, use default value\n" >> ${DMFFILE}
	printf "\t\t* positive: Invalid if negative value\n" >> ${DMFFILE}
	printf "\t\t* unique: There can be only be one provider of this info, disable the other providers\n" >> ${DMFFILE}
	printf "\t\t* input_1_phase: only processed if 1 input phase\n" >> ${DMFFILE}
	printf "\t\t* input_3_phase: only processed if 3 input phase\n" >> ${DMFFILE}
	printf "\t\t* output_1_phase: only processed if 1 output phase\n" >> ${DMFFILE}
	printf "\t\t* output_3_phase: only processed if 3 output phase\n" >> ${DMFFILE}
	printf "\t\t* bypass_1_phase: only processed if 1 bypass phase\n" >> ${DMFFILE}
	printf "\t\t* bypass_1_phase: only processed if 3 bypass phase\n" >> ${DMFFILE}
	printf "\t\t* outlet: outlet template definition\n" >> ${DMFFILE}
	printf "\t\t* outlet_group: outlet group template definition\n" >> ${DMFFILE}
	printf "\t\t* command: instant command definition\n" >> ${DMFFILE}
	printf "\t\tExamples:\n" >> ${DMFFILE}
	printf "\t\t<snmp_info multiplier=\"0.1\" name=\"input.voltage\" oid=\".1.3.6.1.4.1.705.1.6.2.1.2.1\" input_1_phase=\"yes\"/>\n" >> ${DMFFILE}
	printf "\t\t<snmp_info multiplier=\"0.1\" name=\"ups.status\" oid=\".1.3.6.1.4.1.705.1.7.3.0\" string=\"yes\" battery_status=\"yes\" lookup=\"onbatt_info\"/>\n\t\t-->\n" >> ${DMFFILE}

	printf "\n\t\t<!-- To create an alarm lookup structure (as needed in the mib2nut example" >> ${DMFFILE}
	printf "below), use the following kind of declaration:\n" >> ${DMFFILE}
	printf "\t\t<alarm name=\"pw_alarms\">\n" >> ${DMFFILE}
	printf "\t\t\t<info_alarm alarm=\"...\" oid=\"...\" status=\"...\"/>\n" >> ${DMFFILE}
	printf "\t\t</alarm>\n" >> ${DMFFILE}
	printf "\t\tPossible attributes:\n" >> ${DMFFILE}
	printf "\t\t* oid: numeric SNMP OID to match\n" >> ${DMFFILE}
	printf "\t\t* alarm: if present, and different than \"None\", value to be published in *ups.alarm*\n" >> ${DMFFILE}
	printf "\t\t* status: if present, and different than \"None\", value to be published in *ups.status*\n\n" >> ${DMFFILE}
	printf "\t\tExamples:\n" >> ${DMFFILE}
	printf "\t\t<alarm name=\"pw_alarms\">\n" >> ${DMFFILE}
	printf "\t\t\t<info_alarm alarm=\"None\" oid=\"1.3.6.1.4.1.534.1.7.4\" status=\"LB\"/>\n" >> ${DMFFILE}
	printf "\t\t\t<info_alarm alarm=\"Output overload!\" oid=\".1.3.6.1.4.1.534.1.7.7\" status=\"OVER\"/>\n" >> ${DMFFILE}
	printf "\t\t\t<info_alarm alarm=\"Internal failure!\" oid=\".1.3.6.1.4.1.534.1.7.8\" status=\"None\"/>\n" >> ${DMFFILE}
	printf "\t\t</alarm>\n\t\t-->\n" >> ${DMFFILE}
	
	# extract OID string paths, one by one
	LINENB="0"
	while IFS= read -r line; do
		LINENB="`expr $LINENB + 1`"
		FULL_STR_OID="$line"
		STR_OID="`echo $line | cut -d'.' -f1`"
		echo $line | grep STRING > /dev/null
		if [ $? -eq 0 ]; then
			ST_FLAG_TYPE="ST_FLAG_STRING"
			SU_INFOSIZE="SU_INFOSIZE"
		else
			ST_FLAG_TYPE="0"
			SU_INFOSIZE="1"
		fi
		# get the matching numeric OID
		NUM_OID="`sed -n ${LINENB}p ${NUMWALKFILE} | cut -d' ' -f1`"
		printf "\t\t<!-- ${FULL_STR_OID} -->\n\t\t<snmp_info name=\"unmapped.${STR_OID}\", oid=\"${NUM_OID}\", default=\"\"/>\n"
	done < ${STRWALKFILE} >> ${DMFFILE}

	# append footer
	# FIXME: missing license field in mib2nut
	printf "\t</snmp>\n\t<mib2nut auto_check=\"\" mib_name=\"${LDRIVER}_mib\" name=\"${LDRIVER}_mib\" oid=\"${DEVICE_SYSOID}\" snmp_info=\"${LDRIVER}_mib\" version=\"0.1\"/>\n" >> "$DMFFILE"

	printf "\n\t<!-- Data format: -->\n" >> ${DMFFILE}
	printf "\t<!-- To create a MIB mapping entry, use the following kind of declaration:\n" >> ${DMFFILE}
	printf "\t<mib2nut alarms_info=\"...\" auto_check=\"...\" mib_name=\"...\" name=\"...\" oid=\".1.3.6.1.4.1.534.1\" snmp_info=\"...\" version=\"...\"/>" >> ${DMFFILE}
	printf "\tPossible attributes:\n" >> ${DMFFILE}
	printf "\t* oid: sysOID to match to use the present MIB\n" >> ${DMFFILE}
	printf "\t* alarms_info: alarm lookup structure to use, to resolve status and alarms\n" >> ${DMFFILE}
	printf "\t* auto_check: OID to counter check if the present MIB matches the device\n" >> ${DMFFILE}
	printf "\t* mib_name: internal name of the DMF structure\n" >> ${DMFFILE}
	printf "\t* name: friendly name of the DMF structure\n" >> ${DMFFILE}
	printf "\t* snmp_info: snmp_info structure to use\n" >> ${DMFFILE}
	printf "\t* version: version of the present mib2nut structure\n" >> ${DMFFILE}
	printf "\tExamples:\n" >> ${DMFFILE}
	printf "\t<mib2nut alarms_info=\"pw_alarms\" auto_check=\"1.3.6.1.4.1.534.1.1.2.0\" mib_name=\"pw\" name=\"powerware\" oid=\".1.3.6.1.4.1.534.1\" snmp_info=\"pw_mib\" version=\"0.88\"/>\n\t-->\n" >> ${DMFFILE}

	printf "</nut>\n" >> "$DMFFILE"

}

# process command line options
while [ $# -gt 0 ]; do
	if [ $# -gt 1 -a "$1" = "-n" ]; then
		DRIVER="$2"
		shift 2
	elif [ $# -gt 1 -a "$1" = "-M" ]; then
		MIBS_DIRLIST="$MIBS_DIRLIST:$2"
		shift 2
	elif [ "$1" = "-d" ]; then
		DMF=1
		shift
	elif [ "$1" = "-k" ]; then
		KEEP=yes
		shift
	elif [ $# -gt 1 -a "$1" = "-H" ]; then
		HOSTNAME="$2"
		shift 2
	elif [ $# -gt 1 -a "$1" = "-c" ]; then
		COMMUNITY="$2"
		shift 2
	elif [ $# -gt 1 -a "$1" = "-s" ]; then
		SYSOID="$2"
		shift 2
	elif echo "$1" | grep -qv '^-'; then
		if [ $# -gt 1 ]; then
			NUMWALKFILE="$1"
			shift
			STRWALKFILE="$1"
			shift
		else
			NUMWALKFILE="$1"
			shift
			#usage
			#exit 1
		fi
	elif [ "$1" = "--help" -o "$1" = "-h" ]; then
		usage
		exit 0
	else
		echo "Illegal option $1. Try --help for more info." >&2
		exit 1
	fi
done

# check that the needed parameters are provided, depending on the mode
if [ -z "$NUMWALKFILE" ]; then
	# mode 1: directly get SNMP data from a real agent
	echo "Mode 1 selected"
	MODE=1
	NUMWALKFILE=$DFL_NUMWALKFILE
	STRWALKFILE=$DFL_STRWALKFILE

	# check if Net SNMP is available
	if [ -z "`which snmpget`" -o -z "`which snmpwalk`" ]; then
		echo "Net SNMP not found! snmpget and snmpwalk commands are required." >&2
		exit 1
	fi
	# hostname is also mandatory
	while [ -z "$HOSTNAME" ]; do
		printf "\n\tPlease enter the SNMP host IP address or name.\n"
		read -p "SNMP host IP name or address: " HOSTNAME < /dev/tty
		if echo $HOSTNAME | egrep -q '[^a-zA-Z0-9]'; then
			echo "Please use only letters and digits"
			HOSTNAME=""
		fi
	done
	# get data from the agent
	get_snmp_data
else
	# no string walk provided, so mode 3
	if [ -z "$STRWALKFILE" ]; then
		# mode 3: get data from 1 file,
		# Filter according to sysOID on the specific subtree
		# Generate the numeric SNMP walk using this output
		# then use snmptranslate to get the string OIDs and generated the string SNMP walk
		echo "Mode 3 selected"
		MODE=3
		RAWWALKFILE=$NUMWALKFILE
		NUMWALKFILE=$DFL_NUMWALKFILE
		STRWALKFILE=$DFL_STRWALKFILE

		# check for actual file existence
		if [ ! -f "$RAWWALKFILE" ]; then
			echo "SNMP walk dump file is missing on disk. Try --help for more info." >&2
			exit 1
		fi
		# Extract the sysOID
		# Format is "1.3.6.1.2.1.1.2.0 = OID: 1.3.6.1.4.1.4555.1.1.1"
		DEVICE_SYSOID=`grep 1.3.6.1.2.1.1.2.0 $RAWWALKFILE | cut -d' ' -f4`
		if [ -n "$DEVICE_SYSOID" ]; then
			echo "Found sysOID $DEVICE_SYSOID"
		else
			echo "SNMP sysOID is missing in file. Try --help for more info." >&2
			exit 1
		fi

		# Switch to the entry point, and extract the subtree
		# Extract the numeric walk
		echo -n "Extracting numeric SNMP walk..."
		grep $DEVICE_SYSOID $RAWWALKFILE | egrep -v "1.3.6.1.2.1.1.2.0" 2>/dev/null 1> $NUMWALKFILE
		echo " done"

		# Create the string walk from a translation of the numeric one
		echo -n "Converting string SNMP walk..."
		while IFS=' = ' read NUM_OID OID_VALUE
		do
			STR_OID=`snmptranslate -Os  -m ALL -M+. $NUM_OID 2>/dev/null`
			echo "$STR_OID = $OID_VALUE" >> $STRWALKFILE
		done < $NUMWALKFILE
		echo " done"
	else
		# mode 2: get data from files
		echo "Mode 2 selected"
		MODE=2

		# get sysOID value from command line, if needed
		while [ -z "$SYSOID" ]; do
			echo "
Please enter the value of sysOID, as displayed by snmp-ups. For example '.1.3.6.1.4.1.2254.2.4'.
You can get it using: snmpget -v1 -c XXX <host> $SYSOID_NUMBER"
			read -p "Value of sysOID: " SYSOID < /dev/tty
			if echo $SYSOID | egrep -q '[^0-9.]'; then
				echo "Please use only the numeric form, with dots and digits"
				SYSOID=""
			fi
		done
		# check for actual files existence
		if [ ! -f "$NUMWALKFILE" -o  ! -f "$STRWALKFILE" ]; then
			echo "SNMP walk dump files are missing on disk. Try --help for more info." >&2
			exit 1
		fi
	fi
fi

# delete temporary files: this is called just before exiting.
cleanup () {
	rm -f "$DEBUG $DFL_NUMWALKFILE $TMP_NUMWALKFILE $DFL_STRWALKFILE $TMP_STRWALKFILE"
}
if [ -n "$KEEP" ]; then
	trap cleanup EXIT
fi

# prompt use for name of driver
while [ -z "$DRIVER" ]; do
	echo "
Please enter a name for this driver. Use only letters and numbers. Use
natural (upper- and lowercase) capitalization, e.g., 'Belkin', 'APC'."
	read -p "Name of subdriver: " DRIVER < /dev/tty
	if echo $DRIVER | egrep -q '[^a-zA-Z0-9]'; then
		echo "Please use only letters and digits"
		DRIVER=""
	fi
done

# remove blank and "End of MIB" lines
egrep -e "^[[:space:]]?$" -e "End of MIB" -v ${NUMWALKFILE} > ${TMP_NUMWALKFILE}
egrep -e "^[[:space:]]?$" -e "End of MIB" -v ${STRWALKFILE} > ${TMP_STRWALKFILE}
NUMWALKFILE=${TMP_NUMWALKFILE}
STRWALKFILE=${TMP_STRWALKFILE}

# FIXME: sanity checks (! -z contents -a same `wc -l`)
NUM_OID_COUNT="`cat $NUMWALKFILE | wc -l`"
STR_OID_COUNT="`cat $STRWALKFILE | wc -l`"

echo "COUNT = $NUM_OID_COUNT / $NUM_OID_COUNT"

generate_C
if [ "$DMF" -eq 1 ]; then
	generate_DMF
fi

# Display the remaining tasks
cat <<EOF
Done.

For C-style integration, do not forget to:
* bump DRIVER_VERSION in snmp-ups.c (add "0.01")
* copy "${HFILE}" and "${CFILE}" to "../../drivers"
* add #include "${HFILE}" to drivers/snmp-ups.c
* add &${LDRIVER} to drivers/snmp-ups.c:mib2nut[] list,
* add ${LDRIVER}-mib.c to snmp_ups_SOURCES in drivers/Makefile.am
* add ${LDRIVER}-mib.h to dist_noinst_HEADERS in drivers/Makefile.am
* "./autogen.sh && ./configure && make" from the top level directory

If otherwise using DMF, ensure that 'mib2nut' information are present in
snmp-discovery.dmf
EOF
