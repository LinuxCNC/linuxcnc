#!/usr/bin/env python
# -*- encoding: utf-8 -*-
#
#    This is stepconf, a graphical configuration editor for LinuxCNC
#    Copyright 2007 Jeff Epler <jepler@unpythonic.net>
#    stepconf 1.1 revamped by Chris Morley 2014
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
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# This presents and collects the data from the GUI pages
#
# To add pages:
# add the glade file to directory. the first container name will be the reference name (delete the window)
# add the reference name, Text name and active state to private data variable: self.available_page of stepconnf.py
# add function call here: <reference name>_prepare() and <reference name>_finish()
# add GLADE callbacks for the page here.
# add large or common function calls to stepconf.py

import gtk
import os
import gobject

class Pages:
    def __init__(self, app):
        self.d = app.d      # collected data
        self.w = app.w      # widget names
        self.a = app        # parent, stepconf
        self._p = app._p    # private data
        global debug
        debug = self.a.debug
        global dbg
        dbg = self.a.dbg

#********************
# Notebook Controls
#********************
    def on_window1_destroy(self, *args):
        if self.a.warning_dialog (self._p.MESS_ABORT,False):
            gtk.main_quit()
            return True
        else:
            return False

    # seaches (self._p.available_page) from the current page forward,
    # for the next page that is True or till second-to-last page.
    # if state found True: call current page finish function.
    # If that returns False then call the next page prepare function and show page
    def on_button_fwd_clicked(self,widget):
        cur = self.w.notebook1.get_current_page()
        u = cur+1
        cur_name,cur_text,cur_state = self._p.available_page[cur]
        while u < len(self._p.available_page):
            name,text,state = self._p.available_page[u]
            dbg( "FWD search %s,%s,%s,%s,of %d pages"%(u,name,text,state,len(self._p.available_page)-1))
            if state:
                if not self['%s_finish'%cur_name]():
                    self.w.notebook1.set_current_page(u)
                    self['%s_prepare'%name]()
                    self.w.title_label.set_text(text)
                    dbg("set %d current"%u)
                break
            u +=1
        # second-to-last page? change the fwd button to finish and show icon
        if u == len(self._p.available_page)-1:
            self.w.apply_image.set_visible(True)
            self.w.label_fwd.set_text(self._p.MESS_DONE)
        # last page? nothing to prepare just finish
        elif u == len(self._p.available_page):
            name,text,state = self._p.available_page[cur]
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
        cur_name,cur_text,cur_state = self._p.available_page[cur]
        while u > -1:
            name,text,state = self._p.available_page[u]
            dbg( "BACK search %s,%s,%s,%s,of %d pages"%(u,name,text,state,len(self._p.available_page)-1))
            if state:
                if not cur == len(self._p.available_page)-1:
                    self['%s_finish'%cur_name]()
                self.w.notebook1.set_current_page(u)
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

    def set_buttons_sensitive(self,fstate,bstate):
        self.w.button_fwd.set_sensitive(fstate)
        self.w.button_back.set_sensitive(bstate)

    # Sets the visual state of a list of page(s)
    # The page names must be the one used in self._p.available_page
    # If a pages state is false it won't be seen or it's functions called.
    # if you deselect the current page it will show till next time it is cycled
    def page_set_state(self,page_list,state):
        dbg("page_set_state() %s ,%s"%(page_list,state))
        for i,data in enumerate(self._p.available_page):
            name,text,curstate = data
            if name in page_list:
                self._p.available_page[i][2] = state
                dbg("State changed to %s"% state)
                break

#####################################################
# All Page Methods
#####################################################
#***************
# Intialize
#***************
    def initialize(self):
        # one time initialized data
        for i in self._p.alldrivertypes:
            self.w.drivertype.append_text(i[1])
        self.w.drivertype.append_text(_("Other"))
        self.w.title_label.set_text(self._p.available_page[0][1])
        self.w.button_back.set_sensitive(False)
        self.w.label_fwd.set_text(self._p.MESS_START)
        if debug:
            self.w.window1.set_title('Stepconf -debug mode')
        # pport1 combo boxes
        model = self.w.output_list
        model.clear()
        for name in self._p.human_output_names: model.append((name,))
        model = self.w.input_list
        model.clear()
        for name in self._p.human_input_names: model.append((name,))
        # pport2 comboboxes
        model = self.w.pp2_output_list
        model.clear()
        for ind,name in enumerate(self._p.human_output_names):
            if not ind in( 0,1,2,3,4,5,6,7):
                model.append((name,))
        model = self.w.pp2_input_list
        model.clear()
        for name in self._p.human_input_names: model.append((name,))
        self.intro_prepare()

