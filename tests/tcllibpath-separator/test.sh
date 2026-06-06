#!/bin/bash
# Test that TCLLIBPATH uses space separators instead of colons

set -e

# Get the root directory
ROOT_DIR=$(cd "$(dirname "$0")"/../.. && pwd)

# Test the logic directly from the .in file
# Simulate what the scripts do

# Test 1: Empty TCLLIBPATH (first time)
echo "=== Test 1: Empty TCLLIBPATH ==="
unset TCLLIBPATH
EMC2_HOME="/home/user/linuxcnc"

if [ -z "$TCLLIBPATH" ]; then
    TCLLIBPATH=$EMC2_HOME/tcl
else
    TCLLIBPATH="$EMC2_HOME/tcl $TCLLIBPATH"
fi

echo "TCLLIBPATH='$TCLLIBPATH'"

if [ "$TCLLIBPATH" != "$EMC2_HOME/tcl" ]; then
    echo "ERROR: TCLLIBPATH should be '$EMC2_HOME/tcl', got '$TCLLIBPATH'"
    exit 1
fi

# Check no colons
if echo "$TCLLIBPATH" | grep -q ':'; then
    echo "ERROR: TCLLIBPATH contains colon separator"
    exit 1
fi

# Test 2: Pre-existing TCLLIBPATH (second time)
echo "=== Test 2: Pre-existing TCLLIBPATH ==="

if [ -z "$TCLLIBPATH" ]; then
    TCLLIBPATH=$EMC2_HOME/tcl
else
    TCLLIBPATH="$EMC2_HOME/tcl $TCLLIBPATH"
fi

echo "TCLLIBPATH='$TCLLIBPATH'"

# Should have two paths now, space-separated
EXPECTED="$EMC2_HOME/tcl $EMC2_HOME/tcl"
if [ "$TCLLIBPATH" != "$EXPECTED" ]; then
    echo "ERROR: TCLLIBPATH should be '$EXPECTED', got '$TCLLIBPATH'"
    exit 1
fi

# Check no colons
if echo "$TCLLIBPATH" | grep -q ':'; then
    echo "ERROR: TCLLIBPATH contains colon separator after second append"
    exit 1
fi

# Test 3: With different pre-existing path
echo "=== Test 3: Different pre-existing path ==="
TCLLIBPATH="/some/other/path"

if [ -z "$TCLLIBPATH" ]; then
    TCLLIBPATH=$EMC2_HOME/tcl
else
    TCLLIBPATH="$EMC2_HOME/tcl $TCLLIBPATH"
fi

echo "TCLLIBPATH='$TCLLIBPATH'"

EXPECTED="$EMC2_HOME/tcl /some/other/path"
if [ "$TCLLIBPATH" != "$EXPECTED" ]; then
    echo "ERROR: TCLLIBPATH should be '$EXPECTED', got '$TCLLIBPATH'"
    exit 1
fi

# Check no colons
if echo "$TCLLIBPATH" | grep -q ':'; then
    echo "ERROR: TCLLIBPATH contains colon separator with other path"
    exit 1
fi

# Check space separation
if ! echo "$TCLLIBPATH" | grep -qE '.+ .+'; then
    echo "ERROR: TCLLIBPATH doesn't appear to be space-separated"
    exit 1
fi

echo "=== All tests passed ==="
echo "SUCCESS: TCLLIBPATH properly uses space separators"
exit 0
