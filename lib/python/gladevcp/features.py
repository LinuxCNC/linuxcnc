#!/usr/bin/env python
# coding: utf-8
#
# Copyright (c) 2012:
#    Nick Drobchenko - Nick from both cnc-club.ru and linuxcnc.org forums,
#    Fernand Veilleux - FernV from linuxcnc.org forum,
#    Sergey - verser from both cnc-club.ru and linuxcnc.org forums,
#    Meison Kim, Alexander Wigen, Konstantin Navrockiy.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

import gtk
# -----------------------------------------------------------------------------
# ---  SETTINGS : YOU CAN CHANGE FOLLOWING LINES VALUES TO FIT YOUR NEEDS   ---

DEFAULT_DISPLAY = 'axis'  # or 'gmoccapy'
DEFAULT_METRIC = True
DEFAULT_CATALOG = 'mill'  # or could be 'lathe' or any valid sub-directory name in catalog directory

# change icon sizes at will
# following are popular sizes :
# gtk-icon-sizes = "panel=16,16 : panel-menu=16,16 : gtk-menu=16,16 : gtk-button=20,20 :
# gtk-small-toolbar=16,16 : gtk-large-toolbar=24,24 : gtk-dialog=32,32 : gtk-dnd=32,32"

# best options are gtk.ICON_SIZE_MENU, gtk.ICON_SIZE_LARGE_TOOLBAR,
#  gtk.ICON_SIZE_DND and gtk.ICON_SIZE_DIALOG.

MENU_ICON_SIZE = gtk.ICON_SIZE_DND

TV_ICON_SIZE = 30
ADD_MENU_ICON_SIZE = 30
ADD_DLG_IMAGE_SIZE = 65
TOP_FEATURES_ICON_SIZE = 30

# if False, NO_ICON_FILE will be used
DEFAULT_USE_NO_ICON = False
NO_ICON_FILE = 'no-icon.png'

use_top_features_toolbar = True
MAX_TOP_FEATURES = 15
SUPER_TOP_FEATURES_COUNT = 10  # must be less than MAX_TOP_FEATURES

# if False, will use treeview2
# else will have a box with checkboxes, spinbuttons, comboboxes, etc...
DEFAULT_USE_WIDGETS = False
show_tooltip_in_widget_box = True

DEFAULT_DIGITS = '3'

# info at http://www.pygtk.org/pygtk2reference/pango-markup-language.html
header_fmt_str = '<i>%s...</i>'
sub_header_fmt_str = '<i>%s...</i>'
sub_header_fmt_str2 = '<b><i>%s</i></b>'
feature_fmt_str = '<b>%s</b>'
items_fmt_str = '<span foreground="blue" style="oblique"><b>%s</b></span>'

restore_expand_state = True

UNDO_MAX_LEN = 200

# virtual numeric keyboard
vkb_minimum_width = 260
vkb_height = 280
vkb_cancel_on_focus_out = True

# ---  END SETTINGS, CHANGES AFTER THIS LINE WILL AFFECT APPLICATION   -------
# ----------------------------------------------------------------------------

import sys

import pygtk
pygtk.require('2.0')

import pango

from lxml import etree
import gobject
import ConfigParser
import re, os
import getopt
import linuxcnc
import subprocess
import webbrowser
import io
from cStringIO import StringIO
import gettext
import time
import urllib


# About Dialog strings
APP_TITLE = "LinuxCNC-Features"
APP_VERSION = "2.01"
APP_COPYRIGHT = 'Copyright © 2012 Nick Drobchenko aka Nick from cnc-club.ru'
APP_AUTHORS = ['Nick Drobchenko', 'Meison Kim', 'Alexander Wigen',
               'Konstantin Navrockiy', 'Fernand Veilleux', 'Mit Zot']
APP_COMMENTS = 'A GUI to help create NGC files.'
APP_LICENCE = '''This program is free software: you can redistribute
it and/or modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation, either version 2 of
 the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.'''

try :
    t = gettext.translation('linuxcnc-features', '/usr/share/locale')
    _ = t.ugettext
except :
    gettext.install('linuxcnc-features', None, unicode = True)

# These could be changed but recommend to leave as is
INI_DIR = '/ini/'
XML_DIR = '/xml/'
LIB_DIR = '/lib/'
INC_DIR = '/lib/include/'
NGC_DIR = '/scripts/'
EXAMPLES_DIR = '/xml/examples/'
CATALOGS_DIR = '/catalogs/'
TEMPLATES_DIR = '/xml/templates/'
GRAPHICS_DIR = '/graphics/images/'

DEFAULT_TEMPLATE = "def_template.xml"

SUPPORTED_DATA_TYPES = ['sub-header', 'header', 'bool', 'boolean', 'int',
                        'float', 'string', 'combo', 'items', 'filename']
NO_ICON_TYPES = ['sub-header', 'header']
DEFAULT_ICONS = {
    "enabled": "enable.png",
    "items": "items.png"
}
XML_TAG = "LinuxCNC-Features"
PREFERENCES_FILE = "/features.conf"
TOP_FEATURES = "/top.lst"
DEFAULTS = '/defaults.ngc'
POSTAMBLE = '/postamble.ngc'

HOME_PAGE = 'http://fernv.github.io/linuxcnc-features/'

SEL_IS_NONE = 0
SEL_IS_FEATURE = 1
SEL_IS_ITEMS = 2
SEL_IS_HEADER = 3
SEL_IS_PARAM = 4

TARGETS = [
    ('MY_TREE_MODEL_ROW', gtk.TARGET_SAME_WIDGET, 0),
    ('text/plain', 0, 1),
    ('TEXT', 0, 2),
    ('STRING', 0, 3),
    ]

NO_SUBROUTINE_PATH_MSG = '''Warning! There's no SUBROUTINES_PATH in ini file!\n
    Edit your linuxcnc ini file and add in RS274NGC section\n
    SUBROUTINE_PATH = /home/fernand/linuxcnc-features/lib:
    /home/fernand/linuxcnc-features/scripts\n
    or similar to lib and other needed directory'''

NO_PROGRAM_PREFIX_MSG = '''There's no PROGRAM_PREFIX in ini file!\n
    Edit your linuxcnc ini file and add in DISPLAY section\n
    PROGRAM_PREFIX = abs or relative path to scripts directory\n
    i.e. PROGRAM_PREFIX = /home/fernand/linuxcnc-features/scripts'''

DEFAULT_UNITS_NOT_DETECTED = _("""Warning! Units used not detected. 
                                Using imperial/inch!""")

INCLUDE = []
DEFINITIONS = []
PIXBUF_DICT = {}

UNIQUE_ID = 10000

def get_int(s10) :
    try :
        return int(s10)
    except :
        return 0

def get_float(s10) :
    try :
        return float(s10)
    except :
        return 0.0

def search_path(f) :
    if os.path.isfile(f) :
        return f
    src = APP_PATH + f
    if os.path.isfile(src) :
        return src
    src = APP_PATH + INI_DIR + f
    if os.path.isfile(src) :
        return src
    src = APP_PATH + XML_DIR + f
    if os.path.isfile(src) :
        return src
    src = APP_PATH + LIB_DIR + f
    if os.path.isfile(src) :
        return src
    src = APP_PATH + INC_DIR + f
    if os.path.isfile(src) :
        return src
    mess_dlg(_("Can not find file %s") % f)
    return None

def get_pixbuf(icon, size) :
    if size < 22 :
        size = 22
    if ((icon is None) or (icon == "")) :
        if DEFAULT_USE_NO_ICON:
            return None
        else :
            icon = NO_ICON_FILE

    s = icon.split("/")
    icon = s[-1]
    icon_id = icon + str(size)
    icon = APP_PATH + GRAPHICS_DIR + icon

    if (icon_id) in PIXBUF_DICT :
        return PIXBUF_DICT[icon_id]

    if not os.path.isfile(icon):
        StatusBar.push(SB_Pix_context_id, _('Image file not found : %s' % icon))
        print(_('Image file not found : %s' % icon))
        PIXBUF_DICT[icon_id] = None
        return None

    try :
        pix_buf = gtk.gdk.pixbuf_new_from_file(icon)
        h = pix_buf.get_height()
        w = pix_buf.get_width()
        if w > h :
            w, h = size, size * h / w
        else :
            h, w = size, size * w / h
        pix_buf = pix_buf.scale_simple(w, h, gtk.gdk.INTERP_BILINEAR)

        PIXBUF_DICT[icon_id] = pix_buf
        return pix_buf
    except :
        StatusBar.push(SB_Pix_context_id, _('Image file not valid : %s' % icon))
        print(_('Image file not valid : %s' % p))
        PIXBUF_DICT[icon_id] = None
        return None

def mess_dlg(mess):
    dlg = gtk.MessageDialog(parent = None,
        flags = gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
        type = gtk.MESSAGE_WARNING,
        buttons = gtk.BUTTONS_OK, message_format = '%s' % mess)
    dlg.set_title('LinuxCNC-Features')
    dlg.set_keep_above(True)
    dlg.run()
    dlg.destroy()

class CellRendererMx(gtk.CellRendererText):

    def __init__(self) :
        gtk.CellRendererText.__init__(self)
        self.max_value = 99999.9
        self.min_value = -99999.9
        self.data_type = 'string'
        self.options = ''
        self.digits = '3'
        self.param_value = ''
        self.combo_values = []
        self.tooltip = ''
        self.preedit = None
        self.inputKey = ''

    def set_tooltip(self, value):
        self.tooltip = value

    def set_max_value(self, value):
        self.max_value = value

    def set_param_value(self, value):
        self.param_value = value

    def set_min_value(self, value):
        self.min_value = value

    def set_data_type(self, value):
        self.data_type = value

    def set_edit_datatype(self, value):
        self.editdata_type = value

    def set_Input(self, value):
        self.inputKey = value

    def set_fileinfo(self, fname, patrn, mime_type, filter_name):
        self.filename = fname
        self.pattern = patrn
        self.mime_type = mime_type
        self.filter_name = filter_name

    def set_options(self, value):
        self.options = value

    def set_digits(self, value):
        self.digits = value

    def set_preediting(self, value):
        self.preedit = value

    def do_get_property(self, pspec):
        return getattr(self, pspec.name)

    def do_set_property(self, pspec, value):
        setattr(self, pspec.name, value)

    def set_treeview(self, value):
        self.tv = value

    def create_VKB(self, cell_area):
        parent = self.tv.get_toplevel()
        self.vkb = gtk.Dialog("", parent, gtk.DIALOG_MODAL)
        self.vkb.set_decorated(False)
        self.vkb.set_property('skip-taskbar-hint', True)
        self.vkb.set_transient_for(None)

        lbl = gtk.Label(self.tooltip)
        lbl.set_line_wrap(True)
        self.vkb.vbox.pack_start(lbl, expand = False)

        tbl = gtk.Table(rows = 5, columns = 4, homogeneous = True)
        self.vkb.vbox.pack_start(tbl)

        self.result_entry = gtk.Label('')
        self.result_entry.modify_font(pango.FontDescription('sans 16'))
        tbl.attach(self.result_entry, 0, 3, 0, 1,
                   xoptions = gtk.EXPAND | gtk.FILL,
                   yoptions = gtk.EXPAND | gtk.FILL)

        btn = gtk.Button('<-')
        btn.connect("clicked", self.backspace_clicked)
        tbl.attach(btn, 3, 4, 0, 1)

        k = 10
        for i in range(1, 4) :
            k = k - 3
            for j in range(0, 3):
                btn = gtk.Button(str(k + j))
                btn.connect("clicked", self.numbers_clicked)
                tbl.attach(btn, j, j + 1, i, i + 1)

        btn = gtk.Button(label = '0')
        btn.connect("clicked", self.numbers_clicked)

        if self.data_type == 'float' :
            tbl.attach(btn, 0, 1, 4, 5)
            btn = gtk.Button(label = '.')
            btn.connect("clicked", self.dot_clicked)
            tbl.attach(btn, 1, 2, 4, 5)
        else :
            tbl.attach(btn, 0, 2, 4, 5)

        btn = gtk.Button(label = '+/-')
        btn.connect("clicked", self.change_sign_clicked)
        tbl.attach(btn, 2, 3, 4, 5)

        btn = gtk.Button()
        img = gtk.Image()
        img.set_from_stock('gtk-cancel', MENU_ICON_SIZE)
        btn.set_image(img)
        btn.connect("clicked", self.cancel_clicked)
        tbl.attach(btn, 3, 4, 1, 3)

        btn = gtk.Button()
        img = gtk.Image()
        img.set_from_stock('gtk-apply', MENU_ICON_SIZE)
        btn.set_image(img)
        btn.connect("clicked", self.ok_clicked)
        tbl.attach(btn, 3, 4, 3, 5)

        (tree_x, tree_y) = self.tv.get_bin_window().get_origin()
        (tree_w, tree_h) = self.tv.window.get_geometry()[2:4]

        vkb_w = max(tree_w - cell_area.x, 280)
        self.vkb.set_size_request(vkb_w, 260)
        self.vkb.resize(vkb_w, 260)

        x = tree_x + tree_w - vkb_w
        y = tree_y + cell_area.y + cell_area.height
        self.vkb.move(x, y)

        self.vkb.set_keep_above(True)

        self.vkb.connect('key-press-event', self.vkb_key_press_event)
        self.vkb.connect('focus-out-event', self.vkb_focus_out, None)

        if self.inputKey > '' :
            self.vkb_initialize = False
            if self.inputKey == 'BS' :
                self.set_vkb_result('0')
            else :
                self.set_vkb_result(self.inputKey)

            self.inputKey = ''
        else :
            self.vkb_initialize = False
            self.set_vkb_result(self.get_property('text'))
            self.vkb_initialize = True

        self.vkb.show_all()
        btn.grab_focus()

    def ok_clicked(self, btn):
        self.vkb.response(gtk.RESPONSE_OK)

    def cancel_clicked(self, btn):
        self.vkb.response(gtk.RESPONSE_CANCEL)

    def set_vkb_result(self, value):
        if self.vkb_initialize :
            self.result_entry.set_markup('<b>%s</b>' % value[0:10])
            self.vkb_initialize = False
        else :
            old_value = self.result_entry.get_text()
            if (old_value == '0') :
                value = value[1:10]
            if value == '.' :
                value = '0.'
            if (value == '') :
                value = '0'
            self.result_entry.set_markup('<big><b>%s</b></big>' % value[0:10])

    def vkb_key_press_event(self, win, event):
        if event.type == gtk.gdk.KEY_PRESS:
            k_name = gtk.gdk.keyval_name(event.keyval)
            lbl = self.result_entry.get_text()
            if k_name >= 'KP_0' and k_name <= 'KP_9':
                if self.vkb_initialize :
                    self.set_vkb_result(k_name[-1])
                else :
                    self.set_vkb_result(lbl + k_name[-1])
            elif k_name >= '0' and k_name <= '9':
                if self.vkb_initialize :
                    self.set_vkb_result(k_name)
                else :
                    self.set_vkb_result(lbl + k_name)
            elif k_name in ['KP_Enter', 'Enter', 'space']:
                win.response(gtk.RESPONSE_OK)
            elif k_name in ['KP_Decimal', 'period', 'comma'] :
                if (self.editdata_type == 'float') :
                    if self.vkb_initialize :
                        self.set_vkb_result('.')
                    else :
                        if lbl.find('.') == -1 :
                            self.set_vkb_result(lbl + '.')
            elif k_name in ['KP_Subtract', 'KP_Add', 'plus', 'minus'] :
                self.change_sign_clicked(None)
            elif k_name == 'BackSpace' :
                self.backspace_clicked(None)

    def backspace_clicked(self, btn) :
        if self.vkb_initialize :
            self.set_vkb_result('0')
        else :
            self.set_vkb_result(self.result_entry.get_text()[0:-1])

    def numbers_clicked(self, btn):
        if self.vkb_initialize :
            self.set_vkb_result(btn.get_label())
        else :
            self.set_vkb_result(self.result_entry.get_text() + btn.get_label())

    def dot_clicked(self, btn):
        if self.vkb_initialize :
            self.set_vkb_result('.')
        else :
            lbl = self.result_entry.get_text()
            if lbl.find('.') == -1 :
                self.set_vkb_result(lbl + '.')

    def change_sign_clicked(self, btn):
        if self.vkb_initialize :
            self.set_vkb_result('-')
        else :
            lbl = self.result_entry.get_text()
            if lbl.find('-') == -1 :
                if lbl == '0' :
                    self.set_vkb_result('0-')
                else :
                    self.set_vkb_result('-' + lbl)
            else :
                self.set_vkb_result(lbl[1:])

    def vkb_focus_out(self, widget, event, path):
        if vkb_cancel_on_focus_out:
            self.vkb.response(gtk.RESPONSE_CANCEL)
        else :
            self.vkb.response(gtk.RESPONSE_OK)

    def do_get_size(self, widget, cell_area):
        size_tuple = gtk.CellRendererText.do_get_size(self, widget, cell_area)
        return(size_tuple)

    def do_start_editing(self, event, treeview, path, background_area, \
                        cell_area, flags):

        if not self.get_property('editable'):
            self.inputKey = ''
            return None

        if self.preedit is None :
            self.inputKey = ''
            return None
        else :
            self.preedit(self, treeview, path)

        if self.editdata_type in ['header', 'sub-header', 'items'] :
            self.inputKey = ''
            return None

        elif self.editdata_type == 'float' :
            self.create_VKB(cell_area)
            if self.vkb.run() == gtk.RESPONSE_OK:
                val = get_float(self.result_entry.get_text())
                if val > self.max_value :
                    val = self.max_value
                if val < self.min_value :
                    val = self.min_value

                fmt = '{0:0.' + self.digits + 'f}'
                newval = fmt.format(val)
                self.emit('edited', path, newval)
            self.vkb.destroy()
            return None

        elif self.editdata_type == 'int' :
            self.create_VKB(cell_area)
            if self.vkb.run() == gtk.RESPONSE_OK:
                val = get_int(self.result_entry.get_text())
                if val > self.max_value :
                    val = self.max_value
                if val < self.min_value :
                    val = self.min_value
                self.emit('edited', path, str(int(val)))
            self.vkb.destroy()
            return None

        elif self.editdata_type in ['bool', 'boolean']:
            if self.inputKey > '' :
                self.inputKey = ''
                return None
            if get_int(self.param_value) == 0 :
                self.emit('edited', path, '1')
            else :
                self.emit('edited', path, '0')
            return None

        elif self.editdata_type == 'combo':
            if self.inputKey > '' :
                self.inputKey = ''
                return None
            cell = gtk.combo_box_new_text()
            self.combo_values = []
            i = 0
            act = -1
            found = False
            for option in self.options.split(":") :
                opt = option.split('=')
                cell.append_text(opt[0])
                self.combo_values.append(opt[1])
                if (not found) and (opt[1] == self.param_value) :
                    act = i
                    found = True
                i += 1

            cell.set_active(act)
            cell.connect('changed', self.combo_changed, path)
            cell.show()
            cell.grab_focus()
            return cell

        elif self.editdata_type == 'filename':
            if self.inputKey > '' :
                self.inputKey = ''
                return None

            filechooserdialog = gtk.FileChooserDialog(_("Open"), None,
                     gtk.FILE_CHOOSER_ACTION_OPEN,
                     (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL,
                     gtk.STOCK_OK, gtk.RESPONSE_OK))
            try:
                filt = gtk.FileFilter()
                filt.set_name(self.filter_name)

                for option in self.mime_type.split(":") :
                    filt.add_mime_type(option)

                for option in self.pattern.split(":") :
                    filt.add_pattern(option)

                filechooserdialog.add_filter(filt)

                filt = gtk.FileFilter()
                filt.set_name(_("All files"))
                filt.add_pattern("*")
                filechooserdialog.add_filter(filt)

                if os.path.exists(self.filename):
                    filechooserdialog.set_filename(self.filename)
                else :
                    filechooserdialog.set_current_folder(os.getcwd())

                response = filechooserdialog.run()
                if response == gtk.RESPONSE_OK:
                    self.emit('edited', path, filechooserdialog.get_filename())
            finally:
                filechooserdialog.destroy()
            return None

        elif self.editdata_type == 'string':
            self.stringedit_window = gtk.Dialog(parent =
                                                treeview.get_toplevel())
            self.stringedit_window.action_area.hide()
            self.stringedit_window.set_decorated(False)
            self.stringedit_window.set_property('skip-taskbar-hint', True)
            self.stringedit_window.set_transient_for(None)

            self.stringedit_entry = gtk.Entry()
            self.stringedit_entry.set_editable(True)
            self.stringedit_entry.set_has_frame(False)
            self.stringedit_entry.set_property("shadow-type", 0)
            if self.inputKey != 'BS' :
                self.stringedit_entry.set_text(self.get_property('text'))
            self.inputKey = ''

            # map key-press-event handler
            self.stringedit_entry.connect('key-press-event',
                                          self._str_keyhandler)
            self.stringedit_entry.connect('focus-out-event',
                                          self._str_focus_out, path)
            self.stringedit_window.vbox.add(self.stringedit_entry)

            # position the popup below the edited cell
            # and try hard to keep the popup within the toplevel window
            (tree_x, tree_y) = treeview.get_bin_window().get_origin()
            (tree_w, tree_h) = treeview.window.get_geometry()[2:4]
            x = tree_x + cell_area.x
            y = tree_y + cell_area.y
            self.stringedit_window.move(x, y)
            self.stringedit_window.resize(tree_w - cell_area.x,
                                          cell_area.height)
            self.stringedit_window.show_all()

            if self.stringedit_window.run() == gtk.RESPONSE_OK:
                self.emit('edited', path, self.stringedit_entry.get_text())

            self.stringedit_window.destroy()
            return None

        else :
            self.selection = treeview.get_selection()
            self.treestore, self.treeiter = self.selection.get_selected()

            self.textedit_window = gtk.Dialog(parent = treeview.get_toplevel())

            self.textedit_window.action_area.hide()
            self.textedit_window.set_decorated(False)
            self.textedit_window.set_property('skip-taskbar-hint', True)
            self.textedit_window.set_transient_for(None)

            self.textedit = gtk.TextView()
            self.textedit.set_editable(True)
            self.textbuffer = self.textedit.get_buffer()
            self.textedit.set_wrap_mode(gtk.WRAP_WORD)
            self.textbuffer.set_property('text', self.get_property('text'))

            # map key-press-event handler
            self.textedit_window.connect('key-press-event', self._keyhandler)
            self.textedit_window.connect('focus-out-event',
                                         self._focus_out, path)

            scrolled_window = gtk.ScrolledWindow()
            scrolled_window.set_policy(gtk.POLICY_AUTOMATIC,
                                       gtk.POLICY_AUTOMATIC)

            scrolled_window.add(self.textedit)
            self.textedit_window.vbox.add(scrolled_window)
            self.textedit_window.show_all()

            # position the popup below the edited cell
            # and try hard to keep the popup within the toplevel window)
            (tree_x, tree_y) = treeview.get_bin_window().get_origin()
            (tree_w, tree_h) = treeview.window.get_geometry()[2:4]
            (t_w, t_h) = self.textedit_window.window.get_geometry()[2:4]
            x = tree_x + min(cell_area.x, tree_w - t_w +
                             treeview.get_visible_rect().x)
            y = tree_y + min(cell_area.y, tree_h - t_h +
                             treeview.get_visible_rect().y)
            self.textedit_window.move(x, y)
            self.textedit_window.resize(cell_area.width, cell_area.height)

            if self.textedit_window.run() == gtk.RESPONSE_OK:
                (iter_first, iter_last) = self.textbuffer.get_bounds()
                text = self.textbuffer.get_text(iter_first, iter_last)
                self.emit('edited', path, text)

            self.textedit_window.destroy()
            return None

    def do_render(self, window, widget, background_area, cell_area,
                  expose_area, flags):
        if self.data_type in ['bool', 'boolean'] :
            cell_area.width = 30
            chk = gtk.CellRendererToggle()
            chk.set_active(get_int(self.param_value) != 0)
            chk.render(window, widget, background_area, cell_area,
                       expose_area, flags)
        else :
            gtk.CellRendererText.do_render(self, window, widget,
                                           background_area,
                                           cell_area, expose_area, flags)

    def combo_changed(self, combo, path):
        newtext = self.combo_values[combo.get_active()]
        self.emit('edited', path, newtext)

    def _str_focus_out(self, widget, event, path):
        self.stringedit_window.response(gtk.RESPONSE_OK)

    def _str_keyhandler(self, widget, event):
        keyname = gtk.gdk.keyval_name(event.keyval)
        if keyname in ['Return', 'KP_Enter']:
            self.stringedit_window.response(gtk.RESPONSE_OK)

    def _focus_out(self, widget, event, path):
        self.textedit_window.response(gtk.RESPONSE_OK)

    def _keyhandler(self, widget, event):
        keyname = gtk.gdk.keyval_name(event.keyval)
        if event.state & (gtk.gdk.SHIFT_MASK | gtk.gdk.CONTROL_MASK) and \
                gtk.gdk.keyval_name(event.keyval) in ['Return', 'KP_Enter']:
            self.textedit_window.response(gtk.RESPONSE_OK)

