#!/usr/bin/python
# -*- coding:UTF-8 -*-
"""
    luminos (Linux User Machine Interface NOrbert Schechner)
    is a GUI for LinuxCNC based on gmoccapy, codded with gladevcp and Python

    Copyright 2014 Norbert Schechner
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

"""
import traceback            # needed to launch traceback errors
import hal                  # base hal class to react to hal signals
import hal_glib             # needed to make our own hal pins
import gtk                  # base for pygtk widgets and constants
import sys                  # handle system calls
import os                   # needed to get the paths and directorys
import pango                # needed for font settings and changing
import gladevcp.makepins    # needed for the dialog"s calulator widget
import atexit               # needed to register childs to be closed on closing the GUI
import subprocess           # to launch embeddee tabs and other proceses like onboard keyboard
import tempfile             # needed only if the user click new in edit mode to open a new empty file
import linuxcnc             # to get our own error sytsem
import gobject              # needed to add the timer for periodic
import locale               # for setting the language of the GUI
import gettext              # to extract the strings to be translated

from gladevcp.gladebuilder import GladeBuilder

from time import strftime           # needed to add a time stamp with alarm entrys
from time import localtime          # needed to add a time stamp with alarm entrys

# Throws up a dialog with debug info when an error is encountered
def excepthook(exc_type, exc_obj, exc_tb):
    try:
        w = app.widgets.window1
    except KeyboardInterrupt:
        sys.exit(0)
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

debug = True

if debug:
    pydevdir = '/home/emcmesa/Aptana_Studio_3/plugins/org.python.pydev_2.7.0.2013032300/pysrc'

    if os.path.isdir(pydevdir): # and  'emctask' in sys.builtin_module_names:
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
_RELEASE = "0.0.1"
_INCH = 0                           # imperial units are active
_MM = 1                             # metric units are active
# _MANUAL = 1                         # Check for the mode Manual
# _AUTO = 2                           # Check for the mode Auto
# _MDI = 3                            # Check for the mode MDI
_TEMPDIR = tempfile.gettempdir()    # Now we know where the tempdir is, usualy /tmp

# set up paths to files
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
LIBDIR = os.path.join(BASE, "lib", "python")
sys.path.insert(0, LIBDIR)

# as now we know the libdir path we can import our own modules
from gmoccapy import widgets        # a class to handle the widgets
from gmoccapy import player         # a class to handle sounds
from gmoccapy import notification   # this is the module we use for our error handling
from gmoccapy import preferences    # this handles the preferences
from gmoccapy import getiniinfo     # this handles the INI File reading so checking is done in that module
from gmoccapy import dialogs        # this takes the code of all our dialogs

# set up paths to files, part two
CONFIGPATH = os.environ['CONFIG_DIR']
DATADIR = os.path.join(BASE, "share", "gmoccapy")
IMAGEDIR = os.path.join(DATADIR, "images")
XMLNAME = os.path.join(BASE, "share", "luminos", "luminos.glade")
THEMEDIR = "/usr/share/themes"
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

class luminos(object):

    def __init__(self):

        # prepare for translation / internationalisation
        locale.setlocale(locale.LC_ALL, '')
        locale.bindtextdomain("luminos", LOCALEDIR)
        gettext.install("luminos", localedir = LOCALEDIR, unicode = True)
        gettext.bindtextdomain("luminos", LOCALEDIR)

        # needed components to comunicate with hal and linuxcnc
        self.halcomp = hal.component("luminos")
        self.command = linuxcnc.command()
        self.stat = linuxcnc.stat()
        self.error_channel = linuxcnc.error_channel()
        # initial poll, so all is up to date
        self.stat.poll()
        self.error_channel.poll()

        self.builder = gtk.Builder()
        # translation of the glade file will be done with
        self.builder.set_translation_domain("luminos")
        self.builder.add_from_file(XMLNAME)
        self.builder.connect_signals(self)

        self.widgets = widgets.Widgets(self.builder)

        # Our own class to get information from ini the file we use this way, to be sure
        # to get a valid result, as the checks are done in that module
        self.get_ini_info = getiniinfo.GetIniInfo()

        # and this one to read and write the preferences
        self.prefs = preferences.preferences(self.get_ini_info.get_preference_file_path())

        self.start_line = 0       # needed for start from line
        self.stepping = False     # used to sensitize widgets when using step by step

        self.active_gcodes = []   # this are the formated G code values
        self.active_mcodes = []   # this are the formated M code values
        self.gcodes = []          # this are the unformated G code values to check if an update is requiered
        self.mcodes = []          # this are the unformated M code values to check if an update is requiered

        self.wait_mode = None           # wait for being ok to declared mode
        self.tool_change = False        # this is needed to get back to manual mode after a tool change
        self.macrobuttons = []          # The list of all macrios defined in the INI file
        self.log = False                # decide if the actions should be loged
        self.faktor = 1.0               # faktor to calculate difference from machine to display units
        self.fo_counts = 0              # need to calculate diference in counts to change the feed override slider
        self.so_counts = 0              # need to calculate diference in counts to change the spindle override slider
        self.jv_counts = 0              # need to calculate diference in counts to change the jog_vel slider
        self.mv_counts = 0              # need to calculate diference in counts to change the max_speed slider
        self.jog_increments = []        # This holds the increment values
        self.unlock = False             # this value will be set using the hal pin unlock settings
                                        # needed to display the labels
        self.system_list = ("0", "G54", "G55", "G56", "G57", "G58", "G59", "G59.1", "G59.2", "G59.3")
        self.axisnumber_four = ""       # we use this to get the number of the 4-th axis
        self.axisletter_four = None     # we use this to get the letter of the 4-th axis
        self.axisnumber_five = ""       # we use this to get the number of the 5-th axis
        self.axisletter_five = None     # we use this to get the letter of the 5-th axis
        self.notification = notification.Notification() # Our own message system
        self.notification.connect("message_deleted", self._on_message_deleted)
        self.all_homed = False          # will hold True if all axis are homed
        self.no_force_homing = False    # should not be used on industrial machines, but may be helpfull for debugging
        self.faktor = 1.0               # needed to calculate velocitys

        self.xpos = 40                  # The X Position of the main Window
        self.ypos = 30                  # The Y Position of the main Window
        self.width = 979                # The width of the main Window
        self.height = 750               # The heigh of the main Window

        self.gcodeerror = ""            # we need this to avoid multile messages of the same error

        # the default theme = System Theme we store here to be able to go back to that one later
        self.default_theme = gtk.settings_get_default().get_property("gtk-theme-name")

        # the sounds to play if an error or message rises
        self.alert_sound = "/usr/share/sounds/ubuntu/stereo/bell.ogg"
        self.error_sound = "/usr/share/sounds/ubuntu/stereo/dialog-question.ogg"

        self._get_axis_list()
        self._init_extra_axes()
        self._init_jog_increments()
        self._init_macro_button()

        self._init_hal_pins()

        # this are settings to be done before window show
        self._init_preferences()
        self._init_audio()
        self._init_themes()
        self._init_luminos_button()

        # now we initialize the file to load widget
        self._init_file_to_load()

        # set the title of the window, to show the release
        self.widgets.window1.set_title("luminos for linuxcnc %s" % _RELEASE)

        # This is needed to make the glade panel pins availible
        panel = gladevcp.makepins.GladePanel(self.halcomp, XMLNAME , self.builder, None)

        self.halcomp.ready()

        # finaly show the window
        self.widgets.window1.show()
        self._init_IconFileSelection()
        self._init_offsetpage()
        self._init_gremlin()

        # and the embedded tabs stuff
        self._init_dynamic_tabs()

        # and the tooleditor
        self._init_tooleditor()

        # the velocity settings
        self.min_spindle_rev = self.prefs.getpref("spindle_bar_min", 0.0, float)
        self.max_spindle_rev = self.prefs.getpref("spindle_bar_max", 6000.0, float)
        self.widgets.adj_spindle_bar_min.set_value(self.min_spindle_rev)
        self.widgets.adj_spindle_bar_max.set_value(self.max_spindle_rev)
        self.widgets.spindle_feedback_bar.set_property("min", float(self.min_spindle_rev))
        self.widgets.spindle_feedback_bar.set_property("max", float(self.max_spindle_rev))

        # Window position and size
        self.widgets.adj_x_pos.set_value(self.prefs.getpref("x_pos", 10, float))
        self.widgets.adj_y_pos.set_value(self.prefs.getpref("y_pos", 25, float))
        self.widgets.adj_width.set_value(self.prefs.getpref("width", 979, float))
        self.widgets.adj_height.set_value(self.prefs.getpref("height", 750, float))

        # Popup Messages position and size
        self.widgets.adj_x_pos_popup.set_value(self.prefs.getpref("x_pos_popup", 15, float))
        self.widgets.adj_y_pos_popup.set_value(self.prefs.getpref("y_pos_popup", 55, float))
        self.widgets.adj_width_popup.set_value(self.prefs.getpref("width_popup", 250, float))
        self.widgets.adj_max_messages.set_value(self.prefs.getpref("max_messages", 10, float))
        self.widgets.fontbutton_popup.set_font_name(self.prefs.getpref("message_font", "sans 10", str))
        self.widgets.chk_use_frames.set_active(self.prefs.getpref("use_frames", True, bool))

        # and the rest of the widgets
        self.widgets.ntb_jog.set_current_page(0)
        self.widgets.tbtn_optional_blocks.set_active(self.prefs.getpref("blockdel", False))
        self.command.set_block_delete(self.widgets.tbtn_optional_blocks.get_active())
        self.widgets.tbtn_optional_stops.set_active(not self.prefs.getpref("opstop", False))
        self.command.set_optional_stop(self.widgets.tbtn_optional_stops.get_active())
        self.log = self.prefs.getpref("log_actions", False, bool)
        self.widgets.tbtn_log_actions.set_active(self.log)
        self.widgets.chk_show_dro.set_active(self.prefs.getpref("enable_dro", False))
        self.widgets.chk_show_offsets.set_active(self.prefs.getpref("show_offsets", False))
        self.widgets.chk_show_dtg.set_active(self.prefs.getpref("show_dtg", False))
        self.widgets.chk_show_offsets.set_sensitive(self.widgets.chk_show_dro.get_active())
        self.widgets.chk_show_dtg.set_sensitive(self.widgets.chk_show_dro.get_active())

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


        # get if run from line should be used
        rfl = self.prefs.getpref("run_from_line", "no_run", str)
        # and set the corresponding button active
        self.widgets["rbtn_%s_from_line" % rfl].set_active(True)
        if rfl == "no_run":
            self.widgets.btn_from_line.set_sensitive(False)
        else:
            self.widgets.btn_from_line.set_sensitive(True)

        # get the way to unlock the settings
        unlock = self.prefs.getpref("unlock_way", "use", str)
        # and set the corresponding button active
        self.widgets["rbt_%s_unlock" % unlock].set_active(True)
        # if Hal pin should be used, only set the button active, if the pin is high
        if unlock == "hal" and not self.halcomp["unlock-settings"]:
            self.widgets.tbtn_setup.set_sensitive(False)
        self.unlock_code = self.prefs.getpref("unlock_code", "123", str) # get unlock code

        # check if the user want to display preview window insteadt of offsetpage widget
        state = self.prefs.getpref("show_preview_on_offset", False, bool)
        if state:
            self.widgets.rbtn_show_preview.set_active(True)
        else:
            self.widgets.rbtn_show_offsets.set_active(True)

        # check the highlighting type
        # the following would load the python language
        # self.widgets.gcode_view.set_language("python")
        LANGDIR = os.path.join(BASE, "share", "gtksourceview-2.0", "language-specs")
        self.widgets.gcode_view.set_language("gcode", LANGDIR)

        # set the user colors of the DRO
        self.abs_color = self.prefs.getpref("abs_color", "blue", str)
        self.rel_color = self.prefs.getpref("rel_color", "black", str)
        self.dtg_color = self.prefs.getpref("dtg_color", "yellow", str)
        self.homed_color = self.prefs.getpref("homed_color", "green", str)
        self.unhomed_color = self.prefs.getpref("unhomed_color", "red", str)
        self.widgets.abs_colorbutton.set_color(gtk.gdk.color_parse(self.abs_color))
        self.widgets.rel_colorbutton.set_color(gtk.gdk.color_parse(self.rel_color))
        self.widgets.dtg_colorbutton.set_color(gtk.gdk.color_parse(self.dtg_color))
        self.widgets.homed_colorbtn.set_color(gtk.gdk.color_parse(self.homed_color))
        self.widgets.unhomed_colorbtn.set_color(gtk.gdk.color_parse(self.unhomed_color))

        for axis in self.axis_list:
            if axis == self.axisletter_four:
                axis = 4
            if axis == self.axisletter_five:
                axis = 5
            self.widgets["Combi_DRO_%s" % axis].set_property("abs_color", gtk.gdk.color_parse(self.abs_color))
            self.widgets["Combi_DRO_%s" % axis].set_property("rel_color", gtk.gdk.color_parse(self.rel_color))
            self.widgets["Combi_DRO_%s" % axis].set_property("dtg_color", gtk.gdk.color_parse(self.dtg_color))
            self.widgets["Combi_DRO_%s" % axis].set_property("homed_color", gtk.gdk.color_parse(self.homed_color))
            self.widgets["Combi_DRO_%s" % axis].set_property("unhomed_color", gtk.gdk.color_parse(self.unhomed_color))

        self.widgets.adj_start_spindle_RPM.set_value(self.spindle_start_rpm)
        self.widgets.gcode_view.set_sensitive(False)
        self.tooledit_btn_delete_tool = self.widgets.tooledit1.wTree.get_object("delete")
        self.tooledit_btn_add_tool = self.widgets.tooledit1.wTree.get_object("add")
        self.tooledit_btn_reload_tool = self.widgets.tooledit1.wTree.get_object("reload")
        self.tooledit_btn_apply_tool = self.widgets.tooledit1.wTree.get_object("apply")
        self.widgets.tooledit1.hide_buttonbox(True)
        self.widgets.ntb_user_tabs.remove_page(0)

        if not self.get_ini_info.get_embedded_tabs()[2]:
            self.widgets.tbtn_user_tabs.set_sensitive(False)

        # call the function to change the button status
        # so every thing is ready to start
        widgetlist = ["btn_homing", "btn_touch", "btn_tool",
                      "btn_index_tool",
                      "btn_change_tool", "btn_select_tool_by_no",
                     ]
        self._sensitize_widgets(widgetlist, False)

        # this must be done last, otherwise we will get wrong values
        # because the window is not fully realized
        self.init_notification()

        # since the main loop is needed to handle the UI and its events, blocking calls like sleep()
        # will block the UI as well, so everything goes through event handlers (aka callbacks)
        # The gobject.timeout_add() function sets a function to be called at regular intervals
        # the time between calls to the function, in milliseconds
        gobject.timeout_add(100, self._periodic) # time between calls to the function, in milliseconds

    def _get_axis_list (self):
        temp = self.get_ini_info.get_coordinates()
        self.axis_list = []
        for letter in temp:
            if letter.lower() in self.axis_list:
                continue
            if not letter.lower() in ["x", "y", "z", "a", "b", "c", "u", "v", "w"]:
                continue
            self.axis_list.append(letter.lower())

    def _init_extra_axes(self):
        # XYZ machine
        if len(self.axis_list) < 4:
            self.widgets.Combi_DRO_4.hide()
            self.widgets.Combi_DRO_5.hide()
            self.widgets.btn_home_4.hide()
            self.widgets.btn_home_5.hide()
            self.widgets.btn_set_value_4.hide()
            self.widgets.btn_set_value_5.hide()
            return

        # to much axes given, can only handle 5
        if len(self.axis_list) > 5:
            message = _("**** LUNINOS INFO : ****")
            message += _("**** luminos can only handle 5 axis, ****\n**** but you have given %d through your INI file ****\n" % len(self.axis_list))
            message += _("**** luminos will not start ****\n\n")
            print(message)
            self.widgets.window1.destroy()

        # find first 5_th axis
        if len(self.axis_list) == 5:
            self.widgets.lbl_replace_4.hide()
            self.widgets.lbl_replace_5.hide()
            self.widgets.lbl_replace_set_value_4.hide()
            self.widgets.lbl_replace_set_value_5.hide()
            self.axisletter_five = self.axis_list[-1]
            self.axisnumber_five = "xyzabcuvw".index(self.axisletter_five)
            self.widgets.Combi_DRO_5.set_property("joint_number", self.axisnumber_five)
            self.widgets.Combi_DRO_5.change_axisletter(self.axisletter_five.upper())

            image = self.widgets["img_home_%s" % self.axisletter_five]
            self.widgets.btn_home_5.set_image(image)
            self.widgets.btn_home_5.set_property("tooltip-text", _("Home axis %s") % self.axisletter_five.upper())

            image = self.widgets["img_set_value_%s" % self.axisletter_five]
            self.widgets.btn_set_value_5.set_image(image)
            self.widgets.btn_set_value_5.set_property("tooltip-text", _("Touch off axis %s") % self.axisletter_five.upper())

            if self.axisletter_five in "abc":
                self.widgets.Combi_DRO_5.set_property("mm_text_template", "%11.2f")
                self.widgets.Combi_DRO_5.set_property("imperial_text_template", "%11.2f")

        if self.axisletter_five:
            axis_four = list(set(self.axis_list) - set(("x", "y", "z")) - set(self.axisletter_five))
        else:
            self.widgets.Combi_DRO_5.hide()
            self.widgets.lbl_replace_4.hide()
            self.widgets.lbl_replace_set_value_4.hide()
            self.widgets.btn_home_5.hide()
            self.widgets.btn_set_value_5.hide()
            axis_four = list(set(self.axis_list) - set(("x", "y", "z")))
        self.axisletter_four = axis_four[0]
        self.axisnumber_four = "xyzabcuvw".index(self.axisletter_four)
        self.widgets.Combi_DRO_4.set_property("joint_number", self.axisnumber_four)
        self.widgets.Combi_DRO_4.change_axisletter(self.axisletter_four.upper())

        image = self.widgets["img_home_%s" % self.axisletter_four]
        self.widgets.btn_home_4.set_image(image)
        self.widgets.btn_home_4.set_property("tooltip-text", _("Home axis %s") % self.axisletter_four.upper())

        image = self.widgets["img_set_value_%s" % self.axisletter_four]
        self.widgets.btn_set_value_4.set_image(image)
        self.widgets.btn_set_value_4.set_property("tooltip-text", _("Touch off axis %s") % self.axisletter_four.upper())

        if self.axisletter_four in "abc":
            self.widgets.Combi_DRO_4.set_property("mm_text_template", "%11.2f")
            self.widgets.Combi_DRO_4.set_property("imperial_text_template", "%11.2f")

        # We have to change the size of the DRO, to make 4 DRO fit the space we got
        if len(self.axis_list) == 5:
            size = 17
        else:
            size = 20
        for axis in self.axis_list:
            if axis == self.axisletter_four:
                axis = 4
            if axis == self.axisletter_five:
                axis = 5
            self.widgets["Combi_DRO_%s" % axis].set_property("font_size", size)

    def _init_preferences(self):
        # check if NO_FORCE_HOMING is used in ini
        self.no_force_homing = self.get_ini_info.get_no_force_homing()
        self.spindle_start_rpm = self.prefs.getpref('spindle_start_rpm', 300 , float)
        self.jog_rate = self.get_ini_info.get_jog_vel()
        self.jog_rate_max = self.get_ini_info.get_max_jog_vel()
        self.spindle_override_max = self.get_ini_info.get_max_spindle_override()
        self.spindle_override_min = self.get_ini_info.get_min_spindle_override()
        self.feed_override_max = self.get_ini_info.get_max_feed_override()

        # set the slider limmits
        self.widgets.adj_max_vel.configure(self.stat.max_velocity * 60, 0.0,
                                           self.stat.max_velocity * 60 , 1, 0, 0)
        self.widgets.adj_jog_vel.configure(self.jog_rate, 0,
                                           self.jog_rate_max , 1, 0, 0)
        self.widgets.adj_spindle.configure(100, self.spindle_override_min * 100,
                                           self.spindle_override_max * 100, 1, 0, 0)
        self.widgets.adj_feed.configure(100, 0, self.feed_override_max * 100, 1, 0, 0)

        # set the text of the speed and override bars
        self.widgets.max_vel_bar.set_text("%d mm/min" % (self.stat.max_velocity * 60))
        self.widgets.jog_vel_bar.set_text("%d mm/min" % (self.jog_rate))

        # set initial hal pin values
        self.halcomp["jog.jog-velocity"] = self.jog_rate
        self.halcomp["jog.max-velocity"] = self.stat.max_velocity
        self.halcomp["feed-override"] = 100.0
        self.halcomp["spindle.override"] = 100.0

        # the scale to apply to the count of the hardware mpg wheel, to avoid to much turning
        default = (self.stat.max_velocity * 60 - self.stat.max_velocity * 0.1) / 100
        self.scale_max_vel = self.prefs.getpref("scale_max_vel", default, float)
        self.widgets.adj_scale_max_vel.set_value(self.scale_max_vel)
        default = (self.jog_rate_max / 100)
        self.scale_jog_vel = self.prefs.getpref("scale_jog_vel", default, float)
        self.widgets.adj_scale_jog_vel.set_value(self.scale_jog_vel)
        self.scale_spindle_override = self.prefs.getpref("scale_spindle_override", 1, float)
        self.widgets.adj_scale_spindle_override.set_value(self.scale_spindle_override)
        self.scale_feed_override = self.prefs.getpref("scale_feed_override", 1, float)
        self.widgets.adj_scale_feed_override.set_value(self.scale_feed_override)

