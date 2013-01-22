import hal
import gtk
import gladevcp.makepins # needed for the dialog's calulator widget
import pango

_MAN = 0;_MDI = 1;_AUTO = 2;_UNLOCKCODE = 123

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
    # if that returns true then the page is shown
    # otherwise the button is untoggled and the page is not shown
    def on_system_button_clicked(self,widget):
        if self.widgets.notebook_main.get_current_page() == 4:
            self.gscreen.block("system_button")
            widget.set_active(True)
            self.gscreen.unblock("system_button")
            return
        if not self.system_dialog():
            self.gscreen.block("system_button")
            widget.set_active(False)
            self.gscreen.unblock("system_button")
            return
        self.widgets.notebook_main.set_current_page(4)
        self.toggle_modes(widget)

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

    # This dialog is for unlocking the system tab
    # The unlock code number is defined at the top of the page
    def system_dialog(self):
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
        dialog.parse_geometry("400x400")
        dialog.set_decorated(False)
        dialog.show_all()
        self.widgets.data_input.set_sensitive(False)
        response = dialog.run()
        code = calc.get_value()
        dialog.destroy()
        self.widgets.data_input.set_sensitive(True)
        if response == gtk.RESPONSE_ACCEPT:
            if code == _UNLOCKCODE: return True
        return False

    # Connect to gscreens regular signals and add a couple more
    def connect_signals(self,handlers):
        self.gscreen.connect_signals(handlers)
        # connect to handler file callbacks:
        self.gscreen.widgets.metric_select.connect("clicked", self.on_metric_select_clicked)
        temp = "setup_button","mdi_button","run_button","tooledit_button","system_button","offsetpage_button"
        for cb in temp:
                i = "_sighandler_%s"% (cb)
                self.data[i] = int(self.widgets[cb].connect("toggled", self["on_%s_clicked"%cb]))

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
        self.init_sensitive_edit_mode() # local function
        self.data.sensitive_edit_mode.remove("button_menu")
        
        self.gscreen.init_state()
        for i in self.data.axis_list:
            self.widgets["dro_%s1"%i].show()
            self.widgets["dro_%s2"%i].show()
            self.widgets["axis_%s"%i].show()

    def init_sensitive_edit_mode(self):
        self.data.sensitive_edit_mode = ["button_menu","button_graphics","button_override","restart","button_v1_3","button_v1_0",
            "run_button","setup_button","mdi_button","system_button","tooledit_button","ignore_limits"]
    # every 100 milli seconds this gets called
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

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

# standard handler call
def get_handlers(halcomp,builder,useropts,gscreen):
     return [HandlerClass(halcomp,builder,useropts,gscreen)]
