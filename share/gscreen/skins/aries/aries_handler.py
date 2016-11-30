import hal
import hal_glib
import pango
import os
import gtk
from time import strftime
# This is a handler file for using Gscreen's infrastructure
# to load a completely custom glade screen.
# The only things that really matters is that it's saved as a GTK builder project,
# the toplevel window is caller window1 (The default name) and you connect a destroy
# window signal else you can't close down linuxcnc 

# standard handler call
def get_handlers(halcomp,builder,useropts,gscreen):
     return [HandlerClass(halcomp,builder,useropts,gscreen)]

class HandlerClass:

    # This will be pretty standard to gain access to everything
    # emc is for control and status of linuxcnc
    # data is important data from gscreen and linuxcnc
    # widgets is all the widgets from the glade files
    # gscreen is for access to gscreens methods
    def __init__(self, halcomp,builder,useropts,gscreen):
            self.emc = gscreen.emc
            self.cmd = gscreen.emc.emccommand
            self.data = gscreen.data
            self.widgets = gscreen.widgets
            self.gscreen = gscreen

    # This connects siganals without using glade's autoconnect method
    # in this case to destroy the window
    # it calls the method in gscreen: gscreen.on_window_destroy()
    def connect_signals(self,handlers):
        signal_list = [ ["window1","destroy", "on_window1_destroy"],]
        for i in signal_list:
            if len(i) == 3:
                self.gscreen.widgets[i[0]].connect(i[1], self.gscreen[i[2]])
            elif len(i) == 4:
                self.gscreen.widgets[i[0]].connect(i[1], self.gscreen[i[2]],i[3])
        self.widgets.button_shut_down.connect("clicked",self.gscreen.on_window1_destroy)
        self.widgets.light_button_manual.connect("clicked",self.on_light_button_manual_clicked)
        self.widgets.light_button_mdi.connect("clicked",self.on_light_button_mdi_clicked)
        self.widgets.light_button_auto.connect("clicked",self.on_light_button_auto_clicked)
        self.widgets.button_page_up.connect("clicked",self.on_button_page_up_clicked)
        self.widgets.button_zero_axis.connect("clicked",self.on_button_zero_axis_clicked)
        self.widgets.button_set_axis.connect("clicked",self.on_button_set_axis_clicked)
        self.widgets.button_statusbar_pop.connect("clicked",self.on_button_statusbar_pop_clicked)
        self.widgets.button_unhome_selected.connect("clicked",self.on_button_unhome_selected_clicked)
        self.widgets.light_button_offsets.connect("clicked",self.on_light_button_offsets_clicked)
        self.widgets.light_button_tool.connect("clicked",self.on_light_button_tool_clicked)
        self.widgets.light_button_graphics.connect("clicked",self.on_light_button_graphics_clicked)
        self.widgets.light_button_mdi_entry.connect("clicked",self.on_light_button_mdi_entry_clicked)
        self.widgets.hal_status.connect("all-homed",self.on_hal_status_axis_homed)
        self.widgets.hal_status.connect("not-all-homed",self.on_hal_status_axis_not_homed)
        self.widgets.hal_status.connect("user-system-changed",self.on_user_system_changed)
        self.widgets.hal_status.connect("metric-mode-changed",self.on_metric_mode_changed)
        self.widgets.hal_status.connect("current-feed-rate",self.on_current_feedrate_changed)
        self.widgets.hal_status.connect("tool-in-spindle-changed",self.on_tool_in_spindle_changed)
        self.widgets.hal_status.connect("feed-override-changed",self.on_feed_override_changed)
        self.widgets.hal_status.connect("rapid-override-changed",self.on_rapid_override_changed)
        self.widgets.hal_status.connect("spindle-override-changed",self.on_spindle_override_changed)
        self.widgets.hal_status.connect("file-loaded",self.on_file_loaded_changed)
        self.widgets.hal_status.connect("mode-manual",self.on_mode_manual_changed)
        self.widgets.hal_status.connect("mode-mdi",self.on_mode_mdi_changed)
        self.widgets.hal_status.connect("mode-auto",self.on_mode_auto_changed)
        self.widgets.hal_status.connect("spindle-control-changed",self.on_spindle_control_changed)
        self.widgets.hal_status.connect("block-delete-changed",self.on_block_delete_changed)
        self.widgets.hal_status.connect("optional-stop-changed",self.on_optional_stop_changed)
        self.widgets.hal_status.connect("program-pause-changed",self.on_program_pause_changed)
        self.widgets.hal_status.connect("m-code-changed",self.on_m_code_changed)
        self.widgets.hal_status.connect("g-code-changed",self.on_g_code_changed)
        self.widgets.hal_status.connect("homed",self.on_homed_changed)
        self.widgets.hal_status.connect("reload-display",self.reload_display)
    # We don't want Gscreen to initialize it's regular widgets because this custom
    # screen doesn't have most of them. So we add this function call.
    # Since this custom screen uses gladeVCP magic for its interaction with linuxcnc
    # We don't add much to this function, but we do want the window to display.
    # init_show_window will do this
    def initialize_widgets(self):
        self.gscreen.init_show_windows()
        self.gscreen.init_dynamic_tabs()
        #self.gscreen.launch_keyboard()
        self.gscreen.init_statusbar()
        self.gscreen.init_entry()
        self.gscreen.init_tooleditor()
        self.gscreen.init_offsetpage()
        #self.gscreen.initialize_keybindings()
        self.gscreen.keylookup.add_conversion('F1','F1','on_keycall_f1')
        self.gscreen.keylookup.add_conversion('F2','MAN','on_keycall_manual')
        self.gscreen.keylookup.add_conversion('F3','MDI','on_keycall_mdi')
        self.gscreen.keylookup.add_conversion('F4','AUTO','on_keycall_auto')
        self.gscreen.keylookup.add_conversion('F5','F5','on_keycall_f5')
        self.gscreen.keylookup.add_conversion('F6','F6','on_keycall_f6')
        self.gscreen.keylookup.add_conversion('F7','F7','on_keycall_f7')
        self.gscreen.keylookup.add_conversion('F8','F8','on_keycall_f8')
        self.gscreen.keylookup.add_conversion('F12','TEST','on_keycall_HALMETER')
        self.gscreen.keylookup.add_conversion('Home','HOME','on_keycall_home')
        self.gscreen.keylookup.add_conversion('y','SELECT_y','on_keycall_y')
        self.statusbar_id = self.widgets.statusbar1.get_context_id("Statusbar1")
        self.widgets.statusbar1.push(1,"Ready For Homing")

        # Change the font of the main DRO (Gentium,
        font = dro_font = "Brave New Era G98"
        self.widgets.label_axis_x.modify_font(pango.FontDescription(font))
        self.widgets.label_axis_y.modify_font(pango.FontDescription(font))
        self.widgets.label_axis_z.modify_font(pango.FontDescription(font))

        self.widgets.hal_dro_x.modify_font(pango.FontDescription(dro_font))
        self.widgets.hal_dro_y.modify_font(pango.FontDescription(dro_font))
        self.widgets.hal_dro_z.modify_font(pango.FontDescription(dro_font))

        machine_font = dro_machine_font = "Transistor"
        self.widgets.label_machine_axis_x.modify_font(pango.FontDescription(machine_font))
        self.widgets.label_machine_axis_y.modify_font(pango.FontDescription(machine_font))
        self.widgets.label_machine_axis_z.modify_font(pango.FontDescription(machine_font))
        self.widgets.hal_dro_machine_x.modify_font(pango.FontDescription(dro_machine_font))
        self.widgets.hal_dro_machine_y.modify_font(pango.FontDescription(dro_machine_font))
        self.widgets.hal_dro_machine_z.modify_font(pango.FontDescription(dro_machine_font))
        # make sure hal_status updates the widgets after everything loads
        self.widgets.hal_status.forced_update()
        self.widgets.image3.set_from_file('/usr/share/linuxcnc/linuxcncicon.png')
        #self.widgets.image3.set_from_file('/home/chris/linuxcnc/configs/5i25-7i43-sherline-gscreen/aries.jpg')
    # If we need extra HAL pins here is where we do it.
    # Note you must import hal at the top of this script to do it.
    # For gaxis there is no extra pins but since we don't want gscreen to
    # add it's default pins we added this dunmmy function
    def initialize_pins(self):
        self.data['select-x'] = hal_glib.GPin(self.gscreen.halcomp.newpin('select-x', hal.HAL_BIT, hal.HAL_IN))
        self.data['select-x'].connect('value-changed', self.on_select_x)
        self.data['select-y'] = hal_glib.GPin(self.gscreen.halcomp.newpin('select-y', hal.HAL_BIT, hal.HAL_IN))
        self.data['select-y'].connect('value-changed', self.on_select_y)
        self.data['select-z'] = hal_glib.GPin(self.gscreen.halcomp.newpin('select-z', hal.HAL_BIT, hal.HAL_IN))
        self.data['select-z'].connect('value-changed', self.on_select_z)
        self.data['jog-rate'] = hal_glib.GPin(self.gscreen.halcomp.newpin('jog-rate', hal.HAL_FLOAT, hal.HAL_IN))
        self.data['jog-rate'].connect('value-changed', self.on_jog_rate_changed)
        self.data['jog-incr'] = hal_glib.GPin(self.gscreen.halcomp.newpin('jog-incr', hal.HAL_FLOAT, hal.HAL_IN))
        self.data['jog-incr'].connect('value-changed', self.on_jog_incr_changed)
    # every 100 milli seconds this gets called
    # add pass so gscreen doesn't try to update it's regular widgets or
    # add the individual function names that you would like to call.
    def periodic(self):
        self.widgets.label_time.set_label( strftime( "%H:%M:%S" ) + "\n" + strftime( "%d.%m.%Y" ) )
        pass

    # HELPER FUNCTIONS
    def set_dro_attributes(self,state):
        attr = pango.AttrList()
        if state:
            fg_color = pango.AttrForeground(100, 60535, 100, 0, 1)# green
        else:
            fg_color = pango.AttrForeground(60535, 100, 100, 0, 1)# red
        attr.insert(fg_color)
        #attr.insert(pango..AttrStretch(pango.STRETCH_ULTRA_CONDENSED, 0, 1)
        attr.insert(pango.AttrWeight(600, 0, -1))
        attr.insert(pango.AttrScale(6, 0, -1))
        return attr

    # key bindings
    def on_keycall_HALMETER(self,state,SHIFT,CNTRL,ALT):
        if state:
            self.gscreen.on_halmeter()
        return True
    def on_keycall_manual(self,state,SHIFT,CNTRL,ALT):
        if state:
            self.on_light_button_manual_clicked(None,state)
        return True
    def on_keycall_mdi(self,state,SHIFT,CNTRL,ALT):
        if state:
            self.on_light_button_mdi_clicked(None,state)
        return True
    def on_keycall_auto(self,state,SHIFT,CNTRL,ALT):
        if state:
            self.on_light_button_auto_clicked(None,state)
        return True
    def on_keycall_f1(self,state,SHIFT,CNTRL,ALT):
        if state:
            self.on_button_page_up_clicked(None)
        return True
    def on_keycall_f5(self,state,SHIFT,CNTRL,ALT):
        if state:
            mode = self.widgets.hal_status.stat.task_mode
            if mode == 1:
                # manual
                t = self.widgets.notebook_manual
                if t.get_current_page() == 1:
                    self.on_button_zero_axis_clicked(None)
                elif t.get_current_page() == 0:
                    self.cmd.home(0)
                elif t.get_current_page() == 2:
                    self.widgets.statusbar1.push(1,"Closing Linuxcnc!!")
                    self.gscreen.on_window1_destroy(None)
            elif mode == 3:
                # MDI
                pass
            else:
                # auto
                self.widgets.vcp_action_open1.emit('activate')
        return True
    def on_keycall_f6(self,state,SHIFT,CNTRL,ALT):
        if state:
            mode = self.widgets.hal_status.stat.task_mode
            if mode == 1: # manual
                t = self.widgets.notebook_manual
                if t.get_current_page() == 1:
                    self.on_light_button_offsets_clicked(None,None)
                elif t.get_current_page() == 0:
                    self.cmd.home(1)
            elif mode == 3: # MDI
                pass
            else: # auto
                pass
        return True
    def on_keycall_f7(self,state,SHIFT,CNTRL,ALT):
        if state:
            mode = self.widgets.hal_status.stat.task_mode
            if mode == 1: # manual
                t = self.widgets.notebook_manual
                if t.get_current_page() == 1:
                    self.on_light_button_tool_clicked(None,None)
                elif t.get_current_page() == 0:
                    self.cmd.home(2)
            elif mode == 3: # MDI
                pass
            else: # auto
                pass
        return True
    def on_keycall_f8(self,state,SHIFT,CNTRL,ALT):
        if state:
            mode = self.widgets.hal_status.stat.task_mode
            if mode == 1: # manual
                t = self.widgets.notebook_manual
                if t.get_current_page() == 1:
                    self.on_button_set_axis_clicked(None)
                elif t.get_current_page() == 0:
                    self.on_button_unhome_selected_clicked(None)
            elif mode == 3: # MDI
                pass
            else: # auto
                pass
        return True
    def on_keycall_home(self,state,SHIFT,CNTRL,ALT):
        if state:
            mode = self.widgets.hal_status.stat.task_mode
            if mode == 1: # manual
                print 'home selected'
                if self.data['select-x'].get():
                    self.cmd.home(0)
                elif self.data['select-y'].get():
                    self.cmd.home(1)
                elif self.data['select-z'].get():
                    self.cmd.home(2)
        return True
    def on_keycall_y(self,state,SHIFT,CNTRL,ALT):
        if state and self.widgets.hal_status.stat.task_mode == 1:
            self.data['select-y'].set(self.data['select-y'].get()*-1+1)
            mode = self.widgets.hal_status.stat.task_mode
            if mode == 1: # manual
                pass
            return True

    # CALLBACKS FROM HAL PINS
    def on_select_x(self,pin):
        self.widgets.label_axis_x.set_attributes(self.set_dro_attributes(pin.get()))
    def on_select_y(self,pin):
        self.widgets.label_axis_y.set_attributes(self.set_dro_attributes(pin.get()))
    def on_select_z(self,pin):
        self.widgets.label_axis_z.set_attributes(self.set_dro_attributes(pin.get()))
    def on_jog_rate_changed(self,pin):
        self.widgets.label_jog_speed.set_text('%.1f IPM'%pin.get())
    def on_jog_incr_changed(self,pin):
        self.widgets.label_jog_increments.set_text('%.4f inch'%pin.get())

    # CALLBACKS FROM HAL_STATUS
    def on_hal_status_axis_homed(self,widget):
        self.widgets.led_homed.set_active(True)
        self.widgets.statusbar1.push(1,"All Axes Homed")
    def on_hal_status_axis_not_homed(self,widget,data):
        if '0' in data:
            self.widgets.hal_light_button_home_x.set_light_on(False)
        if '1' in data:
            self.widgets.hal_light_button_home_y.set_light_on(False)
        if '2' in data:
            self.widgets.hal_light_button_home_z.set_light_on(False)
        self.widgets.led_homed.set_active(False)
    def on_user_system_changed(self,widget,system_index):
        self.widgets.label_origin_system.set_text('G%s'%(53+int(system_index)))
    def on_metric_mode_changed(self,widget,data):
        if data:
            text='MM'
        else:
            text='INCH'
        self.widgets.label_system_units.set_text(text)
        self.widgets.hal_dro_x.set_property("display_units_mm",data)
        self.widgets.hal_dro_y.set_property("display_units_mm",data)
        self.widgets.hal_dro_z.set_property("display_units_mm",data)
        self.widgets.hal_dro_machine_x.set_property("display_units_mm",data)
        self.widgets.hal_dro_machine_y.set_property("display_units_mm",data)
        self.widgets.hal_dro_machine_z.set_property("display_units_mm",data)
    def on_current_feedrate_changed(self,widget,rate):
        self.widgets.label_velocity.set_text('%.2f'%rate)
    def on_tool_in_spindle_changed(self,widget,data):
        self.widgets.label_tool_number.set_text('T%d'%data)
        tool_data = self.widgets.hal_status.stat.tool_table
        self.widgets.label_tool_diam.set_text('%.4f'%tool_data[0][10])
    def on_feed_override_changed(self,widget,data):
        self.widgets.label_feed_or.set_text('%d%%'%data)
    def on_rapid_override_changed(self,widget,data):
        self.widgets.label_rapid_or.set_text('%d%%'%data)
    def on_spindle_override_changed(self,widget,data):
        self.widgets.label_spindle_or.set_text('%d%%'%data)
    def on_file_loaded_changed(self,widget,path):
        self.widgets.label_program.set_text(os.path.basename(path))
    def on_homed_changed(self,widget,data):
        if '0' in data:
            self.widgets.hal_light_button_home_x.set_light_on(True)
        if '1' in data:
            self.widgets.hal_light_button_home_y.set_light_on(True)
        if '2' in data:
            self.widgets.hal_light_button_home_z.set_light_on(True)

    def on_mode_manual_changed(self,widget):
        self.widgets.light_button_manual.set_light_on(True)
        self.widgets.light_button_mdi.set_light_on(False)
        self.widgets.light_button_auto.set_light_on(False)
        self.widgets.notebook_left.set_current_page(0)
        self.widgets.hpane.set_position(0)
        self.widgets.textview_mcode.hide()
        self.widgets.textview_gcode.hide()
    def on_mode_mdi_changed(self,widget):
        self.widgets.light_button_manual.set_light_on(False)
        self.widgets.light_button_mdi.set_light_on(True)
        self.widgets.light_button_auto.set_light_on(False)
        self.widgets.notebook_left.set_current_page(1)
        self.widgets.hpane.set_position(400)
        self.widgets.textview_mcode.show()
        self.widgets.textview_gcode.show()
    def on_mode_auto_changed(self,widget):
        self.widgets.light_button_manual.set_light_on(False)
        self.widgets.light_button_mdi.set_light_on(False)
        self.widgets.light_button_auto.set_light_on(True)
        self.widgets.notebook_left.set_current_page(2)
        self.widgets.hpane.set_position(1000)
        self.widgets.textview_mcode.show()
        self.widgets.textview_gcode.show()
    def on_light_button_manual_clicked(self,widget,data):
        self.widgets.notebook_manual.show()
        self.widgets.buttonbox_mdi.hide()
        self.widgets.buttonbox_auto.hide()
        self.emc.set_manual_mode()
    def on_light_button_mdi_clicked(self,widget,data):
        self.widgets.notebook_manual.hide()
        self.widgets.buttonbox_mdi.show()
        self.widgets.buttonbox_auto.hide()
        self.emc.set_mdi_mode()
    def on_light_button_auto_clicked(self,widget,data):
        self.widgets.notebook_manual.hide()
        self.widgets.buttonbox_mdi.hide()
        self.widgets.buttonbox_auto.show()
        self.emc.set_auto_mode()
    def on_light_button_graphics_clicked(self,widget,data):
        state = widget.get_active()
        self.widgets.light_button_graphics.set_light_on(state)
        self.widgets.notebook_right.set_current_page(1)
        self.widgets.hpane.set_position(400)
    def on_button_zero_axis_clicked(self,widget):
        if self.data['select-x'].get():
            self.gscreen.mdi_control.set_axis('x',0)
            self.widgets.statusbar1.push(1,"Zero Axis X")
        if self.data['select-y'].get():
            self.gscreen.mdi_control.set_axis('y',0)
            self.widgets.statusbar1.push(1,"Zero Axis Y")
        if self.data['select-z'].get():
            self.gscreen.mdi_control.set_axis('z',0)
            self.widgets.statusbar1.push(1,"Zero Axis Z")
        self.widgets.hal_status.emit('reload-display')
    def on_button_set_axis_clicked(self,widget):
        self.gscreen.launch_numerical_input("on_offset_origin_entry_return",title='Touch Off')
    def on_offset_origin_entry_return(self,widget,result,calc,userdata,userdata2):
        value = calc.get_value()
        if result == gtk.RESPONSE_ACCEPT:
            if value == None:
                return
            pos = self.gscreen.get_qualified_input(value)
            if self.data['select-x'].get():
                self.gscreen.mdi_control.set_axis('x',pos)
                self.widgets.statusbar1.push(1,"Touch Off Axis %s: %f"%('X',pos))
            if self.data['select-y'].get():
                self.gscreen.mdi_control.set_axis('y',pos)
                self.widgets.statusbar1.push(1,"Touch Off Axis %s: %f"%('Y',pos))
            if self.data['select-z'].get():
                self.gscreen.mdi_control.set_axis('z',pos)
                self.widgets.statusbar1.push(1,"Touch Off Axis %s: %f"%('Z',pos))
            self.widgets.hal_status.emit('reload-display')
        widget.destroy()
        self.data.entry_dialog = None

    def on_light_button_offsets_clicked(self,widget,data):
        self.widgets.light_button_offsets.set_light_on(True)
        self.widgets.light_button_tool.set_light_on(False)
        self.widgets.notebook_right.set_current_page(3)
    def on_light_button_tool_clicked(self,widget,data):
        self.widgets.light_button_offsets.set_light_on(False)
        self.widgets.light_button_tool.set_light_on(True)
        self.widgets.notebook_right.set_current_page(2)
    def on_light_button_mdi_entry_clicked(self,widget,data):
        self.widgets.vcp_mdihistory1.entry.grab_focus()
        self.widgets.light_button_mdi_entry.set_light_on(True)
    def on_jog_rate_changed(self,pin):
        rate = pin.get()
        self.widgets.label_jog_speed.set_text('%.2f'%rate)
    def on_spindle_control_changed(self,widget,enabled,direction):
        if not enabled: text ='OFF'
        elif direction < 1:  text= 'CCW'
        else: text = 'CW'
        self.widgets.label_spindle_status.set_text(text)
    def on_optional_stop_changed(self,widget,data):
        self.widgets.led_optional_stop.set_active(data)
    def on_block_delete_changed(self,widget,data):
        self.widgets.led_block_delete.set_active(data)
    def on_program_pause_changed(self,widget,data):
        self.widgets.led_paused.set_active(data)
    def on_m_code_changed(self,widget,data):
        self.widgets.textbuffer1.set_text(data)
    def on_g_code_changed(self,widget,data):
        self.widgets.textbuffer2.set_text(data)
    def on_button_page_up_clicked(self,widget):
        if self.widgets.hal_status.stat.task_mode == 1:
            t = self.widgets.notebook_manual
            if t.get_n_pages()-1 == t.get_current_page():
                t.set_current_page(0)
            else:
                t.next_page()
    def reload_display(self,widget):
        self.widgets.hal_gremlin1.reloadfile(None)
    def on_button_statusbar_pop_clicked(self,widget):
        self.widgets.statusbar1.pop(self.statusbar_id)
    def on_button_unhome_selected_clicked(self,widget):
        if self.data['select-x'].get():
            self.cmd.unhome(0)
            self.widgets.statusbar1.push(1,"Unhome Axis X")
        if self.data['select-y'].get():
            self.cmd.unhome(1)
            self.widgets.statusbar1.push(1,"Unhome Axis Y")
        if self.data['select-z'].get():
            self.cmd.unhome(2)
            self.widgets.statusbar1.push(1,"Unhome Axis Z")
    # boiler code
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)
