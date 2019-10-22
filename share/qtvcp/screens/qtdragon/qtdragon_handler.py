import os
import linuxcnc
import hal

from PyQt5 import QtCore, QtWidgets
from qtvcp.widgets.mdi_line import MDILine as MDI_WIDGET
from qtvcp.widgets.gcode_editor import GcodeEditor as GCODE
from qtvcp.widgets.tool_offsetview import ToolOffsetView as TOOL_TABLE
from qtvcp.widgets.origin_offsetview import OriginOffsetView as OFFSET_VIEW
from qtvcp.lib.keybindings import Keylookup
from qtvcp.core import Status, Action, Info
from qtvcp import logger
from qtvcp.widgets.stylesheeteditor import  StyleSheetEditor as SSE
#from qtvcp.lib.notify import Notify

LOG = logger.getLogger(__name__)
KEYBIND = Keylookup()
STAT = Status()
INFO = Info()
ACTION = Action()
#NOTIFY = Notify()

class HandlerClass:
    def __init__(self, halcomp, widgets, paths):
        self.w = widgets
        self.PATHS = paths
        self.hal = halcomp
        INIPATH = os.environ.get('INI_FILE_NAME', '/dev/null')
        self.inifile = linuxcnc.ini(INIPATH)
        self.STYLEEDITOR = SSE(widgets,paths)
        KEYBIND.add_call('Key_F12','on_keycall_F12')
        
#        STAT.connect('general',self.return_value)
        STAT.connect('state-on', self.machine_on)
        STAT.connect('state-off', self.machine_off)
        STAT.connect('gcode-line-selected', self.gcode_line_selected)
        STAT.connect('hard-limits-tripped', self.hard_limit_tripped)
        STAT.connect("interp-idle", self.interp_idle_changed)
        STAT.connect("user-system-changed", self.user_system_changed)
        STAT.connect("tool-in-spindle-changed", self.tool_in_spindle_changed)
        STAT.connect("file-loaded", self.file_loaded)
        STAT.connect("all-homed", self.all_homed)
        STAT.connect("not-all-homed", self.not_homed)

