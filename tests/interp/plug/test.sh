#!/bin/bash
rs274 -p ../../../lib/libcanterp.so -g canon | awk '{$1=""; print}'
exit ${PIPESTATUS[0]}
