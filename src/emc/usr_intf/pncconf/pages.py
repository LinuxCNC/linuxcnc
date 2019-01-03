#!/usr/bin/env python
# -*- encoding: utf-8 -*-
#
#    This is pncconf, a graphical configuration editor for LinuxCNC
#    Chris Morley copyright 2009
#    pncconf 1.1 revamped by Chris Morley 2014
#    This is based from stepconf, a graphical configuration editor for linuxcnc
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
# This presents and collects the data from the GUI pages
#
# To add pages:
# add the glade file to directory. the first GTK container name will be the reference name (delete the top window)
# add the reference name, Text name and active state to private data variable: self.available_page of private_data.py
# add function call here: <reference name>_prepare(), <reference name>_finish(), and optionally, _init()
# add GLADE callbacks for the page here.
# add large or common function calls to pncconf.py

import gtk
import os
import gobject

class Pages:
    def __init__(self, app):
        self.d = app.d      # collected data
        self.w = app.widgets     # widget names
        self.a = app        # parent, pncconf
        self._p = app._p    # private data
        self.t = app.TESTS
        global debug
        debug = self.a.debugstate
        global dbg
        dbg = self.a.dbg

#********************
# Notebook Controls
#********************

    def on_button_help_clicked(self,widget):
        self.a.show_help()
    def on_help_window_delete_event(self, *args):
        self.w.help_window.hide()
        return True
    def on_print_7i76_button_clicked(self,widget):
        self.a.print_image('map_7i76')
    def on_print_7i77_button_clicked(self,widget):
        self.a.print_image('map_7i77')

    def on_window1_destroy(self, *args):
        if self.a.warning_dialog (self._p.MESS_ABORT,False):
            gtk.main_quit()
            return True
        else:
            return True

    # seaches (self._p.available_page) from the current page forward,
    # for the next page that is True or till second-to-last page.
    # if state found True: call current page finish function.
    # If that returns False then call the next page prepare function and show page
    def on_button_fwd_clicked(self,widget):
        cur = self.w.notebook1.get_current_page()
        u = cur+1
        cur_name,cur_text,cur_init_state,cur_state = self._p.available_page[cur]
        while u < len(self._p.available_page):
            name,text,init_state,state = self._p.available_page[u]
            dbg( "FWD search %s,%s,%s,%s,%s,of %d pages"%(u,name,text,init_state,state,len(self._p.available_page)-1))
            if state and not init_state:
                self.set_buttons_sensitive(0,0)
                dbg( 'Loading page %s'%name)
                self.a.builder.add_from_file(os.path.join(self._p.DATADIR, '%s.glade'%name))
                page = self.a.builder.get_object(name)
                self.w.notebook1.remove_page(u) #remove place holding page
                self.w.notebook1.insert_page(page,None,u)
                self['%s_init'%name]()
                self._p.available_page[u][2] = True # set as initialized now
                self.set_buttons_sensitive(1,1)
            if state:
                if not self['%s_finish'%cur_name](): # finish returning True blocks next page
                    if not self['%s_prepare'%name](): # prepare returning True skips that page
                        self.w.notebook1.set_current_page(u)
                        self.w.title_label.set_text(text)
                        dbg("set %d current"%u)
                    else:
                        self._p.available_page[u][3] = False
                        self.on_button_fwd_clicked(None)
                        return
                break
            else:
                dbg('Page %s skipped'%name)
            u +=1
        # second-to-last page? change the fwd button to finish and show icon
        if u == len(self._p.available_page)-1:
            self.w.apply_image.set_visible(True)
            self.w.label_fwd.set_text(self._p.MESS_DONE)
        # last page? nothing to prepare just finish
        elif u == len(self._p.available_page):
            name,text,init_state,state = self._p.available_page[cur]
            self['%s_finish'%name]()
        # if comming from page 0 to page 1 sensitize 
        # the back button and change fwd button text
        if cur == 0:
            self.w.button_back.set_sensitive(True)
            self.w.label_fwd.set_text(self._p.MESS_FWD)

    # seaches (self._p.available_page) from the current page backward,
    # for the next page that is True or till first page.
    # if state found True: call current page finish function.
    # If that returns False then call the next page prepare function and show page
    def on_button_back_clicked(self,widget):
        cur = self.w.notebook1.get_current_page()
        u = cur-1
        cur_name,cur_text,cur_init_state,cur_state = self._p.available_page[cur]
        while u > -1:
            name,text,init_state,state = self._p.available_page[u]
            dbg( "BACK search %s,%s,%s,%s,%s,of %d pages"%(u,name,text,init_state,state,len(self._p.available_page)-1))
            if state:
                if not cur == len(self._p.available_page)-1:
                    self['%s_finish'%cur_name]()
                self.w.notebook1.set_current_page(u)
                self.set_buttons_sensitive(1,1)
                self['%s_prepare'%name]()
                self.w.title_label.set_text(text)
                dbg("set %d current"%u)
                break
            u -=1
        # Not last page? change finish button text and hide icon
        if u <= len(self._p.available_page):
            self.w.apply_image.set_visible(False)
            self.w.label_fwd.set_text(self._p.MESS_FWD)
        # page 0 ? de-sensitize the back button and change fwd button text 
        if u == 0:
            self.w.button_back.set_sensitive(False)
            self.w.label_fwd.set_text(self._p.MESS_START)

    def set_buttons_sensitive(self,bstate,fstate):
        self.w.button_fwd.set_sensitive(fstate)
        self.w.button_back.set_sensitive(bstate)
        while gtk.events_pending():
            gtk.main_iteration()

    # Sets the visual state of a list of page(s)
    # The page names must be the one used in self._p.available_page
    # If a pages state is false it won't be seen or it's functions called.
    # if you deselect the current page it will show till next time it is cycled
    def page_set_state(self,page_list,state):
        dbg("page_set_state() %s ,%s"%(page_list,state))
        for i,data in enumerate(self._p.available_page):
            name,text,init_state,curstate = data
            if name in page_list:
                self._p.available_page[i][3] = state
                dbg("%s State changed to %s"%(name,state))

#####################################################
# All Page Methods
#####################################################
#***************
# Initialize
#***************
    def initialize(self):
        # one time initialized data
        self.w.title_label.set_text(self._p.available_page[0][1])
        self.w.button_back.set_sensitive(False)
        self.w.label_fwd.set_text(self._p.MESS_START)
        if debug:
            self.w.window1.set_title('Pncconf -debug mode')

        # add some custom signals for motor/encoder scaling and bldc 
        for axis in ["x","y","z","a","s"]:
            cb = ["encoderscale","stepscale"]
            for i in cb:
                self.w[axis + i].connect("value-changed", self.a.motor_encoder_sanity_check,axis)
            cb = ["bldc_incremental_feedback","bldc_use_hall","bldc_use_encoder","bldc_use_index","bldc_fanuc_alignment","bldc_emulated_feedback",
                "bldc_output_hall"]
            for i in cb:
                self.w[axis + i].connect("clicked", self.a.bldc_update,axis)

        self.a.fill_pintype_model()

        self.intro_prepare()
        # get the original background color, must realize the widget first to get the true color.
        # we use this later to high light missing axis info
        self.w.xencoderscale.realize()
        self.a.origbg = self.w.xencoderscale.style.bg[gtk.STATE_NORMAL]
        self.w.window1.set_geometry_hints(min_width=750)

#************
# INTRO PAGE
#************
    def intro_prepare(self):
        self.d.help = "help-welcome.txt"
    def intro_finish(self):
        pass

#************
# START PAGE
#************
    def start_prepare(self):
        self.d.help = "help-load.txt"
        # search for firmware packages

    def start_finish(self):

        if not self.w.createconfig.get_active():
            error = self.a.load_config()
        if self.w.advancedconfig.get_active():
            state = True
        else:
            state = False
        self.d.advanced_option = state
        self.page_set_state(['options','external','realtime'],state)
        self.d.createsymlink = self.w.createsymlink.get_active()
        self.d.createshortcut = self.w.createshortcut.get_active()
        self.w.window1.set_title(_("Point and click configuration - %s.pncconf ") % self.d.machinename)
        self.a.add_external_folder_boardnames()
        # here we initialise the mesa configure page data
        #TODO is this right place?
        self.d._mesa0_configured = False
        self.d._mesa1_configured = False
        #must be filled after loading config for custom signals are added
        self.a.fill_combobox_models()

