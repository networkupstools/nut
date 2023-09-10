#!/bin/sh

# Helper script to generate DOCINFO document revision list for NUT PDF docs.
# Primarily used by maintainers as part of a NUT release preparation -
# see docs/maintainer-guide.txt for more details.
# Note that relying on git tags alone is a chicken-and-egg problem here
# (the updated file should be part of a new release), so it rather helps
# catch up with missed entries.
# Copyright (C) 2023 by Jim Klimov <jimklimov+nut@gmail.com>
# Licensed under GPLv2+ terms

SCRIPTDIR="`dirname "$0"`" \
&& SCRIPTDIR="`cd "${SCRIPTDIR}" && pwd`" \
&& [ -n "${SCRIPTDIR}" ] && [ -d "${SCRIPTDIR}" ] \
|| exit

[ -n "${DOCINFO_XML-}" ] || DOCINFO_XML="${SCRIPTDIR}/docinfo.xml.in"

generate_tags() {
    # NUT v2.6.0 is the oldest release with asciidoc rendered into PDF
    for RELTAG in `git tag -l 'v*' --contains v2.6.0 | grep -E '^v[0-9]+\.[0-9]+\.[0-9]+$' | sed -e 's,-.*$,,' | sort -nr | uniq` ; do
        NUT_RELEASE="`echo "$RELTAG" | sed -e 's,^v,,'`"
        grep "<revnumber>${NUT_RELEASE}</revnumber>" "${DOCINFO_XML}" >/dev/null 2>&1 && continue

        NUT_RELEASER="`git log -1 --pretty=format:'%cn' "${RELTAG}" | tr -d 'a-z '`"
        # Beware to not use "|" in formatting below! Used for sed magic later.
        git log -1 --pretty=tformat:'  <revision>%n    <revnumber>@NUT_RELEASE@</revnumber>%n    <date>%cs</date>%n    <authorinitials>@NUT_RELEASER@</authorinitials>%n    <revremark></revremark>%n  </revision>%n' \
            "${RELTAG}" \
        | sed \
            -e 's,@NUT_RELEASE@,'"${NUT_RELEASE}"',' \
            -e 's,@NUT_RELEASER@,'"${NUT_RELEASER}"','
    done
}

NEWTAGS="`generate_tags`"
[ -n "$NEWTAGS" ] || { echo "SKIP: No new releases found (without a record in DOCINFO)" >&2; exit 0; }

#echo "TODO: Add the following DOCINFO tags:" >&2
#echo "NEWTAGS: $NEWTAGS" >&2

# Have to hide and un-hide EOLs (via "|") and escape
# the leading space to enforce it in "sed a" command
# (it eats whitespace otherwise).
echo "INJECTING to DOCINFO_XML:" >&2
NEWTAGS_SED="`echo "$NEWTAGS" | tr '\n' '|' | sed -e 's,\([/<>]\),\\1,g' -e 's,^ ,\\\\ ,'`" || exit
echo "NEWTAGS_SED: $NEWTAGS_SED" >&2

sed -e '/<!-- AUTOINSERT LOCATION -->/a'"$NEWTAGS_SED" \
    < "${DOCINFO_XML}" \
    | tr '|' '\n' \
    > "${DOCINFO_XML}.tmp"

diff -bu "${DOCINFO_XML}" "${DOCINFO_XML}.tmp"

echo "Was the change acceptable? Press Y to modify the original '${DOCINFO_XML}' file [Y/N]" >&2
read LINE
case "$LINE" in
    y|Y|yes|YES)
        mv -f "${DOCINFO_XML}.tmp" "${DOCINFO_XML}"
        echo "You may want to: git add -p `basename "${DOCINFO_XML}"`" >&2
        echo "...and: make `basename "${DOCINFO_XML}" .in`" >&2
        ;;
    *) echo "NOT APPLYING the change! See '${DOCINFO_XML}.tmp' for investigation!" >&2
esac
