#!/bin/bash
# Quit-path launcher for UI smoke tests.
# Usage: quit-launch.sh <sim-config-ini> <gui-process-match>
#
# Boots linuxcnc + GUI under xvfb-run exactly like launch.sh, waits for
# the NML task to come up (via drive.py) and for the GUI to put a window
# on the screen, then sends SIGTERM to the GUI process *alone* and
# asserts the GUI exits on its own within a short grace. This is the regression guard for the SIGTERM clean-shutdown
# handlers: a GUI that absorbs SIGTERM and has to be SIGKILLed fails.
#
# <gui-process-match> is a pgrep -f pattern identifying the GUI process
# (e.g. "bin/touchy", "bin/gmoccapy"). It must not match the linuxcnc
# launcher or task/motion helpers.
#
# Markers (consumed by checkresult-quit.sh):
#   UI_SMOKE_QUIT_OK    GUI exited on SIGTERM within QUIT_GRACE
#   UI_SMOKE_QUIT_FAIL  GUI never started, showed no window, or ignored TERM

set -u

CONFIG_INI="$1"
GUI_MATCH="$2"
TEST_DIR="${TEST_DIR:-$(cd "$(dirname "$0")" && pwd)}"
LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cd "$TEST_DIR" || exit 1
rm -f ui-smoke.out ui-smoke.err ui-smoke-qt.png linuxcnc.pid

bash "$LIB_DIR/cleanup-runtime.sh"

LINUXCNC_TIMEOUT=240
DRIVER_TIMEOUT=90
# Seconds to wait for the GUI to exit after SIGTERM before declaring it
# stuck. A GUI honouring SIGTERM exits in well under a second; the
# margin covers Cleanup of task/motion on slow CI.
QUIT_GRACE=15
# Seconds to wait, after task is reachable, for the GUI to show a window
# before declaring it never came up.
GUI_UP_TIMEOUT=60

# Shared headless environment (software GL + audio silencing), kept in
# launch-env.sh so launch.sh and quit-launch.sh cannot drift apart.
. "$LIB_DIR/launch-env.sh"

# Arm a core dump so a GUI segfault can be backtraced after the run.
. "$LIB_DIR/crashdump.sh"
crashdump_arm

# Absolute path the offscreen-Qt self-grab writes to (the test dir, this
# shell's cwd); see launch.sh for why a relative name would miss.
export UI_SMOKE_QT_SHOT="$PWD/ui-smoke-qt.png"

export CONFIG_INI LIB_DIR DRIVER_TIMEOUT GUI_MATCH QUIT_GRACE GUI_UP_TIMEOUT

