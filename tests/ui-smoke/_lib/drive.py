#!/usr/bin/env python3
# UI smoke driver.
#
# Default mode (Phase 1): confirm linuxcnc task came up and the GUI did
# not crash. The driver only proves the GUI started and NML is reachable.
#
# --run-program mode (Phase 2): also estop-reset, machine-on, home,
# program_open + auto(RUN), wait for sustained INTERP_IDLE, and assert
# (stat.position_after - stat.position_after_home) equals --expect-delta-mm
# converted to machine units via stat.linear_units. Snapshot-and-delta
# sidesteps per-sim HOME offsets; mm-input + linear_units conversion
# sidesteps per-sim LINEAR_UNITS (axis and touchy sims are inch).

import argparse
import linuxcnc
import os
import sys
import time

CONNECT_TIMEOUT_S = 60.0
SETTLE_S = 3.0
SETTLE_POLLS = 5
POLL_INTERVAL_S = 0.01
# Per-attempt wait timeout for ensure_state / ensure_mode. The state
# normally lands well under 1s; profiling showed nothing benefits from
# more than 3s here, and shorter timeouts trim wall time when a retry
# is needed (notably gmoccapy reverting task_mode AUTO -> MANUAL).
ENSURE_ATTEMPT_TIMEOUT_S = 3.0
# After the desired task_state / task_mode is reached, re-check after
# this long. Some GUIs (notably gmoccapy and qtdragon) run their own
# startup commands that can revert a state we just set; the post-reach
# stability check catches that.
STATE_STABILITY_S = 0.5
STATE_RETRY_BUDGET = 6
# Pause after homing before requesting AUTO. gmoccapy only enables AUTO
# once it has processed the all-homed signal in its own event loop (and
# re-asserts MANUAL itself on that signal). Requesting AUTO before then is
# rejected: it bounces back to MANUAL with an "It is not possible to
# change to Auto Mode" warning. ensure_mode would retry and win, but the
# warning lingers on screen; this settle lets the GUI catch up first.
POST_HOME_SETTLE_S = 2.0

# linuxcnc launcher PID, written to linuxcnc.pid by the launcher and read
# once at startup. The driver watches it so a GUI crash, which tears
# linuxcnc down, fails the test in ~1s with a clear message instead of
# waiting out a long NML poll. A dead task keeps serving its last stat
# buffer, so process liveness is the only reliable crash signal.
_WATCH_PID = None


class LauncherGone(Exception):
    """linuxcnc process group exited (GUI crashed or task died)."""


def _read_pid(path):
    try:
        with open(path) as f:
            return int(f.read().strip())
    except (OSError, ValueError):
        return None


# Crash markers faulthandler and scripts/linuxcnc write to linuxcnc.err
# the instant the GUI dies. The launcher PID can linger in Cleanup, so
# scanning these catches the crash sooner and regardless of which GUI.
_CRASH_MARKERS = ("Fatal Python error", "Segmentation fault", "Aborted")


def _crash_marker_seen():
    try:
        with open("linuxcnc.err") as f:
            return any(m in f.read() for m in _CRASH_MARKERS)
    except OSError:
        return False


def _watchdog():
    """Raise LauncherGone if the GUI has crashed: either the launcher PID
    is gone, or a crash marker appeared in linuxcnc.err. Unknown PID and
    a missing log count as alive, so a not-yet-written file never
    false-fails the test."""
    if _WATCH_PID is not None:
        try:
            os.kill(_WATCH_PID, 0)
        except ProcessLookupError:
            raise LauncherGone()
        except PermissionError:
            pass
    if _crash_marker_seen():
        raise LauncherGone()


