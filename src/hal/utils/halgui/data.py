
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

from random import randint

def get_comp(design, num):
	try:
		c = [x for x in design.complist if x.num == num][0]
	except:
		c = Component(design)
		c.num = num
	return c

class Component:
	def __init__(self, design):
		design.complist.append(self)
		self.design = design
		self.name = ""
		self.num = 0
		self.pins_in = []
		self.pins_out = []
		self.x = randint(0, design.width-150)
		self.y = randint(0, design.height-200)
		self.widget = None
		self.redraw()

	def redraw(self):
		if self.widget:
			self.widget.destroy()
		self.widget = self.design.canvas.add_comp(self, self.x, self.y)
		for pin in self.pins_in + self.pins_out:
			pin.redraw()

	def read_pos(self):
		self.x = self.widget.get_property('x')
		self.y = self.widget.get_property('y')
		for pin in self.pins_in + self.pins_out:
			pin.redraw()

	def move(self, pos):
		self.x, self.y = pos
		self.redraw()
		for pin in self.pins_in + self.pins_out:
			pin.redraw()

def get_pin(component, name, dtype, perm, value):
	try:
		p = [x for x in component.pins_in + component.pins_out if x.name == name][0]
	except:
		p = Pin(component, name, dtype, perm, value)
	return p

class Pin:
	def __init__(self, component, name, dtype, perm, value):
		self.component = component
		self.design = component.design
		self.name = name
		self.dtype = dtype
		self.perm = perm
		self.value = value
		self.signal = None
		self.sigwid = None
		self.x = 0
		self.y = 0

	def redraw(self):
		if self.sigwid:
			self.sigwid.destroy()
			self.sigwid = None
		if self.signal:
			self.sigwid = self.design.canvas.add_sigline(self)

def get_sig(design, name, dtype):
	try:
		c = [x for x in design.siglist if x.name == name][0]
	except:
		c = Signal(design, name, dtype)
	return c

class Signal:
	def __init__(self, design, name, dtype):
		self.design = design
		self.name = name
		self.dtype = dtype
		self.pinlist = []
		self.x = randint(0, design.width-150)
		self.y = randint(0, design.height-200)
		self.widget = None
		design.siglist.append(self)

		self.redraw()

	def addpin(self, pin):
		self.pinlist.append(pin)
		pin.signal = self

	def redraw(self):
		if self.widget:
			self.widget.destroy()
		self.widget = self.design.canvas.add_sig(self, self.x, self.y)

	def read_pos(self):
		self.x = self.widget.get_property('x')
		self.y = self.widget.get_property('y')
		for pin in self.pinlist:
			pin.redraw()

	def move(self, pos):
		self.x, self.y = pos
		self.redraw()
		for pin in self.pinlist:
			pin.redraw()

