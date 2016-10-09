#!/usr/bin/env python
# coding: utf-8
# ------------------------------------------------------------------
# --  NO USER SETTINGS IN THIS FILE -- EDIT PREFERENCES INSTEAD  ---
# ------------------------------------------------------------------

APP_TITLE = "NativeCAM"  # formerly LinuxCNC-Features
APP_COMMENTS = 'A GUI to help create LinuxCNC NGC files.'

APP_COPYRIGHT = '''Copyright © 2012 Nick Drobchenko aka Nick from cnc-club.ru
Copyright © 2016 Fernand Veilleux : fernv at gmail dot com'''
APP_AUTHORS = ['Fernand Veilleux, maintainer', 'Nick Drobchenko, initiator', 'Meison Kim', 'Alexander Wigen',
               'Konstantin Navrockiy', 'Mit Zot', 'Dewey Garrett']

APP_VERSION = "2.5"

APP_LICENCE = '''This program is free software: you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, either version 2 of
the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.'''

import gtk
import sys
import pygtk
pygtk.require('2.0')

import pango
from lxml import etree
import gobject
import ConfigParser
import re, os
import getopt
import shutil
import hashlib
import subprocess
import webbrowser
import io
from cStringIO import StringIO
import gettext
import time
import locale

try :
    import linuxcnc
    SYS_DIR = linuxcnc.SHARE + '/ncam'
    if not os.path.isdir(SYS_DIR) :
        SYS_DIR = os.path.dirname(os.path.realpath(__file__))
except :
    # linuxCNC not installed, must be my Windows pc for development and debugging
    SYS_DIR = os.path.dirname(os.path.realpath(__file__))

locale.setlocale(locale.LC_NUMERIC, '')
localeDICT = {}
localeDICT = locale.localeconv()
decimal_point = localeDICT['decimal_point']

# if False, NO_ICON_FILE will be used
DEFAULT_USE_NO_ICON = False
NO_ICON_FILE = 'no-icon.png'

# info at http://www.pygtk.org/pygtk2reference/pango-markup-language.html
header_fmt_str = '<i>%s...</i>'
sub_header_fmt_str = '<i>%s...</i>'
sub_header_fmt_str2 = '<b><i>%s</i></b>'
feature_fmt_str = '<b>%s</b>'
items_fmt_str = '<span foreground="blue" style="oblique"><b>%s</b></span>'

UNDO_MAX_LEN = 200
QUICK_ACCESS_TOP_COUNT = 0
gmoccapy_time_out = 0.0
developer_menu = False

try :
    t = gettext.translation('ncam', '/usr/share/locale')
    _ = t.ugettext
except :
    gettext.install('ncam', None, unicode = True)

MAX_QUICK_ACCESS_BTN = 15

DEFAULT_CATALOG = "mill"
DEFAULT_METRIC = False

# directories
CFG_DIR = 'cfg'
XML_DIR = 'xml'
LIB_DIR = 'lib'
NGC_DIR = 'scripts'
EXAMPLES_DIR = 'xml/examples'
CATALOGS_DIR = 'catalogs'
TEMPLATES_DIR = XML_DIR
GRAPHICS_DIR = 'graphics'

# files
DEFAULT_TEMPLATE = 'def_template.xml'
CURRENT_WORK = "current_work.xml"
PREFERENCES_FILE = "default.conf"
CONFIG_FILE = 'ncam.conf'
QUICK_ACCESS_FNAME = "quick_access.lst"
GENERATED_FILE = "ncam.ngc"

DEFAULT_EDITOR = 'gedit'  # or any like kate, etc...

SUPPORTED_DATA_TYPES = ['sub-header', 'header', 'bool', 'boolean', 'int', 'tool',
	'text', 'list', 'float', 'string', 'combo', 'combo-user', 'items', 'filename']
NUMBER_TYPES = ['float', 'int']
NO_ICON_TYPES = ['sub-header', 'header']
GROUP_HEADER_TYPES = ['items', 'sub-header', 'header']
DEFAULT_ICONS = {
    "active": "enable.png",
    "enabled": "enable.png",
    "items": "items.png"
}

XML_TAG = "lcnc-ncam"

HOME_PAGE = 'http://fernv.github.io/linuxcnc-features/'

class tv_select :  # 'enum' items
    none, feature, items, header, param = range(5)

INCLUDE = []
DEFINITIONS = []
PIXBUF_DICT = {}

UNIQUE_ID = 10

def get_int(s10) :
    index = s10.find('.')
    if index > -1 :
        s10 = s10[:index - 1]
    try :
        return int(s10)
    except :
        return 0

def get_float(s10) :
    try :
        return float(s10)
    except :
        return 0.0

def search_path(warn, f, *args) :
    if f[0] == "" :
        return None

    if os.path.isfile(f) :
        return f

    src = NCAM_DIR
    i = 0
    j = args.__len__()
    while i < j :
        src = os.path.join(src, args[i])
        i += 1
    src = os.path.join(src, f)
    if os.path.isfile(src) :
        return src

    for pa in [GRAPHICS_DIR, CFG_DIR, CATALOGS_DIR, LIB_DIR, XML_DIR] :
        src = os.path.join(pa, f)
        if os.path.isfile(src) :
            return src

    src = os.path.join(os.getcwd(), f)
    if os.path.isfile(src) :
        return src

    if warn > 0:
        print(_("Can not find file %(filename)s") % {"filename":f})

    if warn == 2 :
        mess_dlg(_("Can not find file %(filename)s") % {"filename":f})
    return None

def get_pixbuf(icon, size) :
    if size < 16 :
        size = 16
    if ((icon is None) or (icon == "")) :
        if DEFAULT_USE_NO_ICON:
            return None
        else :
            icon = NO_ICON_FILE

    s = icon.split("/")
    icon = s[-1]
    icon_id = icon + str(size)
    if (icon_id) in PIXBUF_DICT :
        return PIXBUF_DICT[icon_id]

    icon = search_path(0, icon, GRAPHICS_DIR)
    if (icon is None) :
        print(_('Image file not found : %(filename)s') % {"filename":icon})
        if DEFAULT_USE_NO_ICON:
            return None
        else :
            icon = NO_ICON_FILE

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
        print(_('Image file not valid : %(filename)s') % {"filename":icon})
        PIXBUF_DICT[icon_id] = None
        return None

def mess_dlg(mess, title = "NativeCAM"):
    dlg = gtk.MessageDialog(parent = None,
        flags = gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
        type = gtk.MESSAGE_WARNING,
        buttons = gtk.BUTTONS_OK, message_format = '%s' % mess)
    dlg.set_title(title)
    dlg.set_keep_above(True)
    dlg.run()
    dlg.destroy()

# def copy_to_dir(fname, dir):
    # create directories if necessary
#    dirname = os.path.realpath(os.path.dirname(dir))
#    filename = os.path.basename(fname)
#    if not os.path.exists(dirname):
#        os.makedirs(dirname)
#    open(dirname + '/' + filename, 'w').write(open(fname, 'r').read())


def mess_yesno(mess, title = ""):
    yesno = gtk.MessageDialog(parent = None,
        flags = gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
        type = gtk.MESSAGE_QUESTION,
        buttons = gtk.BUTTONS_YES_NO,
        message_format = '%s' % mess
        )
    yesno.set_title(title)
    yesno.set_keep_above(True)
    response = yesno.run()
    yesno.destroy()
    return response == gtk.RESPONSE_YES

def mess_with_buttons(mess, buttons, title = ""):
    mwb = gtk.Dialog(parent = None, buttons = buttons,
          flags = gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
          )
    mwb.set_title(title)
    finbox = mwb.get_content_area()
    l = gtk.Label(mess)
    finbox.pack_start(l)
    mwb.show_all()
    response = mwb.run()
    mwb.hide()
    mwb.destroy()
    return response

class copymode:  # 'enum' items
    one_at_a_time, yes_to_all, no_to_all = range(3)

def copy_dir_recursive(fromdir, todir,
                       update_ct = 0,
                       mode = copymode.one_at_a_time,
                       overwrite = False,
                       verbose = False) :
    if not os.path.isdir(todir) :
        os.makedirs(todir, 0755)

    for p in os.listdir(fromdir) :
        frompath = os.path.join(fromdir, p)
        topath = os.path.join(todir, p)
        if os.path.isdir(frompath) :
            mode, update_ct = copy_dir_recursive(frompath, topath,
                                      update_ct = update_ct,
                                      mode = mode,
                                      overwrite = overwrite,
                                      verbose = verbose)
            continue

        # copy files
        if not os.path.isfile(topath) or overwrite :
            shutil.copy(frompath, topath)
            update_ct += 1
            continue
        else :  # local file existes and not overwrite
            if (hashlib.md5(open(frompath, 'rb').read()).digest()
                 == hashlib.md5(open(topath, 'rb').read()).digest()) :
                # files are same
                if verbose :
                    print "NOT copying %s to %s" % (p, todir)
            else :  # files are different
                if (os.path.getctime(frompath) < os.path.getctime(topath)) :
                    # different and local file most recent
                    if verbose :
                        print (_('Keeping modified local file %(filename)s') % {"filename":p})
                    pass
                else :  # different and system file is most recent
                    if mode == copymode.yes_to_all :
                        if verbose :
                            print "copying %s to %s" % (p, todir)
                        shutil.copy(frompath, topath)
                        update_ct += 1
                        continue
                    if mode == copymode.no_to_all :
                        os.utime(topath, None)  # touch it
                        continue

                    buttons = (gtk.STOCK_YES, gtk.RESPONSE_YES,
                             gtk.STOCK_NO, gtk.RESPONSE_NO,
                             gtk.STOCK_REFRESH, gtk.RESPONSE_ACCEPT,
                             gtk.STOCK_CANCEL, gtk.RESPONSE_NONE)
                    msg = (_('\nAn updated system file is available:\n\n%(frompath)s\n\n'
                        'YES     -> Use new system file\n'
                        'NO      -> Keep local file\n'
                        'Refresh -> Accept all new system files (don\'t ask again)\n'
                        'Cancel  -> Keep all local files (don\'t ask again)\n') \
                        % {'frompath':frompath})
                    ans = mess_with_buttons(msg, buttons,
                                            title = "NEW file version available")

                    # set copymode
                    if ans == gtk.RESPONSE_YES :
                        pass
                    elif ans == gtk.RESPONSE_ACCEPT :
                        mode = copymode.yes_to_all
                    elif ans == gtk.RESPONSE_NONE :
                        mode = copymode.no_to_all
                    elif ans == gtk.RESPONSE_NO :
                        pass
                    else :
                        ans = gtk.RESPONSE_NO  # anything else (window close etc)

                    # copy or touch
                    if ans == gtk.RESPONSE_YES or mode == copymode.yes_to_all :
                        if verbose:
                            print "copying %s to %s" % (p, todir)
                        shutil.copy(frompath, topath)
                        update_ct += 1

                    if ans == gtk.RESPONSE_NO or mode == copymode.no_to_all :
                        os.utime(topath, None)  # touch it (update timestamp)

    return mode, update_ct

def err_exit(errtxt):
    print errtxt
    mess_dlg (errtxt)
    sys.exit(1)

def exit_if_invalid_rip_standalone():
    # This environmental variable exists if:
    #    started by linuxcnc script
    # or
    #    set by sourcing rip-environment
    if not os.environ.has_key('LINUXCNCVERSION') :
        pname = os.path.realpath(__file__)
        if pname.find("/usr/bin") != 0 :
            etxt = (_('*** Detected Standalone, Run-In-Place\n'
                      '*** without proper environment\n'
                      '*** Setup Run-In-Place first :\n'
                    ))
            idx = pname.find("bin/ncam")
            guess = ""

            if idx >= 0:
                guess = pname[0:idx]
            else :
                idx = pname.find("lib/python/gladevcp/ncam.py")
                if idx >= 0:
                    guess = pname[0:idx]
            etxt = etxt + (_('   $ source %sscripts/rip-environment') % guess)
            err_exit(etxt)
        return

def require_ini_items(fname, ini_instance):
    global NCAM_DIR, NGC_DIR

    val = ini_instance.find('DISPLAY', 'NCAM_DIR')
    if val is None :
        err_exit(_('Ini file <%(inifilename)s>\n'
                    'must have entry for [DISPLAY]NCAM_DIR')
                % {'inifilename':fname})

    val = os.path.expanduser(val)
    if os.path.isabs(val) :
        NCAM_DIR = val
    else :
        NCAM_DIR = (os.path.realpath(os.path.dirname(fname) + '/' + val))

    val = ini_instance.find('DISPLAY', 'PROGRAM_PREFIX')
    if val is None :
        msg = _("There is no PROGRAM_PREFIX in ini file\n"
            "Edit to add in DISPLAY section\n\n"
            "PROGRAM_PREFIX = abs or relative path to scripts directory\n"
            "i.e. PROGRAM_PREFIX = ./scripts or ~/ncam/scripts")
        err_exit(msg)

    val = os.path.expanduser(val)
    if os.path.isabs(val) :
        NGC_DIR = val
    else :
        NGC_DIR = (os.path.realpath(os.path.dirname(fname) + '/' + val))

def require_ncam_lib(fname, ini_instance):
    # presumes already checked:[DISPLAY]NCAM_DIR
    # ini file must specify a [RS274NGC]SUBROUTINE_PATH that
    # includes NCAM_DIR + LIB_DIR (typ: [DISPLAY]NCAM_DIR/lib)

    require_lib = os.path.join(NCAM_DIR, LIB_DIR)
    found_lib_dir = False
    try :
        subroutine_path = ini_instance.find('RS274NGC', 'SUBROUTINE_PATH')
        if subroutine_path is None :
            err_exit(_('Required lib missing:\n\n'
                       '[RS274NGC]SUBROUTINE_PATH'))

        print "[RS274NGC]SUBROUTINE_PATH =", subroutine_path

        for i, d in enumerate(subroutine_path.split(":")):
            d = os.path.expanduser(d)
            if os.path.isabs(d) :
                thedir = d
            else :
                thedir = (os.path.realpath(os.path.dirname(fname) + '/' + d))
            if not os.path.isdir(thedir) :
                continue
            else :
                print ("   SUBROUTINE_PATH[%d] (realpath) = %s" % (i, thedir))
                if not found_lib_dir :
                    found_lib_dir = thedir.find(require_lib) == 0

        print ""

        if not found_lib_dir :
            err_exit (_('\nThe required NativeCAM lib directory :\n<%(lib)s>\n\n'
                      'is not in [RS274NGC]SUBROUTINE_PATH:\n'
                      '<%(path)s>\n\nEdit ini and correct\n'
                    % {'lib':require_lib, 'path':subroutine_path}))

    except Exception, detail :
        err_exit(_('Required NativeCAM lib\n%(err_details)s') % {'err_details':detail})

class Tools():

    def __init__(self):
        self.table_fname = None
        self.list = ''

    def set_file(self, tool_table_file):
        fn = search_path(2, tool_table_file)
        if fn is not None :
            self.table_fname = fn
            self.load_table()

    def load_table(self, *args):
        self.table = None
        self.table = []
        if self.table_fname is not None :
            tbl = open(self.table_fname).read().split("\n")
            for s in tbl :
                s = s.strip()
                if s.find(";") > 1:
                    tnumber = '0'
                    s = s.split(";")
                    tdesc = s[1][0:]
                    s = s[0][0:]
                    s1 = s.split(" ")
                    for s2 in s1 :
                        if (len(s2) > 1) and (s2[0] == 'T') :
                            tnumber = s2[1:]
                    if tnumber != '0' :
                        if tdesc == '' :
                            tdesc = _('no description')
                        self.table.append([int(tnumber), tnumber, tdesc])
        self.table.append ([0, '0', _('None')])
        self.table.sort()

        self.list = ''
        for tool in self.table :
            self.list += tool[1] + ' - ' + tool[2] + '=' + tool[1] + ':'
        self.list = self.list.rstrip(':')

    def get_text(self, tn):
        for tool in self.table :
            if tool[1] == tn :
                return tool[1] + ' - ' + tool[2]
        return '0 - ' + _('None')

