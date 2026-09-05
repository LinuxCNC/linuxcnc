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

# the names in the converted component contain no legacy type segments,
# so a re-run must not emit any rename note
if ! halcompupdate test_update_api.comp >/dev/null 2>rename-notes.txt; then
    echo "halcompupdate re-run failed"
    exit 1
fi
if grep -q "mentions legacy HAL type" rename-notes.txt; then
    echo "spurious rename note for names without type segments"
    exit 1
fi

# constructs that cannot be converted safely must be left unchanged and
# produce a warning
cat > tricky.comp <<'EOF'
component tricky;
pin out float out0;
pin io s32 count;
pin out s32 result-##[8];
pin out s32 out_s32;
pin out bit input_bit;
pin out float floating;
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

# a name spelling a legacy type (any of them, bit and float included)
# gets a gentle rename note with the new-style spelling suggested.  The
# note is not counted as manual-review work and does not change the
# file; the author decides whether the word really meant the type.  A
# word merely containing a type (floating) is not a segment and must
# not be noted.  The note must point at the line of the declaration,
# not of the statement before it.
if ! grep -q "tricky.comp:5: Note: pin name 'out_s32' mentions legacy HAL type 's32'" warnings.txt; then
    echo "expected rename note missing for out_s32 (or wrong line number)"
    exit 1
fi
if ! grep -q "consider renaming to 'out_si32'" warnings.txt; then
    echo "expected rename suggestion missing for out_s32"
    exit 1
fi
if ! grep -q "pin name 'input_bit' mentions legacy HAL type 'bit'" warnings.txt; then
    echo "expected rename note missing for input_bit"
    exit 1
fi
if ! grep -q "consider renaming to 'input_bool'" warnings.txt; then
    echo "expected rename suggestion missing for input_bit"
    exit 1
fi
if grep -q "name 'floating'" warnings.txt; then
    echo "spurious rename note for a word containing 'float'"
    exit 1
fi
if ! grep -q "2 name(s) mention a legacy HAL type (rename is optional)" warnings.txt; then
    echo "rename note count missing from the summary"
    exit 1
fi
# the note itself must not rename: the types are converted, the names stay
for pat in "pin out si32 out_s32;" "pin out bool input_bit;" "pin out real floating;"; do
    if ! grep -q "$pat" tricky.comp; then
        echo "rename note must not modify the name: $pat"
        exit 1
    fi
done

# component and function names are HAL-visible too: the component name
# is the loadrt argument and the module name, functions are exported as
# comp.N.name.  They are noted the same way, and duplicated type
# segments are mentioned once.
cat > naming.comp <<'EOF'
component conv_s32_float;
pin out float value_s32;
function conv_s32_s32;
license "GPL";
;;
FUNCTION(conv_s32_s32) {
    value_s32 = 1.0;
}
EOF
halcompupdate -i --no-backup naming.comp 2>name-notes.txt
if ! grep -q "naming.comp:1: Note: component name 'conv_s32_float'" name-notes.txt; then
    echo "expected component name note missing (or wrong line number)"
    exit 1
fi
if ! grep -q "consider renaming to 'conv_si32_real'" name-notes.txt; then
    echo "expected component rename suggestion missing"
    exit 1
fi
if ! grep -q "naming.comp:2: Note: pin name 'value_s32'" name-notes.txt; then
    echo "expected pin name note missing (or wrong line number)"
    exit 1
fi
if ! grep -q "naming.comp:3: Note: function name 'conv_s32_s32' mentions legacy HAL type 's32'" name-notes.txt; then
    echo "expected function name note missing (or duplicate segments not deduped)"
    exit 1
fi
if grep -q "'s32', 's32'" name-notes.txt; then
    echo "duplicate type segments were not deduped in the note"
    exit 1
fi
if ! grep -q "3 name(s) mention a legacy HAL type (rename is optional)" name-notes.txt; then
    echo "rename note count missing from the naming.comp summary"
    exit 1
fi
for pat in "component conv_s32_float;" "pin out real value_s32;" "function conv_s32_s32;"; do
    if ! grep -q "$pat" naming.comp; then
        echo "rename note must not modify the name: $pat"
        exit 1
    fi
done

# a fully converted component with stale names reports the note count
# only: no mechanical-change summary and no diff-review advice
cat > stale.comp <<'EOF'
component stale;
pin out real out_s32;
function _;
license "GPL";
;;
FUNCTION(_) {
    out_s32_set(1.0);
}
EOF
halcompupdate stale.comp >/dev/null 2>stale-notes.txt
if grep -q "mechanical change" stale-notes.txt; then
    echo "already-converted component must not report mechanical changes"
    exit 1
fi
if grep -q "review the diff" stale-notes.txt; then
    echo "already-converted component must not get diff-review advice"
    exit 1
fi
if ! grep -q "stale.comp: 1 name(s) mention a legacy HAL type (rename is optional)" stale-notes.txt; then
    echo "notes-only summary line missing"
    exit 1
fi
if ! grep -q "^stale.comp:2: Note: pin name 'out_s32'" stale-notes.txt; then
    echo "expected note with correct line number"
    exit 1
fi

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
