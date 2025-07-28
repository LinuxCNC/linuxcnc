#!/usr/bin/env python
# coding: utf-8
# ------------------------------------------------------------------
# --  NO USER SETTINGS IN THIS FILE -- EDIT PREFERENCES INSTEAD  ---
# ------------------------------------------------------------------

APP_COPYRIGHT = '''Copyright © 2017 Fernand Veilleux : fernveilleux@gmail.com
Copyright © 2012 Nick Drobchenko aka Nick from cnc-club.ru'''
APP_AUTHORS = ['Fernand Veilleux, maintainer', 'Nick Drobchenko, initiator',
               'Meison Kim', 'Alexander Wigen', 'Konstantin Navrockiy', 'Mit Zot',
               'Dewey Garrett', 'Karl Jacobs', 'Philip Mullen']

APP_VERSION = "(non deb)"

import gtk
import sys
import pygtk
pygtk.require('2.0')
from gtk import gdk

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
import platform
import pref_edit
import Tkinter
import math

SYS_DIR = os.path.dirname(os.path.realpath(__file__))

locale.setlocale(locale.LC_ALL, '')
decimal_point = locale.localeconv()["decimal_point"]

# if False, NO_ICON_FILE will be used
DEFAULT_USE_NO_ICON = True
NO_ICON_FILE = 'no-icon.png'

# info at http://www.pygtk.org/pygtk2reference/pango-markup-language.html
# when grayed, uses these format
gray_header_fmt_str = '<span foreground="gray" style="oblique">%s...</span>'
gray_sub_header_fmt_str = '<span foreground="gray" style="oblique">%s...</span>'
gray_sub_header_fmt_str2 = '<span foreground="gray" style="oblique" weight="bold">%s</span>'
gray_feature_fmt_str = '<span foreground="gray" weight="bold">%s</span>'
gray_items_fmt_str = '<span foreground="gray" style="oblique" weight="bold">%s</span>'
gray_val = '<span foreground="gray">%s</span>'
# when NOT grayed
header_fmt_str = '<i>%s...</i>'
sub_header_fmt_str = '<i>%s...</i>'
sub_header_fmt_str2 = '<b><i>%s</i></b>'
feature_fmt_str = '<b>%s</b>'
items_fmt_str = '<span foreground="blue" style="oblique"><b>%s</b></span>'

UNDO_MAX_LEN = 200
gmoccapy_time_out = 0.0

# use or test translation file
APP_NAME = 'nativecam'
nativecam_locale = os.getenv('NATIVECAM_LOCALE')
if nativecam_locale is not None :
    translate_test = True
else :
    translate_test = False
    nativecam_locale = '/usr/share/locale'
gettext.bindtextdomain(APP_NAME, nativecam_locale)
gettext.textdomain(APP_NAME)
try :
    lang = gettext.translation(APP_NAME, nativecam_locale, fallback = True)
    lang.install()
    _ = lang.ugettext
except :
    gettext.install(APP_NAME, None, str = True)

APP_TITLE = _("NativeCAM for LinuxCNC")
APP_COMMENTS = _('A GUI to help create LinuxCNC NGC files.')
APP_LICENCE = _('''This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n
It is recommended you use the deb package
''')

VALID_CATALOGS = ['mill', 'plasma', 'lathe']
DEFAULT_CATALOG = "mill"

# directories
CFG_DIR = 'cfg'
PROJECTS_DIR = 'projects'
LIB_DIR = 'lib'
NGC_DIR = 'scripts'
EXAMPLES_DIR = 'examples'
CATALOGS_DIR = 'catalogs'
GRAPHICS_DIR = 'graphics'
DEFAULTS_DIR = 'defaults'
CUSTOM_DIR = 'my-stuff'

# files
DEFAULT_TEMPLATE = 'default_template.xml'
USER_DEFAULT_FILE = 'custom_defaults.conf'
EXCL_MSG_FILE = 'excluded_msg.conf'
CURRENT_WORK = "current_work.xml"
PREFERENCES_FILE = "default.conf"
CONFIG_FILE = 'ncam.conf'
TOOLBAR_FNAME = "toolbar.conf"
TOOLBAR_CUSTOM_FNAME = "toolbar-custom.conf"
GENERATED_FILE = "ncam.ngc"

CURRENT_PROJECT = ''

DEFAULT_EDITOR = 'gedit'

SUPPORTED_DATA_TYPES = ['sub-header', 'header', 'bool', 'boolean', 'int', 'gc-lines',
                        'tool', 'gcode', 'text', 'list', 'float', 'string', 'engrave',
                        'combo', 'combo-user', 'items', 'filename', 'prjname']
NUMBER_TYPES = ['float', 'int']
NO_ICON_TYPES = ['sub-header', 'header']
GROUP_HEADER_TYPES = ['items', 'sub-header', 'header']

XML_TAG = "lcnc-ncam"

HOME_PAGE = 'https://github.com/FernV/NativeCAM'

class tv_select :  # 'enum' items
    none, feature, items, header, param = list(range(5))

class search_warning :
    none, print_only, dialog = list(range(3))

#global variables
INCLUDE = []
DEFINITIONS = []
PIXBUF_DICT = {}
USER_VALUES = {}
USER_SUBROUTINES = []
TB_CATALOG = {}
EXCL_MESSAGES = {}
GLOBAL_PREF = None
UNIQUE_ID = 9

UI_INFO = '''
<ui>
    <toolbar name='dummy'>
        <toolitem action='SingleView' />
        <toolitem action='DualView'/>
        <toolitem action='TopBottom'/>
        <toolitem action='SideSide'/>
        <toolitem action='HideCol'/>
        <toolitem action='SubHdrs'/>
    </toolbar>

    <toolbar name='ToolBar'>
        <toolitem action='Build' />
        <separator />
        <toolitem action='Add' />
        <toolitem action='Duplicate' />
        <toolitem action='Delete' />
        <separator />
        <toolitem action='Undo' />
        <toolitem action='Redo' />
        <separator />
        <toolitem action='MoveUp' />
        <toolitem action='MoveDown' />
        <separator />
        <toolitem action='AppendItm' />
        <toolitem action='RemoveItm' />
        <separator />
        <toolitem action='Collapse' />
    </toolbar>

    <popup name='PopupMenu'>
        <menuitem action='Rename' />
        <menu action='SetDigits'>
            <menuitem action='Digit1'/>
            <menuitem action='Digit2'/>
            <menuitem action='Digit3'/>
            <menuitem action='Digit4'/>
            <menuitem action='Digit5'/>
            <menuitem action='Digit6'/>
        </menu>
        <menuitem action='DataType' />
        <menuitem action='RevertType' />
        <separator />
        <menuitem action='Undo' />
        <menuitem action='Redo' />
        <separator />
        <menuitem action='HideField' />
        <menuitem action='ShowFields' />
        <menuitem action='ChngGrp' />
        <separator />
        <menuitem action='Add' />
        <menuitem action='Duplicate' />
        <menuitem action='Delete' />
        <separator />
        <menuitem action='Cut' />
        <menuitem action='Copy' />
        <menuitem action='Paste' />
        <separator />
        <menuitem action='MoveUp' />
        <menuitem action='MoveDown' />
        <separator />
        <menuitem action='AppendItm' />
        <menuitem action='RemoveItm' />
        <separator />
        <menuitem action='SaveUser' />
        <menuitem action='DeleteUser' />
    </popup>

    <popup name='PopupMenu2'>
        <menu action='SetDigits'>
            <menuitem action='Digit1'/>
            <menuitem action='Digit2'/>
            <menuitem action='Digit3'/>
            <menuitem action='Digit4'/>
            <menuitem action='Digit5'/>
            <menuitem action='Digit6'/>
        </menu>
        <menuitem action='DataType' />
        <menuitem action='RevertType' />
        <separator />
        <menuitem action='Undo' />
        <menuitem action='Redo' />
        <separator />
        <menuitem action='HideField' />
        <menuitem action='ShowFields' />
        <menuitem action='ChngGrp' />
        <separator />
        <menuitem action='SaveUser' />
        <menuitem action='DeleteUser' />
    </popup>
</ui>

'''


def get_int(s10) :
    index = s10.find('.')
    if index > -1 :
        s10 = s10[:index]
    try :
        return int(s10)
    except :
        return 0

def get_float(s10) :
    try :
        return float(s10)
    except :
        try :
            return locale.atof(s10)
        except :
            return 0.0

def get_string(float_val, digits, localized = True):
    fmt = '%' + '0.%sf' % digits
    if localized :
        return (locale.format(fmt, float_val))
    else :
        return (fmt % float_val)

def search_path(warn, f, *argsl) :
    if f == "" :
        return None

    if os.path.isfile(f) :
        return f

    src = NCAM_DIR
    i = 0
    j = argsl.__len__()
    while i < j :
        src = os.path.join(src, argsl[i])
        i += 1
    src = os.path.abspath(os.path.join(src, f))
    if os.path.isfile(src) :
        return src

    for pa in [GRAPHICS_DIR, CFG_DIR, CATALOGS_DIR, LIB_DIR, PROJECTS_DIR] :
        src = os.path.join(pa, f)
        if os.path.isfile(src) :
            return src
    src = os.path.join(os.getcwd(), f)
    if os.path.isfile(src) :
        return src

    if warn > search_warning.none:
        print(_("Can not find file %(filename)s") % {"filename":f})

    if warn == search_warning.dialog :
        mess_dlg(_("Can not find file %(filename)s") % {"filename":f})
    return None

def get_pixbuf(icon, size) :
    if size < 16 :
        size = 16
    if ((icon is None) or (icon.strip() == "")) :
        if DEFAULT_USE_NO_ICON:
            return None
        else :
            icon = NO_ICON_FILE

    icon_id = icon + str(size)

    if (icon_id) in PIXBUF_DICT :
        return PIXBUF_DICT[icon_id]

    icon_fname = search_path(search_warning.none, icon, GRAPHICS_DIR)
    if icon_fname is not None :
        try :
            pix_buf = gdk.pixbuf_new_from_file_at_size(icon_fname, size, size)
            PIXBUF_DICT[icon_id] = pix_buf
            return pix_buf
        except gdk.PixbufError as err :
            print(err)
            PIXBUF_DICT[icon_id] = None
    return None

def translate(fstring):
    # translate the glade file when testing translation
    txt2 = fstring.split('\n')
    fstring = ''
    for line in txt2 :
        inx = line.find('translatable="yes">')
        if inx > -1 :
            inx2 = line.find('</')
            txt = line[inx + 19:inx2]
            line = re.sub(r'%s' % txt, '%s' % _(txt), line)
        fstring += (line + '\n')
    return fstring

def mess_dlg(mess, title = "NativeCAM"):
    dlg = gtk.MessageDialog(parent = None,
        flags = gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
        type = gtk.MESSAGE_WARNING,
        buttons = gtk.BUTTONS_OK, message_format = '%s' % mess)
    dlg.set_title(title)
    dlg.set_keep_above(True)
    dlg.run()
    dlg.destroy()

def mess_yesno(mess, title = ""):
    return mess_with_buttons(mess, (gtk.STOCK_YES, gtk.RESPONSE_YES, \
                             gtk.STOCK_NO, gtk.RESPONSE_NO), title) == gtk.RESPONSE_YES

def mess_with_buttons(mess, buttons, title = ""):
    mwb = gtk.Dialog(parent = None,
                     buttons = buttons,
                     flags = gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
          )
    mwb.set_title(title)
    finbox = mwb.get_content_area()
    l = gtk.Label(mess)
    finbox.pack_start(l)
    mwb.set_keep_above(True)
    mwb.show_all()
    response = mwb.run()
    mwb.hide()
    mwb.destroy()
    return response

class copymode:  # 'enum' items
    one_at_a_time, yes_to_all, no_to_all = list(range(3))

def copy_dir_recursive(fromdir, todir,
                       update_ct = 0,
                       mode = copymode.one_at_a_time,
                       overwrite = False,
                       verbose = False) :
    if not os.path.isdir(todir) :
        os.makedirs(todir, 0o755)

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
        else :  # local file exists and not overwrite
            if (hashlib.md5(open(frompath, 'rb').read()).digest()
                 == hashlib.md5(open(topath, 'rb').read()).digest()) :
                # files are same
                if verbose :
                    print("NOT copying %s to %s" % (p, todir))
            else :  # files are different
                if (os.path.getctime(frompath) < os.path.getctime(topath)) :
                    # different and local file most recent
                    if verbose :
                        print(_('Keeping modified local file %(filename)s') % {"filename":p})
                    pass
                else :  # different and system file is most recent
                    if mode == copymode.yes_to_all :
                        if verbose :
                            print("copying %s to %s" % (p, todir))
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
                                            title = _("NEW file version available"))

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
                            print("copying %s to %s" % (p, todir))
                        shutil.copy(frompath, topath)
                        update_ct += 1

                    if ans == gtk.RESPONSE_NO or mode == copymode.no_to_all :
                        os.utime(topath, None)  # touch it (update timestamp)

    return mode, update_ct

def err_exit(errtxt):
    print(errtxt)
    mess_dlg(errtxt)
    sys.exit(1)

if platform.system() != 'Windows' :
    try :
        import linuxcnc
    except ImportError as detail :
        err_exit(detail)

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
    else :
        if ':' in val :
            val = val.split(':')[0]

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

        print("[RS274NGC]SUBROUTINE_PATH = %s\n  Real paths:" % subroutine_path)

        for i, d in enumerate(subroutine_path.split(":")):
            d = os.path.expanduser(d)
            if os.path.isabs(d) :
                thedir = d
            else :
                thedir = os.path.join(os.path.realpath(os.path.dirname(fname)), d)
            if os.path.isdir(thedir) :
                print("   %s" % (os.path.realpath(thedir)))
                if not found_lib_dir :
                    found_lib_dir = thedir.find(require_lib) == 0

        print("")

        if not found_lib_dir :
            err_exit (_('\nThe required NativeCAM lib directory :\n<%(lib)s>\n\n'
                      'is not in [RS274NGC]SUBROUTINE_PATH:\n'
                      '<%(path)s>\n\nEdit ini and correct\n'
                    % {'lib':require_lib, 'path':subroutine_path}))

    except Exception as detail :
        err_exit(_('Required NativeCAM lib\n%(err_details)s') % {'err_details':detail})

def get_short_id():
    global UNIQUE_ID
    UNIQUE_ID += 1
    return str(UNIQUE_ID)

def create_M_file() :
    p = os.path.join(NCAM_DIR, NGC_DIR, 'M123')
    with open(p, 'wb') as f :
        f.write('#!/usr/bin/env python\n# coding: utf-8\n')

        f.write("import gtk\nimport os\nimport pygtk\npygtk.require('2.0')\nfrom gtk import gdk\n\n")

        f.write("fname = '%s'\n" % os.path.join(NCAM_DIR, CATALOGS_DIR, 'no_skip_dlg.conf'))
        f.write('if os.path.isfile(fname) :\n    exit(0)\n\n')

        f.write("msg = '%s'\n" % _('Stop LinuxCNC program,&#10;toggle the shown button,&#10;then restart'))
        f.write("msg1 = '%s'\n" % _('Skip block not active'))
        f.write("icon_fname = '%s'\n\n" % os.path.join(NCAM_DIR, GRAPHICS_DIR, 'skip_block.png'))

        f.write('dlg = gtk.MessageDialog(parent = None, flags = gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT, type = gtk.MESSAGE_WARNING, buttons = gtk.BUTTONS_NONE, message_format = msg1)\n\n')

        f.write("dlg.set_title('NativeCAM')\ndlg.format_secondary_markup(msg)\n\n")

        f.write('dlg.set_image(gtk.Image())\n')
        f.write('dlg.get_image().set_from_pixbuf(gdk.pixbuf_new_from_file_at_size(icon_fname, 80, 80))\n\n')

        f.write('cb = gtk.CheckButton(label = "%s")\n' % _("Do not show again"))
        f.write('dlg.get_content_area().pack_start(cb, True, True, 0)\n')
        f.write('dlg.add_button(gtk.STOCK_OK, gtk.RESPONSE_OK).grab_focus()\n\n')

        f.write('dlg.set_keep_above(True)\ndlg.show_all()\n\ndlg.run()\n')
        f.write("if cb.get_active() :\n    open(fname, 'w').close()\n")
        f.write('exit(0)\n')

    os.chmod(p, 0o755)
    mess_dlg(_('LinuxCNC needs to be restarted now'))

class Tools(object):

    def __init__(self):
        self.table_fname = None
        self.list = ''
        self.orientation = 0

    def set_file(self, tool_table_file):
        fn = search_path(search_warning.dialog, tool_table_file)
        if fn is not None :
            self.table_fname = fn
            self.load_table()

    def load_table(self, *arg):
        self.table = None
        self.table = []
        if self.table_fname is not None :
            tbl = open(self.table_fname).read().split("\n")
            for s in tbl :
                s = s.strip()
                if ";" in s:
                    tnumber = '0'
                    torient = '0'
                    s = s.split(";")
                    tdesc = s[1][0:]
                    s = s[0][0:]
                    s1 = s.split(" ")
                    for s2 in s1 :
                        if (len(s2) > 1) :
                            if (s2[0] == 'T') :
                                tnumber = s2[1:]
                            elif (s2[0] == 'Q') :
                                torient = s2[1:]
                    if tnumber != '0' :
                        if tdesc == '' :
                            tdesc = _('no description')
                        self.table.append([int(tnumber), tnumber, tdesc, torient])
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

    def save_tool_orient(self, tn):
        if tn == 0 :
            self.orientation = 0
        else :
            for tool in self.table :
                if tool[0] == tn :
                    self.orientation = get_int(tool[3])

    def get_tool_orient(self):
        return self.orientation

