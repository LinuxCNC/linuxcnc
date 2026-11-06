#!/bin/sh
# Regenerate the committed C++ bindings from the schema.
#
# emc_generated.h is committed so that a consumer needs the flatbuffers
# runtime headers and nothing else; flatc is only needed when emc.fbs
# changes.  With --check, regenerate into a temporary directory and compare,
# so a schema change that was not followed by a regeneration fails a test
# instead of living on as a stale header.
set -e
cd "$(dirname "$0")"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

flatc --cpp -o "$tmp" emc.fbs

# The bindings are part of the client-facing contract, so the library header
# is included the way a consumer sees it rather than the way flatc emits it.
sed 's|#include "flatbuffers/flatbuffers.h"|#include <flatbuffers/flatbuffers.h>|' \
    "$tmp/emc_generated.h" > "$tmp/fixed.h"
mv "$tmp/fixed.h" "$tmp/emc_generated.h"

if [ "${1:-}" = --check ]; then
    if ! cmp -s "$tmp/emc_generated.h" emc_generated.h; then
        echo "emc_generated.h does not match emc.fbs" >&2
        echo "run $0 to regenerate it" >&2
        exit 1
    fi
    echo "emc_generated.h matches emc.fbs"
else
    mv "$tmp/emc_generated.h" .
fi