#************
# INTRO PAGE
#************
    def intro_prepare(self):
        pass
    def intro_finish(self):
        pass

#***********
# start PAGE
#***********
    def start_prepare(self):
        self.w.createsymlink.set_active(self.d.createsymlink)
        self.w.createshortcut.set_active(self.d.createshortcut)
        self.w.createsimconfig.set_active(self.d.sim_hardware)

    def start_finish(self):
        if not self.w.createconfig.get_active():
            filter = gtk.FileFilter()
            filter.add_pattern("*.stepconf")
            filter.set_name(_("Machinekit 'stepconf' configuration files"))
            dialog = gtk.FileChooserDialog(_("Modify Existing Configuration"),
                self.w.window1, gtk.FILE_CHOOSER_ACTION_OPEN,
                (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL,
                 gtk.STOCK_OPEN, gtk.RESPONSE_OK))
            dialog.set_default_response(gtk.RESPONSE_OK)
            dialog.add_filter(filter)
            if not self.d._lastconfigname == "" and self.d._chooselastconfig:
                dialog.set_filename(os.path.expanduser("~/machinekit/configs/%s.stepconf"% self.d._lastconfigname))
            dialog.add_shortcut_folder(os.path.expanduser("~/machinekit/configs"))
            dialog.set_current_folder(os.path.expanduser("~/machinekit/configs"))
            dialog.show_all()
            result = dialog.run()
            if result == gtk.RESPONSE_OK:
                filename = dialog.get_filename()
                dialog.destroy()
                self.d.load(filename, self)
            else:
                dialog.destroy()
                return True
        self.d.createsymlink = self.w.createsymlink.get_active()
        self.d.createshortcut = self.w.createshortcut.get_active()
        self.d.sim_hardware = self.w.createsimconfig.get_active()

    # callbacks
    def on_machinename_changed(self, *args):
        temp = self.w.machinename.get_text()
        self.w.confdir.set_text("~/machinekit/configs/%s" % temp.replace(" ","_"))
    def on_drivertype_changed(self, *args):
        self.a.update_drivertype_info()