class CellRendererMx(gtk.CellRendererText):

    def __init__(self) :
        gtk.CellRendererText.__init__(self)
        self.max_value = 999999.9
        self.min_value = -999999.9
        self.data_type = 'string'
        self.options = ''
        self.digits = '3'
        self.param_value = ''
        self.combo_values = []
        self.tooltip = ''
        self.preedit = None
        self.edited = None
        self.inputKey = ''
        self.tool_list = []
        self.new_dt = ''
        self.dt_change = '1'
        self.not_zero = '0'

    def set_tooltip(self, value):
        self.tooltip = value

    def set_max_value(self, value):
        self.max_value = value

    def set_param_value(self, value):
        self.param_value = value

    def set_not_zero(self, value = '0'):
        self.not_zero = value

    def set_min_value(self, value):
        self.min_value = value

    def set_data_type(self, value):
        self.data_type = value

    def set_edit_datatype(self, value):
        self.editdata_type = value

    def set_Input(self, value):
        self.inputKey = value

    def set_fileinfo(self, patrn, mime_type, filter_name):
        self.pattern = patrn
        self.mime_type = mime_type
        self.filter_name = filter_name

    def set_toolinfo(self, toollist):
        self.tool_list = toollist

    def set_options(self, value):
        self.options = value

    def set_digits(self, value):
        self.digits = value

    def set_preediting(self, value):
        self.preedit = value

    def set_edited_user_fn(self, value):
        self.user_edited_fn = value

    def do_get_property(self, pspec):
        return getattr(self, pspec.name)

    def do_set_property(self, pspec, value):
        setattr(self, pspec.name, value)

    def set_treeview(self, value):
        self.tv = value

    def get_treeview(self):
        return self.tv

    def create_VKB(self, cell_area):
        self.vkb = gtk.Dialog(parent = self.tv.get_toplevel())
        self.vkb.set_decorated(False)
        self.vkb.set_transient_for(None)
        self.vkb.set_border_width(3)

        lbl = gtk.Label()
        lbl.set_markup(self.tooltip)
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
        tbl.attach(btn, 3, 4, 1, 2)

        self.new_dt = ''
        btn = gtk.Button('9->A')
        btn.set_tooltip_text(_('Change to string'))
        btn.connect("clicked", self.cdt_request)
        tbl.attach(btn, 3, 4, 0, 1)

        k = 10
        for i in range(1, 4) :
            k = k - 3
            for j in range(0, 3):
                btn = gtk.Button(str(k + j))
                btn.connect("clicked", self.numbers_clicked)
                tbl.attach(btn, j, j + 1, i, i + 1)

        if (self.min_value < 0.0) :
            btn = gtk.Button(label = '+/-')
            btn.connect("clicked", self.change_sign_clicked)
            tbl.attach(btn, 2, 3, 4, 5)
            last_col = 2
        else :
            last_col = 3

        if self.editdata_type == 'float' and get_int(self.digits) > 0 :
            btn = gtk.Button(label = decimal_point)
            btn.connect("clicked", self.dot_clicked)
            tbl.attach(btn, last_col - 1, last_col, 4, 5)
            last_col = last_col - 1

        btn = gtk.Button(label = '0')
        btn.connect("clicked", self.numbers_clicked)
        tbl.attach(btn, 0, last_col, 4, 5)

        btn = gtk.Button()
        img = gtk.Image()
        img.set_from_stock('gtk-cancel', menu_icon_size)
        btn.set_image(img)
        btn.connect("clicked", self.cancel_clicked)
        tbl.attach(btn, 3, 4, 2, 3)

        btn = gtk.Button()
        img = gtk.Image()
        img.set_from_stock('gtk-apply', menu_icon_size)
        btn.set_image(img)
        btn.connect("clicked", self.ok_clicked)
        tbl.attach(btn, 3, 4, 3, 5)

        (tree_x, tree_y) = self.tv.get_bin_window().get_origin()
        (tree_w, tree_h) = self.tv.window.get_geometry()[2:4]

        vkb_w = max(tree_w - cell_area.x, vkb_width)
        self.vkb.set_size_request(vkb_w, vkb_height)
        self.vkb.resize(vkb_w, vkb_height)

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
            self.set_vkb_result(self.param_value.replace('.', decimal_point))
            self.vkb_initialize = True

        self.vkb.show_all()
        btn.grab_focus()

    def cdt_request(self, btn):
        self.new_dt = 'string'
        self.vkb.response(gtk.RESPONSE_OK)

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
            if value == decimal_point :
                value = '0' + decimal_point
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
                if (self.editdata_type == 'float'):
                    if self.vkb_initialize :
                        self.set_vkb_result(decimal_point)
                    else :
                        if lbl.find(decimal_point) == -1 :
                            self.set_vkb_result(lbl + decimal_point)
            elif k_name in ['KP_Subtract', 'KP_Add', 'plus', 'minus']  \
                        and (self.min_value < 0.0) :
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
            self.set_vkb_result(decimal_point)
        else :
            lbl = self.result_entry.get_text()
            if lbl.find(decimal_point) == -1 :
                self.set_vkb_result(lbl + decimal_point)

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
        if vkb_cancel_on_out:
            self.vkb.response(gtk.RESPONSE_CANCEL)
        else :
            self.vkb.response(gtk.RESPONSE_OK)

    def do_get_size(self, widget, cell_area):
        size_tuple = gtk.CellRendererText.do_get_size(self, widget, cell_area)
        return(size_tuple)

    def edit_number(self, itr = None, time_out = 0.0) :
        self.create_VKB(self.cell_area)
        time.sleep(time_out)

        if self.vkb.run() == gtk.RESPONSE_OK:
            val = get_float(self.result_entry.get_text().replace(decimal_point, '.'))
            if val > self.max_value :
                val = self.max_value
            elif val < self.min_value :
                val = self.min_value

            fmt = '{0:0.%sf}' % self.digits
            newval = fmt.format(val)
            if (val == 0.0) and (self.not_zero != '0'):
                mess_dlg(_('Value can not be "%(value)s" for\n\n%(tooltip)s') % \
                         {'value':newval, 'tooltip':self.tooltip})
                self.vkb.destroy()
                return None
        else :
            newval = None
        self.vkb.destroy()
        if (itr is not None) and (newval is not None) :
            self.user_edited_fn(itr, newval, self.new_dt)
        return newval

    def edit_list(self, itr = None, time_out = 0.0):
        self.list_window = gtk.Dialog(parent = self.tv.get_toplevel())
        self.list_window.set_border_width(0)
        self.list_window.set_decorated(False)
        self.list_window.set_transient_for(None)
        vp = gtk.Viewport()
        vp.set_shadow_type(gtk.SHADOW_ETCHED_IN)
        self.list_window.vbox.add(vp)
        sw = gtk.ScrolledWindow()
        sw.set_policy(gtk.POLICY_NEVER, gtk.POLICY_AUTOMATIC)
        vp.add(sw)

        self.list_window.realize()
        self.list_window.resize(1, 1)
        lw, base_height = self.list_window.get_size()

        ls = gtk.ListStore(str, str)
        active_row = 0
        count = 0
        for option in self.options.split(":") :
            opt = option.split('=')
            ls.append([opt[0], opt[1]])
            if (opt[1] == self.param_value) :
                active_row = count
            count += 1

        ls_view = gtk.TreeView(ls)
        ls_view.set_headers_visible(False)
        tvcolumn = gtk.TreeViewColumn('Column 0')
        ls_view.append_column(tvcolumn)
        rdr = gtk.CellRendererText()
        row_height = self.cell_area.height - 4
        rdr.set_fixed_size(self.cell_area.width, row_height)
        tvcolumn.pack_start(rdr, True)
        tvcolumn.add_attribute(rdr, 'text', 0)
        tvcolumn.set_min_width(self.cell_area.width - 2)
        self.list_window.connect('focus-out-event', self.list_out)
        ls_view.connect('button-release-event', self.list_btn_released)
        ls_view.connect('key-press-event', self.list_keypress)
        sw.add(ls_view)

        lw_height = base_height + 4 + self.cell_area.height * min(count, 10)
        (tree_x, tree_y) = self.tv.get_bin_window().get_origin()
        (tree_w, tree_h) = self.tv.get_bin_window().get_geometry()[2:4]
        y = tree_y + min(self.cell_area.y, tree_h - lw_height)

        self.list_window.move(tree_x + self.cell_area.x, y)
        self.list_window.resize(tree_w - self.cell_area.x, lw_height)
        position = int(sw.get_vadjustment().get_upper() * active_row / count)
        sw.get_vadjustment().set_value(position)

        self.list_window.show_all()
        ls_view.set_cursor(active_row)
        ls_view.grab_focus()

        time.sleep(time_out)
        if self.list_window.run() == gtk.RESPONSE_OK:
            model, ls_itr = ls_view.get_selection().get_selected()
            new_val = model.get_value(ls_itr, 1)
        else :
            new_val = None
        self.list_window.destroy()
        if (itr is not None) and (new_val is not None) :
            self.user_edited_fn(itr, new_val, '')
        return new_val

    def edit_string(self, itr = None, time_out = 0.0):
        self.stringedit_window = gtk.Dialog(parent = self.tv.get_toplevel())
        self.stringedit_window.hide()
        self.stringedit_window.set_decorated(False)
        self.stringedit_window.set_transient_for(None)
        self.stringedit_window.set_border_width(0)
        self.new_dt = ''

        self.stringedit_entry = gtk.Entry()
        self.stringedit_entry.set_editable(True)
        if self.inputKey != 'BS' :
            self.stringedit_entry.set_text(self.param_value)
        self.inputKey = ''

#        if (self.dt_change == '1') :
#            self.stringedit_entry.connect('populate-popup', self.str_popup)
        self.stringedit_entry.connect('key-press-event', self._str_keyhandler)
        self.stringedit_entry.connect('focus-out-event', self._str_focus_out)
        self.stringedit_window.vbox.add(self.stringedit_entry)

        # position the popup on the edited cell
        (tree_x, tree_y) = self.tv.get_bin_window().get_origin()
        (tree_w, tree_h) = self.tv.window.get_geometry()[2:4]
        x = tree_x + self.cell_area.x
        y = tree_y + self.cell_area.y
        self.stringedit_window.move(x, y - 3)
        self.stringedit_window.resize(tree_w - self.cell_area.x, self.cell_area.height)
        self.stringedit_window.show_all()
        time.sleep(time_out)

        if self.stringedit_window.run() == gtk.RESPONSE_OK:
            new_val = self.stringedit_entry.get_text()
        else :
            new_val = None
        self.stringedit_window.destroy()
        if (itr is not None) and (new_val is not None) :
            self.user_edited_fn(itr, new_val, self.new_dt)
        return new_val

# a bug in pygtk makes it unavailable
# trying to find a workaround
#     def str_popup(self, widget, menu):
#         menu.prepend(gtk.SeparatorMenuItem())
#
#         menu_item = gtk.MenuItem(_('Change to integer'))
#         menu_item.set_size_request(-1, add_menu_icon_size)
#         menu_item.connect("activate", self.chng_dt, 'int')
#         menu.prepend(menu_item)
#
#         menu_item = gtk.MenuItem(_('Change to float'))
#         menu_item.set_size_request(-1, add_menu_icon_size)
#         menu_item.connect("activate", self.chng_dt, 'float')
#         menu.prepend(menu_item)
#         menu.show_all()

    def chng_dt(self, callback = None, *arg) :
        self.new_dt = arg[0]
        self.stringedit_window.response(gtk.RESPONSE_OK)

    def list_keypress(self, widget, event) :
        keyname = gtk.gdk.keyval_name(event.keyval)
        if keyname in ["Return", "KP_Enter", "space"] :
            self.list_window.response(gtk.RESPONSE_OK)

    def list_out(self, widget, event):
        self.list_window.response(gtk.RESPONSE_CANCEL)

    def list_btn_released(self, widget, event):
        self.list_window.response(gtk.RESPONSE_OK)

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

        if self.editdata_type in GROUP_HEADER_TYPES :
            self.inputKey = ''
            return None

        self.cell_area = cell_area

        if self.editdata_type in NUMBER_TYPES :
#            if self.inputKey == '-' and self.min_value >= 0.0 :
#                self.inputKey = ''
#                return None
#            else :
            result = self.edit_number()
            if result is not None :
                self.edited(self, path, result, self.new_dt)
            return None

        elif self.editdata_type in ['bool', 'boolean']:
            if self.inputKey > '' :
                self.inputKey = ''
                return None
            if get_int(self.param_value) == 0 :
                self.edited(self, path, '1', '')
            else :
                self.edited(self, path, '0', '')
            return None

        elif self.editdata_type in ['combo-user', 'combo', 'tool']:
            self.inputKey = ''
            result = self.edit_list()
            if result is not None :
                self.edited(self, path, result, '')
            return None

        elif self.editdata_type == 'string':
            result = self.edit_string()
            if result is not None :
                self.edited(self, path, result, self.new_dt)
            return None

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

                if os.path.exists(self.param_value):
                    filechooserdialog.set_filename(self.param_value)
                else :
                    filechooserdialog.set_current_folder(os.getcwd())

                response = filechooserdialog.run()
                if response == gtk.RESPONSE_OK:
                    self.edited(self, path, filechooserdialog.get_filename(), '')
            finally:
                filechooserdialog.destroy()
            return None

        else :  # edit multi-line text
            self.selection = treeview.get_selection()
            self.treestore, self.treeiter = self.selection.get_selected()

            self.textedit_window = gtk.Dialog(parent = treeview.get_toplevel())

            self.textedit_window.set_decorated(False)
            self.textedit_window.set_transient_for(None)

            self.textedit = gtk.TextView()
            self.textedit.set_editable(True)
            self.textbuffer = self.textedit.get_buffer()
            self.textedit.set_wrap_mode(gtk.WRAP_WORD)
            self.textbuffer.set_property('text', self.get_property('text'))

            self.textedit_window.connect('key-press-event', self._keyhandler)
            self.textedit_window.connect('focus-out-event', self._focus_out, path)

            scrolled_window = gtk.ScrolledWindow()
            scrolled_window.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)

            scrolled_window.add(self.textedit)
            self.textedit_window.vbox.add(scrolled_window)
            self.textedit_window.realize()

            # position the popup on the edited cell within the treeview
            (tree_x, a) = treeview.window.get_origin()
            (a, tree_y) = treeview.get_bin_window().get_origin()
            (tree_w, tree_h) = treeview.window.get_geometry()[2:4]
            (t_w, t_h) = self.textedit_window.window.get_geometry()[2:4]
            y = tree_y + min(cell_area.y, tree_h - t_h +
                             treeview.get_visible_rect().y)

            self.textedit_window.move(tree_x, y + cell_area.height)
            self.textedit_window.resize(tree_w, cell_area.height + 60)
            self.textedit_window.show_all()

            if self.textedit_window.run() == gtk.RESPONSE_OK:
                (iter_first, iter_last) = self.textbuffer.get_bounds()
                text = self.textbuffer.get_text(iter_first, iter_last)
                self.edited(self, path, text, '')

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

    def _str_focus_out(self, widget, event):
        self.stringedit_window.response(gtk.RESPONSE_OK)

    def _str_keyhandler(self, widget, event):
        keyname = gtk.gdk.keyval_name(event.keyval)
        if keyname in ['Return', 'KP_Enter']:
            self.stringedit_window.response(gtk.RESPONSE_OK)

    def _focus_out(self, widget, event, path):
        self.textedit_window.response(gtk.RESPONSE_OK)

    def _keyhandler(self, widget, event):
        keyname = gtk.gdk.keyval_name(event.keyval)
        if gtk.gdk.keyval_name(event.keyval) in ['Return', 'KP_Enter'] :
            if event.state & (gtk.gdk.SHIFT_MASK | gtk.gdk.CONTROL_MASK) :
                pass
            else :
                event.keyval = 0
                self.textedit_window.response(gtk.RESPONSE_OK)

gobject.type_register(CellRendererMx)

class Parameter() :
    def __init__(self, ini = None, ini_id = None, xml = None) :
        self.attr = {}
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
        else :
            self.attr["type"] = 'string'
        if self.attr["type"] not in SUPPORTED_DATA_TYPES :
            self.attr["type"] = 'string'

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
        return get_pixbuf(icon, treeview_icon_size)

    def get_image(self) :
        return get_pixbuf(self.get_attr("image"), add_dlg_icon_size)

    def get_value(self) :  # , display = False) :
        if default_metric and "metric_value" in self.attr :
            return self.attr["metric_value"]
        else :
            return self.attr["value"] if "value" in self.attr else ""

    def set_value(self, new_val) :
        if default_metric and "metric_value" in self.attr :
            self.attr["metric_value"] = new_val
            if self.get_type() == "float" :
                fmt = '{0:0.%sf}' % self.get_digits()
                self.attr['value'] = fmt.format(get_float(new_val) / 25.4)
        else :
            self.attr["value"] = new_val
            if (self.get_type() == "float") and self.get_attr("metric_value") is not None :
                fmt = '{0:0.%sf}' % self.get_digits()
                self.attr['metric_value'] = fmt.format(get_float(new_val) * 25.4)

    def get_name(self) :
        return self.attr["name"] if "name" in self.attr else ""

    def get_options(self):
        return self.attr["options"] if "options" in self.attr else ""

    def get_type(self):
        return self.attr["type"] if "type" in self.attr else "string"

    def set_type(self, new_type):
        if (new_type != '') and (new_type != self.get_type()) :
            self.attr['type'] = new_type
            self.set_value(self.get_value())

    def get_tooltip(self):
        return self.attr["tool_tip"] if "tool_tip" in self.attr else None

    def get_attr(self, name) :
        return self.attr[name] if name in self.attr else None

    def get_digits(self):
        if self.get_type() == 'int' :
            return '0'
        else :
            return self.attr["digits"] if "digits" in self.attr else default_digits

    def get_not_zero(self):
        return self.attr["no_zero"] if "no_zero" in self.attr else '0'

    def set_digits(self, new_digits) :
        self.attr["digits"] = new_digits
        self.set_value(self.get_value())

    def get_min_value(self):
        return self.attr["minimum_value"] if "minimum_value" in self.attr \
            else "-999999.9"

    def get_max_value(self):
        return self.attr["maximum_value"] if "maximum_value" in self.attr \
            else "999999.9"