gobject.type_register(CellRendererMx)

class Parameter() :
    def __init__(self, ini = None, ini_id = None, xml = None) :
        self.attr = {}
        self.pixbuf = {}
        if ini is not None :
            self.from_ini(ini, ini_id)
        elif xml is not None :
            self.from_xml(xml)

    def __repr__(self) :
        return etree.tostring(self.to_xml(), pretty_print = True)

    def from_ini(self, ini, ini_id) :
        self.attr = {}
        ini = dict(ini)
        for i in ini :
            self.attr[i] = ini[i]
        if "type" in self.attr :
            self.attr["type"] = self.attr["type"].lower()
        if "call" not in self.attr :
            self.attr["call"] = "#" + ini_id.lower()
        self.id = ini_id

    def from_xml(self, xml) :
        for i in xml.keys() :
            self.attr[i] = xml.get(i)

    def to_xml(self) :
        xml = etree.Element("param")
        for i in self.attr :
            xml.set(i, unicode(str(self.attr[i])))
        return xml

    def get_icon(self) :
        icon = self.get_attr("icon")
        if icon is None or icon == "" :
            if self.get_name().lower() in DEFAULT_ICONS:
                icon = DEFAULT_ICONS[self.get_name().lower()]
        return get_pixbuf(icon, TV_ICON_SIZE)
#        return get_pixbuf(self.get_attr("icon"), TV_ICON_SIZE)

    def get_image(self) :
        return get_pixbuf(self.get_attr("image"), ADD_DLG_IMAGE_SIZE)

    def get_value(self) :
        return self.attr["value"] if "value" in self.attr else ""

    def get_name(self) :
        return self.attr["name"] if "name" in self.attr else ""

    def get_options(self):
        return self.attr["options"] if "options" in self.attr else ""

    def get_type(self):
        return self.attr["type"] if "type" in self.attr else "string"

    def get_tooltip(self):
        return self.attr["tool_tip"] if "tool_tip" in self.attr else ""

    def get_attr(self, name) :
        return self.attr[name] if name in self.attr else None

    def get_digits(self):
        return self.attr["digits"] if "digits" in self.attr else DEFAULT_DIGITS

    def get_min_value(self):
        return self.attr["minimum_value"] if "minimum_value" in self.attr \
            else "-99999.9"

    def get_max_value(self):
        return self.attr["maximum_value"] if "maximum_value" in self.attr \
            else "99999.9"

class Feature():
    def __init__(self, src = None, xml = None) :
        self.attr = {}
        self.pixbuf = {}
        self.param = []
        if src is not None :
            self.from_src(src)
        if xml is not None :
            self.from_xml(xml)

    def __repr__(self) :
        return etree.tostring(self.to_xml(), pretty_print = True)

    def get_icon(self) :
        return get_pixbuf(self.get_attr("icon"), TV_ICON_SIZE)

    def get_image(self) :
        return get_pixbuf(self.get_attr("image"), ADD_DLG_IMAGE_SIZE)

    def get_value(self):
        return self.attr["value"] if "value" in self.attr else ""

    def get_type(self):
        return self.attr["type"] if "type" in self.attr else "string"

    def get_tooltip(self):
        return self.attr["tool_tip"] if "tool_tip" in self.attr else ""

    def get_attr(self, attr) :
        return self.attr[attr] if attr in self.attr else None

    def get_name(self):
        return self.attr["name"] if "name" in self.attr else ""

    def from_src(self, src) :
        src_config = ConfigParser.ConfigParser()
        f = open(src).read()

        # add "." in the begining of multiline parameters to save indents
        f = re.sub(r"(?m)^(\ |\t)", r"\1.", f)
        src_config.readfp(io.BytesIO(f))
        # remove "." in the begining of multiline parameters to save indents
        conf = {}
        for section in src_config.sections() :
            conf[section] = {}
            for item in src_config.options(section) :
                s = src_config.get(section, item, raw = True)
                s = re.sub(r"(?m)^\.", "", " " + s)[1:]
                conf[section][item] = s
        self.attr = conf["SUBROUTINE"]

        self.attr["src"] = src

        self.attr["name"] = _(self.get_name())
        # original name
        self.attr["o-name"] = self.attr["name"]

        # get order
        if "order" not in self.attr :
            self.attr["order"] = []
        else :
            self.attr["order"] = self.attr["order"].upper().split()
        self.attr["order"] = [s if s[:6] == "PARAM_" else "PARAM_" + s \
                              for s in self.attr["order"]]

        # get params
        self.param = []
        parameters = self.attr["order"] + [p for p in conf if \
                    (p[:6] == "PARAM_" and p not in self.attr["order"])]
        for s in parameters :
            if s in conf :
                p = Parameter(ini = conf[s], ini_id = s.lower())
                p.attr['name'] = _(p.get_name())
                p.attr['tool_tip'] = _(p.get_attr('tool_tip') \
                            if "tool_tip" in p.attr else p.get_attr('name'))
                opt = p.attr["options"] if "options" in self.attr else None
                if opt and (opt != '') :
                    p.attr['options'] = _(opt)
                if p.get_type() not in SUPPORTED_DATA_TYPES :
                    p.attr['type'] = 'string'
                if p.get_type() in ['int', 'float'] and DEFAULT_METRIC :
                    val = p.get_attr('metric_value')
                    if val is not None:
                        p.attr['value'] = val
                if p.get_type() == 'float' :
                    fmt = '{0:0.' + p.get_digits() + 'f}'
                    p.attr['value'] = fmt.format(get_float(p.get_value()))
                elif p.get_type() == 'int' :
                    p.attr['value'] = str(get_int(p.get_value()))
                self.param.append(p)

        self.attr["id"] = self.attr["type"] + '-000'

        # get gcode parameters
        for l in ["definitions", "before", "call", "after"] :
            l = l.upper()
            if l in conf and "content" in conf[l] :
                self.attr[l.lower()] = re.sub(r"(?m)\r?\n\r?\.", "\n",
                                              conf[l]["content"])
            else :
                self.attr[l.lower()] = ""

    def from_xml(self, xml) :
        self.attr = {}
        for i in xml.keys() :
            self.attr[i] = xml.get(i)

        self.param = []
        for p in xml :
            self.param.append(Parameter(xml = p))

    def to_xml(self) :
        xml = etree.Element("feature")
        for i in self.attr :
            xml.set(i, unicode(str(self.attr[i])))

        for p in self.param :
            xml.append(p.to_xml())
        return xml

    def get_id(self, xml) :
        num = 1
        if xml is not None :
            # get smallest free name
            l = xml.findall(".//feature[@type='%s']" % self.attr["type"])
            num = max([get_int(i.get("id")[-3:]) for i in l ] + [0]) + 1

        if not "o-name" in self.attr :
            self.attr["o-name"] = self.attr["name"]

        self.attr["name"] = self.attr["o-name"]  # + " %03d" % num
        self.attr["id"] = self.attr["type"] + "-%03d" % num

    def get_definitions(self) :
        global DEFINITIONS
        if self.attr["type"] not in DEFINITIONS :
            s = self.process(self.attr["definitions"])
            if s != "" :
                DEFINITIONS.append(self.attr["type"])
            return s + "\n"
        else :
            return ""

    def include(self, srce) :
        src = search_path(srce)
        if src :
            f = open(src)
            s = f.read()
            f.close()
            return s
        else :
            mess_dlg(_("File not found : %s") % srce)
            raise IOError("IOError: File not found : %s" % srce)

    def include_once(self, src) :
        global INCLUDE
        if (src) not in INCLUDE :
            INCLUDE.append(src)
            return self.include(src)
        return ""

    def get_param_value(self, call) :
        call = "#" + call.lower()
        for p in self.param :
            if "call" in p.attr and "value" in p.attr :
                if call == p.attr["call"] :
                    return p.attr['value']
        return None

    def process(self, s) :
        global APP_PATH

        def eval_callback(m) :
            return str(eval(m.group(2), globals(), {"self":self}))

        def exec_callback(m) :
            s = m.group(2)

            # strip starting spaces
            s = s.replace("\t", "  ")
            i = 1e10
            for l in s.split("\n") :
                if l.strip() != "" :
                    i = min(i, len(l) - len(l.lstrip()))
            if i < 1e10 :
                res = ""
                for l in s.split("\n") :
                    res += l[i:] + "\n"
                s = res

            old_stdout = sys.stdout
            redirected_output = StringIO()
            sys.stdout = redirected_output
            exec(s) in {"self":self}
            sys.stdout = old_stdout
            redirected_output.reset()
            out = str(redirected_output.read())
            return out

        def subprocess_callback(m) :
            s = m.group(2)

            # strip starting spaces
            s = s.replace("\t", "  ")
            i = 1e10
            for l in s.split("\n") :
                if l.strip() != "" :
                    i = min(i, len(l) - len(l.lstrip()))
            if i < 1e10 :
                res = ""
                for l in s.split("\n") :
                    res += l[i:] + "\n"
                s = res
            try :
                return subprocess.check_output([s], shell = True)
            except subprocess.CalledProcessError as e:
                mess_dlg('Error with subprocess: %s' % e)
                return ''

        def import_callback(m) :
            fname = m.group(2)
            if fname is not None :
                try :
                    return str(open(fname).read())
                except :
                    StatusBar.push(SB_File_context_id,
                                   _("File not found : %s") % fname)
            else :
                mess_dlg(_("File not found : %(f)s in %(p)s" %
                           {"f":fname, "p":(XML_DIR)}))
                raise IOError("IOError File not found : %(f)s in %(p)s" %
                              {"f":fname, "p":(XML_DIR)})

        s = re.sub(r"%APP_PATH%", "%s" % APP_PATH, s)

        s = re.sub(r"(?i)(<import>(.*?)</import>)", import_callback, s)
        s = re.sub(r"(?i)(<eval>(.*?)</eval>)", eval_callback, s)
        s = re.sub(r"(?ims)(<exec>(.*?)</exec>)", exec_callback, s)

        for p in self.param :
            if "call" in p.attr and "value" in p.attr :
                s = re.sub(r"%s([^A-Za-z0-9_]|$)" %
                           (re.escape(p.attr["call"])), r"%s\1" %
                           (p.attr["value"]), s)

        s = re.sub(r"%APP_PATH%", "%s" % APP_PATH, s)
        s = re.sub(r"(?ims)(<subprocess>(.*?)</subprocess>)",
                   subprocess_callback, s)
        s = re.sub(r"#self_id", "%s" % self.get_attr("id"), s)
        return s

    def get_unique_id(self) :
        global UNIQUE_ID
        UNIQUE_ID = UNIQUE_ID + 1
        return UNIQUE_ID

