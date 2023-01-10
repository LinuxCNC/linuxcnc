############################
# **** IMPORT SECTION **** #
############################
import sys
import os
import linuxcnc

from PyQt5 import QtCore, QtWidgets

from qtvcp.widgets.mdi_line import MDILine as MDI_WIDGET
from qtvcp.widgets.gcode_editor import GcodeEditor as GCODE
from qtvcp.widgets.stylesheeteditor import  StyleSheetEditor as SSE
from qtvcp.widgets.nurbs_editor import NurbsEditor
from qtvcp.lib.keybindings import Keylookup
from qtvcp.lib.toolbar_actions import ToolBarActions

from qtvcp.core import Status, Action

# Set up logging
from qtvcp import logger
LOG = logger.getLogger(__name__)

# Set the log level for this module
#LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

###########################################
# **** instantiate libraries section **** #
###########################################

KEYBIND = Keylookup()
STATUS = Status()
ACTION = Action()
STYLEEDITOR = SSE()
NURBSEDITOR = NurbsEditor()
TOOLBAR = ToolBarActions()

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
        self.PATH = paths
        self._big_view = -1

    ##########################################
    # Special Functions called from QTVCP
    ##########################################

    # For changing functions in widgets we can 'class patch'.
    # class patching must be done before the class is instantiated.
    # 
    def class_patch__(self):
        GCODE.exitCall = self.editor_exit

    # at this point:
    # the widgets are instantiated.
    # the HAL pins are built but HAL is not set ready
    def initialized__(self):
        STATUS.emit('play-sound','SPEAK This is a test screen for Qt V C P')
        STATUS.connect('jogincrement-changed', lambda w, d, t: self.record_jog_incr(d,t))
        KEYBIND.add_call('Key_F3','on_keycall_F3')
        KEYBIND.add_call('Key_F4','on_keycall_F4')
        KEYBIND.add_call('Key_F5','on_keycall_F5')
        KEYBIND.add_call('Key_F6','on_keycall_F6')
        KEYBIND.add_call('Key_F7','on_keycall_F7')
        KEYBIND.add_call('Key_F8','on_keycall_F8')
        KEYBIND.add_call('Key_F9','on_keycall_custom','f9 pressed tesst')
        KEYBIND.add_call('Key_F10','on_keycall_custom','f10 pressed tesst')
        KEYBIND.add_call('Key_F11','on_keycall_F11')
        KEYBIND.add_call('Key_F12','on_keycall_F12')

        TOOLBAR.configure_statusbar(self.w.statusbar,'message_controls')

        self.w.toolOffsetDialog_._geometry_string='0 0 600 400 onwindow '

    def processed_key_event__(self,receiver,event,is_pressed,key,code,shift,cntrl):
        # when typing in MDI, we don't want keybinding to call functions
        # so we catch and process the events directly.
        # We do want ESC, F1 and F2 to call keybinding functions though
        if code not in(QtCore.Qt.Key_Escape,QtCore.Qt.Key_F1 ,QtCore.Qt.Key_F2,
                    QtCore.Qt.Key_F3,QtCore.Qt.Key_F5,QtCore.Qt.Key_F6,
                    QtCore.Qt.Key_F7,QtCore.Qt.Key_F8,QtCore.Qt.Key_F12):

            # search for the top widget of whatever widget received the event
            # then check if it's one we want the keypress events to go to
            flag = False
            receiver2 = receiver
            while receiver2 is not None and not flag:
                if isinstance(receiver2, QtWidgets.QDialog):
                    flag = True
                    break
                if isinstance(receiver2, MDI_WIDGET):
                    flag = True
                    break
                if isinstance(receiver2, GCODE):
                    flag = True
                    break
                receiver2 = receiver2.parent()

            if flag:
                if isinstance(receiver2, GCODE):
                    # if in manual do our keybindings - otherwise
                    # send events to gcode widget
                    if STATUS.is_man_mode() == False:
                        if is_pressed:
                            receiver.keyPressEvent(event)
                            event.accept()
                        return True
                elif is_pressed:
                    receiver.keyPressEvent(event)
                    event.accept()
                    return True
                else:
                    event.accept()
                    return True

        if event.isAutoRepeat():return True

        # ok if we got here then try keybindings function calls
        # KEYBINDING will call functions from handler file as
        # registered by KEYBIND.add_call(KEY,FUNCTION) above
        return KEYBIND.manage_function_calls(self,event,is_pressed,key,shift,cntrl)

    ########################
    # callbacks from STATUS #
    ########################

    #######################
    # callbacks from form #
    #######################
    def widget_switch(self,data):
        self.w.widgetswitcher.show_next()
        state = False
        if self.w.widgetswitcher.get_current_number() == 1:
            state = True
        self.w.Graphics.setdro(state)
        self.w.Graphics.setoverlay(state)

    def set_edit_mode(self, num):
        if num == 2:
            self.w.gcodeeditor.editMode()
        else:
            self.w.gcodeeditor.readOnlyMode()

    #####################
    # general functions #
    #####################

    def kb_jog(self, state, joint, direction, fast = False, linear = True):
        if not STATUS.is_man_mode() or not STATUS.machine_is_on():
            return
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

    # called from 'machine on' button's python command in designer
    # to test that function
    def test_function(self, text=None):
        print(text)

    def editor_exit(self):
        self.w.gcodeeditor.exit()

    def record_jog_incr(self,d, t):
        if d != 0:
            self.L_incr = d
            self.L_text = t
            self.w.btn_toggle_continuous.safecheck(False)

    def toggle_continuous_clicked(self, state):
        if state:
            # set continuous 
            self.w.btn_toggle_continuous.incr_action()
        else:
            # reset previously recorded increment
            ACTION.SET_JOG_INCR(self.L_incr, self.L_text)

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
                ACTION.ABORT()

    # dialogs
    def on_keycall_F3(self,event,state,shift,cntrl):
        if state:
            STATUS.emit('dialog-request',{'NAME':'ORIGINOFFSET'})
    def on_keycall_F4(self,event,state,shift,cntrl):
        if state:
            STATUS.emit('dialog-request',{'NAME':'CAMVIEW','NONBLOCKING':True})
    def on_keycall_F5(self,event,state,shift,cntrl):
        if state:
            STATUS.emit('dialog-request',{'NAME':'MACROTAB'})
    def on_keycall_F6(self,event,state,shift,cntrl):
        if state:
            STATUS.emit('dialog-request',{'NAME':'TOOLOFFSET'})
    def on_keycall_F7(self,event,state,shift,cntrl):
        if state:
            STATUS.emit('dialog-request',{'NAME':'VERSAPROBE'})
    def on_keycall_F8(self,event,state,shift,cntrl):
        if state:
            STATUS.emit('dialog-request',{'NAME':'MACHINELOG','NONBLOCKING':True})
    # f9, f10  call this function with different values
    def on_keycall_custom(self,event,state,shift,cntrl,value):
        if state:
            print('custom keycall function value: ',value)
    def on_keycall_F11(self,event,state,shift,cntrl):
        if state:
            NURBSEDITOR.load_dialog()
    def on_keycall_F12(self,event,state,shift,cntrl):
        if state:
            STYLEEDITOR.load_dialog()


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
    def closing_cleanup__(self):
        pass

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
