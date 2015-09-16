#!/bin/bash
#
# an auxiliary script to produce a "stub" snmp-ups subdriver from
# SNMP data from a real agent or from dump files
#
# Version: 0.5
#
# See also: docs/snmp-subdrivers.txt
#
# Copyright (C)
# 2011 - 2012 Arnaud Quette <arnaud.quette@free.fr>
# 2015        Arnaud Quette <ArnaudQuette@Eaton.com>
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
# - extend to SNMP v3 (auth.)

usage() {
    echo "Usage: $0 [options] [file]"
    echo "Options:"
    echo " -h, --help          -- show this message and quit"
    echo " -n name             -- subdriver name (use natural capitalization)"
    echo " -M DIRLIST          -- colon separated list of directories to also search for MIBs"
    echo " -k                  -- keep temporary files (for debugging)"
    echo ""
    echo "mode 1: get SNMP data from a real agent"
    echo " -H host_address     -- SNMP host IP address or name"
    echo " -c community        -- SNMP v1 community name (default: public)"
    echo ""
    echo "mode 2: get data from files (snmpwalk dumps of 'sysOID' subtree)"
    echo " -s XXXX             -- SNMP OID entry point (sysOID). Ex: '.1.3.6.1.4.1.534.6.6.7'"
    echo " file1 file2         -- read from files instead of an host (using Net SNMP)"
    echo "                        file1: numeric SNMP walk (snmpwalk -On ... <sysOID>)"
    echo "                        file2: string SNMP walk (snmpwalk -Os ... <sysOID>)"
    echo ""
    echo "Notes:"
    echo " For both modes, prefer to copy the specific MIB file(s) for your device in the $0 script directory"
    echo " In such case, for mode 2, also add \"-M.\" to allow the name resolution of OIDs"
    echo ""
    echo "Example:"
    echo "mode 1: $0 -H 192.168.0.1 -n mibname -c mycommunity"
    echo "mode 2: (using sysOID .1.3.6.1.4.1.534.6.6.7)"
    echo " snmpwalk -On -v1 -c mycommunity 192.168.0.1 .1.3.6.1.4.1.534.6.6.7 2>/dev/null 1> numeric-walk-file"
    echo " snmpwalk -Os -v1 -m ALL -M+. -c mycommunity 192.168.0.1 .1.3.6.1.4.1.534.6.6.7 2>/dev/null 1> string-walk-file"
    echo " $0 -s .1.3.6.1.4.1.534.6.6.7 numeric-walk-file string-walk-file"
}

# variables
DRIVER=""
KEEP=""
HOSTNAME=""
MIBS_DIRLIST="+."
COMMUNITY="public"
SYSOID=""
MODE=0

# constants
NAME=gen-snmp-subdriver
TMPDIR="${TEMPDIR:-/tmp}"
DEBUG=`mktemp "$TMPDIR/$NAME-DEBUG.XXXXXX"`
DFL_NUMWALKFILE=`mktemp "$TMPDIR/$NAME-NUMWALK.XXXXXX"`
DFL_STRWALKFILE=`mktemp "$TMPDIR/$NAME-STRWALK.XXXXXX"`
TMP_NUMWALKFILE=`mktemp "$TMPDIR/$NAME-TMP-NUMWALK.XXXXXX"`
TMP_STRWALKFILE=`mktemp "$TMPDIR/$NAME-TMP-STRWALK.XXXXXX"`

get_snmp_data() {
    # 1) get the sysOID (points the mfr specif MIB)
    SYSOID=`snmpget -On -v1 -c $COMMUNITY -Ov $HOSTNAME .1.3.6.1.2.1.1.2.0 | cut -d' ' -f2`

	echo "sysOID retrieved: ${SYSOID}"

    # 2) get the content of the mfr specif MIB
    echo "Retrieving SNMP information. This may take some time"
    snmpwalk -On -v1 -c $COMMUNITY $HOSTNAME $SYSOID 2>/dev/null 1> $DFL_NUMWALKFILE
    snmpwalk -Os -v1 -m ALL -M $MIBS_DIRLIST -c $COMMUNITY $HOSTNAME $SYSOID 2>/dev/null 1> $DFL_STRWALKFILE
}

