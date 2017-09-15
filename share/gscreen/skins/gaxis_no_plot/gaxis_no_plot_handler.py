# Copyright: 2014
# Author:    Chris Morley <chrisinnanaimo@hotmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
import hal
import gtk
_X = 0;_Y = 1;_Z = 2;_A = 3;_B = 4;_C = 5;_U = 6;_V = 7;_W = 8
# This is a handler file for using Gscreen's infrastructure
# to load a completely custom glade screen
# The only things that really matters is that it's saved as a GTK builder project,
# the toplevel window is caller window1 (The default name) and you connect a destroy
# window signal else you can't close down linuxcnc 
class HandlerClass:

    # This will be pretty standard to gain access to everything
    # emc is for control and status of linuxcnc
    # data is important data from gscreen and linuxcnc
    # widgets is all the widgets from the glade files
    # gscreen is for access to gscreens methods
    def __init__(self, halcomp,builder,useropts,gscreen):
            self.emc = gscreen.emc
            self.data = gscreen.data
            self.widgets = gscreen.widgets
            self.gscreen = gscreen

    def jog_x(self,widget,direction,state):
        self.gscreen.do_key_jog(_X,direction,state)
    def jog_y(self,widget,direction,state):
        self.gscreen.do_key_jog(_Y,direction,state)
    def jog_z(self,widget,direction,state):
        self.gscreen.do_key_jog(_Z,direction,state)

    def on_scale_jog_value_changed(self,widget):
        self.gscreen.set_jog_rate(absolute=widget.get_value())

    def on_scale_fo_value_changed(self,widget):
        self.gscreen.set_feed_override((widget.get_value()/100),True)

    def on_scale_so_value_changed(self,widget):
        self.gscreen.set_spindle_override((widget.get_value()/100),True)

    def on_scale_mv_value_changed(self,widget):
        self.gscreen.set_velocity_override((widget.get_value()/100),True)

    def on_jog_speed_changed(self,widget):
        self.data.current_jogincr_index = widget.get_active()

    def on_show_alarm_page(self,widget):
        self.widgets.notebook_debug.set_show_tabs(self.widgets.menuitem5.get_active())
        self.widgets.notebook_debug.set_current_page(0)



    # erase the ready-to-home message on statusbar
    def on_hal_status_all_homed(self,widget):
        print "all-homed"
        self.data.all_homed = True
        self.widgets.statusbar1.remove_message(self.gscreen.statusbar_id,self.gscreen.homed_status_message)

    # when run is pressed, destroy the restart dialog if it's showing
    def on_toolbutton_run_toggled(self,widget):
        if not self.data.restart_dialog == None:
            self.data.restart_dialog.destroy()
            self.data.restart_dialog = None

    # This connects signals without using glade's autoconnect method
    def connect_signals(self,handlers):
        signal_list = [ ["window1","destroy", "on_window1_destroy"],
                        ["restart","clicked", "launch_restart_dialog"],
                        ["metric_select","clicked","on_metric_select_clicked"],
                        ["s_run","clicked","on_button_spindle_controls_clicked"],
                        ["spindle_control","clicked", "on_spindle_control_clicked"],
                        ["spindle_preset","clicked", "on_preset_spindle"],
                        ["spindle_increase","clicked", "on_spindle_speed_adjust"],
                        ["spindle_decrease","clicked", "on_spindle_speed_adjust"],
                        ["run_halshow","clicked", "on_halshow"],
                        ["run_calibration","clicked", "on_calibration"],
                        ["run_status","clicked", "on_status"],
                        ["run_halmeter","clicked", "on_halmeter"],
                        ["run_halscope","clicked", "on_halscope"],
                        ["run_ladder","clicked", "on_ladder"],
                    ]
        for i in signal_list:
            if len(i) == 3:
                self.widgets[i[0]].connect(i[1], self.gscreen[i[2]])
            elif len(i) == 4:
                self.widgets[i[0]].connect(i[1], self.gscreen[i[2]],i[3])
        self.widgets.toolbutton_run.connect("toggled",self.on_toolbutton_run_toggled)
        self.widgets.scale_jog.connect("value_changed",self.on_scale_jog_value_changed)
        self.widgets.scale_fo.connect("value_changed",self.on_scale_fo_value_changed)
        self.widgets.scale_so.connect("value_changed",self.on_scale_so_value_changed)
        self.widgets.scale_mv.connect("value_changed",self.on_scale_mv_value_changed)
        self.widgets.menuitem5.connect("activate",self.on_show_alarm_page)
        self.widgets.jog_speed.connect("changed",self.on_jog_speed_changed)
        self.widgets.sneg.connect("clicked",lambda i:self.gscreen.spindle_adjustment(False,True))
        self.widgets.spos.connect("clicked",lambda i:self.gscreen.spindle_adjustment(True,True))
        for i in('x','y','z'):
            self.widgets['touch_off'+i].connect("clicked", self.on_touch_off_clicked,i)
            self.widgets[i+'neg'].connect("pressed", self['jog_'+i],0,True)
            self.widgets[i+'neg'].connect("released", self['jog_'+i],0,False)
            self.widgets[i+'pos'].connect("pressed", self['jog_'+i],1,True)
            self.widgets[i+'pos'].connect("released", self['jog_'+i],1,False)

    # We don't want Gscreen to initialize it's regular widgets because this custom
    # screen doesn't have most of them. So we add this function call.
    # Since this custom screen uses gladeVCP magic for its interaction with linuxcnc
    # We don't add much to this function, but we do want to be able to change the theme so:
    # We change the GTK theme to what's in gscreen's preference file.
    # gscreen.change_theme() is a method in gscreen that changes the GTK theme of window1
    # gscreen.data.theme_name is the name of the theme from the preference file 
    # To truely be friendly, we should add a way to change the theme directly in the custom screen.
    # we also set up the statusbar and add a ready-to-home message
    def initialize_widgets(self):
        self.gscreen.init_show_windows()
        self.gscreen.init_dynamic_tabs()
        self.gscreen.init_embeded_terminal()
        self.gscreen.change_theme(self.data.theme_name)
        self.gscreen.statusbar_id = self.widgets.statusbar1.get_context_id("Statusbar1")
        self.gscreen.homed_status_message = self.widgets.statusbar1.push(1,"Ready For Homing")
        for num,i in enumerate(self.data.jog_increments):
            print i
            self.widgets.jog_speed.append_text(i)
            if i == "continuous":
                self.data.current_jogincr_index = num
                self.widgets.jog_speed.set_active(num)
