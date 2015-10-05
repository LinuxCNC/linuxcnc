#!/bin/bash
# All expected to fail

# Fanuc 'O...' sub definition mixed with RS274NGC 'O... endsub'/'O... call'
! rs274 -g O...-called-with-O..._call.ngc 2>&1 || exit 1
! rs274 -g O...-ended-with-O..._endsub.ngc 2>&1 || exit 1

# RS274 'O... sub' sub definition mixed with Fanuc 'M99'/'M98'
! rs274 -g O...-sub-called-with-M98.ngc 2>&1 || exit 1
! rs274 -g O...-sub-ended-with-M99.ngc 2>&1 || exit 1
