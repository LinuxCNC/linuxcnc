#!/bin/bash
# drive.sh — run ONE G-code program through gomc-server headlessly and capture
# the motctl command stream (the "motion oracle" for milltask parity testing).
#
#   drive.sh <base-ini> <ngc-file> <out-log>
#
# <base-ini>  an INI whose [HAL] loads milltask + motmod (e.g. parity3.ini).
# <ngc-file>  path to the G-code file to run (absolute, or relative to CWD).
# <out-log>   where to write the captured motctl log.
#
# Requires motmod built with the parity trace compiled in, i.e.
#   make ../cmod/motmod.so EXTRA_CFLAGS=-DMILLTASK_PARITY_TRACE
# (motcmd_trace in src/emc/motion/command.c; off by default). See README.md.
#
# The same script drives whichever milltask is active — the Go gomod (default)
# or the C++ cmod (when cmod/milltask.so is present). That is what makes the
# two logs directly comparable. See run-parity.sh.
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
HALCMD="$ROOT/bin/halcmd"
SERVER="$ROOT/bin/gomc-server"

BASE_INI="${1:?usage: drive.sh <base-ini> <ngc> <out-log>}"
NGC="${2:?}"
OUT="${3:?}"

CFGDIR="$(cd "$(dirname "$BASE_INI")" && pwd)"
NGC_ABS="$(cd "$(dirname "$NGC")" && pwd)/$(basename "$NGC")"

# Per-run INI: base + OPEN_FILE + NO_FORCE_HOMING (so AUTO RUN needs no homing).
RUN_INI="$CFGDIR/_parity_run.ini"
awk -v ngc="$NGC_ABS" '
  /^\[DISPLAY\]/ { print; print "OPEN_FILE = " ngc; next }
  /^\[TRAJ\]/    { print; print "NO_FORCE_HOMING = 1"; next }
  /^OPEN_FILE/   { next }
  /^NO_FORCE_HOMING/ { next }
  { print }
' "$BASE_INI" > "$RUN_INI"

set +u; source "$ROOT/scripts/rip-environment" >/dev/null 2>&1; set -u

pkill -9 -f "gomc-server .*_parity_run.ini" >/dev/null 2>&1
sleep 0.3
rm -f "$OUT"

MOTCTL_LOG="$OUT" "$SERVER" "$RUN_INI" >"$OUT.srvout" 2>&1 &
SRV=$!
cleanup() { kill "$SRV" 2>/dev/null; sleep 0.5; pkill -9 -f "gomc-server .*_parity_run.ini" 2>/dev/null; rm -f "$RUN_INI"; }
trap cleanup EXIT

hc() { "$HALCMD" "$@" 2>/dev/null; }
# bit-pin value: pull the TRUE/FALSE token from the data row.
pin() { "$HALCMD" show pin "$1" 2>/dev/null | grep -oE 'TRUE|FALSE' | head -1; }

# Wait for the REST API to come up.
for i in $(seq 1 50); do
  hc show pin halui.machine.is-on >/dev/null 2>&1 && break
  kill -0 "$SRV" 2>/dev/null || { echo "drive.sh: server exited early; see $OUT.srvout" >&2; exit 2; }
  sleep 0.3
done

pulse() { hc setp "$1" 1 >/dev/null 2>&1; sleep 0.25; hc setp "$1" 0 >/dev/null 2>&1; }

pulse halui.estop.reset
pulse halui.machine.on
for i in $(seq 1 20); do [ "$(pin halui.machine.is-on)" = "TRUE" ] && break; sleep 0.2; done
[ "$(pin halui.machine.is-on)" = "TRUE" ] || { echo "drive.sh: machine did not turn on" >&2; exit 3; }

hc setp halui.mode.auto 1 >/dev/null 2>&1
for i in $(seq 1 20); do [ "$(pin halui.mode.is-auto)" = "TRUE" ] && break; sleep 0.2; done

pulse halui.program.run

# Wait for the program to finish: allow it to start, then wait for idle.
sleep 0.6
for i in $(seq 1 200); do            # up to ~60s
  [ "$(pin halui.program.is-idle)" = "TRUE" ] && break
  kill -0 "$SRV" 2>/dev/null || break
  sleep 0.3
done
sleep 0.3    # flush trailing commands

echo "drive.sh: captured $(wc -l < "$OUT" 2>/dev/null) motion commands -> $OUT" >&2
