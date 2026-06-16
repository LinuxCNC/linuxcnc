#!/bin/bash
# Known-good image comparison for the UI smoke confirm shots. Complements
# screenshot.sh: that grabs confirm.png on a clean run; this compares it to
# a committed reference.png and writes a visual diff.png. Like screenshot.sh
# it carries no state and is a logged no-op whenever it cannot run, so it can
# never turn a pass into a fail.
#
# Policy: we never fail a test on the image difference. freetype/font
# versions differ across distros, so some drift is expected; the diff is
# here to record what changed, not to gate. The function always returns 0.
#
# Local "make a known-good image" workflow: run a test with
# UI_SMOKE_UPDATE_REFERENCE=1 (see make-references.sh) and the freshly
# captured shot is saved as the committed reference instead of compared.

# Pick the ImageMagick compare entry point: IM7 "magick compare", else IM6
# "compare". Echoes nothing and returns 1 if neither is present.
_compare_cmd() {
    if command -v magick >/dev/null 2>&1; then
        echo "magick compare"
    elif command -v compare >/dev/null 2>&1; then
        echo "compare"
    else
        return 1
    fi
}

# compare_to_reference <shot> <reference> <diff>
# Compare the captured shot to the committed reference, writing a highlighted
# diff image. Always returns 0.
compare_to_reference() {
    shot="$1"
    reference="$2"
    diff="$3"

    if [ ! -s "$shot" ]; then
        echo "compare: no shot at $shot, skipping"
        return 0
    fi

    # Update mode: adopt this shot as the new known-good reference.
    if [ "${UI_SMOKE_UPDATE_REFERENCE:-}" = "1" ]; then
        if cp -f "$shot" "$reference"; then
            echo "compare: saved reference $reference (UI_SMOKE_UPDATE_REFERENCE=1)"
        else
            echo "compare: failed to save reference $reference"
        fi
        return 0
    fi

    if [ ! -s "$reference" ]; then
        echo "compare: no reference at $reference yet, skipping (run with UI_SMOKE_UPDATE_REFERENCE=1 to create one)"
        return 0
    fi

    cmd=$(_compare_cmd) || {
        echo "compare: no ImageMagick compare available, skipping"
        return 0
    }

    # -metric AE: count of differing pixels (interpretable); -fuzz absorbs
    # anti-aliasing jitter. compare exits 0 (identical), 1 (differ) or 2
    # (error, e.g. the shots are different sizes). We log the outcome and
    # always succeed.
    metric=$($cmd -metric AE -fuzz 40% "$reference" "$shot" "$diff" 2>&1)
    rc=$?
    case "$rc" in
        0) echo "compare: $shot matches $reference (AE=$metric)" ;;
        1) echo "compare: $shot differs from $reference (AE=$metric differing pixels); diff at $diff" ;;
        *) echo "compare: could not compare $shot to $reference (rc=$rc): $metric" ;;
    esac
    return 0
}
