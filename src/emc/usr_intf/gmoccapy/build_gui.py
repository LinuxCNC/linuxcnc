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

import sys                         # handle system calls
import os                          # needed to get the paths and directories
import gtk                         # base for pygtk widgets and constants
import gobject                     # needed to send signals
from gmoccapy import getiniinfo    # this handles the INI File reading so checking is done in that module
from gmoccapy import preferences   # this handles the preferences
from gladevcp import combi_dro


# localization
import locale
from gladevcp.combi_dro import Combi_DRO
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
LOCALEDIR = os.path.join(BASE, "share", "locale")
DATADIR = os.path.join(BASE, "share", "gmoccapy")
IMAGEDIR = os.path.join(DATADIR, "images")
locale.setlocale(locale.LC_ALL, '')

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
#                    'units_changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_BOOLEAN,)),
                    'home_clicked': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_PYOBJECT,)),
#                    'exit': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
                   }



    def __init__(self, widgets):
        super(Build_GUI, self).__init__()
        self.get_ini_info = getiniinfo.GetIniInfo()
        self.widgets = widgets
                
        # This class will handle all the user preferences
        self.prefs = preferences.preferences(self.get_ini_info.get_preference_file_path())
        self.get_initial_data()

        # set initial values for the GUI
        self.set_initial_values()

    def get_initial_data(self):
        # get the axis list from INI
        self.axis_list = self.get_ini_info.get_axis_list()
        # get the joint axis relation from INI
        self.joint_axis_dic, self.double_axis_letter = self.get_ini_info.get_joint_axis_relation()
        # if it's a lathe config, set the tooleditor style
        self.lathe_mode = self.get_ini_info.get_lathe()
        # the size of the DRO
        self.dro_size = self.prefs.getpref("dro_size", 28, int)

    # initial values for the GUI
    def set_initial_values(self):
        self.widgets.adj_dro_size.set_value(self.dro_size)
        
    
    # the user want to use a security mode
    def user_code(self):
        self.widgets.tbtn_setup.set_sensitive(False)

    # the user want a Logo 
    def logo(self,logofile):
        self.widgets.img_logo.set_from_file(logofile)
        self.widgets.img_logo.show()

        page2 = self.widgets.ntb_jog_JA.get_nth_page(2)
        self.widgets.ntb_jog_JA.reorder_child(page2, 0)
        page1 = self.widgets.ntb_jog_JA.get_nth_page(1)
        self.widgets.ntb_jog_JA.reorder_child(page1, -1)

    def format_DRO(self):
        print("**** GMOCCAPY build_GUI INFO ****")
        print("Entering format DRO")
        # first hide angular jog speed, as we do not know jet if we have an angular axis
        self.widgets.spc_ang_jog_vel.hide()

        # we will destroy all Combi_DRO's not in use, so they do not use resources
        for dro in range(len(self.axis_list), 9):
            self.widgets["Combi_DRO_{0}".format(dro)].destroy()
            print(" Combi_DRO_{0} has been destroyed".format(dro))
        
        # check with axis are used, in this case we have to use the 
        # joint axis dict, as we have to set the DRO with the correct joint
        for dro in self.joint_axis_dic:
            if self.joint_axis_dic[dro][0] in self.double_axis_letter:
                print ("we habe an double axis letter here !!")
                if self.joint_axis_dic[dro][1] == 0:
                    print("OK this is the master joint of axis {0}".format(self.joint_axis_dic[dro])) 
                else:
                    continue
            
            # initialize the DRO with the correct joint and axis values    
            print("Combi_DRO_{0} = joint {1} = axis {2}".format(dro, dro, self.joint_axis_dic[dro]))
            self.widgets["Combi_DRO_{0}".format(dro)].set_joint_no(dro)
            self.widgets["Combi_DRO_{0}".format(dro)].set_axis(self.joint_axis_dic[dro])
            self.widgets["Combi_DRO_{0}".format(dro)].change_axisletter(self.joint_axis_dic[dro].upper())
            self.widgets["Combi_DRO_{0}".format(dro)].show()

            # if we have an angular axis we will show the angular jog speed slider
            if self.joint_axis_dic[dro] in ("a","b","c"):                  
                print("**** GMOCCAPY build_GUI INFO ****")
                print("axis {0} is a rotary axis\n".format(self.joint_axis_dic[dro]))
                print("Will display the angular jog slider\n")
                self.widgets.spc_ang_jog_vel.show()
    
    def rearange_dro(self):
        # we have to re-arrange the DRO's, so first we have 
        # to remove them from the dro_table, otherwise we can not reorder them
        for dro in range(len(self.axis_list)):
            self.widgets.tbl_DRO.remove(self.widgets["Combi_DRO_{0}".format(dro)])

        # if we have less than 4 axis, we can resize the table, as we have 
        # enough space to to display each in its own line  
        if len(self.axis_list) < 4:
            self.widgets.tbl_DRO.resize(len(self.axis_list),1)
            if len(self.axis_list) == 2:
                print("found two axis")
                # Check if we are in lathe mode, than display three DRO!
                # if it's a lathe config, set the tooleditor style
                if self.lathe_mode:
                    self.widgets.tbl_DRO.resize(len(self.axis_list + 1),1)
                    self._this_is_a_lathe()
                    return
            for dro, axis in enumerate(self.axis_list):
                self.widgets.tbl_DRO.attach(self.widgets["Combi_DRO_{0}".format(dro)], 
                                            0, 1, int(dro), int(dro + 1), ypadding = 0)

        # having 4 DRO we need to reduce the size, to fit the availible space
        elif len(self.axis_list) == 4:
            self.widgets.tbl_DRO.resize(4,1)
            for dro, axis in enumerate(self.axis_list):
                self.widgets.tbl_DRO.attach(self.widgets["Combi_DRO_{0}".format(dro)], 
                                            0, 1, int(dro), int(dro + 1), ypadding = 0)
                self.widgets["Combi_DRO_{0}".format(dro)].set_property("font_size", self.dro_size * 0.75)


        # having 5 axis we will display 3 in an own line and to must share the line space
        # size of the DRO must be reduced also
        elif len(self.axis_list) == 5:
            self.widgets.tbl_DRO.resize(4,2)
            for dro, axis in enumerate(self.axis_list):

                if dro < 3:
                    size = self.dro_size * 0.75
                    self.widgets.tbl_DRO.attach(self.widgets["Combi_DRO_{0}".format(dro)], 
                                                0, 2, int(dro), int(dro + 1), ypadding = 0)
                else:
                    size = self.dro_size * 0.65
                    if dro == 3:
                        self.widgets.tbl_DRO.attach(self.widgets["Combi_DRO_{0}".format(dro)], 
                                                    0, 1, int(dro), int(dro + 1), ypadding = 0)
                    else:
                        self.widgets.tbl_DRO.attach(self.widgets["Combi_DRO_{0}".format(dro)], 
                                                    1, 2, int(dro-1), int(dro), ypadding = 0)

                self.widgets["Combi_DRO_{0}".format(dro)].set_property("font_size", size)

        # we have more than 5 axis, now we need to arrange the DRO in 2 columns
        # and reduce there size
        else:
            print("**** GMOCCAPY build_GUI INFO ****")
            print("more than 5 axis ")
            # check if amount of axis is an even number and adapt the needed lines
            if len(self.axis_list) % 2 == 0:
                rows = len(self.axis_list) / 2
            else:
                rows = (len(self.axis_list) + 1) / 2

            self.widgets.tbl_DRO.resize(rows, 2)

            col = 0
            row = 0
            for dro, axis in enumerate(self.axis_list):
                self.widgets["Combi_DRO_{0}".format(dro)].set_property("font_size", self.dro_size * 0.65)
                self.widgets.tbl_DRO.attach(self.widgets["Combi_DRO_{0}".format(dro)], 
                                            col, col+1, row, row + 1, ypadding = 0)

                # calculate if we have to place in the first or the second column
                if (dro % 2 == 1):
                    col = 0
                    row +=1
                else:
                    col += 1

    def make_home_button(self):
        print("**** GMOCCAPY build_GUI INFO ****")
        print("Entering make home button")

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
            joint = self.joint_axis_dic.keys()[self.joint_axis_dic.values().index(value)]

        self.emit("home_clicked", joint)

    def _on_btn_unhome_clicked(self, widget):
        self.emit("home_clicked", widget)

    def _on_btn_home_back_clicked(self, widget):
        self.emit("home_clicked", widget)

    def _this_is_a_lathe(self):
        print("**** GMOCCAPY build_GUI INFO ****")
        print("we have a lathe here")

        # as in _hide_unused_DRO we did not check for lathe, we have to correct this now

        # The DRO 1 we make to a second X DRO to indicate the diameter
        # and the DRO has to be shown
        joint_no = self.joint_axis_dic["x"]
        self.widgets.Combi_DRO_0.set_joint_no(joint_no)
        self.widgets.Combi_DRO_1.set_joint_no(joint_no)
        self.widgets.Combi_DRO_1.set_to_diameter(True)
        self.widgets.Combi_DRO_1.show()

        # we change the axis letters of the DRO's
        self.widgets.Combi_DRO_0.change_axisletter("R")
        self.widgets.Combi_DRO_1.change_axisletter("D")

        # the DRO 2 will be the Z DRO
        joint_no = self.joint_axis_dic["z"]
        print("Axis Z = Joint ", joint_no)
        self.widgets.Combi_DRO_2.set_joint_no(joint_no)
        self.widgets.Combi_DRO_2.set_axis("z")
        self.widgets.Combi_DRO_2.change_axisletter("Z")
        self.widgets.Combi_DRO_2.show()

        # We will use Joint 2 as D DRO and Joint 1 as R DRO, 
        # that's why the are arrange them in different order 
        self.widgets.tbl_DRO.attach(self.widgets.Combi_DRO_0, 0, 1, 0, 1)
        self.widgets.tbl_DRO.attach(self.widgets.Combi_DRO_1, 0, 1, 1, 2)
        self.widgets.tbl_DRO.attach(self.widgets.Combi_DRO_2, 0, 1, 2, 3)

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


    def _make_jog_increments(self, signal):
        # Now we will build the option buttons to select the Jog-rates
        # We do this dynamically, because users are able to set them in INI File
        # because of space on the screen only 10 items are allowed
        # jogging increments

        self.incr_rbt_list = []

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
        rbt0.set_property("name","Continious")
        rbt0.connect("pressed", signal, 0)
        self.widgets.vbtb_jog_incr.pack_start(rbt0, True, True, 0)
        rbt0.set_property("draw_indicator", False)
        rbt0.show()
        rbt0.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
        rbt0.__name__ = "rbt0"
        self.incr_rbt_list.append(rbt0)
        # the rest of the buttons are now added to the group
        # self.no_increments is set while setting the hal pins with self._check_len_increments
        for item in range(1, len(self.jog_increments)):
            name = "rbt{0}".format(item)
            rbt = gtk.RadioButton(rbt0, self.jog_increments[item])
            rbt.set_property("name",name)
            rbt.connect("pressed", signal, self.jog_increments[item])
            self.widgets.vbtb_jog_incr.pack_start(rbt, True, True, 0)
            rbt.set_property("draw_indicator", False)
            rbt.show()
            rbt.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
            self.incr_rbt_list.append(rbt)
        self.active_increment = "rbt0"
        return self.active_increment, self.incr_rbt_list


###############################################################################
##                       internal button handling                            ##
###############################################################################

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
        
        for item in self.tab_ref:
            print("Tab ref entry now", item)    
        