class Feature():
    def __init__(self, src = None, xml = None) :
        self.attr = {}
        self.param = []
        if src is not None :
            self.from_src(src)
        elif xml is not None :
            self.from_xml(xml)

    def __repr__(self) :
        return etree.tostring(self.to_xml(), pretty_print = True)

    def get_icon(self) :
        return get_pixbuf(self.get_attr("icon"), treeview_icon_size)

    def get_image(self) :
        return get_pixbuf(self.get_attr("image"), add_dlg_icon_size)

    def get_value(self):
        return self.attr["value"] if "value" in self.attr else ""

    def set_value(self, new_val):
        self.attr["value"] = new_val

    def get_type(self):
        return self.attr["type"] if "type" in self.attr else "string"

    def get_tooltip(self):
        return self.attr["tool_tip"] if "tool_tip" in self.attr else \
            self.attr["help"] if "help" in self.attr else None

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
                p.attr['name'] = p.get_name()
                p.attr['tool_tip'] = (p.get_attr('tool_tip') \
                            if "tool_tip" in p.attr else p.get_attr('name'))
                opt = p.attr["options"] if "options" in self.attr else None
                if p.get_type() == 'float' :
                    fmt = '{0:0.%sf}' % p.get_digits()
                    p.set_value(fmt.format(get_float(p.get_value())))
                elif p.get_type() == 'int' :
                    p.set_value(str(get_int(p.get_value())))
                self.param.append(p)

        self.attr["id"] = self.attr["type"] + '_000'

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

        if 'short_id' in self.attr :
            del self.attr['short_id']

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
            num = max([get_int(i.get("id")[-3: ]) for i in l] + [0]) + 1
        self.attr["id"] = self.attr["type"] + "_%03d" % num

    def get_short_id(self):
        global UNIQUE_ID
        id = self.attr['short_id'] if 'short_id' in self.attr else None
        if id is None :
            id = str(UNIQUE_ID)
            self.attr["short_id"] = id
            UNIQUE_ID += 1
        return id

    def get_definitions(self) :
        global DEFINITIONS
        if self.attr["type"] not in DEFINITIONS :
            s = self.attr["definitions"] if "definitions" in self.attr else ''
            if s != '' :
                s = self.process(s)
                if s != "" :
                    DEFINITIONS.append(self.attr["type"])
                return s
        return ""

    def include(self, srce) :
        src = search_path(2, srce, LIB_DIR)
        if src is not None:
            return open(src).read()
        return ''

    def include_once(self, src) :
        global INCLUDE
        if src not in INCLUDE :
            INCLUDE.append(src)
            return self.include(src)
        return ""

    def process(self, s, line_leader = '') :

        def eval_callback(m) :
            try :
                return str(eval(m.group(2), globals(), {"self":self}))
            except :
                return ''

        def exec_callback(m) :
            s = m.group(2)

            # strip starting spaces
            s = s.replace("\t", " ")
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
                return subprocess.check_output([s], shell = True,
                                               stderr = subprocess.STDOUT)
            except subprocess.CalledProcessError as e:
                msg = _('Error with subprocess: returncode = %(errcode)s\n'
                         'output = %(output)s\n'
                         'e= %(e)s\n') \
                         % {'errcode':e.returncode, 'output':e.output, 'e':e}
                print msg
                mess_dlg(msg)
                return ''

        def import_callback(m) :
            fname = m.group(2)
            fname = search_path(2, fname, XML_DIR)
            if fname is not None :
                return str(open(fname).read())

        s = re.sub(r"%NCAM_DIR%", "%s" % NCAM_DIR, s)

        for p in self.param :
            if "call" in p.attr and "value" in p.attr :
                s = re.sub(r"%s([^A-Za-z0-9_]|$)" %
                           (re.escape(p.attr["call"])), r"%s\1" %
                           p.get_value(), s)

        s = re.sub(r"%NCAM_DIR%", "%s" % NCAM_DIR, s)
        s = re.sub(r"(?i)(<import>(.*?)</import>)", import_callback, s)
        s = re.sub(r"(?i)(<eval>(.*?)</eval>)", eval_callback, s)
        s = re.sub(r"(?ims)(<exec>(.*?)</exec>)", exec_callback, s)
        s = re.sub(r"(?ims)(<subprocess>(.*?)</subprocess>)",
                   subprocess_callback, s)

        id = self.get_attr("id")
        s = re.sub(r"#self_id", "%s" % id, s)

        if s.find("#ID") > -1 :
            id = self.get_short_id()
            s = re.sub(r"#ID", "%s" % id, s)

        s = s.lstrip('\n').rstrip('\n\t')
        if s == '' :
            return ''
        if line_leader > '' :
            result_s = '\n'
            for line in s.split('\n') :
                result_s += line_leader + line + '\n'
            return result_s + '\n'
        else :
            return '\n' + s + '\n\n'

    def getindent(self) :
        count = get_int(self.attr['indent'] if 'indent' in self.attr else '0')
        return('\t' * count)

    def get_unique_id(self) :
        global UNIQUE_ID
        UNIQUE_ID = UNIQUE_ID + 1
        return UNIQUE_ID

