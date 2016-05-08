#!/bin/bash

# This wrapper around Python scripts uses them to generate XML DMF files
# from existing NUT *-mib.c sources with legacy mapping structures.
# It expects to be located in (executed from) $NUT_SOURCEDIR/scripts/DMF
#
#    Copyright (C) 2016 Michal Vyskocil <MichalVyskocil@eaton.com>
#

for cmib in $(ls -1 ../../drivers/*-mib.c); do
    mib="$(basename "${cmib}" .c)"
    python jsonify-mib.py --test "${cmib}" | \
    python xmlify-mib.py > "${mib}.dmf"
done
