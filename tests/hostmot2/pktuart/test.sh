#!/bin/bash
set -eu
repo_root=$(cd "$(dirname "$0")/../../.." && pwd)
task_tmp=$(mktemp -d)
trap 'rm -f "$task_tmp/check"; rmdir "$task_tmp"' EXIT
cc=${CC:-cc}
flags=(-O1 -g -fno-strict-aliasing -fwrapv -DRTAPI -DUSPACE -D_GNU_SOURCE
    -I"$repo_root/include" -I"$repo_root/src" -I"$repo_root/src/hal/drivers/mesa-hostmot2"
    -ffunction-sections -fdata-sections)
if [ -n "${SANITIZERS:-}" ]; then
    flags+=("-fsanitize=$SANITIZERS" -fno-sanitize-recover=all)
fi
"$cc" "${flags[@]}" "$repo_root/src/hal/drivers/mesa-hostmot2/pktuart.c" \
    "$repo_root/tests/hostmot2/pktuart/test.c" -Wl,--gc-sections -o "$task_tmp/check"
"$task_tmp/check"
