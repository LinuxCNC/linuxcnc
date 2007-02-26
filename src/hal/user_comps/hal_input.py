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

class HalInputDevice:
    def __init__(self, comp, idx, name):
	self.device = linux_event.InputDevice(name)

	self.idx = idx
	self.drive = {}
	self.abs = {}
	self.last = {}
	self.rel_items = []
	self.comp = comp

	for key in self.device.get_bits('EV_KEY'):
	    key = tohalname(key)
	    comp.newpin("%s.%s" % (idx, key), hal.HAL_BIT, hal.HAL_OUT)
	    self.drive[key] = 0

	for axis in self.device.get_bits('EV_REL'):
	    name = tohalname(axis)
	    comp.newpin("%s.%s" % (idx, name), hal.HAL_FLOAT, hal.HAL_OUT)
	    comp.newpin("%s.%s-reset" % (idx, name), hal.HAL_BIT, hal.HAL_IN)
	    self.rel_items.append(name)
	    self.drive[name] = 0

	for axis in self.device.get_bits('EV_ABS'):
	    name = tohalname(axis)
	    self.abs[name] = self.device.get_absinfo(axis)
	    comp.newpin("%s.%s" % (idx, name), hal.HAL_FLOAT, hal.HAL_OUT)
	    self.drive[name] = 0

	for led in self.device.get_bits('EV_LED'):
	    name = tohalname(led)
	    comp.newpin("%s.%s" % (idx, name), hal.HAL_BIT, hal.HAL_IN)
	    self.last[name] = 0
	    self.device.write_event('EV_LED', led, 0)
	
    def update(self):
	while self.device.readable():
	    ev = self.device.read_event()
	    code = tohalname(ev.code)
	    if ev.type == 'EV_KEY':
		if code not in self.drive:
		    print >>sys.stderr, "Unexpcted event EV_KEY", code, ev.code
		    continue
		if ev.value:
		    self.drive[code] = 1
		else:
		    self.drive[code] = 0
	    elif ev.type == 'EV_REL':
		if code not in self.drive:
		    print >>sys.stderr, "Unexpcted event EV_REL", code, ev.code
		    continue
		self.drive[code] += ev.value
	    elif ev.type == 'EV_ABS':
		if code not in self.drive:
		    print >>sys.stderr, "Unexpcted event EV_ABS", code, ev.code
		    continue
		absinfo = self.abs[code]
		if abs(ev.value) < absinfo.flat: ev.value = 0
		scale = max(-absinfo.minimum, absinfo.maximum)
		self.drive[code] = ev.value * 1. / scale

	for r in self.rel_items:
	    reset = self.comp["%s.%s-reset" % (self.idx, r)]
	    if reset: self.drive[r] = 0

	for k, v in self.drive.items():
	    self.comp["%s.%s" % (self.idx, k)] = v

	for k, v in self.last.items():
	    u = self.comp["%s.%s" % (self.idx, k)]
	    if u != self.last[k]:
		self.device.write_event('EV_LED', k.upper(), u)
		self.last[k] = u

h = hal.component("hal_input")
h.setprefix("input")
d = []
for i, f in enumerate(sys.argv[1:]):
    d.append(HalInputDevice(h, i, f))
h.ready()

try:
    while 1:
	time.sleep(.01)
	for i in d: i.update()
except KeyboardInterrupt:
    pass
