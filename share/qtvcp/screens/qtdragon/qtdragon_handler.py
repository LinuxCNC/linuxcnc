import os
import linuxcnc
import hal, hal_glib
from qtvcp.lib import mdi_text as mdiText
from qtvcp.lib.gcodes import GCodes
from PyQt5 import QtCore, QtWidgets, QtGui
from qtvcp.widgets.mdi_line import MDILine as MDI_WIDGET
from qtvcp.widgets.gcode_editor import GcodeEditor as GCODE
from qtvcp.widgets.tool_offsetview import ToolOffsetView as TOOL_TABLE
from qtvcp.widgets.origin_offsetview import OriginOffsetView as OFFSET_VIEW
from qtvcp.lib.keybindings import Keylookup
from qtvcp.core import Status, Action, Info
from qtvcp import logger
from qtvcp.widgets.stylesheeteditor import  StyleSheetEditor as SSE

LOG = logger.getLogger(__name__)
KEYBIND = Keylookup()
STAT = Status()
INFO = Info()
ACTION = Action()

class HandlerClass:
    def __init__(self, halcomp, widgets, paths):
        self.h = halcomp
        self.w = widgets
        self.PATHS = paths
        INIPATH = os.environ.get('INI_FILE_NAME', '/dev/null')
        self.inifile = linuxcnc.ini(INIPATH)
        self.STYLEEDITOR = SSE(widgets,paths)
        self.GCODES = GCodes(widgets)
        self.valid = QtGui.QDoubleValidator(0.0, 999.999, 3)
        
        KEYBIND.add_call('Key_F12','on_keycall_F12')
        KEYBIND.add_call('Key_Pause', 'on_keycall_pause')
        KEYBIND.add_call('Key_Plus', 'on_keycall_plus')
        KEYBIND.add_call('Key_Minus', 'on_keycall_minus')
                
        STAT.connect('general', self.dialog_return)
        STAT.connect('state-on', lambda w: self.enable_onoff(True))
        STAT.connect('state-off', lambda w: self.enable_onoff(False))
        STAT.connect('gcode-line-selected', lambda w, line: self.set_start_line(line))
        STAT.connect('hard-limits-tripped', self.hard_limit_tripped)
        STAT.connect('interp-idle', lambda w: self.set_start_line(0))
        STAT.connect('user-system-changed', self.user_system_changed)
        STAT.connect('file-loaded', self.file_loaded)
        STAT.connect('all-homed', self.all_homed)
        STAT.connect('not-all-homed', lambda w, list: self.set_dro_homed(False))

