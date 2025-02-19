import os, time
from PyQt5 import QtCore, QtWidgets, QtGui
from qtvcp.widgets.gcode_editor import GcodeEditor as GCODE
from qtvcp.widgets.gcode_graphics import GCodeGraphics as GRAPHICS
from qtvcp.widgets.mdi_line import MDILine as MDI_WIDGET
from qtvcp.widgets.tool_offsetview import ToolOffsetView as TOOL_TABLE
from qtvcp.widgets.origin_offsetview import OriginOffsetView as OFFSET_VIEW
from qtvcp.widgets.stylesheeteditor import StyleSheetEditor as SSE
from qtvcp.widgets.file_manager import FileManager as FM
from qtvcp.widgets.status_slider import StatusSlider as SLIDER
from qtvcp.lib.qt_ngcgui.ngcgui import NgcGui
from qtvcp.lib.writer import writer
from qtvcp.lib.keybindings import Keylookup
from qtvcp.lib.gcodes import GCodes
from qtvcp.lib.qt_pdf import PDFViewer
from qtvcp.core import Status, Action, Info, Path, Qhal
from qtvcp import logger
from shutil import copyfile

LOG = logger.getLogger(__name__)
KEYBIND = Keylookup()
STATUS = Status()
INFO = Info()
ACTION = Action()
PATH = Path()
STYLEEDITOR = SSE()
WRITER = writer.Main()
QHAL = Qhal()

try:
    from PyQt5.QtWebEngineWidgets import QWebEnginePage
except:
    LOG.warning('QtDragon Warning with loading QtWebEngineWidget - is python3-pyqt5.qtwebengine installed?')

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
TAB_USER = 11

# constants for (left side) stacked widget
PAGE_UNCHANGED = -1
PAGE_GCODE = 0
PAGE_FILE = 1
PAGE_OFFSET = 2
PAGE_TOOL = 3
PAGE_NGCGUI = 4

DEFAULT = 0
WARNING = 1
CRITICAL = 2

VERSION ='1.4'

