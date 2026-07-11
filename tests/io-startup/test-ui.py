#!/usr/bin/env python3

# Ported to the gomc model: read the startup tool number directly from
# iocontrol.tool-number via halcmd (gomc has no userspace HAL components, so the
# old test-ui HAL component + postgui net are gone).

import subprocess
import time
import sys

with open('expected-startup-tool-number') as f:
    expected = int(f.read())
print("expecting tool number %d" % expected)


def tool_number():
    out = subprocess.check_output(["halcmd", "getp", "iocontrol.tool-number"])
    return int(float(out.split()[-1]))


start = time.time()
while (time.time() - start) < 5.0:
    if tool_number() == expected:
        print("iocontrol.tool-number reached %d" % expected)
        sys.exit(0)
    time.sleep(0.1)

print("Error: iocontrol.tool-number is %d, expected %d" % (tool_number(), expected))
sys.exit(1)