#************
# BASE PAGE
#************
    def base_prepare(self):
        self.d.help = "help-basic.txt"
        self.w.machinename.set_text(self.d.machinename)
        self.w.axes.set_active(self.d.axes)
        self.w.units.set_active(self.d.units)
        self.w.servoperiod.set_value(self.d.servoperiod)
        self.w.machinename.grab_focus()
        if self.d.number_mesa == 1:
            self.w.mesa0_checkbutton.set_active(True)
            self.w.mesa1_checkbutton.set_active(False)
        elif self.d.number_mesa == 2:
            self.w.mesa0_checkbutton.set_active(True)
            self.w.mesa1_checkbutton.set_active(True)
        else:
            self.w.mesa0_checkbutton.set_active(False)
            self.w.mesa1_checkbutton.set_active(False)

        if self.d.number_pports == 3:
             self.w.radio_pp3.set_active(True)
        elif self.d.number_pports == 2:
             self.w.radio_pp2.set_active(True)
        elif self.d.number_pports == 1:
             self.w.radio_pp1.set_active(True)
        else:
             self.w.radio_none.set_active(True)
        self.w.require_homing.set_active(self.d.require_homing)
        self.w.individual_homing.set_active(self.d.individual_homing)
        self.w.restore_joint_position.set_active(self.d.restore_joint_position) 
        self.w.random_toolchanger.set_active(self.d.random_toolchanger) 
        self.w.raise_z_on_toolchange.set_active(self.d.raise_z_on_toolchange) 
        self.w.allow_spindle_on_toolchange.set_active(self.d.allow_spindle_on_toolchange)
        self.w.toolchangeprompt.set_active(self.d.toolchangeprompt)

    def base_finish(self):
        machinename = self.w.machinename.get_text()
        self.d.machinename = machinename.replace(" ","_")
        self.w.window1.set_title(_("Point and click configuration - %s.pncconf ") % self.d.machinename)
        self.d.axes = self.w.axes.get_active()
        if self.d.axes == 0: self.d.available_axes = ['x','y','z','s']
        elif self.d.axes == 1: self.d.available_axes = ['x','y','z','a','s']
        elif self.d.axes == 2: self.d.available_axes = ['x','z','s']
        self.d.units = self.w.units.get_active()
        self.d.servoperiod = self.w.servoperiod.get_value()
        self.page_set_state('mesa1',self.w.mesa1_checkbutton.get_active())
        for let in ['x','y','z','a']:
            if not let in self.d.available_axes:
                state = False
            else:
                state = True
            self.page_set_state('%s_axis'%let,state)
            self.page_set_state('%s_motor'%let,state)
        i = self.w.mesa0_checkbutton.get_active()
        j = self.w.mesa1_checkbutton.get_active()
        self.d.number_mesa = int(i)+int(j)
        if self.w.radio_pp3.get_active():
            self.d.number_pports = 3
        elif self.w.radio_pp2.get_active():
            self.d.number_pports = 2
        elif self.w.radio_pp1.get_active():
            self.d.number_pports = 1
        else:
            self.d.number_pports = 0
        self.page_set_state('pport1',self.d.number_pports>0)
        self.page_set_state('pport2',self.d.number_pports>1)
        if self.d.number_pports == 0 and self.d.number_mesa== 0 :
           self.a.warning_dialog(_("You need to designate a parport and/or mesa I/O device before continuing."),True)
           return True 
        self.d.require_homing = self.w.require_homing.get_active()
        self.d.individual_homing = self.w.individual_homing.get_active()
        self.d.restore_joint_position = self.w.restore_joint_position.get_active() 
        self.d.random_toolchanger = self.w.random_toolchanger.get_active() 
        self.d.raise_z_on_toolchange = self.w.raise_z_on_toolchange.get_active() 
        self.d.allow_spindle_on_toolchange = self.w.allow_spindle_on_toolchange.get_active()
        self.d.toolchangeprompt = self.w.toolchangeprompt.get_active()

    # BASE callbacks
    def on_mesa_checkbutton_toggled(self, *args):
        return
        i = self.w.mesa0_checkbutton.get_active()
        j = self.w.mesa1_checkbutton.get_active()
        self.w.mesa0_boardtitle.set_sensitive(i)
        self.w.mesa1_boardtitle.set_sensitive(j)
        if j and not i:
            self.w.mesa1_checkbutton.set_active(False)
            self.w.mesa1_boardtitle.set_sensitive(False)
        
    def on_pp1_checkbutton_toggled(self, *args): 
        i = self.w.pp1_checkbutton.get_active()   
        self.w.pp1_direction.set_sensitive(i)
        self.w.ioaddr1.set_sensitive(i)
        if i == 0:
           self.w.pp2_checkbutton.set_active(i)
           self.w.ioaddr2.set_sensitive(i)
           self.w.pp3_checkbutton.set_active(i)
           self.w.ioaddr3.set_sensitive(i)

    def on_pp2_checkbutton_toggled(self, *args):
        i = self.w.pp2_checkbutton.get_active()
        if self.w.pp1_checkbutton.get_active() == 0:
            i = 0  
            self.w.pp2_checkbutton.set_active(0)
        self.w.pp2_direction.set_sensitive(i)
        self.w.ioaddr2.set_sensitive(i)
        if i == 0:
           self.w.pp3_checkbutton.set_active(i)
           self.w.ioaddr3.set_sensitive(i)

    def on_pp3_checkbutton_toggled(self, *args): 
        i = self.w.pp3_checkbutton.get_active() 
        if self.w.pp2_checkbutton.get_active() == 0:
          i = 0  
          self.w.pp3_checkbutton.set_active(0)
        self.w.pp3_direction.set_sensitive(i)
        self.w.ioaddr3.set_sensitive(i)      

    def on_machinename_changed(self, *args):
        temp = self.w.machinename.get_text()
        self.w.confdir.set_text("~/linuxcnc/configs/%s" % temp.replace(" ","_"))

    def on_address_search_clicked(self,w):
        self.a.show_help()
        match =  os.popen('lspci -v').read()
        textbuffer = self.w.textoutput.get_buffer()
        try :         
            textbuffer.set_text(match)
            self.w.helpnotebook.set_current_page(2)
            self.w.help_window.show_all()
        except:
            text = _("PCI search page is unavailable\n")
            self.a.warning_dialog(text,True)

    def on_latency_test_clicked(self, w):
        self.latency_pid = os.spawnvp(os.P_NOWAIT,"latency-test", ["latency-test"])
        self.w['window1'].set_sensitive(0)
        gobject.timeout_add(1, self.latency_running_callback)

    def latency_running_callback(self):
        pid, status = os.waitpid(self.latency_pid, os.WNOHANG)
        if pid:
            self.w['window1'].set_sensitive(1)
            return False
        return True

    def on_calculate_ideal_period(self, *args):
        steptime = self.w.steptime.get_value()
        stepspace = self.w.stepspace.get_value()
        latency = self.w.latency.get_value()
        minperiod = self.d.minperiod(steptime, stepspace, latency)
        maxhz = int(1e9 / minperiod)     
        self.w.baseperiod.set_text("%d ns" % minperiod)
        self.w.maxsteprate.set_text("%d Hz" % maxhz)

    def on_units_changed(self,widget):
        if not self.d.units == widget.get_active():
            # change the XYZ axis defaults to metric or imperial
            # This erases any entered data that would make sense to change
            self.d.set_axis_unit_defaults(not widget.get_active())

