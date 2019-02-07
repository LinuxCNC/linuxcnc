#!/usr/bin/env python
# -*- coding:UTF-8 -*-
"""
    A GUI for LinuxCNC based on gladevcp and Python
    Based on the design of moccagui from Tom
    and with a lot of code from gscreen from Chris Morley
    and with the help from Michael Haberler
    and Chris Morley and some more

    Copyright 2012 / 2017 Norbert Schechner
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

"""

import traceback           # needed to launch traceback errors
import hal                 # base hal class to react to hal signals
import hal_glib            # needed to make our own hal pins
import gtk                 # base for pygtk widgets and constants
import sys                 # handle system calls
import os                  # needed to get the paths and directories
import pango               # needed for font settings and changing
import gladevcp.makepins   # needed for the dialog"s calculator widget
import atexit              # needed to register child's to be closed on closing the GUI
import subprocess          # to launch onboard and other processes
import vte                 # To get the embedded terminal
import tempfile            # needed only if the user click new in edit mode to open a new empty file
import linuxcnc            # to get our own error system
import gobject             # needed to add the timer for periodic
import locale              # for setting the language of the GUI
import gettext             # to extract the strings to be translated

from gladevcp.gladebuilder import GladeBuilder

from time import strftime  # needed for the clock in the GUI
from gtk._gtk import main_quit

# Throws up a dialog with debug info when an error is encountered
def excepthook(exc_type, exc_obj, exc_tb):
    try:
        w = app.widgets.window1
    except KeyboardInterrupt:
        sys.exit()
    except NameError:
        w = None
    lines = traceback.format_exception(exc_type, exc_obj, exc_tb)
    m = gtk.MessageDialog(w,
                          gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                          gtk.MESSAGE_ERROR, gtk.BUTTONS_OK,
                          ("Found an error!\nThe following information may be useful in troubleshooting:\n\n")
                          + "".join(lines))
    m.show()
    m.run()
    m.destroy()


sys.excepthook = excepthook

debug = False

if debug:
    pydevdir = '/home/emcmesa/liclipse/plugins/org.python.pydev_4.5.4.201601292050/pysrc'

    if os.path.isdir(pydevdir):  # and  'emctask' in sys.builtin_module_names:
        sys.path.append(pydevdir)
        sys.path.insert(0, pydevdir)
        try:
            import pydevd

            print("pydevd imported, connecting to Eclipse debug server...")
            pydevd.settrace()
        except:
            print("no pydevd module found")
            pass

# constants
#         # gmoccapy  #"
_RELEASE = " 2.3.5"
_INCH = 0                         # imperial units are active
_MM = 1                           # metric units are active

# set names for the tab numbers, its easier to understand the code
# Bottom Button Tabs
_BB_MANUAL = 0
_BB_MDI = 1
_BB_AUTO = 2
_BB_HOME = 3
_BB_TOUCH_OFF = 4
_BB_SETUP = 5
_BB_EDIT = 6
_BB_TOOL = 7
_BB_LOAD_FILE = 8
#_BB_HOME_JOINTS will not be used, we will reorder the notebooks to get the correct page shown

_TEMPDIR = tempfile.gettempdir()  # Now we know where the tempdir is, usualy /tmp

# set up paths to files
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
LIBDIR = os.path.join(BASE, "lib", "python")
sys.path.insert(0, LIBDIR)

# as now we know the libdir path we can import our own modules
from gmoccapy import widgets       # a class to handle the widgets
from gmoccapy import notification  # this is the module we use for our error handling
from gmoccapy import preferences   # this handles the preferences
from gmoccapy import getiniinfo    # this handles the INI File reading so checking is done in that module
from gmoccapy import dialogs       # this takes the code of all our dialogs

_AUDIO_AVAILABLE = False
try:
    import gst
    from gmoccapy import player        # a class to handle sounds
    _AUDIO_AVAILABLE = True
except:
    pass
# set up paths to files, part two
CONFIGPATH = os.environ['CONFIG_DIR']
DATADIR = os.path.join(BASE, "share", "gmoccapy")
IMAGEDIR = os.path.join(DATADIR, "images")
XMLNAME = os.path.join(DATADIR, "gmoccapy.glade")
THEMEDIR = "/usr/share/themes"
USERTHEMEDIR = os.path.join(os.path.expanduser("~"), ".themes")
LOCALEDIR = os.path.join(BASE, "share", "locale")

# path to TCL for external programs eg. halshow
TCLPATH = os.environ['LINUXCNC_TCL_DIR']

# the ICONS should must be in share/gmoccapy/images
ALERT_ICON = os.path.join(IMAGEDIR, "applet-critical.png")
INFO_ICON = os.path.join(IMAGEDIR, "std_info.gif")

# this is for hiding the pointer when using a touch screen
pixmap = gtk.gdk.Pixmap(None, 1, 1, 1)
color = gtk.gdk.Color()
INVISABLE = gtk.gdk.Cursor(pixmap, pixmap, color, color, 0, 0)


class gmoccapy(object):
    def __init__(self, argv):
        
        # prepare for translation / internationalisation
        locale.setlocale(locale.LC_ALL, '')
        locale.bindtextdomain("gmoccapy", LOCALEDIR)
        gettext.install("gmoccapy", localedir=LOCALEDIR, unicode=True)
        gettext.bindtextdomain("gmoccapy", LOCALEDIR)

        # needed components to comunicate with hal and linuxcnc
        self.halcomp = hal.component("gmoccapy")
        self.command = linuxcnc.command()
        self.stat = linuxcnc.stat()

        self.error_channel = linuxcnc.error_channel()
        # initial poll, so all is up to date
        self.stat.poll()
        self.error_channel.poll()

        self.builder = gtk.Builder()
        # translation of the glade file will be done with
        self.builder.set_translation_domain("gmoccapy")
        self.builder.add_from_file(XMLNAME)
        self.builder.connect_signals(self)

        self.widgets = widgets.Widgets(self.builder)

        self.initialized = False  # will be set True after the window has been shown and all
                                  # basic settings has been finished, so we avoid some actions
                                  # because we cause click or toggle events when initializing
                                  # widget states.

        self.start_line = 0  # needed for start from line

        self.active_gcodes = []   # this are the formated G code values
        self.active_mcodes = []   # this are the formated M code values
        self.gcodes = []          # this are the unformatted G code values to check if an update is required
        self.mcodes = []          # this are the unformatted M code values to check if an update is required

        self.distance = 0         # This global will hold the jog distance
        self.tool_change = False  # this is needed to get back to manual mode after a tool change
        self.load_tool = False    # We use this avoid mode switching on reloading the tool on start up of the GUI
        self.macrobuttons = []    # The list of all macros defined in the INI file
        self.fo_counts = 0        # need to calculate difference in counts to change the feed override slider
        self.so_counts = 0        # need to calculate difference in counts to change the spindle override slider
        self.jv_counts = 0        # need to calculate difference in counts to change the jog_vel slider
        self.ro_counts = 0        # need to calculate difference in counts to change the rapid override slider

        self.spindle_override = 1 # holds the feed override value and is needed to be able to react to halui pin
        self.feed_override = 1    # holds the spindle override value and is needed to be able to react to halui pin
        self.rapidrate = 1        # holds the rapid override value and is needed to be able to react to halui pin

        self.incr_rbt_list = []   # we use this list to add hal pin to the button later
        self.jog_increments = []  # This holds the increment values
        self.unlock = False       # this value will be set using the hal pin unlock settings

        # needed to display the labels
        self.system_list = ("0", "G54", "G55", "G56", "G57", "G58", "G59", "G59.1", "G59.2", "G59.3")
        self.dro_size = 28           # The size of the DRO, user may want them bigger on bigger screen
        self.axisnumber_four = ""    # we use this to get the number of the 4-th axis
        self.axisletter_four = None  # we use this to get the letter of the 4-th axis
        self.axisnumber_five = ""    # we use this to get the number of the 5-th axis
        self.axisletter_five = None  # we use this to get the letter of the 5-th axis

        self.notification = notification.Notification()  # Our own message system
        self.notification.connect("message_deleted", self._on_message_deleted)
        self.last_key_event = None, 0  # needed to avoid the auto repeat function of the keyboard
        self.all_homed = False         # will hold True if all axis are homed
        self.faktor = 1.0              # needed to calculate velocities

        self.xpos = 40        # The X Position of the main Window
        self.ypos = 30        # The Y Position of the main Window
        self.width = 979      # The width of the main Window
        self.height = 750     # The height of the main Window

        self.gcodeerror = ""   # we need this to avoid multiple messages of the same error

        self.lathe_mode = None # we need this to check if we have a lathe config

        # the default theme = System Theme we store here to be able to go back to that one later
        self.default_theme = gtk.settings_get_default().get_property("gtk-theme-name")

        self.dialogs = dialogs.Dialogs()
        self.dialogs.connect("play_sound", self._on_play_sound)

        # check the arguments given from the command line (Ini file)
        self.user_mode = False
        self.logofile = None
        for index, arg in enumerate(argv):
            print(index, " = ", arg)
            if arg == "-user_mode":
                self.user_mode = True
                self.widgets.tbtn_setup.set_sensitive(False)
                message = _("**** GMOCCAPY INI Entry **** \n")
                message += _("user mode selected")
                print (message)
            if arg == "-logo":
                self.logofile = str(argv[ index + 1 ])
                message = _("**** GMOCCAPY INI Entry **** \n")
                message += _("logo entry found = {0}").format(self.logofile)
                print (message)
                self.logofile = self.logofile.strip("\"\'")
                if not os.path.isfile(self.logofile):
                    self.logofile = None
                    message = _("**** GMOCCAPY INI Entry Error **** \n")
                    message += _("Logofile entry found, but could not be converted to path.\n")
                    message += _("The file path should not contain any spaces")
                    print(message)

        # check if the user want a Logo (given as command line argument)
        if self.logofile:
            self.widgets.img_logo.set_from_file(self.logofile)
            self.widgets.img_logo.show()

            page2 = self.widgets.ntb_jog_JA.get_nth_page(2)
            self.widgets.ntb_jog_JA.reorder_child(page2, 0)
            page1 = self.widgets.ntb_jog_JA.get_nth_page(1)
            self.widgets.ntb_jog_JA.reorder_child(page1, -1)

        # Our own clas to get information from ini the file we use this way, to be sure
        # to get a valid result, as the checks are done in that module
        self.get_ini_info = getiniinfo.GetIniInfo()

        self.prefs = preferences.preferences(self.get_ini_info.get_preference_file_path())

        self._get_axis_list()
        # self._init_extra_axes() # will be called from _get_axis_list

        self._init_jog_increments()

        self._init_hal_pins()

        self._init_user_messages()

        # set the title of the window, to show the release
        self.widgets.window1.set_title("gmoccapy for linuxcnc {0}".format(_RELEASE))
        self.widgets.lbl_version.set_label("<b>gmoccapy\n{0}</b>".format(_RELEASE))

        panel = gladevcp.makepins.GladePanel(self.halcomp, XMLNAME, self.builder, None)

        self.halcomp.ready()

        # this are settings to be done before window show
        self._init_preferences()

        # finally show the window
        self.widgets.window1.show()

        if self.stat.kinematics_type != linuxcnc.KINEMATICS_IDENTITY:
            self._init_joints_btn()

        self._init_dynamic_tabs()
        self._init_tooleditor()
        self._init_themes()
        self._init_audio()
        self._init_gremlin()
        self._init_hardware_button_order()
        self._init_kinematics_type()
        self._init_hide_cursor()
        self._init_offsetpage()
        self._init_keybindings()
        self._init_IconFileSelection()
        self._init_keyboard()

        # now we initialize the file to load widget
        self._init_file_to_load()

        self._show_offset_tab(False)
        self._show_tooledit_tab(False)
        self._show_iconview_tab(False)

        # the velocity settings
        self.min_spindle_rev = self.prefs.getpref("spindle_bar_min", 0.0, float)
        self.max_spindle_rev = self.prefs.getpref("spindle_bar_max", 6000.0, float)
        self.widgets.adj_spindle_bar_min.set_value(self.min_spindle_rev)
        self.widgets.adj_spindle_bar_max.set_value(self.max_spindle_rev)
        self.widgets.spindle_feedback_bar.set_property("min", float(self.min_spindle_rev))
        self.widgets.spindle_feedback_bar.set_property("max", float(self.max_spindle_rev))

        # Popup Messages position and size
        self.widgets.adj_x_pos_popup.set_value(self.prefs.getpref("x_pos_popup", 45, float))
        self.widgets.adj_y_pos_popup.set_value(self.prefs.getpref("y_pos_popup", 55, float))
        self.widgets.adj_width_popup.set_value(self.prefs.getpref("width_popup", 250, float))
        self.widgets.adj_max_messages.set_value(self.prefs.getpref("max_messages", 10, float))
        self.widgets.fontbutton_popup.set_font_name(self.prefs.getpref("message_font", "sans 10", str))
        self.widgets.chk_use_frames.set_active(self.prefs.getpref("use_frames", True, bool))

        # Only used if the DRO buttons will remain in gmoccapy
        self.widgets.chk_show_dro_btn.set_active(self.prefs.getpref("show_dro_btn", False, bool))
        self.widgets.chk_auto_units.set_active(self.prefs.getpref("use_auto_units", True, bool))
        self.on_chk_show_dro_btn_toggled(None)
        self.on_chk_auto_units_toggled(None)
        if self.widgets.Combi_DRO_x.machine_units == 0:
            self.widgets.tbtn_units.set_active(True)

        self.widgets.tbtn_rel.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        self.widgets.tbtn_dtg.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        self.widgets.tbtn_units.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        # end of the button usage

        # this sets the background colors of several buttons
        # the colors are different for the states of the button
        self.widgets.tbtn_on.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        self.widgets.tbtn_estop.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FF0000"))
        self.widgets.tbtn_estop.modify_bg(gtk.STATE_NORMAL, gtk.gdk.color_parse("#00FF00"))
        self.widgets.rbt_manual.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        self.widgets.rbt_mdi.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        self.widgets.rbt_auto.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        self.widgets.tbtn_setup.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        self.widgets.rbt_forward.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#00FF00"))
        self.widgets.rbt_reverse.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#00FF00"))
        self.widgets.rbt_stop.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        self.widgets.rbt_view_p.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        self.widgets.rbt_view_x.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        self.widgets.rbt_view_y.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        self.widgets.rbt_view_y2.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        self.widgets.rbt_view_z.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        self.widgets.tbtn_flood.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#00FF00"))
        self.widgets.tbtn_fullsize_preview.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        self.widgets.tbtn_fullsize_preview1.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        self.widgets.tbtn_mist.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#00FF00"))
        self.widgets.tbtn_optional_blocks.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        self.widgets.tbtn_optional_stops.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        self.widgets.tbtn_user_tabs.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        self.widgets.tbtn_view_dimension.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        self.widgets.tbtn_view_tool_path.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        self.widgets.tbtn_edit_offsets.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        self.widgets.tbtn_switch_mode.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))

        # tool measurement probe settings
        xpos, ypos, zpos, maxprobe = self.get_ini_info.get_tool_sensor_data()
        if not xpos or not ypos or not zpos or not maxprobe:
            self.widgets.chk_use_tool_measurement.set_active(False)
            self.widgets.chk_use_tool_measurement.set_sensitive(False)
            self.widgets.btn_block_height.set_sensitive(False)
            self.widgets.lbl_tool_measurement.show()
            self.widgets.btn_zero_g92.show()
            self.widgets.btn_block_height.hide()
            print(_("**** GMOCCAPY INFO ****"))
            print(_("**** no valid probe config in INI File ****"))
            print(_("**** disabled tool measurement ****"))
        else:
            self.widgets.lbl_tool_measurement.hide()
            self.widgets.spbtn_probe_height.set_value(self.prefs.getpref("probeheight", -1.0, float))
            self.widgets.spbtn_search_vel.set_value(self.prefs.getpref("searchvel", 75.0, float))
            self.widgets.spbtn_probe_vel.set_value(self.prefs.getpref("probevel", 10.0, float))
            self.widgets.chk_use_tool_measurement.set_active(self.prefs.getpref("use_toolmeasurement", False, bool))
            # to set the hal pin with correct values we emit a toogled
            self.widgets.lbl_x_probe.set_label(str(xpos))
            self.widgets.lbl_y_probe.set_label(str(ypos))
            self.widgets.lbl_z_probe.set_label(str(zpos))
            self.widgets.lbl_maxprobe.set_label(str(maxprobe))
            self.widgets.btn_block_height.set_sensitive(True)
            self.widgets.btn_zero_g92.hide()
            self.widgets.btn_block_height.show()
            self._replace_list_item(4, "btn_zero_g92", "btn_block_height")
            print(_("**** GMOCCAPY INFO ****"))
            print(_("**** found valid probe config in INI File ****"))
            print(_("**** will use auto tool measurement ****"))
        self.widgets.chk_use_tool_measurement.emit("toggled")


        # should the tool in spindle be reloaded on startup?
        self.widgets.chk_reload_tool.set_active(self.prefs.getpref("reload_tool", True, bool))

        # and the rest of the widgets
        self.widgets.rbt_manual.set_active(True)
        self.widgets.ntb_jog.set_current_page(0)
        
        opt_blocks = self.prefs.getpref("blockdel", False, bool)
        self.widgets.tbtn_optional_blocks.set_active(opt_blocks)
        self.command.set_block_delete(opt_blocks)
        
        optional_stops = self.prefs.getpref( "opstop", False, bool )
        self.widgets.tbtn_optional_stops.set_active( optional_stops )
        self.command.set_optional_stop( optional_stops )

        self.widgets.chk_show_dro.set_active(self.prefs.getpref("enable_dro", False, bool))
        self.widgets.chk_show_offsets.set_active(self.prefs.getpref("show_offsets", False, bool))
        self.widgets.chk_show_dtg.set_active(self.prefs.getpref("show_dtg", False, bool))
        self.widgets.chk_show_offsets.set_sensitive(self.widgets.chk_show_dro.get_active())
        self.widgets.chk_show_dtg.set_sensitive(self.widgets.chk_show_dro.get_active())
        self.widgets.cmb_mouse_button_mode.set_active(self.prefs.getpref("mouse_btn_mode", 4, int))

        self.widgets.tbtn_view_tool_path.set_active(self.prefs.getpref("view_tool_path", True, bool))
        self.widgets.tbtn_view_dimension.set_active(self.prefs.getpref("view_dimension", True, bool))
        view = self.prefs.getpref("gremlin_view", "rbt_view_p", str)
        self.widgets[view].set_active(True)

        # get if run from line should be used
        rfl = self.prefs.getpref("run_from_line", "no_run", str)
        # and set the corresponding button active
        self.widgets["rbtn_{0}_from_line".format(rfl)].set_active(True)
        if rfl == "no_run":
            self.widgets.btn_from_line.set_sensitive(False)
        else:
            self.widgets.btn_from_line.set_sensitive(True)

        # get the way to unlock the setting
        unlock = self.prefs.getpref("unlock_way", "use", str)
        # and set the corresponding button active
        self.widgets["rbt_{0}_unlock".format(unlock)].set_active(True)
        # if Hal pin should be used, only set the button active, if the pin is high
        if unlock == "hal" and not self.halcomp["unlock-settings"]:
            self.widgets.tbtn_setup.set_sensitive(False)
        self.unlock_code = self.prefs.getpref("unlock_code", "123", str)  # get unlock code

        # check if the user want to display preview window instead of offsetpage widget
        state = self.prefs.getpref("show_preview_on_offset", False, bool)
        if state:
            self.widgets.rbtn_show_preview.set_active(True)
        else:
            self.widgets.rbtn_show_offsets.set_active(True)

        # check if keyboard shortcuts should be used and set the chkbox widget
        self.widgets.chk_use_kb_shortcuts.set_active(self.prefs.getpref("use_keyboard_shortcuts",
                                                                        False, bool))

        # check the highlighting type
        # the following would load the python language
        # self.widgets.gcode_view.set_language("python")
        LANGDIR = os.path.join(BASE, "share", "gtksourceview-2.0", "language-specs")
        file_path = os.path.join(LANGDIR, "gcode.lang")
        if os.path.isfile(file_path):
            print "**** GMOCCAPY INFO: Gcode.lang found ****"
            self.widgets.gcode_view.set_language("gcode", LANGDIR)

        # set the user colors and digits of the DRO
        self.abs_color = self.prefs.getpref("abs_color", "#0000FF", str)         # blue
        self.rel_color = self.prefs.getpref("rel_color", "#000000", str)         # black
        self.dtg_color = self.prefs.getpref("dtg_color", "#FFFF00", str)         # yellow
        self.homed_color = self.prefs.getpref("homed_color", "#00FF00", str)     # green
        self.unhomed_color = self.prefs.getpref("unhomed_color", "#FF0000", str) # red
        self.widgets.abs_colorbutton.set_color(gtk.gdk.color_parse(self.abs_color))
        self.widgets.rel_colorbutton.set_color(gtk.gdk.color_parse(self.rel_color))
        self.widgets.dtg_colorbutton.set_color(gtk.gdk.color_parse(self.dtg_color))
        self.widgets.homed_colorbtn.set_color(gtk.gdk.color_parse(self.homed_color))
        self.widgets.unhomed_colorbtn.set_color(gtk.gdk.color_parse(self.unhomed_color))

        # set default values according to the machine units
        digits = 3
        if self.stat.linear_units != _MM:
            digits = 4
        self.dro_digits = self.prefs.getpref("dro_digits", digits, int)
        self.widgets.adj_dro_digits.set_value(self.dro_digits)
        # the adjustment change signal will set the dro_digits correct, so no extra need here.

        for axis in self.axis_list:
            if axis == self.axisletter_four:
                axis = 4
            if axis == self.axisletter_five:
                axis = 5
            self.widgets["Combi_DRO_{0}".format(axis)].set_property("abs_color", gtk.gdk.color_parse(self.abs_color))
            self.widgets["Combi_DRO_{0}".format(axis)].set_property("rel_color", gtk.gdk.color_parse(self.rel_color))
            self.widgets["Combi_DRO_{0}".format(axis)].set_property("dtg_color", gtk.gdk.color_parse(self.dtg_color))
            self.widgets["Combi_DRO_{0}".format(axis)].set_property("homed_color", gtk.gdk.color_parse(self.homed_color))
            self.widgets["Combi_DRO_{0}".format(axis)].set_property("unhomed_color", gtk.gdk.color_parse(self.unhomed_color))
            self.widgets["Combi_DRO_{0}".format(axis)].set_property("actual", self.dro_actual)

        self.toggle_readout = self.prefs.getpref("toggle_readout", True, bool)
        self.widgets.chk_toggle_readout.set_active(self.toggle_readout)

        self.widgets.adj_start_spindle_RPM.set_value(self.spindle_start_rpm)
        self.widgets.gcode_view.set_sensitive(False)
        self.widgets.ntb_user_tabs.remove_page(0)
        self._add_macro_button()

        if not self.get_ini_info.get_embedded_tabs()[2]:
            self.widgets.tbtn_user_tabs.set_sensitive(False)

        # call the function to change the button status
        # so every thing is ready to start
        widgetlist = ["rbt_manual", "rbt_mdi", "rbt_auto", "btn_homing", "btn_touch", "btn_tool",
                      "ntb_jog", "spc_feed", "btn_feed_100", "rbt_forward", "btn_index_tool",
                      "rbt_reverse", "rbt_stop", "tbtn_flood", "tbtn_mist", "btn_change_tool",
                      "btn_select_tool_by_no", "btn_spindle_100", "spc_rapid", "spc_spindle",
                      "btn_tool_touchoff_x", "btn_tool_touchoff_z"
        ]
        self._sensitize_widgets(widgetlist, False)

        # Do we control a lathe?
        if self.lathe_mode:
            # is this a backtool lathe?
            self.backtool_lathe = self.get_ini_info.get_backtool_lathe()

            # we first hide the Y button to home and touch off
            self.widgets.btn_home_y.hide()
            self.widgets.btn_set_value_y.hide()
            self.widgets.lbl_replace_y.show()
            self.widgets.lbl_replace_set_value_y.show()
            self.widgets.btn_tool_touchoff_x.show()
            self.widgets.lbl_hide_tto_x.hide()

            # we have to re-arrange the jog buttons, so first remove all button
            self.widgets.tbl_jog_btn_axes.remove(self.widgets.btn_y_minus)
            self.widgets.tbl_jog_btn_axes.remove(self.widgets.btn_y_plus)
            self.widgets.tbl_jog_btn_axes.remove(self.widgets.btn_x_minus)
            self.widgets.tbl_jog_btn_axes.remove(self.widgets.btn_x_plus)
            self.widgets.tbl_jog_btn_axes.remove(self.widgets.btn_z_minus)
            self.widgets.tbl_jog_btn_axes.remove(self.widgets.btn_z_plus)

            # now we place them in a different order
            if self.backtool_lathe:
                self.widgets.tbl_jog_btn_axes.attach(self.widgets.btn_x_plus, 1, 2, 0, 1, gtk.SHRINK, gtk.SHRINK)
                self.widgets.tbl_jog_btn_axes.attach(self.widgets.btn_x_minus, 1, 2, 2, 3, gtk.SHRINK, gtk.SHRINK)
            else:
                self.widgets.tbl_jog_btn_axes.attach(self.widgets.btn_x_plus, 1, 2, 2, 3, gtk.SHRINK, gtk.SHRINK)
                self.widgets.tbl_jog_btn_axes.attach(self.widgets.btn_x_minus, 1, 2, 0, 1, gtk.SHRINK, gtk.SHRINK)
            self.widgets.tbl_jog_btn_axes.attach(self.widgets.btn_z_plus, 2, 3, 1, 2, gtk.SHRINK, gtk.SHRINK)
            self.widgets.tbl_jog_btn_axes.attach(self.widgets.btn_z_minus, 0, 1, 1, 2, gtk.SHRINK, gtk.SHRINK)

            # The Y DRO we make to a second X DRO to indicate the diameter
            self.widgets.Combi_DRO_y.set_to_diameter(True)
            self.widgets.Combi_DRO_y.set_property("joint_number", 0)

            # we change the axis letters of the DRO's
            self.widgets.Combi_DRO_x.change_axisletter("R")
            self.widgets.Combi_DRO_y.change_axisletter("D")

            # and we will have to change the colors of the Y DRO according to the settings
            self.widgets.Combi_DRO_y.set_property("abs_color", gtk.gdk.color_parse(self.abs_color))
            self.widgets.Combi_DRO_y.set_property("rel_color", gtk.gdk.color_parse(self.rel_color))
            self.widgets.Combi_DRO_y.set_property("dtg_color", gtk.gdk.color_parse(self.dtg_color))
            self.widgets.Combi_DRO_y.set_property("homed_color", gtk.gdk.color_parse(self.homed_color))
            self.widgets.Combi_DRO_y.set_property("unhomed_color", gtk.gdk.color_parse(self.unhomed_color))
            self.widgets.Combi_DRO_y.set_property("actual", self.dro_actual)

            # For gremlin we don"t need the following button
            if self.backtool_lathe:
                self.widgets.rbt_view_y2.set_active(True)
            else:
                self.widgets.rbt_view_y.set_active(True)
            self.widgets.rbt_view_p.hide()
            self.widgets.rbt_view_x.hide()
            self.widgets.rbt_view_z.hide()

            # check if G7 or G8 is active
            if "70" in self.stat.gcodes:
                self._switch_to_g7(True)
            else:
                self._switch_to_g7(False)

        else:
            # the Y2 view is not needed on a mill
            self.widgets.rbt_view_y2.hide()
            # X Offset is not necessary on a mill
            self.widgets.lbl_tool_offset_x.hide()
            self.widgets.lbl_offset_x.hide()
            self.widgets.btn_tool_touchoff_x.hide()
            self.widgets.lbl_hide_tto_x.show()

        # this must be done last, otherwise we will get wrong values
        # because the window is not fully realized
        self._init_notification()

        # since the main loop is needed to handle the UI and its events, blocking calls like sleep()
        # will block the UI as well, so everything goes through event handlers (aka callbacks)
        # The gobject.timeout_add() function sets a function to be called at regular intervals
        # the time between calls to the function, in milliseconds
        # CYCLE_TIME = time, in milliseconds, that display will sleep between polls
        cycle_time = self.get_ini_info.get_cycle_time()
        gobject.timeout_add( cycle_time, self._periodic )  # time between calls to the function, in milliseconds

    def set_motion_mode(self, state):
        # 1:teleop, 0: joint