class VKB(object):

    def __init__(self, toplevel, tooltip, min_value, max_value, data_type, convertible) :
        self.dlg = gtk.Dialog(parent = toplevel)
        self.dlg.set_decorated(False)
        self.dlg.set_transient_for(None)
        self.dlg.set_border_width(3)
        self.dlg.set_property("skip-taskbar-hint", True)

        lbl = gtk.Label('')
        lbl.set_line_wrap(True)
        self.dlg.vbox.pack_start(lbl, expand = False)
        lbl.set_markup(tooltip)

        self.entry = gtk.Label('')
        self.entry.modify_font(pango.FontDescription('sans 14'))
        self.entry.set_alignment(1.0, 0.5)
        self.entry.set_property('ellipsize', pango.ELLIPSIZE_START)

        self.min_value = min_value
        self.max_value = max_value
        self.data_type = data_type
        self.convertible_units = convertible

        box = gtk.EventBox()
        box.modify_bg(gtk.STATE_NORMAL, gtk.gdk.Color('#FFFFFF'))

        box.add(self.entry)
        frame = gtk.Frame()
        frame.add(box)

        tbl = gtk.Table(rows = 6, columns = 5, homogeneous = True)
        tbl.attach(frame, 0, 5, 0, 1,
                   xoptions = gtk.EXPAND | gtk.FILL,
                   yoptions = gtk.EXPAND | gtk.FILL)

        self.dlg.vbox.pack_start(tbl)

        btn = gtk.Button(_('BS'))
        btn.connect("clicked", self.input, 'BS')
        btn.set_can_focus(False)
        tbl.attach(btn, 4, 5, 2, 3)

        i = 0
        for lbl in ['F2', 'Pi', '()', '=', 'C'] :
            btn = gtk.Button(lbl)
            btn.connect("clicked", self.input, lbl)
            btn.set_can_focus(False)
            tbl.attach(btn, i, i + 1, 1, 2)
            i = i + 1

        i = 2
        for lbl in ['/', '*', '-', '+'] :
            btn = gtk.Button(lbl)
            btn.connect("clicked", self.input, lbl)
            btn.set_can_focus(False)
            tbl.attach(btn, 3, 4, i, i + 1)
            i = i + 1

        k = 10
        for i in range(2, 5) :
            k = k - 3
            for j in range(0, 3):
                lbl = str(k + j)
                btn = gtk.Button(lbl)
                btn.connect("clicked", self.input, lbl)
                btn.set_can_focus(False)
                tbl.attach(btn, j, j + 1, i, i + 1)

        if (self.min_value < 0.0) :
            btn = gtk.Button('+/-')
            btn.connect("clicked", self.input, '+/-')
            btn.set_can_focus(False)
            tbl.attach(btn, 2, 3, 5, 6)
            last_col = 2
        else :
            last_col = 3

        if self.data_type == 'float' :  # and get_int(self.digits) > 0 :
            btn = gtk.Button(decimal_point)
            btn.connect("clicked", self.input, decimal_point)
            btn.set_can_focus(False)
            tbl.attach(btn, last_col - 1, last_col, 5, 6)
            last_col = last_col - 1

        btn = gtk.Button('0')
        btn.connect("clicked", self.input, '0')
        btn.set_can_focus(False)
        tbl.attach(btn, 0, last_col, 5, 6)

        btn = gtk.Button()
        img = gtk.Image()
        img.set_from_stock('gtk-cancel', menu_icon_size)
        btn.set_image(img)
        btn.connect("clicked", self.cancel)
        btn.set_can_focus(False)
        tbl.attach(btn, 4, 5, 3, 4)

        if self.convertible_units :
            btn = gtk.Button()
            img = gtk.Image()
            img.set_from_pixbuf(get_pixbuf('mm2in.png', treeview_icon_size))
            btn.set_image(img)
            btn.connect("clicked", self.input, 'CV')
            btn.set_can_focus(False)
            tbl.attach(btn, 4, 5, 4, 5)

        self.OKbtn = gtk.Button()
        img = gtk.Image()
        img.set_from_stock('gtk-ok', menu_icon_size)
        self.OKbtn.set_image(img)
        self.OKbtn.connect("clicked", self.ok)
        self.OKbtn.set_can_focus(False)
        if self.convertible_units :
            tbl.attach(self.OKbtn, 4, 5, 5, 6)
        else :
            tbl.attach(self.OKbtn, 4, 5, 4, 6)

        self.dlg.connect('key-press-event', self.key_press_event)
        self.dlg.connect('focus-out-event', self.focus_out)
        self.dlg.set_keep_above(True)

    def __enter__(self):
        self.not_allowed_msg = _("Not allowed - F2 edit")
        self.err_msg = _("Error - F2 edit")
        return self

    def initvalue(self, value, saved, initialize):
        self.entry.set_markup('<b>%s</b>' % value)
        self.save_edit = saved
        self.initialize = initialize

    def run(self, not_allowed):

        def show_error(errm) :
            self.entry.set_markup('<b>%s</b>' % errm)
            self.initialize = True

        self.opened_paren = 0
        while True :
            self.convert_units = False
            self.OKbtn.grab_focus()
            response = self.dlg.run()
            if response == gtk.RESPONSE_OK:
                if self.entry.get_text() in ['', self.not_allowed_msg, self.err_msg] :
                    self.entry.set_markup('<b>0</b>')
                is_good, rval = self.compute(self.entry.get_text())
                if not is_good :
                    show_error(self.err_msg)
                elif self.data_type == 'int' :
                    val = int(rval)
                    a_min = int(self.min_value)
                    a_max = int(self.max_value)

                    if val > a_max :
                        val = a_max
                    elif val < a_min :
                        val = a_min

                    if not_allowed is not None :
                        for na in not_allowed.split(':') :
                            if get_int(na) == val :
                                is_good = False
                                show_error(self.not_allowed_msg)
                                break
                    if is_good :
                        return response, str(val)
                else:
                    if self.convert_units :
                        if default_metric :
                            rval = rval * 25.4
                        else :
                            rval = rval / 25.4

                    if rval > self.max_value :
                        rval = self.max_value
                    elif rval < self.min_value :
                        rval = self.min_value

                    if not_allowed is not None :
                        for na in not_allowed.split(':') :
                            if get_float(na) == rval :
                                is_good = False
                                show_error(self.not_allowed_msg)
                                break
                    if is_good :
                        return response, str(rval)
            else :
                return response, None

    def input(self, btn, data):
        if self.initialize :
            lbl = '0'
            self.initialize = False
            self.opened_paren = 0
        else :
            lbl = self.entry.get_text()
            if lbl in ['0.0', '0.00', '0.000', '0.0000', '0.00000', '0.000000'] :
                lbl = '0'

        if data == 'C' :
            self.entry.set_markup('<b>0</b>')
            self.opened_paren = 0

        elif data == '=' :
            is_good, rval = self.compute(self.entry.get_text())
            if not is_good :
                self.show_error(_("Error - F2 to edit"))
            elif self.data_type == 'int' :
                self.entry.set_markup('<b>%d</b>' % int(rval))
            else :
                self.entry.set_markup('<b>%s</b>' % locale.format('%0.6f', rval))

        elif data == 'F2' :
            self.entry.set_markup('<b>%s</b>' % self.save_edit)

        elif data == 'BS' :
            if (len(lbl) == 1) or lbl == 'Pi':
                self.input(None, 'C')
                return
            elif lbl[-1] == 'i' :
                self.entry.set_markup('<b>%s</b>' % lbl[0:-2])
            elif lbl[-1] == ')' :
                self.opened_paren += self.opened_paren
                self.entry.set_markup('<b>%s</b>' % lbl[0:-1])
            elif lbl[-1] == '(' :
                self.entry.set_markup('<b>%s</b>' % lbl[0:-1])
                self.opened_paren -= 1
            else :
                self.entry.set_markup('<b>%s</b>' % lbl[0:-1])

        elif data == 'CV' :
            self.convert_units = True
            self.dlg.response(gtk.RESPONSE_OK)

        elif data == '+/-' :
            if lbl == '0' :
                self.entry.set_markup('<b>-</b>')
            elif lbl.find('-') == 0 :
                self.entry.set_markup('<b>%s</b>' % lbl[1:])
            else :
                self.entry.set_markup('<b>-%s</b>' % lbl)

        elif data == 'Pi' :
            if lbl == '0' :
                self.entry.set_markup('<b>%s</b>' % data)
            elif lbl[-1] in ['+', '-', '*', '/', '('] :
                self.entry.set_markup('<b>%s%s</b>' % (lbl, data))

        elif data in ['*', '/', '+'] :
            if lbl != '0' and not lbl[-1] in ['+', '-', '*', '/', '('] :
                self.entry.set_markup('<b>%s%s</b>' % (lbl, data))

        elif data == '()' :
            if lbl == '0' :
                self.entry.set_markup('<b>(</b>')
                self.opened_paren = 1
            elif lbl[-1] in ['+', '-', '*', '/', '('] :
                self.entry.set_markup('<b>%s(</b>' % lbl)
                self.opened_paren += 1
            elif lbl[-1] not in ['+', '-', '*', '/', '('] :
                if self.opened_paren > 0 :
                    self.entry.set_markup('<b>%s)</b>' % lbl)
                    self.opened_paren -= 1

        elif data == decimal_point :
            if lbl == '0' :
                self.entry.set_markup('<b>0%s</b>' % data)
            elif lbl[-1] in ['+', '-', '*', '/', '('] :
                self.entry.set_markup('<b>%s0%s</b>' % (lbl, data))
            elif lbl[-1] >= '0' and lbl[-1] <= '9' :
                j = len(lbl)
                i = 0
                while (i < j) :
                    car = lbl[-i]
                    i += 1
                    if car == decimal_point :
                        return
                    if car in ['+', '-', '*', '/', '('] :
                        self.entry.set_markup('<b>%s%s</b>' % (lbl, data))
                        return
                self.entry.set_markup('<b>%s%s</b>' % (lbl, data))

        else :
            if lbl == '0' :  # numbers and minus sign
                self.entry.set_markup('<b>%s</b>' % data)
            elif lbl[-1] not in [')', 'i'] :
                self.entry.set_markup('<b>%s%s</b>' % (lbl, data))

    def key_press_event(self, win, event):
        if event.type == gdk.KEY_PRESS:
            k_name = gdk.keyval_name(event.keyval)
#            print(k_name)
            if ((k_name >= 'KP_0' and k_name <= 'KP_9') or \
                    (k_name >= '0' and k_name <= '9')) :
                self.input(None, k_name[-1])
            elif k_name in ['KP_Decimal', 'period', 'comma', 'KP_Separator'] :
                if (self.data_type == 'float'):
                    self.input(None, decimal_point)
            elif k_name in ['KP_Divide', 'slash'] :
                self.input(None, '/')
            elif k_name in ['KP_Multiply', 'asterisk'] :
                self.input(None, '*')
            elif k_name in ['parenleft', 'parenright'] :
                self.input(None, '()')
            elif k_name == 'F2' :
                self.input(None, 'F2')
            elif k_name in ['C', 'c'] :
                self.input(None, 'C')
            elif k_name == 'equal' :
                self.input(None, '=')
            elif k_name in ['KP_Subtract', 'minus'] :
                self.input(None, '-')
            elif k_name in ['KP_Add', 'plus'] :
                self.input(None, '+')
            elif k_name == 'BackSpace' :
                self.input(None, 'BS')
            elif k_name in ['KP_Enter', 'Return', 'space']:
                self.dlg.response(gtk.RESPONSE_OK)

    def ok(self, btn):
        self.convert_units = False
        self.dlg.response(gtk.RESPONSE_OK)

    def cancel(self, btn):
        self.dlg.response(gtk.RESPONSE_CANCEL)

    def focus_out(self, widget, event):
        if vkb_cancel_on_out:
            self.dlg.response(gtk.RESPONSE_CANCEL)
        else :
            self.dlg.response(gtk.RESPONSE_OK)

    def compute(self, input_string):
        while input_string.count('(') > input_string.count(')') :
            input_string = input_string + ')'
        self.opened_paren = 0
        self.save_edit = input_string

        for i in('-', '+', '/', '*', '(', ')'):
            input_string = input_string.replace(i, " %s " % i)
        input_string = input_string.replace('Pi', str(math.pi))

        qualified = ''
        for i in input_string.split():
            try:
                i = str(locale.atof(i))
                qualified = qualified + str(float(i))
            except:
                qualified = qualified + i

        try :
            return True, eval(qualified)
        except :
            return False, 0.0

    def __exit__(self, type, value, traceback):
        self.dlg.hide()
        self.dlg.destroy()
        self.dlg = None


class CellRendererMx(gtk.CellRendererText):

    def __init__(self, treeview) :
        gtk.CellRendererText.__init__(self)
        self.set_property('xpad', 2)
        self.set_property("wrap-mode", 2)
        self.set_property("editable", True)

        self.max_value = 999999.9
        self.min_value = -999999.9
        self.data_type = 'string'
        self.tv = treeview
        self.options = ''
        self.param_value = ''
        self.combo_values = []
        self.tooltip = ''
        self.preedit = None
        self.edited = None
        self.refresh_fn = None
        self.inputKey = ''
        self.tool_list = []
        self.not_allowed = None
        self.convertible_units = False
        self.convert_units = False
        self.save_edit = ''
        self.opened_paren = 0

    def set_convertible_units(self, value):
        self.convertible_units = value

    def set_tooltip(self, value):
        self.tooltip = value

    def set_max_value(self, value):
        self.max_value = value

    def set_param_value(self, value):
        self.param_value = value

    def set_not_allowed(self, value):
        self.not_allowed = value

    def set_min_value(self, value):
        self.min_value = value

    def set_data_type(self, value):
        self.data_type = value

    def set_edit_datatype(self, value):
        self.editdata_type = value

    def set_refresh_fn(self, value):
        self.refresh_fn = value

    def set_Input(self, value):
        self.inputKey = value

    def set_fileinfo(self, patrn, mime_type, filter_name):
        self.pattern = patrn
        self.mime_type = mime_type
        self.filter_name = filter_name

    def set_toolinfo(self, toollist):
        self.tool_list = toollist

    def set_options(self, value):
        self.options = value.replace('&#176;', '°')

    def set_digits(self, value):
        self.digits = value

    def set_preediting(self, value):
        self.preedit = value

    def get_treeview(self):
        return self.tv

    def do_get_size(self, widget, cell_area):
        return (gtk.CellRendererText.do_get_size(self, widget, cell_area))

    def edit_number(self, time_out = 0.05) :

        with VKB(self.tv.get_toplevel(), self.tooltip, self.min_value, self.max_value,
                 self.editdata_type, self.convertible_units) as vkb :

            tree_x, tree_y = self.tv.get_bin_window().get_origin()
            tree_w, tree_h = self.tv.window.get_geometry()[2:4]

            vkb.dlg.set_size_request(vkb_width, vkb_height)
            vkb.dlg.resize(vkb_width, vkb_height)

            x = tree_w - vkb_width
            if x > self.cell_area.x :
                x = self.cell_area.x
            y = tree_y + self.cell_area.y + self.cell_area.height
            vkb.dlg.move(tree_x + x + 2, y)

            initialize = self.inputKey == ''
            if not initialize :
                if ((self.data_type == 'int') and \
                        (decimal_point in self.inputKey)) or \
                        (self.inputKey == 'BS') :
                    vkb.initvalue('0', self.param_value, initialize)
                else :
                    vkb.initvalue(self.inputKey, self.param_value, initialize)

                self.inputKey = ''
            else :
                vkb.initvalue(self.param_value, self.param_value, initialize)

            vkb.dlg.show_all()
            time.sleep(time_out)
            return vkb.run(self.not_allowed)

    def edit_list(self, time_out = 0.05):
        self.list_window = gtk.Dialog(parent = self.tv.get_toplevel())
        self.list_window.set_border_width(0)
        self.list_window.set_decorated(False)
        self.list_window.set_transient_for(None)
        self.list_window.set_property("skip-taskbar-hint", True)
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

        self.list_window.move(tree_x + self.cell_area.x - 5, y - 2)
        self.list_window.resize(tree_w - self.cell_area.x + 5, lw_height)
        position = int(sw.get_vadjustment().get_upper() * active_row / count)
        sw.get_vadjustment().set_value(position)

        self.list_window.show_all()
        ls_view.set_cursor(active_row)
        ls_view.grab_focus()

        time.sleep(time_out)

        response = self.list_window.run()

        model, ls_itr = ls_view.get_selection().get_selected()
        new_val = model.get_value(ls_itr, 1)
        self.list_window.hide()
        self.list_window.destroy()
        time.sleep(time_out)
        return response, new_val

    def edit_string(self, time_out = 0.05):
        self.stringedit_window = gtk.Dialog(parent = self.tv.get_toplevel())
        self.stringedit_window.hide()
        self.stringedit_window.set_decorated(False)
        self.stringedit_window.set_transient_for(None)
        self.stringedit_window.set_border_width(0)
        self.stringedit_window.set_property("skip-taskbar-hint", True)

        self.stringedit_entry = gtk.Entry()
        self.stringedit_window.vbox.add(self.stringedit_entry)
        self.stringedit_entry.set_editable(True)

        self.stringedit_entry.connect('key-press-event', self.string_edit_keyhandler)

        # position the popup on the edited cell
        (tree_x, tree_y) = self.tv.get_bin_window().get_origin()
        (tree_w, tree_h) = self.tv.window.get_geometry()[2:4]
        x = tree_x + self.cell_area.x
        y = tree_y + self.cell_area.y
        self.stringedit_window.move(x - 4, y - 2)
        self.stringedit_window.resize(tree_w - self.cell_area.x + 4, self.cell_area.height)
        self.stringedit_window.show_all()
        self.stringedit_entry.grab_focus()
        self.stringedit_entry.connect('focus-out-event', self.string_edit_focus_out)

        time.sleep(time_out)
        if self.inputKey != 'BS' :
            self.stringedit_entry.set_text(self.param_value)
        self.inputKey = ''
        response = self.stringedit_window.run()
        new_val = self.stringedit_entry.get_text()
        self.stringedit_window.hide()
        self.stringedit_window.destroy()
        return response, new_val

    def list_keypress(self, widget, event) :
        keyname = gdk.keyval_name(event.keyval)
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

        if self.editdata_type in GROUP_HEADER_TYPES or self.editdata_type == 'grayed' :
            self.inputKey = ''
            return None

        if self.editdata_type == 'prjname' :
            self.inputKey = ''
            return None

        self.cell_area = cell_area

        if self.editdata_type in NUMBER_TYPES :
            response, result = self.edit_number()
            if response == gtk.RESPONSE_OK :
                self.edited(self, path, result)
            return None

        elif self.editdata_type in ['bool', 'boolean']:
            if self.inputKey > '' :
                self.inputKey = ''
                return None
            if self.param_value == '0' :
                self.edited(self, path, '1')
            else :
                self.edited(self, path, '0')
            return None

        elif self.editdata_type in ['combo-user', 'combo', 'tool']:
            self.inputKey = ''
            response, result = self.edit_list()
            if response == gtk.RESPONSE_OK :
                self.edited(self, path, result)
            return None

        elif self.editdata_type in ['string', 'gcode'] :
            response, result = self.edit_string()
            if response == gtk.RESPONSE_OK :
                self.edited(self, path, result)
            return None

        elif self.editdata_type == 'filename':
            if self.inputKey > '' :
                self.inputKey = ''

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
                filechooserdialog.set_keep_above(True)

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
                    self.edited(self, path, filechooserdialog.get_filename())
            finally:
                filechooserdialog.destroy()
            return None

        else :  # edit multi-line text
            self.selection = treeview.get_selection()
            self.treestore, self.treeiter = self.selection.get_selected()

            self.textedit_window = gtk.Dialog(parent = treeview.get_toplevel())
            self.textedit_window.set_decorated(False)
            self.textedit_window.set_transient_for(None)
            self.textedit_window.set_property("skip-taskbar-hint", True)

            self.textedit = gtk.TextView()
            self.textedit.set_editable(True)
            self.textbuffer = self.textedit.get_buffer()
            self.textedit.set_wrap_mode(gtk.WRAP_WORD)
            self.textbuffer.set_property('text', self.get_property('text'))

            self.textedit_window.connect('key-press-event', self.text_edit_keyhandler)
            self.textedit_window.connect('focus-out-event', self.text_edit_focus_out, path)

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

            response = self.textedit_window.run()
            if response == gtk.RESPONSE_OK:
                (iter_first, iter_last) = self.textbuffer.get_bounds()
                text = self.textbuffer.get_text(iter_first, iter_last)
                self.edited(self, path, text)
            self.textedit_window.hide()
            self.textedit_window.destroy()
            self.refresh_fn(self.tv)
            return None

    def do_render(self, win, widget, background_area, cell_area,
                  expose_area, flags):
        if self.data_type in ['bool', 'boolean'] :
            cell_area.width = 30
            chk = gtk.CellRendererToggle()
            chk.set_active(self.param_value == '1')
            chk.render(win, widget, background_area, cell_area,
                       expose_area, flags)
        else :
            gtk.CellRendererText.do_render(self, win, widget,
                                           background_area,
                                           cell_area, expose_area, flags)

    def string_edit_focus_out(self, widget, event):
        self.stringedit_window.response(gtk.RESPONSE_OK)

    def string_edit_keyhandler(self, widget, event):
        keyname = gdk.keyval_name(event.keyval)
        if keyname in ['Return', 'KP_Enter']:
            self.stringedit_window.response(gtk.RESPONSE_OK)

    def text_edit_focus_out(self, widget, event, path):
        self.textedit_window.response(gtk.RESPONSE_OK)

    def text_edit_keyhandler(self, widget, event):
        keyname = gdk.keyval_name(event.keyval)
        if gdk.keyval_name(event.keyval) in ['Return', 'KP_Enter'] :
            if event.state & (gdk.SHIFT_MASK | gdk.CONTROL_MASK) :
                pass
            else :
                event.keyval = 0
                self.textedit_window.response(gtk.RESPONSE_OK)

gobject.type_register(CellRendererMx)

class Parameter(object) :
    def __init__(self, ini = None, ini_id = None, xml = None) :
        self.attr = {}
        if ini is not None :
            self.from_ini(ini, ini_id)
        elif xml is not None :
            self.from_xml(xml)

    def __repr__(self) :
        return etree.tostring(self.to_xml(), pretty_print = True)

    def __delattr__(self, *args, **kwargs):
        return object.__delattr__(self, *args, **kwargs)

    def from_ini(self, ini, ini_id) :
        self.attr = {}
        ini = dict(ini)
        for i in ini :
            self.attr[i] = ini[i]
        if "type" not in self.attr or self.attr["type"] not in SUPPORTED_DATA_TYPES :
            self.attr["type"] = 'string'

        if "call" not in self.attr :
            self.attr["call"] = "#" + ini_id

    def from_xml(self, xml) :
        for i in list(xml.keys()) :
            self.attr[i] = xml.get(i)

    def to_xml(self) :
        xml = etree.Element("param")
        for i in self.attr :
            xml.set(i, unicode(str(self.attr[i])))
        return xml

    def get_icon(self, icon_size) :
        icon = self.get_attr("icon")
        return get_pixbuf(icon, icon_size)

    def get_value(self, editor = False) :
        if self.get_type() == 'float' :
            if default_metric and "metric_value" in self.attr :
                return get_string(get_float(self.attr["value"]) * 25.4, 6, editor)
            else :
                return get_string(get_float(self.attr["value"]), 6, editor)
        else :
            return self.attr["value"] if "value" in self.attr else ""

    def get_ngc_value(self):
        if self.get_type() == 'gcode' :
            val = self.attr["value"] if "value" in self.attr else ""
            if val == '' :
                return '0'
            else :
                return val
        if self.get_type() == 'float' :
            if machine_metric and "metric_value" in self.attr :
                return get_string(get_float(self.attr["value"]) * 25.4, 6, False)
            else :
                return get_string(get_float(self.attr["value"]), 6, False)
        else :
            return self.attr["value"] if "value" in self.attr else ""

    def set_value(self, new_val, parent) :
        done = False
        cancel = False
        if 'on_change' in self.attr :
            exec(self.attr['on_change'])
        if cancel :
            return False
        if not done :
            if self.get_type() == "float" :
                factor = 25.4 if (default_metric and "metric_value" in self.attr) else 1
                new_val = get_string(get_float(new_val) / factor, 10, False)
                old_val = get_string(get_float(self.attr["value"]), 10, False)
            else :
                old_val = self.attr["value"]
            if new_val == old_val :
                return False
            else :
                self.attr["value"] = new_val
        if 'value_changed' in self.attr :
            exec(self.attr['value_changed'])
        return True

    def get_display_string(self) :
        if self.get_type() == "float" :
            if default_metric and "metric_value" in self.attr :
                return get_string(get_float(self.attr["value"]) * 25.4, self.get_digits())
            else :
                return get_string(get_float(self.attr["value"]), self.get_digits())
        else :
            return self.attr["value"] if "value" in self.attr else ""

    def set_hidden(self, hide):
        if hide :
            self.attr['hidden'] = '2'
        elif self.get_hidden() :
            self.attr['hidden'] = '0'
            return 1
        return 0

    def get_grayed(self):
        return (self.attr["grayed"] == '1') if "grayed" in self.attr else False

    def set_grayed(self, value):
        if value :
            self.attr["grayed"] = '1'
        else :
            self.attr["grayed"] = '0'

    def change_group(self):
        t = self.get_type()
        if t in ['sub-header', 'header'] :
            if t == 'sub-header' :
                if 'header' in self.attr :
                    return False
                self.set_type('header')
            else :
                self.set_type('sub-header')
            return True
        return False

    def get_hidden(self):
        return ('hidden' in self.attr) and (self.attr['hidden'] == '2')

    def get_name(self) :
        return _(self.attr["name"]) if "name" in self.attr else ""

    def get_options(self):
        return _(self.attr["options"]) if "options" in self.attr else ""

    def get_type(self):
        return self.attr["type"]

    def set_type(self, new_type):
        self.attr['old_type'] = self.attr['type']
        self.attr['type'] = new_type
        if new_type == 'gcode' and default_metric and "metric_value" in self.attr :
            self.attr["value"] = self.attr["metric_value"]

    def revert_type(self):
        if 'old_type' in self.attr :
            if self.attr['old_type'] == 'float' :
                val = get_float(self.attr['value'])
                if val < get_float(self.get_min_value()) :
                    val = get_float(self.get_min_value())
                if val > get_float(self.get_max_value()) :
                    val = get_float(self.get_max_value())
                self.attr['value'] = str(val)
                self.attr['type'] = 'float'
            elif self.attr['old_type'] == 'int' :
                val = get_int(self.attr['value'])
                if val < get_int(self.get_min_value()) :
                    val = get_int(self.get_min_value())
                if val > get_int(self.get_max_value()) :
                    val = get_int(self.get_max_value())
                self.attr['value'] = str(val)
                self.attr['type'] = 'int'

    def get_tooltip(self):
        return _(self.attr["tool_tip"]) if "tool_tip" in self.attr else self.get_name()

    def get_attr(self, name) :
        return self.attr[name] if name in self.attr else None

    def get_digits(self):
        if self.get_type() == 'int' :
            return '0'
        else :
            return self.attr["digits"] if "digits" in self.attr else default_digits

    def set_digits(self, new_digits) :
        self.attr["digits"] = new_digits

    def get_min_value(self):
        min_v = self.attr["minimum_value"] if "minimum_value" in self.attr \
                        else "-999999.9"
        if self.get_type() == 'float' and default_metric and 'metric_value' in self.attr :
            return str(get_float(min_v) * 25.4)
        else :
            return min_v

    def get_max_value(self):
        max_v = self.attr["maximum_value"] if "maximum_value" in self.attr \
                        else "999999.9"
        if self.get_type() == 'float' and default_metric and 'metric_value' in self.attr :
            return str(get_float(max_v) * 25.4)
        else :
            return max_v

