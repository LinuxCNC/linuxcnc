#!/bin/bash

trim() { sed 's/^ *[0-9][0-9]* //'; }

# Fanuc enabled:
# - with Fanuc sub:  pass
echo "program: fanuc; INI: fanuc"
rs274 -g -i test-fanuc.ini test-fanuc.ngc 2>&1 | trim
# - with rs274ngc sub:  pass (rs274ngc subs not disabled)
echo "program: rs274ngc; INI: fanuc"
rs274 -g -i test-fanuc.ini test-rs274ngc.ngc 2>&1 | trim

# Fanuc disabled:
# - with Fanuc sub:  fail
echo "program: fanuc; INI: NO fanuc"
{ ! rs274 -g -i test-no-fanuc.ini test-fanuc.ngc 2>&1; } | trim || exit 1
# - with rs274ngc sub:  pass
echo "program: rs274ngc; INI: NO fanuc"
rs274 -g -i test-no-fanuc.ini test-rs274ngc.ngc 2>&1 | trim

# Default (Fanuc enabled):
# - with Fanuc sub:  pass
echo "program: fanuc; INI: (none)"
rs274 -g test-fanuc.ngc 2>&1 | trim
# - with rs274ngc sub:  pass (rs274ngc subs not disabled)
echo "program: rs274ngc; INI: (none)"
rs274 -g test-rs274ngc.ngc 2>&1 | trim
