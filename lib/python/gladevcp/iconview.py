#!/usr/bin/env python3

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

'''

import gi
gi.require_version("Gtk","3.0")
gi.require_version("Gdk","3.0")
from gi.repository import Gtk
from gi.repository import Gdk
from gi.repository import Gio
from gi.repository import GObject
from gi.repository import GdkPixbuf
import os
import mimetypes

# Set up logging
from qtvcp import logger

LOG = logger.getLogger(__name__)
# Force the log level for this module
# LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

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
class IconFileSelection(Gtk.Box):

# ToDo:
# - make the button up and down work to move faster from top to bottom
#   unfortunately the selection of column is not available in pygtk before 2.22

    __gtype_name__ = 'IconFileSelection'
    __gproperties__ = {
           'icon_size' : (GObject.TYPE_INT, 'Icon Size', 'Sets the size of the displayed icon',
                        12, 96, 48, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
           'start_dir' : (GObject.TYPE_STRING, 'start directory', 'Sets the directory to start in',
                        ".", GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
           'jump_to_dir' : (GObject.TYPE_STRING, 'jump to directory', 'Sets the directory to jump to ',
                        "~", GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
           'filetypes' : (GObject.TYPE_STRING, 'file filter', 'Sets the filter for the file types to be shown',
                        "ngc,py", GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
           'sortorder' : (GObject.TYPE_INT, 'sorting order', '0 = ASCENDING, 1 = DESCENDING", 2 = FOLDERFIRST, 3 = FILEFIRST',
                        0, 3, 2, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
                      }
    __gproperties = __gproperties__

    __gsignals__ = {
                    'selected': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_STRING,)),
                    'sensitive': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_STRING, GObject.TYPE_BOOLEAN,)),
                    'exit': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, ()),
                   }

    def __init__(self):
        super(IconFileSelection, self).__init__()

        # we need this, because otherwise the buttons will react on creating them
#        self.realized = False

        # set some default values'
        self.icon_size = 48
        self.start_dir = os.path.expanduser('/')
        self.cur_dir = self.start_dir
        self.user_dir = os.path.expanduser('~')
        self.jump_to_dir = os.path.expanduser('/tmp')
        self.filetypes = ("ngc,py")
        self.sortorder = _FOLDERFIRST
        # This will hold the path we will return
        self.path = ""
        self.button_state = {}
        self.old_button_state = {}

        # Make the GUI and connect signals
        vbox = Gtk.Box(homogeneous = False, spacing = 0)
        vbox.set_orientation(Gtk.Orientation.VERTICAL)

        self.buttonbox = Gtk.ButtonBox()
        self.buttonbox.set_layout(Gtk.ButtonBoxStyle.EDGE)
        self.buttonbox.set_property("homogeneous", True)
        vbox.pack_end(self.buttonbox, False, False, 0)

        self.btn_home = Gtk.Button()
        self.btn_home.set_size_request(56, 56)
        image = Gtk.Image()
        pixbuf = Gtk.IconTheme.get_default().load_icon("go-home", 48, 0)
        image.set_from_pixbuf(pixbuf)
        self.btn_home.set_image(image)
        self.btn_home.set_tooltip_text(_("Move to your home directory"))
        self.buttonbox.add(self.btn_home)

        self.btn_dir_up = Gtk.Button();
        self.btn_dir_up.set_size_request(56, 56)
        image = Gtk.Image()
        pixbuf = Gtk.IconTheme.get_default().load_icon("go-top", 48, 0)
        image.set_from_pixbuf(pixbuf)
        self.btn_dir_up.set_image(image)
        self.btn_dir_up.set_tooltip_text(_("Move to parent directory"))
        self.buttonbox.add(self.btn_dir_up)

        self.btn_sel_prev = Gtk.Button()
        self.btn_sel_prev.set_size_request(56, 56)
        image = Gtk.Image()
        pixbuf = Gtk.IconTheme.get_default().load_icon("go-previous", 48, 0)
        image.set_from_pixbuf(pixbuf)
        self.btn_sel_prev.set_image(image)
        self.btn_sel_prev.set_tooltip_text(_("Select the previous file"))
        self.buttonbox.add(self.btn_sel_prev)

        self.btn_sel_next = Gtk.Button()
        self.btn_sel_next.set_size_request(56, 56)
        image = Gtk.Image()
        pixbuf = Gtk.IconTheme.get_default().load_icon("go-next", 48, 0)
        image.set_from_pixbuf(pixbuf)
        self.btn_sel_next.set_image(image)
        self.btn_sel_next.set_tooltip_text(_("Select the next file"))
        self.buttonbox.add(self.btn_sel_next)

# ToDo : Find out how to move one line down or up
#        self.btn_go_down = Gtk.Button()
#        self.btn_go_down.set_size_request(56,56)
#        image = Gtk.Image()
#        image.set_from_stock(Gtk.STOCK_GO_DOWN,48)
#        self.btn_go_down.set_image(image)
#        self.buttonbox.add(self.btn_go_down)
#
#        self.btn_go_up = Gtk.Button()
#        self.btn_go_up.set_size_request(56,56)
#        image = Gtk.Image()
#        image.set_from_stock(Gtk.STOCK_GO_UP,48)
#        self.btn_go_up.set_image(image)
#        self.buttonbox.add(self.btn_go_up)
# ToDo : End

        self.btn_jump_to = Gtk.Button()
        self.btn_jump_to.set_size_request(56, 56)
        image = Gtk.Image()
        pixbuf = Gtk.IconTheme.get_default().load_icon("go-jump", 48, 0)
        image.set_from_pixbuf(pixbuf)
        self.btn_jump_to.set_image(image)
        self.btn_jump_to.set_tooltip_text(_("Jump to user defined directory"))
        self.buttonbox.add(self.btn_jump_to)

        self.btn_select = Gtk.Button()
        self.btn_select.set_size_request(56, 56)
        image = Gtk.Image()
        pixbuf = Gtk.IconTheme.get_default().load_icon("media-playback-start", 48, 0)
        image.set_from_pixbuf(pixbuf)
        self.btn_select.set_image(image)
        self.btn_select.set_tooltip_text(_("select the highlighted file and return the path"))
        self.buttonbox.add(self.btn_select)

        self.btn_exit = Gtk.Button()
        self.btn_exit.set_size_request(56, 56)
        image = Gtk.Image()
        pixbuf = Gtk.IconTheme.get_default().load_icon("application-exit", 48, 0)
        image.set_from_pixbuf(pixbuf)
        self.btn_exit.set_image(image)
        self.btn_exit.set_tooltip_text(_("Close without returning a file path"))
        self.buttonbox.add(self.btn_exit)

        self.dirIcon = Gtk.IconTheme.get_default().load_icon("folder", self.icon_size, 0)

        sw = Gtk.ScrolledWindow()
        sw.set_shadow_type(Gtk.ShadowType.ETCHED_IN)
        sw.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC)
        vbox.pack_start(sw, True, True, 0)

        self.file_label = Gtk.Label(label = "File Label")
        vbox.pack_start(self.file_label, False, True, 0)

        self.store = self._create_store()

        self.iconView = Gtk.IconView.new()
        self.iconView.set_model(self.store)
        self.iconView.set_selection_mode(Gtk.SelectionMode.SINGLE)

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
        self.btn_select.connect("clicked", self.on_btn_select_clicked)
        self.btn_exit.connect("clicked", self.on_btn_exit_clicked)
        # will be emitted, when a icon has been activated, so we keep track of the path
        self.iconView.connect("item-activated", self._on_item_activated)
        # will be emitted, when a icon is activated and the ENTER key has been pressed
        self.iconView.connect("activate-cursor-item", self._on_activate_cursor_item)
        # initialize the button states before connecting to a signal
        self._init_button_state()
        # will be emitted if the selection has changed, this happens also if the user clicks ones on an icon
        self.iconView.connect("selection-changed",  self._on_selection_changed)
        # will be emitted, when the widget is destroyed
        self.connect("destroy", Gtk.main_quit)

        self.pack_start(vbox, fill=True, expand=True, padding=0)
        self.show_all()

        # To use the the events, we have to unmask them
        self.iconView.add_events(Gdk.EventMask.BUTTON_PRESS_MASK)
        self.iconView.connect("button_press_event", self._button_press)

        self._fill_store()
#        self.realized = True

    def _init_button_state(self):
        # we need this to check for differences in the button state
        self.button_state["btn_home"] = self.btn_home.get_sensitive()
        self.button_state["btn_dir_up"] = self.btn_dir_up.get_sensitive()
        self.button_state["btn_sel_prev"] = self.btn_sel_prev.get_sensitive()
        self.button_state["btn_sel_next"] = self.btn_sel_next.get_sensitive()
        self.button_state["btn_jump_to"] = self.btn_jump_to.get_sensitive()
        self.button_state["btn_select"] = self.btn_select.get_sensitive()
        self.old_button_state = self.button_state.copy()

    # With the left mouse button and a dobble click, the file can be selected
    def _button_press(self, widget, event):
        # left button used?
        if event.button == 1:
            # dobble click?
            if event.type == Gdk.EventType._2BUTTON_PRESS:
                self.btn_select.emit("clicked")

    def _on_activate_cursor_item(self, widget):
        self.btn_select.emit("clicked")

    def _get_icon(self, name):
        mime = Gio.content_type_guess(name)
        if mime:
            iconname = Gio.content_type_get_icon(mime[0])
            icon = Gtk.IconTheme.get_default().choose_icon(iconname.get_names(), self.icon_size, 0)
            if icon:
                return Gtk.IconInfo.load_icon(icon)
        else:
            name = Gtk.STOCK_FILE

        return Gtk.IconTheme.get_default().load_icon(name, self.icon_size, 0)

    def _create_store(self):
        store = Gtk.ListStore(str, GdkPixbuf.Pixbuf, bool)
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
                if fl[0] == '.':
                    continue
                if os.path.isdir(os.path.join(self.cur_dir, fl)):
                    try:
                        os.listdir(os.path.join(self.cur_dir, fl))
                        dirs.append(fl)
                        number += 1
                    except OSError:
                        #print ("no rights for ", os.path.join(self.cur_dir, fl), " skip that dir")
                        continue
                else:
                    try:
                        name, ext = fl.rsplit(".", 1)
                        if "*" in self.filetypes:
                            files.append(fl)
                            number += 1
                        elif ext in self.filetypes:
                            files.append(fl)
                            number += 1
                    except ValueError as e:
                        LOG.debug("Tried to split filename without extension ({0}).".format(e))
                    except Exception as e:
                        LOG.exception(e)
                        pass

            if self.sortorder not in [_ASCENDING, _DESCENDING, _FOLDERFIRST, _FILEFIRST]:
                self.sortorder = _FOLDERFIRST

            if self.sortorder == _ASCENDING or self.sortorder == _DESCENDING:
                allobjects = dirs
                allobjects.extend(files)
                allobjects.sort(reverse = not self.sortorder == _ASCENDING)

                for obj in allobjects:
                    if os.path.isdir(os.path.join(self.cur_dir, obj)):
                        self.store.append([obj, self.dirIcon, True])
                    else:
                        icon = self._get_icon(obj)
                        self.store.append([obj, icon, False])

            dirs.sort(key = None, reverse = False)
            files.sort(key = None, reverse = False)
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
        except Exception as e:
            LOG.exception(e)
            pass
        finally:
            # check the stat of the button and set them as they should be
            self.check_button_state()

    def check_button_state(self):
        state = True
        if not self.iconView.get_cursor()[0]:
            state = False
        elif self.model.get_iter_first() == None:
            state = False
        else:
            state = True
        self.btn_sel_next.set_sensitive(state)
        self.button_state["btn_sel_next"] = state
        self.btn_sel_prev.set_sensitive(state)
        self.button_state["btn_sel_prev"] = state
        self.btn_select.set_sensitive(state)
        self.button_state["btn_select"] = state
        state = self.cur_dir == self.user_dir
        self.btn_home.set_sensitive(not state)
        self.button_state["btn_home"] = not state
        state = self.cur_dir == "/"
        self.btn_dir_up.set_sensitive(not state)
        self.button_state["btn_dir_up"] = not state
        state = self.cur_dir == self.jump_to_dir
        self.btn_jump_to.set_sensitive(not state)
        self.button_state["btn_jump_to"] = not state
        state = self.iconView.get_cursor() == None
        self.btn_select.set_sensitive(not state)
        self.button_state["btn_select"] = not state
        self.state_changed()

    def state_changed(self):
        # find the difference (not used)
        # diff = set(self.button_state.items()) - set(self.old_button_state.items())
        for key in self.button_state.keys():
            try:
                if self.button_state[key] != self.old_button_state[key]:
                    self.emit("sensitive",key, self.button_state[key])
            except Exception as e:
                LOG.exception(e)
                continue

        self.old_button_state = self.button_state.copy()

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
        self.cur_dir = self.user_dir
        self._fill_store()

    def on_btn_jump_to_clicked(self, widget):
        self.cur_dir = self.jump_to_dir
        self._fill_store()

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
            actual = self.iconView.get_cursor()[1]
            iter = self.model.get_iter(actual)

            pos = int(self.model.get_string_from_iter(iter))
            first = int(self.model.get_string_from_iter(self.model.get_iter_first()))
            pos = pos - 1
            if pos < first:
                pos = int(self.model.get_string_from_iter(self.get_iter_last(self.model)))
            new_iter = self.model.get_iter_from_string(str(pos))
            new_path = self.model.get_path(new_iter)

        except Exception as e:
            LOG.exception(e)
        #    new_iter = self.model.get_iter_last(self.model)
        new_path = self.model.get_path(new_iter)
        self.iconView.set_cursor(new_path,  None, False)
        self.iconView.select_path(new_path)
        self.check_button_state()

    def on_btn_sel_next_clicked(self, data):
        if not self.btn_sel_next.is_sensitive():
            return None
        try:
            actual = self.iconView.get_cursor()[1]
            iter = self.model.get_iter(actual)

            pos = int(self.model.get_string_from_iter(iter))
            last = int(self.model.get_string_from_iter(self.get_iter_last(self.model)))
            pos = pos + 1
            if pos > last:
                pos = int(self.model.get_string_from_iter(self.model.get_iter_first()))
            new_iter = self.model.get_iter_from_string(str(pos))
            new_path = self.model.get_path(new_iter)
            self.iconView.set_cursor(new_path,  None, False)
            self.iconView.select_path(new_path)

        except Exception as e:
            LOG.exception(e)
            new_iter = self.get_iter_last(self.model)
            new_path = self.model.get_path(new_iter)
        self.iconView.set_cursor(new_path,  None, False)
        self.iconView.select_path(new_path)
        self.check_button_state()

# ToDo: find out how to move a line down or up
#    def on_btn_go_down_clicked(self,data):
#        print("This is the number of the icon selected", self.iconView.get_cursor())
#        col = self.iconView.get_item_column(self.iconView.get_cursor()[0])
#        row = self.iconView.get_item_row(self.iconView.get_cursor()[0])
#        print("This is the column :", col)
#        print("This is the row :", row)
#        print(self.iconView.get_columns())


#        self.iconView.item_activated(self.iconView.get_cursor()[0])
#        print("go down")
#        print("columns = ",self.iconView.get_columns())
#        print("column spacing = ", self.iconView.get_column_spacing())
#        print("item with = ", self.iconView.get_item_width())
#        print("margin = ", self.iconView.get_margin())
#        print("visible range = ", self.iconView.get_visible_range())
#        print("get cursor = ", self.iconView.get_cursor())
#        print("item row = ", self.iconView.get_item_at_row(self.get_selected()))
#        print("item column = ", self.iconView.get_item_column(self.get_selected()))
#
#
#    def on_btn_go_up_clicked(self,data):
#        print("go up")
# ToDo: end

    def set_icon_size(self, iconsize):
        try:
            self.icon_size = iconsize
            self._fill_store()
        except Exception as e:
            LOG.exception(e)
            pass

    def set_directory(self, directory):
        self.cur_dir = os.path.expanduser(directory)
        self._fill_store()

    def set_filetypes(self, filetypes):
        self.filetypes = filetypes.split(",")
        self._fill_store()

    def refresh_filelist(self):
        self._fill_store()

    def get_selected(self):
        return self.on_btn_select_clicked(self)

    def on_btn_select_clicked(self, data):
        try:
            if self.iconView.get_cursor()[0]:
                self.iconView.item_activated(self.iconView.get_cursor()[1])
                if self.path:
                    filepath = self.cur_dir + os.path.sep + self.path
                    self.file_label.set_text(filepath)
                else:
                    self.file_label.set_text("")
                    filepath = None
                if filepath is not None:
                    self.emit('selected', filepath)

        except Exception as e:
            LOG.exception(e)
            pass

    def on_btn_exit_clicked(self, data):
        if __name__ == "__main__":
            Gtk.main_quit()
        self.emit('exit')

    def _on_item_activated(self, widget, item):
        path = self.model[item][COL_PATH]
        isDir = self.model[item][COL_IS_DIRECTORY]

        if not isDir:
            self.path = path
            return

        self.cur_dir = self.cur_dir + os.path.sep + path
        self._fill_store()
        # This is only to advise, that the selection wasn't a file, but an directory
        self.path = None

    def _on_selection_changed(self, widget):
        if not self.btn_select.get_sensitive():
            self.btn_select.set_sensitive(True)
        self.check_button_state()

    def on_btn_dir_up_clicked(self, widget):
        self.cur_dir = os.path.dirname(self.cur_dir)
        self._fill_store()

    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name in list(self.__gproperties.keys()):
            return getattr(self, name)
        else:
            raise AttributeError('unknown iconview get_property %s' % property.name)

    def do_set_property(self, property, value):
        try:
            name = property.name.replace('-', '_')
            if name in list(self.__gproperties.keys()):
                setattr(self, name, value)
                self.queue_draw()
                if name == 'icon_size':
                    self.set_icon_size(value)
                if name == 'start_dir':
                    self.start_dir = os.path.expanduser(value)
                    self.set_directory(self.start_dir)
                if name == 'jump_to_dir':
                    self.jump_to_dir = os.path.expanduser(value)
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
        except Exception as e:
            LOG.exception(e)
            pass

# for testing without glade editor:
def main():
    window = Gtk.Window(type = Gtk.WindowType.TOPLEVEL)

    IFS = IconFileSelection()
    #IFS.set_property("filetypes", "*")
    IFS.set_property("jump_to_dir", "/tmp")

    window.add(IFS)
    window.connect("destroy", Gtk.main_quit)
    window.show_all()
    window.set_size_request(680, 480)
    Gtk.main()


if __name__ == "__main__":
    main()
