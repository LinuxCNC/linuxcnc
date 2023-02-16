import os
import linuxcnc
import hal
import sys
from PyQt5.QtWidgets import QMessageBox
from PyQt5 import QtCore, QtWidgets, QtGui, uic
try:
    from PyQt5.QtWebKitWidgets import QWebView
except ImportError:
    raise Exception("Qtvcp error with woodpecker - is package python-pyqt5.qtwebkit installed?")
from qtvcp.widgets.gcode_editor import GcodeEditor as GCODE
from qtvcp.widgets.mdi_line import MDILine as MDI_WIDGET
from qtvcp.widgets.tool_offsetview import ToolOffsetView as TOOL_TABLE
from qtvcp.widgets.origin_offsetview import OriginOffsetView as OFFSET_VIEW
from qtvcp.widgets.stylesheeteditor import StyleSheetEditor as SSE
from qtvcp.widgets.file_manager import FileManager as FM
from qtvcp.lib.auto_height.auto_height import Auto_Measure
from qtvcp.lib.writer import writer
from qtvcp.lib.keybindings import Keylookup
from qtvcp.lib.gcodes import GCodes
from qtvcp.lib.qt_pdf import PDFViewer 
from qtvcp.widgets.simple_widgets import DoubleScale as PARAMETER
from qtvcp.lib.aux_program_loader import Aux_program_loader
from qtvcp.core import Status, Action, Info, Path, Qhal
from qtvcp import logger
from shutil import copyfile

LOG = logger.getLogger(__name__)
KEYBIND = Keylookup()
AUX_PRGM = Aux_program_loader()
STATUS = Status()
INFO = Info()
ACTION = Action()
PATH = Path()
STYLEEDITOR = SSE()
WRITER = writer.Main()
QHAL = Qhal()
SUBPROGRAM = os.path.join(PATH.LIBDIR, 'touchoff_subprogram.py')

# constants for tab pages
TAB_MAIN = 0
TAB_FILE = 1
TAB_OFFSETS = 2
TAB_TOOL = 3
TAB_STATUS = 4
TAB_PROBE = 5
TAB_CAMERA = 6
TAB_GCODES = 7
TAB_SETUP = 8
TAB_SETTINGS = 9
TAB_UTILITIES = 10
TAB_MDI_TOUCHY = 11
TAB_ABOUT = 12

DEFAULT = 0
WARNING = 1
CRITICAL = 2

