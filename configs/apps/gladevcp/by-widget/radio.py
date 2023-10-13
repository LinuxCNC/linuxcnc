'''
    gladevcp radiobutton example
    lifted from http://old.nabble.com/glade-and-radio-buttons.-td28793060.html

    Michael Haberler 12/2010
'''

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk
from gi.repository import Gdk
from gi.repository import GObject
from gi.repository import Pango

debug = 0

class HandlerClass:

    def print_radiobutton_state(self, widget):
        if widget.get_active():
            state = 'active'
            if self.builder.get_object('showoutput_btn').get_active():
                self.set_color_label(widget.get_label())
            else:
                self.color_lbl.set_label('Unknown')

        else:
            state = 'inactive'
        print('   %s button toggled - now %s ' % ( widget.get_label(), state))

    def on_red_rbtn_toggled(self, widget, data=None):
        print('on_red_rbtn_toggled')
        self.print_radiobutton_state(widget)

    def on_blue_rbtn_toggled(self, widget, data=None):
        print('on_blue_rbtn_toggled')
        self.print_radiobutton_state(widget)

    def on_green_rbtn_toggled(self, widget, data=None):
        print('on_green_rbtn_toggled')
        self.print_radiobutton_state(widget)

    def on_yellow_rbtn_toggled(self, widget, data=None):
        print('on_yellow_rbtn_toggled')
        self.print_radiobutton_state(widget)

    def on_showoutput_btn_toggled(self, widget, data=None):
        if widget.get_active():
            self.color_lbl.set_sensitive(True)
        else:
            self.color_lbl.set_sensitive(False)

    def set_color_label(self, color):
        self.color_lbl.set_label(color)
        mycolor = self.color_lookup[color.lower()]
        self.color_lbl.modify_fg(Gtk.StateFlags.NORMAL, mycolor)

    def on_destroy(self,obj,data=None):
        print("on_destroy")
        self.halcomp.exit() # avoid lingering HAL component
        Gtk.main_quit()

    def __init__(self, halcomp,builder,useropts):
        self.halcomp = halcomp
        self.builder = builder
        self.useropts = useropts

        self.window = self.builder.get_object('window1')
        self.color_lbl = self.builder.get_object('color_lbl')

        fontdesc = Pango.FontDescription("Sans 20")
        self.color_lbl.modify_font(fontdesc)

        self.color_lookup = {
                                'red':       Gdk.color_parse('red'),
                                'blue':     Gdk.color_parse('blue'),
                                'green':    Gdk.color_parse('green'),
                                'yellow':    Gdk.color_parse('yellow')
                                }
        self.on_showoutput_btn_toggled(self.builder.get_object('showoutput_btn'))

def get_handlers(halcomp,builder,useropts):

    global debug
    for cmd in useropts:
        exec(cmd, globals())

    return [HandlerClass(halcomp,builder,useropts)]

