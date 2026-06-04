#!/bin/bash
# Screen capture for the UI smoke launchers. Complements crashdump.sh:
# that one fires only on a segfault, but a GUI can also fail by hanging
# (a modal dialog blocking headless startup, a wedged event loop) with no
# core and no Python traceback. A picture of the root window at failure
# time shows what is actually on screen. Source with no state needed; the
# grab is a no-op (with a logged reason) when there is no display or no
# grabber, so it can never turn a pass into a fail.
#
# Must be called from inside the xvfb-run subshell, where DISPLAY points
# at the Xvfb server and the GUI window still exists. CI uploads the PNG
# as a build artifact (see .github/workflows/ci.yml).

screenshot_grab() {
    out="$1"
    # Offscreen Qt (qtdragon) renders in memory, never to the X server, so
    # an X root grab gets a black frame. Ask the GUI to grab itself
    # instead: the qtdragon shim installs a SIGUSR1 handler that saves its
    # top-level window to ui-smoke-qt.png in the test dir.
    if [ "${QT_QPA_PLATFORM:-}" = "offscreen" ]; then
        screenshot_grab_qt "$out"
        return 0
    fi
    if [ -z "${DISPLAY:-}" ]; then
        echo "screenshot: no DISPLAY set, skipping $out"
        return 0
    fi
    # ImageMagick's import grabs X11 directly with no xwd dependency and
    # is the grabber present in the CI image. Fall back to xwd|convert for
    # local dev boxes that have xwd but not import.
    if command -v import >/dev/null 2>&1; then
        if import -window root "$out" 2>/dev/null; then
            echo "screenshot: wrote $out"
        else
            echo "screenshot: import failed for $out"
        fi
    elif command -v xwd >/dev/null 2>&1 && command -v convert >/dev/null 2>&1; then
        if xwd -root -display "$DISPLAY" 2>/dev/null | convert xwd:- "$out" 2>/dev/null; then
            echo "screenshot: wrote $out"
        else
            echo "screenshot: xwd|convert failed for $out"
        fi
    else
        echo "screenshot: no grabber (import or xwd) available, skipping $out"
    fi
    return 0
}

# Native grab for an offscreen Qt GUI. Find the qtvcp python process,
# SIGUSR1 it, and wait for the shim's handler to drop ui-smoke-qt.png in
# the test dir (the GUI's cwd), then move it to $out. No-op (logged) if
# the GUI or the grab is not found, so it can never fail a test.
screenshot_grab_qt() {
    out="$1"
    # The GUI shim saves to this absolute path (set by the launcher),
    # since the offscreen GUI's cwd is the config mirror, not the test dir.
    shot="${UI_SMOKE_QT_SHOT:-ui-smoke-qt.png}"
    rm -f "$shot"
    pid=""
    for p in $(pgrep -f "qtvcp" 2>/dev/null); do
        arg0=$(tr '\0' '\n' <"/proc/$p/cmdline" 2>/dev/null | head -1)
        case "$(basename "$arg0" 2>/dev/null)" in
            python*) pid="$p"; break ;;
        esac
    done
    if [ -z "$pid" ]; then
        echo "screenshot: qtvcp process not found, skipping $out"
        return 0
    fi
    kill -USR1 "$pid" 2>/dev/null || true
    waited=0
    while [ "$waited" -lt 10 ]; do
        [ -s "$shot" ] && break
        sleep 0.5
        waited=$((waited + 1))
    done
    if [ -s "$shot" ]; then
        mv -f "$shot" "$out"
        echo "screenshot: wrote $out (qt native grab)"
    else
        echo "screenshot: qt native grab produced no file for $out"
    fi
    return 0
}
