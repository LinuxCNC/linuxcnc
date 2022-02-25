#!/usr/bin/env python3

import re
import sys
import subprocess

duplicate_warning = re.compile("WARNING: [^ ]*: '(.*?)' exported twice. Previous.*")

permitted_duplicates = ['kinematicsType', \
                        'kinematicsForward', \
                        'kinematicsInverse', \
                        'kinematicsSwitch', \
                        'kinematicsSwitchable', \
                       ]

kbuild = subprocess.Popen(sys.argv[1:], stderr=subprocess.PIPE)
for line in kbuild.stderr:
    m = duplicate_warning.match(line.decode('utf-8'))
    if m and m.group(1) in permitted_duplicates: continue
    linestring = line.decode('utf-8')

    # (rtai) don't complain for symbols in  hal *.comp example
    # files that duplicate symbols from default modules:
    if "jogcomp"  in linestring: continue
    if "homecomp" in linestring: continue
    if "tpcomp"   in linestring: continue

    sys.stderr.write(line.decode('utf8'))

raise SystemExit(kbuild.wait())