class Features(gtk.VBox):
    __gtype_name__ = "Features"
    __gproperties__ = {}
    __gproperties = __gproperties__

    def __init__(self, *a, **kw):
        global APP_PATH, DEFAULT_CATALOG, DEFAULT_METRIC

        # process passed args
        opt, optl = 'U:c:x:i:t', ["catalog=", "ini="]
        optlist, args = getopt.getopt(sys.argv[1:], opt, optl)
        optlist = dict(optlist)

        if "-t" in optlist :
            # get translations and exit
            self.get_translations()
            sys.exit()

        if "-U" in optlist :
            optlist_, args = getopt.getopt(optlist["-U"].split(), opt, optl)
            optlist.update(optlist_)

        ini = os.getenv("INI_FILE_NAME")
        if "-i" in optlist :
            ini = optlist["-i"]
        elif "--ini" in optlist :
            ini = optlist["--ini"]

        self.display = DEFAULT_DISPLAY

        if (ini is not None):
            APP_PATH = '/usr/local/bin/features'
            if not os.path.exists(APP_PATH):
                mess_dlg('Execute ./setup first')
                sys.exit(1)

            try :
                inifile = linuxcnc.ini(ini)
                self.display = inifile.find('DISPLAY', 'DISPLAY')

                try :
                    val = inifile.find('DISPLAY', 'PROGRAM_PREFIX')
                except :
                    print(NO_PROGRAM_PREFIX_MSG)
                    mess_dlg(NO_PROGRAM_PREFIX_MSG)
                    sys.exit(1)

                try :
                    inifile.find('RS274NGC', 'SUBROUTINE_PATH')
                except :
                    print (NO_SUBROUTINE_PATH_MSG)
                    mess_dlg (NO_SUBROUTINE_PATH_MSG)
                    sys.exit(1)

                try :
                    val = inifile.find('TRAJ', 'LINEAR_UNITS')
                    DEFAULT_METRIC = val in ['mm', 'metric']
                except :
                    print (DEFAULT_UNITS_NOT_DETECTED)
                    mess_dlg (DEFAULT_UNITS_NOT_DETECTED)

            except:
                mess_dlg(_('Can not read LinuxCNC ini file'))

        else :
            APP_PATH = os.path.abspath(os.path.dirname(__file__))

        if "--catalog" in optlist :
            self.catalog_dir = APP_PATH + CATALOGS_DIR + optlist["--catalog"]
        else :
            self.catalog_dir = APP_PATH + CATALOGS_DIR + DEFAULT_CATALOG

        if not os.path.exists(self.catalog_dir + '/menu.xml') :
            mess_dlg(_('Catalog file does not exists : %s') %
                     self.catalog_dir + '/menu.xml')
            IOError('IOError Catalog file does not exists : %s' %
                    self.catalog_dir + '/menu.xml')

        xml = etree.parse(self.catalog_dir + '/menu.xml')
        self.catalog = xml.getroot()

        self.LinuxCNC_connected = False
        self.treestore_selected = None
        self.selected_feature = None
        self.selected_feature_path = None
        self.selected_type = None
        self.iter_selected_type = SEL_IS_NONE
        self.use_widget_box = DEFAULT_USE_WIDGETS
        self.treeview2 = None

        self.embedded = True
        self.undo_list = []
        self.undo_pointer = -1
        self.timeout = None

        # main_window
        gtk.VBox.__init__(self, *a, **kw)

        self.builder = gtk.Builder()
        try :
            self.builder.add_from_file(APP_PATH + "/features.glade")
        except :
            mess_dlg(_("File not found : features.glade"))
            raise IOError("File not found : features.glade")

        self.get_widgets()

        self.read_preferences()
        self.on_scale_change_value(self)

        global SB_Pix_context_id, SB_File_context_id, SB_Params_context_id
        SB_Pix_context_id = StatusBar.get_context_id('Pix')
        SB_File_context_id = StatusBar.get_context_id('File')
        SB_Params_context_id = StatusBar.get_context_id('Params')

        self.treestore = gtk.TreeStore(object, str, gobject.TYPE_BOOLEAN,
                                       gobject.TYPE_BOOLEAN)
        self.master_filter = self.treestore.filter_new()
        self.widgets_viewport = None

        self.details_filter = self.treestore.filter_new()
        self.details_filter.set_visible_column(3)

        self.create_treeview()
        self.main_box.reparent(self)

        if use_top_features_toolbar :
            self.topfeatures_tb = gtk.Toolbar()
            self.main_box.pack_start(self.topfeatures_tb, False, False, 0)
            self.topfeatures_tb.set_icon_size(MENU_ICON_SIZE)
            self.main_box.reorder_child(self.topfeatures_tb, 1)

        self.add_toolbar.set_icon_size(MENU_ICON_SIZE)

        self.default_src = None
        self.post_amble_src = None
        self.create_menu_interface()

        if self.default_src is None :
            self.default_src = self.catalog_dir + DEFAULTS
        try :
            self.defaults = open(self.default_src).read()
        except :
            StatusBar.push(3, _('Can not load NGC default file : %s') %
                           self.default_src)
            mess_dlg(_('Can not load NGC default file : %s') %
                     self.default_src)

        if self.post_amble_src is None :
            self.post_amble_src = self.catalog_dir + POSTAMBLE
        try :
            self.post_amble = open(self.post_amble_src).read()
        except :
            StatusBar.push(3, _('Can not load post amble file : %s') %
                           self.post_amble_src)
            self.post_amble = ''

        if use_top_features_toolbar :
            self.setup_top_features()

        self.create_add_dialog()
        self.actionUseWidgets.set_active(self.use_widget_box)

        self.builder.connect_signals(self)
        self.actionSingleView.set_active(not self.use_dual_views)
        self.actionDualView.set_active(self.use_dual_views)
        self.actionTopBottom.set_active(not self.side_by_side)
        self.actionSideBySide.set_active(self.side_by_side)
        self.actionSubHdrs.set_active(self.sub_hdrs_in_tv1)

        self.menu_new_activate(self)
        self.get_selected_feature(self)
        self.show_all()
        self.addVBox.hide()
        self.feature_pane.set_size_request(int(self.tv_w_adj.value), 100)

        self.set_layout()
        self.treeview.grab_focus()
        self.clipboard = gtk.clipboard_get(gtk.gdk.SELECTION_CLIPBOARD)
        self.update_timeout = gobject.timeout_add(8000, self.update_check_call)

    def update_check_call(self):
        opener = urllib.FancyURLopener({})
        try :
            f = opener.open("https://raw.github.com/FernV/linuxcnc-features/master/version").read()
            if f.find('Not Found') > -1 :
                return
            f = re.sub(r"\n", "", f)
            if (f > APP_VERSION) and (f > self.checked_update) :
                mess_dlg('New update available : %s\nCurrent version : %s' % (f, APP_VERSION))
                webbrowser.open(HOME_PAGE)
                self.checked_update = f
                self.save_preferences()
        except:
            mess_dlg(_('Cannot access update information'))

    def create_mi(self, _action):
        mi = _action.create_menu_item()
        mi.set_image(_action.create_icon(MENU_ICON_SIZE))
        return mi

    def create_menu_interface(self):
        menu_bar = gtk.MenuBar()

        file_menu = gtk.Menu()
        f_menu = gtk.MenuItem(_("_File"))
        f_menu.connect("activate", self.menu_file_activate)
        f_menu.set_submenu(file_menu)
        menu_bar.append(f_menu)

        file_menu.append(self.create_mi(self.actionNew))
        file_menu.append(self.create_mi(self.actionOpen))
        file_menu.append(self.create_mi(self.actionSave))
        file_menu.append(self.create_mi(self.actionSaveTemplate))
        file_menu.append(gtk.SeparatorMenuItem())
        file_menu.append(self.create_mi(self.actionSaveNGC))


        ed_menu = gtk.Menu()
        edit_menu = gtk.MenuItem(_("_Edit"))
        edit_menu.connect("activate", self.menu_edit_activate)
        edit_menu.set_submenu(ed_menu)
        menu_bar.append(edit_menu)

        ed_menu.append(self.create_mi(self.actionUndo))
        ed_menu.append(self.create_mi(self.actionRedo))
        ed_menu.append(gtk.SeparatorMenuItem())

        ed_menu.append(self.create_mi(self.actionCut))
        ed_menu.append(self.create_mi(self.actionCopy))
        ed_menu.append(self.create_mi(self.actionPaste))
        ed_menu.append(gtk.SeparatorMenuItem())

        ed_menu.append(self.create_mi(self.actionAdd))
        ed_menu.append(self.create_mi(self.actionDuplicate))
        ed_menu.append(self.create_mi(self.actionDelete))
        ed_menu.append(gtk.SeparatorMenuItem())

        ed_menu.append(self.create_mi(self.actionMoveUp))
        ed_menu.append(self.create_mi(self.actionMoveDn))
        ed_menu.append(gtk.SeparatorMenuItem())

        ed_menu.append(self.create_mi(self.actionAppendItem))
        ed_menu.append(self.create_mi(self.actionRemoveItem))
        ed_menu.append(gtk.SeparatorMenuItem())

        ed_file_menu = gtk.MenuItem(_('Edit Files'))
        ed_file_menu.connect("activate", self.menu_edit_file_activate)
        ed_menu.append(ed_file_menu)
        sub_menu = gtk.Menu()
        ed_file_menu.set_submenu(sub_menu)

        sub_menu.append(self.create_mi(self.actionEditFeature))
        sub_menu.append(self.create_mi(self.actionReloadFeature))
        sub_menu.append(gtk.SeparatorMenuItem())

        sub_menu.append(self.create_mi(self.actionEditDefaults))
        sub_menu.append(self.create_mi(self.actionReloadDefaults))
        sub_menu.append(gtk.SeparatorMenuItem())

        sub_menu.append(self.create_mi(self.actionEditPostAmble))
        sub_menu.append(self.create_mi(self.actionReloadPostAmble))


        v_menu = gtk.Menu()
        view_menu = gtk.MenuItem(_("_View"))
        view_menu.set_submenu(v_menu)
        menu_bar.append(view_menu)

        v_menu.append(self.create_mi(self.actionCollapse))
        v_menu.append(gtk.SeparatorMenuItem())

        mi = self.actionSingleView.create_menu_item()
        mi.set_size_request(-1, ADD_MENU_ICON_SIZE)
        v_menu.append(mi)

        mi = self.actionDualView.create_menu_item()
        mi.set_size_request(-1, ADD_MENU_ICON_SIZE)
        v_menu.append(mi)

        v_menu.append(gtk.SeparatorMenuItem())

        mi = self.actionTopBottom.create_menu_item()
        mi.set_size_request(-1, ADD_MENU_ICON_SIZE)
        v_menu.append(mi)

        mi = self.actionSideBySide.create_menu_item()
        mi.set_size_request(-1, ADD_MENU_ICON_SIZE)
        v_menu.append(mi)

        v_menu.append(gtk.SeparatorMenuItem())

        if DEFAULT_USE_WIDGETS :
            mi = self.actionUseWidgets.create_menu_item()
            mi.set_size_request(-1, ADD_MENU_ICON_SIZE)
            v_menu.append(mi)

        mi = self.actionHideValCol.create_menu_item()
        mi.set_size_request(-1, ADD_MENU_ICON_SIZE)
        v_menu.append(mi)

        mi = self.actionSubHdrs.create_menu_item()
        mi.set_size_request(-1, ADD_MENU_ICON_SIZE)
        v_menu.append(mi)

        v_menu.append(gtk.SeparatorMenuItem())

        mi = self.actionSaveLayout.create_menu_item()
        mi.set_size_request(-1, ADD_MENU_ICON_SIZE)
        v_menu.append(mi)

        self.menuAdd = gtk.Menu()
        add_menu = gtk.MenuItem(_('_Add'))
        add_menu.set_submenu(self.menuAdd)
        menu_bar.append(add_menu)
        self.add_catalog_items()


        menu_utils = gtk.Menu()
        u_menu = gtk.MenuItem(_('_Utilities'))
        u_menu.set_submenu(menu_utils)

        self.auto_refresh = gtk.CheckMenuItem(_('_Auto-Refresh'))
        self.auto_refresh.set_size_request(-1, ADD_MENU_ICON_SIZE)
        menu_utils.append(self.auto_refresh)
        menu_utils.append(gtk.SeparatorMenuItem())

        menu_config = gtk.ImageMenuItem(_('_Preferences'))
        img = gtk.Image()
        img.set_from_stock('gtk-preferences', MENU_ICON_SIZE)
        menu_config.set_image(img)
        menu_config.connect("activate", self.menu_pref_activate)
        menu_utils.append(menu_config)

        menu_bar.append(u_menu)

        menu_help = gtk.Menu()
        h_menu = gtk.MenuItem(_('_Help'))
        h_menu.connect("activate", self.menu_help_activate)
        h_menu.set_submenu(menu_help)

        self.menu_context_help = gtk.ImageMenuItem(_('_Context Help'))
        img = gtk.Image()
        img.set_from_stock('gtk-help', MENU_ICON_SIZE)
        self.menu_context_help.set_image(img)
        self.menu_context_help.connect("activate", self.menu_context_activate)
        menu_help.append(self.menu_context_help)

        menu_linuxcnc_home = gtk.ImageMenuItem(_('LinuxCNC Home'))
        img = gtk.Image()
        img.set_from_stock('gtk-help', MENU_ICON_SIZE)
        menu_linuxcnc_home.set_image(img)
        menu_linuxcnc_home.connect("activate", self.menu_html_cnc_activate)
        menu_help.append(menu_linuxcnc_home)

        menu_linuxcnc_forum = gtk.ImageMenuItem(_('LinuxCNC Forum'))
        img = gtk.Image()
        img.set_from_stock('gtk-help', MENU_ICON_SIZE)
        menu_linuxcnc_forum.set_image(img)
        menu_linuxcnc_forum.connect("activate", self.menu_html_forum_activate)
        menu_help.append(menu_linuxcnc_forum)

        menu_cnc_russia = gtk.ImageMenuItem(_('CNC-Club Russia'))
        img = gtk.Image()
        img.set_from_stock('gtk-help', MENU_ICON_SIZE)
        menu_cnc_russia.set_image(img)
        menu_cnc_russia.connect("activate", self.menu_html_ru_activate)
        menu_help.append(menu_cnc_russia)
        menu_help.append(gtk.SeparatorMenuItem())

        menu_features_update = gtk.ImageMenuItem(_('Check for update'))
        img = gtk.Image()
        img.set_from_stock('gtk-help', MENU_ICON_SIZE)
        menu_features_update.set_image(img)
        menu_features_update.connect("activate", self.menu_features_update)
        menu_help.append(menu_features_update)
        menu_help.append(gtk.SeparatorMenuItem())

        menu_about = gtk.ImageMenuItem(_('_About'))
        img = gtk.Image()
        img.set_from_stock('gtk-about', MENU_ICON_SIZE)
        menu_about.set_image(img)
        menu_about.connect("activate", self.menu_about_activate)
        menu_help.append(menu_about)

        menu_bar.append(h_menu)

        self.main_box.pack_start(menu_bar, False, False, 0)
        self.main_box.reorder_child(menu_bar, 0)

        self.menubar = menu_bar

    def menu_features_update(self, *args):
        opener = urllib.FancyURLopener({})
        try :
            f = opener.open("https://raw.github.com/FernV/linuxcnc-features/master/version").read()
            if f.find('Not Found') > -1 :
                mess_dlg(_('No new version information'))
                return
            f = re.sub(r"\n", "", f)
            if f == APP_VERSION :
                mess_dlg(_('You have the latest version : %s') % f)
                return
            if (f > APP_VERSION) :
                mess_dlg('New update available : %s\nCurrent version : %s' % (f, APP_VERSION))
                webbrowser.open(HOME_PAGE)
                self.checked_update = f
                self.save_preferences()
        except:
            mess_dlg(_('Cannot access update information'))

    def edit_cut(self, *args):
        self.edit_copy()
        self.delete_clicked()

    def edit_copy(self, *args):
        self.get_expand()
        xml = etree.Element(XML_TAG)
        self.treestore_to_xml_recursion(self.selected_feature_ts_itr, xml, False)
        self.clipboard.set_text(etree.tostring(xml), len = -1)

    def edit_paste(self, *args):
        txt = self.clipboard.wait_for_text()
        if txt:
            valid_xml = txt.find(XML_TAG)
            if valid_xml > -1 :
                xml = etree.fromstring(txt)
                self.import_xml(xml)

    def menu_help_activate(self, *args):
        self.menu_context_help.set_sensitive((self.selected_feature is not None) and \
                (self.selected_feature.get_attr('html_help') is not None))

    def menu_edit_file_activate(self, *args):
        self.actionEditFeature.set_sensitive(self.selected_feature is not None)
        self.actionReloadFeature.set_sensitive(self.selected_feature is not None)

    def menu_edit_activate(self, *args):
        txt = self.clipboard.wait_for_text()
        if txt:
            valid_xml = txt.find(XML_TAG)
            self.actionPaste.set_sensitive(valid_xml > -1)
        else:
            self.actionPaste.set_sensitive(False)

    def menu_html_cnc_activate(self, *args):
        webbrowser.open('http://www.linuxcnc.org')

    def menu_html_forum_activate(self, *args):
        webbrowser.open('http://www.linuxcnc.org/index.php/english/forum/40-subroutines-and-ngcgui')

    def menu_html_ru_activate(self, *args):
        webbrowser.open('www.cnc-club.ru')

    def edit_defaults(self, *args):
        if not os.path.isfile(self.default_src):
            open(self.default_src, 'w').close()
        webbrowser.open(self.default_src)

    def edit_postamble(self, *args):
        if not os.path.isfile(self.post_amble_src):
            open(self.post_amble_src, 'w').close()
        webbrowser.open(self.post_amble_src)

    def edit_feature(self, *args):
        webbrowser.open(self.selected_feature.get_attr('src'))

    def menu_context_activate(self, *args):
        webbrowser.open(self.selected_feature.get_attr('html_help'), new = 2)

    def btn_cancel_add_clicked(self, *args):
        self.addVBox.hide()
        self.feature_Hpane.show()
        self.menubar.set_sensitive(True)
        self.button_tb.set_sensitive(True)
        if use_top_features_toolbar :
            self.topfeatures_tb.set_sensitive(True)

    def create_add_dialog(self):
        self.icon_store = gtk.ListStore(gtk.gdk.Pixbuf, str, str, str, int, str)
        self.add_iconview.set_model(self.icon_store)
        self.add_iconview.set_pixbuf_column(0)
        self.add_iconview.set_text_column(2)
        self.add_iconview.connect("selection-changed", self.catalog_activate)

        self.catalog_path = self.catalog
        self.updating_catalog = True
        self.update_catalog(xml = self.catalog_path)
        self.updating_catalog = False

    def catalog_activate(self, iconview):
        if not self.updating_catalog :
            lst = iconview.get_selected_items()
            itr = self.icon_store.get_iter(lst[0])
            src = self.icon_store.get(itr, 3)[0]
            tag = self.icon_store.get(itr, 1)[0]
            self.updating_catalog = True
            if tag == "parent" :
                self.update_catalog(xml = "parent")
            elif tag in ["sub", "import"] :
                self.addVBox.hide()
                self.feature_Hpane.show()
                self.menubar.set_sensitive(True)
                self.button_tb.set_sensitive(True)
                if use_top_features_toolbar :
                    self.topfeatures_tb.set_sensitive(True)
                self.add_feature(src)
            elif tag == "group" :
                path = self.icon_store.get(itr, 4)[0]
                self.update_catalog(xml = self.catalog_path[path])
            self.updating_catalog = False

    def update_catalog(self, call = None, xml = None) :
        if xml == "parent" :
            self.catalog_path = self.catalog_path.getparent()
        else :
            self.catalog_path = xml
        if self.catalog_path is None :
            self.catalog_path = self.catalog
        self.icon_store.clear()

        # add link to upper level
        if self.catalog_path != self.catalog :
            self.icon_store.append([get_pixbuf("upper-level.png",
                ADD_DLG_IMAGE_SIZE), "parent", _('Back...'), "parent", 0, None])

        for path in range(len(self.catalog_path)) :
            p = self.catalog_path[path]
            if p.tag.lower() in ["group", "sub", "import"] :
                name = p.get("name")  # ) if "name" in p.keys() else None
                src = p.get("src") if "src" in p.keys() else None
                tooltip = p.get("tool_tip") if "tool_tip" in p.keys() else None
                self.icon_store.append([get_pixbuf(p.get("icon"),
                    ADD_DLG_IMAGE_SIZE), p.tag.lower(), name, src, path, tooltip])

    def add_button_clicked(self, *arg) :
        self.feature_Hpane.hide()
        self.addVBox.show()
        self.menubar.set_sensitive(False)
        self.button_tb.set_sensitive(False)
        if use_top_features_toolbar :
            self.topfeatures_tb.set_sensitive(False)

    def topfeatures_click(self, call, src) :
        self.add_feature(src)

    def setup_top_features(self) :
        self.config_top = ConfigParser.ConfigParser()
        try :
            self.config_top.read(self.catalog_dir + TOP_FEATURES)
            topfeatures = self.config_top.get("VAR", "topfeatures", raw = True)
        except :
            topfeatures = ""

        topfeatures = topfeatures.split("\n")
        self.topfeatures_dict = {}
        for s in topfeatures :
            s = s.split("\t")
            if len(s) == 3 :
                self.topfeatures_dict[s[0]] = [int(s[1]), 0]

        feature_list = [s.get("src") for s in self.catalog.findall(".//sub") if "src" in s.keys()]
        self.topfeatures = {}
        self.topfeatures_buttons = {}
        self.topfeatures_topbuttons = {}

        for src in feature_list :
            try :
                src_file = search_path(src)
                if not src_file :
                    continue
                f = Feature(src_file)
                icon = gtk.Image()
                icon.set_from_pixbuf(get_pixbuf(f.get_attr("icon"),
                                                TOP_FEATURES_ICON_SIZE))
                button = gtk.ToolButton(icon, label = f.get_attr("name"))
                button.set_tooltip_markup(f.get_attr("help"))
                button.connect("clicked", self.topfeatures_click, src_file)
                self.topfeatures_buttons[src_file] = button

                icon = gtk.Image()
                icon.set_from_pixbuf(get_pixbuf(f.get_attr("icon"),
                                                TOP_FEATURES_ICON_SIZE))
                button1 = gtk.ToolButton(icon, label = f.get_attr("name"))
                button1.set_tooltip_markup(f.get_attr("help"))
                button1.connect("clicked", self.topfeatures_click, src_file)
                self.topfeatures_topbuttons[src_file] = button1

                self.topfeatures[src_file] = [button, button1, 0, 0]
                if src_file in self.topfeatures_dict :
                    self.topfeatures[src_file][2:] = self.topfeatures_dict[src_file]
            except :
                pass

        self.top_features_ini = {
            "top-features": 3,
            "last-features": 10
        }
        self.top_features_ini["top-features"] = SUPER_TOP_FEATURES_COUNT
        self.top_features_ini["last-features"] = MAX_TOP_FEATURES - SUPER_TOP_FEATURES_COUNT
        self.topfeatures_update()

    def topfeatures_update(self, src = None):
        if src in self.topfeatures :
            self.topfeatures[src][2] += 1
            self.topfeatures[src][3] += 1

        # clear toolbar
        while self.topfeatures_tb.get_n_items() > 0 :
            self.topfeatures_tb.remove(self.topfeatures_tb.get_nth_item(0))

        tf = self.topfeatures.items()

        tf.sort(lambda x, y:-1 if x[1][2] - y[1][2] > 0 else 1)  # sort by i
        for tfi in tf[:self.top_features_ini["top-features"]] :
            self.topfeatures_tb.insert(tfi[1][0], -1)

        self.topfeatures_tb.insert(gtk.SeparatorToolItem(), -1)

        # remove appended buttons so they do not appear twice
        tf = tf[SUPER_TOP_FEATURES_COUNT: ]

        tf.sort(lambda x, y:-1 if x[1][3] - y[1][3] > 0 else 1)  # sort by t
        for tfi in tf[:self.top_features_ini["last-features"]] :
            self.topfeatures_tb.insert(tfi[1][1], -1)

        self.topfeatures_tb.show_all()

        if src :
            # save config
            if "INFO" not in self.config_top.sections() :
                self.config_top.add_section('INFO')
                self.config_top.set('INFO', 'warning',
                    'This file is rewritten every time a feature is added\n'
                    'and shows usage of each one\n'
                    'columns are : source, total usage, last session usage')

            if "VAR" not in self.config_top.sections() :
                self.config_top.add_section('VAR')

            for src in self.topfeatures :
                self.topfeatures_dict[src] = self.topfeatures[src][2:]
            topfeatures = ""
            for src in self.topfeatures_dict :
                topfeatures += "\n%s\t%s\t%s" % (src,
                        self.topfeatures_dict[src][0],
                        self.topfeatures_dict[src][1])

            self.config_top.set("VAR", "topfeatures", topfeatures)
            fname = self.catalog_dir + TOP_FEATURES
            try :
                self.config_top.write(open(fname, "w"))
            except :
                mess_dlg(_("Warning cannot write to topfeatures file %s") % fname)


    def add_catalog_items(self):

        def add_to_menu(grp_menu, path) :
            for ptr in range(len(path)) :
                p = path[ptr]
                if p.tag.lower() == "group":
                    name = p.get("name") if "name" in p.keys() else None
                    a_menu_item = gtk.ImageMenuItem(_(name))

                    tooltip = p.get("tool_tip") if "tool_tip" in p.keys() else None
                    if (tooltip is not None) and (tooltip != ''):
                        a_menu_item.set_tooltip_text(_(tooltip))

                    img = gtk.Image()
                    img.set_from_pixbuf(get_pixbuf(p.get("icon"), ADD_MENU_ICON_SIZE))
                    a_menu_item.set_image(img)

                    grp_menu.append(a_menu_item)
                    a_menu = gtk.Menu()
                    a_menu_item.set_submenu(a_menu)
                    add_to_menu(a_menu, p)

                elif p.tag.lower() == "separator":
                    sep = gtk.SeparatorMenuItem()
                    grp_menu.append(sep)

                elif p.tag.lower() in ["sub", "import"] :
                    name = p.get("name") if "name" in p.keys() else None
                    a_menu_item = gtk.ImageMenuItem(_(name))

                    tooltip = p.get("tool_tip") if "tool_tip" in p.keys() else None
                    if (tooltip is not None) and (tooltip != ''):
                        a_menu_item.set_tooltip_text(_(tooltip))

                    img = gtk.Image()
                    img.set_from_pixbuf(get_pixbuf(p.get("icon"), ADD_MENU_ICON_SIZE))
                    a_menu_item.set_image(img)

                    src = p.get("src") if "src" in p.keys() else None
                    if (src is not None) and (src != ''):
                        a_menu_item.connect("activate", self.add_feature_menu, src)

                    grp_menu.append(a_menu_item)

                elif p.tag.lower() == "defaults" :
                    self.default_src = p.get("src") if "src" in p.keys() else None

                elif p.tag.lower() == "postamble" :
                    self.post_amble_src = p.get("src") if "src" in p.keys() else None

        try :
            add_to_menu(self.menuAdd, self.catalog)
        except :
            mess_dlg(_('Problem adding catalog to menu'))

        sep = gtk.SeparatorMenuItem()
        self.menuAdd.append(sep)

        menu_importXML = gtk.ImageMenuItem(_('Import _XML'))
        img = gtk.Image()
        img.set_from_stock('gtk-revert-to-saved', MENU_ICON_SIZE)
        menu_importXML.set_image(img)
        menu_importXML.connect("activate", self.menu_import_activate)
        self.menuAdd.append(menu_importXML)

        menu_item = gtk.ImageMenuItem(_('_Open'))
        img = gtk.Image()
        img.set_from_stock('gtk-open', MENU_ICON_SIZE)
        menu_item.set_image(img)
        menu_item.connect("activate", self.menu_open_ini_activate)
        menu_item.set_tooltip_text(_("Select a valid or prototype ini"))
        self.menuAdd.append(menu_item)

    def create_treeview(self):
        self.treeview = gtk.TreeView(self.treestore)
        self.treeview.set_grid_lines(gtk.TREE_VIEW_GRID_LINES_VERTICAL)
        self.treeview.set_size_request(int(self.col_width_adj.value) + 50, -1)

        self.builder.get_object("feat_scrolledwindow").add(self.treeview)