#************
# SCREEN PAGE
#************
    def screen_prepare(self):
        self.d.help = "help-gui.txt"
        self.w.combo_screentype.set_active(self.d.frontend-1)
        self.w.default_linear_velocity.set_value( self.d.default_linear_velocity*60)
        self.w.max_feed_override.set_value(self.d.max_feed_override*100 )
        self.w.max_spindle_override.set_value( self.d.max_spindle_override*100)
        self.w.min_spindle_override.set_value( self.d.min_spindle_override*100)
        self.w.max_linear_velocity.set_value( self.d.max_linear_velocity*60)
        self.w.min_linear_velocity.set_value( self.d.min_linear_velocity*60)
        self.w.default_angular_velocity.set_value( self.d.default_angular_velocity*60)
        self.w.max_angular_velocity.set_value( self.d.max_angular_velocity*60)
        self.w.min_angular_velocity.set_value( self.d.min_angular_velocity*60)
        self.w.editor.set_text(self.d.editor)
        if self.d.units == self._p._IMPERIAL :
            temp = self.d.increments_imperial
            tempunits = _("in / min")
        else:
            temp = self.d.increments_metric
            tempunits = _("mm / min")
        self.w.increments.set_text(temp)
        for i in (0,1,2):
            self.w["velunits"+str(i)].set_text(tempunits)
        self.w.position_offset.set_active(self.d.position_offset)
        self.w.position_feedback.set_active(self.d.position_feedback)
        self.w.geometry.set_text(self.d.geometry)
        self.a.read_touchy_preferences()
        self.w.axisforcemax.set_active(self.d.axisforcemax)

        for i in ("touchy","axis"):
            self.w[i+"size"].set_active(self.d[i+"size"][0])
            self.w[i+"width"].set_value(self.d[i+"size"][1])
            self.w[i+"height"].set_value(self.d[i+"size"][2])
            self.w[i+"position"].set_active(self.d[i+"position"][0])
            self.w[i+"xpos"].set_value(self.d[i+"position"][1])
            self.w[i+"ypos"].set_value(self.d[i+"position"][2])
        
        if os.path.exists(self._p.THEMEDIR):
            self.a.get_installed_themes()

    def screen_finish(self):
        # Sanity checks

        self.d.default_linear_velocity = self.w.default_linear_velocity.get_value()/60
        self.d.max_feed_override = self.w.max_feed_override.get_value()/100
        self.d.max_spindle_override = self.w.max_spindle_override.get_value()/100
        self.d.min_spindle_override = self.w.min_spindle_override.get_value()/100
        self.d.max_linear_velocity = self.w.max_linear_velocity.get_value()/60
        self.d.min_linear_velocity = self.w.min_linear_velocity.get_value()/60
        self.d.default_angular_velocity = self.w.default_angular_velocity.get_value()/60
        self.d.max_angular_velocity = self.w.max_angular_velocity.get_value()/60
        self.d.min_angular_velocity = self.w.min_angular_velocity.get_value()/60
        self.d.editor = self.w.editor.get_text()
        if self.d.units == self._p._IMPERIAL :self.d.increments_imperial = self.w.increments.get_text()
        else:self.d.increments_metric = self.w.increments.get_text()
        self.d.geometry = self.w.geometry.get_text()
        self.d.position_offset = self.w.position_offset.get_active()
        self.d.position_feedback = self.w.position_feedback.get_active()

        self.d.frontend = self.w.combo_screentype.get_active() +1

        self.d.touchytheme = self.w.touchytheme.get_active_text()
        self.d.touchyforcemax = self.w.touchyforcemax.get_active()
        self.d.axisforcemax = self.w.axisforcemax.get_active()

        for i in ("touchy","axis"):
            self.d[i+"size"][0] = self.w[i+"size"].get_active()
            self.d[i+"size"][1] = self.w[i+"width"].get_value()
            self.d[i+"size"][2] = self.w[i+"height"].get_value()
            self.d[i+"position"][0] = self.w[i+"position"].get_active()
            self.d[i+"position"][1] = self.w[i+"xpos"].get_value()
            self.d[i+"position"][2] = self.w[i+"ypos"].get_value()

        if self.w.autotouchz.get_active():
            self.d.classicladder = True
            if not self.w.ladderexist.get_active():
                self.w.laddertouchz.set_active(True)

    # callbacks
    def on_loadladder_clicked(self, *args):self.t.load_ladder(self)

    def on_combo_screentype_changed(self,w):
        if w.get_active()+1 == self._p._AXIS:
            self.w.axis_info.set_expanded(True)
            self.w.axis_info.show()
            self.w.gmcpy_info.hide()
            self.w.touchy_info.hide()
        elif w.get_active()+1 == self._p._TOUCHY:
            self.w.touchy_info.set_expanded(True)
            self.w.touchy_info.show()
            self.w.gmcpy_info.hide()
            self.w.axis_info.hide()
        elif w.get_active()+1 == self._p._TKLINUXCNC:
            self.w.axis_info.hide()
            self.w.gmcpy_info.hide()
            self.w.touchy_info.hide()
        elif w.get_active()+1 == self._p._GMOCCAPY:
            self.w.gmcpy_info.set_expanded(True)
            self.w.gmcpy_info.show()
            self.w.axis_info.hide()
            self.w.touchy_info.hide()

    def on_halui_toggled(self, *args):
        i= self.w.halui.get_active()
        self.w.haluitable.set_sensitive(i)

    def on_classicladder_toggled(self, *args):
        i= self.w.classicladder.get_active()
        self.w.digitsin.set_sensitive(i)
        self.w.digitsout.set_sensitive(i)
        self.w.s32in.set_sensitive(i)
        self.w.s32out.set_sensitive(i)
        self.w.floatsin.set_sensitive(i)
        self.w.floatsout.set_sensitive(i)
        self.w.bitmem.set_sensitive(i)
        self.w.wordmem.set_sensitive(i)
        self.w.modbus.set_sensitive(i)
        self.w.ladderblank.set_sensitive(i)
        self.w.ladder1.set_sensitive(i)
        self.w.ladder2.set_sensitive(i)
        self.w.laddertouchz.set_sensitive(i)
        if  self.w.createconfig.get_active():
            self.w.ladderexist.set_sensitive(False)
        else:
            self.w.ladderexist.set_sensitive(i)
        self.w.loadladder.set_sensitive(i)
        self.w.label_digin.set_sensitive(i)
        self.w.label_digout.set_sensitive(i)
        self.w.label_s32in.set_sensitive(i)
        self.w.label_s32out.set_sensitive(i)
        self.w.label_floatin.set_sensitive(i)
        self.w.label_floatout.set_sensitive(i)
        self.w.label_bitmem.set_sensitive(i)
        self.w.label_wordmem.set_sensitive(i)
        self.w.ladderconnect.set_sensitive(i)
        if self.w.laddertouchz.get_active():
            i = self.d.gladevcphaluicmds
            self.w["halui_cmd%d"%(i)].set_text("G38.2 Z-2 F16   ( search for touch off plate )")
            self.w["halui_cmd%d"%(i+1)].set_text("G10 L20 P0 Z.25 ( Ofset current Origin by plate thickness )")
            self.w["halui_cmd%d"%(i+2)].set_text("G0 Z.5           ( Rapid away from touch off plate )")

    def on_xusecomp_toggled(self, *args): self.a.comp_toggle('x')
    def on_yusecomp_toggled(self, *args): self.a.comp_toggle('y')
    def on_zusecomp_toggled(self, *args): self.a.comp_toggle('z')
    def on_ausecomp_toggled(self, *args): self.a.comp_toggle('a')
    def on_xusebacklash_toggled(self, *args): self.a.backlash_toggle('x')
    def on_yusebacklash_toggled(self, *args): self.a.backlash_toggle('y')
    def on_zusebacklash_toggled(self, *args): self.a.backlash_toggle('z')
    def on_ausebacklash_toggled(self, *args): self.a.backlash_toggle('a')

    def on_xdrivertype_changed(self, *args): self.a.driver_changed('x')
    def on_ydrivertype_changed(self, *args): self.a.driver_changed('y')
    def on_zdrivertype_changed(self, *args): self.a.driver_changed('z')
    def on_adrivertype_changed(self, *args): self.a.driver_changed('a')
    def on_sdrivertype_changed(self, *args): self.a.driver_changed('s')

    def on_xbldc_toggled(self, *args): self.a.bldc_toggled('x')
    def on_ybldc_toggled(self, *args): self.a.bldc_toggled('y')
    def on_zbldc_toggled(self, *args): self.a.bldc_toggled('z')
    def on_abldc_toggled(self, *args): self.a.bldc_toggled('a')
    def on_sbldc_toggled(self, *args): self.a.bldc_toggled('s')