class Preferences():

    def __init__(self):
        global default_metric
        default_metric = DEFAULT_METRIC
        self.pref_file = None
        self.cfg_file = None
        self.ngc_init_str = None

    def read(self, nc_dir, cat_name):
        global default_digits, default_metric, add_menu_icon_size, \
            add_dlg_icon_size, quick_access_icon_size, menu_icon_size, \
            treeview_icon_size, vkb_width, vkb_height, vkb_cancel_on_out, \
            toolbar_icon_size, gmoccapy_time_out, developer_menu

        def read_float(cf, section, key, default):
            try :
                return cf.getfloat(section, key)
            except :
                return default

        def read_boolean(cf, section, key, default):
            try :
                return cf.getboolean(section, key)
            except :
                return default

        def read_str(cf, section, key, default):
            try :
                val = cf.get(section, key)
                if val is None :
                    return default
                else :
                    return val
            except :
                return default

        def read_int(cf, section, key, default):
            return int(read_float(cf, section, key, default))

        self.cfg_file = nc_dir + "/" + CONFIG_FILE

        if not os.path.exists(nc_dir + "/" + cat_name):
            os.makedirs(nc_dir + "/" + cat_name)
        self.pref_file = nc_dir + "/" + cat_name + '/' + PREFERENCES_FILE

        if self.ngc_init_str is None :
            self.ngc_init_str = 'G17 G40 G49 G90 G92.1 G94 G54 G64 p0.001'

        config = ConfigParser.ConfigParser()
        config.read(self.cfg_file)

        self.w_adj_value = read_float(config, 'display', 'width', 550)
        self.col_width_adj_value = read_float(config, 'display', 'col_width', 175)
        self.tv_w_adj_value = read_float(config, 'display', 'master_tv_width', 175)
        self.sub_hdrs_in_tv1 = read_boolean(config, 'display', 'subheaders_in_master', False)
        self.restore_expand_state = read_boolean(config, 'display', 'restore_expand_state', True)
        developer_menu = read_boolean(config, 'display', 'developer_menu', False)
        self.use_dual_views = read_boolean(config, 'layout', 'dual_view', True)
        self.side_by_side = read_boolean(config, 'layout', 'side_by_side', True)
        treeview_icon_size = read_int(config, 'icons_size', 'treeview', 28)
        add_menu_icon_size = read_int(config, 'icons_size', 'add_menu', 24)
        menu_icon_size = read_int(config, 'icons_size', 'menu', 4)
        toolbar_icon_size = read_int(config, 'icons_size', 'toolbar', 4)
        add_dlg_icon_size = read_int(config, 'icons_size', 'add_dlg', 65)
        quick_access_icon_size = read_int(config, 'icons_size', 'quick_access_tb', 30)
        vkb_width = read_int(config, 'virtual_kb', 'minimum_width', 260)
        vkb_height = read_int(config, 'virtual_kb', 'height', 260)
        vkb_cancel_on_out = read_boolean(config, 'virtual_kb', 'cancel_on_focus_out', True)

        config.read(self.pref_file)
        self.timeout_value = read_float(config, 'general', 'time_out', 0.300)
        default_digits = read_str(config, 'general', 'digits', '3')
        self.ngc_show_final_cut = read_str(config, 'general', 'show_final_cut', '1')
        self.ngc_show_bottom_cut = read_str(config, 'general', 'show_bottom_cut', '1')
        self.ngc_init_str = read_str(config, 'ngc', 'init_str', self.ngc_init_str)
        self.ngc_post_amble = read_str(config, 'ngc', 'post_amble', " ")
        self.ngc_off_rot_coord_system = read_int(config, 'ngc', 'off_rot_coord_system', 2)
        self.ngc_spindle_speedup_time = read_str(config, 'ngc', 'spindle_acc_time', '1.0')
        gmoccapy_time_out = read_float(config, 'general', 'gmoccapy_time_out', 0.15)

        self.ngc_probe_func = read_str(config, 'probe', 'probe_func', "4")
        self.probe_tool_len_comp = read_str(config, 'probe', 'probe_tool_len_comp', '1')
        if default_metric :
            self.ngc_probe_feed = read_str(config, 'probe_mm', 'probe_feed', '200')
            self.ngc_probe_latch = read_str(config, 'probe_mm', 'probe_latch', '-1')
            self.ngc_probe_latch_feed = read_str(config, 'probe_mm', 'probe_latch_feed', '50')
            self.ngc_probe_tip_dia = read_str(config, 'probe_mm', 'probe_tip_dia', '3.0')
            self.ngc_probe_safe = read_str(config, 'probe_mm', 'probe_safe', '5.0')

            self.drill_center_depth = read_str(config, 'drill_mm', 'center_drill_depth', '-3.0')
        else :
            self.ngc_probe_feed = read_str(config, 'probe', 'probe_feed', '8.0')
            self.ngc_probe_latch = read_str(config, 'probe', 'probe_latch', '-0.05')
            self.ngc_probe_latch_feed = read_str(config, 'probe', 'probe_latch_feed', '2')
            self.ngc_probe_tip_dia = read_str(config, 'probe', 'probe_tip_dia', '0.125')
            self.ngc_probe_safe = read_str(config, 'probe', 'probe_safe', '0.2')

            self.drill_center_depth = read_str(config, 'drill', 'center_drill_depth', '-0.125')

        self.pocket_mode = read_str(config, 'pocket', 'mode', '0')

        self.opt_eng1 = read_str(config, 'optimizing', 'engagement1', '0.20')
        self.opt_eng2 = read_str(config, 'optimizing', 'engagement2', '0.30')
        self.opt_eng3 = read_str(config, 'optimizing', 'engagement3', '0.80')

        self.opt_ff1 = read_str(config, 'optimizing', 'feedfactor1', '1.50')
        self.opt_ff2 = read_str(config, 'optimizing', 'feedfactor2', '1.30')
        self.opt_ff3 = read_str(config, 'optimizing', 'feedfactor3', '1.00')
        self.opt_ff4 = read_str(config, 'optimizing', 'feedfactor4', '0.80')
        self.opt_ff0 = read_str(config, 'optimizing', 'feedfactor0', '1.00')

        self.opt_sf1 = read_str(config, 'optimizing', 'speedfactor1', '1.00')
        self.opt_sf2 = read_str(config, 'optimizing', 'speedfactor2', '1.00')
        self.opt_sf3 = read_str(config, 'optimizing', 'speedfactor3', '1.00')
        self.opt_sf4 = read_str(config, 'optimizing', 'speedfactor4', '0.80')
        self.opt_sf0 = read_str(config, 'optimizing', 'speedfactor0', '1.00')

        self.create_defaults()

    def save_layout(self):
        config = ConfigParser.ConfigParser()
        config.add_section('INFO')
        config.set('INFO', 'display and layout',
            'You can edit these values directly or\n'
            'by using Utilities->Preferences to do it.')
        config.add_section('display')
        config.set('display', 'width', self.w_adj_value)
        config.set('display', 'col_width', self.col_width_adj_value)
        config.set('display', 'master_tv_width', self.tv_w_adj_value)
        config.set('display', 'subheaders_in_master', self.sub_hdrs_in_tv1)
        config.set('display', 'restore_expand_state', self.restore_expand_state)
        config.set('display', 'developer_menu', developer_menu)
        config.add_section('layout')
        config.set('layout', 'dual_view', self.use_dual_views)
        config.set('layout', 'side_by_side', self.side_by_side)
        config.add_section('icons_size')
        config.set('icons_size', 'treeview', treeview_icon_size)
        config.set('icons_size', 'add_menu', add_menu_icon_size)
        config.set('icons_size', 'menu', menu_icon_size)
        config.set('icons_size', 'toolbar', toolbar_icon_size)
        config.set('icons_size', 'add_dlg', add_dlg_icon_size)
        config.set('icons_size', 'quick_access_tb', quick_access_icon_size)
        config.add_section('virtual_kb')
        config.set('virtual_kb', 'minimum_width', vkb_width)
        config.set('virtual_kb', 'height', vkb_height)
        config.set('virtual_kb', 'cancel_on_focus_out', vkb_cancel_on_out)

        with open(self.cfg_file, 'wb') as configfile:
            config.write(configfile)

    def save_defaults(self):
        config = ConfigParser.ConfigParser()
        config.add_section('INFO')
        config.set('INFO', 'Default values and usefull params',
            'You can edit these values directly or\n'
            'by using Utilities->Preferences to do it.')

        config.add_section('general')
        config.set('general', 'time_out', self.timeout_value)
        config.set('general', 'digits', default_digits)
        config.set('general', 'show_final_cut', self.ngc_show_final_cut)
        config.set('general', 'show_bottom_cut', self.ngc_show_bottom_cut)
        config.set('general', 'gmoccapy_time_out', gmoccapy_time_out)

        config.add_section('ngc')
        config.set('ngc', 'init_str', self.ngc_init_str)
        config.set('ngc', 'post_amble', self.ngc_post_amble)
        config.set('ngc', 'off_rot_coord_system', self.ngc_off_rot_coord_system)
        config.set('ngc', 'spindle_acc_time', self.ngc_spindle_speedup_time)

        config.add_section('probe')
        config.set('probe', 'probe_func', self.ngc_probe_func)
        config.set('probe', 'probe_tool_len_comp', self.probe_tool_len_comp)

        if default_metric :
            probe_section = 'probe_mm'
            config.add_section(probe_section)
            drill_section = 'drill_mm'
        else :
            probe_section = 'probe'
            drill_section = 'drill'
        config.add_section(drill_section)

        config.set(probe_section, 'probe_feed', self.ngc_probe_feed)
        config.set(probe_section, 'probe_latch', self.ngc_probe_latch)
        config.set(probe_section, 'probe_latch_feed', self.ngc_probe_latch_feed)
        config.set(probe_section, 'probe_tip_dia', self.ngc_probe_tip_dia)
        config.set(probe_section, 'probe_safe', self.ngc_probe_safe)

        config.set(drill_section, 'center_drill_depth', self.drill_center_depth)

        config.add_section('pocket')
        config.set('pocket', 'mode', self.pocket_mode)

        config.add_section('optimizing')
        config.set('optimizing', 'engagement1', self.opt_eng1)
        config.set('optimizing', 'engagement2', self.opt_eng2)
        config.set('optimizing', 'engagement3', self.opt_eng3)

        config.set('optimizing', 'feedfactor1', self.opt_ff1)
        config.set('optimizing', 'feedfactor2', self.opt_ff2)
        config.set('optimizing', 'feedfactor3', self.opt_ff3)
        config.set('optimizing', 'feedfactor4', self.opt_ff4)
        config.set('optimizing', 'feedfactor0', self.opt_ff0)

        config.set('optimizing', 'speedfactor1', self.opt_sf1)
        config.set('optimizing', 'speedfactor2', self.opt_sf2)
        config.set('optimizing', 'speedfactor3', self.opt_sf3)
        config.set('optimizing', 'speedfactor4', self.opt_sf4)
        config.set('optimizing', 'speedfactor0', self.opt_sf0)

        with open(self.pref_file, 'wb') as configfile:
            config.write(configfile)

    def edit(self, ncam):
        parent = ncam.main_box.get_toplevel()
        self.ncam = ncam

        builder = gtk.Builder()
        try :
            builder.add_from_file(os.path.join(SYS_DIR, "ncam_pref.glade"))
            prefdlg = builder.get_object("prefdlg")

            builder.get_object("hscaleWindowWidth").set_adjustment(ncam.w_adj)
            builder.get_object("hscaleTVWidth").set_adjustment(ncam.tv_w_adj)
            builder.get_object("hscaleNameColWidth").set_adjustment(ncam.col_width_adj)

            self.adj_vkbwidth = builder.get_object("adj_vkbwidth")
            self.adj_vkbwidth.set_value(vkb_width)
            self.adj_vkbheight = builder.get_object("adj_vkbheight")
            self.adj_vkbheight.set_value(vkb_height)
            self.vkb_cancel = builder.get_object("vkb_cancel")
            self.vkb_cancel.set_active(vkb_cancel_on_out)

            self.restore_tvstate = builder.get_object("restore_tvstate")
            self.restore_tvstate.set_active(self.restore_expand_state)

            self.imgMenu = builder.get_object("imgMenu")
            self.adj_menuiconsize = builder.get_object("adj_menuiconsize")
            self.adj_menuiconsize.set_value(menu_icon_size)
            self.adj_menuiconsize.connect("value-changed", self.menu_isize)

            self.imgAddMenu = builder.get_object("imgAddMenu")
            self.adj_addmenuiconsize = builder.get_object("adj_addmenuiconsize")
            self.adj_addmenuiconsize.set_value(add_menu_icon_size)
            self.adj_addmenuiconsize.connect("value-changed", self.addmenu_isize)

            self.imgToolbar = builder.get_object("imgMToolBar")
            self.adj_tbIconSize = builder.get_object("adj_tbIconSize")
            self.adj_tbIconSize.set_value(toolbar_icon_size)
            self.adj_tbIconSize.connect("value-changed", self.toolbar_isize)

            self.imgHistTB = builder.get_object("imgHistTB")
            self.adj_histiconsize = builder.get_object("adj_histiconsize")
            self.adj_histiconsize.set_value(quick_access_icon_size)
            self.adj_histiconsize.connect("value-changed", self.imgHist_isize)

            self.imgTV = builder.get_object("imgTV")
            self.adj_tviconsize = builder.get_object("adj_tviconsize")
            self.adj_tviconsize.set_value(treeview_icon_size)
            self.adj_tviconsize.connect("value-changed", self.tv_isize)

            self.imgAddDlg = builder.get_object("imgAddDlg")
            self.adj_adddlgimgsize = builder.get_object("adj_adddlgimgsize")
            self.adj_adddlgimgsize.set_value(add_dlg_icon_size)
            self.adj_adddlgimgsize.connect("value-changed", self.adddlg_isize)

            self.comboProbe = builder.get_object("comboProbe")
            self.comboProbe.set_active(get_int(self.ngc_probe_func) - 2)
            self.adj_probelatch = builder.get_object("adj_probelatch")
            self.adj_probelatch.set_value(get_float(self.ngc_probe_latch))
            self.adj_probelatchfeed = builder.get_object("adj_probelatchfeed")
            self.adj_probelatchfeed.set_value(get_float(self.ngc_probe_latch_feed))
            self.adj_probefeed = builder.get_object("adj_probefeed")
            self.adj_probefeed.set_value(get_float(self.ngc_probe_feed))
            self.adj_probesafe = builder.get_object("adj_probesafe")
            self.adj_probesafe.set_value(get_float(self.ngc_probe_safe))
            self.adj_probedia = builder.get_object("adj_probedia")
            self.adj_probedia.set_value(get_float(self.ngc_probe_tip_dia))

            self.adj_centerdrill_depth = builder.get_object("adj_centerdrill_depth")
            self.adj_centerdrill_depth.set_value(get_float(self.drill_center_depth))

            self.finalcut_chk = builder.get_object("finalcut_chk")
            self.finalcut_chk.set_active(self.ngc_show_final_cut == '1')
            self.finalbottom_chk = builder.get_object("finalbottom_chk")
            self.finalbottom_chk.set_active(self.ngc_show_bottom_cut == '1')
            self.finalcut_chk.connect("toggled", self.ref_clicked)
            self.finalbottom_lbl = builder.get_object("label29")

            self.comboCoords = builder.get_object("comboCoords")
            self.comboCoords.set_active(self.ngc_off_rot_coord_system)

            self.adj_timeout_value = builder.get_object("adj_timeout_value")
            self.adj_timeout_value.set_value(self.timeout_value)
            self.adj_digits = builder.get_object("adj_digits")
            self.adj_digits.set_value(get_float(default_digits))
            self.adj_spindledelay = builder.get_object("adj_spindledelay")
            self.adj_spindledelay.set_value(get_float(self.ngc_spindle_speedup_time))

            self.adj_gmoccapy = builder.get_object("adj_gmoccapy")
            self.adj_gmoccapy.set_value(gmoccapy_time_out)

            self.dev_chk = builder.get_object("dev_chk")
            self.dev_chk.set_active(developer_menu)
            self.tlo_chk = builder.get_object("tlo_chk")
            self.tlo_chk.set_active(self.probe_tool_len_comp == '1')

            self.entryInit = builder.get_object("entryInit")
            self.entryInit.set_text(self.ngc_init_str)
            self.entryPost = builder.get_object("entryPost")
            self.entryPost.set_text(self.ngc_post_amble)

            self.combo_pck = builder.get_object("combo_pck")
            self.combo_pck.set_active(get_int(self.pocket_mode))

            self.adj_opt_eng1 = builder.get_object("adj_opt_eng1")
            self.adj_opt_eng1.set_value(get_float(self.opt_eng1))
            self.adj_opt_eng2 = builder.get_object("adj_opt_eng2")
            self.adj_opt_eng2.set_value(get_float(self.opt_eng2))
            self.adj_opt_eng3 = builder.get_object("adj_opt_eng3")
            self.adj_opt_eng3.set_value(get_float(self.opt_eng3))

            self.adj_opt_ff1 = builder.get_object("adj_opt_ff1")
            self.adj_opt_ff1.set_value(get_float(self.opt_ff1))
            self.adj_opt_ff2 = builder.get_object("adj_opt_ff2")
            self.adj_opt_ff2.set_value(get_float(self.opt_ff2))
            self.adj_opt_ff3 = builder.get_object("adj_opt_ff3")
            self.adj_opt_ff3.set_value(get_float(self.opt_ff3))
            self.adj_opt_ff4 = builder.get_object("adj_opt_ff4")
            self.adj_opt_ff4.set_value(get_float(self.opt_ff4))
            self.adj_opt_ff0 = builder.get_object("adj_opt_ff0")
            self.adj_opt_ff0.set_value(get_float(self.opt_ff0))

            self.adj_opt_sf1 = builder.get_object("adj_opt_sf1")
            self.adj_opt_sf1.set_value(get_float(self.opt_sf1))
            self.adj_opt_sf2 = builder.get_object("adj_opt_sf2")
            self.adj_opt_sf2.set_value(get_float(self.opt_sf2))
            self.adj_opt_sf3 = builder.get_object("adj_opt_sf3")
            self.adj_opt_sf3.set_value(get_float(self.opt_sf3))
            self.adj_opt_sf4 = builder.get_object("adj_opt_sf4")
            self.adj_opt_sf4.set_value(get_float(self.opt_sf4))
            self.adj_opt_sf0 = builder.get_object("adj_opt_sf0")
            self.adj_opt_sf0.set_value(get_float(self.opt_sf0))

            builder.get_object("buttonSave").connect('clicked', self.save_click)
            prefdlg.set_transient_for(parent)
            prefdlg.set_keep_above(True)
            prefdlg.show_all()
            self.ref_clicked()
            self.menu_isize()
            self.tv_isize()
            self.adddlg_isize()
            self.imgHist_isize()
            self.addmenu_isize()
            self.toolbar_isize()
            prefdlg.run()
            prefdlg.destroy()
        except :
            raise IOError(_("Expected file not found : ncam_pref.glade"))

    def tv_isize(self, *args):
        self.imgTV.set_from_pixbuf(get_pixbuf('circle.png', int(self.adj_tviconsize.get_value())))

    def adddlg_isize(self, *args):
        self.imgAddDlg.set_from_pixbuf(get_pixbuf('circle.png', int(self.adj_adddlgimgsize.get_value())))

    def imgHist_isize(self, *args):
        self.imgHistTB.set_from_pixbuf(get_pixbuf('circle.png', int(self.adj_histiconsize.get_value())))

    def addmenu_isize(self, *args):
        self.imgAddMenu.set_from_pixbuf(get_pixbuf('circle.png', int(self.adj_addmenuiconsize.get_value())))

    def toolbar_isize(self, *args):
        self.imgToolbar.set_from_stock('gtk-save', int(self.adj_tbIconSize.get_value()))

    def menu_isize(self, *args):
        self.imgMenu.set_from_stock('gtk-new', int(self.adj_menuiconsize.get_value()))

    def ref_clicked(self, *args):
        self.finalbottom_chk.set_sensitive(self.finalcut_chk.get_active())
        self.finalbottom_lbl.set_sensitive(self.finalcut_chk.get_active())

    def save_click(self, *args):
        global default_digits, default_metric, add_menu_icon_size, \
            add_dlg_icon_size, quick_access_icon_size, menu_icon_size, \
            treeview_icon_size, vkb_width, vkb_height, vkb_cancel_on_out, \
            toolbar_icon_size, gmoccapy_time_out, developer_menu

        self.w_adj_value = self.ncam.w_adj.get_value()
        self.col_width_adj_value = self.ncam.col_width_adj.get_value()
        self.tv_w_adj_value = self.ncam.tv_w_adj.get_value()

        vkb_width = int(self.adj_vkbwidth.get_value())
        vkb_height = int(self.adj_vkbheight.get_value())
        vkb_cancel_on_out = self.vkb_cancel.get_active()

        self.restore_expand_state = self.restore_tvstate.get_active()
        menu_icon_size = int(self.adj_menuiconsize.get_value())
        toolbar_icon_size = int(self.adj_tbIconSize.get_value())
        add_menu_icon_size = int(self.adj_addmenuiconsize.get_value())
        quick_access_icon_size = int(self.adj_histiconsize.get_value())
        treeview_icon_size = int(self.adj_tviconsize.get_value())
        add_dlg_icon_size = int(self.adj_adddlgimgsize.get_value())

        gmoccapy_time_out = self.adj_gmoccapy.get_value()
        developer_menu = self.dev_chk.get_active()

        self.ngc_probe_func = str(self.comboProbe.get_active() + 2)
        self.ngc_probe_latch = str(self.adj_probelatch.get_value())
        self.ngc_probe_latch_feed = str(self.adj_probelatchfeed.get_value())
        self.ngc_probe_feed = str(self.adj_probefeed.get_value())
        self.ngc_probe_safe = str(self.adj_probesafe.get_value())
        self.ngc_probe_tip_dia = str(self.adj_probedia.get_value())
        self.probe_tool_len_comp = '1' if self.tlo_chk.get_active() else '0'

        self.drill_center_depth = str(self.adj_centerdrill_depth.get_value())

        self.ngc_show_final_cut = '1' if self.finalcut_chk.get_active() else '0'
        self.ngc_show_bottom_cut = '1' if self.finalbottom_chk.get_active() else '0'

        self.ngc_off_rot_coord_system = self.comboCoords.get_active()

        self.timeout_value = self.adj_timeout_value.get_value()
        default_digits = str(int(self.adj_digits.get_value()))
        self.ngc_spindle_speedup_time = str(self.adj_spindledelay.get_value())

        self.ngc_init_str = self.entryInit.get_text()
        self.ngc_post_amble = self.entryPost.get_text()

        self.pocket_mode = str(self.combo_pck.get_active())

        self.opt_eng1 = str(self.adj_opt_eng1.get_value())
        self.opt_eng2 = str(self.adj_opt_eng2.get_value())
        self.opt_eng3 = str(self.adj_opt_eng3.get_value())

        self.opt_ff1 = str(self.adj_opt_ff1.get_value())
        self.opt_ff2 = str(self.adj_opt_ff2.get_value())
        self.opt_ff3 = str(self.adj_opt_ff3.get_value())
        self.opt_ff4 = str(self.adj_opt_ff4.get_value())
        self.opt_ff0 = str(self.adj_opt_ff0.get_value())

        self.opt_sf1 = str(self.adj_opt_sf1.get_value())
        self.opt_sf2 = str(self.adj_opt_sf2.get_value())
        self.opt_sf3 = str(self.adj_opt_sf3.get_value())
        self.opt_sf4 = str(self.adj_opt_sf4.get_value())
        self.opt_sf0 = str(self.adj_opt_sf0.get_value())

        self.save_layout()
        self.save_defaults()
        self.create_defaults()
        self.ncam.autorefresh_call()

    def create_defaults(self):
        s = ("(*** GCode generated by NativeCAM for LinuxCNC ***)\n\n" +
            "(*.ngc files are best viewed with Syntax Highlighting)\n" +
            "(visit https://forum.linuxcnc.org/forum/20-g-code/30840-new-syntax-highlighting-for-gedit)\n" +
            "(or https://github.com/FernV/Gcode-highlight-for-Kate)\n\n")

        self.default = s
        self.default += (self.ngc_init_str + "\n")
        if default_metric :
            self.default += "G21  (metric)"
        else :
            self.default += "G20  (imperial/inches)"

        if self.ngc_off_rot_coord_system < 5 :
            coord = str(5 + self.ngc_off_rot_coord_system)
        else :
            coord = '9.' + str(self.ngc_off_rot_coord_system - 4)
        self.default += ("\n\n#<_off_rot_coord_system>   = 5" + coord + "\n\n")
        self.default += ("#<_spindle_speed_up_delay> = " + self.ngc_spindle_speedup_time + "\n\n")
        self.default += ("#<_show_final_cuts>     = " + self.ngc_show_final_cut + "\n")
        self.default += ("#<_show_bottom_cut>     = " + self.ngc_show_bottom_cut + "\n\n")

        self.default += ("#<_probe_func>          = 38." + self.ngc_probe_func + "\n")
        self.default += ("#<_probe_feed>          = " + self.ngc_probe_feed + "\n")
        self.default += ("#<_probe_latch>         = " + self.ngc_probe_latch + "\n")
        self.default += ("#<_probe_latch_feed>    = " + self.ngc_probe_latch_feed + "\n")
        self.default += ("#<_probe_safe>          = " + self.ngc_probe_safe + "\n")
        self.default += ("#<_probe_tip_dia>       = " + self.ngc_probe_tip_dia + "\n\n")

        self.default += ("#<_probe_tool_len_comp> = " + self.probe_tool_len_comp + "\n")
        self.default += ("#<_tool_probe_z>        = 0\n\n")

        self.default += ("#<center_drill_depth>   = " + self.drill_center_depth + "\n\n")

        self.default += ("#<_units_radius>        = 1  (factor for radius and diameter)\n")
        self.default += ("#<_units_width>         = 1  (factor for width, height, length)\n")
        self.default += ("#<_units_cut_depth>     = 1  (factor for depth)\n\n")

        self.default += ("#<in_polyline>          = 0\n")
        self.default += ("#<_mill_data_start>     = 70\n\n")

        self.default += ("#<_pocket_expand_mode>  = " + self.pocket_mode + "\n\n")

        self.default += ("(optimization values)\n")
        self.default += ("#<_tool_eng1>           = " + self.opt_eng1 + "\n")
        self.default += ("#<_tool_eng2>           = " + self.opt_eng2 + "\n")
        self.default += ("#<_tool_eng3>           = " + self.opt_eng3 + "\n\n")

        self.default += ("#<_feedfactor1>         = " + self.opt_ff1 + "\n")
        self.default += ("#<_feedfactor2>         = " + self.opt_ff2 + "\n")
        self.default += ("#<_feedfactor3>         = " + self.opt_ff3 + "\n")
        self.default += ("#<_feedfactor4>         = " + self.opt_ff4 + "\n")
        self.default += ("#<_feedfactor0>         = " + self.opt_ff0 + "\n\n")

        self.default += ("#<_speedfactor1>        = " + self.opt_sf1 + "\n")
        self.default += ("#<_speedfactor2>        = " + self.opt_sf2 + "\n")
        self.default += ("#<_speedfactor3>        = " + self.opt_sf3 + "\n")
        self.default += ("#<_speedfactor4>        = " + self.opt_sf4 + "\n")
        self.default += ("#<_speedfactor0>        = " + self.opt_sf0 + "\n\n")

        self.default += ("(end defaults)\n\n")

        self.default += ('(This is a built-in safety to help avoid gouging into your work piece)\n')
        self.default += ("/ o<safety_999> repeat [1000]\n")
        self.default += ("/     M0\n")
        self.default += ("/ o<safety_999> endrepeat\n\n")

        self.default += ('(sub definitions)\n')

        self.post_amble = self.ngc_post_amble + "\nM2\n"

class NCam(gtk.VBox):
    __gtype_name__ = "NCam"
    __gproperties__ = {}
    __gproperties = __gproperties__

    def usage(self):
        print """
Standalone Usage:
   ncam [Options]

Options:
   -h | --help                            this text
   -i inifilename | --ini=inifilename     inifile for standalone

Notes:
  For standalone usage:
     a) Specify an inifile name which specifies [DISPLAY]NCAM_DIR
  or
     b) Start in a working directory with pre-existing NativeCAM subdirs
        or if NativeCAM subdirs are not present, they will be created
"""

    def __init__(self, *a, **kw):
        global NCAM_DIR, default_metric, NGC_DIR, SYS_DIR

