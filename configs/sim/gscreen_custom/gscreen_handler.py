# This is a handler file for using Gscreen's infrastructure
# to load a completely custom glade screen
# The only things that really matters is that it's saved as a GTK builder project,
# the toplevel window is caller window1 (The default name) and you connect a destroy
# window signal else you can't close down linuxcnc 
class HandlerClass:

    # this will be pretty standard to gain access to everything
    # emc is for control and status of linuxcnc
    # data is important data from gscreen and linuxcnc
    # widgets is all the widgets from the glade files
    # gscreen is for access to gscreens methods
    #
    # we added setting the gremlin DRO on from the startup,
    # a global variable for the number of key presses,
    # and make only the active axis buttons visible
    def __init__(self, halcomp,builder,useropts,gscreen):
            self.emc = gscreen.emc
            self.data = gscreen.data
            self.widgets = gscreen.widgets
            self.gscreen = gscreen

            self.nhits = 0
            self.widgets.gremlin.set_property('enable_dro',True)
            for i in ("x","y","z","a","b","c","u","v","w","s"):
                if i in self.data.axis_list:
                    self.widgets["axis_%s"%i].set_visible(True)
            self.widgets.offsetpage1.set_row_visible("1",False)

    # This is a new method for a couple of widgets we added callbacks to.
    # The argument 'widget' is a reference to the actual widget that called.
    # In this way we can use this method on a bunch of widgets without knowing
    # their name ahead of time.
    def on_button_press(self,widget,data=None):
        global nhits 
        self.nhits += 1 
        widget.set_label("hits: %d" % self.nhits)

    # This method is overriden from gscreen
    # We selected this method name in the glade file as a callback.
    # Since this method name is the same as one in gscreen,
    # gscreen won't connect a callback to it's method.
    # Meaning this is the only one called.
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
    # we selected this method name in the glade file as a callback 
    def on_machine_state_clicked(self,*args):
        if self.data.estopped:
            return
        elif not self.data.machine_on:
            self.emc.machine_on(1)
            self.widgets.on_label.set_text("Machine On")
        else:
            self.emc.machine_off(1)
            self.widgets.on_label.set_text("Machine Off")

    # here we override gscreen's method of hiding the cursor
    # by writing a method with the same name that gscreen connects a signal to.
    # and our new method in fact calls a sound method and then the hide cursor method
    # that are both in gscreen
    # So now we get a sound when we hide and show the pointer
    def on_hide_cursor(self,widget):
        self.gscreen.audio.set_sound(self.data.alert_sound)
        self.gscreen.audio.run()
        self.gscreen.on_hide_cursor(None)

    # every 100 milli seconds this gets called
    # we add calls to the regular functions for the widgets we are using.
    # and add any extra calls/code 
    def periodic(self):
        self.gscreen.update_mdi_spindle_button()
        self.gscreen.update_spindle_bar()
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

def get_handlers(halcomp,builder,useropts,gscreen):
     return [HandlerClass(halcomp,builder,useropts,gscreen)]