#        if self.stat.kinematics_type == linuxcnc.KINEMATICS_IDENTITY:
#            if self.all_homed or self.no_force_homing:
#                state = 1
        self.command.teleop_enable(state)
        self.command.wait_complete()

    def _get_axis_list(self):
        # begin with an empty axis list
        self.axis_list = self.get_ini_info.get_axis_list()
        self.joint_axis_dic = self.get_ini_info.get_joint_axis_relation()

        # if we receive a None, that means we do not have a trivial kinematics
        # like a scara or robot
        if self.joint_axis_dic == None:
            self._init_extra_axes()
            return

        self._init_extra_axes()

        for axis in self.joint_axis_dic:
            if len(axis) > 1:
                # means we do have double letters in coordinates, i.e. gantry
                # will return (x,y0,y1,z)
                # we only take the first axis, as we aspect the second one to be the slave
                if "0" in axis:
                    pass
                else:
                    continue
            if axis == self.axisletter_four:
                print("Combi_DRO_4 = joint {0}".format(self.joint_axis_dic[axis]))
                self.widgets.Combi_DRO_4.set_joint(self.joint_axis_dic[axis])
            elif axis == self.axisletter_five:
                print("Combi_DRO_5 = joint {0}".format(self.joint_axis_dic[axis]))
                self.widgets.Combi_DRO_5.set_joint(self.joint_axis_dic[axis])
            else:
                print("Combi_DRO_{0} = joint {1}".format(axis[0], self.joint_axis_dic[axis]))
                self.widgets["Combi_DRO_{0}".format(axis[0])].set_joint(self.joint_axis_dic[axis])

    def _init_extra_axes(self):
        # to much axes given, can only handle 5
        if len(self.axis_list) > 5:
            message = _("**** GMOCCAPY INFO : ****")
            message += _("**** gmoccapy can only handle 5 axis, ****\n**** but you have given {0} through your INI file ****\n").format(len(self.axis_list))
            message += _("**** gmoccapy will not start ****\n\n")
            print(message)
            self.dialogs.warning_dialog(self, _("Very critical situation"), message, sound = False)
            sys.exit()

        # XYZ machine or lathe, lathe will be handled in _init_preferences
        if len(self.axis_list) < 4:
            self.widgets.Combi_DRO_4.hide()
            self.widgets.Combi_DRO_5.hide()
            self.widgets.btn_home_4.hide()
            self.widgets.btn_home_5.hide()
            return

        # find first 5_th axis
        if len(self.axis_list) == 5:
            self.widgets.lbl_replace_set_value_y.hide()
            self.widgets.lbl_replace_4.hide()
            self.widgets.lbl_replace_5.hide()
            self.widgets.lbl_replace_set_value_4.hide()
            self.widgets.lbl_replace_set_value_5.hide()
            self.axisletter_five = self.axis_list[-1]
            self.axisnumber_five = "xyzabcuvw".index(self.axisletter_five)
            self.widgets.Combi_DRO_5.set_property("joint_number", self.axisnumber_five)
            self.widgets.Combi_DRO_5.change_axisletter(self.axisletter_five.upper())

            image = self.widgets["img_home_{0}".format(self.axisletter_five)]
            self.widgets.btn_home_5.set_image(image)
            self.widgets.btn_home_5.set_property("tooltip-text", _("Home axis {0}").format(self.axisletter_five.upper()))

            if self.axisletter_five in "abc":
                self.widgets.Combi_DRO_5.set_property("mm_text_template", "%11.2f")
                self.widgets.Combi_DRO_5.set_property("imperial_text_template", "%11.2f")

            image = self.widgets["img_home_{0}".format(self.axisletter_five)]
            self.widgets.btn_home_5.set_image(image)
            self.widgets.btn_home_5.set_property("tooltip-text", _("Home axis {0}").format(self.axisletter_five.upper()))
            self.widgets.btn_home_5.show()

            self.widgets.btn_5_plus.set_label("{0}+".format(self.axisletter_five.upper()))
            self.widgets.btn_5_plus.show()
            self.widgets.btn_5_minus.set_label("{0}-".format(self.axisletter_five.upper()))
            self.widgets.btn_5_minus.show()

            image = self.widgets["img_touch_off_{0}".format(self.axisletter_five)]
            self.widgets.btn_set_value_5.set_image(image)
            self.widgets.btn_set_value_5.set_property("tooltip-text", _("Set axis {0} value to").format(self.axisletter_five.upper()))
            self.widgets.btn_set_value_5.show()

        if self.axisletter_five:
            axis_four = list(set(self.axis_list) - set(("x", "y", "z")) - set(self.axisletter_five))
        else:
            self.widgets.lbl_replace_set_value_y.hide()
            self.widgets.lbl_replace_set_value_4.hide()
            self.widgets.lbl_replace_4.hide()
            self.widgets.Combi_DRO_5.hide()
            self.widgets.btn_home_5.hide()
            axis_four = list(set(self.axis_list) - set(("x", "y", "z")))
        self.axisletter_four = axis_four[0]
        self.axisnumber_four = "xyzabcuvw".index(self.axisletter_four)
        self.widgets.Combi_DRO_4.set_property("joint_number", self.axisnumber_four)
        self.widgets.Combi_DRO_4.change_axisletter(self.axisletter_four.upper())

        image = self.widgets["img_home_{0}".format(self.axisletter_four)]
        self.widgets.btn_home_4.set_image(image)
        self.widgets.btn_home_4.set_property("tooltip-text", _("Home axis {0}").format(self.axisletter_four.upper()))
        self.widgets.btn_home_4.show()

        self.widgets.btn_4_plus.set_label("{0}+".format(self.axisletter_four.upper()))
        self.widgets.btn_4_plus.show()
        self.widgets.btn_4_minus.set_label("{0}-".format(self.axisletter_four.upper()))
        self.widgets.btn_4_minus.show()

        image = self.widgets["img_touch_off_{0}".format(self.axisletter_four)]
        self.widgets.btn_set_value_4.set_image(image)
        self.widgets.btn_set_value_4.set_property("tooltip-text", _("Set axis {0} value to").format(self.axisletter_four.upper()))
        self.widgets.btn_set_value_4.show()

        if self.axisletter_four in "abc":
            self.widgets.Combi_DRO_4.set_property("mm_text_template", "%11.2f")
            self.widgets.Combi_DRO_4.set_property("imperial_text_template", "%11.2f")

        # We have to change the size of the DRO, to make them fit the space
        
        # XYZ machine or lathe, no need to change the size
        if len(self.axis_list) < 4:
            return
        # if we have 4 axis, we split the size of all DRO
        elif len(self.axis_list) < 5:
            size = int(self.dro_size * 0.75)
            for axis in self.axis_list:
                if axis == self.axisletter_four:
                    axis = 4
                    self.widgets.tbl_DRO_45.set_homogeneous(False)
                self.widgets["Combi_DRO_{0}".format(axis)].set_property("font_size", size)

        # if we have 5 axes, we will need some extra space:
        else:
            for axis in self.axis_list:
                size = self.dro_size
                if axis == self.axisletter_four:
                    axis = 4
                    size = int(size * 0.65) # This factor is just testing to ensure the DRO is able to fit with number 9999.999
                if axis == self.axisletter_five:
                    axis = 5
                    size = int(size * 0.65)
                self.widgets["Combi_DRO_{0}".format(axis)].set_property("font_size", size)

    def _init_preferences(self):
        # check if NO_FORCE_HOMING is used in ini
        self.no_force_homing = self.get_ini_info.get_no_force_homing()

        # disable reload tool on start up, if True
        if self.no_force_homing:
            self.widgets.chk_reload_tool.set_sensitive(False)
            self.widgets.chk_reload_tool.set_active(False)
            self.widgets.lbl_reload_tool.set_visible(True)

        # if there is a INI Entry for default spindle speed, we will use that one as default
        # but if there is a setting in our preference file, that one will beet the INI entry
        default_spindle_speed = self.get_ini_info.get_default_spindle_speed()
        self.spindle_start_rpm = self.prefs.getpref( 'spindle_start_rpm', default_spindle_speed, float )

        # if it's a lathe config, set the tooleditor style
        self.lathe_mode = self.get_ini_info.get_lathe()

        # get the values for the sliders
        default_jog_vel = self.get_ini_info.get_jog_vel()
        self.jog_rate_max = self.get_ini_info.get_max_jog_vel()
        self.spindle_override_max = self.get_ini_info.get_max_spindle_override()
        self.spindle_override_min = self.get_ini_info.get_min_spindle_override()
        self.feed_override_max = self.get_ini_info.get_max_feed_override()
        self.rapid_override_max = self.get_ini_info.get_max_rapid_override()
        self.dro_actual = self.get_ini_info.get_position_feedback_actual()

        # set the slider limits
        self.widgets.spc_jog_vel.set_property("min", 0)
        self.widgets.spc_jog_vel.set_property("max", self.jog_rate_max)
        self.widgets.spc_jog_vel.set_value(default_jog_vel)

        self.widgets.spc_spindle.set_property("min", self.spindle_override_min * 100)
        self.widgets.spc_spindle.set_property("max", self.spindle_override_max * 100)
        self.widgets.spc_spindle.set_value(100)

        self.widgets.spc_rapid.set_property("min", 0)
        self.widgets.spc_rapid.set_property("max", self.rapid_override_max * 100)
        self.widgets.spc_rapid.set_value(100)

        self.widgets.spc_feed.set_property("min", 0)
        self.widgets.spc_feed.set_property("max", self.feed_override_max * 100)
        self.widgets.spc_feed.set_value(100)

        # the scales to apply to the count of the hardware mpg wheel, to avoid to much turning
        default = (self.jog_rate_max / 100)
        self.scale_jog_vel = self.prefs.getpref("scale_jog_vel", default, float)
        self.widgets.adj_scale_jog_vel.set_value(self.scale_jog_vel)
        self.scale_spindle_override = self.prefs.getpref("scale_spindle_override", 1, float)
        self.widgets.adj_scale_spindle_override.set_value(self.scale_spindle_override)
        self.scale_feed_override = self.prefs.getpref("scale_feed_override", 1, float)
        self.widgets.adj_scale_feed_override.set_value(self.scale_feed_override)
        self.scale_rapid_override = self.prefs.getpref("scale_rapid_override", 1, float)
        self.widgets.adj_scale_rapid_override.set_value(self.scale_rapid_override)

        # holds the max velocity value and is needed to be able to jog at
        # at max velocity if <SHIFT> is hold during jogging
        self.max_velocity = self.stat.max_velocity

        # set and get all information for turtle jogging
        # self.rabbit_jog will be used in future to store the last value
        # so it can be recovered after jog_vel_mode switch
        self.rabbit_jog = default_jog_vel
        hide_turtle_jog_button = self.prefs.getpref("hide_turtle_jog_button", False, bool)
        self.widgets.chk_turtle_jog.set_active(hide_turtle_jog_button)
        self.turtle_jog_factor = self.prefs.getpref('turtle_jog_factor', 20, int)
        self.widgets.adj_turtle_jog_factor.configure(self.turtle_jog_factor, 1,
                                                     100, 1, 0, 0)
        if hide_turtle_jog_button:
            self.widgets.tbtn_turtle_jog.hide()
            self.turtle_jog_factor = 1
        self.turtle_jog = self.rabbit_jog / self.turtle_jog_factor

        # and according to machine units the digits to display
        if self.stat.linear_units == _MM:
            self.widgets.spc_jog_vel.set_digits(0)
            self.widgets.spc_jog_vel.set_property("unit", _("mm/min"))
        else:
            self.widgets.spc_jog_vel.set_digits(2)
            self.widgets.spc_jog_vel.set_property("unit", _("inch/min"))

        # the size of the DRO
        self.dro_size = self.prefs.getpref("dro_size", 28, int)
        self.widgets.adj_dro_size.set_value(self.dro_size)

    def _init_joints_btn(self):
        # if we have identity kinematics, we do not need to check for the joints button
        if self.stat.kinematics_type == linuxcnc.KINEMATICS_IDENTITY:
            return
        # how many joints do we have
        joints_count = self.stat.joints
        # hide all unneeded button, we do allow 8 joints (0 to 7)
        for joint in range(joints_count, 8):
            self.widgets["btn_j{0}_minus".format(joint)].hide()
            self.widgets["btn_j{0}_plus".format(joint)].hide()

            # and now the joint homing button
            # but only 6 joints are shown, so we leave here
            if joint == 7:
                continue
            self.widgets["btn_home_j{0}".format(joint)].hide()
            self.widgets["lbl_space_j{0}".format(joint)].show()
        if joints_count < 7:
            self.widgets.btn_sel_prev_joints.hide()
            self.widgets.btn_sel_next_joints.hide()
            self.widgets.lbl_space_j7.hide()
        if joints_count < 6:
            self.widgets.lbl_space_jall.show()
            self.widgets.lbl_space_j6.hide()
            self.widgets.lbl_space_sel_prev_joints.show()
            self.widgets.lbl_space_j5.hide()
        # if there are less joints, the above done should work correct

    def _init_jog_increments(self):
        # Now we will build the option buttons to select the Jog-rates
        # We do this dynamically, because users are able to set them in INI File
        # because of space on the screen only 10 items are allowed
        # jogging increments

        # We get the increments from INI File
        self.jog_increments = self.get_ini_info.get_increments()
        if len(self.jog_increments) > 10:
            print(_("**** GMOCCAPY INFO ****"))
            print(_("**** To many increments given in INI File for this screen ****"))
            print(_("**** Only the first 10 will be reachable through this screen ****"))
            # we shorten the incrementlist to 10 (first is default = 0)
            self.jog_increments = self.jog_increments[0:11]

        # The first radio button is created to get a radio button group
        # The group is called according the name off  the first button
        # We use the pressed signal, not the toggled, otherwise two signals will be emitted
        # One from the released button and one from the pressed button
        # we make a list of the buttons to later add the hardware pins to them
        label = _("Continuous")
        rbt0 = gtk.RadioButton(None, label)
        rbt0.connect("pressed", self.on_increment_changed, 0)
        self.widgets.vbtb_jog_incr.pack_start(rbt0, True, True, 0)
        rbt0.set_property("draw_indicator", False)
        rbt0.show()
        rbt0.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        rbt0.__name__ = "rbt0"
        self.incr_rbt_list.append(rbt0)
        # the rest of the buttons are now added to the group
        # self.no_increments is set while setting the hal pins with self._check_len_increments
        for item in range(1, len(self.jog_increments)):
            rbt = "rbt{0}".format(item)
            rbt = gtk.RadioButton(rbt0, self.jog_increments[item])
            rbt.connect("pressed", self.on_increment_changed, self.jog_increments[item])
            self.widgets.vbtb_jog_incr.pack_start(rbt, True, True, 0)
            rbt.set_property("draw_indicator", False)
            rbt.show()
            rbt.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
            rbt.__name__ = "rbt{0}".format(item)
            self.incr_rbt_list.append(rbt)
        self.active_increment = "rbt0"

    def _check_screen2(self):
        # second screen
        self.screen2 = False
        screen2 = os.path.join(CONFIGPATH, "gmoccapy2.glade")
        if os.path.exists(screen2):
            print (_("**** GMOCCAPY INFO ****"))
            print (_("**** gmoccapy screen 2 found ****"))
            try:
                self.builder.add_from_file(screen2)
                self.screen2 = True
            except Exception, e:
                print (_("**** GMOCCAPY ERROR ****"))
                print _("**** screen 2 GLADE ERROR: ****")
                self.widgets.tbtn_use_screen2.set_sensitive(False)
                traceback.print_exc()
        else:
            print (_("**** GMOCCAPY INFO ****"))
            print _("**** No gmoccapy2.glade file present ****")
            self.widgets.tbtn_use_screen2.set_sensitive(False)

# =============================================================
# Dynamic tabs handling Start

    def _init_dynamic_tabs(self):
        # dynamic tabs setup
        self._dynamic_childs = {}
        # register all tabs, so they will be closed together with the GUI
        atexit.register(self._kill_dynamic_childs)

        tab_names, tab_locations, tab_cmd = self.get_ini_info.get_embedded_tabs()
        if not tab_names:
            print (_("**** GMOCCAPY INFO ****"))
            print (_("**** Invalid embedded tab configuration ****"))
            print (_("**** No tabs will be added! ****"))
            return

        try:
            for t, c, name in zip(tab_names, tab_cmd, tab_locations):
                nb = self.widgets[name]
                xid = self._dynamic_tab(nb, t)
                if not xid: continue
                cmd = c.replace('{XID}', str(xid))
                child = subprocess.Popen(cmd.split())
                self._dynamic_childs[xid] = child
                nb.show_all()
        except:
            print(_("ERROR, trying to initialize the user tabs or panels, check for typos"))
        self.set_up_user_tab_widgets(tab_locations)

    # adds the embedded object to a notebook tab or box
    def _dynamic_tab(self, widget, text):
        s = gtk.Socket()
        try:
            widget.append_page(s, gtk.Label(" " + text + " "))
        except:
            try:
                widget.pack_end(s, True, True, 0)
            except:
                return None
        return s.get_id()

    # Gotta kill the embedded processes when gmoccapy closes
    def _kill_dynamic_childs(self):
        for child in self._dynamic_childs.values():
            child.terminate()

    def set_up_user_tab_widgets(self, tab_locations):
        print(tab_locations)
        if tab_locations:
            # make sure the user tabs button is disabled
            # if no ntb_user_tabs in location is given
            if "ntb_user_tabs" in tab_locations:
                self.widgets.tbtn_user_tabs.set_sensitive( True )
            else:
                self.widgets.tbtn_user_tabs.set_sensitive( False )

            if "ntb_preview" in tab_locations:
                self.widgets.ntb_preview.set_property( "show-tabs", True )

            # This is normaly only used for the plasma screen layout
            if "box_coolant_and_spindle" in tab_locations:
                widgetlist = ["box_spindle", "box_cooling"]
                for widget in widgetlist:
                    self.widgets[widget].hide()

            if "box_cooling" in tab_locations:
                widgetlist = ["frm_cooling"]
                for widget in widgetlist:
                    self.widgets[widget].hide()

            if "box_spindle" in tab_locations:
                widgetlist = ["frm_spindle"]
                for widget in widgetlist:
                    self.widgets[widget].hide()

            if "box_vel_info" in tab_locations:
                widgetlist = ["frm_max_vel", "frm_feed_override"]
                for widget in widgetlist:
                    self.widgets[widget].hide()

            if "box_custom_1" in tab_locations:
                self.widgets.box_custom_1.show()

            if "box_custom_2" in tab_locations:
                self.widgets.box_custom_2.show()

            if "box_custom_3" in tab_locations:
                self.widgets.box_custom_3.show()

            if "box_custom_4" in tab_locations:
                self.widgets.box_custom_4.show()

            if "box_tool_and_code_info" in tab_locations:
                widgetlist = ["frm_tool_info", "active_speed_label", "lbl_speed", "box_vel_info"]
                for widget in widgetlist:
                    self.widgets[widget].hide()
                self.widgets.btn_tool.set_sensitive( False )