# TODO: ??? implemnt DnD ???
#        self.treeview.enable_model_drag_source(gtk.gdk.BUTTON1_MASK, TARGETS,
#                gtk.gdk.ACTION_DEFAULT | gtk.gdk.ACTION_MOVE)
#        self.treeview.connect("drag_data_get", self.drag_data_get_data)

#        self.treeview.enable_model_drag_dest(TARGETS,
#                gtk.gdk.ACTION_DEFAULT | gtk.gdk.ACTION_MOVE)
#        self.treeview.connect("drag-data-received", self.drag_drop_data)

        self.treeview.add_events(gtk.gdk.BUTTON_PRESS_MASK)
        self.treeview.connect('button-press-event', self.pop_menu)

        self.treeview.connect('row_activated', self.tv_row_activated)
        self.treeview.connect('key_press_event', self.tv_key_pressed_event)

        # icon and name
        col = gtk.TreeViewColumn(_("Name"))
        col.set_min_width(int(self.col_width_adj.value))
        cell = gtk.CellRendererPixbuf()
        cell.set_fixed_size(TV_ICON_SIZE, TV_ICON_SIZE)
        col.pack_start(cell, expand = False)
        col.set_cell_data_func(cell, self.get_col_icon)

        cell = gtk.CellRendererText()
        col.pack_start(cell, expand = True)
        col.set_cell_data_func(cell, self.get_col_name)
        col.set_resizable(True)
        col.set_sizing(gtk.TREE_VIEW_COLUMN_AUTOSIZE)
        self.treeview.append_column(col)

        # value
        col = gtk.TreeViewColumn(_("Value"))
        self.col_value = col

        self.edit_cell = CellRendererMx()
        self.edit_cell.set_property("editable", True)
        self.edit_cell.connect('edited', self.edited)
        self.edit_cell.set_preediting(self.get_editinfo)
        self.edit_cell.set_treeview(self.treeview)
        col.pack_start(self.edit_cell, expand = True)
        col.set_cell_data_func(self.edit_cell, self.get_col_value)
        col.set_resizable(True)
        col.set_min_width(int(self.col_width_adj.value))
        self.treeview.append_column(col)

        self.treeview.set_tooltip_column(1)
        self.treeview.get_selection().connect("changed",
                                              self.get_selected_feature)

        self.treeview.set_model(self.master_filter)
        self.treeview.set_size_request(int(self.col_width_adj.value), 100)

#    def drag_data_get_data(self, treeview, context, selection, target_id,
#                           etime):
#        treeselection = treeview.get_selection()
#        model, itr = treeselection.get_selected()
#        data = model.get_value(itr, 0)
#        selection.set(selection.target, 8, data)

