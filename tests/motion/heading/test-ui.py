#!/usr/bin/env python3

import linuxcnc
import linuxcnc_util
import hal

import time
import sys
import os
import math


INTERPTIMEOUT = 5   # Max 5 seconds to wait for the interpreter
GCODETIMEOUT  = 120  # Max 60 seconds for the gcode file to run12

#
# Sample collection routine
#
nsamples = 0
lastsample = None
lastrecnum = 0
ctr = 0

def is_arc_valid(sample, tolerance):
    """
    Validates circularity, radius accuracy, and G2/G3 heading consistency.
    Returns True on pass, False on failure.
    """
    global ctr
    # Extract Fields
    motion_type = sample[1]      # 20=G2, 30=G3
    expected_radius = sample[3]
    cx, cy = sample[4], sample[5]
    heading = sample[7]
    normal_heading = sample[8]
    x = sample[10]
    y = sample[11]
    # 2. Radius Check: Distance from tool (x,y) to center (cx,cy)
    actual_radius = math.hypot(x - cx, y - cy)
    if abs(actual_radius - expected_radius) > tolerance:
        ctr = ctr + 1
        if ctr > 40:
            print(f"FAIL Radius after 40 times: Expected {expected_radius}, Got {actual_radius:.4f}")
            ctr = 0
            return False
    ctr = 0
    # 3. Normal Direction Check: Normal must point from tool to center
    angle_to_center = math.degrees(math.atan2(cy - y, cx - x)) % 360
    norm_diff = abs(angle_to_center - (normal_heading % 360))
    if norm_diff > 180: norm_diff = 360 - norm_diff

    if norm_diff > tolerance:
        print(f"FAIL Normal: Expected {angle_to_center:.4f}, Got {normal_heading:.4f}")
        return False
    # 4. Tangency/Direction Check: (Heading vs Normal)
    #G2 (20): Heading should be Normal + 90
    #G3 (30): Heading should be Normal - 90 (or + 270)
    diff = (heading - normal_heading) % 360

    if motion_type == 20:   # G2
        if abs(diff - 90) > tolerance:
            print(f"FAIL: G2 normal heading outside of tolerance",diff)
            return False
    elif motion_type == 30: # G3
        if abs(diff - 270) > tolerance:
            print(f"FAIL: G3 normal heading outside of tolerance",diff)
            return False
    return True


def is_heading_valid(sample1, sample2, tolerance=0.1):
    """
    Returns True if the travel direction between two samples
    matches the reported heading within a given tolerance.
    """
    global ctr
    if sample1[0] != sample2[0]:
        ctr = ctr + 1
    if ctr < 5:  #allow to settle on change of segment
        return True
    else:
        ctr = 0  #Reset
    # Extract coords and reported heading
    x1, y1 = sample1[10], sample1[11]
    x2, y2 = sample2[10], sample2[11]
    reported_heading = sample2[7]

    dx = x2 - x1
    dy = y2 - y1

    # Avoid calculation if there is no movement (prevents jitter errors)
    if math.hypot(dx, dy) < 1e-9:
        return True

    # Calculate angle: atan2(y, x) -> degrees
    calculated_heading = math.degrees(math.atan2(dy, dx))

    # Normalize to 0-360 if your system doesn't use negative degrees
    if calculated_heading < 0:
        calculated_heading += 360

    # Standardize reported_heading to same 0-360 range if needed
    if reported_heading < 0:
        reported_heading += 360

    # Handle the "wrap-around" case (e.g., 359.9 is near 0.1)
    diff = abs(calculated_heading - reported_heading)
    if diff > 180:
        diff = 360 - diff
    return(diff <= tolerance)

