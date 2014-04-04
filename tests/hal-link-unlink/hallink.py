#!/usr/bin/python

import subprocess
import hal

def cmd(arg):
    subprocess.call(arg, shell=True)

h = hal.component("linktest")
h.newpin("in", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("inout", hal.HAL_FLOAT, hal.HAL_IO)
h.ready()

# set pin values before linking
h['in'] = 42
h['inout'] = 43
assert h['in'] == 42
assert h['inout'] == 43

# make sure halcmd setp works as expected
cmd("halcmd setp linktest.in    4712")
cmd("halcmd setp linktest.inout 4713")
assert h['in'] == 4712
assert h['inout'] == 4713

# create virgin signals
cmd("halcmd newsig insig float")
cmd("halcmd newsig inoutsig float")

# link to them
cmd("halcmd net insig    linktest.in")
cmd("halcmd net inoutsig linktest.inout")

# verify the link did not destroy the pin values
assert h['in'] == 4712
assert h['inout'] == 4713

# now change the pin values
h['in'] = 815
h['inout'] = 816

# unlink the pins
cmd("halcmd unlinkp linktest.in")
cmd("halcmd unlinkp linktest.inout")

# verify the unlink did not destroy the pin values
# but are as inherited from the signal:
assert h['in'] == 815
assert h['inout'] == 816

# the signals should be unlinked
#cmd("halcmd show")
