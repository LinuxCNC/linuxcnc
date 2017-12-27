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
from gmoccapy import getiniinfo    # this handles the INI File reading so checking is done in that module
from gmoccapy import preferences   # this handles the preferences
from gladevcp import combi_dro

# localization
import locale
from gladevcp.combi_dro import Combi_DRO
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
LOCALEDIR = os.path.join(BASE, "share", "locale")
locale.setlocale(locale.LC_ALL, '')

class Build_GUI:

    def __init__(self, widgets):
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
        # first hide angular jog speed, as we do not know jet if we have an angular axis
        self.widgets.spc_ang_jog_vel.hide()

        # we will destroy all Combi_DRO's not in use, so they do not use resources
        for dro in range(len(self.axis_list), 9):
            self.widgets["Combi_DRO_{0}".format(dro)].destroy()
            print(" Combi_DRO_{0} has been destroyed".format(dro))
        
        # check with axis are used
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
                print("axis {0} is a rotary axis".format(self.joint_axis_dic[dro]))
                self.widgets.spc_ang_jog_vel.show()
    
    def rearange_dro(self):
     
        # we have to re-arrange the DRO's, so first we have 
        # to remove the uswed ones from the dro_table
        for dro in range(len(self.axis_list)):
            self.widgets.tbl_DRO.remove(self.widgets["Combi_DRO_{0}".format(dro)])
            print("removed Combi_DRO_{0}".format(dro))

        # if we have less than 4 axis, we can resize the table, as we have 
        # enough space to to display each in its own line  
        if len(self.axis_list) <= 5:
            self.widgets.tbl_DRO.resize(len(self.axis_list),1)
            if len(self.axis_list) == 2:
                print("found two axis")
                # Check if we are in lathe mode, than display three DRO!
                # if it's a lathe config, set the tooleditor style
                if self.lathe_mode:
                    self.widgets.tbl_DRO.resize(len(self.axis_list + 1),1)
                    self._this_is_a_lathe()
                    return
            for dro in self.joint_axis_dic:
                self.widgets.tbl_DRO.attach(self.widgets["Combi_DRO_{0}".format(dro)], 
                                            0, 1, int(dro), int(dro + 1), ypadding = 0)


        else:
            # we have more than 5 axis, now we need to arrange the DRO in 2 columns
            if len(self.axis_list) % 2 == 0:
                rows = len(self.axis_list) / 2
            else:
                rows = len(self.axis_list) + 1 / 2
                
            self.widgets.tbl_DRO.resize(rows, 2)
            col = 0
            row = 0
            for dro in self.joint_axis_dic:
                print ("dro = ", dro, "row = ", row, "col = ", col)
                self.widgets.tbl_DRO.attach(self.widgets["Combi_DRO_{0}".format(dro)], 
                                            col, col+1, row, row + 1, ypadding = 0)

                print("DRO % 2 = ", dro % 2)
                if (dro % 2 == 1):
                    col = 0
                    row +=1
                else:
                    col += 1

    def make_home_button(self, signal):
        
        for axis in self.joint_axis_dic:
            btn = gtk.Button(self.joint_axis_dic[axis].upper())
            btn.__name__ = "home_{0}.format(axis)"
            btn.connect("clicked", signal, btn.__name__)
            self.widgets.hbtb_ref_axes.add(btn)
            btn.show()
            
        return

        if len(self.axis_list) == 3:
            print("configured for three axis")

            # the Y2 view is not needed on a mill
            self.widgets.rbt_view_y2.hide()
            # X Offset is not necessary on a mill
            self.widgets.lbl_tool_offset_x.hide()
            self.widgets.lbl_offset_x.hide()
            self.widgets.btn_tool_touchoff_x.hide()
            self.widgets.lbl_hide_tto_x.show()

            return

        if len(self.axis_list) == 4:
            print("configured for four axis")

            # we need to find out the axis letters and set all axis corresponding
            self.widgets.lbl_replace_set_value_y.hide()
            self.widgets.lbl_replace_set_value_4.hide()
            self.widgets.lbl_replace_4.hide()
            self.widgets.btn_home_5.hide()
            
            #self.axis_no = "xyzabcuvws".index(axis.lower())
            #axis_four = list(set(self.axis_list) - set(("x", "y", "z")))

    #        image = self.widgets["img_home_{0}".format(self.axisletter_four)]
    #        self.widgets.btn_home_4.set_image(image)
    #        self.widgets.btn_home_4.set_property("tooltip-text", _("Home axis {0}").format(self.axisletter_four.upper()))
            self.widgets.btn_home_4.show()
    
    #        self.widgets.btn_4_plus.set_label("{0}+".format(self.axisletter_four.upper()))
            self.widgets.btn_4_plus.show()
    #        self.widgets.btn_4_minus.set_label("{0}-".format(self.axisletter_four.upper()))
            self.widgets.btn_4_minus.show()
    
    #        image = self.widgets["img_touch_off_{0}".format(self.axisletter_four)]
    #        self.widgets.btn_set_value_4.set_image(image)
    #        self.widgets.btn_set_value_4.set_property("tooltip-text", _("Set axis {0} value to").format(self.axisletter_four.upper()))
            self.widgets.btn_set_value_4.show()
    
    #        if self.axisletter_four in "abc":
    #            self.widgets.Combi_DRO_3.set_property("mm_text_template", "%11.2f")
    #            self.widgets.Combi_DRO_3.set_property("imperial_text_template", "%11.2f")
              
        return
            


        if len(self.axis_list) == 5:
            self.widgets.lbl_replace_set_value_y.hide()
            self.widgets.lbl_replace_4.hide()
            self.widgets.lbl_replace_5.hide()
            self.widgets.lbl_replace_set_value_4.hide()
            self.widgets.lbl_replace_set_value_5.hide()
            self.axisletter_five = self.axis_list[-1]
            self.axisnumber_five = "xyzabcuvw".index(self.axisletter_five)
            self.widgets.Combi_DRO_4.set_property("joint_number", self.axisnumber_five)
            self.widgets.Combi_DRO_4.change_axisletter(self.axisletter_five.upper())

            image = self.widgets["img_home_{0}".format(self.axisletter_five)]
            self.widgets.btn_home_5.set_image(image)
            self.widgets.btn_home_5.set_property("tooltip-text", _("Home axis {0}").format(self.axisletter_five.upper()))

            if self.axisletter_five in "abc":
                self.widgets.Combi_DRO_4.set_property("mm_text_template", "%11.2f")
                self.widgets.Combi_DRO_4.set_property("imperial_text_template", "%11.2f")

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


        # We have to change the size of the DRO, to make them fit the space
        
        # XYZ machine or lathe, no need to change the size
        if len(self.axis_list) < 4:
            return
        # if we have 4 axis, we split the size of all DRO
        elif len(self.axis_list) < 5:
            size = int(self.dro_size * 0.75)
            self.widgets.tbl_DRO.set_homogeneous(False)
            for dro in range(9):
                self.widgets["Combi_DRO_{0}".format(dro)].set_property("font_size", size)

        # if we have 5 axes, we will need some extra space:
        else:
            for dro in range(9):
                size = self.dro_size
                if dro == 4:
                    size = int(size * 0.65) # This factor is just testing to ensure the DRO is able to fit with number 9999.999
                if dro == 5:
                    size = int(size * 0.65)
                self.widgets["Combi_DRO_{0}".format(dro)].set_property("font_size", size)


    def _this_is_a_lathe(self):
        print("**** GMOCCAPY GUI_edit INFO **** \nWe have a lathe here")

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


    def _init_jog_increments(self, signal):
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
            rbt = "rbt{0}".format(item)
            rbt = gtk.RadioButton(rbt0, self.jog_increments[item])
            rbt.connect("pressed", signal, self.jog_increments[item])
            self.widgets.vbtb_jog_incr.pack_start(rbt, True, True, 0)
            rbt.set_property("draw_indicator", False)
            rbt.show()
            rbt.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))
            rbt.__name__ = "rbt{0}".format(item)
            self.incr_rbt_list.append(rbt)
        self.active_increment = "rbt0"
        return self.active_increment, self.incr_rbt_list

