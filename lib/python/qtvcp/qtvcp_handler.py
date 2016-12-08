import linuxcnc

class HandlerClass:

    # This will be pretty standard to gain access.
    # widgets allows access to  widgets from the qtvcp files
    # at this point the widgets and hal pins are not instantiated
    def __init__(self, halcomp,widgets):
        self.hal = halcomp
        self.w = widgets
        self.stat = linuxcnc.stat()
        self.cmnd = linuxcnc.command()

    def initialized__(self):
        print 'INIT qtvcp handler'

    def halbuttonclicked(self):
        print 'click'

    def estop_toggled(self,pressed):
        print 'estop click',pressed
        if pressed:
            self.cmnd.state(linuxcnc.STATE_ESTOP_RESET)
        else:
            self.cmnd.state(linuxcnc.STATE_ESTOP)

    def machineon_toggled(self,pressed):
        print 'machine on click',pressed
        if pressed:
            self.cmnd.state(linuxcnc.STATE_ON)
        else:
            self.cmnd.state(linuxcnc.STATE_OFF)
                

    def home_clicked(self):
        print 'home click'
        self.cmnd.mode(linuxcnc.MODE_MANUAL)
        self.cmnd.home(-1)

# standard handler call
def get_handlers(halcomp,widgets):
     return [HandlerClass(halcomp,widgets)]
