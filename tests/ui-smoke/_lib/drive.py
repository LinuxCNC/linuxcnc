#!/usr/bin/env python3
# UI smoke driver.
#
# Default mode (Phase 1): confirm linuxcnc task came up and the GUI did
# not crash. The driver only proves the GUI started and NML is reachable;
# with --gui-comp and --gui-window it also waits for the GUI's HAL
# component to be ready and its window to be up.
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
import subprocess
import sys
import time

CONNECT_TIMEOUT_S = 60.0
GUI_READY_TIMEOUT_S = 30.0
SETTLE_POLLS = 5
POLL_INTERVAL_S = 0.01
# Per-attempt wait timeout for ensure_state / ensure_mode. The state
# normally lands well under 1s; profiling showed nothing benefits from
# more than 3s here, and shorter timeouts trim wall time when a retry
# is needed (notably gmoccapy reverting task_mode AUTO -> MANUAL).
ENSURE_ATTEMPT_TIMEOUT_S = 3.0
STATE_RETRY_BUDGET = 6

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

    Catch the full Exception hierarchy so an unexpected error during
    startup is retried like linuxcnc.error."""
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


def wait_gui_ready(comp, timeout):
    """Wait until the GUI's HAL component reports ready. Each GUI marks
    it ready once it has built its widgets and pins, so the driver does
    not command the machine while the GUI is still constructing itself."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        _watchdog()
        try:
            out = subprocess.run(["halcmd", "-s", "show", "comp", comp],
                                 capture_output=True, text=True,
                                 timeout=10).stdout
        except (OSError, subprocess.SubprocessError):
            out = ""
        for line in out.splitlines():
            f = line.split()
            if len(f) >= 4 and f[2] == comp and f[-1] == "ready":
                return True
        time.sleep(0.1)
    sys.stderr.write(
        f"UI_SMOKE_FAIL: GUI component {comp} not ready within {timeout}s\n")
    return False


