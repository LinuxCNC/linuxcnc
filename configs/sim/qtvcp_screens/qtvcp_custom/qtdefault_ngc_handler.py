############################
# **** IMPORT SECTION **** #
############################

from PyQt5 import QtCore
from PyQt5 import QtWidgets
from qtvcp.widgets.origin_offsetview import OriginOffsetView as OFFVIEW_WIDGET
from qtvcp.widgets.tool_offsetview import ToolOffsetView as TOOLVIEW_WIDGET
from qtvcp.widgets.dialog_widget import CamViewDialog as CAMVIEW
from qtvcp.widgets.dialog_widget import MacroTabDialog as LATHEMACRO
from qtvcp.widgets.mdi_line import MDILine as MDI_WIDGET
from qtvcp.widgets.gcode_editor import GcodeEditor as GCODE
from qtvcp.lib.keybindings import Keylookup
from qtvcp.lib.notify import Notify

from qtvcp.core import Status, Action

# Set up logging
from qtvcp import logger

import linuxcnc
import sys
import os

###########################################
# **** instantiate libraries section **** #
###########################################

KEYBIND = Keylookup()
STATUS = Status()
ACTION = Action()
LOG = logger.getLogger(__name__)
# Set the log level for this module
#LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

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
        self.PATH = paths.CONFIGPATH
        self.IMAGE_PATH = paths.IMAGEDIR
        self.STYLE = Styles(widgets, paths)
    ##########################################
    # Special Functions called from QTVCP
    ##########################################

    # at this point:
    # the widgets are instantiated.
    # the HAL pins are built but HAL is not set ready
    def initialized__(self):
        if self.w.PREFS_:
            print 'Using preference file:',self.w.PREFS_.fn
        STATUS.emit('play-alert','SPEAK This is a test screen for Qt V C P')
        # set custom theme
        self.STYLE.dark_style()
        KEYBIND.add_call('Key_F3','on_keycall_F3')
        KEYBIND.add_call('Key_F4','on_keycall_F4')
        KEYBIND.add_call('Key_F5','on_keycall_F5')
        KEYBIND.add_call('Key_F6','on_keycall_F6')

    def processed_key_event__(self,receiver,event,is_pressed,key,code,shift,cntrl):
        # when typing in MDI, we don't want keybinding to call functions
        # so we catch and process the events directly.
        # We do want ESC, F1 and F2 to call keybinding functions though
        if code not in(QtCore.Qt.Key_Escape,QtCore.Qt.Key_F1 ,QtCore.Qt.Key_F2,
                    QtCore.Qt.Key_F3,QtCore.Qt.Key_F5,QtCore.Qt.Key_F5):
            if isinstance(receiver, OFFVIEW_WIDGET) or \
                isinstance(receiver, MDI_WIDGET) or isinstance(receiver, TOOLVIEW_WIDGET):
                if is_pressed:
                    receiver.keyPressEvent(event)
                    event.accept()
                return True
            elif isinstance(receiver, GCODE) and STATUS.is_man_mode() == False:
                if is_pressed:
                    receiver.keyPressEvent(event)
                    event.accept()
                return True
            elif isinstance(receiver,QtWidgets.QDialog):
                print 'dialog'
                return True
        try:
            KEYBIND.call(self,event,is_pressed,shift,cntrl)
            return True
        except Exception as e:
            LOG.info('Exception loading Macros:', exc_info=e)
            print 'Error in, or no function for: %s in handler file for-%s'%(KEYBIND.convert(event),key)
            print 'from %s'% receiver
            return False

    ########################
    # callbacks from STATUS #
    ########################

    #######################
    # callbacks from form #
    #######################

    #####################
    # general functions #
    #####################

    #####################
    # KEY BINDING CALLS #
    #####################
    def on_keycall_ABORT(self,event,state,shift,cntrl):
        if state:
            print 'abort'
            if STATUS.stat.interp_state == linuxcnc.INTERP_IDLE:
                self.w.close()
            else:
                print 'abort'
                self.cmnd.abort()
            self.w.button_home.click()

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

    def on_keycall_F3(self,event,state,shift,cntrl):
        if state:
            self.w.originoffsetdialog.load_dialog()
    def on_keycall_F4(self,event,state,shift,cntrl):
        if state:
            self.w.camviewdialog.load_dialog()
    def on_keycall_F5(self,event,state,shift,cntrl):
        if state:
            self.w.macrotabdialog.load_dialog()
    def on_keycall_F6(self,event,state,shift,cntrl):
        if state:
            self.w.tooloffsetdialog.load_dialog()

    def on_keycall_XPOS(self,event,state,shift,cntrl):
        if state:
            STATUS.do_jog(0, 1, STATUS.current_jog_distance)
        else:
            STATUS.do_jog(0, 0, STATUS.current_jog_distance)
    def on_keycall_XNEG(self,event,state,shift,cntrl):
        if state:
            STATUS.do_jog(0, -1, STATUS.current_jog_distance)
        else:
            STATUS.do_jog(0, 0, STATUS.current_jog_distance)

    def on_keycall_YPOS(self,event,state,shift,cntrl):
        if state:
            STATUS.do_jog(1, 1, STATUS.current_jog_distance)
        else:
            STATUS.do_jog(1, 0, STATUS.current_jog_distance)

    def on_keycall_YNEG(self,event,state,shift,cntrl):
        if state:
            STATUS.do_jog(1, -1, STATUS.current_jog_distance)
        else:
            STATUS.do_jog(1, 0, STATUS.current_jog_distance)

    def on_keycall_ZPOS(self,event,state,shift,cntrl):
        if state:
            STATUS.do_jog(2, 1, STATUS.current_jog_distance)
        else:
            STATUS.do_jog(2, 0, STATUS.current_jog_distance)

    def on_keycall_ZNEG(self,event,state,shift,cntrl):
        if state:
            STATUS.do_jog(2, -1, STATUS.current_jog_distance)
        else:
            STATUS.do_jog(2, 0, STATUS.current_jog_distance)

    ###########################
    # **** closing event **** #
    ###########################
    def closing_cleanup__(self):
        print'CLOSING'

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

