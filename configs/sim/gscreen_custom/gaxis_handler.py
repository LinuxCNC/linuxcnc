# this is a handler file for using Gscreen's infastructure
# to load a completely custom glade screen
# The only things that really matters is that it's saved as a GTK builder project,
# the toplevel window is caller window1 (The default name) and you connect A destroy
# window signal else you can't close down linuxcnc 
class HandlerClass:

    # this will be pretty standard to gain access to everything
    # In this case we didn't specify any signals in glade so we
    # don't need any callbacks here
    def __init__(self, halcomp,builder,useropts,gscreen):
            self.nhits = 0
            self.emc = gscreen.emc
            self.data = gscreen.data
            self.widgets = gscreen.widgets
            self.gscreen = gscreen

# This connects siganals without using glade
# in this case to destroy the window
# it calls the method in gscreen
def connect_signals(gscreen):
    signal_list = [ ["window1","destroy", "on_window1_destroy"],
                    ]
    for i in signal_list:
        gscreen.widgets[i[0]].connect(i[1], gscreen[i[2]])


# you could initialize your widgets here
# overwise add the pass command so gscreen doesn't try to initialize
# the regular widgets
# here we chose to change the GTK theme to whats in gscreen's preference file
def initialize_widgets(gscreen):
    gscreen.change_theme(gscreen.data.theme_name)

# every 50 milli seconds this gets called
# add pass so gscreen does update it's regular widgets
def periodic(gscreen):
    pass

# standard handler call
def get_handlers(halcomp,builder,useropts,gscreen):
     return [HandlerClass(halcomp,builder,useropts,gscreen)]