class Feature(object):
    def __init__(self, src = None, xml = None) :
        self.attr = {}
        self.param = []
        if src is not None :
            self.from_src(src)
        elif xml is not None :
            self.from_xml(xml)

    def __repr__(self) :
        return etree.tostring(self.to_xml(), pretty_print = True)

    def get_grayed(self):
        return (self.attr["grayed"] == '1') if "grayed" in self.attr else False

    def get_icon(self, icon_size) :
        return get_pixbuf(self.get_attr("icon"), icon_size)

    def get_value(self):
        return self.attr["value"] if "value" in self.attr else ""

    def get_version(self):
        return get_float(self.attr["version"]) if "version" in self.attr else 0.0

    def get_display_string(self):
        return self.get_value()

    def set_value(self, new_val):
        self.attr["value"] = new_val

    def get_type(self):
        return self.attr["type"] if "type" in self.attr else "string"

    def get_tooltip(self):
        s = _(self.attr["tool_tip"]) if "tool_tip" in self.attr else \
            _(self.attr["help"]) if "help" in self.attr else None
        return s.replace('&#176;', '°')

    def get_attr(self, attr) :
        return self.attr[attr] if attr in self.attr else None

    def get_param(self, param_id):
        for p in self.param :
            if 'call' in p.attr and p.attr['call'] == "#%s" % param_id :
                return p
        return None

    def get_name(self):
        return _(self.attr["name"]) if "name" in self.attr else _("unname")

    def from_src(self, src) :
        src_config = ConfigParser.ConfigParser()
        uf = io.open(src).read()
        f = str(uf)

        # remove _(" and ")
        f = re.sub(r"_\(\"", "", f)
        f = re.sub(r"\"\)", "", f)

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

        ftype = self.attr["type"]
        if ftype is None :
            raise Exception(_('Type not defined for\n%s') % src)

        # get order
        if "order" not in self.attr :
            self.attr["order"] = []
        else :
            self.attr["order"] = self.attr["order"].upper().split()
        self.attr["order"] = [s if s[:6] == "PARAM_" else "PARAM_" + s \
                              for s in self.attr["order"]]

        self.attr['hidden_count'] = '0'
        # get params
        self.param = []
        parameters = self.attr["order"] + [p for p in conf if \
                    (p[:6] == "PARAM_" and p not in self.attr["order"])]
        for s in parameters :
            if s in conf :
                pn = s.lower()
                p = Parameter(ini = conf[s], ini_id = pn)

                p_id = "%s:%s" % (ftype, pn)
                if (p_id + '--type') in USER_VALUES :
                    p.set_type(USER_VALUES[p_id + '--type'])

                # set hidden as per user preferences
                if (p_id + '--hidden') in USER_VALUES :
                    p.set_hidden(True)
                    self.hide_field()

                if (p_id + '--grayed') in USER_VALUES :
                    p.attr["grayed"] = USER_VALUES[p_id + '--grayed']

                if (p_id + '--name') in USER_VALUES :
                    p.attr["name"] = USER_VALUES[p_id + '--name']

                if (p_id + '--value') in USER_VALUES :
                    p.attr["value"] = USER_VALUES[p_id + '--value']

                self.param.append(p)

        self.attr["id"] = ftype + '_000'

        # get gcode parameters
        for l in ["DEFINITIONS", "BEFORE", "CALL", "AFTER", "VALIDATION", "INIT"] :
            if l in conf and "content" in conf[l] :
                self.attr[l.lower()] = re.sub(r"(?m)\r?\n\r?\.", "\n",
                                              conf[l]["content"])
            else :
                self.attr[l.lower()] = ""

        parent = self
        exec(self.attr['init'])

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
            num = max([get_int(i.get("id")[-3: ]) for i in l] + [0]) + 1
        self.attr["id"] = self.attr["type"] + "_%03d" % num

    def get_definitions(self) :
        s = self.attr["definitions"] if "definitions" in self.attr else ''
        if s != '' :
            s = self.process(s)
        return s

    def include(self, srce) :
        src = search_path(search_warning.dialog, srce, LIB_DIR)
        if src is not None:
            return io.open(src).read()
        return ''

    def include_once(self, src) :
        global INCLUDE
        if src not in INCLUDE :
            INCLUDE.append(src)
            return self.include(src)
        return ""

    def replace_params(self, s):
        for p in self.param :
            if "call" in p.attr and "value" in p.attr :
                if p.attr['type'] == 'text' :
                    note_lines = p.get_value().split('\n')
                    lines = ''
                    for line in note_lines :
                        lines = lines + '( ' + line + ' )\n'
                    s = re.sub(r"%s([^A-Za-z0-9_]|$)" %
                        (re.escape(p.attr["call"])), r"%s\1" %
                        lines, s)
                elif p.attr['type'] == 'gc-lines' :
                    note_lines = p.get_value().split('\n')
                    lines = '\n'
                    for line in note_lines :
                        lines = lines + '\t' + line + '\n'
                    s = re.sub(r"%s([^A-Za-z0-9_]|$)" %
                        (re.escape(p.attr["call"])), r"%s\1" %
                        lines, s)

                else :
                    s = re.sub(r"%s([^A-Za-z0-9_]|$)" %
                       (re.escape(p.attr["call"])), r"%s\1" %
                       p.get_ngc_value(), s)
        return s

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
                print(msg)
                mess_dlg(msg)
                return ''

        def import_callback(m) :
            fname = m.group(2)
            fname = search_path(search_warning.dialog, fname, PROJECTS_DIR)
            if fname is not None :
                return str(open(fname).read())

        s = self.replace_params(s)

        s = re.sub(r"#sub_name", "%s" % self.attr['name'], s)
        s = re.sub(r"%SYS_DIR%", "%s" % SYS_DIR, s)
        f_id = self.get_attr("id")
        s = re.sub(r"#self_id", "%s" % f_id, s)

        s = re.sub(r"(?i)(<import>(.*?)</import>)", import_callback, s)
        s = re.sub(r"(?i)(<eval>(.*?)</eval>)", eval_callback, s)
        s = re.sub(r"(?ims)(<exec>(.*?)</exec>)", exec_callback, s)
        s = re.sub(r"(?ims)(<subprocess>(.*?)</subprocess>)",
                   subprocess_callback, s)

        if "#ID" in s :
            if 'short_id' not in self.attr :
                self.attr['short_id'] = get_short_id()
            s = re.sub(r"#ID", "%s" % self.attr['short_id'], s)

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
        count = get_int(self.attr['indent']) if 'indent' in self.attr else 0
        return('\t' * count)

    def hide_field(self):
        if 'hidden_count' not in self.attr :
            self.attr['hidden_count'] = '1'
        else :
            self.attr['hidden_count'] = str(get_int(self.attr['hidden_count']) + 1)

    def show_all_fields(self):
        result = 0
        for p in self.param :
            result += p.set_hidden(False)
        self.attr['hidden_count'] = '0'
        return result > 0

    def has_hidden_fields(self):
        if 'hidden_count' in self.attr :
            return get_int(self.attr['hidden_count']) > 0
        else :
            return False

    def msg_inv(self, msg, msgid):
        msg = msg.replace('&#176;', '°')
        print('\n%(feature_name)s : %(msg)s' % {'feature_name':self.get_name(), 'msg':msg})

        if (("ALL:msgid-0" in EXCL_MESSAGES) or
                ("%s:msgid-0" % (self.get_type()) in EXCL_MESSAGES) or
                (("%s:msgid-%d" % (self.get_type(), msgid)) in EXCL_MESSAGES)) :
            return

        # create dialog with image and checkbox
        dlg = gtk.MessageDialog(parent = None,
            flags = gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
            type = gtk.MESSAGE_WARNING,
            buttons = gtk.BUTTONS_NONE,
            message_format = self.get_name())
        dlg.set_title('NativeCAM')
        dlg.format_secondary_text(msg)
        img = gtk.Image()
        img.set_from_pixbuf(self.get_icon(add_dlg_icon_size))
        dlg.set_image(img)
        cb = gtk.CheckButton(label = _("Do not show again"))
        dlg.get_content_area().pack_start(cb, True, True, 0)
        dlg.add_button(gtk.STOCK_OK, gtk.RESPONSE_OK).grab_focus()

        dlg.set_keep_above(True)
        dlg.show_all()
        dlg.run()
        if cb.get_active() :
            GLOBAL_PREF.add_excluded_msg(self.get_type(), msgid)
        dlg.destroy()

    def check_hash(self, s, default = 0):
        try :
            return (0 + eval(s.strip('[]')))
        except :
            print(_('%(feature_name)s : can not evaluate %(value)s') % \
                  {'feature_name':self.get_name(), 'value':s})
            return default

    def validate(self):
        VALIDATED = True
        s = self.attr["validation"]
        s = self.replace_params(s)
        s = re.sub(r"#", r"""#""", s)
        exec(s)
        if not VALIDATED :
            print('%s failed validation\n' % self.get_name())
        return True

class Preferences(object):

    def __init__(self):
        global default_metric, machine_metric
        default_metric = None
        self.pref_file = None
        self.cfg_file = None
        self.ngc_init_str = None
#        self.cat_name = None
        self.has_Z_axis = True

    def read(self, cat_name, read_all = True):
        global default_digits, default_metric, add_menu_icon_size, \
            add_dlg_icon_size, quick_access_icon_size, menu_icon_size, \
            treeview_icon_size, vkb_width, vkb_height, vkb_cancel_on_out, \
            toolbar_icon_size, gmoccapy_time_out, NCAM_DIR, no_ini

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

        def read_sbool(cf, section, key, default):
            if read_boolean(cf, section, key, default):
                return '1'
            else :
                return '0'

        def read_str(cf, section, key, default):
            try :
                val = cf.get(section, key).strip()
                if val is None :
                    return default
                else :
                    return val
            except :
                return default

        def read_int(cf, section, key, default):
            return int(round(read_float(cf, section, key, default), 0))

        if cat_name is not None :
            self.cat_name = cat_name

        config = ConfigParser.ConfigParser()

        if read_all :
            self.cfg_file = os.path.join(NCAM_DIR, CATALOGS_DIR, CONFIG_FILE)
            self.pref_file = os.path.join(NCAM_DIR, CATALOGS_DIR, self.cat_name, PREFERENCES_FILE)

            config.read(self.cfg_file)

            self.w_adj_value = read_int(config, 'display', 'width', 550)
            self.col_width_adj_value = read_int(config, 'display', 'name_col_width', 160)
            self.tv_w_adj_value = read_int(config, 'display', 'master_tv_width', 175)
            self.restore_expand_state = read_boolean(config, 'display', 'restore_expand_state', True)
            self.tv2_expandable = read_boolean(config, 'display', 'tv2_expandable', False)
            self.tv_expandable = read_boolean(config, 'display', 'tv_expandable', False)
            self.use_dual_views = read_boolean(config, 'layout', 'dual_view', True)
            self.side_by_side = read_boolean(config, 'layout', 'side_by_side', True)
            self.sub_hdrs_in_tv1 = read_boolean(config, 'layout', 'subheaders_in_master', False)
            self.hide_value_column = read_boolean(config, 'layout', 'hide_value_column', False)
            treeview_icon_size = read_int(config, 'icons_size', 'treeview', 28)
            add_menu_icon_size = read_int(config, 'icons_size', 'add_menu', 24)
            menu_icon_size = read_int(config, 'icons_size', 'menu', 4)
            toolbar_icon_size = read_int(config, 'icons_size', 'toolbar', 5)
            add_dlg_icon_size = read_int(config, 'icons_size', 'add_dlg', 70)
            quick_access_icon_size = read_int(config, 'icons_size', 'ncam_toolbar', 30)
            vkb_width = read_int(config, 'virtual_kb', 'minimum_width', 260)
            vkb_height = read_int(config, 'virtual_kb', 'height', 260)
            vkb_cancel_on_out = read_boolean(config, 'virtual_kb', 'cancel_on_focus_out', True)
            self.name_ellipsis = read_int(config, 'display', 'name-ellipsis', 2)

        config.read(self.pref_file)

        if self.ngc_init_str is None :
            if self.cat_name in ['mill', 'plasma'] :
                self.ngc_init_str = 'G17 G40 G49 G90 G92.1 G94 G54 G64 p0.001'
            elif self.cat_name == 'lathe' :
                self.ngc_init_str = 'G18 G40 G49 G90 G92.1 G94 G54 G64 p0.001'

        self.timeout_value = read_int(config, 'general', 'time_out', 0.300) * 1000
        self.autosave = read_boolean(config, 'general', 'autosave', False)
        default_digits = read_str(config, 'general', 'digits', '3')

        if no_ini :
            default_metric = read_int(config, 'general', 'default_metric', 1) == 1

        self.ngc_show_final_cut = read_sbool(config, 'general', 'show_final_cut', True)
        self.ngc_show_bottom_cut = read_sbool(config, 'general', 'show_bottom_cut', True)
        self.ngc_init_str = read_str(config, 'ngc', 'init_str', self.ngc_init_str)
        self.ngc_post_amble = read_str(config, 'ngc', 'post_amble', " ")
        self.use_pct = read_boolean(config, 'ngc', 'use_pct_signs', False)
        self.ngc_spindle_speedup_time = read_str(config, 'ngc', 'spindle_acc_time', '0.0')
        self.spindle_all_time = read_sbool(config, 'ngc', 'spindle_all_time', True)

        self.ngc_off_rot_coord_system = read_int(config, 'ngc', 'off_rot_coord_system', 2)
        gmoccapy_time_out = read_float(config, 'general', 'gmoccapy_time_out', 0.15)

        self.ngc_probe_func = read_str(config, 'probe', 'probe_func', "4")
        self.probe_tool_len_comp = read_sbool(config, 'probe', 'probe_tool_len_comp', True)
        if default_metric :
            self.ngc_probe_feed = read_str(config, 'probe_mm', 'probe_feed', '200')
            self.ngc_probe_latch = read_str(config, 'probe_mm', 'probe_latch', '-1')
            self.ngc_probe_latch_feed = read_str(config, 'probe_mm', 'probe_latch_feed', '50')
            self.ngc_probe_tip_dia = read_str(config, 'probe_mm', 'probe_tip_dia', '3.0')
            self.ngc_probe_safe = read_str(config, 'probe_mm', 'probe_safe', '5.0')
            self.ngc_probe_height = read_str(config, 'probe_mm', 'probe_height', '0')

            self.drill_center_depth = read_str(config, 'drill_mm', 'center_drill_depth', '-3.0')
        else :
            self.ngc_probe_feed = read_str(config, 'probe', 'probe_feed', '8.0')
            self.ngc_probe_latch = read_str(config, 'probe', 'probe_latch', '-0.05')
            self.ngc_probe_latch_feed = read_str(config, 'probe', 'probe_latch_feed', '2')
            self.ngc_probe_tip_dia = read_str(config, 'probe', 'probe_tip_dia', '0.125')
            self.ngc_probe_safe = read_str(config, 'probe', 'probe_safe', '0.2')
            self.ngc_probe_height = read_str(config, 'probe', 'probe_height', '0')

            self.drill_center_depth = read_str(config, 'drill', 'center_drill_depth', '-0.125')

        self.pocket_mode = read_str(config, 'pocket', 'mode', '0')

        self.opt_eng1 = read_str(config, 'optimizing', 'engagement1', '0.20')
        self.opt_eng2 = read_str(config, 'optimizing', 'engagement2', '0.30')
        self.opt_eng3 = read_str(config, 'optimizing', 'engagement3', '0.80')

        self.opt_ff1 = read_str(config, 'optimizing', 'feedfactor1', '1.60')
        self.opt_ff2 = read_str(config, 'optimizing', 'feedfactor2', '1.40')
        self.opt_ff3 = read_str(config, 'optimizing', 'feedfactor3', '1.25')
        self.opt_ff4 = read_str(config, 'optimizing', 'feedfactor4', '1.00')
        self.opt_ff0 = read_str(config, 'optimizing', 'feedfactor0', '1.00')

        self.opt_sf1 = read_str(config, 'optimizing', 'speedfactor1', '1.25')
        self.opt_sf2 = read_str(config, 'optimizing', 'speedfactor2', '1.25')
        self.opt_sf3 = read_str(config, 'optimizing', 'speedfactor3', '1.25')
        self.opt_sf4 = read_str(config, 'optimizing', 'speedfactor4', '1.00')
        self.opt_sf0 = read_str(config, 'optimizing', 'speedfactor0', '1.00')

        self.plasma_test_mode = read_sbool(config, 'plasma', 'test_mode', True)

        self.read_user_values()
        self.read_excluded_msgs()
        self.create_defaults()

    def read_user_values(self):
        global USER_VALUES, USER_SUBROUTINES

        USER_VALUES = {}
        fname = os.path.join(NCAM_DIR, CATALOGS_DIR, self.cat_name, USER_DEFAULT_FILE)
        config = ConfigParser.ConfigParser()
        config.read(fname)
        USER_SUBROUTINES = config.sections()
        for section in config.sections() :
            for key, val in config.items(section) :
                USER_VALUES[section + ':' + key] = val

    def read_excluded_msgs(self):
        global EXCL_MESSAGES

        EXCL_MESSAGES = {}
        fname = os.path.join(NCAM_DIR, CATALOGS_DIR, self.cat_name, EXCL_MSG_FILE)
        config = ConfigParser.ConfigParser()
        config.read(fname)
        for section in config.sections() :
            for key, val in config.items(section) :
                EXCL_MESSAGES[section + ':' + key] = val

    def add_excluded_msg(self, ftype, msgid):
        fname = os.path.join(NCAM_DIR, CATALOGS_DIR, self.cat_name, EXCL_MSG_FILE)
        parser = ConfigParser.ConfigParser()
        parser.read(fname)

        if not parser.has_section(ftype) :
            parser.add_section(ftype)
        parser.set(ftype, 'msgid-%d' % msgid, 'exclude')

        with open(fname, 'wb') as configfile:
            parser.write(configfile)

        self.read_excluded_msgs()

    def val_show_all(self, ftype = None):
        global EXCL_MESSAGES
        fname = os.path.join(NCAM_DIR, CATALOGS_DIR, self.cat_name, EXCL_MSG_FILE)

        if ftype is None :
            EXCL_MESSAGES = {}
            if os.path.isfile(fname) :
                os.remove(fname)
        else :
            parser = ConfigParser.ConfigParser()
            parser.read(fname)

            if parser.has_section(ftype) :
                parser.remove_section(ftype)

                with open(fname, 'wb') as configfile:
                    parser.write(configfile)

                self.read_excluded_msgs()

    def val_show_none(self, ftype = None) :
        fname = os.path.join(NCAM_DIR, CATALOGS_DIR, self.cat_name, EXCL_MSG_FILE)
        parser = ConfigParser.ConfigParser()

        if ftype is None :
            parser.add_section('ALL')
            parser.set('ALL', 'msgid-0', 'exclude')

        else :
            parser.read(fname)
            if not parser.has_section(ftype) :
                parser.add_section(ftype)
            parser.set(ftype, 'msgid-0', 'exclude')

        with open(fname, 'wb') as configfile:
            parser.write(configfile)

        self.read_excluded_msgs()

    def edit(self, nc):
        if pref_edit.edit_preferences(nc, default_metric, self.cat_name, NCAM_DIR, \
                self.ngc_init_str, self.ngc_post_amble, SYS_DIR) :
            self.read(None)
            return True
        return False

    def create_defaults(self):
        global machine_metric

        if self.use_pct :
            self.default = '%\n'
        else :
            self.default = ''
        self.default += _('(*** GCode generated by NativeCAM for LinuxCNC ***)\n\n')
        self.default += _('(*.ngc files are best viewed with Syntax Highlighting)\n')
        self.default += '(visit https://forum.linuxcnc.org/forum/20-g-code/'
        self.default +=     '30840-new-syntax-highlighting-for-gedit)\n'
        self.default += '(or https://github.com/FernV/Gcode-highlight-for-Kate)\n\n'

        if machine_metric :
            self.default += _("G21  (metric)\n")
        else :
            self.default += _("G20  (imperial/inches)\n")
        self.default += (self.ngc_init_str + "\n\n")

        if self.cat_name == 'mill' :
            self.default += ("\n#<center_drill_depth>       = " + self.drill_center_depth + "\n\n")
            self.default += ("#<_pocket_expand_mode>      = " + self.pocket_mode + "\n\n")

            self.default += _("(optimization values)\n")
            self.default += ("#<_tool_eng1>               = " + self.opt_eng1 + "\n")
            self.default += ("#<_tool_eng2>               = " + self.opt_eng2 + "\n")
            self.default += ("#<_tool_eng3>               = " + self.opt_eng3 + "\n\n")

            self.default += ("#<_feedfactor1>             = " + self.opt_ff1 + "\n")
            self.default += ("#<_feedfactor2>             = " + self.opt_ff2 + "\n")
            self.default += ("#<_feedfactor3>             = " + self.opt_ff3 + "\n")
            self.default += ("#<_feedfactor4>             = " + self.opt_ff4 + "\n")
            self.default += ("#<_feedfactor0>             = " + self.opt_ff0 + "\n\n")

            self.default += ("#<_speedfactor1>            = " + self.opt_sf1 + "\n")
            self.default += ("#<_speedfactor2>            = " + self.opt_sf2 + "\n")
            self.default += ("#<_speedfactor3>            = " + self.opt_sf3 + "\n")
            self.default += ("#<_speedfactor4>            = " + self.opt_sf4 + "\n")
            self.default += ("#<_speedfactor0>            = " + self.opt_sf0 + "\n\n")

            self.default += ("#<_probe_func>              = 38." + self.ngc_probe_func + "\n")
            self.default += ("#<_probe_feed>              = " + self.ngc_probe_feed + "\n")
            self.default += ("#<_probe_latch>             = " + self.ngc_probe_latch + "\n")
            self.default += ("#<_probe_latch_feed>        = " + self.ngc_probe_latch_feed + "\n")
            self.default += ("#<_probe_safe>              = " + self.ngc_probe_safe + "\n")
            self.default += ("#<_probe_tip_dia>           = " + self.ngc_probe_tip_dia + "\n\n")
            self.default += ("#<_probe_tool_len_comp>     = " + self.probe_tool_len_comp + "\n")
            self.default += ("#<probe_height>             = " + self.ngc_probe_height + "\n")
            self.default += ("#<_tool_probe_z>            = 0.0\n")

        if self.cat_name in ['mill', 'plasma'] :
            if self.ngc_off_rot_coord_system < 5 :
                coord = str(5 + self.ngc_off_rot_coord_system)
            else :
                coord = '9.' + str(self.ngc_off_rot_coord_system - 4)
            self.default += ("\n#<_off_rot_coord_system>    = 5" + coord + "\n\n")

            self.default += ("#<_mill_data_start>         = 70\n")
            self.default += ("#<in_polyline>              = 0\n\n")

            if self.has_Z_axis :
                self.default += ("#<_has_z_axis>              = 1\n\n")
            else :
                self.default += ("#<_has_z_axis>              = 0\n\n")

            self.default += ("#<_show_final_cuts>         = " + self.ngc_show_final_cut + "\n")
            self.default += ("#<_show_bottom_cut>         = " + self.ngc_show_bottom_cut + "\n\n")

            self.default += ("#<_spindle_all_time>        = " + self.spindle_all_time + "\n\n")

        if self.cat_name in ['mill', 'lathe'] :
            self.default += ("#<_spindle_speed_up_delay>  = " + self.ngc_spindle_speedup_time + "\n\n")

        if self.cat_name == 'plasma' :
            self.default += ("#<_plasma_test_mode>        = " + self.plasma_test_mode + "\n\n")

        self.default += _("(end defaults)\n\n")

        self.default += ("#<_units_radius>            = 1  (for backward compatibility)\n")
        self.default += ("#<_units_width>             = 1  (for backward compatibility)\n")
        if self.cat_name in ['mill', 'lathe'] :
            self.default += ("#<_units_cut_depth>         = 1  (for backward compatibility)\n\n")
        self.default += ("#<_tool_dynamic_dia>        = 0.0\n\n")

        self.default += _('(This is a built-in safety to help avoid gouging into your work piece)\n')
        if self.cat_name in ['mill', 'plasma'] :
            self.default += ("/ o<safety_999> if [#<_show_final_cuts>]\n")
            self.default += ("/    o<safety_9999> repeat [1000]\n")
            self.default += ("/       M123\n")
            self.default += ("/       M0\n")
            self.default += ("/    o<safety_9999> endrepeat\n")
            self.default += ("/ o<safety_999> endif\n\n")
        else :
            self.default += ("/  o<safety_9999> repeat [1000]\n")
            self.default += ("/    M123\n")
            self.default += ("/    M0\n")
            self.default += ("/  o<safety_9999> endrepeat\n\n")

        self.default += _('\n(sub definitions)\n')