# =============================================================
# Dynamic tabs handling Start
    def _init_dynamic_tabs(self):
        # dynamic tabs setup
        self._dynamic_childs = {}
        # register all tabs, so they will be closed together with the GUI
        atexit.register(self._kill_dynamic_childs)

        tab_names, tab_location, tab_cmd = self.get_ini_info.get_embedded_tabs()
        if not tab_names:
            print (_("**** LUMINOS ERROR ****"))
            print (_("**** Invalid embeded tab configuration ****"))
            print (_("**** No tabs will be added! ****"))
            return

        for t, c , name in zip(tab_names, tab_cmd, tab_location):
            nb = self.widgets[name]
            xid = self._dynamic_tab(nb, t)
            if not xid: continue
            cmd = c.replace('{XID}', str(xid))
            child = subprocess.Popen(cmd.split())
            self._dynamic_childs[xid] = child
            nb.show_all()

    # adds the embedded object to a notebook tab or box
    def _dynamic_tab(self, widget, text):
        socket = gtk.Socket()
        try:
            widget.append_page(socket, gtk.Label(" " + text + " "))
        except:
            try:
                widget.pack_end(socket, True, True, 0)
            except:
                return None
        return socket.get_id()

    # Gotta kill the embedded processes when gmoccapy closes
    def _kill_dynamic_childs(self):
        for child in self._dynamic_childs.values():
            child.terminate()

# Dynamic tabs handling End
# =============================================================

    # first we hide all the axis columns the unhide the ones we want
    # if it's a lathe config we show lathe related columns
    # and we load the tooltable data
    def _init_tooleditor(self):
        self.widgets.tooledit1.set_visible("abcxyzuvwijq", False)
        for axis in self.axis_list:
                self.widgets.tooledit1.set_visible("%s" % axis, True)
#        if len(self.axis_list) > 4:
#            self.widgets.tooledit1.set_property("font", "sans 10")
#        if self.lathe_mode:
#            self.widgets.tooledit1.set_visible("ijq", True)
        # get the path to the tool table
        tooltable = self.get_ini_info.get_toolfile()
        if not tooltable:
            print(_("**** LUMINOS ERROR ****"))
            print(_("**** Did not find a toolfile file in [EMCIO] TOOL_TABLE ****"))
            sys.exit()
        toolfile = os.path.join(CONFIGPATH, tooltable)
        self.widgets.tooledit1.set_filename(toolfile)


    def on_btn_load_clicked(self, widget, data = None):
        self.widgets.ntb_button.set_current_page(8)
        self.widgets.tbtn_fullsize_preview.set_active(True)
        self._show_iconview_tab(True)
        self.widgets.IconFileSelection1.refresh_filelist()
        self.widgets.IconFileSelection1.iconView.grab_focus()

    def _show_offset_tab(self, state):
        page = self.widgets.ntb_preview.get_nth_page(1)
        if page.get_visible() and state or not page.get_visible() and not state:
            return
        if state:
            page.show()
            self._show_tooledit_tab(False)
            self._show_iconview_tab(False)
            self.widgets.ntb_preview.set_property("show-tabs", state)
            self.widgets.ntb_preview.set_current_page(1)
            self.widgets.offsetpage1.mark_active((self.system_list[self.stat.g5x_index]).lower())
        else:
            names = self.widgets.offsetpage1.get_names()
            for system, name in names:
                system_name = "system_name_%s" % system
                self.prefs.putpref(system_name, name, str)
            page.hide()
            self.widgets.tbtn_edit_offsets.set_active(False)
            self.widgets.ntb_preview.set_current_page(0)
            if self.widgets.ntb_preview.get_n_pages() <= 4: # else user tabs are availible
                self.widgets.ntb_preview.set_property("show-tabs", state)

    def _show_tooledit_tab(self, state):
        page = self.widgets.ntb_preview.get_nth_page(2)
#        if page.get_visible()and state or not page.get_visible()and not state:
#            return
        if state:
            page.show()
            self.widgets.ntb_preview.set_property("show-tabs", not state)
            self.widgets.vbox_jog.hide()
            self.widgets.ntb_preview.set_current_page(2)
            self.widgets.tooledit1.set_selected_tool(self.stat.tool_in_spindle)
        else:
            page.hide()
            if self.widgets.ntb_preview.get_n_pages() > 4: # user tabs are availible
                self.widgets.ntb_preview.set_property("show-tabs", not state)
            self.widgets.vbox_jog.show()
            self.widgets.ntb_preview.set_current_page(0)

    def _show_iconview_tab(self, state):
        print ("show iconview tab = ", state)
        page = self.widgets.ntb_preview.get_nth_page(3)
        if state:
            page.show()
            self.widgets.ntb_preview.set_property("show-tabs", not state)
            self.widgets.ntb_preview.set_current_page(3)
            self.widgets.box_info.hide()
            self.widgets.vbox_jog.hide()
            self.widgets.IconFileSelection1.iconView.grab_focus()
        else:
            page.hide()
            if self.widgets.ntb_preview.get_n_pages() > 4: # user tabs are availible
                self.widgets.ntb_preview.set_property("show-tabs", not state)
            self.widgets.ntb_preview.set_current_page(0)
            self.widgets.box_info.show()
            self.widgets.vbox_jog.show()

    def on_btn_sel_next_clicked(self, widget, data = None):
        self.widgets.IconFileSelection1.btn_sel_next.emit("clicked")

    def on_btn_sel_prev_clicked(self, widget, data = None):
        self.widgets.IconFileSelection1.btn_sel_prev.emit("clicked")

    def on_btn_home_clicked(self, widget, data = None):
        self.widgets.IconFileSelection1.btn_home.emit("clicked")

    def on_btn_jump_to_clicked(self, widget, data = None):
        self.widgets.IconFileSelection1.btn_jump_to.emit("clicked")

    def on_btn_dir_up_clicked(self, widget, data = None):
        self.widgets.IconFileSelection1.btn_dir_up.emit("clicked")

    def on_btn_select_clicked(self, widget, data = None):
        self.widgets.IconFileSelection1.btn_select.emit("clicked")

    def on_IconFileSelection1_selected(self, widget, path = None):
        if path:
            # as opening a file do requiere auto mode, we will first check
            # where we are and change back later to our mode.
            # Otherwise it would not be possible to open and esit a file if the machine is off
            mode = self.stat.task_mode
            self.command.mode(linuxcnc.MODE_AUTO)
            self.command.wait_complete()
            self.command.program_open(os.path.abspath(path))
            self.widgets.gcode_view.load_file(os.path.abspath(path))
            self.command.mode(mode)
            self.command.wait_complete()
            self.widgets.ntb_preview.set_current_page(0)
            self.widgets.tbtn_fullsize_preview.set_active(False)
            self.widgets.ntb_button.set_current_page(2)
            self._show_iconview_tab(False)

    def on_ntb_button_switch_page(self, *args):
        message = "ntb_button_page changed to %s" % self.widgets.ntb_button.get_current_page()
        if self.log: self._add_alarm_entry(message)

        if self.widgets.ntb_preview.get_current_page() == 0: # preview tab is active,
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
        if self.widgets.ntb_button.get_current_page() == 6 or self.widgets.ntb_preview.get_current_page() == 3:
            self.widgets.ntb_preview.show()
            self.widgets.hbox_dro.show()
            self.widgets.vbox_jog.set_size_request(360 , -1)
            self.widgets.gcode_view.set_sensitive(0)
            self.widgets.btn_save.set_sensitive(True)
            self.widgets.hal_action_reload.emit("activate")
            self.widgets.ntb_info.show()
            self.widgets.box_info.set_size_request(-1, 200)
            self.widgets.tbl_search.hide()

    # If button exit is klickt, press emergency button bevor closing the application
    def on_btn_exit_clicked(self, widget, data = None):
        print "quit from <btn_exit>"
        print("btn_exit_clicked")
        self.widgets.window1.destroy()

    def on_btn_tgl_DRO_clicked(self, widget, data = None):
        order = self.widgets.Combi_DRO_x.get_order()
        order = [order[2], order[0], order[1]]
        self.on_Combi_DRO_clicked(None, None, order)

    def on_Combi_DRO_clicked(self, widget, joint_number, order):
        for axis in self.axis_list:
            if axis == self.axisletter_four:
                axis = 4
            if axis == self.axisletter_five:
                axis = 5
            self.widgets["Combi_DRO_%s" % axis].set_order(order)

