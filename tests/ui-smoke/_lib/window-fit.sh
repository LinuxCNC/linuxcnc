#!/bin/bash
# Window-fit regression check for the UI smoke tests. A GUI whose window
# is larger than the screen pushes controls off the display with no way
# to reach them. Touchy has done this (no scrolling, so the window grows
# with its content); this guards against it coming back. Run inside the
# xvfb-run subshell, where DISPLAY and the GUI window exist. Prints a
# UI_SMOKE_FAIL line (which checkresult greps) when the window exceeds the
# screen, so a future regression fails the test instead of silently
# producing an unusable window.
#
# window_fit_check <wm-class-pattern>
#   <wm-class-pattern> is matched literally against the xwininfo tree
#   line, e.g. '("touchy" "Touchy")'. The first matching window is checked
#   against the root window (the screen). Needs xwininfo (x11-utils).

window_fit_check() {
    pattern="$1"
    if ! command -v xwininfo >/dev/null 2>&1; then
        echo "UI_SMOKE_FAIL: window-fit: xwininfo not available (install x11-utils)"
        return 1
    fi
    root=$(xwininfo -root 2>/dev/null)
    screen_w=$(echo "$root" | awk '/Width:/{print $2; exit}')
    screen_h=$(echo "$root" | awk '/Height:/{print $2; exit}')
    geom=$(xwininfo -root -tree 2>/dev/null | grep -F "$pattern" \
        | grep -oE '[0-9]+x[0-9]+\+[0-9]+\+[0-9]+' | head -1)
    win_w=${geom%%x*}
    rest=${geom#*x}
    win_h=${rest%%+*}
    if [ -z "$win_w" ] || [ -z "$win_h" ]; then
        echo "UI_SMOKE_FAIL: window-fit: no window matching $pattern found"
        return 1
    fi
    if [ "$win_w" -gt "$screen_w" ] || [ "$win_h" -gt "$screen_h" ]; then
        echo "UI_SMOKE_FAIL: window ${win_w}x${win_h} exceeds screen ${screen_w}x${screen_h}"
        return 1
    fi
    echo "window-fit OK: ${win_w}x${win_h} within ${screen_w}x${screen_h}"
    return 0
}