def connect_and_wait_ready(timeout):
    """Wait until linuxcnc.stat().poll() returns without error and
    reports a non-negative echo_serial_number. The NML status buffer
    can be 'invalid err=3' for the first ~30s while linuxcncsvr is
    still initialising; recreate the stat object on every iteration so
    a stale invalid buffer does not stick after linuxcncsvr is ready.

    Catch the full Exception hierarchy: in early startup stat.poll()
    can raise SystemError ('error return without exception set') when
    the underlying C function reports failure without setting a Python
    exception. Treat that the same as linuxcnc.error and retry."""
    deadline = time.monotonic() + timeout
    last_err = None
    while time.monotonic() < deadline:
        _watchdog()
        try:
            stat = linuxcnc.stat()
            stat.poll()
            if stat.echo_serial_number >= 0:
                return linuxcnc.command(), stat
        except Exception as e:
            last_err = e
        time.sleep(0.5)
    sys.stderr.write(
        f"UI_SMOKE_FAIL: task not ready within {timeout}s "
        f"(last NML error: {last_err})\n")
    return None, None


def wait_until_quiet(stat, predicate, timeout):
    """Poll stat until predicate(stat) is true. Returns True on success,
    False on timeout. Never writes UI_SMOKE_FAIL: caller decides whether
    a timeout here is fatal (and writes its own UI_SMOKE_FAIL line) or
    is part of a retry that may still succeed. checkresult.sh greps for
    any '^UI_SMOKE_FAIL' line, so spurious emissions during retries
    must not happen."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        _watchdog()
        stat.poll()
        if predicate(stat):
            return True
        time.sleep(POLL_INTERVAL_S)
    return False


def wait_until(stat, predicate, timeout, label):
    """Like wait_until_quiet but emits UI_SMOKE_FAIL on timeout. Use
    only when timeout is fatal at the call site (no retry above)."""
    if wait_until_quiet(stat, predicate, timeout):
        return True
    sys.stderr.write(f"UI_SMOKE_FAIL: timeout waiting for {label} after {timeout}s\n")
    return False


def home_all(cmd, stat, timeout):
    """Home every joint. Uses c.home(-1) which respects HOME_SEQUENCE
    if configured. Caller must have already ensured task_state is ON
    via ensure_state; otherwise the home command is rejected with
    'cannot be executed until the machine is out of E-stop and turned
    on'. Mode change uses ensure_mode so a GUI that reverts mode mid-
    sequence (gmoccapy) is detected and retried."""
    if not ensure_mode(cmd, stat, linuxcnc.MODE_MANUAL, "MODE_MANUAL"):
        return False
    cmd.teleop_enable(0)
    cmd.wait_complete()
    stat.poll()
    njoints = stat.joints
    cmd.home(-1)
    if not wait_until(
            stat,
            lambda s: all(s.homed[i] for i in range(njoints)),
            timeout, "all joints homed"):
        return False
    cmd.teleop_enable(1)
    cmd.wait_complete()
    return True


def wait_state(stat, target_state, timeout, label):
    """Poll until stat.task_state == target_state. wait_complete on a
    state-change command only proves task ack'd the NML message, not
    that the underlying state machine has transitioned. Polling
    task_state is the only deterministic signal."""
    return wait_until(
        stat,
        lambda s: s.task_state == target_state,
        timeout, label)


def ensure_state(cmd, stat, target_state, label):
    """Issue c.state(target_state), wait for stat.task_state to reach
    target_state, then verify it stays there across STATE_STABILITY_S.
    If the GUI reverts (e.g. gmoccapy re-issues its own ESTOP on
    startup), retry up to STATE_RETRY_BUDGET times. Returns True on
    stable success, False on exhausted budget."""
    for attempt in range(1, STATE_RETRY_BUDGET + 1):
        cmd.state(target_state)
        cmd.wait_complete()
        if not wait_until_quiet(
                stat, lambda s: s.task_state == target_state,
                ENSURE_ATTEMPT_TIMEOUT_S):
            sys.stderr.write(
                f"WARN: {label} not reached on attempt {attempt}, retrying\n")
            continue
        time.sleep(STATE_STABILITY_S)
        stat.poll()
        if stat.task_state == target_state:
            return True
        sys.stderr.write(
            f"WARN: {label} reverted to task_state={stat.task_state} "
            f"after attempt {attempt}, retrying\n")
    sys.stderr.write(
        f"UI_SMOKE_FAIL: {label} did not hold stable across "
        f"{STATE_RETRY_BUDGET} attempts\n")
    return False


def ensure_mode(cmd, stat, target_mode, label):
    """Same retry+stability pattern as ensure_state, for task_mode."""
    for attempt in range(1, STATE_RETRY_BUDGET + 1):
        cmd.mode(target_mode)
        cmd.wait_complete()
        if not wait_until_quiet(
                stat, lambda s: s.task_mode == target_mode,
                ENSURE_ATTEMPT_TIMEOUT_S):
            sys.stderr.write(
                f"WARN: {label} not reached on attempt {attempt}, retrying\n")
            continue
        time.sleep(STATE_STABILITY_S)
        stat.poll()
        if stat.task_mode == target_mode:
            return True
        sys.stderr.write(
            f"WARN: {label} reverted to task_mode={stat.task_mode} "
            f"after attempt {attempt}, retrying\n")
    sys.stderr.write(
        f"UI_SMOKE_FAIL: {label} did not hold stable across "
        f"{STATE_RETRY_BUDGET} attempts\n")
    return False


PROGRAM_START_TIMEOUT_S = 5.0


def snapshot(stat):
    """Best-effort one-line summary of state fields relevant to Phase 2
    debugging. Caller is expected to have just polled."""
    return (
        f"task_state={stat.task_state} task_mode={stat.task_mode} "
        f"interp_state={stat.interp_state} exec_state={stat.exec_state} "
        f"motion_type={stat.motion_type} queue={stat.queue} "
        f"queued_mdi_commands={stat.queued_mdi_commands} "
        f"file={stat.file!r}")


def wait_program_started(stat, timeout):
    """Wait until interp_state leaves INTERP_IDLE, i.e. the program
    has actually begun executing. Without this guard, a short program
    can finish before wait_program_idle gets its first poll, and the
    settle-window then mistakes the pre-start IDLE for the post-end
    IDLE; we then read stat.position at (0,0,0)."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        _watchdog()
        stat.poll()
        if stat.interp_state != linuxcnc.INTERP_IDLE:
            return True
        time.sleep(POLL_INTERVAL_S)
    stat.poll()
    sys.stderr.write(
        f"UI_SMOKE_FAIL: program did not start within {timeout}s "
        f"(interp_state stayed INTERP_IDLE) state: {snapshot(stat)}\n")
    return False