# from here only needed, if the DRO button will remain in luminos
        if order[0] == "Abs" and self.widgets.tbtn_rel.get_label() != "Abs":
            self.widgets.tbtn_rel.set_active(False)
        if order[0] == "Rel" and self.widgets.tbtn_rel.get_label() != self.widgets.Combi_DRO_x.system:
            self.widgets.tbtn_rel.set_active(True)
        if order[0] == "DTG":
            self.widgets.tbtn_dtg.set_active(True)
        else:
            self.widgets.tbtn_dtg.set_active(False)
# to here only needed, if the DRO button will remain in luminos

# from here only needed, if the DRO button will remain in luminos
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

        widgetlist = ["adj_jog_vel", "adj_max_vel"]

        # self.stat.linear_units will return 1.0 for metric and 1/25,4 for imperial
        # display units not equal machine units
        if metric_units != int(self.stat.linear_units):
            # machine units = metric
            if self.stat.linear_units == _MM:
                self.faktor = (1.0 / 25.4)
            # machine units = imperial
            else:
                self.faktor = 25.4
            self._update_slider(widgetlist)
        else:
            # display units equal machine units would be factor = 1,
            # but if factor not equal 1.0 than we have to reconvert from previous first
            if self.faktor != 1.0:
                self.faktor = 1 / self.faktor
                self._update_slider(widgetlist)
                self.faktor = 1.0
                self._update_slider(widgetlist)

        if metric_units:
            # self.widgets.scl_max_vel.set_digits(0)
            # self.widgets.scl_jog_vel.set_digits(0)
            self.widgets.offsetpage1.display_units_mm = _MM
        else:
            # self.widgets.scl_max_vel.set_digits(3)
            # self.widgets.scl_jog_vel.set_digits(3)
            self.widgets.offsetpage1.display_units_mm = _INCH

    def on_tbtn_rel_toggled(self, widget, data = None):
        if self.widgets.tbtn_dtg.get_active():
            self.widgets.tbtn_dtg.set_active(False)
        if widget.get_active():
            widget.set_label(self.widgets.Combi_DRO_x.system)
            order = ["Rel", "Abs", "DTG"]
        else:
            widget.set_label("Abs")
            order = ["Abs", "DTG", "Rel"]
        self.on_Combi_DRO_clicked(None, None, order)

    def on_tbtn_dtg_toggled(self, widget, data = None):
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

    def on_tbtn_units_toggled(self, widget, data = None):
        if widget.get_active():
            widget.set_label("inch")
            metric_units = False
        else:
            widget.set_label("mm")
            metric_units = True
        for axis in self.axis_list:
            if axis == self.axisletter_four:
                axis = 4
            if axis == self.axisletter_five:
                axis = 5
            self.widgets["Combi_DRO_%s" % axis].set_to_inch(not metric_units)
        # set gremlin_units
        self.widgets.gremlin.set_property("metric_units", metric_units)

    def on_chk_auto_units_toggled(self, widget, data = None):
        for axis in self.axis_list:
            if axis == self.axisletter_four:
                axis = 4
            if axis == self.axisletter_five:
                axis = 5
            self.widgets["Combi_DRO_%s" % axis].set_auto_units(self.widgets.chk_auto_units.get_active())
        self.prefs.putpref("use_auto_units", self.widgets.chk_auto_units.get_active(), bool)

    def on_chk_show_dro_btn_toggled(self, widget, data = None):
        if self.widgets.chk_show_dro_btn.get_active():
            self.widgets.tbl_dro_button.show()
            self.widgets.chk_auto_units.set_active(False)
            self.widgets.chk_auto_units.set_sensitive(False)
        else:
            self.widgets.tbl_dro_button.hide()
            self.widgets.chk_auto_units.set_active(True)
            self.widgets.chk_auto_units.set_sensitive(True)
        self.prefs.putpref("show_dro_btn", self.widgets.chk_show_dro_btn.get_active(), bool)
# to here only needed, if the DRO button will remain in luminos

    # choose a theme to aply
    def on_theme_choice_changed(self, widget):
        theme = widget.get_active_text()
        if theme == None:
            return
        if self.log: self._add_alarm_entry("theme changed to %s" % widget.get_active_text())
        self.prefs.putpref('gtk_theme', theme, str)
        settings = gtk.settings_get_default()
        if theme == "Follow System Theme":
            theme = self.default_theme
        settings.set_string_property("gtk-theme-name", theme, "")

    def on_rbt_unlock_toggled(self, widget, data = None):
        if widget.get_active():
            if widget == self.widgets.rbt_use_unlock:
                self.prefs.putpref("unlock_way", "use", str)
            elif widget == self.widgets.rbt_no_unlock:
                self.prefs.putpref("unlock_way", "no", str)
            else:
                self.prefs.putpref("unlock_way", "hal", str)

    def on_rbtn_run_from_line_toggled(self, widget, data = None):
        if widget.get_active():
            if widget == self.widgets.rbtn_no_run_from_line:
                self.prefs.putpref("run_from_line", "no_run", str)
                self.widgets.btn_from_line.set_sensitive(False)
            else: # widget == self.widgets.rbtn_run_from_line:
                self.prefs.putpref("run_from_line", "run", str)
                self.widgets.btn_from_line.set_sensitive(True)

    def on_rbtn_show_preview_toggled(self, widget, data = None):
        self.prefs.putpref("show_preview_on_offset", widget.get_active(), bool)

    def on_adj_scale_max_vel_value_changed(self, widget, data = None):
        self.prefs.putpref("scale_max_vel", widget.get_value(), float)
        self.scale_max_vel = widget.get_value()

    def on_adj_scale_jog_vel_value_changed(self, widget, data = None):
        self.prefs.putpref("scale_jog_vel", widget.get_value(), float)
        self.scale_jog_vel = widget.get_value()

    def on_adj_scale_feed_override_value_changed(self, widget, data = None):
        self.prefs.putpref("scale_feed_override", widget.get_value(), float)
        self.scale_feed_override = widget.get_value()

    def on_adj_scale_spindle_override_value_changed(self, widget, data = None):
        self.prefs.putpref("scale_spindle_override", widget.get_value(), float)
        self.scale_spindle_override = widget.get_value()

    def on_rbtn_fullscreen_toggled(self, widget):
        if self.log: self._add_alarm_entry("rbtn_fullscreen_toggled to %s" % widget.get_active())
        if widget.get_active():
            self.widgets.window1.fullscreen()
            self.prefs.putpref("screen1", "fullscreen", str)
        else:
            self.widgets.window1.unfullscreen()

    def on_rbtn_maximized_toggled(self, widget):
        if self.log: self._add_alarm_entry("rbtn_maximized_toggled to %s" % widget.get_active())
        if widget.get_active():
            self.widgets.window1.maximize()
            self.prefs.putpref("screen1", "maximized", str)
        else:
            self.widgets.window1.unmaximize()

    def on_rbtn_window_toggled(self, widget):
        if self.log: self._add_alarm_entry("rbtn_window_toggled to %s" % widget.get_active())
        self.widgets.spbtn_x_pos.set_sensitive(widget.get_active())
        self.widgets.spbtn_y_pos.set_sensitive(widget.get_active())
        self.widgets.spbtn_width.set_sensitive(widget.get_active())
        self.widgets.spbtn_height.set_sensitive(widget.get_active())
        # we have to check also if the window is active, because the button is toggled the first time
        # before the window is shown
        if widget.get_active() and self.widgets.window1.is_active():
            self.widgets.window1.move(int(self.widgets.adj_x_pos.get_value()),
                                      int(self.widgets.adj_y_pos.get_value()))
            self.widgets.window1.resize(int(self.widgets.adj_width.get_value()),
                                        int(self.widgets.adj_height.get_value()))
            self.prefs.putpref("screen1", "window", str)

    def on_adj_x_pos_value_changed(self, widget, data = None):
        self.prefs.putpref("x_pos", widget.get_value(), float)
        ypos = int(self.prefs.getpref("y_pos", 10, float))
        self.widgets.window1.move(int(widget.get_value()), ypos)

    def on_adj_y_pos_value_changed(self, widget, data = None):
        self.prefs.putpref("y_pos", widget.get_value(), float)
        xpos = int(self.prefs.getpref("x_pos", 10, float))
        self.widgets.window1.move(xpos, int(widget.get_value()))

    def on_adj_width_value_changed(self, widget, data = None):
        self.prefs.putpref("width", widget.get_value(), float)
        height = int(self.prefs.getpref("height", 750, float))
        self.widgets.window1.resize(int(widget.get_value()), height)

    def on_adj_height_value_changed(self, widget, data = None):
        self.prefs.putpref("height", widget.get_value(), float)
        width = int(self.prefs.getpref("width", 979, float))
        self.widgets.window1.resize(width, int(widget.get_value()))

    def on_chk_hide_cursor_toggled(self, widget, data = None):
        if self.log: self._add_alarm_entry("hide_cursor_toggled to %s" % widget.get_active())
        self.prefs.putpref("hide_cursor", widget.get_active(), bool)
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
        if self.log: self._add_alarm_entry("rel color set to %s" % color)
        self.prefs.putpref('rel_color', color, str)
        self._change_dro_color("rel_color", color)
        self.rel_color = str(color)

    def on_abs_colorbutton_color_set(self, widget):
        color = widget.get_color()
        if self.log: self._add_alarm_entry("abs color set to %s" % color)
        self.prefs.putpref('abs_color', widget.get_color(), str)
        self._change_dro_color("abs_color", color)
        self.abs_color = str(color)

    def on_dtg_colorbutton_color_set(self, widget):
        color = widget.get_color()
        if self.log: self._add_alarm_entry("dtg color set to %s" % color)
        self.prefs.putpref('dtg_color', widget.get_color(), str)
        self._change_dro_color("dtg_color", color)
        self.dtg_color = str(color)

    def on_homed_colorbtn_color_set(self, widget):
        color = widget.get_color()
        if self.log: self._add_alarm_entry("homed color set to %s" % color)
        self.prefs.putpref('homed_color', widget.get_color(), str)
        self._change_dro_color("homed_color", color)
        self.homed_color = str(color)

    def on_unhomed_colorbtn_color_set(self, widget):
        color = widget.get_color()
        if self.log: self._add_alarm_entry("unhomed color set to %s" % color)
        self.prefs.putpref('unhomed_color', widget.get_color(), str)
        self._change_dro_color("unhomed_color", color)
        self.unhomed_color = str(color)

    def on_file_to_load_chooser_file_set(self, widget):
        if self.log: self._add_alarm_entry("file to load on startup set to : %s" % widget.get_filename())
        self.prefs.putpref("open_file", widget.get_filename(), str)

    def on_jump_to_dir_chooser_file_set(self, widget, data = None):
        if self.log: self._add_alarm_entry("jump to dir has been set to : %s" % widget.get_filename())
        self.prefs.putpref("jump_to_dir", widget.get_filename(), str)
        self.widgets.IconFileSelection1.set_property("jump_to_dir", widget.get_filename())

    def on_grid_size_value_changed(self, widget, data = None):
        self.widgets.gremlin.set_property('grid_size', widget.get_value())
        self.prefs.putpref('grid_size', widget.get_value(), float)

    def on_tbtn_log_actions_toggled(self, widget, data = None):
        self.log = widget.get_active()
        self.prefs.putpref("log_actions", widget.get_active(), bool)

    def on_chk_show_dro_toggled(self, widget, data = None):
        if self.log: self._add_alarm_entry("show_gremlin_DRO =  %s" % widget.get_active())
        self.widgets.gremlin.set_property("metric_units", self.widgets.Combi_DRO_x.metric_units)
        self.widgets.gremlin.set_property("enable_dro", widget.get_active())
        self.prefs.putpref("enable_dro", widget.get_active(), bool)
        self.widgets.chk_show_offsets.set_sensitive(widget.get_active())
        self.widgets.chk_show_dtg.set_sensitive(widget.get_active())

    def on_chk_show_dtg_toggled(self, widget, data = None):
        if self.log: self._add_alarm_entry("show_gremlin_DTG =  %s" % widget.get_active())
        self.widgets.gremlin.set_property("show_dtg", widget.get_active())
        self.prefs.putpref("show_dtg", widget.get_active(), bool)

    def on_chk_show_offsets_toggled(self, widget, data = None):
        if self.log: self._add_alarm_entry("show_offset_button_toggled to %s" % widget.get_active())
        self.widgets.gremlin.show_offsets = widget.get_active()
        self.prefs.putpref("show_offsets", widget.get_active(), bool)

    def on_cmb_mouse_button_mode_changed(self, widget):
        index = widget.get_active()
        self.widgets.gremlin.set_property("mouse_btn_mode", index)
        self.prefs.putpref("mouse_btn_mode", index, int)

    def on_tbtn_setup_toggled(self, widget, data = None):
        # first we set to manual mode, as we do not allow changing settings in other modes
        # otherwise external halui commands could start a program while we are in settings
        self.command.mode(linuxcnc.MODE_MANUAL)
        self.command.wait_complete()

        if widget.get_active():
            # deactivate the mode buttons, so changing modes is not possible while we are in settings mode
            print("tbtn_setup_pressed")
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
                if dialogs.system_dialog(self):
                    code = True
            # Lets see if the user has the right to enter settings
            if code:
                self.widgets.ntb_main.set_current_page(1)
                self.widgets.ntb_setup.set_current_page(0)
                self.widgets.ntb_button.set_current_page(5)
            else:
                if self.widgets.rbt_hal_unlock.get_active():
                    message = _("Hal Pin is low, Access denied")
                else:
                    message = _("wrong code entered, Access denied")
                dialogs.warning_dialog(self, _("Just to warn you"), message)
                self._add_alarm_entry(message)
                self.widgets.tbtn_setup.set_active(False)
        else:
            self.widgets.ntb_main.set_current_page(0)
            self.widgets.ntb_button.set_current_page(0)
            self.widgets.ntb_jog.set_current_page(0)

    # this are hal-tools functions
    def on_btn_show_hal_clicked(self, widget, data = None):
        p = os.popen("tclsh %s/bin/halshow.tcl &" % TCLPATH)

    def on_btn_calibration_clicked(self, widget, data = None):
        p = os.popen("tclsh %s/bin/emccalib.tcl -- -ini %s > /dev/null &" % (TCLPATH, sys.argv[2]), "w")

    def on_btn_hal_meter_clicked(self, widget, data = None):
        p = os.popen("halmeter &")

    def on_btn_status_clicked(self, widget, data = None):
        p = os.popen("linuxcnctop  > /dev/null &", "w")

    def on_btn_hal_scope_clicked(self, widget, data = None):
        p = os.popen("halscope  > /dev/null &", "w")

    def on_btn_classicladder_clicked(self, widget, data = None):
        if  hal.component_exists("classicladder_rt"):
            p = os.popen("classicladder  &", "w")
        else:
            dialogs.warning_dialog(self, _("INFO:"),
                                  _("Classicladder real-time component not detected"))
            self._add_alarm_entry(_("ladder not available - is the real-time component loaded?"))

    def on_btn_del_message_clicked(self, widget, data = None):
        # if there are error and user messages, we will delete first the errors
        if self.halcomp["error"] == True:
            number = []
            messages = self.notification.messages
            for message in messages:
                if message[2] == ALERT_ICON:
                    number.append(message[0])
                print message
            self.notification.del_message(number[0])
            if len(number) == 1:
                self.halcomp["error"] = False
        else:
            self.notification.del_last()

    def on_btn_stop_clicked(self, widget, data = None):
        print("stop clicked")
        self.start_line = 0
        self.widgets.gcode_view.set_line_number(0)
        self.widgets.tbtn_pause.set_active(False)

    def on_btn_run_clicked(self, widget, data = None):
        self.command.auto(linuxcnc.AUTO_RUN, self.start_line)

