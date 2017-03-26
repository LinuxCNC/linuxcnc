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
		self.d.load('/tmp/temp.stepconf', self)
		if not self.a.debug:
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
			self.d.load(filename, self)
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
def on_drivertype_changed(self, *args):
	self.a.update_drivertype_info()