# Dynamic tabs handling End
# =============================================================

    # and we load the tooltable data
    def _init_tooleditor(self):

       # get the path to the tool table
        tooltable = self.get_ini_info.get_toolfile()
        if not tooltable:
            message = _("**** GMOCCAPY ERROR ****\n")
            message += _("**** Did not find a toolfile file in [EMCIO] TOOL_TABLE ****")
            print(message)
            self.dialogs.warning_dialog(self, _("Very critical situation"), message, sound = False)
            sys.exit()
        toolfile = os.path.join(CONFIGPATH, tooltable)
        self.widgets.tooledit1.set_filename(toolfile)

        # first we hide all the axis columns the unhide the ones we want
        self.widgets.tooledit1.set_visible("abcxyzuvwijq", False)
        for axis in self.axis_list:
            self.widgets.tooledit1.set_visible("{0}".format(axis), True)

        # if it's a lathe config we show lathe related columns
        if self.lathe_mode:
            self.widgets.tooledit1.set_visible("ijq", True)
            if not self.get_ini_info.get_lathe_wear_offsets():
                # hide the wear offset tabs
                self.widgets.tooledit1.set_lathe_display(False)

        self.widgets.tooledit1.hide_buttonbox(True)

    def _init_themes(self):
        # If there are themes then add them to combo box
        model = self.widgets.theme_choice.get_model()
        model.clear()
        model.append((_("Follow System Theme"),))
        themes = []
        if os.path.exists(USERTHEMEDIR):
            names = os.listdir(USERTHEMEDIR)
            names.sort()
            for dirs in names:
                try:
                    sbdirs = os.listdir(os.path.join(USERTHEMEDIR, dirs))
                    if 'gtk-2.0' in sbdirs:
                        themes.append(dirs)
                except:
                    pass
        if os.path.exists(THEMEDIR):
            names = os.listdir(THEMEDIR)
            names.sort()
            for dirs in names:
                try:
                    sbdirs = os.listdir(os.path.join(THEMEDIR, dirs))
                    if 'gtk-2.0' in sbdirs:
                        themes.append(dirs)
                except:
                    pass
        temp = 0
        theme_name = self.prefs.getpref("gtk_theme", "Follow System Theme", str)
        for index, theme in enumerate(themes):
            model.append((theme,))
            if theme == theme_name:
                temp = index + 1
        self.widgets.theme_choice.set_active(temp)
        settings = gtk.settings_get_default()
        if not theme_name == "Follow System Theme":
            settings.set_string_property("gtk-theme-name", theme_name, "")

    def _init_audio(self):
        # try to add ability for audio feedback to user.
        if _AUDIO_AVAILABLE:
            print (_("**** GMOCCAPY INFO ****"))
            print (_("**** audio available! ****"))

            # the sounds to play if an error or message rises
            self.alert_sound = "/usr/share/sounds/freedesktop/stereo/dialog-warning.oga"
            self.error_sound = "/usr/share/sounds/freedesktop/stereo/dialog-error.oga"

            self.audio = player.Player()
            self.alert_sound = self.prefs.getpref('audio_alert', self.alert_sound, str)
            self.error_sound = self.prefs.getpref('audio_error', self.error_sound, str)
            self.widgets.audio_alert_chooser.set_filename(self.alert_sound)
            self.widgets.audio_error_chooser.set_filename(self.error_sound)
        else:
            print (_("**** GMOCCAPY INFO ****"))
            print (_("**** no audio available! ****"))
            print(_("**** PYGST libray not installed? ****"))
            print(_("**** is python-gstX.XX installed? ****"))

            self.widgets.audio_alert_chooser.set_sensitive(False)
            self.widgets.audio_error_chooser.set_sensitive(False)

    # init the preview
    def _init_gremlin( self ):
        grid_size = self.prefs.getpref( 'grid_size', 1.0, float )
        self.widgets.grid_size.set_value( grid_size )
        self.widgets.gremlin.grid_size = grid_size
        view = self.prefs.getpref( 'view', "p", str )
        self.widgets.gremlin.set_property( "view", view )
        self.widgets.gremlin.set_property( "metric_units", int( self.stat.linear_units ) )
        self.widgets.gremlin.set_property( "mouse_btn_mode", self.prefs.getpref( "mouse_btn_mode", 4, int ) )
        self.widgets.gremlin.set_property( "use_commanded", not self.dro_actual)
        self.widgets.eb_program_label.modify_bg(gtk.STATE_NORMAL, gtk.gdk.Color(0, 0, 0))
        self.widgets.eb_blockheight_label.modify_bg(gtk.STATE_NORMAL, gtk.gdk.Color(0, 0, 0))

    # init the preview
    def _init_hardware_button_order( self ):
        # This is needed only because we connect all the horizontal button
        # to hal pins, so the user can connect them to hardware buttons
        self.h_tabs = []
        tab_main = [(0, "btn_homing"), (1, "btn_touch"), (3, "btn_tool"),
                    (8, "tbtn_fullsize_preview"), (9, "btn_exit")
        ]
        self.h_tabs.append(tab_main)

        tab_mdi = [(9, "btn_show_kbd")]
        self.h_tabs.append(tab_mdi)

        tab_auto = [(0, "btn_load"), (1, "btn_run"), (2, "btn_stop"), (3, "tbtn_pause"),
                    (4, "btn_step"), (5, "btn_from_line"), (6, "tbtn_optional_blocks"),
                    (7, "tbtn_optional_stops"), (8, "tbtn_fullsize_preview1"), (9, "btn_edit")
        ]
        self.h_tabs.append(tab_auto)

        tab_ref = [(1, "btn_home_all"), (3, "btn_home_x"),
                   (5, "btn_home_z"), (8, "btn_unhome_all"), (9, "btn_back_ref")
        ]
        if not self.lathe_mode:
            tab_ref.append((4, "btn_home_y"))
        if len(self.axis_list) == 4:
            tab_ref.append((6, "btn_home_4"))
        if len(self.axis_list) == 5:
            tab_ref.append((6, "btn_home_4"))
            tab_ref.append((7, "btn_home_5"))
        self.h_tabs.append(tab_ref)

        tab_touch = [(0, "tbtn_edit_offsets"), (1, "btn_set_value_x"), (3, "btn_set_value_z"), (6, "btn_zero_g92"),
                     (8, "btn_set_selected"), (9, "btn_back_zero")
        ]
        if not self.lathe_mode:
            tab_touch.append((2, "btn_set_value_y"))

        if len(self.axis_list) == 4:
            tab_touch.append((4, "btn_set_value_4"))
        if len(self.axis_list) == 5:
            tab_touch.append((4, "btn_set_value_4"))
            tab_touch.append((5, "btn_set_value_5"))

        self.h_tabs.append(tab_touch)
        
        tab_setup = [(0, "btn_delete"), (4, "btn_classicladder"), (5, "btn_hal_scope"), (6, "btn_status"),
                     (7, "btn_hal_meter"), (8, "btn_calibration"), (9, "btn_show_hal")
        ]
        self.h_tabs.append(tab_setup)

        tab_edit = [(0, "btn_open_edit"), (2, "btn_save"), (3, "btn_save_as"), (4, "btn_save_and_run"),
                    (6, "btn_new"), (8, "btn_keyb"), (9, "btn_back_edit")
        ]
        self.h_tabs.append(tab_edit)

        tab_tool = [(0, "btn_delete_tool"), (1, "btn_add_tool"), (2, "btn_reload_tooltable"),
                    (3, "btn_apply_tool_changes"), (4, "btn_select_tool_by_no"), (5, "btn_index_tool"),
                    (6, "btn_change_tool"), (8, "btn_tool_touchoff_z"), (9, "btn_back_tool")
        ]
        if self.lathe_mode:
            tab_tool.append((7, "btn_tool_touchoff_x"))
        self.h_tabs.append(tab_tool)

        tab_file = [(0, "btn_home"), (1, "btn_dir_up"), (3, "btn_sel_prev"), (4, "btn_sel_next"),
                    (5, "btn_jump_to"), (7, "btn_select"), (9, "btn_back_file_load")
        ]
        self.h_tabs.append(tab_file)
        
        tab_ref_joints = [(0, "btn_home_j_all"), (1, "btn_home_j0"), (2, "btn_home_j1"), (3, "btn_home_j2"), (4, "btn_home_j3"),
                   (5, "btn_home_j4"), (6, "btn_home_j5"), (7, "btn_sel_next_joints"), (8, "btn_unhome_j_all"), (9, "btn_back_joints")
        ]
        self.h_tabs.append(tab_ref_joints)
        

        self.v_tabs = [(0, "tbtn_estop"), (1, "tbtn_on"), (2, "rbt_manual"), (3, "rbt_mdi"),
                       (4, "rbt_auto"), (5, "tbtn_setup"), (6, "tbtn_user_tabs")
        ]

    def _init_kinematics_type (self):
        if self.stat.kinematics_type != linuxcnc.KINEMATICS_IDENTITY:
            self.widgets.gremlin.set_property( "enable_dro", True )
            self.widgets.gremlin.use_joints_mode = True
            self.widgets.tbtn_switch_mode.show()
            self.widgets.tbtn_switch_mode.set_label(_(" Joint\nmode"))
            self.widgets.tbtn_switch_mode.set_sensitive(False)
            self.widgets.tbtn_switch_mode.set_active(True)
            self.widgets.lbl_replace_mode_btn.hide()
            self.widgets.ntb_jog_JA.set_page(1)
            self.h_tabs[_BB_MANUAL].append((6, "btn_tool"))
            page9 = self.widgets.ntb_button.get_nth_page(9)
            self.widgets.ntb_button.reorder_child(page9, _BB_HOME)
            page4 = self.widgets.ntb_button.get_nth_page(4)
            self.widgets.ntb_button.reorder_child(page4, -1)
            self._reset_joint_button_order_to_default()        
        else:
            self.widgets.gremlin.set_property( "enable_dro", False )
            self.widgets.gremlin.use_joints_mode = False
            self.widgets.tbtn_switch_mode.hide()
            self.widgets.lbl_replace_mode_btn.show()
            self.widgets.ntb_jog_JA.set_page(0)

    # init the function to hide the cursor
    def _init_hide_cursor(self):
        hide_cursor = self.prefs.getpref('hide_cursor', False, bool)
        self.widgets.chk_hide_cursor.set_active(hide_cursor)
        # if hide cursor requested
        # we set the graphics to use touchscreen controls
        if hide_cursor:
            self.widgets.window1.window.set_cursor(INVISABLE)
            self.widgets.gremlin.set_property("use_default_controls", False)
        else:
            self.widgets.window1.window.set_cursor(None)
            self.widgets.gremlin.set_property("use_default_controls", True)

# =============================================================
# Onboard keybord handling Start

    # shows "Onboard" virtual keyboard if available
    # else error message
    def _init_keyboard(self, args="", x="", y=""):
        self.onboard = False

        # now we check if onboard or matchbox-keyboard is installed
        try:
            if os.path.isfile("/usr/bin/onboard"):
                self.onboard_kb = subprocess.Popen(["onboard", "--xid", args, x, y],
                                                   stdin=subprocess.PIPE,
                                                   stdout=subprocess.PIPE,
                                                   close_fds=True)
                print (_("**** GMOCCAPY INFO ****"))
                print (_("**** virtual keyboard program found : <onboard>"))
            elif os.path.isfile("/usr/bin/matchbox-keyboard"):
                self.onboard_kb = subprocess.Popen(["matchbox-keyboard", "--xid"],
                                                   stdin=subprocess.PIPE,
                                                   stdout=subprocess.PIPE,
                                                   close_fds=True)
                print (_("**** GMOCCAPY INFO ****"))
                print (_("**** virtual keyboard program found : <matchbox-keyboard>"))
            else:
                print (_("**** GMOCCAPY INFO ****"))
                print (_("**** No virtual keyboard installed, we checked for <onboard> and <matchbox-keyboard>."))
                self._no_virt_keyboard()
                return
            sid = self.onboard_kb.stdout.readline()
            socket = gtk.Socket()
            self.widgets.key_box.add(socket)
            socket.add_id(long(sid))
            socket.show()
            self.onboard = True
        except Exception, e:
            print (_("**** GMOCCAPY ERROR ****"))
            print (_("**** Error with launching virtual keyboard,"))
            print (_("**** is onboard or matchbox-keyboard installed? ****"))
            traceback.print_exc()
            self._no_virt_keyboard()

        # get when the keyboard should be shown
        # and set the corresponding button active
        # only if onbaoard keyboard is ok.
        if self.onboard:
            self.widgets.chk_use_kb_on_offset.set_active(self.prefs.getpref("show_keyboard_on_offset",
                                                                            False, bool))
            self.widgets.chk_use_kb_on_tooledit.set_active(self.prefs.getpref("show_keyboard_on_tooledit",
                                                                              False, bool))
            self.widgets.chk_use_kb_on_edit.set_active(self.prefs.getpref("show_keyboard_on_edit",
                                                                          True, bool))
            self.widgets.chk_use_kb_on_mdi.set_active(self.prefs.getpref("show_keyboard_on_mdi",
                                                                         True, bool))
            self.widgets.chk_use_kb_on_file_selection.set_active(self.prefs.getpref("show_keyboard_on_file_selection",
                                                                                    False, bool))
        else:
            self.widgets.chk_use_kb_on_offset.set_active(False)
            self.widgets.chk_use_kb_on_tooledit.set_active(False)
            self.widgets.chk_use_kb_on_edit.set_active(False)
            self.widgets.chk_use_kb_on_mdi.set_active(False)
            self.widgets.chk_use_kb_on_file_selection.set_active(False)
            self.widgets.frm_keyboard.set_sensitive(False) 

    def _no_virt_keyboard(self):
        # In this case we will disable the corresponding part on the settings page
        self.widgets.chk_use_kb_on_offset.set_active(False)
        self.widgets.chk_use_kb_on_tooledit.set_active(False)
        self.widgets.chk_use_kb_on_edit.set_active(False)
        self.widgets.chk_use_kb_on_mdi.set_active(False)
        self.widgets.chk_use_kb_on_file_selection.set_active(False)
        self.widgets.frm_keyboard.set_sensitive(False)
        self.widgets.btn_show_kbd.set_sensitive(False)
        self.widgets.btn_show_kbd.set_image(self.widgets.img_brake_macro)
        self.widgets.btn_show_kbd.set_property("tooltip-text", _("interrupt running macro"))
        self.widgets.btn_keyb.set_sensitive(False)

    def _kill_keyboard(self):
        try:
            self.onboard_kb.kill()
            self.onboard_kb.terminate()
            self.onboard_kb = None
        except:
            try:
                self.onboard_kb.kill()
                self.onboard_kb.terminate()
                self.onboard_kb = None
            except:
                pass

# Onboard keybord handling End
# =============================================================

    def _init_offsetpage(self):
        temp = "xyzabcuvw"
        self.widgets.offsetpage1.set_col_visible(temp, False)
        temp = ""
        for axis in self.axis_list:
            temp = temp + axis
        self.widgets.offsetpage1.set_col_visible(temp, True)

        parameterfile = self.get_ini_info.get_parameter_file()
        if not parameterfile:
            message = _("**** GMOCCAPY ERROR ****\n")
            message += _("**** Did not find a parameter file in [RS274NGC] PARAMETER_FILE ****")
            print(message)
            self.dialogs.warning_dialog(self, _("Very critical situation"), message, sound = False)
            sys.exit()
        path = os.path.join(CONFIGPATH, parameterfile)
        self.widgets.offsetpage1.set_filename(path)

        self.widgets.offsetpage1.set_display_follows_program_units()
        if self.stat.program_units != 1:
            self.widgets.offsetpage1.set_to_mm()
            self.widgets.offsetpage1.machine_units_mm = _MM
        else:
            self.widgets.offsetpage1.set_to_inch()
            self.widgets.offsetpage1.machine_units_mm = _INCH
        self.widgets.offsetpage1.hide_buttonbox(True)
        self.widgets.offsetpage1.set_row_visible("1", False)
        self.widgets.offsetpage1.set_font("sans 12")
        self.widgets.offsetpage1.set_foreground_color("#28D0D9")
        self.widgets.offsetpage1.selection_mask = ("Tool", "G5x", "Rot")
        systemlist = ["Tool", "G5x", "Rot", "G92", "G54", "G55", "G56", "G57", "G58", "G59", "G59.1",
                      "G59.2", "G59.3"]
        names = []
        for system in systemlist:
            system_name = "system_name_{0}".format(system)
            name = self.prefs.getpref(system_name, system, str)
            names.append([system, name])
        self.widgets.offsetpage1.set_names(names)

    # Icon file selection stuff
    def _init_IconFileSelection(self):
        self.widgets.IconFileSelection1.set_property("start_dir", self.get_ini_info.get_program_prefix())

        file_ext = self.get_ini_info.get_file_ext()
        filetypes = ""
        for ext in file_ext:
            filetypes += ext.replace("*.", "") + ","
        self.widgets.IconFileSelection1.set_property("filetypes", filetypes)

        jump_to_dir = self.prefs.getpref("jump_to_dir", os.path.expanduser("~"), str)
        self.widgets.jump_to_dir_chooser.set_current_folder(jump_to_dir)
        self.widgets.IconFileSelection1.set_property("jump_to_dir", jump_to_dir)

        self.widgets.IconFileSelection1.show_buttonbox(False)
        self.widgets.IconFileSelection1.show_filelabel(False)
        
        # now we initialize the button states
        self.widgets.btn_home.set_sensitive(self.widgets.IconFileSelection1.btn_home.get_sensitive())
        self.widgets.btn_dir_up.set_sensitive(self.widgets.IconFileSelection1.btn_dir_up.get_sensitive())
        self.widgets.btn_sel_prev.set_sensitive(self.widgets.IconFileSelection1.btn_sel_prev.get_sensitive())
        self.widgets.btn_sel_next.set_sensitive(self.widgets.IconFileSelection1.btn_sel_next.get_sensitive())
        self.widgets.btn_select.set_sensitive(self.widgets.IconFileSelection1.btn_select.get_sensitive())
        self.widgets.btn_jump_to.set_sensitive(self.widgets.IconFileSelection1.btn_jump_to.get_sensitive())
        self.widgets.btn_jump_to.set_sensitive(self.widgets.IconFileSelection1.btn_jump_to.get_sensitive())

    # init the keyboard shortcut bindings
    def _init_keybindings(self):
        try:
            accel_group = gtk.AccelGroup()
            self.widgets.window1.add_accel_group(accel_group)
            self.widgets.button_estop.add_accelerator("clicked", accel_group, 65307, 0, gtk.ACCEL_LOCKED)
        except:
            pass
        self.widgets.window1.connect("key_press_event", self.on_key_event, 1)
        self.widgets.window1.connect("key_release_event", self.on_key_event, 0)

    # Initialize the file to load dialog, setting an title and the correct
    # folder as well as a file filter
    def _init_file_to_load(self):
        file_dir = self.get_ini_info.get_program_prefix()
        self.widgets.file_to_load_chooser.set_current_folder(file_dir)
        title = _("Select the file you want to be loaded at program start")
        self.widgets.file_to_load_chooser.set_title(title)
        self.widgets.ff_file_to_load.set_name("linuxcnc files")
        self.widgets.ff_file_to_load.add_pattern("*.ngc")
        file_ext = self.get_ini_info.get_file_ext()
        for ext in file_ext:
            self.widgets.ff_file_to_load.add_pattern(ext)

    # search for and set up user requested message system.
    # status displays on the statusbat and requires no acknowledge.
    # dialog displays a GTK dialog box with yes or no buttons
    # okdialog displays a GTK dialog box with an ok button
    # dialogs require an answer before focus is sent back to main screen
    def _init_user_messages(self):
        user_messages = self.get_ini_info.get_user_messages()
        if not user_messages:
            return
        for message in user_messages:
            if message[1] == "status":
                pin = hal_glib.GPin(self.halcomp.newpin("messages." + message[2], hal.HAL_BIT, hal.HAL_IN))
                pin.connect("value_changed", self._show_user_message, message)
            elif message[1] == "okdialog":
                pin = hal_glib.GPin(self.halcomp.newpin("messages." + message[2], hal.HAL_BIT, hal.HAL_IN))
                pin.connect("value_changed", self._show_user_message, message)
                pin = hal_glib.GPin(
                    self.halcomp.newpin("messages." + message[2] + "-waiting", hal.HAL_BIT, hal.HAL_OUT))
            elif message[1] == "yesnodialog":
                pin = hal_glib.GPin(self.halcomp.newpin("messages." + message[2], hal.HAL_BIT, hal.HAL_IN))
                pin.connect("value_changed", self._show_user_message, message)
                pin = hal_glib.GPin(
                    self.halcomp.newpin("messages." + message[2] + "-waiting", hal.HAL_BIT, hal.HAL_OUT))
                pin = hal_glib.GPin(
                    self.halcomp.newpin("messages." + message[2] + "-response", hal.HAL_BIT, hal.HAL_OUT))
            else:
                print(_("**** GMOCCAPY ERROR **** /n Message type {0} not supported").format(message[1]))

    def _show_user_message(self, pin, message):
        if message[1] == "status":
            if pin.get():
                self._show_error((0, message[0]))
        elif message[1] == "okdialog":
            self.halcomp["messages." + message[2] + "-waiting"] = 0
            if pin.get():
                self.halcomp["messages." + message[2] + "-waiting"] = 1
                title = "Pin " + message[2] + " message"
                responce = self.dialogs.show_user_message(self, message[0], title)
                self.halcomp["messages." + message[2] + "-waiting"] = 0
        elif message[1] == "yesnodialog":
            if pin.get():
                self.halcomp["messages." + message[2] + "-waiting"] = 1
                self.halcomp["messages." + message[2] + "-response"] = 0
                title = "Pin " + message[2] + " message"
                responce = self.dialogs.yesno_dialog(self, message[0], title)
                self.halcomp["messages." + message[2] + "-waiting"] = 0
                self.halcomp["messages." + message[2] + "-response"] = responce
            else:
                self.halcomp["messages." + message[2] + "-waiting"] = 0
        else:
            print(_("**** GMOCCAPY ERROR **** /n Message type {0} not supported").format(message[1]))

    def _show_offset_tab(self, state):
        page = self.widgets.ntb_preview.get_nth_page(1)
        if page.get_visible() and state or not page.get_visible() and not state:
            return
        if state:
            page.show()
            self.widgets.ntb_preview.set_property("show-tabs", state)
            self.widgets.ntb_preview.set_current_page(1)
            self.widgets.offsetpage1.mark_active((self.system_list[self.stat.g5x_index]).lower())
            if self.widgets.chk_use_kb_on_offset.get_active():
                self.widgets.ntb_info.set_current_page(1)
        else:
            names = self.widgets.offsetpage1.get_names()
            for system, name in names:
                system_name = "system_name_{0}".format(system)
                self.prefs.putpref(system_name, name)
            page.hide()
            self.widgets.tbtn_edit_offsets.set_active(False)
            self.widgets.ntb_preview.set_current_page(0)
            self.widgets.ntb_info.set_current_page(0)
            if self.widgets.ntb_preview.get_n_pages() <= 4:  # else user tabs are available
                self.widgets.ntb_preview.set_property("show-tabs", state)

    def _show_tooledit_tab(self, state):
        page = self.widgets.ntb_preview.get_nth_page(2)
        if page.get_visible() and state or not page.get_visible() and not state:
            return
        if state:
            page.show()
            self.widgets.ntb_preview.set_property("show-tabs", not state)
            self.widgets.vbx_jog.hide()
            self.widgets.ntb_preview.set_current_page(2)
            self.widgets.tooledit1.set_selected_tool(self.stat.tool_in_spindle)
            if self.widgets.chk_use_kb_on_tooledit.get_active():
                self.widgets.ntb_info.set_current_page(1)
        else:
            page.hide()
            if self.widgets.ntb_preview.get_n_pages() > 4:  # user tabs are available
                self.widgets.ntb_preview.set_property("show-tabs", not state)
            self.widgets.vbx_jog.show()
            self.widgets.ntb_preview.set_current_page(0)
            self.widgets.ntb_info.set_current_page(0)

    def _show_iconview_tab(self, state):
        page = self.widgets.ntb_preview.get_nth_page(3)
        if page.get_visible() and state or not page.get_visible() and not state:
            return
        if state:
            page.show()
            self.widgets.ntb_preview.set_property("show-tabs", not state)
            self.widgets.ntb_preview.set_current_page(3)
            if self.widgets.chk_use_kb_on_file_selection.get_active():
                self.widgets.box_info.show()
                self.widgets.ntb_info.set_current_page(1)
        else:
            page.hide()
            if self.widgets.ntb_preview.get_n_pages() > 4:  # user tabs are available
                self.widgets.ntb_preview.set_property("show-tabs", not state)
            self.widgets.ntb_preview.set_current_page(0)
            self.widgets.ntb_info.set_current_page(0)

    # every 100 milli seconds this gets called
    # check linuxcnc for status, error and then update the readout
    def _periodic(self):
        # we put the poll comand in a try, so if the linuxcnc pid is killed
        # from an external command, we also quit the GUI
        try:
            self.stat.poll()
        except:
            raise SystemExit, "gmoccapy can not poll linuxcnc status any more"

        error = self.error_channel.poll()
        if error:
            self._show_error(error)

        if self.gcodes != self.stat.gcodes:
            self._update_active_gcodes()
        if self.mcodes != self.stat.mcodes:
            self._update_active_mcodes()

        if self.lathe_mode:
            if "G8" in self.active_gcodes and self.diameter_mode:
                self._switch_to_g7(False)
            elif "G7" in self.active_gcodes and not self.diameter_mode:
                self._switch_to_g7(True)

        self._update_vel()
        self._update_coolant()
        self._update_spindle()
        self._update_halui_pin()
        self._update_vc()

        self.widgets.lbl_time.set_label(strftime("%H:%M:%S") + "\n" + strftime("%d.%m.%Y"))

        # keep the timer running
        return True

    def _show_error(self, error):
        kind, text = error
        # print kind,text
        if kind in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
            icon = ALERT_ICON
            self.halcomp["error"] = True
        elif kind in (linuxcnc.NML_TEXT, linuxcnc.OPERATOR_TEXT):
            icon = INFO_ICON
        elif kind in (linuxcnc.NML_DISPLAY, linuxcnc.OPERATOR_DISPLAY):
            icon = INFO_ICON
        else:
            icon = ALERT_ICON
        if text == "" or text == None:
            text = _("Unknown error type and no error text given")
        self.notification.add_message(text, icon)

        if _AUDIO_AVAILABLE:
            if kind == 1 or kind == 11:
                self._on_play_sound(None, "error")
            else:
                self._on_play_sound(None, "alert")

    def on_gremlin_gcode_error(self, widget, errortext):
        if self.gcodeerror == errortext:
            return
        else:
            self.gcodeerror = errortext
            self.dialogs.warning_dialog(self, _("Important Warning"), errortext)


# =========================================================
# button handlers Start

    # toggle emergency button
    def on_tbtn_estop_toggled(self, widget, data=None):
        if widget.get_active():  # estop is active, open circuit
            self.command.state(linuxcnc.STATE_ESTOP)
            self.command.wait_complete()
            self.stat.poll()
            if self.stat.task_state == linuxcnc.STATE_ESTOP_RESET:
                widget.set_active(False)
        else:  # estop circuit is fine
            self.command.state(linuxcnc.STATE_ESTOP_RESET)
            self.command.wait_complete()
            self.stat.poll()
            if self.stat.task_state == linuxcnc.STATE_ESTOP:
                widget.set_active(True)
                self._show_error((11, _("ERROR : External ESTOP is set, could not change state!")))

    # toggle machine on / off button
    def on_tbtn_on_toggled(self, widget, data=None):
        if widget.get_active():
            if self.stat.task_state == linuxcnc.STATE_ESTOP:
                widget.set_active(False)
                return
            self.command.state(linuxcnc.STATE_ON)
            self.command.wait_complete()
            self.stat.poll()
            if self.stat.task_state != linuxcnc.STATE_ON:
                widget.set_active(False)
                self._show_error((11, _("ERROR : Could not switch the machine on, is limit switch activated?")))
                self._update_widgets(False)
                return
            self._update_widgets(True)
        else:
            self.command.state(linuxcnc.STATE_OFF)
            self._update_widgets(False)

    # The mode buttons
    def on_rbt_manual_pressed(self, widget, data=None):
        self.command.mode(linuxcnc.MODE_MANUAL)
        self.command.wait_complete()

    def on_rbt_mdi_pressed(self, widget, data=None):
        self.command.mode(linuxcnc.MODE_MDI)
        self.command.wait_complete()

    def on_rbt_auto_pressed(self, widget, data=None):
        self.command.mode(linuxcnc.MODE_AUTO)
        self.command.wait_complete()

    # If button exit is clicked, press emergency button before closing the application
    def on_btn_exit_clicked(self, widget, data=None):
        self.widgets.window1.destroy()

# button handlers End
# =========================================================

