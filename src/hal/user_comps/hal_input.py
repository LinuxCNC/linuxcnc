#!/usr/bin/python
#    Copyright 2007 Jeff Epler <jepler@unpythonic.net>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

import linux_event, sys, os, fcntl, hal, select, time, glob, fnmatch, select
from hal import *

def tohalname(s): return str(s).lower().replace("_", "-")

class HalWrapper:
    def __init__(self, comp):
	self._comp = comp
	self._drive = {}
	self._pins = set()
	self._params = set()

    def drive(self):
	for k, v in self._drive.items(): setattr(self._comp, k, v)

    def newpin(self, *args):
	self._pins.add(args[0])
	return self._comp.newpin(*args)
    def newparam(self, *args):
	self._params.add(args[0])
 	return self._comp.newparam(*args)

    def __getitem__(self, k):
	if k in self._drive: return self._drive[k]
	return self._comp[k]

    def __setitem__(self, k, v):
	if k in self._params:
	    self._comp[k] = v; return
	if k in self._pins:
	    self._comp[k] = v; return
	raise KeyError, k
	self._drive[k] = v

class HalInputDevice:
    def __init__(self, comp, idx, name, parts='KRAL'):
	self.device = linux_event.InputDevice(name)

	self.idx = idx
	self.codes = set()
	self.last = {}
	self.rel_items = []
	self.abs_items = []
	self.comp = comp
        self.parts = parts

        if 'K' in parts:
            for key in self.device.get_bits('EV_KEY'):
                key = tohalname(key)
                self.codes.add(key)
                comp.newpin("%s.%s" % (idx, key), HAL_BIT, HAL_OUT)
                comp.newpin("%s.%s-not" % (idx, key), HAL_BIT, HAL_OUT)
                self.set(key + "-not", 1)

        if 'R' in parts:
            for axis in self.device.get_bits('EV_REL'):
                name = tohalname(axis)
                self.codes.add(name)
                comp.newpin("%s.%s-position" % (idx, name), HAL_FLOAT, HAL_OUT)
                comp.newpin("%s.%s-counts" % (idx, name), HAL_S32, HAL_OUT)
                comp.newpin("%s.%s-reset" % (idx, name), HAL_BIT, HAL_IN)
                comp.newpin("%s.%s-scale" % (idx, name), HAL_FLOAT, HAL_IN)
                self.set(name + '-scale', 1.)
                self.rel_items.append(name)

        if 'A' in parts:
            for axis in self.device.get_bits('EV_ABS'):
                name = tohalname(axis)
                self.codes.add(name)
                absinfo = self.device.get_absinfo(axis)
                comp.newpin("%s.%s-position" % (idx, name), HAL_FLOAT, HAL_OUT)
                comp.newpin("%s.%s-counts" % (idx, name), HAL_S32, HAL_OUT)
                comp.newpin("%s.%s-is-pos" % (idx, name), HAL_BIT, HAL_OUT)
                comp.newpin("%s.%s-is-neg" % (idx, name), HAL_BIT, HAL_OUT)
                comp.newpin("%s.%s-scale" % (idx, name), HAL_FLOAT, HAL_IN)
                comp.newpin("%s.%s-offset" % (idx, name), HAL_FLOAT, HAL_IN)
                comp.newpin("%s.%s-fuzz" % (idx, name), HAL_S32, HAL_IN)
                comp.newpin("%s.%s-flat" % (idx, name), HAL_S32, HAL_IN)
                comp.newparam("%s.%s-min" % (idx, name), HAL_S32, HAL_RO)
                comp.newparam("%s.%s-max" % (idx, name), HAL_S32, HAL_RO)
                center = (absinfo.minimum + absinfo.maximum)/2.
                halfrange = (absinfo.maximum - absinfo.minimum)/2. or 1
                self.set(name + "-counts", absinfo.value)
                self.set(name + "-position",
                    (absinfo.value - center) / halfrange)
                self.set(name + "-scale", halfrange)
                self.set(name + "-offset", center)
                self.set(name + "-fuzz", absinfo.fuzz)
                self.set(name + "-flat", absinfo.flat)
                self.set(name + "-min", absinfo.minimum)
                self.set(name + "-max", absinfo.maximum)
                self.abs_items.append(name)

	self.ledmap = {}
        if 'L' in parts:
            for led in self.device.get_bits('EV_LED'):
                name = tohalname(led)
                self.ledmap[name] = led
                comp.newpin("%s.%s" % (idx, name), HAL_BIT, HAL_IN)
                comp.newpin("%s.%s-invert" % (idx, name), HAL_BIT, HAL_IN)
                self.last[name] = 0
                self.device.write_event('EV_LED', led, 0)
	
    def get(self, name):
	name = "%s.%s" % (self.idx, name)
	return self.comp[name]

    def set(self, name, value):
	name = "%s.%s" % (self.idx, name)
	self.comp[name] = value

    def update(self):
	while self.device.readable():
	    ev = self.device.read_event()
	    if ev.type == 'EV_SYN': continue
	    elif ev.type == 'EV_SND': continue
	    elif ev.type == 'EV_MSC': continue
            elif ev.type == 'EV_LED': continue
            elif ev.type == 'EV_KEY' and 'K' not in self.parts: continue
            elif ev.type == 'EV_REL' and 'R' not in self.parts: continue
            elif ev.type == 'EV_ABS' and 'A' not in self.parts: continue
	    code = tohalname(ev.code)
	    if code not in self.codes:
		print >>sys.stderr, "Unexpected event", ev.type, ev.code
		continue
	    if ev.type == 'EV_KEY':
		if ev.value:
		    self.set(code, 1)
		    self.set(code + "-not", 0)
		else:
		    self.set(code, 0)
		    self.set(code + "-not", 1)
	    elif ev.type == 'EV_REL':
		self.set(code + "-counts", self.get(code + "-counts") + ev.value)
	    elif ev.type == 'EV_ABS':
		flat = self.get(code + "-flat")
		fuzz = self.get(code + "-fuzz")
		center = int(self.get(code + "-offset"))
		if ev.value < center-flat or ev.value > center+flat:
		    value = ev.value
		else: value = center
		if abs(value - self.get(code + "-counts")) > fuzz:
		    self.set(code + "-counts", value)

                
	for a in self.abs_items:
	    value = self.get(a + "-counts")
	    scale = self.get(a + "-scale") or 1
	    offset = self.get(a + "-offset")
	    position = (value - offset) / scale
	    self.set(a + "-position", position)
	    # Use .01 because my Joystick isn't exactly zero at rest. maybe should be a parameter?
	    self.set(a + "-is-neg", (position < -.01) )
	    self.set(a + "-is-pos", (position >  .01) )

	for r in self.rel_items:
	    reset = self.get(r + "-reset")
	    scale = self.get(r + "-scale") or 1
	    if reset: self.set(r + "-counts", 0)
	    self.set(r + "-position", self.get(r + "-counts") / scale)

	for k, v in self.last.items():
	    # Note: this is OK because the hal module always returns True or False for HAL_BIT values
	    u = self.comp["%s.%s" % (self.idx, k)] != self.comp["%s.%s-invert" % (self.idx, k)]
	    if u != self.last[k]:
		led = self.ledmap[k]
		self.device.write_event('EV_LED', led, u)
		self.last[k] = u

h = component("hal_input")
w = HalWrapper(h)
h.setprefix("input")
d = []
i = 0
parts = 'KRAL'
for f in sys.argv[1:]:
    if f.startswith("-"):
        parts = f[1:]
    else:
        try:
            d.append(HalInputDevice(w, i, f, parts))
        except LookupError, detail:
            raise SystemExit, detail
        parts = 'KRAL'
        i += 1
w.drive()
h.ready()

fds = [dev.device.fileno() for dev in d]
try:
    while 1:
	select.select(fds, [], [], .01)
	for i in d: i.update()
	w.drive()
except KeyboardInterrupt:
    pass
