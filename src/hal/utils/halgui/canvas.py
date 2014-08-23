
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

import gobject, gtk
import gnomecanvas, gnome.ui

from data import Component

class HalCanvas(gnomecanvas.Canvas):
	def __init__(self, parent=None):
		self.design = parent
		gnomecanvas.Canvas.__init__(self, aa=False)
		self.set_resize_mode(gtk.RESIZE_IMMEDIATE)
		self.c_parent = parent

		self.boardwid = self.root().add('GnomeCanvasRect', x1=0, y1=0, x2=1, y2=1, fill_color='white', outline_color='black', width_units=2)
		self.board = self.root().add('GnomeCanvasGroup', x=0, y=0)
		self.boardwid.handler_id = self.boardwid.connect('event', self.canvas_event)
		self.size_calc()

	def size_calc(self):
		width = self.design.width
		height = self.design.height
		self.set_scroll_region(-self.design.m_frame, -self.design.m_frame, width+self.design.m_frame, height+self.design.m_frame)
		self.boardwid.set(x2=width, y2=height)

	def add_comp(self, comp, x, y):
		if len(comp.pins_in) > len(comp.pins_out):
			height = 16 * (len(comp.pins_in)+1)
		else:
			height = 16 * (len(comp.pins_out)+1)

		wg = self.board.add('GnomeCanvasGroup', x=x, y=y)
		wr = wg.add('GnomeCanvasRect', x1=0, y1=0, x2=150, y2=height,
			fill_color='white', outline_color='black')
		wr.group = wg
		wr.real = wg
		wr.handler_id = wr.connect('event', self.hover_event)
		wr.handler_id = wr.connect('event', self.comp_event)

		wg.title = wg.add('GnomeCanvasText', x=75, y=8, text=comp.name)
		wg.title.real = wg
		wg.title.handler_id = wg.title.connect('event', self.hover_event)
		wg.title.handler_id = wg.title.connect('event', self.comp_event)

		wg.width = 150 # hardcoded value might be a bad choise
		wg.height = height

		wg.title.rect = wr.rect = wg.rect = wr
		wg.comp = comp

		def addpin(pin, x, y):
			if x:
				pin.x, pin.y = x+75+12, (y*16)+8
			else:
				pin.x, pin.y = x-12, (y*16)+8
			pin.component = comp
			pwg = wg.add('GnomeCanvasGroup', x=x, y=y*16)
			pwr = pwg.add('GnomeCanvasRect', x1=0, y1=0, x2=75, y2=16,
				fill_color='white', outline_color='black')
			pwg.title = pwg.add('GnomeCanvasText', x=40, y=8,
				text=pin.name, justification=gtk.JUSTIFY_LEFT)
			if x:
				pcw = pwg.add('GnomeCanvasRect', x1=75, y1=2, x2=75+12, y2=13,
					fill_color='white', outline_color='black')
			else:
				pcw = pwg.add('GnomeCanvasRect', x1=-12, y1=2, x2=0, y2=13,
					fill_color='white', outline_color='black')

			pwr.real = wg
			pwr.handler_id = pwr.connect('event', self.hover_event)
			pwr.handler_id = pwr.connect('event', self.comp_event)
			pwr.rect = wr

			pwg.title.real = wg
			pwg.title.handler_id = pwg.title.connect('event', self.hover_event)
			pwg.title.handler_id = pwg.title.connect('event', self.comp_event)
			pwg.title.rect = wr

			pcw.pin = pin
			pcw.handler_id = pcw.connect('event', self.hover_event)
			pcw.rect = pcw
	
		for n, pin in enumerate(comp.pins_in):
			addpin(pin, 0, n+1)
		for n, pin in enumerate(comp.pins_out):
			addpin(pin, 75, n+1)

		return wg

	def add_sig(self, sig, x, y):
		wg = self.board.add('GnomeCanvasGroup', x=x, y=y)
		wr = wg.add('GnomeCanvasRect', x1=0, y1=0, x2=20, y2=20,
			fill_color='white', outline_color='black')
		wr.real = wg
		wr.handler_id = wr.connect('event', self.hover_event)
		wr.handler_id = wr.connect('event', self.sig_event)

		wg.title = wg.add('GnomeCanvasText', x=10, y=-5, text=sig.name)
		wg.title.real = wg
		wg.title.handler_id = wg.title.connect('event', self.hover_event)
		wg.title.handler_id = wg.title.connect('event', self.sig_event)

		wr.rect = wg.title.rect = wr
		wg.rect = wr
		wg.sig = sig
		return wg

	def add_sigline(self, pin):
		line = self.board.add('GnomeCanvasLine', width_units=2,
			points=[pin.x+pin.component.x, pin.y+pin.component.y,
			pin.signal.x+10, pin.signal.y+10])
		line.lower_to_bottom()
		return line

	def canvas_event(self, widget, event=None):
		widget.handler_block(widget.handler_id)
		widget.handler_unblock(widget.handler_id)

	def hover_event(self, widget, event=None):
		widget.handler_block(widget.handler_id)
		if event.type == gtk.gdk.ENTER_NOTIFY:
			widget.rect.set(width_units=3)
		elif event.type == gtk.gdk.LEAVE_NOTIFY:
			widget.rect.set(width_units=1)
		widget.handler_unblock(widget.handler_id)

	def comp_event(self, widget, event=None):
		widget.handler_block(widget.handler_id)
		realwid = widget.real
		if event.type == gtk.gdk.BUTTON_PRESS:
			if event.button == 1:
				self.remember_x = event.x
				self.remember_y = event.y
		elif event.type == gtk.gdk.MOTION_NOTIFY:
			if event.state & gtk.gdk.BUTTON1_MASK:
				realwid.move(event.x - self.remember_x, event.y - self.remember_y)
				realwid.comp.read_pos()
				self.remember_x = event.x
				self.remember_y = event.y
		widget.handler_unblock(widget.handler_id)

	def sig_event(self, widget, event=None):
		widget.handler_block(widget.handler_id)
		realwid = widget.real
		if event.type == gtk.gdk.BUTTON_PRESS:
			if event.button == 1:
				self.remember_x = event.x
				self.remember_y = event.y
		elif event.type == gtk.gdk.MOTION_NOTIFY:
			if event.state & gtk.gdk.BUTTON1_MASK:
				realwid.move(event.x - self.remember_x, event.y - self.remember_y)
				realwid.sig.read_pos()
				self.remember_x = event.x
				self.remember_y = event.y
		widget.handler_unblock(widget.handler_id)

class ComponentList(gtk.Frame):
	def __init__(self, parent=None):
		gtk.Frame.__init__(self)
		self.design = parent
		self.list = gtk.ListStore(gobject.TYPE_PYOBJECT, gobject.TYPE_STRING)
		treeview = gtk.TreeView(self.list)
		self.selection = treeview.get_selection()
		self.selected_component = None

		#treeview.connect('cursor-changed', self.selection_event)

		renderer = gtk.CellRendererText()
		column = gtk.TreeViewColumn("Name", renderer, text=1)
		treeview.append_column(column)

		treeview.set_reorderable(True)

		sw = gtk.ScrolledWindow()
		sw.set_shadow_type(gtk.SHADOW_IN)
		sw.set_property('hscrollbar-policy', gtk.POLICY_NEVER)
		sw.add(treeview)

		self.add(sw)
		self.set_shadow_type(gtk.SHADOW_IN)

