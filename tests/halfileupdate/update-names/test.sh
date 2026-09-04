#!/bin/sh
# Test halfileupdate conversion of HAL configuration files.  What the new
# names are is up to the rename tables in the tool, so the test checks that
# the old names are gone and that what replaces them is consistent
# everywhere, not how any one name is spelled.
set -e

workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT
cp core.hal extra.hal later.hal loop.tcl custom.hal machine.ini "$workdir/"
cd "$workdir"

fail () {
    echo "$1"
    exit 1
}

# a name whose component was loaded in another file cannot be converted
# from that file alone, and nothing at all may be written
halfileupdate later.hal > single.diff 2> single.log || \
    fail "single file run failed"
grep -q "no 'loadrt' for it was seen" single.log || \
    fail "single file run did not report the unresolved instance"
if test -s single.diff; then fail "single file run produced a diff"; fi

# dry run over the INI file: the diff has the conversions, no file changes
halfileupdate machine.ini > ini.diff 2> ini.log || fail "INI dry run failed"
grep -q '^+loadrt .* count=2' ini.diff || \
    fail "component rename missing from the diff"
grep -q 'conv-s32-float' core.hal || fail "dry run modified core.hal"

# a component that is removed with no replacement is reported, not rewritten
grep -q "removed with no replacement" ini.log || \
    fail "the removed component was not reported"

# convert, keeping the backups
halfileupdate -i machine.ini > /dev/null 2>&1
for f in core.hal extra.hal later.hal loop.tcl custom.hal machine.ini; do
    test -f "$f.bak" || fail "no backup kept for $f"
done

# the old names are gone from everything that is a HAL name
if grep -q 'conv-s32-float\.' core.hal extra.hal later.hal custom.hal; then
    fail "an old instance name survived"
fi
if grep -q 'in-s32-\|out-s32' core.hal extra.hal custom.hal loop.tcl; then
    fail "an old pin name survived"
fi

# the instance names follow the new component name, in every file, including
# one that only uses what an earlier file loaded
module=$(sed -n 's/^loadrt \([^ ]*\) count=2$/\1/p' core.hal)
test -n "$module" || fail "the count= loadrt line lost its component name"
cvt=$(echo "$module" | tr '_' '-')
grep -q "^addf $cvt.0 servo-thread\$" core.hal || fail "addf not converted"
grep -q "^net a-sig $cvt.0.in\$" core.hal || \
    fail "pin of a default instance not converted"
grep -q "^net d-sig $cvt.1.in " extra.hal || fail "sourced file not converted"
grep -q "^net e-sig $cvt.0.out\$" later.hal || \
    fail "postgui file not converted"

# a names= instance keeps the name the user gave it, only the loadrt changes
grep -q "^loadrt $module names=mycvt\$" core.hal || \
    fail "loadrt of a names= instance not converted"
grep -q 'mycvt.in' core.hal || fail "a names= instance was renamed"

# pins are renamed on a component that is not renamed, and the index of an
# indexed pin is carried over: the two names may differ only in the index
in0=$(sed -n 's/^setp mux-gen.00.\([^ ]*\) 5$/\1/p' core.hal)
in1=$(sed -n 's/^setp mux-gen.01.\([^ ]*\) 6$/\1/p' core.hal)
test -n "$in0" -a -n "$in1" || fail "a setp line lost its pin name"
test "${in0%00}" = "${in1%01}" || \
    fail "the index of an indexed pin was not carried over ($in0, $in1)"
grep -q "^net g-sig mux-gen.00.${in0%00}01\$" custom.hal || \
    fail "file behind a custom INI key not converted"
grep -q "^addf $cvt.1 servo-thread\$" custom.hal || \
    fail "file behind a custom INI key not converted"

# the type word on a newsig line is converted, to the same word the pins
# of that type now use
sig=$(sed -n 's/^newsig type-sig \(.*\)$/\1/p' core.hal)
test "$sig" = "$(echo "$in0" | cut -d- -f2)" || \
    fail "the type on a newsig line was not converted ($sig)"

# linkpp is not a halcmd command any more, so the line is reported
grep -q "'linkpp' is not a halcmd command" ini.log || \
    fail "a linkpp line was not reported"

# names that do not spell a type stay as they are
grep -q '^net c-sig mux-gen.00.sel-bit-00$' core.hal || \
    fail "sel-bit-00 was renamed"

# only the real name of an alias is converted, the alias is the user's
grep -q '^alias pin mux-gen.00.out-[a-z0-9]* nice-name$' core.hal || \
    fail "alias line not converted"

# signal names are never touched, not even one that spells a type
for sig in a-sig s32-raw c-sig d-sig e-sig f-sig g-sig; do
    grep -q -- "$sig" core.hal extra.hal later.hal custom.hal machine.ini || \
        fail "signal $sig disappeared"
done

# a component with no replacement is left for the user to deal with
grep -q '^loadrt conv_s32_s64$' core.hal || \
    fail "a component with no replacement was rewritten"
grep -q '^setp conv-s32-s64.0.in 3$' core.hal || \
    fail "a pin of a component with no replacement was rewritten"

# a haltcl command continued over several lines is converted, and a name
# the file builds itself is left alone and reported
grep -q "^    mux-gen.01.${in0%00}00 \\\\\$" loop.tcl || \
    fail "continued haltcl command not converted"
grep -q "^    $cvt.0.out\$" loop.tcl || \
    fail "last line of a continued haltcl command not converted"
grep -q "^setp mux-gen.01.${in0%00}01 2\$" loop.tcl || \
    fail "plain haltcl command not converted"
grep -q "setp conv-s32-float\.\$i\.in 1" loop.tcl || \
    fail "a name built in the haltcl file was rewritten"
grep -q "is built with a substitution" ini.log || \
    fail "a name built in the haltcl file was not reported"

# the Tcl 'hal' command form is understood
grep -q "^hal net h-sig $cvt.1.in\$" loop.tcl || \
    fail "the 'hal <command>' form was not converted"

# HALCMD lines carry names too, so the INI file itself is converted, and a
# value continued over two lines is one command
grep -q "^HALCMD = net f-sig $cvt.1.out \\\\\$" machine.ini || \
    fail "first line of a continued HALCMD not converted"
grep -q "^         mux-gen.00.${in0%00}00\$" machine.ini || \
    fail "continuation line of a HALCMD not converted"

# a second run must have nothing left to do
halfileupdate machine.ini > second.diff 2>/dev/null
if test -s second.diff; then
    fail "the conversion is not stable on a second run"
fi

exit 0