#    def drag_drop_data(self, treeview, context, x, y,
#                selection, info, etime, *args):
#        self.treeview.stop_emission('by-name')
#
#        model = treeview.get_model()
#        data = selection.data
#        drop_info = treeview.get_dest_row_at_pos(x, y)
#        if drop_info:
#            path, position = drop_info
#            itr = model.get_iter(path)
#            if (position == gtk.TREE_VIEW_DROP_BEFORE
#                or position == gtk.TREE_VIEW_DROP_INTO_OR_BEFORE):
#                model.insert_before(itr, [data])
#            else:
#                model.insert_after(itr, [data])
#        else:
#            model.append([data])
#        if context.action == gtk.gdk.ACTION_MOVE:
#            context.finish(True, True, etime)

    def pop_menu(self, tv, event):
        if event.button == 3:
            self.pop_up = None
            self.pop_up = gtk.Menu()

            pthinfo = tv.get_path_at_pos(int(event.x), int(event.y))
            if pthinfo is not None:
                path, col, cellx, celly = pthinfo
                tv.grab_focus()
                tv.set_cursor(path, col, 0)
                if tv == self.treeview :
                    itr = self.master_filter.get_iter(path)
                    itr = self.master_filter.convert_iter_to_child_iter(itr)
                else :
                    itr = self.details_filter.get_iter(path)
                    itr = self.details_filter.convert_iter_to_child_iter(itr)
                dt = self.treestore.get_value(itr, 0).get_type()

                if dt in ['string', 'float', 'int'] :
                    chng_dt_menu = gtk.MenuItem(_('Change data type'))
                    self.pop_up.append(chng_dt_menu)
                    sub_menu = gtk.Menu()
                    chng_dt_menu.set_submenu(sub_menu)

                    if dt != 'string' :
                        menu_item = gtk.MenuItem(_('String'))
                        menu_item.connect("activate",
                                          self.pop_chng_dt_activate, itr, 1)
                        sub_menu.append(menu_item)

                    if dt != 'int' :
                        menu_item = gtk.MenuItem(_('Integer'))
                        menu_item.connect("activate",
                                          self.pop_chng_dt_activate, itr, 2)
                        sub_menu.append(menu_item)

                    if dt != 'float' :
                        menu_item = gtk.MenuItem(_('Float'))
                        menu_item.connect("activate",
                                          self.pop_chng_dt_activate, itr, 3)
                        sub_menu.append(menu_item)

                    if dt == 'float' :
                        menu_digits = gtk.MenuItem(_('Set digits'))
                        self.pop_up.append(menu_digits)
                        sub_menu = gtk.Menu()
                        menu_digits.set_submenu(sub_menu)

                        i = 1
                        while i < 7 :
                            menu_item = gtk.MenuItem(str(i))
                            menu_item.connect("activate",
                                        self.pop_set_digits_activate, itr, i)
                            sub_menu.append(menu_item)
                            i = i + 1
                    self.pop_up.append(gtk.SeparatorMenuItem())

                self.pop_up.append(self.create_mi(self.actionEditFeature))
                self.pop_up.append(self.create_mi(self.actionReloadFeature))

                self.pop_up.append(gtk.SeparatorMenuItem())

                self.pop_up.append(self.create_mi(self.actionDuplicate))
                self.pop_up.append(self.create_mi(self.actionDelete))

                self.pop_up.append(gtk.SeparatorMenuItem())

            self.pop_up.append(self.create_mi(self.actionAdd))
            self.pop_up.append(gtk.SeparatorMenuItem())
            self.pop_up.append(self.actionSingleView.create_menu_item())
            self.pop_up.append(self.actionDualView.create_menu_item())
            self.pop_up.append(gtk.SeparatorMenuItem())

            self.pop_up.append(self.actionTopBottom.create_menu_item())
            self.pop_up.append(self.actionSideBySide.create_menu_item())
            self.pop_up.append(gtk.SeparatorMenuItem())

            if DEFAULT_USE_WIDGETS :
                self.pop_up.append(self.actionUseWidgets.create_menu_item())
            self.pop_up.append(self.actionHideValCol.create_menu_item())
            self.pop_up.append(self.actionSubHdrs.create_menu_item())

            self.pop_up.show_all()
            self.pop_up.popup(None, None, None, event.button, event.time, None)
            return True

    def pop_set_digits_activate(self, callback = None, *args) :
        self.treestore.get(args[0], 0)[0].attr["digits"] = str(args[1])
        fmt = '{0:0.' + str(args[1]) + 'f}'
        v = get_float(self.treestore.get(args[0], 0)[0].get_value())
        self.treestore.get(args[0], 0)[0].attr["value"] = fmt.format(v)
        self.action()

    def pop_chng_dt_activate(self, callback = None, *args) :
        if args[1] == 1 :
            self.treestore.get(args[0], 0)[0].attr["type"] = 'string'
        elif args[1] == 2 :
            self.treestore.get(args[0], 0)[0].attr["type"] = 'int'
            v = get_float(self.treestore.get(args[0], 0)[0].get_value())
            self.treestore.get(args[0], 0)[0].attr["value"] = '%d' % v
        elif args[1] == 3 :
            self.treestore.get(args[0], 0)[0].attr["type"] = 'float'
            v = get_float(self.treestore.get(args[0], 0)[0].get_value())
            fmt = '{0:0.' + self.treestore.get(args[0], 0)[0].get_digits() + 'f}'
            self.treestore.get(args[0], 0)[0].attr["value"] = fmt.format(v)
        self.action()

    def create_widget_box(self) :
        tbl = gtk.Table(1, 2)
        tbl.set_row_spacings(2)
        tbl.set_col_spacings(3)
        tbl.set_border_width(4)
        row_no = 0

        itr = self.details_filter.get_iter_first()
        while itr is not None :

            lbl = gtk.Label()
            lbl.set_ellipsize(pango.ELLIPSIZE_END)
            lbl.set_property("use-markup", True)
            lbl.set_alignment(0.0, 0.5)
            lbl.set_label(self.details_filter.get_value(itr, 0).get_name())
            lbl.set_tooltip_markup(self.details_filter.get_value(itr, 0).get_attr('tool_tip'))
            tooltip = self.details_filter.get_value(itr, 0).get_attr('tool_tip')

            data_type = self.details_filter.get_value(itr, 0).get_type()

            if data_type in ['sub-header', 'header'] :
                lbl.set_padding(25, 4)
                lbl.set_tooltip_markup(lbl.get_label())
                tbl.attach(lbl, 0, 2, row_no, row_no + 1)
            else :
                hbox = gtk.HBox(False, 4)
                hbox.set_size_request(130, -1)

                img = gtk.Image()
                pbuf = self.details_filter.get_value(itr, 0).get_icon()
                if pbuf :
                    img.set_from_pixbuf(pbuf)
                    pad = (TV_ICON_SIZE - pbuf.get_width()) / 2
                    if pad < 0:
                        pad = 0
                else :
                    pad = TV_ICON_SIZE / 2

                hbox.pack_start(img, expand = False, padding = pad)
                hbox.pack_start(lbl, expand = True, fill = True)
                tbl.attach(hbox, 0, 1, row_no, row_no + 1)

                val = self.details_filter.get_value(itr, 0).get_value()
                if data_type == 'float':
                    try :
                        f_val = float(val)
                    except :
                        StatusBar.push(SB_Params_context_id,
                                    _('Can not convert "%s" to float') % val)
                        data_type = 'string'

                if data_type == 'int':
                    try :
                        i_val = int(val)
                    except :
                        if val != '' :
                            StatusBar.push(SB_Params_context_id,
                                        _('Can not convert "%s" to int') % val)
                            data_type = 'string'
                        else :
                            i_val = 0

                if data_type in ['bool', 'boolean'] :
                    cell = gtk.CheckButton('')
                    cell.set_active(get_int(val) != 0)
                    cell.set_tooltip_markup(lbl.get_tooltip_markup())
                    cell.connect('toggled', self.edited_toggled, itr)
                    tbl.attach(cell, 1, 2, row_no, row_no + 1, ypadding = 4)

                elif data_type == 'combo':
                    cell = gtk.combo_box_new_text()
                    options = self.details_filter.get_value(itr, 0).attr['options']

                    i = 0
                    act = -1
                    found = False
                    for option in options.split(":") :
                        opt = option.split('=')
                        cell.append_text(opt[0])
                        if (not found) and (opt[1] == val) :
                            act = i
                            found = True
                        i += 1

                    cell.set_active(act)
                    cell.connect('changed', self.edited_combo, itr)
                    cell.set_tooltip_markup(lbl.get_tooltip_markup())
                    tbl.attach(cell, 1, 2, row_no, row_no + 1)

                elif data_type == 'float':
                    d = get_int(self.details_filter.get_value(itr, 0).get_digits())
                    mini = get_float(self.details_filter.get_value(itr, 0).get_min_value())
                    maxi = get_float(self.details_filter.get_value(itr, 0).get_max_value())
                    adj = gtk.Adjustment(f_val, mini, maxi, 0.1, 1)
                    cell = gtk.SpinButton(adj, digits = d)
                    cell.set_tooltip_markup(lbl.get_tooltip_markup())
                    adj.connect('value-changed', self.edited_float, itr)
                    tbl.attach(cell, 1, 2, row_no, row_no + 1)

                elif data_type == 'int':
                    mini = int(get_float(self.details_filter.get_value(itr, 0).get_min_value()))
                    maxi = int(get_float(self.details_filter.get_value(itr, 0).get_max_value()))
                    adj = gtk.Adjustment(i_val, mini, maxi, 1, 10)
                    cell = gtk.SpinButton(adj)
                    cell.set_tooltip_markup(lbl.get_tooltip_markup())
                    adj.connect('value-changed', self.edited_int, itr)
                    tbl.attach(cell, 1, 2, row_no, row_no + 1)

                else :  # string
                    cell = gtk.Entry()
                    cell.set_text(val)
                    cell.set_tooltip_markup(lbl.get_tooltip_markup())
                    cell.connect('changed', self.edited_text, itr)
                    tbl.attach(cell, 1, 2, row_no, row_no + 1)

            if tooltip is not None and show_tooltip_in_widget_box :
                row_no += 1
                tbl.resize(row_no + 1, 2)
                lbl = gtk.Label()
                lbl.set_alignment(0.0, 0.5)
                lbl.set_padding(TV_ICON_SIZE + 25, 4)
                lbl.set_ellipsize(pango.ELLIPSIZE_END)
                lbl.set_property("use-markup", True)
                lbl.set_label('<i>%s</i>' % tooltip)
                tbl.attach(lbl, 0, 2, row_no, row_no + 1)

            itr = self.details_filter.iter_next(itr)
            if itr :
                row_no += 1
                tbl.resize(row_no + 1, 2)

        vbox = gtk.VBox()
        vbox.pack_start(tbl, expand = False, fill = False)
        lbl = gtk.Label('')
        vbox.pack_start(lbl, expand = True, fill = True)
        if self.widgets_viewport is not None :
            self.widgets_viewport.destroy()
        self.widgets_viewport = gtk.Viewport()
        self.widgets_viewport.add(vbox)
        self.params_scroll.add(self.widgets_viewport)
        self.params_scroll.show_all()


    def create_second_treeview(self):
        self.treeview2 = gtk.TreeView()
        self.treeview2.add_events(gtk.gdk.BUTTON_PRESS_MASK)
        self.treeview2.connect('button-press-event', self.pop_menu)
        self.treeview2.set_grid_lines(gtk.TREE_VIEW_GRID_LINES_VERTICAL)
        self.treeview2.set_show_expanders(False)

        # icon and name
        col = gtk.TreeViewColumn(_("Name"))
        cell = gtk.CellRendererPixbuf()
        cell.set_fixed_size(TV_ICON_SIZE, TV_ICON_SIZE)

        col.pack_start(cell, expand = False)
        col.set_cell_data_func(cell, self.get_col_icon)

        cell = gtk.CellRendererText()
        col.set_sizing(gtk.TREE_VIEW_COLUMN_AUTOSIZE)
        col.pack_start(cell, expand = True)
        col.set_cell_data_func(cell, self.get_col_name)
        col.set_resizable(True)
        col.set_min_width(int(self.col_width_adj.value))
        self.treeview2.append_column(col)

        # value
        col = gtk.TreeViewColumn(_("Value"))
        cell = CellRendererMx()
        cell.set_property("editable", True)
        cell.connect('edited', self.edited_tv2)
        cell.set_treeview(self.treeview2)
        cell.set_preediting(self.get_editinfo)

        col.pack_start(cell, expand = False)
        col.set_cell_data_func(cell, self.get_col_value)
        self.cell_value2 = cell
        self.col_value2 = col
        col.set_resizable(True)
        col.set_min_width(int(self.col_width_adj.value))
        self.treeview2.append_column(col)

        self.treeview2.set_tooltip_column(1)
        self.treeview2.set_model(self.details_filter)
        self.params_scroll.add(self.treeview2)
        self.treeview2.connect('key-press-event', self.tv2_kp_event)

    def create_actions(self):
        # without accelerators

        self.actionBuild = gtk.Action("actionBuild", _('Create features.ngc'), _('Build gcode and save to features.ngc'), 'gnome-run')
        self.actionBuild.connect('activate', self.action_build)

        self.actionDualView = gtk.RadioAction("actionDualView", _('Dual Views'), _('Dual Views'), '', 0)
        self.actionDualView.connect('activate', self.dual_view_activate)

        self.actionSingleView = gtk.RadioAction("actionSingleView", _('Single View'), _('Single View'), '', 0)
        self.actionSingleView.set_group(self.actionDualView)
        self.actionSingleView.connect('activate', self.single_view_activate)

        self.actionEditDefaults = gtk.Action("actionEditDefaults", _('Edit Defaults'), _('Edit Defaults'), 'gtk-edit')
        self.actionEditDefaults.connect('activate', self.edit_defaults)

        self.actionEditFeature = gtk.Action("actionEditFeature", _('Edit Current Subroutine'), _('Edit Current Subroutine'), 'gtk-edit')
        self.actionEditFeature.connect('activate', self.edit_feature)

        self.actionEditPostAmble = gtk.Action("actionEditPostAmble", _('Edit Post Amble'), _('Edit Post Amble'), 'gtk-edit')
        self.actionEditPostAmble.connect('activate', self.edit_postamble)

        self.actionHideValCol = gtk.ToggleAction("actionHideValCol", _('Hide Master Tree Value Column'), _('Hide Master Tree Value Column'), '')
        self.actionHideValCol.connect('activate', self.hide_value_col)

        self.actionReloadDefaults = gtk.Action("actionReloadDefaults", _('Reload Defaults'), _('Reload Defaults'), 'gtk-refresh')
        self.actionReloadDefaults.connect('activate', self.reload_defaults)

        self.actionReloadFeature = gtk.Action("actionReloadFeature", _('Reload Current Subroutine'), _('Reload Current Subroutine'), 'gtk-refresh')
        self.actionReloadFeature.connect('activate', self.reload_subroutine)

        self.actionReloadPostAmble = gtk.Action("actionReloadPostAmble", _('Reload Post Amble'), _('Reload Post Amble'), 'gtk-refresh')
        self.actionReloadPostAmble.connect('activate', self.reload_postamble)

        self.actionSaveLayout = gtk.Action("actionSaveLayout", _('Save As Default Layout'), _('Save As Default Layout'), 'gtk-save')
        self.actionSaveLayout.connect('activate', self.save_preferences)

        self.actionSaveNGC = gtk.Action("actionSaveNGC", _('Export gcode as RS274NGC'), _('Export gcode as RS274NGC'), 'gtk-save')
        self.actionSaveNGC.connect('activate', self.save_ngc)

        self.actionSaveTemplate = gtk.Action("actionSaveTemplate", _('Save as Default Template'), _('Save as Default Template'), 'gtk-save')
        self.actionSaveTemplate.connect('activate', self.save_default_template)

        self.actionSideBySide = gtk.RadioAction("actionSideBySide", _('Side By Side Layout'), _('Side By Side Layout'), '', 0)
        self.actionSideBySide.connect('activate', self.side_by_side_layout)

        self.actionTopBottom = gtk.RadioAction("actionTopBottom", _('Top / Bottom Layout'), _('Top / Bottom Layout'), '', 0)
        self.actionTopBottom.set_group(self.actionSideBySide)
        self.actionTopBottom.connect('activate', self.top_bottom_layout)

        self.actionUseWidgets = gtk.ToggleAction("actionUseWidgets", _('Use Widgets Box'), _('Use Widgets Box'), '')
        self.actionUseWidgets.connect('activate', self.use_widget)

        self.actionSubHdrs = gtk.ToggleAction("actionSubHdrs", _('Sub-Groups In Master Tree'), _('Sub-Groups In Master Tree'), '')
        self.actionSubHdrs.connect('activate', self.action_SubHdrs)


    def get_widgets(self):
        self.main_box = self.builder.get_object("MainBox")

        self.accelGroup = gtk.AccelGroup()
        self.actionGroup = self.builder.get_object("actiongroup1")

        self.create_actions()
        self.actionGroup.add_action(self.actionBuild)
        self.actionGroup.add_action(self.actionDualView)
        self.actionGroup.add_action(self.actionSingleView)
        self.actionGroup.add_action(self.actionEditDefaults)
        self.actionGroup.add_action(self.actionEditFeature)
        self.actionGroup.add_action(self.actionEditPostAmble)
        self.actionGroup.add_action(self.actionHideValCol)
        self.actionGroup.add_action(self.actionReloadDefaults)
        self.actionGroup.add_action(self.actionReloadFeature)
        self.actionGroup.add_action(self.actionReloadPostAmble)
        self.actionGroup.add_action(self.actionSaveLayout)
        self.actionGroup.add_action(self.actionSaveNGC)
        self.actionGroup.add_action(self.actionSaveTemplate)
        self.actionGroup.add_action(self.actionSideBySide)
        self.actionGroup.add_action(self.actionTopBottom)
        self.actionGroup.add_action(self.actionUseWidgets)
        self.actionGroup.add_action(self.actionSubHdrs)

        self.actionAdd = self.builder.get_object("actionAdd")
        self.actionAdd.set_accel_group(self.accelGroup)
        self.actionCut = self.builder.get_object("actionCut")
        self.actionCut.set_accel_group(self.accelGroup)
        self.actionCollapse = self.builder.get_object("actionCollapse")
        self.actionCollapse.set_accel_group(self.accelGroup)
        self.actionCopy = self.builder.get_object("actionCopy")
        self.actionCopy.set_accel_group(self.accelGroup)
        self.actionPaste = self.builder.get_object("actionPaste")
        self.actionPaste.set_accel_group(self.accelGroup)
        self.actionRemoveItem = self.builder.get_object("actionRemoveItem")
        self.actionRemoveItem.set_accel_group(self.accelGroup)
        self.actionAppendItem = self.builder.get_object("actionAppendItem")
        self.actionAppendItem.set_accel_group(self.accelGroup)
        self.actionMoveDn = self.builder.get_object("actionMoveDn")
        self.actionMoveDn.set_accel_group(self.accelGroup)
        self.actionMoveUp = self.builder.get_object("actionMoveUp")
        self.actionMoveUp.set_accel_group(self.accelGroup)
        self.actionRedo = self.builder.get_object("actionRedo")
        self.actionRedo.set_accel_group(self.accelGroup)
        self.actionUndo = self.builder.get_object("actionUndo")
        self.actionUndo.set_accel_group(self.accelGroup)
        self.actionDelete = self.builder.get_object("actionDelete")
        self.actionDelete.set_accel_group(self.accelGroup)
        self.actionDuplicate = self.builder.get_object("actionDuplicate")
        self.actionDuplicate.set_accel_group(self.accelGroup)
        self.actionNew = self.builder.get_object("actionNew")
        self.actionNew.set_accel_group(self.accelGroup)
        self.actionOpen = self.builder.get_object("actionOpen")
        self.actionOpen.set_accel_group(self.accelGroup)
        self.actionSave = self.builder.get_object("actionSave")
        self.actionSave.set_accel_group(self.accelGroup)

        bbtn = self.builder.get_object("btn_build")
        self.actionBuild.connect_proxy(bbtn)
        bbtn.set_icon_name('gnome-run')

        bbtn = self.builder.get_object("btn_collapse")
        self.actionCollapse.connect_proxy(bbtn)

        self.col_width_adj = self.builder.get_object("col_width_adj")
        self.w_adj = self.builder.get_object("width_adj")
        self.timeout_adj = self.builder.get_object("timeout_adj")
        self.digits_adj = self.builder.get_object("adj_digits")
        self.button_tb = self.builder.get_object("toolbar1")

        self.feature_pane = self.builder.get_object("features_pane")
        self.feature_Hpane = self.builder.get_object("hpaned1")

        self.params_scroll = self.builder.get_object("params_scroll")
        self.frame2 = self.builder.get_object("frame2")
        self.addVBox = self.builder.get_object("frame3")
        self.FeaturesBox = self.builder.get_object("FeaturesBox")
        self.add_iconview = self.builder.get_object("add_iconview")
        self.add_toolbar = self.builder.get_object("add_toolbar")
        self.tv_w_adj = self.builder.get_object("tv_w_adj")

        global StatusBar
        StatusBar = self.builder.get_object("statusbar")


    def get_translations(self, callback = None) :

        def get_menu_strings(path, f_name, trslatbl):
            for ptr in range(len(path)) :
                p = path[ptr]
                if p.tag.lower() == "group":
                    name = p.get("name") if "name" in p.keys() else None
                    if name :
                        trslatbl.append((f_name, name))

                    tooltip = p.get("tool_tip") if "tool_tip" in p.keys() else None
                    if (tooltip is not None) and (tooltip != ''):
                        trslatbl.append((f_name, tooltip))

                    get_menu_strings(p, f_name, trslatbl)

                elif p.tag.lower() == "sub":
                    name = p.get("name") if "name" in p.keys() else None
                    if name :
                        trslatbl.append((f_name, name))

                    tooltip = p.get("tool_tip") if "tool_tip" in p.keys() else None
                    if (tooltip is not None) and (tooltip != ''):
                        trslatbl.append((f_name, tooltip))

        def get_strings():
            os.popen("xgettext --language=Python ./features.py -o ./features.po")
            os.popen("xgettext --language=Glade ./features.glade -o ./glade.po")
            os.popen("sed --in-place ./*.po --expression=s/charset=CHARSET/charset=UTF-8/")
            os.popen("msgcat ./features.po ./glade.po -o ./locale/linuxcnc-features.tmp_po")
            os.popen("rm ./features.po ./glade.po")

                # catalogs
            find = os.popen("find ./%s -name 'menu.xml'" % CATALOGS_DIR).read()
            for s in find.split() :
                translatable = []
                splitname = s.split('/')
                fname = splitname[-1]
                destname = './locale/' + splitname[-1]

                xml = etree.parse(s).getroot()
                get_menu_strings(xml, fname, translatable)

                out = []
                for i in translatable :
                    out.append("#: %s" % i[0])
                    s = i[1].replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n")
                    out.append("_(%s)" % repr(i[1]))

                out = "\n".join(out)
                open(destname, "w").write(out)
                translatable = []
                os.popen("xgettext --language=Python --from-code=UTF-8 %s -o %s" % (destname, destname))
                os.popen("sed --in-place %s --expression=s/charset=CHARSET/charset=UTF-8/" % destname)
                os.popen("msgcat %s ./locale/linuxcnc-features.tmp_po -o ./locale/linuxcnc-features.tmp_po" % destname)
                os.popen("rm %s" % destname)

                # ini files
            find = os.popen("find ./%s -name '*.ini'" % INI_DIR).read()
            for s in find.split() :
                translatable = []
                splitname = s.split('/')
                fname = '../' + splitname[-1]
                destname = './locale/' + splitname[-1]

                f = Feature(s)
                for i in ["name", "help"] :
                    if i in f.attr :
                        s1 = f.attr[i]
                        translatable.append((fname, s1))
                for p in f.param :
                    for i in ["name", "help", "tool_tip", "options"] :
                        if i in p.attr :
                            s1 = p.attr[i]
                            translatable.append((fname, s1))
                out = []
                for i in translatable :
                    out.append("#: %s" % i[0])
                    s = i[1].replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n")
                    out.append("_(%s)" % repr(i[1]))

                out = "\n".join(out)
                open(destname, "w").write(out)

            cmd_line = "find ./locale/ -name '*.ini'"
            find = os.popen(cmd_line).read()
            for s in find.split() :
                os.popen("xgettext --language=Python --from-code=UTF-8 %s -o %s" % (s, s))
                os.popen("sed --in-place %s --expression=s/charset=CHARSET/charset=UTF-8/" % s)
                os.popen("msgcat ./locale/linuxcnc-features.tmp_po %s -o ./locale/linuxcnc-features.tmp_po" % s)
                os.popen("rm %s" % s)

            if os.path.exists("./locale/linuxcnc-features.po") :
                os.popen("msgcat ./locale/linuxcnc-features.tmp_po ./locale/linuxcnc-features.po -o ./locale/linuxcnc-features.po")
                os.remove("./locale/linuxcnc-features.tmp_po")
            else :
                os.rename("./locale/linuxcnc-features.tmp_po", "./locale/linuxcnc-features.po")

        try :
            get_strings()
            mess_dlg('Done !\nFile : ./locale/linuxcnc-features.po')
        except :
            mess_dlg('Error while getting data for translation !')

    def move(self, i) :
        itr = self.master_filter.convert_iter_to_child_iter(self.selected_feature_itr)
        if (i > 0) :
            itr_swap = self.master_filter.convert_iter_to_child_iter(self.iter_next)
        if (i < 0) :
            itr_swap = self.master_filter.convert_iter_to_child_iter(self.iter_prior)
        self.treestore.swap(itr, itr_swap)
        self.get_selected_feature(self)
        self.action()

    def get_selected_feature(self, widget) :
        # will allow import_xml to insert/append after the selected one
        # and define buttons sensitiveness

        old_selected_feature = self.selected_feature
        (model, itr) = self.treeview.get_selection().get_selected()
        self.actionCollapse.set_sensitive(itr is not None)
        if itr :