#        self.gscreen.set_dro_units(self.data.dro_units)
        self.widgets.adjustment_mv.set_value(self.data.maxvelocity*100)
        self.widgets.adjustment_jog.set_upper(self.data.jog_rate_max)
        self.widgets.adjustment_jog.set_value(self.data.jog_rate)
        self.widgets.adjustment_fo.set_upper(self.data.feed_override_max*100)
        self.widgets.adjustment_fo.set_value(self.data.feed_override*100)
        self.widgets.adjustment_so.set_upper(self.data.spindle_override_max*100)
        self.widgets.adjustment_so.set_value(self.data.spindle_override*100)
        self.widgets.notebook_debug.set_show_tabs(False)
        #self.gscreen.keylookup.add_conversion('F4','TEST2','on_keycall_POWER')

    # If we need extra HAL pins here is where we do it.
    # Note you must import hal at the top of this script to build the pins here. or:
    # For gaxis there is only jog pins so we call gscreen to
    # add it's default jog pins
    def initialize_pins(self):
        self.gscreen.init_jog_pins()
 
    # checks the current operating mode according to the UI
    def check_mode(self):
        string = []
        if self.data.mode_order[0] == self.data._MAN and self.widgets.notebook_main.get_current_page() == 1:
            string.append( self.data._MAN)
            string.append(self.data._JOG)
        return string

    # keybinding calls 
    def on_keycall_ESTOP(self,state,SHIFT,CNTRL,ALT):
        if state:
            self.widgets.emc_toggle_estop.emit('activate')
            return True
    def on_keycall_POWER(self,state,SHIFT,CNTRL,ALT):
        if state:
            self.widgets.emc_toggle_power.emit('activate')
            return True
    def on_keycall_INCREMENTS(self,state,SHIFT,CNTRL,ALT):
        if state and self.data._MAN in self.check_mode(): # manual mode required
            print 'hi'
            if SHIFT:
                self.gscreen.set_jog_increments(index_dir = -1)
            else:
                self.gscreen.set_jog_increments(index_dir = 1)
            # update the combo box
            self.widgets.jog_speed.set_active(self.data.current_jogincr_index)
            return True

    def on_touch_off_clicked(self,widget,axis):
        self.gscreen.launch_numerical_input("on_offset_origin_entry_return",axis,None,"Touch off %s"% axis.upper())

    def on_offset_origin_entry_return(self,widget,result,calc,axis,userdata2):
        value = calc.get_value()
        if result == gtk.RESPONSE_ACCEPT:
            if value == None:
                return
            if not axis == "s":
                if axis in('a','b','c'):
                    pos = self.gscreen.get_qualified_input(value,switch=_DEGREE_INPUT)
                else:
                    pos = self.gscreen.get_qualified_input(value)
                self.gscreen.mdi_control.set_axis(axis,pos)
                self.gscreen.reload_plot()
        widget.destroy()
        self.data.entry_dialog = None

    # every 100 milli seconds this gets called
    # add pass so gscreen doesn't try to update it's regular widgets or
    # add the individual function names that you would like to call.
    # In this case we wish to call Gscreen's default function for units button update
    def periodic(self):
        self.gscreen.update_units_button_label()

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

# standard handler call
def get_handlers(halcomp,builder,useropts,gscreen):
     return [HandlerClass(halcomp,builder,useropts,gscreen)]