# =========================================================
# hal status Start

    # use the hal_status widget to control buttons and
    # actions allowed by the user and sensitive widgets
    def on_hal_status_all_homed(self, widget):
        self.all_homed = True
        self.widgets.ntb_button.set_current_page(_BB_MANUAL)
        widgetlist = ["rbt_mdi", "rbt_auto", "btn_index_tool", "btn_change_tool", "btn_select_tool_by_no",
                      "btn_tool_touchoff_x", "btn_tool_touchoff_z", "btn_touch", "tbtn_switch_mode"
        ]
        self._sensitize_widgets(widgetlist, True)
        self.set_motion_mode(1)
        if self.widgets.chk_reload_tool.get_active():
            # if there is already a tool in spindle, the user 
            # homed the second time, unfortunately we will then
            # not get out of MDI mode any more
            # That happen, because the tool in spindle did not change, so the 
            # tool info is not updated and we self.change_tool will not be reseted
            if self.stat.tool_in_spindle != 0:
                return
            self.reload_tool()
            self.command.mode(linuxcnc.MODE_MANUAL)

    def on_hal_status_not_all_homed(self, widget, joints):
        self.all_homed = False
        if self.no_force_homing:
            return
        widgetlist = ["rbt_mdi", "rbt_auto", "btn_index_tool", "btn_touch", "btn_change_tool", "btn_select_tool_by_no",
                      "btn_tool_touchoff_x", "btn_tool_touchoff_z", "btn_touch", "tbtn_switch_mode"
        ]
        self._sensitize_widgets(widgetlist, False)
        self.set_motion_mode(0)
        
    def on_hal_status_file_loaded(self, widget, filename):
        widgetlist = ["btn_use_current" ]
        # this test is only necessary, because of remap and toolchange, it will emit a file loaded signal
        if filename:
            fileobject = file(filename, 'r')
            lines = fileobject.readlines()
            fileobject.close()
            self.halcomp["program.length"] = len(lines)

            if len(filename) > 70:
                filename = filename[0:10] + "..." + filename[len(filename) - 50:len(filename)]
            self.widgets.lbl_program.set_text(filename)
            self._sensitize_widgets(widgetlist, True)
        else:
            self.halcomp["program.length"] = 0
            self._sensitize_widgets(widgetlist, False)
            self.widgets.lbl_program.set_text(_("No file loaded"))

    def on_hal_status_line_changed(self, widget, line):
        self.halcomp["program.current-line"] = line
        # this test is only necessary, because of remap and toolchange, it will emit a file loaded signal
        if self.halcomp["program.length"] > 0:
            self.halcomp["program.progress"] = 100.00 * line / self.halcomp["program.length"]
        else:
            self.halcomp["program.progress"] = 0.0
            # print("Progress = {0:.2f} %".format(100.00 * line / self.halcomp["program.length"]))

    def on_hal_status_interp_idle(self, widget):
        print("IDLE")
        if self.load_tool:
            return
        widgetlist = ["rbt_manual", "ntb_jog", "btn_from_line",
                      "tbtn_flood", "tbtn_mist", "rbt_forward", "rbt_reverse", "rbt_stop",
                      "btn_load", "btn_edit", "tbtn_optional_blocks"
        ]
        if not self.widgets.rbt_hal_unlock.get_active() and not self.user_mode:
            widgetlist.append("tbtn_setup")
        if self.all_homed or self.no_force_homing:
            widgetlist.append("rbt_mdi")
            widgetlist.append("rbt_auto")
            widgetlist.append("btn_index_tool")
            widgetlist.append("btn_change_tool")
            widgetlist.append("btn_select_tool_by_no")
            widgetlist.append("btn_tool_touchoff_x")
            widgetlist.append("btn_tool_touchoff_z")
            widgetlist.append("btn_touch")

        # This happen because hal_glib does emmit the signals in the order that idle is emited later that estop
        if self.stat.task_state == linuxcnc.STATE_ESTOP or self.stat.task_state == linuxcnc.STATE_OFF:
            self._sensitize_widgets(widgetlist, False)
        else:
            self._sensitize_widgets(widgetlist, True)
        for btn in self.macrobuttons:
            btn.set_sensitive(True)
        if self.onboard:
            self.widgets.btn_show_kbd.set_image(self.widgets.img_keyboard)
        else:
            self.widgets.btn_show_kbd.set_image(self.widgets.img_brake_macro)
        self.widgets.btn_run.set_sensitive(True)

        if self.tool_change:
            self.command.mode(linuxcnc.MODE_MANUAL)
            self.command.wait_complete()
            self.tool_change = False

        self.halcomp["program.current-line"] = 0
        self.halcomp["program.progress"] = 0.0

    def on_hal_status_interp_run(self, widget):
        print("RUN")

        widgetlist = ["rbt_manual", "rbt_mdi", "rbt_auto", "tbtn_setup", "btn_index_tool",
                      "btn_from_line", "btn_change_tool", "btn_select_tool_by_no",
                      "btn_load", "btn_edit", "tbtn_optional_blocks", "rbt_reverse", "rbt_stop", "rbt_forward",
                      "btn_tool_touchoff_x", "btn_tool_touchoff_z", "btn_touch"
        ]
        # in MDI it should be possible to add more commands, even if the interpreter is running
        if self.stat.task_mode != linuxcnc.MODE_MDI:
            widgetlist.append("ntb_jog")

        self._sensitize_widgets(widgetlist, False)
        self.widgets.btn_run.set_sensitive(False)

        self.widgets.btn_show_kbd.set_image(self.widgets.img_brake_macro)
        self.widgets.btn_show_kbd.set_property("tooltip-text", _("interrupt running macro"))

    def on_hal_status_tool_in_spindle_changed(self, object, new_tool_no):
        # need to save the tool in spindle as preference, to be able to reload it on startup
        self.prefs.putpref("tool_in_spindle", new_tool_no, int)
        self._update_toolinfo(new_tool_no)

    def on_hal_status_state_estop(self, widget=None):
        self.widgets.tbtn_estop.set_active(True)
        self.widgets.tbtn_estop.set_image(self.widgets.img_emergency)
        self.widgets.tbtn_on.set_image(self.widgets.img_machine_on)
        self.widgets.tbtn_on.set_sensitive(False)
        self.widgets.chk_ignore_limits.set_sensitive(False)
        self.widgets.tbtn_on.set_active(False)
        self.command.mode(linuxcnc.MODE_MANUAL)

    def on_hal_status_state_estop_reset(self, widget=None):
        self.widgets.tbtn_estop.set_active(False)
        self.widgets.tbtn_estop.set_image(self.widgets.img_emergency_off)
        self.widgets.tbtn_on.set_image(self.widgets.img_machine_off)
        self.widgets.tbtn_on.set_sensitive(True)
        self.widgets.ntb_jog.set_sensitive(True)
        self.widgets.ntb_jog_JA.set_sensitive(False)
        self.widgets.vbtb_jog_incr.set_sensitive(False)
        self.widgets.hbox_jog_vel.set_sensitive(False)
        self.widgets.chk_ignore_limits.set_sensitive(True)
        self._check_limits()

    def on_hal_status_state_off(self, widget):
        widgetlist = ["rbt_manual", "rbt_mdi", "rbt_auto", "btn_homing", "btn_touch", "btn_tool",
                      "hbox_jog_vel", "ntb_jog_JA", "vbtb_jog_incr", "spc_feed", "btn_feed_100", "rbt_forward", "btn_index_tool",
                      "rbt_reverse", "rbt_stop", "tbtn_flood", "tbtn_mist", "btn_change_tool", "btn_select_tool_by_no",
                      "btn_spindle_100", "spc_rapid", "spc_spindle",
                      "btn_tool_touchoff_x", "btn_tool_touchoff_z"
        ]
        self._sensitize_widgets(widgetlist, False)
        if self.widgets.tbtn_on.get_active():
            self.widgets.tbtn_on.set_active(False)
        self.widgets.tbtn_on.set_image(self.widgets.img_machine_off)
        self.widgets.btn_exit.set_sensitive(True)
        self.widgets.chk_ignore_limits.set_sensitive(True)
        self.widgets.ntb_main.set_current_page(0)
        self.widgets.ntb_button.set_current_page(_BB_MANUAL)
        self.widgets.ntb_info.set_current_page(0)
        self.widgets.ntb_jog.set_current_page(0)

    def on_hal_status_state_on(self, widget):
        widgetlist = ["rbt_manual", "btn_homing", "btn_touch", "btn_tool",
                      "ntb_jog", "spc_feed", "btn_feed_100", "rbt_forward",
                      "rbt_reverse", "rbt_stop", "tbtn_flood", "tbtn_mist",
                      "btn_spindle_100", "spc_rapid", "spc_spindle"
        ]
        self._sensitize_widgets(widgetlist, True)
        if not self.widgets.tbtn_on.get_active():
            self.widgets.tbtn_on.set_active(True)
        self.widgets.tbtn_on.set_image(self.widgets.img_machine_on)
        self.widgets.btn_exit.set_sensitive(False)
        self.widgets.chk_ignore_limits.set_sensitive(False)
        if self.widgets.ntb_main.get_current_page() != 0:
            self.command.mode(linuxcnc.MODE_MANUAL)
            self.command.wait_complete()

    def on_hal_status_mode_manual(self, widget):
        print ("MANUAL Mode")
        self.widgets.rbt_manual.set_active(True)
        # if setup page is activated, we must leave here, otherwise the pages will be reset
        if self.widgets.tbtn_setup.get_active():
            return
        # if we are in user tabs, we must reset the button
        if self.widgets.tbtn_user_tabs.get_active():
            self.widgets.tbtn_user_tabs.set_active(False)
        self.widgets.ntb_main.set_current_page(0)
        self.widgets.ntb_button.set_current_page(_BB_MANUAL)
        self.widgets.ntb_info.set_current_page(0)
        self.widgets.ntb_jog.set_current_page(0)
        self._check_limits()
        
        # if the status changed, we reset the key event, otherwise the key press
        # event will not change, if the user did the last change with keyboard shortcut
        # This is caused, because we record the last key event to avoid multiple key
        # press events by holding down the key. I.e. One press should only advance one increment
        # on incremental jogging.
        self.last_key_event = None, 0


    def on_hal_status_mode_mdi(self, widget):
        print ("MDI Mode", self.tool_change)

        # if the edit offsets button is active, we do not want to change
        # pages, as the user may want to edit several axis values
        if self.widgets.tbtn_edit_offsets.get_active():
            return

        # self.tool_change is set only if the tool change was commanded
        # from tooledit widget/page, so we do not want to switch the
        # screen layout to MDI, but set the manual widgets
        if self.tool_change:
            self.widgets.ntb_main.set_current_page(0)
            self.widgets.ntb_button.set_current_page(_BB_MANUAL)
            self.widgets.ntb_info.set_current_page(0)
            self.widgets.ntb_jog.set_current_page(0)
            return

        # if MDI button is not sensitive, we are not ready for MDI commands
        # so we have to abort external commands and get back to manual mode
        # This will happen mostly, if we are in settings mode, as we do disable the mode button
        if not self.widgets.rbt_mdi.get_sensitive():
            self.command.abort()
            self.command.mode(linuxcnc.MODE_MANUAL)
            self.command.wait_complete()
            self._show_error((13, _("It is not possible to change to MDI Mode at the moment")))
            return
        else:
            # if we are in user tabs, we must reset the button
            if self.widgets.tbtn_user_tabs.get_active():
                self.widgets.tbtn_user_tabs.set_active(False)
            if self.widgets.chk_use_kb_on_mdi.get_active():
                self.widgets.ntb_info.set_current_page(1)
            else:
                self.widgets.ntb_info.set_current_page(0)
            self.widgets.ntb_main.set_current_page(0)
            self.widgets.ntb_button.set_current_page(_BB_MDI)
            self.widgets.ntb_jog.set_current_page(1)
            self.widgets.hal_mdihistory.entry.grab_focus()
            self.widgets.rbt_mdi.set_active(True)
            
            # if the status changed, we reset the key event, otherwise the key press
            # event will not change, if the user did the last change with keyboard shortcut
            # This is caused, because we record the last key event to avoid multiple key
            # press events by holding down the key. I.e. One press should only advance one increment
            # on incremental jogging.
            self.last_key_event = None, 0


    def on_hal_status_mode_auto(self, widget):
        print ("AUTO Mode")
        # if Auto button is not sensitive, we are not ready for AUTO commands
        # so we have to abort external commands and get back to manual mode
        # This will happen mostly, if we are in settings mode, as we do disable the mode button
        if not self.widgets.rbt_auto.get_sensitive():
            self.command.abort()
            self.command.mode(linuxcnc.MODE_MANUAL)
            self.command.wait_complete()
            self._show_error((13, _("It is not possible to change to Auto Mode at the moment")))
            return
        else:
            # if we are in user tabs, we must reset the button
            if self.widgets.tbtn_user_tabs.get_active():
                self.widgets.tbtn_user_tabs.set_active(False)
            self.widgets.ntb_main.set_current_page(0)
            self.widgets.ntb_button.set_current_page(_BB_AUTO)
            self.widgets.ntb_info.set_current_page(0)
            self.widgets.ntb_jog.set_current_page(2)
            self.widgets.rbt_auto.set_active(True)
            
            # if the status changed, we reset the key event, otherwise the key press
            # event will not change, if the user did the last change with keyboard shortcut
            # This is caused, because we record the last key event to avoid multiple key
            # press events by holding down the key. I.e. One press should only advance one increment
            # on incremental jogging.
            self.last_key_event = None, 0

    def on_hal_status_motion_mode_changed(self, widget, new_mode):
        # Motion mode change in identity kinematics makes no sense
        # so we will not react on the signal and correct the misbehavior
        # self.stat.motion_mode return
        # Mode 1 = joint ; Mode 2 = MDI ; Mode 3 = teleop
        # so in mode 1 we have to show Joints and in Modes 2 and 3 axis values

        if new_mode == 1 and self.stat.kinematics_type != linuxcnc.KINEMATICS_IDENTITY:
            self.widgets.gremlin.set_property("enable_dro", True)
            self.widgets.gremlin.use_joints_mode = True
            self.widgets.tbtn_switch_mode.set_active(True)
            self.widgets.ntb_jog_JA.set_page(1)
        else:
            if not self.widgets.tbtn_fullsize_preview.get_active():
                self.widgets.gremlin.set_property("enable_dro", False)
            self.widgets.gremlin.use_joints_mode = False
            self.widgets.tbtn_switch_mode.set_active(False)
            self.widgets.ntb_jog_JA.set_page(0)
            
# hal status End
# =========================================================

    # There are some settings we can only do if the window is on the screen already
    def on_window1_show(self, widget, data=None):

        # it is time to get the correct estop state and set the button status
        self.stat.poll()
        if self.stat.task_state == linuxcnc.STATE_ESTOP:
            self.widgets.tbtn_estop.set_active(True)
            self.widgets.tbtn_estop.set_image(self.widgets.img_emergency)
            self.widgets.tbtn_on.set_image(self.widgets.img_machine_off)
            self.widgets.tbtn_on.set_sensitive(False)
        else:
            self.widgets.tbtn_estop.set_active(False)
            self.widgets.tbtn_estop.set_image(self.widgets.img_emergency_off)
            self.widgets.tbtn_on.set_sensitive(True)

        # if a file should be loaded, we will do so
        file = self.prefs.getpref("open_file", "", str)
        if file:
            self.widgets.file_to_load_chooser.set_filename(file)
            # self.command.program_open(file)
            self.widgets.hal_action_open.load_file(file)

        # check how to start the GUI
        start_as = "rbtn_" + self.prefs.getpref("screen1", "window", str)
        self.widgets[start_as].set_active(True)
        if start_as == "rbtn_fullscreen":
            self.widgets.window1.fullscreen()
        elif start_as == "rbtn_maximized":
            self.widgets.window1.maximize()
        else:
            self.xpos = int(self.prefs.getpref("x_pos", 40, float))
            self.ypos = int(self.prefs.getpref("y_pos", 30, float))
            self.width = int(self.prefs.getpref("width", 979, float))
            self.height = int(self.prefs.getpref("height", 750, float))

            # set the adjustments according to Window position and size
            self.widgets.adj_x_pos.set_value(self.xpos)
            self.widgets.adj_y_pos.set_value(self.ypos)
            self.widgets.adj_width.set_value(self.width)
            self.widgets.adj_height.set_value(self.height)

            # move and resize the window
            self.widgets.window1.move(self.xpos, self.ypos)
            self.widgets.window1.resize(self.width, self.height)

        self.command.mode(linuxcnc.MODE_MANUAL)
        self.command.wait_complete()

        self.initialized = True

        # does the user want to show screen2
        self._check_screen2()
        if self.screen2:
            self.widgets.tbtn_use_screen2.set_active(self.prefs.getpref("use_screen2", False, bool))

    # kill keyboard and estop machine before closing
    def on_window1_destroy(self, widget, data=None):
        print "estoping / killing gmoccapy"
        if self.onboard:
            self._kill_keyboard()
        self.command.state(linuxcnc.STATE_OFF)
        self.command.state(linuxcnc.STATE_ESTOP)
        gtk.main_quit()

    # What to do if a macro button has been pushed
    def _on_btn_macro_pressed( self, widget = None, data = None ):
        o_codes = data.split()

        command = str( "O<" + o_codes[0] + "> call" )

        for code in o_codes[1:]:
            parameter = self.dialogs.entry_dialog(self, data=None, header=_("Enter value:"),
                                                  label=_("Set parameter {0} to:").format(code), integer=False)
            if parameter == "ERROR":
                print(_("conversion error"))
                self.dialogs.warning_dialog(self, _("Conversion error !"),
                                            ("Please enter only numerical values\nValues have not been applied"))
                return
            elif parameter == "CANCEL":
                return
            else:
                pass
            command = command + " [" + str(parameter) + "] "
# TODO: Should not only clear the plot, but also the loaded program?
        # self.command.program_open("")
        # self.command.reset_interpreter()
        self.widgets.gremlin.clear_live_plotter()
# TODO: End
        self.command.mdi(command)
        for btn in self.macrobuttons:
            btn.set_sensitive(False)
        # we change the widget_image and use the button to interrupt running macros
        if not self.onboard:
            self.widgets.btn_show_kbd.set_sensitive(True)
        self.widgets.btn_show_kbd.set_image(self.widgets.img_brake_macro)
        self.widgets.btn_show_kbd.set_property("tooltip-text", _("interrupt running macro"))
        self.widgets.ntb_info.set_current_page(0)

# helpers functions start
# =========================================================

    def _update_widgets(self, state):
        widgetlist = ["rbt_manual", "btn_homing", "btn_touch", "btn_tool",
                      "hbox_jog_vel", "ntb_jog_JA", "vbtb_jog_incr", "spc_feed", "btn_feed_100", "rbt_forward", "btn_index_tool",
                      "rbt_reverse", "rbt_stop", "tbtn_flood", "tbtn_mist", "btn_change_tool", "btn_select_tool_by_no",
                      "btn_spindle_100", "spc_rapid", "spc_spindle",
                      "btn_tool_touchoff_x", "btn_tool_touchoff_z"
        ]
        self._sensitize_widgets(widgetlist, state)

    def _switch_to_g7(self, state):
        if state:
            self.widgets.Combi_DRO_x.set_property("abs_color", gtk.gdk.color_parse("#F2F1F0"))
            self.widgets.Combi_DRO_x.set_property("rel_color", gtk.gdk.color_parse("#F2F1F0"))
            self.widgets.Combi_DRO_x.set_property("dtg_color", gtk.gdk.color_parse("#F2F1F0"))
            self.widgets.Combi_DRO_y.set_property("abs_color", gtk.gdk.color_parse(self.abs_color))
            self.widgets.Combi_DRO_y.set_property("rel_color", gtk.gdk.color_parse(self.rel_color))
            self.widgets.Combi_DRO_y.set_property("dtg_color", gtk.gdk.color_parse(self.dtg_color))
            self.diameter_mode = True
        else:
            self.widgets.Combi_DRO_y.set_property("abs_color", gtk.gdk.color_parse("#F2F1F0"))
            self.widgets.Combi_DRO_y.set_property("rel_color", gtk.gdk.color_parse("#F2F1F0"))
            self.widgets.Combi_DRO_y.set_property("dtg_color", gtk.gdk.color_parse("#F2F1F0"))
            self.widgets.Combi_DRO_x.set_property("abs_color", gtk.gdk.color_parse(self.abs_color))
            self.widgets.Combi_DRO_x.set_property("rel_color", gtk.gdk.color_parse(self.rel_color))
            self.widgets.Combi_DRO_x.set_property("dtg_color", gtk.gdk.color_parse(self.dtg_color))
            self.diameter_mode = False

    def on_key_event(self, widget, event, signal):

        # get the keyname
        keyname = gtk.gdk.keyval_name(event.keyval)

        # estop with F1 shold work every time
        # so should also escape abort actions
        if keyname == "F1":  # will estop the machine, but not reset estop!
            self.command.state(linuxcnc.STATE_ESTOP)
            return True
        if keyname == "Escape":
            self.command.abort()
            return True
