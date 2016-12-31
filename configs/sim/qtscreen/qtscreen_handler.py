from PyQt4 import QtCore
from qtscreen.keybindings import Keylookup,key_pressed
from qtvcp.qt_glib import QStat
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
        self.klup = Keylookup()
        self.qstat = QStat()
        self.qstat.state_estop.connect(self.say_estop)
        self.qstat.state_on.connect(self.on_state_on)
        self.qstat.state_off.connect(self.on_state_off)
        self.qstat.jograte_changed.connect(self.on_jograte_changed)
        self.qstat.forced_update()

    def on_jograte_changed(self,rate):
        print 'jograte =',rate
        self.jog_velocity = rate

    def say_estop(self):
        print 'saying estop'

    def on_state_on(self):
        print 'on'
        if not self.w.button_machineon.isChecked():
            self.w.button_machineon.click()

    def on_state_off(self):
        print 'off'
        if self.w.button_machineon.isChecked():
            self.w.button_machineon.click()

    def initialized__(self):
        print 'INIT'
        self.w.button_frame.setEnabled(False)

    def processed_key_event__(self,event,is_pressed,key,code,shift,cntrl):
        try:
            self.klup.call(self,event,is_pressed,shift,cntrl)
        except AttributeError:
            print 'no function %s in handler file for-%s'%(self.klup.convert(event),key_pressed(event))
        return True

    def halbuttonclicked(self):
        print 'click'

    def on_keycall_ESTOP(self,event,state,shift,cntrl):
        if state:
            self.w.button_estop.click()
    def on_keycall_POWER(self,event,state,shift,cntrl):
        if state:
            self.w.button_machineon.click()
    def on_keycall_HOME(self,event,state,shift,cntrl):
        if state:
            self.w.button_home.click()

    def on_keycall_XPOS(self,event,state,shift,cntrl):
        if state:
            self.w.jog_pos_x.pressed.emit()
        else:
            self.w.jog_pos_x.released.emit()
    def on_keycall_XNEG(self,event,state,shift,cntrl):
        if state:
            self.w.jog_neg_x.pressed.emit()
        else:
            self.w.jog_neg_x.released.emit()

    def on_keycall_YPOS(self,event,state,shift,cntrl):
        if state:
            self.w.jog_pos_y.pressed.emit()
        else:
            self.w.jog_pos_y.released.emit()

    def on_keycall_YNEG(self,event,state,shift,cntrl):
        if state:
            self.w.jog_neg_y.pressed.emit()
        else:
            self.w.jog_neg_y.released.emit()

    def on_keycall_ZPOS(self,event,state,shift,cntrl):
        if state:
            self.w.jog_pos_z.pressed.emit()
        else:
            self.w.jog_pos_z.released.emit()
    def on_keycall_ZNEG(self,event,state,shift,cntrl):
        if state:
            self.w.jog_neg_z.pressed.emit()
        else:
            self.w.jog_neg_z.released.emit()



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
        #print source.objectName(), 'pressed'
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
        #print source.objectName(), 'released'
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

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

# standard handler call
def get_handlers(halcomp,widgets):
     return [HandlerClass(halcomp,widgets)]