# some global variables
        self.axis_list = INFO.AVAILABLE_AXES
        self.joint_list = INFO.AVAILABLE_JOINTS
        self.max_velocity = INFO.MAX_LINEAR_VELOCITY
        self.system_list = ["G53","G54","G55","G56","G57","G58","G59","G59.1","G59.2","G59.3"]
        self.home_location_x = self.inifile.find('JOINT_0', 'HOME')
        self.home_location_y = self.inifile.find('JOINT_1', 'HOME')
        self.home_location_z = self.inifile.find('JOINT_2', 'HOME')
        self.tool_sensor_x = self.inifile.find('TOOLSENSOR', 'X')
        self.tool_sensor_y = self.inifile.find('TOOLSENSOR', 'Y')
        self.laser_offset_x = self.inifile.find('LASER', 'X')
        self.laser_offset_y = self.inifile.find('LASER', 'Y')
        self.homed = False
        self.start_line = 0
        self.program_length = 0
        self.slow_jog_factor = 10
        self.tool_in_spindle = 0
        self.reload_tool = 0
        self.last_loaded_program = ""
        self.onoff_list = ["widget_controls", "Program_frame", "DRO_frame"]

    def initialized__(self):
        self.init_pins()
        self.init_preferences()
        self.init_widgets()
        self.init_locations()

    # initialize DRO style
        for i in map(str.lower, self.axis_list):
            self.w["dro_axis_{}".format(i)].setStyle(self.w["dro_axis_{}".format(i)].style())

    #############################
    # SPECIAL FUNCTIONS SECTION #
    #############################
    def init_pins(self):
        # these pins are needed so that the touchoff subroutine can read the variables
        self.hal.newpin("touch_height", hal.HAL_FLOAT, hal.HAL_OUT)
        self.hal.newpin("sensor_height", hal.HAL_FLOAT, hal.HAL_OUT)
        self.hal.newpin("zero_height", hal.HAL_FLOAT, hal.HAL_OUT)
        self.hal.newpin("search_vel", hal.HAL_FLOAT, hal.HAL_OUT)
        self.hal.newpin("probe_vel", hal.HAL_FLOAT, hal.HAL_OUT)
        self.hal.newpin("max_probe", hal.HAL_FLOAT, hal.HAL_OUT)
        
    def init_preferences(self):
        if self.w.PREFS_:
            self.reload_tool = self.w.PREFS_.getpref('Tool to load', 0, int,'CUSTOM_FORM_ENTRIES')
            self.last_loaded_program = self.w.PREFS_.getpref('last_file_path', None, str,'BOOK_KEEPING')
            temp1 = self.w.PREFS_.getpref('Reload program', False, bool,'CUSTOM_FORM_ENTRIES')
            temp2 = self.w.PREFS_.getpref('Reload tool', False, bool,'CUSTOM_FORM_ENTRIES')
            temp3 = self.w.PREFS_.getpref('Tool sensor', False, bool,'CUSTOM_FORM_ENTRIES')
        else:
            temp1 = temp2 = temp3 = False
            self.add_alarm("No preference file found")
        self.w.checkBox_reload_program.setChecked(temp1)
        self.w.checkBox_reload_tool.setChecked(temp2)
        self.w.checkBox_tool_sensor.setChecked(temp3)
        self.chk_tool_sensor(temp3)

    def init_widgets(self):
        self.w.main_tab_widget.setCurrentIndex(0)
        self.w.slider_jog.setMaximum(self.max_velocity * 60)
        self.w.slider_jog.setValue(INFO.DEFAULT_LINEAR_JOG_VEL)
        self.w.slider_maxv.setMaximum(self.max_velocity * 60)
        self.w.slider_maxv.setValue(self.max_velocity * 60)
        self.w.slider_feed.setMaximum(INFO.MAX_FEED_OVERRIDE)
        self.w.slider_feed.setValue(100)
        self.w.slider_rapid.setMaximum(100)
        self.w.slider_rapid.setValue(100)
        self.w.slider_spindle.setMinimum(INFO.MIN_SPINDLE_OVERRIDE)
        self.w.slider_spindle.setMaximum(INFO.MAX_SPINDLE_OVERRIDE)
        self.w.slider_spindle.setValue(100)
        self.w.checkBox_override_limits.setChecked(False)
        self.w.checkBox_override_limits.setEnabled(False)
        self.w.filemanager.show()
        self.w.gcode_editor.hide()
        self.w.btn_from_line.setEnabled(False)
       
    def init_locations(self):
        touch_height = self.inifile.find('TOOLSENSOR', 'TOUCH_HEIGHT') or "50"
        sensor_height = self.inifile.find('TOOLSENSOR', 'SENSOR_HEIGHT') or "50"
        max_probe = self.inifile.find('TOOLSENSOR', 'MAXPROBE') or "40"
        search_vel = self.inifile.find('TOOLSENSOR', 'SEARCH_VEL') or "200"
        probe_vel = self.inifile.find('TOOLSENSOR', 'PROBE_VEL') or "50"
        self.w.lbl_maxv.setText(str(self.max_velocity * 60))
        self.w.lbl_touch_height.setText(touch_height)
        self.w.lbl_sensor_height.setText(sensor_height)
        self.w.lbl_maxprobe.setText(max_probe)
        self.w.lbl_search_vel.setText(search_vel)
        self.w.lbl_probe_vel.setText(probe_vel)
        self.hal['touch_height'] = touch_height
        self.hal['sensor_height'] = sensor_height
        self.hal['max_probe'] = max_probe
        self.hal['search_vel'] = search_vel
        self.hal['probe_vel'] = probe_vel
        # home location
        if not self.home_location_x or not self.home_location_y or not self.home_location_z:
            self.w.btn_go_home.setEnabled(False)
            self.w.groupBox_home.hide()
            self.w.lbl_no_home.show()
            self.add_alarm("No valid home location found")
        else:
            self.w.lbl_home_x.setText(self.home_location_x)
            self.w.lbl_home_y.setText(self.home_location_y)
            self.w.lbl_no_home.hide()
        # laser offsets
        if not self.laser_offset_x or not self.laser_offset_y:
            self.w.btn_ref_laser.setEnabled(False)
            self.w.btn_laser_on.setEnabled(False)
            self.w.groupBox_laser.hide()
            self.w.lbl_no_laser_offsets.show()
            self.add_alarm("No valid laser offsets found")
        else:
            self.w.lbl_laser_x.setText(self.laser_offset_x)
            self.w.lbl_laser_y.setText(self.laser_offset_y)
            self.w.lbl_no_laser_offsets.hide()
        # tool sensor location
        if not self.tool_sensor_x or not self.tool_sensor_y:
            self.w.chk_tool_sensor.hide()
            self.w.btn_go_g30.setEnabled(False)
            self.w.groupBox_sensor.hide()
            self.w.lbl_no_toolsensor.show()
            self.add_alarm("No valid tool sensor location found")
        else:
            self.w.lbl_sensor_x.setText(self.tool_sensor_x)
            self.w.lbl_sensor_y.setText(self.tool_sensor_y)
            self.w.lbl_no_toolsensor.hide()

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
    def machine_on(self, obj):
        for widget in self.onoff_list:
            self.w[widget].setEnabled(True)

    def machine_off(self, obj):
        for widget in self.onoff_list:
            self.w[widget].setEnabled(False)

    def gcode_line_selected(self, obj, data):
        if self.w.btn_from_line.isChecked():
            self.start_line = data
            self.w.btn_start.setText("START FROM {}".format(data))

    def interp_idle_changed(self, obj):
        self.start_line = 0
        self.w.btn_start.setText("START FROM 0")
        
    def user_system_changed(self, obj, data):
        sys = self.system_list[int(data)]
        self.w.actionbutton_rel.setText(sys)

    def tool_in_spindle_changed(self, obj, data):
        self.tool_in_spindle = data

    def file_loaded(self, obj, filename):
        if filename is not None:
            self.w.progressBar.setValue(0)
            self.last_loaded_program = filename
            fileobject = file(filename, 'r')
            lines = fileobject.readlines()
            fileobject.close()
            self.program_length = len(lines)
            self.start_line = 0
            self.w.btn_from_line.setEnabled(True)
            self.w.btn_start.setText("START FROM 0")
        else:
            self.w.btn_from_line.setEnabled(False)
            self.add_alarm("Filename not valid")

    def all_homed(self, obj):
        self.homed = True
        self.w.actionbutton_view_p.click()
        for i in map(str.lower, self.axis_list):
            self.w["dro_axis_{}".format(i)].setProperty('homed', True)
            self.w["dro_axis_{}".format(i)].setStyle(self.w["dro_axis_{}".format(i)].style())
        if self.tool_in_spindle == 0 and self.w.checkBox_reload_tool.checkState():
            command = "M61 Q{}".format(self.reload_tool)
            ACTION.CALL_MDI_WAIT(command)
        if self.last_loaded_program and self.w.checkBox_reload_program.checkState():
            ACTION.OPEN_PROGRAM(self.last_loaded_program)
            self.w.filemanager.updateDirectoryView(self.last_loaded_program)

    def not_homed(self, obj, data):
        self.homed = False
        for i in map(str.lower, self.axis_list):
            self.w["dro_axis_{}".format(i)].setProperty('homed', False)
            self.w["dro_axis_{}".format(i)].setStyle(self.w["dro_axis_{}".format(i)].style())

    def hard_limit_tripped(self, obj, tripped, list_of_tripped):
        self.w.checkBox_override_limits.setEnabled(tripped)
        if not tripped:
            self.w.checkBox_override_limits.setChecked(False)
    
    #######################
    # CALLBACKS FROM FORM #
    #######################
    # program frame
    def btn_start_clicked(self):
        if not STAT.is_auto_mode():
            return
        self.w.btn_from_line.setChecked(False)
        self.add_alarm("Started program from line {}".format(self.start_line))
        ACTION.RUN(self.start_line)

    def btn_reload_file_clicked(self):
        if self.last_loaded_program:
            self.w.progressBar.setValue(0)
            ACTION.OPEN_PROGRAM(self.last_loaded_program)

    # tool frame
    def btn_go_home_clicked(self):
        ACTION.CALL_MDI_WAIT("G53 G0 Z0")
        command = "G53 G0 X{} Y{}".format(self.w.lbl_home_x.text(), self.w.lbl_home_y.text())
        ACTION.CALL_MDI_WAIT(command)

    def btn_go_g30_clicked(self):
        ACTION.CALL_MDI_WAIT("G53 G0 Z0")
        command = "G53 G0 X{} Y{}".format(self.w.lbl_sensor_x.text(), self.w.lbl_sensor_y.text())
        ACTION.CALL_MDI_WAIT(command)

    def btn_ref_laser_clicked(self):
        command = "G10 L20 P0 X{} Y{}".format(self.w.lbl_laser_x.text(), self.w.lbl_laser_y.text())
        ACTION.CALL_MDI_WAIT(command)

    def btn_from_line_clicked(self, state):
        if state is False:
            self.start_line = 0
            self.w.btn_start.setText("START FROM 0")

    def btn_touchoff_clicked(self):
        code = None
        if self.tool_in_spindle == 0:
            self.add_alarm("Cannot probe with no tool loaded")
            return
        if self.w.btn_touchoff.text() == "TOOL\nSENSOR":
            self.hal['zero_height'] = self.w.lineEdit_zero_height.text().encode('utf-8')
            code = "o< tool_sensor > call"
        elif self.w.btn_touchoff.text() == "TOUCH\nPLATE":
            code = "o< touch_plate > call"
        if not code is None:
            ACTION.CALL_OWORD(code)
            STAT.emit('forced-update')

    # override frame
    def btn_slow_clicked(self, state):
        if state:
            current = self.w.slider_jog.value()
            self.w.btn_slow.setText("SLOW")
            self.w.slider_jog.setMaximum(self.max_velocity * 60 / self.slow_jog_factor)
            self.w.slider_jog.setValue(current / self.slow_jog_factor)
        else:
            current = self.w.slider_jog.value()
            self.w.btn_slow.setText("FAST")
            self.w.slider_jog.setMaximum(self.max_velocity * 60)
            self.w.slider_jog.setValue(current * self.slow_jog_factor)

    def btn_maxv_max_clicked(self):
        self.w.slider_maxv.setValue(self.max_velocity * 60)

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
            ACTION.CALL_MDI_WAIT("M61 Q{}".format(checked[0]))
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

    # settings tab
    def chk_tool_sensor(self, state):
        if state:
            self.w.btn_touchoff.setText("TOOL\nSENSOR")
        else:
            self.w.btn_touchoff.setText("TOUCH\nPLATE")

    def chk_override_limits(self, state):
        if state:
            print("Override limits set")
            ACTION.SET_LIMITS_OVERRIDE()
        else:
            print("Override limits not set")
        
    #####################
    # GENERAL FUNCTIONS #
    #####################

    def kb_jog(self, state, joint, direction, fast = False, linear = True):
        if not STAT.is_man_mode() or not STAT.machine_is_on():
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
        if state and self.homed is False:
            ACTION.SET_MACHINE_HOMING(-1)

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
    
    def on_keycall_F12(self,event,state,shift,cntrl):
        if state:
            self.STYLEEDITOR.load_dialog()

    ###########################
    # **** closing event **** #
    ###########################
# items to save in preference file
    def closing_cleanup__(self):
        if self.w.PREFS_:
            self.w.PREFS_.putpref('Reload program', self.w.checkBox_reload_program.checkState(), bool, 'CUSTOM_FORM_ENTRIES')
            self.w.PREFS_.putpref('Tool to load', self.tool_in_spindle, int, 'CUSTOM_FORM_ENTRIES')
            self.w.PREFS_.putpref('Reload tool', self.w.checkBox_reload_tool.checkState(), bool, 'CUSTOM_FORM_ENTRIES')
            self.w.PREFS_.putpref('Tool sensor', self.w.checkBox_tool_sensor.checkState(), bool, 'CUSTOM_FORM_ENTRIES')

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
