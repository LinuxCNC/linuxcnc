#!/usr/bin/env python

'''
    This IconView widget shows the contents of the currently selected
    directory on the disk.

    it is based on a tutorial from ZetCode PyGTK tutorial
    the original code is under the BSD license
    author: jan bodnar
    website: zetcode.com 

    Copyright 2012 Norbert Schechner
    nieson@web.de

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

'''

import gtk
import gobject
import os
import mimetypes
import gio

# constants
_ASCENDING = 0
_DESCENDING = 1
_FOLDERFIRST = 2
_FILEFIRST = 3
COL_PATH = 0
COL_PIXBUF = 1
COL_IS_DIRECTORY = 2

# prepared for localization
import gettext
_ = gettext.gettext

# icon_size must be an integer, inbetween 12 and 96, default is 48
# start_dir is the start directory, given as a string like "~/linuxcnc-dev/configs/sim/gmoccapy/nc_files"
# user_dir is a user defined directory to jump to, given as a string like "/media"
# filetypes is a comma separated string, giving the extensions of the files to be shown in the widget
# like "ngc,py,png,hal"
# sortorder one of ASCENDING, DESCENDING, FOLDERFIRST, FILEFIRST
class IconFileSelection(gtk.HBox):

# ToDo:
# - make the button up and down work to move faster from top to bottom
#   unfortuantely the selection of column is not availible in pygtk before 2.22

    __gtype_name__ = 'IconFileSelection'
    __gproperties__ = {
           'icon_size' : (gobject.TYPE_INT, 'Icon Size', 'Sets the size of the displayed icon',
                        12, 96, 48, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
           'start_dir' : (gobject.TYPE_STRING, 'start directory', 'Sets the directory to start in',
                        "/", gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
           'jump_to_dir' : (gobject.TYPE_STRING, 'jump to directory', 'Sets the directory to jump to ',
                        "~", gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
           'filetypes' : (gobject.TYPE_STRING, 'file filter', 'Sets the filter for the file types to be shown',
                        "ngc,py", gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
           'sortorder' : (gobject.TYPE_INT, 'sorting order', '0 = ASCENDING, 1 = DESCENDING", 2 = FOLDERFIRST, 3 = FILEFIRST',
                        0, 3, 2, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
                      }
    __gproperties = __gproperties__

    __gsignals__ = {
                    'selected': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_STRING,)),
                    'exit': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
                   }

    def __init__(self):
        super(IconFileSelection, self).__init__()

        # set some default values'
        self.icon_size = 48
        self.start_dir = os.path.expanduser('/')
        self.cur_dir = self.start_dir
        self.user_dir = os.path.expanduser('~')
        self.filetypes = ("ngc,py")
        self.sortorder = _FOLDERFIRST
        # This will hold the path we will return
        self.path = ""

        # Make the GUI and connect signals
        vbox = gtk.VBox(False, 0)

        self.buttonbox = gtk.HButtonBox()
        self.buttonbox.set_layout(gtk.BUTTONBOX_EDGE)
        self.buttonbox.set_property("homogeneous", True)
        vbox.pack_end(self.buttonbox, False, False, 0)

        self.btn_home = gtk.Button()
        self.btn_home.set_size_request(56, 56)
        image = gtk.Image()
        image.set_from_stock(gtk.STOCK_HOME, 48)
        self.btn_home.set_image(image)
        self.btn_home.set_tooltip_text(_("Move to your home directory"))
        self.buttonbox.add(self.btn_home)

        self.btn_dir_up = gtk.Button();
        self.btn_dir_up.set_size_request(56, 56)
        image = gtk.Image()
        image.set_from_stock(gtk.STOCK_GOTO_TOP, 48)
        self.btn_dir_up.set_image(image)
        self.btn_dir_up.set_tooltip_text(_("Move to parrent directory"))
        self.buttonbox.add(self.btn_dir_up)

        self.btn_sel_prev = gtk.Button()
        self.btn_sel_prev.set_size_request(56, 56)
        image = gtk.Image()
        image.set_from_stock(gtk.STOCK_GO_BACK, 48)
        self.btn_sel_prev.set_image(image)
        self.btn_sel_prev.set_tooltip_text(_("Select the previos file"))
        self.buttonbox.add(self.btn_sel_prev)

        self.btn_sel_next = gtk.Button()
        self.btn_sel_next.set_size_request(56, 56)
        image = gtk.Image()
        image.set_from_stock(gtk.STOCK_GO_FORWARD, 48)
        self.btn_sel_next.set_image(image)
        self.btn_sel_next.set_tooltip_text(_("Select the next file"))
        self.buttonbox.add(self.btn_sel_next)

# ToDo : Find out how to move one line down or up
#        self.btn_go_down = gtk.Button()
#        self.btn_go_down.set_size_request(56,56)
#        image = gtk.Image()
#        image.set_from_stock(gtk.STOCK_GO_DOWN,48)
#        self.btn_go_down.set_image(image)
#        self.buttonbox.add(self.btn_go_down)

#        self.btn_go_up = gtk.Button()
#        self.btn_go_up.set_size_request(56,56)
#        image = gtk.Image()
#        image.set_from_stock(gtk.STOCK_GO_UP,48)
#        self.btn_go_up.set_image(image)
#        self.buttonbox.add(self.btn_go_up)
# ToDo : End

        self.btn_jump_to = gtk.Button()
        self.btn_jump_to.set_size_request(56, 56)
        image = gtk.Image()
        image.set_from_stock(gtk.STOCK_JUMP_TO, 48)
        self.btn_jump_to.set_image(image)
        self.btn_jump_to.set_tooltip_text(_("Jump to user defined directory"))
        self.buttonbox.add(self.btn_jump_to)

        self.btn_select = gtk.Button()
        self.btn_select.set_size_request(56, 56)
        image = gtk.Image()
        image.set_from_stock(gtk.STOCK_OK, 48)
        self.btn_select.set_image(image)
        self.btn_select.set_tooltip_text(_("select the highlighted file and return the path"))
        self.buttonbox.add(self.btn_select)

        self.btn_exit = gtk.Button()
        self.btn_exit.set_size_request(56, 56)
        image = gtk.Image()
        image.set_from_stock(gtk.STOCK_STOP, 48)
        self.btn_exit.set_image(image)
        self.btn_exit.set_tooltip_text(_("Close without returning a file path"))
        self.buttonbox.add(self.btn_exit)

        self.dirIcon = self._get_icon("folder")

        sw = gtk.ScrolledWindow()
        sw.set_shadow_type(gtk.SHADOW_ETCHED_IN)
        sw.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
        vbox.pack_start(sw, True, True, 0)

        self.file_label = gtk.Label("File Label")
        vbox.pack_start(self.file_label, False, True, 0)

        self.store = self._create_store()
        self._fill_store()

        self.iconView = gtk.IconView(self.store)
        self.iconView.set_selection_mode(gtk.SELECTION_SINGLE)

        self.iconView.set_text_column(COL_PATH)
        self.iconView.set_pixbuf_column(COL_PIXBUF)

        sw.add(self.iconView)
        self.iconView.grab_focus()
        self.model = self.iconView.get_model()

        self.btn_dir_up.connect("clicked", self.on_btn_dir_up_clicked)
        self.btn_home.connect("clicked", self.on_btn_home_clicked)
        self.btn_sel_next.connect("clicked", self.on_btn_sel_next_clicked)
        self.btn_sel_prev.connect("clicked", self.on_btn_sel_prev_clicked)
#        self.btn_go_down.connect("clicked", self.on_btn_go_down_clicked)
#        self.btn_go_up.connect("clicked", self.on_btn_go_up_clicked)
        self.btn_jump_to.connect("clicked", self.on_btn_jump_to_clicked)
        self.btn_select.connect("clicked", self.on_btn_get_selected_clicked)
        self.btn_exit.connect("clicked", self.on_btn_exit_clicked)
        # will be emitted, when a icon has been activated, so we keep track of the path
        self.iconView.connect("item-activated", self._on_item_activated)
        # will be emitted, when a icon is activated and the ENTER key has been pressed
        self.iconView.connect("activate-cursor-item", self._on_activate_cursor_item)
        self.connect("destroy", gtk.main_quit)

        self.add(vbox)
        self.show_all()

        # To use the the events, we have to unmask them
        self.iconView.add_events(gtk.gdk.BUTTON_PRESS_MASK)
        self.iconView.connect("button_press_event", self._button_press)

    # With the left mouse button and a dobble click, the file can be selected
    def _button_press(self, widget, event):
        # left button used?
        if event.button == 1:
            # dobble click?
            if event.type == gtk.gdk._2BUTTON_PRESS:
                self.btn_select.emit("clicked")

    def _on_activate_cursor_item(self, widget):
        self.btn_select.emit("clicked")

    def _get_icon(self, name):
        theme = gtk.icon_theme_get_default()
        if name == "folder":
            name = gtk.STOCK_DIRECTORY
        else:
            mime = gio.content_type_guess(name)
            if mime:
                iconname = gio.content_type_get_icon(mime)
                icon = theme.choose_icon(iconname.get_names(), self.icon_size, 0)
                if icon:
                    return gtk.IconInfo.load_icon(icon)
                else:
                    name = gtk.STOCK_FILE
            else:
                name = gtk.STOCK_FILE
        return theme.load_icon(name, self.icon_size, 0)

    def _create_store(self):
        store = gtk.ListStore(str, gtk.gdk.Pixbuf, bool)
        return store

    def _fill_store(self):

        if self.cur_dir == None:
            return

        try:
            self.store.clear()
            number = 0
            dirs = []
            files = []
            for fl in os.listdir(self.cur_dir):
                # we don't want to add hidden files
                if not fl[0] == '.':
                    if os.path.isdir(os.path.join(self.cur_dir, fl)):
                        dirs.append(fl)
                        number += 1
                    else:
                        try:
                            name, ext = fl.rsplit(".", 1)
                            if "*" in self.filetypes:
                                files.append(fl)
                                number += 1
                            elif ext in self.filetypes:
                                files.append(fl)
                                number += 1
                        except:
                            pass

            if self.sortorder not in [_ASCENDING, _DESCENDING, _FOLDERFIRST, _FILEFIRST]:
                self.sortorder = _FOLDERFIRST

            if self.sortorder == _ASCENDING or self.sortorder == _DESCENDING:
                allobjects = dirs
                allobjects.extend(files)
                allobjects.sort(cmp = None, key = None, reverse = not self.sortorder == _ASCENDING)

                for obj in allobjects:
                    if os.path.isdir(os.path.join(self.cur_dir, obj)):
                        self.store.append([obj, self.dirIcon, True])
                    else:
                        icon = self._get_icon(obj)
                        self.store.append([obj, icon, False])

            dirs.sort(cmp = None, key = None, reverse = False)
            files.sort(cmp = None, key = None, reverse = False)
            if self.sortorder == _FOLDERFIRST:
                for dir in dirs:
                    self.store.append([dir, self.dirIcon, True])
                for file in files:
                    icon = self._get_icon(file)
                    self.store.append([file, icon, False])
            elif self.sortorder == _FILEFIRST:
                for file in files:
                    icon = self._get_icon(file)
                    self.store.append([file, icon, False])
                for dir in dirs:
                    self.store.append([dir, self.dirIcon, True])

            if number == 0:
                self.btn_sel_next.set_sensitive(False)
                self.btn_sel_prev.set_sensitive(False)
                self.btn_select.set_sensitive(False)
            else:
                self.btn_sel_next.set_sensitive(True)
                self.btn_sel_prev.set_sensitive(True)
                self.btn_select.set_sensitive(True)
        except:
            pass

    def show_buttonbox(self, state):
        if state:
            self.buttonbox.show()
        else:
            self.buttonbox.hide()

    def show_filelabel(self, state):
        if state:
            self.file_label.show()
        else:
            self.file_label.hide()

    def on_btn_home_clicked(self, widget):
        self.cur_dir = os.path.realpath(os.path.expanduser('~'))
        self._fill_store()
        self.btn_dir_up.set_sensitive(True)

    def on_btn_jump_to_clicked(self, widget):
        self.cur_dir = self.jump_to_dir
        self._fill_store()
        self.btn_dir_up.set_sensitive(True)

    def get_iter_last(self, model):
        itr = model.get_iter_first()
        last = None
        while itr:
            last = itr
            itr = model.iter_next(itr)
        return last

    def on_btn_sel_prev_clicked(self, data):
        if not self.btn_sel_prev.is_sensitive():
            return None
        try:
            actual = self.iconView.get_cursor()[0]
            iter = self.model.get_iter(actual)
            pos = int(self.model.get_string_from_iter(iter))
            first = int(self.model.get_string_from_iter(self.model.get_iter_first()))
            pos = pos - 1
            if pos < first:
                pos = first
            new_iter = self.model.get_iter_from_string(str(pos))
            new_path = self.model.get_path(new_iter)
        except:
            new_iter = self.get_iter_last(self.model)
            new_path = self.model.get_path(new_iter)
        self.iconView.set_cursor(new_path)
        self.iconView.select_path(new_path)

    def on_btn_sel_next_clicked(self, data):
        if not self.btn_sel_next.is_sensitive():
            return None
        try:
            actual = self.iconView.get_cursor()[0]
            iter = self.model.get_iter(actual)
            try:
                new_iter = self.model.iter_next(iter)
                new_path = self.model.get_path(new_iter)
                self.iconView.set_cursor(new_path)
                self.iconView.select_path(new_path)
            except:
                new_path = self.model.get_path(iter)
                self.iconView.set_cursor(new_path)
                self.iconView.select_path(new_path)
        except:
            self.iconView.set_cursor(0)
            self.iconView.select_path(0)

# ToDo: find out how to move a line down or up
#    def on_btn_go_down_clicked(self,data):
#        print("go down")
#        print("columns = ",self.iconView.get_columns())
#        print("column spacing = ", self.iconView.get_column_spacing())
#        print("item with = ", self.iconView.get_item_width())
#        print("margin = ", self.iconView.get_margin())
#        print("visible range = ", self.iconView.get_visible_range())
#        print("get cursor = ", self.iconView.get_cursor())
#        print("item row = ", self.iconView.get_item_at_row(self.get_selected()))
#        print("item column = ", self.iconView.get_item_column(self.get_selected()))


#    def on_btn_go_up_clicked(self,data):
#        print("go up")
# ToDo: end

    def set_icon_size(self, iconsize):
        self.icon_size = iconsize
        self.dirIcon = self._get_icon("folder")
        self._fill_store()

    def set_directory(self, directory):
        self.cur_dir = os.path.expanduser(directory)
        self._fill_store()

    def set_filetypes(self, filetypes):
        self.filetypes = filetypes.split(",")
        self._fill_store()

    def refresh_filelist(self):
        self._fill_store()

    def get_selected(self):
        return on_btn_get_selected_clicked(self)

    def on_btn_get_selected_clicked(self, data):
        try:
            self.iconView.item_activated(self.iconView.get_cursor()[0])
            if self.path:
                filepath = self.cur_dir + os.path.sep + self.path
                self.file_label.set_text(filepath)
            else:
                self.file_label.set_text("")
                filepath = None
            self.emit('selected', filepath)
        except:
            pass

    def on_btn_exit_clicked(self, data):
        if __name__ == "__main__":
            gtk.main_quit()
        self.emit('exit')

    def _on_item_activated(self, widget, item):
        path = self.model[item][COL_PATH]
        isDir = self.model[item][COL_IS_DIRECTORY]

        if not isDir:
            self.path = path
            return

        self.cur_dir = self.cur_dir + os.path.sep + path
        self._fill_store()
        self.btn_dir_up.set_sensitive(True)
        # This is only to advise, that the selection wasn't a file, but an directory
        self.path = None

    def on_btn_dir_up_clicked(self, widget):
        self.cur_dir = os.path.dirname(self.cur_dir)
        self._fill_store()
        sensitive = True
        if self.cur_dir == "/" :
            sensitive = False
        self.btn_dir_up.set_sensitive(sensitive)
        return self.cur_dir

    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name in self.__gproperties.keys():
            return getattr(self, name)
        else:
            raise AttributeError('unknown iconview get_property %s' % property.name)

    def do_set_property(self, property, value):
        try:
            name = property.name.replace('-', '_')
            if name in self.__gproperties.keys():
                setattr(self, name, value)
                self.queue_draw()
                if name == 'icon_size':
                    self.set_icon_size(value)
                if name == 'start_dir':
                    self.start_dir = os.path.expanduser(value)
                    self.set_directory(self.start_dir)
                if name == 'jump_to_dir':
                    self.jump_to_dir = os.path.expanduser(value)
                    self.on_btn_jump_to()
                if name == 'filetypes':
                    self.set_filetypes(value)
                if name == 'sortorder':
                    if value not in [_ASCENDING, _DESCENDING, _FOLDERFIRST, _FILEFIRST]:
                        raise AttributeError('unknown property of sortorder %s' % value)
                    else:
                        self.sortorder = value
                        self._fill_store()
            else:
                raise AttributeError('unknown iconview set_property %s' % property.name)
        except:
            pass

# for testing without glade editor:
def main():
    window = gtk.Window(gtk.WINDOW_TOPLEVEL)

    IFS = IconFileSelection()
    IFS.set_property("filetypes", "*")

    window.add(IFS)
    window.connect("destroy", gtk.main_quit)
    window.show_all()
    window.set_size_request(680, 480)
    gtk.main()

if __name__ == "__main__":
    main()
