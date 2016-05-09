#!/bin/bash

# This wrapper around Python scripts uses them to generate XML DMF files
# from existing NUT *-mib.c sources with legacy mapping structures.
# It expects to be located in (executed from) $NUT_SOURCEDIR/scripts/DMF
#
#   Copyright (C) 2016 Michal Vyskocil <MichalVyskocil@eaton.com>
#   Copyright (C) 2016 Jim Klimov <EvgenyKlimov@eaton.com>
#

# A bashism, important for us here
set -o pipefail

[ -n "${PYTHON-}" ] || PYTHON="`which python2.7`"
[ -n "${PYTHON}" ] && [ -x "$PYTHON" ] || { echo "ERROR: Can not find Python 2.7" >&2; exit 2; }

# Here we only check basic prerequisites (a module provided with Python 2.7
# and an extra module that end-user is expected to pre-install per README).
# Other included modules will be checked when scripts are executed.
echo "INFO: Validating some basics about your Python installation"
for PYMOD in argparse pycparser json; do
    "${PYTHON}" -c "import $PYMOD; print $PYMOD" || \
        { echo "ERROR: Can not use Python module '$PYMOD'" >&2; exit 2; }
done

dmfify_c_file() {
    # One argument: path to a `*-mib.c` filename
    local cmib="$1"
    local mib="$(basename "${cmib}" .c)"

    "${PYTHON}" jsonify-mib.py --test "${cmib}" \
    | "${PYTHON}" xmlify-mib.py > "${mib}.dmf.tmp" \
    && [ -s "${mib}.dmf.tmp" ] \
    || { ERRCODE=$?
        echo "ERROR: Could not parse '${cmib}' into '${mib}.dmf'" >&2
        echo "       You can inspect a copy of the intermediate result in '${mib}.dmf.tmp'" >&2
        return $ERRCODE; }

    mv -f "${mib}.dmf.tmp" "${mib}.dmf"
}

dmfify_NUT_drivers() {
    for cmib in ../../drivers/*-mib.c; do
        [ -s "${cmib}" ] || \
            { echo "ERROR: File not found or is empty: '${cmib}'" >&2; return 2; }
        dmfify_c_file "${cmib}" || return
    done
    return 0
}

if [[ "$#" -gt 0 ]]; then
    echo "INFO: Got some arguments, assuming they are NUT filenames for parsing"
    while [[ "$#" -gt 0 ]]; do
        dmfify_c_file "$1" || exit
        shift
    done
else
    echo "INFO: No arguments provided, will try to parse all NUT drivers"
    dmfify_NUT_drivers || exit
fi

echo "OK - All done"