#        exit_if_invalid_rip_standalone()

        # process passed args
        # x needed for gmoccapy
        # U needed for embedded
        # c also needed

        opt, optl = 'U:h:i:t:c:d:x', ["help", "catalog=", "ini="]
        optlist, args = getopt.getopt(sys.argv[1:], opt, optl)
        optlist = dict(optlist)

        if "-U" in optlist :
            optlist_, args = getopt.getopt(optlist["-U"].split(), opt, optl)
            optlist.update(optlist_)

        if "-h" in optlist or "--help" in optlist:
            self.usage()
            sys.exit(0)

        if "-d" in optlist :  # used when developping with Eclipse and pydev in rip mode
            import platform
            if platform.system() != 'Windows' :
                SYS_DIR = os.path.expanduser('~/linuxcnc-dev/share/ncam')

        self.editor = DEFAULT_EDITOR
        self.pref = Preferences()
        self.tools = Tools()
        self.catalog_dir = DEFAULT_CATALOG

        ini = os.getenv("INI_FILE_NAME")
        if "-i" in optlist :
            ini = optlist["-i"]
        elif "--ini" in optlist :
            ini = optlist["--ini"]

        if (ini is None) :
            # standalone with no -i:
            # beware, files expected/created in this dir
            inifilename = 'NA'
            NCAM_DIR = SYS_DIR
            NGC_DIR = os.path.expanduser('~')
        else :
            try :
                inifilename = os.path.abspath(ini)
                ini_instance = linuxcnc.ini(ini)
            except Exception, detail :
                err_exit(_("Open fails for cfg file : %(inifilename)s\n\n%(detail)s") % \
                           {'inifilename':inifilename, 'detail':detail})

            require_ini_items(inifilename, ini_instance)

            val = ini_instance.find('DISPLAY', 'DISPLAY')
            if val is None :
                msg = (_('not found: [DISPLAY]DISPLAY]'))
                print msg
                mess_dlg (msg)
            elif val not in ['axis', 'gmoccapy'] :
                mess_dlg(_('Display can only be "axis" or "gmoccapy"'))
                sys.exit(-1)

            val = ini_instance.find('DISPLAY', 'LATHE')
            if (val is not None) and (val != '0'):
                self.catalog_dir = 'lathe'

            self.pref.ngc_init_str = ini_instance.find('RS274NGC', 'RS274NGC_STARTUP_CODE')
            if self.pref.ngc_init_str is None :
                self.pref.ngc_init_str = ini_instance.find('HALUI', 'MDI_COMMAND')

            val = ini_instance.find('EMCIO', 'TOOL_TABLE')
            self.tools.set_file(os.path.join(os.path.dirname(ini), val))

            default_metric = ini_instance.find('TRAJ', 'LINEAR_UNITS') in ['mm', 'metric']

            val = ini_instance.find('DISPLAY', 'EDITOR')
            if val is not None :
                self.editor = val

        print ""
        print "NativeCAM info:"
        print "    inifile =", inifilename
        print "    SYS_DIR =", SYS_DIR
        print "   NCAM_DIR =", NCAM_DIR
        print "    program =", __file__
        print "  real path =", os.path.realpath(__file__)
        print ""

        fromdirs = [CATALOGS_DIR, CFG_DIR, LIB_DIR,
                    GRAPHICS_DIR, XML_DIR]

        if ini is None :
            self.ask_to_create_standalone(fromdirs)

        # first use:copy, subsequent: update
        if ini is not None :
            if SYS_DIR != NCAM_DIR :
                self.update_user_tree(fromdirs, NCAM_DIR)
            require_ncam_lib(inifilename, ini_instance)

        self.tools.load_table()
        self.LinuxCNC_connected = False
        self.treestore_selected = None
        self.selected_feature = None
        self.selected_feature_path = None
        self.selected_type = None
        self.iter_selected_type = tv_select.none
        self.treeview2 = None
        self.focused_widget = None

        self.embedded = True
        self.undo_list = []
        self.undo_pointer = -1
        self.timeout = None

        if "--catalog" in optlist :
            self.catalog_dir = optlist["--catalog"]

        catname = 'menu-custom.xml'
        cat_dir_name = search_path(0, catname, CATALOGS_DIR, self.catalog_dir)
        if cat_dir_name is not None :
            print(_('Using menu-custom.xml\n'))
        else :
            catname = 'menu.xml'
            cat_dir_name = search_path(2, catname, CATALOGS_DIR, self.catalog_dir)
            print(_('Using standard menu.xml, no menu-custom.xml found\n'))
        if cat_dir_name is None :
            sys.exit(1)

        xml = etree.parse(cat_dir_name)
        self.catalog = xml.getroot()

        # main_window
        gtk.VBox.__init__(self, *a, **kw)
        self.builder = gtk.Builder()
        try :
            self.builder.add_from_file(os.path.join(SYS_DIR, "ncam.glade"))
        except :
            raise IOError(_("Expected file not found : ncam.glade"))

        self.pref.read(NCAM_DIR + '/' + CATALOGS_DIR, self.catalog_dir)

        if "-t" in optlist :
            # do after self.pref.read()
            # get translations and exit
            self.get_translations()
            sys.exit(0)

        self.get_widgets()

        self.on_scale_change_value(self)

        self.treestore = gtk.TreeStore(object, str, gobject.TYPE_BOOLEAN,
                                       gobject.TYPE_BOOLEAN)
        self.master_filter = self.treestore.filter_new()

        self.details_filter = self.treestore.filter_new()
        self.details_filter.set_visible_column(3)

        self.create_treeview()
        self.main_box.reparent(self)

        self.quick_access_tb = gtk.Toolbar()
        self.main_box.pack_start(self.quick_access_tb, False, False, 0)
        self.quick_access_tb.set_style(gtk.TOOLBAR_ICONS)
        self.quick_access_tb.set_icon_size(menu_icon_size)
        self.main_box.reorder_child(self.quick_access_tb, 1)

        self.create_menu_interface()
        self.setup_quick_access_tb()
        self.create_add_dialog()

        self.builder.connect_signals(self)
        self.actionSingleView.set_active(not self.pref.use_dual_views)
        self.actionDualView.set_active(self.pref.use_dual_views)
        self.actionTopBottom.set_active(not self.pref.side_by_side)
        self.actionSideBySide.set_active(self.pref.side_by_side)
        self.actionSubHdrs.set_active(self.pref.sub_hdrs_in_tv1)

        self.menu_new_activate(self, 1)
        self.get_selected_feature(self)
        self.show_all()
        self.addVBox.hide()
        self.feature_pane.set_size_request(int(self.tv_w_adj.value), 100)

        self.set_layout(self.pref.use_dual_views)
        self.treeview.grab_focus()
        self.clipboard = gtk.clipboard_get(gtk.gdk.SELECTION_CLIPBOARD)

    def ask_to_create_standalone(self, fromdirs) :
        dir_exists = False
        for d in fromdirs:
            if os.path.isdir(os.path.join(NCAM_DIR, d)) :
                dir_exists = True
                break
        if not dir_exists:
            msg = _('Standalone Directory :\n\n%(dir)s\n\nContinue?') % {'dir':NCAM_DIR}
            if not mess_yesno(msg, title = _("NativeCAM CREATE")) :
                sys.exit(0)

    def update_user_tree(self, fromdirs, todir):
        if not os.path.isdir(NGC_DIR) :
            os.makedirs(NGC_DIR, 0755)

        # copy system files to user, make dirs if necessary
        mode = copymode.one_at_a_time
        for d in fromdirs:
            update_ct = 0
            dir_exists = os.path.isdir(os.path.join(NCAM_DIR, d))
            mode, update_ct = copy_dir_recursive(os.path.join(SYS_DIR, d), os.path.join(todir, d),
                                      update_ct = 0,
                                      mode = mode,
                                      overwrite = False,
                                      verbose = False
                                      )
            if dir_exists:
                fmt2 = _('Updated %(qty)3d files in %(dir)s')
            else :
                fmt2 = _('Created %(qty)3d files in %(dir)s')

            print (fmt2 % {'qty':update_ct, 'dir':NCAM_DIR.rstrip('/') + '/' + d.lstrip('/')})
        print('')

    def create_mi(self, _action):
        mi = _action.create_menu_item()
        mi.set_image(_action.create_icon(menu_icon_size))
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
        self.dev_separator = gtk.SeparatorMenuItem()
        ed_menu.append(self.dev_separator)

        self.set_digits_mi = gtk.MenuItem(_('Set digits'))
        self.set_digits_mi.set_size_request(-1, add_menu_icon_size)
        ed_menu.append(self.set_digits_mi)
        sub_menu = gtk.Menu()
        self.set_digits_mi.set_submenu(sub_menu)

        i = 1
        while i < 7 :
            menu_item = gtk.MenuItem(str(i))
            menu_item.set_size_request(-1, add_menu_icon_size)
            menu_item.connect("activate", self.chng_dt_activate, i)
            sub_menu.append(menu_item)
            i = i + 1

        self.chng_dt_sep = gtk.SeparatorMenuItem()
        ed_menu.append(self.chng_dt_sep)

        ed_menu.append(self.create_mi(self.actionEditFeature))
        ed_menu.append(self.create_mi(self.actionReloadFeature))

        v_menu = gtk.Menu()
        view_menu = gtk.MenuItem(_("_View"))
        view_menu.set_submenu(v_menu)
        menu_bar.append(view_menu)

        v_menu.append(self.create_mi(self.actionCollapse))
        v_menu.append(gtk.SeparatorMenuItem())

        mi = self.actionSingleView.create_menu_item()
        mi.set_size_request(-1, menu_icon_size)
        v_menu.append(mi)

        mi = self.actionDualView.create_menu_item()
        mi.set_size_request(-1, menu_icon_size)
        v_menu.append(mi)

        v_menu.append(gtk.SeparatorMenuItem())

        mi = self.actionTopBottom.create_menu_item()
        mi.set_size_request(-1, menu_icon_size)
        v_menu.append(mi)

        mi = self.actionSideBySide.create_menu_item()
        mi.set_size_request(-1, menu_icon_size)
        v_menu.append(mi)

        v_menu.append(gtk.SeparatorMenuItem())

        mi = self.actionHideValCol.create_menu_item()
        mi.set_size_request(-1, menu_icon_size)
        v_menu.append(mi)

        mi = self.actionSubHdrs.create_menu_item()
        mi.set_size_request(-1, menu_icon_size)
        v_menu.append(mi)

        v_menu.append(gtk.SeparatorMenuItem())

        v_menu.append(self.create_mi(self.actionSaveLayout))

        self.menuAdd = gtk.Menu()
        add_menu = gtk.MenuItem(_('_Add'))
        add_menu.set_submenu(self.menuAdd)
        menu_bar.append(add_menu)

        self.add_catalog_items()

        menu_utils = gtk.Menu()
        u_menu = gtk.MenuItem(_('_Utilities'))
        u_menu.set_submenu(menu_utils)

        self.auto_refresh = gtk.CheckMenuItem(_('_Auto-Refresh'))
        self.auto_refresh.set_size_request(-1, menu_icon_size)
        menu_utils.append(self.auto_refresh)
        menu_utils.append(gtk.SeparatorMenuItem())

        mi = gtk.ImageMenuItem(_('_Reload tools table'))
        img = gtk.Image()
        img.set_from_pixbuf(get_pixbuf("tool-01.png", add_menu_icon_size))
        mi.set_image(img)
        mi.connect("activate", self.tools.load_table)
        menu_utils.append(mi)

        menu_config = gtk.ImageMenuItem(_('_Preferences'))
        img = gtk.Image()
        img.set_from_stock('gtk-preferences', menu_icon_size)
        menu_config.set_image(img)
        menu_config.connect("activate", self.menu_pref_activate)
        menu_utils.append(menu_config)

        menu_bar.append(u_menu)

        menu_help = gtk.Menu()
        h_menu = gtk.MenuItem(_('_Help'))
        h_menu.set_submenu(menu_help)

        self.menu_tutorial = gtk.ImageMenuItem(_('NativeCAM on YouTube'))
        img = gtk.Image()
        img.set_from_pixbuf(get_pixbuf("youtube.png", add_menu_icon_size))
        self.menu_tutorial.set_image(img)
        self.menu_tutorial.connect("activate", self.menu_tutorial_activate)
        menu_help.append(self.menu_tutorial)

        menu_linuxcnc_home = gtk.ImageMenuItem(_('LinuxCNC Home'))
        img = gtk.Image()
        img.set_from_pixbuf(get_pixbuf("linuxcncicon.png", add_menu_icon_size))
        menu_linuxcnc_home.set_image(img)
        menu_linuxcnc_home.connect("activate", self.menu_html_cnc_activate)
        menu_help.append(menu_linuxcnc_home)

        menu_linuxcnc_forum = gtk.ImageMenuItem(_('LinuxCNC Forum'))
        img = gtk.Image()
        img.set_from_pixbuf(get_pixbuf("linuxcncicon.png", add_menu_icon_size))
        menu_linuxcnc_forum.set_image(img)
        menu_linuxcnc_forum.connect("activate", self.menu_html_forum_activate)
        menu_help.append(menu_linuxcnc_forum)

        menu_cnc_russia = gtk.ImageMenuItem(_('CNC-Club Russia'))
        img = gtk.Image()
        img.set_from_pixbuf(get_pixbuf("cnc-ru.png", add_menu_icon_size))
        menu_cnc_russia.set_image(img)
        menu_cnc_russia.connect("activate", self.menu_html_ru_activate)
        menu_help.append(menu_cnc_russia)
        menu_help.append(gtk.SeparatorMenuItem())

        menu_about = gtk.ImageMenuItem(_('_About'))
        img = gtk.Image()
        img.set_from_stock('gtk-about', menu_icon_size)
        menu_about.set_image(img)
        menu_about.connect("activate", self.menu_about_activate)
        menu_help.append(menu_about)

        menu_bar.append(h_menu)

        self.main_box.pack_start(menu_bar, False, False, 0)
        self.main_box.reorder_child(menu_bar, 0)

        self.menubar = menu_bar

    def chng_dt_activate(self, *args):
        if args[1] < 10 :
            self.treestore.get(self.selected_param, 0)[0].attr["digits"] = str(args[1])
            fmt = '{0:0.' + str(args[1]) + 'f}'
            v = get_float(self.treestore.get(self.selected_param, 0)[0].get_value())
            self.treestore.get(self.selected_param, 0)[0].set_value(fmt.format(v))

        elif args[1] == 11 :
            self.treestore.get(self.selected_param, 0)[0].attr["type"] = 'string'
        self.selected_type = self.treestore.get(self.selected_param, 0)[0].attr["type"]
        self.action()

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
        if txt and txt.find(XML_TAG) > -1 :
            xml = etree.fromstring(txt)
            self.import_xml(xml)

    def menu_edit_activate(self, *args):
        txt = self.clipboard.wait_for_text()
        if txt:
            self.actionPaste.set_sensitive(txt.find(XML_TAG) > -1)
        else:
            self.actionPaste.set_sensitive(False)

        self.actionEditFeature.set_sensitive(self.selected_feature is not None)
        self.actionReloadFeature.set_sensitive(self.selected_feature is not None)
        self.actionEditFeature.set_visible(developer_menu)
        self.actionReloadFeature.set_visible(developer_menu)
        self.dev_separator.set_visible(developer_menu)

        self.chng_dt_sep.set_visible(self.selected_type == 'float')
        self.set_digits_mi.set_visible(self.selected_type == 'float')

    def menu_html_cnc_activate(self, *args):
        webbrowser.open('http://www.linuxcnc.org')

    def menu_html_forum_activate(self, *args):
        webbrowser.open('http://www.linuxcnc.org/index.php/english/forum/40-subroutines-and-ngcgui')

    def menu_html_ru_activate(self, *args):
        webbrowser.open('www.cnc-club.ru')

    def edit_feature(self, *args):
        subprocess.Popen(self.editor + ' ' +
                         self.selected_feature.get_attr('src'),
                         shell = True, stdin = open(os.devnull, 'r'))

    def menu_tutorial_activate(self, *args):
        webbrowser.open('https://www.youtube.com/channel/UCjOe4VxKL86HyVrshTmiUBQ')

    def btn_cancel_add_clicked(self, *args):
        self.addVBox.hide()
        self.feature_Hpane.show()
        self.menubar.set_sensitive(True)
        self.button_tb.set_sensitive(True)
        self.quick_access_tb.set_sensitive(True)

    def on_add_iconview_key_press(self, widget, event):
        keyname = gtk.gdk.keyval_name(event.keyval)
        if keyname == ['Escape'] :
            self.btn_cancel_add_clicked()
            event.keyval = 0

    def create_add_dialog(self):
        self.icon_store = gtk.ListStore(gtk.gdk.Pixbuf, str, str, str, int, str)
        self.add_iconview.set_model(self.icon_store)
        self.add_iconview.set_pixbuf_column(0)
        self.add_iconview.set_text_column(2)
        self.catalog_path = self.catalog
        self.updating_catalog = True
        self.update_catalog(xml = self.catalog_path)
        self.updating_catalog = False

    def catalog_activate(self, iconview):
        if not self.updating_catalog :
            lst = iconview.get_selected_items()
            if lst is not None and (len(lst) > 0) :
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
                    self.quick_access_tb.set_sensitive(True)
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
                add_dlg_icon_size), "parent", _('Back...'), "parent", 0, None])

        for path in range(len(self.catalog_path)) :
            p = self.catalog_path[path]
            if p.tag.lower() in ["group", "sub", "import"] :
                name = p.get('name') if "name" in p.keys() else 'Un-named'
                src = p.get("src") if "src" in p.keys() else None
                tooltip = p.get('tool_tip') if "tool_tip" in p.keys() else None
                self.icon_store.append([get_pixbuf(p.get("icon"),
                    add_dlg_icon_size), p.tag.lower(), name, src, path, tooltip])

    def add_button_clicked(self, *arg) :
        self.feature_Hpane.hide()
        self.addVBox.show()
        self.menubar.set_sensitive(False)
        self.button_tb.set_sensitive(False)
        self.quick_access_tb.set_sensitive(False)
        self.add_iconview.grab_focus()

    def quick_access_tb_clicked(self, call, src) :
        self.add_feature(src)

    def setup_quick_access_tb(self) :
        self.config_quick_access = ConfigParser.ConfigParser()
        fname = search_path(1, QUICK_ACCESS_FNAME, CATALOGS_DIR, self.catalog_dir)
        self.quick_access_dict = {}
        if fname is not None :
            self.config_quick_access.read(fname)
            quick_access = self.config_quick_access.get("VAR", "quick_access", raw = True)
            quick_access = quick_access.split("\n")
            for s in quick_access :
                s = s.split("\t")
                if len(s) == 4 :
                    self.quick_access_dict[s[0]] = [int(s[1]), float(s[2]), int(s[3])]
        else :
            quick_access = ""

        feature_list = [s.get("src") for s in self.catalog.findall(".//sub") if "src" in s.keys()]
        self.quick_access = {}
        self.quick_access_buttons = {}
        self.quick_access_topbuttons = {}

        for src in feature_list :
            try :
                src_file = search_path(1, src, CFG_DIR)
                if src_file is None:
                    continue
                f = Feature(src_file)
                icon = gtk.Image()
                icon.set_from_pixbuf(get_pixbuf(f.get_attr("icon"),
                        quick_access_icon_size))
                button = gtk.ToolButton(icon, label = f.get_attr("name"))
                button.set_tooltip_markup(f.get_attr("help"))
                button.connect("clicked", self.quick_access_tb_clicked, src_file)
                self.quick_access_buttons[src_file] = button

                icon = gtk.Image()
                icon.set_from_pixbuf(get_pixbuf(f.get_attr("icon"),
                        quick_access_icon_size))
                button1 = gtk.ToolButton(icon, label = f.get_attr("name"))
                button1.set_tooltip_markup(f.get_attr("help"))
                button1.connect("clicked", self.quick_access_tb_clicked, src_file)
                self.quick_access_topbuttons[src_file] = button1

                self.quick_access[src_file] = [button, button1, 0, 0, 0]
                if src_file in self.quick_access_dict :
                    self.quick_access[src_file][2:] = self.quick_access_dict[src_file]
            except :
                pass

        tf = self.quick_access.items()
        tf.sort(lambda x, y:-1 if x[1][4] - y[1][4] > 0 else 1)  # sort by selected order
        for tfi in tf[:MAX_QUICK_ACCESS_BTN] :
            self.quick_access_tb.insert(tfi[1][1], -1)

    def update_quick_access(self, src):
        if src in self.quick_access :
            self.quick_access[src][2] += 1
            self.quick_access[src][3] = time.time()

        if src is not None:
            # save config
            if "INFO" not in self.config_quick_access.sections() :
                self.config_quick_access.add_section('INFO')
                self.config_quick_access.set('INFO', 'warning',
                    'This file is rewritten every time a subroutine is used and shows usage of each one\n'
                    'columns are : source, total usage, last time used and order of preference\n'
                    'Delete this file to reset or edit rank in descending order')

            if "VAR" not in self.config_quick_access.sections() :
                self.config_quick_access.add_section('VAR')

            for sr in self.quick_access :
                self.quick_access_dict[sr] = self.quick_access[sr][2:]
            quick_access = ""
            for sr in self.quick_access_dict :
                quick_access += "\n%s\t%s\t%s\t%s" % (sr,
                        self.quick_access_dict[sr][0],
                        self.quick_access_dict[sr][1],
                        self.quick_access_dict[sr][2])
            self.config_quick_access.set("VAR", "quick_access", quick_access)

            fname = os.path.join(NCAM_DIR, CATALOGS_DIR, self.catalog_dir, QUICK_ACCESS_FNAME)
            try :
                self.config_quick_access.write(open(fname, "w"))
            except :
                mess_dlg(_("WARNING:\nCannot write to quick_access file %(filename)s") % {'filename':fname})

    def add_catalog_items(self):

        def add_to_menu(grp_menu, path) :
            for ptr in range(len(path)) :
                try :
                    p = path[ptr]
                    if p.tag.lower() == "group":
                        name = p.get("name") if "name" in p.keys() else None
                        a_menu_item = gtk.ImageMenuItem(_(name))

                        tooltip = p.get("tool_tip") if "tool_tip" in p.keys() else None
                        if (tooltip is not None) and (tooltip != '') :
                            a_menu_item.set_tooltip_text(_(tooltip))

                        img = gtk.Image()
                        img.set_from_pixbuf(get_pixbuf(p.get("icon"), add_menu_icon_size))
                        a_menu_item.set_image(img)

                        grp_menu.append(a_menu_item)
                        a_menu = gtk.Menu()
                        a_menu_item.set_submenu(a_menu)
                        add_to_menu(a_menu, p)

                    elif p.tag.lower() == "separator":
                        grp_menu.append(gtk.SeparatorMenuItem())

                    elif p.tag.lower() in ["sub", "import"] :
                        name = p.get("name") if "name" in p.keys() else None
                        a_menu_item = gtk.ImageMenuItem(_(name))

                        tooltip = p.get("tool_tip") if "tool_tip" in p.keys() else None
                        if (tooltip is not None) and (tooltip != '') :
                            a_menu_item.set_tooltip_text(_(tooltip))

                        img = gtk.Image()
                        img.set_from_pixbuf(get_pixbuf(p.get("icon"), add_menu_icon_size))
                        a_menu_item.set_image(img)

                        src = p.get("src") if "src" in p.keys() else None
                        if (src is not None) and (src != ''):
                            a_menu_item.connect("activate", self.add_feature_menu, src)

                        grp_menu.append(a_menu_item)
                except:
                    pass

        try :
            add_to_menu(self.menuAdd, self.catalog)
        except :
            mess_dlg(_('Problem adding catalog to menu'))

        self.menuAdd.append(gtk.SeparatorMenuItem())

        menu_importXML = gtk.ImageMenuItem(_('Import _XML file'))
        img = gtk.Image()
        img.set_from_stock('gtk-revert-to-saved', menu_icon_size)
        menu_importXML.set_image(img)
        menu_importXML.connect("activate", self.menu_import_activate)
        self.menuAdd.append(menu_importXML)

        menu_item = gtk.ImageMenuItem(_('_Open cfg file'))
        img = gtk.Image()
        img.set_from_stock('gtk-open', menu_icon_size)
        menu_item.set_image(img)
        menu_item.connect("activate", self.menu_open_cfg_activate)
        menu_item.set_tooltip_text(_("Select a valid or prototype cfg file"))
        self.menuAdd.append(menu_item)

    def create_treeview(self):
        self.treeview = gtk.TreeView(self.treestore)
        self.treeview.set_grid_lines(gtk.TREE_VIEW_GRID_LINES_VERTICAL)
        self.treeview.set_size_request(int(self.col_width_adj.value) + 50, -1)

        self.builder.get_object("feat_scrolledwindow").add(self.treeview)

        self.treeview.add_events(gtk.gdk.BUTTON_PRESS_MASK)
        self.treeview.connect('button-press-event', self.pop_menu)

        self.treeview.connect('row_activated', self.tv_row_activated)
        self.treeview.connect('key_press_event', self.tv_key_pressed_event)

        # icon and name
        col = gtk.TreeViewColumn(_("Name"))
        col.set_min_width(int(self.col_width_adj.value))
        cell = gtk.CellRendererPixbuf()
        cell.set_fixed_size(treeview_icon_size, treeview_icon_size)
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
        self.edit_cell.edited = self.edited
        self.edit_cell.set_preediting(self.get_editinfo)
        self.edit_cell.set_edited_user_fn(self.edited_user)
        self.edit_cell.set_treeview(self.treeview)
        col.pack_start(self.edit_cell, expand = True)
        col.set_cell_data_func(self.edit_cell, self.get_col_value)
        col.set_resizable(True)
        col.set_min_width(int(self.col_width_adj.value))
        self.treeview.append_column(col)

        self.treeview.set_tooltip_column(1)
        self.treeview.connect("cursor-changed", self.get_selected_feature)

        self.treeview.set_model(self.master_filter)
        self.treeview.set_size_request(int(self.col_width_adj.value), 100)

    def save_work(self, *args):
        fname = os.path.join(NCAM_DIR, CATALOGS_DIR, self.catalog_dir, CURRENT_WORK)
        if self.treestore.get_iter_root() is not None :
            xml = self.treestore_to_xml()
            etree.ElementTree(xml).write(fname, pretty_print = True)
        else :
            if os.path.isfile(fname) :
                os.remove(fname)

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

                if dt == 'float' :
                    menu_digits = gtk.MenuItem(_('Set digits'))
                    menu_digits.set_size_request(-1, add_menu_icon_size)
                    self.pop_up.append(menu_digits)
                    sub_menu = gtk.Menu()
                    menu_digits.set_submenu(sub_menu)

                    i = 1
                    while i < 7 :
                        menu_item = gtk.MenuItem(str(i))
                        menu_item.set_size_request(-1, add_menu_icon_size)
                        menu_item.connect("activate",
                                    self.pop_set_digits_activate, itr, i)
                        sub_menu.append(menu_item)
                        i = i + 1
                    self.pop_up.append(gtk.SeparatorMenuItem())

                if developer_menu :
                    self.pop_up.append(self.create_mi(self.actionEditFeature))
                    self.pop_up.append(self.create_mi(self.actionReloadFeature))
                    self.pop_up.append(gtk.SeparatorMenuItem())

                self.pop_up.append(self.create_mi(self.actionDuplicate))
                self.pop_up.append(self.create_mi(self.actionDelete))

                self.pop_up.append(gtk.SeparatorMenuItem())

            self.pop_up.append(self.create_mi(self.actionAdd))
            self.pop_up.append(gtk.SeparatorMenuItem())

            mi = self.actionSingleView.create_menu_item()
            mi.set_size_request(-1, menu_icon_size)
            self.pop_up.append(mi)

            mi = self.actionDualView.create_menu_item()
            mi.set_size_request(-1, menu_icon_size)
            self.pop_up.append(mi)

            self.pop_up.append(gtk.SeparatorMenuItem())

            mi = self.actionTopBottom.create_menu_item()
            mi.set_size_request(-1, menu_icon_size)
            self.pop_up.append(mi)

            mi = self.actionSideBySide.create_menu_item()
            mi.set_size_request(-1, menu_icon_size)
            self.pop_up.append(mi)

            self.pop_up.append(gtk.SeparatorMenuItem())

            mi = self.actionHideValCol.create_menu_item()
            mi.set_size_request(-1, menu_icon_size)
            self.pop_up.append(mi)

            mi = self.actionSubHdrs.create_menu_item()
            mi.set_size_request(-1, menu_icon_size)
            self.pop_up.append(mi)

            self.pop_up.show_all()
            self.pop_up.popup(None, None, None, event.button, event.time, None)
            return True

    def pop_set_digits_activate(self, callback = None, *args) :
        self.treestore.get(args[0], 0)[0].attr["digits"] = str(args[1])
        fmt = '{0:0.' + str(args[1]) + 'f}'
        v = get_float(self.treestore.get(args[0], 0)[0].get_value())
        self.treestore.get(args[0], 0)[0].set_value(fmt.format(v))
        self.action()

    def create_second_treeview(self):
        self.treeview2 = gtk.TreeView()
        self.treeview2.add_events(gtk.gdk.BUTTON_PRESS_MASK)
        self.treeview2.connect('button-press-event', self.pop_menu)
        self.treeview2.connect('cursor-changed', self.tv2_selected)
        self.treeview2.set_grid_lines(gtk.TREE_VIEW_GRID_LINES_VERTICAL)
        self.treeview2.set_show_expanders(False)

        # icon and name
        col = gtk.TreeViewColumn(_("Name"))
        cell = gtk.CellRendererPixbuf()
        cell.set_fixed_size(treeview_icon_size, treeview_icon_size)

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
        cell.edited = self.edited
        cell.set_treeview(self.treeview2)
        cell.set_preediting(self.get_editinfo)
        cell.set_edited_user_fn(self.edited_user)

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

    def tv2_selected(self, tv, *args):
        (model, itr) = tv.get_selection().get_selected()
        if itr is None :
            self.selected_type = 'xxx'
            self.selected_param = None
            self.hint_label.set_text("")
        else :
            itr_m = self.details_filter.convert_iter_to_child_iter(itr)
            self.selected_type = self.treestore.get_value(itr_m, 0).get_type()
            self.selected_param = itr_m
            self.hint_label.set_markup(self.treestore.get_value(itr_m, 0).get_tooltip())

    def create_actions(self):
        # without accelerators

        self.actionBuild = gtk.Action("actionBuild", _('Create %(filename)s') % {'filename':GENERATED_FILE},
                _('Build gcode and save to %(filename)s') % {'filename':GENERATED_FILE}, 'gnome-run')
        self.actionBuild.connect('activate', self.action_build)

        self.actionDualView = gtk.RadioAction("actionDualView", _('Dual Views'),
                _('Dual Views'), '', 0)
        self.actionDualView.connect('activate', self.dual_view_activate)

        self.actionSingleView = gtk.RadioAction("actionSingleView", _('Single View'),
                _('Single View'), '', 0)
        self.actionSingleView.set_group(self.actionDualView)
        self.actionSingleView.connect('activate', self.single_view_activate)

        self.actionEditFeature = gtk.Action("actionEditFeature", _('Edit Current Subroutine'),
                _('Edit Current Subroutine'), 'gtk-edit')
        self.actionEditFeature.connect('activate', self.edit_feature)

        self.actionHideValCol = gtk.ToggleAction("actionHideValCol",
                    _('Hide Master Tree Value Column'), _('Hide Master Tree Value Column'), '')
        self.actionHideValCol.connect('activate', self.hide_value_col)

        self.actionReloadFeature = gtk.Action("actionReloadFeature",
                _('Reload Current Subroutine'), _('Reload Current Subroutine'), 'gtk-refresh')
        self.actionReloadFeature.connect('activate', self.reload_subroutine)

        self.actionSaveLayout = gtk.Action("actionSaveLayout", _('Save As Default Layout'),
                _('Save As Default Layout'), 'gtk-save')

        self.actionSaveNGC = gtk.Action("actionSaveNGC", _('Export gcode as RS274NGC'),
                _('Export gcode as RS274NGC'), 'gtk-save')
        self.actionSaveNGC.connect('activate', self.save_ngc)

        self.actionSaveTemplate = gtk.Action("actionSaveTemplate",
                _('Save as Default Template'), _('Save as Default Template'), 'gtk-save')
        self.actionSaveTemplate.connect('activate', self.save_default_template)

        self.actionSideBySide = gtk.RadioAction("actionSideBySide",
                _('Side By Side Layout'), _('Side By Side Layout'), '', 0)
        self.actionSideBySide.connect('activate', self.side_by_side_layout)

        self.actionTopBottom = gtk.RadioAction("actionTopBottom",
                _('Top / Bottom Layout'), _('Top / Bottom Layout'), '', 0)
        self.actionTopBottom.set_group(self.actionSideBySide)
        self.actionTopBottom.connect('activate', self.top_bottom_layout)

        self.actionSubHdrs = gtk.ToggleAction("actionSubHdrs",
                _('Sub-Groups In Master Tree'), _('Sub-Groups In Master Tree'), '')
        self.actionSubHdrs.connect('activate', self.action_SubHdrs)


    def get_widgets(self):
        self.main_box = self.builder.get_object("MainBox")

        self.accelGroup = gtk.AccelGroup()
        self.actionGroup = self.builder.get_object("actiongroup1")

        self.create_actions()
        self.actionGroup.add_action(self.actionBuild)
        self.actionGroup.add_action(self.actionDualView)
        self.actionGroup.add_action(self.actionSingleView)
        self.actionGroup.add_action(self.actionEditFeature)
        self.actionGroup.add_action(self.actionHideValCol)
        self.actionGroup.add_action(self.actionReloadFeature)
        self.actionGroup.add_action(self.actionSaveLayout)
        self.actionGroup.add_action(self.actionSaveNGC)
        self.actionGroup.add_action(self.actionSaveTemplate)
        self.actionGroup.add_action(self.actionSideBySide)
        self.actionGroup.add_action(self.actionTopBottom)
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
        self.col_width_adj.value = self.pref.col_width_adj_value
        self.w_adj = self.builder.get_object("width_adj")
        self.w_adj.value = self.pref.w_adj_value
        self.tv_w_adj = self.builder.get_object("tv_w_adj")
        self.tv_w_adj.value = self.pref.tv_w_adj_value

        self.button_tb = self.builder.get_object("toolbar1")
        self.button_tb.set_icon_size(toolbar_icon_size)
        self.add_toolbar = self.builder.get_object("add_toolbar")
        self.add_toolbar.set_icon_size(toolbar_icon_size)

        self.feature_pane = self.builder.get_object("ncam_pane")
        self.feature_Hpane = self.builder.get_object("hpaned1")
        self.params_scroll = self.builder.get_object("params_scroll")
        self.frame2 = self.builder.get_object("frame2")
        self.addVBox = self.builder.get_object("frame3")
        self.add_iconview = self.builder.get_object("add_iconview")
        self.hint_label = self.builder.get_object("hint_label")

    def get_translations(self, callback = None) :

        def get_menu_strings(path, f_name, trslatbl):
            for ptr in range(len(path)) :
                p = path[ptr]
                if p.tag.lower() == "group":
                    name = p.get("name") if "name" in p.keys() else None
                    if name is not None :
                        trslatbl.append((f_name, name))

                    tooltip = p.get('tool_tip') if "tool_tip" in p.keys() else None
                    if (tooltip is not None) and (tooltip != '') :
                        trslatbl.append((f_name, tooltip))

                    get_menu_strings(p, f_name, trslatbl)

                elif p.tag.lower() == "sub":
                    name = p.get("name") if "name" in p.keys() else None
                    if name is not None :
                        trslatbl.append((f_name, name))

                    tooltip = p.get('tool_tip') if "tool_tip" in p.keys() else None
                    if (tooltip is not None) and (tooltip != '') :
                        trslatbl.append((f_name, tooltip))

        def get_strings():
            os.popen("xgettext --language=Python ./ncam.py -o ./ncam.po")
            os.popen("xgettext --language=Glade ./ncam.glade ./ncam_pref.glade -o ./glade.po")
            os.popen("sed --in-place ./*.po --expression=s/charset=CHARSET/charset=UTF-8/")
            os.popen("msgcat ./ncam.po ./glade.po -o ./locale/ncam.tmp_po")
            os.popen("rm ./ncam.po ./glade.po")

            # catalogs
            find = os.popen("find ./%s -name 'menu.xml'" % CATALOGS_DIR).read()
            for s in find.split() :
                translatable = []
                d, splitname = os.path.split(s)
                fname = s.lstrip('./')
                destname = './locale/' + splitname

                xml = etree.parse(s).getroot()
                get_menu_strings(xml, fname, translatable)

                out = []
                for i in translatable :
                    out.append("#: %s" % i[0])
                    out.append(_("%s") % repr(i[1]))

                out = "\n".join(out)
                open(destname, "w").write(out)
                translatable = []
                os.popen("xgettext --language=Python --from-code=UTF-8 %s -o %s" % (destname, destname))
                os.popen("sed --in-place %s --expression=s/charset=CHARSET/charset=UTF-8/" % destname)
                os.popen("msgcat %s ./locale/ncam.tmp_po -o ./locale/ncam.tmp_po" % destname)
                os.popen("rm %s" % destname)

            # cfg files
            find = os.popen("find ./%s -name '*.cfg'" % CFG_DIR).read()
            print find
            for s in find.split() :
                translatable = []
                d, splitname = os.path.split(s)
                fname = s
                destname = './locale/' + splitname
                f = Feature(src = s)
                for i in ["name", "help"] :
                    if i in f.attr :
                        translatable.append((fname, f.attr[i]))

                for p in f.param :
                    for i in ["name", "help", "tool_tip", "options"] :
                        if i in p.attr :
                            translatable.append((fname, p.attr[i]))

                out = []
                for i in translatable :
                    out.append("#: %s" % i[0])
                    out.append(_("%s") % repr(i[1]))

                out = "\n".join(out)
                open(destname, "w").write(out)

            cmd_line = "find ./locale/ -name '*.cfg'"
            find = os.popen(cmd_line).read()
            for s in find.split() :
                os.popen("xgettext --language=Python --from-code=UTF-8 %s -o %s" % (s, s))
                os.popen("sed --in-place %s --expression=s/charset=CHARSET/charset=UTF-8/" % s)
                os.popen("msgcat ./locale/ncam.tmp_po %s -o ./locale/ncam.tmp_po" % s)
                os.popen("rm %s" % s)

            if os.path.exists("./locale/ncam.po") :
                os.popen("msgcat ./locale/ncam.tmp_po ./locale/ncam.po -o ./locale/ncam.po")
                os.remove("./locale/ncam.tmp_po")
            else :
                os.rename("./locale/ncam.tmp_po", "./locale/ncam.po")

        try :
            get_strings()
            mess_dlg('Done !\nFile : ./locale/ncam.po')
        except Exception, detail :
            mess_dlg('Error while getting data for translation :\n\n%(detail)s' % \
                       {'detail':detail})

    def move(self, i) :
        itr = self.master_filter.convert_iter_to_child_iter(self.selected_feature_itr)
        if (i > 0) :
            itr_swap = self.master_filter.convert_iter_to_child_iter(self.iter_next)
        if (i < 0) :
            itr_swap = self.master_filter.convert_iter_to_child_iter(self.iter_previous)
        self.treestore.swap(itr, itr_swap)
        self.get_selected_feature(self)
        self.action()

    def get_selected_feature(self, widget) :
        old_selected_feature = self.selected_feature
        (model, itr) = self.treeview.get_selection().get_selected()
        self.actionCollapse.set_sensitive(itr is not None)

        if itr is not None :

            self.selected_type = model.get_value(itr, 0).attr.get("type")
            self.hint_label.set_markup(model.get_value(itr, 0).get_tooltip())

            ts_itr = model.convert_iter_to_child_iter(itr)
            self.selected_param = None

            if self.selected_type == "items" :
                self.iter_selected_type = tv_select.items
                self.items_ts_parent_s = self.treestore.get_string_from_iter(ts_itr)

                self.items_path = model.get_path(itr)
                n_children = model.iter_n_children(itr)
                self.items_lpath = (self.items_path + (n_children,))

            elif self.selected_type in ["header", 'sub-header'] :
                self.iter_selected_type = tv_select.header

            elif self.selected_type in SUPPORTED_DATA_TYPES :
                self.iter_selected_type = tv_select.param
                self.selected_param = ts_itr

            else :
                self.iter_selected_type = tv_select.feature

            tree_path = model.get_path(itr)
            depth = len(tree_path)
            index_s = tree_path[depth - 1]

            if self.iter_selected_type in [tv_select.items, tv_select.header] :
                items_ts_path = self.treestore.get_path(ts_itr)
                ts_itr = self.treestore.iter_parent(ts_itr)
                self.items_ts_parent_s = self.treestore.get_string_from_iter(ts_itr)

            itr_p = itr
            while model.get_value(itr_p, 0).attr.get("type") in SUPPORTED_DATA_TYPES :
                itr_p = model.iter_parent(itr_p)

            self.selected_feature_itr = itr_p
            self.selected_feature = model.get(itr_p, 0)[0]
            ts_itr = model.convert_iter_to_child_iter(itr_p)
            self.selected_feature_ts_itr = ts_itr
            self.feature_ts_path = self.treestore.get_path(ts_itr)
            self.selected_feature_ts_path_s = self.treestore.get_string_from_iter(ts_itr)

            self.iter_next = model.iter_next(itr_p)
            if self.iter_next :
                self.can_move_down = (self.iter_selected_type == tv_select.feature)
                s = str(model.get(self.iter_next, 0)[0])
                self.can_add_to_group = (s.find('type="items"') > -1) and \
                        (self.iter_selected_type == tv_select.feature)
            else :
                self.can_add_to_group = False
                self.can_move_down = False

            self.selected_feature_parent_itr = model.iter_parent(itr_p)
            if self.selected_feature_parent_itr :
                path_parent = model.get_path(self.selected_feature_parent_itr)
                self.can_remove_from_group = (self.iter_selected_type == tv_select.feature) and \
                    model.get_value(self.selected_feature_parent_itr, 0).attr.get("type") == "items"
            else :
                path_parent = None
                self.can_remove_from_group = False

            self.selected_feature_path = model.get_path(itr_p)
            depth = len(self.selected_feature_path)
            index_s = self.selected_feature_path[depth - 1]
            self.can_move_up = (index_s > 0) and \
                (self.iter_selected_type == tv_select.feature)

            if index_s :
                if path_parent is None :
                    path_previous = (index_s - 1,)
                else :
                    path_previous = path_parent[0: depth - 1] + (index_s - 1,)
                self.iter_previous = model.get_iter(path_previous)
            else :
                self.iter_previous = None

        else:
            self.iter_selected_type = tv_select.none
            self.selected_feature = None
            self.selected_type = None
            self.can_move_up = False
            self.can_move_down = False
            self.can_add_to_group = False
            self.can_remove_from_group = False
            n_children = model.iter_n_children(None)
            self.items_lpath = (n_children,)
            tree_path = None
            self.hint_label.set_text('')


        self.can_delete_duplicate = (self.iter_selected_type == tv_select.feature)
        self.set_actions_state()

        if self.pref.use_dual_views :
            if self.iter_selected_type == tv_select.none :
                if self.treeview2 is not None:
                    self.treeview2.set_model(None)

            if ((old_selected_feature == self.selected_feature) and \
                (self.iter_selected_type in [tv_select.items, tv_select.feature, tv_select.header])) \
                    or (old_selected_feature != self.selected_feature) :

                if self.iter_selected_type in [tv_select.items, tv_select.header] :
                    a_filter = self.treestore.filter_new(items_ts_path)
                else :
                    a_filter = self.treestore.filter_new(self.feature_ts_path)
                a_filter.set_visible_column(3)
                self.details_filter = a_filter

                self.treeview2.set_model(self.details_filter)
                self.treeview2.expand_all()

        if tree_path is not None :
            self.treeview.expand_row(tree_path, False)

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
        self.update_quick_access(self.selected_feature.get_attr('src'))
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

    def tv_key_pressed_event(self, widget, event) :
        keyname = gtk.gdk.keyval_name(event.keyval)
        model, itr = self.treeview.get_selection().get_selected()

        self.focused_widget = self.treeview

        if itr is not None :
            path = model.get_path(itr)
        else :
            path = None

        if self.embedded :
            if event.state & gtk.gdk.SHIFT_MASK :
                if event.state & gtk.gdk.CONTROL_MASK :
                    if keyname in ['z', 'Z'] :
                        self.actionRedo.activate()
                        return True

            elif event.state & gtk.gdk.CONTROL_MASK :
                if keyname in ['z', 'Z'] :
                    self.actionUndo.activate()
                    return True

                elif keyname == "Up" :
                    self.actionMoveUp.activate()
                    return True

                elif keyname == "Down" :
                    self.actionMoveDn.activate()
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
                if path is None :
                    return False

                if keyname == "Up" :
                    if path != (0,) :
                        depth = len(path)
                        index_s = path[depth - 1]
                        if index_s > 0 :
                            p = path[0: depth - 1] + (index_s - 1,)
                            iter_p = model.get_iter(p)
                            while iter_p is not None :
                                count = model.iter_n_children(iter_p)
                                if (count == 0) or not self.treeview.row_expanded(model.get_path(iter_p)) :
                                    p = model.get_path(iter_p)
                                    break
                                else :
                                    iter_p = model.iter_nth_child(iter_p, count - 1)
                        else :
                            p = path[0: depth - 1]
                    else :
                        p = None

                    if p is not None :
                        self.treeview.expand_to_path(p)
                        self.treeview.set_cursor(p)
                    return True

                elif keyname == "Down" :
                    p = None
                    if model.iter_has_child(itr) :
                        p = model.get_path(model.iter_children(itr))
                    else :
                        itn = model.iter_next(itr)
                        if itn is not None :
                            p = model.get_path(itn)
                        else :
                            itn = model.iter_parent(itr)
                            while itn is not None :
                                ito = model.iter_next(itn)
                                if ito is not None :
                                    p = model.get_path(ito)
                                    break
                                else :
                                    itn = model.iter_parent(itn)

                    if p is not None :
                        self.treeview.set_cursor(p)
                    return True

                elif keyname == "Left" :
                    self.treeview.collapse_row(path)
                    return True

                elif keyname == "Right" :
                    self.treeview.expand_row(path, False)
                    return True

                elif keyname in ["Return", "KP_Enter", "space"] :
                    self.treeview.set_cursor_on_cell(path, focus_column = self.col_value, start_editing = True)
                    return True

        if keyname == "Tab" and self.treeview2 is not None:
            self.treeview2.grab_focus()
            model, itr = self.treeview2.get_selection().get_selected()
            if itr is None :
                self.treeview2.set_cursor((0,))
            self.hint_label.set_markup(items_fmt_str % _("Parameters treeview focused"))
            return True

        if path is None :
            return False

        if keyname == "BackSpace" :
            self.edit_cell.set_Input('BS')
            self.treeview.set_cursor_on_cell(path, focus_column = self.col_value, start_editing = True)
            return True

        if keyname == "F2" :
            self.treeview.set_cursor_on_cell(path, focus_column = self.col_value, start_editing = True)
            return True

        if (keyname >= "0" and keyname <= "9") or (keyname >= "KP_0" and keyname <= "KP_9") :
            self.edit_cell.set_Input(keyname[-1])
            self.treeview.set_cursor_on_cell(path, focus_column = self.col_value, start_editing = True)
            return True

        elif keyname in ['KP_Decimal', 'period', 'comma'] :
            self.edit_cell.set_Input(decimal_point)
            self.treeview.set_cursor_on_cell(path, focus_column = self.col_value, start_editing = True)
            return True

        elif keyname in ['KP_Subtract', 'KP_Add', 'plus', 'minus'] :
            self.edit_cell.set_Input('-')
            self.treeview.set_cursor_on_cell(path, focus_column = self.col_value, start_editing = True)
            return True

        else :
            return False

    def tv2_kp_event(self, widget, event) :
        keyname = gtk.gdk.keyval_name(event.keyval)
        model, itr = self.treeview2.get_selection().get_selected()

        if itr is None :
            return False

        self.focused_widget = self.treeview2
        path = model.get_path(itr)

        if self.embedded:

            if keyname == "Up" :
                if path != (0,) :
                    depth = len(path)
                    index_s = path[depth - 1]
                    if index_s > 0 :
                        p = path[0: depth - 1] + (index_s - 1,)
                        iter_p = model.get_iter(p)
                        while iter_p is not None :
                            count = model.iter_n_children(iter_p)
                            if (count == 0) :  # or not self.treeview.row_expanded(model.get_path(iter_p)) :
                                p = model.get_path(iter_p)
                                break
                            else :
                                iter_p = model.iter_nth_child(iter_p, count - 1)
                    else :
                        p = path[0: depth - 1]
                else :
                    p = None

                if p is not None :
                    self.treeview2.expand_to_path(p)
                    self.treeview2.set_cursor(p)
                return True

            elif keyname == "Down" :
                p = None
                if model.iter_has_child(itr) :
                    p = model.get_path(model.iter_children(itr))
                else :
                    itn = model.iter_next(itr)
                    if itn is not None :
                        p = model.get_path(itn)
                    else :
                        itn = model.iter_parent(itr)
                        while itn is not None :
                            ito = model.iter_next(itn)
                            if ito is not None :
                                p = model.get_path(ito)
                                break
                            else :
                                itn = model.iter_parent(itn)

                if p is not None :
                    self.treeview2.set_cursor(p)
                return True

            elif keyname in ["Return", "KP_Enter", "space"] :
                self.treeview2.set_cursor_on_cell(path, focus_column = self.col_value2, start_editing = True)
                return True

        if keyname == "Tab" :
            self.treeview.grab_focus()
            self.hint_label.set_markup(items_fmt_str % _("Master treeview focused"))
            return True

        if keyname == "F2" :
            self.treeview2.set_cursor_on_cell(path, focus_column = self.col_value2, start_editing = True)
            return True

        if keyname == "BackSpace" :
            self.cell_value2.set_Input('BS')
            self.treeview2.set_cursor_on_cell(path, focus_column = self.col_value2, start_editing = True)
            return True

        if (keyname >= "0" and keyname <= "9") or (keyname >= "KP_0" and keyname <= "KP_9") :
            self.cell_value2.set_Input(keyname[-1])
            self.treeview2.set_cursor_on_cell(path, focus_column = self.col_value2, start_editing = True)
            return True

        elif keyname in ['KP_Decimal', 'period', 'comma'] :
            self.cell_value2.set_Input(decimal_point)
            self.treeview2.set_cursor_on_cell(path, focus_column = self.col_value2, start_editing = True)
            return True

        elif keyname in ['KP_Subtract', 'KP_Add', 'plus', 'minus'] :
            self.cell_value2.set_Input('-')
            self.treeview2.set_cursor_on_cell(path, focus_column = self.col_value2, start_editing = True)
            return True

        else :
            return False

    def treestore_from_xml(self, xml):
        def recursive(treestore, itr, xmlpath):
            for xml in xmlpath :
                if xml.tag == "feature" :
                    f = Feature(xml = xml)
                    tool_tip = f.get_tooltip()
                    citer = treestore.append(itr, [f, tool_tip, True, False])

                    grp_header = ''

                    for p in f.param :
                        header_name = p.attr["header"].lower() if "header" in p.attr else ''

                        tool_tip = p.get_tooltip()  # if "tool_tip" in p.attr else None
                        p_type = p.get_type()
                        p_hidden = get_int(p.attr['hidden'] if 'hidden' in p.attr else '0')

                        if self.pref.use_dual_views :
                            if self.pref.sub_hdrs_in_tv1 :
                                m_visible = p_type in ['items', 'header', 'sub-header'] and not p_hidden
                                is_visible = p_type not in ['items', 'header', 'sub-header'] and not p_hidden
                            else :
                                m_visible = p_type in ['items', 'header'] and not p_hidden
                                is_visible = p_type not in ['items', 'header'] and not p_hidden
                        else :
                            m_visible = not p_hidden
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
        def recursive(itr, ldr) :
            gcode_def = ""
            gcode = ""
            sub_ldr = ldr
            f = self.treestore.get(itr, 0)[0]
            if f.__class__ == Feature :
                sub_ldr += f.getindent()
                gcode_def += f.get_definitions()
                gcode += f.process(f.attr["before"], ldr)
                gcode += f.process(f.attr["call"], ldr)
            itr = self.treestore.iter_children(itr)
            while itr :
                g, d = recursive(itr, sub_ldr + '\t')
                gcode += g
                gcode_def += d
                itr = self.treestore.iter_next(itr)
            if f.__class__ == Feature :
                gcode += f.process(f.attr["after"], ldr)
            return gcode, gcode_def

        gcode = ""
        gcode_def = ""
        global DEFINITIONS
        DEFINITIONS = []
        global INCLUDE
        INCLUDE = []
        itr = self.treestore.get_iter_root()
        while itr is not None :
            g, d = recursive(itr, '')
            gcode += g
            gcode_def += d
            itr = self.treestore.iter_next(itr)
        return self.pref.default + gcode_def + \
            "(end sub definitions)\n\n" + gcode + self.pref.post_amble

    def action_build(self, *arg) :
        self.autorefresh_call()
        if not self.LinuxCNC_connected :
            mess_dlg(_('LinuxCNC not running\n\nStart LinuxCNC and\n' + \
                    'press Build button again'))
        self.auto_refresh.set_active(self.LinuxCNC_connected)

    def save_ngc(self, *arg) :
        filechooserdialog = gtk.FileChooserDialog(_("Save as ngc..."), None,
            gtk.FILE_CHOOSER_ACTION_SAVE,
            (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_OK, gtk.RESPONSE_OK))
        try :
            filt = gtk.FileFilter()
            filt.set_name("NGC")
            filt.add_mime_type("text/ngc")
            filt.add_pattern("*.ngc")
            filechooserdialog.add_filter(filt)
            filechooserdialog.set_current_folder(NGC_DIR)

            if filechooserdialog.run() == gtk.RESPONSE_OK:
                gcode = self.to_gcode()
                filename = filechooserdialog.get_filename()
                if filename[-4] != ".ngc" not in filename :
                    filename += ".ngc"
                f = open(filename, "w")
                f.write(gcode)
                f.close()
        finally :
            filechooserdialog.destroy()

    def edited_user(self, itr, new_value, new_type):
        if new_type != '' :
            self.treestore.get(itr, 0)[0].set_type(new_type)
        self.treestore.get(itr, 0)[0].set_value(new_value)

    def edited(self, renderer, path, new_text, new_type) :
        self.focused_widget = renderer.get_treeview()
        itr = renderer.get_treeview().get_model().get_iter(path)
        itr = renderer.get_treeview().get_model().convert_iter_to_child_iter(itr)
        old_value = self.treestore.get_value(itr, 0).get_value()
        if renderer.editdata_type == 'combo-user' :
            df = self.treestore.get_value(itr, 0).get_attr('links')
            if (df is not None) :
                for dg in df.split(":") :
                    opt = dg.split('=')
                    self.treestore.get(itr, 0)[0].set_value(new_text)
                    if opt[1] == new_text :
                        children = self.treestore.iter_children(self.treestore.iter_parent(itr))
                        while children is not None :
                            ca = self.treestore.get_value(children, 0).get_attr('call')
                            if ca == '#param_' + opt[0] :
                                renderer.set_tooltip(self.treestore.get_value(children, 0).get_tooltip())
                                dt = self.treestore.get_value(children, 0).get_type()
                                renderer.set_edit_datatype(dt)
                                val = self.treestore.get_value(children, 0).get_value()
                                renderer.set_param_value(val)
                                if dt in NUMBER_TYPES :
                                    renderer.set_param_value(val.replace('.', decimal_point))
                                    renderer.set_max_value(get_float(self.treestore.get_value(children, 0).get_max_value()))
                                    renderer.set_min_value(get_float(self.treestore.get_value(children, 0).get_min_value()))
                                    renderer.set_digits(self.treestore.get_value(children, 0).get_digits())
                                    renderer.set_not_zero(self.treestore.get_value(children, 0).get_not_zero())
                                    renderer.edit_number(children, gmoccapy_time_out)

                                elif dt == 'string' :
                                    renderer.edit_string(children, gmoccapy_time_out)

                                elif dt == 'list' :
                                    renderer.set_options(self.treestore.get_value(children, 0).get_options())
                                    renderer.edit_list(children, gmoccapy_time_out)
                                self.action()
                                break
                            children = self.treestore.iter_next(children)

        if new_type != '' :
            self.treestore.get(itr, 0)[0].set_type(new_type)
        if old_value != new_text :
            self.treestore.get(itr, 0)[0].set_value(new_text)
            self.action()
        self.focused_widget.grab_focus()

    def delete_clicked(self, *arg) :
        if self.iter_next :
            next_path = self.master_filter.get_path(self.selected_feature_itr)
        elif self.iter_previous :
            next_path = self.master_filter.get_path(self.iter_previous)
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
        self.pref.side_by_side = False
        self.frame2.reparent(self.feature_pane)

    def side_by_side_layout(self, *arg):
        self.pref.side_by_side = True
        self.frame2.reparent(self.feature_Hpane)

    def action_collapse(self, *arg) :
        (model, itr) = self.treeview.get_selection().get_selected()
        path = model.get_path(itr)
        self.treeview.collapse_all()
        self.treeview.expand_to_path(path)
        self.treeview.set_cursor(path)
        self.treeview.grab_focus()

    def action_SubHdrs(self, *args):
        self.pref.sub_hdrs_in_tv1 = self.actionSubHdrs.get_active()
        self.treestore_from_xml(self.treestore_to_xml())
        self.expand_and_select(self.path_to_old_selected)

    def import_xml(self, xml_) :
        if xml_.tag != XML_TAG:
            xml_ = xml_.find(".//%s" % XML_TAG)

        if xml_ is not None :
            xml = self.treestore_to_xml()
            if self.iter_selected_type == tv_select.items :
                # will append to items
                dest = xml.find(".//*[@path='%s']/param[@type='items']" %
                                self.items_ts_parent_s)
                opt = 2
                i = -1
                next_path = self.items_lpath
            elif self.iter_selected_type != tv_select.none :
                # will append after parent of selected feature
                dest = xml.find(".//*[@path='%s']" % self.selected_feature_ts_path_s)
                parent = dest.getparent()
                i = parent.index(dest)
                opt = 1
                l_path = len(self.selected_feature_path)
                next_path = (self.selected_feature_path[0:l_path - 1] + \
                        (self.selected_feature_path[l_path - 1] + 1,))
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
        src_file = search_path(2, src, CFG_DIR)
        if src_file is None:
            return

        src_data = open(src_file).read()
        if src_data.find(".//%s" % XML_TAG) > -1 :
            xml = etree.parse(src_file).getroot()
        elif src_data.find('[SUBROUTINE]') > -1 :
            f = Feature(src_file)
            xml = etree.Element(XML_TAG)
            xml.append(f.to_xml())
        else :
            mess_dlg(_("'%(source_file)s' is not a valid cfg or xml file") % {'source_file':src_file})
            return
        self.import_xml(xml)
        self.update_quick_access(src)

    def autorefresh_call(self) :
        try :
            fname = os.path.join(NGC_DIR, GENERATED_FILE)
            f = open(fname, "w")
            f.write(self.to_gcode())
            f.close()
            try:
                linuxCNC = linuxcnc.command()
                self.LinuxCNC_connected = True
                stat = linuxcnc.stat()
                stat.poll()
                if stat.interp_state == linuxcnc.INTERP_IDLE :
                    try :
                        subprocess.check_output(["pgrep", "axis"])
                        subprocess.call(["axis-remote", fname])
                    except :
                        try :
                            subprocess.check_output(["pgrep", "gmoccapy"])
                            linuxCNC.reset_interpreter()
                            time.sleep(gmoccapy_time_out)
                            linuxCNC.mode(linuxcnc.MODE_AUTO)
                            linuxCNC.program_open(fname)
                            time.sleep(1)
                        except :
                            self.LinuxCNC_connected = False
            except :
                self.LinuxCNC_connected = False
                self.auto_refresh.set_active(False)
        except :
            self.LinuxCNC_connected = False

        if self.focused_widget is not None :
            self.focused_widget.grab_focus()
        else :
            self.treeview.grab_focus()

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
            self.timeout = gobject.timeout_add(int(self.pref.timeout_value * 1000),
                    self.autorefresh_call)

    def undo_clicked(self, *arg) :
        save_restore = self.pref.restore_expand_state
        self.pref.restore_expand_state = True
        self.undo_pointer -= 1
        self.treestore_from_xml(etree.fromstring(self.undo_list[self.undo_pointer]))
        self.expand_and_select(self.path_to_old_selected)
        self.pref.restore_expand_state = save_restore
        self.update_do_btns()

    def redo_clicked(self, *arg) :
        save_restore = self.pref.restore_expand_state
        self.pref.restore_expand_state = True
        self.undo_pointer += 1
        self.treestore_from_xml(etree.fromstring(self.undo_list[self.undo_pointer]))
        self.expand_and_select(self.path_to_old_selected)
        self.pref.restore_expand_state = save_restore
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
        etree.ElementTree(xml).write(os.path.join(NCAM_DIR, CATALOGS_DIR, self.catalog_dir, \
                                                   DEFAULT_TEMPLATE), pretty_print = True)

    def menu_new_activate(self, *args):
        global UNIQUE_ID

        UNIQUE_ID = 10
        self.treestore.clear()
        self.clear_undo()
        fn = None
        if (args.__len__() == 2) :
            fn = search_path(1, CURRENT_WORK, CATALOGS_DIR, self.catalog_dir)
        if fn is None :
            fn = search_path(1, DEFAULT_TEMPLATE, CATALOGS_DIR, self.catalog_dir)
        if fn is not None :
            xml = etree.parse(fn)
            self.treestore_from_xml(xml.getroot())
            self.expand_and_select((0,))
        self.autorefresh_call()
        self.auto_refresh.set_active(True)
        self.current_filename = _('Untitle.xml')
        self.file_changed = False
        self.action()

    def set_layout(self, val):
        self.pref.use_dual_views = val
        if self.pref.use_dual_views :
            if self.treeview2 is None :
                self.create_second_treeview()
                self.treeview2.show_all()
            if self.pref.side_by_side :
                self.side_by_side_layout()
            self.frame2.set_visible(True)
        else :
            self.frame2.set_visible(False)
            if self.treeview2 is not None :
                self.treeview2.destroy()
                self.treeview2 = None
        self.treestore_from_xml(self.treestore_to_xml())

        self.actionTopBottom.set_sensitive(self.pref.use_dual_views)
        self.actionSideBySide.set_sensitive(self.pref.use_dual_views)
        self.actionSubHdrs.set_sensitive(self.pref.use_dual_views)

    def dual_view_activate(self, *args) :
        self.set_layout(True)

    def single_view_activate(self, *args) :
        self.set_layout(False)

    def hide_value_col(self, callback = None):
        self.col_value.set_visible(not self.actionHideValCol.get_active())

    def menu_pref_activate(self, callback = None) :
        self.pref.edit(self)

    def on_scale_change_value(self, widget):
        self.main_box.set_size_request(int(self.w_adj.value), 100)

    def col_width_adj_value_changed(self, widget):
        self.col_value.set_min_width(int(self.col_width_adj.value))
        if self.col_value2 is not None:
            self.col_value2.set_min_width(int(self.col_width_adj.value))

    def move_up_clicked(self, *arg) :
        self.move(-1)

    def move_down_clicked(self, *arg) :
        self.move(1)

    def get_col_name(self, column, cell, model, itr) :
        data_type = model.get_value(itr, 0).get_type()
        val = _(model.get_value(itr, 0).get_name())
        if data_type == 'header' :
            cell.set_property('markup', header_fmt_str % val)
        elif data_type == 'sub-header' :
            if  self.pref.use_dual_views and not self.pref.sub_hdrs_in_tv1 :
                cell.set_property('markup', sub_header_fmt_str2 % val)
            else :
                cell.set_property('markup', sub_header_fmt_str % val)
        elif data_type == 'items' :
            cell.set_property('markup', items_fmt_str % val)
        elif data_type in SUPPORTED_DATA_TYPES :
            cell.set_property('markup', val)
        else :
            cell.set_property('markup', feature_fmt_str % val)

    def get_editinfo(self, cell, treeview, path):
        model = treeview.get_model()
        itr = model.get_iter(path)
        data_type = model.get_value(itr, 0).get_type()
        cell.set_edit_datatype(data_type)
        cell.set_param_value(model.get_value(itr, 0).get_value())

        if data_type in ['combo', 'combo-user', 'list']:
            cell.set_options(model.get_value(itr, 0).get_options())

        elif data_type in NUMBER_TYPES:
            cell.set_max_value(get_float(model.get_value(itr, 0).get_max_value()))
            cell.set_min_value(get_float(model.get_value(itr, 0).get_min_value()))
            cell.set_digits(model.get_value(itr, 0).get_digits())
            cell.set_tooltip(model.get_value(itr, 0).get_tooltip())
            cell.set_not_zero(model.get_value(itr, 0).get_not_zero())

        elif data_type == 'tool' :
            cell.set_options(self.tools.list)

        elif data_type == 'filename' :
            cell.set_fileinfo(model.get_value(itr, 0).attr['patterns'], \
                            model.get_value(itr, 0).attr['mime_types'], \
                            model.get_value(itr, 0).attr['filter_name'])


    def get_col_value(self, column, cell, model, itr) :
        f = model.get_value(itr, 0)
        val = f.get_value()
        cell.set_param_value(val)

        data_type = f.get_type()
        cell.set_data_type(data_type)

        if data_type == 'filename':
            h, val = os.path.split(val)

        elif data_type == 'tool' :
            val = self.tools.get_text(val)

        if data_type == 'combo':
            options = f.get_attr('options')
            for option in options.split(":") :
                opt = option.split('=')
                if opt[1] == val :
                    val = opt[0]
                    break

        elif data_type == 'combo-user':
            found = False
            df = f.get_attr('links') if 'links' in f.attr else None
            if (df is not None) :
                stop = False
                for dg in df.split(":") :
                    if stop :
                        break
                    else :
                        opt = dg.split('=')
                        if (opt[1] == val) :
                            try :
                                itr_n = self.treestore.iter_parent(model.convert_iter_to_child_iter(itr))
                                itr_n = self.treestore.iter_children(itr_n)
                                loop = True
                                while loop and (itr_n is not None) :
                                    f = self.treestore.get_value(itr_n, 0)
                                    if f.get_attr('call') == '#param_' + opt[0] :
                                        found = True
                                        data_type = f.get_type()
                                        if data_type == 'list':
                                            link_val = f.get_value()
                                            options = f.get_attr('options')
                                            for option in options.split(":") :
                                                opt = option.split('=')
                                                if opt[1] == link_val :
                                                    val = opt[0]
                                                    loop = False
                                                    stop = True
                                                    break
                                        else :
                                            val = f.get_value()

                                    itr_n = self.treestore.iter_next(itr_n)
                            except :
                                stop = True

            if not found :
                f = model.get_value(itr, 0)
                options = f.attr['options']
                for option in options.split(":") :
                    opt = option.split('=')
                    if opt[1] == val :
                        val = opt[0]
                        break

        if data_type == 'float' :
            val = val.replace('.', decimal_point)

        ps = f.get_attr('prefix')
        if ps is not None :
            val = ps + ' ' + val
        ps = f.get_attr('suffix')
        if ps is not None :
            val += ' ' + ps

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
                if self.pref.restore_expand_state :
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
        filechooserdialog = gtk.FileChooserDialog(_("Import project"), None, \
                gtk.FILE_CHOOSER_ACTION_OPEN, (gtk.STOCK_CANCEL, \
                gtk.RESPONSE_CANCEL, gtk.STOCK_OK, gtk.RESPONSE_OK))
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
            filechooserdialog.set_current_folder(os.path.join(NCAM_DIR, XML_DIR))

            if filechooserdialog.run() == gtk.RESPONSE_OK:
                fname = filechooserdialog.get_filename()
                if not self.file_changed :
                    self.current_filename = fname
                self.import_xml(etree.parse(fname).getroot())
                self.file_changed = False
        finally:
            filechooserdialog.destroy()

    def menu_save_xml_activate(self, callback) :
        filechooserdialog = gtk.FileChooserDialog(_("Save project as..."), None,
                gtk.FILE_CHOOSER_ACTION_SAVE, (gtk.STOCK_CANCEL, \
                gtk.RESPONSE_CANCEL, gtk.STOCK_OK, gtk.RESPONSE_OK))
        try:
            filt = gtk.FileFilter()
            filt.set_name("XML")
            filt.add_mime_type("text/xml")
            filt.add_pattern("*.xml")
            filechooserdialog.add_filter(filt)
            if os.path.exists(self.current_filename):
                filechooserdialog.set_filename(self.current_filename)
            else :
                filechooserdialog.set_current_folder(os.path.join(NCAM_DIR, XML_DIR))
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
        global UNIQUE_ID

        filechooserdialog = gtk.FileChooserDialog(_("Open project"), None,
                gtk.FILE_CHOOSER_ACTION_OPEN, (gtk.STOCK_CANCEL, \
                gtk.RESPONSE_CANCEL, gtk.STOCK_OK, gtk.RESPONSE_OK))
        try:
            filt = gtk.FileFilter()
            filt.set_name("XML")
            filt.add_mime_type("text/xml")
            filt.add_pattern("*.xml")
            filechooserdialog.add_filter(filt)
            filechooserdialog.set_current_folder(os.path.join(NCAM_DIR, XML_DIR))

            if filechooserdialog.run() == gtk.RESPONSE_OK:
                filename = filechooserdialog.get_filename()
                xml = etree.parse(filename)
                self.treestore_from_xml(xml.getroot())
                self.expand_and_select(self.path_to_old_selected)
                UNIQUE_ID = 10
                self.clear_undo()
                self.current_filename = filename
                self.autorefresh_call()
                self.file_changed = False
                self.auto_refresh.set_active(True)
                self.action()
        finally:
            filechooserdialog.destroy()

    def menu_open_cfg_activate(self, callback = None) :
        filechooserdialog = gtk.FileChooserDialog(_("Open a cfg file"), None, \
            gtk.FILE_CHOOSER_ACTION_OPEN, (gtk.STOCK_CANCEL, \
            gtk.RESPONSE_CANCEL, gtk.STOCK_OK, gtk.RESPONSE_OK))
        try:
            filt = gtk.FileFilter()
            filt.set_name("cfgfiles")
            filt.add_mime_type("text/xml")
            filt.add_pattern("*.cfg")
            filechooserdialog.add_filter(filt)
            filechooserdialog.set_current_folder(os.path.join(NCAM_DIR, CFG_DIR))

            if filechooserdialog.run() == gtk.RESPONSE_OK:
                self.add_feature(filechooserdialog.get_filename())
        finally :
            filechooserdialog.destroy()

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
    window = gtk.Dialog("NativeCAM for LinuxCNC", None, gtk.DIALOG_MODAL)
    window.set_title(APP_TITLE)
    ncam = NCam()
    ncam.embedded = False
    window.vbox.add(ncam)
    window.add_accel_group(ncam.accelGroup)
    window.connect("destroy", gtk.main_quit)
    window.set_default_size(400, 800)
    return window.run()

if __name__ == "__main__":
    exit(main())
