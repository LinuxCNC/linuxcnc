#!/bin/bash
# Integration tests for hal-go package

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HAL_GO_DIR="$(dirname "$SCRIPT_DIR")"
TOP_DIR="$(cd "$HAL_GO_DIR/../../.." && pwd)"
PASSTHROUGH="$TOP_DIR/bin/passthrough"
STR_SENDER="$TOP_DIR/bin/str-sender"
STR_RECEIVER="$TOP_DIR/bin/str-receiver"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Temporary file for HAL commands
TMPHAL=""

cleanup() {
    if [[ -n "$TMPHAL" && -f "$TMPHAL" ]]; then
        rm -f "$TMPHAL"
    fi
}
trap cleanup EXIT

pass() {
    echo -e "${GREEN}PASS${NC}: $1"
    : $((TESTS_PASSED++))
}

fail() {
    echo -e "${RED}FAIL${NC}: $1 - $2"
    : $((TESTS_FAILED++))
}

# Create temp hal file and run it
run_hal() {
    TMPHAL=$(mktemp --suffix=.hal)
    cat > "$TMPHAL"
    halrun -f "$TMPHAL" 2>&1
    local result=$?
    rm -f "$TMPHAL"
    TMPHAL=""
    return $result
}

# Check prerequisites
if ! command -v halrun &> /dev/null; then
    echo "ERROR: halrun not found. Please source scripts/rip-environment first."
    exit 1
fi

if [[ ! -x "$PASSTHROUGH" ]]; then
    echo "ERROR: passthrough not found at $PASSTHROUGH"
    echo "Please build with 'make' first."
    exit 1
fi

SKIP_STR_E2E=false
if [[ ! -x "$STR_SENDER" ]]; then
    echo "WARNING: str-sender not found at $STR_SENDER - skipping string e2e test"
    SKIP_STR_E2E=true
fi
if [[ ! -x "$STR_RECEIVER" ]]; then
    echo "WARNING: str-receiver not found at $STR_RECEIVER - skipping string e2e test"
    SKIP_STR_E2E=true
fi

echo "============================================"
echo "hal-go Integration Tests"
echo "============================================"
echo ""

# Test 1: Component loads successfully
echo "Running: Component loads"
: $((TESTS_RUN++))
OUTPUT=$(run_hal <<EOF
loadusr -W $PASSTHROUGH
show comp passthrough
unload passthrough
EOF
) && pass "Component loads" || fail "Component loads" "halrun failed"

# Test 2: All pins are created
echo "Running: Pin creation (10 pins)"
: $((TESTS_RUN++))
OUTPUT=$(run_hal <<EOF
loadusr -W $PASSTHROUGH
show pin passthrough.*
unload passthrough
EOF
)
PIN_COUNT=$(echo "$OUTPUT" | grep -c 'passthrough\.' || true)
if [[ "$PIN_COUNT" -eq 10 ]]; then
    pass "Pin creation (10 pins)"
else
    fail "Pin creation (10 pins)" "Expected 10 pins, got $PIN_COUNT"
fi

# Test 3: Float passthrough works
echo "Running: Float passthrough"
: $((TESTS_RUN++))
OUTPUT=$(run_hal <<EOF
loadusr -W $PASSTHROUGH
setp passthrough.in-float 123.456
loadusr -w sleep 0.2
show pin passthrough.out-float
unload passthrough
EOF
)
if echo "$OUTPUT" | grep -q '123.456'; then
    pass "Float passthrough"
else
    fail "Float passthrough" "Value not passed through"
fi

# Test 4: Bit passthrough works
echo "Running: Bit passthrough"
: $((TESTS_RUN++))
OUTPUT=$(run_hal <<EOF
loadusr -W $PASSTHROUGH
setp passthrough.in-bit true
loadusr -w sleep 0.2
show pin passthrough.out-bit
unload passthrough
EOF
)
if echo "$OUTPUT" | grep -q 'TRUE'; then
    pass "Bit passthrough"
else
    fail "Bit passthrough" "Value not passed through"
fi

