#!/bin/bash

# an auxiliary script to produce a "stub" usbhid-ups subdriver from
# the output of
#
# drivers/usbhid-ups -DD -u root -x generic -x vendorid=XXXX auto
#
# Usage: cat debuginfo | path-to-subdriver.sh
# 
# See also: docs/hid-subdrivers.txt

usage() {
    echo "Usage: $0 [options] [file]"
    echo "Options:"
    echo " -h, --help           -- show this message and quit"
    echo " -n name              -- driver name (use natural capitalization)"
    echo " -v XXXX              -- vendor id"
    echo " -p XXXX              -- product id"
    echo " -k                   -- keep temporary files (for debugging)"
    echo " file                 -- read from file instead of stdin"
}

DRIVER=""
VENDORID=""
PRODUCTID=""
KEEP=""

while [ $# -gt 0 ]; do
    if [ $# -gt 1 -a "$1" = "-n" ]; then
        DRIVER="$2"
        shift 2
    elif [ $# -gt 1 -a "$1" = "-v" ]; then
        VENDORID="$2"
        shift 2
    elif [ $# -gt 1 -a "$1" = "-p" ]; then
        PRODUCTID="$2"
        shift 2
    elif [ "$1" = "-k" ]; then
        KEEP=yes
        shift
    elif echo "$1" | grep -qv '^-'; then
	FILE="$1"
	shift
    elif [ "$1" = "--help" -o "$1" = "-h" ]; then
        usage
        exit 0
    else
        echo "Illegal option $1. Try --help for more info." >&2
        exit 1
    fi
done

# delete temporary files: this is called just before exiting.
cleanup () {
    rm -f "$DEBUG" "$UTABLE" "$USAGES" "$SUBST" "$SEDFILE" "$NEWUTABLE"
}
if [ -z "$KEEP" ]; then
    trap cleanup EXIT
fi

NAME=path-to-subdriver
TMPDIR="${TEMPDIR:-/tmp}"
DEBUG=`mktemp "$TMPDIR/$NAME-DEBUG.XXXXXX"`
UTABLE=`mktemp "$TMPDIR/$NAME-UTABLE.XXXXXX"`
USAGES=`mktemp "$TMPDIR/$NAME-USAGES.XXXXXX"`
SUBST=`mktemp "$TMPDIR/$NAME-SUBST.XXXXXX"`
SEDFILE=`mktemp "$TMPDIR/$NAME-SEDFILE.XXXXXX"`
NEWUTABLE=`mktemp "$TMPDIR/$NAME-NEWUTABLE.XXXXXX"`

# save standard input to a file
if [ -z "$FILE" ]; then
    FILE="$DEBUG"
    cat > "$DEBUG"
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

# try to determine product and vendor id
VENDORID=`cat "$FILE" | sed -n 's/[> ]*- VendorID: \([0-9a-fA-F]*\).*/\1/p' | tail -1`
PRODUCTID=`cat "$FILE" | sed -n 's/[> ]*- ProductID: \([0-9a-fA-F]*\).*/\1/p' | tail -1`

# prompt for productid, vendorid if necessary
if [ -z "$VENDORID" ]; then
    read -p "Vendor ID: " VENDORID < /dev/tty
fi
if [ -z "$PRODUCTID" ]; then
    read -p "Product ID: " PRODUCTID < /dev/tty
fi

LDRIVER=`echo $DRIVER | tr A-Z a-z`
UDRIVER=`echo $DRIVER | tr a-z A-Z`
CFILE="$LDRIVER-hid.c"
HFILE="$LDRIVER-hid.h"

# extract Usage Table
cat "$FILE" | sed -n 's/[> ]*Path: \([^,][^,]*\), Type:.*/\1/p' > "$UTABLE"

# extract Usage codes
cat "$UTABLE" | tr '.' $'\n' | sort -u > "$USAGES"

# make up dummy names for unknown usages
count=0
cat "$USAGES" | egrep '[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]' |\
while read U; do
    count=`expr $count + 1`
    echo "$U $UDRIVER$count"
done > "$SUBST"

# create an sed script for substitutions
cat "$SUBST" | sed 's/\(.*\) \(.*\)/s!\1!\2!g;/' > "$SEDFILE"

# create modified usage table
sed -f "$SEDFILE" < "$UTABLE" > "$NEWUTABLE"

# generate header file
echo "Creating $HFILE"
cat > "$HFILE" <<EOF
/* ${HFILE} - subdriver to monitor ${DRIVER} USB/HID devices with NUT
 *
 *  Copyright (C)
 *  2003 - 2005 Arnaud Quette <arnaud.quette@free.fr>
 *  2005 - 2006 Peter Selinger <selinger@users.sourceforge.net>         
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
 *
 */

#ifndef ${UDRIVER}_HID_H
#define ${UDRIVER}_HID_H

#include "usbhid-ups.h"

#define ${UDRIVER}_HID_VERSION	"${DRIVER} HID 0.1"

extern subdriver_t ${LDRIVER}_subdriver;

#endif /* ${UDRIVER}_HID_H */
EOF

# generate source file
echo "Creating $CFILE"
cat > "$CFILE" <<EOF
/* ${CFILE} - subdriver to monitor ${DRIVER} USB/HID devices with NUT
 *
 *  Copyright (C)
 *  2003 - 2005 Arnaud Quette <arnaud.quette@free.fr>
 *  2005 - 2006 Peter Selinger <selinger@users.sourceforge.net>         
 *
 *  Note: this subdriver was initially generated as a "stub" by the
 *  path-to-subdriver script. It must be customized.
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
 *
 */

#include "usbhid-ups.h"
#include "${HFILE}"
#include "extstate.h" /* for ST_FLAG_STRING */
#include "dstate.h"   /* for STAT_INSTCMD_HANDLED */
#include "main.h"     /* for getval() */
#include "common.h"

#define ${UDRIVER}_HID_VERSION      "${DRIVER} HID 0.1"

#define ${UDRIVER}_VENDORID 0x${VENDORID}

/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* ${UDRIVER} usage table */
static usage_lkp_t ${LDRIVER}_usage_lkp[] = {
EOF

cat "$SUBST" | sed 's/\(.*\) \(.*\)/\t{ "\2",\t0x\1 },/' >> "$CFILE"

cat >> "$CFILE" <<EOF
	{  "\0", 0x0 }
};

static usage_tables_t ${LDRIVER}_utab[] = {
	${LDRIVER}_usage_lkp,
	hid_usage_lkp,
	NULL,
};

/* --------------------------------------------------------------- */
/* HID2NUT lookup table                                            */
/* --------------------------------------------------------------- */

static hid_info_t ${LDRIVER}_hid2nut[] = {

EOF

cat "$NEWUTABLE" | while read U; do
    UL=`echo $U | tr A-Z a-z`
    cat >> "$CFILE" <<EOF
  { "unmapped.${UL}", 0, 0, "${U}", NULL, "%.0f", HU_FLAG_OK, NULL },
EOF
done

cat >> "$CFILE" <<EOF

  /* end of structure. */
  { NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

/* shutdown method for ${DRIVER} */
static int ${LDRIVER}_shutdown(int ondelay, int offdelay) {
	/* FIXME: ondelay, offdelay currently not used */
	
	/* Default method */
	upsdebugx(2, "Trying load.off.");
	if (instcmd("load.off", NULL) == STAT_INSTCMD_HANDLED) {
		return 1;
	}
	upsdebugx(2, "Shutdown failed.");
	return 0;
}

static char *${LDRIVER}_format_model(HIDDevice *hd) {
	return hd->Product;
}

static char *${LDRIVER}_format_mfr(HIDDevice *hd) {
	return hd->Vendor ? hd->Vendor : "${DRIVER}";
}

static char *${LDRIVER}_format_serial(HIDDevice *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int ${LDRIVER}_claim(HIDDevice *hd) {
	if (hd->VendorID != ${UDRIVER}_VENDORID) {
		return 0;
	}
	switch (hd->ProductID) {

	/* accept any known UPS - add devices here as needed */
	case ${PRODUCTID}:
		return 1;

	/* by default, reject, unless the productid option is given */
	default:
		if (getval("productid")) {
			return 1;
		} else {
			upsdebugx(1,
"This ${DRIVER} device (%04x/%04x) is not (or perhaps not yet) supported\n"
"by usbhid-ups. Please make sure you have an up-to-date version of NUT. If\n"
"this does not fix the problem, try running the driver with the\n"
"'-x productid=%04x' option. Please report your results to the NUT user's\n"
"mailing list <nut-upsuser@lists.alioth.debian.org>.\n",
						 hd->VendorID, hd->ProductID, hd->ProductID);
			return 0;
		}
	}
}

subdriver_t ${LDRIVER}_subdriver = {
	${UDRIVER}_HID_VERSION,
	${LDRIVER}_claim,
	${LDRIVER}_utab,
	${LDRIVER}_hid2nut,
	${LDRIVER}_shutdown,
	${LDRIVER}_format_model,
	${LDRIVER}_format_mfr,
	${LDRIVER}_format_serial,
};
EOF

cat <<EOF
Done. 

Do not forget to:
* add #include "${HFILE}" to usbhid-ups.c, 
* add &${LDRIVER}_subdriver to usbhid-ups.c:subdriver_list,
* add ${LDRIVER}-hid.o to USBHID_UPS_SUBDRIVERS in drivers/Makefile.am
* "autoreconf" from the top level directory
* "make depend" in drivers/
EOF


