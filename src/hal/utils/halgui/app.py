
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

import os, sys, gtk, gobject

from design import Design
from load import file_load, file_new
from save import file_save

def appquit(*args):
	gtk.main_quit()

def open_file(action):
	app.openfile()

def save_file(action):
	if app.design.file_name:
		file_save(app.design, app.design.file_name)
	else:
		app.savefile()

def save_file_as(action):
	app.savefile()

def new_file(action):
	file_new(app.design)

def refresh(action):
	app.design.update()

menu_entries = (
	("FileMenu", None, "_File"),
	("New", gtk.STOCK_NEW, None, "<control>N",
		"Create a new file", new_file),
	("Open", gtk.STOCK_OPEN, None, "<control>O",
		"Open an existing file", open_file),
	("Save", gtk.STOCK_SAVE, None, "<control>S",
		"Save file", save_file),
	("Save as", gtk.STOCK_SAVE_AS, None, "<shift><control>S",
		"Save file with new name", save_file_as),
	("Quit", gtk.STOCK_QUIT, None, "<control>Q",
		"Quit program", appquit),
	("ViewMenu", None, "_View"),
	("Refresh", gtk.STOCK_REFRESH, None, "<control>R",
		"Refresh", refresh),
)

class Application(gtk.Window):
	def __init__(self, parent=None):
		global app
		app = self
		gtk.Window.__init__(self)
		try:
			self.set_screen(parent.get_screen())
		except AttributeError:
			self.connect('destroy', lambda *w: gtk.main_quit())

		self.design = Design(self)

		actions = gtk.ActionGroup("Actions")
		actions.add_actions(menu_entries)
		
		ui = gtk.UIManager()
		ui.insert_action_group(actions, 0)
		self.add_accel_group(ui.get_accel_group())

		# better path to 'ui.xml' needed
		uifile = os.path.join(os.path.dirname(sys.argv[0]), "ui.xml")

		try:
			mergeid = ui.add_ui_from_file(uifile)
		except gobject.GError, msg:
			print "error building menus: %s" % (msg)
		
		box1 = gtk.VBox(False, 0)

		self.add(box1)

		box1.pack_start(ui.get_widget("/MenuBar"), False, False)
		box1.pack_start(ui.get_widget("/ToolBar"), False, False)

		box1.pack_start(self.design, True, True)

		statusbar = gtk.Statusbar()
		box1.pack_start(statusbar, False, False)

		self.set_default_size(1024, 768)
		self.settitle()
		self.set_border_width(0)

	def show_app(self):
		self.show_all()

	def settitle(self):
		if self.design.file_name:
			self.set_title("Crapahalic - " + self.design.file_name)
		else:
			self.set_title("Crapahalic")

	def openfile(self):
		dialog = gtk.FileChooserDialog(title="Open...", action=gtk.FILE_CHOOSER_ACTION_OPEN,
			buttons=(gtk.STOCK_CANCEL,gtk.RESPONSE_CANCEL,gtk.STOCK_OPEN,gtk.RESPONSE_OK))
		dialog.set_default_response(gtk.RESPONSE_OK)
		hal_filter = gtk.FileFilter()
		hal_filter.set_name("craphal files")
		hal_filter.add_pattern("*.hal")
		dialog.add_filter(hal_filter)
		response = dialog.run()
		if response == gtk.RESPONSE_OK:
			new_file(None)
			file_load(app.design, dialog.get_filename())
		dialog.destroy()

	def savefile(self):
		dialog = gtk.FileChooserDialog(title="Save...", action=gtk.FILE_CHOOSER_ACTION_SAVE,
			buttons=(gtk.STOCK_CANCEL,gtk.RESPONSE_CANCEL,gtk.STOCK_SAVE,gtk.RESPONSE_OK))
		dialog.set_default_response(gtk.RESPONSE_OK)
		hal_filter = gtk.FileFilter()
		hal_filter.set_name("craphal files")
		hal_filter.add_pattern("*.hal")
		dialog.add_filter(hal_filter)
		response = dialog.run()
		if response == gtk.RESPONSE_OK:
			file_save(app.design, dialog.get_filename())
		dialog.destroy()