# Test 5: S32 passthrough works
echo "Running: S32 passthrough"
: $((TESTS_RUN++))
OUTPUT=$(run_hal <<EOF
loadusr -W $PASSTHROUGH
setp passthrough.in-s32 -42
loadusr -w sleep 0.2
show pin passthrough.out-s32
unload passthrough
EOF
)
if echo "$OUTPUT" | grep -q '\-42'; then
    pass "S32 passthrough"
else
    fail "S32 passthrough" "Value not passed through"
fi

# Test 6: U32 passthrough works
echo "Running: U32 passthrough"
: $((TESTS_RUN++))
OUTPUT=$(run_hal <<EOF
loadusr -W $PASSTHROUGH
setp passthrough.in-u32 0xDEADBEEF
loadusr -w sleep 0.2
show pin passthrough.out-u32
unload passthrough
EOF
)
if echo "$OUTPUT" | grep -qi 'DEADBEEF'; then
    pass "U32 passthrough"
else
    fail "U32 passthrough" "Value not passed through"
fi

# Test 7: String port pins have correct type
echo "Running: String port pin types"
: $((TESTS_RUN++))
OUTPUT=$(run_hal <<EOF
loadusr -W $PASSTHROUGH
show pin passthrough.in-str
show pin passthrough.out-str
unload passthrough
EOF
)
if echo "$OUTPUT" | grep -q 'passthrough.in-str' && echo "$OUTPUT" | grep -q 'passthrough.out-str' && echo "$OUTPUT" | grep -qE '\bport\b'; then
    pass "String port pin types"
else
    fail "String port pin types" "Port pins not found or type incorrect"
fi

# Test 8: String port signal linking
echo "Running: String port signal linking"
: $((TESTS_RUN++))
OUTPUT=$(run_hal <<EOF
loadusr -W $PASSTHROUGH
newsig test-str port
net test-str passthrough.out-str
show sig test-str
unload passthrough
EOF
)
if echo "$OUTPUT" | grep -q 'test-str' && echo "$OUTPUT" | grep -q 'passthrough.out-str'; then
    pass "String port signal linking"
else
    fail "String port signal linking" "Signal not created or not linked"
fi

# Test 9: String port buffer allocation
echo "Running: String port buffer allocation"
: $((TESTS_RUN++))
OUTPUT=$(run_hal <<EOF
loadusr -W $PASSTHROUGH
newsig test-str port
net test-str passthrough.out-str
sets test-str 1024
show sig test-str
unload passthrough
EOF
) && pass "String port buffer allocation" || fail "String port buffer allocation" "Buffer allocation failed"

# Test 10: String end-to-end passthrough
if [[ "$SKIP_STR_E2E" != "true" ]]; then
    echo "Running: String end-to-end passthrough"
    : $((TESTS_RUN++))
    OUTPUT=$(run_hal <<EOF
loadusr -W $STR_SENDER
loadusr -W $STR_RECEIVER
newsig test-msg port
net test-msg str-sender.out str-receiver.in
sets test-msg 1024
loadusr -w sleep 0.5
unload str-sender
unload str-receiver
EOF
    )
    if echo "$OUTPUT" | grep -q 'RECEIVED:hello from go'; then
        pass "String end-to-end passthrough"
    else
        fail "String end-to-end passthrough" "Expected 'RECEIVED:hello from go' in output"
    fi
fi

# Test 11: Clean unload (SIGTERM)
echo "Running: Clean unload (SIGTERM)"
: $((TESTS_RUN++))
OUTPUT=$(run_hal <<EOF
loadusr -W $PASSTHROUGH
unload passthrough
EOF
) && pass "Clean unload (SIGTERM)" || fail "Clean unload (SIGTERM)" "unload failed"

# Test 12: No zombie processes after unload
echo "Running: No zombie processes"
: $((TESTS_RUN++))
run_hal <<EOF
loadusr -W $PASSTHROUGH
unload passthrough
EOF
sleep 0.5
if ! pgrep -f 'passthrough' > /dev/null 2>&1; then
    pass "No zombie processes"
else
    fail "No zombie processes" "Process still running"
fi

echo ""
echo "============================================"
echo "Results: $TESTS_PASSED/$TESTS_RUN passed"
echo "============================================"

if [[ $TESTS_FAILED -gt 0 ]]; then
    exit 1
fi
exit 0
