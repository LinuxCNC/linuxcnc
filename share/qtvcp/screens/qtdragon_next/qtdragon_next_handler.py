# Copyright (c) 2020 Jim Sloot (persei802@gmail.com)
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
import os
import linuxcnc
from connections import Connections
from PyQt5 import QtCore, QtWidgets, QtGui, uic
from PyQt5.QtWidgets import QMessageBox
from qtvcp.widgets.gcode_editor import GcodeEditor as GCODE
from qtvcp.widgets.mdi_history import MDIHistory as MDI_WIDGET
from qtvcp.widgets.tool_offsetview import ToolOffsetView as TOOL_TABLE
from qtvcp.widgets.origin_offsetview import OriginOffsetView as OFFSET_VIEW
from qtvcp.widgets.stylesheeteditor import  StyleSheetEditor as SSE
from qtvcp.widgets.file_manager import FileManager as FM
from qtvcp.lib.keybindings import Keylookup
from qtvcp.core import Status, Action, Info, Path, Tool, Qhal
from qtvcp import logger

VERSION = 2.1
LOG = logger.getLogger(__name__)
LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL
KEYBIND = Keylookup()
STATUS = Status()
INFO = Info()
ACTION = Action()
TOOL = Tool()
PATH = Path()
QHAL = Qhal()
HELP = os.path.join(PATH.SCREENDIR, PATH.BASEPATH, "help_files")
SUBPROGRAM = os.path.join(PATH.LIBDIR, 'touchoff_subprogram.py')

# constants for main pages
TAB_MAIN = 0
TAB_FILE = 1
TAB_OFFSETS = 2
TAB_TOOL = 3
TAB_STATUS = 4
TAB_PROBE = 5
TAB_CAMVIEW = 6
TAB_UTILS = 7
TAB_SETTINGS = 8
TAB_ABOUT = 9

# status message alert levels
DEFAULT =  0
WARNING =  1
CRITICAL = 2


