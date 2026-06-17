#!/bin/bash
# Known-good image comparison for the UI smoke confirm shots. Complements
# screenshot.sh: that grabs confirm.png on a clean run; this compares it to
# a committed reference.png and writes two visual diffs. Like screenshot.sh
# it carries no state and is a logged no-op whenever it cannot run, so it can
# never turn a pass into a fail.
#
# Two diff images, both at 0% fuzz so nothing is hidden:
#   diff.png      red highlight over a faded copy of the reference (where it
#                 changed, ImageMagick "compare" style)
#   diff-abs.png  absolute per-channel difference |b - a|, black where equal
#                 and bright where it changed (unbiased, magnitude preserved)
#
# Policy: we never fail a test on the image difference. freetype/font
# versions differ across distros, so some drift is expected; the diffs are
# here to record what changed, not to gate. The function always returns 0.
#
# Local "make a known-good image" workflow: run a test with
# UI_SMOKE_UPDATE_REFERENCE=1 (see make-references.sh) and the freshly
# captured shot is saved as the committed reference instead of compared.

# Set IM_COMPARE and IM_CONVERT to the IM7 or IM6 entry points; return 1
# if ImageMagick is absent.
_im_tools() {
    if command -v magick >/dev/null 2>&1; then
        IM_COMPARE="magick compare"; IM_CONVERT="magick"
    elif command -v compare >/dev/null 2>&1; then
        IM_COMPARE="compare"; IM_CONVERT="convert"
    else
        return 1
    fi
}

# compare_to_reference <shot> <reference> <diff> <diff_abs>
# Compare the captured shot to the committed reference, writing a red-highlight
# diff and an absolute-difference diff. Always returns 0.
compare_to_reference() {
    shot="$1"
    reference="$2"
    diff="$3"
    diff_abs="$4"

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

    _im_tools || {
        echo "compare: no ImageMagick available, skipping"
        return 0
    }

    # Red-highlight diff at 0% fuzz: -metric AE counts every differing pixel
    # (interpretable), and the same call writes diff.png. compare exits 0
    # (identical), 1 (differ) or 2 (error, e.g. the shots are different
    # sizes). We log the outcome and always succeed.
    metric=$($IM_COMPARE -metric AE -fuzz 0% "$reference" "$shot" "$diff" 2>&1)
    rc=$?
    case "$rc" in
        0) echo "compare: $shot matches $reference (AE=$metric)" ;;
        1) echo "compare: $shot differs from $reference (AE=$metric differing pixels); diff at $diff" ;;
        *) echo "compare: could not compare $shot to $reference (rc=$rc): $metric" ;;
    esac

    # Absolute-difference diff |b - a|: black where equal, bright where it
    # changed. Unbiased (direction does not matter) and keeps magnitude.
    if $IM_CONVERT "$reference" "$shot" -compose difference -composite "$diff_abs" 2>/dev/null; then
        echo "compare: absolute-difference diff at $diff_abs"
    else
        echo "compare: could not write absolute-difference diff $diff_abs"
    fi
    return 0
}