def wait_program_idle(stat, timeout):
    """Wait until interp_state returns to INTERP_IDLE and the motion
    queue is drained for SETTLE_POLLS consecutive polls. Caller must
    have already proven the program started via wait_program_started;
    otherwise this returns immediately on the pre-start IDLE."""
    deadline = time.monotonic() + timeout
    consecutive = 0
    while time.monotonic() < deadline:
        _watchdog()
        stat.poll()
        idle = (
            stat.interp_state == linuxcnc.INTERP_IDLE
            and stat.queue == 0
        )
        if idle:
            consecutive += 1
            if consecutive >= SETTLE_POLLS:
                return True
        else:
            consecutive = 0
        time.sleep(POLL_INTERVAL_S)
    sys.stderr.write(f"UI_SMOKE_FAIL: program did not reach idle within {timeout}s\n")
    return False


def run_program(cmd, stat, ngc_path, expect_delta_mm, tol, run_timeout):
    """Estop reset, machine on, home, snapshot position, load + run ngc,
    verify (final - start) delta matches expect_delta_mm converted to
    machine units."""
    if not ensure_state(cmd, stat, linuxcnc.STATE_ESTOP_RESET,
                        "STATE_ESTOP_RESET"):
        return False
    if not ensure_state(cmd, stat, linuxcnc.STATE_ON, "STATE_ON"):
        return False

    if not home_all(cmd, stat, timeout=60.0):
        return False

    # Let the GUI react to the all-homed transition before requesting AUTO,
    # so it does not reject the mode change (see POST_HOME_SETTLE_S).
    time.sleep(POST_HOME_SETTLE_S)
    stat.poll()

    if not ensure_mode(cmd, stat, linuxcnc.MODE_AUTO, "MODE_AUTO"):
        return False

    # Snapshot start position AFTER homing + AFTER mode transition. The
    # GUI might re-issue mode commands during its own startup; doing the
    # snapshot last means we record the position right before AUTO_RUN.
    stat.poll()
    start_pos = stat.position[:3]

    cmd.program_open(ngc_path)
    cmd.wait_complete()
    # No wait_complete after auto(AUTO_RUN, 0): wait_complete blocks
    # until the operation finishes, which for AUTO_RUN means the whole
    # program completes. That would race wait_program_started; by the
    # time we polled, interp would already be back at INTERP_IDLE.
    cmd.auto(linuxcnc.AUTO_RUN, 0)

    if not wait_program_started(stat, PROGRAM_START_TIMEOUT_S):
        return False
    if not wait_program_idle(stat, run_timeout):
        return False

    # stat.linear_units: machine units per mm. mm machine -> 1.0;
    # inch machine -> 1/25.4 = 0.03937. Multiplying the expected mm
    # delta by linear_units gives the expected delta in machine units,
    # which is what stat.position reports.
    units_per_mm = stat.linear_units
    expect_machine = [d * units_per_mm for d in expect_delta_mm]
    final_pos = stat.position[:3]
    actual_delta = [final_pos[i] - start_pos[i] for i in range(3)]
    err = [abs(actual_delta[i] - expect_machine[i]) for i in range(3)]
    if any(e > tol for e in err):
        sys.stderr.write(
            f"UI_SMOKE_FAIL: delta mismatch "
            f"expect_mm={expect_delta_mm} units_per_mm={units_per_mm} "
            f"expect_machine={expect_machine} "
            f"start={start_pos} final={final_pos} "
            f"actual_delta={actual_delta} err={err} tol={tol}\n")
        return False
    return True


