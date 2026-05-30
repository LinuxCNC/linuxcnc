#!/bin/bash
# Shared launcher for UI smoke tests.
# Usage: launch.sh <sim-config-ini>
#
# Spawns linuxcnc -r <ini> under xvfb-run, then runs the common driver
# script against it via NML. Captures stdout/stderr to per-test files.
#
# Skip vs fail (BsAtHome / hdiethelm review, PR #3999): xvfb-run absence
# is handled by the per-test skip files (skip-if-missing.sh), which
# runtests gates on before invoking test.sh. CI is expected to have all
# required deps; if any python module the GUI needs is missing the test
# should fail loudly rather than silently skip. Per-GUI deps are
# declared in debian/control under !nocheck, so apt-get build-dep
# installs them on CI.

set -u

CONFIG_INI="$1"
shift
DRIVER_ARGS=("$@")
TEST_DIR="${TEST_DIR:-$(cd "$(dirname "$0")" && pwd)}"
LIB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cd "$TEST_DIR" || exit 1
rm -f ui-smoke.out ui-smoke.err linuxcnc.pid

# Pre-launch cleanup: a previous ui-smoke test in the same job may
# have left a daemon listening on the NML TCP port or HAL shared
# memory still attached. Run the shared cleanup once before we start.
bash "$LIB_DIR/cleanup-runtime.sh"

# Launch linuxcnc inside xvfb-run. The outer timeout is a safety net
# so a wedged GUI cannot hang CI. Driver timeout covers connect (60s)
# + GUI settle (3s) + optional Phase 2 run (estop/home/program ~90s).
LINUXCNC_TIMEOUT=300
DRIVER_TIMEOUT=180

# Shared headless environment (software GL + audio silencing), kept in
# launch-env.sh so launch.sh and quit-launch.sh cannot drift apart.
. "$LIB_DIR/launch-env.sh"

# Export the per-invocation values so the inner bash -c receives them
# as proper env vars (avoids embedding paths into the inner script
# via quoting, which breaks on apostrophes / spaces).
export CONFIG_INI LIB_DIR DRIVER_TIMEOUT

# Single quotes around the inner script are intentional: CONFIG_INI,
# LIB_DIR and DRIVER_TIMEOUT are expanded by the inner bash (which sees
# them via the exported env), not by the outer shell.
# shellcheck disable=SC2016
xvfb-run -a --server-args="-screen 0 1024x768x24" \
    timeout "$LINUXCNC_TIMEOUT" \
    bash -c '
        # Run linuxcnc in its own process group so we can signal the
        # whole group cleanly (linuxcnc forks task, motion, GUI, halrun).
        # setsid makes the child a session leader, so its PID equals
        # the PGID and we can group-signal via "kill -- -$PID".
        setsid linuxcnc -r "$CONFIG_INI" >linuxcnc.out 2>linuxcnc.err &
        LINUXCNC_PID=$!
        echo "$LINUXCNC_PID" >linuxcnc.pid

        # The driver polls NML readiness itself (BsAtHome review:
        # avoid real-clock waits where status polling will do).
        # Driver args (Phase 2: --run-program/--expect-pos) come through
        # as positional $@ from the inner bash -c.
        timeout "$DRIVER_TIMEOUT" python3 "$LIB_DIR/drive.py" "$@" >ui-smoke.out 2>ui-smoke.err
        DRIVE_RC=$?

        # Clean shutdown: GUI-specific quit first (lets linuxcnc end
        # its own SIGTERM trap run Cleanup which unloads halrun and
        # reaps shared memory). axis-remote works only for axis but is
        # harmless otherwise. Then group-SIGTERM so the trap runs
        # in-process. Wait up to 60s for Cleanup to finish before
        # falling back to SIGKILL + cleanup-runtime.sh.
        if command -v axis-remote >/dev/null 2>&1; then
            axis-remote --quit 2>/dev/null || true
        fi

        kill -TERM -- -"$LINUXCNC_PID" 2>/dev/null || true
        for _ in $(seq 60); do
            kill -0 "$LINUXCNC_PID" 2>/dev/null || break
            sleep 1
        done
        if kill -0 "$LINUXCNC_PID" 2>/dev/null; then
            echo "WARN: linuxcnc did not exit on SIGTERM, escalating to KILL" >&2
            kill -KILL -- -"$LINUXCNC_PID" 2>/dev/null || true
            sleep 2
            bash "$LIB_DIR/cleanup-runtime.sh"
        fi

        exit "$DRIVE_RC"
    ' _launch "${DRIVER_ARGS[@]}"
RC=$?

# Surface logs so checkresult and CI artifact upload can see them.
echo "=== linuxcnc.out ==="
[ -f linuxcnc.out ] && cat linuxcnc.out
echo "=== linuxcnc.err ==="
[ -f linuxcnc.err ] && cat linuxcnc.err
echo "=== ui-smoke.out ==="
[ -f ui-smoke.out ] && cat ui-smoke.out
echo "=== ui-smoke.err ==="
[ -f ui-smoke.err ] && cat ui-smoke.err

exit "$RC"