#####################################################################
# Styles - put them here to keep things tidy
#####################################################################
class Styles:
    def __init__(self, widgets, paths):
        self.w = widgets
        self.PATH = paths.CONFIGPATH
        self.IMAGE_PATH = paths.IMAGEDIR

    def bright_style(self):
        self.w.setStyleSheet('''#MainWindow {background: black; }
QLineEdit {
background: qradialgradient(cx: 0.3, cy: -0.4,
fx: 0.3, fy: -0.4,
radius: 1.35, stop: 0 #fff, stop: 1 #888);
padding: 1px;
border-style: solid;
border: 2px solid gray;
border-radius: 8px;
}

QPushButton {
color: #333;
border: 2px solid #555;
border-radius: 11px;
padding: 5px;
background: qradialgradient(cx: 0.3, cy: -0.4,
fx: 0.3, fy: -0.4,
radius: 1.35, stop: 0 #fff, stop: 1 #888);
min-width: 40px;
}

 QPushButton:hover {
background: qradialgradient(cx: 0.3, cy: -0.4,
fx: 0.3, fy: -0.4,
radius: 1.35, stop: 0 #fff, stop: 1 #bbb);
}

QPushButton:pressed {
background: qradialgradient(cx: 0.4, cy: -0.1,
fx: 0.4, fy: -0.1,
radius: 1.35, stop: 0 #fff, stop: 1 #ddd);
}
QPushButton:checked {
background: qradialgradient(cx: 0.4, cy: -0.1,
fx: 0.4, fy: -0.1,
radius: 1.35, stop: 0 #fff, stop: 1 #ddd);
}

QSlider::groove:horizontal {
border: 1px solid #bbb;
background: white;
height: 5px;
border-radius: 4px;
}

QSlider::sub-page:horizontal {
background: qlineargradient(x1: 0, y1: 0,    x2: 0, y2: 1,
    stop: 0 #66e, stop: 1 #bbf);
background: qlineargradient(x1: 0, y1: 0.2, x2: 1, y2: 1,
    stop: 0 #bbf, stop: 1 #55f);
border: 1px solid #777;
height: 10px;
border-radius: 4px;
}

QSlider::add-page:horizontal {
background: #fff;
border: 1px solid #777;
height: 10px;
border-radius: 4px;
}

QSlider::handle:horizontal {
background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
    stop:0 #eee, stop:1 #ccc);
border: 1px solid #777;
width: 20px;
margin-top: -7px;
margin-bottom: -75px;
border-radius: 4px;

}

QSlider::handle:horizontal:hover {
background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
    stop:0 #fff, stop:1 #ddd);
border: 1px solid #444;
border-radius: 4px;
}

QSlider::sub-page:horizontal:disabled {
background: #bbb;
border-color: #999;
}

QSlider::add-page:horizontal:disabled {
background: #eee;
border-color: #999;
}

QSlider::handle:horizontal:disabled {
background: #eee;
border: 1px solid #aaa;
border-radius: 4px;
min-height: 30px;
}
#frame_man { border: 3px solid gray;border-radius: 15px;
background: QLinearGradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #0000ff, stop: 1 #141529);
 }

''')

        style ='''#frame { border: 3px solid gray;border-radius: 15px;
background: QLinearGradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #0000ff, stop: 1 #141529);
 } '''
        self.w.frame.setStyleSheet(style)

        style ='''QFrame { border: 3px solid gray;border-radius: 15px;
background: QLinearGradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #0000ff, stop: 1 #141529);
 } '''
        self.w.frame_mdi.setStyleSheet(style)
        self.w.frame_auto.setStyleSheet(style)
        self.w.frame_auto_2.setStyleSheet(style)
        self.w.frame_auto_3.setStyleSheet(style)
        self.w.frame_auto_4.setStyleSheet(style)
        self.w.frame_auto_5.setStyleSheet(style)
        self.w.frame_auto_6.setStyleSheet(style)
        self.w.frame_auto_7.setStyleSheet(style)
        self.w.frame_auto_8.setStyleSheet(style)
        self.w.frame_auto_9.setStyleSheet(style)
        self.w.frame_auto_10.setStyleSheet(style)


    def dark_style(self):
        bgpath = self.IMAGE_PATH+'/hazzy_bg_black.png'
        self.w.setStyleSheet('''#MainWindow {background: black; }
QStatusBar {
background: qradialgradient(cx: 0.3, cy: -0.4,
fx: 0.3, fy: -0.4,
radius: 1.35, stop: 0 #fff, stop: 1 #888);
padding: 1px;
border-style: solid;
border: 2px solid gray;
border-radius: 8px;
}

QLineEdit {
background: qradialgradient(cx: 0.3, cy: -0.4,
fx: 0.3, fy: -0.4,
radius: 1.35, stop: 0 #fff, stop: 1 #888);
padding: 1px;
border-style: solid;
border: 2px solid gray;
border-radius: 8px;
}

QPushButton {
color: #333;
border: 2px solid #555;
border-radius: 11px;
padding: 5px;
background: qradialgradient(cx: 0.3, cy: -0.4,
fx: 0.3, fy: -0.4,
radius: 1.35, stop: 0 #fff, stop: 1 #888);
min-width: 40px;
}

 QPushButton:hover {
background: qradialgradient(cx: 0.3, cy: -0.4,
fx: 0.3, fy: -0.4,
radius: 1.35, stop: 0 #fff, stop: 1 #bbb);
}

QPushButton:pressed {
background: qradialgradient(cx: 0.4, cy: -0.1,
fx: 0.4, fy: -0.1,
radius: 1.35, stop: 0 #fff, stop: 1 #ddd);
}
QPushButton:checked {
background: qradialgradient(cx: 0.4, cy: -0.1,
fx: 0.4, fy: -0.1,
radius: 1.35, stop: 0 #fff, stop: 1 #ddd);
}

QSlider::groove:horizontal {
border: 1px solid #bbb;
background: white;
height: 5px;
border-radius: 4px;
}

QSlider::sub-page:horizontal {
background: qlineargradient(x1: 0, y1: 0,    x2: 0, y2: 1,
    stop: 0 #66e, stop: 1 #bbf);
background: qlineargradient(x1: 0, y1: 0.2, x2: 1, y2: 1,
    stop: 0 #bbf, stop: 1 #55f);
border: 1px solid #777;
height: 10px;
border-radius: 4px;
}

QSlider::add-page:horizontal {
background: #fff;
border: 1px solid #777;
height: 10px;
border-radius: 4px;
}

QSlider::handle:horizontal {
background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
    stop:0 #eee, stop:1 #ccc);
border: 1px solid #777;
width: 20px;
margin-top: -7px;
margin-bottom: -75px;
border-radius: 4px;

}

QSlider::handle:horizontal:hover {
background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
    stop:0 #fff, stop:1 #ddd);
border: 1px solid #444;
border-radius: 4px;
}

QSlider::sub-page:horizontal:disabled {
background: #bbb;
border-color: #999;
}

QSlider::add-page:horizontal:disabled {
background: #eee;
border-color: #999;
}

QSlider::handle:horizontal:disabled {
background: #eee;
border: 1px solid #aaa;
border-radius: 4px;
min-height: 30px;
}
#frame_man { border: 3px solid gray;border-radius: 15px;
background: QLinearGradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #38395a, stop: 1 #141529);
 }

''')
        bgpath = self.IMAGE_PATH+'/frame_bg_blue.png'
        self.w.frame.setStyleSheet("#frame { border-image: url(%s) 0 0 0 0 stretch stretch; }"%bgpath)
        #bgpath = self.IMAGE_PATH+'/Grey.jpg'
        
        style ='''QFrame { border: 3px solid gray;border-radius: 15px;
background: QLinearGradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #38395a, stop: 1 #141529);
 } '''
        self.w.frame_mdi.setStyleSheet(style)
        self.w.frame_auto.setStyleSheet(style)
        self.w.frame_auto_2.setStyleSheet(style)
        self.w.frame_auto_3.setStyleSheet(style)
        self.w.frame_auto_4.setStyleSheet(style)
        self.w.frame_auto_5.setStyleSheet(style)
        self.w.frame_auto_6.setStyleSheet(style)
        self.w.frame_auto_7.setStyleSheet(style)
        self.w.frame_auto_8.setStyleSheet(style)
        self.w.frame_auto_9.setStyleSheet(style)
        self.w.frame_auto_10.setStyleSheet(style)
