import hal
# This is a handler file for using Gscreen's infastructure
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

# **************************************************************************
# Since these next function calls are outside the HandlerClass the reference 
# names don't use self
# **************************************************************************

# This connects siganals without using glade's autoconnect method
# in this case to destroy the window
# it calls the method in gscreen: gscreen.on_window_destroy()
def connect_signals(gscreen):
    signal_list = [ ["window1","destroy", "on_window1_destroy"],
                    ]
    for i in signal_list:
        gscreen.widgets[i[0]].connect(i[1], gscreen[i[2]])


# We don't want Gscreen to initialize it's regular widgets because this custom
# screen doesn't have most of them. So we add this function call.
# Since this custom screen uses gladeVCP magic for its interaction with linuxcnc
# We don't add much to this function, but we do want to be able to change the theme so:
# We change the GTK theme to what's in gscreen's preference file.
# gscreen.change_theme() is a method in gscreen that changes the GTK theme of window1
# gscreen.data.theme_name is the name of the theme from the preference file 
# To truely be friendly, we should add a way to change the theme directly in the custom screen.
def initialize_widgets(gscreen):
    gscreen.change_theme(gscreen.data.theme_name)

# If we need extra HAL pins here is where we do it.
# Note you must import hal at the top of this script to do it.
# For gaxis there is no extra pins but since we don't want gscreen to
# add it's default pins we added this function
def initialize_pins(gscreen):
    pass

# every 50 milli seconds this gets called
# add pass so gscreen doesn't try to update it's regular widgets.
def periodic(gscreen):
    pass

# standard handler call
def get_handlers(halcomp,builder,useropts,gscreen):
     return [HandlerClass(halcomp,builder,useropts,gscreen)]
