'''
    gladevcp radiobutton example
    lifted from http://old.nabble.com/glade-and-radio-buttons.-td28793060.html

    Michael Haberler 12/2010
'''

import pygtk
import gtk
import pango

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
        print '   %s button toggled - now %s ' % ( widget.get_label(), state)

    def on_red_rbtn_toggled(self, widget, data=None):
        print 'on_red_rbtn_toggled'
        self.print_radiobutton_state(widget)

    def on_blue_rbtn_toggled(self, widget, data=None):
        print 'on_blue_rbtn_toggled'
        self.print_radiobutton_state(widget)

    def on_green_rbtn_toggled(self, widget, data=None):
        print 'on_green_rbtn_toggled'
        self.print_radiobutton_state(widget)

    def on_yellow_rbtn_toggled(self, widget, data=None):
        print 'on_yellow_rbtn_toggled'
        self.print_radiobutton_state(widget)

    def on_showoutput_btn_toggled(self, widget, data=None):
        if widget.get_active():
            self.color_lbl.set_sensitive(True)
        else:
            self.color_lbl.set_sensitive(False)

    def set_color_label(self, color):
        self.color_lbl.set_label(color)
        mycolor = self.color_lookup[color.lower()]
        self.color_lbl.modify_fg(gtk.STATE_NORMAL, mycolor)

    def on_destroy(self,obj,data=None):
        print "on_destroy"
        self.halcomp.exit() # avoid lingering HAL component
        gtk.main_quit()

    def __init__(self, halcomp,builder,useropts,compname):
        self.halcomp = halcomp
        self.builder = builder
        self.useropts = useropts

        self.window = self.builder.get_object('window1')
        self.color_lbl = self.builder.get_object('color_lbl')

        fontdesc = pango.FontDescription("Sans 20")
        self.color_lbl.modify_font(fontdesc)

        self.color_lookup = {
                                'red':       gtk.gdk.Color(red=65535),
                                'blue':     gtk.gdk.Color(blue=65535),
                                'green':    gtk.gdk.Color(green=65535),
                                'yellow':    gtk.gdk.color_parse('yellow')
                                }
        self.on_showoutput_btn_toggled(self.builder.get_object('showoutput_btn'))

def get_handlers(halcomp,builder,useropts,compname):

    global debug
    for cmd in useropts:
        exec cmd in globals()

    return [HandlerClass(halcomp,builder,useropts,compname)]

