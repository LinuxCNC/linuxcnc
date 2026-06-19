#!/bin/bash
# Native crash capture for the UI smoke launchers. A GUI segfault lands in
# C/C++ (Qt, dbus, GL); PYTHONFAULTHANDLER (set in launch-env.sh) prints a
# Python traceback to linuxcnc.err naming the frame that called in, which
# is the reliable, environment-independent crash signal and is surfaced in
# every failure log. This helper adds a best-effort native backtrace on
# top, but only when runtests is given -d (ENABLE_CRASHDUMPS=1): arm a core
# dump before launch, and after the run, if a readable core from this run is
# present, gdb-print its backtrace. The core only materialises when we can
# point kernel.core_pattern at a writable dir, which needs root, so we do it
# via sudo (CI has passwordless sudo; the suite never runs as root since
# linuxcnc refuses to). It is opt-in because that sudo changes a global
# system setting; runtests -u further skips the sudo. Without -d the native
# backtrace is off and only the Python traceback remains. Source with
# LIB_DIR set; runs only on the failure path, so green runs pay nothing.

crashdump_arm() {
    # Off unless runtests was given -d: arming changes a global system
    # setting (kernel.core_pattern) via sudo, so it is opt-in. The Python
    # faulthandler traceback does not depend on this and is always present.
    [ "${ENABLE_CRASHDUMPS:-0}" = "1" ] || return 0
    CORE_DIR="$(mktemp -d -t ui-smoke-cores.XXXXXX)"
    export CORE_DIR
    ulimit -c unlimited 2>/dev/null || true
    # core_pattern is global; point it at our writable dir via sudo so the
    # kernel writes a core we can read. Skip under runtests -u to respect a
    # user opting out of root tweaks; if sudo is unavailable the core lands
    # wherever the system core_pattern says and we fall back to the Python
    # traceback. The id -u check is dropped: tests never run as root.
    if [ "${RUNTESTS_NOSUDO:-0}" != "1" ]; then
        sudo sysctl -w "kernel.core_pattern=$CORE_DIR/core.%e.%p" >/dev/null 2>&1 || true
    fi
}

crashdump_report() {
    [ "${ENABLE_CRASHDUMPS:-0}" = "1" ] || return 0
    [ -n "${CORE_DIR:-}" ] || return 0
    local c core=""
    # Only trust a core we know is from this run and can actually read:
    # one the kernel wrote into our fresh CORE_DIR (root path, where we set
    # core_pattern), or a relative "core" in the cwd that postdates arming.
    # A broad /tmp glob would pick up a stale or foreign core (often root-
    # owned), and gdb would just print "Permission denied".
    for c in "$CORE_DIR"/core*; do
        [ -e "$c" ] && [ -r "$c" ] && { core="$c"; break; }
    done
    if [ -z "$core" ]; then
        for c in ./core*; do
            [ -e "$c" ] && [ -r "$c" ] && [ "$c" -nt "$CORE_DIR" ] && { core="$c"; break; }
        done
    fi
    if [ -n "$core" ] && command -v gdb >/dev/null 2>&1; then
        echo "=== crash: native backtrace ($core) ==="
        # "bt" first: gdb auto-selects the faulting thread on a SIGSEGV
        # core. "thread apply all bt" after gives the rest.
        gdb -batch -nx \
            -ex "bt" \
            -ex "echo \n=== all threads ===\n" \
            -ex "thread apply all bt" \
            "$(command -v python3)" "$core" 2>&1 | head -400
    else
        # No readable core (the common non-root case). The Python
        # faulthandler traceback in linuxcnc.err already names the crash
        # site; the native backtrace is only a best-effort extra.
        echo "=== crash: no readable core dump; see the Python traceback in linuxcnc.err above ==="
    fi
    rm -rf "$CORE_DIR"
}
