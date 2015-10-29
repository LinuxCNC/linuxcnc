#!/usr/bin/env python
# coding: utf-8
#
# Copyright (c) 2012: 
#	Nick Drobchenko - Nick from both cnc-club.ru and linuxcnc.org forums,
#	Fernand Veilleux - FernV from linuxcnc.org forum,
#	Sergey - verser from both cnc-club.ru and linuxcnc.org forums,
#	Meison Kim, Alexander Wigen, Konstantin Navrockiy, Fernand Veilleux. 
#
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

import sys

import pygtk
pygtk.require('2.0')

import gtk
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

###############################################################################
####  SETTINGS : YOU CAN CHANGE FOLLOWING LINES VALUES TO FIT YOUR NEEDS   ####

DEFAULT_CATALOG = 'lathe'  # or could be 'lathe'

# These can be changed to fit your directory structure
# use / at the end but not the beginning
INI_DIR = 'ini/'
XML_DIR = 'xml/'
LIB_DIR = 'lib/'
INC_DIR = 'lib/include/'
NGC_DIR = 'scripts/'
EXAMPLES_DIR = 'xml/examples/'
CATALOGS_DIR = 'catalogs/'
TEMPLATES_DIR = 'xml/templates'
GRAPHICS_DIR = 'graphics/images'

DEFAULT_TEMPLATE = "def_template.xml"

# best options are gtk.ICON_SIZE_MENU, gtk.ICON_SIZE_LARGE_TOOLBAR,
#  gtk.ICON_SIZE_DND and gtk.ICON_SIZE_DIALOG.
GTK_ICON_SIZE = gtk.ICON_SIZE_LARGE_TOOLBAR

# icons and images size should not be less than 22
# default icons size in tree view should be 28
ICON_SIZE = 28
# experience with this to have a menu that is easy to use and compare with choice of GTK_ICON_SIZE
MENU_ICON_SIZE = 32
# image size for add dialog
IMAGE_SIZE = 80

DEFAULT_USE_ADD_DIALOG = True
DEFAULT_ADD_DIALOG_WIDTH = 500
DEFAULT_ADD_DIALOG_HEIGHT = 400

# if False, NO_ICON_FILE will be used
DEFAULT_USE_NO_ICON = False
NO_ICON_FILE = 'no-icon.png'
DEFAULT_ICONS = {
	"enabled": "enable.png"
}


MAX_TOP_FEATURES = 15
SUPER_TOP_FEATURES_COUNT = 10  # must be less than MAX_TOP_FEATURES

# if False, will have only 1 treeview
DEFAULT_USE_2_VIEWS = True
DEFAULT_SIDE_BY_SIDE = False

# if False, will have 2 treeview
# else will have a box with checkboxes, spinbuttons, comboboxes, etc...
DEFAULT_USE_WIDGETS = False

DEFAULT_DIGITS = '3'

####  END SETTINGS, CHANGES AFTER THIS LINE WILL AFFECT APPLICATION   ##########
################################################################################

# About Dialog strings
APP_TITLE = "LinuxCNC-Features"
APP_VERSION = "2.0"
APP_COPYRIGHT = 'Copyright © 2012 Nick Drobchenko aka Nick from cnc-club.ru'
APP_AUTHORS = ['Nick Drobchenko', 'Meison Kim', 'Alexander Wigen', 'Konstantin Navrockiy', 'Fernand Veilleux']
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

NO_ICON_TYPES = ['hdr', 'header']
XML_TAG = "LinuxCNC-Features"
PREFERENCES_FILE = "features.conf"
TOP_FEATURES = "top.lst"
DEFAULTS = 'defaults.ngc'

HOME_PAGE = 'http://fernv.github.io/linuxcnc-features/'

SEL_IS_NONE = 0
SEL_IS_FEATURE = 1
SEL_IS_ITEMS = 2
SEL_IS_PARAM = 3

DEFAULT_REFRESH_TIME_OUT = 1000
WIDTH_REQUEST = 425
UNDO_MAX_LEN = 200
MIN_NAME_COL_WIDTH = 175

INCLUDE = []
DEFINITIONS = []
PIXBUF_DICT = {}
FEATURE_DICT = {}

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
    icon = APP_PATH + GRAPHICS_DIR +"/" + icon

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
        self.values = ''
        self.digits = '3'
        self.edit_combo_values = ''
        self.param_value = ''
        self.combo_values = []

    def set_max_value(self, value):
        self.max_value = value

    def set_param_value(self, value):
        self.param_value = value

    def set_min_value(self, value):
        self.min_value = value

    def set_data_type(self, value):
        self.data_type = value

    def set_options(self, value):
        self.options = value

    def set_digits(self, value):
        self.digits = value

    def do_get_property(self, pspec):
        return getattr(self, pspec.name)

    def do_set_property(self, pspec, value):
        setattr(self, pspec.name, value)

    def set_treeview(self, value):
        self.tv = value

    def do_get_size(self, widget, cell_area):
        size_tuple = gtk.CellRendererText.do_get_size(self, widget, cell_area)
        return(size_tuple)

    def do_start_editing(self, event, treeview, path, background_area, \
                        cell_area, flags):

        if not self.get_property('editable'):
            return None

        if self.data_type in ['header', 'hdr', 'items'] :
            return None

        elif self.data_type == 'float' :
            spin = gtk.SpinButton(digits = get_int(self.digits))
            spin.get_adjustment().configure(value = get_float(self.param_value), \
                        lower = self.min_value, upper = self.max_value, \
                        step_increment = 0.1, page_increment = 1.0, page_size = 0)
            spin.connect('value-changed', self.float_value_changed, path, \
                        self.digits)
            spin.connect('key-press-event', self.spin_key_press_event, path)
            spin.show()
            spin.grab_focus()
            return spin

        elif self.data_type == 'int' :
            spinInt = gtk.SpinButton()
            spinInt.get_adjustment().configure(value = get_int(self.param_value), \
                        lower = self.min_value, upper = self.max_value, \
                        step_increment = 1, page_increment = 10, page_size = 0)
            spinInt.set_numeric(True)
            spinInt.connect('value-changed', self.int_value_changed, path)
            spinInt.connect('key-press-event', self.spinint_key_press_event, path)
            spinInt.show()
            spinInt.grab_focus()
            return spinInt

        elif self.data_type in ['bool', 'boolean']:
            if get_int(self.param_value) == 0 :
                self.emit('edited', path, '1')
            else :
                self.emit('edited', path, '0')
            return None

        elif self.data_type == 'combo':
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

        elif self.data_type == 'string':
            return gtk.CellRendererText.do_start_editing(self, event, \
                        treeview, path, background_area, cell_area, flags)

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
            self.textbuffer.set_property('text', self.get_property('text'))

            # map key-press-event handler
            self.textedit_window.connect('key-press-event', self._keyhandler)
            self.textedit_window.connect('focus-out-event', self._focus_out, path)

            scrolled_window = gtk.ScrolledWindow()
            scrolled_window.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)

            scrolled_window.add(self.textedit)
            self.textedit_window.vbox.add(scrolled_window)
            self.textedit_window.show_all()

            # position the popup below the edited cell (and try hard to keep the popup within the toplevel window)
            (tree_x, tree_y) = treeview.get_bin_window().get_origin()
            (tree_w, tree_h) = treeview.window.get_geometry()[2:4]
            (t_w, t_h) = self.textedit_window.window.get_geometry()[2:4]
            x = tree_x + min(cell_area.x, tree_w - t_w + treeview.get_visible_rect().x)
            y = tree_y + min(cell_area.y, tree_h - t_h + treeview.get_visible_rect().y)
            self.textedit_window.move(x, y)
            self.textedit_window.resize(cell_area.width, cell_area.height)

            if self.textedit_window.run() == gtk.RESPONSE_OK:
                (iter_first, iter_last) = self.textbuffer.get_bounds()
                text = self.textbuffer.get_text(iter_first, iter_last)
                self.textedit_window.destroy()
                self.emit('edited', path, text)
            else:
                self.textedit_window.destroy()

            return None

    def do_render(self, window, widget, background_area, cell_area, expose_area, flags):
        if self.data_type == 'bool' :
            chk = gtk.CellRendererToggle()
            chk.set_active(get_int(self.param_value) != 0)
            chk.render(window, widget, background_area, cell_area, expose_area, flags)
        else :
            gtk.CellRendererText.do_render(self, window, widget, background_area, \
                                    cell_area, expose_area, flags)

    def entry_editing_done(self, entry, path):
        self.emit('edited', path, entry.get_text())

    def int_value_changed(self, spin, path):
        self.emit('edited', path, str(int(spin.get_value())))

    def combo_changed(self, combo, path):
        newtext = self.combo_values[combo.get_active()]
        self.emit('edited', path, newtext)

    def float_value_changed(self, spin, *args):
        fmt = '{0:0.' + args[1] + 'f}'
        newval = fmt.format(spin.get_adjustment().get_value())
        self.emit('edited', args[0], newval)

    def spin_key_press_event(self, spin, event, path):
        if event.type == gtk.gdk.KEY_PRESS:
            if gtk.gdk.keyval_name(event.keyval) == 'Up':
                spin.spin(gtk.SPIN_STEP_FORWARD, 0.1)
                spin.grab_focus()
                return True
            if gtk.gdk.keyval_name(event.keyval) == 'Down':
                spin.spin(gtk.SPIN_STEP_BACKWARD, 0.1)
                spin.grab_focus()
                return True
            if gtk.gdk.keyval_name(event.keyval) == 'Page_Up':
                spin.spin(gtk.SPIN_STEP_FORWARD, 1)
                spin.grab_focus()
                return True
            if gtk.gdk.keyval_name(event.keyval) == 'Page_Down':
                spin.spin(gtk.SPIN_STEP_BACKWARD, 1)
                spin.grab_focus()
                return True
            if gtk.gdk.keyval_name(event.keyval) in ["Return", "KP_Enter"] :
                self.tv.grab_focus()

    def spinint_key_press_event(self, spin, event, path):
        if event.type == gtk.gdk.KEY_PRESS:
            if gtk.gdk.keyval_name(event.keyval) == 'Up':
                spin.spin(gtk.SPIN_STEP_FORWARD, 1)
                spin.grab_focus()
                return True
            if gtk.gdk.keyval_name(event.keyval) == 'Down':
                spin.spin(gtk.SPIN_STEP_BACKWARD, 1)
                spin.grab_focus()
                return True
            if gtk.gdk.keyval_name(event.keyval) == 'Page_Up':
                spin.spin(gtk.SPIN_STEP_FORWARD, 10)
                spin.grab_focus()
                return True
            if gtk.gdk.keyval_name(event.keyval) == 'Page_Down':
                spin.spin(gtk.SPIN_STEP_BACKWARD, 10)
                spin.grab_focus()
                return True
            if gtk.gdk.keyval_name(event.keyval) in ["Return", "KP_Enter"] :
                self.tv.grab_focus()

    def _focus_out(self, widget, event, path):
        (iter_first, iter_last) = self.textbuffer.get_bounds()
        text = self.textbuffer.get_text(iter_first, iter_last)
        self.textedit_window.destroy()
        self.emit('edited', path, text)

    def _keyhandler(self, widget, event):
        keyname = gtk.gdk.keyval_name(event.keyval)
        if event.state & (gtk.gdk.SHIFT_MASK | gtk.gdk.CONTROL_MASK) and \
                gtk.gdk.keyval_name(event.keyval) in ['Return', 'KP_Enter']:
            self.textedit_window.response(gtk.RESPONSE_OK)
            return True