class NCam(gtk.VBox):
    __gtype_name__ = "NCam"
    __gproperties__ = {}
    __gproperties = __gproperties__

    def __init__(self, *a, **kw):
        global NCAM_DIR, default_metric, NGC_DIR, SYS_DIR, no_ini, TOOL_TABLE, \
            GLOBAL_PREF, machine_metric

        arg_start = (sys.argv[0:].index('-U') + 1) if "-U" in sys.argv[0:] else 1
        opt, optl = 'U:x:c:i:t', ["catalog=", "ini="]
        try :
            optlist, arg = getopt.getopt(sys.argv[arg_start:], opt, optl)
            optlist = dict(optlist)
        except getopt.GetoptError as err:
            err_exit(err)

        # initialize class variables
        self.add_iconview = None
        self.can_add_to_group = False
        self.can_delete_duplicate = False
        self.can_move_down = False
        self.can_move_up = False
        self.can_remove_from_group = False
        self.catalog_dir = DEFAULT_CATALOG
        self.catalog_path = None
        self.catalog_src = None
        self.click_x = 0
        self.click_y = 0
        self.details_filter = None
        self.editor = DEFAULT_EDITOR
        self.file_changed = False
        self.focused_widget = None
        self.icon_store = None
        self.items_lpath = None
        self.items_path = None
        self.items_ts_parent_s = None
        self.iter_next = None
        self.iter_previous = None
        self.iter_selected_type = tv_select.none
        self.show_not_connected = False
        self.menubar = None
        self.name_cell = None
        self.name_cell2 = None
        self.nc_toolbar = None
        self.newnamedlg = None
        self.params_scroll = None
        self.path_to_new_selected = None
        self.path_to_old_selected = None
        self.selected_feature = None
        self.selected_feature_itr = None
        self.selected_feature_parent_itr = None
        self.selected_feature_path = None
        self.selected_feature_ts_itr = None
        self.selected_param = None
        self.selected_type = 'xxx'
        self.selection = None
        self.timeout = None
        self.treestore_selected = None
        self.treeview = None
        self.treeview2 = None
        self.tv1_icon_cell = None
        self.tv2_icon_cell = None
        self.undo_list = []
        self.undo_pointer = -1

        self.pref = Preferences()
        TOOL_TABLE = Tools()

        machine_metric = True

        if "-c" in optlist :
            self.catalog_dir = optlist["-c"]
        elif "--catalog" in optlist :
            self.catalog_dir = optlist["--catalog"]

        ini = os.getenv("INI_FILE_NAME")
        if "-i" in optlist :
            ini = optlist["-i"]
        elif "--ini" in optlist :
            ini = optlist["--ini"]

        no_ini = ini is None

        if no_ini :
            # standalone with no --ini:
            inifilename = 'NA'
            # beware, files expected/created in this dir
            NCAM_DIR = os.path.expanduser('~/nativecam')
            NGC_DIR = NCAM_DIR + '/' + NGC_DIR
        else :
            try :
                inifilename = os.path.abspath(ini)
                ini_instance = linuxcnc.ini(ini)
            except Exception as detail :
                err_exit(_("Open fails for ini file : %(inifilename)s\n\n%(detail)s") % \
                           {'inifilename':inifilename, 'detail':detail})

            require_ini_items(inifilename, ini_instance)

            val = ini_instance.find('DISPLAY', 'DISPLAY')
            if val not in ['axis', 'gmoccapy', 'gscreen'] :
                mess_dlg(_("DISPLAY can only be 'axis', 'gmoccapy' or 'gscreen'"))
                sys.exit(-1)

            val = ini_instance.find('DISPLAY', 'GLADEVCP')
            if val is None :
                val = ini_instance.find('DISPLAY', 'EMBED_TAB_COMMAND')

            if val is not None :
                if 'mill' in val :
                    self.catalog_dir = 'mill'
                elif 'lathe' in val :
                    self.catalog_dir = 'lathe'
                elif 'plasma'in val :
                    self.catalog_dir = 'plasma'

            val = ini_instance.find('DISPLAY', 'LATHE')
            if (val is not None) and val.lower() in ['1', 'true'] :
                self.catalog_dir = 'lathe'

            self.pref.ngc_init_str = ini_instance.find('RS274NGC', 'RS274NGC_STARTUP_CODE')

            val = ini_instance.find('EMCIO', 'TOOL_TABLE')
            TOOL_TABLE.set_file(os.path.join(os.path.dirname(ini), val))

            machine_metric = ini_instance.find('TRAJ', 'LINEAR_UNITS') in ['mm', 'metric']
            default_metric = machine_metric

            val = ini_instance.find('DISPLAY', 'EDITOR')
            if val is not None :
                self.editor = val

            val = ini_instance.find('TRAJ', 'COORDINATES')
            if val is not None :
                self.pref.has_Z_axis = ('Z' in val)

        print("\nNativeCAM info:")
        print("   inifile = %s" % inifilename)
        print("  NCAM_DIR = %s" % NCAM_DIR)
        print("   SYS_DIR = %s" % SYS_DIR)
        print("   program = %s\n" % os.path.realpath(__file__))

        fromdirs = [CATALOGS_DIR, CUSTOM_DIR]

        if no_ini :
            self.ask_to_create_standalone(fromdirs)

        # first use:copy, subsequent: update
        if SYS_DIR != NCAM_DIR :
            self.update_user_tree(fromdirs, NCAM_DIR)

        if ini is not None :
            require_ncam_lib(inifilename, ini_instance)

        TOOL_TABLE.load_table()

        # find the catalog and menu file
        catname = self.catalog_dir + '/menu-custom.xml'
        cat_dir_name = search_path(search_warning.none, catname, CATALOGS_DIR)
        if cat_dir_name is not None :
            print(_('Using %s\n') % (catname))
        else :
            catname = self.catalog_dir + '/menu.xml'
            cat_dir_name = search_path(search_warning.dialog, catname, CATALOGS_DIR)
            print(_('Using default %(mnu)s,  no %(dir)s/menu-custom.xml found\n') %
                  {'mnu':catname, 'dir':self.catalog_dir})
        if cat_dir_name is None :
            sys.exit(1)

        mnu_xml = open(cat_dir_name).read()
        mnu_xml = re.sub(r"_\(", "", mnu_xml)
        mnu_xml = re.sub(r"\)_", "", mnu_xml)
        self.catalog = etree.fromstring(mnu_xml)

        self.pref.read(self.catalog_dir)
        GLOBAL_PREF = self.pref

        # main_window
        gtk.VBox.__init__(self, *a, **kw)
        self.builder = gtk.Builder()
        try :
            gf = io.open(os.path.join(SYS_DIR, "ncam.glade")).read()
        except IOError as reason :
            err_exit(reason)

        # testing translation file
        if translate_test :
            gf = translate(gf)
        else :
            self.builder.set_translation_domain('nativecam')

        self.builder.add_from_string(gf)

        self.get_widgets()
        self.main_box.reparent(self)

        self.on_scale_change_value(self)

        # create treestore and treeview
        self.treestore = gtk.TreeStore(object, str, bool, bool)
        self.master_filter = self.treestore.filter_new()

        self.details_filter = self.treestore.filter_new()
        self.details_filter.set_visible_column(3)

        self.create_treeview()

        # create actions, uimanager and add menu and toolbars
        self.action_group = gtk.ActionGroup("my_actions")
        self.create_actions()

        self.uimanager = gtk.UIManager()
        self.uimanager.insert_action_group(self.action_group)
        self.accelGroup = self.uimanager.get_accel_group()
        self.uimanager.add_ui_from_string(UI_INFO)

        self.get_actions_reference()
        self.create_menubar()

        self.main_toolbar = self.uimanager.get_widget("/ToolBar")
        self.main_toolbar.set_can_focus(False)
        self.main_box.pack_start(self.main_toolbar, False, False, 0)

        self.get_toolbar_actions()
        self.create_nc_toolbar()

        self.pop_up = self.uimanager.get_widget("/PopupMenu")
        self.pop_up2 = self.uimanager.get_widget("/PopupMenu2")

        self.create_add_dialog()

        self.builder.connect_signals(self)
        self.set_preferences()

        if not os.path.isfile(os.path.join(NCAM_DIR, NGC_DIR, 'M123')):
            create_M_file()

        self.load_currentWork()
        self.get_selected_feature(self.treeview)
        self.show_all()
        self.actionCurrent.set_visible(not self.pref.autosave)
        self.addVBox.hide()
        self.set_layout(None)

        self.feature_pane.set_size_request(int(self.tv_w_adj.get_value()), 100)

        self.clipboard = gtk.clipboard_get(gdk.SELECTION_CLIPBOARD)
        self.edit_menu_activate()
        self.treeview.grab_focus()
        self.show_not_connected = True
        self.actionAutoRefresh.connect('toggled', self.autorefresh_call)


    def ask_to_create_standalone(self, fromdirs) :
        for d in fromdirs:
            if os.path.isdir(os.path.join(NCAM_DIR, d)) :
                return
        msg = _('Create Standalone Directory :\n\n%(dir)s\n\nContinue?') % {'dir':NCAM_DIR}
        if not mess_yesno(msg, title = _("NativeCAM CREATE")) :
            sys.exit(0)

    def update_user_tree(self, fromdirs, todir):

        if not os.path.isdir(NCAM_DIR) :
            os.makedirs(NCAM_DIR, 0o755)

        if not os.path.isdir(NGC_DIR) :
            os.makedirs(NGC_DIR, 0o755)

        srcdir = os.path.join(NCAM_DIR, CUSTOM_DIR)
        if not os.path.exists(srcdir) :
            os.mkdir(srcdir, 0o755)

        srcdir = os.path.join(NCAM_DIR, LIB_DIR)

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

            if update_ct > 0 :
                print(fmt2 % {'qty':update_ct, 'dir':NCAM_DIR.rstrip('/') + '/' + d.lstrip('/')})
        print('')

        for s in VALID_CATALOGS :
            # copy default files if not exist
            srcdir = os.path.join(SYS_DIR, DEFAULTS_DIR, s)
            if os.path.exists(srcdir) :
                for f in os.listdir(srcdir) :
                    dst = os.path.join(NCAM_DIR, CATALOGS_DIR, s, PROJECTS_DIR, f)
                    if not os.path.exists(dst) :
                        try :
                            shutil.copy(os.path.join(srcdir, f), dst)
                        except Exception as error :
                            mess_dlg(_("Error copying file : %(f)s\nCode : %(c)s") \
                                     % {'f':f, 'c':error})

            # create links to examples directories
            srcdir = os.path.join(SYS_DIR, EXAMPLES_DIR, s)
            if os.path.exists(srcdir) :
                dst = os.path.join(NCAM_DIR, CATALOGS_DIR, s, PROJECTS_DIR, EXAMPLES_DIR)
                if os.path.exists(dst) and not os.path.islink(dst) :
                    shutil.rmtree(dst)
                    if os.path.exists(dst) :
                        try :
                            os.remove(dst)
                        except OSError :
                            os.rmdir(dst)
                if not os.path.exists(dst) :
                    try :
                        os.symlink(srcdir, dst)
                    except Exception as err :
                        mess_dlg(_("Error creating link : %(s)s -> %(d)s\nCode : %(c)s") \
                                 % {'s':srcdir, 'd':dst, 'c':err})

        def move_files(dir_processed) :
            mov_src = os.path.join(NCAM_DIR, dir_processed)
            mov_dst = os.path.join(NCAM_DIR, CUSTOM_DIR, dir_processed)
            cmp_dir = os.path.join(SYS_DIR, dir_processed)

            if not os.path.isdir(mov_dst) :
                os.makedirs(mov_dst, 0o755)

            for p in os.listdir(mov_src) :
                frompath = os.path.join(mov_src, p)
                if os.path.isdir(frompath) :
                    move_files(os.path.join(dir_processed, p))
                else :
                    cmp_path = os.path.join(cmp_dir, p)
                    if not os.path.exists(cmp_path) :
                        shutil.copy(frompath, os.path.join(mov_dst, p))

        for s in [LIB_DIR, GRAPHICS_DIR, CFG_DIR] :
            srcdir = os.path.join(NCAM_DIR, s)
            if os.path.isdir(srcdir) :
                # move files that are not in SYS_DIR directories
                if not os.path.islink(srcdir) :
                    move_files(s)
                    shutil.rmtree(srcdir)

            tdir = os.path.join(SYS_DIR, s)
            if os.path.islink(srcdir) and (tdir != srcdir) :
                os.remove(srcdir)
            # replace dir with a link
            if not os.path.isdir(srcdir) :
                os.symlink(tdir, srcdir)

    def create_menubar(self):
        def create_mi(_action, imgfile = None):
            mi = _action.create_menu_item()
            if imgfile == None :
                mi.set_image(_action.create_icon(menu_icon_size))
            else :
                img = gtk.Image()
                img.set_from_pixbuf(get_pixbuf(imgfile, add_menu_icon_size))
                mi.set_image(img)
            return mi

        if self.menubar is not None :
            self.menubar.destroy()
        self.menubar = gtk.MenuBar()

#        a = create_mi(self.actionOpen)
#        a.destroy

        # Projects menu
        file_menu = gtk.Menu()
        file_menu.append(create_mi(self.actionNew))
        file_menu.append(create_mi(self.actionOpen))
        file_menu.append(create_mi(self.actionOpenExample))
        file_menu.append(gtk.SeparatorMenuItem())
        file_menu.append(create_mi(self.actionSave))
        file_menu.append(create_mi(self.actionCurrent))
        file_menu.append(create_mi(self.actionSaveTemplate))
        file_menu.append(gtk.SeparatorMenuItem())
        file_menu.append(create_mi(self.actionSaveNGC))

        f_menu = create_mi(self.actionProject)
        f_menu.set_submenu(file_menu)
        self.menubar.append(f_menu)

        # Edit menu
        ed_menu = gtk.Menu()
        ed_menu.append(create_mi(self.actionUndo))
        ed_menu.append(create_mi(self.actionRedo))
        ed_menu.append(gtk.SeparatorMenuItem())

        ed_menu.append(create_mi(self.actionCut))
        ed_menu.append(create_mi(self.actionCopy))
        ed_menu.append(create_mi(self.actionPaste))
        ed_menu.append(gtk.SeparatorMenuItem())

        ed_menu.append(create_mi(self.actionAdd))
        ed_menu.append(create_mi(self.actionDuplicate))
        ed_menu.append(create_mi(self.actionDelete))
        ed_menu.append(gtk.SeparatorMenuItem())

        ed_menu.append(create_mi(self.actionMoveUp))
        ed_menu.append(create_mi(self.actionMoveDown))
        ed_menu.append(gtk.SeparatorMenuItem())

        ed_menu.append(create_mi(self.actionAppendItm))
        ed_menu.append(create_mi(self.actionRemoveItm))

        self.sep1 = gtk.SeparatorMenuItem()
        ed_menu.append(self.sep1)
        self.adt_mi = create_mi(self.actionDataType)
        ed_menu.append(self.adt_mi)
        self.art_mi = create_mi(self.actionRevertType)
        ed_menu.append(self.art_mi)

        edit_menu = create_mi(self.actionEditMenu)
        edit_menu.set_submenu(ed_menu)
        self.menubar.append(edit_menu)

        # View menu
        v_menu = gtk.Menu()
        self.aren_mi = create_mi(self.actionRename)
        v_menu.append(self.aren_mi)
        self.agrp_mi = create_mi(self.actionChngGrp)
        v_menu.append(self.agrp_mi)
        self.sep3 = gtk.SeparatorMenuItem()
        v_menu.append(self.sep3)
        v_menu.append(self.actionHideField.create_menu_item())
        v_menu.append(self.actionShowF.create_menu_item())
        self.sep2 = gtk.SeparatorMenuItem()
        v_menu.append(self.sep2)

        digits_menu = gtk.Menu()
        digits_menu.append(create_mi(self.actionDigit1))
        digits_menu.append(create_mi(self.actionDigit2))
        digits_menu.append(create_mi(self.actionDigit3))
        digits_menu.append(create_mi(self.actionDigit4))
        digits_menu.append(create_mi(self.actionDigit5))
        digits_menu.append(create_mi(self.actionDigit6))
        self.d_menu = create_mi(self.actionSetDigits)
        self.d_menu.set_submenu(digits_menu)
        v_menu.append(self.d_menu)

        v_menu.append(gtk.SeparatorMenuItem())
        v_menu.append(self.actionSingleView.create_menu_item())
        v_menu.append(self.actionDualView.create_menu_item())
        v_menu.append(gtk.SeparatorMenuItem())
        v_menu.append(self.actionTopBottom.create_menu_item())
        v_menu.append(self.actionSideSide.create_menu_item())
        v_menu.append(gtk.SeparatorMenuItem())
        v_menu.append(self.actionHideCol.create_menu_item())
        v_menu.append(self.actionSubHdrs.create_menu_item())
        v_menu.append(gtk.SeparatorMenuItem())
        v_menu.append(create_mi(self.actionSaveLayout))

        view_menu = create_mi(self.actionViewMenu)
        view_menu.set_submenu(v_menu)
        self.menubar.append(view_menu)

        # Add menu
        menuAdd = gtk.Menu()
        self.add_catalog_items(menuAdd)
        menuAdd.append(gtk.SeparatorMenuItem())
        menuAdd.append(create_mi(self.actionLoadCfg))
        menuAdd.append(create_mi(self.actionImportXML))

        add_menu = create_mi(self.actionAddMenu)
        add_menu.set_submenu(menuAdd)
        self.menubar.append(add_menu)

        # Utilities menu
        menu_utils = gtk.Menu()

        menu_utils.append(self.actionChUnits.create_menu_item())
        menu_utils.append(self.actionAutoRefresh.create_menu_item())
        menu_utils.append(gtk.SeparatorMenuItem())
        menu_utils.append(create_mi(self.actionLoadTools))
        menu_utils.append(gtk.SeparatorMenuItem())
        menu_utils.append(create_mi(self.actionSaveUser))
        menu_utils.append(create_mi(self.actionDeleteUser))

        menu_utils.append(gtk.SeparatorMenuItem())

        menu_val = gtk.Menu()
        menu_val.append(create_mi(self.actionValAllDlg))
        menu_val.append(create_mi(self.actionValNoDlg))
        menu_val.append(gtk.SeparatorMenuItem())
        menu_val.append(create_mi(self.actionValFeatDlg))
        menu_val.append(create_mi(self.actionValFeatNone))

        u_menu = create_mi(self.actionValidationMenu)
        u_menu.set_submenu(menu_val)
        menu_utils.append(u_menu)

        menu_utils.append(gtk.SeparatorMenuItem())
        menu_utils.append(create_mi(self.actionPreferences))

        u_menu = create_mi(self.actionUtilMenu)
        u_menu.set_submenu(menu_utils)
        self.menubar.append(u_menu)

        # Help menu
        menu_help = gtk.Menu()
        menu_help.append(create_mi(self.actionYouTube, "youtube.png"))
