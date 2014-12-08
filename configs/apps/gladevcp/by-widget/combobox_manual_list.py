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
        print "on_destroy"
        self.halcomp.exit() # avoid lingering HAL component
        gtk.main_quit()

    def on_changed(self, combobox, data=None):
        model = combobox.get_model()
        index = combobox.get_active()
        if index:
            print 'index of selected value',index
        return


    def __init__(self, halcomp,builder,useropts,compname):
        self.halcomp = halcomp
        self.builder = builder
        self.useropts = useropts

        self.combo = self.builder.get_object('hal_combobox1')


        self.list_store = gtk.ListStore(gobject.TYPE_FLOAT)
        self.list_store.append([3.14])
        self.list_store.append([2.71828])
        self.list_store.append([1.67])
        self.list_store.append([47.11])
        self.list_store.append([42.0])
        self.combo.set_model(self.list_store)
        self.combo.set_active(0)
        cell = gtk.CellRendererText()
        self.combo.pack_start(cell, True)
        self.combo.add_attribute(cell, "text", 0)


def get_handlers(halcomp,builder,useropts,compname):

    global debug
    for cmd in useropts:
        exec cmd in globals()

    return [HandlerClass(halcomp,builder,useropts,compname)]

