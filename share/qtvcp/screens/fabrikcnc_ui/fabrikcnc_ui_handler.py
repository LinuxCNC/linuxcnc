############################
# **** IMPORT SECTION **** #
############################
import sys
import os
import linuxcnc

from PyQt5 import QtCore, QtWidgets

from qtvcp.widgets.mdi_line import MDILine as MDI_WIDGET
from qtvcp.widgets.gcode_editor import GcodeEditor as GCODE
from qtvcp.lib.keybindings import Keylookup
from qtvcp.lib.toolbar_actions import ToolBarActions
from qtvcp.widgets.stylesheeteditor import  StyleSheetEditor as SSE
from qtvcp.core import Status, Action, Info

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
INFO = Info()
TOOLBAR = None
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
        self.PATHS = paths
        self.STYLEEDITOR = SSE(widgets,paths)
        global TOOLBAR
        TOOLBAR = ToolBarActions(path=paths)
        STATUS.connect('general',self.return_value)
        STATUS.connect('motion-mode-changed',self.motion_mode)
        STATUS.connect('user-system-changed', self._set_user_system_text)

    ##########################################
    # Special Functions called from QTSCREEN
    ##########################################

    # at this point:
    # the widgets are instantiated.
    # the HAL pins are built but HAL is not set ready
    def initialized__(self):
        KEYBIND.add_call('Key_F12','on_keycall_F12')
        TOOLBAR.configure_submenu(self.w.menuRecent, 'recent_submenu')
        TOOLBAR.configure_submenu(self.w.menuHoming, 'home_submenu')
        TOOLBAR.configure_submenu(self.w.menuUnhome, 'unhome_submenu')
        TOOLBAR.configure_submenu(self.w.menuZeroCoordinateSystem, 'zero_systems_submenu')
        TOOLBAR.configure_action(self.w.actionEstop, 'estop')
        TOOLBAR.configure_action(self.w.actionMachineOn, 'power')
        TOOLBAR.configure_action(self.w.actionOpen, 'load')
        TOOLBAR.configure_action(self.w.actionReload, 'Reload')
        TOOLBAR.configure_action(self.w.actionRun, 'run')
        TOOLBAR.configure_action(self.w.actionPause, 'pause')
        TOOLBAR.configure_action(self.w.actionStop, 'abort')
        TOOLBAR.configure_action(self.w.actionSkip, 'block_delete')
        TOOLBAR.configure_action(self.w.actionOptionalStop, 'optional_stop')
        TOOLBAR.configure_action(self.w.actionZoomIn, 'zoom_in')
        TOOLBAR.configure_action(self.w.actionZoomOut, 'zoom_out')
        TOOLBAR.configure_action(self.w.actionFrontView, 'view_x')
        TOOLBAR.configure_action(self.w.actionSideView, 'view_y')
        TOOLBAR.configure_action(self.w.actionRotatedView, 'view_z2')
        TOOLBAR.configure_action(self.w.actionTopView, 'view_z')
        TOOLBAR.configure_action(self.w.actionPerspectiveView, 'view_p')
        TOOLBAR.configure_action(self.w.actionClearPlot, 'view_clear')
        TOOLBAR.configure_action(self.w.actionQuit, 'Quit', lambda d:self.w.close())
        TOOLBAR.configure_action(self.w.actionProperties, 'gcode_properties')
        TOOLBAR.configure_action(self.w.actionCalibration, 'load_calibration')
        TOOLBAR.configure_action(self.w.actionStatus, 'load_status')
        TOOLBAR.configure_action(self.w.actionHalshow, 'load_halshow')
        TOOLBAR.configure_action(self.w.actionHalmeter, 'load_halmeter')
        TOOLBAR.configure_action(self.w.actionHalscope, 'load_halscope')
        TOOLBAR.configure_action(self.w.actionAbout, 'about')
        TOOLBAR.configure_action(self.w.actionTouchoffWorkplace, 'touchoffworkplace')
        TOOLBAR.configure_action(self.w.actionEdit, 'edit', self.edit)
        TOOLBAR.configure_action(self.w.actionTouchoffFixture, 'touchofffixture')
        TOOLBAR.configure_action(self.w.actionRunFromLine, 'runfromline')
        self.w.actionQuickRef.triggered.connect(self.quick_reference)

    def processed_key_event__(self,receiver,event,is_pressed,key,code,shift,cntrl):
        # when typing in MDI, we don't want keybinding to call functions
        # so we catch and process the events directly.
        # We do want ESC, F1 and F2 to call keybinding functions though
        if code not in(QtCore.Qt.Key_Escape,QtCore.Qt.Key_F1 ,QtCore.Qt.Key_F2,
                    QtCore.Qt.Key_F3,QtCore.Qt.Key_F5,QtCore.Qt.Key_F5):

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
                    flag = False
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

        # ok if we got here then try keybindings
        try:
            return KEYBIND.call(self,event,is_pressed,shift,cntrl)
        except NameError as e:
            LOG.debug('Exception in KEYBINDING: {}'.format (e))
        except Exception as e:
            LOG.debug('Exception in KEYBINDING:', exc_info=e)
            print 'Error in, or no function for: %s in handler file for-%s'%(KEYBIND.convert(event),key)
            return False

    ########################
    # callbacks from STATUS #
    ########################

    # process the STATUS return message from set-tool-offset
    def return_value(self, w, message):
        num = message['RETURN']
        code = bool(message['ID'] == 'FORM__')
        name = bool(message['NAME'] == 'ENTRY')
        if num is not None and code and name:
            LOG.debug('message return:{}'.format (message))
            axis = message['AXIS']
            fixture = message['FIXTURE']
            ACTION.SET_TOOL_OFFSET(axis,num,fixture)
            STATUS.emit('update-machine-log', 'Set tool offset of Axis %s to %f' %(axis, num), 'TIME')

    def motion_mode(self, w, mode):
        #print STATUS.stat.joints
        #print STATUS.stat.kinematics_type
        #print INFO.AVAILABLE_AXES
        #print INFO.GET_NAME_FROM_JOINT
        if mode == linuxcnc.TRAJ_MODE_COORD:
            print 'Mode Coordinate'
        # Joint mode
        elif mode == linuxcnc.TRAJ_MODE_FREE:
            if STATUS.stat.kinematics_type == linuxcnc.KINEMATICS_IDENTITY:
                self.show_axes()
            else:
                print 'Mode Free'
                self.show_joints()
        elif mode == linuxcnc.TRAJ_MODE_TELEOP:
            print 'Mode Teleop'
            self.show_axes()

    #######################
    # callbacks from form #
    #######################

    def tool_offset_clicked(self):
        conversion = {0:"X", 1:"Y", 2:"Z", 3:"A", 4:"B", 5:"C", 6:"U", 7:"V", 8:"W"}
        axis = STATUS.get_selected_axis()
        mess = {'NAME':'ENTRY','ID':'FORM__', 'AXIS':conversion[axis],
            'FIXTURE':self.w.actionTouchoffWorkplace.isChecked(), 'TITLE':'Set Tool Offset'}
        STATUS.emit('dialog-request', mess)
        LOG.debug('message sent:{}'.format (mess))

    #####################
    # general functions #
    #####################

    def show_joints(self):
        for i in range(0,9):
            if i in INFO.AVAILABLE_JOINTS:
                self.w['ras_label_%s'%i].show()
                self.w['ras_%s'%i].show()
                self.w['ras_label_%s'%i].setText('J%d'%i)
                try:
                    self.w['machine_label_j%d'%i].setText('<html><head/><body><p><span style=" font-size:20pt; font-weight:600;">Joint %d:</span></p></body></html>'%i)
                except:
                    pass
                continue
            self.w['ras_label_%s'%i].hide()
            self.w['ras_%s'%i].hide()

    def show_axes(self):
        for i in range(0,9):
            j = INFO.GET_NAME_FROM_JOINT.get(i)
            if j and len(j) == 1:
                self.w['ras_label_%s'%i].show()
                self.w['ras_%s'%i].show()
                self.w['ras_label_%s'%i].setText('%s'%j)
                try:
                    self.w['machine_label_j%d'%i].setText('<html><head/><body><p><span style=" font-size:20pt; font-weight:600;">Machine %s:</span></p></body></html>' %j)
                except:
                    pass
                continue
            self.w['ras_label_%s'%i].hide()
            self.w['ras_%s'%i].hide()

    def _set_user_system_text(self, w, data):
        print data
        convert = { 1:"G54 ", 2:"G55 ", 3:"G56 ", 4:"G57 ", 5:"G58 ", 6:"G59 ", 7:"G59.1 ", 8:"G59.2 ", 9:"G59.3 "}
        unit = convert[int(data)]
        for i in ('x','y','z'):
            self.w['dro_label_g5x_%s'%i].imperial_template = unit + i.upper() + '%9.4f'
            self.w['dro_label_g5x_%s'%i].metric_template = unit + i.upper() + '%10.3f'
            self.w['dro_label_g5x_%s'%i].update_units()
        self.w.dro_label_g5x_r.angular_template = unit + 'R      %3.2f'
        self.w.dro_label_g5x_r.update_units()
        self.w.dro_label_g5x_r.update_rotation(None, STATUS.stat.rotation_xy)

    def edit(self, widget, state):
        if state:
            self.w.gcode_editor.editMode()
        else:
            self.w.gcode_editor.readOnlyMode()

    def quick_reference(self):
        help1 = [
    ("F1", _("Emergency stop")),
    ("F2", _("Turn machine on")),
    ("", ""),
    ("X", _("Activate first axis")),
    ("Y", _("Activate second axis")),
    ("Z", _("Activate third axis")),
    ("A", _("Activate fourth axis")),
    ("` or 0,1..8", _("Activate first through ninth joint <br>if joints radiobuttons visible")),
    ("", _("")),
    ("`,1..9,0", _("Set Feed Override from 0% to 100%")),
    ("", _("if axes radiobuttons visible")),
    (_(", and ."), _("Select jog speed")),
    (_("< and >"), _("Select angular jog speed")),
    (_("I, Shift-I"), _("Select jog increment")),
    ("C", _("Continuous jog")),
    (_("Home"), _("Send active joint home")),
    (_("Ctrl-Home"), _("Home all joints")),
    (_("Shift-Home"), _("Zero G54 offset for active axis")),
    (_("End"), _("Set G54 offset for active axis")),
    (_("Ctrl-End"), _("Set tool offset for loaded tool")),
    ("-, =", _("Jog active axis or joint")),
    (";, '", _("Select Max velocity")),

    ("", ""),
    (_("Left, Right"), _("Jog first axis or joint")),
    (_("Up, Down"), _("Jog second axis or joint")),
    (_("Pg Up, Pg Dn"), _("Jog third axis or joint")),
    (_("Shift+above jogs"), _("Jog at traverse speed")),
    ("[, ]", _("Jog fourth axis or joint")),

    ("", ""),
    ("D", _("Toggle between Drag and Rotate mode")),
    (_("Left Button"), _("Pan, rotate or select line")),
    (_("Shift+Left Button"), _("Rotate or pan")),
    (_("Right Button"), _("Zoom view")),
    (_("Wheel Button"), _("Rotate view")),
    (_("Rotate Wheel"), _("Zoom view")),
    (_("Control+Left Button"), _("Zoom view")),
]
        help2 = [
    ("F3", _("Manual control")),
    ("F5", _("Code entry (MDI)")),
    (_("Control-M"), _("Clear MDI history")),
    (_("Control-H"), _("Copy selected MDI history elements")),
    ("",          _("to clipboard")),
    (_("Control-Shift-H"), _("Paste clipboard to MDI history")),
    ("L", _("Override Limits")),
    ("", ""),
    ("O", _("Open program")),
    (_("Control-R"), _("Reload program")),
    (_("Control-S"), _("Save g-code as")),
    ("R", _("Run program")),
    ("T", _("Step program")),
    ("P", _("Pause program")),
    ("S", _("Resume program")),
    ("ESC", _("Stop running program, or")),
    ("", _("stop loading program preview")),
    ("", ""),
    ("F7", _("Toggle mist")),
    ("F8", _("Toggle flood")),
    ("B", _("Spindle brake off")),
    (_("Shift-B"), _("Spindle brake on")),
    ("F9", _("Turn spindle clockwise")),
    ("F10", _("Turn spindle counterclockwise")),
    ("F11", _("Turn spindle more slowly")),
    ("F12", _("Turn spindle more quickly")),
    (_("Control-K"), _("Clear live plot")),
    ("V", _("Cycle among preset views")),
    ("F4", _("Cycle among preview, DRO, and user tabs")),
    ("@", _("toggle Actual/Commanded")),
    ("#", _("toggle Relative/Machine")),
    (_("Ctrl-Space"), _("Clear notifications")),
    (_("Alt-F, M, V"), _("Open a Menu")),
]
        help =  zip(help1,help2)
        msg = QtWidgets.QDialog()
        msg.setWindowTitle("Quick Reference")
        button = QtWidgets.QPushButton("Ok")
        button.clicked.connect(lambda: msg.close())
        edit = QtWidgets.QTextEdit()
        edit.setLineWrapMode(0)

        mess = '''<TABLE border="1"><COLGROUP>
                <COL><COL align="char" char="."><THEAD>
                <TR><TH>Key <TH>Command<TH>Key <TH>Command
                <TBODY>'''
        for i,j in help:
            m='<TR><TD><b>%s</b>        <TD>%s<TD><b>%s</b>        <TD>%s'%(i[0],i[1],j[0],j[1])
            mess += m
        mess += '</TABLE'
        edit.setText(mess)
        edit.setReadOnly(True)
        layout = QtWidgets.QVBoxLayout()
        layout.addWidget(edit)
        layout.addWidget(button)
        msg.setLayout(layout)
        msg.setMinimumSize(700,800)
        msg.show()
        retval = msg.exec_()


    # keyboard jogging from key binding calls
    # double the rate if fast is true 
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

    def on_keycall_F12(self,event,state,shift,cntrl):
        if state:
            self.STYLEEDITOR.load_dialog()

    ###########################
    # **** closing event **** #
    ###########################

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