# process command line options
while [ $# -gt 0 ]; do
    if [ $# -gt 1 -a "$1" = "-n" ]; then
        DRIVER="$2"
        shift 2
    elif [ $# -gt 1 -a "$1" = "-M" ]; then
        MIBS_DIRLIST="$MIBS_DIRLIST:$2"
        shift 2
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
            usage
            exit 1
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
        echo "
    Please enter the SNMP host IP address or name."
        read -p "SNMP host IP name or address: " HOSTNAME < /dev/tty
        if echo $HOSTNAME | egrep -q '[^a-zA-Z0-9]'; then
            echo "Please use only letters and digits"
            HOSTNAME=""
        fi
    done
    # get data from the agent
    get_snmp_data
else
    # mode 2: get data from files
    MODE=2

    # get sysOID value from command line, if needed
    while [ -z "$SYSOID" ]; do
        echo "
Please enter the value of sysOID, as displayed by snmp-ups. For example '.1.3.6.1.4.1.2254.2.4'.
You can get it using: snmpget -v1 -c XXX <host> .1.3.6.1.2.1.1.2.0"
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

# create file names
LDRIVER=`echo $DRIVER | tr A-Z a-z`
UDRIVER=`echo $DRIVER | tr a-z A-Z`
CFILE="$LDRIVER-mib.c"
HFILE="$LDRIVER-mib.h"

# generate header file
echo "Creating $HFILE"
cat > "$HFILE" <<EOF
/* ${HFILE} - subdriver to monitor ${DRIVER} SNMP devices with NUT
 *
 *  Copyright (C)
 *  2011 - 2012	Arnaud Quette <arnaud.quette@free.fr>
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
cat > "$CFILE" <<EOF
/* ${CFILE} - subdriver to monitor ${DRIVER} SNMP devices with NUT
 *
 *  Copyright (C)
 *  2011 - 2012	Arnaud Quette <arnaud.quette@free.fr>
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

#define ${UDRIVER}_SYSOID       "${SYSOID}"

/* To create a value lookup structure (as needed on the 2nd line of the example
 * below), use the following kind of declaration, outside of the present snmp_info_t[]:
 * static info_lkp_t onbatt_info[] = {
 * 	{ 1, "OB" },
 * 	{ 2, "OL" },
 * 	{ 0, "NULL" }
 * };
 */

/* ${UDRIVER} Snmp2NUT lookup table */
static snmp_info_t ${LDRIVER}_mib[] = {

	/* Data format:
	 * { info_type, info_flags, info_len, OID, dfl, flags, oid2info, setvar },
	 *
	 *	info_type:	NUT INFO_ or CMD_ element name
	 *	info_flags:	flags to set in addinfo
	 *	info_len:	length of strings if STR
	 *				cmd value if CMD, multiplier otherwise
	 *	OID: SNMP OID or NULL
	 *	dfl: default value
	 *	flags: snmp-ups internal flags (FIXME: ...)
	 *	oid2info: lookup table between OID and NUT values
	 *	setvar: variable to set for SU_FLAG_SETINT
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
	 * 	{ 0, "NULL" }
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
cat >> "$CFILE" <<EOF

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, 0, NULL }
};

mib2nut_info_t	${LDRIVER} = { "${LDRIVER}", ${UDRIVER}_MIB_VERSION, NULL, NULL, ${LDRIVER}_mib, ${UDRIVER}_SYSOID };
EOF

# Display the remaining tasks
cat <<EOF
Done.

Do not forget to:
* bump DRIVER_VERSION in snmp-ups.c (add "0.01")
* copy "${HFILE}" and "${CFILE}" to "../../drivers"
* add #include "${HFILE}" to drivers/snmp-ups.c
* add &${LDRIVER} to drivers/snmp-ups.c:mib2nut[] list,
* add ${LDRIVER}-mib.c to snmp_ups_SOURCES in drivers/Makefile.am
* add ${LDRIVER}-mib.h to dist_noinst_HEADERS in drivers/Makefile.am
* "./autogen.sh && ./configure && make" from the top level directory
EOF