# ToDo:
# Check if homed, otherwise do not allow to change mode

        # change between teleop and world mode
        if keyname == "F12" or keyname == "$":
            # only change mode pressing the key, not releasing it
            if signal:
                # No mode switch to joints on Identity kinematics
                if self.stat.kinematics_type != linuxcnc.KINEMATICS_IDENTITY:
                    # Mode 1 = joint ; Mode 3 = teleop
                    if self.stat.motion_mode != 1:
                        self.set_motion_mode(0) # set joint mode
                    else:
                        self.set_motion_mode(1) # set teleop mode
            return True

        # This will avoid executing the key press event several times caused by keyboard auto repeat
        if self.last_key_event[0] == keyname and self.last_key_event[1] == signal:
            return True

        try:
            if keyname == "F2" and signal:
                # only turn on if no estop!
                if self.widgets.tbtn_estop.get_active():
                    return True
                self.widgets.tbtn_on.emit("clicked")
                return True
        except:
            pass

        if keyname == "space" and signal:
            if event.state & gtk.gdk.CONTROL_MASK:  # only do it when control is hold down
                self.notification.del_message(-1)
                self.widgets.window1.grab_focus()
                return

        if keyname == "Super_L" and signal:  # Left Windows
            self.notification.del_last()
            self.widgets.window1.grab_focus()
            return

        # if the user do not want to use keyboard shortcuts, we leave here
        # in this case we do not return true, otherwise entering code in MDI history
        # and the integrated editor will not work
        if not self.widgets.chk_use_kb_shortcuts.get_active():
            print("Settings say: do not use keyboard shortcuts, abort")
            return

        # Only in MDI mode the RETURN key should execute a command
        if keyname == "Return" and signal and self.stat.task_mode == linuxcnc.MODE_MDI:
            # print("Got enter in MDI")
            self.widgets.hal_mdihistory.submit()
            self.widgets.hal_mdihistory.entry.grab_focus()
            # we need to leave here, otherwise the check for jogging 
            # only allowed in manual mode will finish the sub
            return True

        # mode change are only allowed if the interpreter is idle, like mode switching
        if keyname == "F3" or keyname == "F5":
            if self.stat.interp_state != linuxcnc.INTERP_IDLE:
                if signal: # Otherwise the message will be shown twice
                    self._show_error((13, _("Mode change is only allowed if the interpreter is idle!")))
                return
            else:
                # F3 change to manual mode
                if keyname == "F3" and signal:
                    self.command.mode(linuxcnc.MODE_MANUAL)
                    self.command.wait_complete()
                    # we need to leave here, otherwise the check for jogging 
                    # only allowed in manual mode will finish the sub
                    self.last_key_event = keyname, signal
                    return True
        
                # F5 should change to mdi mode
                if keyname == "F5" and signal:
                    self.command.mode(linuxcnc.MODE_MDI)
                    self.command.wait_complete()        
                    # we need to leave here, otherwise the check for jogging 
                    # only allowed in manual mode will finish the sub
                    self.last_key_event = keyname, signal
                    return True

        # in AUTO Mode we will allow the following key shortcuts
        # R = run program
        # P = pause program
        # S = resume program
        if self.stat.task_mode == linuxcnc.MODE_AUTO:
            # if we are in edit mode do not start a program!
            if self.widgets.ntb_button.get_current_page() == _BB_EDIT:
                return

            # all makes only sense, if a program is loaded, 
            # if so, the button use current is sensitive
            if not self.widgets.btn_use_current.get_sensitive():
                return

            if (keyname == "R" or keyname == "r") and self.stat.interp_state == linuxcnc.INTERP_IDLE:
                self.command.auto(linuxcnc.AUTO_RUN,0)

            if (keyname == "p" or keyname == "P") and self.widgets.tbtn_pause.get_sensitive():
                self.command.auto(linuxcnc.AUTO_PAUSE)

            if (keyname == "S" or keyname == "s"):
                self.command.auto(linuxcnc.AUTO_RESUME)
                if self.widgets.tbtn_pause.get_active():
                    self.widgets.tbtn_pause.set_active(False)

        # Only in manual mode jogging with keyboard is allowed
        # in this case we do not return true, otherwise entering code in MDI history
        # and the integrated editor will not work
        # we also check if we are in settings or user page
        if self.stat.task_mode != linuxcnc.MODE_MANUAL or not self.widgets.ntb_main.get_current_page() == 0:
            return

        # This is just to avoid a terminal message, that this keys are not implemented:
        if (keyname == "R" or keyname == "r" or
            keyname == "p" or keyname == "P" or
            keyname == "S" or keyname == "s"):
            return

        # offset page is active, so keys must go through
        if self.widgets.ntb_preview.get_current_page() == 1:
            return

        # tooledit page is active, so keys must go through
        if self.widgets.ntb_preview.get_current_page() == 2:
            return

        # take care of different key handling for lathe operation
        if self.lathe_mode:
            if keyname == "Page_Up" or keyname == "Page_Down" or keyname == "KP_Page_Up" or keyname == "KP_Page_Down":
                return

        if event.state & gtk.gdk.SHIFT_MASK:  # SHIFT is hold down, fast jogging active
            fast = True
        else:
            fast = False

        if keyname == "Up" or keyname == "KP_Up":
            if self.lathe_mode:
                if self.backtool_lathe:
                    widget = self.widgets.btn_x_plus
                else:
                    widget = self.widgets.btn_x_minus
            else:
                widget = self.widgets.btn_y_plus
            if signal:
                self.on_btn_jog_pressed(widget, fast)
            else:
                self.on_btn_jog_released(widget)
        elif keyname == "Down" or keyname == "KP_Down":
            if self.lathe_mode:
                if self.backtool_lathe:
                    widget = self.widgets.btn_x_minus
                else:
                    widget = self.widgets.btn_x_plus
            else:
                widget = self.widgets.btn_y_minus
            if signal:
                self.on_btn_jog_pressed(widget, fast)
            else:
                self.on_btn_jog_released(widget)
        elif keyname == "Left" or keyname == "KP_Left":
            if self.lathe_mode:
                widget = self.widgets.btn_z_minus
            else:
                widget = self.widgets.btn_x_minus
            if signal:
                self.on_btn_jog_pressed(widget, fast)
            else:
                self.on_btn_jog_released(widget)
        elif keyname == "Right" or keyname == "KP_Right":
            if self.lathe_mode:
                widget = self.widgets.btn_z_plus
            else:
                widget = self.widgets.btn_x_plus
            if signal:
                self.on_btn_jog_pressed(widget, fast)
            else:
                self.on_btn_jog_released(widget)
        elif keyname == "Page_Up" or keyname == "KP_Page_Up":
            widget = self.widgets.btn_z_plus
            if signal:
                self.on_btn_jog_pressed(widget, fast)
            else:
                self.on_btn_jog_released(widget)
        elif keyname == "Page_Down" or keyname == "KP_Page_Down":
            widget = self.widgets.btn_z_minus
            if signal:
                self.on_btn_jog_pressed(widget, fast)
            else:
                self.on_btn_jog_released(widget)
        elif keyname == "I" or keyname == "i":
            if signal:
                if self.stat.state != 1:  # still moving
                    return
                # The active button name is hold in self.active_increment
                if keyname == "I":
                    # so lets increment it by one
                    rbt = int(self.active_increment[-1]) + 1
                    # we check if we are still in the allowed limit
                    if rbt > len(self.jog_increments) - 1:  # beginning from zero
                        rbt = 0
                else:  # must be "i"
                    # so lets reduce it by one
                    rbt = int(self.active_increment[-1]) - 1
                    # we check if we are still in the allowed limit
                    if rbt < 0:
                        rbt = len(self.jog_increments) - 1  # beginning from zero
                # we set the corresponding button active
                self.incr_rbt_list[rbt].set_active(True)
                # and we have to update all pin and variables
                self.on_increment_changed(self.incr_rbt_list[rbt], self.jog_increments[rbt])
        else:
            print("This key has not been implemented yet")
            print "Key {0} ({1:d}) was pressed".format(keyname, event.keyval), signal, self.last_key_event
        self.last_key_event = keyname, signal
        return True

    # Notification stuff.
    def _init_notification(self):
        start_as = "rbtn_" + self.prefs.getpref("screen1", "window", str)
        xpos, ypos = self.widgets.window1.window.get_origin()
        self.notification.set_property('x_pos', self.widgets.adj_x_pos_popup.get_value())
        self.notification.set_property('y_pos', self.widgets.adj_y_pos_popup.get_value())
        self.notification.set_property('message_width', self.widgets.adj_width_popup.get_value())
        if int(self.widgets.adj_max_messages.get_value()) != 0:
            self.notification.set_property('max_messages', self.widgets.adj_max_messages.get_value())
        self.notification.set_property('use_frames', self.widgets.chk_use_frames.get_active())
        self.notification.set_property('font', self.widgets.fontbutton_popup.get_font_name())
        self.notification.set_property('icon_size', 48)
        self.notification.set_property('top_to_bottom', True)

    # This is the jogging part
    def on_increment_changed(self, widget=None, data=None):
        if self.stat.interp_state != linuxcnc.INTERP_IDLE:
            return

        if data == 0:
            self.distance = 0
        else:
            self.distance = self._parse_increment(data)
        self.halcomp["jog.jog-increment"] = self.distance
        self.active_increment = widget.__name__

    def _from_internal_linear_unit(self, v, unit=None):
        if unit is None:
            unit = self.stat.linear_units
        lu = (unit or 1) * 25.4
        return v * lu

    def _parse_increment(self, jogincr):
        if jogincr.endswith("mm"):
            scale = self._from_internal_linear_unit(1 / 25.4)
        elif jogincr.endswith("cm"):
            scale = self._from_internal_linear_unit(10 / 25.4)
        elif jogincr.endswith("um"):
            scale = self._from_internal_linear_unit(.001 / 25.4)
        elif jogincr.endswith("in") or jogincr.endswith("inch"):
            scale = self._from_internal_linear_unit(1.)
        elif jogincr.endswith("mil"):
            scale = self._from_internal_linear_unit(.001)
        else:
            scale = 1
        jogincr = jogincr.rstrip(" inchmuil")
        if "/" in jogincr:
            p, q = jogincr.split("/")
            jogincr = float(p) / float(q)
        else:
            jogincr = float(jogincr)
        return jogincr * scale

    def _replace_list_item(self, int_tab, old_value, new_value):
        list = self.h_tabs[int_tab]
        self.h_tabs[int_tab] = []
        for item in list:
            if item[1] == old_value:
                new_tupple = (item[0], new_value)
                item = new_tupple
                print(_("**** GMOCCAPY INFO ****"))
                print(_("**** replaced {0} to {1} ****").format(old_value, new_value))
            self.h_tabs[int_tab].append(item)

    # check if macros are in the INI file and add them to MDI Button List
    def _add_macro_button(self):
        macros = self.get_ini_info.get_macros()

        # if no macros at all are found, we receieve a NONW, so we have to check:
        if not macros:
            num_macros = 0
        else:
            num_macros = len( macros )

        if num_macros > 9:
            message = _( "**** GMOCCAPY INFO ****\n" )
            message += _( "**** found more than 9 macros, only the first 9 will be used ****" )
            print( message )

            num_macros = 9
        for increment in range(0, num_macros):
            name = macros[increment]
            lbl = name.split()[0]
            # shorten / break line of the name if it is to long
            if len( lbl ) > 11:
                lbl = lbl[0:10] + "\n" + lbl[11:20]
            btn = gtk.Button( lbl, None, False )
            btn.connect( "pressed", self._on_btn_macro_pressed, name )
            btn.position = increment
            # we add the button to a list to be able later to see what macro to execute
            self.macrobuttons.append(btn)
            self.widgets.hbtb_MDI.pack_start(btn, True, True, 0)
            btn.show()
        # if there is still place, we fill it with empty labels, to be sure the button will not be on different
        # places if the amount of macros change.
        if num_macros < 9:
            for label_space in range(num_macros, 9):
                lbl = "lbl_sp_{0}".format(label_space)
                lbl = gtk.Label(lbl)
                lbl.position = label_space
                lbl.set_text("")
                self.widgets.hbtb_MDI.pack_start(lbl, True, True, 0)
                lbl.show()
        self.widgets.hbtb_MDI.non_homogeneous = False

    def show_try_errors(self):
        exc_type, exc_value, exc_traceback = sys.exc_info()
        formatted_lines = traceback.format_exc().splitlines()
        print(_("**** GMOCCAPY ERROR ****"))
        print(_("**** {0} ****").format(formatted_lines[0]))
        traceback.print_tb(exc_traceback, limit=1, file=sys.stdout)
        print (formatted_lines[-1])

    def _sensitize_widgets(self, widgetlist, value):
        for name in widgetlist:
            try:
                self.widgets[name].set_sensitive(value)
            except Exception, e:
                print (_("**** GMOCCAPY ERROR ****"))
                print _("**** No widget named: {0} to sensitize ****").format(name)
                traceback.print_exc()

    def _update_active_gcodes(self):
        # active G codes
        active_codes = []
        temp = []
        for code in sorted(self.stat.gcodes[1:]):
            if code == -1:
                continue
            if code % 10 == 0:
                temp.append("{0}".format(code / 10))
            else:
                temp.append("{0}.{1}".format(code / 10, code % 10))
        for num, code in enumerate(temp):
            if num == 8:
                active_codes.append("\n")
            active_codes.append("G" + code)
        self.active_gcodes = active_codes
        self.gcodes = self.stat.gcodes
        self.widgets.active_gcodes_label.set_label(" ".join(self.active_gcodes))

    def _update_active_mcodes(self):
        # M codes
        active_codes = []
        temp = []
        for code in sorted(self.stat.mcodes[1:]):
            if code == -1:
                continue
            temp.append("{0}".format(code))
        for code in (temp):
            active_codes.append("M" + code)
        self.active_mcodes = active_codes
        self.mcodes = self.stat.mcodes
        self.widgets.active_mcodes_label.set_label(" ".join(self.active_mcodes))

    # Update the velocity labels
    def _update_vel(self):
        # self.stat.program_units will return 1 for inch, 2 for mm and 3 for cm
        real_feed = float(self.stat.settings[1] * self.stat.feedrate)
        if self.stat.program_units != 1:
            self.widgets.lbl_current_vel.set_text("{0:d}".format(int(self.stat.current_vel * 60.0 * self.faktor)))
            if "G95" in self.active_gcodes:
                feed_str = "{0:d}".format(int(self.stat.settings[1]))
                real_feed_str = "F  {0:.2f}".format(real_feed)
            else:
                feed_str = "{0:d}".format(int(self.stat.settings[1]))
                real_feed_str = "F  {0:d}".format(int(real_feed))
        else:
            self.widgets.lbl_current_vel.set_text("{0:.2f}".format(self.stat.current_vel * 60.0 * self.faktor))
            if "G95" in self.active_gcodes:
                feed_str = "{0:.4f}".format(self.stat.settings[1])
                real_feed_str = "F {0:.4f}".format(real_feed)
            else:
                feed_str = "{0:.3f}".format(self.stat.settings[1])
                real_feed_str = "F {0:.3f}".format(real_feed)

        # converting 0.0 to string brings nothing, so the string is empty
        # happens only on start up
        if not real_feed:
            real_feed_str = "F  0"

        self.widgets.lbl_active_feed.set_label(feed_str)
        self.widgets.lbl_feed_act.set_text(real_feed_str)

    def _update_coolant(self):
        if self.stat.flood:
            if not self.widgets.tbtn_flood.get_active():
                self.widgets.tbtn_flood.set_active(True)
                self.widgets.tbtn_flood.set_image(self.widgets.img_coolant_on)
        else:
            if self.widgets.tbtn_flood.get_active():
                self.widgets.tbtn_flood.set_active(False)
                self.widgets.tbtn_flood.set_image(self.widgets.img_coolant_off)
        if self.stat.mist:
            if not self.widgets.tbtn_mist.get_active():
                self.widgets.tbtn_mist.set_active(True)
                self.widgets.tbtn_mist.set_image(self.widgets.img_mist_on)
        else:
            if self.widgets.tbtn_mist.get_active():
                self.widgets.tbtn_mist.set_active(False)
                self.widgets.tbtn_mist.set_image(self.widgets.img_mist_off)

    def _update_halui_pin(self):
        if self.spindle_override != self.stat.spindle[0]['override']:
            self.initialized = False
            self.widgets.spc_spindle.set_value(self.stat.spindle[0]['override'] * 100)
            self.spindle_override = self.stat.spindle[0]['override']
            self.initialized = True
        if self.feed_override != self.stat.feedrate:
            self.initialized = False
            self.widgets.spc_feed.set_value(self.stat.feedrate * 100)
            self.feed_override = self.stat.feedrate
            self.initialized = True
        if self.rapidrate != self.stat.rapidrate:
            self.initialized = False
            self.widgets.spc_rapid.set_value(self.stat.rapidrate * 100)
            self.rapidrate = self.stat.rapidrate
            self.initialized = True

    def _update_slider(self, widgetlist):
        # update scales and sliders, this must happen if sliders shows units
        # like papid_vel and jog_vel
        for widget in widgetlist:
            value = self.widgets[widget].get_value()
            min = self.widgets[widget].get_property("min")
            max = self.widgets[widget].get_property("max")
            self.widgets[widget].set_property("min", min * self.faktor)
            self.widgets[widget].set_property("max", max * self.faktor)
            self.widgets[widget].set_value(value * self.faktor)

        self.scale_jog_vel = self.scale_jog_vel * self.faktor
        
        self.rabbit_jog = self.rabbit_jog * self.faktor
        self.turtle_jog = self.turtle_jog * self.faktor            

    def _change_dro_color(self, property, color):
        for axis in self.axis_list:
            if axis == self.axisletter_four:
                axis = 4
            if axis == self.axisletter_five:
                axis = 5
            self.widgets["Combi_DRO_{0}".format(axis)].set_property(property, color)
        if self.lathe_mode:
            self.widgets.Combi_DRO_y.set_property(property, color)
            # check if G7 or G8 is active
            # this is set on purpose wrong, because we want the periodic
            # to update the state correctly
            if "G7" in self.active_gcodes:
                self.diameter_mode = False
            else:
                self.diameter_mode = True

    def _update_toolinfo(self, tool):
        toolinfo = self.widgets.tooledit1.get_toolinfo(tool)
        if toolinfo:
            # Doku
            # toolinfo[0] = cell toggle
            # toolinfo[1] = tool number
            # toolinfo[2] = pocket number
            # toolinfo[3] = X offset
            # toolinfo[4] = Y offset
            # toolinfo[5] = Z offset
            # toolinfo[6] = A offset
            # toolinfo[7] = B offset
            # toolinfo[8] = C offset
            # toolinfo[9] = U offset
            # toolinfo[10] = V offset
            # toolinfo[11] = W offset
            # toolinfo[12] = tool diameter
            # toolinfo[13] = frontangle
            # toolinfo[14] = backangle
            # toolinfo[15] = tool orientation
            # toolinfo[16] = tool info
            self.widgets.lbl_tool_no.set_text(str(toolinfo[1]))
            self.widgets.lbl_tool_dia.set_text(toolinfo[12])
            self.halcomp["tool-diameter"] = float(locale.atof(toolinfo[12]))
            self.widgets.lbl_tool_name.set_text(toolinfo[16])

        # we do not allow touch off with no tool mounted, so we set the
        # corresponding widgets unsensitized and set the description accordingly
        if tool <= 0:
            self.widgets.lbl_tool_no.set_text("0")
            self.widgets.lbl_tool_dia.set_text("0")
            self.widgets.lbl_tool_name.set_text(_("No tool description available"))
            self.widgets.btn_tool_touchoff_x.set_sensitive(False)
            self.widgets.btn_tool_touchoff_z.set_sensitive(False)
        else:
            self.widgets.btn_tool_touchoff_x.set_sensitive(True)
            self.widgets.btn_tool_touchoff_z.set_sensitive(True)

        if self.load_tool:
            self.load_tool = False
            self.on_hal_status_interp_idle(None)
            return

        if "G43" in self.active_gcodes and self.stat.task_mode != linuxcnc.MODE_AUTO:
            self.command.mode(linuxcnc.MODE_MDI)
            self.command.wait_complete()
            self.command.mdi("G43")
            self.command.wait_complete()

# helpers functions end
# =========================================================

    def on_adj_dro_digits_value_changed(self, widget, data=None):
        if not self.initialized:
            return
        self.dro_digits = int(widget.get_value())
        self.prefs.putpref("dro_digits", self.dro_digits, int)
        if self.stat.program_units != 1:
            format_string_mm = "%" + str(13 - self.dro_digits) + "." + str(self.dro_digits) + "f"
            format_string_inch = "%" + str(13 - self.dro_digits - 1) + "." + str(self.dro_digits + 1) + "f"
        else:
            format_string_inch = "%" + str(13 - self.dro_digits) + "." + str(self.dro_digits) + "f"
            format_string_mm = "%" + str(13 - self.dro_digits + 1) + "." + str(self.dro_digits - 1) + "f"

        for axis in self.axis_list:
            if axis == self.axisletter_four:
                axis = 4
            if axis == self.axisletter_five:
                axis = 5
            self.widgets["Combi_DRO_{0}".format(axis)].set_property("mm_text_template", format_string_mm)
            self.widgets["Combi_DRO_{0}".format(axis)].set_property("imperial_text_template", format_string_inch)

        if self.lathe_mode:
            self.widgets.Combi_DRO_y.set_property("mm_text_template", format_string_mm)
            self.widgets.Combi_DRO_y.set_property("imperial_text_template", format_string_inch)

    def on_chk_toggle_readout_toggled(self, widget, data=None):
        state = widget.get_active()
        self.prefs.putpref("toggle_readout", state)
        self.toggle_readout = state
        for axis in self.axis_list:
            if axis == self.axisletter_four:
                axis = 4
            if axis == self.axisletter_five:
                axis = 5
            self.widgets["Combi_DRO_{0}".format(axis)].set_property("toggle_readout", state)

    def on_Combi_DRO_clicked(self, widget, joint_number, order):
        if not self.toggle_readout:
            return
        for axis in self.axis_list:
            if axis == self.axisletter_four:
                axis = 4
            if axis == self.axisletter_five:
                axis = 5
            self.widgets["Combi_DRO_{0}".format(axis)].set_order(order)
        if self.lathe_mode:
            self.widgets.Combi_DRO_y.set_order(order)
        self._offset_changed(None, None)
# from here only needed, if the DRO button will remain in gmoccapy
        if order[0] == "Abs" and self.widgets.tbtn_rel.get_label() != "Abs":
            self.widgets.tbtn_rel.set_active(False)
        if order[0] == "Rel" and self.widgets.tbtn_rel.get_label() != self.widgets.Combi_DRO_x.system:
            self.widgets.tbtn_rel.set_active(True)
        if order[0] == "DTG":
            self.widgets.tbtn_dtg.set_active(True)
        else:
            self.widgets.tbtn_dtg.set_active(False)
# to here only needed, if the DRO button will remain in gmoccapy

    def _offset_changed(self, pin, tooloffset):
        if self.widgets.Combi_DRO_x.machine_units == _MM:
            self.widgets.lbl_tool_offset_z.set_text("{0:.3f}".format(self.halcomp["tooloffset-z"]))
            self.widgets.lbl_tool_offset_x.set_text("{0:.3f}".format(self.halcomp["tooloffset-x"]))
        else:
            self.widgets.lbl_tool_offset_z.set_text("{0:.4f}".format(self.halcomp["tooloffset-z"]))
            self.widgets.lbl_tool_offset_x.set_text("{0:.4f}".format(self.halcomp["tooloffset-x"]))

    def on_offsetpage1_selection_changed(self, widget, system, name):
        if system not in self.system_list[1:] or self.widgets.tbtn_edit_offsets.get_active():
            self.widgets.btn_set_selected.set_sensitive(False)
        else:
            self.widgets.btn_set_selected.set_sensitive(True)

# =========================================================
# from here only needed, if the DRO button will remain in gmoccapy

    def on_Combi_DRO_system_changed(self, widget, system):
        if self.widgets.tbtn_rel.get_active():
            self.widgets.tbtn_rel.set_label(system)
        else:
            self.widgets.tbtn_rel.set_label("Abs")

    def on_Combi_DRO_units_changed(self, widget, metric_units):
        # if the user do not wish to use auto units, we leave here
        if not self.widgets.chk_auto_units.get_active():
            return

        # set gremlin_units
        self.widgets.gremlin.set_property("metric_units", metric_units)

        widgetlist = ["spc_jog_vel"]

        # self.stat.linear_units will return 1.0 for metric and 1/25,4 for imperial
        # display units not equal machine units
        if metric_units != int(self.stat.linear_units):
            # machine units = metric
            if self.stat.linear_units == _MM:
                self.faktor = (1.0 / 25.4)
            # machine units = imperial
            else:
                self.faktor = 25.4
            self.turtle_jog = self.turtle_jog * self.faktor
            self.rabbit_jog = self.rabbit_jog * self.faktor
            self._update_slider( widgetlist )

        else:
            # display units equal machine units would be factor = 1,
            # but if factor not equal 1.0 than we have to reconvert from previous first
            self.turtle_jog = self.turtle_jog / self.faktor
            self.rabbit_jog = self.rabbit_jog / self.faktor
            if self.faktor != 1.0:
                self.faktor = 1 / self.faktor
                self._update_slider(widgetlist)
                self.faktor = 1.0
                self._update_slider(widgetlist)

        if metric_units:
            self.widgets.spc_jog_vel.set_digits(0)
            self.widgets.spc_jog_vel.set_property("unit", _("mm/min"))
        else:
            self.widgets.spc_jog_vel.set_digits(2)
            self.widgets.spc_jog_vel.set_property("unit", _("inch/min"))
            
    def on_tbtn_rel_toggled(self, widget, data=None):
        if self.widgets.tbtn_dtg.get_active():
            self.widgets.tbtn_dtg.set_active(False)
        if widget.get_active():
            widget.set_label(self.widgets.Combi_DRO_x.system)
            order = ["Rel", "Abs", "DTG"]
        else:
            widget.set_label("Abs")
            order = ["Abs", "DTG", "Rel"]
        self.on_Combi_DRO_clicked(None, None, order)

    def on_tbtn_dtg_toggled(self, widget, data=None):
        if widget.get_active():
            widget.set_label("GTD")
            order = ["DTG", "Rel", "Abs"]
        else:
            widget.set_label("DTG")
            if self.widgets.tbtn_rel.get_active():
                order = ["Rel", "Abs", "DTG"]
            else:
                order = ["Abs", "DTG", "Rel"]
        self.on_Combi_DRO_clicked(None, None, order)

    def on_tbtn_units_toggled(self, widget, data=None):
        if widget.get_active():
            widget.set_label(_("inch"))
            metric_units = False
        else:
            widget.set_label(_("mm"))
            metric_units = True
        for axis in self.axis_list:
            if axis == self.axisletter_four:
                axis = 4
            if axis == self.axisletter_five:
                axis = 5
            self.widgets["Combi_DRO_{0}".format(axis)].set_to_inch(not metric_units)
        if self.lathe_mode:
            self.widgets.Combi_DRO_y.set_to_inch(not metric_units)
        # set gremlin_units
        self.widgets.gremlin.set_property("metric_units", metric_units)

    def on_chk_auto_units_toggled(self, widget, data=None):
        for axis in self.axis_list:
            if axis == self.axisletter_four:
                axis = 4
            if axis == self.axisletter_five:
                axis = 5
            self.widgets["Combi_DRO_{0}".format(axis)].set_auto_units(self.widgets.chk_auto_units.get_active())
        if self.lathe_mode:
            self.widgets.Combi_DRO_y.set_auto_units(self.widgets.chk_auto_units.get_active())
        self.prefs.putpref("use_auto_units", self.widgets.chk_auto_units.get_active())

    def on_chk_show_dro_btn_toggled(self, widget, data=None):
        if self.widgets.chk_show_dro_btn.get_active():
            self.widgets.tbl_dro_button.show()
            self.widgets.chk_auto_units.set_active(False)
            self.widgets.chk_auto_units.set_sensitive(False)
        else:
            self.widgets.tbl_dro_button.hide()
            self.widgets.chk_auto_units.set_active(True)
            self.widgets.chk_auto_units.set_sensitive(True)
        self.prefs.putpref("show_dro_btn", self.widgets.chk_show_dro_btn.get_active())

# to here only needed, if the DRO button will remain in gmoccapy
# =========================================================

    def on_adj_x_pos_popup_value_changed(self, widget, data=None):
        if not self.initialized:
            return
        self.prefs.putpref("x_pos_popup", widget.get_value(), float)
        self._init_notification()

    def on_adj_y_pos_popup_value_changed(self, widget, data=None):
        if not self.initialized:
            return
        self.prefs.putpref("y_pos_popup", widget.get_value(), float)
        self._init_notification()

    def on_adj_width_popup_value_changed(self, widget, data=None):
        if not self.initialized:
            return
        self.prefs.putpref("width_popup", widget.get_value(), float)
        self._init_notification()

    def on_adj_max_messages_value_changed(self, widget, data=None):
        if not self.initialized:
            return
        self.prefs.putpref("max_messages", widget.get_value(), float)
        self._init_notification()

    def on_chk_use_frames_toggled(self, widget, data=None):
        if not self.initialized:
            return
        self.prefs.putpref("use_frames", widget.get_active())
        self._init_notification()

    def on_fontbutton_popup_font_set(self, font):
        self.prefs.putpref("message_font", self.widgets.fontbutton_popup.get_font_name())
        self._init_notification()

    def on_btn_launch_test_message_pressed(self, widget=None, data=None):
        index = len(self.notification.messages)
        text = _("Halo, welcome to the test message {0}").format(index)
        self._show_error((13, text))

    def on_chk_turtle_jog_toggled(self, widget, data=None):
        state = widget.get_active()
        self.prefs.putpref("hide_turtle_jog_button", state)
        self.widgets.tbl_turtle_jog_factor.set_sensitive(not state)
        if state:
            self.widgets.tbtn_turtle_jog.hide()
        else:
            self.widgets.tbtn_turtle_jog.show()
            self.turtle_jog_factor = self.prefs.getpref('turtle_jog_factor', 20, int)
            self.widgets.adj_turtle_jog_factor.configure(self.turtle_jog_factor, 1,
                                                         100, 1, 0, 0)
            self.turtle_jog = self.jog_rate_max / self.turtle_jog_factor

    def on_adj_turtle_jog_factor_value_changed(self, widget, data=None):
        if not self.initialized:
            return
        self.turtle_jog_factor = int(widget.get_value())
        self.prefs.putpref("turtle_jog_factor", self.turtle_jog_factor, int)
        self.turtle_jog = self.rabbit_jog / self.turtle_jog_factor
        if self.widgets.tbtn_turtle_jog.get_active():
            self.widgets.spc_jog_vel.set_property("min", 0)
            self.widgets.spc_jog_vel.set_property("max", self.jog_rate_max / self.turtle_jog_factor)
            self.widgets.spc_jog_vel.set_value(self.turtle_jog)

    def on_tbtn_turtle_jog_toggled( self, widget, data = None ):
        # due to imperial and metric options we have to get first the values of the widget
        max = self.widgets.spc_jog_vel.max
        min = self.widgets.spc_jog_vel.min
        value = self.widgets.spc_jog_vel.get_value()
        
        if widget.get_active():
            self.rabbit_jog = value
            widget.set_image( self.widgets.img_turtle_jog )
            self.widgets.spc_jog_vel.set_property("min", min)
            self.widgets.spc_jog_vel.set_property("max", max / self.turtle_jog_factor)
            self.widgets.spc_jog_vel.set_value(self.turtle_jog)
        else:
            self.turtle_jog = value
            widget.set_image( self.widgets.img_rabbit_jog )
            self.widgets.spc_jog_vel.set_property("min", min)
            self.widgets.spc_jog_vel.set_property("max", max * self.turtle_jog_factor)
            self.widgets.spc_jog_vel.set_value(self.rabbit_jog)

    def _on_turtle_jog_enable(self, pin):
        self.widgets.tbtn_turtle_jog.set_active(pin.get())

    def on_btn_jog_pressed(self, widget, data=None):
        # only in manual mode we will allow jogging the axis at this development state
        if not self.stat.enabled or self.stat.task_mode != linuxcnc.MODE_MANUAL:
            return

        joint_btn = False
        joint_or_axis = widget.get_label()[0]
        if not joint_or_axis.lower() in "xyzabcuvw":
            # OK, it may be a Joints button
            if joint_or_axis in "012345678":
                joint_btn = True
            else:
                print ("unknown joint or axis {0}".format(joint_or_axis))
                return

        if not joint_btn:
            # get the axisnumber
            joint_axis_number = "xyzabcuvw".index(joint_or_axis.lower())
        else:
            joint_axis_number = "012345678".index(joint_or_axis)

        # if data = True, then the user pressed SHIFT for Jogging and
        # want's to jog at full speed
        if data:
            value = self.stat.max_velocity
        else:
            value = self.widgets.spc_jog_vel.get_value() / 60

        velocity = value * (1 / self.faktor)

        dir = widget.get_label()[1]
        if dir == "+":
            direction = 1
        else:
            direction = -1

        JOGMODE = self._get_jog_mode()
     
        if self.distance <> 0:  # incremental jogging
            self.command.jog(linuxcnc.JOG_INCREMENT, JOGMODE, joint_axis_number, direction * velocity, self.distance)
        else:  # continuous jogging
            self.command.jog(linuxcnc.JOG_CONTINUOUS, JOGMODE, joint_axis_number, direction * velocity)

    def on_btn_jog_released(self, widget, data=None):

        # only in manual mode we will allow jogging the axis at this development state
        # needed to avoid error on stat up, machine not on
        if not self.stat.enabled or self.stat.task_mode != linuxcnc.MODE_MANUAL:
            return

        joint_btn = False
        joint_axis = widget.get_label()[0]
        if not joint_axis.lower() in "xyzabcuvw":
            # OK, it may be a Joints button
            if joint_axis in "012345678":
                joint_btn = True
            else:
                print ("unknown axis {0}".format(joint_axis))
                return

        if not joint_btn:
            # get the axisnumber
            joint_axis_number = "xyzabcuvw".index(joint_axis.lower())
        else:
            joint_axis_number = "012345678".index(joint_axis)

        JOGMODE = self._get_jog_mode()

        # Otherwise the movement would stop before the desired distance was moved
        if self.distance == 0:
            self.command.jog(linuxcnc.JOG_STOP, JOGMODE, joint_axis_number)

    def _get_jog_mode(self):
        # self.stat.motion_mode ==
        # 1 = Joint
        # 2 = MDI
        # 3 = TELOP
        if self.stat.motion_mode == 1:
            JOGMODE = 1
        else:
            JOGMODE = 0
        return JOGMODE

    # use the current loaded file to be loaded on start up
    def on_btn_use_current_clicked(self, widget, data=None):
        if self.stat.file:
            self.widgets.file_to_load_chooser.set_filename(self.stat.file)
            self.prefs.putpref("open_file", self.stat.file)

    # Clear the status to load a file on start up, so there will not be loaded a program
    # on the next start of the GUI
    def on_btn_none_clicked(self, widget, data=None):
        self.widgets.file_to_load_chooser.set_filename(" ")
        self.prefs.putpref("open_file", " ")

    def on_ntb_main_switch_page(self, widget, page, page_num, data=None):
        if self.widgets.tbtn_setup.get_active():
            if page_num != 1L:  # setup page is active,
                self.widgets.tbtn_setup.set_active(False)

    def on_tbtn_setup_toggled(self, widget, data=None):
        # first we set to manual mode, as we do not allow changing settings in other modes
        # otherwise external halui commands could start a program while we are in settings
        self.command.mode(linuxcnc.MODE_MANUAL)
        self.command.wait_complete()
        
        if widget.get_active():
            # deactivate the mode buttons, so changing modes is not possible while we are in settings mode
            self.widgets.rbt_manual.set_sensitive(False)
            self.widgets.rbt_mdi.set_sensitive(False)
            self.widgets.rbt_auto.set_sensitive(False)
            code = False
            # here the user don"t want an unlock code
            if self.widgets.rbt_no_unlock.get_active():
                code = True
            # if hal pin is true, we are allowed to enter settings, this may be
            # realized using a key switch
            if self.widgets.rbt_hal_unlock.get_active() and self.halcomp["unlock-settings"]:
                code = True
            # else we ask for the code using the system.dialog
            if self.widgets.rbt_use_unlock.get_active():
                if self.dialogs.system_dialog(self):
                    code = True
            # Lets see if the user has the right to enter settings
            if code:
                self.widgets.ntb_main.set_current_page(1)
                self.widgets.ntb_setup.set_current_page(0)
                self.widgets.ntb_button.set_current_page(_BB_SETUP)
            else:
                if self.widgets.rbt_hal_unlock.get_active():
                    message = _("Hal Pin is low, Access denied")
                else:
                    message = _("wrong code entered, Access denied")
                self.dialogs.warning_dialog(self, _("Just to warn you"), message)
                self.widgets.tbtn_setup.set_active(False)
        else:
            # check witch button should be sensitive, depending on the state of the machine
            if self.stat.task_state == linuxcnc.STATE_ESTOP:
                # estopped no mode available
                self.widgets.rbt_manual.set_sensitive(False)
                self.widgets.rbt_mdi.set_sensitive(False)
                self.widgets.rbt_auto.set_sensitive(False)
            if (self.stat.task_state == linuxcnc.STATE_ON) and not self.all_homed:
                # machine on, but not homed, only manual allowed
                self.widgets.rbt_manual.set_sensitive(True)
                self.widgets.rbt_mdi.set_sensitive(False)
                self.widgets.rbt_auto.set_sensitive(False)
            if (self.stat.task_state == linuxcnc.STATE_ON) and (self.all_homed or self.no_force_homing):
                # all OK, make all modes available
                self.widgets.rbt_manual.set_sensitive(True)
                self.widgets.rbt_mdi.set_sensitive(True)
                self.widgets.rbt_auto.set_sensitive(True)
            # this is needed here, because we do not
            # change mode, so on_hal_status_manual will not be called
            self.widgets.ntb_main.set_current_page(0)
            self.widgets.ntb_button.set_current_page(_BB_MANUAL)
            self.widgets.ntb_info.set_current_page(0)
            self.widgets.ntb_jog.set_current_page(0)

            # if we are in user tabs, we must reset the button
            if self.widgets.tbtn_user_tabs.get_active():
                self.widgets.tbtn_user_tabs.set_active(False)

    # Show or hide the user tabs
    def on_tbtn_user_tabs_toggled(self, widget, data=None):
        if widget.get_active():
            self.widgets.ntb_main.set_current_page(2)
            self.widgets.tbtn_fullsize_preview.set_sensitive(False)
        else:
            self.widgets.ntb_main.set_current_page(0)
            self.widgets.tbtn_fullsize_preview.set_sensitive(True)