#####################
# VCP
#####################
    def vcp_prepare(self):
        self.d.help = "help-vcp.txt"
        self.w.pyvcp.set_active(self.d.pyvcp)
        self.on_pyvcp_toggled()
        self.w.pyvcpexist.set_active(self.d.pyvcpexist)
        self.w.pyvcp1.set_active(self.d.pyvcp1)
        self.w.pyvcpblank.set_active(self.d.pyvcpblank)
        self.w.pyvcpconnect.set_active(self.d.pyvcpconnect)
        for i in ("gladevcp","gladesample","gladeexists","spindlespeedbar","spindleatspeed","gladevcpforcemax",
                "zerox","zeroy","zeroz","zeroa","autotouchz","centerembededgvcp","sideembededgvcp","standalonegvcp",
                "gladevcpposition","gladevcpsize","pyvcpposition","pyvcpsize"):
            self.w[i].set_active(self.d[i])
        for i in ("maxspeeddisplay","gladevcpwidth","gladevcpheight","gladevcpxpos","gladevcpypos",
                    "pyvcpwidth","pyvcpheight","pyvcpxpos","pyvcpypos"):
            self.w[i].set_value(self.d[i])
        self.a.update_gladevcp()

    def vcp_finish(self):
        if not self.w.createconfig.get_active():
            if self.w.gladevcp.get_active() and self.w.gladesample.get_active():
                if self.a.gladevcp_sanity_check():
                    return True
            if self.w.pyvcp.get_active() and not self.w.pyvcpexist.get_active():
                if self.a.pyvcp_sanity_check():
                    return True
        self.d.pyvcpblank = self.w.pyvcpblank.get_active()
        self.d.pyvcp1 = self.w.pyvcp1.get_active()
        self.d.pyvcpexist = self.w.pyvcpexist.get_active()
        self.d.pyvcp = self.w.pyvcp.get_active()
        self.d.pyvcpconnect = self.w.pyvcpconnect.get_active() 
        if self.d.pyvcp == True:
           if self.w.pyvcpblank.get_active() == True:
              self.d.pyvcpname = "blank.xml"
              self.d.pyvcphaltype = 0
           if self.w.pyvcp1.get_active() == True:
              self.d.pyvcpname = "spindle.xml"
              self.d.pyvcphaltype = 1
           if self.w.pyvcpexist.get_active() == True:
              self.d.pyvcpname = "pyvcp-panel.xml"
        for i in ("gladevcp","gladesample","spindlespeedbar","spindleatspeed","gladevcpforcemax",
                "centerembededgvcp","sideembededgvcp","standalonegvcp","gladeexists",
                "gladevcpposition","gladevcpsize","pyvcpposition","pyvcpsize","autotouchz"):
            self.d[i] = self.w[i].get_active()
        # set HALUI commands ( on advanced page) based on the user requested glade buttons
        i =  self.d.gladevcphaluicmds = 0
        for temp in(("zerox","G10 L20 P0 X0 ( Set X to zero )"),("zeroy","G10 L20 P0 Y0 ( Set Y to zero )"),
                    ("zeroz","G10 L20 P0 Z0 ( Set Z to zero )"),("zeroa","G10 L20 P0 A0 ( Set A to zero )")):
            self.d[temp[0]] = self.w[temp[0]].get_active()
            if self.d[temp[0]]:
                self.d.halui = True
                self.d["halui_cmd%d"% i] = temp[1]
                i += 1
                self.d.gladevcphaluicmds += 1
        for i in ("maxspeeddisplay","gladevcpwidth","gladevcpheight","gladevcpxpos","gladevcpypos",
                    "pyvcpwidth","pyvcpheight","pyvcpxpos","pyvcpypos"):
            self.d[i] = self.w[i].get_value()
        self.d.gladevcptheme = self.w.gladevcptheme.get_active_text()
        # make sure there is a copy of the choosen gladevcp panel in /tmp/
        # We will copy it later into our config folder
        self.t.gladevcptestpanel(self)

    # CALL BACKS
    def on_gladevcp_toggled(self,*args):
        self.a.update_gladevcp()
    def on_pyvcp_toggled(self,*args):
        i= self.w.pyvcp.get_active()
        self.w.pyvcpblank.set_sensitive(i)
        self.w.pyvcp1.set_sensitive(i)
        self.w.pyvcp2.set_sensitive(i)
        self.w.pyvcpgeometry.set_sensitive(i)
        if  self.w.createconfig.get_active():
            self.w.pyvcpexist.set_sensitive(False)
        else:
            self.w.pyvcpexist.set_sensitive(i)
        self.w.displaypanel.set_sensitive(i)
        self.w.pyvcpconnect.set_sensitive(i)

    def on_displaypanel_clicked(self,*args):
        self.t.testpanel(self)
    def on_displaygladevcp_clicked(self,*args):
        self.t.display_gladevcp_panel()