#************
# BASIC PAGE
#************
    def base_prepare(self):
        self.w.drivetime_expander.set_expanded(True)
        self.w.machinename.set_text(self.d.machinename)
        self.w.axes.set_active(self.d.axes)
        self.w.units.set_active(self.d.units)
        self.w.latency.set_value(self.d.latency)
        self.w.steptime.set_value(self.d.steptime)
        self.w.stepspace.set_value(self.d.stepspace)
        self.w.dirsetup.set_value(self.d.dirsetup)
        self.w.dirhold.set_value(self.d.dirhold)
        self.w.drivertype.set_active(self.a.drivertype_toindex())
        self.w.ioaddr.set_text(self.d.ioaddr)
        self.w.machinename.grab_focus()
        self.w.ioaddr2.set_text(self.d.ioaddr2) 
        #self.w.ioaddr3.set_text(self.d.ioaddr3)
        #self.w.pp3_direction.set_active(self.d.pp3_direction)
        if self.d.number_pports>2:
             self.w.radio_pp3.set_active(True)
        elif self.d.number_pports>1:
             self.w.radio_pp2.set_active(True)
        else:
             self.w.radio_pp1.set_active(True)

    def base_finish(self):
        self.w.drivetime_expander.set_expanded(False)
        machinename = self.w.machinename.get_text()
        self.d.machinename = machinename.replace(" ","_")
        self.d.axes = self.w.axes.get_active()
        self.d.units = self.w.units.get_active()
        self.d.drivertype = self.a.drivertype_toid(self.w.drivertype.get_active())
        self.d.steptime = self.w.steptime.get_value()
        self.d.stepspace = self.w.stepspace.get_value()
        self.d.dirsetup = self.w.dirsetup.get_value()
        self.d.dirhold = self.w.dirhold.get_value()
        self.d.latency = self.w.latency.get_value()
        if self.w.radio_pp3.get_active() and self.w.radio_pp2.get_active():
            self.d.number_pports = 3
        elif self.w.radio_pp2.get_active():
            self.d.number_pports = 2
        else:
            self.d.number_pports = 1
        self.page_set_state('pport2',self.w.radio_pp2.get_active())
        dbg("active axes: %s = %d"% (self.w.axes.get_active_text(),self.d.axes))
        self.page_set_state('axisz','Z' in self.w.axes.get_active_text())
        self.page_set_state('axisy','Y' in self.w.axes.get_active_text())
        self.page_set_state('axisa','A' in self.w.axes.get_active_text())

    # Basic page callbacks
    def on_pp2_checkbutton_toggled(self, *args): 
        i = self.w.pp2_checkbutton.get_active()   
        self.w.pp2_direction.set_sensitive(i)
        self.w.ioaddr2.set_sensitive(i)
        if i == 0:
           self.w.pp3_checkbutton.set_active(i)
           self.w.ioaddr3.set_sensitive(i)

    def on_pp3_checkbutton_toggled(self, *args): 
        i = self.w.pp3_checkbutton.get_active() 
        if self.w.pp2_checkbutton.get_active() ==0:
          i=0  
          self.w.pp3_checkbutton.set_active(0)
        self.w.pp3_direction.set_sensitive(i)
        self.w.ioaddr3.set_sensitive(i)

    def on_latency_test_clicked(self,widget):
        self.a.run_latency_test()

    def on_calculate_ideal_period(self,widget):
        self.a.calculate_ideal_period()

    def on_units_changed(self,widget):
        if not self.d.units == widget.get_active():
            # change the XYZ axis defaults to metric or imperial
            # This erases any entered data that would make sense to change
            self.d.set_axis_unit_defaults(not widget.get_active())

