#!/bin/bash
# Test: G5x offsets from var file are applied to motion commands.
# Verifies that the Go milltask reads PARAMETER_FILE from the INI,
# loads the var file, and applies G54 X offset to G0 X0.
#
# Expected: joint.0.motor-pos-cmd ≈ -80.76 (G54 X offset from sim_mm.var)
# Previously broken: motor-pos-cmd was 0 (offsets not loaded).

set -e

LINUXCNC_DIR="$(cd "$(dirname "$0")/.." && pwd)"
INI="configs/sim/axis/axis_mm.ini"
VAR_FILE="configs/sim/axis/sim_mm.var"
API="http://127.0.0.1:5080/api/v1/milltask"
TIMEOUT=10
PASS=0
FAIL=0

cleanup() {
    if [[ -n "$SERVER_PID" ]]; then
        kill "$SERVER_PID" 2>/dev/null
        wait "$SERVER_PID" 2>/dev/null
    fi
    # Restore milltask.so if we moved it
    if [[ -f "$LINUXCNC_DIR/cmod/milltask.so.test_bak" ]]; then
        mv "$LINUXCNC_DIR/cmod/milltask.so.test_bak" "$LINUXCNC_DIR/cmod/milltask.so"
    fi
}
trap cleanup EXIT

die() { echo "FAIL: $1" >&2; exit 1; }

api_post() {
    local path="$1" data="$2"
    local resp
    resp=$(curl -s -w '\n%{http_code}' -X POST "${API}/${path}" \
        -H 'Content-Type: application/json' -d "$data")
    local code=$(echo "$resp" | tail -1)
    local body=$(echo "$resp" | head -1)
    if [[ "$code" != "200" ]]; then
        echo "  API ${path} returned HTTP $code: $body" >&2
        return 1
    fi
    echo "$body"
}

wait_for_server() {
    local i=0
    while ! curl -s -o /dev/null "$API/state" 2>/dev/null; do
        sleep 1
        i=$((i + 1))
        if [[ $i -ge $TIMEOUT ]]; then
            die "Server did not start within ${TIMEOUT}s"
        fi
    done
}

echo "=== G5x Offset Integration Test ==="
echo ""

# --- Setup ---
cd "$LINUXCNC_DIR"

# Ensure Go milltask is used (remove C plugin if present)
if [[ -f "cmod/milltask.so" ]]; then
    echo "Moving cmod/milltask.so aside to use Go milltask..."
    mv cmod/milltask.so cmod/milltask.so.test_bak
fi

# Show the expected offset from var file
G54_X=$(awk '$1 == 5221 { print $2 }' "$VAR_FILE")
echo "G54 X offset in var file: $G54_X"
echo ""

# --- Start server ---
echo "Starting gomc-server..."
scripts/linuxcnc "$INI" > /tmp/test_g5x.log 2>&1 &
SERVER_PID=$!
wait_for_server
echo "Server ready (PID $SERVER_PID)"
echo ""

# --- Test sequence ---
echo "Step 1: Estop Reset"
res=$(api_post "state" '{"state":2}') || die "estop-reset failed"
echo "  Response: $res"

echo "Step 2: Machine On"
res=$(api_post "state" '{"state":4}') || die "machine-on failed"
echo "  Response: $res"

sleep 0.5

echo "Step 3: Home All Joints"
res=$(api_post "home" '{"joint":-1}') || die "home failed"
echo "  Response: $res"

sleep 3

echo "Step 4: Set MDI Mode"
res=$(api_post "mode" '{"mode":3}') || die "set-mode failed"
echo "  Response: $res"

sleep 0.5

echo "Step 5: MDI 'G0 X0'"
res=$(api_post "mdi" '{"command":"G0 X0"}') || die "mdi failed"
echo "  Response: $res"

sleep 2

# --- Check result ---
echo ""
echo "=== Checking Result ==="
MOTOR_POS=$(halcmd getp joint.0.motor-pos-cmd 2>/dev/null | awk '{print $NF}')
echo "joint.0.motor-pos-cmd = $MOTOR_POS"

# Verify it's not zero (the bug was: offset not applied → position = 0)
if [[ "$MOTOR_POS" == "0" ]]; then
    echo "FAIL: motor-pos-cmd is 0 — G5x offset NOT applied!"
    FAIL=$((FAIL + 1))
else
    echo "PASS: motor-pos-cmd is non-zero ($MOTOR_POS) — G5x offset applied"
    PASS=$((PASS + 1))
fi

# Verify it's approximately equal to G54_X (within 2mm tolerance for home offset)
EXPECTED_APPROX=$(echo "$G54_X" | awk '{printf "%.0f", $1}')
ACTUAL_APPROX=$(echo "$MOTOR_POS" | awk '{printf "%.0f", $1}')
DIFF=$(echo "$EXPECTED_APPROX $ACTUAL_APPROX" | awk '{d=$1-$2; print (d<0?-d:d)}')

if [[ "$DIFF" -le 2 ]]; then
    echo "PASS: motor-pos-cmd ($MOTOR_POS) ≈ G54 X offset ($G54_X) within 2mm"
    PASS=$((PASS + 1))
else
    echo "FAIL: motor-pos-cmd ($MOTOR_POS) too far from G54 X offset ($G54_X), diff=${DIFF}mm"
    FAIL=$((FAIL + 1))
fi

echo ""
echo "=========================================="
echo "Results: $PASS passed, $FAIL failed"
echo "=========================================="

if [[ $FAIL -gt 0 ]]; then
    exit 1
fi
exit 0
