import hal

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

    # We don't want Gscreen to initialize it's regular widgets because this custom
    # screen doesn't have most of them. So we add this function call.
    # Since this custom screen uses gladeVCP magic for its interaction with linuxcnc
    # We don't add much to this function, but we do want the window to display.
    # init_show_window will do this
    def initialize_widgets(self):
        self.gscreen.init_show_windows()

    # If we need extra HAL pins here is where we do it.
    # Note you must import hal at the top of this script to do it.
    # For gaxis there is no extra pins but since we don't want gscreen to
    # add it's default pins we added this dunmmy function
    def initialize_pins(self):
        pass

    # every 100 milli seconds this gets called
    # add pass so gscreen doesn't try to update it's regular widgets or
    # add the individual function names that you would like to call.
    def periodic(self):
        pass