#***************
# options PAGE
#***************
    def options_prepare(self):
        self.w.pyvcp.set_active(self.d.pyvcp)
        self.on_pyvcp_toggled()
        if  not self.w.createconfig.get_active():
           if os.path.exists(os.path.expanduser("~/machinekit/configs/%s/custompanel.xml" % self.d.machinename)):
                self.w.radiobutton8.set_active(True)
        self.w.classicladder.set_active(self.d.classicladder)
        self.w.modbus.set_active(self.d.modbus)
        self.w.digitsin.set_value(self.d.digitsin)
        self.w.digitsout.set_value(self.d.digitsout)
        self.w.s32in.set_value(self.d.s32in)
        self.w.s32out.set_value(self.d.s32out)
        self.w.floatsin.set_value(self.d.floatsin)
        self.w.floatsout.set_value(self.d.floatsout)
        self.w.halui.set_active(self.d.halui)
        self.w.ladderconnect.set_active(self.d.ladderconnect)
        self.w.pyvcpconnect.set_active(self.d.pyvcpconnect)
        self.on_classicladder_toggled()
        self.w.manualtoolchange.set_active(self.d.manualtoolchange)
        if  not self.w.createconfig.get_active():
           if os.path.exists(os.path.expanduser("~/machinekit/configs/%s/custom.clp" % self.d.machinename)):
                self.w.radiobutton4.set_active(True)

    def options_finish(self):
        SIG = self._p
        self.d.pyvcp = self.w.pyvcp.get_active()
        self.d.classicladder = self.w.classicladder.get_active()
        self.d.modbus = self.w.modbus.get_active()
        self.d.digitsin = self.w.digitsin.get_value()
        self.d.digitsout = self.w.digitsout.get_value()
        self.d.s32in = self.w.s32in.get_value()
        self.d.s32out = self.w.s32out.get_value()
        self.d.floatsin = self.w.floatsin.get_value()
        self.d.floatsout = self.w.floatsout.get_value()
        self.d.halui = self.w.halui.get_active()    
        self.d.pyvcpconnect = self.w.pyvcpconnect.get_active()  
        self.d.ladderconnect = self.w.ladderconnect.get_active()   
        self.d.manualtoolchange = self.w.manualtoolchange.get_active()       
        if self.d.classicladder:
           if self.w.radiobutton1.get_active() == True:
              if self.d.tempexists:
                   self.d.laddername='TEMP.clp'
              else:
                   self.d.laddername= 'blank.clp'
                   self.d.ladderhaltype = 0
           if self.w.radiobutton2.get_active() == True:
              self.d.laddername = 'estop.clp'
              inputs = self.a.build_input_set()
              if SIG.ESTOP_IN not in inputs:
                 self.a.warning_dialog(self._p.MESS_NO_ESTOP,True)
                 return True # don't advance the page
              self.d.ladderhaltype = 1
           if self.w.radiobutton3.get_active() == True:
                 self.d.laddername = 'serialmodbus.clp'
                 self.d.modbus = 1
                 self.w.modbus.set_active(self.d.modbus) 
                 self.d.ladderhaltype = 0          
           if self.w.radiobutton4.get_active() == True:
              self.d.laddername='custom.clp'
           else:
               if os.path.exists(os.path.expanduser("~/machinekit/configs/%s/custom.clp" % self.d.machinename)):
                  if not self.a.warning_dialog(self._p.MESS_CL_REWRITE,False):
                     return True # don't advance the page
           if self.w.radiobutton1.get_active() == False:
              if os.path.exists(os.path.join(self._p.distdir, "configurable_options/ladder/TEMP.clp")):
                 if not self.a.warning_dialog(self._p.MESS_CL_EDITTED,False):
                   return True # don't advance the page
        if self.d.pyvcp == True:
           if self.w.radiobutton5.get_active() == True:
              self.d.pyvcpname = "blank.xml"
              self.pyvcphaltype = 0
           if self.w.radiobutton6.get_active() == True:
              self.d.pyvcpname = "spindle.xml"
              self.d.pyvcphaltype = 1
           if self.w.radiobutton8.get_active() == True:
              self.d.pyvcpname = "custompanel.xml"
           else:
              if os.path.exists(os.path.expanduser("~/machinekit/configs/%s/custompanel.xml" % self.d.machinename)):
                 if not self.a.warning_dialog(self._p.MESS_PYVCP_REWRITE,False):
                   return True

    # options page callback
    def on_loadladder_clicked(self, *args):
        self.a.load_ladder(self)

    def on_display_pyvcp_clicked(self,*args):
        self.a.testpanel(self)

    def on_classicladder_toggled(self, *args):
        i= self.w.classicladder.get_active()
        self.w.ladder_box.set_sensitive(i)
        if  self.w.createconfig.get_active():
            self.w.radiobutton4.set_sensitive(False)
        else:
            self.w.radiobutton4.set_sensitive(i)
        if not i:
            self.w.clpins_expander.set_expanded(False)

    def on_pyvcp_toggled(self,*args):
        i= self.w.pyvcp.get_active()
        if  self.w.createconfig.get_active():
            self.w.radiobutton8.set_sensitive(False)
        else:
            self.w.radiobutton8.set_sensitive(i)
        self.w.pyvcp_box.set_sensitive(i)

#************
# pport1 PAGE
#************
    def pport1_prepare(self):
        self._p.in_pport_prepare = True
        for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
            p = 'pin%d' % pin
            self.w[p].set_wrap_width(3)
            self.w[p].set_active(self._p.hal_output_names.index(self.d[p]))
            p = 'pin%dinv' % pin
            self.w[p].set_active(self.d[p])
        for pin in (10,11,12,13,15):
            p = 'pin%d' % pin
            self.w[p].set_wrap_width(3)
            self.w[p].set_active(self._p.hal_input_names.index(self.d[p]))
            p = 'pin%dinv' % pin
            self.w[p].set_active(self.d[p])
        self.w.pin1.grab_focus()
        self.w.ioaddr.set_text(self.d.ioaddr)
        self._p.in_pport_prepare = False

    def pport1_finish(self):
        for pin in (10,11,12,13,15):
            p = 'pin%d' % pin
            self.d[p] = self._p.hal_input_names[self.w[p].get_active()]
        for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
            p = 'pin%d' % pin
            self.d[p] = self._p.hal_output_names[self.w[p].get_active()]
        for pin in (1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17):
            p = 'pin%dinv' % pin
            self.d[p] = self.w[p].get_active()
        self.d.ioaddr = self.w.ioaddr.get_text()
        self.page_set_state('spindle',self.a.has_spindle_speed_control())

    # pport1 callbacks
    def on_exclusive_check_pp1(self, widget):
        self.a.do_exclusive_inputs(widget,1)

    def on_preselect_button_clicked(self, widget):
        state = self.w.preset_combo.get_active()
        print state
        if state == 0:
            self.a.preset_sherline_outputs()
        elif state ==1:
            self.a.preset_xylotex_outputs()
        elif state ==2:
            self.a.preset_tb6560_3axes_outputs()
        elif state ==3:
            self.a.preset_tb6560_4axes_outputs()

