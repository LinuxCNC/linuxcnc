#!/bin/bash
# Quit-path launcher for UI smoke tests.
# Usage: quit-launch.sh <sim-config-ini> <gui-process-match>
#
# Boots linuxcnc + GUI under xvfb-run exactly like launch.sh, waits for
# the NML task to come up (via drive.py), then sends SIGTERM to the GUI
# process *alone* and asserts the GUI exits on its own within a short
# grace. This is the regression guard for the SIGTERM clean-shutdown
# handlers: a GUI that absorbs SIGTERM and has to be SIGKILLed fails.
#
# <gui-process-match> is a pgrep -f pattern identifying the GUI process
# (e.g. "bin/touchy", "bin/gmoccapy"). It must not match the linuxcnc
# launcher or task/motion helpers.
#
# Markers (consumed by checkresult-quit.sh):
#   UI_SMOKE_QUIT_OK    GUI exited on SIGTERM within QUIT_GRACE
#   UI_SMOKE_QUIT_FAIL  GUI never started, was not found, or ignored TERM

set -u

CONFIG_INI="$1"
GUI_MATCH="$2"
TEST_DIR="${TEST_DIR:-$(cd "$(dirname "$0")" && pwd)}"
LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cd "$TEST_DIR" || exit 1
rm -f ui-smoke.out ui-smoke.err linuxcnc.pid

bash "$LIB_DIR/cleanup-runtime.sh"

LINUXCNC_TIMEOUT=240
DRIVER_TIMEOUT=90
# Seconds to wait for the GUI to exit after SIGTERM before declaring it
# stuck. A GUI honouring SIGTERM exits in well under a second; the
# margin covers Cleanup of task/motion on slow CI.
QUIT_GRACE=15

# Shared headless environment (software GL + audio silencing), kept in
# launch-env.sh so launch.sh and quit-launch.sh cannot drift apart.
. "$LIB_DIR/launch-env.sh"

# Arm a core dump so a GUI segfault can be backtraced after the run.
. "$LIB_DIR/crashdump.sh"
crashdump_arm

export CONFIG_INI LIB_DIR DRIVER_TIMEOUT GUI_MATCH QUIT_GRACE

# shellcheck disable=SC2016
xvfb-run -a --server-args="-screen 0 1024x768x24" \
    timeout "$LINUXCNC_TIMEOUT" \
    bash -c '
        setsid linuxcnc -r "$CONFIG_INI" >linuxcnc.out 2>linuxcnc.err &
        LINUXCNC_PID=$!
        echo "$LINUXCNC_PID" >linuxcnc.pid

        # Wait until the task is reachable (GUI has constructed and the
        # NML round-trip works). Reuse the phase-1 driver for readiness.
        timeout "$DRIVER_TIMEOUT" python3 "$LIB_DIR/drive.py" >ui-smoke.out 2>ui-smoke.err
        if ! grep -q "^UI_SMOKE_OK$" ui-smoke.out; then
            echo "UI_SMOKE_QUIT_FAIL: GUI did not come up; cannot test quit"
            kill -KILL -- -"$LINUXCNC_PID" 2>/dev/null || true
            bash "$LIB_DIR/cleanup-runtime.sh"
            exit 1
        fi

        # Identify the GUI process. pgrep -f matches against the whole
        # command line, so wrapper processes (the linuxcnc launcher, the
        # xvfb-run shell, this bash -c) also match because the GUI name
        # appears in the config path or the embedded script text. Every
        # such wrapper has a shell or xvfb-run as argv[0]; the real GUI
        # is a python interpreter. Pick the first match whose argv[0]
        # basename is a python binary.
        GUI_PID=""
        for p in $(pgrep -f "$GUI_MATCH"); do
            arg0=$(tr "\0" "\n" <"/proc/$p/cmdline" 2>/dev/null | head -1)
            case "$(basename "$arg0" 2>/dev/null)" in
                python*) GUI_PID="$p"; break ;;
            esac
        done
        if [ -z "$GUI_PID" ]; then
            echo "UI_SMOKE_QUIT_FAIL: GUI process matching \"$GUI_MATCH\" not found"
            kill -KILL -- -"$LINUXCNC_PID" 2>/dev/null || true
            bash "$LIB_DIR/cleanup-runtime.sh"
            exit 1
        fi

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

exit "$RC"
