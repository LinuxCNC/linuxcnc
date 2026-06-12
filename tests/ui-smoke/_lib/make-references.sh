#!/bin/bash
# Generate (or refresh) the committed known-good reference.png images by
# running the ui-smoke tests with UI_SMOKE_UPDATE_REFERENCE=1, which makes
# compare.sh save each clean confirm shot as that test's reference.png
# instead of comparing against it.
#
# Run from a built run-in-place tree with the rip environment sourced:
#   . scripts/rip-environment
#   tests/ui-smoke/_lib/make-references.sh            # all run-program GUIs
#   tests/ui-smoke/_lib/make-references.sh axis       # just one
#
# Only the GUIs that capture a confirm shot (the --run-program tests) have a
# reference. Review the resulting PNGs before committing; they are baselines
# from this machine's fonts and will drift on other distros (which is why the
# comparison never fails a test, only records a diff).

set -u

LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SMOKE_DIR="$(cd "$LIB_DIR/.." && pwd)"
ROOT="$(cd "$SMOKE_DIR/../.." && pwd)"

if ! command -v runtests >/dev/null 2>&1; then
    echo "make-references: 'runtests' not found; source scripts/rip-environment first" >&2
    exit 1
fi

# GUIs whose tests grab a confirm shot (i.e. run a program).
GUIS=("$@")
if [ "${#GUIS[@]}" -eq 0 ]; then
    GUIS=(axis gmoccapy touchy qtdragon)
fi

export UI_SMOKE_UPDATE_REFERENCE=1

rc=0
for gui in "${GUIS[@]}"; do
    dir="$SMOKE_DIR/$gui"
    if [ ! -x "$dir/test.sh" ]; then
        echo "make-references: no test at $dir, skipping"
        continue
    fi
    echo "=== make-references: $gui ==="
    runtests "$dir" || rc=1
    if [ -s "$dir/reference.png" ]; then
        echo "make-references: wrote $dir/reference.png"
    else
        echo "make-references: WARNING no reference.png produced for $gui" >&2
        rc=1
    fi
done

exit "$rc"