#************
# pport2 PAGE
#************
    def pport2_prepare(self):
        self._p.in_pport_prepare = True
        for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
            p = 'pp2_pin%d' % pin
            self.w[p].set_wrap_width(3)
            self.w[p].set_active(self._p.hal_output_names.index(self.d[p])-8)
            p = 'pp2_pin%dinv' % pin
            self.w[p].set_active(self.d[p])

        for pin in (2,3,4,5,6,7,8,9,10,11,12,13,15):
            p = 'pp2_pin%d_in' % pin
            self.w[p].set_wrap_width(3)
            self.w[p].set_active(self._p.hal_input_names.index(self.d[p]))
            p = 'pp2_pin%d_in_inv' % pin
            self.w[p].set_active(self.d[p])
        self.w.pp2_pin1.grab_focus()
        self.w.pp2_direction.set_active(self.d.pp2_direction)
        self.on_pp2_direction_changed(self.w.pp2_direction)
        self.w.ioaddr2.set_text(self.d.ioaddr2)
        self._p.in_pport_prepare = False

    def pport2_finish(self):
        for pin in (1,2,3,4,5,6,7,8,9,14,16,17):
            p = 'pp2_pin%d' % pin
            self.d[p] = self._p.hal_output_names[self.w[p].get_active()+8]
            p = 'pp2_pin%dinv' % pin
            self.d[p] = self.w[p].get_active()
        for pin in (2,3,4,5,6,7,8,9,10,11,12,13,15):
            p = 'pp2_pin%d_in' % pin
            self.d[p] = self._p.hal_input_names[self.w[p].get_active()]
            p = 'pp2_pin%d_in_inv' % pin
            self.d[p] = self.w[p].get_active()
        self.d.pp2_direction = self.w.pp2_direction.get_active()
        self.d.ioaddr2 = self.w.ioaddr2.get_text()

    # pport2 callbacks:
    def on_pp2_direction_changed(self,widget):
        state = widget.get_active()
        for i in (2,3,4,5,6,7,8,9):
            self.w['pp2_pin%s_in_box'%i].set_visible(state)
            self.w['pp2_pin%s_out_box'%i].set_visible(not state)

    def on_exclusive_check_pp2(self, widget):
        self.a.do_exclusive_inputs(widget,2)


#*******************
# AXIS X PAGE
#*******************
    def axisx_prepare(self):
        self.axis_prepare('x')
    def axisx_finish(self):
        self.axis_done('x')
    # AXIS X callbacks
    def on_xsteprev_changed(self, *args): self.a.update_pps('x')
    def on_xmicrostep_changed(self, *args): self.a.update_pps('x')
    def on_xpulleyden_changed(self, *args): self.a.update_pps('x')
    def on_xpulleynum_changed(self, *args): self.a.update_pps('x')
    def on_xleadscrew_changed(self, *args): self.a.update_pps('x')
    def on_xmaxvel_changed(self, *args): self.a.update_pps('x')
    def on_xmaxacc_changed(self, *args): self.a.update_pps('x')
    def on_xaxistest_clicked(self, *args): self.a.test_axis('x')