#-----------------------------------------------------------
# spindle stuff
#-----------------------------------------------------------

    def _on_spindle_forward_changed(self, pin):
        if self.log: self._add_alarm_entry("spindle_forward_recieved")
        if pin.get():
            self._set_spindle("forward")
            print("spindle_forward_recieved")

    def _on_spindle_reverse_changed(self, pin):
        if self.log: self._add_alarm_entry("spindle_reverse_recieved")
        if pin.get():
            self._set_spindle("reverse")
            print("spindle_reverse_recieved")

    def _on_spindle_stop_changed(self, pin):
        if self.log: self._add_alarm_entry("spindle_stop_recieved")
        if pin.get():
            self._set_spindle("stop")
            print("spindle_stop_recieved")

    def _set_spindle(self, command):
        # if we are in estop state, we will have to leave here, otherwise
        # we get an error, that switching spindle off is not allowed with estop
        if self.stat.task_state == linuxcnc.STATE_ESTOP:
            return
        rpm = self._check_spindle_range()
        # as the commanded value will be multiplied with speed override,
        # we take care of that
        rpm_out = rpm / self.stat.spindlerate
        self.widgets.lbl_spindle_act.set_label("S %s" % int(rpm))
        if self.log: self._add_alarm_entry("Spindle set to %i rpm, mode is %s" % (rpm, self.stat.task_mode))

        # if we do not check this, we will get an error in auto mode
        if self.stat.task_mode == linuxcnc.MODE_AUTO:
            if self.stat.interp_state == linuxcnc.INTERP_READING or self.stat.interp_state == linuxcnc.INTERP_WAITING:
                return
        if command == "stop":
            self.command.spindle(0)
            self.widgets.lbl_spindle_act.set_label("S 0")
        if command == "forward":
            self.command.spindle(1, rpm_out)
        elif command == "reverse":
            self.command.spindle(-1, rpm_out)
        else:
            self._add_alarm_entry(_("Something went wrong, we have an unknown widget"))

    def _check_spindle_range(self):
        rpm = self.stat.settings[2]
        if rpm == 0:
            rpm = abs(self.spindle_start_rpm)

        spindle_override = self.widgets.adj_spindle.get_value() / 100
        real_spindle_speed = rpm * spindle_override

        if real_spindle_speed > self.max_spindle_rev:
            real_spindle_speed = self.max_spindle_rev
        elif real_spindle_speed < self.min_spindle_rev:
            real_spindle_speed = self.min_spindle_rev
        return real_spindle_speed

    def on_btn_spindle_100_clicked(self, widget, data = None):
        if self.log: self._add_alarm_entry("spindle override has been reseted to 100 %")
        self.widgets.adj_spindle.set_value(100)





    # The actions of the buttons
    def _on_h_button_changed(self, pin):
        self._add_alarm_entry("got h_button_signal %s" % pin.name)
        # we check if the button is pressed ore release,
        # otehrwise a signal will be emitted, wenn the button is released and
        # the signal drob down to zero
        if not pin.get():
            return
        # lets see on witch button_box we are
        page = self.widgets.ntb_button.get_current_page()
        # witch button has been pressed
        btn = str(pin.name)
        # from the list we declared under __init__ we get the button number
        nr = int(btn[-1])
        tab = self.h_tabs[page] # see in the __init__ section for the declaration of self.tabs
        button = None
        # we check if there is a button or the user pressed a hardware button under
        # a non existing software button
        for index in tab:
            if int(index[0]) == nr:
                # this is the name of the button
                button = index[1]
        if button:
            # only emit a signal if the button is sensitive, otherwise
            # running actions may be interupted
            if self.widgets[button].get_sensitive() == False:
                print("%s not_sensitive" % button)
                self._add_alarm_entry("%s not_sensitive" % button)
                return
            self.widgets[button].emit("clicked")
        else:
            # as we are generating the macro buttons dynamecely, we can"t use the same
            # method as above, here is how we do it
            if page == 1: # macro page
                # does the user press a valid hardware button?
                if nr < len(self.macrobuttons):
                    button = self.macrobuttons[nr] # This list is generated in add_macros_buttons(self)
                    # is the button sensitive?
                    if button.get_sensitive() == False:
                        print("%s not_sensitive" % button)
                        return
                    button.emit("pressed")
                else:
                    print("No function on this button")
                    self._add_alarm_entry("%s not_sensitive" % button)
            else:
                print("No function on this button")

    def _on_v_button_changed(self, pin):
        self._add_alarm_entry("got v_button_signal %s" % pin.name)
        if not pin.get():
            return
        btn = str(pin.name)
        nr = int(btn[-1])
        tab = self.v_tabs # see in the __init__ section for the declaration of self.tabs
        button = None
        for index in tab:
            if int(index[0]) == nr:
                # this is the name of the button
                button = index[1]
        if button:
            # only emit a signal if the button is sensitive, otherwise
            # running actions may be interupted
            if self.widgets[button].get_sensitive() == False:
                print("%s not_sensitive" % button)
                self._add_alarm_entry("%s not_sensitive" % button)
                return
            button_pressed_list = ()
            button_toggled_list = ("tbtn_setup")
            if button in button_pressed_list:
                self.widgets[button].set_active(True)
                self.widgets[button].emit("pressed")
            elif button in button_toggled_list:
                self.widgets[button].set_active(not self.widgets[button].get_active())
            else:
                self.widgets[button].emit("clicked")
        else:
            print("No button found in v_tabs from %s" % pin.name)
            self._add_alarm_entry("No button found in v_tabs from %s" % pin.name)

    def _on_message_deleted(self, widget, messages):
        number = []
        for message in messages:
            if message[2] == ALERT_ICON:
                number.append(message[0])
        if len(number) == 0:
            self.halcomp["error"] = False

    def _del_message_changed(self, pin):
        if pin.get():
            if len(self.notification.messages) == 0:
                return
            self.on_btn_del_message_clicked(None)

    # There are some settings we can only do if the window is on the screen allready
    def on_window1_show(self, widget, data = None):

        self.bg_color = self.widgets.eb_side_button.style.bg[gtk.STATE_NORMAL]

        # it is time to get the correct estop state and set the button status
        self.stat.poll()
        if self.stat.task_state == linuxcnc.STATE_ESTOP:
            self.widgets.eb_side_button.modify_bg(gtk.STATE_NORMAL, gtk.gdk.Color(red = 65535))
            print("Estop active")
        else:
            print("Estop not active")

        # if a file should be loaded, we will do so
        file = self.prefs.getpref("open_file", "", str)
        if file :
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
            xpos = int(self.prefs.getpref("x_pos", 10, float))
            ypos = int(self.prefs.getpref("y_pos", 10, float))
            width = int(self.prefs.getpref("width", 979, float))
            height = int(self.prefs.getpref("height", 750, float))
            self.widgets.window1.move(xpos, ypos)
            self.widgets.window1.resize(width, height)

        # does the user want to show screen2
        self.widgets.tbtn_use_screen2.set_active(self.prefs.getpref("use_screen2", False, bool))
        self.command.mode(linuxcnc.MODE_MANUAL)
        self.command.wait_complete()

    # kill keyboard and estop machine before closing
    def on_window1_destroy(self, widget, data = None):
        print "estopping / killing luminos"
        self.command.state(linuxcnc.STATE_OFF)
        self.command.state(linuxcnc.STATE_ESTOP)
        time.sleep(2)
        gtk.main_quit()

# =========================================================
# bottom button Start

    # The homing functions
    def on_btn_homing_clicked(self, widget, data = None):
        print("btn_homing_clicked")
        self.widgets.ntb_button.set_current_page(3)

    def on_btn_home_all_clicked(self, widget, data = None):
        print("button ref all clicked")
        # home -1 means all
        self.command.home(-1)

    def on_btn_home_selected_clicked(self, widget, data = None):
        print("button home selected clicked")
        if widget == self.widgets.btn_home_x:
            axis = 0
        elif widget == self.widgets.btn_home_y:
            axis = 1
        elif widget == self.widgets.btn_home_z:
            axis = 2
        elif widget == self.widgets.btn_home_4:
            axis = "xyzabcuvw".index(self.axisletter_four)
        elif widget == self.widgets.btn_home_5:
            axis = "xyzabcuvw".index(self.axisletter_four)
        self.command.home(axis)

    def on_btn_unhome_all_clicked(self, widget, data = None):
        print("button unhome all clicked")
        self.all_homed = False
        # -1 for all
        self.command.unhome(-1)

    def on_tbtn_fullsize_preview_toggled(self, widget, data = None):
        if self.log: self._add_alarm_entry("Fullsize Preview set to %s" % widget.get_active())
        if widget.get_active():
            self.widgets.box_info.hide()
            self.widgets.vbox_jog.hide()
            self.widgets.gremlin.set_property("metric_units", self.widgets.Combi_DRO_x.metric_units)
            self.widgets.gremlin.set_property("enable_dro", True)
        else:
            self.widgets.box_info.show()
            self.widgets.vbox_jog.show()
            if not self.widgets.chk_show_dro.get_active():
                self.widgets.gremlin.set_property("enable_dro", False)

    # Three back buttons to be able to leave notebook pages
    # All use the same callback offset
    def on_btn_back_clicked(self, widget, data = None):
        if self.log: self._add_alarm_entry("btn_back_clicked")
        if self.widgets.ntb_button.get_current_page() == 6: # edit mode, go back to auto_buttons
            self.widgets.ntb_button.set_current_page(2)
        elif self.widgets.ntb_button.get_current_page() == 8: # File selection mode
            self.widgets.ntb_button.set_current_page(2)
        else: # else we go to main button on manual
            self.widgets.ntb_button.set_current_page(0)
            self.widgets.ntb_main.set_current_page(0)
            self.widgets.ntb_preview.set_current_page(0)

    # The offset settings, set to zero
    def on_btn_touch_clicked(self, widget, data = None):
        if self.log: self._add_alarm_entry("btn_touch_clicked")
        self.widgets.ntb_button.set_current_page(4)
        self._show_offset_tab(True)
        if self.widgets.rbtn_show_preview.get_active():
            self.widgets.ntb_preview.set_current_page(0)

    def on_tbtn_edit_offsets_toggled(self, widget, data = None):
        state = widget.get_active()
        self.widgets.offsetpage1.edit_button.set_active(state)
        widgetlist = ["btn_zero_g92", "btn_block_height", "btn_set_value_x", "btn_set_value_y", "btn_set_value_z",
                      "btn_set_value_4", "btn_set_value_5", "btn_zero_g92", "btn_set_selected", "ntb_jog"
                     ]
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
#            self._replace_list_item(4, "btn_block_height", "btn_zero_g92")
        elif not state and self.widgets.chk_use_tool_measurement.get_active():
            self.widgets.btn_zero_g92.hide()
            self.widgets.btn_block_height.show()