class HandlerClass:
    def __init__(self, halcomp, widgets, paths):
        self.h = halcomp
        self.w = widgets
        self.valid = QtGui.QDoubleValidator(-999.999, 999.999, 3)
        self.styleeditor = SSE(widgets, paths)
        self.style_path = os.path.dirname(PATH.QSS)
        self.current_style = ""
        KEYBIND.add_call('Key_F4', 'on_keycall_F4')
        KEYBIND.add_call('Key_F12','on_keycall_F12')
        KEYBIND.add_call('Key_Pause', 'on_keycall_PAUSE')
        KEYBIND.add_call('Key_Any', 'on_keycall_PAUSE')
        # references to utility objects to be initialized
        self.probe = None
        self.tool_db = None
        self.zlevel = None
        # some global variables
        self.tool_list = []
        self.next_available = 0
        self.proc = None
        self.pause_dialog = None
        self.factor = 1.0
        self.run_color = QtGui.QColor('green')
        self.stop_color = QtGui.QColor('red')
        self.pause_color = QtGui.QColor('yellow')
        self.about_html = os.path.join(PATH.SCREENDIR, PATH.BASEPATH, "help_files/about.html")
        self.start_line = 0
        self.run_time = 0
        self.runtime_save = ""
        self.time_tenths = 0
        self.timer_on = False
        self.min_spindle_rpm = int(INFO.MIN_SPINDLE_SPEED)
        self.max_spindle_rpm = int(INFO.MAX_SPINDLE_SPEED)
        self.max_linear_velocity = int(INFO.MAX_TRAJ_VELOCITY)
        self.default_linear_jog_vel = int(INFO.DEFAULT_LINEAR_JOG_VEL)
        self.max_angular_velocity = int(INFO.MAX_ANGULAR_JOG_VEL)
        self.default_angular_jog_vel = int(INFO.DEFAULT_ANGULAR_JOG_VEL)
        self.spindle_is_paused = False
        self.progress = 0
        self.feed_ovr = 1.0
        self.axis_list = INFO.AVAILABLE_AXES
        self.jog_from_name = INFO.GET_JOG_FROM_NAME
        self.system_list = ["G54","G55","G56","G57","G58","G59","G59.1","G59.2","G59.3"]
        self.slow_linear_jog = False
        self.slow_angular_jog = False
        self.slow_jog_factor = 10
        self.reload_tool = 0
        self.last_loaded_program = ""
        self.current_loaded_program = None
        self.first_turnon = True
        self.macros_defined = False
        self.source_file = ""
        self.destination_file = ""
        self.pause_delay = 0
        self.pause_timer = QtCore.QTimer()
        self.status_timer = QtCore.QTimer()
        self.status_timer.setInterval(10000) # 10 seconds
        self.icon_btns = {'action_exit': 'SP_BrowserStop'}

        self.unit_label_list = ["zoffset_units", "max_probe_units", "retract_dist_units", "z_safe_travel_units",
                                "toolsensor_units", "touchplate_units", "rotary_units"]

        self.unit_speed_list = ["search_vel_units", "probe_vel_units"]

        self.lineedit_list = ["work_height", "touch_height", "sensor_height", "laser_x", "laser_y", "camera_x",
                              "camera_y", "search_vel", "probe_vel", "retract_distance", "max_probe", "eoffset",
                              "eoffset_count", "sensor_x", "sensor_y", "z_safe_travel", "rotary_height"]

        self.auto_disable_list = ["run_from_line", "pause_spindle", "enable_comp", "touchplate", "tool_sensor"]

        self.onoff_list = ["program", "tool", "dro", "overrides", "feedrate", "spindle"]

        self.axis_a_list = ["action_zero_a", "dro_axis_a", "axistoolbutton_a", "btn_goto_zero_a", "lbl_max_angular",
                            "lbl_max_angular_vel", "lbl_angular_increment", "lbl_rotary_setting", "lineEdit_rotary_height",
                            "lbl_rotary_units", "jogincrements_angular"]

        STATUS.connect('state-on', lambda w: self.enable_onoff(True))
        STATUS.connect('state-off', lambda w: self.enable_onoff(False))
        STATUS.connect('mode-manual', lambda w: self.enable_auto(False))
        STATUS.connect('mode-mdi', lambda w: self.enable_auto(False))
        STATUS.connect('mode-auto', lambda w: self.enable_auto(True))
        STATUS.connect('gcode-line-selected', lambda w, line: self.set_start_line(line))
        STATUS.connect('hard-limits-tripped', self.hard_limit_tripped)
        STATUS.connect('user-system-changed', lambda w, data: self.user_system_changed(data))
        STATUS.connect('metric-mode-changed', lambda w, mode: self.metric_mode_changed(mode))
        STATUS.connect('tool-in-spindle-changed', lambda w, tool: self.tool_changed(tool))
        STATUS.connect('current-feed-rate', lambda w, rate: self.w.gauge_feedrate.update_value(rate * self.factor))
        STATUS.connect('command-running', self.command_running)
        STATUS.connect('command-stopped', self.command_stopped)
        STATUS.connect('file-loaded', lambda w, filename: self.file_loaded(filename))
        STATUS.connect('all-homed', self.all_homed)
        STATUS.connect('not-all-homed', self.not_all_homed)
        STATUS.connect('periodic', lambda w: self.update_status())
        STATUS.connect('interp-idle', lambda w: self.stop_timer())
        STATUS.connect('program-pause-changed', lambda w, state: self.pause_changed(state))
        STATUS.connect('override-limits-changed', lambda w, state, data: self._check_override_limits(state, data))

    def class_patch__(self):
        self.old_fman = FM.load
        FM.load = self.load_code

    def initialized__(self):
        self.init_pins()
        self.init_preferences()
        self.init_tooldb()
        self.init_widgets()
        self.init_probe()
        self.init_utils()
        self.init_joypads()
        self.w.stackedWidget_gcode.setCurrentIndex(0)
        self.w.stackedWidget_log.setCurrentIndex(0)
        self.w.stackedWidget_pgm_control.setCurrentIndex(self.w.chk_use_joypad.isChecked())
        self.w.btn_dimensions.setChecked(True)
        self.w.page_buttonGroup.buttonClicked.connect(self.main_tab_changed)
        self.w.program_buttonGroup.buttonClicked.connect(self.pgm_button_pressed)
        self.w.filemanager.onUserClicked()
        self.use_mpg_changed(self.w.chk_use_mpg.isChecked())
        self.use_camera_changed(self.w.chk_use_camera.isChecked())
        self.probe_offset_edited()
        # disable widgets for A axis if not present
        if "A" not in self.axis_list:
            for i in self.axis_a_list:
                self.w[i].hide()
        # set validators for lineEdit widgets
        for val in self.lineedit_list:
            self.w['lineEdit_' + val].setValidator(self.valid)
        self.w.lineEdit_max_power.setValidator(QtGui.QIntValidator(0, 9999))
        self.w.lineEdit_spindle_delay.setValidator(QtGui.QIntValidator(0, 99))
        # set unit labels according to machine mode
        unit = "METRIC" if INFO.MACHINE_IS_METRIC else "IMPERIAL"
        self.w.lbl_machine_units.setText(unit)
        unit = "MM" if INFO.MACHINE_IS_METRIC else "IN"
        for i in self.unit_label_list:
            self.w['lbl_' + i].setText(unit)
        for i in self.unit_speed_list:
            self.w['lbl_' + i].setText(unit + "/MIN")
        self.w.setWindowFlags(QtCore.Qt.FramelessWindowHint)
        # macro buttons defined in INI under [MDI_COMMAND_LIST]
        for i in range(10):
            button = self.w['btn_macro' + str(i)]
            num = button.property('ini_mdi_number')
            try:
                code = INFO.MDI_COMMAND_LIST[num]
                self.macros_defined = True
            except:
                button.hide()
        # if no macros defined, hide the show_macros button
        if not self.macros_defined:
            self.w.btn_show_macros.hide()
        self.w.group_macro_buttons.hide()
        # connect all signals to corresponding slots
        connect = Connections(self, self.w)
        self.w.tooloffsetview.tablemodel.layoutChanged.connect(self.get_checked_tools)
        self.w.tooloffsetview.tablemodel.dataChanged.connect(lambda new, old, roles: self.tool_data_changed(new, old, roles))
        self.status_timer.timeout.connect(self.clear_statusbar)
    #############################
    # SPECIAL FUNCTIONS SECTION #
    #############################
    def init_pins(self):
        # spindle control pins
        pin = QHAL.newpin("spindle-amps", Qhal.HAL_U32, Qhal.HAL_IN)
        pin.value_changed.connect(self.spindle_pwr_changed)
        pin = QHAL.newpin("spindle-volts", Qhal.HAL_U32, Qhal.HAL_IN)
        pin.value_changed.connect(self.spindle_pwr_changed)
        pin = QHAL.newpin("spindle-fault", Qhal.HAL_U32, Qhal.HAL_IN)
        pin.value_changed.connect(self.spindle_fault_changed)
        pin = QHAL.newpin("modbus-errors", Qhal.HAL_U32, Qhal.HAL_IN)
        pin.value_changed.connect(self.mb_errors_changed)
        pin = QHAL.newpin("modbus-fault", Qhal.HAL_BIT, Qhal.HAL_IN)
        pin.value_changed.connect(self.modbus_fault_changed)
        QHAL.newpin("spindle-inhibit", Qhal.HAL_BIT, Qhal.HAL_OUT)
        # external offset control pins
        QHAL.newpin("eoffset-count", Qhal.HAL_S32, Qhal.HAL_OUT)
        pin = QHAL.newpin("eoffset-value", Qhal.HAL_FLOAT, Qhal.HAL_IN)
        pin.value_changed.connect(self.eoffset_value_changed)
        pin = QHAL.newpin("comp-count", Qhal.HAL_S32, Qhal.HAL_IN)
        pin.value_changed.connect(self.comp_count_changed)
        pin = QHAL.newpin("map-ready", Qhal.HAL_BIT, Qhal.HAL_IN)
        pin.value_changed.connect(self.map_ready_changed)
        QHAL.newpin("comp-on", Qhal.HAL_BIT, Qhal.HAL_OUT)
        # MPG axis select pins
        pin = QHAL.newpin("axis-select-x", Qhal.HAL_BIT, Qhal.HAL_IN)
        pin.value_changed.connect(self.show_selected_axis)
        pin = QHAL.newpin("axis-select-y", Qhal.HAL_BIT, Qhal.HAL_IN)
        pin.value_changed.connect(self.show_selected_axis)
        pin = QHAL.newpin("axis-select-z", Qhal.HAL_BIT, Qhal.HAL_IN)
        pin.value_changed.connect(self.show_selected_axis)
        pin = QHAL.newpin("axis-select-a", Qhal.HAL_BIT, Qhal.HAL_IN)
        pin.value_changed.connect(self.show_selected_axis)
        # MPG encoder disable
        QHAL.newpin("mpg-disable", Qhal.HAL_BIT, Qhal.HAL_OUT)
        # MPG increment
        pin = QHAL.newpin("mpg_increment", Qhal.HAL_FLOAT, Qhal.HAL_IN)
        pin.value_changed.connect(self.mpg_increment_changed)
        
    def init_preferences(self):
        if not self.w.PREFS_:
            self.add_status("No preference file found, enable preferences in screenoptions widget", CRITICAL)
            return
        self.last_loaded_program = self.w.PREFS_.getpref('last_loaded_file', None, str,'BOOK_KEEPING')
        self.reload_tool = self.w.PREFS_.getpref('Tool to load', 0, int,'CUSTOM_FORM_ENTRIES')
        self.w.lineEdit_laser_x.setText(str(self.w.PREFS_.getpref('Laser X', 100, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_laser_y.setText(str(self.w.PREFS_.getpref('Laser Y', -20, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_camera_x.setText(str(self.w.PREFS_.getpref('Camera X', 10, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_camera_y.setText(str(self.w.PREFS_.getpref('Camera Y', 10, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_probe_x.setText(str(self.w.PREFS_.getpref('Probe X', 0, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_probe_y.setText(str(self.w.PREFS_.getpref('Probe Y', 0, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_work_height.setText(str(self.w.PREFS_.getpref('Work Height', 20, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_touch_height.setText(str(self.w.PREFS_.getpref('Touch Height', 40, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_sensor_x.setText(str(self.w.PREFS_.getpref('Sensor Location X', 20, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_sensor_y.setText(str(self.w.PREFS_.getpref('Sensor Location Y', 20, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_sensor_height.setText(str(self.w.PREFS_.getpref('Sensor Height', 40, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_search_vel.setText(str(self.w.PREFS_.getpref('Search Velocity', 40, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_probe_vel.setText(str(self.w.PREFS_.getpref('Probe Velocity', 10, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_max_probe.setText(str(self.w.PREFS_.getpref('Max Probe', 10, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_retract_distance.setText(str(self.w.PREFS_.getpref('Retract Distance', 10, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_z_safe_travel.setText(str(self.w.PREFS_.getpref('Z Safe Travel', 10, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_rotary_height.setText(str(self.w.PREFS_.getpref('Rotary Height', 10, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_eoffset_count.setText(str(self.w.PREFS_.getpref('Eoffset count', 0, int, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_max_power.setText(str(self.w.PREFS_.getpref('Max Spindle Power', 0, int, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_spindle_delay.setText(str(self.w.PREFS_.getpref('Spindle Delay', 2, int, 'CUSTOM_FORM_ENTRIES')))
        self.w.chk_reload_program.setChecked(self.w.PREFS_.getpref('Reload program', False, bool,'CUSTOM_FORM_ENTRIES'))
        self.w.chk_reload_tool.setChecked(self.w.PREFS_.getpref('Reload tool', False, bool,'CUSTOM_FORM_ENTRIES'))
        self.w.chk_use_keyboard.setChecked(self.w.PREFS_.getpref('Use keyboard', False, bool, 'CUSTOM_FORM_ENTRIES'))
        self.w.chk_use_camera.setChecked(self.w.PREFS_.getpref('Use camera', False, bool, 'CUSTOM_FORM_ENTRIES'))
        self.w.chk_use_mpg.setChecked(self.w.PREFS_.getpref('Use MPG jog', False, bool, 'CUSTOM_FORM_ENTRIES'))
        self.w.chk_use_mdi_keyboard.setChecked(self.w.PREFS_.getpref('Use MDI Keyboard', False, bool, 'CUSTOM_FORM_ENTRIES'))
        self.w.chk_use_joypad.setChecked(self.w.PREFS_.getpref('Use program joypad', False, bool, 'CUSTOM_FORM_ENTRIES'))
        self.w.chk_inhibit_selection.setChecked(self.w.PREFS_.getpref('Inhibit display mouse selection', True, bool, 'CUSTOM_FORM_ENTRIES'))
        self.current_style = self.w.PREFS_.getpref('style_QSS_Path', 'DEFAULT' , str, 'BOOK_KEEPING')
        
    def closing_cleanup__(self):
        if not self.w.PREFS_: return
        if self.last_loaded_program is not None:
            self.w.PREFS_.putpref('last_loaded_directory', os.path.dirname(self.last_loaded_program), str, 'BOOK_KEEPING')
            self.w.PREFS_.putpref('last_loaded_file', self.last_loaded_program, str, 'BOOK_KEEPING')
        self.w.PREFS_.putpref('Tool to load', STATUS.get_current_tool(), int, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Laser X', self.w.lineEdit_laser_x.text(), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Laser Y', self.w.lineEdit_laser_y.text(), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Camera X', self.w.lineEdit_camera_x.text(), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Camera Y', self.w.lineEdit_camera_y.text(), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Probe X', self.w.lineEdit_probe_x.text(), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Probe Y', self.w.lineEdit_probe_y.text(), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Work Height', self.w.lineEdit_work_height.text(), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Touch Height', self.w.lineEdit_touch_height.text(), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Sensor Location X', self.w.lineEdit_sensor_x.text(), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Sensor Location Y', self.w.lineEdit_sensor_y.text(), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Sensor Height', self.w.lineEdit_sensor_height.text(), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Search Velocity', self.w.lineEdit_search_vel.text(), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Probe Velocity', self.w.lineEdit_probe_vel.text(), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Max Probe', self.w.lineEdit_max_probe.text(), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Retract Distance', self.w.lineEdit_retract_distance.text(), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Z Safe Travel', self.w.lineEdit_z_safe_travel.text(), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Rotary Height', self.w.lineEdit_rotary_height.text(), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Eoffset count', self.w.lineEdit_eoffset_count.text(), int, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Max Spindle Power', self.w.lineEdit_max_power.text(), int, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Spindle Delay', self.w.lineEdit_spindle_delay.text(), int, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Reload program', self.w.chk_reload_program.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Reload tool', self.w.chk_reload_tool.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Use keyboard', self.w.chk_use_keyboard.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Use camera', self.w.chk_use_camera.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Use MPG jog', self.w.chk_use_mpg.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Use MDI Keyboard', self.w.chk_use_mdi_keyboard.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Use program joypad', self.w.chk_use_joypad.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('style_QSS_Path', self.current_style , str, 'BOOK_KEEPING')
        self.w.PREFS_.putpref('Inhibit display mouse selection', self.w.chk_inhibit_selection.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')

    def init_widgets(self):
        self.w.main_tab_widget.setCurrentIndex(TAB_MAIN)
        self.w.adj_linear_jog.setValue(self.default_linear_jog_vel)
        self.w.adj_linear_jog.setMaximum(self.max_linear_velocity)
        self.w.adj_angular_jog.setValue(self.default_angular_jog_vel)
        self.w.adj_angular_jog.setMaximum(self.max_angular_velocity)
        self.w.adj_spindle_ovr.setValue(100)
        self.w.chk_override_limits.setChecked(False)
        self.w.chk_override_limits.setEnabled(False)
        self.w.lbl_home_x.setText(INFO.get_error_safe_setting('JOINT_0', 'HOME',"50"))
        self.w.lbl_home_y.setText(INFO.get_error_safe_setting('JOINT_1', 'HOME',"50"))
        self.w.lbl_max_velocity.setText(f"{self.max_linear_velocity}")
        self.w.lbl_max_angular.setText(f"{self.max_angular_velocity}")
        self.w.lineEdit_min_rpm.setText(f"{self.min_spindle_rpm}")
        self.w.lineEdit_max_rpm.setText(f"{self.max_spindle_rpm}")
        # gcode file history
        self.w.cmb_gcode_history.addItem("No File Loaded")
        self.w.cmb_gcode_history.view().setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        # gcode editor mode
        self.w.gcode_viewer.readOnlyMode()
        # ABOUT pages
        from lib.setup_about import Setup_About
        self.about_pages = Setup_About(self.w, self)
        self.about_pages.init_about()
        # mdi history
        self.w.mdihistory.MDILine.setFixedHeight(30)
        self.w.mdihistory.MDILine.setPlaceholderText('MDI:')
        self.use_mdi_keyboard_changed(self.w.chk_use_mdi_keyboard.isChecked())
        self.w.cmb_mdi_texts.addItem("SELECT")
        self.w.cmb_mdi_texts.addItem("HALSHOW")
        self.w.cmb_mdi_texts.addItem("HALMETER")
        self.w.cmb_mdi_texts.addItem("HALSCOPE")
        self.w.cmb_mdi_texts.addItem("STATUS")
        self.w.cmb_mdi_texts.addItem("CLASSICLADDER")
        self.w.cmb_mdi_texts.addItem("CALIBRATION")
        self.w.cmb_mdi_texts.addItem("PREFERENCE")
        self.w.cmb_mdi_texts.addItem("CLEAR HISTORY")
        # set calculator mode for menu buttons
        for i in ("x", "y", "z", "a"):
            self.w["axistoolbutton_" + i].set_dialog_code('CALCULATOR')
        # disable mouse wheel events on comboboxes
        self.w.cmb_gcode_history.wheelEvent = lambda event: None
        self.w.cmb_stylesheet.wheelEvent = lambda event: None
        self.w.cmb_icon_select.wheelEvent = lambda event: None
        self.w.jogincrements_linear.wheelEvent = lambda event: None
        self.w.jogincrements_angular.wheelEvent = lambda event: None
        # turn off table grids
        self.w.filemanager.table.setShowGrid(False)
        self.w.offset_table.setShowGrid(False)
        self.w.tooloffsetview.setShowGrid(False)
        # move clock to statusbar
        text = self.w.lbl_logo.text()
        self.w.lbl_logo.setText(text + ' - V' + str(VERSION))
        self.w.statusbar.addPermanentWidget(self.w.lbl_logo)
        self.w.statusbar.addPermanentWidget(self.w.lbl_clock)
        # set homing buttons to correct joints
        self.w.action_home_x.set_joint(self.jog_from_name['X'])
        self.w.action_home_y.set_joint(self.jog_from_name['Y'])
        self.w.action_home_z.set_joint(self.jog_from_name['Z'])
        # set Zero reference buttons to correct joints
        self.w.action_zero_x.set_joint(self.jog_from_name['X'])
        self.w.action_zero_y.set_joint(self.jog_from_name['Y'])
        self.w.action_zero_z.set_joint(self.jog_from_name['Z'])
        # set axis reference buttons to correct joints
        self.w.axistoolbutton_x.set_joint(self.jog_from_name['X'])
        self.w.axistoolbutton_y.set_joint(self.jog_from_name['Y'])
        self.w.axistoolbutton_z.set_joint(self.jog_from_name['Z'])
        if 'A' in self.axis_list:
            self.w.action_zero_a.set_joint(self.jog_from_name['A'])
            self.w.axistoolbutton_a.set_joint(self.jog_from_name['A'])
        # initialize gauges
        self.w.gauge_feedrate._value_font_size = 12
        self.w.gauge_feedrate._label_font_size = 8
        self.w.gauge_feedrate.set_threshold(self.max_linear_velocity)
        self.w.gauge_spindle._value_font_size = 12
        self.w.gauge_spindle.set_threshold(self.max_spindle_rpm)
        # apply standard button icons
        for key in self.icon_btns:
            style = self.w[key].style()
            icon = style.standardIcon(getattr(QtWidgets.QStyle, self.icon_btns[key]))
            self.w[key].setIcon(icon)
        # populate tool icon combobox
        path = os.path.join(PATH.CONFIGPATH, "tool_icons")
        self.w.cmb_icon_select.addItem('SELECT TOOL ICON')
        if os.path.isdir(path):
            icons = os.listdir(path)
            icons.sort()
            for item in icons:
                if item.endswith(".png"):
                    self.w.cmb_icon_select.addItem(item)
        else:
            LOG.info(f"No tool icons found in {path}")
        self.w.cmb_icon_select.addItem("undefined")
        # populate stylesheet combobox
        styles = []
        qss_path = os.path.dirname(PATH.QSS)
        self.add_status(f"Using stylesheets from {qss_path}")
        for fname in os.listdir(qss_path):
            if fname.endswith(".qss"):
                styles.append(os.path.basename(fname))
        if styles:
            self.w.cmb_stylesheet.addItems(styles)
        else:
            self.add_status("No Stylesheets Found", WARNING)
            self.w.cmb_stylesheet.setCurrentText('No Stylesheets Found')
            self.w.cmb_stylesheet.setEnabled(False)
        style_name = os.path.basename(self.current_style)
        idx = self.w.cmb_stylesheet.findText(style_name, QtCore.Qt.MatchExactly)
        self.w.cmb_stylesheet.setCurrentIndex(idx)
        # spindle pause delay timer
        self.pause_timer.setSingleShot(True)
        self.pause_timer.timeout.connect(self.spindle_pause_timer)

    def init_tooldb(self):
        from lib.tool_db import Tool_Database
        self.tool_db = Tool_Database(self.w, self)
        self.db_helpfile = os.path.join(HELP, 'tooldb_help.html')
        self.tool_db.hal_init()
        self.btn_tool_db_clicked(False)

    def init_probe(self):
        from lib.basic_probe import BasicProbe
        self.probe = BasicProbe(self)
        self.probe.setObjectName('basicprobe')
        self.w.probe_layout.addWidget(self.probe)
        self.probe.hal_init()

    def init_utils(self):
        from setup_utils import Setup_Utils
        self.setup_utils = Setup_Utils(self.w, self)
        self.setup_utils.init_utils()
        self.get_next_available()
        self.tool_db.update_tools(self.tool_list)

    def init_joypads(self):
        self.w.jog_xy.set_text_color(QtGui.QColor("cyan"))
        self.w.jog_xy.setCenterText(f"{self.w.adj_linear_jog.value}")
        if "A" in self.axis_list:
            self.w.jog_az.setFont(QtGui.QFont('Lato Heavy', 9))
            self.w.jog_az.set_text_color(QtGui.QColor("cyan"))
            self.w.jog_az.setCenterText(f"{self.w.adj_angular_jog.value}")
        self.w.jog_xy.set_tooltip('C', "Linear jograte")
        self.w.jog_az.set_tooltip('C', "Angular jograte")
        self.w.pgm_control.set_tooltip('T', "RUN")
        self.w.pgm_control.set_tooltip('L', "RELOAD")
        self.w.pgm_control.set_tooltip('R', "STEP")
        self.w.pgm_control.set_tooltip('B', "PAUSE")
        self.w.pgm_control.set_tooltip('C', "STOP")
        self.w.pgm_control.set_true_color(self.stop_color)
        self.w.lbl_pgm_color.setStyleSheet('Background-color: red;')

    def processed_key_event__(self, receiver, event, is_pressed, key, code, shift, cntrl):
        # when typing in MDI, we don't want keybinding to call functions
        # so we catch and process the events directly.
        # We do want ESC, F1 and F2 to call keybinding functions though
        if code not in(QtCore.Qt.Key_Escape, QtCore.Qt.Key_F1 , QtCore.Qt.Key_F2):
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
                    if STATUS.is_man_mode() == False or not receiver2.isReadOnly():
                        if is_pressed:
                            receiver.keyPressEvent(event)
                            event.accept()
                        return True
                if is_pressed:
                    receiver.keyPressEvent(event)
                    event.accept()
                    return True
                else:
                    event.accept()
                    return True

        if event.isAutoRepeat(): return True
        # ok if we got here then try keybindings
        return KEYBIND.manage_function_calls(self, event, is_pressed, key, shift, cntrl)

    #########################
    # CALLBACKS FROM STATUS #
    #########################

    def spindle_pwr_changed(self):
        max_power = float(self.w.lineEdit_max_power.text())
        # this calculation assumes a power factor of 0.8
        power = float(self.h['spindle-volts'] * self.h['spindle-amps'] * 1.386) # V x I x PF x sqrt(3)
        try: # in case of divide by zero
            pc_power = int((power / max_power) * 100)
        except Exception as e:
            print("Calculation error : ", e)
            return
        if pc_power > 100:
            pc_power = 100
        self.w.spindle_power.setValue(pc_power)

    def spindle_fault_changed(self):
        fault = hex(self.h['spindle-fault'])
        self.w.lbl_spindle_fault.setText(fault)

    def mb_errors_changed(self):
        errors = self.h['modbus-errors']
        self.w.lbl_mb_errors.setText(str(errors))
        self.add_status(f"Modbus link has {errors} errors", WARNING)

    def modbus_fault_changed(self):
        if self.h['modbus-fault'] is True:
            self.add_status("VFD communication lost", CRITICAL)
            ACTION.ABORT()
        else:
            self.add_status("VFD communication established")

    def eoffset_value_changed(self):
        self.w.lineEdit_eoffset.setText(format(self.h['eoffset-value'], '.3f'))

    def comp_count_changed(self):
        if self.spindle_is_paused is True: return
        if self.w.btn_enable_comp.isChecked():
            self.h['eoffset-count'] = self.h['comp-count']

    def map_ready_changed(self):
        if self.h['map-ready'] is True:
            try:
                self.zlevel.map_ready()
            except Exception as e:
                self.add_status(f"Error - {e}")
            
    def mpg_increment_changed(self):
        unit = "mm" if INFO.MACHINE_IS_METRIC else "in"
        text = str(self.h["mpg_increment"] * 4)
        self.w.lineEdit_mpg_increment.setText(text + " " + unit)

    def command_running(self, obj):
        if STATUS.is_auto_mode():
            self.w.pgm_control.set_true_color(self.run_color)
            self.w.lbl_pgm_color.setStyleSheet('Background-color: green;')

    def command_stopped(self, obj):
        if self.w.btn_pause_spindle.isChecked():
            self.h['spindle-inhibit'] = False
            self.h['eoffset-count'] = 0
        self.w.pgm_control.set_true_color(self.stop_color)
        self.w.lbl_pgm_color.setStyleSheet('Background-color: red;')

    def user_system_changed(self, data):
        sys = self.system_list[int(data) - 1]
        self.w.actionbutton_rel.setText(sys)
        txt = sys.replace('.', '_')
        self.w["action_" + txt.lower()].setChecked(True)

    def metric_mode_changed(self, mode):
        if mode == INFO.MACHINE_IS_METRIC:
            self.factor = 1.0
        elif mode:
            self.factor = 25.4
        else:
            self.factor = 1/25.4
        # adjust feedrate gauge
        if mode:
            units = "MM"
            ticks = 10
            maxr = int(self.max_linear_velocity / 100)
            self.add_status("Switched to metric units")
        else:
            units = "IN"
            ticks = 8
            maxr = round(self.max_linear_velocity * self.factor / 10) * 10
            self.add_status("Switched to imperial units")
        self.w.gauge_feedrate.set_label(units + "/MIN")
        self.w.gauge_feedrate.set_num_ticks(ticks)
        self.w.gauge_feedrate.set_max_reading(maxr)
        self.w.gauge_feedrate.set_max_value(self.max_linear_velocity * self.factor)
        self.w.gauge_feedrate.set_threshold(self.max_linear_velocity * self.factor * self.feed_ovr)

    def tool_changed(self, tool):
        self.tool_db.set_checked_tool(tool)
        icon = self.tool_db.get_tool_icon(tool)
        if icon is None:
            self.w.lbl_tool_image.setText("Image\nUndefined")
        else:
            icon_file = os.path.join(PATH.CONFIGPATH, 'tool_icons/' + icon)
            self.w.lbl_tool_image.setPixmap(QtGui.QPixmap(icon_file))
        maxz = self.tool_db.get_maxz(tool)
        self.w.lineEdit_max_depth.setText(str(maxz))

    def file_loaded(self, filename):
        if filename is not None:
            self.add_status(f"Loaded file {filename}")
            self.w.progressBar.reset()
            self.last_loaded_program = filename
            self.current_loaded_program = filename
            self.w.lineEdit_runtime.setText("00:00:00")
        else:
            self.add_status("Filename not valid", WARNING)

    def percent_loaded_changed(self, pc):
        if self.progress == pc: return
        self.progress = pc
        if pc < 0:
            self.w.progressBar.setValue(0)
            self.w.progressBar.setFormat('PROGRESS')
        else:
            self.w.progressBar.setValue(pc)
            self.w.progressBar.setFormat(f'LOADING: {pc}%')

    def percent_done_changed(self, pc):
        if self.progress == pc: return
        self.progress = pc
        if pc < 0:
            self.w.progressBar.setValue(0)
            self.w.progressBar.setFormat('PROGRESS')
        else:
            self.w.progressBar.setValue(pc)
            self.w.progressBar.setFormat(f'PROGRESS: {pc}%')

    def homed(self, obj, joint):
        i = int(joint)
        axis = INFO.GET_NAME_FROM_JOINT.get(i).lower()
        try:
            widget = self.w[f"dro_axis_{axis}"]
            widget.setProperty('homed', True)
            widget.style().unpolish(widget)
            widget.style().polish(widget)
        except:
            pass

    def all_homed(self, obj):
        self.w.btn_home_all.setText("ALL\nHOMED")
        self.w.btn_home_all.setProperty('homed', True)
        self.w.btn_home_all.style().unpolish(self.w.btn_home_all)
        self.w.btn_home_all.style().polish(self.w.btn_home_all)
        if self.first_turnon is True:
            self.first_turnon = False
            if self.w.chk_reload_tool.isChecked():
                command = f"M61 Q{self.reload_tool}"
                ACTION.CALL_MDI(command)
            if self.last_loaded_program is not None and self.w.chk_reload_program.isChecked():
                if os.path.isfile(self.last_loaded_program):
                    self.w.cmb_gcode_history.addItem(self.last_loaded_program)
                    self.w.cmb_gcode_history.setCurrentIndex(self.w.cmb_gcode_history.count() - 1)
                    ACTION.OPEN_PROGRAM(self.last_loaded_program)
        ACTION.SET_MANUAL_MODE()
        self.w.manual_mode_button.setChecked(True)
        self.add_status("All axes homed")

    def not_all_homed(self, obj, list):
        self.w.btn_home_all.setText("HOME\nALL")
        self.w.btn_home_all.setProperty('homed', False)
        self.w.btn_home_all.style().unpolish(self.w.btn_home_all)
        self.w.btn_home_all.style().polish(self.w.btn_home_all)

    def pause_changed(self, state):
        if state:
            self.w.pgm_control.set_true_color(self.pause_color)
            self.w.lbl_pgm_color.setStyleSheet('Background-color: yellow;')
        else:
            self.w.pgm_control.set_true_color(self.run_color)
            self.w.lbl_pgm_color.setStyleSheet('Background-color: green;')

    def update_status(self):
        # runtimer
        if self.timer_on is False or STATUS.is_auto_paused(): return
        self.time_tenths += 1
        if self.time_tenths == 10:
            self.time_tenths = 0
            self.run_time += 1
            hours, remainder = divmod(self.run_time, 3600)
            minutes, seconds = divmod(remainder, 60)
            self.w.lineEdit_runtime.setText(f"{hours:02d}:{minutes:02d}:{seconds:02d}")

    def hard_limit_tripped(self, obj, tripped, list_of_tripped):
        self.add_status("Hard limits tripped", CRITICAL)
        self.w.chk_override_limits.setEnabled(tripped)
        if not tripped:
            self.w.chk_override_limits.setChecked(False)

    # keep check button in synch of external changes
    def _check_override_limits(self,state,data):
        if 0 in data:
            self.w.chk_override_limits.setChecked(False)
        else:
            self.w.chk_override_limits.setChecked(True)
    
    #######################
    # CALLBACKS FROM FORM #
    #######################

    # main button bar
    def main_tab_changed(self, btn):
        index = btn.property("index")
        if index is None: return
        if index == self.w.main_tab_widget.currentIndex(): return
        spindle_inhibit = False
        if STATUS.is_auto_mode() and index != TAB_SETTINGS:
            self.add_status("Cannot switch pages while in AUTO mode", WARNING)
            self.w.main_tab_widget.setCurrentIndex(TAB_MAIN)
            self.w.btn_main.setChecked(True)
            self.w.groupBox_preview.setTitle(self.w.btn_main.text())
            return
        if index == TAB_PROBE:
            spindle_inhibit = True
        self.w.led_inhibit.setState(spindle_inhibit)
        self.w.mdihistory.MDILine.spindle_inhibit(spindle_inhibit)
        self.h['spindle-inhibit'] = spindle_inhibit
        self.w.main_tab_widget.setCurrentIndex(index)
        self.w.groupBox_preview.setTitle(btn.text())

    # preview frame
    def btn_show_macros_clicked(self, state):
        if state:
            self.w.group_macro_buttons.show()
        else:
            self.w.group_macro_buttons.hide()

    # gcode frame
    def cmb_gcode_history_clicked(self):
        if self.w.cmb_gcode_history.currentIndex() == 0: return
        filename = self.w.cmb_gcode_history.currentText()
        if filename == self.last_loaded_program:
            self.add_status("Selected program is already loaded", WARNING)
        else:
            ACTION.OPEN_PROGRAM(filename)

    def mdi_select_text(self):
        if self.w.cmb_mdi_texts.currentIndex() <= 0: return
        self.w.mdihistory.MDILine.setText(self.w.cmb_mdi_texts.currentText())
        self.w.cmb_mdi_texts.setCurrentIndex(0)

    def mdi_enter_pressed(self):
        self.w.mdihistory.run_command()
        self.w.mdihistory.MDILine.clear()

    # program frame
    def pgm_button_pressed(self, btn):
        if btn == 'T' or btn == self.w.btn_run:
            self.btn_run_pressed()
        elif btn == 'B' or btn == self.w.btn_pause:
            if STATUS.is_on_and_idle(): return
            self.pause_program()
        elif btn == "L" or btn == self.w.btn_reload:
            if STATUS.is_auto_running():
                self.add_status("Cannot reload program while running", WARNING)
                return
            self.btn_reload_file_pressed()
        elif btn == "R" or btn == self.w.btn_step:
            ACTION.STEP()
        elif btn == "C" or btn == self.w.btn_stop:
            ACTION.ABORT()
            ACTION.SET_MANUAL_MODE()
            self.w.pgm_control.set_tooltip('B', "PAUSE")

    def btn_run_pressed(self):
        if self.w.main_tab_widget.currentIndex() != 0:
            return
        if not STATUS.is_auto_mode():
            self.add_status("Must be in AUTO mode to run a program", WARNING)
            return
        if STATUS.is_auto_running():
            self.add_status("Program is already running", WARNING)
            return
        if self.current_loaded_program is None:
            self.add_status("No program has been loaded", WARNING)
            return
        self.w.pgm_control.set_true_color(self.run_color)
        self.w.lbl_pgm_color.setStyleSheet('Background-color: green;')
        self.run_time = 0
        self.w.lineEdit_runtime.setText("00:00:00")
        if self.start_line <= 1:
            ACTION.RUN(0)
        else:
            # instantiate run from line preset dialog
            info = f'<b>Running From Line: {self.start_line} <\b>'
            mess = {'NAME' : 'RUNFROMLINE',
                    'TITLE' : 'Preset Dialog',
                    'ID' : '_RUNFROMLINE',
                    'MESSAGE' : info,
                    'LINE' : self.start_line,
                    'NONBLOCKING' : True}
            ACTION.CALL_DIALOG(mess)
        self.add_status(f"Started {self.current_loaded_program} from line {self.start_line}")
        self.timer_on = True

    def pause_program(self):
        if STATUS.is_auto_paused():
            if self.pause_delay > 0:
                self.add_status("Wait for spindle at speed before resuming", WARNING)
                return
            self.w.pgm_control.set_tooltip('B', "PAUSE")
            self.w.btn_pause.setText("PAUSE")
            ACTION.PAUSE()
        else:
            self.w.pgm_control.set_tooltip('B', "RESUME")
            self.w.btn_pause.setText("RESUME")
            ACTION.PAUSE()
            if self.w.btn_pause_spindle.isChecked():
                self.pause_spindle()

    def pause_spindle(self):
        self.spindle_is_paused = True
        fval = float(self.w.lineEdit_eoffset_count.text())
        self.h['eoffset-count'] = (fval * 1000) + self.h['comp-count']
        self.h['spindle-inhibit'] = True
        self.add_status(f"Spindle paused at {self.w.lineEdit_runtime.text()}")
        # modify runtime text
        self.runtime_save = self.w.lineEdit_runtime.text()
        self.pause_delay = int(self.w.lineEdit_spindle_delay.text())
        self.w.lineEdit_runtime.setText(f"WAIT {self.pause_delay}")
        # instantiate warning box
        icon = QMessageBox.Warning
        title = "SPINDLE PAUSED"
        info = "Wait for spindle at speed signal before resuming"
        button = QMessageBox.Ok
        retval = self.message_box(icon, title, info, button)
        if retval == QMessageBox.Ok:
            self.h['spindle-inhibit'] = False
            # add time delay for spindle to attain speed
            self.pause_timer.start(1000)

    def btn_reload_file_pressed(self):
        if self.last_loaded_program:
            self.w.progressBar.reset()
            self.add_status(f"Loaded program file {self.last_loaded_program}")
            ACTION.OPEN_PROGRAM(self.last_loaded_program)

    def btn_run_from_line_clicked(self, state):
        self.w.gcodegraphics.set_inhibit_selection(not state)
        text = "ENABLED" if state else "DISABLED"
        self.w.lbl_start_line.setText(text)
        if not state:
            self.start_line = 1

    # jogging frame
    def jog_xy_pressed(self, btn):
        if btn == "C":
            self.slow_linear_jog = not self.slow_linear_jog
            self.slow_button_clicked('xy')
            return
        axis = 'X' if btn in "LR" else 'Y'
        direction = 1 if btn in "RT" else -1
        self.w.jog_xy.set_highlight(axis, True)
        ACTION.ensure_mode(linuxcnc.MODE_MANUAL)
        ACTION.DO_JOG(axis, direction)

    def jog_az_pressed(self, btn):
        if btn == "C":
            if "A" in self.axis_list:
                self.slow_angular_jog = not self.slow_angular_jog
                self.slow_button_clicked('az')
            return
        if btn in "LR" and not "A" in self.axis_list: return
        axis = 'A' if btn in "LR" else 'Z'
        direction = 1 if btn in "RT" else -1
        self.w.jog_az.set_highlight(axis, True)
        ACTION.ensure_mode(linuxcnc.MODE_MANUAL)
        ACTION.DO_JOG(axis, direction)

    def jog_xy_released(self, btn):
        if btn == "C": return
        axis = 'X' if btn in "LR" else 'Y'
        self.w.jog_xy.set_highlight(axis, False)
        if STATUS.get_jog_increment() == 0:
            ACTION.ensure_mode(linuxcnc.MODE_MANUAL)
            ACTION.DO_JOG(axis, 0)

    def jog_az_released(self, btn):
        if btn == "C": return
        if btn in "LR" and not "A" in self.axis_list: return
        axis = 'A' if btn in "LR" else 'Z'
        self.w.jog_az.set_highlight(axis, False)
        inc = STATUS.get_jog_increment_angular() if axis == 'A' else STATUS.get_jog_increment()
        if inc == 0:
            ACTION.ensure_mode(linuxcnc.MODE_MANUAL)
            ACTION.DO_JOG(axis, 0)
            
    def use_mpg_changed(self, state):
        if state:
            self.w.lbl_mpg_increment.show()
            self.w.lineEdit_mpg_increment.show()
            self.h['mpg-disable'] = False
        else:
            self.w.lbl_mpg_increment.hide()
            self.w.lineEdit_mpg_increment.hide()
            self.h['mpg-disable'] = True

    # bottom button row
    def btn_pause_spindle_clicked(self, state):
        text = 'enabled' if state else 'disabled'
        self.add_status(f"Spindle pause {text}")

    def btn_enable_comp_clicked(self, state):
        if state:
            fname = os.path.join(PATH.CONFIGPATH, "probe_points.txt")
            if not os.path.isfile(fname):
                self.add_status(fname + " not found")
                return
            self.h['comp-on'] = True
            self.h['eoffset-count'] = self.h['comp-count']
            self.add_status("Z level compensation ON")
        else:
            self.h['comp-on'] = False
            self.h['eoffset-count'] = 0
            self.add_status("Z level compensation OFF")

    def btn_goto_location_clicked(self):
        dest = self.w.sender().property('location')
        ACTION.CALL_MDI("G90")
        if dest == 'zero':
            ACTION.CALL_MDI_WAIT("G53 G0 Z0", 30)
            ACTION.CALL_MDI_WAIT("X0 Y0", 30)
        elif dest == 'home':
            ACTION.CALL_MDI_WAIT("G53 G0 Z0", 30)
            cmd = f"G53 G0 X{self.w.lbl_home_x.text()} Y{self.w.lbl_home_y.text()}"
            ACTION.CALL_MDI_WAIT(cmd, 30)
        elif dest == 'sensor':
            ACTION.CALL_MDI_WAIT("G53 G0 Z0", 30)
            cmd = f"G53 G0 X{self.w.lineEdit_sensor_x.text()} Y{self.w.lineEdit_sensor_y.text()}"
            ACTION.CALL_MDI_WAIT(cmd, 30)
        elif dest == 'zero_a':
            ACTION.CALL_MDI_WAIT("G0 A0", 30)

    def btn_ref_laser_clicked(self):
        if self.w.btn_laser_on.isChecked():
            x = float(self.w.lineEdit_laser_x.text())
            y = float(self.w.lineEdit_laser_y.text())
            if not STATUS.is_metric_mode():
                x = x / 25.4
                y = y / 25.4
            self.add_status("Laser offsets set")
            command = f"G10 L20 P0 X{x:3.4f} Y{y:3.4f}"
            ACTION.CALL_MDI(command)
        else:
            self.add_status("Laser must be on to set laser offset", WARNING)

    def btn_touchoff_pressed(self):
        if STATUS.get_current_tool() == 0:
            self.add_status("Cannot touchoff with no tool loaded", WARNING)
            return
        if not STATUS.is_all_homed():
            self.add_status("Must be homed to perform tool touchoff", WARNING)
            return
        sensor = self.w.sender().property('sensor')
        self.touchoff(sensor)

    # DRO frame
    def btn_home_all_clicked(self, obj):
        if not STATUS.is_all_homed():
            ACTION.SET_MACHINE_HOMING(-1)
        else:
        # instantiate dialog box
            icon = QMessageBox.Question
            title = "UNHOME ALL AXES"
            info = "Do you want to Unhome all axes?"
            buttons = QMessageBox.Cancel | QMessageBox.Ok
            retval = self.message_box(icon, title, info, buttons)
            if retval == QMessageBox.Ok:
                ACTION.SET_MACHINE_UNHOMED(-1)

    # override frame
    def slow_button_clicked(self, btn):
        adj = 'adj_linear_jog' if btn == 'xy' else 'adj_angular_jog'
        state = self.slow_linear_jog if btn == 'xy' else self.slow_angular_jog
        if state is True:
            value = int(self.w[adj].value / self.slow_jog_factor)
            maxval = int(self.w[adj].maximum() / self.slow_jog_factor)
            hival = int(self.w[adj].hi_value / self.slow_jog_factor)
            lowval = int(self.w[adj].low_value / self.slow_jog_factor)
            step = 10
        else:
            value = int(self.w[adj].value * self.slow_jog_factor)
            maxval = int(self.w[adj].maximum() * self.slow_jog_factor)
            hival = int(self.w[adj].hi_value * self.slow_jog_factor)
            lowval = int(self.w[adj].low_value * self.slow_jog_factor)
            step = 100
        self.w[adj].low_value = lowval
        self.w[adj].hi_value = hival
        self.w[adj].setMaximum(maxval)
        self.w[adj].setValue(value)
        self.w[adj].setStep(step)
        self.w[adj].valueChanged.emit(value)
        if btn == 'xy':
            color = QtGui.QColor("yellow") if self.slow_linear_jog else QtGui.QColor("cyan")
            self.w.jog_xy.set_text_color(color)
            self.w.jog_xy.setCenterText(f"{value}")
        elif 'A' in self.axis_list:
            color = QtGui.QColor("yellow") if self.slow_angular_jog else QtGui.QColor("cyan")
            self.w.jog_az.set_text_color(color)
            self.w.jog_az.setCenterText(f"{value}")

    def adj_feed_ovr_changed(self, value):
        frac = int(value * self.max_linear_velocity * self.factor / 100)
        self.w.gauge_feedrate.set_threshold(frac)
        self.feed_ovr = value / 100

    def adj_spindle_ovr_changed(self, value):
        frac = int(value * self.max_spindle_rpm / 100)
        self.w.gauge_spindle.set_threshold(frac)

    def adj_linear_changed(self, value):
        rate = str(value)
        self.w.jog_xy.setCenterText(rate)

    def adj_angular_changed(self, value):
        rate = str(value)
        self.w.jog_az.setCenterText(rate)

    # TOOL tab
    def btn_add_tool_pressed(self):
        if not STATUS.is_on_and_idle():
            self.add_status("Status must be ON and IDLE", WARNING)
            return
        array = [self.next_available, self.next_available, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 'New tool']
        TOOL.ADD_TOOL(array)
        self.add_status(f"Added tool {self.next_available}")
        self.tool_db.add_tool(self.next_available)
        self.get_next_available()

    def btn_delete_tool_pressed(self):
        if not STATUS.is_on_and_idle():
            self.add_status("Status must be ON and IDLE", WARNING)
            return
        tools = self.get_checked_tools()
        if not tools:
            self.add_status("No tool selected to delete", WARNING)
            return
        TOOL.DELETE_TOOLS(tools)
        self.add_status(f"Deleted tool {tools[0]}")
        self.tool_db.delete_tool(tools[0])
        self.get_next_available()

    def btn_load_tool_pressed(self):
        tool = self.get_checked_tools()
        if len(tool) > 1:
            self.add_status("Select only 1 tool to load", CRITICAL)
        elif tool:
            ACTION.CALL_MDI(f"M61 Q{tool[0]} G43")
            self.add_status(f"Tool {tool[0]} loaded")
        else:
            self.add_status("No tool selected", WARNING)

    def btn_unload_tool_pressed(self):
        ACTION.CALL_MDI("M61 Q0")

    def btn_tool_db_clicked(self, state):
        self.w.stackedWidget_tools.setCurrentIndex(state)
        if state:
            for item in ["add_tool", "delete_tool", "load_tool", "unload_tool"]:
                self.w['btn_' + item].hide()
            for item in ["enable_edit", "export_data", "db_help"]:
                self.w['btn_' + item].show()
        else:
            for item in ["add_tool", "delete_tool", "load_tool", "unload_tool"]:
                self.w['btn_' + item].show()
            for item in ["enable_edit", "export_data", "db_help"]:
                self.w['btn_' + item].hide()

    def show_db_help_page(self):
        self.setup_utils.show_help_page(self.db_helpfile)

    # STATUS tab
    def btn_clear_status_clicked(self):
        STATUS.emit('update-machine-log', None, 'DELETE')

    def btn_save_log_clicked(self):
        if self.w.btn_select_log.isChecked():
            text = self.w.integrator_log.toPlainText()
        else:
            text = self.w.machinelog.toPlainText()
        filename = self.w.lbl_clock.text()
        filename = 'status_' + filename.replace(' ','_') + '.txt'
        self.add_status(f"Saving log to {filename}")
        with open(filename, 'w') as f:
            f.write(text)

    def btn_dimensions_clicked(self, state):
        self.w.gcodegraphics.show_extents_option = state
        self.w.gcodegraphics.clear_live_plotter()
        
    # CAMVIEW tab
    def cam_zoom_changed(self, value):
        self.w.camview.scale = float(value) / 10

    def cam_dia_changed(self, value):
        self.w.camview.diameter = value

    def cam_rot_changed(self, value):
        self.w.camview.rotation = float(value) / 10

    def btn_ref_camera_clicked(self):
        x = float(self.w.lineEdit_camera_x.text())
        y = float(self.w.lineEdit_camera_y.text())
        if not STATUS.is_metric_mode():
            x = x / 25.4
            y = y / 25.4
        self.add_status("Camera offsets set")
        command = f"G10 L20 P0 X{x:3.4f} Y{y:3.4f}"
        ACTION.CALL_MDI(command)

    # SETTINGS tab
    def override_limits_changed(self, state):
        if state:
            print("Override limits set")
            ACTION.SET_LIMITS_OVERRIDE()
        else:
            print("Override limits not set")

    def use_camera_changed(self, state):
        if state :
            self.w.btn_camera.show()
        else:
            self.w.btn_camera.hide()

    def use_mdi_keyboard_changed(self, state):
        if state:
            self.w.widget_mdi_controls.hide()
        else:
            self.w.widget_mdi_controls.show()
        self.w.mdihistory.set_soft_keyboard(state)

    def use_joypad_changed(self, state):
        index = 1 if state else 0
        self.w.stackedWidget_pgm_control.setCurrentIndex(index)

    def cmb_stylesheet_clicked(self, index):
        style = self.w.cmb_stylesheet.currentText()
        fname = os.path.join(self.style_path, style)
        self.current_style = fname
        file = QtCore.QFile(fname)
        file.open(QtCore.QFile.ReadOnly)
        styleSheet = QtCore.QTextStream(file)
        self.w.setStyleSheet("")
        self.w.setStyleSheet(styleSheet.readAll())
        self.add_status(f"Stylesheet set to {style}")

    def edit_gcode_changed(self, state):
        if state:
            self.w.gcode_viewer.editMode()
        else:
            self.w.gcode_viewer.readOnlyMode()

    #####################
    # GENERAL FUNCTIONS #
    #####################
    def tool_data_changed(self, new, old, roles):
        if new.column() > 1: return
        old_list = self.tool_list
        self.get_next_available()
        new_list = self.tool_list
        old_tno = list(set(old_list) - set(new_list))
        new_tno = list(set(new_list) - set(old_list))
        self.tool_db.update_tool_no(old_tno[0], new_tno[0])
        
    def get_checked_tools(self):
        checked = self.w.tooloffsetview.get_checked_list()
        if checked: self.tool_db.set_checked_tool(checked[0])
        return checked

    def get_next_available(self):
        array = TOOL.GET_TOOL_ARRAY()
        tool_list = []
        for line in array:
            tool_list.append(line[0])
        self.tool_list = tool_list
        for tno in range(0, 100):
            if tno not in tool_list: break
        self.next_available = tno
        self.w.lineEdit_next_available.setText(str(tno))

    def probe_offset_edited(self):
        x = self.w.lineEdit_probe_x.text()
        y = self.w.lineEdit_probe_y.text()
        self.probe.set_offsets(x, y)

    def show_selected_axis(self, obj):
        if self.w.chk_use_mpg.isChecked():
            self.w.jog_xy.set_highlight('X', bool(self.h['axis-select-x'] is True))
            self.w.jog_xy.set_highlight('Y', bool(self.h['axis-select-y'] is True))
            self.w.jog_az.set_highlight('Z', bool(self.h['axis-select-z'] is True))
            if 'A' in self.axis_list:
                self.w.jog_az.set_highlight('A', bool(self.h['axis-select-a'] is True))

    def load_code(self, fname):
        if fname.endswith(".ngc") or fname.endswith(".py"):
            self.w.cmb_gcode_history.addItem(fname)
            self.w.cmb_gcode_history.setCurrentIndex(self.w.cmb_gcode_history.count() - 1)
            ACTION.OPEN_PROGRAM(fname)
            self.add_status(f"Loaded program file : {fname}")
            self.w.main_tab_widget.setCurrentIndex(TAB_MAIN)
            self.w.btn_main.setChecked(True)
        elif fname.endswith(".html"):
            self.setup_utils.show_html(fname)
            self.w.main_tab_widget.setCurrentIndex(TAB_UTILS)
            self.w.btn_utils.setChecked(True)
            self.add_status(f"Loaded HTML file : {fname}")
        elif fname.endswith(".pdf"):
            self.setup_utils.show_pdf(fname)
            self.w.main_tab_widget.setCurrentIndex(TAB_UTILS)
            self.w.btn_utils.setChecked(True)
            self.add_status(f"Loaded PDF file : {fname}")
        else:
            self.add_status("Unknown or invalid filename", WARNING)

    def touchoff(self, selector):
        if selector == 'touchplate':
            z_offset = self.w.lineEdit_touch_height.text()
        elif selector == 'toolsensor':
            z_offset = float(self.w.lineEdit_sensor_height.text()) - float(self.w.lineEdit_work_height.text())
            z_offset = str(z_offset)
        else:
            self.add_status("Unknown touchoff routine specified", WARNING)
            return
        self.add_status(f"Touchoff to {selector} started")
        self.start_touchoff()
        string_to_send = "touchoff$" \
                        + self.w.lineEdit_search_vel.text() + "$" \
                        + self.w.lineEdit_probe_vel.text() + "$" \
                        + self.w.lineEdit_max_probe.text() + "$" \
                        + self.w.lineEdit_retract_distance.text() + "$" \
                        + self.w.lineEdit_z_safe_travel.text() + "$" \
                        + z_offset + "\n"
        self.proc.writeData(bytes(string_to_send, 'utf-8'))

    def spindle_pause_timer(self):
        self.pause_delay -= 1
        if self.pause_delay <= 0:
            self.spindle_is_paused = False
            self.h['eoffset-count'] = self.h['comp-count']
            self.add_status("Program resumed")
            self.w.lineEdit_runtime.setText(self.runtime_save)
        else:
            self.w.lineEdit_runtime.setText(f"WAIT {self.pause_delay}")
            self.pause_timer.start(1000)
        
    def kb_jog(self, state, axis, direction):
        if not STATUS.is_man_mode() or not STATUS.machine_is_on():
            self.add_status('Machine must be ON and in Manual mode to jog', WARNING)
            return
        if state == 0: direction = 0
        ACTION.DO_JOG(axis, direction)

    def add_status(self, message, level=DEFAULT):
        if level == WARNING:
            self.w.statusbar.setStyleSheet("color: yellow")
            self.status_timer.start()
        elif level == CRITICAL:
            self.w.statusbar.setStyleSheet("color: red")
            self.status_timer.start()
        else:
            self.w.statusbar.setStyleSheet("color: white")
        self.w.statusbar.showMessage(message)
        if not message == "":
            STATUS.emit('update-machine-log', message, 'TIME')

    def clear_statusbar(self):
        self.add_status("")
        self.status_timer.stop()

    def enable_auto(self, state):
        for btn in self.auto_disable_list:
            self.w["btn_" + btn].setEnabled(not state)
        if state:
            self.w.btn_main.setChecked(True)
            self.w.main_tab_widget.setCurrentIndex(TAB_MAIN)

    def enable_onoff(self, state):
        text = "ON" if state else "OFF"
        self.add_status("Machine " + text)
        self.h['eoffset-count'] = 0
        for widget in self.onoff_list:
            self.w["frame_" + widget].setEnabled(state)
        self.w.jog_xy.setEnabled(state)
        self.w.jog_az.setEnabled(state)
        self.w.pgm_control.setEnabled(state)
        if "A" not in self.axis_list:
            self.w.axistoolbutton_a.setEnabled(False)
            self.w.adj_angular_jog.setEnabled(False)

    def set_start_line(self, line):
        self.w.gcodegraphics.highlight_graphics(line)
        if self.w.btn_run_from_line.isChecked():
            self.start_line = line
            self.w.lbl_start_line.setText(f"{self.start_line}")

    def use_keyboard(self):
        if self.w.chk_use_keyboard.isChecked():
            return True
        else:
            self.add_status('Keyboard shortcuts are disabled', WARNING)
            return False

    def stop_timer(self):
        self.w.pgm_control.set_true_color(self.stop_color)
        self.w.lbl_pgm_color.setStyleSheet('Background-color: red;')
        if self.timer_on:
            self.timer_on = False
            self.add_status(f"Run timer stopped at {self.w.lineEdit_runtime.text()}")

    def start_touchoff(self):
        if self.proc is not None:
            self.add_status("Touchoff routine is already running", WARNING)
            return
        self.proc = QtCore.QProcess()
        self.proc.setReadChannel(QtCore.QProcess.StandardOutput)
        self.proc.started.connect(self.touchoff_started)
        self.proc.readyReadStandardOutput.connect(self.read_stdout)
        self.proc.readyReadStandardError.connect(self.read_stderror)
        self.proc.finished.connect(self.touchoff_finished)
        self.proc.start(f'python3 {SUBPROGRAM}')

    def read_stdout(self):
        qba = self.proc.readAllStandardOutput()
        line = qba.data()
        self.parse_line(line)

    def read_stderror(self):
        qba = self.proc.readAllStandardError()
        line = qba.data()
        self.parse_line(line)

    def parse_line(self, line):
        line = line.decode("utf-8")
        if "COMPLETE" in line:
            self.add_status("Touchoff routine returned success")
        elif "ERROR" in line:
            self.add_status(line)

    def touchoff_started(self):
        LOG.info(f"TouchOff subprogram started with PID {self.proc.processId()}\n")

    def touchoff_finished(self, exitCode, exitStatus):
        LOG.info(f"Touchoff Process finished - exitCode {exitCode} exitStatus {exitStatus}")
        self.proc = None

    def message_box(self, icon, title, info, buttons):
        msg = QMessageBox()
        msg.setIcon(icon)
        msg.setWindowTitle(title)
        msg.setText(info)
        msg.setStandardButtons(buttons)
        return msg.exec_()

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
        if state and not STATUS.is_all_homed() and self.use_keyboard():
            ACTION.SET_MACHINE_HOMING(-1)

    def on_keycall_PAUSE(self,event,state,shift,cntrl):
        if state and STATUS.is_auto_mode() and self.use_keyboard():
            self.pause_program()

    def on_keycall_XPOS(self,event,state,shift,cntrl):
        if self.use_keyboard():
            self.kb_jog(state, 'X', 1)

    def on_keycall_XNEG(self,event,state,shift,cntrl):
        if self.use_keyboard():
            self.kb_jog(state, 'X', -1)

    def on_keycall_YPOS(self,event,state,shift,cntrl):
        if self.use_keyboard():
            self.kb_jog(state, 'Y', 1)

    def on_keycall_YNEG(self,event,state,shift,cntrl):
        if self.use_keyboard():
            self.kb_jog(state, 'Y', -1)

    def on_keycall_ZPOS(self,event,state,shift,cntrl):
        if self.use_keyboard():
            self.kb_jog(state, 'Z', 1)

    def on_keycall_ZNEG(self,event,state,shift,cntrl):
        if self.use_keyboard():
            self.kb_jog(state, 'Z', -1)
    
    def on_keycall_APOS(self,event,state,shift,cntrl):
        if self.use_keyboard() and 'A' in self.axis_list:
            self.kb_jog(state, 'A', 1)

    def on_keycall_ANEG(self,event,state,shift,cntrl):
        if self.use_keyboard() and 'A' in self.axis_list:
            self.kb_jog(state, 'A', -1)

    def on_keycall_F4(self,event,state,shift,cntrl):
        if state:
            mess = {'NAME':'CALCULATOR', 'TITLE':'Calculator', 'ID':'_calculator_'}
            ACTION.CALL_DIALOG(mess)

    def on_keycall_F12(self,event,state,shift,cntrl):
        if state:
            self.styleeditor.load_dialog()

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
