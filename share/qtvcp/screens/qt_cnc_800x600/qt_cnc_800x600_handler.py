############################
# **** IMPORT SECTION **** #
############################

from PyQt5 import QtCore
from PyQt5 import QtWidgets
from PyQt5 import QtGui
from qtvcp.lib.keybindings import Keylookup
from qtvcp.lib.aux_program_loader import Aux_program_loader
from qtvcp.lib.notify import Notify
from qtvcp.widgets.dialog_widget import LcncDialog
from qtvcp.lib.preferences import Access
from qtvcp.widgets.overlay_widget import FocusOverlay

from qtvcp.core import Status, Action
import linuxcnc
import sys
import os

###########################################
# **** instantiate libraries section **** #
###########################################

KEYBIND = Keylookup()
STATUS = Status()
ACTION = Action()
AUX_PRGM = Aux_program_loader()
NOTE = Notify()
MSG = LcncDialog()
PREFS = Access()

###################################
# **** HANDLER CLASS SECTION **** #
###################################

class HandlerClass:

    ########################
    # **** INITIALIZE **** #
    ########################
    # widgets allows access to  widgets from the qtvcp files
    # at this point the widgets and hal pins are not instantiated
    def __init__(self, halcomp,widgets,paths):
        self.hal = halcomp
        self.w = widgets
        self.stat = linuxcnc.stat()
        self.cmnd = linuxcnc.command()
        self.error = linuxcnc.error_channel()
        self.jog_velocity = 10.0
        self.PATH = paths.CONFIGPATH
        self.IMAGE_PATH = paths.IMAGEDIR
        #print paths.CONFIGPATH
        # connect to GStat to catch linuxcnc events
        STATUS.connect('state-estop', self.say_estop)
        STATUS.connect('state-on', self.on_state_on)
        STATUS.connect('state-off', self.on_state_off)
        STATUS.connect('jograte-changed', self.on_jograte_changed)
        STATUS.connect('periodic', self.on_periodic)

        # Read user preferences
        self.desktop_notify = PREFS.getpref('desktop_notify', True, bool)
        self.shutdown_check = PREFS.getpref('shutdown_check', True, bool)

    ##########################################
    # Special Functions called from QTSCREEN
    ##########################################

    # at this point:
    # the widgets are instantiated.
    # the HAL pins are built but HAL is not set ready
    def initialized__(self):
        # Give notify library a reference to the statusbar
        NOTE.statusbar = self.w.statusbar
        if self.desktop_notify:
            NOTE.notify('Welcome','This is a test screen for Qtscreen',None,4)
        self.w.button_frame.setEnabled(False)
        self.w.jog_slider.setValue(self.jog_velocity)
        self.w.feed_slider.setValue(100)
        self.w.rapid_slider.setValue(100)

        # add a backgrund image

        bgpath = self.IMAGE_PATH+'/bg_800x600.png'
        self.w.setStyleSheet("#centralwidget { border-image: url(%s) 0 0 0 0 stretch stretch; }"%bgpath)
        bgpath = self.IMAGE_PATH+'/frame_bg_blue.png'
        self.w.frame.setStyleSheet("#frame { border-image: url(%s) 0 0 0 0 stretch stretch; }"%bgpath)
        bgpath = self.IMAGE_PATH+'/frame_bg_grey.png'
        self.w.frame_2.setStyleSheet("QFrame { border-image: url(%s) 0 0 0 0 stretch stretch; }"%bgpath)

        # add overlay to topWidget
        self.w.overlay = FocusOverlay(self.w)
        self.w.overlay.setGeometry(0, 0, 800, 600)
        self.w.overlay.hide()

    def processed_key_event__(self,receiver,event,is_pressed,key,code,shift,cntrl):
        # when typing in MDI, we don't want keybinding to call functions
        # so we catch and process the events directly.
        # We do want ESC, F1 and F2 to call keybinding functions though
        if self.w.mdi_line == receiver and code not in(16777216,16777264,16777216):
            if is_pressed:
                self.w.mdi_line.keyPressEvent(event)
                event.accept()
            return True
        try:
            KEYBIND.call(self,event,is_pressed,shift,cntrl)
            return True
        except AttributeError:
            print 'no function %s in handler file for-%s'%(KEYBIND.convert(event),key)
            #print 'from %s'% receiver
            return False

    ########################
    # callbacks from STATUS #
    ########################
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

    def on_jograte_changed(self, w, rate):
        self.jog_velocity = rate

    def on_error_message(self, w, message):
        NOTE.notify('Error',message,QtWidgets.QMessageBox.Information,10)

    def on_periodic(self,w):
        try:
            e = self.error.poll()
            if e:
                kind, text = e
                if kind in (linuxcnc.NML_ERROR, linuxcnc.OPERATOR_ERROR):
                    if self.desktop_notify:
                        NOTE.notify('ERROR',text,None,4)
                elif kind in (linuxcnc.NML_TEXT, linuxcnc.OPERATOR_TEXT):
                   if self.desktop_notify:
                        NOTE.notify('OP MESSAGE',text,None,4)
                elif kind in (linuxcnc.NML_DISPLAY, linuxcnc.OPERATOR_DISPLAY):
                   if self.desktop_notify:
                        NOTE.notify('DISPLAY',text,None,4)
        except:
            pass

    #######################
    # callbacks from form #
    #######################

    def zero_axis(self):
        name = self.w.sender().text()
        print name
        if 'X' in name:
            STATUS.set_axis_origin('x',0)
        elif 'Y' in name:
            STATUS.set_axis_origin('y',0)
        elif 'Z' in name:
            STATUS.set_axis_origin('z',0)

    def launch_status(self):
        AUX_PRGM.load_status()

    def launch_halmeter(self):
        AUX_PRGM.load_halmeter()

    def change_jograte(self, rate):
        ACTION.SET_JOG_RATE(float(rate))

    def change_feedrate(self, rate):
        self.cmnd.feedrate(rate/100.0)

    def change_rapidrate(self, rate):
        self.cmnd.rapidrate(rate/100.0)

    def estop_toggled(self,pressed):
        print 'estop click',pressed
        if pressed:
            self.cmnd.state(linuxcnc.STATE_ESTOP_RESET)
        else:
            self.cmnd.state(linuxcnc.STATE_ESTOP)
            self.w.overlay.text='Estopped!!'
            self.w.overlay.bg_color = QtGui.QColor(100, 0, 0,150)
            self.w.overlay.show()

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
        #print source.objectName(), 'released'
        if 'X' in source.text():
            self.continous_jog(0, 0)
        elif 'Y' in source.text():
            self.continous_jog(1, 0)
        elif 'Z' in source.text():
            self.continous_jog(2, 0)

    def home_clicked(self):
        print 'home click'
        self.cmnd.mode(linuxcnc.MODE_MANUAL)
        self.cmnd.home(-1)

    def loadfile_clicked(self):
        print 'load'
        self.w.overlay.text='Open File'
        self.w.overlay.bg_color = QtGui.QColor(0, 0, 0,200)
        self.w.overlay.show()
        #self.w.centralwidget.setEnabled(False)
        fname = QtWidgets.QFileDialog.getOpenFileName(None, 'Open file',
                os.path.join(os.path.expanduser('~'), 'linuxcnc/nc_files/examples'))
        fname =fname[0]
        print fname
        if fname:
            NOTE.notify('Loaded',str(fname),None,10)
            f = open(str(fname), 'r')
            self.cmnd.mode(linuxcnc.MODE_AUTO)
            self.cmnd.program_open(str(fname))
            STATUS.emit('file-loaded', fname)
        #self.w.centralwidget.setEnabled(True)
        self.w.overlay.hide()

    def runfile_clicked(self):
        print 'run file'
        self.cmnd.mode(linuxcnc.MODE_AUTO)
        self.cmnd.auto(linuxcnc.AUTO_RUN,0)

    def stopfile_clicked(self):
        print 'stop file'
        self.cmnd.mode(linuxcnc.MODE_AUTO)
        self.cmnd.abort()

    def pausefile_clicked(self):
        print 'pause file',STATUS.stat.paused
        if not STATUS.stat.paused:
            #self.cmnd.mode(linuxcnc.MODE_AUTO)
            self.cmnd.auto(linuxcnc.AUTO_PAUSE)
        else:
            print 'resume'
            self.cmnd.auto(linuxcnc.AUTO_RESUME)

    #####################
    # general functions #
    #####################

    def kb_jog(self, state, joint, direction, fast = False, linear = True):
        if linear:
            distance = STATUS.get_jog_increment()
            rate = STATUS.get_jograte()/60
        else:
            distance = STATUS.get_jog_increment_angular()
            rate = STATUS.get_jograte_angular()/60
        if state:
            if fast:
                rate = rate * 2
            ACTION.JOG(joint, direction, rate, distance)
        else:
            ACTION.JOG(joint, 0, 0, 0)

    def continous_jog(self, axis, direction):
        ACTION.DO_JOG(axis, direction)

    #####################
    # KEY BINDING CALLS #
    #####################

    # Machine control
    def on_keycall_ESTOP(self,event,state,shift,cntrl):
        if state:
            ACTION.SET_ESTOP_STATE(STATUS.estop_is_clear())
    def on_keycall_POWER(self,event,state,shift,cntrl):
        if state:
            ACTION.SET_MACHINE_STATE(not STATUS.machine_is_on())
    def on_keycall_HOME(self,event,state,shift,cntrl):
        if state:
            if STATUS.is_all_homed():
                ACTION.SET_MACHINE_UNHOMED(-1)
            else:
                ACTION.SET_MACHINE_HOMING(-1)
    def on_keycall_ABORT(self,event,state,shift,cntrl):
        if state:
            if STATUS.stat.interp_state == linuxcnc.INTERP_IDLE:
                self.w.close()
            else:
                self.cmnd.abort()

    # Linear Jogging
    def on_keycall_XPOS(self,event,state,shift,cntrl):
        self.kb_jog(state, 0, 1, shift)

    def on_keycall_XNEG(self,event,state,shift,cntrl):
        self.kb_jog(state, 0, -1, shift)

    def on_keycall_YPOS(self,event,state,shift,cntrl):
        self.kb_jog(state, 1, 1, shift)

    def on_keycall_YNEG(self,event,state,shift,cntrl):
        self.kb_jog(state, 1, -1, shift)

    def on_keycall_ZPOS(self,event,state,shift,cntrl):
        self.kb_jog(state, 2, 1, shift)

    def on_keycall_ZNEG(self,event,state,shift,cntrl):
        self.kb_jog(state, 2, -1, shift)

    def on_keycall_APOS(self,event,state,shift,cntrl):
        pass
        #self.kb_jog(state, 3, 1, shift, False)

    def on_keycall_ANEG(self,event,state,shift,cntrl):
        pass
        #self.kb_jog(state, 3, -1, shift, linear=False)


    ###########################
    # **** closing event **** #
    ###########################
    def closeEvent(self, event):
        self.w.overlay.text='     SHUTDOWN?'
        self.w.overlay.bg_color = QtGui.QColor(0, 0, 0,150)
        self.w.overlay.show()

        if self.shutdown_check:
            answer = MSG.showdialog('Do you want to shutdown now?',
                details='You can set a preference to not see this message',
                 display_type=MSG.YN_TYPE)
            if not answer:
                self.w.overlay.hide()
                event.ignore()
                return
        self.w.overlay.hide()
        event.accept()

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

################################
# required handler boiler code #
################################

def get_handlers(halcomp,widgets,paths):
     return [HandlerClass(halcomp,widgets,paths)]