#            self._replace_list_item(4, "btn_zero_g92", "btn_block_height")

        if not state: # we must switch back to manual mode, otherwise jogging is not possible
            self.command.mode(linuxcnc.MODE_MANUAL)
            self.command.wait_complete()

    def on_offsetpage1_selection_changed(self, widget, system, name):
        if self.log: self._add_alarm_entry("Selection Changed to %s %s" % (system, name))
        if system not in self.system_list[1: ] or self.widgets.tbtn_edit_offsets.get_active():
            self.widgets.btn_set_selected.set_sensitive(False)
        else:
            self.widgets.btn_set_selected.set_sensitive(True)

    def on_btn_zero_g92_clicked(self, widget, data = None):
        self.widgets.offsetpage1.zero_g92(self)

    def on_btn_block_height_clicked(self, widget, data = None):
        probeheight = self.widgets.spbtn_probe_height.get_value()
        blockheight = dialogs.entry_dialog(self, data = None, header = _("Enter the block height"),
                                   label = _("Block height measured from base table"), integer = False)

        if blockheight == "CANCEL" or blockheight == "ERROR":
            return
        if blockheight != False or blockheight == 0:
            self.halcomp["tool.blockheight"] = blockheight
            self.halcomp["tool.probeheight"] = probeheight
            self.prefs.putpref("tool.blockheight", blockheight, float)
            self.prefs.putpref("tool.probeheight", probeheight, float)
        else:
            self.prefs.putpref("tool.blockheight", 0.0, float)
            self.prefs.putpref("tool.probeheight", 0.0, float)
            print(_("Conversion error in btn_block_height"))
            self._add_alarm_entry(_("Offset conversion error because off wrong entry"))
            dialogs.warning_dialog(self, _("Conversion error in btn_block_height!"),
                                  _("Please enter only numerical values\nValues have not been applied"))

        # set koordinate system to new origin
        origin = self.get_ini_info.get_axis_2_min_limit() + blockheight
        self.command.mode(linuxcnc.MODE_MDI)
        self.command.wait_complete()
        self.command.mdi("G10 L2 P0 Z%s" % origin)
        self.widgets.hal_action_reload.emit("activate")
        self.command.mode(linuxcnc.MODE_MANUAL)
        self.command.wait_complete()

    def on_btn_set_value_clicked(self, widget, data = None):
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
            # normaly this can not hapen.
            self._add_alarm_entry(_("Offset could not be set, because off unknown axis %s") % axis)
            self._show_error((11, _("Offset could not be set, because off unknown axis %s" % axis)))
            return
        self._add_alarm_entry("btn_set_value_%s_clicked" % axis)
        preset = self.prefs.getpref("offset_axis_%s" % axis, 0, float)
        offset = dialogs.entry_dialog(self, data = preset, header = _("Enter value for axis %s") % axis.upper(),
                                   label = _("Set axis %s to:") % axis.upper(), integer = False)
        if offset == "CANCEL" or offset == "ERROR":
            return
        if offset != False or offset == 0:
            self._add_alarm_entry(_("offset {0} set to {1:f}").format(axis, offset))
            self.command.mode(linuxcnc.MODE_MDI)
            self.command.wait_complete()
            command = "G10 L20 P0 %s%f" % (axis, offset)
            self.command.mdi(command)
            self.widgets.hal_action_reload.emit("activate")
            self.command.mode(linuxcnc.MODE_MANUAL)
            self.command.wait_complete()
            self.prefs.putpref("offset_axis_%s" % axis, offset, float)
        else:
            print(_("Conversion error in btn_set_value"))
            self._add_alarm_entry(_("Offset conversion error because off wrong entry"))
            dialogs.warning_dialog(self, _("Conversion error in btn_set_value!"),
                                  _("Please enter only numerical values\nValues have not been applied"))

    def on_btn_set_selected_clicked(self, widget, data = None):
        system , name = self.widgets.offsetpage1.get_selected()
        if not system:
            message = _("you did not selected a system to be changed to, so nothing will be changed")
            self._add_alarm_entry(message)
            self._show_error((13, message))
            return
        if system == self.system_list[self.stat.g5x_index]:
            return
        else:
            self.command.mode(linuxcnc.MODE_MDI)
            self.command.wait_complete()
            self.command.mdi(system)
            self.command.mode(linuxcnc.MODE_MANUAL)
            self.command.wait_complete()

    # tool stuff
    def on_btn_tool_clicked(self, widget, data = None):
        if self.log: self._add_alarm_entry("btn_tool_clicked")
        if self.widgets.tbtn_fullsize_preview.get_active():
            self.widgets.tbtn_fullsize_preview.set_active(False)
        self.widgets.ntb_button.set_current_page(7)
        self._show_tooledit_tab(True)

    def on_btn_delete_tool_clicked(self, widget, data = None):
        if self.log: self._add_alarm_entry("on_btn_delete_tool_clicked")
        self.tooledit_btn_delete_tool.emit("clicked")

    def on_btn_add_tool_clicked(self, widget, data = None):
        if self.log: self._add_alarm_entry("on_btn_add_tool_clicked")
        self.tooledit_btn_add_tool.emit("clicked")

    def on_btn_reload_tooltable_clicked(self, widget, data = None):
        if self.log: self._add_alarm_entry("on_btn_reload_tooltable_clicked")
        self.tooledit_btn_reload_tool.emit("clicked")

    def on_btn_apply_tool_changes_clicked(self, widget, data = None):
        if self.log: self._add_alarm_entry("on_btn_apply_tool_changes_clicked")
        self.tooledit_btn_apply_tool.emit("clicked")
        tool = self.widgets.tooledit1.get_selected_tool()

    # select a tool entering a number
    def on_btn_select_tool_by_no_clicked(self, widget, data = None):
        value = dialogs.entry_dialog(self, data = None, header = _("Enter the tool number as integer "),
                                   label = _("Select the tool to change"), integer = True)
        if value == "ERROR":
            message = _("Conversion error because of wrong entry for tool number\n")
            message += _("enter only integer nummbers")
            self._show_error((11, message))
            self._add_alarm_entry(message)
            return
        elif value == "CANCEL":
            self._add_alarm_entry(_("entry for selection of tool number has been canceled"))
            return
        elif int(value) == self.stat.tool_in_spindle:
            message = _("Selected tool is already in spindle, no change needed.")
            self._show_error((13, message))
            self._add_alarm_entry(message)
            return
        else:
            self.tool_change = True
            self.command.mode(linuxcnc.MODE_MDI)
            self.command.wait_complete()
            command = "T%s M6" % int(value)
            self.command.mdi(command)

    # set tool with M61 Q? or with T? M6
    def on_btn_selected_tool_clicked(self, widget, data = None):
        tool = self.widgets.tooledit1.get_selected_tool()
        if tool == None:
            message = _("you selected no or more than one tool, the tool selection must be unique")
            self._show_error((13, message))
            self._add_alarm_entry(message)
            return
        if tool == self.stat.tool_in_spindle:
            message = _("Selected tool is already in spindle, no change needed.")
            self._show_error((13, message))
            self._add_alarm_entry(message)
            return
        if tool or tool == 0:
            self.tool_change = True
            tool = int(tool)
            self.command.mode(linuxcnc.MODE_MDI)
            self.command.wait_complete()

            if widget == self.widgets.btn_change_tool:
                command = "T%s M6" % tool
            else:
                command = "M61 Q%s" % tool
            self.command.mdi(command)
            if self.log: self._add_alarm_entry("set_tool_with %s" % command)
        else:
            message = _("Could not understand the entered tool number. Will not change anything")
            self._show_error((13, message))

# bottom button End
# =========================================================

# =========================================================
# gremlin relevant calls Start

    def on_rbt_view_p_toggled(self, widget, data = None):
        print ("radio button p toggled to ", self.widgets.rbt_view_p.get_active())
        if self.log: self._add_alarm_entry("rbt_view_p_toggled")
        if self.widgets.rbt_view_p.get_active():
            self.widgets.gremlin.set_property("view", "p")
        self.prefs.putpref("gremlin_view", "rbt_view_p", str)

    def on_rbt_view_x_toggled(self, widget, data = None):
        if self.log: self._add_alarm_entry("rbt_view_x_toggled")
        if self.widgets.rbt_view_x.get_active():
            self.widgets.gremlin.set_property("view", "x")
        self.prefs.putpref("gremlin_view", "rbt_view_x", str)

    def on_rbt_view_y_toggled(self, widget, data = None):
        if self.log: self._add_alarm_entry("rbt_view_y_toggled")
        if self.widgets.rbt_view_y.get_active():
            self.widgets.gremlin.set_property("view", "y")
        self.prefs.putpref("gremlin_view", "rbt_view_y", str)

    def on_rbt_view_z_toggled(self, widget, data = None):
        if self.log: self._add_alarm_entry("rbt_view_z_toggled")
        if self.widgets.rbt_view_z.get_active():
            self.widgets.gremlin.set_property("view", "z")
        self.prefs.putpref("gremlin_view", "rbt_view_z", str)

    def on_rbt_view_y2_toggled(self, widget, data = None):
        if self.log: self._add_alarm_entry("rbt_view_y2_toggled")
        if self.widgets.rbt_view_y2.get_active():
            self.widgets.gremlin.set_property("view", "y2")
        self.prefs.putpref("gremlin_view", "rbt_view_y2", str)

    def on_btn_zoom_in_clicked(self, widget, data = None):
        if self.log: self._add_alarm_entry("btn_zoom_in_clicked")
        self.widgets.gremlin.zoom_in()

    def on_btn_zoom_out_clicked(self, widget, data = None):
        if self.log: self._add_alarm_entry("btn_zoom_out_clicked")
        self.widgets.gremlin.zoom_out()

    def on_btn_delete_view_clicked(self, widget, data = None):
        if self.log: self._add_alarm_entry("btn_delete_view_clicked")
        self.widgets.gremlin.clear_live_plotter()

    def on_tbtn_view_dimension_toggled(self, widget, data = None):
        if self.log: self._add_alarm_entry("tbtn_view_dimensions_toggled")
        self.widgets.gremlin.set_property("show_extents_option", widget.get_active())
        self.prefs.putpref("view_dimension", self.widgets.tbtn_view_dimension.get_active(), bool)

    def on_tbtn_view_tool_path_toggled(self, widget, data = None):
        if self.log: self._add_alarm_entry("btn_view_tool_path_clicked")
        self.widgets.gremlin.set_property("show_live_plot", widget.get_active())
        self.prefs.putpref("view_tool_path", self.widgets.tbtn_view_tool_path.get_active(), bool)

    def on_gremlin_line_clicked(self, widget, line):
        self.widgets.gcode_view.set_line_number(line)

# gremlin relevant calls End
# =========================================================


