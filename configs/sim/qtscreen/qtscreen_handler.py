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
        self.jog_velocity = 1.0

    def initialized__(self):
        print 'INIT'
        self.w.button_frame.setEnabled(False)

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
            self.w.button_frame.setEnabled(True)
        else:
            self.cmnd.state(linuxcnc.STATE_OFF)
            self.w.button_frame.setEnabled(False)

    def jog_pressed(self):
        d = 1
        source = self.w.sender()
        print source.objectName(), 'pressed'
        if '-' in source.text():
            d = -1
        if 'X' in source.text():
            self.continous_jog(0, d)
        elif 'Y' in source.text():
            self.continous_jog(1, d)
        elif 'Z' in source.text():
            self.continous_jog(2, d)

    def jog_released(self):
        source = self.w.sender()
        print source.objectName(), 'released'
        if 'X' in source.text():
            self.continous_jog(0, 0)
        elif 'Y' in source.text():
            self.continous_jog(1, 0)
        elif 'Z' in source.text():
            self.continous_jog(2, 0)

    def continous_jog(self, axis, direction):
        if direction == 0:
            self.cmnd.jog(linuxcnc.JOG_STOP, axis)
        else:
            rate = self.jog_velocity
            self.cmnd.jog(linuxcnc.JOG_CONTINUOUS, axis, direction * rate)

    def home_clicked(self):
        print 'home click'
        self.cmnd.mode(linuxcnc.MODE_MANUAL)
        self.cmnd.home(-1)

# standard handler call
def get_handlers(halcomp,widgets):
     return [HandlerClass(halcomp,widgets)]