#********************
# AXIS Y
#********************
    def axisy_prepare(self):
        self.axis_prepare('y')
    def axisy_finish(self):
        self.axis_done('y')
    # AXIS Y callbacks
    def on_ysteprev_changed(self, *args): self.a.update_pps('y')
    def on_ymicrostep_changed(self, *args): self.a.update_pps('y')
    def on_ypulleyden_changed(self, *args): self.a.update_pps('y')
    def on_ypulleynum_changed(self, *args): self.a.update_pps('y')
    def on_yleadscrew_changed(self, *args): self.a.update_pps('y')
    def on_ymaxvel_changed(self, *args): self.a.update_pps('y')
    def on_ymaxacc_changed(self, *args): self.a.update_pps('y')
    def on_yaxistest_clicked(self, *args): self.a.test_axis('y')

#********************
# AXIS Z PAGE
#********************
    def axisz_prepare(self):
        self.axis_prepare('z')
    def axisz_finish(self):
        self.axis_done('z')
    # AXIS Z callbacks
    def on_zsteprev_changed(self, *args): self.a.update_pps('z')
    def on_zmicrostep_changed(self, *args): self.a.update_pps('z')
    def on_zpulleyden_changed(self, *args): self.a.update_pps('z')
    def on_zpulleynum_changed(self, *args): self.a.update_pps('z')
    def on_zleadscrew_changed(self, *args): self.a.update_pps('z')
    def on_zmaxvel_changed(self, *args): self.a.update_pps('z')
    def on_zmaxacc_changed(self, *args): self.a.update_pps('z')
    def on_zaxistest_clicked(self, *args): self.a.test_axis('z')

#********************
# AXIS A PAGE
#********************
    def axisa_prepare(self):
        self.axis_prepare('a')
    def axisa_finish(self):
        self.axis_done('a')
    # AXIS A callbacks
    def on_asteprev_changed(self, *args): self.a.update_pps('a')
    def on_amicrostep_changed(self, *args): self.a.update_pps('a')
    def on_apulleyden_changed(self, *args): self.a.update_pps('a')
    def on_apulleynum_changed(self, *args): self.a.update_pps('a')
    def on_aleadscrew_changed(self, *args): self.a.update_pps('a')
    def on_amaxvel_changed(self, *args): self.a.update_pps('a')
    def on_amaxacc_changed(self, *args): self.a.update_pps('a')
    def on_aaxistest_clicked(self, *args): self.a.test_axis('a')

#*********************
# General Axis methods and callbacks
#*********************

    def on_jogminus_pressed(self, w):
        self.a.jogminus = 1
        self.a.update_axis_test()

    def on_jogminus_released(self, w):
        self.a.jogminus = 0
        self.a.update_axis_test()

    def on_jogplus_pressed(self, w):
        self.a.jogplus = 1
        self.a.update_axis_test()
    def on_jogplus_released(self, w):
        self.a.jogplus = 0
        self.a.update_axis_test()

    def update_axis_params(self, *args):
        self.a.update_axis_test()

    def axis_prepare(self, axis):
        def set_text(n): self.w[axis + n].set_text("%s" % self.d[axis + n])
        def set_active(n): self.w[axis + n].set_active(self.d[axis + n])
        SIG = self._p
        set_text("steprev")
        set_text("microstep")
        set_text("pulleynum")
        set_text("pulleyden")
        set_text("leadscrew")
        set_text("maxvel")
        set_text("maxacc")
        set_text("homepos")
        set_text("minlim")
        set_text("maxlim")
        set_text("homesw")
        set_text("homevel")
        set_active("latchdir")

        if axis == "a":
            self.w[axis + "screwunits"].set_text(_("degree / rev"))
            self.w[axis + "velunits"].set_text(_("deg / s"))
            self.w[axis + "accunits"].set_text(_("deg / s²"))
            self.w[axis + "accdistunits"].set_text(_("deg"))
            self.w[axis + "scaleunits"].set_text(_("Steps / deg"))
        elif self.d.units:
            self.w[axis + "screwunits"].set_text(_("mm / rev"))
            self.w[axis + "velunits"].set_text(_("mm / s"))
            self.w[axis + "accunits"].set_text(_("mm / s²"))
            self.w[axis + "accdistunits"].set_text(_("mm"))
            self.w[axis + "scaleunits"].set_text(_("Steps / mm"))
        else:
            self.w[axis + "screwunits"].set_text(_("rev / in"))
            self.w[axis + "velunits"].set_text(_("in / s"))
            self.w[axis + "accunits"].set_text(_("in / s²"))
            self.w[axis + "accdistunits"].set_text(_("in"))
            self.w[axis + "scaleunits"].set_text(_("Steps / in"))

        inputs = self.a.build_input_set()
        thisaxishome = set((SIG.ALL_HOME, SIG.ALL_LIMIT_HOME, "home-" + axis, "min-home-" + axis,
                            "max-home-" + axis, "both-home-" + axis))
        homes = bool(inputs & thisaxishome)
        self.w[axis + "homesw"].set_sensitive(homes)
        self.w[axis + "homevel"].set_sensitive(homes)
        self.w[axis + "latchdir"].set_sensitive(homes)

        self.w[axis + "steprev"].grab_focus()
        gobject.idle_add(lambda: self.a.update_pps(axis))

    def axis_done(self, axis):
        def get_text(n): self.d[axis + n] = float(self.w[axis + n].get_text())
        def get_active(n): self.d[axis + n] = self.w[axis + n].get_active()
        get_text("steprev")
        get_text("microstep")
        get_text("pulleynum")
        get_text("pulleyden")
        get_text("leadscrew")
        get_text("maxvel")
        get_text("maxacc")
        get_text("homepos")
        get_text("minlim")
        get_text("maxlim")
        get_text("homesw")
        get_text("homevel")
        get_active("latchdir")

