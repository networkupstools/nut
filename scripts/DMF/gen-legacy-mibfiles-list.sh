#!/bin/sh

# This script lists existing NUT *-mib.c sources with legacy MIB-mapping
# structures into a "legacy-mibfiles-list.inc" to be included by Makefile.
# It also generates the rule snippets to convert such files from C to DMF
# in a (hopefully) portable manner across different Make implementations.
# It expects to be located in (executed from) $NUT_SOURCEDIR/scripts/DMF
# of the real (e.g. not out-of-tree) NUT codebase, before configure is run.
# Be portable - no bash etc... plain minimal shell. Tested with bash, dash,
# busybox sh and ksh for good measure.
#
#   Copyright (C) 2016 Jim Klimov <EvgenyKlimov@eaton.com>
#

# Relative to here we look for old sources
_SCRIPT_DIR="`cd $(dirname "$0") && pwd`" || \
    _SCRIPT_DIR="./" # Fallback can fail

# Note that technically it is not required that this is an ".in" template
# as the data is currently static. However, it helps streamline out-of-tree
# builds (e.g. distcheck) using the powers of automake/configure scriptware.
OUTFILE=legacy-mibfiles-list.mk.in

[ -z "${DEBUG-}" ] && DEBUG=no

cd "$_SCRIPT_DIR" || exit

list_LEGACY_NUT_C_MIBS() {
    i=0
    for cmib in ../../drivers/*-mib.c; do
        [ -s "${cmib}" ] || \
            { [ "$DEBUG" = yes ] && echo "WARN: File not found or is empty: '${cmib}'" >&2; continue; }
        echo "`basename ${cmib}`"
        i="`expr $i + 1`"
    done
    [ "$i" = 0 ] && echo "ERROR: No files found" >&2 && return 2
    [ "$DEBUG" = yes ] && echo "INFO: Found $i files OK" >&2
    return 0
}

LEGACY_NUT_C_MIBS_LIST=""
sort_LEGACY_NUT_C_MIBS() {
    [ -n "${LEGACY_NUT_C_MIBS_LIST}" ] || \
        { LEGACY_NUT_C_MIBS_LIST="`list_LEGACY_NUT_C_MIBS`" && \
          LEGACY_NUT_C_MIBS_LIST="`echo "$LEGACY_NUT_C_MIBS_LIST" | sort | uniq`"; } \
        || return $?
    [ -n "${LEGACY_NUT_C_MIBS_LIST}" ] || return $?
    echo "$LEGACY_NUT_C_MIBS_LIST"
}

print_makefile_LEGACY_NUT_DMF_RULES() {
    echo "LEGACY_NUT_C_MIBS ="
    echo "LEGACY_NUT_DMFS ="
    echo 'LEGACY_NUT_DMF_SYMLINKS ='
    echo 'INSTALL_NUT_DMF_SYMLINKS ='

    printf '\n# NOTE: DMFSNMP_SUBDIR, DMFSNMP_RES_SUBDIR, DMFGEN_DEPS and DMFGEN_CMD,\n# and dmfsnmpdir and dmfsnmpresdir (for install) are defined in Makefile.am\n\n'

    for CMIBBASE in `sort_LEGACY_NUT_C_MIBS` ; do
        case "$CMIBBASE" in
            */*) CMIBFILE="$CMIBBASE"; CMIBBASE="`basename "$CMIBBASE"`" ;;
            *)   CMIBFILE='$(abs_top_srcdir)/drivers/'"$CMIBBASE" ;;
        esac
        DMFBASE="`basename "$CMIBBASE" .c`".dmf
        DMFFILE='$(DMFSNMP_RES_SUBDIR)/'"$DMFBASE"
        case "$DMFBASE" in
            ietf-mib.dmf) L="S90_${DMFBASE}" ;;
            S*|K*) ;;
            *) L="S10_${DMFBASE}" ;;
        esac
        DMFLINK='$(DMFSNMP_SUBDIR)/'"$L"
        printf 'LEGACY_NUT_C_MIBS +=\t%s\n' "$CMIBFILE"
        printf 'LEGACY_NUT_DMFS   +=\t%s\n' "$DMFFILE"
        printf '%s : %s $(DMFGEN_DEPS)\n\t@DMFFILE="%s"; CMIBFILE="%s"; $(DMFGEN_CMD)\n\n' "$DMFFILE" "$CMIBFILE" "$DMFFILE" "$CMIBFILE"
        printf 'LEGACY_NUT_DMF_SYMLINKS += %s\n' "$DMFLINK"
        printf '%s : %s\n\t@DMFFILE="%s"; DMFLINK="%s"; $(DMFLNK_CMD)\n\n' "$DMFLINK" "$DMFFILE" "$DMFFILE" "$DMFLINK"
        INSTFILE='$(DESTDIR)$(dmfsnmpresdir)/'"$DMFBASE"
        INSTLINK='$(DESTDIR)$(dmfsnmpdir)/'"$L"
        printf 'INSTALL_NUT_DMF_SYMLINKS += %s\n' "$INSTLINK"
        #printf '%s : dmfsnmpres_DATA\n' "$INSTFILE"
        printf '%s : %s\n\t@DMFFILE="%s"; DMFLINK="%s"; $(DMFLNK_CMD)\n\n\n' "$INSTLINK" "$INSTFILE" "$INSTFILE" "$INSTLINK"
    done || return
    echo ""
    return 0
}

print_makefile() {
    echo "### This file was automatically generated at `TZ=UTC date`"
    sort_LEGACY_NUT_C_MIBS > /dev/null || return $? # prepare cache
    print_makefile_LEGACY_NUT_DMF_RULES
}

rm -f "$OUTFILE"
OUTTEXT="`print_makefile`" || { RES=$?; echo "FAILED to generate $OUTFILE content" >&2; rm -f "$OUTFILE"; exit $RES; }
echo "$OUTTEXT" > "$OUTFILE" || { RES=$?; echo "FAILED to generate $OUTFILE" >&2; rm -f "$OUTFILE"; exit $RES; }

echo "OK - Generated $OUTFILE" >&2
exit 0
