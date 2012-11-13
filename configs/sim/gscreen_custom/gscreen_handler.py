
class HandlerClass:

    # this will be pretty standard to gain access to everything
    def __init__(self, halcomp,builder,useropts,gscreen):
            self.nhits = 0
            self.emc = gscreen.emc
            self.data = gscreen.data
            self.widgets = gscreen.widgets
            self.gscreen = gscreen

    # This is a new method for a couple of widgets we added callbacks to
    # the argument 'widget' is a reference to the actual widget that called.
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

def get_handlers(halcomp,builder,useropts,gscreen):
     return [HandlerClass(halcomp,builder,useropts,gscreen)]