class HandlerClass:
    def __init__(self, halcomp, widgets, paths):
        self.h = halcomp
        self.w = widgets
        self.gcodes = GCodes(widgets)
        # This validator precludes using comma as a decimal
        self.valid = QtGui.QRegExpValidator(QtCore.QRegExp('-?[0-9]{0,6}[.][0-9]{0,3}'))
        self.KEYBIND = KEYBIND
        KEYBIND.add_call('Key_F11','on_keycall_F11')
        KEYBIND.add_call('Key_F12','on_keycall_F12')
        KEYBIND.add_call('Key_Pause', 'on_keycall_pause')
        KEYBIND.add_call('Key_Any', 'on_keycall_pause')

        KEYBIND.add_call('Key_Period','on_keycall_jograte',1)
        KEYBIND.add_call('Key_Comma','on_keycall_jograte',0)
        KEYBIND.add_call('Key_Greater','on_keycall_angular_jograte',1)
        KEYBIND.add_call('Key_Less','on_keycall_angular_jograte',0)

        # some global variables
        self.factor = 1.0
        self._spindle_wait = False
        self.probe = None
        self.default_setup = os.path.join(PATH.CONFIGPATH, "default_setup.html")
        self.docs = os.path.join(PATH.SCREENDIR, PATH.BASEPATH,'docs/getting_started.html')
        self.start_line = 0
        self.run_time = 0
        self.time_tenths = 0
        self._last_count = 0
        self.timer_on = False
        self.home_all = False
        self.min_spindle_rpm = INFO.MIN_SPINDLE_SPEED
        self.max_spindle_rpm = INFO.MAX_SPINDLE_SPEED
        self.spindle_lift_pins_present = False
        self.system_list = ["G54","G55","G56","G57","G58","G59","G59.1","G59.2","G59.3"]
        self.slow_jog_factor = 10
        self.reload_tool = 0
        self.last_loaded_program = ""
        self.first_turnon = True
        self._maintab_cycle = 1
        self._lastSelectButton = None
        self.MPGFocusWidget = None
        self.CycleFocusWidget = None
        self.lineedit_list = ["work_height", "touch_height", "sensor_height", "laser_x", "laser_y",
                              "sensor_x", "sensor_y", "camera_x", "camera_y",
                              "search_vel", "probe_vel", "max_probe", "eoffset_count"]
        self.onoff_list = ["frame_program", "frame_tool", "frame_dro", "frame_override", "frame_status"]
        self.auto_list = ["chk_eoffsets", "cmb_gcode_history","lineEdit_eoffset_count"]
        self.axis_4_list = ["axis_select_4", "dro_axis_4", "action_zero_4",
                            "dro_button_stack_4", "plus_jogbutton_4", "minus_jogbutton_4"]
        self.axis_5_list = ["axis_select_5", "dro_axis_5", "action_zero_5",
                            "dro_button_stack_5",
                            "plus_jogbutton_5", "minus_jogbutton_5"]
        self.button_response_list = ["btn_start", "btn_home_all", "btn_home_x", "btn_home_y",
                            "btn_home_z", "action_home_4","action_home_5", "btn_reload_file", "macrobutton0", "macrobutton1",
                            "macrobutton2", "macrobutton3", "macrobutton4", "macrobutton5", "macrobutton6",
                            "macrobutton7", "macrobutton8", "macrobutton9"]
        self.statusbar_reset_time = 10000 # ten seconds

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
        STATUS.connect('program-pause-changed', lambda w, state: self.update_pause_button(state))
        STATUS.connect('actual-spindle-speed-changed', lambda w, speed: self.update_rpm(speed))
        STATUS.connect('user-system-changed', lambda w, data: self.user_system_changed(data))
        STATUS.connect('metric-mode-changed', lambda w, mode: self.metric_mode_changed(mode))
        STATUS.connect('file-loaded', self.file_loaded)
        STATUS.connect('all-homed', self.all_homed)
        STATUS.connect('not-all-homed', self.not_all_homed)
        STATUS.connect('periodic', lambda w: self.periodic_update())
        STATUS.connect('command-stopped', lambda w: self.stop_timer())
        STATUS.connect('progress', lambda w,p,t: self.updateProgress(p,t))
        STATUS.connect('override-limits-changed', lambda w, state, data: self._check_override_limits(state, data))
        STATUS.connect('graphics-gcode-properties', lambda w, d: self.update_gcode_properties(d))

        self.html = """<html>
<head>
<title>Test page for the download:// scheme</title>
</head>
<body>
<h1>Setup Tab</h1>
<p>If you select a file with .html as a file ending, it will be shown here..</p>
<li><a href="http://linuxcnc.org/docs/2.9/html/">Documents online</a></li>
<li><a href="http://linuxcnc.org/docs/2.9/html/gui/qtdragon.html">QtDragon online</a></li>
<li><a href="file://%s">Local files</a></li>
<img src="file://%s" alt="lcnc_swoop" />
<hr />
</body>
</html>
""" %(  os.path.expanduser('~/linuxcnc'),
        os.path.join(paths.IMAGEDIR,'lcnc_swoop.png'))

    def class_patch__(self):
        # override file manager load button
        self.old_fman = FM.load
        FM.load = self.load_code

        # override NGCGui path check
        NgcGui.check_linuxcnc_paths_fail = self.check_linuxcnc_paths_fail_override

    def initialized__(self):
        self.init_pins()
        self.init_preferences()
        self.init_widgets()
        self.init_probe()
        self.init_utils()
        self.w.stackedWidget_log.setCurrentIndex(0)
        self.w.stackedWidget.setCurrentIndex(0)
        self.w.stackedWidget_dro.setCurrentIndex(0)
        self.w.btn_spindle_pause.setEnabled(False)
        self.w.btn_dimensions.setChecked(True)
        self.w.page_buttonGroup.buttonClicked.connect(self.main_tab_changed)
        self.w.selectButtonGroup.buttonClicked.connect(self.MPG_select_changed)
        self.w.filemanager_usb.showMediaDir(quiet = True)

    # hide or initiate 4th/5th AXIS dro/jog
        flag = False
        flag4 = True
        num = 4
        for temp in ('A','B','C','U','V','W'):
            if temp in INFO.AVAILABLE_AXES:
                if temp in ('A','B','C'):
                    flag = True
                self.initiate_axis_dro(num,temp)
                num +=1
                if num ==6:
                    break
        # no 5th axis
        if num < 6:
            for i in self.axis_5_list:
                self.w[i].hide()
        # no 4th axis
        if num < 5 :
            for i in self.axis_4_list:
                self.w[i].hide()
        # angular increment controls
        if flag:
           self.w.lbl_increments_linear.setText("INCREMENTS")
        else:
            self.w.widget_jog_angular.hide()
            self.w.widget_increments_angular.hide()

    # set validators for lineEdit widgets
        for val in self.lineedit_list:
            self.w['lineEdit_' + val].setValidator(self.valid)
        self.w.lineEdit_eoffset_count.setValidator(QtGui.QIntValidator(0,100))
    # check for default setup html file
        try:
            # web view widget for SETUP page
            if self.w.webwidget:
                self.toolBar = QtWidgets.QToolBar(self.w)
                self.w.tabWidget_setup.setCornerWidget(self.toolBar)

                self.zoomBtn = QtWidgets.QPushButton(self.w)
                self.zoomBtn.setEnabled(True)
                self.zoomBtn.setMinimumSize(64, 40)
                self.zoomBtn.setIconSize(QtCore.QSize(38, 38))
                self.zoomBtn.setIcon(QtGui.QIcon(QtGui.QPixmap(':/buttons/images/zoom.png')))
                self.zoomBtn.clicked.connect(self.zoomWeb)
                self.toolBar.addWidget(self.zoomBtn)

                self.homeBtn = QtWidgets.QPushButton(self.w)
                self.homeBtn.setEnabled(True)
                self.homeBtn.setMinimumSize(64, 40)
                self.homeBtn.setIconSize(QtCore.QSize(38, 38))
                self.homeBtn.setIcon(QtGui.QIcon(':/qt-project.org/styles/commonstyle/images/up-32.png'))
                self.homeBtn.clicked.connect(self.homeWeb)
                self.toolBar.addWidget(self.homeBtn)

                self.backBtn = QtWidgets.QPushButton(self.w)
                self.backBtn.setEnabled(True)
                self.backBtn.setMinimumSize(64, 40)
                self.backBtn.setIconSize(QtCore.QSize(38, 38))
                self.backBtn.setIcon(QtGui.QIcon(':/qt-project.org/styles/commonstyle/images/left-32.png'))
                self.backBtn.clicked.connect(self.back)
                self.toolBar.addWidget(self.backBtn)

                self.forBtn = QtWidgets.QPushButton(self.w)
                self.forBtn.setEnabled(True)
                self.forBtn.setMinimumSize(64, 40)
                self.forBtn.setIconSize(QtCore.QSize(38, 38))
                self.forBtn.setIcon(QtGui.QIcon(':/qt-project.org/styles/commonstyle/images/right-32.png'))
                self.forBtn.clicked.connect(self.forward)
                self.toolBar.addWidget(self.forBtn)

                self.writeBtn = QtWidgets.QPushButton('SetUp\n Writer',self.w)
                self.writeBtn.setMinimumSize(64, 40)
                self.writeBtn.setEnabled(True)
                self.writeBtn.clicked.connect(self.writer)
                self.toolBar.addWidget(self.writeBtn)

                if os.path.exists(self.default_setup):
                    self.w.webwidget.load(QtCore.QUrl.fromLocalFile(self.default_setup))
                else:
                    self.w.webwidget.setHtml(self.html)
                self.w.webwidget.page().urlChanged.connect(self.onLoadFinished)

        except Exception as e:
            print("No default setup file found - {}".format(e))

        # PDF setup page
        self.PDFView = PDFViewer.PDFView()
        self.w.layout_PDF.addWidget(self.PDFView)
        self.PDFView.loadSample('setup_tab')

        # Show assigned macrobuttons define in INI under [MDI_COMMAND_LIST]
        flag = True
        for b in range(0,10):
            button = self.w['macrobutton{}'.format(b)]
            # prefer named INI MDI commands
            key = button.property('ini_mdi_key')
            if key == '' or INFO.get_ini_mdi_command(key) is None:
                # fallback to legacy nth line
                key = button.property('ini_mdi_number')
            try:
                code = INFO.get_ini_mdi_command(key)
                if code is None: raise Exception
                flag = False
            except:
                button.hide()
        # no buttons hide frame
        if flag:
            self.w.frame_macro_buttons.hide()

        message = "--- QtDragon Version {} on Linuxcnc {} ---".format(
            VERSION, STATUS.get_linuxcnc_version())
        STATUS.emit('update-machine-log', message, None)

    def init_utils(self):
        from qtvcp.lib.gcode_utility.facing import Facing
        self.facing = Facing()
        self.w.layout_facing.addWidget(self.facing)

        try:
            from qtvcp.lib.gcode_utility.hole_enlarge import Hole_Enlarge
            self.hole_enlarge = Hole_Enlarge()
            ACTION.ADD_WIDGET_TO_TAB(self.w.tabWidget_utilities,self.hole_enlarge, 'Hole Enlarge')
        except Exception as e:
            LOG.info("Utility hole enlarge unavailable: {}".format(e))

        from qtvcp.lib.gcode_utility.hole_circle import Hole_Circle
        self.hole_circle = Hole_Circle()
        self.w.layout_hole_circle.addWidget(self.hole_circle)

        # load the NgcGui widget into the utilities tab
        # then move (warp) the info tab from it to the left tab widget
        self.ngcgui = NgcGui()
        self.w.layout_ngcgui.addWidget(self.ngcgui)
        self.ngcgui.warp_info_frame(self.w.ngcGuiLeftLayout)

    #############################
    # SPECIAL FUNCTIONS SECTION #
    #############################
    def init_pins(self):
        # spindle control pins
        pin = QHAL.newpin("spindle-amps", QHAL.HAL_FLOAT, QHAL.HAL_IN)
        pin.value_changed.connect(self.spindle_pwr_changed)

        pin = QHAL.newpin("spindle-volts", QHAL.HAL_FLOAT, QHAL.HAL_IN)
        pin.value_changed.connect(self.spindle_pwr_changed)

        pin = QHAL.newpin("spindle-fault-u32", QHAL.HAL_U32, QHAL.HAL_IN)
        pin.value_changed.connect(self.spindle_fault_changed)
        pin = QHAL.newpin("spindle-fault", QHAL.HAL_S32, QHAL.HAL_IN)
        pin.value_changed.connect(self.spindle_fault_changed)

        pin = QHAL.newpin("spindle-modbus-errors-u32", QHAL.HAL_U32, QHAL.HAL_IN)
        pin.value_changed.connect(self.mb_errors_changed)
        pin = QHAL.newpin("spindle-modbus-errors", QHAL.HAL_S32, QHAL.HAL_IN)
        pin.value_changed.connect(self.mb_errors_changed)

        pin = QHAL.newpin("spindle-modbus-connection", QHAL.HAL_BIT, QHAL.HAL_IN)
        pin.value_changed.connect(self.mb_connection_changed)

        QHAL.newpin("spindle-inhibit", QHAL.HAL_BIT, QHAL.HAL_OUT)

        pin = QHAL.newpin("external-pause", QHAL.HAL_BIT, QHAL.HAL_IN)
        pin.value_changed.connect(self.btn_pause_clicked)

        # external offset control pins
        QHAL.newpin("eoffset-enable", QHAL.HAL_BIT, QHAL.HAL_OUT)
        QHAL.newpin("eoffset-clear", QHAL.HAL_BIT, QHAL.HAL_OUT)
        self.h['eoffset-clear'] = True
        QHAL.newpin("eoffset-spindle-count", QHAL.HAL_S32, QHAL.HAL_OUT)
        pin = QHAL.newpin("eoffset-is-active", QHAL.HAL_BIT, QHAL.HAL_IN)
        pin.value_changed.connect(self.external_offset_state_changed)

        # total external offset
        pin = QHAL.newpin("eoffset-value", QHAL.HAL_FLOAT, QHAL.HAL_IN)

        self.pin_mpg_in = QHAL.newpin('mpg-in',QHAL.HAL_S32, QHAL.HAL_IN)
        self.pin_mpg_in.value_changed.connect(lambda s: self.external_mpg(s))

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
        self.w.lineEdit_max_probe.setText(str(self.w.PREFS_.getpref('Max Probe', 10, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_retract_distance.setText(str(self.w.PREFS_.getpref('Retract Distance', 10, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_z_safe_travel.setText(str(self.w.PREFS_.getpref('Z Safe Travel', 10, float, 'CUSTOM_FORM_ENTRIES')))
        self.w.lineEdit_eoffset_count.setText(str(self.w.PREFS_.getpref('Eoffset count', 0, int, 'CUSTOM_FORM_ENTRIES')))
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
        self.cam_xscale_changed(self.w.PREFS_.getpref('Camview xscale', 100, int, 'CUSTOM_FORM_ENTRIES'))
        self.cam_yscale_changed(self.w.PREFS_.getpref('Camview yscale', 100, int, 'CUSTOM_FORM_ENTRIES'))
        self.w.camview._camNum = self.w.PREFS_.getpref('Camview cam number', 0, int, 'CUSTOM_FORM_ENTRIES')

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
        self.w.PREFS_.putpref('Retract Distance', self.w.lineEdit_retract_distance.text().encode('utf-8'), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Z Safe Travel', self.w.lineEdit_z_safe_travel.text().encode('utf-8'), float, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Eoffset count', self.w.lineEdit_eoffset_count.text().encode('utf-8'), int, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('External offsets', self.w.chk_eoffsets.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Reload program', self.w.chk_reload_program.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Reload tool', self.w.chk_reload_tool.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Use keyboard', self.w.chk_use_keyboard.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Run from line', self.w.chk_run_from_line.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Use virtual keyboard', self.w.chk_use_virtual.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Use tool sensor', self.w.chk_use_tool_sensor.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Use camera', self.w.chk_use_camera.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Use alpha display mode', self.w.chk_alpha_mode.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Inhibit display mouse selection', self.w.chk_inhibit_selection.isChecked(), bool, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Camview xscale', self.cam_xscale_percent(), int, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Camview yscale', self.cam_yscale_percent(), int, 'CUSTOM_FORM_ENTRIES')
        self.w.PREFS_.putpref('Camview cam number', self.w.camview._camNum, int, 'CUSTOM_FORM_ENTRIES')

    def init_widgets(self):
        self.adjust_stacked_widgets(TAB_MAIN)
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
            self.w.lbl_retract_dist_units.setText('INCH')
            self.w.lbl_z_safe_travel_units.setText('INCH')

        #set up gcode list
        self.gcodes.setup_list()

        # hide user tab button if no user tabs
        if self.w.stackedWidget_mainTab.count() == 11:
            self.w.btn_user.hide()

    def init_probe(self):
        probe = INFO.get_error_safe_setting('PROBE', 'USE_PROBE', 'none').lower()
        if probe == 'versaprobe':
            LOG.info("Using Versa Probe")
            from qtvcp.widgets.versa_probe import VersaProbe
            self._probeLibrary = VersaProbe
            self.probe = VersaProbe()
            self.probe.setObjectName('versaprobe')
            # only use cycle start button to start probing
            self.probe.setProperty('runImmediately',False)
            self.probe.setFocusPolicy(QtCore.Qt.ClickFocus)

        elif probe == 'basicprobe':
            LOG.info("Using Basic Probe")
            from qtvcp.widgets.basic_probe import BasicProbe
            self._probeLibrary = BasicProbe
            self.probe = BasicProbe()
            self.probe.setObjectName('basicprobe')
            # only use cycle start button to start probing
            self.probe.setProperty('runImmediately',False)
            self.probe.setFocusPolicy(QtCore.Qt.ClickFocus)

        else:
            LOG.info("No valid probe widget specified")
            self.w.btn_probe.hide()
            return
        self.w.probe_layout.addWidget(self.probe)
        self.probe.hal_init()

    def processed_focus_event__(self, receiver, event):
        #print(receiver.parent(), receiver)

        if isinstance(receiver.parent(), GCODE):
            print ('Gcode editor focus',receiver.parent().parent().objectName())
            self.removeMPGFocusBorder()

            name = receiver.parent().parent().objectName()
            color = self.w.screen_options.property('user4Color').name()
            self.colorMPGFocusBorder(name, receiver.parent(), color)

        elif isinstance(receiver, GRAPHICS):
            print ('Gcode graphics focus',receiver,receiver.parent().objectName())
            self.removeMPGFocusBorder()

            name = receiver.parent().objectName()
            color = self.w.screen_options.property('user4Color').name()
            self.colorMPGFocusBorder(name, receiver, color)

        elif isinstance(receiver.parent(), FM):
            self.removeMPGFocusBorder()

            print ('File Manager focus',receiver.parent().parent().objectName())
            name = receiver.parent().parent().objectName()
            color = self.w.screen_options.property('user4Color').name()
            self.colorMPGFocusBorder(name, receiver.parent(), color)

        elif isinstance(receiver, SLIDER):
            print('Slider',receiver.objectName(),receiver.parent())
            if not receiver in(self.w.slider_jog_linear,self.w.slider_jog_angular,self.w.slider_maxv_ovr):
                self.removeMPGFocusBorder()
                name = receiver.objectName()
                color = self.w.screen_options.property('user4Color').name()
                self.colorMPGFocusBorder(name, receiver, color)

        elif isinstance(receiver, TOOL_TABLE):
            self.removeMPGFocusBorder()

            print ('Tool Offset focus',receiver.parent().objectName())
            name = receiver.parent().objectName()
            color = self.w.screen_options.property('user4Color').name()
            self.colorMPGFocusBorder(name, receiver, color)

        elif isinstance(receiver, OFFSET_VIEW):
            self.removeMPGFocusBorder()

            print ('origon Offset focus',receiver.parent().objectName())
            name = receiver.parent().objectName()
            color = self.w.screen_options.property('user4Color').name()
            self.colorMPGFocusBorder(name, receiver, color)

        elif isinstance(receiver, MDI_WIDGET):
            self.removeMPGFocusBorder()
            self.removeCycleFocusBorder()

            print ('MDI line focus',receiver.parent().objectName())
            name = receiver.parent().objectName()
            color = self.w.screen_options.property('user5Color').name()
            self.colorCycleFocusBorder(name, receiver, color)

        elif isinstance(receiver, self._probeLibrary):
            self.removeCycleFocusBorder()

            print ('Versa/Basic Probe focus',receiver.parent().objectName())
            name = receiver.parent().objectName()
            color = self.w.screen_options.property('user5Color').name()
            self.colorCycleFocusBorder(name, receiver, color)

        # show virtual keyboard
        if not self.w.chk_use_virtual.isChecked() or STATUS.is_auto_mode(): return
        if isinstance(receiver, QtWidgets.QLineEdit):
            if not receiver.isReadOnly():
                self.w.stackedWidget_dro.setCurrentIndex(1)
        elif isinstance(receiver, QtWidgets.QTableView):
            self.w.stackedWidget_dro.setCurrentIndex(1)

    def removeMPGFocusBorder(self):
        try:
            self.MPGFocusWidget.setStyleSheet( '')
            name = self.MPGFocusWidgetBorder
            self.w[name].setStyleSheet('')
        except:
            pass

    def removeCycleFocusBorder(self):
        try:
            self.CycleFocusWidget.setStyleSheet( '')
            name = self.CycleFocusWidgetBorder
            self.w[name].setStyleSheet('')
        except:
            pass

    def colorMPGFocusBorder(self, name, receiver, colorName):
        self.MPGFocusWidgetBorder = name
        self.MPGFocusWidget = receiver
        self.w[name].setStyleSheet('#%s {border: 3px solid %s;}'%(name,colorName))

    def colorCycleFocusBorder(self, name, receiver, colorName):
        self.CycleFocusWidgetBorder = name
        self.CycleFocusWidget = receiver
        self.w[name].setStyleSheet('#%s {border: 3px solid %s;}'%(name,colorName))

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

    def before_loop__(self):
        # no spindle lift without pins connected
        self.spindle_lift_pins_present = True
        for i in ('qtdragon.eoffset-is-active','qtdragon.spindle-inhibit','qtdragon.eoffset-clear',
                'qtdragon.eoffset-spindle-count','qtdragon.eoffset-value'):
            # no driving pin cennected?
            if not self.h.hal.pin_has_writer(i):
                self.spindle_lift_pins_present = False
                #self.add_status('{} pin not connected in a HAL FILE'.format(i))
                LOG.warning("{} not connected".format(i))

        # halui pins could cause resume without the spindle running
        # and would not honour spindle lift. Should use qtdragon pin provided
        for i in ('halui.program.resume','halui.program.pause'):
            if self.h.hal.pin_has_writer(i):
                self.spindle_lift_pins_present = False
                LOG.warning("HALUI pause/resume pin(s) connected - spindle lift not available")
                self.add_status('Warning: {} pin connected in a HAL file, spindle lift disabled'.format(i), WARNING)
                break

        # set defaults lower then max if wanted
        DEFAULT_RAPID_OVERRIDE = float(INFO.get_error_safe_setting("DISPLAY", "DEFAULT_RAPID_OVERRIDE", .5)) * 100
        ACTION.SET_RAPID_RATE(DEFAULT_RAPID_OVERRIDE)
        DEFAULT_VELOCITY_OVERRIDE = float(INFO.get_error_safe_setting("DISPLAY", "DEFAULT_MAX_VELOCITY", 1800))
        ACTION.SET_MAX_VELOCITY_RATE(DEFAULT_VELOCITY_OVERRIDE)

    #########################
    # CALLBACKS FROM STATUS #
    #########################

    def spindle_pwr_changed(self, data):
        # this calculation assumes the voltage is line to neutral
        # and that the synchronous motor spindle has a power factor of 0.9
        power = self.h['spindle-volts'] * self.h['spindle-amps'] * 0.9 # Watts = V x I x PF
        amps = "{:1.1f}".format(self.h['spindle-amps'])
        pwr = "{:1.1f}".format(power)
        self.w.lbl_spindle_amps.setText(amps)
        self.w.lbl_spindle_power.setText(pwr)

    def spindle_fault_changed(self, data):
        fault = hex(data)
        self.w.lbl_spindle_fault.setText(fault)

    def mb_errors_changed(self, data):
        self.w.lbl_mb_errors.setText(str(data))

    def mb_connection_changed(self, data):
        if data:
            self.w.lbl_mb_errors.setStyleSheet('')
        else:
            self.w.lbl_mb_errors.setStyleSheet('background-color:{};'.format(
                        self.w.screen_options.property('user5Color').name()))

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
            self.lowerSpindle()
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
        self.w.systemtoolbutton.setText(sys)

    def metric_mode_changed(self, mode):
        rate = (float(self.w.slider_rapid_ovr.value()) / 100)
        if mode is False:
            self.w.lbl_jog_linear.setText('INCH/<sup>MIN</sup>')
            self.factor = INFO.convert_machine_to_imperial(INFO.MAX_TRAJ_VELOCITY)
        else:
            self.w.lbl_jog_linear.setText('MM/<sup>MIN</sup>')
            self.factor = INFO.convert_machine_to_metric(INFO.MAX_TRAJ_VELOCITY)
        self.w.lbl_max_rapid.setText("{:4.0f}".format(rate * self.factor))

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
        self.w.btn_home_all.setText("ALL\nHOMED")
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
        self.w.btn_home_all.setText("HOME\nALL")

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

    # main button bar
    def main_tab_changed(self, btn):
        index = btn.property("index")

        if index == TAB_USER:
            pass
        # if you select the tab showing, force the DRO to show
        elif index == self.w.stackedWidget_mainTab.currentIndex():
            self.w.stackedWidget_dro.setCurrentIndex(0)

            if index == TAB_MAIN and STATUS.is_auto_mode():
                self._maintab_cycle +=1
        if index is None: return

        # adjust the stack widgets depending on modes
        self.adjust_stacked_widgets(index)

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
           self.add_status("Machine must be all homed", CRITICAL)
           return

        if STATUS.is_man_mode():
            self.add_status("Must be in AUTO or MDI mode to run a program", WARNING, noLog=True)
            return

        if STATUS.is_mdi_mode():
            if isinstance(self.CycleFocusWidget, MDI_WIDGET):
                if self.CycleFocusWidget.isVisible():
                    self.add_status("Running MDI command: {}".format(str((self.CycleFocusWidget.text()).strip())))
                    self.w.mdiline.submit()
            elif isinstance(self.CycleFocusWidget, self._probeLibrary):
                if self.CycleFocusWidget.isVisible():
                    self.add_status("Running Probe routine command")
                    self.probe.cycle_start()
                else:
                    self.add_status("Probe routine cycle start focus error", CRITICAL, noLog=True)
            else:
                self.add_status("No cycle start object selected", WARNING, noLog=True)
            return

        # in auto mode
        if not  os.path.exists(self.last_loaded_program):
            self.add_status("No program to execute", WARNING, noLog=True)
            return
        if self.w.stackedWidget_mainTab.currentIndex() != 0:
            self.add_status("Switch view mode to MAIN", WARNING)
            return
        if STATUS.is_auto_running():
            self.add_status("Program is already running", WARNING)
            return
        if self.start_line <= 1:
            ACTION.RUN(self.start_line)
        else:
            # instantiate run from line preset dialog
            info = '<b>Running From Line: {} <\b>'.format(self.start_line)
            mess = {'NAME':'RUNFROMLINE', 'TITLE':'Preset Dialog', 'ID':'_RUNFROMLINE', 'MESSAGE':info, 'LINE':self.start_line}
            ACTION.CALL_DIALOG(mess)

        self.start_timer()
        self.add_status("Started program from line {}".format(self.start_line))

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

        if STATUS.is_metric_mode():
            x = INFO.convert_machine_to_metric(x)
            y = INFO.convert_machine_to_metric(y)
        else:
            x = INFO.convert_machine_to_imperial(x)
            y = INFO.convert_machine_to_imperial(y)

        ACTION.CALL_MDI("G90")
        ACTION.CALL_MDI_WAIT("G53 G0 Z0")
        command = "G53 G0 X{:3.4f} Y{:3.4f}".format(x, y)
        ACTION.CALL_MDI_WAIT(command, 10)
 
    def btn_ref_laser_clicked(self):
        x = float(self.w.lineEdit_laser_x.text())
        y = float(self.w.lineEdit_laser_y.text())

        if STATUS.is_metric_mode():
            x = INFO.convert_machine_to_metric(x)
            y = INFO.convert_machine_to_metric(y)
        else:
            x = INFO.convert_machine_to_imperial(x)
            y = INFO.convert_machine_to_imperial(y)

        self.add_status("Laser offsets set")
        command = "G10 L20 P0 X{:3.4f} Y{:3.4f}".format(x, y)
        ACTION.CALL_MDI(command)
    
    def btn_ref_camera_clicked(self):
        x = float(self.w.lineEdit_camera_x.text())
        y = float(self.w.lineEdit_camera_y.text())

        if STATUS.is_metric_mode():
            x = INFO.convert_machine_to_metric(x)
            y = INFO.convert_machine_to_metric(y)
        else:
            x = INFO.convert_machine_to_imperial(x)
            y = INFO.convert_machine_to_imperial(y)

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

    def btn_touchoff_clicked(self):
        if STATUS.get_current_tool() == 0:
            self.add_status("Cannot touchoff with no tool loaded", CRITICAL)
            return
        if not STATUS.is_all_homed():
            self.add_status("Must be homed to perform tool touchoff", CRITICAL)
            return
        # instantiate dialog box
        sensor = self.w.sender().property('sensor')
        unit = "mm" if INFO.MACHINE_IS_METRIC else "in"
        info = "Ensure tooltip is within {} {} of tool sensor and click OK".format(self.w.lineEdit_max_probe.text(),unit)
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
        
    # camview tab
    def cam_zoom_changed(self, value):
        self.w.camview.scale = float(value) / 10

    def cam_dia_changed(self, value):
        self.w.camview.diameter = value

    def cam_rot_changed(self, value):
        self.w.camview.rotation = float(value) / 10

    # scaling of the camera image for size aspect corrections
    # set from preference file
    def cam_xscale_changed(self, value):
        self.w.camview.scaleX  = float(value/100)
    def cam_xscale_percent(self):
        return self.w.camview.scaleX * 100
    def cam_yscale_changed(self, value):
        self.w.camview.scaleY  = float(value/100)
    def cam_yscale_percent(self):
        return self.w.camview.scaleY * 100

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
                self.add_status("Override limits cleared")

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

    # show ngcgui info tab (in the stackedWidget) if ngcgui utilities
    # tab is selected
    def tab_utilities_changed(self, num):
        if num == 2:
            self.w.stackedWidget.setCurrentIndex(PAGE_NGCGUI)
        else:
            self.w.stackedWidget.setCurrentIndex(PAGE_GCODE)

    def btn_about_clicked(self):
        self.add_status("QtDragon Version {} on Linuxcnc {} ".format(
            VERSION, STATUS.get_linuxcnc_version()), CRITICAL)
        info = ACTION.GET_ABOUT_INFO()
        self.w.aboutDialog_.showdialog()

    def btn_zoomin_clicked(self):
        self.w.gcode_viewer.editor.zoomIn()
    def btn_zoomout_clicked(self):
        self.w.gcode_viewer.editor.zoomOut()

    def btn_spindle_z_up_clicked(self):
        fval = int(self.w.lineEdit_eoffset_count.text())
        if INFO.MACHINE_IS_METRIC:
            fval += 5
        else:
            fval += 1
        self.w.lineEdit_eoffset_count.setText(str(fval))
        if self.h['eoffset-clear'] != True:
            self.h['eoffset-spindle-count'] = int(fval)

    def btn_spindle_z_down_clicked(self):
        fval = int(self.w.lineEdit_eoffset_count.text())
        if INFO.MACHINE_IS_METRIC:
            fval -= 5
        else:
            fval -= 1
        if fval <0: fval = 0
        self.w.lineEdit_eoffset_count.setText(str(fval))
        if self.h['eoffset-clear'] != True:
            self.h['eoffset-spindle-count'] = int(fval)

    def btn_pause_clicked(self, data):
        if self.w.action_pause._isSignalsBlocked(): return
        if data:
            ACTION.PAUSE_MACHINE()
            self.w.btn_spindle_pause.setEnabled(False)

            self.h['eoffset-spindle-count'] = 0
            self.h['spindle-inhibit'] = False

            if int(self.w.lineEdit_eoffset_count.text()) == 0: return
            if not self.w.btn_spindle_pause.isChecked(): return

            self.liftSpindle()

        # no lowering required - resume
        elif int(self.w.lineEdit_eoffset_count.text()) == 0  or \
                not self.w.btn_spindle_pause.isChecked() or \
                self.h['eoffset-clear']:
            ACTION.RESUME()
            # only enable the lift selection button, if pins are connected
            if self.spindle_lift_pins_present:
                self.w.btn_spindle_pause.setEnabled(True)

        # lowering required before resuming
        elif STATUS.is_auto_paused():
            self.h['spindle-inhibit'] = False
            self.add_status('Spindle re-started')
            # If spindle at speed is connected to a pin, use it lower spindle
            if self.h.hal.pin_has_writer('spindle.0.at-speed'):
                self._spindle_wait=True
                return
            else:
            # or wait for dialog to close before lowering spindle
            # instantiate warning box
                info = "Wait for spindle at speed signal before resuming"
                mess = {'NAME':'MESSAGE', 'ICON':'WARNING', 'ID':'_wait_resume_',
                         'MESSAGE':'CAUTION', 'MORE':info, 'TYPE':'OK'}
                ACTION.CALL_DIALOG(mess)


    # from HAL pin 'eoffset-is-active'
    # unpause machine if external offsets state is false 
    def external_offset_state_changed(self, data):
        print('eoffset state',data)
        # only if running a program and  only if machine in on
        if not STATUS.is_auto_running() or not STATUS.machine_is_on():
            return

        # if GUI setting is at zero lift - ignore
        if int(self.w.lineEdit_eoffset_count.text()) == 0:return

        # only if pin is false (external offsets are off)
        if not data:
            ACTION.RESUME()
            # only enable the lift selection button if pins are connected
            if self.spindle_lift_pins_present:
                self.w.btn_spindle_pause.setEnabled(True)
            # make step program button active again
            self.w.action_step.setEnabled(True)

    def liftSpindle(self):
        self.w.action_step.setEnabled(False)
        # set external offsets to lift spindle
        self.h['eoffset-clear'] = False
        self.h['eoffset-enable'] = self.w.chk_eoffsets.isChecked()
        fval = int(self.w.lineEdit_eoffset_count.text())
        self.h['eoffset-spindle-count'] = int(fval)
        self.h['spindle-inhibit'] = True
        self.add_status("Spindle stopped and raised {}".format(fval))

    def lowerSpindle(self):
        self.h['eoffset-spindle-count'] = 0
        self.h['eoffset-clear'] = True
        self.add_status('Spindle lowered')

    # from abort button
    def disable_spindle_pause(self,data):
        pass

    # make sure the pause button follows STATUS pause messages
    # as well as physical button pushes
    def update_pause_button(self,data):
        self.w.action_pause._blockSignals(True)
        self.w.action_pause.setChecked(data)
        # only enable the lift selection button if pins are connected
        if self.spindle_lift_pins_present:
            self.w.btn_spindle_pause.setEnabled(not data)
        self.w.action_pause._blockSignals(False)

    def btn_systemtool_toggled(self, state):
        if state:
            STATUS.emit('dro-reference-change-request', 1)

    def MPG_select_changed(self, button):
        print(button)
        # Auto exclusive doesn't allow unchecking all buttons
        # We force it here
        if button == self._lastSelectButton:
                button.group().setExclusive(False)
                button.setChecked(False)
                button.group().setExclusive(True)
                self._lastSelectButton = None
                return
        #self.set_statusbar('MPG output Selected: {}'.format(cmd.toolTip()),DEFAULT,noLog=True)
        self._lastSelectButton = button

    #####################
    # GENERAL FUNCTIONS #
    #####################

    # file manager widget overridden function
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
            self.adjust_stacked_widgets(TAB_MAIN)
            self.w.filemanager.recordBookKeeping()

            # adjust ending to check for related HTML setup files
            fname = filename+'.html'
            try:
                if os.path.exists(fname):
                    self.w.webwidget.loadFile(fname)
                    self.add_status("Loaded HTML file : {}".format(fname), CRITICAL)
                else:
                    self.w.webwidget.setHtml(self.html)
            except Exception as e:
                self.add_status("Error loading HTML file {} : {}".format(fname,e))
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
                self.w.webwidget.loadFile(fname)
                self.add_status("Loaded HTML file : {}".format(fname))
                self.w.stackedWidget_mainTab.setCurrentIndex(TAB_SETUP)
                self.w.stackedWidget.setCurrentIndex(0)
                self.w.tabWidget_setup.setCurrentIndex(1)
                self.w.btn_setup.setChecked(True)
                self.w.jogging_frame.hide()
            except Exception as e:
                self.add_status("Error loading HTML file {} : {}".format(fname,e))
        else:
            if os.path.exists(fname):
                self.PDFView.loadView(fname)
                self.add_status("Loaded PDF file : {}".format(fname))
                self.w.stackedWidget_mainTab.setCurrentIndex(TAB_SETUP)
                self.w.stackedWidget.setCurrentIndex(0)
                self.w.tabWidget_setup.setCurrentIndex(1)
                self.w.btn_setup.setChecked(True)
                self.w.jogging_frame.hide()

    # NGCGui library overridden function
    # adds an error message to status
    def check_linuxcnc_paths_fail_override(self, fname):
        self.add_status("NGCGUI Path {} not in linuxcnc's SUBROUTINE_PATH INI entry".format(fname), CRITICAL)
        return ''

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
                mess += '<span style=" font-size:18pt; font-weight:600; color:black;">%s </span>\
<span style=" font-size:18pt; font-weight:600; color:#aa0000;">%s</span>\
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

        max_probe = self.w.lineEdit_max_probe.text()
        search_vel = self.w.lineEdit_search_vel.text()
        probe_vel = self.w.lineEdit_probe_vel.text()
        retract = self.w.lineEdit_retract_distance.text()
        safe_z = self.w.lineEdit_z_safe_travel.text()
        self.add_status("Touchoff to {} started with {} {} {} {} {} {}".format(selector,
                search_vel, probe_vel, max_probe, 
                z_offset, retract, safe_z))
        rtn = ACTION.TOUCHPLATE_TOUCHOFF(search_vel, probe_vel, max_probe, 
                z_offset, retract, safe_z, self.touchoff_return,self.touchoff_error)
        if not rtn == 1:
            self.add_status(rtn, CRITICAL)

    def touchoff_return(self, data):
        self.add_status("Touchplate touchoff routine returned successfully")
        self.add_status("Touchplate returned: "+data, CRITICAL)

    def touchoff_error(self, data):
        ACTION.SET_ERROR_MESSAGE(data)
        self.add_status(data, CRITICAL)

    def kb_jog(self, state, joint, direction, fast = False, linear = True):
        ACTION.SET_MANUAL_MODE()
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

    def add_status(self, message, alertLevel = DEFAULT, noLog = False):
        if alertLevel==DEFAULT:
            self.set_style_default()
        elif alertLevel==WARNING:
            self.set_style_warning()
        else:
            self.set_style_critical()
        self.w.lineEdit_statusbar.setText(message)
        if noLog:
            return
        STATUS.emit('update-machine-log', message, 'TIME')

    def enable_auto(self, state):
        for widget in self.auto_list:
            self.w[widget].setEnabled(state)
        # need to let adjust stack function know the mode changed
        # not auto
        if state is True:
            self.adjust_stacked_widgets(self.w.stackedWidget_mainTab.currentIndex(),mode_change = True)
        # auto mode
        else:
            self.w.btn_main.setChecked(True)
            self.adjust_stacked_widgets(TAB_MAIN,mode_change = True)


    def enable_onoff(self, state):
        if state:
            self.add_status("Machine ON")
        else:
            self.add_status("Machine OFF")
        self.w.btn_spindle_pause.setChecked(False)
        self.h['eoffset-spindle-count'] = 0
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

    def periodic_update(self):
        # if waiting and up to speed, lower spindle
        if self._spindle_wait:
            if bool(self.h.hal.get_value('spindle.0.at-speed')):
                self.lowerSpindle()
                self.h['eoffset-clear'] = False
                self._spindle_wait = False

        self.update_runtimer()

    def update_runtimer(self):
        if not self.timer_on or STATUS.is_auto_paused():
            return

        tick = time.time()
        elapsed_time = tick - self.timer_tick
        self.run_time += elapsed_time
        self.timer_tick = tick

        hours, remainder = divmod(int(self.run_time), 3600)
        minutes, seconds = divmod(remainder, 60)
        self.w.lbl_runtime.setText("{:02d}:{:02d}:{:02d}".format(hours, minutes, seconds))


    def start_timer(self):
        self.run_time = 0
        self.timer_on = True
        self.timer_tick = time.time()

    def stop_timer(self):
        self.timer_on = False
        if STATUS.is_auto_mode():
            self.add_status("Run timer stopped at {}".format(self.w.lbl_runtime.text()))

    # web page zoom
    def zoomWeb(self):
        # webview
        try:
            f = self.w.webwidget.zoomFactor() +.5
            if f > 2:f = 1
            self.w.webwidget.setZoomFactor(f)
        except:
            pass

        # PDF
        try:
            f = self.PDFView.zoomFactor() +.5
            if f > 2:f = 1
            self.PDFView.setZoomFactor(f)
        except:
            pass
        
    # go directly the default HTML page
    def homeWeb(self):
        try:
            if os.path.exists(self.default_setup):
                self.w.webwidget.load(QtCore.QUrl.fromLocalFile(self.default_setup))
            else:
                self.w.webwidget.setHtml(self.html)
        except:
            pass
    # setup tab's web page back button
    def back(self):
        try:
            try:
                self.w.webwidget.page().triggerAction(QWebEnginePage.Back)
            except:
                if os.path.exists(self.default_setup):
                    self.w.webwidget.load(QtCore.QUrl.fromLocalFile(self.default_setup))
                else:
                    self.w.webwidget.setHtml(self.html)
        except:
            pass

    # setup tab's web page forward button
    def forward(self):
        try:
            try:
                self.w.webwidget.page().triggerAction(QWebEnginePage.Forward)
            except:
                self.w.webwidget.load(QtCore.QUrl.fromLocalFile(self.docs))
        except:
            pass

    # setup tab's web page - enable/disable buttons
    def onLoadFinished(self):
        try:
            if self.w.webwidget.history().canGoBack():
                self.backBtn.setEnabled(True)
            else:
                self.backBtn.setEnabled(False)

            if self.w.webwidget.history().canGoForward():
                self.forBtn.setEnabled(True)
            else:
                self.forBtn.setEnabled(False)
        except:
            pass

    def writer(self):
        WRITER.show()

    def endcolor(self):
        self.timer = QtCore.QTimer()
        self.timer.timeout.connect(self.clear_status_bar)
        self.timer.start(self.statusbar_reset_time)

    def clear_status_bar(self):
        self.set_style_default()
        self.w.lineEdit_statusbar.setText('')

    # change Status bar text color
    def set_style_default(self):
        c = self.w.screen_options.property('user1Color').name()
        self.w.lineEdit_statusbar.setStyleSheet(
                "background-color: {} ;color: rgb(0,0,0)".format(c))  #default white

    def set_style_warning(self):
        c = self.w.screen_options.property('user2Color').name()
        self.w.lineEdit_statusbar.setStyleSheet(
                "background-color: {} ;color: rgb(0,0,0)".format(c))  #yellow
        self.endcolor()

    def set_style_critical(self):
        c = self.w.screen_options.property('user3Color').name()
        self.w.lineEdit_statusbar.setStyleSheet(
                "background-color: {} ;color: rgb(0,0,0)".format(c))   #orange
        self.endcolor()

    def adjust_stacked_widgets(self,requestedIndex,mode_change=False):
        IGNORE = -1
        SHOW_DRO = 0
        mode = ['','','Auto','MDI'][STATUS.get_current_mode()]
        if mode_change:
            premode = ['','','Auto','MDI'][STATUS.get_previous_mode()]
        else:
            premode=mode
        currentIndex = self.w.stackedWidget_mainTab.currentIndex()
        indexList = ['main','file','offsets','tool','status','probe','cam',
                    'gcode','setup','settings','util','user']

        if mode == 'Auto':
            seq = {TAB_MAIN: (TAB_MAIN,PAGE_GCODE,False,SHOW_DRO,False),
                    TAB_FILE: (TAB_MAIN,PAGE_GCODE,False,SHOW_DRO,False),
                    TAB_OFFSETS: (TAB_MAIN,PAGE_GCODE,False,SHOW_DRO,False),
                    TAB_TOOL: (TAB_MAIN,PAGE_GCODE,False,SHOW_DRO,False),
                    TAB_STATUS: (requestedIndex,PAGE_GCODE,False,SHOW_DRO,False),
                    TAB_PROBE: (TAB_MAIN,PAGE_GCODE,False,SHOW_DRO,False),
                    TAB_CAMERA: (requestedIndex,PAGE_UNCHANGED,False,SHOW_DRO,False),
                    TAB_GCODES: (requestedIndex,PAGE_UNCHANGED,False,SHOW_DRO,False),
                    TAB_SETUP: (requestedIndex,PAGE_UNCHANGED,False,SHOW_DRO,False),
                    TAB_SETTINGS: (requestedIndex,PAGE_GCODE,False,SHOW_DRO,False),
                    TAB_UTILITIES: (TAB_MAIN,PAGE_GCODE,False,SHOW_DRO,False),
                    TAB_USER: (requestedIndex,PAGE_UNCHANGED,IGNORE,IGNORE,False) }
        else:
            seq = {TAB_MAIN: (requestedIndex,PAGE_GCODE,True,SHOW_DRO,True),
                    TAB_FILE: (requestedIndex,PAGE_FILE,True,IGNORE,False),
                    TAB_OFFSETS: (requestedIndex,PAGE_OFFSET,True,IGNORE,False),
                    TAB_TOOL: (requestedIndex,PAGE_TOOL,True,IGNORE,False),
                    TAB_STATUS: (requestedIndex,PAGE_UNCHANGED,True,SHOW_DRO,False),
                    TAB_PROBE: (requestedIndex,PAGE_GCODE,True,SHOW_DRO,False),
                    TAB_CAMERA: (requestedIndex,PAGE_UNCHANGED,True,IGNORE,True),
                    TAB_GCODES: (requestedIndex,PAGE_UNCHANGED,False,SHOW_DRO,False),
                    TAB_SETUP: (requestedIndex,PAGE_UNCHANGED,False,IGNORE,False),
                    TAB_SETTINGS: (requestedIndex,PAGE_UNCHANGED,False,SHOW_DRO,False),
                    TAB_UTILITIES: (requestedIndex,PAGE_UNCHANGED,True,SHOW_DRO,False),
                    TAB_USER: (requestedIndex,PAGE_UNCHANGED,IGNORE,IGNORE,True) }

        rtn =  seq.get(requestedIndex)

        # if not found (None) use defaults
        if rtn is None:
            main_index = requestedIndex
            stacked_index = 0
            show_JogControls = True
            show_dro = 0
            show_macro = True
        else:
            main_index,stacked_index,show_JogControls,show_dro,show_macro = rtn

        # user tab button covers multiple tabs so adjust name
        # for extra user tabs
        if currentIndex >= len(indexList):
            tabId = 'user{}'.format(currentIndex - len(indexList))
        else:
            tabId = indexList[currentIndex]
        name = 'splitterSettings-{}{}'.format(tabId,premode)
        #print ('CURRENT:',name)
        # record current qsplitter settings
        self.w.settings.beginGroup("qtdragon-{}".format(self.w.splitter_h.objectName()))
        self.w.settings.setValue(name, QtCore.QVariant(self.w.splitter_h.saveState().data()))
        self.w.settings.endGroup()

        # ignore, show or hide jog controls
        if show_JogControls == IGNORE:
            pass
        elif show_JogControls:
            self.w.jogging_frame.show()
        else:
            self.w.jogging_frame.hide()

        # show DRO rather then keyboard.
        if show_dro > IGNORE:
            self.w.stackedWidget_dro.setCurrentIndex(0)

        # macros can only be run in manual or mdi mode
        if show_macro:
            self.w.frame_macro_buttons.show()
        else:
            self.w.frame_macro_buttons.hide()

        # show ngcgui info tab if utilities tab is selected
        # but only if the utilities tab has ngcgui selected
        if main_index == TAB_UTILITIES:
            if self.w.tabWidget_utilities.currentIndex() == 2:
                self.w.stackedWidget.setCurrentIndex(PAGE_NGCGUI)
            else:
                self.w.stackedWidget.setCurrentIndex(PAGE_GCODE)

        # adjust the stacked widget (left side MDI/Gcode stack)
        if stacked_index > PAGE_UNCHANGED:
            self.w.stackedWidget.setCurrentIndex(stacked_index)

        # toggle home/tool offsets buttons in DRO section
        if main_index == TAB_TOOL:
            num = 1
        else:
            num = 0
        for n,i in enumerate(INFO.AVAILABLE_AXES):
            if n >2:
                self.w['dro_button_stack_%s'%(n+1)].setCurrentIndex(num)
            else:
                self.w['dro_button_stack_%s'%i.lower()].setCurrentIndex(num)

        # user tabs button cycles between all user tabs
        main_current = self.w.stackedWidget_mainTab.currentIndex()
        if main_index == TAB_USER and main_current >= TAB_USER:
                next = main_current +1
                if next == self.w.stackedWidget_mainTab.count():
                    next = TAB_USER
                self.w.stackedWidget_mainTab.setCurrentIndex(next)
        else:
            # set main tab to adjusted index
            self.w.stackedWidget_mainTab.setCurrentIndex(main_index)

        # if indexes don't match then request is disallowed
        # give a warning and reset the button check
        if main_index != requestedIndex and not main_index in(TAB_CAMERA,TAB_GCODES,TAB_SETUP):
            self.add_status("Cannot switch pages while in AUTO mode", WARNING)
            self.w.stackedWidget_mainTab.setCurrentIndex(0)
            self.w.btn_main.setChecked(True)

        # user tab button covers multiple tabs
        # adjust name depending in what user tab is showing
        cur = self.w.stackedWidget_mainTab.currentIndex()
        if cur >= len(indexList):
            tabId = 'user{}'.format(cur - len(indexList))
        else:
            tabId = indexList[cur]

        # if in auto mode and the main tab button is pressed
        # cycle between full gcode, split and pull graphics
        if main_index == TAB_MAIN and mode =='Auto':
            if self._maintab_cycle >3: self._maintab_cycle = 1
            if self._maintab_cycle == 2:
                self.w.frame_top_left.hide()
                self.w.frm_backplot.show()
                return
            elif self._maintab_cycle == 3:
                self.w.frame_top_left.show()
                self.w.frm_backplot.hide()
                return
            else:
                self.w.frame_top_left.show()
                self.w.frm_backplot.show()
        else:
            self.w.frame_top_left.show()
            self.w.frm_backplot.show()

        # adjust window splitter size as per saved adjustments

        name = 'splitterSettings-{}{}'.format(tabId,mode)
        #print ('NOW:',name)
        # restore new qsplitter setting
        self.w.settings.beginGroup("qtdragon-{}".format(self.w.splitter_h.objectName()))
        splitterSetting = self.w.settings.value(name)
        self.w.settings.endGroup()
        if not splitterSetting is None:
            try:
                self.w.splitter_h.restoreState(QtCore.QByteArray(splitterSetting))
            except Exception as e:
                print(e)

    # set axis 4/5 dro widgets to the proper axis
    # TODO do this with all the axes for more flexibility
    def initiate_axis_dro(self, num, axis):
        jnum = INFO.GET_JOG_FROM_NAME.get(axis)
        # DRO uses axis index
        index = "XYZABCUVW".index(axis)
        self.w['dro_axis_{}'.format(num)].setProperty('Qjoint_number',index)
        self.w['action_zero_{}'.format(num)].setProperty('axis_letter',axis)
        try:
            self.w['axis_select_{}'.format(num)].setProperty('axis_letter',axis)
            self.w['axis_select_{}'.format(num)].setText('{}'.format(axis))
        except:
            pass
        try:
            cmd = 'G90 G0 {}0'.format(axis)
            self.w['action_cmd_{}'.format(num)].setProperty('command_text_string',cmd)
        except:
            pass
        self.w['action_home_{}'.format(num)].setProperty('axis_letter',axis)
        self.w['action_home_{}'.format(num)].setProperty('joint_number_status',jnum)
        self.w['action_home_{}'.format(num)].setProperty('joint',index)
        self.w['offsettoolbutton_{}'.format(num)].setProperty('axis_letter',axis)
        self.w['plus_jogbutton_{}'.format(num)].setProperty('axis_letter',axis)
        self.w['plus_jogbutton_{}'.format(num)].setProperty('joint_number',jnum)
        a = axis.lower()
        try:
            icn = QtGui.QIcon(QtGui.QPixmap(':/buttons/images/{}_plus_jog_button.png'.format(a)))
            if icn.isNull(): raise Exception
            self.w['plus_jogbutton_{}'.format(num)].setIcon(icn)
        except Exception as e:
            self.w['plus_jogbutton_{}'.format(num)].setProperty('text','{}+'.format(axis))
        self.w['minus_jogbutton_{}'.format(num)].setProperty('axis_letter',axis)
        self.w['minus_jogbutton_{}'.format(num)].setProperty('joint_number',jnum)
        try:
            icn = QtGui.QIcon(QtGui.QPixmap(':/buttons/images/{}_minus_jog_button.png'.format(a)))
            if icn.isNull(): raise Exception
            self.w['minus_jogbutton_{}'.format(num)].setIcon(icn)
        except Exception as e:
            self.w['minus_jogbutton_{}'.format(num)].setProperty('text','{}-'.format(axis))

    # MPG scrolling of program or MDI history
    def external_mpg(self, count):
        diff = count - self._last_count
        if self.w.btn_mpg_scroll.isChecked():
            currentIndex = self.w.stackedWidget_mainTab.currentIndex()
            if isinstance(self.MPGFocusWidget, SLIDER):
                #print('Slider',self.MPGFocusWidget.objectName(),self.MPGFocusWidget.parent())
                if self.MPGFocusWidget is self.w.slider_feed_ovr:
                    scaled = (STATUS.stat.feedrate * 100 + diff)
                    if scaled <0 :scaled = 0
                    elif scaled > INFO.MAX_FEED_OVERRIDE:scaled = INFO.MAX_FEED_OVERRIDE
                    ACTION.SET_FEED_RATE(scaled)
                elif  self.MPGFocusWidget is self.w.slider_rapid_ovr:
                    scaled = (STATUS.stat.rapidrate * 100 + diff)
                    if scaled <0 :scaled = 0
                    elif scaled > 100:scaled = 100
                    ACTION.SET_RAPID_RATE(scaled)
                elif  self.MPGFocusWidget is self.w.slider_spindle_ovr:
                    scaled = (STATUS.stat.spindle[0]['override'] * 100 + diff)
                    if scaled < INFO.MIN_SPINDLE_OVERRIDE:scaled = INFO.MIN_SPINDLE_OVERRIDE
                    elif scaled > INFO.MAX_SPINDLE_OVERRIDE:scaled = INFO.MAX_SPINDLE_OVERRIDE
                    ACTION.SET_SPINDLE_RATE(scaled)

            elif isinstance(self.MPGFocusWidget, GRAPHICS):
                if self.w.actionbutton_pan_rpyaye.isChecked():
                    ACTION.ADJUST_GRAPHICS_ROTATE(diff,diff)
                else:
                    ACTION.ADJUST_GRAPHICS_PAN(diff,0)
            elif isinstance(self.MPGFocusWidget, GCODE):
                self.w.gcode_editor.jump_line(diff)
                self.w.gcode_viewer.jump_line(diff)
            elif isinstance(self.MPGFocusWidget, MDI_WIDGET):
                if diff <0:
                   self.MPGFocusWidget.line_down()
                else:
                   self.MPGFocusWidget.line_up()

            elif currentIndex == TAB_FILE:
                if isinstance(self.MPGFocusWidget, FM):
                    widget =  self.MPGFocusWidget
                else:
                    widget = self.w.filemanager
                if diff <0:
                   widget.down()
                else:
                   widget.up()
            elif currentIndex == TAB_TOOL:
                if isinstance(self.MPGFocusWidget, TOOL_TABLE):
                    print('scroll offset view')
                    if diff <0:
                       self.MPGFocusWidget.down()
                    else:
                       self.MPGFocusWidget.up()
            elif currentIndex == TAB_OFFSETS:
                if isinstance(self.MPGFocusWidget, OFFSET_VIEW):
                    print('scroll offset view')
                    if diff <0:
                       self.MPGFocusWidget.down()
                    else:
                       self.MPGFocusWidget.up()

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
            self.w.action_pause.click()

    def on_keycall_jograte(self,event,state,shift,cntrl,value):
        if state and self.use_keyboard():
            if value == 1:
                ACTION.SET_JOG_RATE_FASTER()
            else:
                ACTION.SET_JOG_RATE_SLOWER()

    def on_keycall_angular_jograte(self,event,state,shift,cntrl,value):
        if state and self.use_keyboard():
            if value == 1:
                ACTION.SET_JOG_RATE_ANGULAR_FASTER()
            else:
                ACTION.SET_JOG_RATE_ANGULAR_SLOWER()

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

    def on_keycall_F12(self,event,state,shift,cntrl):
        if state:
            STYLEEDITOR.load_dialog()

    def on_keycall_F11(self,event,state,shift,cntrl):
        if self.w.isFullScreen() == False and state == True:
            self.w.showFullScreen()
        else:
            if self.w.isFullScreen() == True and state == True:
                self.w.showNormal()



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
