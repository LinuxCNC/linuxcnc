'''
    HAL combobox

    demo for adding a dynamic list of values

    Michael Haberler 12/2010
'''

#import pygtk
import gtk
import gobject

debug = 0


class HandlerClass:

    def on_destroy(self,obj,data=None):
        print "on_destroy, combobox active=%d" %(self.combo.get_active())
        self.halcomp.exit() # avoid lingering HAL component
        gtk.main_quit()

    def on_changed(self, combobox, data=None):
        print "on_changed %f %d" % (combobox.hal_pin_f.get(),combobox.hal_pin_s.get())

    def __init__(self, halcomp,builder,useropts,compname):
        self.halcomp = halcomp
        self.builder = builder
        self.useropts = useropts

        self.combo = self.builder.get_object('hal_combobox1')

def get_handlers(halcomp,builder,useropts,compname):

    global debug
    for cmd in useropts:
        exec cmd in globals()

    return [HandlerClass(halcomp,builder,useropts,compname)]