# =========================================================
# The homing functions
    def on_btn_homing_clicked(self, widget, data=None):
        self.widgets.ntb_button.set_current_page(_BB_HOME)

    def on_btn_home_all_clicked(self, widget, data=None):
        self.set_motion_mode(0)
        # home -1 means all
        self.command.home(-1)

    def on_btn_unhome_all_clicked(self, widget, data=None):
        self.set_motion_mode(0)
        # -1 for all
        self.command.unhome(-1)

    def on_btn_home_selected_clicked(self, widget, data=None):
        if self.stat.kinematics_type == linuxcnc.KINEMATICS_IDENTITY:
            # we can switch without any risk to joint mode and home the selected joint
            # but if the machine is a gantry, we need to do special check, as
            # on XYYZ machine joint 2 is not Z
            if widget == self.widgets.btn_home_x:
                if "x0" in self.joint_axis_dic:
                    joint = self.joint_axis_dic["x0"]
                else:
                    joint = self.joint_axis_dic["x"]
            elif widget == self.widgets.btn_home_y:
                if "y0" in self.joint_axis_dic:
                    joint = self.joint_axis_dic["y0"]
                else:
                    joint = self.joint_axis_dic["y"]
            elif widget == self.widgets.btn_home_z:
                if "z0" in self.joint_axis_dic:
                    joint = self.joint_axis_dic["z0"]
                else:
                    joint = self.joint_axis_dic["z"]
            elif widget == self.widgets.btn_home_4:
                joint = self.joint_axis_dic[self.axisletter_four]
            elif widget == self.widgets.btn_home_5:
                joint = self.joint_axis_dic[self.axisletter_five]

        else:
            for button in range(0,8):
                if widget == self.widgets["btn_home_j{0}".format(button)]:
                    joint = button
                    break

        self.command.home(joint)
        
    def on_btn_sel_next_joints_clicked(self, widget, data=None):
        widget.hide()
        for item in range(0, 3):
            self.widgets["btn_home_j{0}".format(item)].hide()
        for item in range(5, 8):
            self.widgets["btn_home_j{0}".format(item)].show()
        self.widgets.btn_sel_prev_joints.show()

        #reorder the hardware button handling list
        self._replace_list_item(9, "btn_home_j0", "btn_sel_prev_joints")
        self._replace_list_item(9, "btn_home_j5", "btn_sel_next_joints")
        self._replace_list_item(9, "btn_home_j4", "btn_home_j6")
        self._replace_list_item(9, "btn_home_j3", "btn_home_j5")
        self._replace_list_item(9, "btn_home_j2", "btn_home_j4")
        self._replace_list_item(9, "btn_home_j1", "btn_home_j3")

    def on_btn_sel_prev_joints_clicked(self, widget, data=None):
        widget.hide()
        for item in range(0,3):
            self.widgets["btn_home_j{0}".format(item)].show()
        for item in range(5,8):
            self.widgets["btn_home_j{0}".format(item)].hide()
        self.widgets.btn_sel_next_joints.show()
        self._reset_joint_button_order_to_default()
        
    def _reset_joint_button_order_to_default(self):
        if self.stat.joints < 6:
            return
        self.h_tabs[_BB_HOME] = [(0, "btn_home_j_all"), (1, "btn_home_j0"), (2, "btn_home_j1"), (3, "btn_home_j2"), (4, "btn_home_j3"),
                   (5, "btn_home_j4"), (6, "btn_home_j5"), (7, "btn_sel_next_joints"), (8, "btn_unhome_j_all"), (9, "btn_back_joints")
        ]

# The homing functions
# =========================================================

    def _check_limits(self):
        for axis in self.axis_list:
            axisnumber = "xyzabcuvw".index(axis)
            if self.stat.limit[axisnumber] != 0:
                return True
        if self.widgets.chk_ignore_limits.get_active():
            self.widgets.chk_ignore_limits.set_active(False)
        return False

    def _ignore_limits(self, pin):
        self.widgets.chk_ignore_limits.set_active(pin.get())

    def on_chk_ignore_limits_toggled(self, widget, data=None):
        if self.widgets.chk_ignore_limits.get_active():
            if not self._check_limits():
                self._show_error((11, _("ERROR : No limit switch is active, ignore limits will not be set.")))
                return
            self.command.override_limits()

    def on_tbtn_fullsize_preview_toggled(self, widget, data=None):
        if widget.get_active():
            self.widgets.box_info.hide()
            self.widgets.vbx_jog.hide()
            self.widgets.gremlin.set_property("metric_units", self.widgets.Combi_DRO_x.metric_units)
            self.widgets.gremlin.set_property("enable_dro", True)
            if self.lathe_mode:
                self.widgets.gremlin.set_property("show_lathe_radius", not self.diameter_mode)
        else:
            self.widgets.box_info.show()
            self.widgets.vbx_jog.show()
            if not self.widgets.chk_show_dro.get_active():
                self.widgets.gremlin.set_property("enable_dro", False)
        
# =========================================================
# this are hal-tools copied from gsreen function
    def on_btn_show_hal_clicked(self, widget, data=None):
        p = os.popen("tclsh {0}/bin/halshow.tcl &".format(TCLPATH))

    def on_btn_calibration_clicked(self, widget, data=None):
        p = os.popen("tclsh {0}/bin/emccalib.tcl -- -ini {1} > /dev/null &".format(TCLPATH, sys.argv[2]), "w")

    def on_btn_hal_meter_clicked(self, widget, data=None):
        p = os.popen("halmeter &")

    def on_btn_status_clicked(self, widget, data=None):
        p = os.popen("linuxcnctop  > /dev/null &", "w")

    def on_btn_hal_scope_clicked(self, widget, data=None):
        p = os.popen("halscope  > /dev/null &", "w")

    def on_btn_classicladder_clicked(self, widget, data=None):
        if hal.component_exists("classicladder_rt"):
            p = os.popen("classicladder  &", "w")
        else:
            self.dialogs.warning_dialog(self, _("INFO:"),
                                   _("Classicladder real-time component not detected"))

# =========================================================
# spindle stuff

    def _update_spindle(self):
        if self.stat.spindle[0]['direction'] > 0:
            self.widgets.rbt_forward.set_active(True)
        elif self.stat.spindle[0]['direction'] < 0:
            self.widgets.rbt_reverse.set_active(True)
        elif not self.widgets.rbt_stop.get_active():
            self.widgets.rbt_stop.set_active(True)

        # this is needed, because otherwise a command S0 would not set active btn_stop
        if not abs(self.stat.spindle[0]['speed']):
            self.widgets.rbt_stop.set_active(True)
            return

        # set the speed label in active code frame
        if self.stat.spindle[0]['speed'] == 0:
            speed = self.stat.settings[2]
        else:
            speed = self.stat.spindle[0]['speed']
        self.widgets.active_speed_label.set_label("{0:.0f}".format(abs(speed)))
        self.widgets.lbl_spindle_act.set_text("S {0}".format(int(speed * self.spindle_override)))

    def _update_vc(self):
        if self.stat.spindle[0]['direction'] != 0:
            if self.stat.spindle[0]['speed'] == 0:
                speed = self.stat.settings[2]
            else:
                speed = self.stat.spindle[0]['speed']
            if not self.lathe_mode:
                diameter = self.halcomp["tool-diameter"]
            else:
                diameter = int(self.widgets.Combi_DRO_x.get_position()[1] * 2)
            vc = abs(int(speed * self.spindle_override) * diameter * 3.14 / 1000)
        else:
            vc = 0
        if vc >= 100:
            text = "Vc= {0:d}".format(int(vc))
        elif vc >= 10:
            text = "Vc= {0:2.1f}".format(vc)
        else:
            text = "Vc= {0:.2f}".format(vc)
        self.widgets.lbl_vc.set_text(text)

    def on_rbt_forward_clicked(self, widget, data=None):
        if widget.get_active():
            widget.set_image(self.widgets.img_forward_on)
            self._set_spindle("forward")
        else:
            self.widgets.rbt_forward.set_image(self.widgets.img_forward)

    def on_rbt_reverse_clicked(self, widget, data=None):
        if widget.get_active():
            widget.set_image(self.widgets.img_reverse_on)
            self._set_spindle("reverse")
        else:
            widget.set_image(self.widgets.img_reverse)

    def on_rbt_stop_clicked(self, widget, data=None):
        if widget.get_active():
            widget.set_image(self.widgets.img_stop_on)
            self._set_spindle("stop")
        else:
            self.widgets.rbt_stop.set_image(self.widgets.img_sstop)

    def _set_spindle(self, command):
        # if we are in estop state, we will have to leave here, otherwise
        # we get an error, that switching spindle off is not allowed with estop
        if self.stat.task_state == linuxcnc.STATE_ESTOP:
            return

        # if we do not check this, we will get an error in auto mode and sub
        # calls from MDI containing i.e. G96 would not run, as the speed will
        # be setted to the commanded value due the next code part
        if self.stat.task_mode != linuxcnc.MODE_MANUAL:
            if self.stat.interp_state == linuxcnc.INTERP_READING or self.stat.interp_state == linuxcnc.INTERP_WAITING:
                if self.stat.spindle[0]['direction'] > 0:
                    self.widgets.rbt_forward.set_sensitive(True)
                    self.widgets.rbt_reverse.set_sensitive(False)
                    self.widgets.rbt_stop.set_sensitive(False)
                elif self.stat.spindle[0]['direction'] < 0:
                    self.widgets.rbt_forward.set_sensitive(False)
                    self.widgets.rbt_reverse.set_sensitive(True)
                    self.widgets.rbt_stop.set_sensitive(False)
                else:
                    self.widgets.rbt_forward.set_sensitive(False)
                    self.widgets.rbt_reverse.set_sensitive(False)
                    self.widgets.rbt_stop.set_sensitive(True)
                return

        rpm = self._check_spindle_range()
        # as the commanded value will be multiplied with speed override,
        # we take care of that but we have to check for speed override 
        # to not be zero to avoid division by zero error
        try:
            rpm_out = rpm / self.stat.spindle[0]['override']
        except:
            rpm_out = 0
        self.widgets.lbl_spindle_act.set_label("S {0}".format(int(rpm)))

        if command == "stop":
            self.command.spindle(0)
            self.widgets.lbl_spindle_act.set_label("S 0")
        elif command == "forward":
            self.command.spindle(1, rpm_out)
        elif command == "reverse":
            self.command.spindle(-1, rpm_out)
        else:
            print(_("Something went wrong, we have an unknown spindle widget {0}").format(command))

    def _check_spindle_range(self):
        rpm = (self.stat.settings[2])
        if rpm == 0:
            rpm = abs(self.spindle_start_rpm)

        spindle_override = self.widgets.spc_spindle.get_value() / 100
        real_spindle_speed = rpm * spindle_override

        if real_spindle_speed > self.max_spindle_rev:
            real_spindle_speed = self.max_spindle_rev
        elif real_spindle_speed < self.min_spindle_rev:
            real_spindle_speed = self.min_spindle_rev
        return real_spindle_speed

    def on_btn_spindle_100_clicked(self, widget, data=None):
        self.widgets.spc_spindle.set_value(100)

    def on_spc_spindle_value_changed(self, widget, data=None):
        if not self.initialized:
            return
        # this is in a try except, because on initializing the window the values are still zero
        # so we would get an division / zero error
        real_spindle_speed = 0
        value = widget.get_value()
        try:
            if not abs(self.stat.settings[2]):
                if self.widgets.rbt_forward.get_active() or self.widgets.rbt_reverse.get_active():
                    speed = self.stat.spindle[0]['speed']
                else:
                    speed = 0
            else:
                speed = abs(self.stat.spindle[0]['speed'])
            spindle_override = value / 100
            real_spindle_speed = speed * spindle_override
            if real_spindle_speed > self.max_spindle_rev:
                value_to_set = value / (real_spindle_speed / self.max_spindle_rev)
                real_spindle_speed = self.max_spindle_rev
            elif real_spindle_speed < self.min_spindle_rev:
                value_to_set = value / (real_spindle_speed / self.min_spindle_rev)
                real_spindle_speed = self.min_spindle_rev
            else:
                value_to_set = spindle_override * 100
            widget.set_value(value_to_set)
            self.spindle_override = value_to_set / 100
            self.command.spindleoverride(value_to_set / 100)
        except:
            pass

    def on_adj_start_spindle_RPM_value_changed(self, widget, data=None):
        self.spindle_start_rpm = widget.get_value()
        self.prefs.putpref("spindle_start_rpm", self.spindle_start_rpm, float)

    def on_adj_spindle_bar_min_value_changed(self, widget, data=None):
        self.min_spindle_rev = widget.get_value()
        self.prefs.putpref("spindle_bar_min", self.min_spindle_rev, float)
        self.widgets.spindle_feedback_bar.set_property("min", self.min_spindle_rev)

    def on_adj_spindle_bar_max_value_changed(self, widget, data=None):
        self.max_spindle_rev = widget.get_value()
        self.prefs.putpref("spindle_bar_max", self.max_spindle_rev, float)
        self.widgets.spindle_feedback_bar.set_property("max", self.max_spindle_rev)

# =========================================================
# Coolant an mist coolant button
    def on_tbtn_flood_toggled(self, widget, data=None):
        if self.stat.flood and self.widgets.tbtn_flood.get_active():
            return
        elif not self.stat.flood and not self.widgets.tbtn_flood.get_active():
            return
        elif self.widgets.tbtn_flood.get_active():
            self.widgets.tbtn_flood.set_image(self.widgets.img_coolant_on)
            self.command.flood(linuxcnc.FLOOD_ON)
        else:
            self.widgets.tbtn_flood.set_image(self.widgets.img_coolant_off)
            self.command.flood(linuxcnc.FLOOD_OFF)

    def on_tbtn_mist_toggled(self, widget, data=None):
        if self.stat.mist and self.widgets.tbtn_mist.get_active():
            return
        elif not self.stat.mist and not self.widgets.tbtn_mist.get_active():
            return
        elif self.widgets.tbtn_mist.get_active():
            self.widgets.tbtn_mist.set_image(self.widgets.img_mist_on)
            self.command.mist(linuxcnc.MIST_ON)
        else:
            self.widgets.tbtn_mist.set_image(self.widgets.img_mist_off)
            self.command.mist(linuxcnc.MIST_OFF)

