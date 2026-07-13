#!/bin/sh
# Verify an external client program builds and links against the gomc
# control-API client library (libgmi).  This replaces the classic NML client
# build check (which linked the now-removed -lnml -llinuxcnc).
#
# NB: libgmi.so does not declare its libcurl/libcjson dependencies, so a
# consumer must add -lcurl -lcjson explicitly (see PRODUCTION_READINESS.md).
set -x
gcc -I${HEADERS} \
    build-ui.c \
    -L ${LIBDIR} -lgmi -lcurl -lcjson \
    -o /dev/null
