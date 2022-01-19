#!/bin/bash

# an auxiliary script to produce a "stub" usbhid-ups subdriver from
# the output of
#
# drivers/usbhid-ups -DD -u root -x generic -x vendorid=XXXX auto
#
# Usage: cat debuginfo | gen-usbhid-subdriver.sh
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

NAME=gen-usbhid-subdriver
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
VENDORID=`cat "$FILE" | sed -n 's/.*- VendorID: \([0-9a-fA-F]*\).*/\1/p' | tail -1`
PRODUCTID=`cat "$FILE" | sed -n 's/.*- ProductID: \([0-9a-fA-F]*\).*/\1/p' | tail -1`

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
cat "$FILE" | sed -n 's/.*Path: \([^,][^,]*\), Type:.*/\1/p' > "$UTABLE"

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
 *  2003 - 2009	Arnaud Quette <ArnaudQuette@Eaton.com>
 *  2005 - 2006	Peter Selinger <selinger@users.sourceforge.net>
 *  2008 - 2009	Arjen de Korte <adkorte-guest@alioth.debian.org>
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

#ifndef ${UDRIVER}_HID_H
#define ${UDRIVER}_HID_H

#include "usbhid-ups.h"

extern subdriver_t ${LDRIVER}_subdriver;

#endif /* ${UDRIVER}_HID_H */
EOF

# generate source file
echo "Creating $CFILE"
cat > "$CFILE" <<EOF
/* ${CFILE} - subdriver to monitor ${DRIVER} USB/HID devices with NUT
 *
 *  Copyright (C)
 *  2003 - 2012	Arnaud Quette <ArnaudQuette@Eaton.com>
 *  2005 - 2006	Peter Selinger <selinger@users.sourceforge.net>
 *  2008 - 2009	Arjen de Korte <adkorte-guest@alioth.debian.org>
 *  2013 Charles Lepple <clepple+nut@gmail.com>
 *
 *  TODO: Add year and name for new subdriver author (contributor)
 *  Mention in docs/acknowledgements.txt if this is a vendor contribution
 *
 *  Note: this subdriver was initially generated as a "stub" by the
 *  gen-usbhid-subdriver script. It must be customized.
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

#include "usbhid-ups.h"
#include "${HFILE}"
#include "main.h"	/* for getval() */
#include "usb-common.h"

#define ${UDRIVER}_HID_VERSION	"${DRIVER} HID 0.1"
/* FIXME: experimental flag to be put in upsdrv_info */

/* ${DRIVER} */
#define ${UDRIVER}_VENDORID	0x${VENDORID}

/* USB IDs device table */
static usb_device_id_t ${LDRIVER}_usb_device_table[] = {
	/* ${DRIVER} */
	{ USB_DEVICE(${UDRIVER}_VENDORID, 0x${PRODUCTID}), NULL },

	/* Terminating entry */
	{ 0, 0, NULL }
};


/* --------------------------------------------------------------- */
/*      Vendor-specific usage table */
/* --------------------------------------------------------------- */

/* ${UDRIVER} usage table */
static usage_lkp_t ${LDRIVER}_usage_lkp[] = {
EOF

cat "$SUBST" | sed 's/\(.*\) \(.*\)/\t{ "\2",\t0x\1 },/' >> "$CFILE"

cat >> "$CFILE" <<EOF
	{ NULL, 0 }
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

cat "$NEWUTABLE" | sort -u | while read U; do
    UL=`echo $U | tr A-Z a-z`
    cat >> "$CFILE" <<EOF
	{ "unmapped.${UL}", 0, 0, "${U}", NULL, "%.0f", 0, NULL },
EOF
done

cat >> "$CFILE" <<EOF

	/* end of structure. */
	{ NULL, 0, 0, NULL, NULL, NULL, 0, NULL }
};

static const char *${LDRIVER}_format_model(HIDDevice_t *hd) {
	return hd->Product;
}

static const char *${LDRIVER}_format_mfr(HIDDevice_t *hd) {
	return hd->Vendor ? hd->Vendor : "${DRIVER}";
}

static const char *${LDRIVER}_format_serial(HIDDevice_t *hd) {
	return hd->Serial;
}

/* this function allows the subdriver to "claim" a device: return 1 if
 * the device is supported by this subdriver, else 0. */
static int ${LDRIVER}_claim(HIDDevice_t *hd)
{
	int status = is_usb_device_supported(${LDRIVER}_usb_device_table, hd);

	switch (status)
	{
	case POSSIBLY_SUPPORTED:
		/* by default, reject, unless the productid option is given */
		if (getval("productid")) {
			return 1;
		}
		possibly_supported("${DRIVER}", hd);
		return 0;

	case SUPPORTED:
		return 1;

	case NOT_SUPPORTED:
	default:
		return 0;
	}
}

subdriver_t ${LDRIVER}_subdriver = {
	${UDRIVER}_HID_VERSION,
	${LDRIVER}_claim,
	${LDRIVER}_utab,
	${LDRIVER}_hid2nut,
	${LDRIVER}_format_model,
	${LDRIVER}_format_mfr,
	${LDRIVER}_format_serial,
};
EOF

cat <<EOF
Done.

Do not forget to:
* add #include "${HFILE}" to drivers/usbhid-ups.c,
* add &${LDRIVER}_subdriver to drivers/usbhid-ups.c:subdriver_list,
* add ${LDRIVER}-hid.c to USBHID_UPS_SUBDRIVERS in drivers/Makefile.am
* add ${LDRIVER}-hid.h to dist_noinst_HEADERS in drivers/Makefile.am
* "autoreconf" from the top level directory
EOF
