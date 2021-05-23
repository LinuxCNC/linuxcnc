#!/bin/bash
rs274 -p ${LIBDIR}/libcanterp.so -g canon | awk '{$1=""; print}'
exit ${PIPESTATUS[0]}
