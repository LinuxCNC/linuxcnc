
class HandlerClass:

    def __init__(self, halcomp,builder,useropts,gscreen):
            self.nhits = 0
            self.emc = gscreen.emc
            self.data = gscreen.data
            self.widgets = gscreen.widgets

    def on_button_press(self,widget,data=None):
        global nhits 
        self.nhits += 1 
        widget.set_label("hits: %d" % self.nhits)

    def on_estop_clicked(self,*args):
        if self.data.estopped:
            self.emc.estop_reset(1)
        else:
            self.emc.machine_off(1)
            self.emc.estop(1)
            self.widgets.on_label.set_text("Machine Off")

    def on_machine_state_clicked(self,*args):
        if self.data.estopped:
            return
        elif not self.data.machine_on:
            self.emc.machine_on(1)
            self.widgets.on_label.set_text("Machine On")
        else:
            self.emc.machine_off(1)
            self.widgets.on_label.set_text("Machine Off")

def get_handlers(halcomp,builder,useropts,gscreen):
     return [HandlerClass(halcomp,builder,useropts,gscreen)]