#        menu_help.append(create_mi(self.actionYouTrans, "youtube.png"))
        menu_help.append(gtk.SeparatorMenuItem())
        menu_help.append(create_mi(self.actionCNCHome, "linuxcncicon.png",))
        menu_help.append(create_mi(self.actionForum, "linuxcncicon.png",))
        menu_help.append(gtk.SeparatorMenuItem())
        menu_help.append(create_mi(self.actionAbout))

        h_menu = create_mi(self.actionHelpMenu)
        h_menu.set_submenu(menu_help)
        self.menubar.append(h_menu)

        self.mnu_current_project = gtk.MenuItem(label = '')
        self.menubar.append(self.mnu_current_project)

        self.main_box.pack_start(self.menubar, False, False, 0)

    def action_build(self, *arg) :
        self.autorefresh_call()

    def action_cut(self, *arg):
        self.action_copy()
        self.action_delete()

    def action_copy(self, *arg):
        self.get_expand()
        xml = etree.Element(XML_TAG)
        self.treestore_to_xml_recursion(self.selected_feature_ts_itr, xml, False)
        self.clipboard.set_text(etree.tostring(xml), len = -1)
        self.actionPaste.set_sensitive(True)

    def action_paste(self, *arg):
        txt = self.clipboard.wait_for_text()
        if txt and (XML_TAG in txt) :
            self.import_xml(etree.fromstring(txt))

    def edit_menu_activate(self, *arg):
        txt = self.clipboard.wait_for_text()
        if txt:
            self.actionPaste.set_sensitive(XML_TAG in txt)
        else:
            self.actionPaste.set_sensitive(False)

        self.adt_mi.set_visible(self.selected_type in ['float', 'int'])
        self.art_mi.set_visible(self.selected_type == 'gcode')
        self.sep1.set_visible(self.selected_type in ['float', 'int', 'gcode'])

    def utilMenu_activate(self, *arg):
        if default_metric :
            self.actionChUnits.set_label(_("Change Units to Imperial"))
        else :
            self.actionChUnits.set_label(_("Change Units to Metric"))

    def validation_menu_activate(self, *arg):
        self.actionValFeatDlg.set_sensitive(self.selected_feature is not None)
        self.actionValFeatNone.set_sensitive(self.selected_feature is not None)
        if self.selected_feature is not None :
            self.actionValFeatDlg.set_label('%s %s' % (_('Show All For'), self.selected_feature.get_name()))
            self.actionValFeatNone.set_label('%s %s' % (_('Show None For'), self.selected_feature.get_name()))

    def view_menu_activate(self, *arg):
        self.agrp_mi.set_visible(self.selected_type in ["sub-header", "header"] and \
                                 self.actionDualView.get_active())
        self.aren_mi.set_visible(self.iter_selected_type == tv_select.feature)
        self.sep3.set_visible(self.agrp_mi.get_visible() or self.aren_mi.get_visible())

        self.d_menu.set_visible(self.selected_type == 'float')
        self.sep2.set_visible(self.selected_type == 'float')

    def action_lcncHome(self, *arg):
        webbrowser.open('http://www.linuxcnc.org')

    def action_lcncForum(self, *arg):
        webbrowser.open('http://www.linuxcnc.org/index.php/english/forum/40-subroutines-and-ngcgui')

    def action_preferences(self, *arg):
        old_quick_access_icon_size = quick_access_icon_size
        old_treeview_icon_size = treeview_icon_size
        old_menu_icon_size = menu_icon_size
        old_add_menu_icon_size = add_menu_icon_size
        old_add_dlg_icon_size = add_dlg_icon_size
        old_view = self.actionDualView.get_active()

        if self.pref.edit(self) :
            if old_quick_access_icon_size != quick_access_icon_size :
                self.create_nc_toolbar()
                self.nc_toolbar.show_all()
            if old_treeview_icon_size != treeview_icon_size :
                self.tv1_icon_cell.set_fixed_size(treeview_icon_size, treeview_icon_size)
                if self.treeview2 is not None:
                    self.tv2_icon_cell.set_fixed_size(treeview_icon_size, treeview_icon_size)
            if (old_menu_icon_size != menu_icon_size) or (old_add_menu_icon_size != add_menu_icon_size) :
                self.create_menubar()
                self.menubar.show_all()
            if old_add_dlg_icon_size != add_dlg_icon_size :
                self.update_catalog()

            self.set_preferences()
            self.autorefresh_call()

    def action_ValAllDlg(self, *arg):
        self.pref.val_show_all()
        self.to_gcode()

    def action_ValNoDlg(self, *arg):
        self.pref.val_show_none()

    def action_ValFeatDlg(self, *arg):
        self.pref.val_show_all(self.selected_feature.get_type())

    def action_ValFeatNone(self, *arg):
        self.pref.val_show_none(self.selected_feature.get_type())

    def action_youTube(self, *arg):
        webbrowser.open('https://www.youtube.com/channel/UCjOe4VxKL86HyVrshTmiUBQ')

    def action_youTrans(self, *arg):