#************
# EXTERAL PAGE
#************
    def external_prepare(self):
        self.d.help = "help-extcontrols.txt"
        if self.d.multimpg :
            self.w.multimpg.set_active(1)
        else:
            self.w.sharedmpg.set_active(1)
        if self.d.fo_usempg :
            self.w.fo_usempg.set_active(1)
        else:
            self.w.fo_useswitch.set_active(1)
        if self.d.mvo_usempg :
            self.w.mvo_usempg.set_active(1)
        else:
            self.w.mvo_useswitch.set_active(1)
        if self.d.so_usempg :
            self.w.so_usempg.set_active(1)
        else:
            self.w.so_useswitch.set_active(1)
        self.w.mitsub_vfd.set_active(self.d.mitsub_vfd)
        self.w.gs2_vfd.set_active(self.d.gs2_vfd)
        self.w.gs2_vfd_slave.set_value(self.d.gs2_vfd_slave)
        self.w.gs2_vfd_accel.set_value(self.d.gs2_vfd_accel)
        self.w.gs2_vfd_deaccel.set_value(self.d.gs2_vfd_deaccel)
        self.search_for_serial_device_name()
        self.w.gs2_vfd_device_name.set_active(0)
        model = self.w.gs2_vfd_device_name.get_model()
        for num,i in enumerate(model):
            if i[0] == self.d.gs2_vfd_port:
                self.w.gs2_vfd_device_name.set_active(num)
        self.w.gs2_vfd_baud.set_active(0)
        model = self.w.gs2_vfd_baud.get_model()
        for num,i in enumerate(model):
            if i[1] == self.d.gs2_vfd_baud:
                self.w.gs2_vfd_baud.set_active(num)
        self.w.serial_vfd.set_active(self.d.serial_vfd)
        self.w.jograpidrate.set_value(self.d.jograpidrate)
        self.w.singlejogbuttons.set_active(self.d.singlejogbuttons)
        self.w.multijogbuttons.set_active(self.d.multijogbuttons)
        self.w.externalmpg.set_active(self.d.externalmpg)
        self.w.externaljog.set_active(self.d.externaljog)
        self.w.externalfo.set_active(self.d.externalfo)
        self.w.externalmvo.set_active(self.d.externalmvo)
        self.w.externalso.set_active(self.d.externalso)
        self.w.sharedmpg.set_active(self.d.sharedmpg)
        self.w.multimpg.set_active(self.d.multimpg)
        self.w.incrselect.set_active(self.d.incrselect)
        for i in ("mpg","fo","so","mvo"):
            self.w[i+"debounce"].set_active(self.d[i+"debounce"])
            self.w[i+"debouncetime"].set_value(self.d[i+"debouncetime"])
            self.w[i+"graycode"].set_active(self.d[i+"graycode"])
            self.w[i+"ignorefalse"].set_active(self.d[i+"ignorefalse"])
        if self.d.units == self._p._IMPERIAL :
            tempunits = "in"
        else:
            tempunits = "mm"      
        for i in range(0,16):
            self.w["foincrvalue"+str(i)].set_value(self.d["foincrvalue"+str(i)])
            self.w["mvoincrvalue"+str(i)].set_value(self.d["mvoincrvalue"+str(i)])
            self.w["soincrvalue"+str(i)].set_value(self.d["soincrvalue"+str(i)])
            self.w["mpgincrvalue"+str(i)].set_value(self.d["mpgincrvalue"+str(i)])
            self.w["mpgincr"+str(i)].set_text(tempunits)

        self.w.jograpidunits.set_text(tempunits+" / min")
        for i in range(0,4):
            self.w["joystickjograpidunits%d"%i].set_text(tempunits+" / min")
        self.w.joystickjog.set_active(self.d.joystickjog)
        self.w.usbdevicename.set_text(self.d.usbdevicename)
        for i in range(0,4):
            self.w["joystickjograpidrate%d"%i].set_value(self.d["joystickjograpidrate%d"%i])
        for temp in ("joycmdxpos","joycmdxneg","joycmdypos","joycmdyneg","joycmdzpos","joycmdzneg","joycmdapos","joycmdaneg","joycmdrapida","joycmdrapidb",
            "joycmdanalogx","joycmdanalogy","joycmdanalogz","joycmdanaloga"):
            self.w[temp].set_text(self.d[temp])

    def external_finish(self):
        self.d.mitsub_vfd = self.w.mitsub_vfd.get_active()
        self.d.gs2_vfd = self.w.gs2_vfd.get_active()
        self.d.gs2_vfd_slave = self.w.gs2_vfd_slave.get_value()
        self.d.gs2_vfd_accel = self.w.gs2_vfd_accel.get_value()
        self.d.gs2_vfd_deaccel = self.w.gs2_vfd_deaccel.get_value()
        self.d.gs2_vfd_port = self.w.gs2_vfd_device_name.get_active_text()
        model = self.w.gs2_vfd_baud.get_model()
        index = self.w.gs2_vfd_baud.get_active()
        self.d.gs2_vfd_baud = model[index][1]
        self.d.serial_vfd = self.w.serial_vfd.get_active()
        self.d.multimpg = self.w.multimpg.get_active()
        self.d.fo_usempg = self.w.fo_usempg.get_active()
        self.d.fo_useswitch = self.w.fo_useswitch.get_active()
        self.d.mvo_usempg = self.w.mvo_usempg.get_active()
        self.d.mvo_useswitch = self.w.mvo_useswitch.get_active()
        self.d.so_usempg = self.w.so_usempg.get_active()
        self.d.so_useswitch = self.w.so_useswitch.get_active()
        self.d.jograpidrate = self.w.jograpidrate.get_value()
        self.d.singlejogbuttons = self.w.singlejogbuttons.get_active()
        self.d.multijogbuttons = self.w.multijogbuttons.get_active()
        self.d.externalmpg = self.w.externalmpg.get_active()
        self.d.externaljog = self.w.externaljog.get_active()
        self.d.externalfo = self.w.externalfo.get_active()
        self.d.externalso = self.w.externalso.get_active()
        self.d.externalmvo = self.w.externalmvo.get_active()
        self.d.sharedmpg = self.w.sharedmpg.get_active()
        self.d.multimpg = self.w.multimpg.get_active()
        self.d.incrselect = self.w.incrselect.get_active()
        for i in ("mpg","fo","so","mvo"):
            self.d[i+"debounce"] = self.w[i+"debounce"].get_active()
            self.d[i+"debouncetime"] = self.w[i+"debouncetime"].get_value()
            self.d[i+"graycode"] = self.w[i+"graycode"].get_active()
            self.d[i+"ignorefalse"] = self.w[i+"ignorefalse"].get_active()
        for i in range (0,16):
            self.d["foincrvalue"+str(i)] = self.w["foincrvalue"+str(i)].get_value()
            self.d["mvoincrvalue"+str(i)] = self.w["mvoincrvalue"+str(i)].get_value()
            self.d["soincrvalue"+str(i)] = self.w["soincrvalue"+str(i)].get_value()
            self.d["mpgincrvalue"+str(i)] = self.w["mpgincrvalue"+str(i)].get_value()
        self.d.usbdevicename = self.w.usbdevicename.get_text()
        self.d.joystickjog = self.w.joystickjog.get_active()
        for i in range(0,4):
            self.d["joystickjograpidrate%d"%i] = self.w["joystickjograpidrate%d"%i].get_value()
        for temp in ("joycmdxpos","joycmdxneg","joycmdypos","joycmdyneg","joycmdzpos","joycmdzneg","joycmdapos",
        "joycmdaneg","joycmdrapida","joycmdrapidb","joycmdanalogx","joycmdanalogy","joycmdanalogz","joycmdanaloga"):
            self.d[temp] = self.w[temp].get_text()
        #self.w.joyjogexpander.set_expanded(False)

    # callbacks
    def on_addrule_clicked(self, *args):
        self.a.add_device_rule()

    def on_joystickjog_toggled(self, *args):
        if self.w.externaljog.get_active() == True and self.w.joystickjog.get_active() == True:
            self.w.externaljog.set_active(False)
        self.w.notebook_ext_page.set_current_page(1)
        self.on_external_options_toggled()

    def on_externaljog_toggled(self, *args):
        if self.w.joystickjog.get_active() == True and self.w.externaljog.get_active() == True:
            self.w.joystickjog.set_active(False)
        self.w.notebook_ext_page.set_current_page(2)
        self.on_external_options_toggled()

    def on_externalso_toggled(self, *args):
        self.w.notebook_ext_page.set_current_page(6)
        self.on_external_options_toggled()
    def on_externalmvo_toggled(self, *args):
        self.w.notebook_ext_page.set_current_page(5)
        self.on_external_options_toggled()
    def on_externalfo_toggled(self, *args):
        self.w.notebook_ext_page.set_current_page(4)
        self.on_external_options_toggled()
    def on_externalmpg_toggled(self, *args):
        self.w.notebook_ext_page.set_current_page(3)
        self.on_external_options_toggled()
    def on_spindle_vfd_toggled(self, *args):
        self.w.notebook_ext_page.set_current_page(7)
        self.on_external_options_toggled()

    def on_external_options_toggled(self, *args):
        self.w.externaljogbox.set_sensitive(self.w.externaljog.get_active())
        self.w.externalmpgbox.set_sensitive(self.w.externalmpg.get_active())
        self.w.externalfobox.set_sensitive(self.w.externalfo.get_active())
        self.w.externalmvobox.set_sensitive(self.w.externalmvo.get_active())
        self.w.externalsobox.set_sensitive(self.w.externalso.get_active())      
        self.w.foexpander.set_sensitive(self.w.fo_useswitch.get_active())
        self.w.mvoexpander.set_sensitive(self.w.mvo_useswitch.get_active())
        self.w.soexpander.set_sensitive(self.w.so_useswitch.get_active())
        self.w.joystickjogbox.set_sensitive(self.w.joystickjog.get_active())
        
        i =  self.w.incrselect.get_active()
        for j in range(1,16):
            self.w["incrlabel%d"% j].set_sensitive(i)
            self.w["mpgincrvalue%d"% j].set_sensitive(i)
            self.w["mpgincr%d"% j].set_sensitive(i)

    def on_joysearch_clicked(self, *args):
        self.a.search_for_device_rule()
    def on_joysticktest_clicked(self, *args):
        self.a.test_joystick()

    def search_for_serial_device_name(self):
        match = os.popen("""ls /sys/class/tty/*/device/driver | grep 'driver' | cut -d "/" -f 5""").read().split()
        model = self.w.gs2_vfd_device_name.get_model()
        model.clear()
        for item in match:
            name = '/dev/%s'%item
            model.append((name,))

#************
# MESA0 PAGE
#************
    def mesa0_init(self):
        self._p.prepare_block = True
        self.w.mesa0_boardtitle.set_model(self.w.mesa_boardname_store)
        self.a.init_mesa_signals(0)
        self._p.prepare_block = False

    def mesa0_prepare(self):
        self.d.help = "help-mesa.txt"
        def lookup(name):
            temp = -1
            for search,item in enumerate(self._p.MESA_BOARDNAMES):
                #print self._p.MESA_BOARDNAMES[search],name
                if self._p.MESA_BOARDNAMES[search]  == name:
                    temp = search
            return temp
        # search for boardname - if it's not there add it
        name_vector = lookup(self.d.mesa0_boardtitle)
        if name_vector == -1:
            self._p.MESA_BOARDNAMES.append(self.d.mesa0_boardtitle)
            model = self.w.mesa0_boardtitle.get_model()
            model.append((self.d.mesa0_boardtitle,))
            name_vector = lookup(self.d.mesa0_boardtitle)

        self.w.mesa0_boardtitle.set_active(name_vector)
        self._p.prepare_block = True
        self.a.init_mesa_options(0)
        self._p.prepare_block = False
        self.w.mesa0_parportaddrs.set_text(self.d.mesa0_parportaddrs)
        self.w.mesa0_card_addrs.set_text(self.d.mesa0_card_addrs)

    def mesa0_finish(self):
        model = self.w.mesa0_boardtitle.get_model()
        active = self.w.mesa0_boardtitle.get_active()
        if active < 0:
            title = None
        else: title = model[active][0]
        if not self.d._mesa0_configured:
            self.a.warning_dialog(_("You need to configure the mesa0 page.\n Choose the board type, firmware, component amounts and press 'Accept component changes' button'"),True)
            return True # don't advance page
        if not self.d.mesa0_currentfirmwaredata[self._p._BOARDTITLE] ==  title:
            self.a.warning_dialog(_("The chosen Mesa0 board is different from the current displayed.\nplease press 'Accept component changes' button'"),True)
            return True # don't advance page
        self.d.mesa0_boardtitle = self.w.mesa0_boardtitle.get_active_text()
        self.d.mesa0_parportaddrs = self.w.mesa0_parportaddrs.get_text()
        self.d.mesa0_card_addrs = self.w.mesa0_card_addrs.get_text()
        self.a.mesa_data_transfer(0)
        if self.d.number_mesa <2:
            error = self.a.signal_sanity_check()
            if error: return True # don't advance page
        self.page_set_state('s_motor',self.a.has_spindle_speed_control())

    # mesa page signals for callbacks must be built manually (look in pncconf.py init_mesa_signals() )
    # This is because the page in not inialized/loaded until needed
    # callbacks:
    def on_mesapanel_clicked(self, *args):
        self.t.launch_mesa_panel()

    def on_mesa0_discovery_clicked(self, *args):
        info = self.a.discover_mesacards()
        if not info == None:
            self.a.discovery_selection_update(info,0)

