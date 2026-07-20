#!/bin/sh
# Test halcompupdate migration of an old-style component to the new HAL API
set -e

workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT
cp test_update_api.comp "$workdir/"
cd "$workdir"

# dry-run must report changes via a diff, and not modify the file
if ! halcompupdate test_update_api.comp | grep -q '^+pin out real out0'; then
    echo "halcompupdate dry-run did not produce the expected diff"
    exit 1
fi
if grep -q "pin out real out0" test_update_api.comp; then
    echo "dry-run modified the input file"
    exit 1
fi

# --check must exit nonzero when conversion is needed
if halcompupdate --check test_update_api.comp 2>/dev/null; then
    echo "--check did not detect a convertible file"
    exit 1
fi

# convert in place
halcompupdate -i --no-backup test_update_api.comp 2>/dev/null

# declarations must be converted
for pat in "pin in real in0" "pin out real out0" "pin out bool flag" \
           "pin io si32 count" "pin out si32 result" "param rw real gain" \
           "param rw bool enable"; do
    if ! grep -q "$pat" test_update_api.comp; then
        echo "converted declaration missing: $pat"
        exit 1
    fi
done

# body writes must use setters (including compound assignments, increments,
# array pins and writes inside #define macros)
for pat in "out0_set(tmp)" "flag_set(tmp > 1.0)" \
           "count_set(count + 1)" "count_set(count + (1))" \
           "result_set(i, steps(i) \* 2)" \
           "flag_set(1)"; do
    if ! grep -q "$pat" test_update_api.comp; then
        echo "converted body missing: $pat"
        exit 1
    fi
done

# the converted component must compile
if ! halcompile --compile test_update_api.comp >compile.log 2>&1; then
    echo "converted component does not compile"
    cat compile.log
    exit 1
fi

# a second run must be a no-op
if ! halcompupdate --check test_update_api.comp 2>/dev/null; then
    echo "halcompupdate output is not stable on a second run"
    exit 1
fi

# constructs that cannot be converted safely must be left unchanged and
# produce a warning
cat > tricky.comp <<'EOF'
component tricky;
pin out float out0;
pin io s32 count;
pin out s32 result-##[8];
param rw float gain;
function _;
license "GPL";
;;
extern void helper(double*);
FUNCTION(_) {
    int old = count++;
    int i = 0;
    result(i++) += 2;
    (*out0_ptr)++;
    helper(&gain);
}
EOF
halcompupdate -i --no-backup tricky.comp 2>warnings.txt
for pat in "int old = count++;" "result(i++) += 2;" "(\*out0_ptr)++;" "helper(&gain);"; do
    if ! grep -q "$pat" tricky.comp; then
        echo "unsafe construct was modified: $pat"
        exit 1
    fi
done
for pat in "postfix ++" "index with side effects" "parenthesized dereference" "address of pin/param"; do
    if ! grep -q "$pat" warnings.txt; then
        echo "expected warning missing: $pat"
        exit 1
    fi
done

exit 0

