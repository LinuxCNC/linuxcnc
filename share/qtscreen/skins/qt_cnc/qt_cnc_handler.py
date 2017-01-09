from PyQt4 import QtCore
from PyQt4 import QtGui
from qtscreen.keybindings import Keylookup,key_pressed
from qtscreen.aux_program_loader import Aux_program_loader
from qtvcp.qt_glib import GStat
import linuxcnc
import sys
# instantiate libraries
KEYBIND = Keylookup()
GSTAT = GStat()
AUX_PRGM = Aux_program_loader()
class HandlerClass:

    # This will be pretty standard to gain access.
    # widgets allows access to  widgets from the qtvcp files
    # at this point the widgets and hal pins are not instantiated
    def __init__(self, halcomp,widgets,paths):
        self.hal = halcomp
        self.w = widgets
        self.stat = linuxcnc.stat()
        self.cmnd = linuxcnc.command()
        self.jog_velocity = 10.0

        # connect to GStat to catch linuxcnc events
        GSTAT.connect('state-estop', self.say_estop)
        GSTAT.connect('state-on', self.on_state_on)
        GSTAT.connect('state-off', self.on_state_off)
        GSTAT.connect('jograte-changed', self.on_jograte_changed)

    def on_jograte_changed(self, w, rate):
        self.jog_velocity = rate

    def change_jograte(self, rate):
        GSTAT.set_jog_rate(float(rate))

    def say_estop(self,w):
        print 'saying estop'

    def on_state_on(self,w):
        print 'on'
        if not self.w.button_machineon.isChecked():
            self.w.button_machineon.click()

    def on_state_off(self,w):
        print 'off'
        if self.w.button_machineon.isChecked():
            self.w.button_machineon.click()

    def initialized__(self):
        print 'INIT'
        self.w.button_frame.setEnabled(False)
        self.w.jog_slider.setValue(self.jog_velocity)
        GSTAT.forced_update()
        AUX_PRGM.load_status()

    def processed_key_event__(self,event,is_pressed,key,code,shift,cntrl):
        try:
            KEYBIND.call(self,event,is_pressed,shift,cntrl)
        except AttributeError:
            print 'no function %s in handler file for-%s'%(KEYBIND.convert(event),key_pressed(event))
        return True

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
            speed = direction * self.jog_velocity/60
            self.cmnd.jog(linuxcnc.JOG_CONTINUOUS, axis, speed)

    def home_clicked(self):
        print 'home click'
        self.cmnd.mode(linuxcnc.MODE_MANUAL)
        self.cmnd.home(-1)

    def loadfile_clicked(self):
        print 'load'
        fname = QtGui.QFileDialog.getOpenFileName(self.w, 'Open file', 
                '/home/chris/linuxcnc/nc_files')
        print fname
        f = open(fname, 'r')
        with f:        
            data = f.read()
        self.w.gcode.setText(data) 
        self.cmnd.mode(linuxcnc.MODE_AUTO)
        self.cmnd.program_open(fname)
        GSTAT.emit('file-loaded', fname)

    def runfile_clicked(self):
        print 'run file'
        self.cmnd.mode(linuxcnc.MODE_AUTO)
        self.cmnd.auto(linuxcnc.AUTO_RUN,0)

    # KEY BINDING CALLS
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


    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

# standard handler call
def get_handlers(halcomp,widgets,paths):
     return [HandlerClass(halcomp,widgets,paths)]