#************
# MESA1 PAGE
#************
    def mesa1_init(self):
        self._p.prepare_block = True
        self.w.mesa1_boardtitle.set_model(self.w.mesa_boardname_store)
        self.a.init_mesa_signals(1)
        self._p.prepare_block = False

    def mesa1_prepare(self):
        self.d.help = "help-mesa.txt"
        temp = 0
        for search,item in enumerate(self._p.MESA_BOARDNAMES):
            if self._p.MESA_BOARDNAMES[search]  == self.d.mesa1_boardtitle:
                temp = search
        self.w.mesa1_boardtitle.set_active(temp)
        self.a.init_mesa_options(1)
        self.w.mesa1_parportaddrs.set_text(self.d.mesa1_parportaddrs)
        self.w.mesa1_card_addrs.set_text(self.d.mesa1_card_addrs)

    def mesa1_finish(self):
        model = self.w.mesa1_boardtitle.get_model()
        active = self.w.mesa1_boardtitle.get_active()
        if active < 0:
            title = None
        else: title = model[active][0]
        if not self.d._mesa1_configured:
            self.a.warning_dialog(_("You need to configure the mesa1 page.\n Choose the board type, firmware, component amounts and press 'Accept component changes' button'"),True)
            return True
        if not self.d.mesa1_currentfirmwaredata[self._p._BOARDTITLE] ==  title:
            self.a.warning_dialog(_("The chosen Mesa1 board is different from the current displayed.\nplease press 'Accept component changes' button'"),True)
            return True
        self.d.mesa1_boardtitle = self.w.mesa1_boardtitle.get_active_text()
        self.d.mesa1_parportaddrs = self.w.mesa1_parportaddrs.get_text()
        self.d.mesa1_card_addrs = self.w.mesa1_card_addrs.get_text()
        self.a.mesa_data_transfer(1)
        error = self.a.signal_sanity_check()
        if error: return True
        self.page_set_state('s_motor',self.a.has_spindle_speed_control())

    # mesa page signals for callbacks must be built manually (look in pncconf.py init_mesa_signals() )
    # This is because the page in not inialized/loaded until needed
    # callbacks:

    def on_mesa1_discovery_clicked(self, *args):
        info = self.a.discover_mesacards()
        if not info == None:
            self.a.discovery_selection_update(info,1)

#************
# pport1 PAGE
#************
    def pport1_init(self):
        self._p.prepare_block = True
        # set Parport tree stores
        for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
            p = 'pp1_Opin%d' % pin
            self.w[p].set_model(self.d._gpioosignaltree)
        for pin in (2,3,4,5,6,7,8,9,10,11,12,13,15):
            p = 'pp1_Ipin%d' % pin
            self.w[p].set_model(self.d._gpioisignaltree)
        for connector in("pp1",):
            # initialize parport input / inv pins
            for pin in (2,3,4,5,6,7,8,9,10,11,12,13,15):
                cb = "%s_Ipin%d"% (connector,pin)
                i = "_%ssignalhandler"% cb
                self.d[i] = int(self.w[cb].connect("changed", self.a.on_general_pin_changed,"parport",connector,"Ipin",None,pin,False))
                i = "_%sactivatehandler"% cb
                self.d[i] = int(self.w[cb].child.connect("activate", self.a.on_general_pin_changed,"parport",connector,"Ipin",None,pin,True))
                self.w[cb].connect('changed', self.a.do_exclusive_inputs,1,cb)
            # initialize parport output / inv pins
            for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
                cb = "%s_Opin%d"% (connector,pin)
                i = "_%ssignalhandler"% cb
                self.d[i] = int(self.w[cb].connect("changed", self.a.on_general_pin_changed,"parport",connector,"Opin",None,pin,False))
                i = "_%sactivatehandler"% cb
                self.d[i] = int(self.w[cb].child.connect("activate", self.a.on_general_pin_changed,"parport",connector,"Opin",None,pin,True))
        self.w.pp1_direction.connect('changed', self.on_pp1_direction_changed)
        self.w.pp1_address_search.connect('clicked', self.on_address_search_clicked)
        self.w.pp1_testbutton.connect('clicked', self.on_pport_panel_clicked)
        self._p.prepare_block = False

    def pport1_prepare(self):
        self.d.help = 5
        c = self.d.pp1_direction
        self._p.prepare_block = True
        for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
            p = 'pp1_Opin%d' % pin
            self.a.set_pport_combo(p)
            p = 'pp1_Opin%d_inv' % pin
            self.w[p].set_active(self.d[p])

        for pin in (2,3,4,5,6,7,8,9,10,11,12,13,15):
            p = 'pp1_Ipin%d' % pin
            self.a.set_pport_combo(p)
            p = 'pp1_Ipin%d_inv' % pin
            self.w[p].set_active(self.d[p])

        # Can't use parport test till all the parport pages are loaded
        if  self.d.number_pports >1:
            state = False
        else:
            state = True
        self.w.pp1_testbutton.set_visible(state)
        self.w.pp1_Opin1.grab_focus()
        self.w.pp1_direction.set_active(self.d.pp1_direction)
        self.on_pp1_direction_changed(self.w.pp1_direction)
        self.w.ioaddr1.set_text(self.d.ioaddr1)
        self._p.prepare_block = False

    def pport1_finish(self):
        #check input pins
        portname = 'pp1'
        for pin in (2,3,4,5,6,7,8,9,10,11,12,13,15):
            direction = "Ipin"         
            pinv = '%s_Ipin%d_inv' % (portname, pin)
            signaltree = self.d._gpioisignaltree
            signaltocheck = self._p.hal_input_names
            p, signal, invert = self.a.pport_push_data(portname,direction,pin,pinv,signaltree,signaltocheck)
            self.d[p] = signal
            self.d[pinv] = invert
        # check output pins
        for pin in (1,2,3,4,5,6,7,8,9,14,16,17):           
            direction = "Opin"
            pinv = '%s_Opin%d_inv' % (portname, pin)
            signaltree = self.d._gpioosignaltree
            signaltocheck = self._p.hal_output_names
            p, signal, invert = self.a.pport_push_data(portname,direction,pin,pinv,signaltree,signaltocheck)
            self.d[p] = signal
            self.d[pinv] = invert
        self.d.pp1_direction = self.w.pp1_direction.get_active()
        self.d.ioaddr1 = self.w.ioaddr1.get_text()
        self.page_set_state('s_motor',self.a.has_spindle_speed_control())

    # pport1 callbacks:
    def on_pp1_direction_changed(self,widget):
        state = widget.get_active()
        for i in (2,3,4,5,6,7,8,9):
            self.w['pp1_Ipin%s_in_box'%i].set_visible(state)
            self.w['pp1_Opin%s_out_box'%i].set_visible(not state)

    def on_pport_panel_clicked(self, *args):self.t.parporttest(self)

