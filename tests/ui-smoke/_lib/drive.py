#!/usr/bin/env python3
# Minimal UI smoke driver: confirm linuxcnc task came up and the GUI
# did not crash. The smoke layer answers Bertho's "does it start"
# question only; functional behaviour (home, run a file, verify
# position) belongs in tests/ui-functional/ (Phase 2).

import linuxcnc
import sys
import time

CONNECT_TIMEOUT_S = 60.0
SETTLE_S = 3.0


def connect_and_wait_ready(timeout):
    """Wait until linuxcnc.stat().poll() returns without error and
    reports a non-negative echo_serial_number. The NML status buffer
    can be 'invalid err=3' for the first ~30s while linuxcncsvr is
    still initialising; recreate the stat object on every iteration so
    a stale invalid buffer does not stick after linuxcncsvr is ready."""
    deadline = time.monotonic() + timeout
    last_err = None
    while time.monotonic() < deadline:
        try:
            stat = linuxcnc.stat()
            stat.poll()
            if stat.echo_serial_number >= 0:
                return linuxcnc.command(), stat
        except linuxcnc.error as e:
            last_err = e
        time.sleep(0.5)
    sys.stderr.write(
        f"UI_SMOKE_FAIL: task not ready within {timeout}s "
        f"(last NML error: {last_err})\n")
    return None, None


def main():
    cmd, stat = connect_and_wait_ready(CONNECT_TIMEOUT_S)
    if cmd is None:
        return 1

    # Give the GUI process enough time to finish constructing itself
    # (load .ui files, compile resources.py if needed, etc.) and
    # settle. If the GUI was going to crash on startup it has crashed
    # by now.
    time.sleep(SETTLE_S)

    # Re-check task is still alive; a GUI crash may have torn linuxcnc
    # down via Cleanup.
    try:
        stat.poll()
    except linuxcnc.error as e:
        sys.stderr.write(f"UI_SMOKE_FAIL: task disappeared after GUI startup: {e}\n")
        return 1

    print("UI_SMOKE_OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