#            tree_path = model.get_path(itr)
            ts_itr = model.convert_iter_to_child_iter(itr)
            self.selected_type = model.get_value(itr, 0).attr.get("type")
            if self.selected_type == "items" :
                self.iter_selected_type = SEL_IS_ITEMS
                self.items_ts_parent_s = self.treestore.get_string_from_iter(ts_itr)

                self.items_path = model.get_path(itr)
                n_children = model.iter_n_children(itr)
                self.items_lpath = (self.items_path + (n_children,))

            elif self.selected_type in ["header", 'sub-header'] :
                self.iter_selected_type = SEL_IS_HEADER
            elif self.selected_type in SUPPORTED_DATA_TYPES :
                self.iter_selected_type = SEL_IS_PARAM
            else :
                self.iter_selected_type = SEL_IS_FEATURE

            if self.iter_selected_type in [SEL_IS_ITEMS, SEL_IS_HEADER] :
                items_ts_path = self.treestore.get_path(ts_itr)
                ts_itr = self.treestore.iter_parent(ts_itr)
                self.items_ts_parent_s = self.treestore.get_string_from_iter(ts_itr)

            while model.get_value(itr, 0).attr.get("type") in SUPPORTED_DATA_TYPES :
                itr = model.iter_parent(itr)

            self.selected_feature_itr = itr
            self.selected_feature = model.get(itr, 0)[0]
            ts_itr = model.convert_iter_to_child_iter(itr)
            self.selected_feature_ts_itr = ts_itr
            self.feature_ts_path = self.treestore.get_path(ts_itr)
            self.selected_feature_ts_path_s = self.treestore.get_string_from_iter(ts_itr)

            self.iter_next = model.iter_next(itr)
            if self.iter_next :
                self.can_move_down = (self.iter_selected_type == SEL_IS_FEATURE)
                s = str(model.get(self.iter_next, 0)[0])
                self.can_add_to_group = (s.find('type="items"') > -1) and \
                        (self.iter_selected_type == SEL_IS_FEATURE)
            else :
                self.can_add_to_group = False
                self.can_move_down = False

            self.selected_feature_parent_itr = model.iter_parent(itr)
            if self.selected_feature_parent_itr :
                path_parent = model.get_path(self.selected_feature_parent_itr)
                self.can_remove_from_group = (self.iter_selected_type == SEL_IS_FEATURE) and \
                    model.get_value(self.selected_feature_parent_itr, 0).attr.get("type") == "items"
            else :
                path_parent = None
                self.can_remove_from_group = False

            self.selected_feature_path = model.get_path(itr)
            depth = len(self.selected_feature_path)
            index_s = self.selected_feature_path[depth - 1]
            self.can_move_up = (index_s > 0) and \
                (self.iter_selected_type == SEL_IS_FEATURE)

            if index_s :
                if path_parent is None :
                    path_prior = (index_s - 1,)
                else :
                    path_prior = path_parent[0: ] + (index_s - 1,)
                self.iter_prior = model.get_iter(path_prior)
            else :
                self.iter_prior = None

        else:
            self.iter_selected_type = SEL_IS_NONE
            self.selected_feature = None
            self.selected_type = None
            self.can_move_up = False
            self.can_move_down = False
            self.can_add_to_group = False
            self.can_remove_from_group = False
            n_children = model.iter_n_children(None)
            self.items_lpath = (n_children,)
#            tree_path = None


        self.can_delete_duplicate = (self.iter_selected_type == SEL_IS_FEATURE)
        self.set_actions_state()

        if self.use_dual_views :
            if self.iter_selected_type == SEL_IS_NONE :
                if self.use_widget_box :
                    if self.widgets_viewport is not None :
                        self.widgets_viewport.destroy()
                        self.widgets_viewport = None
                elif self.treeview2 :
                    self.treeview2.set_model(None)

            if ((old_selected_feature == self.selected_feature) and \
                (self.iter_selected_type in [SEL_IS_ITEMS, SEL_IS_FEATURE, SEL_IS_HEADER])) or \
                (old_selected_feature != self.selected_feature) :

                if self.iter_selected_type in [SEL_IS_ITEMS, SEL_IS_HEADER] :
                    a_filter = self.treestore.filter_new(items_ts_path)
                else :
                    a_filter = self.treestore.filter_new(self.feature_ts_path)
                a_filter.set_visible_column(3)
                self.details_filter = a_filter
                if not self.sub_hdrs_in_tv1 :
                    self.treeview.expand_all()

                if self.use_widget_box :
                    self.create_widget_box()
                else :
                    self.treeview2.set_model(self.details_filter)
                    self.treeview2.expand_all()

    def add_to_item_clicked(self, call) :
        ts_itr = self.master_filter.convert_iter_to_child_iter(self.iter_next)
        pnext = self.treestore.get_string_from_iter(ts_itr)
        xml = self.treestore_to_xml()
        src = xml.find(".//*[@path='%s']" % self.selected_feature_ts_path_s)
        dst = xml.find(".//*[@path='%s']/param[@type='items']" % pnext)
        if (dst is not None) and (src is not None) :
            src.set("new-selected", "True")
            dst.insert(0, src)
            dst.set("expanded", "True")
            dst = xml.find(".//*[@path='%s']" % pnext)
            dst.set("expanded", "True")
            self.treestore_from_xml(xml)
            self.expand_and_select(self.path_to_new_selected)
            self.action()

    def remove_from_item_clicked(self, call) :
        xml = self.treestore_to_xml()
        src = xml.find(".//*[@path='%s']" % self.selected_feature_ts_path_s)
        src.set("new-selected", "True")
        parent = src.getparent().getparent()
        n = None
        while parent != xml and not (parent.tag == "param" and parent.get("type") == "items") and parent is not None :
            p = parent
            parent = parent.getparent()
            n = parent.index(p)
        if parent is not None and n is not None:
            parent.insert(n, src)
            self.treestore_from_xml(xml)
            self.expand_and_select(self.path_to_new_selected)
            self.action()

    def expand_and_select(self, path):
        if path is not None :
            self.treeview.expand_to_path(path)
            self.treeview.set_cursor(path)
        else :
            self.treeview.expand_to_path((0,))
            self.treeview.set_cursor((0,))

    def duplicate_clicked(self, *arg) :
        if use_top_features_toolbar :
            self.topfeatures_update(self.selected_feature.get_attr('src'))
        xml = etree.Element(XML_TAG)
        self.treestore_to_xml_recursion(self.selected_feature_ts_itr, xml, False)
        self.import_xml(xml)

    def tv_row_activated(self, tv, path, col) :
        if tv.row_expanded(path) :
            tv.collapse_row(path)
        else:
            tv.expand_row(path, False)

    def tv_w_adj_value_changed(self, *args):
        self.feature_pane.set_size_request(int(self.tv_w_adj.value), 100)

