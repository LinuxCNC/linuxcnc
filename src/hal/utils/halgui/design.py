
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

import gtk
import re

from canvas import HalCanvas, ComponentList
from data import get_comp, get_pin, get_sig

class Design(gtk.Frame):
	def __init__(self, parent):
		self.app = parent
		gtk.Frame.__init__(self)

		self.file_name = ""
		self.width = 640
		self.height = 480
		self.m_frame = 5

		self.complist = []
		self.siglist = []

		sw = gtk.ScrolledWindow()

		canvas_frame = gtk.Frame(label = "HAL Editor")
		clist_frame = gtk.Frame(label = "Components")

		self.canvas = HalCanvas(self)
		canvas_frame.add(sw)
		sw.add(self.canvas)
		sw.set_shadow_type(gtk.SHADOW_IN)

		self.clist = ComponentList(self)
		clist_frame.add(self.clist)

		paned = gtk.HPaned()
		paned.set_border_width(2)
		paned.pack1(canvas_frame, True, True)
		paned.pack2(clist_frame, False, True)
		paned.set_position(800)

		self.add(paned)

	def update(self):
		dellist = self.complist[:]
		if self.file_name:
			f = open(self.file_name)
		else:
			return # This must be handled
		state = 0
		for line in f:
			#line = re.sub(r"(\d+)\s*(\(\d+\))", r"\1\2", line)
			if not line.strip():
				state = 0
			elif state == 0:
				if line.strip() == "Loaded HAL Components:":
					state = 1
				if line.strip() == "Component Pins:":
					state = 2
			elif state == 1:
				num, space, name = line.split()
				if re.match("\D", num):
					continue
				comp = get_comp(self, num)
				comp.name = name
				comp.pins_in = []
				comp.pins_out = []
				comp.redraw()
				try: dellist.remove(comp)
				except ValueError: pass
			elif state == 2:
				dlist = line.split()
				sigd, sig = None, None
				num, dtype, perm, value, name = dlist[:5]
				if len(dlist) == 7:
					sigd, sig = dlist[-2:]
				if re.match("\D", num):
					continue
				comp = get_comp(self, num)
				name = '.'.join(name.split('.')[-2:])
				pin = get_pin(comp, name, dtype, perm, value)
				if perm.find('R') >= 0:
					comp.pins_in.append(pin)
				if perm.find('W') >= 0:
					comp.pins_out.append(pin)
				if sig:
					signal = get_sig(self, sig, sigd)
					signal.addpin(pin)
				comp.redraw()
			for comp in dellist[:]:
				del comp

		self.rearrange()

	def rearrange(self):
		comps = self.complist[:]
		start = []
		for comp in comps:
			pin = None
			for pin in comp.pins_in:
				if pin.signal:
					break
			if not pin or not pin.signal:
				start.append(comp)

		cx, cy = 20, 20

		while comps:
			if start:
				comp = start.pop()
				if comp in comps:
					comps.remove(comp)
			else:
				comp = comps.pop()

			comp.x, comp.y = cx, cy
			cx += comp.widget.width + 40
			comp.redraw()

			sig = None
			for pin in comp.pins_out:
				if pin.signal:
					sig = pin.signal
					#sig.x, sig.y = cx, cy
					sig.move((cx, cy))
					cy += 40
					if sig.pinlist[0].component == comp:
						newcomp = sig.pinlist[1].component
					else:
						newcomp = sig.pinlist[0].component
					if not newcomp in start:
						start.append(newcomp)
			if sig:
				cx += 60
				cy = 20