gobject.type_register(CellRendererMx)


class Parameter() :
    def __init__(self, ini = None, ini_id = None, xml = None) :
        self.attr = {}
        self.pixbuf = {}
        if ini != None :
            self.from_ini(ini, ini_id)
        elif xml != None :
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
    	if icon == None or icon == "" :
    		if self.get_name().lower() in DEFAULT_ICONS: 
    			icon = DEFAULT_ICONS[self.get_name().lower()]
        return get_pixbuf(icon, ICON_SIZE)

    def get_image(self) :
        return get_pixbuf(self.get_attr("image"), IMAGE_SIZE)

    def get_value(self) :
        return self.attr["value"] if "value" in self.attr else ""

    def get_name(self) :
        return self.attr["name"] if "name" in self.attr else ""

    def get_type(self):
        return self.attr["type"] if "type" in self.attr else "string"

    def get_attr(self, name) :
        return self.attr[name] if name in self.attr else None

    def get_options(self):
        return self.attr["options"] if "options" in self.attr else "No options"

    def get_digits(self):
        return self.attr["digits"] if "digits" in self.attr else DEFAULT_DIGITS

    def get_min_value(self):
        return self.attr["minimum_value"] if "minimum_value" in self.attr else "-99999.9"

    def get_max_value(self):
        return self.attr["maximum_value"] if "maximum_value" in self.attr else "99999.9"

