#!/bin/bash
# Native crash capture for the UI smoke launchers. A GUI segfault lands in
# C/C++ (Qt, dbus, GL); PYTHONFAULTHANDLER (set in launch-env.sh) prints a
# Python traceback to linuxcnc.err naming the frame that called in, which is
# the reliable, environment-independent crash signal and is surfaced in every
# failure log. This helper adds a best-effort native backtrace on top when
# runtests is given -d (ENABLE_CRASHDUMPS=1): raise the core size limit
# before launch, and after the run, if a readable core from this run is
# found, gdb-print its backtrace. Cores are collected from wherever the
# system puts them: systemd-coredump via coredumpctl, or a plain-file
# kernel.core_pattern (the CI workflow points it at CORE_DIR with a job-level
# sudo sysctl on the disposable runner; local runs often just get ./core).
# No sudo and no global system changes here. Source with LIB_DIR set; the
# report runs only on the failure path, so green runs pay nothing.

crashdump_arm() {
    # Off unless runtests was given -d. The Python faulthandler traceback
    # does not depend on this and is always present.
    [ "${ENABLE_CRASHDUMPS:-0}" = "1" ] || return 0
    # CORE_DIR is where the CI workflow's core_pattern writes; it also
    # receives a core extracted via coredumpctl.
    CORE_DIR="${UI_SMOKE_CORE_DIR:-/tmp/linuxcnc-ui-smoke-cores}"
    mkdir -p "$CORE_DIR" 2>/dev/null || true
    export CORE_DIR
    ulimit -c unlimited 2>/dev/null || true
    crashdump_arm_time=$(date +%s)
    # systemd-coredump keeps the environment of the crashed process, and
    # its list is shared by every sandbox of a parallel run, so a core is
    # ours only when it carries this tag.
    UI_SMOKE_CRASH_TAG=$(cat /proc/sys/kernel/random/uuid)
    export UI_SMOKE_CRASH_TAG
}

crashdump_report() {
    [ "${ENABLE_CRASHDUMPS:-0}" = "1" ] || return 0
    [ -n "${CORE_DIR:-}" ] || return 0
    local c core=""
    # Only trust a core we know is from this run and can actually read: one
    # in CORE_DIR, a relative "core" in the cwd that postdates arming, or
    # one systemd-coredump logged since arming (coredumpctl needs no root
    # for our own processes). A broad /tmp glob would pick up a stale or
    # foreign core, and gdb would just print "Permission denied".
    for c in "$CORE_DIR"/core*; do
        [ -e "$c" ] && [ -r "$c" ] && { core="$c"; break; }
    done
    if [ -z "$core" ]; then
        for c in ./core*; do
            [ -e "$c" ] && [ -r "$c" ] && [ "$c" -nt "$CORE_DIR" ] && { core="$c"; break; }
        done
    fi
    # coredumpctl matches a path against the executable it recorded, which
    # is the versioned interpreter (/usr/bin/python3.13), not the python3
    # link or the GUI script name, so match the resolved path. The PID it
    # records is the host one, so find our core by its tag and dump that.
    local py pid
    py=$(readlink -f "$(command -v python3)")
    if [ -z "$core" ] && [ -n "$py" ] && command -v coredumpctl >/dev/null 2>&1; then
        pid=$(journalctl -q -a -o json --output-fields=COREDUMP_PID,COREDUMP_ENVIRON \
                --since "@$crashdump_arm_time" COREDUMP_EXE="$py" 2>/dev/null |
            grep -F "UI_SMOKE_CRASH_TAG=$UI_SMOKE_CRASH_TAG" | tail -n 1 |
            sed -n 's/.*"COREDUMP_PID":"\([0-9]*\)".*/\1/p')
        if [ -n "$pid" ]; then
            coredumpctl dump "$pid" --output="$CORE_DIR/core.coredumpctl" >/dev/null 2>&1 || true
            [ -s "$CORE_DIR/core.coredumpctl" ] && core="$CORE_DIR/core.coredumpctl"
        fi
    fi
    if [ -n "$core" ] && command -v gdb >/dev/null 2>&1; then
        echo "=== crash: native backtrace ($core) ==="
        # "bt" first: gdb auto-selects the faulting thread on a SIGSEGV
        # core. "thread apply all bt" after gives the rest.
        gdb -batch -nx \
            -ex "bt" \
            -ex "echo \n=== all threads ===\n" \
            -ex "thread apply all bt" \
            "${py:-python3}" "$core" 2>&1 | head -400
    else
        # No readable core. The Python faulthandler traceback in
        # linuxcnc.err already names the crash site; the native backtrace
        # is only a best-effort extra.
        echo "=== crash: no readable core dump; see the Python traceback in linuxcnc.err above ==="
    fi
    rm -rf "$CORE_DIR"
}
