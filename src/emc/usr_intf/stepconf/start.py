#!/usr/bin/env python
#
#    This is stepconf, a graphical configuration editor for LinuxCNC
#    Copyright 2007 Jeff Epler <jepler@unpythonic.net>
#    stepconf 1.1 revamped by Chris Morley 2014
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
#
#***********
# start PAGE
#***********
import os
import xml.dom.minidom
from gi.repository import Gtk
from gi.repository import GObject

def start_prepare(self):
	self.w.createsymlink.set_active(self.d.createsymlink)
	self.w.createshortcut.set_active(self.d.createshortcut)
	self.w.createsimconfig.set_active(self.d.sim_hardware)

def start_finish(self):
	if self.w.importmach.get_active():
		print 'Import Mach config'
		from stepconf import import_mach
		self.load('/tmp/temp.stepconf', self)
		if not self._p.debug:
			os.remove('/tmp/temp.stepconf')
	elif not self.w.createconfig.get_active():
		filter = Gtk.FileFilter()
		filter.add_pattern("*.stepconf")
		filter.set_name(_("LinuxCNC 'stepconf' configuration files"))
		dialog = Gtk.FileChooserDialog(_("Modify Existing Configuration"),
			self.w.window1, Gtk.FileChooserAction.OPEN,
			(Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
			Gtk.STOCK_OPEN, Gtk.ResponseType.OK))
		dialog.set_default_response(Gtk.ResponseType.OK)
		dialog.add_filter(filter)
		if not self.d._lastconfigname == "" and self.d._chooselastconfig:
			dialog.set_filename(os.path.expanduser("~/linuxcnc/configs/%s.stepconf"% self.d._lastconfigname))
		dialog.add_shortcut_folder(os.path.expanduser("~/linuxcnc/configs"))
		dialog.set_current_folder(os.path.expanduser("~/linuxcnc/configs"))
		dialog.show_all()
		result = dialog.run()
		if result == Gtk.ResponseType.OK:
			filename = dialog.get_filename()
			dialog.destroy()
			self.load(filename, self)
		else:
			dialog.destroy()
			return True
	self.d.createsymlink = self.w.createsymlink.get_active()
	self.d.createshortcut = self.w.createshortcut.get_active()
	self.d.sim_hardware = self.w.createsimconfig.get_active()

# callbacks
def on_machinename_changed(self, *args):
	temp = self.w.machinename.get_text()
	self.w.confdir.set_text("~/linuxcnc/configs/%s" % temp.replace(" ","_"))


def load(self, filename, app=None, force=False):
	def str2bool(s):
		return s == 'True'

	converters = {'string': str, 'float': float, 'int': int, 'bool': str2bool, 'eval': eval}

	d = xml.dom.minidom.parse(open(filename, "r"))
	for n in d.getElementsByTagName("property"):
		name = n.getAttribute("name")
		conv = converters[n.getAttribute('type')]
		text = n.getAttribute('value')
		setattr(self.d, name, conv(text))

	warnings = []
	for f, m in self.d.md5sums:
		m1 = self.md5sum(f)
		if m1 and m != m1:
			warnings.append(_("File %r was modified since it was written by stepconf") % f)
	if warnings:
		warnings.append("")
		warnings.append(_("Saving this configuration file will discard configuration changes made outside stepconf."))
		if app:
			dialog = Gtk.MessageDialog(app.w.window1,
				Gtk.DialogFlags.MODAL | Gtk.DialogFlags.DESTROY_WITH_PARENT,
				Gtk.MessageType.WARNING, Gtk.ButtonsType.OK,
					 "\n".join(warnings))
			dialog.show_all()
			dialog.run()
			dialog.destroy()
		else:
			for para in warnings:
				for line in textwrap.wrap(para, 78): print line
				print
			print
			if force: return
			response = raw_input(_("Continue? "))
			if response[0] not in _("yY"): raise SystemExit, 1

	for p in (10,11,12,13,15):
		pin = "pin%d" % p
		p = self.d[pin]
	for p in (1,2,3,4,5,6,7,8,9,14,16,17):
		pin = "pin%d" % p
		p = self.d[pin]