# =========================================================
# helpers Start

    def _init_jog_increments(self):
        # we will get the jog increments to select the Jog-rates with hal pin
        # We do this dynamicly, because users are able to set them in INI File

        # We get the increments from INI File
        increments = self.get_ini_info.get_increments()
        # if the user has given valid List, we do will have to insert 0 for continious
        # If the user entry was missing, get ini info has done that for us
        if increments[0] != 0:
            increments.insert(0, 0)
        self.jog_increments = []
        for jogincr in increments:
            self.jog_increments.append(self._parse_increment(jogincr))
        number = 0

    def _on_pin_incr_changed(self, pin, number):
        if not pin.get():
            return
        self.halcomp["jog.jog-increment"] = self.jog_increments[int(number)]

    def _parse_increment(self, jogincr):
        if jogincr == 0:
            return 0
        elif jogincr.endswith("mm"):
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

    def _from_internal_linear_unit(self, value):
        # self.stat.linear_units returns 1 for metric and 1/25.4 for imperial
        unit = self.stat.linear_units
        factor = unit * 25.4
        return value * factor

    def _on_pin_jog_changed(self, pin, axisnumber, direction):
        # only in manual mode we will allow jogging the axis at this development state
        if not self.stat.task_mode == linuxcnc.MODE_MANUAL:
            return

        if pin.get():
            velocity = self.halcomp["jog.jog-velocity"] / 60
            if self.halcomp["jog.jog-increment"] <> 0: # incremental jogging
                self.command.jog(linuxcnc.JOG_INCREMENT, axisnumber, direction * velocity, self.halcomp["jog.jog-increment"])
            else: # continuous jogging
                self.command.jog(linuxcnc.JOG_CONTINUOUS, axisnumber, direction * velocity)
        else:
            # If self.halcomp["jog.jog-increment"] not zero, then ther might be more that one push to get more distance
            # i.e. the user pushes 10 times 1 mm to make a distance from 10 mm
            if self.halcomp["jog.jog-increment"] <> 0:
                pass
            else:
                self.command.jog(linuxcnc.JOG_STOP, axisnumber)

    def _on_jv_counts_changed(self, pin, widget):
        counts = pin.get()
        difference = (counts - self.jv_counts) * self.scale_jog_vel
        self.jv_counts = counts
        val = self.widgets[widget].get_value() + difference
        if val < 0:
            val = 0
        self.widgets[widget].set_value(val)
        if self.widgets.Combi_DRO_x.metric_units:
            self.widgets.jog_vel_bar.set_text("%d mm/min" % val)
        else:
            self.widgets.jog_vel_bar.set_text("%.3f inch/min" % val)
        self.halcomp["jog.jog-velocity"] = val * (1 / self.faktor)

    def _on_mv_counts_changed(self, pin, widget):
        counts = pin.get()
        difference = (counts - self.mv_counts) * self.scale_max_vel
        self.mv_counts = counts
        val = self.widgets[widget].get_value() + difference
        if val < 0:
            val = 0
        self.widgets[widget].set_value(val)
        if self.widgets.Combi_DRO_x.metric_units == _MM:
            self.widgets.max_vel_bar.set_text("%d mm/min" % val)
        else:
            self.widgets.max_vel_bar.set_text("%.3f inch/min" % val)
        self.halcomp["jog.max-velocity"] = val * (1 / self.faktor)

    def _on_fo_counts_changed(self, pin, widget):
        counts = pin.get()
        difference = (counts - self.fo_counts) * self.scale_feed_override
        self.fo_counts = counts
        val = self.widgets[widget].get_value() + difference
        if val < 0:
            val = 0
        # this has to be set in try/except, or we get an error while creating the hal pins
        try:
            self.widgets[widget].set_value(val)
            self.widgets.feed_override_bar.set_text("%s \%" % val)
            self.halcomp["override.feed-override"] = val
        except:
            pass

    def _on_so_counts_changed(self, pin, widget):
        counts = pin.get()
        difference = (counts - self.so_counts) * self.scale_spindle_override
        self.so_counts = counts
        val = self.widgets[widget].get_value() + difference
        if val < 0:
            val = 0
        # this has to be set in try/except, or we get an error while creating the hal pins
        try:
            self.widgets[widget].set_value(val)
            self.widgets.spindle_override_bar.set_text("%s \%" % val)
            self.halcomp["spindle.override"] = val
        except:
            pass

    def _init_audio(self):
        # try to add ability for audio feedback to user.
        self._AUDIO_AVAILABLE = False
        try:
            import gst
            self._AUDIO_AVAILABLE = True
            print (_("**** LUMINOS INFO ****"))
            print (_("**** audio available! ****"))
        except:
            print (_("**** LUMINOS INFO ****"))
            print (_("**** no audio available! ****"))
            print(_("**** PYGST libray not installed? ****"))
            return

        if self._AUDIO_AVAILABLE:
            self.audio = player.Player()
            self.alert_sound = self.prefs.getpref('audio_alert', self.alert_sound, str)
            self.error_sound = self.prefs.getpref('audio_error', self.error_sound, str)
            self.widgets.audio_alert_chooser.set_filename(self.alert_sound)
            self.widgets.audio_error_chooser.set_filename(self.error_sound)
        else:
            self.widgets.audio_alert_chooser.set_sensitiv(False)
            self.widgets.audio_error_chooser.set_sensitiv(False)

    # init the preview
    def _init_gremlin(self):
        grid_size = self.prefs.getpref('grid_size', 1.0 , float)
        self.widgets.grid_size.set_value(grid_size)
        self.widgets.gremlin.grid_size = grid_size
        self.widgets.gremlin.set_property("metric_units", int(self.stat.linear_units))
        self.widgets.gremlin.set_property("mouse_btn_mode", self.prefs.getpref("mouse_btn_mode", 4, int))
        self.widgets.tbtn_view_tool_path.set_active(self.prefs.getpref("view_tool_path", True))
        self.widgets.tbtn_view_dimension.set_active(self.prefs.getpref("view_dimension", True))
        view = self.prefs.getpref("gremlin_view", "rbt_view_p", str)
        self.widgets[view].set_active(True)
        view = self.prefs.getpref('view', "p", str)
        self.widgets.gremlin.set_property("view", view)

    def _init_themes(self):
        # If there are themes then add them to combo box
        if os.path.exists(THEMEDIR):
            model = self.widgets.theme_choice.get_model()
            model.clear()
            model.append(("Follow System Theme",))
            temp = 0
            names = os.listdir(THEMEDIR)
            names.sort()
            theme_name = self.prefs.getpref("gtk_theme", "Follow System Theme", str)
            for search, dirs in enumerate(names):
                model.append((dirs,))
                if dirs == theme_name:
                    temp = search + 1
            self.widgets.theme_choice.set_active(temp)
        settings = gtk.settings_get_default()
        if not theme_name == "Follow System Theme":
            settings.set_string_property("gtk-theme-name", theme_name, "")

    # check if macros are in the INI file and add them to MDI Button List
    def _init_macro_button(self):
        macros = self.get_ini_info.get_macros()
        num_macros = len(macros)
        if num_macros > 9:
            message = _("**** LUMINOS INFO ****")
            message += _("\n**** found more than 9 macros, only the first 9 will be used ****")
            print(message)
            num_macros = 9
        for increment in range(0, num_macros):
            name = macros[increment]
            # shorten the name if it is to long
            if len(name) > 11:
                lbl = name[0:10]
            else:
                lbl = macros[increment]
            btn = gtk.Button(lbl, None, False)
            btn.connect("pressed", self._on_btn_macro_pressed, name)
            btn.position = increment
            # we add the button to a list to be able later to see what makro to excecute
            self.macrobuttons.append(btn)
            self.widgets.hbtb_MDI.pack_start(btn, True, True, 0)
            btn.show()
        # if there is still place, we fill it with empty labels, to be sure the button will not be on differnt
        # places if the amount of macros change.
        if num_macros < 9:
            for label_space in range(num_macros, 9):
                lbl = "lbl_sp_%s" % label_space
                lbl = gtk.Label(lbl)
                lbl.position = label_space
                lbl.set_text("")
                self.widgets.hbtb_MDI.pack_start(lbl, True, True, 0)
                lbl.show()
        self.widgets.hbtb_MDI.non_homogeneous = False

    # What to do if a macro button has been pushed
    def _on_btn_macro_pressed(self, widget = None, data = None):
        o_codes = data.split()
        subroutines_path = self.get_ini_info.get_subroutine_path()
        if not subroutines_path:
            message = _("**** LUMINOS ERROR ****")
            message += _("\n**** No subroutine folder or program prefix is given in the ini file **** \n")
            message += _("**** so the corresponding file could not be found ****")
            dialogs.warning_dialog(self, _("Important Warning"), message)
            print(message)
            return
        file = subroutines_path + "/" + o_codes[0] + ".ngc"
        if not os.path.isfile(file):
            message = _("**** LUMINOS ERROR ****")
            message += _("\n**** File %s of the macro could not be found ****\n" % [o_codes[0] + ".ngc"])
            message += _("**** we searched in subdirectory %s ****" % [subroutines_folder])
            dialogs.warning_dialog(self, _("Important Warning"), message)
            print(message)
            return
        command = str("O<" + o_codes[0] + "> call")
        for code in o_codes[1:]:
            parameter = dialogs.entry_dialog(self, data = None, header = _("Enter value:"),
                                          label = _("Set parameter %s to:") % code, integer = False)
            if parameter == "ERROR":
                print(_("conversion error"))
                print(_("Conversion error because off wrong entry for macro %s") % o_codes[0])
                dialogs.warning_dialog(self, _("Conversion error !"),
                                      _("Please enter only numerical values\nValues have not been applied"))
                return
            elif parameter == "CANCEL":
                print(_("entry for macro %s has been canceled") % o_codes[0])
                return
            else:
                print(_("macro {0} , parameter {1} set to {2:f}").format(o_codes[0], code, parameter))
            command = command + " [" + str(parameter) + "] "
# TODO: Should not only clear the plot, but also the loaded programm?
        # self.command.program_open("")
        # self.command.reset_interpreter()
        self.widgets.gremlin.clear_live_plotter()
# TODO: End
        self.command.mdi(command)
        for btn in self.macrobuttons:
            btn.set_sensitive(False)
        # we change the widget_image and use the button to interupt running macros
        self.widgets.btn_show_kbd.set_image(self.widgets.img_brake_macro)
        self.widgets.btn_show_kbd.set_property("tooltip-text", _("interrupt running macro"))

    def _init_luminos_button(self):
        # This is needed only because we connect all the horizontal button
        # to hal pins, so the user can conect them to hardware buttons
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

        tab_ref = [(1, "btn_home_all"), (2, "btn_home_x"), (3, "btn_home_y"),
                   (4, "btn_home_z"), (8, "btn_unhome_all"), (9, "btn_back_ref")
                  ]
        if len(self.axis_list) == 4:
            tab_ref.append((5, "btn_home_4"))
        if len(self.axis_list) == 5:
            tab_ref.append((5, "btn_home_4"))
            tab_ref.append((6, "btn_home_5"))
        self.h_tabs.append(tab_ref)

        tab_touch = [(0, "tbtn_edit_offsets"), (1, "btn_zero_g92"), (2, "btn_block_height"), (3, "btn_set_value_x"),
                     (4, "btn_set_value_y"), (5, "btn_set_value_z"), (6, "btn_set_value_4"), (7, "btn_set_value_5"),
                     (8, "btn_set_selected"), (9, "btn_back_zero")
                    ]
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
                    (3, "btn_apply_tool_changes"), (5, "btn_select_tool_by_no"), (6, "btn_index_tool"),
                    (7, "btn_change_tool"), (9, "btn_back_tool")
                   ]
        self.h_tabs.append(tab_tool)

        tab_file = [(0, "btn_home"), (1, "btn_dir_up"), (3, "btn_sel_prev"), (4, "btn_sel_next"),
                    (5, "btn_jump_to"), (7, "btn_select"), (9, "btn_back_file_load")
                   ]
        self.h_tabs.append(tab_file)

        self.v_tabs = [(5, "tbtn_setup"), (6, "tbtn_user_tabs")
                      ]

    def _update_widgets(self, state):
        widgetlist = ["btn_homing", "btn_touch", "btn_tool",
                      "btn_index_tool",
                      "btn_change_tool", "btn_select_tool_by_no",
                     ]
        self._sensitize_widgets(widgetlist, state)

    # Notification stuff.
    def init_notification(self):
        start_as = "rbtn_" + self.prefs.getpref("screen1", "window", str)
        xpos, ypos = self.widgets.window1.window.get_origin()
        self.notification.set_property('x_pos' , self.widgets.adj_x_pos_popup.get_value())
        self.notification.set_property('y_pos' , self.widgets.adj_y_pos_popup.get_value())
        self.notification.set_property('message_width' , self.widgets.adj_width_popup.get_value())
        if int(self.widgets.adj_max_messages.get_value()) != 0:
            self.notification.set_property('max_messages', self.widgets.adj_max_messages.get_value())
        self.notification.set_property('use_frames', self.widgets.chk_use_frames.get_active())
        self.notification.set_property('font', self.widgets.fontbutton_popup.get_font_name())
# TODO:
        # this ones are not finished yet in notifications, we do add them at
        # a later development state, just to show they are there
        self.notification.set_property('icon_size' , 48)
        self.notification.set_property('top_to_bottom', True)
# TODO: End

    # Icon file selection stuff
    def _init_IconFileSelection(self):
        self.widgets.IconFileSelection1.set_property("start_dir", self.get_ini_info.get_program_prefix())

        iconsize = 48
        self.widgets.IconFileSelection1.set_property("icon_size", iconsize)

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

    def _init_offsetpage(self):
        temp = "xyzabcuvw"
        self.widgets.offsetpage1.set_col_visible(temp, False)
        temp = ""
        for axis in self.axis_list:
            temp = temp + axis
        self.widgets.offsetpage1.set_col_visible(temp, True)
        parameterfile = self.get_ini_info.get_parameter_file()
        if not parameterfile:
            print(_("**** GMOCCAPY ERROR ****"))
            print(_("**** Did not find a parameter file in [RS274NGC] PARAMETER_FILE ****"))
            sys.exit()
        path = os.path.join(CONFIGPATH, parameterfile)
        self.widgets.offsetpage1.set_display_follows_program_units()
        if self.stat.program_units != 1:
            self.widgets.offsetpage1.set_to_mm()
            self.widgets.offsetpage1.machine_units_mm = _MM
        else:
            self.widgets.offsetpage1.set_to_inch()
            self.widgets.offsetpage1.machine_units_mm = _INCH
        self.widgets.offsetpage1.set_filename(path)
        self.widgets.offsetpage1.hide_buttonbox(True)
        self.widgets.offsetpage1.set_row_visible("1", False)
        self.widgets.offsetpage1.set_font("sans 12")
        self.widgets.offsetpage1.set_foreground_color("#28D0D9")
        self.widgets.offsetpage1.selection_mask = ("Tool", "G5x", "Rot")
        systemlist = ["Tool", "G5x", "Rot", "G92", "G54", "G55", "G56", "G57", "G58", "G59", "G59.1",
                   "G59.2", "G59.3"]
        names = []
        for system in systemlist:
            system_name = "system_name_%s" % system
            name = self.prefs.getpref(system_name, system, str)
            names.append([system, name])
        self.widgets.offsetpage1.set_names(names)

    def _add_alarm_entry(self, message):
        try:
            textbuffer = self.widgets.alarm_history.get_buffer()
            textbuffer.insert_at_cursor(strftime("%a, %d %b %Y %H:%M:%S     -", localtime()) + message + "\n")
        except:
            self.show_try_errors()

    def show_try_errors(self):
        exc_type, exc_value, exc_traceback = sys.exc_info()
        formatted_lines = traceback.format_exc().splitlines()
        print(_("**** LUMINOS ERROR ****"))
        print(_("**** %s ****" % formatted_lines[0]))
        traceback.print_tb(exc_traceback, limit = 1, file = sys.stdout)
        print (formatted_lines[-1])

    def _sensitize_widgets(self, widgetlist, value):
        for name in widgetlist:
                self.widgets[name].set_sensitive(value)

    def _change_dro_color(self, property, color):
        for axis in self.axis_list:
            if axis == self.axisletter_four:
                axis = 4
            self.widgets["Combi_DRO_%s" % axis].set_property(property, color)

    def _OK_for_mdi(self):
