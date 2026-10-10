#!/bin/bash
set -eu
repo_root=$(cd "$(dirname "$0")/../../.." && pwd)
test_dir="$repo_root/tests/hostmot2/sserial"
task_tmp=$(mktemp -d)
trap 'rm -f "$task_tmp/check" "$task_tmp/byte"; rmdir "$task_tmp"' EXIT
cc=${CC:-cc}
flags=(-O1 -g -fno-strict-aliasing -fwrapv -DRTAPI -DUSPACE -D_GNU_SOURCE
    -I"$repo_root/include" -I"$repo_root/src" -I"$repo_root/src/hal/drivers/mesa-hostmot2"
    -ffunction-sections -fdata-sections)
if [ -n "${SANITIZERS:-}" ]; then
    flags+=("-fsanitize=$SANITIZERS" -fno-sanitize-recover=all)
fi
driver="$repo_root/src/hal/drivers/mesa-hostmot2"
"$cc" "${flags[@]}" -Drealloc=hm2_test_realloc "$driver/sserial.c" "$driver/abs_encoder.c" \
    "$driver/pins.c" "$test_dir/test.c" -Wl,--gc-sections -lm -o "$task_tmp/check"
"$task_tmp/check"
"$cc" "${flags[@]}" "$driver/setsserial.c" "$test_dir/byte-test.c" \
    -Wl,--gc-sections -o "$task_tmp/byte"
"$task_tmp/byte"