def wait_gui_window(pattern, timeout):
    """Wait until a top-level window whose xwininfo tree line holds
    pattern (its WM_CLASS) is viewable. A GUI maps its window once it
    has built itself and runs its main loop; gmoccapy reports its HAL
    component ready well before, while it still sends its own startup
    commands (a jog stop per axis among them), which abort a homing
    started meanwhile."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        _watchdog()
        try:
            tree = subprocess.run(["xwininfo", "-root", "-tree"],
                                  capture_output=True, text=True,
                                  timeout=10).stdout
            for line in tree.splitlines():
                if pattern not in line:
                    continue
                info = subprocess.run(["xwininfo", "-id", line.split()[0]],
                                      capture_output=True, text=True,
                                      timeout=10).stdout
                if "Map State: IsViewable" in info:
                    return True
        except (OSError, subprocess.SubprocessError, IndexError):
            pass
        time.sleep(0.1)
    sys.stderr.write(
        f"UI_SMOKE_FAIL: no viewable window {pattern} within {timeout}s\n")
    return False


def wait_until(stat, predicate, timeout, label):
    """Like wait_until_quiet but emits UI_SMOKE_FAIL on timeout. Use
    only when timeout is fatal at the call site (no retry above)."""
    if wait_until_quiet(stat, predicate, timeout):
        return True
    sys.stderr.write(f"UI_SMOKE_FAIL: timeout waiting for {label} after {timeout}s\n")
    return False


# A GUI acts on the state it last polled, so under load it can undo a
# state the driver just set: gmoccapy has turned the machine off in the
# middle of homing. Such an attempt is retried.
HOME_RETRY_BUDGET = 6
# The sims home in a second or two; cap the per-attempt wait so a
# reverted attempt does not sit out the full caller timeout.
HOME_ATTEMPT_TIMEOUT_S = 15.0


def home_all(cmd, stat, timeout):
    """Home every joint via c.home(-1) (respects HOME_SEQUENCE).
    Ensures ESTOP_RESET + ON + MANUAL and retries the whole sequence
    (see HOME_RETRY_BUDGET)."""
    attempt_timeout = min(timeout, HOME_ATTEMPT_TIMEOUT_S)
    deadline = time.monotonic() + timeout
    for attempt in range(1, HOME_RETRY_BUDGET + 1):
        # Task reports ESTOP_RESET only while the machine is off, and an
        # ESTOP_RESET command does not turn it off, so requesting it on a
        # retry after the machine came on would never be met.
        stat.poll()
        if stat.task_state != linuxcnc.STATE_ON:
            if not (ensure_state(cmd, stat, linuxcnc.STATE_ESTOP_RESET,
                                 "STATE_ESTOP_RESET") and
                    ensure_state(cmd, stat, linuxcnc.STATE_ON, "STATE_ON")):
                continue
        if not ensure_mode(cmd, stat, linuxcnc.MODE_MANUAL, "MODE_MANUAL"):
            continue
        cmd.teleop_enable(0)
        cmd.wait_complete()
        stat.poll()
        njoints = stat.joints
        cmd.home(-1)
        all_homed = lambda s: all(s.homed[i] for i in range(njoints))
        # Homing has stopped for good once the machine is off, or once it
        # was seen running and no joint is homing or homed any more: an
        # abort clears both on every joint, while between two steps of a
        # HOME_SEQUENCE some joint is already homed. gmoccapy's late
        # reactions to the machine coming on abort homing that way.
        seen = []
        def stopped(s):
            homing = any(s.joint[i]["homing"] for i in range(njoints))
            if homing:
                seen.append(True)
            return (s.task_state != linuxcnc.STATE_ON or
                    (seen and not homing and
                     not any(s.homed[i] for i in range(njoints))))
        done = wait_until_quiet(
            stat, lambda s: all_homed(s) or stopped(s),
            attempt_timeout) and all_homed(stat)
        # On a slow machine homing can still be running here. Homing
        # again would restart joints whose home switch another, already
        # homed joint now holds active (the gmoccapy sim shares one switch
        # between X and Z), so let a running sequence finish instead.
        if not done and any(stat.joint[i]["homing"] for i in range(njoints)):
            done = wait_until_quiet(
                stat,
                lambda s: all_homed(s) or not any(
                    s.joint[i]["homing"] for i in range(njoints)),
                max(0.0, deadline - time.monotonic())) and all_homed(stat)
        if done:
            cmd.teleop_enable(1)
            cmd.wait_complete()
            return True
        sys.stderr.write(
            f"WARN: homing did not complete on attempt {attempt}, retrying\n")
    sys.stderr.write(
        f"UI_SMOKE_FAIL: homing did not complete across "
        f"{HOME_RETRY_BUDGET} attempts\n")
    return False


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
    """Issue c.state(target_state) and wait for stat.task_state to reach
    target_state, up to STATE_RETRY_BUDGET times. A GUI can still undo
    it later; the home_all and run_program retry loops catch that.
    Returns False (with a WARN line) on exhausted budget, so the caller
    can start over. ON cannot be reached from ESTOP, so a GUI's ESTOP
    ends the wait for ON at once."""
    give_up = lambda s: (target_state == linuxcnc.STATE_ON and
                         s.task_state == linuxcnc.STATE_ESTOP)
    for attempt in range(1, STATE_RETRY_BUDGET + 1):
        cmd.state(target_state)
        cmd.wait_complete()
        if wait_until_quiet(
                stat, lambda s: s.task_state == target_state or give_up(s),
                ENSURE_ATTEMPT_TIMEOUT_S) and not give_up(stat):
            return True
        if give_up(stat):
            sys.stderr.write(f"WARN: {label} undone by ESTOP, starting over\n")
            return False
        sys.stderr.write(
            f"WARN: {label} not reached on attempt {attempt}, retrying\n")
    return False


def ensure_mode(cmd, stat, target_mode, label):
    """Same retry pattern as ensure_state, for task_mode. A GUI can
    switch the mode back right after the driver's request (qtdragon and
    gmoccapy switch to MANUAL once they see all-homed); a command newer
    than the driver's with the mode not as requested shows that, so ask
    again at once."""
    for attempt in range(1, STATE_RETRY_BUDGET + 1):
        cmd.mode(target_mode)
        cmd.wait_complete()
        serial = cmd.serial
        overridden = lambda s: (s.echo_serial_number > serial and
                                s.task_mode != target_mode)
        if wait_until_quiet(
                stat, lambda s: s.task_mode == target_mode or overridden(s),
                ENSURE_ATTEMPT_TIMEOUT_S) and not overridden(stat):
            return True
        sys.stderr.write(
            f"WARN: {label} not reached on attempt {attempt}, retrying\n")
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
    IDLE; we then read stat.position at (0,0,0). Quiet on timeout so
    run_program can retry; the caller decides when it is fatal. Gives
    up at once when a GUI has switched out of AUTO or turned the machine
    off, either of which rejects the run."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        _watchdog()
        stat.poll()
        if stat.interp_state != linuxcnc.INTERP_IDLE:
            return True
        if (stat.task_mode != linuxcnc.MODE_AUTO or
                stat.task_state != linuxcnc.STATE_ON):
            return False
        time.sleep(POLL_INTERVAL_S)
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


# Retry budget for the program run. Same GUI-revert hole as homing
# (see HOME_RETRY_BUDGET): a revert to MANUAL after ensure_mode leaves
# AUTO_RUN rejected or aborts the run, so retry from the mode change.
RUN_RETRY_BUDGET = 3


def load_and_settle(cmd, ngc_path, post_home_settle):
    """Open the program after homing: by then every GUI has opened its
    own startup file (axis, touchy), which an earlier open collides
    with. Then pause post_home_settle before AUTO.

    gmoccapy makes AUTO selectable only once its own status poll has
    processed all-homed, and answers an earlier AUTO with an abort and a
    warning that stays on screen. It shows nothing the driver could wait
    on (no pin; under load stale reactions send commands too), so give
    it this long; a rejection that still happens is retried. The same
    pause lets its status poll see the new file while the interp is
    IDLE; seen only at AUTO_RUN, the 'file-loaded' emit waits for the
    program end and gmoccapy shows a bogus progress percentage."""
    cmd.program_open(ngc_path)
    cmd.wait_complete()
    time.sleep(post_home_settle)


def run_program(cmd, stat, ngc_path, expect_delta_mm, tol, run_timeout,
                post_home_settle):
    """Home, load ngc, snapshot position, run ngc, verify (final -
    start) delta matches expect_delta_mm converted to machine units."""
    if not home_all(cmd, stat, 60.0):
        return False
    load_and_settle(cmd, ngc_path, post_home_settle)

    for attempt in range(1, RUN_RETRY_BUDGET + 1):
        # A GUI can turn the machine off after homing (see
        # HOME_RETRY_BUDGET). The joints stay homed, so unhome them and
        # home again: gmoccapy offers AUTO only after it sees all-homed.
        stat.poll()
        if stat.task_state != linuxcnc.STATE_ON:
            sys.stderr.write("WARN: machine went off after homing, homing again\n")
            cmd.unhome(-1)
            cmd.wait_complete()
            if not (wait_until(stat,
                               lambda s: not any(s.homed[:s.joints]),
                               ENSURE_ATTEMPT_TIMEOUT_S, "unhomed") and
                    home_all(cmd, stat, 60.0)):
                return False
            load_and_settle(cmd, ngc_path, post_home_settle)

        if not ensure_mode(cmd, stat, linuxcnc.MODE_AUTO, "MODE_AUTO"):
            continue

        # Snapshot start position AFTER homing + AFTER mode transition.
        # The GUI might re-issue mode commands during its own startup;
        # doing the snapshot last means we record the position right
        # before AUTO_RUN.
        stat.poll()
        start_pos = stat.position[:3]

        # No wait_complete after auto(AUTO_RUN, 0): wait_complete blocks
        # until the operation finishes, which for AUTO_RUN means the
        # whole program completes. That would race wait_program_started;
        # by the time we polled, interp would already be back at IDLE.
        cmd.auto(linuxcnc.AUTO_RUN, 0)

        if not wait_program_started(stat, PROGRAM_START_TIMEOUT_S):
            if attempt < RUN_RETRY_BUDGET:
                sys.stderr.write(
                    f"WARN: program did not start on attempt {attempt}, retrying\n")
            continue

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
        # A GUI that rejects AUTO after the run began aborts it and goes
        # back to MANUAL (gmoccapy); run it again from where it stopped.
        if (any(e > tol for e in err) and
                stat.task_mode != linuxcnc.MODE_AUTO and
                attempt < RUN_RETRY_BUDGET):
            sys.stderr.write(
                f"WARN: run aborted by a mode change on attempt {attempt}, "
                f"retrying\n")
            continue
        break
    else:
        stat.poll()
        sys.stderr.write(
            f"UI_SMOKE_FAIL: program did not start across "
            f"{RUN_RETRY_BUDGET} attempts state: {snapshot(stat)}\n")
        return False

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
    ap.add_argument("--gui-comp", metavar="NAME",
                    help="HAL component of the GUI; wait for it to be ready "
                         "before driving the machine")
    ap.add_argument("--gui-window", metavar="WM_CLASS",
                    help="WM_CLASS of the GUI's main window as xwininfo shows "
                         "it; wait for the window to be viewable")
    ap.add_argument("--post-home-settle", type=float, default=0.0,
                    metavar="SECONDS",
                    help="pause after homing for a GUI that offers AUTO only "
                         "once it has seen all joints homed (gmoccapy)")
    args = ap.parse_args()

    if args.run_program and args.expect_delta_mm is None:
        ap.error("--run-program requires --expect-delta-mm DX,DY,DZ")

    global _WATCH_PID
    _WATCH_PID = _read_pid("linuxcnc.pid")

    try:
        cmd, stat = connect_and_wait_ready(CONNECT_TIMEOUT_S)
        if cmd is None:
            return 1

        if args.gui_comp and not wait_gui_ready(args.gui_comp,
                                                GUI_READY_TIMEOUT_S):
            return 1
        if args.gui_window and not wait_gui_window(args.gui_window,
                                                   GUI_READY_TIMEOUT_S):
            return 1

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
                               args.tol, args.run_timeout,
                               args.post_home_settle):
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