#        print("OK_FOR MDI ?")
        if not self.stat.estop:
#            print("Not estoped")
            if self.stat.enabled:
#                print("Enabled")
#                print("Homed", self.stat.homed)
#                print("force Homed", self.no_force_homing)
                if self.all_homed or self.no_force_homing:
                    if self.stat.interp_state == linuxcnc.INTERP_IDLE:
                        return True
        return False

    def _OK_for_manual(self):
#        print("OK_FOR Manual ?")
        if not self.stat.estop:
#            print("Not estoped")
            if self.stat.enabled:
#                print("Enabled")
                if self.stat.interp_state == linuxcnc.INTERP_IDLE:
#                    print("Interpreter idle")
                    return True
        return False

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
            self.widgets.lbl_tool_name.set_text(toolinfo[16])

        # we do not allow touch off with no tool mounted, so we set the
        # coresponding widgets unsensitive and set the description acordingly
        if tool == 0:
            self.widgets.lbl_tool_no.set_text("0")
            self.widgets.lbl_tool_dia.set_text("0")
            self.widgets.lbl_tool_name.set_text(_("No tool description available"))

        if "G43" in self.active_gcodes and self.stat.task_mode != linuxcnc.MODE_AUTO:
            self.command.mode(linuxcnc.MODE_MDI)
            self.command.wait_complete()
            self.command.mdi("G43")
            self.command.wait_complete()

        if self.tool_change:
            self.tool_change = False
            self.command.mode(linuxcnc.MODE_MANUAL)
            self.command.wait_complete()
            self.widgets.tooledit1.set_selected_tool(tool)
            # self.widgets.ntb_preview.queue_draw()
            print "new drawn"

    def _update_slider(self, widgetlist):
        # update scales and sliders
        for widget in widgetlist:
            value = self.widgets[widget].get_value()
            max = self.widgets[widget].upper
            min = self.widgets[widget].lower
            self.widgets[widget].lower = min * self.faktor
            self.widgets[widget].upper = max * self.faktor
            self.widgets[widget].set_value(value * self.faktor)
        self.scale_jog_vel = self.scale_jog_vel * self.faktor
        self._on_jv_counts_changed(self.jog_speed_counts, "adj_jog_vel")
        self.scale_max_vel = self.scale_max_vel * self.faktor
        self._on_mv_counts_changed(self.max_vel_counts, "adj_max_vel")

    def _update_active_gcodes(self):
        # active G codes
        active_codes = []
        temp = []
        for code in sorted(self.stat.gcodes[1:]):
            if code == -1:
                continue
            if code % 10 == 0:
                temp.append("%d" % (code / 10))
            else:
                temp.append("%d.%d" % (code / 10, code % 10))
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
            temp.append("%d" % code)
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
            self.widgets.lbl_current_vel.set_text("%d" % (self.stat.current_vel * 60.0 * self.faktor))
            if "G95" in self.active_gcodes:
                feed_str = "%d" % self.stat.settings[1]
                real_feed_str = "F  %.2f" % real_feed
            else:
                feed_str = "%d" % self.stat.settings[1]
                real_feed_str = "F  %.d" % real_feed
        else:
            self.widgets.lbl_current_vel.set_text("%.3f" % (self.stat.current_vel * 60.0 * self.faktor))
            if "G95" in self.active_gcodes:
                feed_str = "%.4f" % self.stat.settings[1]
                real_feed_str = "F %.4f" % real_feed
            else:
                feed_str = "%.3f" % self.stat.settings[1]
                real_feed_str = "F %.3f" % real_feed

        # converting 0.0 to string brings nothing, so the string is empty
        # happens only on start up
        if not real_feed:
            real_feed_str = "F  0"

        self.widgets.lbl_active_feed.set_label(feed_str)
        self.widgets.lbl_feed_act.set_text(real_feed_str)

        # set the speed label in active code frame
        self.widgets.active_speed_label.set_label("%.0f" % self.stat.settings[2])

    def _offset_changed(self, pin, tooloffset):
        if self.widgets.Combi_DRO_x.machine_units == _MM:
            self.widgets.lbl_tool_offset_z.set_text("%.3f" % self.halcomp["tool.tooloffset-z"])
        else:
            self.widgets.lbl_tool_offset_z.set_text("%.4f" % self.halcomp["tool.tooloffset-z"])

    # Here we create a manual tool change dialog
    def on_tool_change(self, widget):
        change = self.halcomp['tool.toolchange-change']
        toolnumber = self.halcomp['tool.toolchange-number']
        if change:
            # if toolnumber = 0 we will get an error because we will not be able to get
            # any tooldescription, so we avoid that case
            if toolnumber == 0:
                message = _("Please remove the mounted tool and press OK when done")
            else:
                tooldescr = self.widgets.tooledit1.get_toolinfo(toolnumber)[16]
                message = _("Please change to tool\n\n# {0:d}     {1}\n\n then click OK.").format(toolnumber, tooldescr)
            result = dialogs.warning_dialog(self, message, title = _("Manual Toolchange"))
            if result :
                self.halcomp["tool.toolchange-changed"] = True
            else:
                self.command.abort()
                self.halcomp['tool.toolchange-number'] = self.stat.tool_in_spindle
                self.halcomp['tool.toolchange-change'] = False
                self.halcomp['tool.toolchange-changed'] = True
                message = _("Tool Change has been aborted!\n")
                message += _("The old tool will remain set!")
                message += self.stat.tool_in_spindle + self.halcomp['tool.toolchange-number']
                self._show_error((13, message))
        else:
            self.halcomp['tool.toolchange-changed'] = False


# helpers End
# =========================================================


# =========================================================
# hal status Start

    def on_hal_status_state_estop(self, widget = None):
        print("estop")
        self.widgets.eb_side_button.modify_bg(gtk.STATE_NORMAL, gtk.gdk.Color(red = 65535))

    def on_hal_status_state_estop_reset(self, widget = None):
        print("estop_reset")
        self.widgets.eb_side_button.modify_bg(gtk.STATE_NORMAL, gtk.gdk.Color(red = 65535, green = 65535, blue = 0))

    def on_hal_status_state_off(self, widget):
        print("state_off")
        self.halcomp["state.is-on"] = False
        self.halcomp["state.is-off"] = True
        widgetlist = ["btn_homing", "btn_touch", "btn_tool", "ntb_jog", "btn_index_tool",
                      "btn_change_tool", "btn_select_tool_by_no",
                     ]
        self._sensitize_widgets(widgetlist, False)
        if self.stat.task_state == linuxcnc.STATE_ESTOP:
            self.widgets.eb_side_button.modify_bg(gtk.STATE_NORMAL, gtk.gdk.Color(red = 65535))
        else:
            self.widgets.eb_side_button.modify_bg(gtk.STATE_NORMAL, gtk.gdk.Color(red = 65535, green = 65535, blue = 0))
        self.widgets.btn_exit.set_sensitive(True)

    def on_hal_status_state_on(self, widget):
        print("state_on")
        self.halcomp["state.is-on"] = True
        self.halcomp["state.is-off"] = False
        widgetlist = ["btn_homing", "btn_touch", "btn_tool", "ntb_jog"]
        self._sensitize_widgets(widgetlist, True)
        self.widgets.eb_side_button.modify_bg(gtk.STATE_NORMAL, self.bg_color)
        self.widgets.btn_exit.set_sensitive(False)

    def _mode_manual(self, pin):
        if not pin.get():
            return
        if self.widgets.tbtn_setup.get_active() == True:
            return
        print("Mode Manual requested")

        self.wait_mode = linuxcnc.MODE_MANUAL
        # we will have to check also if OK for manual, because a change is not allowed
        # while the interpreter is not idle!
        print("result OK for Manual = ", self._OK_for_manual())
        if not self._OK_for_manual():
#            self._show_error((13, _("It is not possible to change to Manual Mode at the moment")))
            state = False
        else:
            self.command.mode(linuxcnc.MODE_MANUAL)
            self.command.wait_complete()
            self.halcomp["mode.is-manual"] = True
            self.halcomp["mode.is-mdi"] = False
            self.halcomp["mode.is-auto"] = False
            state = True
            self.wait_mode = None

        widgetlist = ["btn_homing", "btn_touch", "btn_tool"]
        for widget in widgetlist:
            self.widgets[widget].set_sensitive(state)
        self.widgets.ntb_main.set_current_page(0)
        self.widgets.ntb_button.set_current_page(0)
        self.widgets.ntb_jog.set_current_page(0)

    def _mode_mdi(self, pin):
        if not pin.get():
            return
        print("Mode MDI requested")
        self.wait_mode = linuxcnc.MODE_MDI
        # if not OK for MDI, we are not ready for MDI commands
        # so we have to abort external commands and get back to manual mode
        # This will hapen mostly, if we are in settings mode, as we do disable the mode button
#        print("result OK for MDI = ", self._OK_for_mdi())
        if self._OK_for_mdi():
            # if the edit offest button is active, a mode change would happen, messing up the
            # luminos mode handling in relation to halui.mode
            if self.widgets.tbtn_edit_offsets.get_active():
                self.widgets.tbtn_edit_offsets.set_active(False)
        if not self._OK_for_mdi() or self.widgets.tbtn_setup.get_active():
#           self._show_error((13, _("It is not possible to change to MDI Mode at the moment")))
            state = False
        else:
            for button in self.macrobuttons:
                button.set_sensitive(True)
            self.command.mode(linuxcnc.MODE_MDI)
            self.command.wait_complete()
            self.halcomp["mode.is-manual"] = False
            self.halcomp["mode.is-mdi"] = True
            self.halcomp["mode.is-auto"] = False
            state = True
            self.wait_mode = None

        widgetlist = ["hal_mdihistory"]
        for button in self.macrobuttons:
            button.set_sensitive(state)
        for widget in widgetlist:
            self.widgets[widget].set_sensitive(state)
        self.widgets.ntb_main.set_current_page(0)
        self.widgets.ntb_button.set_current_page(1)
        self.widgets.ntb_jog.set_current_page(1)
        self.widgets.hal_mdihistory.entry.grab_focus()

    def _mode_auto(self, pin):
        if not pin.get():
            return
        print("Mode Auto requested")
        self.wait_mode = linuxcnc.MODE_AUTO
        # we will check if it is OK to change to Auto Mode, it is OK if MDI is allowed
#        print("result OK for AUTO = ", self._OK_for_mdi())
        if not self._OK_for_mdi() or self.widgets.tbtn_setup.get_active():
#            self._show_error((13, _("It is not possible to change to AUTO Mode at the moment")))
            state = False
        else:
            self.command.mode(linuxcnc.MODE_AUTO)
            self.command.wait_complete()
            self.halcomp["mode.is-manual"] = False
            self.halcomp["mode.is-mdi"] = False
            self.halcomp["mode.is-auto"] = True
            state = True
            self.wait_mode = None

        widgetlist = ["btn_run", "btn_stop", "tbtn_pause", "btn_step", "btn_from_line",
                      "tbtn_optional_blocks", "tbtn_optional_stops"]
        for widget in widgetlist:
            self.widgets[widget].set_sensitive(state)
        # if the file load menue is open, we will leave here
        if self.widgets.ntb_preview.get_current_page() == 3:
            return
        self.widgets.ntb_main.set_current_page(0)
        self.widgets.ntb_button.set_current_page(2)
        self.widgets.ntb_jog.set_current_page(2)

    # use the hal_status widget to control buttons and
    # actions allowed by the user and sensitive widgets
    def on_hal_status_all_homed(self, widget):
        self.all_homed = True
        print("all_homed")
        self.widgets.ntb_button.set_current_page(0)
        widgetlist = ["btn_index_tool", "btn_change_tool", "btn_select_tool_by_no",
                       "btn_touch"
                     ]
        self._sensitize_widgets(widgetlist, True)

    def on_hal_status_not_all_homed(self, *args):
        self.all_homed = False
        self._add_alarm_entry("not_all_homed")
        widgetlist = ["btn_index_tool", "btn_touch", "btn_change_tool", "btn_select_tool_by_no",
                       "btn_touch"
                     ]
        self._sensitize_widgets(widgetlist, False)

    def on_hal_status_homed(self, widget, data):
        print("Axis %s are homed") % "XYZABCUVW"[int(data[0])]

    def on_hal_status_file_loaded(self, widget, filename):
        print("file_loaded_%s" % filename)
        if len(filename) > 50:
            filename = filename[0:10] + "..." + filename[len(filename) - 39:len(filename)]
        self.widgets.lbl_program.set_text(filename)
        self.widgets.btn_use_current.set_sensitive(True)

    def on_hal_status_interp_idle(self, widget):
        print("idle")
        widgetlist = ["btn_step", "ntb_jog", "btn_from_line",
                      "btn_load", "btn_edit", "tbtn_optional_blocks"
                     ]
        if not self.widgets.rbt_hal_unlock.get_active():
            widgetlist.append("tbtn_setup")
        if self.all_homed or self.no_force_homing:
            widgetlist.append("btn_index_tool")
            widgetlist.append("btn_change_tool")
            widgetlist.append("btn_select_tool_by_no")
            widgetlist.append("btn_touch")
        self._sensitize_widgets(widgetlist, True)
        for btn in self.macrobuttons:
            btn.set_sensitive(True)
        self.widgets.btn_run.set_sensitive(True)

    def on_hal_status_interp_run(self, widget):
        print("run")
        widgetlist = ["tbtn_setup", "btn_step", "btn_index_tool",
                      "btn_from_line", "btn_change_tool", "btn_select_tool_by_no",
                      "btn_load", "btn_edit", "tbtn_optional_blocks",
                       "btn_touch"
                     ]
        # in MDI it should be possible to add more commands, even if the interpreter is running
        # so w add the correspondinng widget only if we are not in MDO mode
        if self.stat.task_mode <> linuxcnc.MODE_MDI:
            widgetlist.append("ntb_jog")

        self._sensitize_widgets(widgetlist, False)
        self.widgets.btn_run.set_sensitive(False)
        # the user want to run step by step
        if self.stepping == True:
            self.widgets.btn_step.set_sensitive(True)
            self.widgets.tbtn_pause.set_sensitive(False)

    def on_hal_status_tool_in_spindle_changed(self, object, new_tool_no):
        print("tool_in_spindle has changed to %s" % new_tool_no)
        self._update_toolinfo(new_tool_no)

    # The actions of the buttons
    def _on_h_button_changed(self, pin):
        self._add_alarm_entry("got h_button_signal %s" % pin.name)
        # we check if the button is pressed ore release,
        # otehrwise a signal will be emitted, wenn the button is released and
        # the signal drob down to zero
        if not pin.get():
            return
        # lets see on witch button_box we are
        page = self.widgets.ntb_button.get_current_page()
        # witch button has been pressed
        btn = str(pin.name)
        # from the list we declared under __init__ we get the button number
        nr = int(btn[-1])
        tab = self.h_tabs[page] # see in the __init__ section for the declaration of self.h_tabs
        button = None
        # we check if there is a button or the user pressed a hardware button under
        # a non existing software button
        for index in tab:
            if int(index[0]) == nr:
                # this is the name of the button
                button = index[1]
        if button:
            # only emit a signal if the button is sensitive, otherwise
            # running actions may be interupted
            if self.widgets[button].get_sensitive() == False:
                print("%s not_sensitive" % button)
                self._add_alarm_entry("%s not_sensitive" % button)
                return
            self.widgets[button].emit("clicked")
        else:
            # as we are generating the macro buttons dynamecely, we can"t use the same
            # method as above, here is how we do it
            if page == 1: # macro page
                # does the user press a valid hardware button?
                if nr < len(self.macrobuttons):
                    button = self.macrobuttons[nr] # This list is generated in add_macros_buttons(self)
                    # is the button sensitive?
                    if button.get_sensitive() == False:
                        print("%s not_sensitive" % button)
                        return
                    button.emit("pressed")
                else:
                    print("No function on this button")
                    self._add_alarm_entry("%s not_sensitive" % button)
            else:
                print("No function on this button")

    def _on_v_button_changed(self, pin):
        self._add_alarm_entry("got v_button_signal %s" % pin.name)
        if not pin.get():
            return
        btn = str(pin.name)
        nr = int(btn[-1])
        tab = self.v_tabs # see in the __init__ section for the declaration of self.v_tabs
        button = None
        for index in tab:
            if int(index[0]) == nr:
                # this is the name of the button
                button = index[1]
        if button:
            # only emit a signal if the button is sensitive, otherwise
            # running actions may be interupted
            if self.widgets[button].get_sensitive() == False:
                print("%s not_sensitive" % button)
                self._add_alarm_entry("%s not_sensitive" % button)
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
            print("No button found in v_tabs from %s" % pin.name)
            self._add_alarm_entry("No button found in v_tabs from %s" % pin.name)


