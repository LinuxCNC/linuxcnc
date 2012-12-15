import hal
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

    # erase the ready-to-home message on statusbar
    def on_hal_status_all_homed(self,widget):
        print "all-homed"
        self.data.all_homed = True
        self.widgets.statusbar1.remove_message(self.gscreen.statusbar_id,self.gscreen.homed_status_message)

    # This connects siganals without using glade's autoconnect method
    # in this case to destroy the window
    # it calls the method in gscreen: gscreen.on_window_destroy()
    # and run-at-line dialog
    def connect_signals(self,handlers):
        signal_list = [ ["window1","destroy", "on_window1_destroy"],
                        ["restart_ok","clicked", "restart_dialog_return", True],
                        ["restart_cancel","clicked", "restart_dialog_return", False],
                        ["restart","clicked", "launch_restart_dialog"],
                        ["restart_line_up","clicked", "restart_up"],
                        ["restart_line_down","clicked", "restart_down"],
                        ["restart_line_input","value_changed", "restart_set_line"],
                        ["metric_select","clicked","on_metric_select_clicked"],
                    ]
        for i in signal_list:
            if len(i) == 3:
                self.gscreen.widgets[i[0]].connect(i[1], self.gscreen[i[2]])
            elif len(i) == 4:
                self.gscreen.widgets[i[0]].connect(i[1], self.gscreen[i[2]],i[3])

    # We don't want Gscreen to initialize it's regular widgets because this custom
    # screen doesn't have most of them. So we add this function call.
    # Since this custom screen uses gladeVCP magic for its interaction with linuxcnc
    # We don't add much to this function, but we do want to be able to change the theme so:
    # We change the GTK theme to what's in gscreen's preference file.
    # gscreen.change_theme() is a method in gscreen that changes the GTK theme of window1
    # gscreen.data.theme_name is the name of the theme from the preference file 
    # To truely be friendly, we should add a way to change the theme directly in the custom screen.
    # we also set up the statusbar and add a ready-to-home message
    def initialize_widgets(self):
        self.gscreen.change_theme(self.data.theme_name)
        self.gscreen.statusbar_id = self.widgets.statusbar1.get_context_id("Statusbar1")
        self.gscreen.homed_status_message = self.widgets.statusbar1.push(1,"Ready For Homing")

    # If we need extra HAL pins here is where we do it.
    # Note you must import hal at the top of this script to do it.
    # For gaxis there is no extra pins but since we don't want gscreen to
    # add it's default pins we added this function
    def initialize_pins(self):
        pass

    # every 50 milli seconds this gets called
    # add pass so gscreen doesn't try to update it's regular widgets or
    # add the individual function names that you would like to call.
    # In this case we wish to call Gscreen's default function for units button update
    def periodic(self):
        self.gscreen.update_units_button_label()

# standard handler call
def get_handlers(halcomp,builder,useropts,gscreen):
     return [HandlerClass(halcomp,builder,useropts,gscreen)]