#        webbrowser.open('https://www.youtube.com/channel/UCjOe4VxKL86HyVrshTmiUBQ')
        pass

    def on_destroy(self, *arg):
        if self.pref.autosave :
            self.action_saveCurrent()

    def btn_cancel_add(self, *arg):
        self.addVBox.hide()
        self.feature_Hpane.show()
        self.menubar.set_sensitive(True)
        self.main_toolbar.set_sensitive(True)
        self.nc_toolbar.set_sensitive(True)

    def create_add_dialog(self):
        self.icon_store = gtk.ListStore(gdk.Pixbuf, str, str, str, int, str)
        self.add_iconview.set_model(self.icon_store)
        self.add_iconview.set_pixbuf_column(0)
        self.add_iconview.set_text_column(2)

        if self.catalog.tag == 'xml' :
            self.catalog_src = self.catalog
        else :
            for _ptr in range(len(self.catalog)) :
                _p = self.catalog[_ptr]
                if _p.tag.lower() in ["menu", "group"] :
                    self.catalog_src = _p
                    break
        self.update_catalog()

    def catalog_activate(self, iconview):
        lst = iconview.get_selected_items()
        if lst is not None and (len(lst) > 0) :
            itr = self.icon_store.get_iter(lst[0])
            src = self.icon_store.get(itr, 3)[0]
            tag = self.icon_store.get(itr, 1)[0]
            if tag == "parent" :
                self.update_catalog(xml = "parent")
            elif tag in ["menuitem", "sub"] :
                self.addVBox.hide()
                self.feature_Hpane.show()
                self.menubar.set_sensitive(True)
                self.main_toolbar.set_sensitive(True)
                self.nc_toolbar.set_sensitive(True)
                self.add_feature(None, src)
            elif tag in ["menu", "group"] :
                path = self.icon_store.get(itr, 4)[0]
                self.update_catalog(xml = self.catalog_path[path])

    def update_catalog(self, xml = None) :
        if xml is not None and xml == "parent" :
            self.catalog_path = self.catalog_path.getparent()
        else :
            self.catalog_path = xml

        if self.catalog_path is None :
            self.catalog_path = self.catalog_src

        self.icon_store.clear()

        # add link to upper level
        if (self.catalog_path != self.catalog_src) :
            self.icon_store.append([get_pixbuf("upper-level.png",
                add_dlg_icon_size), "parent", _('Back...'), "parent", 0, None])

        for path in range(len(self.catalog_path)) :
            p = self.catalog_path[path]
            if p.tag.lower() in ["menuitem", "menu", "group", "sub"] :
                name = p.get('name') if "name" in list(p.keys()) else 'Un-named'
                src = p.get("src") if "src" in list(p.keys()) else None
                tooltip = _(p.get('tool_tip')) if "tool_tip" in \
                        list(p.keys()) else None
                self.icon_store.append([get_pixbuf(p.get("icon"), add_dlg_icon_size),
                        p.tag.lower(), _(name), src, path, tooltip])

    def action_add(self, *arg) :
        self.feature_Hpane.hide()
        self.addVBox.show()
        self.menubar.set_sensitive(False)
        self.main_toolbar.set_sensitive(False)
        self.nc_toolbar.set_sensitive(False)
        self.add_iconview.grab_focus()

    def add_catalog_items(self, menu_add):

        def add_to_menu(grp_menu, path) :
            for ptr in range(len(path)) :
                try :
                    p = path[ptr]
                    if p.tag.lower() in ["menu", "menuitem", "group", "sub"] :
                        name = p.get("name") if "name" in p.keys() else ""
                        a_menu_item = gtk.ImageMenuItem(_(name))

                        tooltip = _(p.get("tool_tip")) if "tool_tip" in p.keys() else None
                        if (tooltip is not None) and (tooltip != '') :
                            a_menu_item.set_tooltip_markup(_(tooltip))

                        icon = p.get('icon')
                        if icon is not None :
                            img = gtk.Image()
                            img.set_from_pixbuf(get_pixbuf(icon, add_menu_icon_size))
                            a_menu_item.set_image(img)

                        src = p.get('src')
                        if src is not None :
                            a_menu_item.connect("activate", self.add_feature, src)

                        grp_menu.append(a_menu_item)

                        if p.tag.lower() in ['menu', "group"] :
                            a_menu = gtk.Menu()
                            a_menu_item.set_submenu(a_menu)
                            add_to_menu(a_menu, p)

                    elif p.tag.lower() == "separator":
                        grp_menu.append(gtk.SeparatorMenuItem())
                except:
                    pass

        if self.catalog.tag != 'ncam_ui' :
            mess_dlg(_('Menu is old format, no toolbar defined.\nUpdate to new format'))
            add_to_menu(menu_add, self.catalog)
        else :
            for _ptr in range(len(self.catalog)) :
                _p = self.catalog[_ptr]
                if _p.tag.lower() in ["menu", "group"] :
                    add_to_menu(menu_add, _p)

    def create_treeview(self):
        self.treeview = gtk.TreeView(self.treestore)
        self.treeview.set_grid_lines(gtk.TREE_VIEW_GRID_LINES_VERTICAL)
        self.builder.get_object("feat_scrolledwindow").add(self.treeview)

        self.treeview.add_events(gdk.BUTTON_PRESS_MASK)
        self.treeview.connect('button-press-event', self.pop_menu)
        self.treeview.connect('row_activated', self.tv_row_activated)
        self.treeview.connect('key_press_event', self.tv_key_pressed_event)

        # icon and name
        col = gtk.TreeViewColumn(_("Name"))
        cell = gtk.CellRendererPixbuf()
        cell.set_fixed_size(treeview_icon_size, treeview_icon_size)
        self.tv1_icon_cell = cell
        col.pack_start(cell, expand = False)
        col.set_cell_data_func(cell, self.get_col_icon)
        col.set_min_width(int(self.col_width_adj.get_value()))

        self.name_cell = gtk.CellRendererText()
        col.pack_start(self.name_cell, expand = True)
        col.set_cell_data_func(self.name_cell, self.get_col_name)
        col.set_resizable(True)
        self.name_cell.set_property('ellipsize', self.pref.name_ellipsis)
        self.name_cell.set_property('xpad', 2)

        self.treeview.append_column(col)

        # value
        col = gtk.TreeViewColumn(_("Value"))

        cell = CellRendererMx(self.treeview)
        cell.edited = self.edited
        cell.set_preediting(self.get_editinfo)
        cell.set_refresh_fn(self.get_selected_feature)
        col.pack_start(cell, expand = True)
        col.set_cell_data_func(cell, self.get_col_value)
        col.set_min_width(200)
        col.set_resizable(True)
        self.treeview.append_column(col)

        self.treeview.set_tooltip_column(1)
        self.treeview.connect("cursor-changed", self.get_selected_feature)

        self.treeview.set_model(self.master_filter)

    def action_saveUser(self, *arg) :
        fname = os.path.join(NCAM_DIR, CATALOGS_DIR, self.catalog_dir, USER_DEFAULT_FILE)
        parser = ConfigParser.ConfigParser()
        parser.read(fname)

        section = self.selected_feature.get_type()
        if parser.has_section(section) :
            parser.remove_section(section)
        parser.add_section(section)

        for p in self.selected_feature.param :
            t = p.get_type()
            s = p.attr['call'].lstrip('#')
            parser.set(section, s + '--type', t)
            if 'value' in p.attr :
                parser.set(section, s + '--value', p.attr['value'])
            if p.get_hidden() :
                parser.set(section, s + '--hidden', '2')
            if 'grayed' in p.attr :
                parser.set(section, s + '--grayed', p.attr['grayed'])

        with open(fname, 'wb') as configfile:
            parser.write(configfile)

        self.pref.read_user_values()
        self.actionDeleteUser.set_sensitive(self.selected_feature.get_type() in USER_SUBROUTINES)

    def action_deleteUser(self, *arg):
        fname = os.path.join(NCAM_DIR, CATALOGS_DIR, self.catalog_dir,
                             USER_DEFAULT_FILE)
        parser = ConfigParser.ConfigParser()
        parser.read(fname)

        section = self.selected_feature.get_type()
        if parser.has_section(section) :
            parser.remove_section(section)
            with open(fname, 'wb') as configfile:
                parser.write(configfile)
            self.pref.read_user_values()
            self.actionDeleteUser.set_sensitive(self.selected_feature.get_type() in USER_SUBROUTINES)

    def action_saveCurrent(self, *arg):
        fname = os.path.join(NCAM_DIR, CATALOGS_DIR, self.catalog_dir, PROJECTS_DIR, CURRENT_WORK)
        if self.treestore.get_iter_root() is not None :
            xml = self.treestore_to_xml()
            etree.ElementTree(xml).write(fname, pretty_print = True)
        else :
            if os.path.isfile(fname) :
                os.remove(fname)

    def pop_menu(self, tv, event):
        if event.button == 3:
            self.click_x = int(event.x)
            self.click_y = int(event.y)
            path = tv.get_path_at_pos(self.click_x, self.click_y - 1)
            if path is not None:
                path = path[0]
                tv.set_cursor(path)
            else :
                selection = tv.get_selection()
                if selection is not None :
                    model, itr = selection.get_selected()
                    itr = model.get_iter_first()
                    if itr is not None :
                        tv.set_cursor(model.get_path(itr))

            self.edit_menu_activate()
            if tv == self.treeview :
                self.pop_up.popup(None, None, None, event.button, event.time, None)
            else :
                self.pop_up2.popup(None, None, None, event.button, event.time, None)
            return True

    def action_digits(self, *arg) :
        self.treestore.get(self.selected_param, 0)[0].set_digits(arg[1][0])
        self.refresh_views()

    def create_second_treeview(self):
        self.treeview2 = gtk.TreeView()
        self.treeview2.add_events(gdk.BUTTON_PRESS_MASK)
        self.treeview2.connect('button-press-event', self.pop_menu)
        self.treeview2.connect('cursor-changed', self.tv2_selected)
        self.treeview2.connect('row_activated', self.tv_row_activated)
        self.treeview2.set_grid_lines(gtk.TREE_VIEW_GRID_LINES_VERTICAL)
        self.treeview2.set_show_expanders(self.pref.tv2_expandable)
        if self.pref.tv2_expandable :
            self.treeview2.set_level_indentation(-5)
        else :
            self.treeview2.set_level_indentation(12)

        # icon and name
        col = gtk.TreeViewColumn(_("Name"))
        cell = gtk.CellRendererPixbuf()
        cell.set_fixed_size(treeview_icon_size, treeview_icon_size)
        self.tv2_icon_cell = cell

        col.pack_start(cell, expand = False)
        col.set_cell_data_func(cell, self.get_col_icon)

        self.name_cell2 = gtk.CellRendererText()
        self.name_cell2.set_property('xpad', 2)
        self.name_cell2.set_property('ellipsize', self.pref.name_ellipsis)
        col.pack_start(self.name_cell2, expand = True)
        col.set_cell_data_func(self.name_cell2, self.get_col_name)
        col.set_resizable(True)
        col.set_min_width(int(self.col_width_adj.get_value()))
        self.treeview2.append_column(col)

        # value
        col = gtk.TreeViewColumn(_("Value"))
        cell = CellRendererMx(self.treeview2)
        cell.set_property("editable", True)
        cell.edited = self.edited
        cell.set_preediting(self.get_editinfo)
        cell.set_refresh_fn(self.get_selected_feature)

        col.pack_start(cell, expand = False)
        col.set_cell_data_func(cell, self.get_col_value)
        col.set_resizable(True)
        col.set_min_width(200)
        self.treeview2.append_column(col)

        self.treeview2.set_tooltip_column(1)
        self.treeview2.set_model(self.treestore)
        self.treeview2.set_model(self.details_filter)
        self.params_scroll.add(self.treeview2)
        self.treeview2.connect('key-press-event', self.tv_key_pressed_event)

    def tv2_selected(self, tv, *arg):
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
            if self.selected_type in GROUP_HEADER_TYPES :
                tree_path = model.get_path(itr)
                if not tv.row_expanded(tree_path) :
                    tv.expand_row(tree_path, True)

        self.set_actions_sensitives()

    def get_toolbar_actions(self):
        global TB_CATALOG

        MENU_LISTING = {}

        def add_actions(path) :
            for ptr in range(len(path)) :
                try :
                    p = path[ptr]
                    if p.tag.lower() == "menuitem":
                        name = p.get("name")
                        actionname = p.get("action")
                        tooltip = p.get("tool_tip")
                        src = p.get("src")
                        icon = p.get("icon")

                        if (actionname is not None) and (src is not None) :
                            MENU_LISTING[actionname] = [name, tooltip, src, icon]
                    if p.tag.lower() == "menu" :
                        add_actions(p)
                except :
                    return

        def add_toolbar_def(path):
            toolbar_rank = 0
            for ptr in range(len(path)) :
                try :
                    p = path[ptr]
                    if p.tag.lower() == "separator":
                        TB_CATALOG[toolbar_rank] = "separator"
                    elif p.tag.lower() == 'toolitem':
                        TB_CATALOG[toolbar_rank] = MENU_LISTING[p.get("action")]
                except :
                    return
                toolbar_rank += 1

        for _ptr in range(len(self.catalog)) :
            _p = self.catalog[_ptr]
            if _p.tag.lower() == "menu":
                add_actions(_p)
            elif _p.tag.lower() == "toolbar":
                TB_CATALOG = {}
                add_toolbar_def(_p)

    def create_actions(self):
        def ca(actionname, stock_id, label, accel, tooltip, callback, *args):
            act = gtk.Action(actionname, label, tooltip, stock_id)
            if callback is not None :
                act.connect('activate', callback, args)
            if accel is not None :
                self.action_group.add_action_with_accel(act, accel)
            else :
                self.action_group.add_action(act)
            return act

        # actions related to projects_("Create a New Project")("Open A Project")_("Open a Saved Project xml file")_('Save Project')
        # "<control>X"
        self.actionProject = ca('Project', None, _("_Projects"), None, None, None)
        self.actionNew = ca("New", gtk.STOCK_NEW, None, "<control>N", None, self.action_new_project)
        self.actionOpen = ca("Open", gtk.STOCK_OPEN, None, "<control>O", None, self.action_open_project, 0)
        self.actionOpenExample = ca("OpenExample", None, _('Open Example'), '', _('Open Example Project'), self.action_open_project, 1)
        self.actionSave = ca("Save", gtk.STOCK_SAVE, None, "<control>S", _("Save project as xml file"), self.action_save_project)
        self.actionSaveTemplate = ca("SaveTemplate", None, _('Save as Default Template'), '', _("Save project as default template"), self.action_save_template)
        self.actionSaveNGC = ca("SaveNGC", None, _('Export gcode as RS274NGC'), '', _('Export gcode as RS274NGC'), self.action_save_ngc)

        # actions related to editing
        self.actionEditMenu = ca("EditMenu", None, _("_Edit"), None, None, self.edit_menu_activate)
        self.actionUndo = ca("Undo", gtk.STOCK_UNDO, None, "<control>Z", _('Undo last operation'), self.action_undo)
        self.actionRedo = ca("Redo", gtk.STOCK_REDO, None, "<control><shift>Z", _('Cancel last Undo'), self.action_redo)
        self.actionCut = ca("Cut", gtk.STOCK_CUT, None, "<control>X", _('Cut selected subroutine to clipboard'), self.action_cut)
        self.actionCopy = ca("Copy", gtk.STOCK_COPY, None, "<control>C", _('Copy selected subroutine to clipboard'), self.action_copy)
        self.actionPaste = ca("Paste", gtk.STOCK_PASTE, None, "<control>V", _('Paste from clipboard'), self.action_paste)
        self.actionAdd = ca("Add", gtk.STOCK_ADD, None, "<control>Insert", _('Add a subroutine'), self.action_add)
        self.actionDuplicate = ca("Duplicate", gtk.STOCK_COPY, _('Duplicate'), "<control>D", _('Duplicate selected subroutine'), self.action_duplicate)
        self.actionDelete = ca("Delete", gtk.STOCK_REMOVE, None, "<control>Delete", _('Remove selected subroutine'), self.action_delete)
        self.actionAppendItm = ca("AppendItm", gtk.STOCK_INDENT, _("Add to Items"), "<control>Right", _("Add to Items"), self.action_appendItm)
        self.actionRemoveItm = ca("RemoveItm", gtk.STOCK_UNINDENT, _("Remove from Items"), "<control>Left", _('Remove from Items'), self.action_removeItem)
        self.actionMoveUp = ca("MoveUp", gtk.STOCK_GO_UP, _('Move up'), "<control>Up", _('Move up'), self.move, 1)
        self.actionMoveDown = ca("MoveDown", gtk.STOCK_GO_DOWN, _('Move down'), "<control>Down", _('Move down'), self.move, -1)
        self.actionSaveUser = ca("SaveUser", gtk.STOCK_SAVE, _('Save Values as Defaults'), '', _('Save Values of this Subroutine as Defaults'), self.action_saveUser)
        self.actionDeleteUser = ca("DeleteUser", gtk.STOCK_CANCEL, _("Delete Custom Default Values"), None, _("Delete Custom Default Values"), self.action_deleteUser)
        self.actionSetDigits = ca("SetDigits", None, _('Set Digits'), None, None, None)
        self.actionDigit1 = ca("Digit1", None, '1', None, None, self.action_digits, '1')
        self.actionDigit2 = ca("Digit2", None, '2', None, None, self.action_digits, '2')
        self.actionDigit3 = ca("Digit3", None, '3', None, None, self.action_digits, '3')
        self.actionDigit4 = ca("Digit4", None, '4', None, None, self.action_digits, '4')
        self.actionDigit5 = ca("Digit5", None, "5", None, None, self.action_digits, '5')
        self.actionDigit6 = ca("Digit6", None, '6', None, None, self.action_digits, '6')

        # actions related to adding subroutines
        self.actionAddMenu = ca("AddMenu", None, _("_Add"), None, None, None)
        self.actionLoadCfg = ca("LoadCfg", gtk.STOCK_OPEN, _('Add a Prototype Subroutine'), '', _('Add a Subroutine Definition File'), self.action_loadCfg)
        self.actionImportXML = ca("ImportXML", gtk.STOCK_REVERT_TO_SAVED, _('Import a Project File'), None, _('Import a Project Into the Current One'), self.action_importXML)

        # actions related to view
        self.actionViewMenu = ca("ViewMenu", None, _("_View"), None, None, self.view_menu_activate)
        self.actionCollapse = ca("Collapse", gtk.STOCK_ZOOM_OUT, _("Collapse All Other Nodes"), '<control>K', _("Collapse All Other Nodes"), self.action_collapse)
        self.actionSaveLayout = ca("SaveLayout", gtk.STOCK_SAVE, _('Save As Default Layout'), '', _('Save As Default Layout'), self.action_saveLayout)

        self.action_group.add_radio_actions([
            ("SingleView", None, _('Single View'), None, None, 1),
            ("DualView", None,  _('Dual Views'), None, None, 2)
        ], 1, self.set_layout)

        self.action_group.add_radio_actions([
            ("TopBottom", None, _('Top / Bottom Layout'), None, None, 1),
            ("SideSide", None, _('Side By Side Layout'), None, None, 2)
        ], 1, self.set_layout)

        self.actionHideCol = gtk.ToggleAction("HideCol", _('Master Value Column Hidden'), _('In master treeview'), None)
        self.actionHideCol.connect("toggled", self.set_layout)
        self.action_group.add_action(self.actionHideCol)

        self.actionSubHdrs = gtk.ToggleAction("SubHdrs", _('Sub-Groups In Master Tree'), _('Sub-Groups In Master Tree'), None)
        self.actionSubHdrs.connect("toggled", self.set_layout)
        self.action_group.add_action(self.actionSubHdrs)

        # actions related to utilities
        self.actionUtilMenu = ca("UtilitiesMenu", None, _("_Utilities"), None, None, self.utilMenu_activate)
        self.actionLoadTools = ca("LoadTools", gtk.STOCK_REFRESH, _("Reload Tool Table"), None, _("Reload Tool Table"), TOOL_TABLE.load_table)
        self.actionPreferences = ca("Preferences", gtk.STOCK_PREFERENCES, _("Edit Preferences"), None, _("Edit Preferences"), self.action_preferences)

        self.actionAutoRefresh = gtk.ToggleAction("AutoRefresh", _("Auto-refresh"), _('Auto-refresh LinuxCNC'), None)
        self.actionAutoRefresh.set_active(False)
        self.action_group.add_action(self.actionAutoRefresh)

        self.actionChUnits = ca("ChUnits", None, _("Change Units"), None, _(""), self.action_chUnits)

        # actions related to validations
        self.actionValidationMenu = ca("ValidationMenu", gtk.STOCK_INFO, _("_Validation Messages"), None, None, self.validation_menu_activate)
        self.actionValAllDlg = ca("ValAllDlg", gtk.STOCK_YES, _("Show All"), None, _("Show All Non-validation Messages"), self.action_ValAllDlg)
        self.actionValNoDlg = ca("ValNoDlg", gtk.STOCK_NO, _("Show None"), None, _("Do Not Show Any Messages"), self.action_ValNoDlg)
        self.actionValFeatDlg = ca("ValFeatDlg", gtk.STOCK_YES, _("Show All For Current Type"), None, None, self.action_ValFeatDlg)
        self.actionValFeatNone = ca("ValFeatNone", gtk.STOCK_NO, _("Show None For Current Type"), None, None, self.action_ValFeatNone)

        # actions related to help
        self.actionHelpMenu = ca("HelpMenu", None, _("_Help"), None, None, None)
        self.actionYouTube = ca("YouTube", None, _('NativeCAM on YouTube'), None, None, self.action_youTube)
        self.actionYouTrans = ca("YouTranslate", None, _('Translating NativeCAM'), None, None, self.action_youTrans)
        self.actionCNCHome = ca("CNCHome", None, _("LinuxCNC web Site"), None, None, self.action_lcncHome)
        self.actionForum = ca("CNCForum", None, _('LinuxCNC Forum'), None, None, self.action_lcncForum)
        self.actionAbout = ca("About", gtk.STOCK_ABOUT, None, None, None, self.action_about)

        # actions related to toolbars and popup
        self.actionHideField = ca("HideField", None, _("Hide Selected Field"), None, _("Hide Selected Field"), self.action_hideField)
        self.actionShowF = ca("ShowFields", None, _("Show All Fields"), None, _("Show All Fields"), self.action_showFields)
        self.actionCurrent = ca("Current", gtk.STOCK_SAVE, _("Save Project as Current Work"), '', _('Save Project as Current Work'), self.action_saveCurrent)
        self.actionBuild = ca("Build", gtk.STOCK_EXECUTE, _('Generate %(filename)s') % {'filename':GENERATED_FILE}, None,
                     _('Generate %(filename)s and load it in LinuxCNC') % {'filename':GENERATED_FILE}, self.action_build)
        self.actionRename = ca("Rename", None, _("Rename Subroutine"), None, _('Rename Subroutine'), self.action_renameF)
        self.actionChngGrp = ca("ChngGrp", None, _("Group <-- --> Sub-group"), None, _('Group <-- --> Sub-group'), self.action_chng_group)
        self.actionDataType = ca("DataType", None, _("Change to GCode"), None, _('Change to GCode'), self.action_gcode)
        self.actionRevertType = ca("RevertType", None, _("Revert to original type"), None, _('Revert to original type'), self.action_revert_type)

    def get_actions_reference(self) :
        self.actionSingleView = self.uimanager.get_action("/dummy/SingleView")
        self.actionDualView = self.uimanager.get_action("/dummy/DualView")
        self.actionTopBottom = self.uimanager.get_action("/dummy/TopBottom")
        self.actionSideSide = self.uimanager.get_action("/dummy/SideSide")
        self.actionHideCol = self.uimanager.get_action("/dummy/HideCol")
        self.actionSubHdrs = self.uimanager.get_action("/dummy/SubHdrs")

    def action_saveLayout(self, *arg) :
        cfg_file = os.path.join(NCAM_DIR, CATALOGS_DIR, CONFIG_FILE)
        parser = ConfigParser.ConfigParser()
        parser.read(cfg_file)

        if not parser.has_section('layout') :
            parser.add_section('layout')
        parser.set('layout', 'subheaders_in_master', self.actionSubHdrs.get_active())
        parser.set('layout', 'hide_value_column', self.actionHideCol.get_active())
        parser.set('layout', 'dual_view', self.actionDualView.get_active())
        parser.set('layout', 'side_by_side', self.actionSideSide.get_active())

        with open(cfg_file, 'wb') as configfile:
            parser.write(configfile)

    def set_preferences(self):
        self.main_box.reorder_child(self.menubar, 0)
        self.main_box.reorder_child(self.main_toolbar, 1)
        self.main_box.reorder_child(self.nc_toolbar, 2)

        self.main_toolbar.set_icon_size(toolbar_icon_size)
        self.add_toolbar.set_icon_size(toolbar_icon_size)

        self.actionSubHdrs.set_active(self.pref.sub_hdrs_in_tv1)
        self.actionHideCol.set_active(self.pref.hide_value_column)
        self.actionTopBottom.set_active(not self.pref.side_by_side)
        self.actionSideSide.set_active(self.pref.side_by_side)
        self.actionSingleView.set_active(not self.pref.use_dual_views)
        self.actionDualView.set_active(self.pref.use_dual_views)

        self.actionCurrent.set_visible(not self.pref.autosave)
        self.name_cell.set_property('ellipsize', self.pref.name_ellipsis)
        self.treeview.set_show_expanders(self.pref.tv_expandable)
        if self.pref.tv_expandable :
            self.treeview.set_level_indentation(-5)
        else :
            self.treeview.set_level_indentation(12)

        if self.treeview2 is not None :
            self.name_cell2.set_property('ellipsize', self.pref.name_ellipsis)
            self.treeview2.set_show_expanders(self.pref.tv2_expandable)
            if self.pref.tv2_expandable :
                self.treeview2.set_level_indentation(-5)
            else :
                self.treeview2.set_level_indentation(12)

    def get_widgets(self):
        self.main_box = self.builder.get_object("MainBox")
        self.col_width_adj = self.builder.get_object("col_width_adj")
        self.col_width_adj.set_value(self.pref.col_width_adj_value)
        self.w_adj = self.builder.get_object("width_adj")
        self.w_adj.set_value(self.pref.w_adj_value)
        self.tv_w_adj = self.builder.get_object("tv_w_adj")
        self.tv_w_adj.set_value(self.pref.tv_w_adj_value)

        self.add_toolbar = self.builder.get_object("add_toolbar")
        self.add_toolbar.set_icon_size(toolbar_icon_size)

        self.feature_pane = self.builder.get_object("ncam_pane")
        self.feature_Hpane = self.builder.get_object("hpaned1")
        self.params_scroll = self.builder.get_object("params_scroll")
        self.frame2 = self.builder.get_object("frame2")
        self.addVBox = self.builder.get_object("frame3")
        self.add_iconview = self.builder.get_object("add_iconview")
        self.hint_label = self.builder.get_object("hint_label")

    def move(self, *arg) :
        itr = self.master_filter.convert_iter_to_child_iter(self.selected_feature_itr)
        if (arg[1][0] < 0) :
            itr_swap = self.master_filter.convert_iter_to_child_iter(self.iter_next)
        elif (arg[1][0] > 0) :
            itr_swap = self.master_filter.convert_iter_to_child_iter(self.iter_previous)
        self.treestore.swap(itr, itr_swap)
        self.get_selected_feature(self.treeview)
        self.action()

    def get_selected_feature(self, widget) :
        old_selected_feature = self.selected_feature
        (model, itr) = self.treeview.get_selection().get_selected()

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
                self.selected_param = ts_itr

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
                self.can_add_to_group = ('type="items"' in s) and \
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
            self.selected_type = 'xxx'
            self.can_move_up = False
            self.can_move_down = False
            self.can_add_to_group = False
            self.can_remove_from_group = False
            n_children = model.iter_n_children(None)
            self.items_lpath = (n_children,)
            tree_path = None
            self.hint_label.set_text('')

        if self.actionDualView.get_active() :
            if self.iter_selected_type == tv_select.none :
                if self.treeview2 is None:
                    self.create_second_treeview()
                else :
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

                self.treeview2.set_model(self.treestore)
                self.treeview2.set_model(self.details_filter)
                self.treeview2.expand_all()

        if tree_path is not None :
            self.treeview.expand_to_path(tree_path + (0, 0))
        self.can_delete_duplicate = (self.iter_selected_type == tv_select.feature)
        self.set_actions_sensitives()


    def action_appendItm(self, *arg) :
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
            self.action(xml)

    def action_removeItem(self, *arg) :
        xml = self.treestore_to_xml()
        src = xml.find(".//*[@path='%s']" % self.selected_feature_ts_path_s)
        src.set("new-selected", "True")
        parent = src.getparent().getparent()
        n = None
        while parent != xml and \
                    not (parent.tag == "param" and parent.get("type") == "items") and \
                    parent is not None :
            p = parent
            parent = parent.getparent()
            n = parent.index(p)
        if parent is not None and n is not None:
            parent.insert(n, src)
            self.treestore_from_xml(xml)
            self.expand_and_select(self.path_to_new_selected)
            self.action(xml)

    def expand_and_select(self, path):
        if path is not None :
            self.treeview.expand_to_path(path)
            self.treeview.set_cursor(path)
        else :
            self.treeview.expand_to_path((0,))
            self.treeview.set_cursor((0,))

    def action_duplicate(self, *arg) :
        xml = etree.Element(XML_TAG)
        self.treestore_to_xml_recursion(self.selected_feature_ts_itr, xml, False)
        self.import_xml(xml)

    def tv_row_activated(self, tv, path, col) :
        if tv.row_expanded(path) :
            tv.collapse_row(path)
        else:
            tv.expand_row(path, True)

    def tv_w_adj_value_changed(self, *arg):
        self.feature_pane.set_size_request(int(self.tv_w_adj.get_value()), 100)

    def col_width_adj_value_changed(self, *arg):
        self.treeview.get_column(0).set_min_width(int(self.col_width_adj.get_value()))
        if self.treeview2 is not None :
            self.treeview2.get_column(0).set_min_width(int(self.col_width_adj.get_value()))

    def tv_key_pressed_event(self, widget, event) :
        keyname = gdk.keyval_name(event.keyval)
        model, itr = widget.get_selection().get_selected()

        self.focused_widget = widget

        if itr is not None :
            path = model.get_path(itr)
        else :
            path = None

        if event.state & gdk.SHIFT_MASK :
            if event.state & gdk.CONTROL_MASK :
                if keyname in ['z', 'Z'] :
                    self.actionRedo.activate()

        elif event.state & gdk.CONTROL_MASK :
            if keyname in ['z', 'Z'] :
                self.actionUndo.activate()

            elif keyname == "Up" :
                self.actionMoveUp.activate()

            elif keyname == "Down" :
                self.actionMoveDown.activate()

            elif keyname == "Left" :
                self.actionRemoveItm.activate()

            elif keyname == "Right" :
                self.actionAppendItm.activate()

            elif keyname == "Insert" :
                self.actionAdd.activate()

            elif keyname == "Delete" :
                self.actionDelete.activate()

            elif (keyname in ["d", "D"]) :
                self.actionDuplicate.activate()

            elif (keyname in ["x", "X"]) :
                self.actionCut.activate()

            elif (keyname in ["c", "C"]) :
                self.actionCopy.activate()

            elif (keyname in ["v", "V"]) :
                self.actionPaste.activate()

            elif (keyname in ["n", "N"]) :
                self.actionNew.activate()

            elif (keyname in ["o", "O"]) :
                self.actionOpen.activate()

            elif (keyname in ["s", "S"]) :
                self.actionSave.activate()

            elif (keyname in ["k", "K"]) :
                self.actionCollapse.activate()

        else :

            if keyname == "Tab" and self.treeview2 is not None:
                if widget == self.treeview :
                    self.hint_label.set_markup(items_fmt_str % _("Secondary treeview focused"))
                    self.treeview2.grab_focus()
                    model, itr = self.treeview2.get_selection().get_selected()
                    if itr is None :
                        self.treeview2.set_cursor((0,))
                    else :
                        p = model.get_path(itr)
                        self.treeview2.set_cursor(p)
                else :
                    self.hint_label.set_markup(items_fmt_str % _("Primary treeview focused"))
                    self.treeview.grab_focus()
                    model, itr = self.treeview.get_selection().get_selected()
                    p = model.get_path(itr)
                    self.treeview.set_cursor(p)

            elif path is None :
                return False

            elif keyname == "Up" :
                if path != (0,) :
                    depth = len(path)
                    index_s = path[depth - 1]
                    if index_s > 0 :
                        p = path[0: depth - 1] + (index_s - 1,)
                        iter_p = model.get_iter(p)
                        while iter_p is not None :
                            count = model.iter_n_children(iter_p)
                            if (count == 0) or not widget.row_expanded(model.get_path(iter_p)) :
                                p = model.get_path(iter_p)
                                break
                            else :
                                iter_p = model.iter_nth_child(iter_p, count - 1)
                    else :
                        p = path[0: depth - 1]

                    widget.expand_to_path(p)
                    widget.set_cursor(p)

            elif keyname == "Down" :
                p = None
                if widget.row_expanded(path) :
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
                    widget.set_cursor(p)

            elif keyname == "Left" :
                if widget.row_expanded(path) :
                    widget.collapse_row(path)
                else :
                    depth = len(path)
                    d_len = 1
                    if depth > d_len :
                        apath = path[0:depth - d_len]
                        widget.set_cursor_on_cell(apath)
                        widget.collapse_row(apath)

            elif keyname == "Right" :
                widget.expand_to_path(path + (0, 0))

            elif keyname == "Home" :
                widget.set_cursor((0,))

            elif keyname == "Page_Up" :
                if path != (0,) :
                    depth = len(path)
                    index_s = path[depth - 1]
                    if index_s > 0 and widget.row_expanded(path) :
                        widget.set_cursor(path[0: depth - 1] + (index_s - 1,))
                    else :
                        if depth > 1 :
                            widget.set_cursor(path[0: depth - 1],)
                        else :
                            widget.set_cursor((path[0] - 1,))

            elif keyname == "End" :
                p = (path[0],)
                iter_p = model.get_iter(p)
                ito = iter_p
                while iter_p is not None :
                    p = model.get_path(iter_p)
                    ito = iter_p
                    iter_p = model.iter_next(iter_p)

                while widget.row_expanded(p) :
                    p = model.get_path(model.iter_children(ito))
                    iter_p = model.get_iter(p)
                    ito = iter_p
                    while iter_p is not None :
                        p = model.get_path(iter_p)
                        ito = iter_p
                        iter_p = model.iter_next(iter_p)

                widget.set_cursor(p)

            elif keyname == "Page_Down" :
                itr_n = model.iter_next(itr)
                if itr_n is not None :
                    widget.set_cursor(model.get_path(itr_n))
                else :
                    itr_n = model.iter_children(itr)
                    if itr_n is not None :
                        widget.set_cursor(model.get_path(itr_n))

            elif keyname in ["Return", "KP_Enter", "space", "F2"] :
                widget.set_cursor_on_cell(path, focus_column = widget.get_column(1), start_editing = True)

            elif keyname == "BackSpace" :
                widget.get_column(1).get_cell_renderers()[0].set_Input('BS')
                widget.set_cursor_on_cell(path, focus_column = widget.get_column(1), start_editing = True)

            elif (keyname[-1] >= "0" and keyname[-1] <= "9") :
                widget.get_column(1).get_cell_renderers()[0].set_Input(keyname[-1])
                widget.set_cursor_on_cell(path, focus_column = widget.get_column(1), start_editing = True)

            elif keyname in ['KP_Decimal', 'period', 'comma', 'KP_Separator'] :
                widget.get_column(1).get_cell_renderers()[0].set_Input('0' + decimal_point)
                widget.set_cursor_on_cell(path, focus_column = widget.get_column(1), start_editing = True)

            elif keyname in ['KP_Subtract', 'KP_Add', 'plus', 'minus'] :
                widget.get_column(1).get_cell_renderers()[0].set_Input('-')
                widget.set_cursor_on_cell(path, focus_column = widget.get_column(1), start_editing = True)

            else :
                return False

        return True

    def treestore_from_xml(self, xml):

        treestore = gtk.TreeStore(object, str, gobject.TYPE_BOOLEAN, gobject.TYPE_BOOLEAN)

        def recursive(itr, xmlpath):
            for xml in xmlpath :
                if xml.tag == "feature" :
                    f = Feature(xml = xml)
                    tool_tip = f.get_tooltip()
                    citer = treestore.append(itr, [f, tool_tip, True, False])

                    grp_header = ''

                    for p in f.param :
                        header_name = p.attr["header"].lower() if "header" in p.attr else ''

                        tool_tip = p.get_tooltip() if "tool_tip" in p.attr else None
                        p_type = p.get_type()
                        p_hidden = get_int(p.attr['hidden'] if 'hidden' in p.attr else '0')

                        if self.actionDualView.get_active() :
                            if self.actionSubHdrs.get_active() :
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
                            recursive(piter, xmlpath_)
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
                            if (header_name == '') or (grp_header == '') :
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


        if xml is not None :
            recursive(treestore.get_iter_root(), xml)
        self.treestore = treestore
        self.master_filter = self.treestore.filter_new()
        self.master_filter.set_visible_column(2)
        self.treeview.set_model(self.treestore)
        self.treeview.set_model(self.master_filter)
        self.set_expand()

    def to_gcode(self, *arg) :
        global UNIQUE_ID
        UNIQUE_ID = 9

        def recursive(itr, ldr) :
            gcode_def = ""
            gcode = ""
            sub_ldr = ldr
            f = self.treestore.get(itr, 0)[0]
            if f.__class__ is Feature :
                f.validate()
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
            if f.__class__ is Feature :
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
        if self.pref.use_pct :
            return self.pref.default + gcode_def + \
            _("(end sub definitions)\n\n") + gcode + self.pref.ngc_post_amble + '\n%\n'
        else :
            return self.pref.default + gcode_def + \
            _("(end sub definitions)\n\n") + gcode + self.pref.ngc_post_amble + '\nM2\n'

    def action_save_ngc(self, *arg) :
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
            filechooserdialog.set_keep_above(True)
            filechooserdialog.set_transient_for(self.get_toplevel())

            if filechooserdialog.run() == gtk.RESPONSE_OK:
                gcode = self.to_gcode()
                filename = filechooserdialog.get_filename()
                if filename[-4] != ".ngc" not in filename :
                    filename += ".ngc"
                with open(filename, "wb") as f:
                    f.write(self.to_gcode())
                f.close()
        finally :
            filechooserdialog.destroy()

    def edited(self, renderer, path, new_value) :
        self.focused_widget = renderer.get_treeview()
        itr = renderer.get_treeview().get_model().get_iter(path)
        itr = renderer.get_treeview().get_model().convert_iter_to_child_iter(itr)
        param = self.treestore.get_value(itr, 0)

        # find parent to pass as arg to param.set_value
        parent_itr = self.treestore.iter_parent(itr)
        while self.treestore.get(parent_itr, 0)[0].__class__ is not Feature :
            parent_itr = self.treestore.iter_parent(parent_itr)
        parent = self.treestore.get_value(parent_itr, 0)

        value_changed = False

        if renderer.editdata_type == 'combo-user' :
            p_name = None
            df = param.get_attr('links')
            if (df is not None) :
                for dg in df.split(":") :
                    opt = dg.split('=')
                    if (opt[1] == new_value) :
                        p_name = '#param_' + opt[0]
                        break

            if p_name is not None :
                itr_n = self.treestore.iter_parent(itr)
                itr_n = self.treestore.iter_children(itr_n)

                # finding the linked param
                param_e = None
                while (itr_n is not None) :
                    param_u = self.treestore.get_value(itr_n, 0)
                    if param_u.get_attr('call') == p_name :
                        param_e = param_u
                        break
                    itr_n = self.treestore.iter_next(itr_n)

                if param_e is not None :
                    r = gtk.RESPONSE_NONE
                    renderer.set_tooltip(param_e.get_tooltip())
                    dt = param_e.get_type()
                    renderer.set_edit_datatype(dt)
                    renderer.set_param_value(param_e.get_value(True))
                    if dt in NUMBER_TYPES :
                        renderer.set_max_value(get_float(param_e.get_max_value()))
                        renderer.set_min_value(get_float(param_e.get_min_value()))
                        renderer.set_not_allowed(param_e.get_attr('not_allowed'))
                        r, v = renderer.edit_number(gmoccapy_time_out)

                    elif dt in ['string', 'gcode'] :
                        r, v = renderer.edit_string(gmoccapy_time_out)

                    elif dt == 'list' :
                        renderer.set_options(param_e.get_options())
                        r, v = renderer.edit_list(gmoccapy_time_out)

                    if r == gtk.RESPONSE_OK :
                        value_changed = param_e.set_value(v, parent)
                    else :
                        return

        if param.set_value(new_value, parent) or value_changed:
            self.refresh_views()
            self.action()
        self.focused_widget.grab_focus()

    def action_delete(self, *arg) :
        if self.iter_next is not None :
            next_path = self.master_filter.get_path(self.selected_feature_itr)
        elif self.iter_previous is not None :
            next_path = self.master_filter.get_path(self.iter_previous)
        elif self.selected_feature_parent_itr is not None :
            next_path = self.master_filter.get_path(self.selected_feature_parent_itr)
        else :
            next_path = None

        if self.selected_feature_ts_itr is not None :
            self.treestore.remove(self.selected_feature_ts_itr)

        if next_path is not None :
            self.treeview.set_cursor(next_path)

        self.action()
        self.get_selected_feature(self.treeview)

    def action_collapse(self, *arg) :
        (model, itr) = self.treeview.get_selection().get_selected()
        path = model.get_path(itr)
        if self.treeview2 is not None :
            (model, itr) = self.treeview2.get_selection().get_selected()
        self.treeview.collapse_all()
        self.treeview.expand_to_path(path)
        self.treeview.set_cursor(path)

        if (self.treeview2 is not None) :
            self.treeview2.collapse_all()
            if  (itr is not None) :
                path = model.get_path(itr)
                self.treeview2.expand_to_path(path)
                self.treeview2.set_cursor(path)

        if self.focused_widget is not None :
            self.focused_widget.grab_focus()

    def action_renameF(self, *arg):
        self.newnamedlg = gtk.MessageDialog(parent = None,
            flags = gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
            type = gtk.MESSAGE_QUESTION,
            buttons = gtk.BUTTONS_OK_CANCEL
        )
        old_name = self.selected_feature.get_attr('name')
        self.newnamedlg.set_markup(_('Enter new name for'))
        self.newnamedlg.format_secondary_markup(old_name)
        self.newnamedlg.set_title('NativeCAM')
        edit_entry = gtk.Entry()
        edit_entry.set_editable(True)
        edit_entry.set_text(old_name)
        edit_entry.connect('key-press-event', self.action_rename_keyhandler)
        self.newnamedlg.vbox.add(edit_entry)
        self.newnamedlg.set_keep_above(True)

        (tree_x, tree_y) = self.treeview.get_bin_window().get_origin()
        self.newnamedlg.move(tree_x, tree_y + self.click_y)

        self.newnamedlg.show_all()
        response = self.newnamedlg.run()
        if (response == gtk.RESPONSE_OK) :
            newname = edit_entry.get_text().lstrip(' ')
            if newname > '' :
                self.selected_feature.attr['name'] = newname
                self.refresh_views()
        self.newnamedlg.destroy()

    def action_rename_keyhandler(self, widget, event):
        keyname = gdk.keyval_name(event.keyval)
        if keyname in ['Return', 'KP_Enter']:
            self.newnamedlg.response(gtk.RESPONSE_OK)

    def import_xml(self, xml_i) :
        if xml_i.tag != XML_TAG:
            xml_i = xml_i.find(".//%s" % XML_TAG)

        if xml_i is not None :
            xml = self.treestore_to_xml()
            if self.iter_selected_type == tv_select.none :
                opt = 0
                next_path = None
            elif self.iter_selected_type == tv_select.items :
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

            for x in xml_i :
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
                    if 'short_id' in f.attr :
                        del f.attr['short_id']
                    xf.set("name", f.attr["name"])
                    xf.set("id", f.attr["id"])

            self.treestore_from_xml(xml)
            self.expand_and_select(next_path)
            self.get_selected_feature(self.treeview)
            self.action(xml)

    def add_feature(self, widget, src) :
        src_file = search_path(search_warning.dialog, src, CFG_DIR)
        if src_file is None:
            return

        with open(src_file) as a :
            src_data = a.read()
        if (".//%s" % XML_TAG) in src_data :
            xml = etree.parse(src_file).getroot()
        elif ('[SUBROUTINE]' in src_data)  :
            f = Feature(src = src_file)
            f.attr['src'] = src
            xml = etree.Element(XML_TAG)
            xml.append(f.to_xml())
        else :
            mess_dlg(_("'%(source_file)s' is not a valid cfg or xml file") % {'source_file':src_file})
            return
        self.import_xml(xml)

    def autorefresh_call(self, *arg) :
        fname = os.path.join(NGC_DIR, GENERATED_FILE)
        with open(fname, "wb") as f:
            f.write(self.to_gcode())

        try:
            linuxCNC = linuxcnc.command()
            stat = linuxcnc.stat()
            stat.poll()
            if stat.interp_state == linuxcnc.INTERP_IDLE :
                try :
                    Tkinter.Tk().tk.call("send", "axis", ("remote", "open_file_name", fname))
                except Tkinter.TclError as detail:
                    linuxCNC.reset_interpreter()
                    time.sleep(gmoccapy_time_out)
                    linuxCNC.mode(linuxcnc.MODE_AUTO)
                    time.sleep(0.3)
                    stat.poll()
                    if stat.task_mode == linuxcnc.MODE_AUTO:
                        linuxCNC.program_open(fname)
                    else:
                        mess_dlg(_('LinuxCNC could not change to AUTO mode. Generated NC file was not loaded.'))
        except Exception as e:
            self.actionAutoRefresh.set_active(False)
            if self.show_not_connected :
                mess_dlg(_('LinuxCNC not running\n\nStart LinuxCNC and\nactivate Auto-refresh menu item'))

        if self.focused_widget is not None :
            self.focused_widget.grab_focus()
        else :
            self.treeview.grab_focus()

        return False

    def action_hideField(self, *arg):
        path = self.master_filter.get_path(self.selected_feature_itr)
        self.treestore.get(self.selected_param, 0)[0].set_hidden(True)
        self.selected_feature.hide_field()
        xml_ = self.treestore_to_xml()
        self.treestore_from_xml(xml_)
        self.expand_and_select(path)
        self.action(xml = xml_, refresh = False)

    def action_showFields(self, *args):
        path = self.master_filter.get_path(self.selected_feature_itr)
        if self.selected_feature.show_all_fields() :
            xml_ = self.treestore_to_xml()
            self.treestore_from_xml(xml_)
            self.expand_and_select(path)
            self.action(xml = xml_, refresh = False)

    def action_chng_group(self, *arg):
        if self.treestore.get(self.selected_param, 0)[0].change_group() :
            path = self.master_filter.get_path(self.selected_feature_itr)
            xml_ = self.treestore_to_xml()
            self.treestore_from_xml(xml_)
            self.expand_and_select(path)
            self.action(xml = xml_, refresh = False)

    def action_gcode(self, *arg) :
        self.treestore.get(self.selected_param, 0)[0].set_type('gcode')
        self.selected_type = 'gcode'
        self.refresh_views()
        self.action()

    def action_revert_type(self, *arg) :
        self.treestore.get(self.selected_param, 0)[0].revert_type()
        self.selected_type = self.treestore.get(self.selected_param, 0)[0].get_type()
        self.refresh_views()
        self.action()

    def action_chUnits(self, *args):
        global default_metric
        default_metric = not default_metric
        self.pref.read(None, False)
        self.refresh_views()

    def refresh_views(self):
        self.treeview.queue_draw()
        if self.treeview2 is not None :
            self.treeview2.queue_draw()

    def action(self, xml = None, refresh = True) :
        if xml is None :
            xml = self.treestore_to_xml()

        self.undo_list = self.undo_list[:self.undo_pointer + 1]
        self.undo_list = self.undo_list[max(0, len(self.undo_list) - UNDO_MAX_LEN):]
        self.undo_list.append(etree.tostring(xml))
        self.undo_pointer = len(self.undo_list) - 1

        self.update_do_btns(refresh)

    def update_do_btns(self, refresh):
        self.set_do_buttons_state()
        if self.actionAutoRefresh.get_active() and refresh:
            self.timeout = gobject.timeout_add(self.pref.timeout_value,
                    self.autorefresh_call)

    def action_undo(self, *arg) :
        save_restore = self.pref.restore_expand_state
        self.pref.restore_expand_state = True
        self.undo_pointer -= 1
        self.treestore_from_xml(etree.fromstring(self.undo_list[self.undo_pointer]))
        self.expand_and_select(self.path_to_old_selected)
        self.pref.restore_expand_state = save_restore
        self.update_do_btns(True)

    def action_redo(self, *arg) :
        save_restore = self.pref.restore_expand_state
        self.pref.restore_expand_state = True
        self.undo_pointer += 1
        self.treestore_from_xml(etree.fromstring(self.undo_list[self.undo_pointer]))
        self.expand_and_select(self.path_to_old_selected)
        self.pref.restore_expand_state = save_restore
        self.update_do_btns(True)

    def set_do_buttons_state(self):
        self.actionUndo.set_sensitive(self.undo_pointer > 0)
        self.actionRedo.set_sensitive(self.undo_pointer < (len(self.undo_list) - 1))

    def clear_undo(self, *arg) :
        self.undo_list = []
        self.undo_pointer = -1
        self.set_do_buttons_state()

    def action_about(self, *arg):
        dialog = gtk.AboutDialog()
        dialog.set_name(APP_TITLE)

        try :
            ver = subprocess.check_output(["dpkg-query", "--show", "--showformat='${Version}'", "nativecam"])
            dialog.set_version(str(ver).strip("'"))
        except :
            dialog.set_version(APP_VERSION)

        try :
            data = open('/usr/share/doc/nativecam/copyright', 'r').read()
            dialog.set_license(data)
        except :
            dialog.set_license(APP_LICENCE)

        dialog.set_authors(APP_AUTHORS)
        dialog.set_comments(APP_COMMENTS)
        dialog.set_copyright(APP_COPYRIGHT)
        dialog.set_website(HOME_PAGE)
        dialog.run()
        dialog.destroy()

    def action_save_template(self, *arg):
        xml = self.treestore_to_xml()
        etree.ElementTree(xml).write(os.path.join(NCAM_DIR, CATALOGS_DIR, \
                    self.catalog_dir, PROJECTS_DIR, DEFAULT_TEMPLATE), pretty_print = True)

    def load_currentWork(self):
        global CURRENT_PROJECT

        self.treestore.clear()
        self.clear_undo()
        fn = search_path(search_warning.none, CURRENT_WORK, \
                         CATALOGS_DIR, self.catalog_dir, PROJECTS_DIR)
        if fn is not None :
            xml = etree.parse(fn).getroot()
            nxml = self.update_features(xml)
            self.treestore_from_xml(nxml)
            self.expand_and_select((0,))
            CURRENT_PROJECT = _('Untitle.xml')
            self.display_proj_name()
            self.file_changed = False
            self.action(nxml)
        else :
            print(_('Previous work not saved as current work'))
            self.action_new_project()

    def action_new_project(self, *arg):
        global CURRENT_PROJECT

        self.treestore.clear()
        self.clear_undo()
        fn = search_path(search_warning.none, DEFAULT_TEMPLATE, \
                         CATALOGS_DIR, self.catalog_dir, PROJECTS_DIR)
        if fn is None :
            print(_('No default template saved'))
        else :
            xml = etree.parse(fn).getroot()
            xml = self.update_features(xml)
            self.treestore_from_xml(xml)
            self.expand_and_select((0,))
        CURRENT_PROJECT = _('Untitle.xml')
        self.display_proj_name()
        self.file_changed = False
        self.action()

    def set_layout(self, *arg):
        if self.actionDualView.get_active() :
            if self.treeview2 is None :
                self.create_second_treeview()
                self.treeview2.show_all()
            if self.actionSideSide.get_active() :
                self.frame2.reparent(self.feature_Hpane)
            else :
                self.frame2.reparent(self.feature_pane)
        else :
            if self.treeview2 is not None :
                self.treeview2.destroy()
                self.treeview2 = None

        self.treestore_from_xml(self.treestore_to_xml())
        self.expand_and_select(self.path_to_new_selected)

        self.frame2.set_visible(self.actionDualView.get_active())
        self.actionTopBottom.set_sensitive(self.actionDualView.get_active())
        self.actionSideSide.set_sensitive(self.actionDualView.get_active())
        self.actionSubHdrs.set_sensitive(self.actionDualView.get_active())
        self.actionHideCol.set_sensitive(self.actionDualView.get_active())
        self.treeview.get_column(1).set_visible(self.actionSingleView.get_active() or \
                                    not self.actionHideCol.get_active())

    def on_scale_change_value(self, widget):
        self.main_box.set_size_request(int(self.w_adj.get_value()), 100)

    def create_nc_toolbar(self):
        if self.nc_toolbar is not None :
            self.nc_toolbar.destroy()
        self.nc_toolbar = gtk.Toolbar()
        self.nc_toolbar.set_style(gtk.TOOLBAR_ICONS)
        self.nc_toolbar.set_can_focus(False)

        count = len(TB_CATALOG)
        for x in range(count) :
            li = TB_CATALOG[x]
            if li == 'separator' :
                self.nc_toolbar.insert(gtk.SeparatorToolItem(), -1)
            else :
                if li[3] is not None :
                    icon = gtk.Image()
                    icon.set_from_pixbuf(get_pixbuf(li[3], quick_access_icon_size))
                    button = gtk.ToolButton(icon_widget = icon, label = _(li[0]))
                else :
                    button = gtk.ToolButton(label = li[0])
                if li[1] is not None :
                    button.set_tooltip_markup(_(li[1]))
                button.connect('clicked', self.add_feature, li[2])
                self.nc_toolbar.insert(button, -1)

        self.main_box.pack_start(self.nc_toolbar, False, False, 0)

    def get_col_name(self, column, cell, model, itr, *arg) :
        data_type = model.get_value(itr, 0).get_type()
        val = model.get_value(itr, 0).get_name()
        if model.get_value(itr, 0).get_grayed() :
            if data_type == 'header' :
                cell.set_property('markup', gray_header_fmt_str % val)
            elif data_type == 'sub-header' :
                if  self.actionDualView.get_active() and not self.actionSubHdrs.get_active() :
                    cell.set_property('markup', gray_sub_header_fmt_str2 % val)
                else :
                    cell.set_property('markup', gray_sub_header_fmt_str % val)
            elif data_type == 'items' :
                cell.set_property('markup', gray_items_fmt_str % val)
            elif data_type in SUPPORTED_DATA_TYPES :
                cell.set_property('markup', gray_val % val)
            else :
                cell.set_property('markup', gray_feature_fmt_str % val)

        else :
            if data_type == 'header' :
                cell.set_property('markup', header_fmt_str % val)
            elif data_type == 'sub-header' :
                if  self.actionDualView.get_active() and not self.actionSubHdrs.get_active() :
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
        param = model.get_value(itr, 0)

        if param.get_grayed() :
            data_type = 'grayed'
        else :
            data_type = param.get_type()
            cell.set_param_value(param.get_value(True))
            cell.set_tooltip(_(param.get_tooltip()))

        cell.set_edit_datatype(data_type)

        if data_type in ['combo', 'combo-user', 'list']:
            cell.set_options(_(param.get_options()))

        elif data_type in NUMBER_TYPES:
            cell.set_max_value(get_float(param.get_max_value()))
            cell.set_min_value(get_float(param.get_min_value()))
            cell.set_not_allowed(param.get_attr('not_allowed'))
            cell.set_convertible_units('metric_value' in param.attr)

        elif data_type == 'tool' :
            cell.set_options(TOOL_TABLE.list)

        elif data_type == 'filename' :
            cell.set_fileinfo(param.attr['patterns'], \
                            param.attr['mime_types'], \
                            param.attr['filter_name'])


    def get_col_value(self, column, cell, model, itr, *arg) :
        global CURRENT_PROJECT

        param = model.get_value(itr, 0)
        val = param.get_value()
        dval = param.get_display_string()

        cell.set_param_value(val)

        data_type = param.get_type()
        cell.set_data_type(data_type)

        if data_type == 'filename':
            h, dval = os.path.split(val)

        elif data_type == 'prjname':
            h, dval = os.path.split(CURRENT_PROJECT)
            dval, h = os.path.splitext(dval)

        elif data_type == 'tool' :
            dval = TOOL_TABLE.get_text(val)

        if data_type == 'combo':
            options = _(param.get_attr('options'))
            for option in options.split(":") :
                opt = option.split('=')
                if opt[1] == val :
                    dval = opt[0]
                    break

        elif data_type == 'combo-user':
            p_name = None
            df = param.get_attr('links')
            if df is not None :
                for dg in df.split(":") :
                    opt = dg.split('=')
                    if (opt[1] == val) :
                        p_name = '#param_' + opt[0]
                        break

            # not a user defined value but one proposed
            if p_name is None :
                options = _(param.attr['options'])
                for option in options.split(":") :
                    opt = option.split('=')
                    if opt[1] == val :
                        dval = opt[0]
                        break
            else :
                itr_n = model.convert_iter_to_child_iter(itr)
                itr_n = self.treestore.iter_parent(itr_n)
                itr_n = self.treestore.iter_children(itr_n)

                # finding the linked param
                while (itr_n is not None) :
                    param = self.treestore.get_value(itr_n, 0)
                    if param.get_attr('call') == p_name :
                        break
                    itr_n = self.treestore.iter_next(itr_n)

                if param is not None :
                    data_type = param.get_type()
                    link_val = param.get_value()
                    if data_type == 'list':
                        options = _(param.get_attr('options'))
                        for option in options.split(":") :
                            opt = option.split('=')
                            if opt[1] == link_val :
                                dval = opt[0]
                                break
                    else :
                        dval = param.get_display_string()

        ps = param.get_attr('prefix')
        if ps is not None :
            dval = ps + ' ' + dval
        ps = param.get_attr('suffix')
        if ps is not None :
            dval += ' ' + ps

        if data_type == 'text' :
            cell.set_property("wrap-width", 180)
        else :
            cell.set_property("wrap-width", -1)
        if param.get_grayed() :
            cell.set_property('markup', gray_val % dval.replace('&#176;', '°'))
        else :
            cell.set_property('text', dval.replace('&#176;', '°'))


    def get_col_icon(self, column, cell, model, itr) :
        if model.get_value(itr, 0).get_type() in NO_ICON_TYPES :
            cell.set_property('pixbuf', None)
        else :
            cell.set_property('pixbuf', model.get_value(itr, 0).get_icon(treeview_icon_size))

    def treestore_to_xml_recursion(self, itr, xmlpath, allitems = True):
        while itr :
            f = self.treestore.get(itr, 0)[0]
            if f.__class__ is Feature :
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
        itr = self.treestore.get_iter_root()
        if itr is not None :
            try :
                self.treestore_to_xml_recursion(itr, xml)
                return xml
            except Exception as detail :
                print(_('Error in treestore_to_xml\n%(err_details)s') % {'err_details':detail})
                mess_dlg(_('Error in treestore_to_xml\n%(err_details)s') % {'err_details':detail})
        else :
            self.iter_selected_type = tv_select.none
            return xml

    def set_expand(self) :
        def treestore_set_expand(model, path, itr) :
            try :
                mf_itr = self.master_filter.convert_child_iter_to_iter(itr)
                mf_pa = self.master_filter.get_path(mf_itr)
                p = model.get(itr, 0)[0].attr
                if ("expanded" in p and p["expanded"] == "True") \
                        and self.pref.restore_expand_state :
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
        self.selection = self.treeview.get_selection()
        self.selection.unselect_all()
        self.treestore.foreach(treestore_set_expand)

    def get_expand(self) :
        self.selection = self.treeview.get_selection()
        model, pathlist = self.selection.get_selected_rows()

        def treestore_get_expand(model, path, itr) :
            p = model.get(itr, 0)[0]
            p.attr["path"] = model.get_string_from_iter(itr)
            try :
                mf_itr = self.master_filter.convert_child_iter_to_iter(itr)
                mf_pa = self.master_filter.get_path(mf_itr)
                p.attr["old-selected"] = mf_pa in pathlist
                p.attr["new-selected"] = False
                p.attr["expanded"] = self.treeview.row_expanded(mf_pa)
            except :
                # not in treeview
                pass

        self.treestore.foreach(treestore_get_expand)

    def action_importXML(self, *arg) :
        filechooserdialog = gtk.FileChooserDialog(_("Import project"), None, \
                gtk.FILE_CHOOSER_ACTION_OPEN, (gtk.STOCK_CANCEL, \
                gtk.RESPONSE_CANCEL, gtk.STOCK_OK, gtk.RESPONSE_OK))
        try:
            filt = gtk.FileFilter()
            filt.set_name(_("NativeCAM projects"))
            filt.add_mime_type("text/xml")
            filt.add_pattern("*.xml")
            filechooserdialog.add_filter(filt)
            filt = gtk.FileFilter()
            filt.set_name(_("All files"))
            filt.add_pattern("*")
            filechooserdialog.add_filter(filt)
            filechooserdialog.set_current_folder(os.path.join(NCAM_DIR, CATALOGS_DIR, self.catalog_dir, PROJECTS_DIR))
            filechooserdialog.set_keep_above(True)
            filechooserdialog.set_transient_for(self.get_toplevel())

            if filechooserdialog.run() == gtk.RESPONSE_OK:
                fname = filechooserdialog.get_filename()
                try :
                    xml = self.update_features(etree.parse(fname).getroot())
                    self.import_xml(xml)
                    self.file_changed = True
                except etree.ParseError as err :
                    mess_dlg(err, _("Import project"))
        finally:
            filechooserdialog.destroy()

    # will update with new features version and keep the previous values
    def update_features(self, xml_i):
        new_xml = etree.Element(XML_TAG)

        def upd2(parent):
            exec(parent.attr['init'])

        def recursive(xmlpath, dst):
            for xml in xmlpath :
                if xml.tag == "feature" :
                    f_A = Feature(xml = xml)
                    src_f = search_path(search_warning.none, f_A.get_attr('src'), CFG_DIR)
                    if src_f is None :
                        f = f_A
                    else :
                        f_B = Feature(src = src_f)
                        if f_B.get_version() > f_A.get_version() :
                            f_B.attr['src'] = f_A.attr['src']
                            # assign all old values
                            f_B.attr['name'] = f_A.attr['name']
                            f_B.attr['expanded'] = f_A.attr['expanded']
                            f_B.attr['old-selected'] = f_A.attr['old-selected']
                            f_B.attr['new-selected'] = f_A.attr['new-selected']
                            if f_A.get_value() != '' :
                                f_B.set_value(f_A.get_value())
                            if 'hidden_count' in f_A.attr :
                                f_B.attr['hidden_count'] = f_A.attr['hidden_count']

                            for p in f_A.param :
                                call = p.attr['call']
                                for q in f_B.param :
                                    if q.attr['call'] == call :
                                        q.attr['path'] = p.attr['path']
                                        if 'value' in p.attr :
                                            q.attr['value'] = p.attr['value']
                                        if 'minimum_value' in p.attr :
                                            q.attr['minimum_value'] = p.attr['minimum_value']
                                        if 'maximum_value' in p.attr :
                                            q.attr['maximum_value'] = p.attr['maximum_value']
                                        if 'hidden' in p.attr :
                                            q.attr['hidden'] = p.attr['hidden']
                                        if 'grayed' in p.attr :
                                            q.attr['grayed'] = p.attr['grayed']
                                        break
                            f = f_B
                            if 'init' in f.attr :
                                upd2(f)
                        else :
                            f = f_A

                    f.get_id(new_xml)
                    if 'short_id' in f.attr :
                        del f.attr['short_id']
                    if "validation" not in f.attr :
                        f.attr["validation"] = ""
                    for p in f_A.param :
                        if 'no_zero' in p.attr :
                            del p.attr['no_zero']
                            p.attr['not_allowed'] = '0'

                    if dst is None :
                        new_xml.append(f.to_xml())
                    else :
                        dest = new_xml.find(".//*[@path='%s']" % dst)
                        dest.append(f.to_xml())

                xmlp = xml.find(".//param[@type='items']")
                if xmlp is not None :
                    recursive(xmlp, xmlp.get("path"))

        recursive(xml_i, None)
        return new_xml


    def action_save_project(self, *arg) :
        global CURRENT_PROJECT

        filechooserdialog = gtk.FileChooserDialog(_("Save project as..."), None,
                gtk.FILE_CHOOSER_ACTION_SAVE, (gtk.STOCK_CANCEL, \
                gtk.RESPONSE_CANCEL, gtk.STOCK_OK, gtk.RESPONSE_OK))
        try:
            filt = gtk.FileFilter()
            filt.set_name(_("NativeCAM projects"))
            filt.add_mime_type("text/xml")
            filt.add_pattern("*.xml")
            filechooserdialog.add_filter(filt)
            d, fname = os.path.split(CURRENT_PROJECT)
            filechooserdialog.set_current_folder(os.path.join(NCAM_DIR, CATALOGS_DIR, self.catalog_dir, PROJECTS_DIR))
            filechooserdialog.set_current_name(fname)
            filechooserdialog.set_do_overwrite_confirmation(True)
            filechooserdialog.set_keep_above(True)
            filechooserdialog.set_transient_for(self.get_toplevel())

            if filechooserdialog.run() == gtk.RESPONSE_OK:
                xml = self.treestore_to_xml()
                CURRENT_PROJECT = filechooserdialog.get_filename()
                if CURRENT_PROJECT[-4] != ".xml" not in CURRENT_PROJECT :
                    CURRENT_PROJECT += ".xml"
                etree.ElementTree(xml).write(CURRENT_PROJECT, pretty_print = True)
                self.file_changed = False
        finally:
            self.display_proj_name()
            filechooserdialog.destroy()

    def display_proj_name(self):
        h, t = os.path.split(CURRENT_PROJECT)
        t, h = os.path.splitext(t)
        self.mnu_current_project.set_label(_(' "%s"') % t)

    def action_open_project(self, *arg):
        global CURRENT_PROJECT

        if arg[1][0] == 0 :  # user project
            dlg_title = _("Open project")
            flt_name = _("NativeCAM projects")
            dir_ = os.path.join(NCAM_DIR, CATALOGS_DIR, self.catalog_dir, PROJECTS_DIR)
        else :  # example
            dlg_title = _("Open example project")
            flt_name = _("NativeCAM example projects")
            dir_ = os.path.join(NCAM_DIR, CATALOGS_DIR, self.catalog_dir, PROJECTS_DIR, EXAMPLES_DIR)

        filechooserdialog = gtk.FileChooserDialog(dlg_title, None,
                gtk.FILE_CHOOSER_ACTION_OPEN, (gtk.STOCK_CANCEL, \
                gtk.RESPONSE_CANCEL, gtk.STOCK_OK, gtk.RESPONSE_OK))
        try:
            filt = gtk.FileFilter()
            filt.set_name(flt_name)
            if arg[1][0] == 0 :
                filt.add_mime_type("text/xml")
                filt.add_pattern("*.xml")
            else :
                filt.add_pattern("*.*")
            filechooserdialog.add_filter(filt)
            filechooserdialog.set_current_folder(dir_)
            filechooserdialog.set_keep_above(True)
            filechooserdialog.set_transient_for(self.get_toplevel())

            if filechooserdialog.run() == gtk.RESPONSE_OK:
                filename = filechooserdialog.get_filename()
                src_data = open(filename).read()
                if src_data.find(XML_TAG) != 1 :
                    subprocess.call(["xdg-open '%s'" % filename], shell = True)
                else :
                    xml = etree.fromstring(src_data)
                    xml = self.update_features(xml)
                    self.treestore_from_xml(xml)
                    self.expand_and_select(self.path_to_old_selected)
                    self.clear_undo()
                    CURRENT_PROJECT = filename
                    self.file_changed = False
                    self.action(xml)
        finally:
            self.display_proj_name()
            filechooserdialog.destroy()

    def action_loadCfg(self, *arg) :
        filechooserdialog = gtk.FileChooserDialog(_("Open a cfg file"), None, \
                    gtk.FILE_CHOOSER_ACTION_OPEN, \
                    (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL,
                     gtk.STOCK_OK, gtk.RESPONSE_OK))
        try:
            filt = gtk.FileFilter()
            filt.set_name(_("Config files"))
            filt.add_mime_type("text/xml")
            filt.add_pattern("*.cfg")
            filechooserdialog.add_filter(filt)
            filechooserdialog.set_current_folder(os.path.join(NCAM_DIR, CUSTOM_DIR))
            filechooserdialog.set_keep_above(True)
            filechooserdialog.set_transient_for(self.get_toplevel())

            if filechooserdialog.run() == gtk.RESPONSE_OK:
                self.add_feature(None, filechooserdialog.get_filename())
        finally :
            filechooserdialog.destroy()

    def set_actions_sensitives(self):
        self.actionCollapse.set_sensitive(self.selected_feature is not None)

        self.actionSave.set_sensitive(self.selected_feature is not None)
        self.actionSaveTemplate.set_sensitive(self.selected_feature is not None)
        self.actionSaveNGC.set_sensitive(self.selected_feature is not None)

        self.actionSaveUser.set_sensitive(self.selected_feature is not None)
        self.actionDeleteUser.set_sensitive((self.selected_feature is not None) and \
                    (self.selected_feature.get_type() in USER_SUBROUTINES))

        self.actionDelete.set_sensitive(self.can_delete_duplicate)
        self.actionDuplicate.set_sensitive(self.can_delete_duplicate)
        self.actionMoveUp.set_sensitive(self.can_move_up)
        self.actionMoveDown.set_sensitive(self.can_move_down)
        self.actionAppendItm.set_sensitive(self.can_add_to_group)
        self.actionRemoveItm.set_sensitive(self.can_remove_from_group)
        self.actionCut.set_sensitive(self.can_delete_duplicate)
        self.actionCopy.set_sensitive(self.can_delete_duplicate)

        self.actionChngGrp.set_visible(self.selected_type in ["sub-header", "header"] and \
                            self.actionDualView.get_active())
        self.actionSetDigits.set_visible(self.selected_type == 'float')
        self.actionDataType.set_visible(self.selected_type in ['float', 'int'])
        self.actionRevertType.set_visible(self.selected_type == 'gcode')

        self.actionRename.set_visible(self.iter_selected_type == tv_select.feature)

        self.actionHideField.set_sensitive((self.selected_type in SUPPORTED_DATA_TYPES) and \
                                      (self.selected_type != 'items'))
        self.actionShowF.set_sensitive(self.selected_feature is not None and \
                                       self.selected_feature.has_hidden_fields())


