import hal
import gtk
import gladevcp.makepins # needed for the dialog's calulator widget
import pango

_MAN = 0;_MDI = 1;_AUTO = 2;_LOCKTOGGLE = 1

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

    # This is a new method that calls a gscreen method to toggle the DRO units
    # Gscreen's regular unit button saves the state
    # for startup, This one just changes it for the session
    def on_metric_select_clicked(self,widget):
        data = (self.data.dro_units -1) * -1
        self.gscreen.set_dro_units(data,False)
        for i in ("1","2","3"):
            for letter in self.data.axis_list:
                axis = "dro_%s%s"% (letter,i)
                try:
                    self.widgets[axis].set_property("display_units_mm",data)
                except:
                    pass

    def on_diameter_mode_pressed(self, widget):
        data = widget.get_active()
        print "switch diam mode",data
        self.gscreen.set_diameter_mode(data)
        for i in ("1","2","3"):
            axis = "dro_x%s"% (i)
            if data:
                self.widgets[axis].set_to_diameter()
            else:
                self.widgets[axis].set_to_radius()

    # This is a new method for our button
    # we selected this method name in the glade file as a signal callback 
    def on_estop_clicked(self,*args):
        print "estop"
        if self.data.estopped:
            self.emc.estop_reset(1)
            self.gscreen.add_alarm_entry("Machine Estop Reset")
        else:
            self.emc.machine_off(1)
            self.emc.estop(1)
            self.widgets.on_label.set_text("Machine Off")
            self.gscreen.add_alarm_entry("Machine Estopped and Powered Off")
        return True

    # This is a new method for our new button
    # we selected this method name in the glade file as a signal callback 
    def on_machine_state_clicked(self,widget):
        if self.data.estopped:
            return
        elif not self.data.machine_on:
            self.emc.machine_on(1)
            self.widgets.on_label.set_text("Machine On")
            self.gscreen.add_alarm_entry("Machine powered on")
        else:
            self.emc.machine_off(1)
            self.widgets.on_label.set_text("Machine Off")
            self.gscreen.add_alarm_entry("Machine powered off")

    # display the main tab and set the mode to setup
    def on_setup_button_clicked(self,widget):
        self.widgets.notebook_main.set_current_page(0)
        self.data.mode_order = _MAN,_MDI,_AUTO
        label = self.data.mode_labels
        self.widgets.button_mode.set_label(label[self.data.mode_order[0]])
        self.gscreen.mode_changed(self.data.mode_order[0])
        self.toggle_modes(widget)

    # display the main tab and set the mode to run
    def on_run_button_clicked(self,widget):
        self.widgets.notebook_main.set_current_page(0)
        self.data.mode_order = _AUTO,_MAN,_MDI
        label = self.data.mode_labels
        self.widgets.button_mode.set_label(label[self.data.mode_order[0]])
        self.gscreen.mode_changed(self.data.mode_order[0])
        self.toggle_modes(widget)

    # display the main tab and set the mode to MDI
    def on_mdi_button_clicked(self,widget):
        self.widgets.notebook_main.set_current_page(0)
        self.data.mode_order = _MDI,_MAN,_AUTO
        label = self.data.mode_labels
        self.widgets.button_mode.set_label(label[self.data.mode_order[0]])
        self.gscreen.mode_changed(self.data.mode_order[0])
        self.toggle_modes(widget)

    # This is called when the system button is toggled
    # If the page is not showing it displays a unlock code dialog
    # unless you have already unlocked the page.
    # if that returns true then the page is shown
    # otherwise the button is untoggled and the page is not shown
    # if you press the system buttonn when the system page is already showing
    # it will relock the page
    def on_system_button_clicked(self,widget):
        if self.widgets.notebook_main.get_current_page() == 4:
            self.gscreen.block("system_button")
            widget.set_active(True)
            self.gscreen.unblock("system_button")
            global _LOCKTOGGLE
            _LOCKTOGGLE=1
            self.widgets.system_button.set_label(" System\n(Locked)")
            self.gscreen.add_alarm_entry("System page re-locked")
            self.on_setup_button_clicked(self.widgets.setup_button)
            return
        if not self.system_dialog():
            self.gscreen.block("system_button")
            widget.set_active(False)
            self.gscreen.unblock("system_button")
            return
        self.widgets.notebook_main.set_current_page(4)
        self.toggle_modes(widget)
        self.widgets.system_button.set_label(" System\n        ")

    # Display the tooledit tab
    def on_tooledit_button_clicked(self,widget):
        self.widgets.notebook_main.set_current_page(3)
        self.toggle_modes(widget)

    # Display the offsetpage tab
    def on_offsetpage_button_clicked(self,widget):
        self.widgets.notebook_main.set_current_page(2)
        self.toggle_modes(widget)

    # This toggles the buttons so only one is presses at any one time
    def toggle_modes(self,widget):
        temp = "setup_button","mdi_button","run_button","tooledit_button","system_button","offsetpage_button"
        for i in temp:
            state = False
            if self.widgets[i] == widget: state = True
            self.gscreen.block(i)
            self.widgets[i].set_active(state)
            self.gscreen.unblock(i)

    def on_button_edit_clicked(self,widget):
        state = widget.get_active()
        if not state:
            self.gscreen.edited_gcode_check()
        self.widgets.notebook_main.set_current_page(0)
        self.widgets.notebook_main.set_show_tabs(not (state))
        self.gscreen.edit_mode(state)
        if not state and self.widgets.button_full_view.get_active():
            self.gscreen.set_full_graphics_view(True)
        if self.data.edit_mode:
            self.widgets.mode_select_box.hide()
            self.widgets.search_box.show()
            self.widgets.button_edit.set_label("Exit\nEdit")
        else:
            self.widgets.mode_select_box.show()
            self.widgets.search_box.hide()
            self.widgets.button_edit.set_label("Edit")
        self.widgets.notebook_main.set_show_tabs(False)

    # This dialog is for unlocking the system tab
    # The unlock code number is defined at the top of the page
    def system_dialog(self):
        global _LOCKTOGGLE
        if _LOCKTOGGLE == 0: return True
        dialog = gtk.Dialog("Enter System Unlock Code",
                   self.widgets.window1,
                   gtk.DIALOG_DESTROY_WITH_PARENT,
                   (gtk.STOCK_CANCEL, gtk.RESPONSE_REJECT,
                    gtk.STOCK_OK, gtk.RESPONSE_ACCEPT))
        label = gtk.Label("Enter System Unlock Code")
        label.modify_font(pango.FontDescription("sans 20"))
        calc = gladevcp.Calculator()
        dialog.vbox.pack_start(label)
        dialog.vbox.add(calc)
        calc.set_value("")
        calc.set_property("font","sans 20")
        calc.set_editable(True)
        calc.entry.connect("activate", lambda w : dialog.emit('response',gtk.RESPONSE_ACCEPT))
        dialog.parse_geometry("400x400")
        dialog.set_decorated(False)
        dialog.show_all()
        calc.num_pad_only(True)
        calc.integer_entry_only(True)
        response = dialog.run()
        code = calc.get_value()
        dialog.destroy()
        if response == gtk.RESPONSE_ACCEPT:
            if code == int(self.data.unlock_code):
                self.gscreen.add_alarm_entry("System page unlocked")
                _LOCKTOGGLE = 0
                return True
        _LOCKTOGGLE = 1
        return False

    def on_abs_colorbutton_color_set(self,widget):
        self.gscreen.set_abs_color()
        color = self.data.abs_color
        fg_color = pango.AttrForeground(color[0],color[1],color[2], 0, 11)
        for i in self.data.axis_list:
            axis = "dro_%s1"% i
            attr = self.widgets[axis].get_attributes()
            attr.insert(fg_color)
            self.widgets[axis].set_attributes(attr)

    def on_rel_colorbutton_color_set(self,widget):
        self.gscreen.set_rel_color()
        color = self.data.rel_color
        fg_color = pango.AttrForeground(color[0],color[1],color[2], 0, 11)
        for i in self.data.axis_list:
            axis = "dro_%s2"% i
            attr = self.widgets[axis].get_attributes()
            attr.insert(fg_color)
            self.widgets[axis].set_attributes(attr)

    def on_dtg_colorbutton_color_set(self,widget):
        self.gscreen.set_dtg_color()
        color = self.data.dtg_color
        fg_color = pango.AttrForeground(color[0],color[1],color[2], 0, 11)
        for i in self.data.axis_list:
            axis = "dro_%s3"% i
            attr = self.widgets[axis].get_attributes()
            attr.insert(fg_color)
            self.widgets[axis].set_attributes(attr)

    def on_hal_status_not_all_homed(self,widget,data):
        temp =[]
        for letter in self.data.axis_list:
            axnum = "xyzabcuvws".index(letter)
            if str(axnum) in data:
                self.widgets["home_%s"%letter].set_text(" ")
                temp.append(" %s"%letter.upper())
        self.gscreen.add_alarm_entry(_("There are unhomed axes: %s"%temp))

    def on_hal_status_axis_homed(self,widget,data):
        for letter in self.data.axis_list:
            axnum = "xyzabcuvws".index(letter)
            if str(axnum) in data:
                self.widgets["home_%s"%letter].set_text("*")
            else:
                self.widgets["home_%s"%letter].set_text(" ")

    def on_show_dtg_pressed(self, widget):
        data = widget.get_active()
        self.widgets.dtg_vbox.set_visible(data)
        self.gscreen.set_show_dtg(data)

    # Connect to gscreens regular signals and add a couple more
    def connect_signals(self,handlers):
        self.gscreen.connect_signals(handlers)
        # connect to handler file callbacks:
        self.gscreen.widgets.metric_select.connect("clicked", self.on_metric_select_clicked)
        self.gscreen.widgets.diameter_mode.connect("clicked", self.on_diameter_mode_pressed)
        temp = "setup_button","mdi_button","run_button","tooledit_button","system_button","offsetpage_button"
        for cb in temp:
                i = "_sighandler_%s"% (cb)
                self.data[i] = int(self.widgets[cb].connect("toggled", self["on_%s_clicked"%cb]))
        self.widgets.hal_status.connect("not-all-homed",self.on_hal_status_not_all_homed)
        self.widgets.hal_status.connect("homed",self.on_hal_status_axis_homed)
        self.widgets.abs_colorbutton.connect("color-set", self.on_abs_colorbutton_color_set)
        self.widgets.rel_colorbutton.connect("color-set", self.on_rel_colorbutton_color_set)
        self.widgets.dtg_colorbutton.connect("color-set", self.on_dtg_colorbutton_color_set)
        self.widgets.unlock_number.connect("value-changed",self.gscreen.on_unlock_number_value_changed)
        self.widgets.show_dtg.connect("clicked", self.on_show_dtg_pressed)

    # We don't want Gscreen to initialize ALL it's regular widgets because this custom
    # screen doesn't have them all -just most of them. So we call the ones we want
    def initialize_widgets(self):
        self.gscreen.init_show_windows()
        self.gscreen.init_dynamic_tabs()
        #self.gscreen.init_axis_frames()
        #self.gscreen.init_dro_colors()
        self.gscreen.init_screen2()
        self.gscreen.init_fullscreen1()
        self.gscreen.init_gremlin()
        self.gscreen.init_manual_spindle_controls()
        self.gscreen.init_dro_colors()
        self.init_dro() # local function
        self.gscreen.init_audio()
        self.gscreen.init_desktop_notify()
        self.gscreen.init_statusbar()
        self.gscreen.init_entry()
        self.gscreen.init_tooleditor()
        self.gscreen.init_offsetpage()
        self.gscreen.init_embeded_terminal()
        self.gscreen.init_themes()
        self.gscreen.init_screen1_geometry()
        self.gscreen.init_running_options()
        self.gscreen.init_hide_cursor()
        #self.gscreen.init_mode()
        self.gscreen.mode_changed(self.data.mode_order[0])
        self.gscreen.init_sensitive_on_off()
        self.gscreen.init_sensitive_run_idle()
        self.gscreen.init_sensitive_all_homed()
        self.gscreen.init_sensitive_override_mode()
        self.gscreen.init_sensitive_graphics_mode()
        self.gscreen.init_sensitive_origin_mode()
        self.init_sensitive_edit_mode() # local function
        for i in ("setup_button","mdi_button","run_button","tooledit_button","offsetpage_button","button_index_tool"):
            self.data.sensitive_override_mode.append(i)
            self.data.sensitive_graphics_mode.append(i)
            self.data.sensitive_origin_mode.append(i) 
        self.widgets["spindle-at-speed"].set_property("on_color","black")
        self.gscreen.init_unlock_code()
        self.gscreen.init_state()
        for i in self.data.axis_list:
            self.widgets["dro_%s1"%i].show()
            self.widgets["dro_%s2"%i].show()
            self.widgets["dro_%s3"%i].show()
            self.widgets["axis_%s"%i].show()
            self.widgets["home_%s"%i].show()
        #self.widgets.offsetpage1.set_highlight_color("lightblue")
        self.widgets.offsetpage1.set_font("sans 18")
        self.widgets.offsetpage1.set_row_visible("1",False)
        self.widgets.tooledit1.set_font("sans 18")
        if self.data.embedded_keyboard:
            self.gscreen.launch_keyboard()

    def init_sensitive_edit_mode(self):
        self.data.sensitive_edit_mode = ["button_graphics","button_override","button_restart","button_cycle_start","button_single_step",
            "run_button","setup_button","mdi_button","system_button","tooledit_button","ignore_limits",
            "offsetpage_button"]

    def init_dro(self):
        self.on_abs_colorbutton_color_set(None)
        self.on_rel_colorbutton_color_set(None)
        self.on_dtg_colorbutton_color_set(None)
        self.widgets.show_dtg.set_active(self.data.show_dtg)
        self.on_show_dtg_pressed(self.widgets.show_dtg)
        self.gscreen.init_dro()
        data = self.data.dro_units 
        for i in ("1","2","3"):
            for letter in self.data.axis_list:
                axis = "dro_%s%s"% (letter,i)
                try:
                    self.widgets[axis].set_property("display_units_mm",data)
                except:
                    pass

    # every 100 milli seconds this gets called
    # we add calls to the regular functions for the widgets we are using.
    # and add any extra calls/code 
    def periodic(self):
        self.update_mdi_spindle_button() # local method
        self.gscreen.update_spindle_bar()
        #self.gscreen.update_dro()
        self.gscreen.update_active_gcodes()
        self.gscreen.update_active_mcodes()
        self.gscreen.update_aux_coolant_pins()
        self.gscreen.update_feed_speed_label()
        self.gscreen.update_tool_label()
        self.gscreen.update_coolant_leds()
        self.gscreen.update_estop_led()
        self.gscreen.update_machine_on_led()
        self.gscreen.update_limit_override()
        self.gscreen.update_override_label()
        self.gscreen.update_jog_rate_label()
        self.gscreen.update_mode_label()
        self.gscreen.update_units_button_label()

    # spindle controls
    def update_mdi_spindle_button(self):
        self.widgets.at_speed_label.set_label(_("%d RPM"%abs(self.data.spindle_speed)))
        label = self.widgets.spindle_control.get_label()
        speed = self.data.spindle_speed
        if speed == 0 and not label == _("Start"):
            temp = _("Start")
            self.widgets["spindle-at-speed"].set_property("on_color","black")
        elif speed and not label == _("Stop"):
            temp = _("Stop")
            self.widgets["spindle-at-speed"].set_property("on_color","green")
        else: return
        self.widgets.spindle_control.set_label(temp)

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

# standard handler call
def get_handlers(halcomp,builder,useropts,gscreen):
     return [HandlerClass(halcomp,builder,useropts,gscreen)]