# TODO: stand-alone and embeded does not behave the same, hard work to do here

    def tv_key_pressed_event(self, widget, event) :
        keyname = gtk.gdk.keyval_name(event.keyval)
        selection = self.treeview.get_selection()
        model, pathlist = selection.get_selected_rows()
        path = pathlist[0] if len(pathlist) > 0 else None

        if self.embedded :
            if event.state & gtk.gdk.SHIFT_MASK :
                if event.state & gtk.gdk.CONTROL_MASK :
                    if keyname in ['z', 'Z'] :
                        self.actionRedo.activate()
                        self.treeview.grab_focus()
                        return True

            elif event.state & gtk.gdk.CONTROL_MASK :
                if keyname in ['z', 'Z'] :
                    self.actionUndo.activate()
                    self.treeview.grab_focus()
                    return True

                elif keyname == "Up" :
                    self.actionMoveUp.activate()
                    self.treeview.grab_focus()
                    return True

                elif keyname == "Down" :
                    self.actionMoveDn.activate()
                    self.treeview.grab_focus()
                    return True

                elif keyname == "Left" :
                    self.actionRemoveItem.activate()
                    return True

                elif keyname == "Right" :
                    self.actionAppendItem.activate()
                    return True

                elif keyname == "Insert" :
                    self.actionAdd.activate()
                    return True

                elif keyname == "Delete" :
                    self.actionDelete.activate()
                    return True

                elif (keyname in ["D", "d"]) :
                    self.actionDuplicate.activate()
                    return True

                elif (keyname in ["X", "x"]) :
                    self.actionCut.activate()
                    return True

                elif (keyname in ["C", "c"]) :
                    self.actionCopy.activate()
                    return True

                elif (keyname in ["V", "v"]) :
                    self.actionPaste.activate()
                    return True

                elif (keyname in ["X", "x"]) :
                    self.actionCut.activate()
                    return True

                elif (keyname in ["N", "n"]) :
                    self.actionNew.activate()
                    return True

                elif (keyname in ["O", "o"]) :
                    self.actionOpen.activate()
                    return True

                elif (keyname in ["S", "s"]) :
                    self.actionSave.activate()
                    return True

                elif (keyname in ["K", "k"]) :
                    self.actionCollapse.activate()
                    return True

            else :
                if keyname == "Up" :
                    rect = self.treeview.get_cell_area(path, self.col_value)
                    path = self.treeview.get_path_at_pos(rect[0], rect[1] - 1)
                    if path :
                        path = path[0]
                    self.treeview.set_cursor(path, focus_column = self.col_value)
                    return True

                elif keyname == "Down" :
                    rect = self.treeview.get_cell_area(path, self.col_value)
                    path = self.treeview.get_path_at_pos(rect[0], rect[1] + rect[3] + 1)
                    if path :
                        path = path[0]
                    self.treeview.set_cursor(path, focus_column = self.col_value)
                    return True

                elif keyname == "Left" :
                    if path :
                        self.treeview.collapse_row(path)
                    return True

                elif keyname == "Right" :
                    if path :
                        self.treeview.expand_row(path, False)
                    return True

                elif keyname in ["Return", "KP_Enter", "space"] :
                    if path :
                        self.treeview.set_cursor_on_cell(path, focus_column = self.col_value, start_editing = True)
                    return True

        if keyname == "BackSpace" :
            if path :
                self.edit_cell.set_Input('BS')
                self.treeview.set_cursor_on_cell(path, focus_column = self.col_value, start_editing = True)
            return True

        if keyname == "F2" :
            if path :
                self.treeview.set_cursor_on_cell(path, focus_column = self.col_value, start_editing = True)
                self.treeview.grab_focus()
            return True

        if keyname >= "KP_0" and keyname <= "KP_9" :
            if path :
                self.edit_cell.set_Input(keyname[-1])
                self.treeview.set_cursor_on_cell(path, focus_column = self.col_value, start_editing = True)
            return True

        if keyname >= "0" and keyname <= "9" :
            if path :
                self.edit_cell.set_Input(keyname)
                self.treeview.set_cursor_on_cell(path, focus_column = self.col_value, start_editing = True)
            return True

        elif keyname in ['KP_Decimal', 'period', 'comma'] :
            if path :
                self.edit_cell.set_Input('.')
                self.treeview.set_cursor_on_cell(path, focus_column = self.col_value, start_editing = True)
            return True

        elif keyname in ['KP_Subtract', 'KP_Add', 'plus', 'minus'] :
            if path :
                self.edit_cell.set_Input('-')
                self.treeview.set_cursor_on_cell(path, focus_column = self.col_value, start_editing = True)
            return True

        else :
            return False

    def tv2_kp_event(self, widget, event) :
        keyname = gtk.gdk.keyval_name(event.keyval)
        selection = self.treeview2.get_selection()
        model, pathlist = selection.get_selected_rows()
        path = pathlist[0] if len(pathlist) > 0 else None

        if self.embedded:

            if keyname == "Up" :
                rect = self.treeview2.get_cell_area(path, self.col_value2)
                path = self.treeview2.get_path_at_pos(rect[0], rect[1] - 1)
                if path :
                    path = path[0]
                self.treeview2.set_cursor(path, focus_column = self.col_value2)
                return True

            elif keyname == "Down" :
                rect = self.treeview2.get_cell_area(path, self.col_value2)
                path = self.treeview2.get_path_at_pos(rect[0], rect[1] + rect[3] + 1)
                if path :
                    path = path[0]
                self.treeview2.set_cursor(path, focus_column = self.col_value2)
                return True

            elif keyname in ["Return", "KP_Enter", "space"] :
                print(keyname)
                if path :
                    self.treeview2.set_cursor_on_cell(path, focus_column = self.col_value2, start_editing = True)
                return True

        if keyname == "F2" :
            if path :
                self.treeview2.set_cursor_on_cell(path, focus_column = self.col_value2, start_editing = True)
                self.treeview2.grab_focus()
            return True

        if keyname == "BackSpace" :
            if path :
                self.cell_value2.set_Input('BS')
                self.treeview2.set_cursor_on_cell(path, focus_column = self.col_value2, start_editing = True)
            return True

        if keyname >= "KP_0" and keyname <= "KP_9" :
            if path :
                self.cell_value2.set_Input(keyname[-1])
                self.treeview2.set_cursor_on_cell(path, focus_column = self.col_value2, start_editing = True)
            return True

        if keyname >= "0" and keyname <= "9" :
            if path :
                self.cell_value2.set_Input(keyname)
                self.treeview2.set_cursor_on_cell(path, focus_column = self.col_value2, start_editing = True)
            return True

        elif keyname in ['KP_Decimal', 'period', 'comma'] :
            if path :
                self.cell_value2.set_Input('.')
                self.treeview2.set_cursor_on_cell(path, focus_column = self.col_value2, start_editing = True)
            return True

        elif keyname in ['KP_Subtract', 'KP_Add', 'plus', 'minus'] :
            if path :
                self.cell_value2.set_Input('-')
                self.treeview2.set_cursor_on_cell(path, focus_column = self.col_value2, start_editing = True)
            return True

        else :
            return False

    def show_help(self) :
        html_hlp = self.selected_feature.get_attr("html_help")
        if html_hlp :
            webbrowser.open(html_hlp, new = 2)

    def treestore_from_xml(self, xml):
        def recursive(treestore, itr, xmlpath):
            for xml in xmlpath :
                if xml.tag == "feature" :
                    f = Feature(xml = xml)
                    tool_tip = f.attr["tool_tip"] if "tool_tip" in f.attr else None
                    if not tool_tip :
                        tool_tip = f.attr["help"] if "help" in f.attr else None
                    citer = treestore.append(itr, [f, tool_tip, True, False])

                    grp_header = ''

                    for p in f.param :
                        header_name = p.attr["header"].lower() if "header" in p.attr else ''

                        tool_tip = p.attr["tool_tip"] if "tool_tip" in p.attr else None
                        p_type = p.get_type()

                        if self.use_dual_views :
                            if self.sub_hdrs_in_tv1 :
                                m_visible = p_type in ['items', 'header', 'sub-header']
                                is_visible = p_type not in ['items', 'header', 'sub-header']
                            else :
                                m_visible = p_type in ['items', 'header']
                                is_visible = p_type not in ['items', 'header']
                        else :
                            m_visible = True
                            is_visible = False

                        if p_type == "items" :
                            piter = treestore.append(citer, [p, tool_tip, m_visible, is_visible])
                            xmlpath_ = xml.find(".//param[@type='items']")
                            recursive(treestore, piter, xmlpath_)
                        elif p_type in ['header', 'sub-header'] :
                            if (header_name == '') :
                                hiter = treestore.append(citer, [p, tool_tip, m_visible, is_visible])
                            elif grp_header == header_name :
                                hiter = treestore.append(hiter, [p, tool_tip, m_visible, is_visible])
                            else :
                                while True :
                                    f_ = treestore.get_value(hiter, 0)
                                    if f_ == f :
                                        hiter = treestore.append(citer, [p, tool_tip, m_visible, is_visible])
                                        break
                                    if f_.attr['call'][7:] == header_name :
                                        hiter = treestore.append(hiter, [p, tool_tip, m_visible, is_visible])
                                        break
                                    hiter = treestore.iter_parent(hiter)
                            grp_header = p.attr['call'][7:]
                        else :
                            if (header_name == '') :
                                treestore.append(citer, [p, tool_tip, m_visible, is_visible])
                            elif grp_header == header_name :
                                treestore.append(hiter, [p, tool_tip, m_visible, is_visible])
                            else :
                                while True :
                                    f_ = treestore.get_value(hiter, 0)
                                    if f_ == f :
                                        treestore.append(citer, [p, tool_tip, m_visible, is_visible])
                                        grp_header = ''
                                        break
                                    if f_.attr['call'][7:] == header_name :
                                        treestore.append(hiter, [p, tool_tip, m_visible, is_visible])
                                        break
                                    hiter = treestore.iter_parent(hiter)


        treestore = gtk.TreeStore(object, str, gobject.TYPE_BOOLEAN, gobject.TYPE_BOOLEAN)
        recursive(treestore, treestore.get_iter_root(), xml)
        self.treestore = treestore
        self.master_filter = self.treestore.filter_new()
        self.master_filter.set_visible_column(2)
        self.treeview.set_model(self.master_filter)
        self.set_expand()


    def to_gcode(self, *arg) :
        def recursive(itr) :
            gcode_def = ""
            gcode = ""
            f = self.treestore.get(itr, 0)[0]
            if f.__class__ == Feature :
                gcode_def += f.get_definitions()
                gcode += f.process(f.attr["before"])
                gcode += f.process(f.attr["call"])
            itr = self.treestore.iter_children(itr)
            while itr :
                g, d = recursive(itr)
                gcode += g
                gcode_def += d
                itr = self.treestore.iter_next(itr)
            if f.__class__ == Feature :
                gcode += f.process(f.attr["after"]) + "\n"
            return gcode, gcode_def

        if DEFAULT_METRIC :
            detect = "(Metric detected)\nG21\n\n"
        else :
            detect = "(Metric not detected, using inch)\nG20\n\n"

        gcode = ""
        gcode_def = ""
        global DEFINITIONS
        DEFINITIONS = []
        global INCLUDE
        INCLUDE = []
        itr = self.treestore.get_iter_root()
        while itr is not None :
            g, d = recursive(itr)
            gcode += g
            gcode_def += d
            itr = self.treestore.iter_next(itr)
        return detect + self.defaults + "\n(Definitions)\n" + gcode_def + \
            "(End definitions)\n" + gcode + '\n' + self.post_amble + "\nM2"


    def action_build(self, *arg) :
        self.autorefresh_call()
        if not self.LinuxCNC_connected :
            mess_dlg(_("LinuxCNC not running\n\nStart LinuxCNC with the right config ini\nand press Build button again"))
        else :
            self.auto_refresh.set_active(True)


    def save_ngc(self, *arg) :
        filechooserdialog = gtk.FileChooserDialog(_("Save as..."), None, gtk.FILE_CHOOSER_ACTION_SAVE,
            (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_OK, gtk.RESPONSE_OK))
        try :
            filt = gtk.FileFilter()
            filt.set_name("NGC")
            filt.add_mime_type("text/ngc")
            filt.add_pattern("*.ngc")
            filechooserdialog.add_filter(filt)
            filechooserdialog.set_current_folder(APP_PATH + NGC_DIR)

            response = filechooserdialog.run()
            if response == gtk.RESPONSE_OK:
                gcode = self.to_gcode()
                filename = filechooserdialog.get_filename()
                if filename[-4] != ".ngc" not in filename :
                    filename += ".ngc"
                f = open(filename, "w")
                f.write(gcode)
            f.close()
        finally :
            filechooserdialog.destroy()

    def edited(self, renderer, path, new_text) :
        itr = self.master_filter.get_iter(path)
        itr = self.master_filter.convert_iter_to_child_iter(itr)
        old_value = self.treestore.get_value(itr, 0).get_value()
        if old_value != new_text :
            self.treestore.get(itr, 0)[0].attr["value"] = new_text
            self.action()
        self.treeview.grab_focus()

    def edited_tv2(self, renderer, path, new_text) :
        itr = self.details_filter.get_iter(path)
        itr = self.details_filter.convert_iter_to_child_iter(itr)
        old_value = self.treestore.get_value(itr, 0).get_value()
        if old_value != new_text :
            self.treestore.get(itr, 0)[0].attr["value"] = new_text
            self.action()
        self.treeview2.grab_focus()

    def edited_text(self, entry, *arg) :
        itr = self.details_filter.convert_iter_to_child_iter(*arg)
        old_value = self.treestore.get_value(itr, 0).get_value()
        new_value = entry.get_text()
        if old_value != new_value :
            self.treestore.get(itr, 0)[0].attr["value"] = new_value
            self.action()

    def edited_combo(self, combo, *arg):
        itr = self.details_filter.convert_iter_to_child_iter(arg[0])
        old_value = self.treestore.get_value(itr, 0).get_value()
        options = self.treestore.get_value(itr, 0).attr['options']
        option = options.split(':')[combo.get_active()][1]
        new_value = option.split('=')[1]
        if old_value != new_value :
            self.treestore.get(itr, 0)[0].attr["value"] = new_value
            self.action()

    def edited_toggled(self, chk, *arg) :
        itr = self.details_filter.convert_iter_to_child_iter(*arg)
        if get_int(self.treestore.get_value(itr, 0).get_value()) :
            self.treestore.get(itr, 0)[0].attr["value"] = '0'
        else :
            self.treestore.get(itr, 0)[0].attr["value"] = '1'
        self.action()

    def edited_float(self, adj, *arg) :
        itr = self.details_filter.convert_iter_to_child_iter(arg[0])
        fmt = '{0:0.' + self.treestore.get(itr, 0)[0].get_digits() + 'f}'
        new_value = fmt.format(adj.get_value())
        old_value = self.treestore.get_value(itr, 0).get_value()
        if old_value != new_value :
            self.treestore.get(itr, 0)[0].attr["value"] = new_value
            self.action()

    def edited_int(self, adj, *arg) :
        itr = self.details_filter.convert_iter_to_child_iter(arg[0])
        old_value = self.treestore.get_value(itr, 0).get_value()
        new_value = '%d' % adj.get_value()
        if old_value != new_value :
            self.treestore.get(itr, 0)[0].attr["value"] = new_value
            self.action()

    def delete_clicked(self, *arg) :
        if self.iter_next :
            next_path = self.master_filter.get_path(self.selected_feature_itr)
        elif self.iter_prior :
            next_path = self.master_filter.get_path(self.iter_prior)
        elif self.selected_feature_parent_itr :
            next_path = self.master_filter.get_path(self.selected_feature_parent_itr)
        else :
            next_path = None

        self.treestore.remove(self.selected_feature_ts_itr)

        if next_path is not None :
            self.treeview.set_cursor(next_path)

        self.action()
        self.get_selected_feature(self)

    def top_bottom_layout(self, *arg):
        self.side_by_side = False
        self.frame2.reparent(self.feature_pane)

    def side_by_side_layout(self, *arg):
        self.side_by_side = True
        self.frame2.reparent(self.feature_Hpane)

    def action_collapse(self, *arg) :
        (model, itr) = self.treeview.get_selection().get_selected()
        path = model.get_path(itr)
        self.treeview.collapse_all()
        self.treeview.expand_to_path(path)
        self.treeview.set_cursor(path)
        self.treeview.grab_focus()

    def action_SubHdrs(self, *args):
        self.sub_hdrs_in_tv1 = self.actionSubHdrs.get_active()
        self.treestore_from_xml(self.treestore_to_xml())
        self.expand_and_select(self.path_to_old_selected)

    def import_xml(self, xml_) :
        if xml_.tag != XML_TAG:
            xml_ = xml_.find(".//%s" % XML_TAG)

        if xml_ is not None :
            xml = self.treestore_to_xml()
            if self.iter_selected_type == SEL_IS_ITEMS :
                # will append to items
                dest = xml.find(".//*[@path='%s']/param[@type='items']" % self.items_ts_parent_s)
                opt = 2
                i = -1
                next_path = self.items_lpath
            elif self.iter_selected_type != SEL_IS_NONE :
                # will append after parent of selected feature
                dest = xml.find(".//*[@path='%s']" % self.selected_feature_ts_path_s)
                parent = dest.getparent()
                i = parent.index(dest)
                opt = 1
                l_path = len(self.selected_feature_path)
                next_path = (self.selected_feature_path[0:l_path - 1] + (self.selected_feature_path[l_path - 1] + 1,))
            else :  # None selected
                opt = 0
                next_path = self.items_lpath

            for x in xml_ :
                if opt == 1 :
                    i += 1
                    parent.insert(i, x)
                elif opt == 2 :
                    dest.append(x)
                else :
                    xml.append(x)

                l = x.findall(".//feature")
                if x.tag == "feature" :
                    l = [x] + l
                for xf in l :
                    f = Feature(xml = xf)
                    f.get_id(xml)
                    xf.set("name", f.attr["name"])
                    xf.set("id", f.attr["id"])

            self.treestore_from_xml(xml)
            self.expand_and_select(next_path)
            self.get_selected_feature(self)
            self.action()

    def add_feature_menu(self, widget, src):
        self.add_feature(src)

    def add_feature(self, src) :
        src_file = search_path(src)
        if not src_file :
            return

        src_data = open(src_file).read(25)
        valid_xml = src_data.find(".//%s" % XML_TAG)
        if valid_xml > -1 :
            xml = etree.parse(src_file).getroot()
        else :
            if src_data.find('[SUBROUTINE]') == -1 :
                IOError("'%s' is not a valid file")
            if use_top_features_toolbar :
                self.topfeatures_update(src)
            xml = etree.Element(XML_TAG)
            f = Feature(src_file)
            xml.append(f.to_xml())
        self.import_xml(xml)

    def autorefresh_call(self) :
        fname = APP_PATH + NGC_DIR + "features.ngc"
        f = open(fname, "w")
        f.write(self.to_gcode())
        f.close()
        try:
            linuxCNC = linuxcnc.command()
            self.LinuxCNC_connected = True
            stat = linuxcnc.stat()
            stat.poll()
            if stat.interp_state == linuxcnc.INTERP_IDLE :
                if self.display == 'axis':
                    subprocess.call(["axis-remote", fname])
                elif self.display == 'gmoccapy':
                    linuxCNC.reset_interpreter()
                    time.sleep(0.1)
                    linuxCNC.mode(linuxcnc.MODE_AUTO)
                    linuxCNC.program_open(fname)
        except :
            self.auto_refresh.set_active(False)
            self.LinuxCNC_connected = False

    def action(self, xml = None) :
        if xml is None :
            xml = self.treestore_to_xml()

        self.undo_list = self.undo_list[:self.undo_pointer + 1]
        self.undo_list = self.undo_list[max(0, len(self.undo_list) - UNDO_MAX_LEN):]
        self.undo_list.append(etree.tostring(xml))
        self.undo_pointer = len(self.undo_list) - 1
        self.update_do_btns()

    def update_do_btns(self):
        self.set_do_buttons_state()
        if self.auto_refresh.get_active() :
            if self.timeout is not None :
                gobject.source_remove(self.timeout)
            self.timeout = gobject.timeout_add(int(self.timeout_adj.value * 1000), self.autorefresh_call)

    def undo_clicked(self, *arg) :
        global restore_expand_state
        save_restore = restore_expand_state
        restore_expand_state = True
        self.undo_pointer -= 1
        self.treestore_from_xml(etree.fromstring(self.undo_list[self.undo_pointer]))
        self.expand_and_select(self.path_to_old_selected)
        restore_expand_state = save_restore
        self.update_do_btns()

    def redo_clicked(self, *arg) :
        global restore_expand_state
        save_restore = restore_expand_state
        restore_expand_state = True
        self.undo_pointer += 1
        self.treestore_from_xml(etree.fromstring(self.undo_list[self.undo_pointer]))
        self.expand_and_select(self.path_to_old_selected)
        restore_expand_state = save_restore
        self.update_do_btns()

    def set_do_buttons_state(self):
        self.actionUndo.set_sensitive(self.undo_pointer > 0)
        self.actionRedo.set_sensitive(self.undo_pointer < (len(self.undo_list) - 1))

    def clear_undo(self, *arg) :
        self.undo_list = []
        self.undo_pointer = -1
        self.set_do_buttons_state()

    def menu_file_activate(self, *args):
        self.actionBuild.set_sensitive(self.treestore.get_iter_root() is not None)
        self.actionSave.set_sensitive(self.treestore.get_iter_root() is not None)
        self.actionSaveTemplate.set_sensitive(self.treestore.get_iter_root() is not None)
        self.actionSaveNGC.set_sensitive(self.treestore.get_iter_root() is not None)

    def menu_about_activate(self, *args):
        dialog = gtk.AboutDialog()
        dialog.set_name(APP_TITLE)
        dialog.set_version(APP_VERSION)
        dialog.set_copyright(APP_COPYRIGHT)
        dialog.set_authors(APP_AUTHORS)
        dialog.set_comments(_(APP_COMMENTS))
        dialog.set_license(_(APP_LICENCE))
        dialog.set_website(HOME_PAGE)
        dialog.run()
        dialog.destroy()

    def save_default_template(self, *args):
        xml = self.treestore_to_xml()
        etree.ElementTree(xml).write(self.catalog_dir + DEFAULT_TEMPLATE, pretty_print = True)

    def menu_new_activate(self, *args):
        self.treestore.clear()
        self.clear_undo()
        if os.path.isfile(self.catalog_dir + DEFAULT_TEMPLATE):
            xml = etree.parse(self.catalog_dir + DEFAULT_TEMPLATE)
            self.treestore_from_xml(xml.getroot())
            self.expand_and_select((0,))
        self.autorefresh_call()
        self.auto_refresh.set_active(True)
        self.current_filename = _('Untitle.xml')
        self.file_changed = False
        self.action()

    def set_layout(self):
        if self.use_dual_views :
            if self.use_widget_box :
                if self.treeview2 is not None :
                    self.treeview2.destroy()
                    self.treeview2 = None
            else :
                if self.widgets_viewport is not None :
                    self.widgets_viewport.destroy()
                    self.widgets_viewport = None
                if self.treeview2 is None :
                    self.create_second_treeview()
                    self.treeview2.show_all()
                if self.side_by_side :
                    self.side_by_side_layout()
            self.frame2.set_visible(True)
        else :
            self.frame2.set_visible(False)
            if self.treeview2 is not None :
                self.treeview2.destroy()
                self.treeview2 = None
            if self.widgets_viewport is not None :
                self.widgets_viewport.destroy()
                self.widgets_viewport = None
        self.treestore_from_xml(self.treestore_to_xml())

        self.actionTopBottom.set_sensitive(self.use_dual_views)
        self.actionSideBySide.set_sensitive(self.use_dual_views)
        self.actionUseWidgets.set_sensitive(self.use_dual_views)
        self.actionSubHdrs.set_sensitive(self.use_dual_views)

    def dual_view_activate(self, *args) :
        self.use_dual_views = True
        self.set_layout()

    def single_view_activate(self, *args) :
        self.use_dual_views = False
        self.set_layout()

    def use_widget(self, callback = None) :
        self.use_widget_box = self.actionUseWidgets.get_active()
        if self.use_widget_box :
            if self.treeview2 is not None :
                self.treeview2.destroy()
                self.treeview2 = None
        if self.use_dual_views :
            path = self.selected_feature_path
            if not self.use_widget_box :
                if self.widgets_viewport is not None :
                    self.widgets_viewport.destroy()
                    self.widgets_viewport = None
                self.create_second_treeview()
                self.treeview2.show_all()
            self.treestore_from_xml(self.treestore_to_xml())
            if path :
                self.treeview.set_cursor(path)
                self.get_selected_feature(self)

    def hide_value_col(self, callback = None):
        self.col_value.set_visible(not self.actionHideValCol.get_active())

    def menu_pref_activate(self, callback = None) :
        global DEFAULT_DIGITS

        parent = self.main_box.get_toplevel()
        prefdlg = gtk.Dialog(_("Preferences"), parent,
            gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
            (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_OK, gtk.RESPONSE_ACCEPT))

        align = gtk.Alignment()
        align.set_padding(4, 4, 8, 8)
        tbl = gtk.Table(4, 2, False)
        tbl.set_col_spacings(6)
        lbl = gtk.Label(_("Name Column Minimum Width"))
        lbl.set_alignment(0.0, 0.5)
        tbl.attach(lbl, 0, 1, 0, 1)
        lbl = gtk.Label(_("Master Treeview Minimum Width"))
        lbl.set_alignment(0.0, 0.5)
        tbl.attach(lbl, 0, 1, 1, 2)
        lbl = gtk.Label(_("Window Minimum Width"))
        lbl.set_alignment(0.0, 0.5)
        tbl.attach(lbl, 0, 1, 2, 3)
        lbl = gtk.Label(_("Auto-refresh timeout"))
        lbl.set_alignment(0.0, 0.5)
        lbl.set_size_request(225, 40)
        tbl.attach(lbl, 0, 1, 3, 4)
        lbl = gtk.Label(_("Default Decimals"))
        lbl.set_alignment(0.0, 0.5)
        lbl.set_size_request(225, 40)
        tbl.attach(lbl, 0, 1, 4, 5)
        scale = gtk.HScale(self.col_width_adj)
        scale.set_digits(0)
        scale.set_size_request(300, 40)
        tbl.attach(scale, 1, 2, 0, 1,)
        scale = gtk.HScale(self.tv_w_adj)
        scale.set_digits(0)
        scale.set_size_request(300, 40)
        tbl.attach(scale, 1, 2, 1, 2,)
        scale = gtk.HScale(self.w_adj)
        scale.set_digits(0)
        scale.set_size_request(300, 40)
        tbl.attach(scale, 1, 2, 2, 3,)
        spin = gtk.SpinButton(self.timeout_adj, 0.0, 2)
        tbl.attach(spin, 1, 2, 3, 4,)
        spin = gtk.SpinButton(self.digits_adj)
        tbl.attach(spin, 1, 2, 4, 5,)

        align.add(tbl)
        prefdlg.vbox.pack_start(align)
        prefdlg.set_transient_for(parent)
        prefdlg.set_keep_above(True)
        prefdlg.show_all()
        if prefdlg.run() == gtk.RESPONSE_ACCEPT :
            DEFAULT_DIGITS = str(int(self.digits_adj.value))
            self.save_preferences(None)
        prefdlg.destroy()

    def save_preferences(self, *args):
        config = ConfigParser.RawConfigParser()

        config.add_section('general')
        config.set('general', 'version', APP_VERSION)
        config.set('general', 'chk_version', self.checked_update)
        config.set('general', 'refresh_time_out', self.timeout_adj.value)
        config.set('general', 'width', self.w_adj.value)
        config.set('general', 'col_width', self.col_width_adj.value)
        config.set('general', 'digits', self.digits_adj.value)
        config.set('general', 'dual_view', self.use_dual_views)
        config.set('general', 'use_widgets', self.use_widget_box)
        config.set('general', 'side_by_side', self.side_by_side)
        config.set('general', 'subheaders_in_MASTER', self.sub_hdrs_in_tv1)
        config.set('general', 'features_tv_width', self.tv_w_adj.value)

        with open(APP_PATH + PREFERENCES_FILE, 'wb') as configfile:
            config.write(configfile)


    def read_preferences(self):
        global APP_PATH, DEFAULT_DIGITS

        def read_float(cf, key, default):
            try :
                val = cf.getfloat('general', key)
            except :
                val = default
            return val

        def read_boolean(cf, key, default):
            try :
                val = cf.getboolean('general', key)
            except :
                val = default
            return val

        def read_str(cf, key, default):
            try :
                val = cf.get('general', key)
            except :
                val = default
            return val

        config = ConfigParser.ConfigParser()
        config.read(APP_PATH + PREFERENCES_FILE)
        self.timeout_adj.value = read_float(config, 'refresh_time_out', 1.000)
        self.w_adj.value = read_float(config, 'width', 425)
        self.col_width_adj.value = read_float(config, 'col_width', 175)
        self.digits_adj.value = read_float(config, 'digits', get_float(DEFAULT_DIGITS))
        DEFAULT_DIGITS = str(int(self.digits_adj.value))
        self.use_dual_views = read_boolean(config, 'dual_view', True)
        self.use_widget_box = read_boolean(config, 'use_widgets', DEFAULT_USE_WIDGETS)
        self.side_by_side = read_boolean(config, 'side_by_side', True)
        self.sub_hdrs_in_tv1 = read_boolean(config, 'subheaders_in_MASTER', True)
        self.tv_w_adj.value = read_float(config, 'features_tv_width', 175)
        self.checked_update = read_str('general', 'chk_version', '2.0')

