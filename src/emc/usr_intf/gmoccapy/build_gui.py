#!/usr/bin/env python
# -*- coding:UTF-8 -*-
"""
This file is part of gmoccapy and contains all information to rearange 
the widgets of GUI to fit the users needs
Most information we need, can be taken from the INI file

    Copyright 2017 Norbert Schechner
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
import traceback                   # needed to launch trace python errors

import sys                         # handle system calls
import os                          # needed to get the paths and directories
import gtk                         # base for pygtk widgets and constants
import gobject                     # needed to send signals
import locale                      # for setting the language of the GUI
import gettext                     # to extract the strings to be translated
import subprocess                  # to launch onboard and other processes
import tempfile                    # needed only if the user click new in edit 
                                   # mode to open a new empty file

import gladevcp.makepins           # needed for the dialog"s calculator widget
from gladevcp import combi_dro     # we will need it to make the DRO

from gmoccapy import widgets       # a class to handle the widgets
from gmoccapy import getiniinfo    # this handles the INI File reading so 
                                   # checking is done in that module
from gmoccapy import preferences   # this handles the preferences


from gladevcp.combi_dro import Combi_DRO


BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
LOCALEDIR = os.path.join(BASE, "share", "locale")
DATADIR = os.path.join(BASE, "share", "gmoccapy")
IMAGEDIR = os.path.join(DATADIR, "images")

# set up paths to files, part two
#CONFIGPATH = os.environ['CONFIG_DIR']
XMLNAME = os.path.join(DATADIR, "gmoccapy.glade")
#THEMEDIR = "/usr/share/themes"
#USERTHEMEDIR = os.path.join(os.path.expanduser("~"), ".themes")

# localization
#LOCALEDIR = os.path.join(BASE, "share", "locale")
#import locale
#locale.setlocale(locale.LC_ALL, '')

# path to TCL for external programs eg. halshow
TCLPATH = os.environ['LINUXCNC_TCL_DIR']

# the ICONS should must be in share/gmoccapy/images
ALERT_ICON = os.path.join(IMAGEDIR, "applet-critical.png")
INFO_ICON = os.path.join(IMAGEDIR, "std_info.gif")

# this is for hiding the pointer when using a touch screen
pixmap = gtk.gdk.Pixmap(None, 1, 1, 1)
color = gtk.gdk.Color()
INVISABLE = gtk.gdk.Cursor(pixmap, pixmap, color, color, 0, 0)

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


# the class must inherit from gobject to be able to send signals
class Build_GUI(gobject.GObject):
    
    '''
    This file is part of gmoccapy and will handle all the GUI related code.
    It will build part of the GUI dynamically, according to the user 
    configuration. All relevant information will be read from the INI File.
    '''

    __gtype_name__ = 'Build_GUI'
    __gproperties__ = {    }
    __gproperties = __gproperties__

    __gsignals__ = {
#                    'clicked': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_STRING, gobject.TYPE_PYOBJECT)),
                    'estop_active'    : (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_BOOLEAN,)),
                    'on_active'       : (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_BOOLEAN,)),
                    'set_manual'      : (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_BOOLEAN,)),
                    'home_clicked'    : (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_PYOBJECT,)),
                    'unhome_clicked'  : (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_PYOBJECT,)),
                    'jog_incr_changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_PYOBJECT,)),

                    'exit': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
                   }



    def __init__(self, halcomp, _RELEASE):
        super(Build_GUI, self).__init__()
        
        # prepare for translation / internationalization
        locale.setlocale(locale.LC_ALL, '')
        locale.bindtextdomain("gmoccapy", LOCALEDIR)
        gettext.install("gmoccapy", localedir=LOCALEDIR, unicode=True)
        gettext.bindtextdomain("gmoccapy", LOCALEDIR)

        self.halcomp = halcomp


        self.builder = gtk.Builder()
        # translation of the glade file will be done with
        self.builder.set_translation_domain("gmoccapy")
        self.builder.add_from_file(XMLNAME)
        self.builder.connect_signals(self)

        # get all widgets as class, so they can be called directly
        self.widgets = widgets.Widgets(self.builder)

        self.get_ini_info = getiniinfo.GetIniInfo()
#        self.widgets = widgets
        self._RELEASE = _RELEASE
        # This class will handle all the user preferences
        self.prefs = preferences.preferences(self.get_ini_info.get_preference_file_path())
        self._get_ini_data()
        self._get_pref_data()

        # make al widgets we create dynamically
        self._make_DRO()
        self._make_home_button()
        self._make_jog_increments()

        # set initial values for the GUI
        self._set_initial_values()

        # check for virtual keyboard
        self._init_keyboard()

        # set initial values for all widgets
        self._init_widgets()
        self._activate_widgets()
        
        self._get_tab_ref()
        
        panel = gladevcp.makepins.GladePanel(self.halcomp, XMLNAME, self.builder, None)


###############################################################################
##                     create widgets dynamically                            ##
###############################################################################    

    def _make_DRO(self):
        print("**** GMOCCAPY build_GUI INFO ****")
        print("**** Entering make_DRO")
        
        # if we have a lathe, we will need an additional DRO to display
        # diameter and radius, we build that separately
        if self.lathe_mode:
            self._this_is_a_lathe()
        # on all other cases we build one DRO for each axis
        self.dro_dic = {} 
        for pos, axis in enumerate(self.axis_list):
            joint = self._get_joint_from_joint_axis_dic(axis)
            dro = Combi_DRO()
            dro.set_joint_no(joint)
            dro.set_axis(axis)
            dro.change_axisletter(axis.upper())
            dro.show()
            dro.set_property("name", "Combi_DRO_{0}".format(axis.upper()))
            dro.connect("clicked", self._on_DRO_clicked)
            self.dro_dic[dro.name] = dro
        self._arange_dro()

    def _make_home_button(self):
        print("**** GMOCCAPY build_GUI INFO ****")
        print("**** Entering make home button")

        # lets find out, how many axis we got
        num_axis = len(self.axis_list)
        
        # as long as the number of axis is less 6 we can use the standard layout
        # we can display 6 axis without the second space label
        # and 7 axis if we do not display the first space label either
        # if we have more than 7 axis, we need arrows to switch the visible ones
        if num_axis < 7:
            lbl = self._get_space_label("lbl_space_0")
            self.widgets.hbtb_ref_axes.pack_end(lbl)
    
        file = "ref_all.png"
        filepath = os.path.join(IMAGEDIR, file)
        btn = self._get_button_with_image("home_axis_0", filepath, None)
        btn.set_property("tooltip-text", _("Press to home all axis"))
        btn.connect("clicked", self._on_btn_home_clicked)
        # we use pack_end, so the widgets will be moved from right to left
        # and are displayed the way we want
        self.widgets.hbtb_ref_axes.pack_end(btn)

        if num_axis > 7:
            # show the previous arrow to switch visible homing button)
            btn = self._get_button_with_image("previous_button", None, gtk.STOCK_GO_BACK)
            btn.set_sensitive(False)
            btn.set_property("tooltip-text", _("Press to display previous homing button"))
            btn.connect("clicked", self._on_btn_previous_clicked)
            self.widgets.hbtb_ref_axes.pack_end(btn)

        # do not use this label, to allow one more axis
        if num_axis < 6:
            lbl = self._get_space_label("lbl_space_2")
            self.widgets.hbtb_ref_axes.pack_end(lbl)

        for pos, axis in enumerate(self.axis_list):

            file = "ref_{0}.png".format(axis)
            filepath = os.path.join(IMAGEDIR, file)

            name = "home_axis_{0}".format(axis)
            btn = self._get_button_with_image(name, filepath, None)
            btn.set_property("tooltip-text", _("Press to home axis {0}".format(axis.upper())))
            btn.connect("clicked", self._on_btn_home_clicked)

            self.widgets.hbtb_ref_axes.pack_end(btn)

            # if we have more than 7 axis we need to hide some button
            if num_axis > 7:
                if pos > 4:
                    btn.hide()

        if num_axis > 7:
            # show the next arrow to switch visible homing button)
            btn = self._get_button_with_image("next_button", None, gtk.STOCK_GO_FORWARD)
            btn.set_property("tooltip-text", _("Press to display next homing button"))
            btn.connect("clicked", self._on_btn_next_clicked)
            self.widgets.hbtb_ref_axes.pack_end(btn)

        # if there is space left, fill it with space labels
        count = pos = self.widgets.hbtb_ref_axes.child_get_property(btn,"position")
        for lbl in range( pos + 1 , 8):
            count += 1
            lbl = self._get_space_label("lbl_space_{0}".format(count))
            self.widgets.hbtb_ref_axes.pack_end(lbl)
 
        file = "unhome.png"
        filepath = os.path.join(IMAGEDIR, file)

        name = "unhome_axis_0"
        btn = self._get_button_with_image(name, filepath, None)
        btn.set_property("tooltip-text", _("Press to unhome all axis"))
        btn.connect("clicked", self._on_btn_unhome_clicked)
        self.widgets.hbtb_ref_axes.pack_end(btn)
        
        name = "home_axis_back"
        btn = self._get_button_with_image(name, None, gtk.STOCK_UNDO)
        btn.set_property("tooltip-text", _("Press to returnn to main button list"))
        btn.connect("clicked", self._on_btn_home_back_clicked)
        self.widgets.hbtb_ref_axes.pack_end(btn)
        
        self.home_button_dic = {}
        children = self.widgets.hbtb_ref_axes.get_children()
        for child in children:
            self.home_button_dic[child.name] = child

        self._get_tab_ref()

    def _make_jog_increments(self):
        # Now we will build the option buttons to select the Jog-rates
        # We do this dynamically, because users are able to set them in INI File
        # because of space on the screen only 10 items are allowed
        # jogging increments

        self.incr_dic = {}

        # We get the increments from INI File
        if len(self.jog_increments) > 10:
            print(_("**** GMOCCAPY build_GUI INFO ****"))
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
        rbt0.set_property("name","rbt_0")
        rbt0.connect("pressed", self._jog_increment_changed, 0)
        self.widgets.vbtb_jog_incr.pack_start(rbt0, True, True, 0)
        rbt0.set_property("draw_indicator", False)
        rbt0.show()
        rbt0.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        self.incr_dic[rbt0.name] = rbt0
        # the rest of the buttons are now added to the group
        # self.no_increments is set while setting the hal pins with self._check_len_increments
        for item in range(1, len(self.jog_increments)):
            name = "rbt_{0}".format(item)
            rbt = gtk.RadioButton(rbt0, self.jog_increments[item])
            rbt.set_property("name",name)
            rbt.connect("pressed", self._jog_increment_changed, self.jog_increments[item])
            self.widgets.vbtb_jog_incr.pack_start(rbt, True, True, 0)
            rbt.set_property("draw_indicator", False)
            rbt.show()
            rbt.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
            self.incr_dic[rbt.name] = rbt
        self.active_increment = rbt0
        return self.active_increment, self.incr_dic

# =============================================================
# Onboard keybord handling Start

    # shows "onboard" or "matchbox" virtual keyboard if available
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
                print (_("**** GMOCCAPY build_GUI INFO ****"))
                print (_("**** virtual keyboard program found : <onboard>"))
            elif os.path.isfile("/usr/bin/matchbox-keyboard"):
                self.onboard_kb = subprocess.Popen(["matchbox-keyboard", "--xid"],
                                                   stdin=subprocess.PIPE,
                                                   stdout=subprocess.PIPE,
                                                   close_fds=True)
                print (_("**** GMOCCAPY build_GUI INFO ****"))
                print (_("**** virtual keyboard program found : <matchbox-keyboard>"))
            else:
                print (_("**** GMOCCAPY build_GUI INFO ****"))
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
            print (_("**** GMOCCAPY build_GUI ERROR ****"))
            print (_("**** Error with launching virtual keyboard,"))
            print (_("**** is onboard or matchbox-keyboard installed? ****"))
            traceback.print_exc()
            self._no_virt_keyboard()

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


    def _jog_increment_changed(self, widget, increment):
        print("widget = ", widget.name)
        print("increment = ", increment)
        self.emit("jof_incr_changed", widget, increment)


    def _this_is_a_lathe(self):
        print("**** GMOCCAPY build_GUI INFO ****")
        print("**** we have a lathe here")
        
        print(self.joint_axis_dic)

        for dro in range(len(self.axis_list) + 1, 9):
            self.widgets["Combi_DRO_{0}".format(dro)].destroy()
            print(" Combi_DRO_{0} has been destroyed".format(dro))

        for dro in range(len(self.axis_list) + 1):
            self.widgets.tbl_DRO.remove(self.widgets["Combi_DRO_{0}".format(dro)])

        self.widgets.tbl_DRO.resize(len(self.axis_list) + 1, 1)

        # We will use Joint 1 as D DRO and Joint 1 as R DRO, 
        # that's why the are arrange them in different order 
        self.widgets.tbl_DRO.attach(self.widgets.Combi_DRO_0, 0, 1, 0, 1)
        self.widgets.tbl_DRO.attach(self.widgets.Combi_DRO_1, 0, 1, 1, 2)
        self.widgets.tbl_DRO.attach(self.widgets.Combi_DRO_2, 0, 1, 2, 3)

        # The DRO 1 we make to a second X DRO to indicate the diameter
        # and the DRO has to be shown
        joint = self._get_joint_from_joint_axis_dic("x")
        print("Axis X = Joint ", joint)
        self.widgets.Combi_DRO_0.set_joint_no(joint)
        self.widgets.Combi_DRO_1.set_joint_no(joint)
        self.widgets.Combi_DRO_1.set_axis("x")
        self.widgets.Combi_DRO_1.set_to_diameter(True)
        self.widgets.Combi_DRO_1.show()

        # we change the axis letters of the DRO's
        self.widgets.Combi_DRO_0.change_axisletter("R")
        self.widgets.Combi_DRO_1.change_axisletter("D")

        # the DRO 2 will be the Z DRO
        joint = self._get_joint_from_joint_axis_dic("z")
        print("Axis Z = Joint ", joint)
        self.widgets.Combi_DRO_2.set_joint_no(joint)
        self.widgets.Combi_DRO_2.set_axis("z")
        self.widgets.Combi_DRO_2.change_axisletter("Z")
        self.widgets.Combi_DRO_2.show()


        # we first hide the Y button to home and touch off
        self.widgets.btn_set_value_y.hide()
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

        # is this a backtool lathe?
        self.backtool_lathe = self.get_ini_info.get_backtool_lathe()

        # now we place them in a different order
        if self.backtool_lathe:
            self.widgets.tbl_jog_btn_axes.attach(self.widgets.btn_x_plus, 1, 2, 0, 1, gtk.SHRINK, gtk.SHRINK)
            self.widgets.tbl_jog_btn_axes.attach(self.widgets.btn_x_minus, 1, 2, 2, 3, gtk.SHRINK, gtk.SHRINK)
        else:
            self.widgets.tbl_jog_btn_axes.attach(self.widgets.btn_x_plus, 1, 2, 2, 3, gtk.SHRINK, gtk.SHRINK)
            self.widgets.tbl_jog_btn_axes.attach(self.widgets.btn_x_minus, 1, 2, 0, 1, gtk.SHRINK, gtk.SHRINK)
        self.widgets.tbl_jog_btn_axes.attach(self.widgets.btn_z_plus, 2, 3, 1, 2, gtk.SHRINK, gtk.SHRINK)
        self.widgets.tbl_jog_btn_axes.attach(self.widgets.btn_z_minus, 0, 1, 1, 2, gtk.SHRINK, gtk.SHRINK)

        # and we will have to change the colors of the Y DRO according to the settings
#                self.widgets.Combi_DRO_1.set_property("abs_color", gtk.gdk.color_parse(self.abs_color))
#                self.widgets.Combi_DRO_1.set_property("rel_color", gtk.gdk.color_parse(self.rel_color))
#                self.widgets.Combi_DRO_1.set_property("dtg_color", gtk.gdk.color_parse(self.dtg_color))
#                self.widgets.Combi_DRO_1.set_property("homed_color", gtk.gdk.color_parse(self.homed_color))
#                self.widgets.Combi_DRO_1.set_property("unhomed_color", gtk.gdk.color_parse(self.unhomed_color))
#                self.widgets.Combi_DRO_1.set_property("actual", self.dro_actual)

        # For gremlin we don"t need the following button
        if self.backtool_lathe:
            self.widgets.rbt_view_y2.set_active(True)
        else:
            self.widgets.rbt_view_y.set_active(True)
        self.widgets.rbt_view_p.hide()
        self.widgets.rbt_view_x.hide()
        self.widgets.rbt_view_z.hide()

#                # check if G7 or G8 is active
#                if "70" in self.stat.gcodes:
#                    self._switch_to_g7(True)
#                else:
#                    self._switch_to_g7(False)


###############################################################################
##                        hide or display widgets                            ##
###############################################################################

    # the user want a Logo 
    def logo(self,logofile):
        self.widgets.img_logo.set_from_file(logofile)
        self.widgets.img_logo.show()

        page2 = self.widgets.ntb_jog_JA.get_nth_page(2)
        self.widgets.ntb_jog_JA.reorder_child(page2, 0)
        page1 = self.widgets.ntb_jog_JA.get_nth_page(1)
        self.widgets.ntb_jog_JA.reorder_child(page1, -1)

    # if we have an angular axis we will show the angular jog speed slider
    def _angular_axis_availible(self):
        self.widgets.spc_ang_jog_vel.hide()
        for axis in ("a","b","c"):
            if axis in self.joint_axis_dic.values():
                self.widgets.spc_ang_jog_vel.show()
                print("**** GMOCCAPY build_GUI INFO ****")
                print("**** axis {0} is a rotary axis\n".format(self.joint_axis_dic[dro]))
                print("**** Will display the angular jog slider\n")
                break

    def _remove_homing_button(self):
        for child in self.home_button_dic:
            self.widgets.hbtb_ref_axes.remove(self.home_button_dic[child])

    def _put_home_all_and_previous(self):
        self.widgets.hbtb_ref_axes.pack_start(self.home_button_dic["home_axis_0"])
        self.widgets.hbtb_ref_axes.pack_start(self.home_button_dic["previous_button"])

    def _put_axis_button(self, start, end):
        for axis in self.axis_list[start : end]:
            name = "home_axis_{0}".format(axis.lower())
            self.home_button_dic[name].show()
            self.widgets.hbtb_ref_axes.pack_start(self.home_button_dic[name])

    def _put_unhome_and_back(self):
        self.widgets.hbtb_ref_axes.pack_start(self.home_button_dic["next_button"])
        self.widgets.hbtb_ref_axes.pack_start(self.home_button_dic["unhome_axis_0"])
        self.widgets.hbtb_ref_axes.pack_end(self.home_button_dic["home_axis_back"])

    def _hide_homing_button(self, start, end):
        for axis in self.axis_list[start:end]:
            name = "home_axis_{0}".format(axis.lower())
            self.home_button_dic[name].hide()
            self.widgets.hbtb_ref_axes.pack_start(self.home_button_dic[name])

###############################################################################
##                       internal button handling                            ##
###############################################################################

    def on_tbtn_estop_toggled(self, widget, data = None):
        print("build GUI  estop toggled", widget.get_active())
        state = widget.get_active()
        if state:
            print("estop OK")
            self.widgets.tbtn_estop.set_image(self.widgets.img_emergency_off)
            self.widgets.tbtn_on.set_image(self.widgets.img_machine_off)
            self.widgets.tbtn_on.set_sensitive(True)
            self.widgets.ntb_jog.set_sensitive(True)
            self.widgets.ntb_jog_JA.set_sensitive(False)
            self.widgets.vbtb_jog_incr.set_sensitive(False)
            self.widgets.hbox_jog_vel.set_sensitive(False)
            self.widgets.chk_ignore_limits.set_sensitive(True)
        else:
            print("estop NOK")
            self.widgets.tbtn_estop.set_image(self.widgets.img_emergency)
            self.widgets.tbtn_on.set_image(self.widgets.img_machine_on)
            self.widgets.tbtn_on.set_sensitive(False)
            self.widgets.chk_ignore_limits.set_sensitive(False)
            self.widgets.tbtn_on.set_active(False)

        self.emit("estop_active", not state)


    # toggle machine on / off button
    def on_tbtn_on_toggled(self, widget, data=None):
        state = widget.get_active()
        if widget.get_active():  # machine is on
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
            self.widgets.ntb_main.set_current_page(_BB_MANUAL)
        else:  # machine is off
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
            
        self.emit("on_active", state)

    def _on_DRO_clicked(self, widget, joint, order):
        print("clicked on DRO ", widget.name, joint, order)
        for dro in self.dro_dic:
            self.dro_dic[dro].set_order(order)
        return

# ToDo
#        if self.lathe_mode:
#            self.widgets.Combi_DRO_1.set_order(order)

# from here only needed, if the DRO button will remain in gmoccapy
        self._offset_changed(None, None)
        if order[0] == "Abs" and self.widgets.tbtn_rel.get_label() != "Abs":
            self.widgets.tbtn_rel.set_active(False)
        if order[0] == "Rel" and self.widgets.tbtn_rel.get_label() != self.widgets.Combi_DRO_0.system:
            self.widgets.tbtn_rel.set_active(True)
        if order[0] == "DTG":
            self.widgets.tbtn_dtg.set_active(True)
        else:
            self.widgets.tbtn_dtg.set_active(False)
# to here only needed, if the DRO button will remain in gmoccapy

    def _on_btn_previous_clicked(self, widget, data = None):
        self._remove_homing_button()
        self._put_home_all_and_previous()
        self._put_axis_button(0 , 5)
        self._put_unhome_and_back()
        self._hide_homing_button(5,len(self.axis_list))
        
        self.home_button_dic["previous_button"].set_sensitive(False)
        self.home_button_dic["next_button"].set_sensitive(True)

        self._get_tab_ref()

    def _on_btn_next_clicked(self, widget, data = None):
        self._remove_homing_button()
        self._put_home_all_and_previous()
        self._put_axis_button(len(self.axis_list) - 5 , len(self.axis_list))
        self._put_unhome_and_back()
        self._hide_homing_button(0,len(self.axis_list) - 5)
        
        self.home_button_dic["previous_button"].set_sensitive(True)
        self.home_button_dic["next_button"].set_sensitive(False)
        
        self._get_tab_ref()

    def _on_btn_home_clicked(self, widget):
        # home axis or joint?
        if "axis" in widget.name:
            value = widget.name[-1]
            # if widget.name is home_axis_0 the home all button has been clicked
            # we send joint -1 to home all axis at once
            if value == "0":
                joint = -1
                self.emit("home_clicked", joint)
                return
            # if the selected axis is a double axis we will only give the command
            # to home tha master axis, witch should end with 0 
            if value in self.double_axis_letter:
                value = value + "0"
            # now get the joint from directory by the value
            joint = self._get_joint_from_joint_axis_dic(value)

        self.emit("home_clicked", joint)

    def _on_btn_unhome_clicked(self, widget):
        self.emit("unhome_clicked", widget)

    def _on_btn_home_back_clicked(self, widget):
        self.emit("home_clicked", widget)

    # If button exit is clicked, press emergency button before closing the application
    def on_btn_exit_clicked(self, widget, data=None):
        self.widgets.window1.destroy()

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

    def on_tbtn_setup_toggled(self, widget, data=None):
        # first we set to manual mode, as we do not allow changing settings in other modes
        # otherwise external halui commands could start a program while we are in settings
        self.emit("set_manual", True)
        
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
#            if self.stat.task_state == linuxcnc.STATE_ESTOP:
#                # estopped no mode available
#                self.widgets.rbt_manual.set_sensitive(False)
#                self.widgets.rbt_mdi.set_sensitive(False)
#                self.widgets.rbt_auto.set_sensitive(False)
#            if (self.stat.task_state == linuxcnc.STATE_ON) and not self.all_homed:
#                # machine on, but not homed, only manual allowed
#                self.widgets.rbt_manual.set_sensitive(True)
#                self.widgets.rbt_mdi.set_sensitive(False)
#                self.widgets.rbt_auto.set_sensitive(False)
#            if (self.stat.task_state == linuxcnc.STATE_ON) and (self.all_homed or self.no_force_homing):
#                # all OK, make all modes available
#                self.widgets.rbt_manual.set_sensitive(True)
#                self.widgets.rbt_mdi.set_sensitive(True)
#                self.widgets.rbt_auto.set_sensitive(True)
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

    def on_btn_homing_clicked(self, widget, data=None):
        self.widgets.ntb_button.set_current_page(_BB_HOME)



# ToDo Start : shell the button remain in gmoccapy ??

    def on_chk_auto_units_toggled(self, widget, data=None):
        for axis in self.axis_list:
            dro_name = "Combi_DRO_{0}".format(axis.upper())
            dro = self.dro_dic[dro_name]
            dro.set_auto_units(self.widgets.chk_auto_units.get_active())
        if self.lathe_mode:
            self.dro_dic["Combi_DRO_Y"].set_auto_units(self.widgets.chk_auto_units.get_active())
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

# ToDo End : shell the button remain in gmoccapy ??


###############################################################################
##                          helper functions                                 ##
###############################################################################
        
    def _get_button_with_image(self, name, filepath, stock):
        image = gtk.Image()
        image.set_size_request(48,48)
        btn = self._get_button(name, image)
        if filepath:
            image.set_from_file(filepath)
        else:
            image.set_from_stock(stock, 48)
        return btn

    def _get_button(self, name, image):
        btn = gtk.Button()
        btn.set_size_request(85,56)
        btn.add(image)
        btn.set_property("name", name)
        btn.show_all()
        return btn

    def _get_space_label(self, name):
        lbl = gtk.Label("")
        lbl.set_property("name", name)
        lbl.set_size_request(85,56)
        lbl.show()
        return lbl

    def _get_tab_ref(self):
        # get the position of each button to be able to connect to hardware button
        unsorted_tab_ref = []
        for element in self.home_button_dic:
            if self.home_button_dic[element].get_visible():
                pos = self.widgets.hbtb_ref_axes.child_get_property(self.home_button_dic[element],"position")
                unsorted_tab_ref.append((pos, element))
        
        sorted_tab_ref = sorted(unsorted_tab_ref, key=lambda tub: tub[0])
        
        self.tab_ref=[]
        for pos, entry in enumerate(sorted_tab_ref):
            self.tab_ref.append((pos, entry[1]))
        
    def _get_joint_from_joint_axis_dic(self, value):
        return self.joint_axis_dic.keys()[self.joint_axis_dic.values().index(value)]

    def _arange_dro(self):
        # if we have less than 4 axis, we can resize the table, as we have 
        # enough space to display each one in it's own line  
        if len(self.axis_list) < 4:
            self._place_in_table(len(self.axis_list),1, self.dro_size)

        # having 4 DRO we need to reduce the size, to fit the available space
        elif len(self.axis_list) == 4:
            self._place_in_table(len(self.axis_list),1, self.dro_size * 0.75)

        # having 5 axis we will display 3 in an one line and two DRO share 
        # the last line, the size of the DRO must be reduced also
        # this is a special case so we do not use _place_in_table
        elif len(self.axis_list) == 5:
            self.widgets.tbl_DRO.resize(4,2)
            for dro, axis in enumerate(self.axis_list):
                dro_name = "Combi_DRO_{0}".format(axis.upper())
                if dro < 3:
                    size = self.dro_size * 0.75
                    self.widgets.tbl_DRO.attach(self.dro_dic[dro_name], 
                                                0, 2, int(dro), int(dro + 1), ypadding = 0)
                else:
                    size = self.dro_size * 0.65
                    if dro == 3:
                        self.widgets.tbl_DRO.attach(self.dro_dic[dro_name], 
                                                    0, 1, int(dro), int(dro + 1), ypadding = 0)
                    else:
                        self.widgets.tbl_DRO.attach(self.dro_dic[dro_name], 
                                                    1, 2, int(dro-1), int(dro), ypadding = 0)
                self.dro_dic[dro_name].set_property("font_size", size)

        else:
            print("**** GMOCCAPY build_GUI INFO ****")
            print("**** more than 5 axis ")
            # check if amount of axis is an even number, adapt the needed lines
            if len(self.axis_list) % 2 == 0:
                rows = len(self.axis_list) / 2
            else:
                rows = (len(self.axis_list) + 1) / 2
            self._place_in_table(rows, 2, self.dro_size * 0.65)

    def _place_in_table(self, rows, cols, dro_size):
        self.widgets.tbl_DRO.resize(rows, cols)
        col = 0
        row = 0
        for dro, axis in enumerate(self.axis_list):
            dro_name = "Combi_DRO_{0}".format(axis.upper())

            self.dro_dic[dro_name].set_property("font_size", dro_size)
            self.widgets.tbl_DRO.attach(self.dro_dic[dro_name], 
                                        col, col+1, row, row + 1, ypadding = 0)
            if cols > 1:
                # calculate if we have to place in the first or the second column
                if (dro % 2 == 1):
                    col = 0
                    row +=1
                else:
                    col += 1
            else:
                row += 1

    def _get_ini_data(self):
        # get the axis list from INI
        self.axis_list = self.get_ini_info.get_axis_list()
        # get the joint axis relation from INI
        self.joint_axis_dic, self.double_axis_letter = self.get_ini_info.get_joint_axis_relation()
        # if it's a lathe config, set the tooleditor style
        self.lathe_mode = self.get_ini_info.get_lathe()
        # check if the user want actual or commanded for the DRO
        self.dro_actual = self.get_ini_info.get_position_feedback_actual()
        # the given Jog Increments
        self.jog_increments = self.get_ini_info.get_increments()

    def _get_pref_data(self):
        # the size of the DRO
        self.dro_size = self.prefs.getpref("dro_size", 28, int)
        # the colors of the DRO
        self.abs_color = self.prefs.getpref("abs_color", "#0000FF", str)         # blue
        self.rel_color = self.prefs.getpref("rel_color", "#000000", str)         # black
        self.dtg_color = self.prefs.getpref("dtg_color", "#FFFF00", str)         # yellow
        self.homed_color = self.prefs.getpref("homed_color", "#00FF00", str)     # green
        self.unhomed_color = self.prefs.getpref("unhomed_color", "#FF0000", str) # red
        
        # the velocity settings
        self.min_spindle_rev = self.prefs.getpref("spindle_bar_min", 0.0, float)
        self.max_spindle_rev = self.prefs.getpref("spindle_bar_max", 6000.0, float)

    def _sensitize_widgets(self, widgetlist, value):
        for name in widgetlist:
            try:
                self.widgets[name].set_sensitive(value)
            except Exception, e:
                print (_("**** GMOCCAPY ERROR ****"))
                print _("**** No widget named: {0} to sensitize ****").format(name)
                traceback.print_exc()


###############################################################################
##                            signal handling                                ##
###############################################################################

    # kill keyboard and estop machine before closing
    def on_window1_destroy(self, widget, data=None):
        print "estoping / killing gmoccapy"
        if self.onboard:
            self._kill_keyboard()
        self.emit("exit")
        
###############################################################################
##                        hal status handling                                ##
###############################################################################

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
            self.emit("set_manual", True)

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
        #self._check_limits()
        
        # if the status changed, we reset the key event, otherwise the key press
        # event will not change, if the user did the last change with keyboard shortcut
        # This is caused, because we record the last key event to avoid multiple key
        # press events by holding down the key. I.e. One press should only advance one increment
        # on incremental jogging.
        self.last_key_event = None, 0


    def on_hal_status_tool_in_spindle_changed(self, object, new_tool_no):
        # need to save the tool in spindle as preference, to be able to reload it on startup
        self.prefs.putpref("tool_in_spindle", new_tool_no, int)
        self._update_toolinfo(new_tool_no)


###############################################################################
##                            modify widgets                                 ##
###############################################################################

    # the user want to use a security mode
    def user_code(self):
        self.widgets.tbtn_setup.set_sensitive(False)
        
    def update_widgets(self, state):
        widgetlist = ["rbt_manual", "btn_homing", "btn_touch", "btn_tool",
                      "hbox_jog_vel", "ntb_jog_JA", "vbtb_jog_incr", "spc_feed", "btn_feed_100", "rbt_forward", "btn_index_tool",
                      "rbt_reverse", "rbt_stop", "tbtn_flood", "tbtn_mist", "btn_change_tool", "btn_select_tool_by_no",
                      "btn_spindle_100", "spc_rapid", "spc_spindle",
                      "btn_tool_touchoff_x", "btn_tool_touchoff_z"
        ]
        self._sensitize_widgets(widgetlist, state)


###############################################################################
##                     set widgets to start value                            ##
###############################################################################    

    def _init_widgets(self):
        # set the title of the window, to show the release
        self.widgets.window1.set_title("gmoccapy for linuxcnc {0}".format(self._RELEASE))
        self.widgets.lbl_version.set_label("<b>gmoccapy\n{0}</b>".format(self._RELEASE))
        
        # this sets the background colors of several buttons
        # the colors are different for the states of the button
        self.widgets.tbtn_on.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        self.widgets.tbtn_estop.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#00FF00"))
        self.widgets.tbtn_estop.modify_bg(gtk.STATE_NORMAL, gtk.gdk.color_parse("#FF0000"))
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

        # set start colors of the color selection button
        self.widgets.abs_colorbutton.set_color(gtk.gdk.color_parse(self.abs_color))
        self.widgets.rel_colorbutton.set_color(gtk.gdk.color_parse(self.rel_color))
        self.widgets.dtg_colorbutton.set_color(gtk.gdk.color_parse(self.dtg_color))
        self.widgets.homed_colorbtn.set_color(gtk.gdk.color_parse(self.homed_color))
        self.widgets.unhomed_colorbtn.set_color(gtk.gdk.color_parse(self.unhomed_color))

        # set the colors for the DRO
        for axis in self.axis_list:
            dro_name = "Combi_DRO_{0}".format(axis.upper())
            dro = self.dro_dic[dro_name]
            dro.set_property("abs_color", gtk.gdk.color_parse(self.abs_color))
            dro.set_property("rel_color", gtk.gdk.color_parse(self.rel_color))
            dro.set_property("dtg_color", gtk.gdk.color_parse(self.dtg_color))
            dro.set_property("homed_color", gtk.gdk.color_parse(self.homed_color))
            dro.set_property("unhomed_color", gtk.gdk.color_parse(self.unhomed_color))
            dro.set_property("actual", self.dro_actual)

        # set values to dro size adjustments
        self.widgets.adj_dro_size.set_value(self.dro_size)

# ToDo Start : shell the button remain in gmoccapy ??

        # Only used if the DRO buttons will remain in gmoccapy
        self.widgets.chk_show_dro_btn.set_active(self.prefs.getpref("show_dro_btn", False, bool))
        self.widgets.chk_auto_units.set_active(self.prefs.getpref("use_auto_units", True, bool))

        try: 
            if self.dro_dic["Combi_DRO_X"].machine_units == 0:
                self.widgets.tbtn_units.set_active(True)
        except:
            print("We do not have a X axis, very strange")

        self.widgets.tbtn_rel.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        self.widgets.tbtn_dtg.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        self.widgets.tbtn_units.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))

# ToDo End : shell the button remain in gmoccapy ??


        # set the velocity settings to adjustments
        self.widgets.adj_spindle_bar_min.set_value(self.min_spindle_rev)
        self.widgets.adj_spindle_bar_max.set_value(self.max_spindle_rev)
        self.widgets.spindle_feedback_bar.set_property("min", float(self.min_spindle_rev))
        self.widgets.spindle_feedback_bar.set_property("max", float(self.max_spindle_rev))
        
        # should the tool in spindle be reloaded on startup?
        self.widgets.chk_reload_tool.set_active(self.prefs.getpref("reload_tool", True, bool))
        
        # set the buttons on the settings page for additional DRO button 
        self.widgets.chk_show_dro.set_active(self.prefs.getpref("enable_dro", False, bool))
        self.widgets.chk_show_offsets.set_active(self.prefs.getpref("show_offsets", False, bool))
        self.widgets.chk_show_dtg.set_active(self.prefs.getpref("show_dtg", False, bool))
        self.widgets.chk_show_offsets.set_sensitive(self.widgets.chk_show_dro.get_active())
        self.widgets.chk_show_dtg.set_sensitive(self.widgets.chk_show_dro.get_active())

        # gremlin related stuff
        self.widgets.tbtn_view_tool_path.set_active(self.prefs.getpref("view_tool_path", True, bool))
        self.widgets.tbtn_view_dimension.set_active(self.prefs.getpref("view_dimension", True, bool))
        view = self.prefs.getpref("gremlin_view", "rbt_view_p", str)
        self.widgets[view].set_active(True)
        self.widgets.cmb_mouse_button_mode.set_active(self.prefs.getpref("mouse_btn_mode", 4, int))
        
        # Popup Messages position and size
        self.widgets.adj_x_pos_popup.set_value(self.prefs.getpref("x_pos_popup", 45, float))
        self.widgets.adj_y_pos_popup.set_value(self.prefs.getpref("y_pos_popup", 55, float))
        self.widgets.adj_width_popup.set_value(self.prefs.getpref("width_popup", 250, float))
        self.widgets.adj_max_messages.set_value(self.prefs.getpref("max_messages", 10, float))
        self.widgets.fontbutton_popup.set_font_name(self.prefs.getpref("message_font", "sans 10", str))
        self.widgets.chk_use_frames.set_active(self.prefs.getpref("use_frames", True, bool))
        
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

        # call the function to change the button status
        # so every thing is ready to start
        widgetlist = ["rbt_manual", "rbt_mdi", "rbt_auto", "btn_homing", "btn_touch", "btn_tool",
                      "ntb_jog", "spc_feed", "btn_feed_100", "rbt_forward", "btn_index_tool",
                      "rbt_reverse", "rbt_stop", "tbtn_flood", "tbtn_mist", "btn_change_tool",
                      "btn_select_tool_by_no", "btn_spindle_100", "spc_rapid", "spc_spindle",
                      "btn_tool_touchoff_x", "btn_tool_touchoff_z"
        ]
        self._sensitize_widgets(widgetlist, False)

        self.widgets.tbtn_on.set_sensitive(False)

###############################################################################
##                   initial clicking and toggling                           ##
###############################################################################    

    def _activate_widgets(self):

# ToDo Start : shell the button remain in gmoccapy ??

        self.on_chk_show_dro_btn_toggled(None)
        self.on_chk_auto_units_toggled(None)
        
# ToDo End : shell the button remain in gmoccapy ??

###############################################################################
##                  set initial global values value                          ##
###############################################################################    

    # initial values for the GUI
    def _set_initial_values(self):
        self.initialized = False  # will be set True after the window has been shown and all
                                  # basic settings has been finished, so we avoid some actions
                                  # because we cause click or toggle events when initializing
                                  # widget states.
        self.all_homed = False    # will hold True if all axis are homed
# ToDo : start check if this is needed
        self.gcodeerror = ""
# ToDo : end check if this is needed