# hal status End
# =========================================================


# =========================================================
# Hal Pin Handling Start

    # We need extra HAL pins here is where we do it.
    # we make pins for the hardware buttons witch can be placed around the
    # screen to activate the coresponding buttons on the GUI
    def _init_hal_pins(self):
        # generate the horizontal button pins
        for h_button in range(0, 10):
            self.signal = hal_glib.GPin(self.halcomp.newpin("lum-button.h-button-%s" % h_button,
                                                                    hal.HAL_BIT, hal.HAL_IN))
            self.signal.connect("value_changed", self._on_h_button_changed)

        # generate the vertical button pins
        for v_button in range(0, 7):
            self.signal = hal_glib.GPin(self.halcomp.newpin("lum-button.v-button-%s" % v_button,
                                                                    hal.HAL_BIT, hal.HAL_IN))
            self.signal.connect("value_changed", self._on_v_button_changed)

        # lets make our pins to show machine status
        self.halcomp.newpin("state.is-estoped", hal.HAL_BIT, hal.HAL_OUT)
        self.halcomp.newpin("state.is-on", hal.HAL_BIT, hal.HAL_OUT)
        self.halcomp.newpin("state.is-off", hal.HAL_BIT, hal.HAL_OUT)

        # make some pins to indicate the mode to the hardware
        self.halcomp.newpin("mode.is-manual", hal.HAL_BIT, hal.HAL_OUT)
        self.halcomp.newpin("mode.is-mdi", hal.HAL_BIT, hal.HAL_OUT)
        self.halcomp.newpin("mode.is-auto", hal.HAL_BIT, hal.HAL_OUT)
        self.halcomp["mode.is-manual"] = True

        # make pin to change the modes from external button
        self.pin_manual = hal_glib.GPin(self.halcomp.newpin("mode.manual", hal.HAL_BIT, hal.HAL_IN))
        self.pin_manual.connect("value_changed", self._mode_manual)
        self.pin_mdi = hal_glib.GPin(self.halcomp.newpin("mode.mdi", hal.HAL_BIT, hal.HAL_IN))
        self.pin_mdi.connect("value_changed", self._mode_mdi)
        self.pin_auto = hal_glib.GPin(self.halcomp.newpin("mode.auto", hal.HAL_BIT, hal.HAL_IN))
        self.pin_auto.connect("value_changed", self._mode_auto)

        # make an error pin to indiocate a error to hardware
        self.halcomp.newpin("error", hal.HAL_BIT, hal.HAL_OUT)

        # make a pin to delete a notification message
        self.pin_del_message = hal_glib.GPin(self.halcomp.newpin("delete-message", hal.HAL_BIT, hal.HAL_IN))
        self.pin_del_message.connect("value_changed", self._del_message_changed)

        # generate the pins to set the increments
        number = 0
        for jog_incr in self.jog_increments:
            halpin = hal_glib.GPin(self.halcomp.newpin("jog.jog-inc-%s" % number,
                                                                    hal.HAL_BIT, hal.HAL_IN))
            halpin.connect("value_changed", self._on_pin_incr_changed, number)
            number += 1

        # jog_increment out pin
        self.jog_increment = hal_glib.GPin(self.halcomp.newpin("jog.jog-increment", hal.HAL_FLOAT, hal.HAL_OUT))

        # hal pin for jogging the axis
        for axis in self.axis_list:
            # get the axisnumber
            axisnumber = "xyzabcuvws".index(axis.lower())
            halpin = hal_glib.GPin(self.halcomp.newpin("jog.jog-%d-plus" % axisnumber,
                                                                    hal.HAL_BIT, hal.HAL_IN))
            halpin.connect("value_changed", self._on_pin_jog_changed, axisnumber, 1)
            halpin = hal_glib.GPin(self.halcomp.newpin("jog.jog-%d-minus" % axisnumber,
                                                                    hal.HAL_BIT, hal.HAL_IN))
            halpin.connect("value_changed", self._on_pin_jog_changed, axisnumber, -1)

        # pins for speed control, connect encoders to them
        self.jog_speed_counts = hal_glib.GPin(self.halcomp.newpin("jog.jog-speed-counts", hal.HAL_S32, hal.HAL_IN))
        self.jog_speed_counts.connect("value_changed", self._on_jv_counts_changed, "adj_jog_vel")
        self.max_vel_counts = hal_glib.GPin(self.halcomp.newpin("jog.max-vel-counts", hal.HAL_S32, hal.HAL_IN))
        self.max_vel_counts.connect("value_changed", self._on_mv_counts_changed, "adj_max_vel")
        self.halcomp.newpin("jog.jog-velocity", hal.HAL_FLOAT, hal.HAL_OUT)
        self.halcomp.newpin("jog.max-velocity", hal.HAL_FLOAT, hal.HAL_OUT)

        # pins for override control, connect encoders to them
        self.feed_override_counts = hal_glib.GPin(self.halcomp.newpin("feed-override-counts", hal.HAL_S32, hal.HAL_IN))
        self.feed_override_counts.connect("value_changed", self._on_fo_counts_changed, "adj_feed")
        self.spindle_override_counts = hal_glib.GPin(self.halcomp.newpin("spindle.override-counts", hal.HAL_S32, hal.HAL_IN))
        self.spindle_override_counts.connect("value_changed", self._on_so_counts_changed, "adj_spindle")
        self.halcomp.newpin("feed-override", hal.HAL_FLOAT, hal.HAL_OUT)
        self.halcomp.newpin("spindle.override", hal.HAL_FLOAT, hal.HAL_OUT)

        # make the pins for tool measurement
        self.probeheight = hal_glib.GPin(self.halcomp.newpin("tool.probeheight", hal.HAL_FLOAT, hal.HAL_OUT))
        self.blockheight = hal_glib.GPin(self.halcomp.newpin("tool.blockheight", hal.HAL_FLOAT, hal.HAL_OUT))
        self.enable_toolmeasurement = hal_glib.GPin(self.halcomp.newpin("tool.toolmeasurement", hal.HAL_BIT, hal.HAL_OUT))
        self.probe_search_vel = hal_glib.GPin(self.halcomp.newpin("tool.searchvel", hal.HAL_FLOAT, hal.HAL_OUT))
        self.probe_vel = hal_glib.GPin(self.halcomp.newpin("tool.probevel", hal.HAL_FLOAT, hal.HAL_OUT))

        # make pins to react to tool_offset changes
        self.pin_offset_z = hal_glib.GPin(self.halcomp.newpin("tool.tooloffset-z", hal.HAL_FLOAT, hal.HAL_IN))
        self.pin_offset_z.connect("value_changed", self._offset_changed, "tool.tooloffset-z")

        # for manual tool change dialog
        self.halcomp.newpin("tool.toolchange-number", hal.HAL_S32, hal.HAL_IN)
        self.halcomp.newpin("tool.toolchange-changed", hal.HAL_BIT, hal.HAL_OUT)
        self.pin_change_tool = hal_glib.GPin(self.halcomp.newpin('tool.toolchange-change', hal.HAL_BIT, hal.HAL_IN))
        self.pin_change_tool.connect('value_changed', self.on_tool_change)

        # for spindle control
        self.spindle_forward = hal_glib.GPin(self.halcomp.newpin("spindle.forward", hal.HAL_BIT, hal.HAL_IN))
        self.spindle_forward.connect("value_changed", self._on_spindle_forward_changed)
        self.spindle_reverse = hal_glib.GPin(self.halcomp.newpin("spindle.reverse", hal.HAL_BIT, hal.HAL_IN))
        self.spindle_reverse.connect("value_changed", self._on_spindle_reverse_changed)
        self.spindle_stop = hal_glib.GPin(self.halcomp.newpin("spindle.stop", hal.HAL_BIT, hal.HAL_IN))
        self.spindle_stop.connect("value_changed", self._on_spindle_stop_changed)


# Hal Pin Handling End
# =========================================================

    # every 100 milli seconds this gets called
    # check linuxcnc for status, error and then update the readout
    def _periodic(self):
        self.stat.poll()
        error = self.error_channel.poll()
        if error:
            self._show_error(error)
        if self.wait_mode:
            if self.stat.task_mode != self.wait_mode:
                if self.wait_mode == linuxcnc.MODE_MANUAL:
                    self._mode_manual(self.pin_manual)
                elif self.wait_mode == linuxcnc.MODE_MDI:
                    self._mode_mdi(self.pin_mdi)
                elif self.wait_mode == linuxcnc.MODE_AUTO:
                    self._mode_auto(self.pin_auto)
                else:
                    self._show_error((11, _("Waiting for unknown mode %s" % self.stat.task_mode)))

        if self.gcodes != self.stat.gcodes:
            self._update_active_gcodes()
        if self.mcodes != self.stat.mcodes:
            self._update_active_mcodes()

        self._update_vel()

        # keep the timer running
        return True

    def _show_error(self, error):
        kind, text = error
        # print kind,text
        if "joint" in text:
            for letter in self.axis_list:
                axnum = "xyzabcuvws".index(letter)
                text = text.replace("joint %d" % axnum, "Axis %s" % letter.upper())
        if kind in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
            icon = ALERT_ICON
            type = _("Error Message")
            self.halcomp["error"] = True
        elif kind in (linuxcnc.NML_TEXT, linuxcnc.OPERATOR_TEXT):
            icon = INFO_ICON
            type = _("Message")
        elif kind in (linuxcnc.NML_DISPLAY, linuxcnc.OPERATOR_DISPLAY):
            icon = INFO_ICON
            type = _("Message")
        self.notification.add_message(text, icon)

        if self._AUDIO_AVAILABLE:
            if kind == 1 or kind == 11:
                self.audio.set_sound(self.error_sound)
            else:
                self.audio.set_sound(self.alert_sound)
            self.audio.run()

    def on_gremlin_gcode_error(self, widget, errortext):
        if self.gcodeerror == errortext:
            return
        else:
            self.gcodeerror = errortext
            if self.log: self._add_alarm_entry(errortext)
            self._show_error((linuxcnc.NML_ERROR, errortext))

    # only for debugging, will be deleted later
    def on_btn_debug_clicked(self, widget, data = None):
        # diferent reasons, we will change them often

        print("coordinate system index = ", self.stat.g5x_index)

if __name__ == "__main__":
    app = luminos()

    inifile = sys.argv[2]
    print ("**** LUMINOS INFO : inifile = %s ****:" % sys.argv[2])
    postgui_halfile = app.get_ini_info.get_postgui_halfile()
    print ("**** LUMINOS INFO : postgui halfile = %s ****:" % postgui_halfile)

    if postgui_halfile:
        res = os.spawnvp(os.P_WAIT, "halcmd", ["halcmd", "-i", inifile, "-f", postgui_halfile])
        if res:
            raise SystemExit, res
    gtk.main()