#        try :
#        	v = config.get('version', '2.0')#
# 	        if v < APP_VERSION :
# 	            self.save_preferences()
#        except :
#            self.save_preferences()

    def on_scale_change_value(self, widget):
        self.main_box.set_size_request(int(self.w_adj.value), 100)

    def col_width_adj_value_changed(self, widget):
        if self.col_value :
            self.col_value.set_min_width(int(self.col_width_adj.value))
        if self.col_value2 :
            self.col_value2.set_min_width(int(self.col_width_adj.value))

    def move_up_clicked(self, *arg) :
        self.move(-1)

    def move_down_clicked(self, *arg) :
        self.move(1)

    def get_col_name(self, column, cell, model, itr) :
        data_type = model.get_value(itr, 0).get_type()
        if data_type == 'header' :
            cell.set_property('markup', header_fmt_str % model.get_value(itr, 0).get_name())
        elif data_type == 'sub-header' :
            if  self.use_dual_views and not self.sub_hdrs_in_tv1 :
                cell.set_property('markup', sub_header_fmt_str2 % model.get_value(itr, 0).get_name())
            else :
                cell.set_property('markup', sub_header_fmt_str % model.get_value(itr, 0).get_name())
        elif data_type == 'items' :
            cell.set_property('markup', items_fmt_str % model.get_value(itr, 0).get_name())
        elif data_type in SUPPORTED_DATA_TYPES :
            cell.set_property('markup', model.get_value(itr, 0).get_name())
        else :
            cell.set_property('markup', feature_fmt_str % model.get_value(itr, 0).get_name())

    def get_editinfo(self, cell, treeview, path):
        model = treeview.get_model()
        itr = model.get_iter(path)
        data_type = model.get_value(itr, 0).get_type()
        cell.set_edit_datatype(data_type)

        if data_type == 'combo':
            opt = model.get_value(itr, 0).get_options()
            cell.set_options(opt)

        elif data_type in ['float', 'int']:
            cell.set_max_value(get_float(model.get_value(itr, 0).get_max_value()))
            cell.set_min_value(get_float(model.get_value(itr, 0).get_min_value()))
            cell.set_digits(model.get_value(itr, 0).get_digits())
            cell.set_tooltip(model.get_value(itr, 0).get_tooltip())

        elif data_type == 'filename' :
            cell.set_fileinfo(model.get_value(itr, 0).get_value(), \
                            model.get_value(itr, 0).attr['patterns'], \
                            model.get_value(itr, 0).attr['mime_types'], \
                            model.get_value(itr, 0).attr['filter_name'])


    def get_col_value(self, column, cell, model, itr) :
        val = model.get_value(itr, 0).get_value()
        cell.set_param_value(val)

        data_type = model.get_value(itr, 0).get_type()
        cell.set_data_type(data_type)

        if data_type == 'combo':
            options = model.get_value(itr, 0).attr['options']
            for option in options.split(":") :
                opt = option.split('=')
                if opt[1] == val :
                    cell.set_property('text', opt[0])
                    break
        elif data_type == 'filename':
            h, t = os.path.split(val)
            cell.set_property('text', t)
        else :
            cell.set_property('text', val)

    def get_col_icon(self, column, cell, model, itr) :
        if model.get_value(itr, 0).get_type() in NO_ICON_TYPES :
            cell.set_property('pixbuf', None)
        else :
            cell.set_property('pixbuf', model.get_value(itr, 0).get_icon())

    def treestore_to_xml_recursion(self, itr, xmlpath, allitems = True):
        while itr :
            f = self.treestore.get(itr, 0)[0]
            if f.__class__ == Feature :
                xmlpath.append(f.to_xml())

            # check for the childrens
            citer = self.treestore.iter_children(itr)
            while citer :
                p = self.treestore.get(citer, 0)[0]
                itm = p.get_attr('type')
                if (itm == 'items'):
                    pa = f.get_attr('path')
                    xmlpath_ = xmlpath.find(".//*[@path='%s']/param[@type='items']" % pa)
                    if xmlpath_ is not None:
                        self.treestore_to_xml_recursion(self.treestore.iter_children(citer), xmlpath_)
                citer = self.treestore.iter_next(citer)

            # check for next items
            if allitems :
                itr = self.treestore.iter_next(itr)
            else :
                itr = None

    def treestore_to_xml(self) :
        self.get_expand()
        xml = etree.Element(XML_TAG)
        self.treestore_to_xml_recursion(self.treestore.get_iter_root(), xml)
        return xml

    def set_expand(self) :
        def treestore_set_expand(model, path, itr, self) :
            try :
                mf_itr = self.master_filter.convert_child_iter_to_iter(itr)
                mf_pa = self.master_filter.get_path(mf_itr)
                p = model.get(itr, 0)[0].attr
                if restore_expand_state :
                    if "expanded" in p and p["expanded"] == "True":
                        self.treeview.expand_row(mf_pa, False)
                    if "old-selected" in p and p["old-selected"] == "True":
                        self.treeview.set_cursor(mf_pa)
                        self.path_to_old_selected = mf_pa
                    if "new-selected" in p and p["new-selected"] == "True":
                        self.treeview.set_cursor(mf_pa)
                        self.path_to_new_selected = mf_pa
            except :
                # not in treeview
                pass

        self.path_to_new_selected = None
        self.path_to_old_selected = None
        self.selected_pathlist = []
        self.selection = self.treeview.get_selection()
        self.selection.unselect_all()
        self.treestore.foreach(treestore_set_expand, self)

    def get_expand(self) :
        def treestore_get_expand(model, path, itr, self) :
            p = model.get(itr, 0)[0]
            p.attr["path"] = model.get_string_from_iter(itr)
            try :
                mf_itr = self.master_filter.convert_child_iter_to_iter(itr)
                mf_pa = self.master_filter.get_path(mf_itr)
                p.attr["old-selected"] = mf_pa in self.selected_pathlist
                p.attr["new-selected"] = False
                p.attr["expanded"] = self.treeview.row_expanded(mf_pa)
            except :
                # not in treeview
                pass

        self.selection = self.treeview.get_selection()
        model, pathlist = self.selection.get_selected_rows()
        self.selected_pathlist = pathlist
        self.treestore.foreach(treestore_get_expand, self)

    def menu_import_activate(self, calback) :
        filechooserdialog = gtk.FileChooserDialog(_("Import"), None, gtk.FILE_CHOOSER_ACTION_OPEN,
            (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_OK, gtk.RESPONSE_OK))
        try:
            filt = gtk.FileFilter()
            filt.set_name("XML")
            filt.add_mime_type("text/xml")
            filt.add_pattern("*.xml")
            filechooserdialog.add_filter(filt)
            filt = gtk.FileFilter()
            filt.set_name(_("All files"))
            filt.add_pattern("*")
            filechooserdialog.add_filter(filt)
            filechooserdialog.set_current_folder(APP_PATH + XML_DIR)

            if filechooserdialog.run() == gtk.RESPONSE_OK:
                fname = filechooserdialog.get_filename()
                if not self.file_changed :
                    self.current_filename = fname
                self.import_xml(etree.parse(fname).getroot())
                self.file_changed = False
        finally:
            filechooserdialog.destroy()

    def menu_save_xml_activate(self, callback) :
        filechooserdialog = gtk.FileChooserDialog(_("Save as..."), None, gtk.FILE_CHOOSER_ACTION_SAVE,
                (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_OK, gtk.RESPONSE_OK))
        try:
            filt = gtk.FileFilter()
            filt.set_name("XML")
            filt.add_mime_type("text/xml")
            filt.add_pattern("*.xml")
            filechooserdialog.add_filter(filt)
            if os.path.exists(self.current_filename):
                filechooserdialog.set_filename(self.current_filename)
            else :
                filechooserdialog.set_current_folder(APP_PATH + XML_DIR)
                filechooserdialog.set_current_name(self.current_filename)
            filechooserdialog.set_do_overwrite_confirmation(True)

            if filechooserdialog.run() == gtk.RESPONSE_OK:
                xml = self.treestore_to_xml()
                self.current_filename = filechooserdialog.get_filename()
                if self.current_filename[-4] != ".xml" not in self.current_filename :
                    self.current_filename += ".xml"
                etree.ElementTree(xml).write(self.current_filename, pretty_print = True)
                self.file_changed = False
        finally:
            filechooserdialog.destroy()

    def menu_open_activate(self, callback = None) :
        filechooserdialog = gtk.FileChooserDialog(_("Open"), None, gtk.FILE_CHOOSER_ACTION_OPEN, (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_OK, gtk.RESPONSE_OK))
        try:
            filt = gtk.FileFilter()
            filt.set_name("XML")
            filt.add_mime_type("text/xml")
            filt.add_pattern("*.xml")
            filechooserdialog.add_filter(filt)
            filechooserdialog.set_current_folder(APP_PATH + XML_DIR)

            if filechooserdialog.run() == gtk.RESPONSE_OK:
                filename = filechooserdialog.get_filename()
                xml = etree.parse(filename)
                self.treestore_from_xml(xml.getroot())
                self.expand_and_select(self.path_to_old_selected)
                self.clear_undo()
                self.current_filename = filename
                self.autorefresh_call()
                self.file_changed = False
                self.auto_refresh.set_active(True)
                self.action()
        finally:
            filechooserdialog.destroy()

    def menu_open_ini_activate(self, callback = None) :
        filechooserdialog = gtk.FileChooserDialog(_("Open"), None, \
			gtk.FILE_CHOOSER_ACTION_OPEN, (gtk.STOCK_CANCEL, \
			gtk.RESPONSE_CANCEL, gtk.STOCK_OK, gtk.RESPONSE_OK))
        try:
            filt = gtk.FileFilter()
            filt.set_name("inifiles")
            filt.add_mime_type("text/xml")
            filt.add_pattern("*.ini")
            filechooserdialog.add_filter(filt)
            filechooserdialog.set_current_folder(APP_PATH + INI_DIR)

            if filechooserdialog.run() == gtk.RESPONSE_OK:
                self.add_feature(filechooserdialog.get_filename())
        finally:
            filechooserdialog.destroy()

    def reload_defaults(self, callback = None) :
        try :
            self.defaults = open(self.default_src).read()
            self.action()
        except :
            mess_dlg(_('Can not reload NGC defaults: %s' % self.default_src))

    def reload_postamble(self, callback = None) :
        try :
            self.post_amble = open(self.post_amble_src).read()
            self.action()
        except :
            mess_dlg(_('Can not reload post amble file: %s' % self.post_amble_src))

    def reload_subroutine(self, *args):
        src = self.selected_feature.get_attr('src')
        self.auto_refresh.set_active(False)
        self.add_feature(src)

    def set_actions_state(self):
        self.actionBuild.set_sensitive(self.treestore.get_iter_root() is not None)

        self.actionDelete.set_sensitive(self.can_delete_duplicate)
        self.actionDuplicate.set_sensitive(self.can_delete_duplicate)
        self.actionMoveUp.set_sensitive(self.can_move_up)
        self.actionMoveDn.set_sensitive(self.can_move_down)
        self.actionAppendItem.set_sensitive(self.can_add_to_group)
        self.actionRemoveItem.set_sensitive(self.can_remove_from_group)
        self.actionCut.set_sensitive(self.can_delete_duplicate)
        self.actionCopy.set_sensitive(self.can_delete_duplicate)

def main():
    window = gtk.Dialog("LinuxCNC Features", None, gtk.DIALOG_MODAL)
    window.set_title(APP_TITLE)
    features = Features()
    features.embedded = False
    window.vbox.add(features)
    window.add_accel_group(features.accelGroup)
    window.connect("destroy", gtk.main_quit)
    window.set_default_size(400, 800)
    return window.run()

if __name__ == "__main__":
    exit(main())
