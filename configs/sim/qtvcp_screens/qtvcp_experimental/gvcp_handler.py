import gtk

class HandlerClass:

    def __init__(self, halcomp,builder,useropts):
        self.halcomp = halcomp
        self.builder = builder
        #print 'GLADEVCP HANDLER INIT'

        # This catches the signal from Touchy to say that the tab is exposed 
        t = self.builder.get_object('eventbox1')
        t.connect('map-event',self.on_map_event)
        t.add_events(gtk.gdk.STRUCTURE_MASK)

    # This catches our messages from another program
    def event(self,w,event):
        print event.message_type,event.data
        if event.message_type == 'Gladevcp':
            if event.data[:7] == 'Visible':
                print 'VISIBLE'
            else:
                print 'HIDDEN'

    # We connect to client-events from the new toplevel widget
    def on_map_event(self, widget, data=None):
        top = widget.get_toplevel()
        print "map event"
        top.connect('client-event', self.event)

def get_handlers(halcomp,builder,useropts):
    return [HandlerClass(halcomp,builder,useropts)]