def verify_ini(fname, ctlog, in_tab) :
    path2ui = os.path.join(SYS_DIR, 'ncam.ui')
    req = '# required NativeCAM item :\n'

    with open(fname, 'r') as b :
        txt = b.read()
    if (path2ui not in txt) or ('my-stuff' not in txt) :
        if not os.path.exists(fname + '.bak') :
            with open(fname + '.bak', 'w') as b :
                b.write(txt)
                print(_('Backup file created : %s.bak') % fname)

        if (txt.find('--catalog=mill') > 0) or (txt.find('-cmill') > 0) :
            ctlog = 'mill'
        elif (txt.find('--catalog=lathe') > 0) or (txt.find('-clathe') > 0) :
            ctlog = 'lathe'
        elif (txt.find('--catalog=plasma') > 0) or (txt.find('-cplasma') > 0) :
            ctlog = 'plasma'

        txt1 = ''
        txt2 = txt.split('\n')
        for line in txt2 :
            txt1 += line.lstrip(' \t') + '\n'

        parser = ConfigParser.RawConfigParser()
        try :
            parser.readfp(io.BytesIO(txt1))

            dp = parser.get('DISPLAY', 'DISPLAY').lower()
            if dp not in ['gmoccapy', 'axis', 'gscreen'] :
                mess_dlg(_("DISPLAY can only be 'axis', 'gmoccapy' or 'gscreen'"))
                sys.exit(-1)

            try :
                old_sub_path = ':' + parser.get('RS274NGC', 'SUBROUTINE_PATH')
            except :
                old_sub_path = ''

            try :
                c = parser.get('DISPLAY', 'LATHE')
                if c.lower() in ['1', 'true'] :
                    ctlog = 'lathe'
            except :
                pass

            txt = re.sub(r"%s" % req, '', txt)

            if dp == 'axis' :
                if in_tab :
                    newstr = '%s%s%s%s %s\n' % (req, 'EMBED_TAB_NAME = NativeCAM\n', \
                            'EMBED_TAB_COMMAND = gladevcp -x {XID} -U --catalog=', \
                            ctlog, path2ui)
                    txt = re.sub(r"\[DISPLAY\]", "[DISPLAY]\n" + newstr, txt)
                else :
                    newstr = '%sGLADEVCP = -U --catalog=%s %s\n' % (req, ctlog, path2ui)
                    try :
                        oldstr = 'GLADEVCP = %s' % parser.get('DISPLAY', 'gladevcp')
                        txt = re.sub(r"%s" % oldstr, newstr, txt)
                    except :
                        txt = re.sub(r"\[DISPLAY\]", "[DISPLAY]\n" + newstr, txt)

            elif (dp == 'gmoccapy') :
                if in_tab :
                    newstr = '%s%s%s%s%s %s\n' % (req, 'EMBED_TAB_NAME = NativeCAM\n', \
                            'EMBED_TAB_LOCATION = ntb_user_tabs\n', \
                            'EMBED_TAB_COMMAND = gladevcp -x {XID} -U --catalog=', \
                            ctlog, path2ui)
                    txt = re.sub(r"\[DISPLAY\]", "[DISPLAY]\n" + newstr, txt)
                else :
                    newstr = '%sEMBED_TAB_LOCATION = box_right\n' % req
                    try :
                        oldstr = 'EMBED_TAB_LOCATION = %s' % parser.get('DISPLAY', 'embed_tab_location')
                        txt = re.sub(r"%s" % oldstr, newstr, txt)
                    except :
                        txt = re.sub(r"\[DISPLAY\]", "[DISPLAY]\n" + newstr, txt)

                    newstr = '%sEMBED_TAB_NAME = right_side_panel\n' % req
                    try :
                        oldstr = 'EMBED_TAB_NAME = %s' % parser.get('DISPLAY', 'embed_tab_name')
                        txt = re.sub(r"%s" % oldstr, newstr, txt)
                    except :
                        txt = re.sub(r"\[DISPLAY\]", "[DISPLAY]\n" + newstr, txt)

                    newstr = '%sEMBED_TAB_COMMAND = gladevcp -x {XID} -U --catalog=%s %s\n' % (req, ctlog, path2ui)
                    try :
                        oldstr = 'EMBED_TAB_COMMAND = %s' % parser.get('DISPLAY', 'embed_tab_command')
                        txt = re.sub(r"%s" % oldstr, newstr, txt)
                    except :
                        txt = re.sub(r"\[DISPLAY\]", "[DISPLAY]\n" + newstr, txt)

            else :  # gscreen
                newstr = '%sEMBED_TAB_COMMAND = gladevcp -x {XID} -U --catalog=%s %s\n' % (req, ctlog, path2ui)
                try :
                    oldstr = 'EMBED_TAB_COMMAND = %s' % parser.get('DISPLAY', 'embed_tab_command')
                    txt = re.sub(r"%s" % oldstr, newstr, txt)
                except :
                    txt = re.sub(r"\[DISPLAY\]", "[DISPLAY]\n" + newstr, txt)

                newstr = '%sEMBED_TAB_LOCATION = vcp_box\n' % req
                try :
                    oldstr = 'EMBED_TAB_LOCATION = %s' % parser.get('DISPLAY', 'embed_tab_location')
                    txt = re.sub(r"%s" % oldstr, newstr, txt)
                except :
                    txt = re.sub(r"\[DISPLAY\]", "[DISPLAY]\n" + newstr, txt)

                newstr = '%sEMBED_TAB_NAME = NativeCAM\n' % req
                try :
                    oldstr = 'EMBED_TAB_NAME = %s' % parser.get('DISPLAY', 'embed_tab_name')
                    txt = re.sub(r"%s" % oldstr, newstr, txt)
                except :
                    txt = re.sub(r"\[DISPLAY\]", "[DISPLAY]\n" + newstr, txt)

            newstr = '%sPROGRAM_PREFIX = ncam/scripts/\n' % req
            try :
                oldstr = 'PROGRAM_PREFIX = ' + parser.get('DISPLAY', 'program_prefix')
                txt = re.sub(r"%s" % oldstr, newstr, txt)
            except :
                txt = re.sub(r"\[DISPLAY\]", "[DISPLAY]\n" + newstr, txt)

            newstr = '%sNCAM_DIR = ncam\n' % req
            try :
                oldstr = 'NCAM_DIR = ' + parser.get('DISPLAY', 'ncam_dir')
                txt = re.sub(r"%s" % oldstr, newstr, txt)
            except :
                txt = re.sub(r"\[DISPLAY\]", "[DISPLAY]\n" + newstr, txt)

            if not 'ncam/my-stuff:ncam/lib/' in old_sub_path :
                newstr = '%sSUBROUTINE_PATH = ncam/my-stuff:ncam/lib/%s:ncam/lib/utilities%s\n' % \
                    (req, ctlog, old_sub_path)
                try :
                    oldstr = 'SUBROUTINE_PATH = ' + parser.get('RS274NGC', 'subroutine_path')
                    txt = re.sub(r"%s" % oldstr, newstr, txt)
                except :
                    txt = re.sub(r"\[RS274NGC\]", "[RS274NGC]\n" + newstr, txt)

            open(fname, 'w').write(txt)

            with open(fname, 'w') as b :
                b.write(txt)
                print(_('Success in modifying inifile :\n  %s') % fname)

        except Exception as detail :
            err_exit(_('Error modifying ini file\n%(err_details)s') % {'err_details':detail})