class HandlerClass:
    def __init__(self, halcomp, widgets, paths):
        self.h = halcomp
        self.w = widgets
        self.PATHS = paths
        self.gcodes = GCodes(widgets)
        self._last_count = 0
        self.valid = QtGui.QRegExpValidator(QtCore.QRegExp('-?[0-9]{0,6}[.][0-9]{0,3}'))
        self.styleeditor = SSE(widgets, paths)
        KEYBIND.add_call('Key_F10','on_keycall_F10')
        KEYBIND.add_call('Key_F11','on_keycall_F11')
        KEYBIND.add_call('Key_F12','on_keycall_F12')
        KEYBIND.add_call('Key_Pause', 'on_keycall_pause')
        KEYBIND.add_call('Key_Any', 'on_keycall_pause')

        INIPATH = os.environ.get('INI_FILE_NAME', '/dev/null')
        self.iniFile = linuxcnc.ini(INIPATH)

        self.unitsPerMm = 1
        self.units = self.iniFile.find('TRAJ', 'LINEAR_UNITS')
        if self.units == 'inch':
            self.unitsPerMm = 0.03937
                
        # some global variables
        self.probe = None
        self.factor = 1.0
        self.default_setup = os.path.join(PATH.CONFIGPATH, "default_setup.html")
        self.docs = os.path.join(PATH.SCREENDIR, PATH.BASEPATH,'docs/getting_started.html')
        self.start_line = 0
        self.run_time = 0
        self.time_tenths = 0
        self.timer_on = False
        self.home_all = False
        self.run_color = QtGui.QColor('green')
        self.stop_color = QtGui.QColor('red')
        self.pause_color = QtGui.QColor('yellow')
        self.default_about = os.path.join(PATH.CONFIGPATH, "help_files/about.html")
        self.min_spindle_rpm = INFO.MIN_SPINDLE_SPEED
        self.max_spindle_rpm = INFO.MAX_SPINDLE_SPEED
        self.system_list = ["G54","G55","G56","G57","G58","G59","G59.1","G59.2","G59.3"]
        self.tab_index_code = (0, 4, 5, 6, 3, 8, 5, 3, 4, 3, 7, 8, 3)
        self.slow_jog_factor = 10
        self.reload_tool = 0
        self.last_loaded_program = ""
        self.first_turnon = True
        self.source_file = ""
        self.tool_icons = {}
        self.lineedit_list = ["work_height", "touch_height", "sensor_height", "laser_x", "laser_y",
                              "sensor_x", "sensor_y", "camera_x", "camera_y",
                              "search_vel", "probe_vel", "max_probe", "eoffset_count"]
        self.onoff_list = ["frame_tool", "frame_dro", "frame_override", "frame_status"]
        self.auto_list = ["chk_eoffsets", "cmb_gcode_history", "btn_pause_spindle", "btn_file", "btn_offsets", "btn_tool", "btn_status", "btn_probe", "btn_camera", "btn_utils", "btn_touchy"]
        self.axis_a_list = ["label_axis_a", "dro_axis_a", "action_zero_a", "axistoolbutton_a", "dro_button_stack_a",
                             "widget_jog_angular", "widget_increments_angular", "a_plus_jogbutton", "a_minus_jogbutton"]
        self.axis_b_list = ["label_axis_b", "dro_axis_b",  "action_zero_b",  "axistoolbutton_b",
                            "dro_button_stack_b", "b_plus_jogbutton", "b_minus_jogbutton"]
        self.button_response_list = ["btn_start", "btn_home_all", "dro_button_stack_x", "dro_button_stack_y",
                            "dro_button_stack_z", "dro_button_stack_a", "dro_button_stack_b", "btn_reload_file"]

        STATUS.connect('general', self.dialog_return)
        STATUS.connect('state-on', lambda w: self.enable_onoff(True))
        STATUS.connect('state-off', lambda w: self.enable_onoff(False))
        STATUS.connect('mode-manual', lambda w: self.enable_auto(True))
        STATUS.connect('mode-mdi', lambda w: self.enable_auto(True))
        STATUS.connect('mode-auto', lambda w: self.enable_auto(False))
        STATUS.connect('interp-run', lambda w: self.set_button_response_state(True))
        STATUS.connect('interp-idle', lambda w: self.set_button_response_state(False))
        STATUS.connect('gcode-line-selected', lambda w, line: self.set_start_line(line))
        STATUS.connect('graphics-line-selected', lambda w, line: self.set_start_line(line))
        STATUS.connect('hard-limits-tripped', self.hard_limit_tripped)
        STATUS.connect('program-pause-changed', lambda w, state: self.w.btn_pause_spindle.setEnabled(state))
        STATUS.connect('actual-spindle-speed-changed', lambda w, speed: self.update_rpm(speed))
        STATUS.connect('user-system-changed', lambda w, data: self.user_system_changed(data))
        STATUS.connect('metric-mode-changed', lambda w, mode: self.metric_mode_changed(mode))
        STATUS.connect('tool-in-spindle-changed', lambda w, tool: self.tool_changed(tool))
        STATUS.connect('file-loaded', self.file_loaded)
        STATUS.connect('all-homed', self.all_homed)
        STATUS.connect('not-all-homed', self.not_all_homed)
        STATUS.connect('periodic', lambda w: self.update_runtimer())
        STATUS.connect('command-stopped', lambda w: self.stop_timer())
        STATUS.connect('progress', lambda w,p,t: self.updateProgress(p,t))
        STATUS.connect('override-limits-changed', lambda w, state, data: self._check_override_limits(state, data))
        STATUS.connect('graphics-gcode-properties', lambda w, d: self.update_gcode_properties(d))          
        self._block_signal = False

        self.html = """<html>
<head>
<title>Test page for the download:// scheme</title>
</head>
<body>
<h1>Setup Tab</h1>
<p>If you select a file with .html as a file ending, it will be shown here..</p>
<li><a href="http://linuxcnc.org/docs/devel/html/">Documents online</a></li>
<li><a href="file://">Local files</a></li>
<img src="file://%s" alt="lcnc_swoop" />
<hr />
</body>
</html>
""" %(os.path.join(paths.IMAGEDIR,'lcnc_swoop.png'))


    def class_patch__(self):
        self.old_fman = FM.load
        FM.load = self.load_code

    def initialized__(self):
        self.w.pushbutton_metric.clicked[bool].connect(self.change_mode)
        self.init_pins()
        self.init_preferences()
        self.init_widgets()
        self.init_probe()
        self.init_utils()
        self.w.stackedWidget_log.setCurrentIndex(0)        
        self.w.stackedWidget_dro.setCurrentIndex(0)
        self.w.stackedWidget_2.setCurrentIndex(0)        
        self.w.btn_pause_spindle.setEnabled(False)
        self.w.btn_dimensions.setChecked(True)
        self.w.page_buttonGroup.buttonClicked.connect(self.main_tab_changed)
        self.w.filemanager_usb.showMediaDir(quiet = True)
        self.w.btn_mdi_clear.pressed.connect(lambda: self.w.mdihistory.MDILine.clear())

        STATUS.connect('feed-override-changed', lambda w, data: self.w.pushbutton_fo.setText('FO {0:.0f}%'.format(data)))
        STATUS.connect('rapid-override-changed', lambda w, data: self.w.pushbutton_ro.setText('RO {0:.0f}%'.format(data)))
        STATUS.connect('spindle-override-changed', lambda w, data: self.w.pushbutton_so.setText('SO {0:.0f}%'.format(data)))
        STATUS.connect('jogincrement-changed', lambda w, incr,label:self.updateIncrementPin(incr))

    # hide widgets for A axis if not present
        if "A" not in INFO.AVAILABLE_AXES:
            for i in self.axis_a_list:
                self.w[i].hide()
            self.w.lbl_increments_linear.setText("INCREMENTS")
        if "B" not in INFO.AVAILABLE_AXES:
            for i in self.axis_b_list:
                self.w[i].hide()
    # set validators for lineEdit widgets
        for val in self.lineedit_list:
            self.w['lineEdit_' + val].setValidator(self.valid)
    # check for default setup html file
        try:
            # web view widget for SETUP page
            if self.w.web_view:
                self.toolBar = QtWidgets.QToolBar(self.w)
                self.w.tabWidget_setup.setCornerWidget(self.toolBar)

                self.backBtn = QtWidgets.QPushButton(self.w)
                self.backBtn.setEnabled(True)
                self.backBtn.setIconSize(QtCore.QSize(56, 34))
                self.backBtn.setIcon(QtGui.QIcon(':/qt-project.org/styles/commonstyle/images/left-32.png'))
                self.backBtn.clicked.connect(self.back)
                self.toolBar.addWidget(self.backBtn)

                self.forBtn = QtWidgets.QPushButton(self.w)
                self.forBtn.setEnabled(True)
                self.forBtn.setIconSize(QtCore.QSize(56, 34))
                self.forBtn.setIcon(QtGui.QIcon(':/qt-project.org/styles/commonstyle/images/right-32.png'))
                self.forBtn.clicked.connect(self.forward)
                self.toolBar.addWidget(self.forBtn)

                self.writeBtn = QtWidgets.QPushButton('SetUp\n Writer',self.w)
                self.writeBtn.setMinimumSize(64,40)
                self.writeBtn.setEnabled(True)
                self.writeBtn.clicked.connect(self.writer)
                self.toolBar.addWidget(self.writeBtn)

                self.w.layout_HTML.addWidget(self.w.web_view)
                if os.path.exists(self.default_setup):
                    self.w.web_view.load(QtCore.QUrl.fromLocalFile(self.default_setup))
                else:
                    self.w.web_view.setHtml(self.html)
        except Exception as e:
            print("No default setup file found - {}".format(e))

        # PDF setup page
        self.PDFView = PDFViewer.PDFView()
        self.w.layout_PDF.addWidget(self.PDFView)
        self.PDFView.loadSample('setup_tab')

        # check for default about html file
        try:
            url = QtCore.QUrl("file:///" + self.default_about)
            self.web_view_about.load(url)
        except Exception as e:
            print("No default setup file found - {}".format(e))

    def init_utils(self):
        from qtvcp.lib.gcode_utility.facing import Facing
        self.facing = Facing()
        self.w.layout_facing.addWidget(self.facing)

        from qtvcp.lib.gcode_utility.hole_circle import Hole_Circle
        self.hole_circle = Hole_Circle()
        self.w.layout_hole_circle.addWidget(self.hole_circle)

        # load the NgcGui widget into the utilities tab
        # then move (warp) the info tab from it to the left tab widget
        from qtvcp.lib.qt_ngcgui.ngcgui import NgcGui
        self.ngcgui = NgcGui()
        self.w.layout_ngcgui.addWidget(self.ngcgui)
        self.ngcgui.warp_info_frame(self.w.ngcGuiLeftLayout)

        #from auto_height import Auto_Measure
        self.auto_measure = Auto_Measure(self.w)
        self.w.layout_workpiece.addWidget(self.auto_measure)
        self.auto_measure._hal_init()
        # set up Zlevel probing
        from zlevel import ZLevel
        self.zlevel = ZLevel()
        self.w.layout_zlevel.insertWidget(0, self.zlevel)
        self.zlevel._hal_init()

    #############################
    # SPECIAL FUNCTIONS SECTION #
    #############################
    def init_pins(self):
        # spindle control pins
        pin = QHAL.newpin("spindle-amps", QHAL.HAL_FLOAT, QHAL.HAL_IN)
        pin.value_changed.connect(self.spindle_pwr_changed)
        pin = QHAL.newpin("spindle-volts", QHAL.HAL_FLOAT, QHAL.HAL_IN)
        pin.value_changed.connect(self.spindle_pwr_changed)
        pin = QHAL.newpin("spindle-fault", QHAL.HAL_U32, QHAL.HAL_IN)
        pin.value_changed.connect(self.spindle_fault_changed)
        pin = QHAL.newpin("spindle-modbus-errors", QHAL.HAL_U32, QHAL.HAL_IN)
        pin.value_changed.connect(self.mb_errors_changed)
        QHAL.newpin("spindle-inhibit", QHAL.HAL_BIT, QHAL.HAL_OUT)
        # external offset control pins
        QHAL.newpin("eoffset-enable", QHAL.HAL_BIT, QHAL.HAL_OUT)
        QHAL.newpin("eoffset-clear", QHAL.HAL_BIT, QHAL.HAL_OUT)
        QHAL.newpin("eoffset-spindle-count", QHAL.HAL_S32, QHAL.HAL_OUT)
        QHAL.newpin("eoffset-count", QHAL.HAL_S32, QHAL.HAL_OUT)

        pin = QHAL.newpin("eoffset-value", QHAL.HAL_S32, QHAL.HAL_IN)
        pin.value_changed.connect(self.eoffset_changed)

        pin = QHAL.newpin("eoffset-zlevel-count", QHAL.HAL_S32, QHAL.HAL_IN)
        pin.value_changed.connect(self.comp_count_changed)
        pin = QHAL.newpin("comp-count", QHAL.HAL_S32, QHAL.HAL_IN)
        pin.value_changed.connect(self.compensate_count_changed)
        pin = QHAL.newpin("map-ready", Qhal.HAL_BIT, Qhal.HAL_IN)
        pin.value_changed.connect(self.map_ready_changed)
        QHAL.newpin("comp-on", Qhal.HAL_BIT, Qhal.HAL_OUT)

        self.pin_mpg_in = self.h.newpin('mpg-in',hal.HAL_S32, hal.HAL_IN)
        self.pin_mpg_in.value_changed.connect(lambda s: self.external_mpg(s))
        self.wheel_x = self.h.newpin('jog.wheel.x',hal.HAL_BIT, hal.HAL_OUT)
        self.wheel_y = self.h.newpin('jog.wheel.y',hal.HAL_BIT, hal.HAL_OUT)
        self.wheel_z = self.h.newpin('jog.wheel.z',hal.HAL_BIT, hal.HAL_OUT)
        self.wheel_a = self.h.newpin('jog.wheel.a',hal.HAL_BIT, hal.HAL_OUT)
        self.wheel_b = self.h.newpin('jog.wheel.b',hal.HAL_BIT, hal.HAL_OUT)
        self.jog_increment = self.h.newpin('jog.wheel.incement',hal.HAL_FLOAT, hal.HAL_OUT)

        # MPG axis select pins
        pin = QHAL.newpin("axis-select-x", Qhal.HAL_BIT, Qhal.HAL_IN)        
        pin = QHAL.newpin("axis-select-y", Qhal.HAL_BIT, Qhal.HAL_IN)        
        pin = QHAL.newpin("axis-select-z", Qhal.HAL_BIT, Qhal.HAL_IN)        
        pin = QHAL.newpin("axis-select-a", Qhal.HAL_BIT, Qhal.HAL_IN)
        pin = QHAL.newpin("axis-select-b", Qhal.HAL_BIT, Qhal.HAL_IN)

    def init_preferences(self):
        if not self.w.PREFS_:
            self.add_status("CRITICAL - no preference file found, enable preferences in screenoptions widget")
            return
        self.last_loaded_program = self.w.PREFS_.getpref('last_loaded_file', None, str,'BOOK_KEEPING')
        self.reload_tool = self.w.PREFS_.getpref('Tool to load', 0, int,'CUSTOM_FORM_ENTRIES')
        self.w.lineEdit_laser_x.setText(str(self.w.PREFS_.getpref('Laser X', 100, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_laser_y.setText(str(self.w.PREFS_.getpref('Laser Y', -20, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_sensor_x.setText(str(self.w.PREFS_.getpref('Sensor X', 10, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_sensor_y.setText(str(self.w.PREFS_.getpref('Sensor Y', 10, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_camera_x.setText(str(self.w.PREFS_.getpref('Camera X', 10, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_camera_y.setText(str(self.w.PREFS_.getpref('Camera Y', 10, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_work_height.setText(str(self.w.PREFS_.getpref('Work Height', 20, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_touch_height.setText(str(self.w.PREFS_.getpref('Touch Height', 40, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_sensor_height.setText(str(self.w.PREFS_.getpref('Sensor Height', 40, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_search_vel.setText(str(self.w.PREFS_.getpref('Search Velocity', 40, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_probe_vel.setText(str(self.w.PREFS_.getpref('Probe Velocity', 10, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_z_safe_travel.setText(str(self.w.PREFS_.getpref('Z Safe Travel', 10, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_max_probe.setText(str(self.w.PREFS_.getpref('Max Probe', 10, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_eoffset_count.setText(str(self.w.PREFS_.getpref('Eoffset count', 0, int, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_retract_distance.setText(str(self.w.PREFS_.getpref('Retract Distance', 10, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.chk_eoffsets.setChecked(self.w.PREFS_.getpref('External offsets', False, bool, 'CUSTOM_FORM_ENTRIES'))
        self.w.chk_reload_program.setChecked(self.w.PREFS_.getpref('Reload program', False, bool,'CUSTOM_FORM_ENTRIES'))
        self.w.chk_reload_tool.setChecked(self.w.PREFS_.getpref('Reload tool', False, bool,'CUSTOM_FORM_ENTRIES'))
        self.w.chk_use_keyboard.setChecked(self.w.PREFS_.getpref('Use keyboard', False, bool, 'CUSTOM_FORM_ENTRIES'))
        self.w.chk_run_from_line.setChecked(self.w.PREFS_.getpref('Run from line', False, bool, 'CUSTOM_FORM_ENTRIES'))
        self.w.chk_use_virtual.setChecked(self.w.PREFS_.getpref('Use virtual keyboard', False, bool, 'CUSTOM_FORM_ENTRIES'))
        self.w.chk_use_tool_sensor.setChecked(self.w.PREFS_.getpref('Use tool sensor', False, bool, 'CUSTOM_FORM_ENTRIES'))
        self.w.chk_use_camera.setChecked(self.w.PREFS_.getpref('Use camera', False, bool, 'CUSTOM_FORM_ENTRIES'))
        self.w.chk_alpha_mode.setChecked(self.w.PREFS_.getpref('Use alpha display mode', False, bool, 'CUSTOM_FORM_ENTRIES'))
        self.w.chk_inhibit_selection.setChecked(self.w.PREFS_.getpref('Inhibit display mouse selection', True, bool, 'CUSTOM_FORM_ENTRIES'))
        self.w.chk_use_mdi_keyboard.setChecked(self.w.PREFS_.getpref('Use MDI Keyboard', False, bool, 'CUSTOM_FORM_ENTRIES'))  
        self.w.cone_size.setValue(self.w.PREFS_.getpref('Preview cone size', 0.5, float, 'GUI_OPTIONS'))
        self.w.grid_size.setValue(self.w.PREFS_.getpref('Preview grid size', 0, float, 'GUI_OPTIONS'))
        self.cone_size_changed(self.w.cone_size.value())
        self.grid_size_changed(self.w.grid_size.value())
        # read tool icon text file
        fname = os.path.join(PATH.CONFIGPATH, "tool_icons/tool_icons.txt")
        if os.path.isfile(fname):
            with open(fname) as file:
                for line in file:
                    line = line.rstrip()
                    (key, val) = line.split(':')
                    self.tool_icons[int(key)] = val        
        
    def closing_cleanup__(self):
        if not self.w.PREFS_: return
        if self.last_loaded_program is not None:
            self.w.PREFS_.putpref('last_loaded_directory', os.path.dirname(self.last_loaded_program), str, 'BOOK_KEEPING')
            self.w.PREFS_.putpref('last_loaded_file', self.last_loaded_program, str, 'BOOK_KEEPING')
        self.w.PREFS_.putpref('Tool to load', STATUS.get_current_tool(), int, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Laser X', self.w.lineEdit_laser_x.text().encode('utf-8'), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Laser Y', self.w.lineEdit_laser_y.text().encode('utf-8'), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Sensor X', self.w.lineEdit_sensor_x.text().encode('utf-8'), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Sensor Y', self.w.lineEdit_sensor_y.text().encode('utf-8'), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Camera X', self.w.lineEdit_camera_x.text().encode('utf-8'), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Camera Y', self.w.lineEdit_camera_y.text().encode('utf-8'), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Work Height', self.w.lineEdit_work_height.text().encode('utf-8'), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Touch Height', self.w.lineEdit_touch_height.text().encode('utf-8'), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Sensor Height', self.w.lineEdit_sensor_height.text().encode('utf-8'), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Search Velocity', self.w.lineEdit_search_vel.text().encode('utf-8'), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Probe Velocity', self.w.lineEdit_probe_vel.text().encode('utf-8'), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Max Probe', self.w.lineEdit_max_probe.text().encode('utf-8'), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Z Safe Travel', self.w.lineEdit_z_safe_travel.text().encode('utf-8'), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Eoffset count', self.w.lineEdit_eoffset_count.text().encode('utf-8'), int, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('External offsets', self.w.chk_eoffsets.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Retract Distance', self.w.lineEdit_retract_distance.text().encode('utf-8'), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Reload program', self.w.chk_reload_program.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Reload tool', self.w.chk_reload_tool.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Use keyboard', self.w.chk_use_keyboard.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Run from line', self.w.chk_run_from_line.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Use virtual keyboard', self.w.chk_use_virtual.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Use tool sensor', self.w.chk_use_tool_sensor.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Use camera', self.w.chk_use_camera.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Use alpha display mode', self.w.chk_alpha_mode.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Inhibit display mouse selection', self.w.chk_inhibit_selection.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Use MDI Keyboard', self.w.chk_use_mdi_keyboard.isChecked(), bool, 'CUSTOM_FORM_ENTRIES') 
        self.w.PREFS_.putpref('Preview grid size', self.w.grid_size.value(), float, 'GUI_OPTIONS')
        # save tool table icons
        path = os.path.join(PATH.CONFIGPATH, "tool_icons")
        if os.path.isdir(path):
            fname = os.path.join(path, "tool_icons.txt")
            f = open(fname, "w")
            for key in self.tool_icons:
                f.write(str(key) + ":" + self.tool_icons[key] + "\n")        

    def init_widgets(self):
        self.w.main_tab_widget.setCurrentIndex(TAB_MAIN)
        self.w.chk_override_limits.setChecked(False)
        self.w.chk_override_limits.setEnabled(False)
        self.w.lbl_maxv_percent.setText("100 %")
        self.w.lbl_home_x.setText(INFO.get_error_safe_setting('JOINT_0', 'HOME',"50"))
        self.w.lbl_home_y.setText(INFO.get_error_safe_setting('JOINT_1', 'HOME',"50"))
        self.w.cmb_gcode_history.addItem("No File Loaded")
        self.w.cmb_gcode_history.wheelEvent = lambda event: None
        self.w.jogincrements_linear.wheelEvent = lambda event: None
        self.w.jogincrements_angular.wheelEvent = lambda event: None
        self.w.gcode_editor.hide()
        self.w.filemanager.list.setAlternatingRowColors(False)
        self.w.filemanager_usb.list.setAlternatingRowColors(False)
        self.w.filemanager_usb.showList()

        # gcode file history
        self.w.cmb_gcode_history.addItem("No File Loaded")
        self.w.cmb_gcode_history.view().setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        # mdi history
        self.w.mdihistory.MDILine.setFixedHeight(30)
        self.w.mdihistory.MDILine.setPlaceholderText('MDI:')
        self. use_mdi_keyboard_changed(self.w.chk_use_mdi_keyboard.isChecked())
        self.w.cmb_mdi_texts.addItem("SELECT")
        self.w.cmb_mdi_texts.addItem("HALSHOW")
        self.w.cmb_mdi_texts.addItem("HALMETER")
        self.w.cmb_mdi_texts.addItem("HALSCOPE")
        self.w.cmb_mdi_texts.addItem("STATUS")
        self.w.cmb_mdi_texts.addItem("CLASSICLADDER")
        self.w.cmb_mdi_texts.addItem("CALIBRATION")
        self.w.cmb_mdi_texts.addItem("PREFERENCE")
        self.w.cmb_mdi_texts.addItem("CLEAR HISTORY")

        #tool icons
        self.w.cmb_tool_icons.addItem("SELECT\nICON")
        self.w.cmb_tool_icons.view().setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        path = os.path.join(PATH.CONFIGPATH, "tool_icons")
        if os.path.isdir(path):
            icons = os.listdir(path)
            for item in icons:
                if not item.endswith(".txt"):
                    self.w.cmb_tool_icons.addItem(item)
            self.w.cmb_tool_icons.addItem("undefined")
            self.w.cmb_tool_icons.setCurrentIndex(0)
        else:
            pass

        # set calculator mode for menu buttons
        for i in ("x", "y", "z", "a", "b"):
            self.w["axistoolbutton_" + i].set_dialog_code('CALCULATOR')
       
        # disable mouse wheel events on comboboxes
        self.w.cmb_gcode_history.wheelEvent = lambda event: None
        self.w.cmb_stylesheet.wheelEvent = lambda event: None        
        self.w.jogincrements_linear.wheelEvent = lambda event: None
        self.w.jogincrements_angular.wheelEvent = lambda event: None
        self.w.gcode_editor.hide()
        # turn off table grids
        self.w.filemanager.table.setShowGrid(False)
        self.w.filemanager_usb.table.setShowGrid(False)
        self.w.tooloffsetview.setShowGrid(False)
        self.w.offset_table.setShowGrid(False)

        #self.web_view = QWebView()
        self.web_view_about = QWebView()
        self.w.layout_about.addWidget(self.web_view_about)     

        if not INFO.MACHINE_IS_METRIC:
            self.w.lbl_tool_sensor_B2W.setText('INCH')
            self.w.lbl_tool_sensor_B2S.setText('INCH')
            self.w.lbl_touchheight_units.setText('INCH')
            self.w.lbl_max_probe_units.setText('INCH')
            self.w.lbl_search_vel_units.setText('INCH/<sup> MIN</sup>')
            self.w.lbl_probe_vel_units.setText('INCH/<sup> MIN</sup>')
            self.w.lbl_z_ext_offset.setText('INCH')
            self.w.lbl_tool_sensor_loc.setText('INCH')
            self.w.lbl_laser_offset.setText('INCH')
            self.w.lbl_camera_offset.setText('INCH')
            self.w.lbl_touchheight_units.setText('INCH')

        # populate stylesheet combobox - basically a copy of styleeditor combobox
        for i in range(self.styleeditor.styleSheetCombo.count()):
            item = self.styleeditor.styleSheetCombo.itemText(i)
            self.w.cmb_stylesheet.addItem(item)

        #set up gcode list
        self.gcodes.setup_list()

    def init_probe(self):
        probe = INFO.get_error_safe_setting('PROBE', 'USE_PROBE', 'none').lower()
        if probe == 'versaprobe':
            LOG.info("Using Versa Probe")
            from qtvcp.widgets.versa_probe import VersaProbe
            self.probe = VersaProbe()
            self.probe.setObjectName('versaprobe')
        elif probe == 'basicprobe':
            LOG.info("Using Basic Probe")
            from qtvcp.widgets.basic_probe import BasicProbe
            self.probe = BasicProbe()
            self.probe.setObjectName('basicprobe')
        else:
            LOG.info("No valid probe widget specified")
            self.w.btn_probe.hide()
            return
        self.w.probe_layout.addWidget(self.probe)
        self.probe.hal_init()

    def processed_focus_event__(self, receiver, event):
        if not self.w.chk_use_virtual.isChecked() or STATUS.is_auto_mode(): return
        if isinstance(receiver, QtWidgets.QLineEdit):
            if not receiver.isReadOnly():
                self.w.stackedWidget_dro.setCurrentIndex(1)
        elif isinstance(receiver, QtWidgets.QTableView):
            self.w.stackedWidget_dro.setCurrentIndex(1)
#        elif isinstance(receiver, QtWidgets.QCommonStyle):
#            return
    
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
                if isinstance(receiver2, writer.Main):
                    flag = True
                    break
                receiver2 = receiver2.parent()

            if flag:
                if isinstance(receiver2, GCODE):
                    # if in manual or in readonly mode do our keybindings - otherwise
                    # send events to gcode widget
                    if STATUS.is_man_mode() == False or not receiver2.isReadOnly():
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

    #########################
    # CALLBACKS FROM STATUS #
    #########################

    def updateIncrementPin(self, incr):
        self.jog_increment.set(incr)

    def spindle_pwr_changed(self, data):
        # this calculation assumes the voltage is line to neutral
        # and that the synchronous motor spindle has a power factor of 0.9
        power = self.h['spindle-volts'] * self.h['spindle-amps'] * 2.7 # 3 x V x I x PF
        amps = "{:1.1f}".format(self.h['spindle-amps'])
        pwr = "{:1.1f}".format(power)
        self.w.lbl_spindle_amps.setText(amps)
        self.w.lbl_spindle_power.setText(pwr)

    def spindle_fault_changed(self, data):
        fault = hex(self.h['spindle-fault'])
        self.w.lbl_spindle_fault.setText(fault)

    def spindle_faster(self):
        if STATUS.is_spindle_on(0):
            ACTION.SET_SPINDLE_FASTER(0)

    def spindle_slover(self):
        if STATUS.is_spindle_on(0):
            ACTION.SET_SPINDLE_SLOWER(0)

    def eoffset_changed(self, data):
        self.w.z_comp_eoffset_value.setText(format(data*.001, '.3f'))
        
    def comp_count_changed(self):
        if self.w.btn_enable_comp.isChecked():
            self.h['eoffset-count'] = self.h['eoffset-zlevel-count']

    def compensate_count_changed(self):        
        if self.w.btn_enable_comp.isChecked():
            self.h['eoffset-count'] = self.h['comp-count']

    def map_ready_changed(self):
        if self.h['map-ready'] is True:
            self.w.lbl_height_map.setPixmap(QtGui.QPixmap('height_map.png'))

    def command_stopped(self, obj):
        if self.w.btn_pause_spindle.isChecked():
            self.h['spindle-inhibit'] = False
            self.h['eoffset-count'] = 0 

    def mb_errors_changed(self, data):
        errors = self.h['spindle-modbus-errors']
        self.w.lbl_mb_errors.setText(str(errors))

    def dialog_return(self, w, message):
        rtn = message.get('RETURN')
        name = message.get('NAME')
        plate_code = bool(message.get('ID') == '_touchplate_')
        sensor_code = bool(message.get('ID') == '_toolsensor_')
        wait_code = bool(message.get('ID') == '_wait_resume_')
        unhome_code = bool(message.get('ID') == '_unhome_')
        overwrite = bool(message.get('ID') == '_overwrite_')
        if plate_code and name == 'MESSAGE' and rtn is True:
            self.touchoff('touchplate')
        elif sensor_code and name == 'MESSAGE' and rtn is True:
            self.touchoff('sensor')
        elif wait_code and name == 'MESSAGE':
            self.h['eoffset-clear'] = False
        elif unhome_code and name == 'MESSAGE' and rtn is True:
            ACTION.SET_MACHINE_UNHOMED(-1)
        elif overwrite and name == 'MESSAGE':
            if rtn is True:
                self.do_file_copy()
            else:
                self.add_status("File not copied")

    def user_system_changed(self, data):
        sys = self.system_list[int(data) - 1]
        self.w.offset_table.selectRow(int(data) + 3)
        self.w.actionbutton_rel.setText(sys)

    def metric_mode_changed(self, mode):
        rate = (float(self.w.slider_rapid_ovr.value()) / 100)
        if mode is False:
            self.w.lbl_jog_linear.setText('INCH/<sup>MIN</sup>')
            self.factor = INFO.convert_machine_to_imperial(INFO.MAX_TRAJ_VELOCITY)
        else:
            self.w.lbl_jog_linear.setText('MM/<sup>MIN</sup>')
            self.factor = INFO.convert_machine_to_metric(INFO.MAX_TRAJ_VELOCITY)
        self.w.lbl_max_rapid.setText("{:4.0f}".format(rate * self.factor))

    def tool_changed(self, tool):
        if tool in self.tool_icons and self.tool_icons[tool] != "undefined":
            img_path = self.tool_icons[tool]
            img = os.path.join(PATH.CONFIGPATH, "tool_icons/" + img_path)
            self.w.lbl_tool_image.setPixmap(QtGui.QPixmap(img))
        else:
            self.w.lbl_tool_image.clear()
            self.w.lbl_tool_image.setText("NO\nTOOL\nICON")
            self.add_status("No icon selected for this tool")

    def file_loaded(self, obj, filename):
        if os.path.basename(filename).count('.') > 1:
            self.last_loaded_program = ""
            return
        if filename is not None:
            self.add_status("Loaded file {}".format(filename))
            self.w.progressBar.setValue(0)
            self.last_loaded_program = filename
            self.w.lbl_runtime.setText("00:00:00")
        else:
            self.add_status("Filename not valid", CRITICAL)

    def runFromLineClicked(self):
        text = self.w.runFromLineEdit.text()        
        if text !='':
            ACTION.RUN(line = int(text))

    def btn_start_macro_clicked(self):
        if STATUS.is_mdi_mode():
           print ('run MDI')
        self.w.mditouchy.run_command()
        return

    def updateProgress(self, p,text):
        if p <0:
            self.w.progressBar.setValue(0)
            self.w.progressBar.setFormat('PROGRESS')
        else:
            self.w.progressBar.setValue(p)
            self.w.progressBar.setFormat('{}: {}%'.format(text, p))

    def percent_loaded_changed(self, fraction):
        if fraction <0:
            self.w.progressBar.setValue(0)
            self.w.progressBar.setFormat('PROGRESS')
        else:
            self.w.progressBar.setValue(fraction)
            self.w.progressBar.setFormat('LOADING: {}%'.format(fraction))

    def percent_done_changed(self, fraction):
        self.w.progressBar.setValue(fraction)
        if fraction <0:
            self.w.progressBar.setValue(0)
            self.w.progressBar.setFormat('PROGRESS')
        else:
            self.w.progressBar.setFormat('COMPLETE: {}%'.format(fraction))

    def all_homed(self, obj):
        self.home_all = True
        self.w.btn_home_all.setText("ALL HOMED")
        if self.first_turnon is True:
            self.first_turnon = False
            if self.w.chk_reload_tool.isChecked():
                command = "M61 Q{} G43".format(self.reload_tool)
                ACTION.CALL_MDI(command)                
            if self.last_loaded_program is not None and self.w.chk_reload_program.isChecked():
                if os.path.isfile(self.last_loaded_program):
                    self.w.cmb_gcode_history.addItem(self.last_loaded_program)
                    self.w.cmb_gcode_history.setCurrentIndex(self.w.cmb_gcode_history.count() - 1)
                    self.w.cmb_gcode_history.setToolTip(self.last_loaded_program)
                    ACTION.OPEN_PROGRAM(self.last_loaded_program)
        ACTION.SET_MANUAL_MODE()
        self.w.manual_mode_button.setChecked(True)

    def not_all_homed(self, obj, list):
        self.home_all = False
        self.w.btn_home_all.setText("HOME ALL")

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

    def set_button_response_state(self, state):
        for i in (self.button_response_list):
            self.w[i].setEnabled(not state)

    #######################
    # CALLBACKS FROM FORM #
    #######################

    def updateJogState(self):
        state = self.w.pushbutton_jog.isChecked()
        selected = None
        if state:
            ACTION.SET_MANUAL_MODE()
            selected = STATUS.get_selected_axis()
        for temp in INFO.AVAILABLE_AXES:
            if selected == temp:
                self['wheel_{}'.format(temp.lower())].set(state)
            else:
                self['wheel_{}'.format(temp.lower())].set(False)

    def full_screen(self, state):
        if state:
            self.w.stackedWidget_0.setCurrentIndex(1)
            self.w.widgetswitcher.show_id_widget(1)
        else:
            self.w.stackedWidget_0.setCurrentIndex(0)
            self.w.widgetswitcher.show_id_widget(0)

    def switch(self, state):
        #print state
        if state:
            self.w.stackedWidget_2.setCurrentIndex(8)
            self.w.widgetswitcher_2.show_id_widget(1)           
        elif state == 0:
            self.w.widgetswitcher_2.show_id_widget(0)
            self.w.main_tab_widget.setCurrentIndex(0)

    # main button bar
    def main_tab_changed(self, btn):
        index = btn.property("index")
        if index == self.w.main_tab_widget.currentIndex():
            self.w.stackedWidget_dro.setCurrentIndex(0)
        if index is None: return        
        self.w.main_tab_widget.setCurrentIndex(index)
        self.w.stackedWidget_2.setCurrentIndex(self.tab_index_code[index])                
        if index == TAB_MAIN:
            self.w.stackedWidget_dro.setCurrentIndex(0)
            self.w.stackedWidget_2.setCurrentIndex(0)
        if index == TAB_UTILITIES:
            self.w.tabWidget_utilities.setCurrentIndex(2)
            self.w.stackedWidget_2.setCurrentIndex(7)             
        # toggle home/tool offsets buttons in DRO section
        if index == TAB_TOOL:
            num = 1
        else:
            num = 0
        for i in INFO.AVAILABLE_AXES:
            self.w['dro_button_stack_%s'%i.lower()].setCurrentIndex(num)
            
    def mdi_select_text(self):
        if self.w.cmb_mdi_texts.currentIndex() <= 0: return
        self.w.mdihistory.MDILine.setText(self.w.cmb_mdi_texts.currentText())
        self.w.cmb_mdi_texts.setCurrentIndex(0)

    def mdi_enter_pressed(self):
        self.w.mdihistory.run_command()
        self.w.mdihistory.MDILine.clear()

    # gcode frame
    def cmb_gcode_history_clicked(self):
        if self.w.cmb_gcode_history.currentIndex() == 0: return
        filename = self.w.cmb_gcode_history.currentText()
        if filename == self.last_loaded_program:
            self.add_status("Selected program is already loaded")
        else:
            ACTION.OPEN_PROGRAM(filename)

    # program frame
    def btn_start_clicked(self, obj):
        if not STATUS.is_all_homed():
           self.add_status("Machine must be is homed", CRITICAL)
           return
        if not  os.path.exists(self.last_loaded_program):
            self.add_status("No program to execute", WARNING)
            return
        if not STATUS.is_auto_mode():
            self.add_status("Must be in AUTO mode to run a program", WARNING)
            return
        if self.w.main_tab_widget.currentIndex() != 0:
            self.add_status("Switch view mode to MAIN", WARNING)
            return
        if STATUS.is_auto_running():
            self.add_status("Program is already running", WARNING)
            return
        self.run_time = 0
        self.w.lbl_runtime.setText("00:00:00")
        if self.start_line <= 1:
            ACTION.RUN(self.start_line)
        else:
            # instantiate run from line preset dialog
            info = '<b>Running From Line: {} <\b>'.format(self.start_line)
            mess = {'NAME':'RUNFROMLINE', 'TITLE':'Preset Dialog', 'ID':'_RUNFROMLINE', 'MESSAGE':info, 'LINE':self.start_line}
            ACTION.CALL_DIALOG(mess)
        self.add_status("Started program from line {}".format(self.start_line))
        self.timer_on = True

    def btn_pause_spindle_clicked(self, state):
        eofsets = self.w.chk_eoffsets.isChecked()
        if eofsets:
            self.w.action_pause.setEnabled(not state)
            self.w.action_step.setEnabled(not state)
            if state:
            # set external offsets to lift spindle
                self.h['eoffset-enable'] = self.w.chk_eoffsets.isChecked()
                fval = float(self.w.lineEdit_eoffset_count.text())
                self.h['eoffset-spindle-count'] = int(fval)
                self.w.spindle_eoffset_value.setText(self.w.lineEdit_eoffset_count.text())
                self.h['spindle-inhibit'] = True
                #self.w.btn_enable_comp.setChecked(False)
                #self.w.widget_zaxis_offset.hide()
                if not QHAL.hal.component_exists("z_level_compensation"):
                    self.add_status("Z level compensation HAL component not loaded", CRITICAL)
                    return
                #self.h['comp-on'] = False
            else:
                self.h['eoffset-spindle-count'] = 0
                self.w.spindle_eoffset_value.setText('0')
                #self.h['eoffset-clear'] = True
                self.h['spindle-inhibit'] = False
                if STATUS.is_auto_running():
                # instantiate warning box
                    info = "Wait for spindle at speed signal before resuming"
                    mess = {'NAME':'MESSAGE', 'ICON':'WARNING',
                        'ID':'_wait_resume_', 'MESSAGE':'CAUTION',
                        'NONBLOCKING':'True', 'MORE':info, 'TYPE':'OK'}
                    ACTION.CALL_DIALOG(mess)

    def btn_enable_comp_clicked(self, state):
        if state:
            fname = os.path.join(PATH.CONFIGPATH, "probe_points.txt")
            if not os.path.isfile(fname):
                self.add_status(fname + " not found", CRITICAL)
                self.w.btn_enable_comp.setChecked(False)
                return
            if not QHAL.hal.component_exists("z_level_compensation"):
                self.add_status("Z level compensation HAL component not loaded", CRITICAL)
                self.w.btn_enable_comp.setChecked(False)
                return
            self.h['comp-on'] = True
            self.add_status("Z level compensation ON")
        else:
            if not QHAL.hal.component_exists("z_level_compensation"):
                self.add_status("Z level compensation HAL component not loaded", CRITICAL)
                return
            self.h['comp-on'] = False
            self.add_status("Z level compensation OFF", WARNING)

    def btn_stop_clicked(self):
        if not self.w.btn_pause_spindle.isChecked(): return
        self.w.btn_pause_spindle.setChecked(False)
        self.btn_pause_spindle_clicked(False)

    def btn_reload_file_clicked(self):
        if self.last_loaded_program:
            self.w.progressBar.setValue(0)
            self.add_status("Loaded program file {}".format(self.last_loaded_program))
            ACTION.OPEN_PROGRAM(self.last_loaded_program)

    # DRO frame
    def btn_home_all_clicked(self, obj):
        if self.home_all is False:
            ACTION.SET_MACHINE_HOMING(-1)
        else:
        # instantiate dialog box
            info = "Unhome All Axes?"
            mess = {'NAME':'MESSAGE', 'ID':'_unhome_', 'MESSAGE':'UNHOME ALL', 'MORE':info, 'TYPE':'OKCANCEL'}
            ACTION.CALL_DIALOG(mess)

    def btn_home_clicked(self):
        axisnum = self.w.sender().property('joint')
        joint = INFO.get_jnum_from_axisnum(axisnum)
        if STATUS.is_joint_homed(joint) == True:
            ACTION.SET_MACHINE_UNHOMED(joint)
        else:
            ACTION.SET_MACHINE_HOMING(joint)
        
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

    def slider_maxv_changed(self, value):
        maxpc = (float(value) / INFO.MAX_LINEAR_JOG_VEL) * 100
        self.w.lbl_maxv_percent.setText("{:3.0f} %".format(maxpc))

    def slider_rapid_changed(self, value):
        rapid = (float(value) / 100) * self.factor
        self.w.lbl_max_rapid.setText("{:4.0f}".format(rapid))

    def btn_maxv_100_clicked(self):
        self.w.slider_maxv_ovr.setValue(INFO.MAX_LINEAR_JOG_VEL)

    def btn_maxv_50_clicked(self):
        self.w.slider_maxv_ovr.setValue(INFO.MAX_LINEAR_JOG_VEL / 2)

    # file tab
    def btn_gcode_edit_clicked(self, state):
        if not STATUS.is_on_and_idle():
            return
        if state:
            self.w.filemanager.hide()
            self.w.widget_file_copy.hide()
            self.w.gcode_editor.show()
            self.w.gcode_editor.editMode()
        else:
            self.w.filemanager.show()
            self.w.widget_file_copy.show()
            self.w.gcode_editor.hide()
            self.w.gcode_editor.readOnlyMode()

    def btn_load_file_clicked(self):
        fname = self.w.filemanager.getCurrentSelected()
        if fname[1] is True:
            self.load_code(fname[0])

    def btn_copy_file_clicked(self):
        if self.w.btn_gcode_edit.isChecked(): return
        if self.w.sender() == self.w.btn_copy_right:
            source = self.w.filemanager_usb.getCurrentSelected()
            target = self.w.filemanager.getCurrentSelected()
        elif self.w.sender() == self.w.btn_copy_left:
            source = self.w.filemanager.getCurrentSelected()
            target = self.w.filemanager_usb.getCurrentSelected()
        else:
            return
        if source[1] is False:
            self.add_status("Specified source is not a file", WARNING)
            return
        self.source_file = source[0]
        if target[1] is True:
            self.destination_file = os.path.join(os.path.dirname(target[0]), os.path.basename(source[0]))
        else:
            self.destination_file = os.path.join(target[0], os.path.basename(source[0]))
        if os.path.isfile(self.destination_file):
            info = "{} already exists in destination directory".format(self.destination_file)
            mess = {'NAME':'MESSAGE', 'ICON':'WARNING', 'ID':'_overwrite_', 'MESSAGE':'OVERWRITE FILE?', 'MORE':info, 'TYPE':'YESNO','NONBLOCKING':True}
            ACTION.CALL_DIALOG(mess)
        else:
            self.do_file_copy()

    # offsets tab
    def btn_goto_sensor_clicked(self):
        x = float(self.w.lineEdit_sensor_x.text())
        y = float(self.w.lineEdit_sensor_y.text())
        if not STATUS.is_metric_mode():
            x = x / 25.4
            y = y / 25.4
        ACTION.CALL_MDI("G90")
        ACTION.CALL_MDI_WAIT("G53 G0 Z0")
        command = "G53 G0 X{:3.4f} Y{:3.4f}".format(x, y)
        ACTION.CALL_MDI_WAIT(command, 10)
 
    def btn_ref_laser_clicked(self):
        x = float(self.w.lineEdit_laser_x.text())
        y = float(self.w.lineEdit_laser_y.text())
        if not STATUS.is_metric_mode():
            x = x / 25.4
            y = y / 25.4
        self.add_status("Laser offsets set")
        command = "G10 L20 P0 X{:3.4f} Y{:3.4f}".format(x, y)
        ACTION.CALL_MDI(command)
    
    def btn_ref_camera_clicked(self):
        x = float(self.w.lineEdit_camera_x.text())
        y = float(self.w.lineEdit_camera_y.text())
        if not STATUS.is_metric_mode():
            x = x / 25.4
            y = y / 25.4
        self.add_status("Camera offsets set")
        command = "G10 L20 P0 X{:3.4f} Y{:3.4f}".format(x, y)
        ACTION.CALL_MDI(command)
    
    # tool tab
    def btn_m61_clicked(self):
        checked = self.w.tooloffsetview.get_checked_list()
        if len(checked) > 1:
            self.add_status("Select only 1 tool to load", WARNING)
        elif checked:
            self.add_status("Loaded tool {}".format(checked[0]))
            ACTION.CALL_MDI("M61 Q{} G43".format(checked[0]))            
        else:
            self.add_status("No tool selected", CRITICAL)

    def cmb_tool_icons_clicked(self, index):
        if index == 0: return
        checked = self.w.tooloffsetview.get_checked_list()
        if not checked:
            self.add_status("No tool has been checked")
        else:
            icon = self.w.cmb_tool_icons.currentText()
            tool = checked[0]
            self.tool_icons[tool] = icon
            img = os.path.join(PATH.CONFIGPATH, "tool_icons/" + icon)
            self.w.lbl_tool_image.setPixmap(QtGui.QPixmap(img))
        self.w.cmb_tool_icons.setCurrentIndex(0)

    def on_keycall_F10(self,event,state,shift,cntrl):
        self.w.showNormal()

    def on_keycall_F11(self,event,state,shift,cntrl):
        if self.w.isFullScreen() == False:
             self.w.showFullScreen()

    def btn_touchoff_clicked(self):
        if STATUS.get_current_tool() == 0:
            self.add_status("Cannot touchoff with no tool loaded", CRITICAL)
            return
        if not STATUS.is_all_homed():
            self.add_status("Must be homed to perform tool touchoff", CRITICAL)
            return
        # instantiate dialog box
        sensor = self.w.sender().property('sensor')
        info = "Ensure tooltip is within {} mm of tool sensor and click OK".format(self.w.lineEdit_max_probe.text())
        mess = {'NAME':'MESSAGE', 'ID':sensor, 'MESSAGE':'TOOL TOUCHOFF', 'MORE':info, 'TYPE':'OKCANCEL'}
        ACTION.CALL_DIALOG(mess)
        
    # status tab
    def btn_clear_status_clicked(self):
        STATUS.emit('update-machine-log', None, 'DELETE')

    def btn_save_status_clicked(self):
        text = self.w.machinelog.toPlainText()
        filename = self.w.lbl_clock.text()
        filename = 'status_' + filename.replace(' ','_') + '.txt'
        self.add_status("Saving Status file to {}".format(filename))
        with open(filename, 'w') as f:
            f.write(text)

    def btn_dimensions_clicked(self, state):
        self.w.gcodegraphics.show_extents_option = state
        self.w.gcodegraphics.clear_live_plotter()

    # DRO
    def btn_setdro_clicked(self, state):
        self.w.gcodegraphics.setdro(state)
        self.w.gcodegraphics.setoverlay(state)
    def getdro(self):
        return self.enable_dro

    def btn_show_offsets_clicked(self, state):
        self.w.gcodegraphics.setShowOffsets(state)
        self.w.gcodegraphics.setoverlay(state)
    def getShowOffsets(self):
        return self.show_offsets
        
    # camview tab
    def cam_zoom_changed(self, value):
        self.w.camview.scale = float(value) / 10

    def cam_dia_changed(self, value):
        self.w.camview.diameter = value

    def cam_rot_changed(self, value):
        self.w.camview.rotation = float(value) / 10

    def grid_size_changed(self, data):
      # grid size is in inches
        grid = data / self.unitsPerMm / 25.4
        self.w.gcodegraphics.grid_size = grid

    def cone_size_changed(self, data):
        self.w.gcodegraphics.set_cone_basesize(data)

    # settings tab
    def chk_override_limits_checked(self, state):
        # only toggle override if it's not in synch with the button
        if state and not STATUS.is_limits_override_set():
            self.add_status("Override limits set", WARNING)
            ACTION.TOGGLE_LIMITS_OVERRIDE()
        elif not state and STATUS.is_limits_override_set():
            error = ACTION.TOGGLE_LIMITS_OVERRIDE()
            # if override can't be released set the check button to reflect this
            if error == False:
                self.w.chk_override_limits.blockSignals(True)
                self.w.chk_override_limits.setChecked(True)
                self.w.chk_override_limits.blockSignals(False)
            else:
                self.add_status("Override limits not set")

    def chk_run_from_line_checked(self, state):
        self.w.btn_start.setText("START\n1") if state else self.w.btn_start.setText("CYCLE\nSTART")

    def chk_alpha_mode_changed(self, state):
        self.w.gcodegraphics.set_alpha_mode(state)

    def chk_inhibit_selection_changed(self, state):
        self.w.gcodegraphics.set_inhibit_selection(state)

    def chk_use_camera_changed(self, state):
        self.w.btn_ref_camera.setEnabled(state)
        self.w.btn_camera.show() if state else self.w.btn_camera.hide()

    def chk_use_sensor_changed(self, state):
        self.w.btn_touch_sensor.setEnabled(state)

    def chk_use_virtual_changed(self, state):
        if not state:
            self.w.stackedWidget_dro.setCurrentIndex(0)

    def use_mdi_keyboard_changed(self, state):
        if state:
            self.w.widget_mdi_controls.hide()
        else:
            self.w.widget_mdi_controls.show()
        self.w.mdihistory.set_soft_keyboard(state)

    def chk_eoffsets_checked(self, state):
        if state:
            self.w.lineEdit_eoffset_count.text()

    # show ngcgui info tab (in the stackedWidget) if ngcgui utilites
    # tab is selected
    def tab_utilities_changed(self, num):
        if num == 2:
            self.w.stackedWidget_2.setCurrentIndex(7)
        else:
            self.w.stackedWidget_2.setCurrentIndex(4)

    def apply_stylesheet_clicked(self, index):
        if self.w.cmb_stylesheet.currentText() == "As Loaded": return
        self.styleeditor.styleSheetCombo.setCurrentIndex(index)
        self.styleeditor.on_applyButton_clicked()

    def btn_gripper_clicked(self):
        AUX_PRGM.load_gcode_ripper()

    #####################
    # GENERAL FUNCTIONS #
    #####################
    def mode_changed(self,data):
        self._block_signal = True
        self.w.pushbutton_metric.setChecked(data)
        # if using state labels option update the labels
        if self.w.pushbutton_metric._state_text:
           self.w.pushbutton_metric.setText(None)
        self._block_signal = False

    def change_mode(self, data):
        if self._block_signal: return
        if data:
            ACTION.CALL_MDI('G21')
        else:
            ACTION.CALL_MDI('G20')

    def load_code(self, fname):
        if fname is None: return
        filename, file_extension = os.path.splitext(fname)

        # loading ngc then HTML/PDF

        if not file_extension in (".html", '.pdf'):
            if not (INFO.program_extension_valid(fname)):
                self.add_status("Unknown or invalid filename extension {}".format(file_extension), CRITICAL)
                return
            self.w.cmb_gcode_history.addItem(fname)
            self.w.cmb_gcode_history.setCurrentIndex(self.w.cmb_gcode_history.count() - 1)
            self.w.cmb_gcode_history.setToolTip(fname)
            ACTION.OPEN_PROGRAM(fname)
            self.add_status("Loaded program file : {}".format(fname))
            self.w.main_tab_widget.setCurrentIndex(TAB_MAIN)
            self.w.stackedWidget_dro.setCurrentIndex(0)
            self.w.stackedWidget_2.setCurrentIndex(0)
            self.w.btn_main.setChecked(True)
            self.w.filemanager.recordBookKeeping()

            # adjust ending to check for related HTML setup files
            fname = filename+'.html'
            if os.path.exists(fname):
                self.w.web_view.load(QtCore.QUrl.fromLocalFile(fname))
                self.add_status("Loaded HTML file : {}".format(fname), CRITICAL)
            else:
                self.w.web_view.setHtml(self.html)

            # look for PDF setup files
            # load it with system program
            fname = filename+'.pdf'
            if os.path.exists(fname):
                self.PDFView.loadView(fname)
                self.add_status("Loaded PDF file : {}".format(fname))
            else:
                self.PDFView.loadSample('setup_tab')
            return

        # loading HTML/PDF directly

        if file_extension == ".html":
            try:
                self.w.web_view.load(QtCore.QUrl.fromLocalFile(fname))
                self.add_status("Loaded HTML file : {}".format(fname))
                self.w.main_tab_widget.setCurrentIndex(TAB_SETUP)
                self.w.stackedWidget_2.setCurrentIndex(3)
                self.w.btn_setup.setChecked(True)
                self.w.tabWidget_setup.setCurrentIndex(0)
                                
            except Exception as e:
                print("Error loading HTML file : {}".format(e))
        else:
            if os.path.exists(fname):
                self.PDFView.loadView(fname)
                self.add_status("Loaded PDF file : {}".format(fname))
                self.w.main_tab_widget.setCurrentIndex(TAB_SETUP)
                self.w.stackedWidget_2.setCurrentIndex(3)
                self.w.btn_setup.setChecked(True)
                self.w.tabWidget_setup.setCurrentIndex(1)

    def update_gcode_properties(self, props ):
        # substitute nice looking text:
        property_names = {
            'name': "Name:", 'size': "Size:",
    '       tools': "Tool order:", 'g0': "Rapid distance:",
            'g1': "Feed distance:", 'g': "Total distance:",
            'run': "Run time:",'machine_unit_sys':"Machine Unit System:",
            'x': "X bounds:",'x_zero_rxy':'X @ Zero Rotation:',
            'y': "Y bounds:",'y_zero_rxy':'Y @ Zero Rotation:',
            'z': "Z bounds:",'z_zero_rxy':'Z @ Zero Rotation:',
            'a': "A bounds:", 'b': "B bounds:",
            'c': "C bounds:",'toollist':'Tool Change List:',
            'gcode_units':"Gcode Units:"
        }

        smallmess = mess = ''
        if props:
            for i in props:
                smallmess += '<b>%s</b>: %s<br>' % (property_names.get(i), props[i])
                mess += '<span style=" font-size:15pt; font-weight:600; color:black;">%s </span>\
<span style=" font-size:15pt; font-weight:600; color:#aa0000;">%s</span>\
<br>'% (property_names.get(i), props[i])

        # put the details into the properties page
        self.w.textedit_properties.setText(mess)
        return
        # pop a dialog of the properties
        msg = QtWidgets.QMessageBox()
        msg.setIcon(QtWidgets.QMessageBox.Information)
        msg.setText(smallmess)
        msg.setWindowTitle("Gcode Properties")
        msg.setStandardButtons(QtWidgets.QMessageBox.Ok)
        msg.show()
        retval = msg.exec_()                
                    
    def touchoff(self, selector):
        if selector == 'touchplate':
            z_offset = float(self.w.lineEdit_touch_height.text())
        elif selector == 'sensor':
            z_offset = float(self.w.lineEdit_sensor_height.text()) - float(self.w.lineEdit_work_height.text())
        else:
            self.add_status("Unknown touchoff routine specified", CRITICAL)
            return
        self.add_status("Touchoff to {} started".format(selector))
        max_probe = self.w.lineEdit_max_probe.text()
        search_vel = self.w.lineEdit_search_vel.text()
        probe_vel = self.w.lineEdit_probe_vel.text()
        rtn = ACTION.TOUCHPLATE_TOUCHOFF(search_vel, probe_vel, max_probe, z_offset)
        if rtn == 0:
            self.add_status("Touchoff routine is already running", CRITICAL)

    def kb_jog(self, state, joint, direction, fast = False, linear = True):
        if not STATUS.is_man_mode() or not STATUS.machine_is_on():
            self.add_status('Machine must be ON and in Manual mode to jog', CRITICAL)
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

    def add_status(self, message, alertLevel = DEFAULT):
        if alertLevel==DEFAULT:
            self.set_style_default()
        elif alertLevel==WARNING:
            self.set_style_warning()
        else:
            self.set_style_critical()
        self.w.lineEdit_statusbar.setText(message)
        STATUS.emit('update-machine-log', message, 'TIME')

    def enable_auto(self, state):
        for widget in self.auto_list:
            self.w[widget].setEnabled(state)
        if state is True:
            if self.w.main_tab_widget.currentIndex() != TAB_SETUP:               
                self.w.stackedWidget_2.show()
        else:
            if self.w.main_tab_widget.currentIndex() != TAB_PROBE:
                self.w.stackedWidget_2.setCurrentIndex(8)
                self.w.btn_main.setChecked(True)
                self.w.main_tab_widget.setCurrentIndex(TAB_MAIN)
                self.w.stackedWidget_2.setCurrentIndex(3)
                self.w.stackedWidget_dro.setCurrentIndex(0)

    def enable_onoff(self, state):
        if state:
            self.add_status("Machine ON")
        else:
            self.add_status("Machine OFF")
        self.w.btn_pause_spindle.setChecked(False)
        self.h['eoffset-count'] = 0
        for widget in self.onoff_list:
            self.w[widget].setEnabled(state)

    def set_start_line(self, line):
        if self.w.chk_run_from_line.isChecked():
            self.start_line = line
            self.w.btn_start.setText("START\n{}".format(self.start_line))
        else:
            self.start_line = 1

    def use_keyboard(self):
        if self.w.chk_use_keyboard.isChecked():
            return True
        else:
            self.add_status('Keyboard shortcuts are disabled')
            return False

    def do_file_copy(self):
        try:
            copyfile(self.source_file, self.destination_file)
            self.add_status("Copied file from {} to {}".format(self.source_file, self.destination_file))
        except Exception as e:
            self.add_status("Unable to copy file. %s" %e)

    def update_rpm(self, speed):
        if self.max_spindle_rpm < int(speed) < self.min_spindle_rpm:
            if STATUS.is_spindle_on():
                self.w.lbl_spindle_set.setProperty('in_range', False)
                self.w.lbl_spindle_set.style().unpolish(self.w.lbl_spindle_set)
                self.w.lbl_spindle_set.style().polish(self.w.lbl_spindle_set)
        else:
            self.w.lbl_spindle_set.setProperty('in_range', True)
            self.w.lbl_spindle_set.style().unpolish(self.w.lbl_spindle_set)
            self.w.lbl_spindle_set.style().polish(self.w.lbl_spindle_set)

    def update_runtimer(self):
        if self.timer_on is False or STATUS.is_auto_paused(): return
        self.time_tenths += 1
        if self.time_tenths == 10:
            self.time_tenths = 0
            self.run_time += 1
            hours, remainder = divmod(self.run_time, 3600)
            minutes, seconds = divmod(remainder, 60)
            self.w.lbl_runtime.setText("{:02d}:{:02d}:{:02d}".format(hours, minutes, seconds))

    def stop_timer(self):
        self.timer_on = False
        if STATUS.is_auto_mode():
            self.add_status("Run timer stopped at {}".format(self.w.lbl_runtime.text()))

    def back(self):
        if os.path.exists(self.default_setup):
            self.w.web_view.load(QtCore.QUrl.fromLocalFile(self.default_setup))
        else:
            self.w.web_view.setHtml(self.html)
        #self.w.web_view.page().triggerAction(QWebEnginePage.Back)

    def forward(self):
        self.w.web_view.load(QtCore.QUrl.fromLocalFile(self.docs))
        #self.w.web_view.page().triggerAction(QWebEnginePage.Forward)

    def writer(self):
        WRITER.show()

    # change Status bar text color
    def set_style_default(self):
        self.w.lineEdit_statusbar.setStyleSheet("background-color: rgb(252, 252, 252);color: rgb(0,0,0)")  #default white
    def set_style_warning(self):
        self.w.lineEdit_statusbar.setStyleSheet("background-color: rgb(242, 246, 103);color: rgb(0,0,0)")  #yelow
    def set_style_critical(self):
        self.w.lineEdit_statusbar.setStyleSheet("background-color: rgb(255, 144, 0);color: rgb(0,0,0)")   #orange
        
    def external_mpg(self, count):
        diff = count - self._last_count
        if self.w.pushbutton_fo.isChecked():
            scaled = (STATUS.stat.feedrate * 100 + diff)
            if scaled <0 :scaled = 0
            elif scaled > INFO.MAX_FEED_OVERRIDE:scaled = INFO.MAX_FEED_OVERRIDE
            ACTION.SET_FEED_RATE(scaled)
        elif self.w.pushbutton_ro.isChecked():
            scaled = (STATUS.stat.rapidrate * 100 + diff)
            if scaled <0 :scaled = 0
            elif scaled > 100:scaled = 100
            ACTION.SET_RAPID_RATE(scaled)
        elif self.w.pushbutton_so.isChecked():
            scaled = (STATUS.stat.spindle[0]['override'] * 100 + diff)
            if scaled < INFO.MIN_SPINDLE_OVERRIDE:scaled = INFO.MIN_SPINDLE_OVERRIDE
            elif scaled > INFO.MAX_SPINDLE_OVERRIDE:scaled = INFO.MAX_SPINDLE_OVERRIDE
            ACTION.SET_SPINDLE_RATE(scaled)
        self._last_count = count

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

    def on_keycall_pause(self,event,state,shift,cntrl):
        if state and STATUS.is_auto_mode() and self.use_keyboard():
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
    
    def on_keycall_APOS(self,event,state,shift,cntrl):
        if self.use_keyboard():
            self.kb_jog(state, 3, 1, shift, False)

    def on_keycall_ANEG(self,event,state,shift,cntrl):
        if self.use_keyboard():
            self.kb_jog(state, 3, -1, shift, False)

    def on_keycall_BPOS(self,event,state,shift,cntrl):
        if self.use_keyboard():
            self.kb_jog(state, 4, 1, shift, False)

    def on_keycall_BNEG(self,event,state,shift,cntrl):
        if self.use_keyboard():
            self.kb_jog(state, 4, -1, shift, False)

    def on_keycall_F12(self,event,state,shift,cntrl):
        if state:
            STYLEEDITOR.load_dialog()

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