# some global variables
        self.axis_list = INFO.AVAILABLE_AXES
        self.max_linear_velocity = INFO.MAX_LINEAR_VELOCITY
        self.max_spindle_rpm = INFO.MAX_SPINDLE_SPEED
        self.system_list = ["G53","G54","G55","G56","G57","G58","G59","G59.1","G59.2","G59.3"]
        self.slow_jog_factor = 10
        self.reload_tool = 0
        self.last_loaded_program = ""
        self.first_turnon = True
        self.onoff_list = ["frame_program", "frame_tool", "frame_dro"]
        self.axis_a_list = ["label_axis_a", "dro_axis_a", "action_zero_a", "axistoolbutton_a",
                            "action_home_a", "widget_jog_angular", "widget_increments_angular",
                            "a_plus_jogbutton", "a_minus_jogbutton"]

    def initialized__(self):
        self.init_pins()
        self.init_preferences()
        self.init_widgets()
        self.GCODES.setup_list()
        self.w.stackedWidget_log.setCurrentIndex(0)

        if "A" not in self.axis_list:
            for i in self.axis_a_list:
                self.w[i].hide()
            self.w.lbl_increments_linear.setText("INCREMENTS")

    #############################
    # SPECIAL FUNCTIONS SECTION #
    #############################
    def init_pins(self):
        pin = self.h.newpin("spindle_speed_fb", hal.HAL_FLOAT, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self.spindle_fb_changed)
        pin = self.h.newpin("spindle_amps", hal.HAL_FLOAT, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self.spindle_amps_changed)
        pin = self.h.newpin("spindle_power", hal.HAL_FLOAT, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self.spindle_power_changed)
        pin = self.h.newpin("spindle_fault", hal.HAL_U32, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self.spindle_fault_changed)
        pin = self.h.newpin("modbus-errors", hal.HAL_U32, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self.mb_errors_changed)
        
    def init_preferences(self):
        if not self.w.PREFS_:
            self.add_alarm("CRITICAL - no preference file found, enable preferences in screenoptions widget")
            return
        self.last_loaded_program = self.w.PREFS_.getpref('last_file_path', None, str,'BOOK_KEEPING')
        self.reload_tool = self.w.PREFS_.getpref('Tool to load', 0, int,'CUSTOM_FORM_ENTRIES')
        self.w.chk_reload_program.setChecked(self.w.PREFS_.getpref('Reload program', False, bool,'CUSTOM_FORM_ENTRIES'))
        self.w.chk_reload_tool.setChecked(self.w.PREFS_.getpref('Reload tool', False, bool,'CUSTOM_FORM_ENTRIES'))
        self.w.chk_use_keyboard.setChecked(self.w.PREFS_.getpref('Use keyboard', False, bool, 'CUSTOM_FORM_ENTRIES'))
        self.w.chk_run_from_line.setChecked(self.w.PREFS_.getpref('Run from line', False, bool, 'CUSTOM_FORM_ENTRIES'))
        
    def closing_cleanup__(self):
        if not self.w.PREFS_: return
        self.w.PREFS_.putpref('Tool to load', self.w.statuslabel_tool.text().encode('utf-8'), int, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Reload program', self.w.chk_reload_program.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Reload tool', self.w.chk_reload_tool.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Use keyboard', self.w.chk_use_keyboard.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Run from line', self.w.chk_run_from_line.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')

    def init_widgets(self):
        self.w.main_tab_widget.setCurrentIndex(0)
        self.w.slider_jog_linear.setMaximum(self.max_linear_velocity * 60)
        self.w.slider_jog_linear.setValue(INFO.DEFAULT_LINEAR_JOG_VEL)
        self.w.slider_jog_angular.setMaximum(INFO.MAX_ANGULAR_JOG_VEL)
        self.w.slider_jog_angular.setValue(INFO.DEFAULT_ANGULAR_JOG_VEL)
        self.w.slider_feed_ovr.setMaximum(INFO.MAX_FEED_OVERRIDE)
        self.w.slider_feed_ovr.setValue(100)
        self.w.slider_rapid_ovr.setMaximum(100)
        self.w.slider_rapid_ovr.setValue(100)
        self.w.slider_spindle_ovr.setMinimum(INFO.MIN_SPINDLE_OVERRIDE)
        self.w.slider_spindle_ovr.setMaximum(INFO.MAX_SPINDLE_OVERRIDE)
        self.w.slider_spindle_ovr.setValue(100)
        self.w.chk_override_limits.setChecked(False)
        self.w.chk_override_limits.setEnabled(False)
        self.w.filemanager.show()
        self.w.gcode_editor.hide()
        self.w.lbl_max_rapid.setText(str(self.max_linear_velocity * 60))
        self.w.lbl_search_vel.setText(self.inifile.find('TOOLSENSOR', 'SEARCH_VEL') or "200")
        self.w.lbl_probe_vel.setText(self.inifile.find('TOOLSENSOR', 'PROBE_VEL') or "50")
        self.w.lbl_max_probe.setText(self.inifile.find('TOOLSENSOR', 'MAXPROBE') or "10")
        self.w.lbl_touch_height.setText(self.inifile.find('TOOLSENSOR', 'TOUCH') or "50")
        self.w.lbl_laser_x.setText(self.inifile.find('LASER', 'X') or "100")
        self.w.lbl_laser_y.setText(self.inifile.find('LASER', 'Y') or "-10")
        self.w.lbl_home_x.setText(self.inifile.find('JOINT_0', 'HOME') or "50")
        self.w.lbl_home_y.setText(self.inifile.find('JOINT_1', 'HOME') or "50")
        #set up gcode list
        self.w.gcode_list.currentRowChanged.connect(self.list_row_changed)
        titles = mdiText.gcode_titles()
        for key in sorted(titles.iterkeys()):
            self.w.gcode_list.addItem(key + ' ' + titles[key])
        self.w.gcode_description.setReadOnly(True)

    def processed_key_event__(self,receiver,event,is_pressed,key,code,shift,cntrl):
        # when typing in MDI, we don't want keybinding to call functions
        # so we catch and process the events directly.
        # We do want ESC, F1 and F2 to call keybinding functions though
        if code not in(QtCore.Qt.Key_Escape,QtCore.Qt.Key_F1 ,QtCore.Qt.Key_F2):
#                    QtCore.Qt.Key_F3,QtCore.Qt.Key_F4,QtCore.Qt.Key_F5):

            # search for the top widget of whatever widget received the event
            # then check if it's one we want the keypress events to go to
            flag = False
            receiver2 = receiver
            while receiver2 is not None and not flag:
                if isinstance(receiver2, QtWidgets.QDialog):
                    flag = True
                    break
                if isinstance(receiver2, QtWidgets.QLineEdit):
                    flag = True
                    break
                if isinstance(receiver2, MDI_WIDGET):
                    flag = True
                    break
                if isinstance(receiver2, GCODE):
                    flag = True
                    break
                if isinstance(receiver2, TOOL_TABLE):
                    flag = True
                    break
                if isinstance(receiver2, OFFSET_VIEW):
                    flag = True
                    break
                receiver2 = receiver2.parent()

            if flag:
                if isinstance(receiver2, GCODE):
                    # if in manual do our keybindings - otherwise
                    # send events to gcode widget
                    if STAT.is_man_mode() == False:
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
            self.add_alarm('Exception in KEYBINDING: {}'.format (e))
        except Exception as e:
            LOG.error('Exception in KEYBINDING:', exc_info=e)
            print 'Error in, or no function for: %s in handler file for-%s'%(KEYBIND.convert(event),key)
            return False

    #########################
    # CALLBACKS FROM STATUS #
    #########################

    def spindle_fb_changed(self, data):
        rpm = int(self.h['spindle_speed_fb'])
        self.w.lbl_spindle_rpm.setText(str(rpm))
        if float(self.w.label_spindle_set.text()) > self.max_spindle_rpm:
            self.w.label_spindle_set.setProperty('overspeed', True)
        else:
            self.w.label_spindle_set.setProperty('overspeed', False)
        self.w.label_spindle_set.setStyle(self.w.label_spindle_set.style())

    def spindle_amps_changed(self, data):
        amps = "{:1.1f}".format(self.h['spindle_amps'])
        self.w.lbl_spindle_amps.setText(amps)

    def spindle_power_changed(self, data):
        power = "{:4.1f}".format(self.h['spindle_power'])
        self.w.lbl_spindle_power.setText(power)

    def spindle_fault_changed(self, data):
        fault = hex(self.h['spindle_fault'])
        self.w.lbl_spindle_fault.setText(fault)

    def mb_errors_changed(self, data):
        errors = self.h['modbus-errors']
        self.w.lbl_mb_errors.setText(str(errors))

    def dialog_return(self, w, message):
        rtn = message.get('RETURN')
        name = message.get('NAME')
        jog_code = bool(message.get('ID') == '_touchoff_')
        if jog_code and name == 'MESSAGE' and rtn is True:
            z_offset = float(self.w.lbl_touch_height.text())
            max_probe = self.w.lbl_max_probe.text()
            search_vel = self.w.lbl_search_vel.text()
            probe_vel = self.w.lbl_probe_vel.text()
            ACTION.CALL_MDI("G21 G49")
            ACTION.CALL_MDI("G10 L20 P0 Z0")
            ACTION.CALL_MDI("G91")
            command = "G38.2 Z-{} F{}".format(max_probe, search_vel)
            if ACTION.CALL_MDI_WAIT(command, 10) == -1: return
            if ACTION.CALL_MDI_WAIT("G1 Z4.0") == -1: return
            ACTION.CALL_MDI("G4 P0.5")
            command = "G38.2 Z-4.4 F{}".format(probe_vel)
            if ACTION.CALL_MDI_WAIT(command, 10) == -1: return
            command = "G10 L20 P0 Z{}".format(z_offset)
            ACTION.CALL_MDI(command)
            command = "G1 Z10.0 F{}".format(search_vel)
            ACTION.CALL_MDI_WAIT(command, 10)
            ACTION.CALL_MDI("G90")

    def user_system_changed(self, obj, data):
        sys = self.system_list[int(data)]
        self.w.actionbutton_rel.setText(sys)

    def file_loaded(self, obj, filename):
        if filename is not None:
            self.w.progressBar.setValue(0)
            self.last_loaded_program = filename
        else:
            self.add_alarm("Filename not valid")

    def all_homed(self, obj):
        self.set_dro_homed(True)
        if self.first_turnon is True:
            self.first_turnon = False
            if self.w.chk_reload_tool.isChecked():
                command = "M61 Q{}".format(self.reload_tool)
                ACTION.CALL_MDI(command)
            if self.last_loaded_program and self.w.chk_reload_program.isChecked():
                ACTION.OPEN_PROGRAM(self.last_loaded_program)
                self.w.filemanager.updateDirectoryView(self.last_loaded_program)
        
    def hard_limit_tripped(self, obj, tripped, list_of_tripped):
        self.w.chk_override_limits.setEnabled(tripped)
        if not tripped:
            self.w.chk_override_limits.setChecked(False)
    
    #######################
    # CALLBACKS FROM FORM #
    #######################

    # program frame
    def btn_start_clicked(self):
        if self.w.main_tab_widget.currentIndex() != 0:
            return
        if not STAT.is_auto_mode():
            self.add_alarm("Must be in AUTO mode to run a program")
            return
        self.w.btn_start.setProperty('running', True)
        self.w.btn_start.setStyle(self.w.btn_start.style())
        start_line = int(self.w.lbl_start_line.text().encode('utf-8'))
        self.add_alarm("Started program from line {}".format(start_line))
        ACTION.RUN(start_line)

    def btn_reload_file_clicked(self):
        if self.last_loaded_program:
            self.w.progressBar.setValue(0)
            ACTION.OPEN_PROGRAM(self.last_loaded_program)

    # tool frame
    def btn_go_home_clicked(self):
        ACTION.CALL_MDI_WAIT("G53 G0 Z0")
        command = "G53 G0 X{} Y{}".format(self.w.lbl_home_x.text(), self.w.lbl_home_y.text())
        ACTION.CALL_MDI_WAIT(command, 10)
 
    def btn_ref_laser_clicked(self):
        command = "G10 L20 P0 X{} Y{}".format(self.w.lbl_laser_x.text(), self.w.lbl_laser_y.text())
        ACTION.CALL_MDI(command)

    def btn_touchoff_clicked(self):
        if self.w.statuslabel_tool.text() == "0":
            self.add_alarm("Cannot touchoff with no tool loaded")
            return
        if not STAT.is_all_homed():
            self.add_alarm("Must be homed to perform tool touchoff")
            return
        max_probe = self.w.lbl_max_probe.text()
        self.add_alarm("Tool touchoff started")
        # instantiate dialog box
        info = "Jog to within {} mm of touch plate and click OK".format(max_probe)
        mess = {'NAME':'MESSAGE', 'ID':'_touchoff_', 'MESSAGE':'TOOL TOUCHOFF', 'MORE':info, 'TYPE':'OKCANCEL'}
        STAT.emit('dialog-request', mess)
        
    # override frame
    def slow_button_clicked(self, state):
        slider = self.w.sender().property('slider')
        current = self.w[slider].value()
        max = self.w[slider].maximum()
        if state:
            self.w.sender().setText("SLOW")
            self.w[slider].setMaximum(max / self.slow_jog_factor)
            self.w[slider].setValue(current / self.slow_jog_factor)
            self.w[slider].setPageStep(10)
        else:
            self.w.sender().setText("FAST")
            self.w[slider].setMaximum(max * self.slow_jog_factor)
            self.w[slider].setValue(current * self.slow_jog_factor)
            self.w[slider].setPageStep(100)

    # file tab
    def btn_gcode_edit_clicked(self, state):
        if not STAT.is_on_and_idle():
            return
        for x in ["load", "next", "prev"]:
            self.w["btn_file_{}".format(x)].setEnabled(not state)
        if state:
            self.w.filemanager.hide()
            self.w.gcode_editor.show()
            self.w.gcode_editor.editMode()
        else:
            self.w.filemanager.show()
            self.w.gcode_editor.hide()
            self.w.gcode_editor.readOnlyMode()

    # tool tab
    def btn_m61_clicked(self):
        checked = self.w.tooloffsetview.get_checked_list()
        if len(checked) > 1:
            self.add_alarm("Select only 1 tool to load")
        elif checked:
            ACTION.CALL_MDI("M61 Q{}".format(checked[0]))
        else:
            self.add_alarm("No tool selected")

    # alarm tab
    def btn_clear_alarms_clicked(self):
        STAT.emit('update-machine-log', None, 'DELETE')

    def btn_save_alarms_clicked(self):
        text = self.w.machinelog.toPlainText()
        filename = self.w.lbl_clock.text().encode('utf-8')
        filename = 'alarms_' + filename.replace(' ','_') + '.txt'
        with open(filename, 'w') as f:
            f.write(text)

    # gcode tab
    def list_row_changed(self, row):
        line = self.w.gcode_list.currentItem().text().encode('utf-8')
        text = line.split(' ')[0]
        desc = mdiText.gcode_descriptions(text) or 'No Match'
        self.w.gcode_description.clear()
        self.w.gcode_description.insertPlainText(desc)
        if text != 'null':
            words = mdiText.gcode_words()
            if text in words:
                parm = text + ' '
                for index, value in enumerate(words[text], start=0):
                    parm += value
                self.w.gcode_parameters.setText(parm)
            else:
                self.w.gcode_parameters.setText('')

    # settings tab
    def chk_override_limits_checked(self, state):
        if state:
            print("Override limits set")
            ACTION.SET_LIMITS_OVERRIDE()
        else:
            print("Override limits not set")

    def chk_run_from_line_checked(self, state):
        if not state:
            self.w.lbl_start_line.setText('1')

    #####################
    # GENERAL FUNCTIONS #
    #####################

    def kb_jog(self, state, joint, direction, fast = False, linear = True):
        if not STAT.is_man_mode() or not STAT.machine_is_on():
            self.add_alarm('Machine must be ON and in Manual mode to jog')
            return
        if linear:
            distance = STAT.get_jog_increment()
            rate = STAT.get_jograte()/60
        else:
            distance = STAT.get_jog_increment_angular()
            rate = STAT.get_jograte_angular()/60
        if state:
            if fast:
                rate = rate * 2
            ACTION.JOG(joint, direction, rate, distance)
        else:
            ACTION.JOG(joint, 0, 0, 0)

    def add_alarm(self, message):
        STAT.emit('update-machine-log', message, 'TIME')

    def alarm_added(self):
        self.w.led_alarm.setState(True)

    def tab_changed(self, index):
        self.w.btn_gcode_edit.setChecked(False)
        self.btn_gcode_edit_clicked(False)
        if index == 4:
            self.w.led_alarm.setState(False)

    def set_dro_homed(self, state):
        self.w.action_home_all.setProperty('homed', state)
        self.w.action_home_all.setStyle(self.w.action_home_all.style())
        for i in map(str.lower, self.axis_list):
            self.w["dro_axis_{}".format(i)].setProperty('homed', state)
            self.w["dro_axis_{}".format(i)].setStyle(self.w["dro_axis_{}".format(i)].style())

    def enable_onoff(self, state):
        for widget in self.onoff_list:
            self.w[widget].setEnabled(state)

    def set_start_line(self, line):
        if self.w.chk_run_from_line.isChecked():
            self.w.lbl_start_line.setText(str(line))
        else:
            self.w.lbl_start_line.setText('1')
            self.add_alarm('Run from line is disabled')

    def use_keyboard(self):
        if self.w.chk_use_keyboard.isChecked():
            return True
        else:
            self.add_alarm('Keyboard shortcuts are disabled')
            return False

    #####################
    # KEY BINDING CALLS #
    #####################

    def on_keycall_ESTOP(self,event,state,shift,cntrl):
        if state:
            ACTION.SET_ESTOP_STATE(True)

    def on_keycall_POWER(self,event,state,shift,cntrl):
        if state:
            ACTION.SET_MACHINE_STATE(False)

    def on_keycall_ABORT(self,event,state,shift,cntrl):
        if state:
            ACTION.ABORT()

    def on_keycall_HOME(self,event,state,shift,cntrl):
        if state and not STAT.is_all_homed() and self.use_keyboard():
            ACTION.SET_MACHINE_HOMING(-1)

    def on_keycall_pause(self,event,state,shift,cntrl):
        if state and STAT.is_auto_mode() and self.use_keyboard():
            ACTION.PAUSE()

    def on_keycall_XPOS(self,event,state,shift,cntrl):
        if self.use_keyboard():
            self.kb_jog(state, 0, 1, shift)

    def on_keycall_XNEG(self,event,state,shift,cntrl):
        if self.use_keyboard():
            self.kb_jog(state, 0, -1, shift)

    def on_keycall_YPOS(self,event,state,shift,cntrl):
        if self.use_keyboard():
            self.kb_jog(state, 1, 1, shift)

    def on_keycall_YNEG(self,event,state,shift,cntrl):
        if self.use_keyboard():
            self.kb_jog(state, 1, -1, shift)

    def on_keycall_ZPOS(self,event,state,shift,cntrl):
        if self.use_keyboard():
            self.kb_jog(state, 2, 1, shift)

    def on_keycall_ZNEG(self,event,state,shift,cntrl):
        if self.use_keyboard():
            self.kb_jog(state, 2, -1, shift)
    
    def on_keycall_plus(self,event,state,shift,cntrl):
        if self.use_keyboard():
            self.kb_jog(state, 3, 1, shift, False)

    def on_keycall_minus(self,event,state,shift,cntrl):
        if self.use_keyboard():
            self.kb_jog(state, 3, -1, shift, False)

    def on_keycall_F12(self,event,state,shift,cntrl):
        if state:
            self.STYLEEDITOR.load_dialog()

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

def get_handlers(halcomp, widgets, paths):
    return [HandlerClass(halcomp, widgets, paths)]
