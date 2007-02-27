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

import linux_event, sys, os, fcntl, hal, select, time, glob, fnmatch

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
	print args[0]
	self._params.add(args[0])
 	return self._comp.newparam(*args)

    def __getitem__(self, k):
	if k in self._drive: return self._drive[k]
	return self._comp[k]

    def __setitem__(self, k, v):
	if k in self._params:
	    self._comp[k] = v; return
	if k not in self._pins: raise KeyError, k
	self._drive[k] = v

class HalInputDevice:
    def __init__(self, comp, idx, name):
	self.device = linux_event.InputDevice(name)

	self.idx = idx
	self.codes = set()
	self.last = {}
	self.rel_items = []
	self.comp = comp

	for key in self.device.get_bits('EV_KEY'):
	    key = tohalname(key)
	    self.codes.add(key)
	    comp.newpin("%s.%s" % (idx, key), hal.HAL_BIT, hal.HAL_OUT)
	    comp.newpin("%s.%s-not" % (idx, key), hal.HAL_BIT, hal.HAL_OUT)
	    self.set(key + "-not", 1)

	for axis in self.device.get_bits('EV_REL'):
	    name = tohalname(axis)
	    self.codes.add(name)
	    comp.newpin("%s.%s-position" % (idx, name), hal.HAL_FLOAT, hal.HAL_OUT)
	    comp.newpin("%s.%s-counts" % (idx, name), hal.HAL_S32, hal.HAL_OUT)
	    comp.newpin("%s.%s-reset" % (idx, name), hal.HAL_BIT, hal.HAL_IN)
	    comp.newparam("%s.%s-position-scale" % (idx, name), hal.HAL_FLOAT, hal.HAL_RW)
	    self.set(name + '-position-scale', 1.)
	    self.rel_items.append(name)

	for axis in self.device.get_bits('EV_ABS'):
	    name = tohalname(axis)
	    self.codes.add(name)
	    absinfo = self.device.get_absinfo(axis)
	    comp.newpin("%s.%s-position" % (idx, name), hal.HAL_FLOAT, hal.HAL_OUT)
	    comp.newpin("%s.%s-counts" % (idx, name), hal.HAL_S32, hal.HAL_OUT)
	    comp.newparam("%s.%s-position-scale" % (idx, name), hal.HAL_FLOAT, hal.HAL_RW)
	    comp.newparam("%s.%s-fuzz" % (idx, name), hal.HAL_FLOAT, hal.HAL_RW)
	    comp.newparam("%s.%s-flat" % (idx, name), hal.HAL_FLOAT, hal.HAL_RW)
	    self.set(name + "-position-scale", float(max(-absinfo.minimum, absinfo.maximum) or 1))
	    self.set(name + "-fuzz", absinfo.fuzz)
	    self.set(name + "-flat", absinfo.flat)

	self.ledmap = {}
	for led in self.device.get_bits('EV_LED'):
	    name = tohalname(led)
	    self.ledmap[name] = led
	    comp.newpin("%s.%s" % (idx, name), hal.HAL_BIT, hal.HAL_IN)
	    comp.newparam("%s.%s-invert" % (idx, name), hal.HAL_BIT, hal.HAL_RW)
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
	    if ev.type == 'EV_MSC': continue
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
		if ev.value < -flat: value = ev.value + flat
		elif ev.value > flat: value = ev.value - flat
		else: value = 0
		if abs(value - self.get(code + "-counts")) > fuzz:
		    self.set(code + "-counts", ev.value)
		    self.set(code + "-position", ev.value / (self.get(code + "-position-scale") or 1))

	for r in self.rel_items:
	    reset = self.get(r + "-reset")
	    if reset: self.set(r + "-counts", 0)
	    self.set(r + "-position", self.get(r + "-counts") / (self.get(r + "-position-scale") or 1))

	for k, v in self.last.items():
	    # Note: this is OK because the hal module always returns True or False for HAL_BIT values
	    u = self.comp["%s.%s" % (self.idx, k)] != self.comp["%s.%s-invert" % (self.idx, k)]
	    if u != self.last[k]:
		led = self.ledmap[k]
		self.device.write_event('EV_LED', led, u)
		self.last[k] = u

h = hal.component("hal_input")
w = HalWrapper(h)
h.setprefix("input")
d = []
for i, f in enumerate(sys.argv[1:]):
    d.append(HalInputDevice(w, i, f))
h.ready()

try:
    while 1:
	time.sleep(.01)
	for i in d: i.update()
	w.drive()
except KeyboardInterrupt:
    pass
