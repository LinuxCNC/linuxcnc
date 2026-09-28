#!/bin/bash

# Test the dashed-identifier migration of update_ini --fix-identifiers:
# dashed section and tag names in the ini file, in an #INCLUDE'd file and
# in a custom xhc-hb04 layout cfg are rewritten to use underscores, .bak
# copies are kept and invalid identifiers are left alone.

UPDATE_INI=update_ini
INIVALUE=inivalue

command -v "$UPDATE_INI" > /dev/null 2>&1 || { echo "*** Missing update_ini executable"; exit 1; }
command -v "$INIVALUE" > /dev/null 2>&1 || { echo "*** Missing inivalue executable"; exit 1; }

cat > test.ini <<'EOF'
#INCLUDE buttons.inc
[EMC]
VERSION = 1.1
MACHINE = dash identifier test

[XHC_HB04_CONFIG]
layout = mylayout.cfg
coords = x y z

[XHC-HB04-EXTRA]
some-key = 1
# continuation lines belong to the value and must not be rewritten
note = do not touch start-pause =\
  goto-zero = inside a value
multi-key = first\
  second
# comment ending in a continuation\
also-not-a-tag = still comment text
# identifiers split by a continuation are rewritten too
split-t\
ag = 42
[SECTION-SPL\
IT]
inner-key = 9
EOF

cat > buttons.inc <<'EOF'
[XHC_HB04_BUTTONS]
start-pause = std_start_pause
goto-zero = halui.mdi-command-00
step = xhc-hb04.stepsize-up
EOF

cat > mylayout.cfg <<'EOF'
[XHC-HB04]
BUTTON=01:button-stop
BUTTON=02:button-start-pause
EOF

echo "--- update_ini --fix-identifiers output"
"$UPDATE_INI" --fix-identifiers test.ini

echo "--- converted test.ini"
cat test.ini
echo "--- converted buttons.inc"
cat buttons.inc
echo "--- converted mylayout.cfg"
cat mylayout.cfg
echo "--- backups kept"
ls -- *.bak

echo "--- inivalue after conversion"
"$INIVALUE" --sec=XHC_HB04_EXTRA --var=some_key test.ini
"$INIVALUE" --sec=XHC_HB04_BUTTONS --var=start_pause test.ini
"$INIVALUE" --sec=XHC_HB04_BUTTONS --var=goto_zero test.ini
echo "--- continued values survive (dashed text inside values untouched)"
"$INIVALUE" --sec=XHC_HB04_EXTRA --var=note test.ini
"$INIVALUE" --sec=XHC_HB04_EXTRA --var=multi_key test.ini
echo "--- split identifiers"
"$INIVALUE" --sec=XHC_HB04_EXTRA --var=split_tag test.ini
"$INIVALUE" --sec=SECTION_SPLIT --var=inner_key test.ini

# identifiers that are invalid even without the dash must be left alone
cat > bad.ini <<'EOF'
[SECTION]
-bad-key = 1
0bad = 2
EOF
cp bad.ini bad.ini.orig
"$UPDATE_INI" --fix-identifiers bad.ini
echo "--- invalid identifiers untouched"
if diff bad.ini.orig bad.ini; then
    echo "bad.ini unchanged"
else
    echo "*** bad.ini was modified"
fi

# an existing .bak blocks the whole conversion, nothing may be written
cat > conflict.ini <<'EOF'
#INCLUDE conflict.inc
[XHC-HB04-EXTRA]
some-key = 1
EOF
cat > conflict.inc <<'EOF'
[XHC_HB04_BUTTONS]
start-pause = std_start_pause
EOF
echo "ancient backup" > conflict.inc.bak
"$UPDATE_INI" --fix-identifiers conflict.ini
echo "exit=$?"
echo "--- conflict.ini untouched"
cat conflict.ini
echo "--- conflict.inc untouched"
cat conflict.inc

# a converted file that still does not parse is reported
echo "--- converted file that still fails to parse"
cat > bad2.ini <<'EOF'
[SEC-TION]
-bad-key = 1
EOF
"$UPDATE_INI" --fix-identifiers bad2.ini 2>/dev/null
echo "exit=$?"
echo "--- bad2.ini converted, .bak kept"
cat bad2.ini
ls -- bad2.ini.bak

# a write failure midway must roll back the files already written
echo "--- write failure rolls back"
if [ "$(id -u)" = 0 ]; then
    echo "skipped: running as root, permissions not enforced"
else
    mkdir sub
    cat > roll.ini <<'EOF'
#INCLUDE sub/blocked.inc
[XHC-HB04-EXTRA]
some-key = 1
EOF
    cat > sub/blocked.inc <<'EOF'
[XHC_HB04_BUTTONS]
start-pause = std_start_pause
EOF
    cp roll.ini roll.ini.orig
    chmod a-w sub
    "$UPDATE_INI" --fix-identifiers roll.ini
    echo "exit=$?"
    chmod +w sub
    echo "--- roll.ini restored"
    if diff roll.ini.orig roll.ini; then
        echo "roll.ini unchanged"
    else
        echo "*** roll.ini was left converted"
    fi
    echo "--- sub/blocked.inc untouched"
    cat sub/blocked.inc
    if [ -e roll.ini.bak ]; then
        echo "*** roll.ini.bak left behind"
    else
        echo "roll.ini.bak removed"
    fi
fi

rm -rf test.ini test.ini.bak buttons.inc buttons.inc.bak \
      mylayout.cfg mylayout.cfg.bak bad.ini bad.ini.orig bad2.ini bad2.ini.bak \
      conflict.ini conflict.inc conflict.inc.bak \
      roll.ini roll.ini.orig sub
