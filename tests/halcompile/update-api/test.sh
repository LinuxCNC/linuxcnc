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

# writes to pins/params in EXTRA_SETUP become setters on references that
# halcompile initializes only after extra_setup() runs, so the converted
# component would crash at load: the conversion must still happen and a
# warning must be issued.  Writes in EXTRA_CLEANUP and FUNCTION must not warn.
cat > extrasetup.comp <<'EOF'
component extrasetup;
pin out bit flag;
param rw s32 level;
function _;
license "GPL";
;;
EXTRA_SETUP(){
    flag = 1;
    level = 5;
}
EXTRA_CLEANUP(){
    level = 0;
}
FUNCTION(_) {
    level = 0;
}
EOF
halcompupdate -i --no-backup extrasetup.comp 2>setup-warnings.txt
if ! grep -q "flag_set(1)" extrasetup.comp || ! grep -q "level_set(5)" extrasetup.comp; then
    echo "EXTRA_SETUP writes were not converted"
    exit 1
fi
for pat in "pin 'flag' is written in EXTRA_SETUP" "param 'level' is written in EXTRA_SETUP"; do
    if ! grep -q "$pat" setup-warnings.txt; then
        echo "expected warning missing: $pat"
        exit 1
    fi
done
if [ "$(grep -c "POST_EXPORT()" setup-warnings.txt)" != "2" ]; then
    echo "warnings should point at POST_EXPORT()"
    exit 1
fi
if [ "$(grep -c "is written in EXTRA_SETUP" setup-warnings.txt)" != "2" ]; then
    echo "EXTRA_CLEANUP or FUNCTION write triggered a spurious warning"
    exit 1
fi

# legacy hal_*_t types: in bodies the volatile qualifier is preserved
# (semantics-identical); in 'variable' declarations it cannot be
# expressed, so the declaration is left unchanged with a warning.
cat > voltest.comp <<'EOF'
component voltest;
pin out float out0;
variable hal_bit_t vflag;
variable float bias;
function _;
license "GPL";
;;
FUNCTION(_) {
    hal_bit_t local = 0;
    hal_float_t acc = bias;
    out0 = acc;
}
EOF
halcompupdate -i --no-backup voltest.comp 2>vol-warnings.txt
if ! grep -q "volatile rtapi_bool local" voltest.comp; then
    echo "body hal_bit_t did not keep its volatile qualifier"
    exit 1
fi
if ! grep -q "volatile rtapi_real acc" voltest.comp; then
    echo "body hal_float_t did not keep its volatile qualifier"
    exit 1
fi
if ! grep -q "variable rtapi_real bias" voltest.comp; then
    echo "plain float variable was not converted"
    exit 1
fi
if ! grep -q "variable hal_bit_t vflag" voltest.comp; then
    echo "variable hal_bit_t was modified; it must be left for manual review"
    exit 1
fi
if ! grep -q "variable 'vflag' uses legacy HAL type 'hal_bit_t'" vol-warnings.txt; then
    echo "expected warning missing for variable hal_bit_t"
    exit 1
fi
for pat in "'hal_bit_t' converted to 'volatile rtapi_bool'" \
           "'hal_float_t' converted to 'volatile rtapi_real'"; do
    if ! grep -q "$pat" vol-warnings.txt; then
        echo "expected volatile-kept warning missing: $pat"
        exit 1
    fi
done
if ! grep -q "construct(s) left for manual review" vol-warnings.txt; then
    echo "closing summary missing"
    exit 1
fi
# fix the reported variable by hand, then the component must compile
sed -i 's/variable hal_bit_t vflag/variable rtapi_bool vflag/' voltest.comp
if ! halcompile --compile voltest.comp >vol-compile.log 2>&1; then
    echo "volatile-converted component does not compile"
    cat vol-compile.log
    exit 1
fi

exit 0