#*********
# SPINDLE PAGE
#*********
    def spindle_prepare(self):
        SIG = self._p
        self.w['spindlecarrier'].set_text("%s" % self.d.spindlecarrier)
        self.w['spindlespeed1'].set_text("%s" % self.d.spindlespeed1)
        self.w['spindlespeed2'].set_text("%s" % self.d.spindlespeed2)
        self.w['spindlepwm1'].set_text("%s" % self.d.spindlepwm1)
        self.w['spindlepwm2'].set_text("%s" % self.d.spindlepwm2)
        self.w['spindlecpr'].set_text("%s" % self.d.spindlecpr)
        self.w['spindlenearscale'].set_value(self.d.spindlenearscale * 100)
        self.w['spindlefiltergain'].set_value(self.d.spindlefiltergain)
        self.w['usespindleatspeed'].set_active(self.d.usespindleatspeed)

        if SIG.PHA in (self.d.pin10, self.d.pin11, self.d.pin12, self.d.pin13, self.d.pin15):
            self.w.spindlecpr.set_sensitive(1)
            self.w.spindlefiltergain.show()
            self.w.spindlefiltergainlabel.show()
            self.w.spindlenearscale.show()
            self.w.usespindleatspeed.show()
            self.w.spindlenearscaleunitlabel.show()
        else:
            self.w.spindlecpr.set_sensitive(0)
            self.w.spindlefiltergain.hide()
            self.w.spindlefiltergainlabel.hide()
            self.w.spindlenearscale.hide()
            self.w.usespindleatspeed.hide()
            self.w.spindlenearscaleunitlabel.hide()

    def spindle_finish(self):
        self.d.spindlecarrier = float(self.w.spindlecarrier.get_text())
        self.d.spindlespeed1 = float(self.w.spindlespeed1.get_text())
        self.d.spindlespeed2 = float(self.w.spindlespeed2.get_text())
        self.d.spindlepwm1 = float(self.w.spindlepwm1.get_text())
        self.d.spindlepwm2 = float(self.w.spindlepwm2.get_text())
        self.d.spindlecpr = float(self.w.spindlecpr.get_text())
        self.d.spindlenearscale = self.w.spindlenearscale.get_value()/100
        self.d.spindlefiltergain = self.w.spindlefiltergain.get_value()
        self.d.usespindleatspeed = self.w['usespindleatspeed'].get_active()

    # Spindle page callbacks
    def on_usespindleatspeed_toggled(self,*args):
        self.w.spindlenearscale.set_sensitive(self.w.usespindleatspeed.get_active())

#*************
# FINISH PAGE
#*************
    def finished_prepare(self):
        pass
    def finished_finish(self):
        self.a.buid_config()

# BOILER CODE
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)