def usage():
    print("""
Standalone Usage:
   ncam [Options]

Options :
    -h | --help                this text
   (-i | --ini=) inifilename   inifile used
   (-c | --catalog=) catalog   valid catalogs = mill, plasma, lathe
    -t | --tab                 axis and gmoccapy only, put NativeCAM in a new tab

To prepare your inifile to use NativeCAM embedded,
   a) Start in a working directory with your LinuxCNC configuration ini file
   b) Type this command :
     ncam (-i | --ini=)inifilename (-c | --catalog=)(valid catalog for this configuration)

   A backup of your inifile will be created before it is modified.

   After success, you can use it embedded  :
     linuxcnc inifilename

""")

if __name__ == "__main__":
    # process args
    args = sys.argv[1:]
    if "-h" in args or "--help" in args:
        usage()
        sys.exit(0)

    try :
        optlist, args = getopt.getopt(sys.argv[1:], 'c:i:t', ["catalog=", "ini="])
    except getopt.GetoptError as err:
        print(err)  # will print something like "option -a not recognized"
        usage()
        sys.exit(2)

    optlist = dict(optlist)

    if "-i" in optlist :
        ini = optlist["-i"]
    elif "--ini" in optlist :
        ini = optlist["--ini"]
    else :
        ini = None

    if (ini is not None) :
        if "-c" in optlist :
            catalog = optlist["-c"]
        elif "--catalog" in optlist :
            catalog = optlist["--catalog"]
        else :
            catalog = DEFAULT_CATALOG
        if not catalog in VALID_CATALOGS :
            usage()
            sys.exit(3)

        in_tab = ("-t" in optlist) or ("--tab" in optlist)
        verify_ini(os.path.abspath(ini), catalog, in_tab)

    window = gtk.Dialog(APP_TITLE, None, gtk.DIALOG_MODAL)
    ncam = NCam()
    window.vbox.add(ncam)
    ncam.actionCurrent.set_visible(True)
    window.add_accel_group(ncam.accelGroup)
    window.connect("destroy", gtk.main_quit)
    window.set_default_size(400, 800)
    exit(window.run())