#************
# pport2 PAGE
#************
    def pport2_init(self):
        # set Parport tree stores
        for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
            p = 'pp2_Opin%d' % pin
            self.w[p].set_model(self.d._gpioosignaltree)
        for pin in (2,3,4,5,6,7,8,9,10,11,12,13,15):
            p = 'pp2_Ipin%d' % pin
            self.w[p].set_model(self.d._gpioisignaltree)
        for connector in("pp2",):
            # initialize parport input / inv pins
            for pin in (2,3,4,5,6,7,8,9,10,11,12,13,15):
                cb = "%s_Ipin%d"% (connector,pin)
                i = "_%ssignalhandler"% cb
                self.d[i] = int(self.w[cb].connect("changed", self.a.on_general_pin_changed,"parport",connector,"Ipin",None,pin,False))
                i = "_%sactivatehandler"% cb
                self.d[i] = int(self.w[cb].child.connect("activate", self.a.on_general_pin_changed,"parport",connector,"Ipin",None,pin,True))
                self.w[cb].connect('changed', self.a.do_exclusive_inputs,2,cb)
            # initialize parport output / inv pins
            for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
                cb = "%s_Opin%d"% (connector,pin)
                i = "_%ssignalhandler"% cb
                self.d[i] = int(self.w[cb].connect("changed", self.a.on_general_pin_changed,"parport",connector,"Opin",None,pin,False))
                i = "_%sactivatehandler"% cb
                self.d[i] = int(self.w[cb].child.connect("activate", self.a.on_general_pin_changed,"parport",connector,"Opin",None,pin,True))
        self.w.pp2_direction.connect('changed', self.on_pp2_direction_changed)
        self.w.pp2_address_search.connect('clicked', self.on_address_search_clicked)
        self.w.pp2_testbutton.connect('clicked', self.on_pport_panel_clicked)

    def pport2_prepare(self):
        self.d.help = 5
        c = self.d.pp2_direction
        self._p.prepare_block = True
        for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
            p = 'pp2_Opin%d' % pin
            self.a.set_pport_combo(p)
            p = 'pp2_Opin%d_inv' % pin
            self.w[p].set_active(self.d[p])

        for pin in (2,3,4,5,6,7,8,9,10,11,12,13,15):
            p = 'pp2_Ipin%d' % pin
            self.a.set_pport_combo(p)
            p = 'pp2_Ipin%d_inv' % pin
            self.w[p].set_active(self.d[p])

        self.w.pp2_Opin1.grab_focus()
        self.w.pp2_direction.set_active(self.d.pp2_direction)
        self.on_pp2_direction_changed(self.w.pp2_direction)
        self.w.ioaddr2.set_text(self.d.ioaddr2)
        self._p.prepare_block = False

    def pport2_finish(self):
        #check input pins
        portname = 'pp2'
        for pin in (2,3,4,5,6,7,8,9,10,11,12,13,15):
            direction = "Ipin"         
            pinv = '%s_Ipin%d_inv' % (portname, pin)
            signaltree = self.d._gpioisignaltree
            signaltocheck = self._p.hal_input_names
            p, signal, invert = self.a.pport_push_data(portname,direction,pin,pinv,signaltree,signaltocheck)
            self.d[p] = signal
            self.d[pinv] = invert
        # check output pins
        for pin in (1,2,3,4,5,6,7,8,9,14,16,17):           
            direction = "Opin"
            pinv = '%s_Opin%d_inv' % (portname, pin)
            signaltree = self.d._gpioosignaltree
            signaltocheck = self._p.hal_output_names
            p, signal, invert = self.a.pport_push_data(portname,direction,pin,pinv,signaltree,signaltocheck)
            self.d[p] = signal
            self.d[pinv] = invert
        self.d.pp2_direction = self.w.pp2_direction.get_active()
        self.d.ioaddr2 = self.w.ioaddr2.get_text()
        self.page_set_state('s_motor',self.a.has_spindle_speed_control())

    # pport2 callbacks:
    def on_pp2_direction_changed(self,widget):
        state = widget.get_active()
        for i in (2,3,4,5,6,7,8,9):
            self.w['pp2_Ipin%s_in_box'%i].set_visible(state)
            self.w['pp2_Opin%s_out_box'%i].set_visible(not state)

    def on_parportpanel_clicked(self, *args):self.t.parporttest(self)
#************
# X_MOTOR PAGE
#************
    def x_motor_prepare(self):
        #self.w.xencoderscale.modify_bg(gtk.STATE_NORMAL, self.w.xencoderscale.get_colormap().alloc_color("red"))
        self.d.help = "help-axismotor.txt"
        self.a.axis_prepare('x')
        state = True
        if not self.a.pwmgen_sig('x'): # if not servo control we can de-sensitze PID data
            state = self.d.advanced_option
        self.w.xservo_info.set_sensitive(state)
    def x_motor_finish(self):
        self.a.axis_done('x')
    # callbacks
    def on_xcalculatescale_clicked(self, *args): self.a.calculate_scale('x')
    def on_xaxistest_clicked(self, *args): self.t.test_axis('x')
    def on_xaxistune_clicked(self, *args): self.t.tune_axis('x')
    def on_xaxis_prepare(self, *args): self.a.axis_prepare('x')
#************
# X_AXIS PAGE
#************
    def x_axis_prepare(self):
        self.d.help = "help-axisconfig.txt"
    def x_axis_finish(self):
        self.a.axis_done('x')
#************
# Y_MOTOR PAGE
#************
    def y_motor_prepare(self):
        self.d.help = "help-axismotor.txt"
        self.a.axis_prepare('y')
        state = True
        if not self.a.pwmgen_sig('y'):
            state = self.d.advanced_option
        self.w.yservo_info.set_sensitive(state)
    def y_motor_finish(self):
        pass
    # callbacks
    def on_ycalculatescale_clicked(self, *args): self.a.calculate_scale('y')
    def on_yaxistest_clicked(self, *args): self.t.test_axis('y')
    def on_yaxistune_clicked(self, *args): self.t.tune_axis('y')
    def on_yaxis_prepare(self, *args): self.a.axis_prepare('y')
#************
# Y_AXIS PAGE
#************
    def y_axis_prepare(self):
        self.d.help = "help-axisconfig.txt"
    def y_axis_finish(self):
        self.a.axis_done('y')
#************
# Z_MOTOR PAGE
#************
    def z_motor_prepare(self):
        self.d.help = "help-axismotor.txt"
        self.a.axis_prepare('z')
        state = True
        if not self.a.pwmgen_sig('z'):
            state = self.d.advanced_option
        self.w.zservo_info.set_sensitive(state)
    def z_motor_finish(self):
        self.a.axis_done('z')
    # callbacks
    def on_zcalculatescale_clicked(self, *args): self.a.calculate_scale('z')
    def on_zaxistest_clicked(self, *args): self.t.test_axis('z')
    def on_zaxistune_clicked(self, *args): self.t.tune_axis('z')
    def on_zaxis_prepare(self, *args): self.a.axis_prepare('z')
#************
# Z_AXIS PAGE
#************
    def z_axis_prepare(self):
        self.d.help = "help-axisconfig.txt"
    def z_axis_finish(self):
        self.a.axis_done('z')
#************
# A_MOTOR PAGE
#************
    def a_motor_prepare(self):
        self.d.help = "help-axismotor.txt"
        self.a.axis_prepare('a')
        state = True
        if not self.a.pwmgen_sig('a'):
            state = self.d.advanced_option
        self.w.aservo_info.set_sensitive(state)
    def a_motor_finish(self):
        self.a.axis_done('a')
    # callbacks
    def on_acalculatescale_clicked(self, *args): self.a.calculate_scale('a')
    def on_aaxistest_clicked(self, *args): self.t.test_axis('a')
    def on_aaxistune_clicked(self, *args): self.t.tune_axis('a')
    def on_aaxis_prepare(self, *args): self.a.axis_prepare('a')
#************
# A_AXIS PAGE
#************
    def a_axis_prepare(self):
        self.d.help = "help-axisconfig.txt"
    def a_axis_finish(self):
        self.a.axis_done('a')
#************
# Spindle PAGE
#************
    def s_motor_prepare(self):
        self.a.axis_prepare('s')
        self.a.useatspeed_toggled()
        self.d.help = "help-axismotor.txt"
    def s_motor_finish(self):
        self.a.axis_done('s')
    # callbacks
    def on_scalculatescale_clicked(self, *args): self.a.calculate_spindle_scale()
    def on_saxistest_clicked(self, *args): self.t.test_axis('s')
    def on_saxistune_clicked(self, *args): self.t.tune_axis('s')
    def on_saxis_prepare(self, *args): self.a.axis_prepare('s')
    def on_suseatspeed_toggled(self,widget): self.a.useatspeed_toggled()
    def on_suseoutputrange2_toggled(self,widget): self.a.useoutputrange2_toggled()