def parse_xyz(s):
    parts = [float(p) for p in s.split(",")]
    if len(parts) != 3:
        raise argparse.ArgumentTypeError("expected x,y,z (three comma-separated floats)")
    return parts


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--run-program", metavar="NGC",
                    help="g-code file to load and run (enables Phase 2 mode)")
    ap.add_argument("--expect-delta-mm", type=parse_xyz, metavar="DX,DY,DZ",
                    help="expected XYZ delta in mm from post-home position "
                         "(required with --run-program). Driver converts to "
                         "machine units via stat.linear_units so the same "
                         "value works on inch and mm sims.")
    ap.add_argument("--tol", type=float, default=1e-4,
                    help="position tolerance per axis in machine units "
                         "(default: 1e-4)")
    ap.add_argument("--run-timeout", type=float, default=60.0,
                    help="program-completion timeout in seconds (default: 60)")
    args = ap.parse_args()

    if args.run_program and args.expect_delta_mm is None:
        ap.error("--run-program requires --expect-delta-mm DX,DY,DZ")

    global _WATCH_PID
    _WATCH_PID = _read_pid("linuxcnc.pid")

    try:
        cmd, stat = connect_and_wait_ready(CONNECT_TIMEOUT_S)
        if cmd is None:
            return 1

        # Give the GUI process enough time to finish constructing itself
        # (load .ui files, compile resources.py if needed, etc.) and
        # settle. If the GUI was going to crash on startup it has crashed
        # by now.
        time.sleep(SETTLE_S)
        _watchdog()

        # Re-check task is still alive; a GUI crash may have torn linuxcnc
        # down via Cleanup.
        try:
            stat.poll()
        except linuxcnc.error as e:
            sys.stderr.write(f"UI_SMOKE_FAIL: task disappeared after GUI startup: {e}\n")
            return 1

        if args.run_program:
            if not run_program(cmd, stat,
                               args.run_program, args.expect_delta_mm,
                               args.tol, args.run_timeout):
                return 1
    except LauncherGone:
        sys.stderr.write(
            "UI_SMOKE_FAIL: linuxcnc exited before the driver finished; "
            "the GUI crashed or task died. See linuxcnc.out / linuxcnc.err "
            "above for the backtrace.\n")
        return 1

    print("UI_SMOKE_OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
