#!/bin/sh
python <<EOF
import glob, os, sys

fail = 0
for t in sorted(glob.glob('*.ngc')):
    print >>sys.stderr, "#", t
    error = open(t).readline()
    if error.startswith(';'): expected = error[1:-1]
    elif error.startswith('('): expected = error[1:-2]
    else: expected = "%s: Test does not specify expected error" % t
    p = os.popen("rs274 -g %s 2>&1 > /dev/null" % t)
    output = p.readlines()
    r = p.close()
    print "# ->", r
    if not r:
	print "%s: Interpreter accepted bad gcode" % t
	fail += 1
	continue

    if len(output) < 2:
	print "%s: Unexpected interpreter output: %r" % output
	fail += 1
	continue

    err = output[-2].strip()
    if err != expected:
	print "%s: Expected %r, got %r instead" % (t, expected, err)
	fail += 1

if fail:
    raise SystemExit, "%d failures" % fail
EOF