class Feature():
    def __init__(self, src = None, xml = None) :
        self.attr = {}
        self.pixbuf = {}
        self.param = []
        if src != None :
            self.from_src(src)
        if xml != None :
            self.from_xml(xml)

    def __repr__(self) :
        return etree.tostring(self.to_xml(), pretty_print = True)

    def get_icon(self) :
        return get_pixbuf(self.get_attr("icon"), ICON_SIZE)

    def get_image(self) :
        return get_pixbuf(self.get_attr("image"), IMAGE_SIZE)

    def get_value(self):
        return self.attr["value"] if "value" in self.attr else ""

    def get_type(self):
        return self.attr["type"] if "type" in self.attr else "string"

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
        self.attr["order"] = [s if s[:6] == "PARAM_" else "PARAM_" + s for s in self.attr["order"]]

        # get params
        self.param = []
        parameters = self.attr["order"] + [p for p in conf if (p[:6] == "PARAM_" and p not in self.attr["order"])]
        for s in parameters :
            if s in conf :
                p = Parameter(ini = conf[s], ini_id = s.lower())
                p.attr['name'] = _(p.get_name())
                p.attr['tool_tip'] = _(p.get_attr('tool_tip'))
                p.attr['options'] = _(p.get_options())
                if p.get_type() == 'float' :
                    fmt = '{0:0.' + p.get_digits() + 'f}'
                    p.attr['value'] = fmt.format(get_float(p.get_value()))
                elif p.get_type() == 'int' :
                    p.attr['value'] = str(get_int(p.get_value()))
                self.param.append(p)

        # get gcode parameters
        for l in ["definitions", "before", "call", "after"] :
            l = l.upper()
            if l in conf and "content" in conf[l] :
                self.attr[l.lower()] = re.sub(r"(?m)\r?\n\r?\.", "\n", conf[l]["content"])
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
        if xml != None :
            # get smallest free name
            l = xml.findall(".//feature[@type='%s']" % self.attr["type"])
            num = max([get_int(i.get("name")[-3:]) for i in l ] + [0]) + 1

        if not "o-name" in self.attr :
            self.attr["o-name"] = self.attr["name"]

        self.attr["name"] = self.attr["o-name"] + " %03d" % num
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

        def import_callback(m) :
            fname = m.group(2)
            if fname != None :
                try :
                    return str(open(fname).read())
                except :
                    StatusBar.push(SB_File_context_id, _("File not found : %s") % fname)
            else :
                mess_dlg(_("File not found : %(f)s in %(p)s" % {"f":fname, "p":(XML_DIR)}))
                raise IOError("IOError File not found : %(f)s in %(p)s" % {"f":fname, "p":(XML_DIR)})

        s = re.sub(r"(?i)(<import>(.*?)</import>)", import_callback, s)
        s = re.sub(r"(?i)(<eval>(.*?)</eval>)", eval_callback, s)
        s = re.sub(r"(?ims)(<exec>(.*?)</exec>)", exec_callback, s)

        for p in self.param :
            if "call" in p.attr and "value" in p.attr :
                s = re.sub(r"%s([^A-Za-z0-9_]|$)" % (re.escape(p.attr["call"])), r"%s\1" % (p.attr["value"]), s)

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
        global APP_PATH, DEFAULT_CATALOG

        # process passed args
        opt, optl = 'U:c:x:i', ["catalog=", "ini="]
        optlist, args = getopt.getopt(sys.argv[1:], opt, optl)
        optlist = dict(optlist)

        if "-U" in optlist :
            optlist_, args = getopt.getopt(optlist["-U"].split(), opt, optl)
            optlist.update(optlist_)

        ini = os.getenv("INI_FILE_NAME")
        if "-i" in optlist :
            ini = optlist["-i"]
        elif "--ini" in optlist :
            ini = optlist["--ini"]

        APP_PATH = os.getcwd() + '/'
        if (ini is not None):
            try :
                inifile = linuxcnc.ini(ini)
                try :
                    val = inifile.find('DISPLAY', 'FEATURES_PATH')
                    APP_PATH = val + '/'
                except :
                    print("Warning! There's no FEATURES_PATH in ini file")

                try :
                    inifile.find('RS274NGC', 'SUBROUTINE_PATH')
                except :
                    print(_("Warning! There's no SUBROUTINES_PATH in ini file!"))
            except:
                mess_dlg(_('Can not read LinuxCNC ini file'))

        if "--catalog" in optlist :
            self.catalog_dir = APP_PATH + CATALOGS_DIR + optlist["--catalog"] + '/'
        else :
            self.catalog_dir = APP_PATH + CATALOGS_DIR + DEFAULT_CATALOG + '/'

        if not os.path.exists(self.catalog_dir + 'menu.xml') :
            mess_dlg(_('Catalog file does not exists : %s') % self.catalog_dir + 'menu.xml')
            IOError('IOError Catalog file does not exists : %s' % self.catalog_dir + 'menu.xml')

        xml = etree.parse(self.catalog_dir + 'menu.xml')
        self.catalog = xml.getroot()

        self.LinuxCNC_connected = False
        self.treestore_selected = None
        self.selected_feature = None
        self.selected_feature_path = None
        self.iter_selected_type = SEL_IS_NONE
        self.use_dual_views = DEFAULT_USE_2_VIEWS
        self.use_widget_box = DEFAULT_USE_WIDGETS
        self.side_by_side = DEFAULT_SIDE_BY_SIDE
        self.treeview2 = None

        self.embedded = True
        self.undo_list = []
        self.undo_pointer = -1
        self.timeout = None

        # create utilities and main_window
        gtk.VBox.__init__(self, *a, **kw)

        self.builder = gtk.Builder()
        try :
            self.builder.add_from_file(APP_PATH + "features.glade")
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

        self.treestore = gtk.TreeStore(object, str, gobject.TYPE_BOOLEAN, gobject.TYPE_BOOLEAN)
        self.master_filter = self.treestore.filter_new()
        self.widgets_viewport = None

        self.details_filter = self.treestore.filter_new()
        self.details_filter.set_visible_column(3)

        self.create_treeview()
        self.main_box.reparent(self)

        self.topfeatures_tb = gtk.Toolbar()
        self.main_box.pack_start(self.topfeatures_tb, False, False, 0)
        self.main_box.reorder_child(self.topfeatures_tb, 1)

        self.default_src = None
        self.create_menu_interface()

        if self.default_src == None :
            self.default_src = self.catalog_dir + DEFAULTS
        try :
            self.defaults = open(self.default_src).read()
        except :
            StatusBar.push(3, _('Can not load NGC default file : %s') % self.default_src)
            mess_dlg(_('Can not load NGC default file : %s') % self.default_src)

        self.setup_top_features()
        if DEFAULT_USE_ADD_DIALOG :
            self.create_add_dialog()

        self.use_widget_menu.set_active(self.use_widget_box)

        self.single_v_menu.set_active(not self.use_dual_views)
        self.dual_v_menu.set_active(self.use_dual_views)

        self.builder.connect_signals(self)

        self.menu_new_activate(self)
        self.get_selected_feature(self)
        self.show_all()

        if self.side_by_side :
            self.side_layout_activate(None)
        else :
            self.top_bottom_layout_activate(None)
        self.menu_v_activate(None, self.use_dual_views)
        self.treeview.grab_focus()


    def create_menu_interface(self):
        menu_bar = gtk.MenuBar()

        file_menu = gtk.Menu()

        menu_new = gtk.ImageMenuItem(_('_New'))
        img = gtk.Image()
        img.set_from_stock('gtk-new', GTK_ICON_SIZE)
        menu_new.set_image(img)
        menu_new.connect("activate", self.menu_new_activate)
        file_menu.append(menu_new)

        menu_open = gtk.ImageMenuItem(_('_Open'))
        img = gtk.Image()
        img.set_from_stock('gtk-open', GTK_ICON_SIZE)
        menu_open.set_image(img)
        menu_open.connect("activate", self.menu_open_activate)
        file_menu.append(menu_open)

        self.menu_save_xml = gtk.ImageMenuItem(_('_Save'))
        img = gtk.image_new_from_icon_name('gtk-save', GTK_ICON_SIZE)
        self.menu_save_xml.set_image(img)
        self.menu_save_xml.connect("activate", self.menu_save_xml_activate)
        file_menu.append(self.menu_save_xml)

        self.menu_save_template = gtk.ImageMenuItem(_('Save as Default Template'))
        img = gtk.image_new_from_icon_name('gtk-save', GTK_ICON_SIZE)
        self.menu_save_template.set_image(img)
        self.menu_save_template.connect("activate", self.menu_save_default_activate)
        file_menu.append(self.menu_save_template)

        sep = gtk.SeparatorMenuItem()
        file_menu.append(sep)

        self.menu_save_ngc = gtk.ImageMenuItem(_('_Export as RS274NGC'))
        img = gtk.Image()
        img.set_from_stock('gtk-save-as', GTK_ICON_SIZE)
        self.menu_save_ngc.set_image(img)
        self.menu_save_ngc.connect("activate", self.menu_save_ngc_activate)
        file_menu.append(self.menu_save_ngc)

        f_menu = gtk.MenuItem(_("_File"))
        f_menu.connect("activate", self.menu_file_activate)
        f_menu.set_submenu(file_menu)
        menu_bar.append(f_menu)

        v_menu = gtk.Menu()

        view_menu = gtk.MenuItem(_("_View"))
        view_menu.connect("activate", self.menu_view_activate)
        view_menu.set_submenu(v_menu)

        self.single_v_menu = gtk.RadioMenuItem(None, _("Singl_e View"))
        self.single_v_menu.connect("activate", self.menu_v_activate, False)
        v_menu.append(self.single_v_menu)

        self.dual_v_menu = gtk.RadioMenuItem(self.single_v_menu, _("Split _Features/Params Views"))
        self.dual_v_menu.connect("activate", self.menu_v_activate, True)
        v_menu.append(self.dual_v_menu)

        self.hide_val_col_mnu = gtk.CheckMenuItem(_("_Hide Features Value Column"))
        self.hide_val_col_mnu.connect("toggled", self.menu_hide_col_activate)
        v_menu.append(self.hide_val_col_mnu)

        sep = gtk.SeparatorMenuItem()
        v_menu.append(sep)

        self.top_bottom_menu = gtk.RadioMenuItem(None, _("_Top / Bottom Layout"))
        self.top_bottom_menu.set_active(not self.side_by_side)
        self.top_bottom_menu.connect("activate", self.top_bottom_layout_activate)
        v_menu.append(self.top_bottom_menu)

        self.side2side_menu = gtk.RadioMenuItem(self.top_bottom_menu, _("_Side By Side Layout"))
        self.side2side_menu.set_active(self.side_by_side)
        self.side2side_menu.connect("activate", self.side_layout_activate)
        v_menu.append(self.side2side_menu)

        sep = gtk.SeparatorMenuItem()
        v_menu.append(sep)

        self.use_widget_menu = gtk.CheckMenuItem(_("_Use Widgets Box"))
        self.use_widget_menu.connect("toggled", self.menu_use_widget_activate)
        v_menu.append(self.use_widget_menu)

        self.show_headers_menu = gtk.CheckMenuItem(_("Sho_w Headers"))
        self.show_headers_menu.set_active(self.show_headers)
        self.show_headers_menu.connect("toggled", self.show_headers_menu_toggled)
        v_menu.append(self.show_headers_menu)

        sep = gtk.SeparatorMenuItem()
        v_menu.append(sep)

        self.save_layout_menu = gtk.ImageMenuItem(_("Save As Default _Layout"))
        img = gtk.image_new_from_icon_name('gtk-save', GTK_ICON_SIZE)
        self.save_layout_menu.set_image(img)
        self.save_layout_menu.connect("activate", self.save_preferences)
        v_menu.append(self.save_layout_menu)

        menu_bar.append(view_menu)

        self.menuAdd = gtk.Menu()
        add_menu = gtk.MenuItem(_('_Add'))
        add_menu.set_submenu(self.menuAdd)

        menu_bar.append(add_menu)
        self.add_catalog_items()

        menu_utils = gtk.Menu()
        u_menu = gtk.MenuItem(_('_Utilities'))
        u_menu.connect('activate', self.menu_util_activate)
        u_menu.set_submenu(menu_utils)

        self.auto_refresh = gtk.CheckMenuItem(_('_Auto-Refresh'))
        menu_utils.append(self.auto_refresh)

        menu_edit_def = gtk.ImageMenuItem(_('_Edit Defaults'))
        img = gtk.Image()
        img.set_from_stock('gtk-edit', GTK_ICON_SIZE)
        menu_edit_def.set_image(img)
        menu_edit_def.connect("activate", self.menu_edit_defaults_activate)
        menu_utils.append(menu_edit_def)

        menu_reload_def = gtk.ImageMenuItem(_('_Reload Defaults'))
        img = gtk.Image()
        img.set_from_stock('gtk-refresh', GTK_ICON_SIZE)
        menu_reload_def.set_image(img)
        menu_reload_def.connect("activate", self.reload_defaults_activate)
        menu_utils.append(menu_reload_def)

        self.menu_edit_feature = gtk.ImageMenuItem(_('Edit Current _Feature'))
        img = gtk.Image()
        img.set_from_stock('gtk-edit', GTK_ICON_SIZE)
        self.menu_edit_feature.set_image(img)
        self.menu_edit_feature.connect("activate", self.menu_edit_feature_activate)
        menu_utils.append(self.menu_edit_feature)

        self.menu_reload_feature = gtk.ImageMenuItem(_('Reload _Current Feature'))
        img = gtk.Image()
        img.set_from_stock('gtk-refresh', GTK_ICON_SIZE)
        self.menu_reload_feature.set_image(img)
        self.menu_reload_feature.connect("activate", self.reload_feature_activate)
        menu_utils.append(self.menu_reload_feature)

        sep = gtk.SeparatorMenuItem()
        menu_utils.append(sep)

        menu_config = gtk.ImageMenuItem(_('_Preferences'))
        img = gtk.Image()
        img.set_from_stock('gtk-preferences', GTK_ICON_SIZE)
        menu_config.set_image(img)
        menu_config.connect("activate", self.menu_pref_activate)
        menu_utils.append(menu_config)

        menu_translate_app = gtk.ImageMenuItem(_('_Get Strings For Translation'))
        img = gtk.Image()
        img.set_from_stock('gtk-convert', GTK_ICON_SIZE)
        menu_translate_app.set_image(img)
        menu_translate_app.connect("activate", self.get_translations)
        menu_utils.append(menu_translate_app)

        menu_bar.append(u_menu)

        menu_help = gtk.Menu()
        h_menu = gtk.MenuItem(_('_Help'))
        h_menu.connect("activate", self.menu_help_activate)
        h_menu.set_submenu(menu_help)

        self.menu_context_help = gtk.ImageMenuItem(_('_Context Help'))
        img = gtk.Image()
        img.set_from_stock('gtk-help', GTK_ICON_SIZE)
        self.menu_context_help.set_image(img)
        self.menu_context_help.connect("activate", self.menu_context_activate)
        menu_help.append(self.menu_context_help)

        menu_general_help = gtk.ImageMenuItem(_('_Home Page'))
        img = gtk.Image()
        img.set_from_stock('gtk-help', GTK_ICON_SIZE)
        menu_general_help.set_image(img)
        menu_general_help.connect("activate", self.menu_html_gen_activate)
        menu_help.append(menu_general_help)

        sep = gtk.SeparatorMenuItem()
        menu_help.append(sep)

        menu_linuxcnc_home = gtk.ImageMenuItem(_('LinuxCNC Home'))
        img = gtk.Image()
        img.set_from_stock('gtk-help', GTK_ICON_SIZE)
        menu_linuxcnc_home.set_image(img)
        menu_linuxcnc_home.connect("activate", self.menu_html_CNC_activate)
        menu_help.append(menu_linuxcnc_home)

        menu_linuxcnc_forum = gtk.ImageMenuItem(_('LinuxCNC Forum'))
        img = gtk.Image()
        img.set_from_stock('gtk-help', GTK_ICON_SIZE)
        menu_linuxcnc_forum.set_image(img)
        menu_linuxcnc_forum.connect("activate", self.menu_html_FORUM_activate)
        menu_help.append(menu_linuxcnc_forum)

        menu_cnc_russia = gtk.ImageMenuItem(_('CNC-Club Russia'))
        img = gtk.Image()
        img.set_from_stock('gtk-help', GTK_ICON_SIZE)
        menu_cnc_russia.set_image(img)
        menu_cnc_russia.connect("activate", self.menu_html_RU_activate)
        menu_help.append(menu_cnc_russia)

        sep = gtk.SeparatorMenuItem()
        menu_help.append(sep)

        menu_about = gtk.ImageMenuItem(_('_About'))
        img = gtk.Image()
        img.set_from_stock('gtk-about', GTK_ICON_SIZE)
        menu_about.set_image(img)
        menu_about.connect("activate", self.menu_about_activate)
        menu_help.append(menu_about)

        menu_bar.append(h_menu)

        self.main_box.pack_start(menu_bar, False, False, 0)
        self.main_box.reorder_child(menu_bar, 0)

    def menu_help_activate(self, *args):
        self.menu_context_help.set_sensitive((self.selected_feature is not None) and \
                (self.selected_feature.get_attr('html_help') is not None))

    def menu_util_activate(self, *args):
        self.menu_edit_feature.set_sensitive(self.selected_feature is not None)
        self.menu_reload_feature.set_sensitive(self.selected_feature is not None)

    def menu_html_gen_activate(self, *args):
        webbrowser.open(HOME_PAGE)

    def menu_html_CNC_activate(self, *args):
        webbrowser.open('http://www.linuxcnc.org')

    def menu_html_FORUM_activate(self, *args):
        webbrowser.open('http://www.linuxcnc.org/index.php/english/forum/40-subroutines-and-ngcgui')

    def menu_html_RU_activate(self, *args):
        webbrowser.open('www.cnc-club.ru')

    def menu_edit_defaults_activate(self, *args):
        webbrowser.open(self.default_src)

    def menu_edit_feature_activate(self, *args):
        webbrowser.open(self.selected_feature.get_attr('src'))

    def menu_context_activate(self, *args):
        webbrowser.open(self.selected_feature.get_attr('html_help'), new = 2)

    def menu_view_activate(self, *args):
        self.top_bottom_menu.set_sensitive(self.dual_v_menu.get_active())
        self.side2side_menu.set_sensitive(self.dual_v_menu.get_active())
        self.use_widget_menu.set_sensitive(self.dual_v_menu.get_active())

    def create_add_dialog(self):
        add_button = gtk.ToolButton(stock_id = "gtk-add")
        add_button.connect("clicked", self.add_button_clicked)
        self.topfeatures_tb.insert(add_button, 0)
        self.topfeatures_tb.insert(gtk.SeparatorToolItem(), 1)

        self.add_iconview = gtk.IconView()
        self.icon_store = gtk.ListStore(gtk.gdk.Pixbuf, str, str, str, int, str)
        self.add_iconview.set_model(self.icon_store)
        self.add_iconview.set_pixbuf_column(0)
        self.add_iconview.set_text_column(2)
        self.add_iconview.set_tooltip_column(5)
        self.add_iconview.connect("item-activated", self.catalog_activate)

        self.catalog_path = self.catalog
        self.update_catalog(xml = self.catalog_path)

        parent = self.main_box.get_parent().get_parent()
        self.add_dialog = gtk.Dialog(_("Add Feature"), parent, gtk.RESPONSE_CANCEL or gtk.DIALOG_MODAL, (gtk.STOCK_CLOSE, gtk.RESPONSE_REJECT))
        self.add_dialog.set_transient_for(parent)

        scroll = gtk.ScrolledWindow()
        scroll.add(self.add_iconview)
        self.add_dialog.vbox.pack_start(scroll)

        hbox = gtk.HBox()
        button = gtk.Button(_("Catalog Root"))
        button.connect("clicked", self.update_catalog, self.catalog)
        hbox.pack_start(button)

        button = gtk.Button(_("Upper Level"))
        button.connect("clicked", self.update_catalog, "parent")
        hbox.pack_start(button)

        self.add_dialog.vbox.pack_start(hbox, False)
        self.add_dialog.resize(DEFAULT_ADD_DIALOG_WIDTH, DEFAULT_ADD_DIALOG_HEIGHT)

        self.add_dialog.show_all()
        self.add_dialog.hide()

    def catalog_activate(self, iconview, path) :
        itr = self.icon_store.get_iter(path)
        src = self.icon_store.get(itr, 3)[0]
        tag = self.icon_store.get(itr, 1)[0]
        if tag == "parent" :
            self.update_catalog(xml = "parent")
        elif tag in ["sub", "import"] :
            self.add_dialog.hide()
            self.add_feature(src)
        elif tag == "group" :
            path = self.icon_store.get(itr, 4)[0]
            self.update_catalog(xml = self.catalog_path[path])

    def update_catalog(self, call = None, xml = None) :
        if xml == "parent" :
            self.catalog_path = self.catalog_path.getparent()
        else :
            self.catalog_path = xml
        if self.catalog_path == None :
            self.catalog_path = self.catalog
        self.icon_store.clear()

        # add link to upper level
        if self.catalog_path != self.catalog :
            self.icon_store.append([get_pixbuf("upper-level.png", IMAGE_SIZE), "parent", "", "parent", 0, None])

        for path in range(len(self.catalog_path)) :
            p = self.catalog_path[path]
            if p.tag.lower() in ["group", "sub", "import"] :
                name = _(p.get("name")) if "name" in p.keys() else None
                src = p.get("src") if "src" in p.keys() else None
                tooltip = p.get("tool_tip") if "tool_tip" in p.keys() else None
                self.icon_store.append([get_pixbuf(p.get("icon"), IMAGE_SIZE), p.tag.lower(), name, src, path, tooltip])


    def add_button_clicked(self, *arg) :
        self.add_dialog.run()
        self.add_dialog.hide()

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
                icon.set_from_pixbuf(get_pixbuf(f.get_attr("icon"), ICON_SIZE))
                button = gtk.ToolButton(icon, label = f.get_attr("name"))
                button.set_tooltip_markup(f.get_attr("help"))
                button.connect("clicked", self.topfeatures_click, src_file)
                self.topfeatures_buttons[src_file] = button

                icon = gtk.Image()
                icon.set_from_pixbuf(get_pixbuf(f.get_attr("icon"), ICON_SIZE))
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
            # button topbutton i t
            self.topfeatures[src][2] += 1
            self.topfeatures[src][3] += 1

        # clear toolbar
        start_at = 2 if DEFAULT_USE_ADD_DIALOG else 0
        while self.topfeatures_tb.get_n_items() > start_at :
            self.topfeatures_tb.remove(self.topfeatures_tb.get_nth_item(start_at))

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
                topfeatures += "\n%s\t%s\t%s" % (src, self.topfeatures_dict[src][0], self.topfeatures_dict[src][1])

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
                    if (tooltip != None) and (tooltip != ''):
                        a_menu_item.set_tooltip_text(_(tooltip))

                    img = gtk.Image()
                    img.set_from_pixbuf(get_pixbuf(p.get("icon"), MENU_ICON_SIZE))
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
                    if (tooltip != None) and (tooltip != ''):
                        a_menu_item.set_tooltip_text(_(tooltip))

                    img = gtk.Image()
                    img.set_from_pixbuf(get_pixbuf(p.get("icon"), MENU_ICON_SIZE))
                    a_menu_item.set_image(img)

                    src = p.get("src") if "src" in p.keys() else None
                    if (src != None) and (src != ''):
                        a_menu_item.connect("activate", self.add_feature_menu, src)

                    grp_menu.append(a_menu_item)

                elif p.tag.lower() == "defaults" :
                    self.default_src = p.get("src") if "src" in p.keys() else None

        try :
            add_to_menu(self.menuAdd, self.catalog)
        except :
            mess_dlg(_('Problem adding catalog to menu'))

        sep = gtk.SeparatorMenuItem()
        self.menuAdd.append(sep)

        menu_importXML = gtk.ImageMenuItem(_('Import _XML'))
        img = gtk.Image()
        img.set_from_stock('gtk-revert-to-saved', GTK_ICON_SIZE)
        menu_importXML.set_image(img)
        menu_importXML.connect("activate", self.menu_import_activate)
        self.menuAdd.append(menu_importXML)


        menu_item = gtk.ImageMenuItem(_('_Open'))
        img = gtk.Image()
        img.set_from_stock('gtk-open', GTK_ICON_SIZE)
        menu_item.set_image(img)
        menu_item.connect("activate", self.menu_open_ini_activate)
        menu_item.set_tooltip_text(_("Select a valid or prototype ini"))
        self.menuAdd.append(menu_item)

    def create_treeview(self):
        self.treeview.add_events(gtk.gdk.BUTTON_PRESS_MASK)
        self.treeview.connect('button-press-event', self.pop_menu)
        # icon and name
        col = gtk.TreeViewColumn(_("Name"))
        cell = gtk.CellRendererPixbuf()
        col.pack_start(cell, expand = False)
        col.set_cell_data_func(cell, self.get_col_icon)

        cell = gtk.CellRendererText()
        cell.set_property('ellipsize', pango.ELLIPSIZE_END)
        col.pack_start(cell, expand = True)
        col.set_cell_data_func(cell, self.get_col_name)
        col.set_resizable(True)
        col.set_min_width(int(self.col_width_adj.value))
        self.treeview.append_column(col)

        # value
        col = gtk.TreeViewColumn(_("Value"))
        self.col_value = col

        self.edit_cell = CellRendererMx()
        self.edit_cell.set_property("editable", True)
        self.edit_cell.connect('edited', self.edited)
        self.edit_cell.set_treeview(self.treeview)
        col.pack_start(self.edit_cell, expand = True)
        col.set_cell_data_func(self.edit_cell, self.get_col_value)
        col.set_resizable(True)
        col.set_min_width(int(self.col_width_adj.value))
        self.treeview.append_column(col)

        self.treeview.set_tooltip_column(1)
        self.treeview.get_selection().connect("changed", self.get_selected_feature)

        self.treeview.set_model(self.master_filter)
        self.treeview.set_size_request(int(self.col_width_adj.value), 100)

    def pop_view_activate(self, callback = None, *args) :
        self.single_v_menu.set_active(self.single_view_menu.get_active())

    def pop_dual_view_activate(self, callback = None, *args) :
        self.dual_v_menu.set_active(self.dual_view_menu.get_active())

    def pop_menu_hide_col_activate(self, callback = None, *args) :
        self.hide_val_col_mnu.set_active(self.hide_val_col_menu.get_active())

    def pop_top_bottom_layout_activate(self, callback = None, *args) :
        self.top_bottom_menu.set_active(self.top_bottom_pop_menu.get_active())

    def pop_side_layout_activate(self, callback = None, *args) :
        self.side2side_menu.set_active(self.side2side_pop_menu.get_active())

    def pop_show_headers_menu_toggled(self, callback = None, *args) :
        self.show_headers_menu.set_active(self.pop_show_headers_menu.get_active())

    def pop_menu_use_widget_activate(self, callback = None, *args) :
        self.use_widget_menu.set_active(self.use_widget_pop_menu.get_active())

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
                        menu_item.connect("activate", self.pop_chng_dt_activate, itr, 1)
                        sub_menu.append(menu_item)

                    if dt != 'int' :
                        menu_item = gtk.MenuItem(_('Integer'))
                        menu_item.connect("activate", self.pop_chng_dt_activate, itr, 2)
                        sub_menu.append(menu_item)

                    if dt != 'float' :
                        menu_item = gtk.MenuItem(_('Float'))
                        menu_item.connect("activate", self.pop_chng_dt_activate, itr, 3)
                        sub_menu.append(menu_item)

                    if dt == 'float' :
                        menu_digits = gtk.MenuItem(_('Set digits'))
                        self.pop_up.append(menu_digits)
                        sub_menu = gtk.Menu()
                        menu_digits.set_submenu(sub_menu)

                        i = 1
                        while i < 7 :
                            menu_item = gtk.MenuItem(str(i))
                            menu_item.connect("activate", self.pop_set_digits_activate, itr, i)
                            sub_menu.append(menu_item)
                            i = i + 1
                    sep = gtk.SeparatorMenuItem()
                    self.pop_up.append(sep)

                mnu_edit_feature = gtk.ImageMenuItem(_('_Edit Current Feature'))
                img = gtk.Image()
                img.set_from_stock('gtk-edit', GTK_ICON_SIZE)
                mnu_edit_feature.set_image(img)
                mnu_edit_feature.connect("activate", self.menu_edit_feature_activate)
                self.pop_up.append(mnu_edit_feature)

                mnu_reload_feature = gtk.ImageMenuItem(_('Re_load Current Feature'))
                img = gtk.Image()
                img.set_from_stock('gtk-refresh', GTK_ICON_SIZE)
                mnu_reload_feature.set_image(img)
                mnu_reload_feature.connect("activate", self.reload_feature_activate)
                self.pop_up.append(mnu_reload_feature)

                sep = gtk.SeparatorMenuItem()
                self.pop_up.append(sep)

                mnu_duplicate = gtk.ImageMenuItem(_('_Duplicate Current Feature'))
                img = gtk.Image()
                img.set_from_stock('gtk-copy', GTK_ICON_SIZE)
                mnu_duplicate.set_image(img)
                mnu_duplicate.connect("activate", self.btn_duplicate_clicked)
                self.pop_up.append(mnu_duplicate)

                mnu_del_feature = gtk.ImageMenuItem(_('Re_move Current Feature'))
                img = gtk.Image()
                img.set_from_stock('gtk-delete', GTK_ICON_SIZE)
                mnu_del_feature.set_image(img)
                mnu_del_feature.connect("activate", self.btn_delete_clicked)
                self.pop_up.append(mnu_del_feature)

                sep = gtk.SeparatorMenuItem()
                self.pop_up.append(sep)

            self.single_view_menu = gtk.RadioMenuItem(None, _("Singl_e View"))
            self.single_view_menu.set_active(self.single_v_menu.get_active())
            self.single_view_menu.connect("activate", self.pop_view_activate)
            self.pop_up.append(self.single_view_menu)

            self.dual_view_menu = gtk.RadioMenuItem(self.single_view_menu, _("Split _Features/Params Views"))
            self.dual_view_menu.set_active(self.dual_v_menu.get_active())
            self.dual_view_menu.connect("activate", self.pop_dual_view_activate)
            self.pop_up.append(self.dual_view_menu)

            self.hide_val_col_menu = gtk.CheckMenuItem(_("_Hide Features Value Column"))
            self.hide_val_col_menu.set_active(self.hide_val_col_mnu.get_active())
            self.hide_val_col_menu.connect("toggled", self.pop_menu_hide_col_activate)
            self.pop_up.append(self.hide_val_col_menu)

            sep = gtk.SeparatorMenuItem()
            self.pop_up.append(sep)

            self.top_bottom_pop_menu = gtk.RadioMenuItem(None, _("_Top / Bottom Layout"))
            self.top_bottom_pop_menu.set_sensitive(self.dual_v_menu.get_active())
            self.top_bottom_pop_menu.set_active(not self.side_by_side)
            self.top_bottom_pop_menu.connect("activate", self.pop_top_bottom_layout_activate)
            self.pop_up.append(self.top_bottom_pop_menu)

            self.side2side_pop_menu = gtk.RadioMenuItem(self.top_bottom_menu, _("_Side By Side Layout"))
            self.side2side_pop_menu.set_sensitive(self.dual_v_menu.get_active())
            self.side2side_pop_menu.set_active(self.side_by_side)
            self.side2side_pop_menu.connect("activate", self.pop_side_layout_activate)
            self.pop_up.append(self.side2side_pop_menu)

            sep = gtk.SeparatorMenuItem()
            self.pop_up.append(sep)

            self.use_widget_pop_menu = gtk.CheckMenuItem(_("_Use Widgets Box"))
            self.use_widget_pop_menu.set_sensitive(self.dual_v_menu.get_active())
            self.use_widget_pop_menu.set_active(self.use_widget_menu.get_active())
            self.use_widget_pop_menu.connect("toggled", self.pop_menu_use_widget_activate)
            self.pop_up.append(self.use_widget_pop_menu)

            self.pop_show_headers_menu = gtk.CheckMenuItem(_("Sho_w Headers"))
            self.pop_show_headers_menu.set_active(self.show_headers)
            self.pop_show_headers_menu.connect("toggled", self.pop_show_headers_menu_toggled)
            self.pop_up.append(self.pop_show_headers_menu)

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

            data_type = self.details_filter.get_value(itr, 0).get_type()

            if data_type in ['hdr', 'header'] :
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
                    pad = (ICON_SIZE - pbuf.get_width()) / 2
                    if pad < 0:
                        pad = 0
                else :
                    pad = ICON_SIZE / 2

                hbox.pack_start(img, expand = False, padding = pad)
                hbox.pack_start(lbl, expand = True, fill = True)
                tbl.attach(hbox, 0, 1, row_no, row_no + 1)

                val = self.details_filter.get_value(itr, 0).get_value()
                if data_type == 'float':
                    try :
                        f_val = float(val)
                    except :
                        StatusBar.push(SB_Params_context_id, _('Can not convert "%s" to float') % val)
                        data_type = 'string'

                if data_type == 'int':
                    try :
                        i_val = int(val)
                    except :
                        if val != '' :
                            StatusBar.push(SB_Params_context_id, _('Can not convert "%s" to int') % val)
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
                    options = self.details_filter.get_value(itr, 0).get_options()

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

            itr = self.details_filter.iter_next(itr)
            if itr :
                row_no += 1
                tbl.resize(row_no + 1, 2)

        vbox = gtk.VBox()
        vbox.pack_start(tbl, expand = False, fill = False)
        lbl = gtk.Label('')
        vbox.pack_start(lbl, expand = True, fill = True)
        if self.widgets_viewport != None :
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

        # icon and name
        col = gtk.TreeViewColumn(_("Name"))
        cell = gtk.CellRendererPixbuf()
        col.pack_start(cell, expand = False)
        col.set_cell_data_func(cell, self.get_col_icon)

        cell = gtk.CellRendererText()
        cell.set_property('ellipsize', pango.ELLIPSIZE_END)
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


    def get_widgets(self):
        self.main_box = self.builder.get_object("FeaturesBox")
        self.treeview = self.builder.get_object("treeview")
        self.treestore = self.builder.get_object("treestore1")

        self.btn_build = self.builder.get_object("btn_build")
        self.btn_duplicate = self.builder.get_object("btn_duplicate")
        self.btn_delete = self.builder.get_object("btn_delete")
        self.btn_collapse = self.builder.get_object("btn_collapse")

        self.btn_undo = self.builder.get_object("btn_undo")
        self.btn_redo = self.builder.get_object("btn_redo")

        self.btn_move_up = self.builder.get_object("btn_move_up")
        self.btn_move_down = self.builder.get_object("btn_move_down")

        self.btn_add_to_item = self.builder.get_object("btn_add_to_item")
        self.btn_remove_from_item = self.builder.get_object("btn_remove_from_item")

        self.col_width_adj = self.builder.get_object("col_width_adj")
        self.w_adj = self.builder.get_object("width_adj")
        self.timeout_adj = self.builder.get_object("timeout_adj")
        self.digits_adj = self.builder.get_object("adj_digits")
        self.button_tb = self.builder.get_object("toolbar1")

        self.feature_pane = self.builder.get_object("features_pane")
        self.feature_Hpane = self.builder.get_object("hpaned1")

        self.params_scroll = self.builder.get_object("params_scroll")
        self.frame2 = self.builder.get_object("frame2")

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
                    if (tooltip != None) and (tooltip != ''):
                        trslatbl.append((f_name, tooltip))

                    get_menu_strings(p, f_name, trslatbl)

                elif p.tag.lower() == "sub":
                    name = p.get("name") if "name" in p.keys() else None
                    if name :
                        trslatbl.append((f_name, name))

                    tooltip = p.get("tool_tip") if "tool_tip" in p.keys() else None
                    if (tooltip != None) and (tooltip != ''):
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
        old_iter_selected_type = self.iter_selected_type
        (model, itr) = self.treeview.get_selection().get_selected()
        self.btn_collapse.set_sensitive(itr != None)
        if itr :
            ts_itr = model.convert_iter_to_child_iter(itr)
            depth = self.treestore.iter_depth(ts_itr)
            if depth % 2 == 1 :
                if model.get_value(itr, 0).attr.get("type") == "items":
                    self.items_path = model.get_path(itr)
                    self.iter_selected_type = SEL_IS_ITEMS
                    items_ts_path = self.treestore.get_path(ts_itr)
                    ts_itr = self.treestore.iter_parent(ts_itr)
                    self.items_ts_parent_s = self.treestore.get_string_from_iter(ts_itr)
                else :
                    self.iter_selected_type = SEL_IS_PARAM
                itr = model.iter_parent(itr)
            else :
                self.iter_selected_type = SEL_IS_FEATURE

            self.selected_feature_itr = itr
            self.selected_feature = model.get(itr, 0)[0]
            ts_itr = model.convert_iter_to_child_iter(itr)
            self.selected_feature_ts_itr = ts_itr
            self.feature_ts_path = self.treestore.get_path(ts_itr)
            self.selected_feature_ts_path_s = self.treestore.get_string_from_iter(ts_itr)

            self.selected_feature_path = model.get_path(itr)

            self.iter_next = model.iter_next(itr)
            if self.iter_next != None :
                self.can_move_down = True
                s = str(model.get(self.iter_next, 0)[0])
                self.can_add_to_group = (s.find('type="items"') > -1) and \
                        (self.iter_selected_type != SEL_IS_ITEMS)
            else :
                self.can_add_to_group = False
                self.can_move_down = False

            self.selected_feature_parent_itr = model.iter_parent(itr)
            if self.selected_feature_parent_itr != None :
                path_parent = model.get_path(self.selected_feature_parent_itr)
                self.can_remove_from_group = (self.iter_selected_type != SEL_IS_ITEMS) and model.get_value(\
                    self.selected_feature_parent_itr, 0).attr.get("type") == "items"
            else :
                path_parent = None
                self.can_remove_from_group = False

            self.selected_feature_path = model.get_path(itr)
            depth = len(self.selected_feature_path)
            index_s = self.selected_feature_path[depth - 1]
            self.can_move_up = index_s > 0

            if index_s :
                if path_parent == None :
                    path_prior = (index_s - 1,)
                else :
                    path_prior = path_parent[0: ] + (index_s - 1,)
                self.iter_prior = model.get_iter(path_prior)
            else :
                self.iter_prior = None

        else:
            self.iter_selected_type = SEL_IS_NONE
            self.selected_feature = None
            self.can_move_up = False
            self.can_move_down = False
            self.can_add_to_group = False
            self.can_remove_from_group = False

        self.can_delete_duplicate = (self.iter_selected_type == SEL_IS_FEATURE)
        self.set_buttons_state()

        if self.use_dual_views :
            if self.iter_selected_type == SEL_IS_NONE :
                if self.use_widget_box :
                    if self.widgets_viewport != None :
                        self.widgets_viewport.destroy()
                        self.widgets_viewport = None
                elif self.treeview2 :
                    self.treeview2.set_model(None)

            if ((old_selected_feature == self.selected_feature) and \
                (old_iter_selected_type != self.iter_selected_type) and \
                (self.iter_selected_type in [SEL_IS_ITEMS, SEL_IS_FEATURE])) or \
                (old_selected_feature != self.selected_feature) :

                if self.iter_selected_type == SEL_IS_ITEMS :
                    a_filter = self.treestore.filter_new(items_ts_path)
                else :
                    a_filter = self.treestore.filter_new(self.feature_ts_path)
                a_filter.set_visible_column(3)
                self.details_filter = a_filter

                if self.use_widget_box :
                    self.create_widget_box()
                else :
                    self.treeview2.set_model(self.details_filter)

    def btn_add_to_item_clicked(self, call) :
        ts_itr = self.master_filter.convert_iter_to_child_iter(self.iter_next)
        pnext = self.treestore.get_string_from_iter(ts_itr)
        xml = self.treestore_to_xml()
        src = xml.find(".//*[@path='%s']" % self.selected_feature_ts_path_s)
        dst = xml.find(".//*[@path='%s']/param[@type='items']" % pnext)
        if dst != None :
            src.set("selected", "True")
            dst.insert(0, src)
            dst.set("expanded", "True")
            dst = xml.find(".//*[@path='%s']" % pnext)
            dst.set("expanded", "True")
            self.treestore_from_xml(xml)
            self.action(xml)

    def btn_remove_from_item_clicked(self, call) :
        xml = self.treestore_to_xml()
        src = xml.find(".//*[@path='%s']" % self.selected_feature_ts_path_s)
        src.set("selected", "True")
        parent = src.getparent().getparent()
        n = None
        while parent != xml and not (parent.tag == "param" and parent.get("type") == "items") and parent is not None :
            p = parent
            parent = parent.getparent()
            n = parent.index(p)
        if parent is not None and n != None:
            parent.insert(n, src)
            self.treestore_from_xml(xml)
            self.action(xml)

    def btn_duplicate_clicked(self, *arg) :
        self.topfeatures_update(self.selected_feature.get_attr('src'))
        xml = etree.Element(XML_TAG)
        self.treestore_to_xml_recursion(self.selected_feature_ts_itr, xml, False)
        self.import_xml(xml)

    def tv_row_activated(self, tv, path, col) :
        if tv.row_expanded(path) :
            tv.collapse_row(path)
        else:
            tv.expand_row(path, self.use_dual_views)

    def tv_key_pressed_event(self, widget, event) :
        keyname = gtk.gdk.keyval_name(event.keyval)
        selection = self.treeview.get_selection()
        model, pathlist = selection.get_selected_rows()
        path = pathlist[0] if len(pathlist) > 0 else None

        if event.state & gtk.gdk.CONTROL_MASK:
            if keyname in ['z', 'Z'] and (self.btn_undo.get_sensitive()) :
                self.btn_undo_clicked()
                self.treeview.grab_focus()
                return True

            elif keyname == "Up" and self.can_move_up :
                self.move(-1)
                self.treeview.grab_focus()
                return True

            elif keyname == "Down" and self.can_move_down :
                self.move(1)
                self.treeview.grab_focus()
                return True

            elif keyname == "Left" and self.can_remove_from_group :
                self.btn_remove_from_item_clicked(self)
                return True

            elif keyname == "Right" and self.can_add_to_group:
                self.btn_add_to_item_clicked(self)
                return True

            elif keyname == "Delete" and self.can_delete_duplicate :
                self.btn_delete_clicked()
                return True

            elif (keyname == "D" or keyname == "d") and self.can_delete_duplicate :
                self.btn_duplicate_clicked()
                return True

        elif self.embedded:
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

            elif keyname in ["Return", "KP_Enter", "space"] :
                if path :
                    self.treeview.set_cursor_on_cell(path, focus_column = self.col_value, start_editing = True)
                return True

            elif keyname == "Left" :
                self.treeview.collapse_row(path)
                return True

            elif keyname == "Right" :
                self.treeview.expand_row(path, False)
                return True

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
                if path :
                    self.treeview2.set_cursor_on_cell(path, focus_column = self.col_value2, start_editing = True)
                return True

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
                    for p in f.param :
                        if p.get_type() in ['header', 'hdr'] :
                            tool_tip = p.attr['name']
                        else :
                            tool_tip = p.attr["tool_tip"] if "tool_tip" in p.attr else None

                        if self.use_dual_views :
                            if p.get_type() in ['header', 'hdr']:
                                is_visible = self.show_headers
                                m_visible = False
                            else :
                                is_visible = p.get_attr("type") != "items"
                                m_visible = p.get_attr("type") == "items"
                        else :
                            is_visible = False
                            if p.get_type() in ['header', 'hdr']:
                                m_visible = self.show_headers
                            else :
                                m_visible = True

                        piter = treestore.append(citer, [p, tool_tip, m_visible, is_visible])
                        if p.get_attr("type") == "items" :
                            xmlpath_ = xml.find(".//param[@type='items']")
                            recursive(treestore, piter, xmlpath_)

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

        gcode = ""
        gcode_def = ""
        global DEFINITIONS
        DEFINITIONS = []
        global INCLUDE
        INCLUDE = []
        itr = self.treestore.get_iter_root()
        while itr != None :
            g, d = recursive(itr)
            gcode += g
            gcode_def += d
            itr = self.treestore.iter_next(itr)
        return self.defaults + "\n(Definitions)\n" + gcode_def + "(End definitions)\n" + gcode + "\nM2"


    def btn_build_clicked(self, *arg) :
        self.autorefresh_call()
        if not self.LinuxCNC_connected :
            mess_dlg(_("LinuxCNC not running\n\nStart LinuxCNC with the right config ini\nand press Build button again"))
        else :
            self.auto_refresh.set_active(True)


    def menu_save_ngc_activate(self, *arg) :
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
        self.treestore.get(itr, 0)[0].attr["value"] = new_text
        self.action()
        self.treeview.grab_focus()

    def edited_tv2(self, renderer, path, new_text) :
        itr = self.details_filter.get_iter(path)
        itr = self.details_filter.convert_iter_to_child_iter(itr)
        self.treestore.get(itr, 0)[0].attr["value"] = new_text
        self.action()
        self.treeview2.grab_focus()

    def edited_text(self, entry, *arg) :
        itr = self.details_filter.convert_iter_to_child_iter(*arg)
        self.treestore.get(itr, 0)[0].attr["value"] = entry.get_text()
        self.action()

    def edited_combo(self, combo, *arg):
        itr = self.details_filter.convert_iter_to_child_iter(arg[0])
        options = self.treestore.get_value(itr, 0).get_options()
        option = options.split(':')[combo.get_active()]
        self.treestore.get(itr, 0)[0].attr["value"] = option.split('=')[1]
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
        self.treestore.get(itr, 0)[0].attr["value"] = fmt.format(adj.get_value())
        self.action()

    def edited_int(self, adj, *arg) :
        itr = self.details_filter.convert_iter_to_child_iter(arg[0])
        self.treestore.get(itr, 0)[0].attr["value"] = '%d' % adj.get_value()
        self.action()

    def btn_delete_clicked(self, *arg) :
        if self.iter_next :
            next_path = self.master_filter.get_path(self.selected_feature_itr)
        elif self.iter_prior :
            next_path = self.master_filter.get_path(self.iter_prior)
        elif self.selected_feature_parent_itr :
            next_path = self.master_filter.get_path(self.selected_feature_parent_itr)
        else :
            next_path = None

        self.treestore.remove(self.selected_feature_ts_itr)

        if next_path != None :
            self.treeview.set_cursor(next_path)

        self.get_selected_feature(self)
        self.action()

    def top_bottom_layout_activate(self, *arg):
        self.side_by_side = False
        self.frame2.reparent(self.feature_pane)

    def side_layout_activate(self, *arg):
        self.side_by_side = True
        self.frame2.reparent(self.feature_Hpane)

    def btn_collapse_clicked(self, *arg) :
        path = self.selected_feature_path
        self.treeview.collapse_all()
        self.treeview.expand_to_path(path)
        self.treeview.set_cursor(path)
        self.treeview.grab_focus()

    def import_xml(self, xml_) :
        if xml_.tag != XML_TAG:
            xml_ = xml_.find(".//%s" % XML_TAG)

        if xml_ != None :
            xml = self.treestore_to_xml()
            if self.iter_selected_type == SEL_IS_ITEMS :
                # will append to items
                dest = xml.find(".//*[@path='%s']/param[@type='items']" % self.items_ts_parent_s)
                opt = 2
                i = -1
                next_path = self.items_path
                expand = self.use_dual_views
            elif self.iter_selected_type in [SEL_IS_PARAM, SEL_IS_FEATURE] :
                # will append after parent of selected feature
                dest = xml.find(".//*[@path='%s']" % self.selected_feature_ts_path_s)
                parent = dest.getparent()
                i = parent.index(dest)
                opt = 1
                l_path = len(self.selected_feature_path)
                next_path = (self.selected_feature_path[0:l_path - 1] + (self.selected_feature_path[l_path - 1] + 1,))
                expand = not self.use_dual_views
            else :  # None selected
                opt = 0
                next_path = None

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

            if next_path is not None :
                if expand :
                    self.treeview.expand_row(next_path, expand)
                self.treeview.set_cursor(next_path)

            self.get_selected_feature(self)
            self.action(xml)

    def add_feature_menu(self, widget, src):
        self.add_feature(src)

    def add_feature(self, src) :
        src_file = search_path(src)
        if not src_file :
            return

        src_data = open(src_file).read(25)
        xml_search = src_data.find(".//%s" % XML_TAG)
        if xml_search > -1 :
            xml = etree.parse(src_file).getroot()
        else :
            if src_data.find('[SUBROUTINE]') == -1 :
                IOError("'%s' is not a valid file")
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
            stat = linuxcnc.stat()
            stat.poll()
            if stat.interp_state == linuxcnc.INTERP_IDLE :
                linuxCNC.reset_interpreter()
                linuxCNC.mode(linuxcnc.MODE_AUTO)
                linuxCNC.program_open(fname)
                try:
                    subprocess.call(["axis-remote", fname])
                except:
                    pass
                self.LinuxCNC_connected = True
        except :
            self.auto_refresh.set_active(False)
            self.LinuxCNC_connected = False

    def action(self, xml = None) :
        if xml == None :
            xml = self.treestore_to_xml()

        self.undo_list = self.undo_list[:self.undo_pointer + 1]
        self.undo_list = self.undo_list[max(0, len(self.undo_list) - UNDO_MAX_LEN):]
        self.undo_list.append(etree.tostring(xml))
        self.undo_pointer = len(self.undo_list) - 1
        self.update_do_btns()

    def update_do_btns(self):
        self.set_do_buttons_state()
        if self.auto_refresh.get_active() :
            if self.timeout != None :
                gobject.source_remove(self.timeout)
            self.timeout = gobject.timeout_add(int(self.timeout_adj.value * 1000), self.autorefresh_call)

    def btn_undo_clicked(self, *arg) :
        self.undo_pointer -= 1
        self.treestore_from_xml(etree.fromstring(self.undo_list[self.undo_pointer]))
        self.update_do_btns()

    def btn_redo_clicked(self, *arg) :
        self.undo_pointer += 1
        self.treestore_from_xml(etree.fromstring(self.undo_list[self.undo_pointer]))
        self.update_do_btns()

    def set_do_buttons_state(self):
        self.btn_undo.set_sensitive(self.undo_pointer > 0)
        self.btn_redo.set_sensitive(self.undo_pointer < (len(self.undo_list) - 1))

    def clear_undo(self, *arg) :
        self.undo_list = []
        self.undo_pointer = -1
        self.set_do_buttons_state()

    def menu_file_activate(self, *args):
        self.menu_save_ngc.set_sensitive(self.treestore.get_iter_root() != None)
        self.menu_save_xml.set_sensitive(self.treestore.get_iter_root() != None)
        self.menu_save_template.set_sensitive(self.treestore.get_iter_root() != None)

    def menu_about_activate(self, *args):
        dialog = gtk.AboutDialog()
        dialog.set_name(APP_TITLE)
        dialog.set_version(APP_VERSION)
        dialog.set_copyright(APP_COPYRIGHT)
        dialog.set_authors(APP_AUTHORS)
        dialog.set_comments(_(APP_COMMENTS))
        dialog.set_license(_(APP_LICENCE))
        dialog.run()
        dialog.destroy()

    def menu_save_default_activate(self, *args):
        xml = self.treestore_to_xml()
        etree.ElementTree(xml).write(self.catalog_dir + DEFAULT_TEMPLATE, pretty_print = True)

    def menu_new_activate(self, *args):
        self.treestore.clear()
        self.clear_undo()
        if os.path.isfile(self.catalog_dir + DEFAULT_TEMPLATE):
            xml = etree.parse(self.catalog_dir + DEFAULT_TEMPLATE)
            self.treestore_from_xml(xml.getroot())
        self.autorefresh_call()
        self.auto_refresh.set_active(True)
        self.current_filename = _('Untitle.xml')
        self.file_changed = False

    def menu_v_activate(self, callback = None, *args) :
        self.use_dual_views = args[0]

        if self.use_dual_views :
            if self.use_widget_box :
                if self.treeview2 != None :
                    self.treeview2.destroy()
                    self.treeview2 = None
            else :
                if self.widgets_viewport != None :
                    self.widgets_viewport.destroy()
                    self.widgets_viewport = None
                if self.treeview2 == None :
                    self.create_second_treeview()
                    self.treeview2.show_all()
            self.frame2.set_visible(True)
        else :
            self.frame2.set_visible(False)
            if self.treeview2 != None :
                self.treeview2.destroy()
                self.treeview2 = None
            if self.widgets_viewport != None :
                self.widgets_viewport.destroy()
                self.widgets_viewport = None
        self.treestore_from_xml(self.treestore_to_xml())

    def show_headers_menu_toggled(self, *args):
        self.show_headers = self.show_headers_menu.get_active()
        path = self.selected_feature_path
        self.treestore_from_xml(self.treestore_to_xml())
        if (path):
            self.treeview.expand_to_path(path)
            self.treeview.set_cursor(path)
        self.treeview.grab_focus()

    def menu_use_widget_activate(self, callback = None) :
        self.use_widget_box = self.use_widget_menu.get_active()
        if self.use_widget_box :
            if self.treeview2 != None :
                self.treeview2.destroy()
                self.treeview2 = None
        if self.use_dual_views :
            path = self.selected_feature_path
            if not self.use_widget_box :
                if self.widgets_viewport != None :
                    self.widgets_viewport.destroy()
                    self.widgets_viewport = None
                self.create_second_treeview()
                self.treeview2.show_all()
            self.treestore_from_xml(self.treestore_to_xml())
            if path :
                self.treeview.set_cursor(path)
                self.get_selected_feature(self)

    def menu_hide_col_activate(self, callback = None):
        self.col_value.set_visible(not self.hide_val_col_mnu.get_active())


    def menu_pref_activate(self, callback = None) :
        global DEFAULT_DIGITS

        parent = self.main_box.get_parent().get_parent().get_parent()
        prefdlg = gtk.Dialog(_("Preferences"), parent,
            gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
            (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_OK, gtk.RESPONSE_ACCEPT))

        align = gtk.Alignment()
        align.set_padding(4, 4, 8, 8)
        tbl = gtk.Table(3, 2, False)
        tbl.set_col_spacings(6)
        lbl = gtk.Label(_("Name Column Minimum Width"))
        lbl.set_alignment(0.0, 0.5)
        tbl.attach(lbl, 0, 1, 0, 1)
        lbl = gtk.Label(_("Window Minimum Width"))
        lbl.set_alignment(0.0, 0.5)
        tbl.attach(lbl, 0, 1, 1, 2)
        lbl = gtk.Label(_("Auto-refresh timeout"))
        lbl.set_alignment(0.0, 0.5)
        lbl.set_size_request(225, 40)
        tbl.attach(lbl, 0, 1, 2, 3)
        lbl = gtk.Label(_("Default Decimals"))
        lbl.set_alignment(0.0, 0.5)
        lbl.set_size_request(225, 40)
        tbl.attach(lbl, 0, 1, 3, 4)
        scale = gtk.HScale(self.col_width_adj)
        scale.set_digits(0)
        scale.set_size_request(300, 40)
        tbl.attach(scale, 1, 2, 0, 1,)
        scale = gtk.HScale(self.w_adj)
        scale.set_digits(0)
        scale.set_size_request(300, 40)
        tbl.attach(scale, 1, 2, 1, 2,)
        spin = gtk.SpinButton(self.timeout_adj, 0.0, 2)
        tbl.attach(spin, 1, 2, 2, 3,)
        spin = gtk.SpinButton(self.digits_adj)
        tbl.attach(spin, 1, 2, 3, 4,)

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
        config.set('general', 'refresh_time_out', self.timeout_adj.value)
        config.set('general', 'width', self.w_adj.value)
        config.set('general', 'col_width', self.col_width_adj.value)
        config.set('general', 'digits', self.digits_adj.value)
        config.set('general', 'dual_view', self.use_dual_views)
        config.set('general', 'use_widgets', self.use_widget_box)
        config.set('general', 'side_by_side', self.side_by_side)
        config.set('general', 'show_headers', self.show_headers)

        with open(PREFERENCES_FILE, 'wb') as configfile:
            config.write(configfile)


    def read_preferences(self):
        global DEFAULT_REFRESH_TIME_OUT, WIDTH_REQUEST, MIN_NAME_COL_WIDTH, DEFAULT_DIGITS

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

        config = ConfigParser.ConfigParser()
        config.read(PREFERENCES_FILE)
        self.timeout_adj.value = read_float(config, 'refresh_time_out', DEFAULT_REFRESH_TIME_OUT / 1000)
        self.w_adj.value = read_float(config, 'width', WIDTH_REQUEST)
        self.col_width_adj.value = read_float(config, 'col_width', MIN_NAME_COL_WIDTH)
        self.digits_adj.value = read_float(config, 'digits', get_float(DEFAULT_DIGITS))
        DEFAULT_DIGITS = str(int(self.digits_adj.value))
        self.use_dual_views = read_boolean(config, 'dual_view', DEFAULT_USE_2_VIEWS)
        self.use_widget_box = read_boolean(config, 'use_widgets', DEFAULT_USE_WIDGETS)
        self.side_by_side = read_boolean(config, 'side_by_side', DEFAULT_SIDE_BY_SIDE)
        self.show_headers = read_boolean(config, 'show_headers', True)

    def on_scale_change_value(self, widget):
        self.main_box.set_size_request(int(self.w_adj.value), 100)

    def col_width_adj_value_changed(self, widget):
        if self.col_value :
            self.col_value.set_min_width(int(self.col_width_adj.value))
        if self.col_value2 :
            self.col_value2.set_min_width(int(self.col_width_adj.value))

    def btn_move_up_clicked(self, *arg) :
        self.move(-1)

    def btn_move_down_clicked(self, *arg) :
        self.move(1)

    def get_col_name(self, column, cell, model, itr) :
        cell.set_property('markup', model.get_value(itr, 0).get_name())

    def get_col_value(self, column, cell, model, itr) :
        cell.set_param_value(model.get_value(itr, 0).get_value())

        data_type = model.get_value(itr, 0).get_type()
        cell.set_data_type(data_type)

        if data_type == 'combo':
            options = model.get_value(itr, 0).get_options()
            cell.set_options(options)
            for option in options.split(":") :
                opt = option.split('=')
                if opt[1] == model.get_value(itr, 0).get_value() :
                    cell.set_property('text', opt[0])
                    break
        else :
            cell.set_property('text', model.get_value(itr, 0).get_value())

        if data_type in ['float', 'int']:
            cell.set_max_value(get_float(model.get_value(itr, 0).get_max_value()))
            cell.set_min_value(get_float(model.get_value(itr, 0).get_min_value()))
            cell.set_digits(model.get_value(itr, 0).get_digits())

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
                    if xmlpath_ != None:
                        self.treestore_to_xml_recursion(self.treestore.iter_children(citer), xmlpath_)
                citer = self.treestore.iter_next(citer)

            # check for next items
            if allitems :
                itr = self.treestore.iter_next(itr)
            else :
                itr = None

    def treestore_to_xml(self, *arg):
        xml = etree.Element(XML_TAG)
        self.get_expand()
        self.treestore_to_xml_recursion(self.treestore.get_iter_root(), xml)
        return xml

    def set_expand(self) :
        def treestore_set_expand(model, path, itr, self) :
            try :
                mf_itr = self.master_filter.convert_child_iter_to_iter(itr)
                mf_pa = self.master_filter.get_string_from_iter(mf_itr)
                p = model.get(itr, 0)[0].attr
                if "expanded" in p and p["expanded"] == "True":
                    self.treeview.expand_row(mf_pa, False)
                if "selected" in p and p["selected"] == "True":
                    self.selection.select_path(mf_pa)
            except :
                pass

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
                mf_pa = self.master_filter.get_string_from_iter(mf_itr)
                p.attr["expanded"] = self.treeview.row_expanded(mf_pa)
                p.attr["selected"] = mf_pa in self.selected_pathlist
            except :
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
                self.clear_undo()
                self.current_filename = filename
                self.autorefresh_call()
                self.file_changed = False
                self.auto_refresh.set_active(True)
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

    def reload_defaults_activate(self, callback = None) :
        try :
            self.defaults = open(self.default_src).read()
            self.action()
        except :
            mess_dlg(_('Can not reload NGC defaults: %s' % self.default_src))

    def reload_feature_activate(self, *args):
        src = self.selected_feature.get_attr('src')
        if src in FEATURE_DICT:
            del FEATURE_DICT[src]
        self.add_feature(src)

    def set_buttons_state(self):
        self.btn_build.set_sensitive(self.treestore.get_iter_root() != None)

        self.btn_delete.set_sensitive(self.can_delete_duplicate)
        self.btn_duplicate.set_sensitive(self.can_delete_duplicate)
        self.btn_move_up.set_sensitive(self.can_move_up)
        self.btn_move_down.set_sensitive(self.can_move_down)
        self.btn_add_to_item.set_sensitive(self.can_add_to_group)
        self.btn_remove_from_item.set_sensitive(self.can_remove_from_group)

def main():
    window = gtk.Dialog("LinuxCNC Features", None, gtk.DIALOG_MODAL)
    window.set_title(APP_TITLE)
    features = Features()
    features.embedded = False
    window.vbox.add(features)
    window.connect("destroy", gtk.main_quit)
    window.set_default_size(400, 800)
    return window.run()

if __name__ == "__main__":
    exit(main())