#************
# Options PAGE
#************
    def options_prepare(self):      
        self.d.help = "help-advanced.txt"
        self.w.classicladder.set_active(self.d.classicladder)
        self.w.modbus.set_active(self.d.modbus)
        self.w.digitsin.set_value(self.d.digitsin)
        self.w.digitsout.set_value(self.d.digitsout)
        self.w.s32in.set_value(self.d.s32in)
        self.w.s32out.set_value(self.d.s32out)
        self.w.floatsin.set_value(self.d.floatsin)
        self.w.floatsout.set_value(self.d.floatsout)
        self.w.bitmem.set_value(self.d.bitmem)
        self.w.wordmem.set_value(self.d.wordmem)
        self.w.halui.set_active(self.d.halui)
        self.w.ladderexist.set_active(self.d.ladderexist)
        self.w.laddertouchz.set_active(self.d.laddertouchz)
        self.on_halui_toggled()
        for i in range(0,15):
            self.w["halui_cmd"+str(i)].set_text(self.d["halui_cmd"+str(i)])  
        self.w.ladderconnect.set_active(self.d.ladderconnect)      
        self.on_classicladder_toggled()

    def options_finish(self):
        self.d.classicladder = self.w.classicladder.get_active()
        self.d.modbus = self.w.modbus.get_active()
        self.d.digitsin = self.w.digitsin.get_value()
        self.d.digitsout = self.w.digitsout.get_value()
        self.d.s32in = self.w.s32in.get_value()
        self.d.s32out = self.w.s32out.get_value()
        self.d.floatsin = self.w.floatsin.get_value()
        self.d.bitmem = self.w.bitmem.get_value()
        self.d.wordmem = self.w.wordmem.get_value()
        self.d.floatsout = self.w.floatsout.get_value()
        self.d.halui = self.w.halui.get_active()
        self.d.ladderexist = self.w.ladderexist.get_active()
        self.d.laddertouchz = self.w.laddertouchz.get_active()
        for i in range(0,15):
            self.d["halui_cmd"+str(i)] = self.w["halui_cmd"+str(i)].get_text()         
        self.d.ladderconnect = self.w.ladderconnect.get_active()          
        if self.d.classicladder:
            if self.w.ladderblank.get_active() == True:
                if self.d.tempexists:
                    self.d.laddername='TEMP.clp'
                else:
                    self.d.laddername= 'blank.clp'
                    self.d.ladderhaltype = 0
            if self.w.ladder1.get_active() == True:
                self.d.laddername = 'estop.clp'
                has_estop = self.d.findsignal("estop-ext")
                if not has_estop:
                    self.a.warning_dialog(_("You need to designate an E-stop input pin for this ladder program."),True)
                    return True
                self.d.ladderhaltype = 1
            if self.w.ladder2.get_active() == True:
                self.d.laddername = 'serialmodbus.clp'
                self.d.modbus = 1
                self.w.modbus.set_active(self.d.modbus) 
                self.d.ladderhaltype = 0
            if self.w.laddertouchz.get_active() == True:
                has_probe = self.d.findsignal("probe-in")
                if not has_probe:
                    self.a.warning_dialog(_("You need to designate a probe input pin for this ladder program."),True)
                    return True
                self.d.ladderhaltype = 2
                self.d.laddername = 'touchoff_z.clp'
                self.d.halui = True
                self.w.halui.set_active(True)
            if self.w.ladderexist.get_active() == True:
                self.d.laddername='custom.clp'
            else:
                if os.path.exists(os.path.expanduser("~/linuxcnc/configs/%s/custom.clp" % self.d.machinename)):
                    if not self.a.warning_dialog(_("OK to replace existing custom ladder program?\nExisting\
 Custom.clp will be renamed custom_backup.clp.\nAny existing file named -custom_backup.clp- will be lost.\
Selecting 'existing ladder program' will avoid this warning"),False):
                        return True 
            if self.w.ladderexist.get_active() == False:
                if os.path.exists(os.path.join(self._p.DISTDIR, "configurable_options/ladder/TEMP.clp")):
                    if not self.a.warning_dialog(_("You edited a ladder program and have selected a \
different program to copy to your configuration file.\nThe edited program will be lost.\n\nAre you sure?  "),False):
                        return True

#************
# REALTIME PAGE
#************
    def realtime_prepare(self):
        self.d.help = "help-realtime.txt"
        self.w.userneededpid.set_value(self.d.userneededpid)
        self.w.userneededabs.set_value(self.d.userneededabs)
        self.w.userneededscale.set_value(self.d.userneededscale)
        self.w.userneededmux16.set_value(self.d.userneededmux16)
        self.w.userneededlowpass.set_value(self.d.userneededlowpass)

        if not self.d._components_is_prepared:
            textbuffer = self.w.loadcompservo.get_buffer()
            for i in self.d.loadcompservo:
                if i == '': continue
                textbuffer.insert_at_cursor(i+"\n" )
            textbuffer = self.w.addcompservo.get_buffer()
            for i in self.d.addcompservo:
                if i == '': continue
                textbuffer.insert_at_cursor(i+"\n" )
            textbuffer = self.w.loadcompbase.get_buffer()
            for i in self.d.loadcompbase:
                if i == '': continue
                textbuffer.insert_at_cursor(i+"\n" )
            textbuffer = self.w.addcompbase.get_buffer()
            for i in self.d.addcompbase:
                if i == '': continue
                textbuffer.insert_at_cursor(i+"\n" )
            self.d._components_is_prepared = True

    def realtime_finish(self):
        self.d.userneededpid = int(self.w.userneededpid.get_value())
        self.d.userneededabs = int(self.w.userneededabs.get_value())
        self.d.userneededscale = int(self.w.userneededscale.get_value())
        self.d.userneededmux16 = int(self.w.userneededmux16.get_value())
        self.d.userneededlowpass = int(self.w.userneededlowpass.get_value())

        textbuffer = self.w.loadcompservo.get_buffer()
        startiter = textbuffer.get_start_iter()
        enditer = textbuffer.get_end_iter()
        test = textbuffer.get_text(startiter,enditer)
        i = test.split('\n')
        self.d.loadcompservo = i
        textbuffer = self.w.addcompservo.get_buffer()
        startiter = textbuffer.get_start_iter()
        enditer = textbuffer.get_end_iter()
        test = textbuffer.get_text(startiter,enditer)
        i = test.split('\n')
        self.d.addcompservo = i
        textbuffer = self.w.loadcompbase.get_buffer()
        startiter = textbuffer.get_start_iter()
        enditer = textbuffer.get_end_iter()
        test = textbuffer.get_text(startiter,enditer)
        i = test.split('\n')
        self.d.loadcompbase = i
        textbuffer = self.w.addcompbase.get_buffer()
        startiter = textbuffer.get_start_iter()
        enditer = textbuffer.get_end_iter()
        test = textbuffer.get_text(startiter,enditer)
        i = test.split('\n')
        self.d.addcompbase = i
#*************
# FINISH PAGE
#*************
    def finished_prepare(self):
        pass
    def finished_finish(self):
        self.a.clean_unused_ports()
        self.a.buid_config()
#**************
# tune test
#**************
    # callbacks
    def on_update_tune_params(self, *args):
        self.t.update_tune_test_params()
    def on_tune_jogminus_pressed(self, w):
        self.t.tune_jogminus(1)
    def on_tune_jogminus_released(self, w):
        self.t.tune_jogminus(0)
    def on_tune_jogplus_pressed(self, w):
        self.t.tune_jogplus(1)
    def on_tune_jogplus_released(self, w):
        self.t.tune_jogplus(0)
    def on_tuneinvertmotor_toggled(self,w):
        self.t.toggle_tuneinvertmotor()
#**************
# openloop test
#**************
    # callbacks
    def update_oloop_params(self, w):
        self.t.update_axis_params()
    def on_oloop_enableamp_toggled(self, w):
        self.t.oloop_enableamp()
    def on_oloop_jogminus_pressed(self, w):
        self.t.oloop_jogminus(1)
    def on_oloop_jogminus_released(self, w):
        self.t.oloop_jogminus(0)
    def on_oloop_jogplus_pressed(self, w):
        self.t.oloop_jogplus(1)
    def on_oloop_jogplus_released(self, w):
        self.t.oloop_jogplus(0)
    def on_resetbutton_pressed(self, w):
        self.t.oloop_resetencoder(1)
    def on_resetbutton_released(self, w):
        self.t.oloop_resetencoder(0)

# BOILER CODE
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)