# shellcheck disable=SC2016
xvfb-run -a --server-args="-screen 0 $UI_SMOKE_XVFB_SCREEN" \
    timeout "$LINUXCNC_TIMEOUT" \
    bash -c '
        setsid linuxcnc -r "$CONFIG_INI" >linuxcnc.out 2>linuxcnc.err &
        LINUXCNC_PID=$!
        echo "$LINUXCNC_PID" >linuxcnc.pid

        # Wait until the task is reachable (the NML round-trip works).
        # Reuse the phase-1 driver for readiness.
        timeout "$DRIVER_TIMEOUT" python3 "$LIB_DIR/drive.py" >ui-smoke.out 2>ui-smoke.err
        if ! grep -q "^UI_SMOKE_OK$" ui-smoke.out; then
            echo "UI_SMOKE_QUIT_FAIL: GUI did not come up; cannot test quit"
            kill -KILL -- -"$LINUXCNC_PID" 2>/dev/null || true
            bash "$LIB_DIR/cleanup-runtime.sh"
            exit 1
        fi

        # Task being reachable says nothing about the GUI: the launcher
        # starts it after task, and on a loaded runner it can still be
        # importing, or constructing, seconds later. A SIGTERM there
        # tests startup, not quit. Wait for the process, then for a
        # window of its own on the screen: that is a GUI running.
        #
        # pgrep -f matches against the whole command line, so wrapper
        # processes (the linuxcnc launcher, the xvfb-run shell, this
        # bash -c) also match because the GUI name appears in the config
        # path or the embedded script text. Every such wrapper has a
        # shell or xvfb-run as argv[0]; the real GUI is a python
        # interpreter. Pick the first match whose argv[0] basename is a
        # python binary.
        find_gui_pid() {
            for p in $(pgrep -f "$GUI_MATCH"); do
                arg0=$(tr "\0" "\n" <"/proc/$p/cmdline" 2>/dev/null | head -1)
                case "$(basename "$arg0" 2>/dev/null)" in
                    python*) echo "$p"; return 0 ;;
                esac
            done
            return 1
        }
        # A viewable top-level window whose _NET_WM_PID is the GUI. GTK
        # and Qt both set the property; xvfb runs no window manager, so
        # top-levels are children of the root window.
        #
        # Offscreen Qt never draws to the X server. Its shim saves a
        # self-grab on SIGUSR1, and only once a top-level is visible, so
        # the grab file appearing is the same signal. The shim installs
        # the handler at interpreter startup; until then SIGUSR1 would
        # kill the process, so ask only once /proc says it is caught.
        gui_window_up() {
            if [ "${QT_QPA_PLATFORM:-}" = "offscreen" ]; then
                [ -s "$UI_SMOKE_QT_SHOT" ] && return 0
                caught=$(awk "/^SigCgt:/{print \$2}" "/proc/$1/status" 2>/dev/null)
                if [ -n "$caught" ] && [ $(( 0x$caught >> 9 & 1 )) -eq 1 ]; then
                    kill -USR1 "$1" 2>/dev/null || true
                fi
                return 1
            fi
            for w in $(xwininfo -root -children 2>/dev/null | grep -oE "^ +0x[0-9a-f]+"); do
                xprop -id "$w" _NET_WM_PID 2>/dev/null | grep -q "= $1\$" || continue
                xwininfo -id "$w" 2>/dev/null | grep -q "Map State: IsViewable" && return 0
            done
            return 1
        }
        GUI_PID=""
        waited=0
        while [ "$waited" -lt "$GUI_UP_TIMEOUT" ]; do
            [ -n "$GUI_PID" ] || GUI_PID=$(find_gui_pid)
            if [ -n "$GUI_PID" ] && gui_window_up "$GUI_PID"; then
                break
            fi
            kill -0 "$LINUXCNC_PID" 2>/dev/null || break
            sleep 1
            waited=$((waited + 1))
        done
        if [ -z "$GUI_PID" ]; then
            echo "UI_SMOKE_QUIT_FAIL: GUI process matching \"$GUI_MATCH\" not found"
            kill -KILL -- -"$LINUXCNC_PID" 2>/dev/null || true
            bash "$LIB_DIR/cleanup-runtime.sh"
            exit 1
        fi
        if [ "$waited" -ge "$GUI_UP_TIMEOUT" ]; then
            echo "UI_SMOKE_QUIT_FAIL: GUI (pid $GUI_PID) showed no window within ${GUI_UP_TIMEOUT}s"
            . "$LIB_DIR/screenshot.sh"
            screenshot_grab screenshot.png
            kill -KILL -- -"$LINUXCNC_PID" 2>/dev/null || true
            bash "$LIB_DIR/cleanup-runtime.sh"
            exit 1
        fi
        echo "GUI (pid $GUI_PID) up with a window after ${waited}s"

        # Send SIGTERM to the GUI alone and time how long it takes to go.
        kill -TERM "$GUI_PID" 2>/dev/null || true
        waited=0
        while [ "$waited" -lt "$QUIT_GRACE" ]; do
            kill -0 "$GUI_PID" 2>/dev/null || break
            sleep 1
            waited=$((waited + 1))
        done

        if kill -0 "$GUI_PID" 2>/dev/null; then
            echo "UI_SMOKE_QUIT_FAIL: GUI (pid $GUI_PID) still alive ${QUIT_GRACE}s after SIGTERM"
            # A GUI that absorbs SIGTERM is usually blocked on a modal it
            # cannot dismiss headless. Photograph it before teardown so the
            # offending dialog is visible. The GUI is still up here.
            . "$LIB_DIR/screenshot.sh"
            screenshot_grab screenshot.png
            RC=1
        else
            echo "UI_SMOKE_QUIT_OK: GUI exited ${waited}s after SIGTERM"
            RC=0
        fi

        # Tear down whatever is left (task/motion, or the GUI on failure).
        kill -TERM -- -"$LINUXCNC_PID" 2>/dev/null || true
        for _ in $(seq 30); do
            kill -0 "$LINUXCNC_PID" 2>/dev/null || break
            sleep 1
        done
        if kill -0 "$LINUXCNC_PID" 2>/dev/null; then
            kill -KILL -- -"$LINUXCNC_PID" 2>/dev/null || true
            sleep 2
            bash "$LIB_DIR/cleanup-runtime.sh"
        fi
        exit "$RC"
    '
RC=$?

echo "=== linuxcnc.err ==="
[ -f linuxcnc.err ] && cat linuxcnc.err
echo "=== ui-smoke.out ==="
[ -f ui-smoke.out ] && cat ui-smoke.out
echo "=== ui-smoke.err ==="
[ -f ui-smoke.err ] && cat ui-smoke.err

# If the GUI dumped a core, print its native backtrace.
crashdump_report

# Note any failure screenshot for the CI artifact step and reviewer.
[ -f screenshot.png ] && echo "=== screenshot: $TEST_DIR/screenshot.png ==="

exit "$RC"
