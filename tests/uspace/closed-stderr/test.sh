#!/bin/bash
# stderr is a pipe without a reader: every message rtapi_app prints
# raises SIGPIPE
rm -f dead
mkfifo dead
# open the fifo for writing, then drop its only reader
# shellcheck disable=SC2094
exec 3<>dead 4>dead 3<&-
rm -f dead
halrun threads.hal 2>&4 | grep -o ' a ('
echo "halrun exit ${PIPESTATUS[0]}"
