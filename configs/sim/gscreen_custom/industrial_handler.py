import hal
_MAN = 0;_MDI = 1;_AUTO = 2
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
        for i in ("1","2"):
            for axis in ("dro_x","dro_y","dro_z"):
                self.widgets[axis+i].set_property("display_units_mm",data)

    # This is a new method for our button
    # we selected this method name in the glade file as a signal callback 
    def on_estop_clicked(self,*args):
        print "estop"
        if self.data.estopped:
            self.emc.estop_reset(1)
        else:
            self.emc.machine_off(1)
            self.emc.estop(1)
            self.widgets.on_label.set_text("Machine Off")
        return True

    # This is a new method for our new button
    # we selected this method name in the glade file as a signal callback 
    def on_machine_state_clicked(self,*args):
        if self.data.estopped:
            return
        elif not self.data.machine_on:
            self.emc.machine_on(1)
            self.widgets.on_label.set_text("Machine On")
        else:
            self.emc.machine_off(1)
            self.widgets.on_label.set_text("Machine Off")

    # These three method are used to select the thre different mode directly
    def on_setup_button_clicked(self,widget):
        self.widgets.notebook_main.set_current_page(0)
        self.data.mode_order = _MAN,_MDI,_AUTO
        label = self.data.mode_labels
        self.widgets.button_mode.set_label(label[self.data.mode_order[0]])
        self.gscreen.mode_changed(self.data.mode_order[0])

    def on_run_button_clicked(self,widget):
        self.widgets.notebook_main.set_current_page(0)
        self.data.mode_order = _AUTO,_MAN,_MDI
        label = self.data.mode_labels
        self.widgets.button_mode.set_label(label[self.data.mode_order[0]])
        self.gscreen.mode_changed(self.data.mode_order[0])

    def on_MDI_button_clicked(self,widget):
        self.widgets.notebook_main.set_current_page(0)
        self.data.mode_order = _MDI,_MAN,_AUTO
        label = self.data.mode_labels
        self.widgets.button_mode.set_label(label[self.data.mode_order[0]])
        self.gscreen.mode_changed(self.data.mode_order[0])

    # Connect to gscreens regular signals and add a couple more
    def connect_signals(self,handlers):
        self.gscreen.connect_signals(handlers)
        # connect to handler file callbacks:
        self.gscreen.widgets.metric_select.connect("clicked", self.on_metric_select_clicked)

    # We don't want Gscreen to initialize ALL it's regular widgets because this custom
    # screen doesn't have them all -just most of them. So we call the ones we want
    def initialize_widgets(self):
        #self.gscreen.init_axis_frames()
        #self.gscreen.init_dro_colors()
        self.gscreen.init_screen2()
        self.gscreen.init_fullscreen1()
        self.gscreen.init_gremlin()
        self.gscreen.init_manual_spindle_controls()
        #self.gscreen.init_dro()
        self.gscreen.init_audio()
        self.gscreen.init_statusbar()
        self.gscreen.init_entry()
        self.gscreen.init_tooleditor()
        self.gscreen.init_embeded_terminal()
        self.gscreen.init_themes()
        self.gscreen.init_screen1_geometry()
        self.gscreen.init_running_options()
        self.gscreen.init_hide_cursor()
        self.data.mode_labels = ["Set-Up Mode","MDI Mode","Run Mode"]
        self.gscreen.init_mode()
        self.gscreen.init_sensitive_on_off()
        self.data.sensitive_on_off.append("mode_box")
        self.gscreen.init_sensitive_run_idle()
        self.data.sensitive_run_idle.append("mode_box")
        self.gscreen.init_sensitive_all_homed()
        self.data.sensitive_all_homed.append("index_tool")
        self.gscreen.init_state()
        for i in self.data.axis_list:
            self.widgets["dro_%s1"%i].show()
            self.widgets["dro_%s2"%i].show()
            self.widgets["axis_%s"%i].show()

    # every 50 milli seconds this gets called
    # we add calls to the regular functions for the widgets we are using.
    # and add any extra calls/code 
    def periodic(self):
        self.gscreen.update_mdi_spindle_button()
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

# standard handler call
def get_handlers(halcomp,builder,useropts,gscreen):
     return [HandlerClass(halcomp,builder,useropts,gscreen)]