# =========================================================
# feed stuff
    def on_spc_feed_value_changed(self, widget, data=None):
        if not self.initialized:
            return
        value = widget.get_value() / 100
        self.feed_override = value
        self.command.feedrate(value)

    def on_btn_feed_100_clicked(self, widget, data=None):
        self.widgets.spc_feed.set_value(100)

    def on_spc_rapid_value_changed(self, widget, data=None):
        if not self.initialized:
            return
        value = widget.get_value() / 100
        self.rapidrate = value
        self.command.rapidrate(value)

    # this are the MDI thinks we need
    def on_btn_delete_clicked(self, widget, data=None):
        message = _("Do you really want to delete the MDI history?\n")
        message += _("this will not delete the MDI History file, but will\n")
        message += _("delete the listbox entries for this session")
        result = self.dialogs.yesno_dialog(self, message, _("Attention!!"))
        if result:
            self.widgets.hal_mdihistory.model.clear()

    def on_tbtn_use_screen2_toggled(self, widget, data=None):
        self.prefs.putpref("use_screen2", widget.get_active())
        if widget.get_active():
            self.widgets.window2.show()
            if self.widgets.rbtn_window.get_active():
                try:
                    pos = self.widgets.window1.get_position()
                    size = self.widgets.window1.get_size()
                    left = pos[0] + size[0]
                    self.widgets.window2.move(left, pos[1])
                except:
                    pass
        else:
            self.widgets.window2.hide()

    def on_btn_show_kbd_clicked(self, widget, data=None):
        # if the image is img_brake macro, we want to interrupt the running macro
        if self.widgets.btn_show_kbd.get_image() == self.widgets.img_brake_macro:
            self.command.abort()
            for btn in self.macrobuttons:
                btn.set_sensitive(True)
            if self.onboard:
                self.widgets.btn_show_kbd.set_image(self.widgets.img_keyboard)
                self.widgets.btn_show_kbd.set_property("tooltip-text", _("This button will show or hide the keyboard"))
            else:
                self.widgets.btn_show_kbd.set_sensitive(False)
                
        elif self.widgets.ntb_info.get_current_page() == 1:
            self.widgets.ntb_info.set_current_page(0)
        else:
            self.widgets.ntb_info.set_current_page(1)
        # special case if we are in edit mode
        if self.widgets.ntb_button.get_current_page() == _BB_EDIT:
            if self.widgets.ntb_info.get_visible():
                self.widgets.box_info.set_size_request(-1, 50)
                self.widgets.ntb_info.hide()
            else:
                self.widgets.box_info.set_size_request(-1, 250)
                self.widgets.ntb_info.show()

    def on_ntb_info_switch_page(self, widget, page, page_num, data=None):
        if self.stat.task_mode == linuxcnc.MODE_MDI:
            self.widgets.hal_mdihistory.entry.grab_focus()
        elif self.stat.task_mode == linuxcnc.MODE_AUTO:
            self.widgets.gcode_view.grab_focus()

    # Three back buttons to be able to leave notebook pages
    # All use the same callback offset
    def on_btn_back_clicked(self, widget, data=None):
        if self.widgets.ntb_button.get_current_page() == _BB_EDIT:  # edit mode, go back to auto_buttons
            self.widgets.ntb_button.set_current_page(_BB_AUTO)
            if self.widgets.tbtn_fullsize_preview1.get_active():
                self.widgets.vbx_jog.set_visible(False)
        elif self.widgets.ntb_button.get_current_page() == _BB_LOAD_FILE:  # File selection mode
            self.widgets.ntb_button.set_current_page(_BB_AUTO)
        else:  # else we go to main button on manual
            self.widgets.ntb_button.set_current_page(_BB_MANUAL)
            self.widgets.ntb_main.set_current_page(0)
            self.widgets.ntb_preview.set_current_page(0)

    # The offset settings, set to zero
    def on_btn_touch_clicked(self, widget, data=None):
        self.widgets.ntb_button.set_current_page(_BB_TOUCH_OFF)
        self._show_offset_tab(True)
        if self.widgets.rbtn_show_preview.get_active():
            self.widgets.ntb_preview.set_current_page(0)

    def on_tbtn_edit_offsets_toggled(self, widget, data=None):
        state = widget.get_active()
        self.widgets.offsetpage1.edit_button.set_active(state)
        widgetlist = ["btn_set_value_x", "btn_set_value_y", "btn_set_value_z", 
                      "btn_set_selected", "ntb_jog", "btn_set_selected", 
                      "btn_zero_g92","rbt_mdi","rbt_auto","tbtn_setup"
                      ]
        if self.widgets.tbtn_user_tabs.get_sensitive():
            widgetlist.append("tbtn_user_tabs")
        self._sensitize_widgets(widgetlist, not state)

        if state:
            self.widgets.ntb_preview.set_current_page(1)
        else:
            self.widgets.ntb_preview.set_current_page(0)

        # we have to replace button calls in our list to make all hardware button
        # activate the correct button call
        if state and self.widgets.chk_use_tool_measurement.get_active():
            self.widgets.btn_zero_g92.show()
            self.widgets.btn_block_height.hide()
            self._replace_list_item(4, "btn_block_height", "btn_zero_g92")
        elif not state and self.widgets.chk_use_tool_measurement.get_active():
            self.widgets.btn_zero_g92.hide()
            self.widgets.btn_block_height.show()
            self._replace_list_item(4, "btn_zero_g92", "btn_block_height")

        if not state:  # we must switch back to manual mode, otherwise jogging is not possible
            self.command.mode(linuxcnc.MODE_MANUAL)
            self.command.wait_complete()

        # show virtual keyboard?
        if state and self.widgets.chk_use_kb_on_offset.get_active():
            self.widgets.ntb_info.set_current_page(1)
            self.widgets.ntb_preview.set_current_page(1)

    def on_btn_zero_g92_clicked(self, widget, data=None):
        self.command.mode(linuxcnc.MODE_MDI)
        self.command.wait_complete()
        self.command.mdi("G92.1")
        self.command.mode(linuxcnc.MODE_MANUAL)
        self.command.wait_complete()
        self.widgets.btn_touch.emit("clicked")

    def on_btn_set_value_clicked(self, widget, data=None):
        if widget == self.widgets.btn_set_value_x:
            axis = "x"
        elif widget == self.widgets.btn_set_value_y:
            axis = "y"
        elif widget == self.widgets.btn_set_value_z:
            axis = "z"
        elif widget == self.widgets.btn_set_value_4:
            axis = self.axisletter_four
        elif widget == self.widgets.btn_set_value_5:
            axis = self.axisletter_five
        else:
            axis = "Unknown"
            message = _("Offset {0} could not be set, because off unknown axis").format(axis)
            self.dialogs.warning_dialog(self, _("Wrong offset setting!"), message)
            return
        if self.lathe_mode and axis =="x":
            if self.diameter_mode:
                preset = self.prefs.getpref("diameter offset_axis_{0}".format(axis), 0, float)
                offset = self.dialogs.entry_dialog(self, data=preset, header=_("Enter value for diameter"),
                                                   label=_("Set diameter to:"), integer=False)
            else:
                preset = self.prefs.getpref("radius offset_axis_{0}".format(axis), 0, float)
                offset = self.dialogs.entry_dialog(self, data=preset, header=_("Enter value for radius"),
                                                   label=_("Set radius to:"), integer=False)
        else:
            preset = self.prefs.getpref("offset_axis_{0}".format(axis), 0, float)
            offset = self.dialogs.entry_dialog(self, data=preset, header=_("Enter value for axis {0}").format(axis),
                                               label=_("Set axis {0} to:").format(axis), integer=False)
        if offset == "CANCEL":
            return
        elif offset == "ERROR":
            print(_("Conversion error in btn_set_value"))
            self.dialogs.warning_dialog(self, _("Conversion error in btn_set_value!"),
                                   _("Please enter only numerical values. Values have not been applied"))
        else:
            self.command.mode(linuxcnc.MODE_MDI)
            self.command.wait_complete()
            command = "G10 L20 P0 {0}{1:f}".format(axis, offset)
            self.command.mdi(command)
            self.widgets.hal_action_reload.emit("activate")
            self.command.mode(linuxcnc.MODE_MANUAL)
            self.command.wait_complete()
            self.prefs.putpref("offset_axis_{0}".format(axis), offset, float)

    def on_btn_set_selected_clicked(self, widget, data=None):
        system, name = self.widgets.offsetpage1.get_selected()
        if not system:
            message = _("you did not selected a system to be changed to, so nothing will be changed")
            self.dialogs.warning_dialog(self, _("Important Warning!"), message)
            return
        if system == self.system_list[self.stat.g5x_index]:
            return
        else:
            self.command.mode(linuxcnc.MODE_MDI)
            self.command.wait_complete()
            self.command.mdi(system)
            self.command.mode(linuxcnc.MODE_MANUAL)
            self.command.wait_complete()

    def on_spbtn_probe_height_value_changed(self, widget, data=None):
        self.halcomp["probeheight"] = widget.get_value()
        self.prefs.putpref("probeheight", widget.get_value(), float)

    def on_spbtn_search_vel_value_changed(self, widget, data=None):
        self.halcomp["searchvel"] = widget.get_value()
        self.prefs.putpref("searchvel", widget.get_value(), float)

    def on_spbtn_probe_vel_value_changed(self, widget, data=None):
        self.halcomp["probevel"] = widget.get_value()
        self.prefs.putpref("probevel", widget.get_value(), float)

    def on_chk_use_tool_measurement_toggled(self, widget, data=None):
        if widget.get_active():
            self.widgets.eb_blockheight_label.show()
            self.widgets.frm_probe_pos.set_sensitive(True)
            self.widgets.frm_probe_vel.set_sensitive(True)
            self.halcomp["toolmeasurement"] = True
            self.halcomp["searchvel"] = self.widgets.spbtn_search_vel.get_value()
            self.halcomp["probevel"] = self.widgets.spbtn_probe_vel.get_value()
            self.halcomp["probeheight"] = self.widgets.spbtn_probe_height.get_value()
        else:
            self.widgets.eb_blockheight_label.hide()
            self.widgets.frm_probe_pos.set_sensitive(False)
            self.widgets.frm_probe_vel.set_sensitive(False)
            self.halcomp["toolmeasurement"] = False
            self.halcomp["searchvel"] = 0.0
            self.halcomp["probevel"] = 0.0
            self.halcomp["probeheight"] = 0.0
        self.prefs.putpref("use_toolmeasurement", widget.get_active())

    def on_chk_reload_tool_toggled(self, widget, data=None):
        state = widget.get_active()
        self.reload_tool_enabled = state
        self.prefs.putpref("reload_tool", state)

    def on_btn_block_height_clicked(self, widget, data=None):
        probeheight = self.widgets.spbtn_probe_height.get_value()
        preset = self.prefs.getpref("blockheight", 0.0, float)
        blockheight = self.dialogs.entry_dialog(self, data=preset, header=_("Enter the block height"),
                                                label=_("Block height measured from base table"), integer=False)

        if blockheight == "CANCEL" or blockheight == "ERROR":
            return
        if blockheight != False or blockheight == 0:
            self.halcomp["blockheight"] = blockheight
            self.halcomp["probeheight"] = probeheight
            self.prefs.putpref("blockheight", blockheight, float)
            self.prefs.putpref("probeheight", probeheight, float)
        else:
            self.prefs.putpref("blockheight", 0.0, float)
            self.prefs.putpref("probeheight", 0.0, float)
            self.dialogs.warning_dialog(self, _("Conversion error in btn_block_height!"),
                                        _("Please enter only numerical values\nValues have not been applied"))

        # set coordinate system to new origin
        origin = self.get_ini_info.get_axis_2_min_limit() + blockheight
        self.command.mode(linuxcnc.MODE_MDI)
        self.command.wait_complete()
        self.command.mdi("G10 L2 P0 Z{0}".format(origin))
        self.widgets.hal_action_reload.emit("activate")
        self.command.mode(linuxcnc.MODE_MANUAL)
        self.command.wait_complete()

    # choose a theme to aply
    def on_theme_choice_changed(self, widget):
        theme = widget.get_active_text()
        if theme == None:
            return
        self.prefs.putpref('gtk_theme', theme)
        settings = gtk.settings_get_default()
        if theme == "Follow System Theme":
            theme = self.default_theme
        settings.set_string_property("gtk-theme-name", theme, "")

    def on_rbt_unlock_toggled(self, widget, data=None):
        if widget.get_active():
            if widget == self.widgets.rbt_use_unlock:
                self.prefs.putpref("unlock_way", "use")
            elif widget == self.widgets.rbt_no_unlock:
                self.prefs.putpref("unlock_way", "no")
            else:
                self.prefs.putpref("unlock_way", "hal")

    def on_rbtn_run_from_line_toggled(self, widget, data=None):
        if widget.get_active():
            if widget == self.widgets.rbtn_no_run_from_line:
                self.prefs.putpref("run_from_line", "no_run")
                self.widgets.btn_from_line.set_sensitive(False)
            else:  # widget == self.widgets.rbtn_run_from_line:
                self.prefs.putpref("run_from_line", "run")
                self.widgets.btn_from_line.set_sensitive(True)

    def on_chk_use_kb_on_offset_toggled(self, widget, data=None):
        self.prefs.putpref("show_keyboard_on_offset", widget.get_active())

    def on_chk_use_kb_on_tooledit_toggled(self, widget, data=None):
        self.prefs.putpref("show_keyboard_on_tooledit", widget.get_active())

    def on_chk_use_kb_on_edit_toggled(self, widget, data=None):
        self.prefs.putpref("show_keyboard_on_edit", widget.get_active())

    def on_chk_use_kb_on_mdi_toggled(self, widget, data=None):
        self.prefs.putpref("show_keyboard_on_mdi", widget.get_active())

    def on_chk_use_kb_on_file_selection_toggled(self, widget, data=None):
        self.prefs.putpref("show_keyboard_on_file_selection", widget.get_active())

    def on_chk_use_kb_shortcuts_toggled(self, widget, data=None):
        self.prefs.putpref("use_keyboard_shortcuts", widget.get_active())

    def on_rbtn_show_preview_toggled(self, widget, data=None):
        self.prefs.putpref("show_preview_on_offset", widget.get_active())

    def on_adj_scale_jog_vel_value_changed(self, widget, data=None):
        self.prefs.putpref("scale_jog_vel", widget.get_value(), float)
        self.scale_jog_vel = widget.get_value()

    def on_adj_scale_feed_override_value_changed(self, widget, data=None):
        self.prefs.putpref("scale_feed_override", widget.get_value(), float)
        self.scale_feed_override = widget.get_value()

    def on_adj_scale_rapid_override_value_changed(self, widget, data=None):
        self.prefs.putpref("scale_rapid_override", widget.get_value(), float)
        self.scale_rapid_override = widget.get_value()

    def on_adj_scale_spindle_override_value_changed(self, widget, data=None):
        self.prefs.putpref("scale_spindle_override", widget.get_value(), float)
        self.scale_spindle_override = widget.get_value()

    def on_rbtn_fullscreen_toggled(self, widget):
        if widget.get_active():
            self.widgets.window1.fullscreen()
            self.prefs.putpref("screen1", "fullscreen")
        else:
            self.widgets.window1.unfullscreen()

    def on_rbtn_maximized_toggled(self, widget):
        if widget.get_active():
            self.widgets.window1.maximize()
            self.prefs.putpref("screen1", "maximized")
        else:
            self.widgets.window1.unmaximize()

    def on_rbtn_window_toggled(self, widget):
        self.widgets.spbtn_x_pos.set_sensitive(widget.get_active())
        self.widgets.spbtn_y_pos.set_sensitive(widget.get_active())
        self.widgets.spbtn_width.set_sensitive(widget.get_active())
        self.widgets.spbtn_height.set_sensitive(widget.get_active())
        # we have to check also if the window is active, because the button is toggled the first time
        # before the window is shown
        if widget.get_active() and self.widgets.window1.is_active():
            self.widgets.window1.move(self.xpos, self.ypos)
            self.widgets.window1.resize(self.width, self.height)
            self.prefs.putpref("screen1", "window")

    def on_adj_x_pos_value_changed(self, widget, data=None):
        if not self.initialized:
            return
        value = int(widget.get_value())
        self.prefs.putpref("x_pos", value, float)
        self.xpos = value
        self.widgets.window1.move(value, self.ypos)

    def on_adj_y_pos_value_changed(self, widget, data=None):
        if not self.initialized:
            return
        value = int(widget.get_value())
        self.prefs.putpref("y_pos", value, float)
        self.ypos = value
        self.widgets.window1.move(self.xpos, value)

    def on_adj_width_value_changed(self, widget, data=None):
        if not self.initialized:
            return
        value = int(widget.get_value())
        self.prefs.putpref("width", value, float)
        self.width = value
        self.widgets.window1.resize(value, self.height)

    def on_adj_height_value_changed(self, widget, data=None):
        if not self.initialized:
            return
        value = int(widget.get_value())
        self.prefs.putpref("height", value, float)
        self.height = value
        self.widgets.window1.resize(self.width, value)

    def on_adj_dro_size_value_changed(self, widget, data=None):
        value = int(widget.get_value())
        self.prefs.putpref("dro_size", value, int)
        self.dro_size = value

        for axis in self.axis_list:
            size = self.dro_size
            if axis == self.axisletter_four:
                axis = 4
                size = int(size * 0.75)
            if axis == self.axisletter_five:
                axis = 5
                size = int(size * 0.75)
            self.widgets["Combi_DRO_{0}".format(axis)].set_property("font_size", size)
            if self.lathe_mode:
                self.widgets.Combi_DRO_y.set_property("font_size", size)

    def on_chk_hide_cursor_toggled(self, widget, data=None):
        self.prefs.putpref("hide_cursor", widget.get_active())
        self.hide_cursor = widget.get_active()
        if widget.get_active():
            self.widgets.window1.window.set_cursor(INVISABLE)
        else:
            self.widgets.window1.window.set_cursor(None)
        self.abs_color = self.prefs.getpref("abs_color", "blue", str)
        self.rel_color = self.prefs.getpref("rel_color", "black", str)
        self.dtg_color = self.prefs.getpref("dtg_color", "yellow", str)
        self.homed_color = self.prefs.getpref("homed_color", "green", str)
        self.unhomed_color = self.prefs.getpref("unhomed_color", "red", str)

    def on_rel_colorbutton_color_set(self, widget):
        color = widget.get_color()
        self.prefs.putpref('rel_color', color)
        self._change_dro_color("rel_color", color)
        self.rel_color = str(color)

    def on_abs_colorbutton_color_set(self, widget):
        color = widget.get_color()
        self.prefs.putpref('abs_color', widget.get_color())
        self._change_dro_color("abs_color", color)
        self.abs_color = str(color)

    def on_dtg_colorbutton_color_set(self, widget):
        color = widget.get_color()
        self.prefs.putpref('dtg_color', widget.get_color())
        self._change_dro_color("dtg_color", color)
        self.dtg_color = str(color)

    def on_homed_colorbtn_color_set(self, widget):
        color = widget.get_color()
        self.prefs.putpref('homed_color', widget.get_color())
        self._change_dro_color("homed_color", color)
        self.homed_color = str(color)

    def on_unhomed_colorbtn_color_set(self, widget):
        color = widget.get_color()
        self.prefs.putpref('unhomed_color', widget.get_color())
        self._change_dro_color("unhomed_color", color)
        self.unhomed_color = str(color)

    def on_file_to_load_chooser_file_set(self, widget):
        self.prefs.putpref("open_file", widget.get_filename())

    def on_jump_to_dir_chooser_file_set(self, widget, data=None):
        path = widget.get_filename()
        self.prefs.putpref("jump_to_dir", path)
        self.widgets.IconFileSelection1.set_property("jump_to_dir", path)

    def on_grid_size_value_changed(self, widget, data=None):
        self.widgets.gremlin.set_property('grid_size', widget.get_value())
        self.prefs.putpref('grid_size', widget.get_value(), float)

    def on_tbtn_log_actions_toggled(self, widget, data=None):
        self.prefs.putpref("log_actions", widget.get_active())

    def on_chk_show_dro_toggled(self, widget, data=None):
        self.widgets.gremlin.set_property("metric_units", self.widgets.Combi_DRO_x.metric_units)
        self.widgets.gremlin.set_property("enable_dro", widget.get_active())
        self.prefs.putpref("enable_dro", widget.get_active())
        self.widgets.chk_show_offsets.set_sensitive(widget.get_active())
        self.widgets.chk_show_dtg.set_sensitive(widget.get_active())

    def on_chk_show_dtg_toggled(self, widget, data=None):
        self.widgets.gremlin.set_property("show_dtg", widget.get_active())
        self.prefs.putpref("show_dtg", widget.get_active())

    def on_chk_show_offsets_toggled(self, widget, data=None):
        self.widgets.gremlin.show_offsets = widget.get_active()
        self.prefs.putpref("show_offsets", widget.get_active())

    def on_cmb_mouse_button_mode_changed(self, widget):
        index = widget.get_active()
        self.widgets.gremlin.set_property("mouse_btn_mode", index)
        self.prefs.putpref("mouse_btn_mode", index, int)

# =========================================================
# tool stuff
    # This is used to reload the tool in spindle after starting the GUI
    # This is called from the all_homed_signal
    def reload_tool(self):
        tool_to_load = self.prefs.getpref("tool_in_spindle", 0, int)
        if tool_to_load == 0:
            return
        self.load_tool = True
        self.tool_change = True

        self.command.mode(linuxcnc.MODE_MDI)
        self.command.wait_complete()

        command = "M61 Q {0} G43".format(tool_to_load)
        self.command.mdi(command)
        self.command.wait_complete()

    def on_btn_tool_clicked(self, widget, data=None):
        if self.widgets.tbtn_fullsize_preview.get_active():
            self.widgets.tbtn_fullsize_preview.set_active(False)
        self.widgets.ntb_button.set_current_page(_BB_TOOL)
        self._show_tooledit_tab(True)

    # Here we create a manual tool change dialog
    def on_tool_change(self, widget):
        change = self.halcomp['toolchange-change']
        toolnumber = self.halcomp['toolchange-number']
        if change:
            # if toolnumber = 0 we will get an error because we will not be able to get
            # any tool description, so we avoid that case
            if toolnumber == 0:
                message = _("Please remove the mounted tool and press OK when done")
            else:
                tooldescr = self.widgets.tooledit1.get_toolinfo(toolnumber)[16]
                message = _("Please change to tool\n\n# {0:d}     {1}\n\n then click OK.").format(toolnumber, tooldescr)
            result = self.dialogs.warning_dialog(self, message, title=_("Manual Tool change"))
            if result:
                self.halcomp["toolchange-changed"] = True
            else:
                print"toolchange abort", self.stat.tool_in_spindle, self.halcomp['toolchange-number']
                self.command.abort()
                self.halcomp['toolchange-number'] = self.stat.tool_in_spindle
                self.halcomp['toolchange-change'] = False
                self.halcomp['toolchange-changed'] = True
                message = _("Tool Change has been aborted!\n")
                message += _("The old tool will remain set!")
                self.dialogs.warning_dialog(self, message)
        else:
            self.halcomp['toolchange-changed'] = False

    def on_btn_delete_tool_clicked(self, widget, data=None):
        act_tool = self.stat.tool_in_spindle
        if act_tool == self.widgets.tooledit1.get_selected_tool():
            message = _("You are trying to delete the tool mounted in the spindle\n")
            message += _("This is not allowed, please change tool prior to delete it")
            self.dialogs.warning_dialog(self, _("Warning Tool can not be deleted!"), message)
            return

        self.widgets.tooledit1.delete(None)
        self.widgets.tooledit1.set_selected_tool(act_tool)

    def on_btn_add_tool_clicked(self, widget, data=None):
        self.widgets.tooledit1.add(None)

    def on_btn_reload_tooltable_clicked(self, widget, data=None):
        self.widgets.tooledit1.reload(None)
        self.widgets.tooledit1.set_selected_tool(self.stat.tool_in_spindle)

    def on_btn_apply_tool_changes_clicked(self, widget, data=None):
        self.widgets.tooledit1.save(None)
        self.widgets.tooledit1.set_selected_tool(self.stat.tool_in_spindle)

    def on_btn_tool_touchoff_clicked(self, widget, data=None):
        if not self.widgets.tooledit1.get_selected_tool():
            message = _("No or more than one tool selected in tool table")
            message += _("Please select only one tool in the table")
            self.dialogs.warning_dialog(self, _("Warning Tool Touch off not possible!"), message)
            return

        if self.widgets.tooledit1.get_selected_tool() != self.stat.tool_in_spindle:
            message = _("you can not touch of a tool, witch is not mounted in the spindle")
            message += _("your selection has been reseted to the tool in spindle")
            self.dialogs.warning_dialog(self, _("Warning Tool Touch off not possible!"), message)
            self.widgets.tooledit1.reload(self)
            self.widgets.tooledit1.set_selected_tool(self.stat.tool_in_spindle)
            return

        if "G41" in self.active_gcodes or "G42" in self.active_gcodes:
            message = _("Tool touch off is not possible with cutter radius compensation switched on!\n")
            message += _("Please emit an G40 before tool touch off")
            self.dialogs.warning_dialog(self, _("Warning Tool Touch off not possible!"), message)
            return

        if widget == self.widgets.btn_tool_touchoff_x:
            axis = "x"
        elif widget == self.widgets.btn_tool_touchoff_z:
            axis = "z"
        else:
            self.dialogs.warning_dialog(self, _("Real big error!"),
                                   _("You managed to come to a place that is not possible in on_btn_tool_touchoff"))
            return

        value = self.dialogs.entry_dialog(self, data=None,
                                     header=_("Enter value for axis {0} to set:").format(axis.upper()),
                                     label=_("Set parameter of tool {0:d} and axis {1} to:").format(self.stat.tool_in_spindle, axis.upper()),
                                     integer=False)

        if value == "ERROR":
            message = _("Conversion error because of wrong entry for touch off axis {0}").format(axis.upper())
            self.dialogs.warning_dialog(self, _("Conversion error !"), message)
            return
        elif value == "CANCEL":
            return
        else:
            command = "G10 L10 P{0} {1}{2}".format(self.stat.tool_in_spindle, axis, value)
            self.command.mode(linuxcnc.MODE_MDI)
            self.command.wait_complete()
            self.command.mdi(command)
            self.command.wait_complete()
            if "G43" in self.active_gcodes:
                self.command.mdi("G43")
                self.command.wait_complete()
            self.command.mode(linuxcnc.MODE_MANUAL)
            self.command.wait_complete()

    # select a tool entering a number
    def on_btn_select_tool_by_no_clicked(self, widget, data=None):
        value = self.dialogs.entry_dialog(self, data=None, header=_("Enter the tool number as integer "),
                                     label=_("Select the tool to change"), integer=True)
        if value == "ERROR":
            message = _("Conversion error because of wrong entry for tool number\n")
            message += _("enter only integer numbers")
            self.dialogs.warning_dialog(self, _("Conversion error !"), message)
            return
        elif value == "CANCEL":
            return
        elif int(value) == self.stat.tool_in_spindle:
            message = _("Selected tool is already in spindle, no change needed.")
            self.dialogs.warning_dialog(self, _("Important Warning!"), message)
            return
        else:
            self.tool_change = True
            self.command.mode(linuxcnc.MODE_MDI)
            self.command.wait_complete()
            command = "T{0} M6".format(int(value))
            self.command.mdi(command)

    # set tool with M61 Q? or with T? M6
    def on_btn_selected_tool_clicked(self, widget, data=None):
        tool = self.widgets.tooledit1.get_selected_tool()
        if tool == None:
            message = _("you selected no or more than one tool, the tool selection must be unique")
            self.dialogs.warning_dialog(self, _("Important Warning!"), message)
            return
        if tool == self.stat.tool_in_spindle:
            message = _("Selected tool is already in spindle, no change needed.")
            self.dialogs.warning_dialog(self, _("Important Warning!"), message)
            return
        if tool or tool == 0:
            self.tool_change = True
            tool = int(tool)
            self.command.mode(linuxcnc.MODE_MDI)
            self.command.wait_complete()

            if widget == self.widgets.btn_change_tool:
                command = "T{0} M6".format(tool)
            else:
                command = "M61 Q{0}".format(tool)
            self.command.mdi(command)
        else:
            message = _("Could not understand the entered tool number. Will not change anything")
            self.dialogs.warning_dialog(self, _("Important Warning!"), message)

