#!/bin/bash
# Native crash capture for the UI smoke launchers. A GUI segfault is the
# failure these tests most need to explain, and it lands in C/C++ (Qt,
# dbus, GL) where PYTHONFAULTHANDLER stops at the event-loop frame. Arm a
# core dump before launch; after the run, if the GUI left a core, print a
# native backtrace into the log so CI shows the faulting frame directly.
# Source with LIB_DIR set; runs only on the failure path, so green runs
# pay nothing.

crashdump_arm() {
    CORE_DIR="$(mktemp -d -t ui-smoke-cores.XXXXXX)"
    export CORE_DIR
    ulimit -c unlimited 2>/dev/null || true
    # core_pattern is global; only set it if already root. Never sudo:
    # the suite must run unattended. Non-root falls back to a cwd "core".
    if [ "$(id -u)" = 0 ]; then
        sysctl -w "kernel.core_pattern=$CORE_DIR/core.%e.%p" >/dev/null 2>&1 || true
    fi
}

crashdump_report() {
    [ -n "${CORE_DIR:-}" ] || return 0
    local core
    # shellcheck disable=SC2012  # mktemp dir, no odd filenames
    core=$(ls -t "$CORE_DIR"/core* ./core* /tmp/core* 2>/dev/null | head -1)
    if [ -n "$core" ]; then
        echo "=== crash: native backtrace ($core) ==="
        # gdb reads the core; pull it in if missing, only when root.
        if ! command -v gdb >/dev/null 2>&1 && [ "$(id -u)" = 0 ]; then
            apt-get install -y -q gdb >/dev/null 2>&1 || true
        fi
        if command -v gdb >/dev/null 2>&1; then
            # "bt" first: gdb auto-selects the faulting thread on a SIGSEGV
            # core. "thread apply all bt" after gives the rest.
            gdb -batch -nx \
                -ex "bt" \
                -ex "echo \n=== all threads ===\n" \
                -ex "thread apply all bt" \
                "$(command -v python3)" "$core" 2>&1 | head -400
        else
            echo "(gdb unavailable; core left at $core)"
        fi
    fi
    rm -rf "$CORE_DIR"
}