def parse_sample(sample):
    """
    Parses a tuple of sampler data for tests
    Fields: line-number, motion-type, feedrate, arc-radius, arc-center-x, arc-center-y, arc-center-z,
            heading, normal-heading, iscircle, x-pos-fb, y-pos-fb
    """
    global lastsample
    global lastrecnum

    print("{:6d} {}".format(nsamples, sample))

    if None == lastsample or sample[0] != lastsample[0]:
        return True
    if sample[1] not in (00,10,20,30):
        print("Gcode line is not G0,G1,G2 or G3, nothing to do")
        return True
    if (sample[1] == 20 or sample[1] == 30):
        # gcode is G2 but G3
        if not is_arc_valid(sample, 0.001):
            print("G2/G3 tests are invalid at record ", lastrecnum)
            return False
    if (lastsample[1] == sample[1] == 10): # we have at least 2 G1 records
        if not is_heading_valid(lastsample, sample, tolerance=0.001):
            print("G0 heading or other values does not agree with expected value at record ",lastrecnum)
            return False
    if (lastsample[1] == sample[1] == 0): # we have at least 2 G0 records
        if not is_heading_valid(lastsample, sample, tolerance=0.001):
            print("G0 heading or other values does not agree with expected value at record ",lastrecnum)
            return False
    lastrecnum = lastrecnum + 1
    lastsample = sample

#
# Sample collection routine
#
def read_sample():
    global nsamples, lastsample
    sample = sampler.read()
    if None == sample:
        print("Error: read_sample() read None (readable={}, depth={})".format(samples.readable, sampler.depth))
        sys.exit(1)
    if sample != lastsample and 0 != sample[0]: # Don't handle duplicates and non-gcode
        if not parse_sample(sample):
            print("Test Failed")
        nsamples += 1
        lastsample = sample

#
# Test the command line arguments. It should have a .ngc file we can read
#
ngcfile = None
# Crudely parse command like args to find: -ngc file
for i in range(1, len(sys.argv)-1):
    if "-ngc" == sys.argv[i]:
        ngcfile = sys.argv[i+1]
        break

if not ngcfile:
    print("Missing NGC-file; run with: test-ui.py -ngc ngcfile.ngc")
    sys.exit(1)

if not os.path.exists(ngcfile):
    print("NGC-file '{}' does not exist".format(ngcfile))
    sys.exit(1)

#
# Setup a hal component and connect to the sampler stream
#
h = hal.component("python-ui")
sampler = hal.stream(h, hal.sampler_base, "ssfffffffbff")
h.ready()

#
# connect to LinuxCNC
#

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

l = linuxcnc_util.LinuxCNC(command=c, status=s, error=e)
# Enable machine
c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)

# Home all joints
c.home(-1)
c.wait_complete()
l.wait_for_home([1, 1, 1, 0, 0, 0, 0, 0, 0])

#
# Setup to run GCode program
#
c.mode(linuxcnc.MODE_AUTO)
c.program_open(ngcfile)

# Enable sampling
hal.set_p("sampler.0.enable", "1")

# Start the GCode
c.auto(linuxcnc.AUTO_RUN, 1)

# Wait for the interpreter to start running the program
start = time.time()
while time.time() - start < INTERPTIMEOUT:
    s.poll()
    if s.interp_state != linuxcnc.INTERP_IDLE:
        print("Interpreter active...")
        break
    time.sleep(0.01)
if s.interp_state == linuxcnc.INTERP_IDLE:
    print("Timed out starting interpreter")
    sys.exit(1)

#
# Loop until the program finishes while collecting data
#
start = time.time()
s.poll()
while s.interp_state != linuxcnc.INTERP_IDLE and time.time() - start < GCODETIMEOUT:
    # Continuously drain the sampler buffer
    while sampler.readable:
        read_sample()
    time.sleep(0.01)
    s.poll()

if s.interp_state != linuxcnc.INTERP_IDLE:
    print("Timed out running the GCode program")
    sys.exit(1)

# Disable the sampler
hal.set_p("sampler.0.enable", "0")

# Give RT time to finalize the last sample(s)
time.sleep(0.01)

# Drain any remaining samples
while sampler.readable:
    read_sample()
# if we get here it all worked!
sys.exit(0)