# =========================================================
# gremlin relevant calls
    def on_rbt_view_p_toggled(self, widget, data=None):
        if self.widgets.rbt_view_p.get_active():
            self.widgets.gremlin.set_property("view", "p")
        self.prefs.putpref("gremlin_view", "rbt_view_p")

    def on_rbt_view_x_toggled(self, widget, data=None):
        if self.widgets.rbt_view_x.get_active():
            self.widgets.gremlin.set_property("view", "x")
        self.prefs.putpref("gremlin_view", "rbt_view_x")

    def on_rbt_view_y_toggled(self, widget, data=None):
        if self.widgets.rbt_view_y.get_active():
            self.widgets.gremlin.set_property("view", "y")
        self.prefs.putpref("gremlin_view", "rbt_view_y")

    def on_rbt_view_z_toggled(self, widget, data=None):
        if self.widgets.rbt_view_z.get_active():
            self.widgets.gremlin.set_property("view", "z")
        self.prefs.putpref("gremlin_view", "rbt_view_z")

    def on_rbt_view_y2_toggled(self, widget, data=None):
        if self.widgets.rbt_view_y2.get_active():
            self.widgets.gremlin.set_property("view", "y2")
        self.prefs.putpref("gremlin_view", "rbt_view_y2")

    def on_btn_zoom_in_clicked(self, widget, data=None):
        self.widgets.gremlin.zoom_in()

    def on_btn_zoom_out_clicked(self, widget, data=None):
        self.widgets.gremlin.zoom_out()

    def on_btn_delete_view_clicked(self, widget, data=None):
        self.widgets.gremlin.clear_live_plotter()

    def on_tbtn_view_dimension_toggled(self, widget, data=None):
        self.widgets.gremlin.set_property("show_extents_option", widget.get_active())
        self.prefs.putpref("view_dimension", self.widgets.tbtn_view_dimension.get_active())

    def on_tbtn_view_tool_path_toggled(self, widget, data=None):
        self.widgets.gremlin.set_property("show_live_plot", widget.get_active())
        self.prefs.putpref("view_tool_path", self.widgets.tbtn_view_tool_path.get_active())

    def on_gremlin_line_clicked(self, widget, line):
        self.widgets.gcode_view.set_line_number(line)

    def on_btn_load_clicked(self, widget, data=None):
        self.widgets.ntb_button.set_current_page(_BB_LOAD_FILE)
        self.widgets.ntb_preview.set_current_page(3)
        self.widgets.tbtn_fullsize_preview.set_active(True)
        self._show_iconview_tab(True)
        self.widgets.IconFileSelection1.refresh_filelist()
        self.widgets.IconFileSelection1.iconView.grab_focus()
        self.gcodeerror = ""

    def on_btn_sel_next_clicked(self, widget, data=None):
        self.widgets.IconFileSelection1.btn_sel_next.emit("clicked")

    def on_btn_sel_prev_clicked(self, widget, data=None):
        self.widgets.IconFileSelection1.btn_sel_prev.emit("clicked")

    def on_btn_home_clicked(self, widget, data=None):
        self.widgets.IconFileSelection1.btn_home.emit("clicked")

    def on_btn_jump_to_clicked(self, widget, data=None):
        self.widgets.IconFileSelection1.btn_jump_to.emit("clicked")

    def on_btn_dir_up_clicked(self, widget, data=None):
        self.widgets.IconFileSelection1.btn_dir_up.emit("clicked")

    def on_btn_select_clicked(self, widget, data=None):
        self.widgets.IconFileSelection1.btn_select.emit("clicked")

    def on_IconFileSelection1_selected(self, widget, path=None):
        if path:
            self.widgets.hal_action_open.load_file(path)
            self.widgets.ntb_preview.set_current_page(0)
            self.widgets.tbtn_fullsize_preview.set_active(False)
            self.widgets.ntb_button.set_current_page(_BB_AUTO)
            self._show_iconview_tab(False)

    def on_IconFileSelection1_sensitive(self, widget, buttonname, state):
        self.widgets[buttonname].set_sensitive(state)

    def on_IconFileSelection1_exit(self, widget):
        self.widgets.ntb_preview.set_current_page(0)
        self.widgets.tbtn_fullsize_preview.set_active(False)
        self._show_iconview_tab(False)

    # edit a program or make a new one
    def on_btn_edit_clicked(self, widget, data=None):
        self.widgets.ntb_button.set_current_page(_BB_EDIT)
        self.widgets.ntb_preview.hide()
        self.widgets.hbox_dro.hide()
        width = self.widgets.window1.allocation.width
        width -= self.widgets.vbtb_main.allocation.width
        width -= self.widgets.box_right.allocation.width
        width -= self.widgets.box_left.allocation.width
        self.widgets.vbx_jog.set_size_request(width, -1)
        if not self.widgets.vbx_jog.get_visible():
            self.widgets.vbx_jog.set_visible(True)
        self.widgets.gcode_view.set_sensitive(True)
        self.widgets.gcode_view.grab_focus()
        if self.widgets.chk_use_kb_on_edit.get_active():
            self.widgets.ntb_info.set_current_page(1)
            self.widgets.box_info.set_size_request(-1, 250)
        else:
            self.widgets.ntb_info.hide()
            self.widgets.box_info.set_size_request(-1, 50)
        self.widgets.tbl_search.show()
        self.gcodeerror = ""

    # Search and replace handling in edit mode
    # undo changes while in edit mode
    def on_btn_undo_clicked(self, widget, data=None):
        self.widgets.gcode_view.undo()

    # search backward while in edit mode
    def on_btn_search_back_clicked(self, widget, data=None):
        self.widgets.gcode_view.text_search(direction=False,
                                            mixed_case=self.widgets.chk_ignore_case.get_active(),
                                            text=self.widgets.search_entry.get_text())

    # search forward while in edit mode
    def on_btn_search_forward_clicked(self, widget, data=None):
        self.widgets.gcode_view.text_search(direction=True,
                                            mixed_case=self.widgets.chk_ignore_case.get_active(),
                                            text=self.widgets.search_entry.get_text())

    # replace text in edit mode
    def on_btn_replace_clicked(self, widget, data=None):
        self.widgets.gcode_view.replace_text_search(direction=True,
                                                    mixed_case=self.widgets.chk_ignore_case.get_active(),
                                                    text=self.widgets.search_entry.get_text(),
                                                    re_text=self.widgets.replace_entry.get_text(),
                                                    replace_all=self.widgets.chk_replace_all.get_active())

    # redo changes while in edit mode
    def on_btn_redo_clicked(self, widget, data=None):
        self.widgets.gcode_view.redo()

    # if we leave the edit mode, we will have to show all widgets again
    def on_ntb_button_switch_page(self, *args):
        if self.widgets.ntb_preview.get_current_page() == 0:  # preview tab is active,
            # check if offset tab is visible, if so we have to hide it
            page = self.widgets.ntb_preview.get_nth_page(1)
            if page.get_visible():
                self._show_offset_tab(False)
        elif self.widgets.ntb_preview.get_current_page() == 1:
            self._show_offset_tab(False)
        elif self.widgets.ntb_preview.get_current_page() == 2:
            self._show_tooledit_tab(False)
        elif self.widgets.ntb_preview.get_current_page() == 3:
            self._show_iconview_tab(False)

        if self.widgets.tbtn_fullsize_preview.get_active():
            self.widgets.tbtn_fullsize_preview.set_active(False)
        if self.widgets.ntb_button.get_current_page() == _BB_EDIT or self.widgets.ntb_preview.get_current_page() == _BB_HOME:
            self.widgets.ntb_preview.show()
            self.widgets.hbox_dro.show()
            self.widgets.vbx_jog.set_size_request(360, -1)
            self.widgets.gcode_view.set_sensitive(0)
            self.widgets.btn_save.set_sensitive(True)
            self.widgets.hal_action_reload.emit("activate")
            self.widgets.ntb_info.set_current_page(0)
            self.widgets.ntb_info.show()
            self.widgets.box_info.set_size_request(-1, 200)
            self.widgets.tbl_search.hide()

    # make a new file
    def on_btn_new_clicked(self, widget, data=None):
        tempfilename = os.path.join(_TEMPDIR, "temp.ngc")
        content = self.get_ini_info.get_RS274_start_code()
        if content == None:
            content = " "
        content += "\n\n\n\nM2"
        gcodefile = open(tempfilename, "w")
        gcodefile.write(content)
        gcodefile.close()
        if self.widgets.lbl_program.get_label() == tempfilename:
            self.widgets.hal_action_reload.emit("activate")
        else:
            self.widgets.hal_action_open.load_file(tempfilename)
            # self.command.program_open(tempfilename)
        self.widgets.gcode_view.grab_focus()
        self.widgets.btn_save.set_sensitive(False)

    def on_tbtn_optional_blocks_toggled(self, widget, data=None):
        opt_blocks = widget.get_active()
        self.command.set_block_delete(opt_blocks)
        self.prefs.putpref("blockdel", opt_blocks)
        self.widgets.hal_action_reload.emit("activate")

    def on_tbtn_optional_stops_toggled(self, widget, data=None):
        opt_stops = widget.get_active()
        self.command.set_optional_stop(opt_stops)
        self.prefs.putpref("opstop", opt_stops)

    # this can not be done with the status widget,
    # because it will not emit a RESUME signal
    def on_tbtn_pause_toggled(self, widget, data=None):
        widgetlist = ["rbt_forward", "rbt_reverse", "rbt_stop"]
        self._sensitize_widgets(widgetlist, widget.get_active())

    def on_btn_stop_clicked(self, widget, data=None):
        self.command.abort()
        self.start_line = 0
        self.widgets.gcode_view.set_line_number(0)
        self.widgets.tbtn_pause.set_active(False)

    def on_btn_run_clicked(self, widget, data=None):
        self.command.auto(linuxcnc.AUTO_RUN, self.start_line)

    def on_btn_from_line_clicked(self, widget, data=None):
        self.dialogs.restart_dialog(self)

    def on_change_sound(self, widget, sound=None):
        file = widget.get_filename()
        if file:
            if widget == self.widgets.audio_error_chooser:
                self.error_sound = file
                self.prefs.putpref("audio_error", file)
            else:
                self.alert_sound = file
                self.prefs.putpref("audio_alert", file)

    def on_tbtn_switch_mode_toggled(self, widget, data=None):
        if widget.get_active():
            self.widgets.tbtn_switch_mode.set_label(_(" Joint\nmode"))
            # Mode 1 = joint ; Mode 2 = MDI ; Mode 3 = teleop
            # so in mode 1 we have to show Joints and in Modes 2 and 3 axis values
            self.set_motion_mode(0)
        else:
            self.widgets.tbtn_switch_mode.set_label(_("World\nmode"))
            self.set_motion_mode(1)

# =========================================================
# Hal Pin Handling Start

    def _on_counts_changed(self, pin, widget):
        if not self.initialized:
            return
        difference = 0
        counts = pin.get()
        if self.halcomp["feed.feed-override.count-enable"]:
            if widget == "spc_feed":
                difference = (counts - self.fo_counts) * self.scale_feed_override
                self.fo_counts = counts
                self._check_counts(counts)
        if self.halcomp["rapid.rapid-override.count-enable"]:
            if widget == "spc_rapid":
                difference = (counts - self.ro_counts) * self.scale_rapid_override
                self.ro_counts = counts
                self._check_counts(counts)
        if self.halcomp["spindle.spindle-override.count-enable"]:
            if widget == "spc_spindle":
                difference = (counts - self.so_counts) * self.scale_spindle_override
                self.so_counts = counts
                self._check_counts(counts)
        if self.halcomp["jog.jog-velocity.count-enable"]:
            if widget == "spc_jog_vel":
                difference = (counts - self.jv_counts) * self.scale_jog_vel
                if self.widgets.tbtn_turtle_jog.get_active():
                    difference = difference / self.turtle_jog_factor
                self.jv_counts = counts
                self._check_counts(counts)
        if not self.halcomp["feed.feed-override.count-enable"] \
                            and not self.halcomp["spindle.spindle-override.count-enable"] \
                            and not self.halcomp["jog.jog-velocity.count-enable"] \
                            and not self.halcomp["rapid.rapid-override.count-enable"]:
            self._check_counts(counts)

        val = self.widgets[widget].get_value() + difference
        if val < 0:
            val = 0
        if difference != 0:
            self.widgets[widget].set_value(val)

    def _check_counts(self, counts):
        # as we do not know how the user did connect the jog wheels, we have to check all
        # possibilities. Does he use only one jog wheel and a selection switch or do he use
        # a mpg for each slider or one for speeds and one for override, or ??
        if self.halcomp["feed.feed-override.counts"] == self.halcomp["spindle.spindle-override.counts"]:
            if self.halcomp["feed.feed-override.count-enable"] and self.halcomp["spindle.spindle-override.count-enable"]:
                return
            self.fo_counts = self.so_counts = counts
        if self.halcomp["feed.feed-override.counts"] == self.halcomp["jog.jog-velocity.counts"]:
            if self.halcomp["feed.feed-override.count-enable"] and self.halcomp["jog.jog-velocity.count-enable"]:
                return
            self.fo_counts = self.jv_counts = counts
        if self.halcomp["feed.feed-override.counts"] == self.halcomp["rapid.rapid-override.counts"]:
            if self.halcomp["feed.feed-override.count-enable"] and self.halcomp["rapid.rapid-override.count-enable"]:
                return
            self.fo_counts = self.ro_counts = counts
        if self.halcomp["spindle.spindle-override.counts"] == self.halcomp["jog.jog-velocity.counts"]:
            if self.halcomp["spindle.spindle-override.count-enable"] and self.halcomp["jog.jog-velocity.count-enable"]:
                return
            self.so_counts = self.jv_counts = counts
        if self.halcomp["spindle.spindle-override.counts"] == self.halcomp["rapid.rapid-override.counts"]:
            if self.halcomp["spindle.spindle-override.count-enable"] and self.halcomp["rapid.rapid-override.count-enable"]:
                return
            self.so_counts = self.ro_counts = counts
        if self.halcomp["jog.jog-velocity.counts"] == self.halcomp["rapid.rapid-override.counts"]:
            if self.halcomp["jog.jog-velocity.count-enable"] and self.halcomp["rapid.rapid-override.count-enable"]:
                return
            self.jv_counts = self.ro_counts = counts

    def _on_analog_enable_changed(self, pin, widget):
        if not self.initialized:
            return
        if widget == "spc_spindle":
            if pin.get():
                self.widgets.btn_spindle_100.hide()
            else:
                self.widgets.btn_spindle_100.show()
        if widget == "spc_feed":
            if pin.get():
                self.widgets.btn_feed_100.hide()
            else:
                self.widgets.btn_feed_100.show()
        # widget can also be spc_jog_vel and spc_rapid
        self.widgets[widget].hide_button(pin.get())
        
        if pin.get():
            # special case of jog_vel, as we have to take care of both modes,
            # more details see _on_analog_value_changed
            if self.widgets.tbtn_turtle_jog.get_active():
                value = self.rabbit_jog = self.jog_rate_max * self.halcomp["jog.jog-velocity.direct-value"]
            elif not self.widgets.tbtn_turtle_jog.get_active():
                value = self.turtle_jog = self.jog_rate_max / self.turtle_jog_factor * self.halcomp["jog.jog-velocity.direct-value"]
            self.widgets.spc_jog_vel.set_value(value)

    def _on_analog_value_changed(self, pin, widget):
        if not self.initialized:
            return
        if widget == "spc_jog_vel" and not self.halcomp["jog.jog-velocity.analog-enable"]:
            return
        if widget == "spc_feed" and not self.halcomp["feed.feed-override.analog-enable"]:
            return
        if widget == "spc_spindle" and not self.halcomp["spindle.spindle-override.analog-enable"]:
            return
        if widget == "spc_rapid" and not self.halcomp["rapid.rapid-override.analog-enable"]:
            return
        percentage = pin.get()
        if percentage > 1.0:
            percentage = 1.0
        range = self.widgets[widget].get_property("max") - self.widgets[widget].get_property("min")
        try:  # otherwise a value of 0.0 would give an error
            value = self.widgets[widget].get_property("min") + (range * percentage)
        except:
            value = 0
        self.widgets[widget].set_value(value)

        # special case of jog_vel, as we have to take care of both modes,
        # meaning the analog value must be applied to both! If we do not do this,
        # it might be that a user has analog in signal set to 0.5 and switch the mode
        # but in the other mode he had only 0.3 from its value, so a small change of the 
        # analog in to 0.51 would result in a jump of 20 %! 
        if self.widgets.tbtn_turtle_jog.get_active():
            self.rabbit_jog = self.jog_rate_max * pin.get()
        elif not self.widgets.tbtn_turtle_jog.get_active():
            self.turtle_jog = self.jog_rate_max / self.turtle_jog_factor * pin.get()

    def _on_unlock_settings_changed(self, pin):
        if not self.initialized:
            return
        if not self.widgets.rbt_hal_unlock.get_active() and not self.user_mode:
            return
        self.widgets.tbtn_setup.set_sensitive(pin.get())

    def _on_play_sound(self, widget, sound = None):
        print(self,widget,sound)
        if _AUDIO_AVAILABLE and sound:
            if sound == "error":
                self.audio.set_sound(self.error_sound)
            elif sound == "alert":
                self.audio.set_sound(self.alert_sound)
            else:
                print("got unknown sound to play")
                return
            self.audio.run()

    def _on_message_deleted(self, widget, messages):
        number = []
        for message in messages:
            if message[2] == ALERT_ICON:
                number.append(message[0])
        if len(number) == 0:
            self.halcomp["error"] = False

    def _del_message_changed(self, pin):
        if pin.get():
            if self.halcomp["error"] == True:
                number = []
                messages = self.notification.messages
                for message in messages:
                    if message[2] == ALERT_ICON:
                        number.append(message[0])
                self.notification.del_message(number[0])
                if len(number) == 1:
                    self.halcomp["error"] = False
            else:
                self.notification.del_last()

    def _on_pin_incr_changed(self, pin, buttonnumber):
        if self.stat.state != 1:
            self.command.abort()
            self.command.wait_complete()
        if not pin.get():
            return
        data = self.jog_increments[int(buttonnumber)]
        self.on_increment_changed(self.incr_rbt_list[int(buttonnumber)], data)
        self.incr_rbt_list[int(buttonnumber)].set_active(True)

    def _on_pin_jog_axis_changed(self, pin, axis, direction):
        if self.stat.kinematics_type != linuxcnc.KINEMATICS_IDENTITY:
            if self.stat.motion_mode == 1 and pin.get():
                message = _("Axis jogging is only allowed in world mode, but you are in joint mode!")
                print(message)
                self._show_error((13, message))
                return

        if axis not in "xyz":
            if axis == self.axisletter_four:
                axis = 4
            if axis == self.axisletter_five:
                axis = 5
        if direction == 1:
            widget = self.widgets["btn_{0}_plus".format(axis)]
        else:
            widget = self.widgets["btn_{0}_minus".format(axis)]
        if pin.get():
            self.on_btn_jog_pressed(widget)
        else:
            self.on_btn_jog_released(widget)

    def _on_pin_jog_joint_changed(self, pin, joint, direction):
        if self.stat.motion_mode != 1 and pin.get():
            message = _("Joint jogging is only allowed in joint mode, but you are in world mode!")
            print(message)
            self._show_error((13, message))
            return

        if direction == 1:
            widget = self.widgets["btn_j{0}_plus".format(str(joint))]
        else:
            widget = self.widgets["btn_j{0}_minus".format(str(joint))]
        if pin.get():
            self.on_btn_jog_pressed(widget)
        else:
            self.on_btn_jog_released(widget)

    def _reset_overide(self, pin, type):
        if pin.get():
            if type == "rapid":
                self.command.rapidrate(1.0)
                return
            self.widgets["btn_{0}_100".format(type)].emit("clicked")
            
    def _on_blockheight_value_changed(self, pin):
        self.widgets.lbl_blockheight.set_text("blockheight = {0:.3f}".format(pin.get()))

# =========================================================
# The actions of the buttons
    def _on_h_button_changed(self, pin):
        # we check if the button is pressed ore release,
        # otherwise a signal will be emitted, if the button is released and
        # the signal drob down to zero
        if not pin.get():
            return
        # lets see on witch button_box we are
        page = self.widgets.ntb_button.get_current_page()
        # witch button has been pressed
        btn = str(pin.name)
        # from the list we declared under __init__ we get the button number
        nr = int(btn[-1])
        tab = self.h_tabs[page]  # see in the __init__ section for the declaration of self.tabs
        button = None
        # we check if there is a button or the user pressed a hardware button under
        # a non existing software button
        for index in tab:
            if int(index[0]) == nr:
                # this is the name of the button
                button = index[1]
        if button:
            # only emit a signal if the button is sensitive, otherwise
            # running actions may be interrupted
            if not self.widgets[button].get_sensitive():
                print("{0} not_sensitive".format(button))
                return
            self.widgets[button].emit("clicked")
            print("Button {0} has been clicked".format(button))
        else:
            # as we are generating the macro buttons dynamically, we can"t use the same
            # method as above, here is how we do it
            if page == 1:  # macro page
                # does the user press a valid hardware button?
                if nr < len(self.macrobuttons):
                    button = self.macrobuttons[nr]  # This list is generated in add_macros_buttons(self)
                    # is the button sensitive?
                    if not button.get_sensitive():
                        print("{0} not_sensitive".format(button))
                        return
                    button.emit("pressed")
                else:
                    print("No function on this button")
            else:
                print("No function on this button")

    def _on_v_button_changed(self, pin):
        if not pin.get():
            return
        btn = str(pin.name)
        nr = int(btn[-1])
        tab = self.v_tabs  # see in the __init__ section for the declaration of self.tabs
        button = None
        for index in tab:
            if int(index[0]) == nr:
                # this is the name of the button
                button = index[1]
        if button:
            # only emit a signal if the button is sensitive, otherwise
            # running actions may be interrupted
            if self.widgets[button].get_sensitive() == False:
                print("{0} not_sensitive".format(button))
                return
            button_pressed_list = ("rbt_manual", "rbt_mdi", "rbt_auto")
            button_toggled_list = ("tbtn_setup")
            if button in button_pressed_list:
                self.widgets[button].set_active(True)
                self.widgets[button].emit("pressed")
            elif button in button_toggled_list:
                self.widgets[button].set_active(not self.widgets[button].get_active())
            else:
                self.widgets[button].emit("clicked")
        else:
            print("No button found in v_tabs from {0}".format(pin.name))

# We need extra HAL pins here is where we do it.
# we make pins for the hardware buttons witch can be placed around the
# screen to activate the corresponding buttons on the GUI
    def _init_hal_pins(self):
        # generate the horizontal button pins
        for h_button in range(0, 10):
            pin = self.halcomp.newpin("h-button.button-{0}".format(h_button), hal.HAL_BIT, hal.HAL_IN)
            hal_glib.GPin(pin).connect("value_changed", self._on_h_button_changed)

        # generate the vertical button pins
        for v_button in range(0, 7):
            pin = self.halcomp.newpin("v-button.button-{0}".format(v_button), hal.HAL_BIT, hal.HAL_IN)
            hal_glib.GPin(pin).connect("value_changed", self._on_v_button_changed)

        # buttons for jogging the axis
        for jog_button in self.axis_list:
            pin = self.halcomp.newpin("jog.axis.jog-{0}-plus".format(jog_button), hal.HAL_BIT, hal.HAL_IN)
            hal_glib.GPin(pin).connect("value_changed", self._on_pin_jog_axis_changed, jog_button, 1)
            pin = self.halcomp.newpin("jog.axis.jog-{0}-minus".format(jog_button), hal.HAL_BIT, hal.HAL_IN)
            hal_glib.GPin(pin).connect("value_changed", self._on_pin_jog_axis_changed, jog_button, -1)

        if self.stat.kinematics_type != linuxcnc.KINEMATICS_IDENTITY:
            for joint_button in range(0, self.stat.joints):
                pin = self.halcomp.newpin("jog.joint.jog-{0}-plus".format(joint_button), hal.HAL_BIT, hal.HAL_IN)
                hal_glib.GPin(pin).connect("value_changed", self._on_pin_jog_joint_changed, joint_button, 1)
                pin = self.halcomp.newpin("jog.joint.jog-{0}-minus".format(joint_button), hal.HAL_BIT, hal.HAL_IN)
                hal_glib.GPin(pin).connect("value_changed", self._on_pin_jog_joint_changed, joint_button, -1)

        # jog_increment out pin
        self.halcomp.newpin("jog.jog-increment", hal.HAL_FLOAT, hal.HAL_OUT)

        # generate the pins to set the increments
        for buttonnumber in range(0, len(self.jog_increments)):
            pin = self.halcomp.newpin("jog.jog-inc-{0}".format(buttonnumber), hal.HAL_BIT, hal.HAL_IN)
            hal_glib.GPin(pin).connect("value_changed", self._on_pin_incr_changed, buttonnumber)

        # make the pin for unlocking settings page
        pin = self.halcomp.newpin("unlock-settings", hal.HAL_BIT, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self._on_unlock_settings_changed)

        # generate the pins to connect encoders to the sliders
        pin = self.halcomp.newpin("feed.feed-override.counts", hal.HAL_S32, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self._on_counts_changed, "spc_feed")
        pin = self.halcomp.newpin("spindle.spindle-override.counts", hal.HAL_S32, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self._on_counts_changed, "spc_spindle")
        pin = self.halcomp.newpin("jog.jog-velocity.counts", hal.HAL_S32, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self._on_counts_changed, "spc_jog_vel")
        pin = self.halcomp.newpin("rapid.rapid-override.counts", hal.HAL_S32, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self._on_counts_changed, "spc_rapid")
        self.halcomp.newpin("feed.feed-override.count-enable", hal.HAL_BIT, hal.HAL_IN)
        self.halcomp.newpin("spindle.spindle-override.count-enable", hal.HAL_BIT, hal.HAL_IN)
        self.halcomp.newpin("jog.jog-velocity.count-enable", hal.HAL_BIT, hal.HAL_IN)
        self.halcomp.newpin("rapid.rapid-override.count-enable", hal.HAL_BIT, hal.HAL_IN)

        # generate the pins to connect analog inputs for sliders
        pin = self.halcomp.newpin("feed.feed-override.analog-enable", hal.HAL_BIT, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self._on_analog_enable_changed, "spc_feed")
        pin = self.halcomp.newpin("spindle.spindle-override.analog-enable", hal.HAL_BIT, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self._on_analog_enable_changed, "spc_spindle")
        pin = self.halcomp.newpin("jog.jog-velocity.analog-enable", hal.HAL_BIT, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self._on_analog_enable_changed, "spc_jog_vel")
        pin = self.halcomp.newpin("rapid.rapid-override.analog-enable", hal.HAL_BIT, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self._on_analog_enable_changed, "spc_rapid")
        pin = self.halcomp.newpin("feed.feed-override.direct-value", hal.HAL_FLOAT, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self._on_analog_value_changed, "spc_feed")
        pin = self.halcomp.newpin("spindle.spindle-override.direct-value", hal.HAL_FLOAT, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self._on_analog_value_changed, "spc_spindle")
        pin = self.halcomp.newpin("jog.jog-velocity.direct-value", hal.HAL_FLOAT, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self._on_analog_value_changed, "spc_jog_vel")
        pin = self.halcomp.newpin("rapid.rapid-override.direct-value", hal.HAL_FLOAT, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self._on_analog_value_changed, "spc_rapid")

        # make a pin to set turtle jog vel
        pin = self.halcomp.newpin("jog.turtle-jog", hal.HAL_BIT, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self._on_turtle_jog_enable)

        # make the pins for tool measurement
        self.halcomp.newpin("probeheight", hal.HAL_FLOAT, hal.HAL_OUT)
        pin = self.halcomp.newpin("blockheight", hal.HAL_FLOAT, hal.HAL_OUT)
        hal_glib.GPin(pin).connect("value_changed", self._on_blockheight_value_changed)
        preset = self.prefs.getpref("blockheight", 0.0, float)
        self.halcomp["blockheight"] = preset
        self.halcomp.newpin("toolmeasurement", hal.HAL_BIT, hal.HAL_OUT)
        self.halcomp.newpin("searchvel", hal.HAL_FLOAT, hal.HAL_OUT)
        self.halcomp.newpin("probevel", hal.HAL_FLOAT, hal.HAL_OUT)

        # make pins to react to tool_offset changes
        pin = self.halcomp.newpin("tooloffset-x", hal.HAL_FLOAT, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self._offset_changed, "tooloffset-x")
        pin = self.halcomp.newpin("tooloffset-z", hal.HAL_FLOAT, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self._offset_changed, "tooloffset-z")
        self.halcomp.newpin("tool-diameter", hal.HAL_FLOAT, hal.HAL_OUT)

        # make a pin to delete a notification message
        pin = self.halcomp.newpin("delete-message", hal.HAL_BIT, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self._del_message_changed)

        # for manual tool change dialog
        self.halcomp.newpin("toolchange-number", hal.HAL_S32, hal.HAL_IN)
        self.halcomp.newpin("toolchange-changed", hal.HAL_BIT, hal.HAL_OUT)
        pin = self.halcomp.newpin('toolchange-change', hal.HAL_BIT, hal.HAL_IN)
        hal_glib.GPin(pin).connect('value_changed', self.on_tool_change)

        # make a pin to reset feed override to 100 %
        pin = self.halcomp.newpin("feed.reset-feed-override", hal.HAL_BIT, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self._reset_overide, "feed")

        # make a pin to reset rapid override to 100 %
        pin = self.halcomp.newpin("rapid.reset-rapid-override", hal.HAL_BIT, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self._reset_overide, "rapid")

        # make a pin to reset spindle override to 100 %
        pin = self.halcomp.newpin("spindle.reset-spindle-override", hal.HAL_BIT, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self._reset_overide, "spindle")

        # make an error pin to indicate a error to hardware
        self.halcomp.newpin("error", hal.HAL_BIT, hal.HAL_OUT)

        # make pins to indicate program progress information
        self.halcomp.newpin("program.length", hal.HAL_S32, hal.HAL_OUT)
        self.halcomp.newpin("program.current-line", hal.HAL_S32, hal.HAL_OUT)
        self.halcomp.newpin("program.progress", hal.HAL_FLOAT, hal.HAL_OUT)

        # make a pin to set ignore limits
        pin = self.halcomp.newpin("ignore-limits", hal.HAL_BIT, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self._ignore_limits)

# Hal Pin Handling End
# =========================================================

if __name__ == "__main__":
    app = gmoccapy(sys.argv)

    inifile = sys.argv[2]
    print ("**** GMOCCAPY INFO : inifile = {0} ****:".format(sys.argv[2]))
    postgui_halfile = app.get_ini_info.get_postgui_halfile()
    print ("**** GMOCCAPY INFO : postgui halfile = {0} ****:".format(postgui_halfile))

    if postgui_halfile:
        if postgui_halfile.lower().endswith('.tcl'):
            res = os.spawnvp(os.P_WAIT, "haltcl", ["haltcl", "-i", inifile, postgui_halfile])
        else:
            res = os.spawnvp(os.P_WAIT, "halcmd", ["halcmd", "-i", inifile, "-f", postgui_halfile])
        if res:
            raise SystemExit, res

    gtk.main()

