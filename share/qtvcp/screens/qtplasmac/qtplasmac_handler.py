VERSION = '1.235.273'

'''
qtplasmac_handler.py

Copyright (C) 2020, 2021, 2022, 2023  Phillip A Carter
Copyright (C) 2020, 2021, 2022, 2023  Gregory D Carl

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
'''

import os
import sys
from shutil import copy as COPY
from shutil import move as MOVE
from subprocess import Popen, PIPE
from subprocess import run as RUN
from subprocess import call as CALL
from importlib import reload
import time
import tarfile
import math
import glob
import linuxcnc
import hal
from OpenGL.GL import glTranslatef
from PyQt5 import QtCore, QtWidgets, QtGui
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *
from PyQt5.QtGui import *
from PyQt5.Qsci import QsciScintilla
from qtvcp import logger
from qtvcp.core import Status, Action, Info, Tool
from qtvcp.lib.gcodes import GCodes
from qtvcp.lib.keybindings import Keylookup
from qtvcp.lib.preferences import Access
from qtvcp.lib.qtplasmac import tooltips as TOOLTIPS
from qtvcp.lib.qtplasmac import set_offsets as OFFSETS
from qtvcp.lib.qtplasmac import updater as UPDATER
from qtvcp.widgets.camview_widget import CamView as CAM
from qtvcp.widgets.file_manager import FileManager as FILE_MAN
from qtvcp.widgets.gcode_editor import GcodeEditor as EDITOR
from qtvcp.widgets.gcode_editor import GcodeDisplay as DISPLAY
from qtvcp.widgets.mdi_history import MDIHistory as MDI_HISTORY
from qtvcp.widgets.mdi_line import MDILine as MDI_LINE
from qtvcp.widgets.origin_offsetview import OriginOffsetView as OFFSETVIEW
from qtvcp.widgets.origin_offsetview import MyTableModel as OFFSET_TABLE
from qtvcp.widgets.screen_options import ScreenOptions as OPTIONS
from qtvcp.widgets.simple_widgets import DoubleScale as DOUBLESCALE
from qtvcp.widgets.status_label import StatusLabel as STATLABEL
from qtvcp.widgets.stylesheeteditor import  StyleSheetEditor as SSE
from qtvcp.lib.aux_program_loader import Aux_program_loader
from plasmac import run_from_line as RFL
from rs274.glcanon import GlCanonDraw
from qt5_graphics import Lcnc_3dGraphics as DRO

LOG = logger.getLogger(__name__)
KEYBIND = Keylookup()
STATUS = Status()
INFO = Info()
ACTION = Action()
TOOL = Tool()
AUX_PRGM = Aux_program_loader()
INIPATH = os.environ.get('INI_FILE_NAME', '/dev/null')

_translate = QCoreApplication.translate

# a vertical line as a separator on the status bar
class VLine(QFrame):
    def __init__(self):
        super(VLine, self).__init__()
        self.setFrameShape(self.VLine|self.Plain)

# dummy class to raise an exception for style issues
class ColorError(Exception):
    pass

# click signal for some labels
def click_signal(widget):
    class Filter(QObject):
        clicked = pyqtSignal()
        def eventFilter(self, obj, event):
            if obj == widget:
                if event.type() == QEvent.MouseButtonRelease:
                    if obj.rect().contains(event.pos()):
                        self.clicked.emit()
                        return True
            return False
    filter = Filter(widget)
    widget.installEventFilter(filter)
    return filter.clicked

  # the main handler
class HandlerClass:
    from qtvcp.lib.qtplasmac import conversational as CONV
    # when self.w.button_frame changes size
    def eventFilter(self, object, event):
        if (event.type() == QtCore.QEvent.Resize):
            self.size_changed(object)
        return True

    def __init__(self, halcomp, widgets, paths):
        self.firstRun = True
        self.h = halcomp
        self.w = widgets
        self.h.comp.setprefix('qtplasmac')
        self.PATHS = paths
        self.iniFile = INFO.INI
        self.foreColor = '#ffee06'
        # ensure M190 exists in config directory
        if not os.path.isfile(os.path.join(self.PATHS.CONFIGPATH, 'M190')):
            if '/usr' in self.PATHS.BASEDIR:
                m190Path = os.path.join(self.PATHS.BASEDIR, 'share/doc/linuxcnc/examples/sample-configs/sim/qtplasmac')
            else:
                m190Path = os.path.join(self.PATHS.BASEDIR, 'configs/sim/qtplasmac')
            # M190 already exists for RIP sims
            if os.path.realpath(m190Path) != os.path.realpath(self.PATHS.CONFIGPATH):
                COPY(os.path.join(m190Path, 'M190'), os.path.join(self.PATHS.CONFIGPATH, 'M190'))
        self.machineName = self.iniFile.find('EMC', 'MACHINE')
        self.unitsPerMm = 1
        self.units = self.iniFile.find('TRAJ', 'LINEAR_UNITS')
        if self.units == 'inch':
            self.units = 'in'
            self.unitsPerMm = 0.03937
        # open prefs file so we can use it for updates
        # it will not exist prior to V1.222.170 2022/03/08
        if os.path.isfile(os.path.join(self.PATHS.CONFIGPATH, self.machineName + '.prefs')):
            self.PREFS = Access(os.path.join(self.PATHS.CONFIGPATH, self.machineName + '.prefs'))
        else:
            self.PREFS = None
        self.updateIni = []
        self.update_check()
        self.PREFS = Access(os.path.join(self.PATHS.CONFIGPATH, self.machineName + '.prefs'))
        self.STYLEEDITOR = SSE(widgets, paths)
        self.GCODES = GCodes(widgets)
        self.IMAGES = os.path.join(self.PATHS.IMAGEDIR, 'qtplasmac/images/')
        self.landscape = True
        if os.path.basename(self.PATHS.XML) == 'qtplasmac_9x16.ui':
            self.landscape = False
        self.upFile = os.path.join(self.PATHS.CONFIGPATH, 'user_periodic.py')
        KEYBIND.add_call('Key_F12','on_keycall_F12')
        KEYBIND.add_call('Key_F9','on_keycall_F9')
        KEYBIND.add_call('Key_Plus', 'on_keycall_PLUS')
        KEYBIND.add_call('Key_Minus', 'on_keycall_MINUS')
        KEYBIND.add_call('Key_R', 'on_keycall_RUN')
        KEYBIND.add_call('Key_Any', 'on_keycall_PAUSE')
        KEYBIND.add_call('Key_o', 'on_keycall_OPEN')
        KEYBIND.add_call('Key_l', 'on_keycall_LOAD')
        KEYBIND.add_call('Key_1', 'on_keycall_NUMBER', 1)
        KEYBIND.add_call('Key_Exclam', 'on_keycall_NUMBER', 1)
        KEYBIND.add_call('Key_2', 'on_keycall_NUMBER', 2)
        KEYBIND.add_call('Key_At', 'on_keycall_NUMBER', 2)
        KEYBIND.add_call('Key_3', 'on_keycall_NUMBER', 3)
        KEYBIND.add_call('Key_NumberSign', 'on_keycall_NUMBER', 3)
        KEYBIND.add_call('Key_4', 'on_keycall_NUMBER', 4)
        KEYBIND.add_call('Key_Dollar', 'on_keycall_NUMBER', 4)
        KEYBIND.add_call('Key_5', 'on_keycall_NUMBER', 5)
        KEYBIND.add_call('Key_Percent', 'on_keycall_NUMBER', 5)
        KEYBIND.add_call('Key_6', 'on_keycall_NUMBER', 6)
        KEYBIND.add_call('Key_AsciiCircum', 'on_keycall_NUMBER', 6)
        KEYBIND.add_call('Key_7', 'on_keycall_NUMBER', 7)
        KEYBIND.add_call('Key_Ampersand', 'on_keycall_NUMBER', 7)
        KEYBIND.add_call('Key_8', 'on_keycall_NUMBER', 8)
        KEYBIND.add_call('Key_Asterisk', 'on_keycall_NUMBER', 8)
        KEYBIND.add_call('Key_9', 'on_keycall_NUMBER', 9)
        KEYBIND.add_call('Key_ParenLeft', 'on_keycall_NUMBER', 9)
        KEYBIND.add_call('Key_0', 'on_keycall_NUMBER', 0)
        KEYBIND.add_call('Key_ParenRight', 'on_keycall_NUMBER', 0)
        # there are no default keys for joint/axis 4
        KEYBIND.add_call('Key_Comma', 'on_keycall_BPOS')
        KEYBIND.add_call('Key_Less', 'on_keycall_BPOS')
        KEYBIND.add_call('Key_Period', 'on_keycall_BNEG')
        KEYBIND.add_call('Key_Greater', 'on_keycall_BNEG')
        KEYBIND.add_call('Key_End', 'on_keycall_END')
        KEYBIND.add_call('Key_Delete', 'on_keycall_DELETE')
        KEYBIND.add_call('Alt+Key_Return', 'on_keycall_ALTRETURN')
        KEYBIND.add_call('Alt+Key_Enter', 'on_keycall_ALTRETURN')
        KEYBIND.add_call('Key_Return', 'on_keycall_RETURN')
        KEYBIND.add_call('Key_Enter', 'on_keycall_RETURN')
        KEYBIND.add_call('Key_QuoteLeft', 'on_keycall_QUOTELEFT')
        KEYBIND.add_call('Key_AsciiTilde', 'on_keycall_QUOTELEFT')
        self.axisList = [x.lower() for x in INFO.AVAILABLE_AXES]
        self.systemList = ['G53','G54','G55','G56','G57','G58','G59','G59.1','G59.2','G59.3']
        self.slowJogFactor = 10
        self.jogFast = False
        self.jogSlow = False
        self.lastLoadedProgram = 'None'
        self.estopOnList = []
        self.idleList = ['file_clear', 'file_open', 'file_reload']
        self.idleOnList = ['home_x', 'home_y', 'home_z', 'home_a', 'home_all']
        self.idleHomedList = ['touch_x', 'touch_y', 'touch_z', 'touch_a', 'touch_b', 'touch_xy', \
                              'mdi_show', 'height_lower', 'height_raise', 'wcs_button', 'set_offsets']
        self.pausedValidList= []
        self.jogButtonList = ['jog_x_plus', 'jog_x_minus', 'jog_y_plus', 'jog_y_minus', 'jog_z_plus', \
                              'jog_z_minus', 'jog_a_plus', 'jog_a_minus', 'jog_b_plus', 'jog_b_minus']
        self.jogSyncList = []
        self.axisAList = ['dro_a', 'dro_label_a', 'home_a', 'touch_a', 'jog_a_plus', 'jog_a_minus']
        self.axisBList = ['dro_b', 'dro_label_b', 'home_b', 'touch_b', 'jog_b_plus', 'jog_b_minus']
        self.xMin = float(self.iniFile.find('AXIS_X', 'MIN_LIMIT'))
        self.xMax = float(self.iniFile.find('AXIS_X', 'MAX_LIMIT'))
        self.yMin = float(self.iniFile.find('AXIS_Y', 'MIN_LIMIT'))
        self.yMax = float(self.iniFile.find('AXIS_Y', 'MAX_LIMIT'))
        self.zMin = float(self.iniFile.find('AXIS_Z', 'MIN_LIMIT'))
        self.zMax = float(self.iniFile.find('AXIS_Z', 'MAX_LIMIT'))
        self.xLen = self.xMax - self.xMin
        self.yLen = self.yMax - self.yMin
        self.thcFeedRate = float(self.iniFile.find('AXIS_Z', 'MAX_VELOCITY')) * \
                           float(self.iniFile.find('AXIS_Z', 'OFFSET_AV_RATIO')) * 60
        self.maxHeight = self.zMax - self.zMin
        self.maxPidP = self.thcFeedRate / self.unitsPerMm * 0.1
        self.tmpPath = '/tmp/qtplasmac/'
        if not os.path.isdir(self.tmpPath):
            os.mkdir(self.tmpPath)
        self.materialFile = '{}_material.cfg'.format(self.machineName)
        self.tmpMaterialFile = '{}{}'.format(self.tmpPath, self.materialFile.replace('.cfg','.tmp'))
        self.tmpMaterialFileGCode = '{}{}'.format(self.tmpPath, self.materialFile.replace('.cfg','.gcode'))
        self.gcodeErrorFile = '{}gcode_errors.txt'.format(self.tmpPath)
        self.materialFileDict = {}
        self.materialDict = {}
        self.materialNumList = []
        self.materialUpdate = False
        self.autoChange = False
        self.pmx485Exists = False
        self.pmx485Connected = False
        self.pmx485CommsError = False
        self.pmx485FaultCode = 0.0
        self.pmx485ArcTime = 0.0
        self.pmx485LabelState = None
        self.currentX = self.currentY = 0
        self.degreeSymbol = u"\u00b0"
        self.cameraOn = False
        self.fTmp = '{}temp.ngc'.format(self.tmpPath)
        self.fNgc = '{}shape.ngc'.format(self.tmpPath)
        self.fNgcBkp = '{}backup.ngc'.format(self.tmpPath)
        self.fNgcSent = '{}sent_shape.ngc'.format(self.tmpPath)
        self.filteredBkp = '{}filtered_bkp.ngc'.format(self.tmpPath)
        self.oldConvButton = False
        self.convWidgetsLoaded = False
        self.programPrefix = self.iniFile.find('DISPLAY', 'PROGRAM_PREFIX') or os.environ['LINUXCNC_NCFILES_DIR']
        self.dialogError = False
        self.cutTypeText = ''
        self.heightOvr = 0.0
        self.heightOvrScale = 0.1
        self.old_ovr_counts = 0
        self.startLine = 0
        self.preRflFile = ''
        self.preClearFile = ''
        self.rflActive = False
        self.ccButton, self.otButton, self.ptButton, self.tpButton = '', '', '', ''
        self.ctButton, self.scButton, self.frButton, self.mcButton = '', '', '', ''
        self.ovButton, self.llButton, self.tlButton = '', '', ''
        self.halTogglePins = {}
        self.halPulsePins = {}
        self.torchOn = False
        self.progRun = False
        self.rapidOn = False
        self.probeOn = False
        self.gcodeProps = {}
        self.framing = False
        self.boundsError = {'loaded': False, 'framing': False, 'align': False}
        self.obLayout = ''
        self.notifyColor = 'normal'
        self.firstHoming = False
        self.droScale = 1
        self.mdiError = False
        self.extLaserButton = False
        self.virtualMachine = False
        self.realTimeDelay = False
        # plasmac states
        self.IDLE           =  0
        self.PROBE_HEIGHT   =  1
        self.PROBE_DOWN     =  2
        self.PROBE_UP       =  3
        self.ZERO_HEIGHT    =  4
        self.PIERCE_HEIGHT  =  5
        self.TORCH_ON       =  6
        self.ARC_OK         =  7
        self.PIERCE_DELAY   =  8
        self.PUDDLE_JUMP    =  9
        self.CUT_HEGHT      = 10
        self.CUT_MODE_01    = 11
        self.CUT_MODE_2     = 12
        self.PAUSE_AT_END   = 13
        self.SAFE_HEIGHT    = 14
        self.MAX_HEIGHT     = 15
        self.END_CUT        = 16
        self.END_JOB        = 17
        self.TORCH_PULSE    = 18
        self.PAUSED_MOTION  = 19
        self.OHMIC_TEST     = 20
        self.PROBE_TEST     = 21
        self.SCRIBING       = 22
        self.CONS_CHNG_ON   = 23
        self.CONS_CHNG_OFF  = 24
        self.CUT_REC_ON     = 25
        self.CUT_REC_OFF    = 26
        self.DEBUG          = 27

# called by qtvcp.py
    def initialized__(self):
        # ensure we get all startup errors
        STATUS.connect('error', self.error_update)
        STATUS.connect('graphics-gcode-error', lambda o, e:self.error_update(o, linuxcnc.OPERATOR_ERROR, e))
        STATUS.emit('update-machine-log', '--- {} - QtPlasmaC v{}, powered by QtVCP on LinuxCNC v{} ---'.format(self.machineName, VERSION, linuxcnc.version.split(':')[0]), None)
        self.make_hal_pins()
        self.init_preferences()
        self.hide_widgets()
        self.init_widgets()
        # hijack the qtvcp shutdown to our own close event
        self.w.screen_options.QTVCP_INSTANCE_.closeEvent = self.closeEvent
        self.w.button_frame.installEventFilter(self.w)
        self.link_hal_pins()
        self.statistics_init()
        self.set_axes_and_joints()
        self.set_spinbox_parameters()
        self.load_plasma_parameters()
        self.set_mode()
        self.user_button_setup()
        self.set_buttons_state([self.estopOnList], True)
        self.check_material_file()
        self.load_materials()
        self.offset_peripherals()
        self.set_probe_offset_pins()
        self.wcs_rotation('get')
        STATUS.connect('state-estop', lambda w:self.estop_state(True))
        STATUS.connect('state-estop-reset', lambda w:self.estop_state(False))
        STATUS.connect('state-on', lambda w:self.power_state(True))
        STATUS.connect('state-off', lambda w:self.power_state(False))
        STATUS.connect('hard-limits-tripped', self.hard_limit_tripped)
        STATUS.connect('user-system-changed', self.user_system_changed)
        STATUS.connect('file-loaded', self.file_loaded)
        STATUS.connect('homed', self.joint_homed)
        STATUS.connect('all-homed', self.joints_all_homed)
        STATUS.connect('not-all-homed', self.joint_unhomed)
        STATUS.connect('gcode-line-selected', lambda w, line:self.set_start_line(line))
        STATUS.connect('graphics-line-selected', lambda w, line:self.set_start_line(line))
        STATUS.connect('g-code-changed', self.gcodes_changed)
        STATUS.connect('m-code-changed', self.mcodes_changed)
        STATUS.connect('program-pause-changed', self.pause_changed)
        STATUS.connect('graphics-loading-progress', self.progress_changed)
        STATUS.connect('interp-paused', self.interp_paused)
        STATUS.connect('interp-idle', self.interp_idle)
        STATUS.connect('interp-reading', self.interp_reading)
        STATUS.connect('interp-waiting', self.interp_waiting)
        STATUS.connect('interp-run', self.interp_running)
        STATUS.connect('jograte-changed', self.jog_rate_changed)
        STATUS.connect('graphics-gcode-properties', lambda w, d:self.update_gcode_properties(d))
        STATUS.connect('system_notify_button_pressed', self.system_notify_button_pressed)
        STATUS.connect('tool-in-spindle-changed', self.tool_changed)
        STATUS.connect('periodic', lambda w: self.update_periodic())
        STATUS.connect('metric-mode-changed', self.metric_mode_changed)
        STATUS.connect('motion-type-changed', lambda w, data: self.motion_type_changed(data))
        self.startupTimer = QTimer()
        self.startupTimer.timeout.connect(self.startup_timeout)
        self.startupTimer.setSingleShot(True)
        self.shutdownTimer = QTimer()
        self.shutdownTimer.timeout.connect(self.shutdown_timeout)
        self.shutdownTime = 2000
        self.laserTimer = QTimer()
        self.laserTimer.timeout.connect(self.laser_timeout)
        self.laserTimer.setSingleShot(True)
        self.ohmicLedTimer = QTimer()
        self.ohmicLedTimer.timeout.connect(self.ohmic_led_timeout)
        self.ohmicLedTimer.setSingleShot(True)
        self.set_color_styles()
        self.autorepeat_keys(False)
        self.vm_check()
        # set hal pins only after initialized__ has begun
        # some locales won't set pins before this phase
        self.thcFeedRatePin.set(self.thcFeedRate)
        self.pmPort = None
        if self.PREFS.getpref('Port', '', str, 'POWERMAX'):
            self.pmPort = self.PREFS.getpref('Port', '', str, 'POWERMAX')
        if self.pmPort and self.pmx485_check(self.pmPort):
            self.pmx485_startup(self.pmPort)
        else:
            self.w.gas_pressure.hide()
            self.w.gas_pressure_label.hide()
            self.w.cut_mode.hide()
            self.w.cut_mode_label.hide()
            self.w.pmx485_frame.hide()
            self.w.pmx_stats_frame.hide()
        if self.w.mdihistory.rows:
            self.mdiLast = self.w.mdihistory.model.item(self.w.mdihistory.rows - 1).text()
        else:
            self.mdiLast = None
        self.w.mdihistory.MDILine.spindle_inhibit(True)
        if self.updateIni:
            self.update_iniwrite()
        self.startupTimer.start(250)

# called by qtvcp.py, can override qtvcp settings or qtvcp allowed user options (via INI)
    def before_loop__(self):
        self.w.setWindowTitle('{} - QtPlasmaC v{}, powered by QtVCP on LinuxCNC v{}'.format(self.machineName, VERSION, linuxcnc.version.split(':')[0]))
        self.iconPath = 'share/icons/hicolor/scalable/apps/linuxcnc_alt/linuxcncicon_plasma.svg'
        appPath = os.path.realpath(os.path.dirname(sys.argv[0]))
        self.iconBase = '/usr' if appPath == '/usr/bin' else appPath.replace('/bin', '/debian/extras/usr')
        self.w.setWindowIcon(QIcon(os.path.join(self.iconBase, self.iconPath)))


#########################################################################################################################
# CLASS PATCHING SECTION #
#########################################################################################################################
# called by qtvcp.py
    def class_patch__(self):
        self.gcode_editor_patch()
        self.camview_patch()
        self.offset_table_patch()
        self.qt5_graphics_patch()
        self.screen_options_patch()

# patched gcode editor functions
    def gcode_editor_patch(self):
        self.old_saveReturn = EDITOR.saveReturn
        EDITOR.saveReturn = self.new_saveReturn
        self.old_openReturn = EDITOR.openReturn
        EDITOR.openReturn = self.new_openReturn
        self.old_exitCall = EDITOR.exitCall
        EDITOR.exitCall = self.new_exitCall
        self.old_gcodeLexerCall = EDITOR.gcodeLexerCall
        EDITOR.gcodeLexerCall = self.new_gcodeLexerCall
        self.old_pythonLexerCall = EDITOR.pythonLexerCall
        EDITOR.pythonLexerCall = self.new_pythonLexerCall
        self.old_returnFromDialog = EDITOR.returnFromDialog
        EDITOR.returnFromDialog = self.new_returnFromDialog
        DISPLAY.load_preference = self.new_load_preference
        self.old_set_line_number = DISPLAY.set_line_number
        DISPLAY.set_line_number = self.new_set_line_number

    # save a non gcode file and don't load it into linuxcnc
    def new_saveReturn(self, filename):
        saved = ACTION.SAVE_PROGRAM(self.w.gcode_editor.editor.text(), filename)
        if saved is not None:
            self.w.gcode_editor.editor.setModified(False)
            if saved[-3:] in ['ngc', '.nc', 'tap']:
                if self.rflActive:
                    self.clear_rfl()
                ACTION.OPEN_PROGRAM(saved)

    # open a non gcode file and don't load it into linuxcnc
    def new_openReturn(self, filename):
        if filename[-3:] in ['ngc', '.nc', 'tap']:
            ACTION.OPEN_PROGRAM(filename)
        else:
            self.w.gcode_editor.editor.load_text(filename)
        self.w.gcode_editor.editor.setModified(False)

    # modify the closing of the gcode editor
    def new_exitCall(self):
        if self.w.gcode_editor.editor.isModified():
            head = _translate('HandlerClass', 'Unsaved Changes')
            msg0 = _translate('HandlerClass', 'Unsaved changes will be lost')
            msg1 = _translate('HandlerClass', 'Do you want to exit')
            if not self.dialog_show_yesno(QMessageBox.Question, head, '\n{}\n\n{}?\n'.format(msg0, msg1)):
                return
        self.w.preview_stack.setCurrentIndex(0)
        if self.fileOpened == True and self.w.gcode_editor.editor.isModified():
            self.file_reload_clicked()
        elif self.fileOpened == True and not self.w.gcode_editor.editor.isModified():
            self.set_run_button_state()
        elif self.fileOpened == False:
            self.w.gcode_editor.editor.new_text()
            self.w.gcode_editor.editor.setModified(False)
            self.view_t_pressed()
        self.w.gcode_editor.editMode()
        self.vkb_hide()
        ACTION.SET_MANUAL_MODE()

    # we don't use lexer colors
    def new_gcodeLexerCall(self):
        pass

    # we don't use lexer colors
    def new_pythonLexerCall(self):
        pass

    # don't allow rfl.ngc as a file name
    def new_returnFromDialog(self, w, message):
        if message.get('NAME') == self.w.gcode_editor.load_dialog_code:
            path = message.get('RETURN')
            code = bool(message.get('ID') == '%s__'% self.w.gcode_editor.objectName())
            if path and code:
                self.w.gcode_editor.openReturn(path)
        elif message.get('NAME') == self.w.gcode_editor.save_dialog_code:
            path = message.get('RETURN')
            code = bool(message.get('ID') == '%s__'% self.w.gcode_editor.objectName())
            if path and code:
                if not os.path.basename(path) in ['rfl', 'rfl.ngc']:
                    self.w.gcode_editor.saveReturn(path)
                else:
                    head = _translate('HandlerClass', 'Save Error')
                    msg0 = _translate('HandlerClass', 'The file name')
                    msg1 = _translate('HandlerClass', 'is not allowed')
                    self.dialog_show_ok(QMessageBox.Warning, '{}'.format(head), '\n{} "{}" {}\n\n'.format(msg0, os.path.basename(path), msg1))
                    self.w.gcode_editor.getSaveFileName()
                    return

    # load the qtplasmac preferences file rather than the qtvcp preferences file
    def new_load_preference(self, w):
        self.w.gcode_editor.editor.load_text(os.path.join(self.PATHS.CONFIGPATH, self.machineName + '.prefs'))
        self.w.gcode_editor.editor.setCursorPosition(self.w.gcode_editor.editor.lines(), 0)

    # dont highlight lines selected from the editor in the preview window
    def new_set_line_number(self, line):
        if self.w.sender():
            if self.w.sender().objectName() == 'gcode_editor_display':
                return
            else:
                STATUS.emit('gcode-line-selected', line+1)

# patched camera functions
    def camview_patch(self):
        self.old_wheelEvent = CAM.wheelEvent
        CAM.wheelEvent = self.new_wheelEvent
        self.old_drawText = CAM.drawText
        CAM.drawText = self.new_drawText
        self.old_mousePressEvent = CAM.mousePressEvent
        CAM.mousePressEvent = self.new_mousePressEvent
        self.old_mouseDoubleClickEvent = CAM.mouseDoubleClickEvent
        CAM.mouseDoubleClickEvent = self.new_mouseDoubleClickEvent

    # format the angle display
    def new_drawText(self, event, qp):
        qp.setPen(self.w.camview.text_color)
        qp.setFont(self.w.camview.font)
        if self.w.camview.pix:
            qp.drawText(self.w.camview.rect(), QtCore.Qt.AlignTop, '{:0.3f}{}'.format(self.w.camview.rotation, self.degreeSymbol))
        else:
            qp.drawText(self.w.camview.rect(), QtCore.Qt.AlignCenter, self.w.camview.text)

    # limit scale and diameter, don't allow mouse rotation
    def new_wheelEvent(self, event):
        mouseState = qApp.mouseButtons()
        w = self.w.camview.size().width()
        h = self.w.camview.size().height()
        if event.angleDelta().y() < 0:
            if mouseState == QtCore.Qt.NoButton:
                self.w.camview.diameter -= 2
            if mouseState == QtCore.Qt.LeftButton:
                self.w.camview.scale -= .1
        else:
            if mouseState == QtCore.Qt.NoButton:
                self.w.camview.diameter += 2
            if mouseState == QtCore.Qt.LeftButton:
                self.w.camview.scale += .1
        if self.w.camview.diameter < 2: self.w.camview.diameter = 2
        if self.w.camview.diameter > w - 5: self.w.camview.diameter = w - 5
        if self.w.camview.diameter > h - 5: self.w.camview.diameter = h - 5
        if self.w.camview.scale < 1: self.w.camview.scale = 1
        if self.w.camview.scale > 5: self.w.camview.scale = 5

    # inhibit mouse single clicks
    def new_mousePressEvent(self, event):
        pass

    # don't reset rotation with double click
    def new_mouseDoubleClickEvent(self, event):
        if event.button() & QtCore.Qt.LeftButton:
            self.w.camview.scale = 1
        elif event.button() & QtCore.Qt.MiddleButton:
            self.w.camview.diameter = 20

# patched offset table functions
    def offset_table_patch(self):
        self.old_flags = OFFSET_TABLE.flags
        OFFSET_TABLE.flags = self.new_flags

    # we don't allow editing z axis offsets
    def new_flags(self, index):
        if not index.isValid():
            return None
        if index.column() == 9 and index.row() in(0, 1, 2, 3):
            return Qt.ItemIsEnabled
        elif index.row() == 0:
            return Qt.ItemIsEnabled
        elif index.row() == 1 and not index.column() == 2:
            return Qt.NoItemFlags
        elif index.column() == 2:
            return Qt.ItemIsEnabled
        else:
            return Qt.ItemIsEditable | Qt.ItemIsEnabled | Qt.ItemIsSelectable

# patched qt5_graphics functions
    def qt5_graphics_patch(self):
        self.old_dro_format = DRO.dro_format
        DRO.dro_format = self.new_dro_format

    # replace dro with current material
    def new_dro_format(self,s,spd,dtg,limit,homed,positions,axisdtg,g5x_offset,g92_offset,tlo_offset):
        text = self.get_overlay_text()
        return limit, homed, text, text

# patched screen options functions
    def screen_options_patch(self):
        self.old_process_error = OPTIONS.process_error
        OPTIONS.process_error = self.new_process_error

    # we want custom notifications for jog errors
    def new_process_error(self, w, kind, text):
        self.realTimeDelay = False
        O = self.w.screen_options
        N = O.QTVCP_INSTANCE_._NOTICE
        if 'jog-inhibit' in text:
            if self.w.led_float_switch.hal_pin.get():
                text = _translate('HandlerClass', 'Float Switch has disabled jogging')
            elif self.ohmicLedInPin.get():
                text = _translate('HandlerClass', 'Ohmic Probe has disabled jogging')
            elif self.w.led_breakaway_switch.hal_pin.get():
                text = _translate('HandlerClass', 'Breakaway Switch has disabled jogging')
        elif 'jog-stop' in text or 'jog-stop-immediate' in text:
            if self.w.led_float_switch.hal_pin.get():
                text = _translate('HandlerClass', 'Float Switch has aborted active jogging')
            elif self.ohmicLedInPin.get():
                text = _translate('HandlerClass', 'Ohmic Probe has aborted active jogging')
            elif self.w.led_breakaway_switch.hal_pin.get():
                text = _translate('HandlerClass', 'Breakaway Switch has aborted active jogging')
        elif self.virtualMachine and 'unexpected realtime delay' in text.lower():
            text = 'Error suppressed:\n"{}"\nRealtime delays are expected in a virtual environment'.format(text.strip())
            self.realTimeDelay = True
        if O.desktop_notify:
            if 'on limit switch error' in text:
                N.update(O.notify_hard_limits, title='Machine Error:', message=text, msgs=O.notify_max_msgs)
            elif kind == linuxcnc.OPERATOR_ERROR and not self.realTimeDelay:
                N.update(O.notify_critical, title='Operator Error:', message=text, msgs=O.notify_max_msgs)
            elif kind == linuxcnc.OPERATOR_TEXT:
                N.update(O.notify_critical, title='Operator Text:', message=text, msgs=O.notify_max_msgs)
            elif kind == linuxcnc.OPERATOR_DISPLAY:
                N.update(O.notify_critical, title='Operator Display:', message=text, msgs=O.notify_max_msgs)
            elif kind == linuxcnc.NML_ERROR:
                N.update(O.notify_critical, title='Internal NML Error:', message=text, msgs=O.notify_max_msgs)
            elif kind == linuxcnc.NML_TEXT:
                N.update(O.notify_critical, title='Internal NML Text:', message=text, msgs=O.notify_max_msgs)
            elif kind == linuxcnc.NML_DISPLAY:
                N.update(O.notify_critical, title='Internal NML Display:', message=text, msgs=O.notify_max_msgs)
            elif kind == STATUS.TEMPARARY_MESSAGE:
                N.update(O.notify_normal,
                              title='Operator Info:',
                              message=text,
                              status_timeout=0,
                              timeout=2,
                              msgs=O.notify_max_msgs)
        if O.play_sounds and O.mchnMsg_play_sound:
            STATUS.emit('play-sound', '%s' % O.mchnMsg_sound_type)
            if O.mchnMsg_speak_errors:
                if kind in (linuxcnc.OPERATOR_ERROR, linuxcnc.NML_ERROR):
                    STATUS.emit('play-sound', 'SPEAK %s ' % text)
            if O.mchnMsg_speak_text:
                if kind in (linuxcnc.OPERATOR_TEXT, linuxcnc.NML_TEXT,
                            linuxcnc.OPERATOR_DISPLAY, STATUS.TEMPARARY_MESSAGE):
                    STATUS.emit('play-sound', 'SPEAK %s ' % text)
        STATUS.emit('update-machine-log', text, 'TIME')


#########################################################################################################################
# SPECIAL FUNCTIONS SECTION #
#########################################################################################################################
    def make_hal_pins(self):
        self.colorFgPin = self.h.newpin('color_fg', hal.HAL_S32, hal.HAL_OUT)
        self.colorBgPin = self.h.newpin('color_bg', hal.HAL_S32, hal.HAL_OUT)
        self.colorBgAltPin = self.h.newpin('color_bgalt', hal.HAL_S32, hal.HAL_OUT)
        self.consChangePin = self.h.newpin('consumable_changing', hal.HAL_BIT, hal.HAL_IN)
        self.convBlockLoaded = self.h.newpin('conv_block_loaded', hal.HAL_BIT, hal.HAL_IN)
        self.convTabDisable = self.h.newpin('conv_disable', hal.HAL_BIT, hal.HAL_IN)
        self.cutTypePin = self.h.newpin('cut_type', hal.HAL_S32, hal.HAL_IN)
        self.developmentPin = self.h.newpin('development', hal.HAL_BIT, hal.HAL_IN)
        self.extAbortPin = self.h.newpin('ext_abort', hal.HAL_BIT, hal.HAL_IN)
        self.extAutoVoltsEnablePin = self.h.newpin('ext_auto_volts_enable', hal.HAL_BIT, hal.HAL_IN)
        self.extChangeConsPin = self.h.newpin('ext_consumables', hal.HAL_BIT, hal.HAL_IN)
        self.extCornerLockEnablePin = self.h.newpin('ext_cornerlock_enable', hal.HAL_BIT, hal.HAL_IN)
        self.extCutRecCancelPin = self.h.newpin('ext_cutrec_cancel', hal.HAL_BIT, hal.HAL_IN)
        self.extCutRecRevPin = self.h.newpin('ext_cutrec_rev', hal.HAL_BIT, hal.HAL_IN)
        self.extCutRecFwdPin = self.h.newpin('ext_cutrec_fwd', hal.HAL_BIT, hal.HAL_IN)
        self.extCutRecNPin = self.h.newpin('ext_cutrec_n', hal.HAL_BIT, hal.HAL_IN)
        self.extCutRecNEPin = self.h.newpin('ext_cutrec_ne', hal.HAL_BIT, hal.HAL_IN)
        self.extCutRecEPin = self.h.newpin('ext_cutrec_e', hal.HAL_BIT, hal.HAL_IN)
        self.extCutRecSEPin = self.h.newpin('ext_cutrec_se', hal.HAL_BIT, hal.HAL_IN)
        self.extCutRecSPin = self.h.newpin('ext_cutrec_s', hal.HAL_BIT, hal.HAL_IN)
        self.extCutRecSWPin = self.h.newpin('ext_cutrec_sw', hal.HAL_BIT, hal.HAL_IN)
        self.extCutRecWPin = self.h.newpin('ext_cutrec_w', hal.HAL_BIT, hal.HAL_IN)
        self.extCutRecNWPin = self.h.newpin('ext_cutrec_nw', hal.HAL_BIT, hal.HAL_IN)
        self.extFramingPin = self.h.newpin('ext_frame_job', hal.HAL_BIT, hal.HAL_IN)
        self.extHeightOvrCountsPin = self.h.newpin('ext_height_ovr_counts', hal.HAL_S32, hal.HAL_IN)
        self.extHeightOvrCountEnablePin = self.h.newpin('ext_height_ovr_count_enable', hal.HAL_BIT, hal.HAL_IN)
        self.extHeightOvrMinusPin = self.h.newpin('ext_height_ovr_minus', hal.HAL_BIT, hal.HAL_IN)
        self.extHeightOvrPlusPin = self.h.newpin('ext_height_ovr_plus', hal.HAL_BIT, hal.HAL_IN)
        self.extHeightOvrResetPin = self.h.newpin('ext_height_ovr_reset', hal.HAL_BIT, hal.HAL_IN)
        self.extHeightOvrScalePin = self.h.newpin('ext_height_ovr_scale', hal.HAL_FLOAT, hal.HAL_IN)
        self.extIgnoreArcOkPin = self.h.newpin('ext_ignore_arc_ok', hal.HAL_BIT, hal.HAL_IN)
        self.extJogSlowPin = self.h.newpin('ext_jog_slow', hal.HAL_BIT, hal.HAL_IN)
#        self.extKerfCrossEnablePin = self.h.newpin('ext_kerfcross_enable', hal.HAL_BIT, hal.HAL_IN)
        self.extLaserTouchOffPin = self.h.newpin('ext_laser_touchoff', hal.HAL_BIT, hal.HAL_IN)
        self.extMeshModePin = self.h.newpin('ext_mesh_mode', hal.HAL_BIT, hal.HAL_IN)
        self.extOhmicPin = self.h.newpin('ext_ohmic', hal.HAL_BIT, hal.HAL_IN)
        self.extOhmicProbeEnablePin = self.h.newpin('ext_ohmic_probe_enable', hal.HAL_BIT, hal.HAL_IN)
        self.extPausePin = self.h.newpin('ext_pause', hal.HAL_BIT, hal.HAL_IN)
        self.extPowerPin = self.h.newpin('ext_power', hal.HAL_BIT, hal.HAL_IN)
        self.extProbePin = self.h.newpin('ext_probe', hal.HAL_BIT, hal.HAL_IN)
        self.extPulsePin = self.h.newpin('ext_pulse', hal.HAL_BIT, hal.HAL_IN)
        self.extRunPausePin = self.h.newpin('ext_run_pause', hal.HAL_BIT, hal.HAL_IN)
        self.extRunPin = self.h.newpin('ext_run', hal.HAL_BIT, hal.HAL_IN)
        self.extThcEnablePin = self.h.newpin('ext_thc_enable', hal.HAL_BIT, hal.HAL_IN)
        self.extTorchEnablePin = self.h.newpin('ext_torch_enable', hal.HAL_BIT, hal.HAL_IN)
        self.extTouchOffPin = self.h.newpin('ext_touchoff', hal.HAL_BIT, hal.HAL_IN)
        self.extVoidLockEnablePin = self.h.newpin('ext_voidlock_enable', hal.HAL_BIT, hal.HAL_IN)
        self.gcodeScalePin = self.h.newpin('gcode_scale', hal.HAL_FLOAT, hal.HAL_OUT)
        self.heightOverridePin = self.h.newpin('height_override', hal.HAL_FLOAT, hal.HAL_OUT)
        self.jogInhibited = self.h.newpin('jog_inhibited', hal.HAL_BIT, hal.HAL_IN)
        self.laserOnPin = self.h.newpin('laser_on', hal.HAL_BIT, hal.HAL_OUT)
        self.offsetSetProbePin = self.h.newpin('offset_set_probe', hal.HAL_BIT, hal.HAL_OUT)
        self.offsetSetScribePin = self.h.newpin('offset_set_scribe', hal.HAL_BIT, hal.HAL_OUT)
        self.laserRecStatePin = self.h.newpin('laser_recovery_state', hal.HAL_S32, hal.HAL_IN)
        self.materialChangePin = self.h.newpin('material_change', hal.HAL_S32, hal.HAL_IN)
        self.materialChangeNumberPin = self.h.newpin('material_change_number', hal.HAL_S32, hal.HAL_IN)
        self.materialChangeTimeoutPin = self.h.newpin('material_change_timeout', hal.HAL_BIT, hal.HAL_IN)
        self.materialReloadPin = self.h.newpin('material_reload', hal.HAL_BIT, hal.HAL_IN)
        self.materialTempPin = self.h.newpin('material_temp', hal.HAL_S32, hal.HAL_IN)
        self.offsetsActivePin = self.h.newpin('offsets_active', hal.HAL_BIT, hal.HAL_IN)
        self.ohmicLedInPin = self.h.newpin('ohmic_led_in', hal.HAL_BIT, hal.HAL_IN)
        self.paramTabDisable = self.h.newpin('param_disable', hal.HAL_BIT, hal.HAL_IN)
        self.settingsTabDisable = self.h.newpin('settings_disable', hal.HAL_BIT, hal.HAL_IN)
        self.plasmacStatePin = self.h.newpin('plasmac_state', hal.HAL_S32, hal.HAL_IN)
        self.pmx485CurrentPin = self.h.newpin('pmx485_current', hal.HAL_FLOAT, hal.HAL_IN)
        self.pmx485CurrentMaxPin = self.h.newpin('pmx485_current_max', hal.HAL_FLOAT, hal.HAL_IN)
        self.pmx485CurrentMinPin = self.h.newpin('pmx485_current_min', hal.HAL_FLOAT, hal.HAL_IN)
        self.pmx485FaultPin = self.h.newpin('pmx485_fault', hal.HAL_FLOAT, hal.HAL_IN)
        self.pmx485ModePin = self.h.newpin('pmx485_mode', hal.HAL_FLOAT, hal.HAL_IN)
        self.pmx485PressurePin = self.h.newpin('pmx485_pressure', hal.HAL_FLOAT, hal.HAL_IN)
        self.pmx485PressureMaxPin = self.h.newpin('pmx485_pressure_max', hal.HAL_FLOAT, hal.HAL_IN)
        self.pmx485PressureMinPin = self.h.newpin('pmx485_pressure_min', hal.HAL_FLOAT, hal.HAL_IN)
        self.pmx485StatusPin = self.h.newpin('pmx485_status', hal.HAL_BIT, hal.HAL_IN)
        self.pmx485ArcTimePin = self.h.newpin('pmx485_arc_time', hal.HAL_FLOAT, hal.HAL_IN)
        self.probeTestErrorPin = self.h.newpin('probe_test_error', hal.HAL_BIT, hal.HAL_IN)
        self.out0Pin = self.h.newpin('ext_out_0', hal.HAL_BIT, hal.HAL_OUT)
        self.out1Pin = self.h.newpin('ext_out_1', hal.HAL_BIT, hal.HAL_OUT)
        self.out2Pin = self.h.newpin('ext_out_2', hal.HAL_BIT, hal.HAL_OUT)
        self.sensorActive = self.h.newpin('sensor_active', hal.HAL_BIT, hal.HAL_IN)
        self.tabsAlwaysEnabled = self.h.newpin('tabs_always_enabled', hal.HAL_BIT, hal.HAL_IN)
        self.thcFeedRatePin = self.h.newpin('thc_feed_rate', hal.HAL_FLOAT, hal.HAL_OUT)
        self.xOffsetPin = self.h.newpin('x_offset', hal.HAL_FLOAT, hal.HAL_IN)
        self.yOffsetPin = self.h.newpin('y_offset', hal.HAL_FLOAT, hal.HAL_IN)
        self.zHeightPin = self.h.newpin('z_height', hal.HAL_FLOAT, hal.HAL_IN)
        self.zOffsetPin = self.h.newpin('z_offset_counts', hal.HAL_S32, hal.HAL_IN)

    def link_hal_pins(self):
        #arc parameters
        CALL(['halcmd', 'net', 'plasmac:arc-fail-delay', 'qtplasmac.arc_fail_delay-f', 'plasmac.arc-fail-delay'])
        CALL(['halcmd', 'net', 'plasmac:arc-max-starts', 'qtplasmac.arc_max_starts-s', 'plasmac.arc-max-starts'])
        CALL(['halcmd', 'net', 'plasmac:restart-delay', 'qtplasmac.arc_restart_delay-f', 'plasmac.restart-delay'])
        CALL(['halcmd', 'net', 'plasmac:arc-voltage-scale', 'qtplasmac.arc_voltage_scale-f', 'plasmac.arc-voltage-scale'])
        CALL(['halcmd', 'net', 'plasmac:arc-voltage-offset', 'qtplasmac.arc_voltage_offset-f', 'plasmac.arc-voltage-offset'])
        CALL(['halcmd', 'net', 'plasmac:height-per-volt', 'qtplasmac.height_per_volt-f', 'plasmac.height-per-volt'])
        CALL(['halcmd', 'net', 'plasmac:arc-ok-high', 'qtplasmac.arc_ok_high-f', 'plasmac.arc-ok-high'])
        CALL(['halcmd', 'net', 'plasmac:arc-ok-low', 'qtplasmac.arc_ok_low-f', 'plasmac.arc-ok-low'])
        #thc parameters
        CALL(['halcmd', 'net', 'plasmac:thc-feed-rate', 'qtplasmac.thc_feed_rate', 'plasmac.thc-feed-rate'])
        CALL(['halcmd', 'net', 'plasmac:thc-auto', 'qtplasmac.thc_auto', 'plasmac.thc-auto'])
        CALL(['halcmd', 'net', 'plasmac:thc-delay', 'qtplasmac.thc_delay-f', 'plasmac.thc-delay'])
        CALL(['halcmd', 'net', 'plasmac:thc-sample-counts', 'qtplasmac.thc_sample_counts-s', 'plasmac.thc-sample-counts'])
        CALL(['halcmd', 'net', 'plasmac:thc-sample-threshold', 'qtplasmac.thc_sample_threshold-f', 'plasmac.thc-sample-threshold'])
        CALL(['halcmd', 'net', 'plasmac:thc-threshold', 'qtplasmac.thc_threshold-f', 'plasmac.thc-threshold'])
        CALL(['halcmd', 'net', 'plasmac:pid-p-gain', 'qtplasmac.pid_p_gain-f', 'plasmac.pid-p-gain'])
        CALL(['halcmd', 'net', 'plasmac:pid-i-gain', 'qtplasmac.pid_i_gain-f', 'plasmac.pid-i-gain'])
        CALL(['halcmd', 'net', 'plasmac:pid-d-gain', 'qtplasmac.pid_d_gain-f', 'plasmac.pid-d-gain'])
        CALL(['halcmd', 'net', 'plasmac:cornerlock-threshold', 'qtplasmac.cornerlock_threshold-f', 'plasmac.cornerlock-threshold'])
        CALL(['halcmd', 'net', 'plasmac:voidlock-slope', 'qtplasmac.voidlock_slope-s', 'plasmac.voidlock-slope'])
        #probe parameters
        CALL(['halcmd', 'net', 'plasmac:float-switch-travel', 'qtplasmac.float_switch_travel-f', 'plasmac.float-switch-travel'])
        CALL(['halcmd', 'net', 'plasmac:probe-feed-rate', 'qtplasmac.probe_feed_rate-f', 'plasmac.probe-feed-rate'])
        CALL(['halcmd', 'net', 'plasmac:probe-start-height', 'qtplasmac.probe_start_height-f', 'plasmac.probe-start-height'])
        CALL(['halcmd', 'net', 'plasmac:ohmic-probe-offset', 'qtplasmac.ohmic_probe_offset-f', 'plasmac.ohmic-probe-offset'])
        CALL(['halcmd', 'net', 'plasmac:ohmic-max-attempts', 'qtplasmac.ohmic_max_attempts-s', 'plasmac.ohmic-max-attempts'])
        CALL(['halcmd', 'net', 'plasmac:skip-ihs-distance', 'qtplasmac.skip_ihs_distance-f', 'plasmac.skip-ihs-distance'])
        #safety parameters
        CALL(['halcmd', 'net', 'plasmac:safe-height', 'qtplasmac.safe_height-f', 'plasmac.safe-height'])
        #scribe parameters
        CALL(['halcmd', 'net', 'plasmac:scribe-arm-delay', 'qtplasmac.scribe_arm_delay-f', 'plasmac.scribe-arm-delay'])
        CALL(['halcmd', 'net', 'plasmac:scribe-on-delay', 'qtplasmac.scribe_on_delay-f', 'plasmac.scribe-on-delay'])
        #spotting parameters
        CALL(['halcmd', 'net', 'plasmac:spotting-threshold', 'qtplasmac.spotting_threshold-f', 'plasmac.spotting-threshold'])
        CALL(['halcmd', 'net', 'plasmac:spotting-time', 'qtplasmac.spotting_time-f', 'plasmac.spotting-time'])
        #motion parameters
        CALL(['halcmd', 'net', 'plasmac:setup-feed-rate', 'qtplasmac.setup_feed_rate-f', 'plasmac.setup-feed-rate'])
        #material parameters
        CALL(['halcmd', 'net', 'plasmac:cut-feed-rate', 'qtplasmac.cut_feed_rate-f', 'plasmac.cut-feed-rate'])
        CALL(['halcmd', 'net', 'plasmac:cut-height', 'qtplasmac.cut_height-f', 'plasmac.cut-height'])
        CALL(['halcmd', 'net', 'plasmac:cut-volts', 'qtplasmac.cut_volts-f', 'plasmac.cut-volts'])
        CALL(['halcmd', 'net', 'plasmac:kerf-width', 'qtplasmac.kerf_width-f', 'plasmac.kerf-width'])
        CALL(['halcmd', 'net', 'plasmac:pause-at-end', 'qtplasmac.pause_at_end-f', 'plasmac.pause-at-end'])
        CALL(['halcmd', 'net', 'plasmac:pierce-delay', 'qtplasmac.pierce_delay-f', 'plasmac.pierce-delay'])
        CALL(['halcmd', 'net', 'plasmac:pierce-height', 'qtplasmac.pierce_height-f', 'plasmac.pierce-height'])
        CALL(['halcmd', 'net', 'plasmac:puddle-jump-delay', 'qtplasmac.puddle_jump_delay-f', 'plasmac.puddle-jump-delay'])
        CALL(['halcmd', 'net', 'plasmac:puddle-jump-height', 'qtplasmac.puddle_jump_height-f', 'plasmac.puddle-jump-height'])
        #monitor
        CALL(['halcmd', 'net', 'plasmac:arc_ok_out', 'plasmac.arc-ok-out', 'qtplasmac.led_arc_ok'])
        CALL(['halcmd', 'net', 'plasmac:arc_voltage_out', 'plasmac.arc-voltage-out', 'qtplasmac.arc_voltage'])
        CALL(['halcmd', 'net', 'plasmac:breakaway-switch-out', 'qtplasmac.led_breakaway_switch'])
        CALL(['halcmd', 'net', 'plasmac:cornerlock-is-locked', 'plasmac.cornerlock-is-locked', 'qtplasmac.led_corner_lock'])
        CALL(['halcmd', 'net', 'plasmac:float-switch-out', 'qtplasmac.led_float_switch'])
        CALL(['halcmd', 'net', 'plasmac:voidlock-is-locked', 'plasmac.voidlock-is-locked', 'qtplasmac.led_void_lock'])
        CALL(['halcmd', 'net', 'plasmac:led-up', 'plasmac.led-up', 'qtplasmac.led_thc_up'])
        CALL(['halcmd', 'net', 'plasmac:led-down', 'plasmac.led-down', 'qtplasmac.led_thc_down'])
        CALL(['halcmd', 'net', 'plasmac:ohmic-probe-out', 'qtplasmac.ohmic_led_in'])
        CALL(['halcmd', 'net', 'plasmac:thc-active', 'plasmac.thc-active', 'qtplasmac.led_thc_active'])
        CALL(['halcmd', 'net', 'plasmac:thc-enabled', 'plasmac.thc-enabled', 'qtplasmac.led_thc_enabled'])
        CALL(['halcmd', 'net', 'plasmac:torch-on', 'qtplasmac.led_torch_on'])
        #control
        CALL(['halcmd', 'net', 'plasmac:cornerlock-enable', 'qtplasmac.cornerlock_enable', 'plasmac.cornerlock-enable'])
        CALL(['halcmd', 'net', 'plasmac:voidlock-enable', 'qtplasmac.voidlock_enable', 'plasmac.voidlock-enable'])
        CALL(['halcmd', 'net', 'plasmac:mesh-enable', 'qtplasmac.mesh_enable', 'plasmac.mesh-enable'])
        CALL(['halcmd', 'net', 'plasmac:ignore-arc-ok-1', 'qtplasmac.ignore_arc_ok', 'plasmac.ignore-arc-ok-1'])
        CALL(['halcmd', 'net', 'plasmac:ohmic-probe-enable', 'qtplasmac.ohmic_probe_enable', 'plasmac.ohmic-probe-enable'])
        CALL(['halcmd', 'net', 'plasmac:thc-enable', 'qtplasmac.thc_enable', 'plasmac.thc-enable'])
        CALL(['halcmd', 'net', 'plasmac:use-auto-volts', 'qtplasmac.use_auto_volts', 'plasmac.use-auto-volts'])
        CALL(['halcmd', 'net', 'plasmac:torch-enable', 'qtplasmac.torch_enable', 'plasmac.torch-enable'])
        #offsets
        CALL(['halcmd', 'net', 'plasmac:x-offset-current', 'qtplasmac.x_offset'])
        CALL(['halcmd', 'net', 'plasmac:y-offset-current', 'qtplasmac.y_offset'])
        CALL(['halcmd', 'net', 'plasmac:offsets-active', 'qtplasmac.offsets_active'])
        #override
        CALL(['halcmd', 'net', 'plasmac:height-override','qtplasmac.height_override','plasmac.height-override'])
        #INI
        CALL(['halcmd', 'net', 'plasmac:axis-x-max-limit', 'ini.x.max_limit', 'plasmac.axis-x-max-limit'])
        CALL(['halcmd', 'net', 'plasmac:axis-x-min-limit', 'ini.x.min_limit', 'plasmac.axis-x-min-limit'])
        CALL(['halcmd', 'net', 'plasmac:axis-y-max-limit', 'ini.y.max_limit', 'plasmac.axis-y-max-limit'])
        CALL(['halcmd', 'net', 'plasmac:axis-y-min-limit', 'ini.y.min_limit', 'plasmac.axis-y-min-limit'])
        CALL(['halcmd', 'net', 'plasmac:axis-z-max-limit', 'ini.z.max_limit', 'plasmac.axis-z-max-limit'])
        CALL(['halcmd', 'net', 'plasmac:axis-z-min-limit', 'ini.z.min_limit', 'plasmac.axis-z-min-limit'])
        # misc
        CALL(['halcmd', 'net', 'plasmac:consumable-changing', 'plasmac.consumable-changing', 'qtplasmac.consumable_changing'])
        CALL(['halcmd', 'net', 'plasmac:gcode-scale', 'plasmac.gcode-scale', 'qtplasmac.gcode_scale'])
        CALL(['halcmd', 'net', 'plasmac:jog-inhibit', 'qtplasmac.jog_inhibited'])
        CALL(['halcmd', 'net', 'plasmac:laser-on', 'qtplasmac.laser_on'])
        CALL(['halcmd', 'net', 'plasmac:laser-recovery-state', 'plasmac.laser-recovery-state', 'qtplasmac.laser_recovery_state'])
        CALL(['halcmd', 'net', 'plasmac:probe-test-error', 'plasmac.probe-test-error', 'qtplasmac.probe_test_error'])
        CALL(['halcmd', 'net', 'plasmac:sensor_active', 'plasmac.sensor-active', 'qtplasmac.sensor_active'])
        CALL(['halcmd', 'net', 'plasmac:state', 'plasmac.state-out', 'qtplasmac.plasmac_state'])
        CALL(['halcmd', 'net', 'plasmac:z-height', 'plasmac.z-height', 'qtplasmac.z_height'])
        CALL(['halcmd', 'net', 'plasmac:z-offset-counts', 'qtplasmac.z_offset_counts'])
        CALL(['halcmd', 'net', 'plasmac:offset-set-probe', 'plasmac.offset-set-probe', 'qtplasmac.offset_set_probe'])
        CALL(['halcmd', 'net', 'plasmac:offset-set-scribe', 'plasmac.offset-set-scribe', 'qtplasmac.offset_set_scribe'])

# *** add system hal pin changes here that may affect existing configs ***
# *** these may be removed after auto updating is implemented          ***
        if not hal.pin_has_writer('plasmac.feed-upm'): # if feed-upm is not yet connected in hal
            CALL(['halcmd', 'net', 'plasmac:feed-upm', 'motion.feed-upm', 'plasmac.feed-upm'])

    def init_preferences(self):
        self.mode = self.PREFS.getpref('Mode', 0, int,'GUI_OPTIONS')
        self.autorepeat_skip = self.PREFS.getpref('Autorepeat all', False, bool, 'GUI_OPTIONS')
        self.flash_error = self.PREFS.getpref('Flash error', False, bool,'GUI_OPTIONS')
        self.w.chk_keyboard_shortcuts.setChecked(self.PREFS.getpref('Use keyboard shortcuts', False, bool, 'GUI_OPTIONS'))
        self.w.chk_soft_keyboard.setChecked(self.PREFS.getpref('Use soft keyboard', False, bool, 'GUI_OPTIONS'))
        self.w.chk_overlay.setChecked(self.PREFS.getpref('Show materials', True, bool, 'GUI_OPTIONS'))
        self.w.chk_run_from_line.setChecked(self.PREFS.getpref('Run from line', True, bool, 'GUI_OPTIONS'))
        self.w.chk_tool_tips.setChecked(self.PREFS.getpref('Tool tips', True, bool, 'GUI_OPTIONS'))
        self.w.chk_exit_warning.setChecked(self.PREFS.getpref('Exit warning', True, bool, 'GUI_OPTIONS'))
        self.exitMessage = self.PREFS.getpref('Exit warning text', '', str, 'GUI_OPTIONS')
        self.w.cone_size.setValue(self.PREFS.getpref('Preview cone size', 0.5, float, 'GUI_OPTIONS'))
        self.w.grid_size.setValue(self.PREFS.getpref('Preview grid size', 0, float, 'GUI_OPTIONS'))
        self.w.table_zoom_scale.setValue(self.PREFS.getpref('T view zoom scale', 1, float, 'GUI_OPTIONS'))
        self.zPlusOverrideJog = self.PREFS.getpref('Override jog inhibit via Z+', False, bool, 'GUI_OPTIONS')
        self.w.color_foregrnd.setStyleSheet('background-color: {}'.format(self.PREFS.getpref('Foreground', '#ffee06', str, 'COLOR_OPTIONS')))
        self.w.color_foregalt.setStyleSheet('background-color: {}'.format(self.PREFS.getpref('Highlight', '#ffee06', str, 'COLOR_OPTIONS')))
        self.w.color_led.setStyleSheet('background-color: {}'.format(self.PREFS.getpref('LED', '#ffee06', str, 'COLOR_OPTIONS')))
        self.w.color_backgrnd.setStyleSheet('background-color: {}'.format(self.PREFS.getpref('Background', '#16160e', str, 'COLOR_OPTIONS')))
        self.w.color_backgalt.setStyleSheet('background-color: {}'.format(self.PREFS.getpref('Background Alt', '#36362e', str, 'COLOR_OPTIONS')))
        self.w.color_frams.setStyleSheet('background-color: {}'.format(self.PREFS.getpref('Frames', '#ffee06', str, 'COLOR_OPTIONS')))
        self.w.color_estop.setStyleSheet('background-color: {}'.format(self.PREFS.getpref('Estop', '#ff0000', str, 'COLOR_OPTIONS')))
        self.w.color_disabled.setStyleSheet('background-color: {}'.format(self.PREFS.getpref('Disabled', '#b0b0b0', str, 'COLOR_OPTIONS')))
        self.w.color_preview.setStyleSheet('background-color: {}'.format(self.PREFS.getpref('Preview', '#000000', str, 'COLOR_OPTIONS')))
        self.lastLoadedProgram = self.w.PREFS_.getpref('RecentPath_0', 'None', str,'BOOK_KEEPING')
        TOOLTIPS.tool_tips_changed(self.w)
        self.soft_keyboard()
        self.cone_size_changed(self.w.cone_size.value())
        self.grid_size_changed(self.w.grid_size.value())
        self.set_basic_colors()
        self.w.sd_text.setText(self.exitMessage)

    def hide_widgets(self):
        for b in ['RUN', 'PAUSE', 'ABORT']:
            if self.PREFS.getpref('Hide {}'.format(b.lower()), False, bool,'GUI_OPTIONS'):
                self.w[b.lower()].hide()
                if self.landscape:
                    self.w.machine_frame.setMaximumHeight(self.w.machine_frame.maximumHeight() - 44)
                    self.w.machine_frame.setMinimumHeight(self.w.machine_frame.maximumHeight())

    def init_widgets(self):
        droPos = self.PREFS.getpref('DRO position', 'bottom', str, 'GUI_OPTIONS')
        if droPos.lower() == 'top':
            #designer can change the layout name in the .ui file
            #this will find the name and move the dro to the top
            lay = self.w.dro_gcode_frame.children()[0].objectName()
            self.w[lay].removeWidget(self.w.dro_frame)
            self.w[lay].insertWidget(0,self.w.dro_frame)
        text = _translate('HandlerClass', 'CONTINUOUS')
        self.w.jogincrements.setItemText(0, text)
        self.w.main_tab_widget.setCurrentIndex(0)
        self.w.preview_stack.setCurrentIndex(0)
        self.w.gcode_stack.setCurrentIndex(0)
        self.w.jog_stack.setCurrentIndex(0)
        self.w.chk_override_limits.setChecked(False)
        self.w.chk_override_limits.setEnabled(False)
        self.w.chk_override_jog.setChecked(False)
        self.w.chk_override_jog.setEnabled(False)
        self.w.thc_enable.setChecked(self.PREFS.getpref('THC enable', True, bool, 'ENABLE_OPTIONS'))
        self.w.cornerlock_enable.setChecked(self.PREFS.getpref('Corner lock enable', True, bool, 'ENABLE_OPTIONS'))
        self.w.voidlock_enable.setChecked(self.PREFS.getpref('Void lock enable', False, bool, 'ENABLE_OPTIONS'))
        self.w.use_auto_volts.setChecked(self.PREFS.getpref('Use auto volts', True, bool, 'ENABLE_OPTIONS'))
        self.w.ohmic_probe_enable.setChecked(self.PREFS.getpref('Ohmic probe enable', False, bool, 'ENABLE_OPTIONS'))
        self.w.thc_auto.setChecked(self.PREFS.getpref('THC auto', False, bool, 'ENABLE_OPTIONS'))
        self.thc_auto_changed(self.w.thc_auto.isChecked())
        self.w.error_label = QLabel()
        self.w.tool_label = STATLABEL()
        self.w.gcodes_label = STATLABEL()
        self.w.mcodes_label = STATLABEL()
        self.w.lbl_tool = STATLABEL()
        self.w.lbl_gcodes = STATLABEL()
        self.w.lbl_mcodes = STATLABEL()
        self.w.statusbar.addPermanentWidget(self.w.error_label, stretch=1)
        self.w.error_label.setObjectName('error_label')
        self.w.statusbar.addPermanentWidget(VLine())
        self.w.statusbar.addPermanentWidget(self.w.tool_label)
        self.w.tool_label.setObjectName('tool_label')
        self.w.tool_label.setText('TOOL: ')
        self.w.statusbar.addPermanentWidget(self.w.lbl_tool)
        self.w.lbl_tool.setObjectName('lbl_tool')
        self.w.statusbar.addPermanentWidget(VLine())
        self.w.statusbar.addPermanentWidget(self.w.gcodes_label)
        self.w.gcodes_label.setObjectName('gcodes_label')
        self.w.gcodes_label.setText('G-CODES: ')
        self.w.statusbar.addPermanentWidget(self.w.lbl_gcodes)
        self.w.lbl_gcodes.setObjectName('lbl_gcodes')
        self.w.statusbar.addPermanentWidget(VLine())
        self.w.statusbar.addPermanentWidget(self.w.mcodes_label)
        self.w.mcodes_label.setObjectName('mcodes_label')
        self.w.mcodes_label.setText('M-CODES: ')
        self.w.statusbar.addPermanentWidget(self.w.lbl_mcodes)
        self.w.lbl_mcodes.setObjectName('lbl_mcodes')
        text = _translate('HandlerClass', 'MOVE')
        self.w.cut_rec_move_label.setText('{}\n{}'.format(text, self.w.kerf_width.text()))
        self.w.filemanager.windowLayout.setContentsMargins(4,0,4,0)
        self.w.filemanager.windowLayout.setSpacing(4)
        # hide load button (redundant to select button)
        self.w.filemanager.loadButton.hide()
        # for copy/paste control if required
        self.w.filemanager.copy_control.hide()
        # add vertical and horizontal scroll bars to the materials QComboBoxs 
        self.w.material_selector.view().setVerticalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        self.w.material_selector.view().setHorizontalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        self.w.conv_material.view().setVerticalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        self.w.conv_material.view().setHorizontalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        self.w.materials_box.view().setVerticalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        self.w.materials_box.view().setHorizontalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        self.w.gcode_display.set_margin_width(3)
        self.w.gcode_display.setBraceMatching(False)
        self.w.gcode_display.setCaretWidth(0)
        self.w.gcode_display.setCornerWidget(QLabel())
        self.w.gcode_editor.topBox.setContentsMargins(4,4,4,4)
        self.w.gcode_editor.bottomBox.setContentsMargins(4,4,4,4)
        self.w.gcode_editor.set_margin_width(3)
        self.w.gcode_editor.editor.setBraceMatching(False)
        self.w.gcode_editor.editor.setCaretWidth(4)
        self.w.gcode_editor.editor.setCornerWidget(QLabel())
        self.w.gcode_editor.editMode()
        self.w.gcode_editor.pythonLexerAction.setVisible(False)
        self.w.gcode_editor.gCodeLexerAction.setVisible(False)
        self.w.gcode_editor.label.setText('')
        self.w.gcodegraphics.enable_dro = True
        self.w.gcodegraphics.set_alpha_mode(True)
        self.w.gcodegraphics.setShowOffsets(False)
        self.w.gcodegraphics._font = 'monospace 11'
        self.w.conv_preview.enable_dro = True
        self.w.conv_preview.set_cone_basesize(0.1)
        self.w.conv_preview.set_view('Z')
        self.w.conv_preview.show_tool = False
        self.w.conv_preview.show_limits = False
        self.w.conv_preview.show_small_origin = False
        self.w.conv_preview.set_alpha_mode(True)
        self.w.conv_preview.setShowOffsets(False)
        self.w.conv_preview._font = 'monospace 11'
        self.w.conv_preview.inhibit_selection = True
        self.w.conv_preview.updateGL()
        self.w.conv_preview.setInhibitControls(True)
        self.w.estopButton = self.PREFS.getpref('Estop type', 0, int, 'GUI_OPTIONS')
        if self.w.estopButton == 0:
            self.w.estop.setEnabled(False)
        if self.w.estopButton == 1:
            self.w.estop.hide()
        self.w.camview.cross_color = QtCore.Qt.red
        self.w.camview.cross_pointer_color = QtCore.Qt.red
        self.w.camview.font = QFont('arial,helvetica', 16)
        self.flashState = False
        self.flashRate = 4
        self.flasher = self.flashRate
        self.manualCut = False
        self.jogPreManCut = [False, INFO.DEFAULT_LINEAR_JOG_VEL, 0]
        self.probeTest = False
        self.torchPulse = False
        self.rflSelected = False
        self.fileOpened = False
        self.fileClear = False
        self.error_present = False
        self.laserButtonState = 'laser'
        self.camButtonState = 'markedge'
        self.overlayProgress = QProgressBar(self.w.gcode_display)
        self.overlayProgress.setOrientation(Qt.Vertical)
        self.overlayProgress.setInvertedAppearance(True)
        self.overlayProgress.setFormat('')
        self.overlayProgress.hide()
        self.camNum = self.PREFS.getpref('Camera port', 0, int, 'CAMERA_OFFSET')

    def get_overlay_text(self):
        text = ['']
        if not self.w.chk_overlay.isChecked():
            return text
        scale = 1
        if self.units == 'in' and self.w.dro_z.display_units_mm:
            scale = 25.4
        elif self.units == 'mm' and not self.w.dro_z.display_units_mm:
            scale = 1 / 25.4
        if not self.w.dro_z.display_units_mm:
            fr = 1
            ou = 3
        else:
            fr = 0
            ou = 2
        if '.' in self.w.cut_feed_rate.text() and len(self.w.cut_feed_rate.text().split('.')[0]) > 3:
            text.append('FR: {:.{}f}'.format(float(self.w.cut_feed_rate.text().split('.')[0]) * scale, fr))
        else:
            text.append('FR: {:.{}f}'.format(float(self.w.cut_feed_rate.text()) * scale, fr))
        text.append('PH: {:.{}f}'.format(float(self.w.pierce_height.text()) * scale, ou))
        text.append('PD: {}'.format(self.w.pierce_delay.text()))
        text.append('CH: {:.{}f}'.format(float(self.w.cut_height.text()) * scale, ou))
        text.append('KW: {:.{}f}'.format(float(self.w.kerf_width.text()) * scale, ou))
        if self.pmx485Exists:
            text.append('CA: {}'.format(self.w.cut_amps.text()))
        return text

    def offset_peripherals(self):
        self.camOffsetX = 0.0
        self.camOffsetY = 0.0
        self.laserOffsetX = 0.0
        self.laserOffsetY = 0.0
        self.probeOffsetX = 0.0
        self.probeOffsetY = 0.0
        self.probeDelay = 0.0
        head = _translate('HandlerClass', 'Prefs File Error')
        # laser
        try:
            self.laserOffsetX = self.PREFS.getpref('X axis', 0, float, 'LASER_OFFSET')
            self.laserOffsetY = self.PREFS.getpref('Y axis', 0, float, 'LASER_OFFSET')
        except:
            self.w.laser.hide()
            msg0 = _translate('HandlerClass', 'Invalid entry for laser offset')
            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n'.format(head, msg0))
        if self.laserOffsetX or self.laserOffsetY:
            self.idleHomedList.append('laser')
            self.w.laser.setEnabled(False)
        else:
            self.w.laser.hide()
        # camera
        try:
            self.camOffsetX = self.PREFS.getpref('X axis', 0, float, 'CAMERA_OFFSET')
            self.camOffsetY = self.PREFS.getpref('Y axis', 0, float, 'CAMERA_OFFSET')
        except:
            self.w.camera.hide()
            msg0 = _translate('HandlerClass', 'Invalid entry for camera offset')
            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n'.format(head, msg0))
        if self.camOffsetX or self.camOffsetY:
            self.w.camview.set_camnum(self.camNum)
            self.idleHomedList.append('camera')
            self.w.camera.setEnabled(False)
        else:
            self.w.camera.hide()
        # probing
        try:
            self.probeOffsetX = self.PREFS.getpref('X axis', 0, float, 'OFFSET_PROBING')
            self.probeOffsetY = self.PREFS.getpref('Y axis', 0, float, 'OFFSET_PROBING')
            self.probeDelay = self.PREFS.getpref('Delay', 0, float, 'OFFSET_PROBING')
        except:
            msg0 = _translate('HandlerClass', 'Invalid entry for probe offset')
            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n'.format(head, msg0))

# called by the modified closeEvent function in this handler
    def closing_cleanup__(self):
        # disconnect powermax
        self.w.pmx485_enable.setChecked(False)
        # close soft keyboard
        self.vkb_hide()
        # turn autorepeat back on for the OS
        self.autorepeat_keys(True)
        # save the log files
        self.save_logfile(5)
        # save preferences
        if not self.PREFS: return
        self.PREFS.putpref('Use keyboard shortcuts', self.w.chk_keyboard_shortcuts.isChecked(), bool, 'GUI_OPTIONS')
        self.PREFS.putpref('Use soft keyboard', self.w.chk_soft_keyboard.isChecked(), bool, 'GUI_OPTIONS')
        self.PREFS.putpref('Show materials', self.w.chk_overlay.isChecked(), bool, 'GUI_OPTIONS')
        self.PREFS.putpref('Run from line', self.w.chk_run_from_line.isChecked(), bool, 'GUI_OPTIONS')
        self.PREFS.putpref('Tool tips', self.w.chk_tool_tips.isChecked(), bool, 'GUI_OPTIONS')
        self.PREFS.putpref('Exit warning', self.w.chk_exit_warning.isChecked(), bool, 'GUI_OPTIONS')
        self.PREFS.putpref('Exit warning text', self.exitMessage, str, 'GUI_OPTIONS')
        self.PREFS.putpref('Preview cone size', self.w.cone_size.value(), float, 'GUI_OPTIONS')
        self.PREFS.putpref('Preview grid size', self.w.grid_size.value(), float, 'GUI_OPTIONS')
        self.PREFS.putpref('T view zoom scale', self.w.table_zoom_scale.value(), float, 'GUI_OPTIONS')
        self.PREFS.putpref('THC auto', self.w.thc_auto.isChecked(), bool, 'ENABLE_OPTIONS')
        self.PREFS.putpref('Override jog inhibit via Z+', self.zPlusOverrideJog, bool, 'GUI_OPTIONS')
        self.PREFS.putpref('THC enable', self.w.thc_enable.isChecked(), bool, 'ENABLE_OPTIONS')
        self.PREFS.putpref('Corner lock enable', self.w.cornerlock_enable.isChecked(), bool, 'ENABLE_OPTIONS')
        self.PREFS.putpref('Void lock enable', self.w.voidlock_enable.isChecked(), bool, 'ENABLE_OPTIONS')
        self.PREFS.putpref('Use auto volts', self.w.use_auto_volts.isChecked(), bool, 'ENABLE_OPTIONS')
        self.PREFS.putpref('Ohmic probe enable', self.w.ohmic_probe_enable.isChecked(), bool, 'ENABLE_OPTIONS')

    def save_logfile(self, numLogs):
            logPre = 'machine_log_'
            logFiles = []
            logFiles = [f for f in os.listdir(self.PATHS.CONFIGPATH) if f.startswith(logPre)]
            logFiles.sort()
            if len(logFiles) > (numLogs - 1):
                for f in range(0, len(logFiles) - (numLogs - 1)):
                    os.remove(logFiles[0])
                    logFiles = logFiles[1:]
            text = self.w.machinelog.toPlainText()
            logName = '{}/{}{}.txt'.format(self.PATHS.CONFIGPATH, logPre, time.strftime('%y-%m-%d_%H-%M-%S'))
            with open(logName, 'w') as f:
                f.write(text)

# called by qt_makegui.py
    def processed_key_event__(self,receiver,event,is_pressed,key,code,shift,cntrl):
        # when typing in MDI, we don't want keybinding to call functions
        # so we catch and process the events directly.
        # We do want ESC, F1 and F2 to call keybinding functions though
        if code not in(Qt.Key_Escape, Qt.Key_F1, Qt.Key_F2):
            # search for the top widget of whatever widget received the event
            # then check if it's one we want the keypress events to go to
            flag = False
            conversational = False
            mdiBlank = False
            receiver2 = receiver
            while receiver2 is not None and not flag:
                if isinstance(receiver2, QtWidgets.QDialog):
                    flag = True
                    break
                if isinstance(receiver2, QtWidgets.QListView):
                    flag = True
                    break
                if isinstance(receiver2, FILE_MAN):
                    flag = True
                    break
                if isinstance(receiver2, MDI_LINE):
                    if self.w.mdihistory.MDILine.text().rstrip() == '':
                        mdiBlank = True
                    flag = True
                    break
                if isinstance(receiver2, MDI_HISTORY):
                    if self.w.mdihistory.MDILine.text().rstrip() == '':
                        mdiBlank = True
                    flag = True
                    break
                if isinstance(receiver2, EDITOR):
                    flag = True
                    break
                if isinstance(receiver2, DOUBLESCALE):
                    flag = True
                    break
                if isinstance(receiver2, OFFSETVIEW):
                    flag = True
                    break
                if self.w.main_tab_widget.currentIndex() == 1 and \
                   (isinstance(receiver2, QtWidgets.QLineEdit) or \
                   isinstance(receiver2, QtWidgets.QComboBox) or \
                   isinstance(receiver2, QtWidgets.QPushButton) or \
                   isinstance(receiver2, QtWidgets.QRadioButton)):
                    conversational = True
                    flag = True
                    break
                if self.w.main_tab_widget.currentIndex() == 3:
                    flag = True
                    break
                receiver2 = receiver2.parent()
            if flag:
                if is_pressed:
                    if conversational and (code == Qt.Key_Tab or code == Qt.Key_BackTab):
                        self.keyPressEvent(event)
                    else:
                        receiver.keyPressEvent(event)
                    if mdiBlank and (code == Qt.Key_Return or code == Qt.Key_Enter):
                        self.keyPressEvent(event)
                    event.accept()
                    return True
                else:
                    event.accept()
                    return True
        if event.isAutoRepeat():
            return True
        return KEYBIND.manage_function_calls(self, event, is_pressed, key, shift, cntrl)

    def size_changed(self, object):
        bHeight = self.w.file_edit.height() + 4
        rows = int((object.height() - 24) / bHeight)
        if self.landscape:
            if rows == 4:
                self.buttons_hide(7, 20)
            elif rows == 5:
                self.buttons_hide(9, 20)
                self.buttons_show(7, 8)
            elif rows == 6:
                self.buttons_hide(11, 20)
                self.buttons_show(7, 10)
            elif rows == 7:
                self.buttons_hide(13, 20)
                self.buttons_show(7, 12)
            elif rows == 8:
                self.buttons_hide(15, 20)
                self.buttons_show(7, 14)
            elif rows == 9:
                self.buttons_hide(17, 20)
                self.buttons_show(7, 16)
            elif rows == 10:
                self.buttons_hide(19, 20)
                self.buttons_show(7, 18)
            else:
                self.buttons_show(7, 20)
        else:
            if rows == 15:
                self.buttons_hide(16, 20)
                self.buttons_show(9, 15)
            if rows == 16:
                self.buttons_hide(17, 20)
                self.buttons_show(9, 16)
            elif rows == 17:
                self.buttons_hide(18, 20)
                self.buttons_show(9, 17)
            elif rows == 18:
                self.buttons_hide(19, 20)
                self.buttons_show(9, 18)
            elif rows == 19:
                self.buttons_hide(19, 20)
                self.buttons_show(9, 19)
            else:
                self.buttons_show(9, 20)

    def buttons_hide(self, first, last):
        for b in range(first, last + 1):
            self.w['button_{}'.format(b)].hide()

    def buttons_show(self, first, last):
        for b in range(first, last + 1):
            self.w['button_{}'.format(b)].show()


#########################################################################################################################
# CALLBACKS FROM STATUS #
#########################################################################################################################
    def estop_state(self, state):
        if state:
            self.w.power.setChecked(False)
            self.w.power.setStyleSheet(' \
                    QPushButton {{ color: {2}; background: {1}; border-color: {2} }} \
                    QPushButton:pressed {{ color: {2}; background: {1}; border-color: {0} }}' \
                    .format(self.foreColor, self.backColor, self.disabledColor))
            if not self.firstRun:
                log = _translate('HandlerClass', 'Emergency stop pressed')
                STATUS.emit('update-machine-log', log, 'TIME')
        else:
            self.w.power.setStyleSheet(' \
                    QPushButton {{ color: {0}; background: {1}; border-color: {0} }} \
                    QPushButton:pressed {{ color: {1}; background: {3}; border-color: {0} }} \
                    QPushButton:checked {{ color: {1}; background: {3}; border-color: {0} }} \
                    QPushButton:checked:pressed {{ color: {0}; background: {1}; border-color: {0} }}' \
                    .format(self.foreColor, self.backColor, self.disabledColor, self.fore1Color))
            if not self.firstRun:
                log = _translate('HandlerClass', 'Emergency stop cleared')
                STATUS.emit('update-machine-log', log, 'TIME')

    def power_state(self, state):
        if state:
            self.w.power.setChecked(True)
            self.set_buttons_state([self.idleOnList], True)
            if self.tpButton and not self.w.torch_enable.isChecked():
                self.w[self.tpButton].setEnabled(False)
            if self.otButton and not self.w.ohmic_probe_enable.isChecked():
                self.w[self.otButton].setEnabled(False)
            if STATUS.is_all_homed():
                self.set_buttons_state([self.idleHomedList], True)
                if self.convBlockLoaded.get():
                    self.wcs_rotation('set')
            else:
                self.set_buttons_state([self.idleHomedList], False)
            if not self.firstRun:
                log = _translate('HandlerClass', 'GUI power on')
                STATUS.emit('update-machine-log', log, 'TIME')
        else:
            self.w.power.setChecked(False)
            self.set_buttons_state([self.idleOnList, self.idleHomedList], False)
            if self.ptButton and hal.get_value('plasmac.probe-test'):
                self.probe_test(False)
            if not self.firstRun:
                log = _translate('HandlerClass', 'GUI power off')
                STATUS.emit('update-machine-log', log, 'TIME')
        self.set_run_button_state()
        self.set_jog_button_state()

    def interp_idle(self, obj):
        hal.set_p('plasmac.consumable-change', '0')
        if self.single_cut_request:
            self.single_cut_request = False
            if self.oldFile and self.fileOpened:
                ACTION.OPEN_PROGRAM(self.oldFile)
            else:
                self.fileOpened = True
                self.file_clear_clicked()
            self.w[self.scButton].setEnabled(True)
            if self.g91:
                ACTION.CALL_MDI_WAIT('G91')
        if not self.manualCut:
            self.set_buttons_state([self.idleList], True)
        if self.lastLoadedProgram == 'None':
            self.w.file_reload.setEnabled(False)
        if STATUS.machine_is_on() and not self.manualCut:
            self.set_buttons_state([self.idleOnList], True)
            if self.tpButton and not self.w.torch_enable.isChecked():
                self.w[self.tpButton].setEnabled(False)
            if self.otButton and not self.w.ohmic_probe_enable.isChecked():
                self.w[self.otButton].setEnabled(False)
            if STATUS.is_all_homed():
                self.set_buttons_state([self.idleHomedList], True)
            else:
                self.set_buttons_state([self.idleHomedList], False)
        else:
            self.set_buttons_state([self.idleOnList, self.idleHomedList], False)
        self.w.jog_stack.setCurrentIndex(0)
        self.w.abort.setEnabled(False)
        if self.ccButton:
            self.button_normal(self.ccButton)
        self.set_tab_jog_states(True)
        self.set_run_button_state()
        self.set_jog_button_state()
        if self.jobRunning and obj:
            if self.w.torch_enable.isChecked():
                self.statistics_save()
            else:
                self.jobRunning = False
            log = _translate('HandlerClass', 'Machine idle')
            STATUS.emit('update-machine-log', log, 'TIME')
            self.statistics_show()
        ACTION.SET_MANUAL_MODE()

    def set_run_button_state(self):
        if STATUS.machine_is_on() and STATUS.is_all_homed() and \
           STATUS.is_interp_idle() and not self.offsetsActivePin.get() and \
           self.plasmacStatePin.get() == 0 and not self.boundsError['loaded'] and not self.fileClear:
            if int(self.w.materials_box.currentText().split(': ', 1)[0]) >= 1000000:
                self.w.materials_box.setCurrentIndex(0)
            if self.w.gcode_display.lines() > 1:
                self.w.run.setEnabled(True)
                if self.frButton:
                    self.w[self.frButton].setEnabled(True)
            if self.manualCut:
                self.manualCut = False
                self.set_mc_states(True)
            if self.probeTest:
                self.set_tab_jog_states(True)
                self.probeTest = False
            self.set_buttons_state([self.idleList, self.idleOnList, self.idleHomedList], True)
            self.w.abort.setEnabled(False)
        else:
            self.w.run.setEnabled(False)
            if self.frButton:
                self.w[self.frButton].setEnabled(False)

    def set_jog_button_state(self):
        if STATUS.machine_is_on() and STATUS.is_interp_idle() and not self.offsetsActivePin.get():
            for widget in self.jogButtonList:
                self.w[widget].setEnabled(True)
            if STATUS.is_all_homed():
                for widget in self.jogSyncList:
                    self.w[widget].setEnabled(True)
            else:
                for widget in self.jogSyncList:
                    self.w[widget].setEnabled(False)
        else:
            for widget in self.jogButtonList:
                self.w[widget].setEnabled(False)
            for widget in self.jogSyncList:
                self.w[widget].setEnabled(False)

    def interp_paused(self, obj):
        pass

    def interp_running(self, obj):
        self.w.run.setEnabled(False)
        if self.frButton:
            self.w[self.frButton].setEnabled(False)
        self.set_buttons_state([self.idleList, self.idleOnList, self.idleHomedList], False)
        self.w.abort.setEnabled(True)
        self.w.height_lower.setEnabled(True)
        self.w.height_raise.setEnabled(True)
        self.w.height_reset.setEnabled(True)
        if STATUS.is_auto_mode() and self.w.gcode_stack.currentIndex() != 0:
            self.w.gcode_stack.setCurrentIndex(0)
        self.set_tab_jog_states(False)
        self.set_jog_button_state()

    def interp_reading(self, obj):
        pass

    def interp_waiting(self, obj):
        pass

    def pause_changed(self, obj, state):
        if hal.get_value('plasmac.paused-motion') or hal.get_value('plasmac.cut-recovering'):
            if state:
                self.w.wcs_button.setEnabled(False)
            if hal.get_value('plasmac.program-is-paused'):
                if self.tpButton and self.w.torch_enable.isChecked():
                    self.w[self.tpButton].setEnabled(True)
                if self.otButton and self.w.ohmic_probe_enable.isChecked():
                    self.w[self.otButton].setEnabled(True)
            elif not hal.get_value('plasmac.program-is-paused') and not hal.get_value('plasmac.paused-motion'):
                if self.tpButton:
                    self.w[self.tpButton].setEnabled(False)
                if self.otButton:
                    self.w[self.otButton].setEnabled(False)
            return
        if state:
            # time delay workaround to ensure userspace pins/variables have time to set
            time.sleep(0.1)
            if self.ccButton and hal.get_value('plasmac.stop-type-out'):
                self.w[self.ccButton].setEnabled(True)
            if self.tpButton and self.w.torch_enable.isChecked():
                self.w[self.tpButton].setEnabled(True)
            if self.otButton and self.w.ohmic_probe_enable.isChecked():
                self.w[self.otButton].setEnabled(True)
            self.w.wcs_button.setEnabled(False)
            self.set_tab_jog_states(True)
            log = _translate('HandlerClass', 'Cycle paused')
            STATUS.emit('update-machine-log', log, 'TIME')
        else:
            self.w.jog_stack.setCurrentIndex(0)
            if self.ccButton:
                self.w[self.ccButton].setEnabled(False)
            if self.tpButton and STATUS.is_auto_running():
                self.w[self.tpButton].setEnabled(False)
            if self.otButton and STATUS.is_auto_running():
                self.w[self.otButton].setEnabled(False)
            if STATUS.is_auto_running():
                self.set_tab_jog_states(False)

    def jog_rate_changed(self, object, value):
        msg0 = _translate('HandlerClass', 'JOG')
        self.w.jogs_label.setText('{}\n{:.0f}'.format(msg0, STATUS.get_jograte()))

    def progress_changed(self, object, percent):
        if percent < 0:
            self.overlayProgress.setValue(0)
            self.overlayProgress.hide()
            self.w.gcode_display.setHorizontalScrollBarPolicy(Qt.ScrollBarAsNeeded)
            self.w.gcode_display.setVerticalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        else:
            self.overlayProgress.setValue(percent)
            self.overlayProgress.show()
            self.overlayProgress.setFixedHeight(self.w.gcode_display.geometry().height())
            self.overlayProgress.move(self.w.gcode_display.geometry().width() - 20, 0)
            self.w.gcode_display.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
            self.w.gcode_display.setVerticalScrollBarPolicy(Qt.ScrollBarAlwaysOff)

    def user_system_changed(self, obj, data):
        sys = self.systemList[int(data)]
        self.w.wcs_button.setText('WCS\n{}'.format(sys))
        if ACTION.prefilter_path:
            self.file_reload_clicked()

    def file_loaded(self, obj, filename):
        if os.path.basename(filename).count('.') > 1:
            self.lastLoadedProgram = ''
            return
        if filename is not None:
            self.overlayProgress.setValue(0)
            self.lastLoadedProgram = filename
            if not self.cameraOn and self.w.preview_stack.currentIndex() != 4:
                self.w.preview_stack.setCurrentIndex(0)
                self.vkb_hide()
            self.w.file_open.setText(os.path.basename(filename))
            if not self.single_cut_request:
                self.fileOpened = True
            text = _translate('HandlerClass', 'EDIT')
            self.w.edit_label.setText('{}: {}'.format(text, filename))
            if self.w.gcode_stack.currentIndex() != 0:
                self.w.gcode_stack.setCurrentIndex(0)
            self.w.file_reload.setEnabled(True)
        self.w.gcodegraphics.logger.clear()
        self.w.gcodegraphics.clear_highlight()
        if self.preRflFile and self.preRflFile != ACTION.prefilter_path:
            self.rflActive = False
            self.startLine = 0
            self.preRflFile = ''
        msgList, units, xMin, yMin, xMax, yMax = self.bounds_check('loaded', 0, 0)
        if self.boundsError['loaded']:
            head = _translate('HandlerClass', 'Axis Limit Error')
            msgs = ''
            for n in range(0, len(msgList), 3):
                if msgList[n + 1] == 'MAX':
                    msg0 = _translate('HandlerClass', 'move would exceed the maximum limit by')
                else:
                    msg0 = _translate('HandlerClass', 'move would exceed the minimum limit by')
                fmt = '\n{} {} {}{}' if msgs else '{} {} {}{}'
                msgs += fmt.format(msgList[n], msg0, msgList[n + 2], units)
            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n'.format(head, msgs))
            self.set_run_button_state()
            if self.single_cut_request:
                self.single_cut_request = False
                if self.oldFile and not 'single_cut' in self.oldFile:
                    ACTION.OPEN_PROGRAM(self.oldFile)
                    self.set_run_button_state()
                self.w[self.scButton].setEnabled(True)
                self.w.run.setEnabled(False)
        else:
            if self.single_cut_request:
                ACTION.RUN()
            self.set_run_button_state()
            if self.fileClear:
                self.fileClear = False
                self.fileOpened = False
                text = _translate('HandlerClass', 'OPEN')
                self.w.file_open.setText(text)
                text = _translate('HandlerClass', 'EDIT')
                self.w.edit_label.setText('{}'.format(text))
                self.w.gcode_editor.editor.new_text()
                self.w.gcode_editor.editor.setModified(False)
                self.w.gcode_display.new_text()
                self.w.gcode_display.set_margin_width(3)
                self.w.gcode_editor.set_margin_width(3)
        ACTION.SET_MANUAL_MODE()
        if not len(self.w.gcode_display.text()):
            self.w.gcode_display.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        if self.w.main_tab_widget.currentIndex():
            self.w.main_tab_widget.setCurrentIndex(0)
        if self.fileOpened:
            if not (self.w.view_p.isChecked() or self.w.view_z.isChecked()):
                self.view_t_pressed()
        else:
            self.view_t_pressed()

    def joints_all_homed(self, obj):
        self.interp_idle(None)
        if not self.firstHoming:
            ACTION.CALL_MDI_WAIT('T0 M6')
            ACTION.SET_MANUAL_MODE()
            self.firstHoming = True
        self.w.gcodegraphics.updateGL()
        self.w.conv_preview.updateGL()
        log = _translate('HandlerClass', 'Machine homed')
        STATUS.emit('update-machine-log', log, 'TIME')

    def joint_homed(self, obj, joint):
        dro = self.coordinates[int(joint)]
        self.w['dro_{}'.format(dro)].setProperty('homed', True)
        self.w['dro_{}'.format(dro)].setStyle(self.w['dro_{}'.format(dro)].style())
        self.w['dro_label_{}'.format(dro)].setProperty('homed', True)
        self.w['dro_label_{}'.format(dro)].setStyle(self.w['dro_label_{}'.format(dro)].style())
        self.w.update
        STATUS.emit('dro-reference-change-request', 1)
        self.w.gcodegraphics.logger.clear()

    def joint_unhomed(self, obj, joints):
        for joint in joints:
            dro = self.coordinates[int(joint)]
            self.w['dro_{}'.format(dro)].setProperty('homed', False)
            self.w['dro_{}'.format(dro)].setStyle(self.w['dro_{}'.format(dro)].style())
            self.w['dro_label_{}'.format(dro)].setProperty('homed', False)
            self.w['dro_label_{}'.format(dro)].setStyle(self.w['dro_label_{}'.format(dro)].style())
        if len(joints) < len(self.coordinates):
            self.w.home_all.setEnabled(True)
        self.w.update
        STATUS.emit('dro-reference-change-request', 1)
        self.interp_idle(None)
        self.w.gcodegraphics.updateGL()
        self.w.conv_preview.updateGL()

    def hard_limit_tripped(self, obj, tripped, list_of_tripped):
        self.w.chk_override_limits.setEnabled(tripped)
        if not tripped:
            self.w.chk_override_limits.setChecked(False)

    def jog_inhibited_changed(self, state):
        if state:
            self.w.chk_override_jog.setEnabled(state)
            self.w.releaseKeyboard()
        else:
           if not self.w.led_float_switch.hal_pin.get() and not self.ohmicLedInPin.get() and \
                                                            not self.w.led_breakaway_switch.hal_pin.get():
                self.w.chk_override_jog.setChecked(False)
                hal.set_p('plasmac.override-jog', '0')

    def sensor_active_changed(self, state):
        if not state:
            self.w.chk_override_jog.setEnabled(False)
            self.w.chk_override_jog.setChecked(False)
            hal.set_p('plasmac.override-jog', str(state))

    def z_offset_changed(self, height):
        if STATUS.is_interp_paused() and not height:
            if(hal.get_value('plasmac.stop-type-out') or hal.get_value('plasmac.cut-recovering')):
                self.w.set_cut_recovery()
                self.w.laser.setEnabled(True)

    def override_jog_changed(self, state):
        if state:
            hal.set_p('plasmac.override-jog', '1')
        else:
            hal.set_p('plasmac.override-jog', '0')

    def tool_changed(self, obj, tool):
        if tool == 0:
            self.w.lbl_tool.setText('TORCH')
        elif tool == 1:
            self.w.lbl_tool.setText('SCRIBE')
        else:
            self.w.lbl_tool.setText('UNKNOWN')

    def gcodes_changed(self, obj, cod):
        if self.units == 'in' and STATUS.is_metric_mode():
            self.droScale = 25.4
            self.gcodeScalePin.set(25.4)
        elif self.units == 'mm' and not STATUS.is_metric_mode():
            self.droScale = 1 / 25.4
            self.gcodeScalePin.set(1 / 25.4)
        else:
            self.droScale = 1
            self.gcodeScalePin.set(1)
        self.w.lbl_gcodes.setText('{}'.format(cod))

    def mcodes_changed(self, obj, cod):
        self.w.lbl_mcodes.setText('{}'.format(cod))

    def metric_mode_changed(self, obj, state):
        self.w.gcodegraphics.updateGL()

    def set_start_line(self, line):
        if self.fileOpened:
            if self.w.chk_run_from_line.isChecked():
                if self.w.sender():
                    if self.w.sender().objectName() == 'gcode_editor_display':
                        return
                if line > 1:
                    msg0 = _translate('HandlerClass', 'SELECTED')
                    self.w.run.setText('{} {}'.format(msg0, line))
                    self.runText = '{} {}'.format(msg0, line)
                    self.rflSelected = True
                    self.startLine = line - 1
                elif self.rflActive:
                    txt0 = _translate('HandlerClass', 'RUN FROM LINE')
                    txt1 = _translate('HandlerClass', 'CYCLE START')
                    self.runText = '{}\n{}'.format(txt0, txt1)
                    self.rflSelected = False
                else:
                    self.startLine = 0
                    self.rflSelected = False
                    self.w.gcodegraphics.clear_highlight()
            if line < 1:
                self.w.gcode_display.setCursorPosition(0, 0)
                self.w.gcode_display.moveMarker(0)

    def update_gcode_properties(self, props):
        if props:
            self.gcodeProps = props
            if props['gcode_units'] == 'in':
                STATUS.emit('metric-mode-changed', False)
            else:
                STATUS.emit('metric-mode-changed', True)

    def error_update(self, obj, kind, error):
        if not self.realTimeDelay:
            if kind == linuxcnc.OPERATOR_ERROR:
                self.error_status(True)
                self.mdiError = True
            if kind == linuxcnc.NML_ERROR:
                self.error_status(True)


#########################################################################################################################
# CALLBACKS FROM FORM #
#########################################################################################################################
    def ext_run(self, state):
        if self.w.run.isEnabled() and state:
            self.run_clicked()

    def ext_abort(self, state):
        if self.w.abort.isEnabled() and state:
            self.abort_pressed()

    def ext_pause(self, state):
        if self.w.pause.isEnabled() and state:
            ACTION.PAUSE()

    def ext_touch_off(self, state):
        if self.w.touch_xy.isEnabled() and state:
            self.touch_xy_clicked()

    def ext_laser_touch_off(self, state):
        if self.w.laser.isVisible():
            if state:
                self.extLaserButton = True
                self.laser_pressed()
            else:
                self.extLaserButton = False
                self.laser_clicked()

    def ext_jog_slow(self, state):
        if self.w.jog_slow.isEnabled() and state:
            self.jog_slow_pressed(True)

    def ext_run_pause(self, state):
        if self.w.run.isEnabled() and state:
            self.run_clicked()
        elif self.w.pause.isEnabled() and state:
            ACTION.PAUSE()

    def power_button(self, action, state):
        if action == 'pressed':
            self.shutdownTimer.start(self.shutdownTime)
        elif action == 'released':
            self.shutdownTimer.stop()
        elif action == 'clicked':
            if STATUS.estop_is_clear():
                ACTION.SET_MACHINE_STATE(not STATUS.machine_is_on())
            else:
                self.w.power.setChecked(False)
        elif action == 'external' and self.w.power.isEnabled() and not self.firstRun:
            if state:
                self.shutdownTimer.start(self.shutdownTime)
                self.w.power.setDown(True)
            else:
                self.shutdownTimer.stop()
                self.w.power.setDown(False)
                self.w.power.click()

    def run_clicked(self):
        if self.convBlockLoaded.get():
            self.wcs_rotation('get')
        if self.startLine and self.rflSelected:
            self.w.run.setEnabled(False)
            if self.frButton:
                self.w[self.frButton].setEnabled(False)
            self.rflActive = True
            self.rflSelected = False
            if self.developmentPin.get():
                reload(RFL)
            head = _translate('HandlerClass', 'Gcode Error')
            data = RFL.run_from_line_get(self.lastLoadedProgram, self.startLine)
            # cannot do run from line within a subroutine or if using cutter compensation
            if data['error']:
                if data['compError']:
                    msg0 = _translate('HandlerClass', 'Cannot run from line while')
                    msg1 = _translate('HandlerClass', 'cutter compensation is active')
                    STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n{}\n'.format(head, msg0, msg1))
                if data['subError']:
                    msg0 = _translate('HandlerClass', 'Cannot do run from line')
                    msg1 = _translate('HandlerClass', 'inside subroutine')
                    msg2 = ''
                    for sub in data['subError']:
                        msg2 += ' {}'.format(sub)
                    STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n{}{}\n'.format(head, msg0, msg1, msg2))
                self.clear_rfl()
                self.set_run_button_state()
            else:
                # get user input
                userInput = self.dialog_run_from_line()
                # rfl cancel clicked
                if userInput['cancel']:
                    if 'rfl.ngc' in self.lastLoadedProgram:
                        self.set_start_line(-1)
                    else:
                        self.clear_rfl()
                    self.set_run_button_state()
                else:
                    # rfl load clicked
                    rflFile = '{}rfl.ngc'.format(self.tmpPath)
                    result = RFL.run_from_line_set(rflFile, data, userInput, self.unitsPerMm)
                    # leadin cannot be used
                    if result['error']:
                        msg0 = _translate('HandlerClass', 'Unable to calculate a leadin for this cut')
                        msg1 = _translate('HandlerClass', 'Program will run from selected line with no leadin applied')
                        STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n{}\n'.format(head, msg0, msg1))
                    # load rfl file
                    if ACTION.prefilter_path or self.lastLoadedProgram != 'None':
                        self.preRflFile = ACTION.prefilter_path or self.lastLoadedProgram
                    ACTION.OPEN_PROGRAM(rflFile)
                    ACTION.prefilter_path = self.preRflFile
                    self.set_run_button_state()
                    txt0 = _translate('HandlerClass', 'RUN FROM LINE')
                    txt1 = _translate('HandlerClass', 'CYCLE START')
                    self.runText = '{}\n{}'.format(txt0, txt1)
                    log = _translate('HandlerClass', 'Run from line loaded')
                    log1 = _translate('HandlerClass', 'Start line')
                    STATUS.emit('update-machine-log', '{} - {}: {}'.format(log, log1, (self.startLine + 1)), 'TIME')
        elif not self.run_critical_check():
            self.jobRunning = True
            ACTION.RUN(0)
            log = _translate('HandlerClass', 'Cycle started')
            if self.w.torch_enable.isChecked():
                log1 = _translate('HandlerClass', 'Torch enabled')
            else:
                log1 = _translate('HandlerClass', 'Torch disabled')
            STATUS.emit('update-machine-log', '{} - {}'.format(log, log1), 'TIME')

    def abort_pressed(self):
        if self.manualCut:
            ACTION.SET_SPINDLE_STOP(0)
            if self.mcButton:
                self.button_normal(self.mcButton)
                self.w[self.mcButton].setEnabled(False)
            self.w.abort.setEnabled(False)
            log = _translate('HandlerClass', 'Manual cut aborted')
            STATUS.emit('update-machine-log', log, 'TIME')
            return
        elif self.probeTest:
            self.probe_test_stop()
            log = _translate('HandlerClass', 'Probe test aborted')
            STATUS.emit('update-machine-log', log, 'TIME')
            return
        elif self.torchPulse:
            self.torch_pulse(True)
            log = _translate('HandlerClass', 'Torch pulse aborted')
            STATUS.emit('update-machine-log', log, 'TIME')
        else:
            ACTION.ABORT()
            if hal.get_value('plasmac.cut-recovery'):
                hal.set_p('plasmac.cut-recovery', '0')
                self.laserOnPin.set(0)
            self.interp_idle(None)
            if self.convBlockLoaded.get():
                self.wcs_rotation('set')
            log = _translate('HandlerClass', 'Cycle aborted')
            STATUS.emit('update-machine-log', log, 'TIME')

    def pause_pressed(self):
        if hal.get_value('plasmac.cut-recovering'):
            self.w.jog_stack.setCurrentIndex(0)
            self.laserOnPin.set(0)
        self.cancelWait = False
        self.w.laser.setEnabled(False)
        if STATUS.is_auto_paused():
            log = _translate('HandlerClass', 'Cycle resumed')
            STATUS.emit('update-machine-log', log, 'TIME')

    def user_button_pressed(self, button):
        self.user_button_down(button)

    def user_button_released(self, button):
        self.user_button_up(button)

    def height_ovr_pressed(self, state, height):
        if state:
            if height:
                self.heightOvr += height * self.w.thc_threshold.value()
            else:
                self.heightOvr = 0
        if self.heightOvr < -9:
            self.heightOvr = -9
        if self.heightOvr > 9:
            self.heightOvr = 9
        self.heightOverridePin.set(self.heightOvr)
        self.w.height_ovr_label.setText('{:.2f}'.format(self.heightOvr))

    def height_ovr_encoder(self,value):
        if (value != self.old_ovr_counts and self.extHeightOvrCountEnablePin.get()):
            self.heightOvr += (value-self.old_ovr_counts) * self.w.thc_threshold.value() * self.heightOvrScale
            if self.heightOvr < -9:
                self.heightOvr = -9
            if self.heightOvr > 9:
                self.heightOvr = 9
            self.heightOverridePin.set(self.heightOvr)
            self.w.height_ovr_label.setText('{:.2f}'.format(self.heightOvr))
        self.old_ovr_counts = value

    def height_ovr_scale_change(self,value):
        if value:self.heightOvrScale = value

    def touch_xy_clicked(self):
        self.touch_off_xy(0, 0)
        self.w.gcodegraphics.logger.clear()

    def setup_feed_rate_changed(self):
        if self.w.probe_feed_rate.value() > self.w.setup_feed_rate.value():
            self.w.probe_feed_rate.setValue(self.w.setup_feed_rate.value())
        self.w.probe_feed_rate.setMaximum(self.w.setup_feed_rate.value())

    def save_plasma_clicked(self):
        self.save_plasma_parameters()

    def reload_plasma_clicked(self):
        self.load_plasma_parameters()

    def backup_clicked(self):
        self.save_logfile(6)
        bkpPath = '{}'.format(os.path.expanduser('~'))
        bkpName = '{}_V{}_{}.tar.gz'.format(self.machineName, VERSION, time.strftime('%y-%m-%d_%H-%M-%S'))
        tmpFile = os.path.join(self.PATHS.CONFIGPATH, 'config_info.txt')
        lcncInfo = (Popen('linuxcnc_info -s', stdout=PIPE, stderr=PIPE, shell=True).communicate()[0]).decode('utf-8')
        network = (Popen('lspci | grep -i net', stdout=PIPE, stderr=PIPE, shell=True).communicate()[0]).decode('utf-8')
        with open(tmpFile, 'a') as outFile:
            if network:
                outFile.write('lspci | grep -i net:\n{}\n'.format(network))
            else:
                outFile.write('Unknown error with "lspci | grep -i net" command\n\n')
            if lcncInfo:
                outFile.write('linuxcnc_info:\n')
                try:
                    with open('/tmp/linuxcnc_info.txt', 'r') as inFile:
                        for line in inFile:
                            outFile.write(line)
                except:
                    outFile.write("Unknown error opening /tmp/linuxcnc_info.txt\n")
            else:
                outFile.write('Unknown error creating /tmp/linuxcnc_info.txt\n')
        with tarfile.open('{}/{}'.format(bkpPath, bkpName), mode='w:gz', ) as archive:
            archive.add('{}'.format(self.PATHS.CONFIGPATH))
        try:
            os.remove(tmpFile)
        except:
            log = _translate('HandlerClass', 'Unknown error removing')
            STATUS.emit('update-machine-log', '{} {}'.format(log, tmpFile), 'TIME')
        head = _translate('HandlerClass', 'Backup Complete')
        msg0 = _translate('HandlerClass', 'A compressed backup of the machine configuration including the machine logs has been saved in your home directory as')
        msg1 = _translate('HandlerClass', 'It is safe to delete this file at any time')
        self.dialog_show_ok(QMessageBox.Information, head, '\n{}:\n{}\n\n{}\n'.format(msg0, bkpName, msg1))

    def set_offsets_clicked(self):
        if self.developmentPin.get():
            reload(OFFSETS)
        self.w.main_tab_widget.setCurrentIndex(0)
        if STATUS.stat.rotation_xy:
            ACTION.CALL_MDI_WAIT('G10 L2 P0 R0')
            ACTION.SET_MANUAL_MODE()
        OFFSETS.dialog_show(self, self.w, self.PREFS, INIPATH, STATUS, ACTION, TOOL)

    def feed_label_pressed(self):
        self.w.feed_slider.setValue(100)

    def rapid_label_pressed(self):
        self.w.rapid_slider.setValue(100)

    def jogs_label_pressed(self):
        self.w.jog_slider.setValue(INFO.DEFAULT_LINEAR_JOG_VEL)

    def gui_button_jog(self, state, joint, direction):
        shift = False
        if state and joint == 'z' and direction == 1 and self.zPlusOverrideJog and self.w.chk_override_jog.isEnabled():
                self.w.chk_override_jog.setChecked(True)
        if STATUS.is_joint_mode():
            self.kb_jog(state, self.coordinates.index(joint), direction, shift)
        else:
            self.kb_jog(state, ['x','y','z','a','b'].index(joint), direction, shift)

    def view_t_pressed(self):
        t = time.time() + 0.01
        while time.time() < t:
            QApplication.processEvents()
        self.w.gcodegraphics.set_view('Z')
        mid, size = GlCanonDraw.extents_info(self.w.gcodegraphics)
        if self.gcodeProps:
            mult = 1
            if self.units == 'in' and self.gcodeProps['gcode_units'] == 'mm':
                mult = 0.03937
            elif self.units == 'mm' and self.gcodeProps['gcode_units'] == 'in':
                mult = 25.4
            x = (round(float(self.gcodeProps['x'].split()[0]) * mult, 4))
            y = (round(float(self.gcodeProps['y'].split()[0]) * mult, 4))
            xl = (round(float(self.gcodeProps['x'].split('=')[1].split()[0]) * mult, 4))
            yl = (round(float(self.gcodeProps['y'].split('=')[1].split()[0]) * mult, 4))
        else:
            x = y = xl = yl = 0
        if (mid[0] == 0 and mid[1] == 0) or mid[0] > self.xLen or mid[1] > self.yLen or \
           self.w.view_t.isChecked() or self.w.view_t.isDown() or self.fileClear:
            mult = 1 if self.units == 'in' else 25.4
            zoomScale = (self.w.table_zoom_scale.value() * 2)
            mid = [(self.xLen - (x * 2) - xl) / mult / 2, (self.yLen - (y * 2) - yl) / mult / 2, 0]
            size = [self.xLen / mult / zoomScale, self.yLen / mult / zoomScale, 0]
        glTranslatef(-mid[0], -mid[1], -mid[2])
        self.w.gcodegraphics.set_eyepoint_from_extents(size[0], size[1])
        self.w.gcodegraphics.perspective = False
        self.w.gcodegraphics.lat = self.w.gcodegraphics.lon = 0
        self.w.gcodegraphics.updateGL()

    def view_p_pressed(self):
        self.w.gcodegraphics.set_view('P')

    def view_z_pressed(self):
        self.w.gcodegraphics.set_view('Z')

    def view_clear_pressed(self):
        self.w.gcodegraphics.logger.clear()

    def pan_left_pressed(self):
        self.w.gcodegraphics.recordMouse(0,0)
        self.w.gcodegraphics.translateOrRotate(-self.w.gcodegraphics._view_incr,0)

    def pan_right_pressed(self):
        self.w.gcodegraphics.recordMouse(0,0)
        self.w.gcodegraphics.translateOrRotate(self.w.gcodegraphics._view_incr,0)

    def pan_up_pressed(self):
        self.w.gcodegraphics.recordMouse(0,0)
        self.w.gcodegraphics.translateOrRotate(0,-self.w.gcodegraphics._view_incr)

    def pan_down_pressed(self):
        self.w.gcodegraphics.recordMouse(0,0)
        self.w.gcodegraphics.translateOrRotate(0,self.w.gcodegraphics._view_incr)

    def zoom_in_pressed(self):
        self.w.gcodegraphics.zoomin()

    def zoom_out_pressed(self):
        self.w.gcodegraphics.zoomout()

    def gcode_display_loaded(self):
        gcodeLines = len(str(self.w.gcode_display.lines()))
        self.w.gcode_display.set_margin_width(gcodeLines)
        self.w.gcode_editor.set_margin_width(gcodeLines)

    def file_clear_clicked(self):
        if self.fileOpened:
            self.fileClear = True
            if self.rflActive:
                self.clear_rfl()
            clearFile = '{}qtplasmac_program_clear.ngc'.format(self.tmpPath)
            with open(clearFile, 'w') as outFile:
                outFile.write('m2')
            if ACTION.prefilter_path:
                if 'single_cut' in ACTION.prefilter_path:
                    self.preClearFile = self.oldFile
                else:
                    if self.lastLoadedProgram != 'None':
                        self.preClearFile = ACTION.prefilter_path or self.lastLoadedProgram
            ACTION.OPEN_PROGRAM(clearFile)
            ACTION.prefilter_path = self.preClearFile
            self.w.material_selector.setCurrentIndex(0)
            self.w.conv_material.setCurrentIndex(0)
            if self.w.lbl_tool.text() != 'TORCH' and STATUS.is_on_and_idle() and STATUS.is_all_homed():
                ACTION.CALL_MDI_WAIT('T0 M6')
                ACTION.CALL_MDI_WAIT('G43 H0')
                ACTION.SET_MANUAL_MODE()
            log = _translate('HandlerClass', 'Program cleared')
            STATUS.emit('update-machine-log', log, 'TIME')
        else:
            self.view_t_pressed()

    def file_open_clicked(self):
        self.w.preview_stack.setCurrentIndex(1)
        self.vkb_hide()
        self.w.filemanager.table.setFocus()

    def file_edit_clicked(self):
        if STATUS.stat.interp_state == linuxcnc.INTERP_IDLE and self.w.preview_stack.currentIndex() != 2:
            self.w.preview_stack.setCurrentIndex(2)
            self.w.run.setEnabled(False)
            self.w.gcode_editor.editor.setFocus()
            if self.fileOpened:
                self.w.gcode_editor.editor.load_text(ACTION.prefilter_path)
                text = _translate('HandlerClass', 'EDIT')
                self.w.edit_label.setText('{}: {}'.format(text, ACTION.prefilter_path))
                self.w.gcode_editor.editor.setModified(False)
                try:
                    if os.path.getsize(self.gcodeErrorFile):
                        with open(self.gcodeErrorFile, 'r') as inFile:
                            self.w.gcode_editor.select_line(int(inFile.readline().rstrip()) - 1)
                            inFile.seek(0)
                            for line in inFile:
                                self.w.gcode_editor.editor.userHandle = \
                                self.w.gcode_editor.editor.markerAdd(int(line) - 1, self.w.gcode_editor.editor.USER_MARKER_NUM)
                except:
                    pass
            self.vkb_show()
        else:
            self.new_exitCall()

    def mdi_show_clicked(self):
        if STATUS.is_on_and_idle() and STATUS.is_all_homed() and self.w.gcode_stack.currentIndex() != 1:
            self.w.gcode_stack.setCurrentIndex(1)
            self.vkb_show()
        else:
            self.w.gcode_stack.setCurrentIndex(0)
            ACTION.SET_MANUAL_MODE()
            self.vkb_hide()

    def file_cancel_clicked(self):
        self.w.preview_stack.setCurrentIndex(0)
        self.vkb_hide()
        ACTION.SET_MANUAL_MODE()

    def cone_size_changed(self, data):
        self.w.gcodegraphics.set_cone_basesize(data)

    def grid_size_changed(self, data):
        # grid size is in inches
        grid = data / self.unitsPerMm / 25.4
        self.w.gcodegraphics.grid_size = grid

    def main_tab_changed(self, tab):
        t = time.time() + 0.01
        while time.time() < t:
            QApplication.processEvents()
        if tab == 0:
            if self.w.preview_stack.currentIndex() == 2:
                self.vkb_show()
            else:
                self.vkb_hide()
            if self.w.gcode_stack.currentIndex() == 0 and self.w.preview_stack.currentIndex() == 0:
                self.autorepeat_keys(False)
        elif tab == 1:
            self.w.conv_preview.logger.clear()
            self.w.conv_preview.set_current_view()
            if self.developmentPin.get():
                reload(self.CONV)
            self.CONV.conv_setup(self, self.w)
            self.vkb_show(True)
            self.autorepeat_keys(True)
        elif tab == 2:
            self.vkb_show(True)
            self.autorepeat_keys(True)
        elif tab == 3:
            self.vkb_show()
            self.autorepeat_keys(True)
        if self.w.main_tab_widget.currentIndex() == self.w.main_tab_widget.count() - 1:
            self.vkb_hide()
            self.w.machinelog.moveCursor(QTextCursor.End)
            self.w.machinelog.setCursorWidth(0)
            self.error_status(False)

    def z_height_changed(self, value):
        self.w.dro_z.update_user(value * self.droScale)

    def offsets_active_changed(self, value):
        if not value:
            # set z dro to normal made
            self.w.dro_z.setProperty('Qreference_type', 1)
            self.set_run_button_state()
            self.set_jog_button_state()

    def consumable_change_changed(self, value):
        if self.ccButton:
            if value:
                self.cutrec_buttons_enable(False)
                self.cutrec_motion_enable(False)
                if self.tpButton:
                    self.w[self.tpButton].setEnabled(False)
                if self.otButton:
                    self.w[self.otButton].setEnabled(False)
                log = _translate('HandlerClass', 'Consumable change initiated')
                STATUS.emit('update-machine-log', log, 'TIME')
            else:
                self.cutrec_buttons_enable(True)
                self.cutrec_motion_enable(True)
                if STATUS.is_interp_paused():
                    self.w.pause.setEnabled(True)
                    self.w[self.ccButton].setEnabled(True)
                    if self.tpButton and self.w.torch_enable.isChecked():
                        self.w[self.tpButton].setEnabled(True)
                    if self.otButton:
                        self.w[self.otButton].setEnabled(True)
                else:
                    if self.ccButton:
                        self.w[self.ccButton].setEnabled(False)
                self.button_normal(self.ccButton)
                log = _translate('HandlerClass', 'Consumable change completed')
                STATUS.emit('update-machine-log', log, 'TIME')

    def plasmac_state_changed(self, state):
        if ((state > self.PROBE_UP and not STATUS.is_interp_idle()) or state == self.PROBE_TEST) and hal.get_value('axis.z.eoffset-counts'):
            # set z dro to offset mode
            self.w.dro_z.setProperty('Qreference_type', 10)
        if state == self.IDLE:
            self.set_run_button_state()
            self.set_jog_button_state()

    def file_reload_clicked(self):
        if self.rflActive:
            self.clear_rfl()
            self.set_run_button_state()
        if ACTION.prefilter_path or self.lastLoadedProgram != 'None':
            file = ACTION.prefilter_path or self.lastLoadedProgram
            if os.path.exists(file):
                self.overlayProgress.setValue(0)
                ACTION.OPEN_PROGRAM(file)
                log = _translate('HandlerClass', 'Reloaded')
                STATUS.emit('update-machine-log', '{}: {}'.format(log, file), 'TIME')
            else:
                head = _translate('HandlerClass', 'File Error')
                msg0 = _translate('HandlerClass', 'does not exist')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} {}\n'.format(head, file, msg0))

    def jog_slow_pressed(self, external=False):
        if self.w.jog_slow.isChecked():
            self.w.jog_slow.setText(_translate('HandlerClass', 'FAST'))
            self.w.jog_slider.setMaximum(self.w.jog_slider.maximum() * self.slowJogFactor)
            self.w.jog_slider.setValue(self.w.jog_slider.value() * self.slowJogFactor)
            self.w.jog_slider.setPageStep(100)
            self.previousJogSpeed = self.w.jog_slider.value()
            if external:
                self.w.jog_slow.setChecked(False)
        else:
            self.w.jog_slow.setText(_translate('HandlerClass', 'SLOW'))
            self.w.jog_slider.setValue(self.w.jog_slider.value() / self.slowJogFactor)
            self.w.jog_slider.setMaximum(self.w.jog_slider.maximum() / self.slowJogFactor)
            self.w.jog_slider.setPageStep(10)
            if external:
                self.w.jog_slow.setChecked(True)

    def chk_override_limits_changed(self, state):
        if state:
            ACTION.TOGGLE_LIMITS_OVERRIDE()

    def param_tab_changed(self, state):
        if state:
            self.w.main_tab_widget.setTabEnabled(2, False)
        else:
            self.w.main_tab_widget.setTabEnabled(2, True)

    def settings_tab_changed(self, state):
        if state:
            self.w.main_tab_widget.setTabEnabled(3, False)
        else:
            self.w.main_tab_widget.setTabEnabled(3, True)

    def conv_tab_changed(self, state):
        if state:
            self.w.main_tab_widget.setTabEnabled(1, False)
        else:
            self.w.main_tab_widget.setTabEnabled(1, True)

    def save_shutdown_message_clicked(self):
        self.PREFS.putpref('Exit warning text', self.w.sd_text.text(), str, 'GUI_OPTIONS')
        self.exitMessage = self.w.sd_text.text()

    def reload_shutdown_message_clicked(self):
        self.w.sd_text.setText(self.PREFS.getpref('Exit warning text', '', str, 'GUI_OPTIONS'))

    def save_user_button_clicked(self):
        self.estopOnList = []
        self.idleList = ['file_clear', 'file_open', 'file_reload']
        self.idleOnList = ['home_x', 'home_y', 'home_z', 'home_a', 'home_all']
        self.idleHomedList = ['touch_x', 'touch_y', 'touch_z', 'touch_a', 'touch_b', 'touch_xy', \
                              'mdi_show', 'height_lower', 'height_raise', 'wcs_button', 'set_offsets']
        self.ccButton, self.otButton, self.ptButton, self.tpButton = '', '', '', ''
        self.ctButton, self.scButton, self.frButton, self.mcButton = '', '', '', ''
        self.ovButton, self.llButton, self.tlButton = '', '', ''
        self.halTogglePins = {}
        self.halPulsePins = {}
        for n in range(1, 21):
            self.PREFS.putpref('{} Name'.format(n), self.w['ub_name_{}'.format(n)].text(), str, 'BUTTONS')
            self.PREFS.putpref('{} Code'.format(n), self.w['ub_code_{}'.format(n)].text(), str, 'BUTTONS')
        self.user_button_setup()
        self.set_buttons_state([self.estopOnList], True)
        if STATUS.is_interp_idle():
            self.set_buttons_state([self.idleList], True)
            if STATUS.machine_is_on():
                self.set_buttons_state([self.idleOnList], True)
                if STATUS.is_all_homed():
                    self.set_buttons_state([self.idleHomedList], True)
                else:
                    self.set_buttons_state([self.idleHomedList], False)
            else:
                self.set_buttons_state([self.idleOnList, self.idleHomedList], False)
        else:
            self.set_buttons_state([self.idleList, self.idleOnList, self.idleHomedList], False)

    def reload_user_button_clicked(self):
        for n in range(1, 21):
            self.w['ub_name_{}'.format(n)].clear()
            self.w['ub_code_{}'.format(n)].clear()
        self.user_button_setup()


#########################################################################################################################
# GENERAL FUNCTIONS #
#########################################################################################################################
# called by ScreenOptions, this function overrides ScreenOption's closeEvent
    def closeEvent(self, event):
        O = self.w.screen_options
        if self.w.chk_exit_warning.isChecked() or not STATUS.is_interp_idle():
            icon = QMessageBox.Question if STATUS.is_interp_idle() else QMessageBox.Critical
            head = _translate('HandlerClass', 'Shutdown')
            if not STATUS.is_interp_idle():
                msg0 = _translate('HandlerClass', 'Current operation is not complete')
                msg0 += '!\n\n'
            else:
                msg0 = ''
            if self.exitMessage:
                exitLines = self.exitMessage.split('\\')
                for line in exitLines:
                    msg0 += '{}\n'.format(line)
                msg0 += '\n'
            msg0 += _translate('HandlerClass', 'Do you want to shutdown QtPlasmaC')
            if self.dialog_show_yesno(icon, head, '\n{}?\n'.format(msg0)):
                if O.PREFS_ and O.play_sounds and O.shutdown_play_sound:
                    STATUS.emit('play-sound', O.shutdown_exit_sound_type)
                O.QTVCP_INSTANCE_.settings.sync()
                O.QTVCP_INSTANCE_.shutdown()
                O.QTVCP_INSTANCE_.panel_.shutdown()
                STATUS.shutdown()
                event.accept()
            else:
                event.ignore()
        else:
            if O.PREFS_ and O.play_sounds and O.shutdown_play_sound:
                STATUS.emit('play-sound', O.shutdown_exit_sound_type)
            O.QTVCP_INSTANCE_.settings.sync()
            O.QTVCP_INSTANCE_.shutdown()
            O.QTVCP_INSTANCE_.panel_.shutdown()
            STATUS.shutdown()

    def update_check(self):
        # newest update must be added last in this function
        # if any writing to the INI file is required then that needs
        # to be done later in the update_iniwrite function
        halfiles = self.iniFile.findall('HAL', 'HALFILE') or None
        machinePrefsFile = os.path.join(self.PATHS.CONFIGPATH, self.machineName + '.prefs')
        qtvcpPrefsFile = os.path.join(self.PATHS.CONFIGPATH, 'qtvcp.prefs')
        # use qtplasmac_comp.hal for component connections (pre V1.221.154 2022/01/18)
        if halfiles and not [f for f in halfiles if 'plasmac.tcl' in f] and not \
            [f for f in halfiles if 'qtplasmac_comp.hal' in f]:
            UPDATER.add_component_hal_file(self.PATHS.CONFIGPATH, halfiles)
            self.updateIni.append(154)
        # split out qtplasmac specific prefs into a separate file (pre V1.222.170 2022/03/08)
        if not os.path.isfile(machinePrefsFile):
            old = os.path.join(self.PATHS.CONFIGPATH, 'qtplasmac.prefs')
            if os.path.isfile(old):
                UPDATER.split_prefs_file(old, qtvcpPrefsFile, machinePrefsFile)
            self.PREFS = Access(os.path.join(self.PATHS.CONFIGPATH, self.machineName + '.prefs'))
        # move conversational prefs from qtvcp.prefs to <machine_name>.prefs (pre V1.222.187 2022/05/03)
        if os.path.isfile(qtvcpPrefsFile) and os.path.isfile(machinePrefsFile):
            with open(qtvcpPrefsFile, 'r') as inFile:
                data = inFile.readlines()
                if [line for line in data if '[CONVERSATIONAL]' in line]:
                    UPDATER.move_prefs(qtvcpPrefsFile, machinePrefsFile)
        # change RS274 startup parameters from a subroutine (pre V1.224.207 2022/06/22)
        startupCode = self.iniFile.find('RS274NGC', 'RS274NGC_STARTUP_CODE')
        if 'metric_startup' in startupCode or 'imperial_startup' in startupCode:
            self.updateIni.append(207)
        # remove the qtplasmac link from the config directory (pre V1.225.208 2022/06/29)
        if os.path.lexists(os.path.join(self.PATHS.CONFIGPATH, 'qtplasmac')):
            # stage 1: set up for unlinking on the next run of qtplasmac
            if 'code.py' in self.iniFile.find('FILTER', 'ngc'):
                self.updateIni.append(208)
            # stage 2: remove the qtplasmac link
            else:
                if os.path.islink(os.path.join(self.PATHS.CONFIGPATH, 'qtplasmac')):
                    os.unlink(os.path.join(self.PATHS.CONFIGPATH, 'qtplasmac'))
                else:
                    os.rmdir(os.path.join(self.PATHS.CONFIGPATH, 'qtplasmac'))
        # move qtplasmac options from INI file to prefs file pre V1.227.219 2022/07/14)
        if not self.PREFS.has_section('BUTTONS'):
            UPDATER.move_options_to_prefs_file(self.iniFile, self.PREFS)
            self.updateIni.append(219)
        # move port info from [GUI_OPTIONS] section (if it was moved via V1.227.219 update) to [POWERMAX] section
        with open(machinePrefsFile, 'r') as inFile:
            data = inFile.read()
            if data.count('Port') > 1:
                UPDATER.move_port(self.PREFS)

    def update_iniwrite(self):
        # this is for updates that write to the INI file
        if 154 in self.updateIni:
            UPDATER.add_component_hal_file_iniwrite(INIPATH)
        if 207 in self.updateIni:
            UPDATER.rs274ngc_startup_code_iniwrite(INIPATH)
        if 208 in self.updateIni:
            UPDATER.remove_qtplasmac_link_iniwrite(INIPATH)
        if 219 in self.updateIni:
            UPDATER.move_options_to_prefs_file_iniwrite(INIPATH)

    def motion_type_changed(self, value):
        if value == 0 and STATUS.is_mdi_mode():
            ACTION.SET_MANUAL_MODE()

    def wcs_rotation(self, wcs):
        if wcs == 'get':
            self.currentRotation = STATUS.stat.rotation_xy
            self.currentX = STATUS.stat.g5x_offset[0]
            self.currentY = STATUS.stat.g5x_offset[1]
        elif wcs == 'set':
            ACTION.CALL_MDI_WAIT('G10 L2 P0 X{} Y{} R{}'.format(self.currentX, self.currentY, self.currentRotation))
            if self.currentRotation != STATUS.stat.rotation_xy:
                self.w.gcodegraphics.set_current_view()
            ACTION.SET_MANUAL_MODE()

    def set_buttons_state(self, buttonLists, state):
        for buttonList in buttonLists:
            for button in buttonList:
                if state and STATUS.is_interp_paused() and button not in self.pausedValidList:
                    continue
                if not state and button == self.tpButton and self.torchTimer.isActive():
                    continue
                self.w[button].setEnabled(state)
        if self.laserRecStatePin.get():
            self.w.laser.setEnabled(False)
        if self.tpButton and not self.w.torch_enable.isChecked():
            self.w[self.tpButton].setEnabled(False)
        if self.frButton and self.w.gcode_display.lines() == 1:
            self.w[self.frButton].setEnabled(False)
        if self.ccButton and not STATUS.is_interp_paused():
            self.w[self.ccButton].setEnabled(False)

    def system_notify_button_pressed(self, object, button, state):
        if button in ['clearAll', 'close', 'lastFive'] and state:
            self.error_status(False)

    def error_status(self, state):
        self.error_present = state
        if state:
            text = _translate('HandlerClass', 'ERROR SENT TO MACHINE LOG')
        else:
            text = ''
        self.w.error_label.setText('{}'.format(text))

    def touch_off_xy(self, x, y):
        if STATUS.is_on_and_idle() and STATUS.is_all_homed():
            ACTION.CALL_MDI_WAIT('G10 L20 P0 X{} Y{}'.format(x, y))
            if self.fileOpened == True:
                self.file_reload_clicked()
            ACTION.SET_MANUAL_MODE()

    def bounds_check(self, boundsType, xOffset, yOffset):
        framing = True if 'framing' in boundsType else False
        self.boundsError[boundsType] = False
        msgList = []
        boundsMultiplier = 1
        if self.units == 'in' and self.gcodeProps['gcode_units'] == 'mm':
            boundsMultiplier = 0.03937
        elif self.units == 'mm' and self.gcodeProps['gcode_units'] == 'in':
            boundsMultiplier = 25.4
        if framing:
            xStart = STATUS.stat.g5x_offset[0] + xOffset
            yStart = STATUS.stat.g5x_offset[1] + yOffset
            xMin = round(float(self.gcodeProps['x_zero_rxy'].split()[0]) * boundsMultiplier + xOffset, 5)
            xMax = round(float(self.gcodeProps['x_zero_rxy'].split()[2]) * boundsMultiplier + xOffset, 5)
            yMin = round(float(self.gcodeProps['y_zero_rxy'].split()[0]) * boundsMultiplier + yOffset, 5)
            yMax = round(float(self.gcodeProps['y_zero_rxy'].split()[2]) * boundsMultiplier + yOffset, 5)
            coordinates = [[xStart, yStart], [xMin, yMin], [xMin, yMax], [xMax, yMax], [xMax, yMin]]
            frame_points, xMin, yMin, xMax, yMax = self.rotate_frame(coordinates)
        else:
            xMin = round(float(self.gcodeProps['x'].split()[0]) * boundsMultiplier + xOffset, 5)
            xMax = round(float(self.gcodeProps['x'].split()[2]) * boundsMultiplier + xOffset, 5)
            yMin = round(float(self.gcodeProps['y'].split()[0]) * boundsMultiplier + yOffset, 5)
            yMax = round(float(self.gcodeProps['y'].split()[2]) * boundsMultiplier + yOffset, 5)
        if xMin < self.xMin:
            amount = float(self.xMin - xMin)
            msgList.append('X')
            msgList.append('MIN')
            msgList.append('{:0.2f}'.format(amount))
            self.boundsError[boundsType] = True
        if xMax > self.xMax:
            amount = float(xMax - self.xMax)
            msgList.append('X')
            msgList.append('MAX')
            msgList.append('{:0.2f}'.format(amount))
            self.boundsError[boundsType] = True
        if yMin < self.yMin:
            amount = float(self.yMin - yMin)
            msgList.append('Y')
            msgList.append('MIN')
            msgList.append('{:0.2f}'.format(amount))
            self.boundsError[boundsType] = True
        if yMax > self.yMax:
            amount = float(yMax - self.yMax)
            msgList.append('Y')
            msgList.append('MAX')
            msgList.append('{:0.2f}'.format(amount))
            self.boundsError[boundsType] = True
        if framing:
            return msgList, self.units, xMin, yMin, xMax, yMax, frame_points
        else:
            return msgList, self.units, xMin, yMin, xMax, yMax

    def rotate_frame(self, coordinates):
        angle = math.radians(STATUS.stat.rotation_xy)
        cos = math.cos(angle)
        sin = math.sin(angle)
        frame_points = [coordinates[0]]
        ox = frame_points[0][0]
        oy = frame_points[0][1]
        for x, y in coordinates[1:]:
            tox = x - ox
            toy = y - oy
            rx = (tox * cos) - (toy * sin) + ox
            ry = (tox * sin) + (toy * cos) + oy
            frame_points.append([rx, ry])
        xMin = min(frame_points[1:])[0]
        xMax = max(frame_points[1:])[0]
        yMin = min(frame_points[1:])[1]
        yMax = max(frame_points[1:])[1]
        return frame_points, xMin, yMin, xMax, yMax

    def save_plasma_parameters(self):
        self.PREFS.putpref('Arc OK High', self.w.arc_ok_high.value(), float, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('Arc OK Low', self.w.arc_ok_low.value(), float, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('Arc Maximum Starts', self.w.arc_max_starts.value(), int, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('Arc Fail Timeout', self.w.arc_fail_delay.value(), float, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('Arc Voltage Offset', self.w.arc_voltage_offset.value(), float, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('Arc Voltage Scale', self.w.arc_voltage_scale.value(), float, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('Velocity Anti Dive Threshold', self.w.cornerlock_threshold.value(), float, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('Float Switch Travel', self.w.float_switch_travel.value(), float, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('Height Per Volt', self.w.height_per_volt.value(), float, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('Void Sense Slope', self.w.voidlock_slope.value(), int, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('Ohmic Maximum Attempts', self.w.ohmic_max_attempts.value(), int, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('Ohmic Probe Offset', self.w.ohmic_probe_offset.value(), float, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('Pid P Gain', self.w.pid_p_gain.value(), float, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('Pid D Gain', self.w.pid_d_gain.value(), float, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('Pid I Gain', self.w.pid_i_gain.value(), float, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('Probe Feed Rate', self.w.probe_feed_rate.value(), float, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('Probe Start Height', self.w.probe_start_height.value(), float, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('Arc Restart Delay', self.w.arc_restart_delay.value(), float, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('Safe Height', self.w.safe_height.value(), float, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('Scribe Arming Delay', self.w.scribe_arm_delay.value(), float, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('Scribe On Delay', self.w.scribe_on_delay.value(), float, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('Setup Feed Rate', self.w.setup_feed_rate.value(), float, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('Skip IHS Distance', self.w.skip_ihs_distance.value(), float, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('Spotting Threshold', self.w.spotting_threshold.value(), float, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('Spotting Time', self.w.spotting_time.value(), float, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('THC Delay', self.w.thc_delay.value(), float, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('THC Sample Counts', self.w.thc_sample_counts.value(), int, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('THC Sample Threshold', self.w.thc_sample_threshold.value(), float, 'PLASMA_PARAMETERS')
        self.PREFS.putpref('THC Threshold', self.w.thc_threshold.value(), float, 'PLASMA_PARAMETERS')

    def load_plasma_parameters(self):
        self.w.setup_feed_rate.setValue(self.PREFS.getpref('Setup Feed Rate', self.thcFeedRate * 0.8, float, 'PLASMA_PARAMETERS'))
        self.w.probe_feed_rate.setMaximum(self.w.setup_feed_rate.value())
        self.w.arc_fail_delay.setValue(self.PREFS.getpref('Arc Fail Timeout', 3, float, 'PLASMA_PARAMETERS'))
        self.w.arc_ok_high.setValue(self.PREFS.getpref('Arc OK High', 250, float, 'PLASMA_PARAMETERS'))
        self.w.arc_ok_low.setValue(self.PREFS.getpref('Arc OK Low', 60, float, 'PLASMA_PARAMETERS'))
        self.w.arc_max_starts.setValue(self.PREFS.getpref('Arc Maximum Starts', 3, int, 'PLASMA_PARAMETERS'))
        self.w.arc_voltage_offset.setValue(self.PREFS.getpref('Arc Voltage Offset', 0, float, 'PLASMA_PARAMETERS'))
        self.w.arc_voltage_scale.setValue(self.PREFS.getpref('Arc Voltage Scale', 1, float, 'PLASMA_PARAMETERS'))
        self.w.cornerlock_threshold.setValue(self.PREFS.getpref('Velocity Anti Dive Threshold', 90, float, 'PLASMA_PARAMETERS'))
        self.w.float_switch_travel.setValue(self.PREFS.getpref('Float Switch Travel', round(1.5 * self.unitsPerMm, 2), float, 'PLASMA_PARAMETERS'))
        self.w.height_per_volt.setValue(self.PREFS.getpref('Height Per Volt', round(0.1 * self.unitsPerMm, 3), float, 'PLASMA_PARAMETERS'))
        self.w.voidlock_slope.setValue(self.PREFS.getpref('Void Sense Slope', 500, int, 'PLASMA_PARAMETERS'))
        self.w.ohmic_max_attempts.setValue(self.PREFS.getpref('Ohmic Maximum Attempts', 0, int, 'PLASMA_PARAMETERS'))
        self.w.ohmic_probe_offset.setValue(self.PREFS.getpref('Ohmic Probe Offset', 0, float, 'PLASMA_PARAMETERS'))
        self.w.pid_p_gain.setValue(self.PREFS.getpref('Pid P Gain', 10, float, 'PLASMA_PARAMETERS'))
        self.w.pid_d_gain.setValue(self.PREFS.getpref('Pid D Gain', 0, float, 'PLASMA_PARAMETERS'))
        self.w.pid_i_gain.setValue(self.PREFS.getpref('Pid I Gain', 0, float, 'PLASMA_PARAMETERS'))
        self.w.probe_feed_rate.setValue(self.PREFS.getpref('Probe Feed Rate', round(300 * self.unitsPerMm, 0), float, 'PLASMA_PARAMETERS'))
        self.w.probe_start_height.setValue(self.PREFS.getpref('Probe Start Height', round(25 * self.unitsPerMm, 0), float, 'PLASMA_PARAMETERS'))
        self.w.arc_restart_delay.setValue(self.PREFS.getpref('Arc Restart Delay', 1, float, 'PLASMA_PARAMETERS'))
        self.w.safe_height.setValue(self.PREFS.getpref('Safe Height', round(25 * self.unitsPerMm, 0), float, 'PLASMA_PARAMETERS'))
        self.w.setup_feed_rate.setValue(self.PREFS.getpref('Setup Feed Rate', self.thcFeedRate * 0.8, float, 'PLASMA_PARAMETERS'))
        self.w.scribe_arm_delay.setValue(self.PREFS.getpref('Scribe Arming Delay', 0, float, 'PLASMA_PARAMETERS'))
        self.w.scribe_on_delay.setValue(self.PREFS.getpref('Scribe On Delay', 0, float, 'PLASMA_PARAMETERS'))
        self.w.skip_ihs_distance.setValue(self.PREFS.getpref('Skip IHS Distance', 0, float, 'PLASMA_PARAMETERS'))
        self.w.spotting_threshold.setValue(self.PREFS.getpref('Spotting Threshold', 1, float, 'PLASMA_PARAMETERS'))
        self.w.spotting_time.setValue(self.PREFS.getpref('Spotting Time', 0, float, 'PLASMA_PARAMETERS'))
        self.w.thc_delay.setValue(self.PREFS.getpref('THC Delay', 0.5, float, 'PLASMA_PARAMETERS'))
        self.w.thc_sample_counts.setValue(self.PREFS.getpref('THC Sample Counts', 50, int, 'PLASMA_PARAMETERS'))
        self.w.thc_sample_threshold.setValue(self.PREFS.getpref('THC Sample Threshold', 1, float, 'PLASMA_PARAMETERS'))
        self.w.thc_threshold.setValue(self.PREFS.getpref('THC Threshold', 1, float, 'PLASMA_PARAMETERS'))

    def set_signal_connections(self):
        self.w.power.pressed.connect(lambda:self.power_button("pressed", True))
        self.w.power.released.connect(lambda:self.power_button("released", False))
        self.w.power.clicked.connect(lambda:self.power_button("clicked", None))
        self.w.run.clicked.connect(self.run_clicked)
        self.w.pause.pressed.connect(self.pause_pressed)
        self.w.abort.pressed.connect(self.abort_pressed)
        self.w.file_reload.clicked.connect(self.file_reload_clicked)
        self.w.jog_slow.pressed.connect(self.jog_slow_pressed)
        self.w.chk_soft_keyboard.stateChanged.connect(self.soft_keyboard)
        self.w.chk_override_limits.stateChanged.connect(self.chk_override_limits_changed)
        self.w.chk_overlay.stateChanged.connect(self.overlay_update)
        self.w.chk_tool_tips.stateChanged.connect(lambda:TOOLTIPS.tool_tips_changed(self.w))
        self.w.torch_enable.stateChanged.connect(lambda w:self.torch_enable_changed(w))
        self.w.ohmic_probe_enable.stateChanged.connect(lambda w:self.ohmic_probe_enable_changed(w))
        self.w.thc_auto.stateChanged.connect(lambda w:self.thc_auto_changed(w))
        self.w.cone_size.valueChanged.connect(self.cone_size_changed)
        self.w.grid_size.valueChanged.connect(self.grid_size_changed)
        self.w.gcode_display.linesChanged.connect(self.gcode_display_loaded)
        self.w.gcode_editor.percentDone.connect(lambda w:self.progress_changed(None, w))
        self.w.file_clear.clicked.connect(self.file_clear_clicked)
        self.w.file_open.clicked.connect(self.file_open_clicked)
        self.w.file_edit.clicked.connect(self.file_edit_clicked)
        self.w.mdi_show.clicked.connect(self.mdi_show_clicked)
        self.w.file_cancel.clicked.connect(self.file_cancel_clicked)
        self.w.color_foregrnd.clicked.connect(lambda:self.openColorDialog(self.w.color_foregrnd))
        self.w.color_foregalt.clicked.connect(lambda:self.openColorDialog(self.w.color_foregalt))
        self.w.color_led.clicked.connect(lambda:self.openColorDialog(self.w.color_led))
        self.w.color_backgrnd.clicked.connect(lambda:self.openColorDialog(self.w.color_backgrnd))
        self.w.color_backgalt.clicked.connect(lambda:self.openColorDialog(self.w.color_backgalt))
        self.w.color_frams.clicked.connect(lambda:self.openColorDialog(self.w.color_frams))
        self.w.color_estop.clicked.connect(lambda:self.openColorDialog(self.w.color_estop))
        self.w.color_disabled.clicked.connect(lambda:self.openColorDialog(self.w.color_disabled))
        self.w.color_preview.clicked.connect(lambda:self.openColorDialog(self.w.color_preview))
        self.w.save_plasma.clicked.connect(self.save_plasma_clicked)
        self.w.reload_plasma.clicked.connect(self.reload_plasma_clicked)
        self.w.backup.clicked.connect(self.backup_clicked)
        self.w.set_offsets.clicked.connect(self.set_offsets_clicked)
        self.w.save_material.clicked.connect(self.save_materials_clicked)
        self.w.reload_material.clicked.connect(self.reload_materials_clicked)
        self.w.new_material.clicked.connect(lambda:self.new_material_clicked(0, 0))
        self.w.delete_material.clicked.connect(self.delete_material_clicked)
        self.w.setup_feed_rate.valueChanged.connect(self.setup_feed_rate_changed)
        self.w.touch_xy.clicked.connect(self.touch_xy_clicked)
        self.w.materials_box.currentIndexChanged.connect(lambda w:self.material_changed(w))
        self.w.material_selector.currentIndexChanged.connect(lambda w:self.selector_changed(w))
        self.w.conv_material.currentIndexChanged.connect(lambda w:self.conv_material_changed(w))
        self.w.sd_save.clicked.connect(self.save_shutdown_message_clicked)
        self.w.sd_reload.clicked.connect(self.reload_shutdown_message_clicked)
        self.w.ub_save.clicked.connect(self.save_user_button_clicked)
        self.w.ub_reload.clicked.connect(self.reload_user_button_clicked)
        self.materialChangePin.value_changed.connect(lambda w:self.material_change_pin_changed(w))
        self.materialChangeNumberPin.value_changed.connect(lambda w:self.material_change_number_pin_changed(w))
        self.materialChangeTimeoutPin.value_changed.connect(lambda w:self.material_change_timeout_pin_changed(w))
        self.materialReloadPin.value_changed.connect(lambda w:self.material_reload_pin_changed(w))
        self.materialTempPin.value_changed.connect(lambda w:self.material_temp_pin_changed(w))
        self.w.height_lower.pressed.connect(lambda:self.height_ovr_pressed(1,-1))
        self.w.height_raise.pressed.connect(lambda:self.height_ovr_pressed(1,1))
        self.w.height_reset.pressed.connect(lambda:self.height_ovr_pressed(1,0))
        self.w.button_1.pressed.connect(lambda:self.user_button_pressed(1))
        self.w.button_1.released.connect(lambda:self.user_button_released(1))
        self.w.button_2.pressed.connect(lambda:self.user_button_pressed(2))
        self.w.button_2.released.connect(lambda:self.user_button_released(2))
        self.w.button_3.pressed.connect(lambda:self.user_button_pressed(3))
        self.w.button_3.released.connect(lambda:self.user_button_released(3))
        self.w.button_4.pressed.connect(lambda:self.user_button_pressed(4))
        self.w.button_4.released.connect(lambda:self.user_button_released(4))
        self.w.button_5.pressed.connect(lambda:self.user_button_pressed(5))
        self.w.button_5.released.connect(lambda:self.user_button_released(5))
        self.w.button_6.pressed.connect(lambda:self.user_button_pressed(6))
        self.w.button_6.released.connect(lambda:self.user_button_released(6))
        self.w.button_7.pressed.connect(lambda:self.user_button_pressed(7))
        self.w.button_7.released.connect(lambda:self.user_button_released(7))
        self.w.button_8.pressed.connect(lambda:self.user_button_pressed(8))
        self.w.button_8.released.connect(lambda:self.user_button_released(8))
        self.w.button_9.pressed.connect(lambda:self.user_button_pressed(9))
        self.w.button_9.released.connect(lambda:self.user_button_released(9))
        self.w.button_10.pressed.connect(lambda:self.user_button_pressed(10))
        self.w.button_10.released.connect(lambda:self.user_button_released(10))
        self.w.button_11.pressed.connect(lambda:self.user_button_pressed(11))
        self.w.button_11.released.connect(lambda:self.user_button_released(11))
        self.w.button_12.pressed.connect(lambda:self.user_button_pressed(12))
        self.w.button_12.released.connect(lambda:self.user_button_released(12))
        self.w.button_13.pressed.connect(lambda:self.user_button_pressed(13))
        self.w.button_13.released.connect(lambda:self.user_button_released(13))
        self.w.button_14.pressed.connect(lambda:self.user_button_pressed(14))
        self.w.button_14.released.connect(lambda:self.user_button_released(14))
        self.w.button_15.pressed.connect(lambda:self.user_button_pressed(15))
        self.w.button_15.released.connect(lambda:self.user_button_released(15))
        self.w.button_16.pressed.connect(lambda:self.user_button_pressed(16))
        self.w.button_16.released.connect(lambda:self.user_button_released(16))
        self.w.button_17.pressed.connect(lambda:self.user_button_pressed(17))
        self.w.button_17.released.connect(lambda:self.user_button_released(17))
        self.w.button_18.pressed.connect(lambda:self.user_button_pressed(18))
        self.w.button_18.released.connect(lambda:self.user_button_released(18))
        self.w.button_19.pressed.connect(lambda:self.user_button_pressed(19))
        self.w.button_19.released.connect(lambda:self.user_button_released(19))
        self.w.button_20.pressed.connect(lambda:self.user_button_pressed(20))
        self.w.button_20.released.connect(lambda:self.user_button_released(20))
        self.w.cut_rec_speed.valueChanged.connect(lambda w:self.cutrec_speed_changed(w))
        self.w.kerf_width.valueChanged.connect(lambda w:self.cutrec_move_changed(w))
        self.w.jog_x_plus.pressed.connect(lambda:self.gui_button_jog(1, 'x', 1))
        self.w.jog_x_plus.released.connect(lambda:self.gui_button_jog(0, 'x', 1))
        self.w.jog_x_minus.pressed.connect(lambda:self.gui_button_jog(1, 'x', -1))
        self.w.jog_x_minus.released.connect(lambda:self.gui_button_jog(0, 'x', -1))
        self.w.jog_y_plus.pressed.connect(lambda:self.gui_button_jog(1, 'y', 1))
        self.w.jog_y_plus.released.connect(lambda:self.gui_button_jog(0, 'y', 1))
        self.w.jog_y_minus.pressed.connect(lambda:self.gui_button_jog(1, 'y', -1))
        self.w.jog_y_minus.released.connect(lambda:self.gui_button_jog(0, 'y', -1))
        self.w.jog_z_plus.pressed.connect(lambda:self.gui_button_jog(1, 'z', 1))
        self.w.jog_z_plus.released.connect(lambda:self.gui_button_jog(0, 'z', 1))
        self.w.jog_z_minus.pressed.connect(lambda:self.gui_button_jog(1, 'z', -1))
        self.w.jog_z_minus.released.connect(lambda:self.gui_button_jog(0, 'z', -1))
        self.w.jog_a_plus.pressed.connect(lambda:self.gui_button_jog(1, 'a', 1))
        self.w.jog_a_plus.released.connect(lambda:self.gui_button_jog(0, 'a', 1))
        self.w.jog_a_minus.pressed.connect(lambda:self.gui_button_jog(1, 'a', -1))
        self.w.jog_a_minus.released.connect(lambda:self.gui_button_jog(0, 'a', -1))
        self.w.jog_b_plus.pressed.connect(lambda:self.gui_button_jog(1, 'b', 1))
        self.w.jog_b_plus.released.connect(lambda:self.gui_button_jog(0, 'b', 1))
        self.w.jog_b_minus.pressed.connect(lambda:self.gui_button_jog(1, 'b', -1))
        self.w.jog_b_minus.released.connect(lambda:self.gui_button_jog(0, 'b', -1))
        self.w.cut_rec_fwd.pressed.connect(lambda:self.cutrec_motion(1))
        self.w.cut_rec_fwd.released.connect(lambda:self.cutrec_motion(0))
        self.w.cut_rec_rev.pressed.connect(lambda:self.cutrec_motion(-1))
        self.w.cut_rec_rev.released.connect(lambda:self.cutrec_motion(0))
        self.w.cut_rec_cancel.pressed.connect(lambda:self.cutrec_cancel_pressed(1))
        self.w.cut_rec_n.pressed.connect(lambda:self.cutrec_move(1, 0, 1))
        self.w.cut_rec_ne.pressed.connect(lambda:self.cutrec_move(1, 1, 1))
        self.w.cut_rec_e.pressed.connect(lambda:self.cutrec_move(1, 1, 0))
        self.w.cut_rec_se.pressed.connect(lambda:self.cutrec_move(1, 1, -1))
        self.w.cut_rec_s.pressed.connect(lambda:self.cutrec_move(1, 0, -1))
        self.w.cut_rec_sw.pressed.connect(lambda:self.cutrec_move(1, -1, -1))
        self.w.cut_rec_w.pressed.connect(lambda:self.cutrec_move(1, -1, 0))
        self.w.cut_rec_nw.pressed.connect(lambda:self.cutrec_move(1, -1, 1))
        self.xOffsetPin.value_changed.connect(lambda v:self.cutrec_offset_changed(v, self.yOffsetPin.get()))
        self.yOffsetPin.value_changed.connect(lambda v:self.cutrec_offset_changed(self.xOffsetPin.get(), v))
        self.offsetsActivePin.value_changed.connect(lambda v:self.offsets_active_changed(v))
        self.consChangePin.value_changed.connect(lambda v:self.consumable_change_changed(v))
        self.w.cam_mark.clicked.connect(self.cam_mark_clicked)
        self.w.cam_goto.clicked.connect(self.cam_goto_clicked)
        self.w.cam_zoom_plus.pressed.connect(self.cam_zoom_plus_pressed)
        self.w.cam_zoom_minus.pressed.connect(self.cam_zoom_minus_pressed)
        self.w.cam_dia_plus.pressed.connect(self.cam_dia_plus_pressed)
        self.w.cam_dia_minus.pressed.connect(self.cam_dia_minus_pressed)
        self.w.view_p.pressed.connect(self.view_p_pressed)
        self.w.view_z.pressed.connect(self.view_z_pressed)
        self.w.view_t.pressed.connect(self.view_t_pressed)
        self.w.view_clear.pressed.connect(self.view_clear_pressed)
        self.w.pan_left.pressed.connect(self.pan_left_pressed)
        self.w.pan_right.pressed.connect(self.pan_right_pressed)
        self.w.pan_up.pressed.connect(self.pan_up_pressed)
        self.w.pan_down.pressed.connect(self.pan_down_pressed)
        self.w.zoom_in.pressed.connect(self.zoom_in_pressed)
        self.w.zoom_out.pressed.connect(self.zoom_out_pressed)
        self.w.camera.pressed.connect(self.camera_pressed)
        self.w.laser.pressed.connect(self.laser_pressed)
        self.w.laser.clicked.connect(self.laser_clicked)
        self.w.main_tab_widget.currentChanged.connect(lambda w:self.main_tab_changed(w))
        self.zHeightPin.value_changed.connect(lambda v:self.z_height_changed(v))
        self.plasmacStatePin.value_changed.connect(lambda v:self.plasmac_state_changed(v))
        self.w.feed_label.pressed.connect(self.feed_label_pressed)
        self.w.rapid_label.pressed.connect(self.rapid_label_pressed)
        self.w.jogs_label.pressed.connect(self.jogs_label_pressed)
        self.paramTabDisable.value_changed.connect(lambda v:self.param_tab_changed(v))
        self.settingsTabDisable.value_changed.connect(lambda v:self.settings_tab_changed(v))
        self.convTabDisable.value_changed.connect(lambda v:self.conv_tab_changed(v))
        self.w.cut_time_reset.pressed.connect(lambda:self.statistic_reset('cut_time', 'Cut time'))
        self.w.probe_time_reset.pressed.connect(lambda:self.statistic_reset('probe_time', 'Probe time'))
        self.w.paused_time_reset.pressed.connect(lambda:self.statistic_reset('paused_time', 'Paused time'))
        self.w.run_time_reset.pressed.connect(lambda:self.statistic_reset('run_time', 'Program run time'))
        self.w.torch_time_reset.pressed.connect(lambda:self.statistic_reset('torch_time', 'Torch on time'))
        self.w.rapid_time_reset.pressed.connect(lambda:self.statistic_reset('rapid_time', 'Rapid time'))
        self.w.cut_length_reset.pressed.connect(lambda:self.statistic_reset('cut_length', 'Cut length'))
        self.w.pierce_reset.pressed.connect(lambda:self.statistic_reset('pierce_count', 'Pierce count'))
        self.w.all_reset.pressed.connect(self.statistics_reset)
        self.extPowerPin.value_changed.connect(lambda v:self.power_button("external", v))
        self.extRunPin.value_changed.connect(lambda v:self.ext_run(v))
        self.extPausePin.value_changed.connect(lambda v:self.ext_pause(v))
        self.extAbortPin.value_changed.connect(lambda v:self.ext_abort(v))
        self.extTouchOffPin.value_changed.connect(lambda v:self.ext_touch_off(v))
        self.extLaserTouchOffPin.value_changed.connect(lambda v:self.ext_laser_touch_off(v))
        self.extRunPausePin.value_changed.connect(lambda v:self.ext_run_pause(v))
        self.extHeightOvrPlusPin.value_changed.connect(lambda v:self.height_ovr_pressed(v,1))
        self.extHeightOvrMinusPin.value_changed.connect(lambda v:self.height_ovr_pressed(v,-1))
        self.extHeightOvrResetPin.value_changed.connect(lambda v:self.height_ovr_pressed(v,0))
        self.extHeightOvrCountsPin.value_changed.connect(lambda v:self.height_ovr_encoder(v))
        self.extHeightOvrScalePin.value_changed.connect(lambda v:self.height_ovr_scale_change(v))
        self.extChangeConsPin.value_changed.connect(lambda v:self.ext_change_consumables(v))
        self.extCutRecRevPin.value_changed.connect(lambda v:self.cutrec_motion(-v))
        self.extCutRecFwdPin.value_changed.connect(lambda v:self.cutrec_motion(v))
        self.extCutRecNPin.value_changed.connect(lambda v:self.cutrec_move(v, 0, 1))
        self.extCutRecNEPin.value_changed.connect(lambda v:self.cutrec_move(v, 1, 1))
        self.extCutRecEPin.value_changed.connect(lambda v:self.cutrec_move(v, 1, 0))
        self.extCutRecSEPin.value_changed.connect(lambda v:self.cutrec_move(v, 1, -1))
        self.extCutRecSPin.value_changed.connect(lambda v:self.cutrec_move(v, 0, -1))
        self.extCutRecSWPin.value_changed.connect(lambda v:self.cutrec_move(v, -1, -1))
        self.extCutRecWPin.value_changed.connect(lambda v:self.cutrec_move(v, -1, 0))
        self.extCutRecNWPin.value_changed.connect(lambda v:self.cutrec_move(v, -1, 1))
        self.extCutRecCancelPin.value_changed.connect(lambda v:self.cutrec_cancel_pressed(v))
        self.extTorchEnablePin.value_changed.connect(lambda v:self.ext_torch_enable_changed(v))
        self.extThcEnablePin.value_changed.connect(lambda v:self.ext_thc_enable_changed(v))
        self.extCornerLockEnablePin.value_changed.connect(lambda v:self.ext_corner_lock_enable_changed(v))
        self.extVoidLockEnablePin.value_changed.connect(lambda v:self.ext_void_lock_enable_changed(v))
        self.extIgnoreArcOkPin.value_changed.connect(lambda v:self.ext_ignore_arc_ok_changed(v))
        self.extMeshModePin.value_changed.connect(lambda v:self.ext_mesh_mode_changed(v))
        self.extOhmicProbeEnablePin.value_changed.connect(lambda v:self.ext_ohmic_probe_enable_changed(v))
        self.extAutoVoltsEnablePin.value_changed.connect(lambda v:self.ext_auto_volts_enable_changed(v))
        self.extJogSlowPin.value_changed.connect(self.ext_jog_slow)
        self.extProbePin.value_changed.connect(lambda v:self.ext_probe_test(v))
        self.extPulsePin.value_changed.connect(lambda v:self.ext_torch_pulse(v))
        self.extOhmicPin.value_changed.connect(lambda v:self.ext_ohmic_test(v))
        self.extFramingPin.value_changed.connect(lambda v:self.ext_frame_job(v))
        self.probeTestErrorPin.value_changed.connect(lambda v:self.probe_test_error(v))
        self.w.preview_stack.currentChanged.connect(self.preview_stack_changed)
        self.w.gcode_stack.currentChanged.connect(self.gcode_stack_changed)
        click_signal(self.w.material_label).connect(self.show_material_selector)
        click_signal(self.w.velocity_label).connect(self.show_material_selector)
        click_signal(self.w.velocity_show).connect(self.show_material_selector)
        self.w.conv_line.pressed.connect(lambda:self.conv_call('line'))
        self.w.conv_circle.pressed.connect(lambda:self.conv_call('circle'))
        self.w.conv_ellipse.pressed.connect(lambda:self.conv_call('ellipse'))
        self.w.conv_triangle.pressed.connect(lambda:self.conv_call('triangle'))
        self.w.conv_rectangle.pressed.connect(lambda:self.conv_call('rectangle'))
        self.w.conv_polygon.pressed.connect(lambda:self.conv_call('polygon'))
        self.w.conv_bolt.pressed.connect(lambda:self.conv_call('bolt'))
        self.w.conv_slot.pressed.connect(lambda:self.conv_call('slot'))
        self.w.conv_star.pressed.connect(lambda:self.conv_call('star'))
        self.w.conv_gusset.pressed.connect(lambda:self.conv_call('gusset'))
        self.w.conv_sector.pressed.connect(lambda:self.conv_call('sector'))
        self.w.conv_block.pressed.connect(lambda:self.conv_call('block'))
        self.w.conv_new.pressed.connect(lambda:self.conv_call('new'))
        self.w.conv_save.pressed.connect(lambda:self.conv_call('save'))
        self.w.conv_settings.pressed.connect(lambda:self.conv_call('settings'))
        self.w.conv_send.pressed.connect(lambda:self.conv_call('send'))
        self.w.chk_override_jog.stateChanged.connect(self.override_jog_changed)
        self.jogInhibited.value_changed.connect(lambda v:self.jog_inhibited_changed(v))
        self.sensorActive.value_changed.connect(lambda v:self.sensor_active_changed(v))
        self.zOffsetPin.value_changed.connect(lambda v:self.z_offset_changed(v))
        self.laserRecStatePin.value_changed.connect(lambda v:self.laser_recovery_state_changed(v))
        self.ohmicLedInPin.value_changed.connect(lambda v:self.ohmic_sensed(v))

    def conv_call(self, operation):
        if self.developmentPin.get():
            reload(self.CONV)
        if operation == 'block':
            self.CONV.conv_block_pressed(self, self.w)
        elif operation == 'new':
            self.CONV.conv_new_pressed(self, self.w, 'button')
        elif operation == 'save':
            self.CONV.conv_save_pressed(self, self.w)
        elif operation == 'settings':
            self.CONV.conv_settings_pressed(self, self.w)
        elif operation == 'send':
            self.CONV.conv_send_pressed(self, self.w)
        else:
            self.CONV.conv_shape_request(self, self.w, 'conv_{}'.format(operation), True)

    def set_axes_and_joints(self):
        kinematics = self.iniFile.find('KINS', 'KINEMATICS').lower().replace('=','').replace('trivkins','').replace(' ','') or None
        #kinstype = None
        self.coordinates = 'xyz'
        if kinematics:
            if 'kinstype' in kinematics:
                #kinstype = kinematics.lower().replace(' ','').split('kinstype')[1]
                if 'coordinates' in kinematics:
                    kinematics = kinematics.lower().replace(' ','').split('kinstype')[0]
            if 'coordinates' in kinematics:
                self.coordinates = kinematics.split('coordinates')[1].lower()
        else:
            head = _translate('HandlerClass', 'INI File Error')
            msg0  = _translate('HandlerClass', 'Error in [KINS]KINEMATICS in the INI file')
            msg1 = _translate('HandlerClass', 'reverting to default coordinates of xyz')
            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n{}\n'.format(head, msg0, msg1))
        # hide axis a if not being used
        if 'a' not in self.axisList:
            for i in self.axisAList:
                self.w[i].hide()
        # hide axis b if not being used
        if 'b' not in self.axisList:
            for i in self.axisBList:
                self.w[i].hide()
        # setup home buttons
        for axis in self.axisList:
            self.w['home_{}'.format(axis)].set_joint(self.coordinates.index(axis))
            self.w['home_{}'.format(axis)].set_joint_number(self.coordinates.index(axis))
        for joint in range(len(self.coordinates)):
            # check if home all button required
            if not self.iniFile.find('JOINT_{}'.format(joint), 'HOME_SEQUENCE'):
                self.w.home_all.hide()
            # check if not joggable before homing
            elif self.iniFile.find('JOINT_{}'.format(joint), 'HOME_SEQUENCE').startswith('-'):
                if 'jog_{}_plus'.format(self.coordinates[joint]) not in self.jogSyncList:
                    self.jogSyncList.append('jog_{}_plus'.format(self.coordinates[joint]))
                    self.jogSyncList.append('jog_{}_minus'.format(self.coordinates[joint]))
                    self.jogButtonList.remove('jog_{}_plus'.format(self.coordinates[joint]))
                    self.jogButtonList.remove('jog_{}_minus'.format(self.coordinates[joint]))

    def set_mode(self):
        block1 = ['arc_ok_high', 'arc_ok_high_lbl', 'arc_ok_low', 'arc_ok_low_lbl' ]
        block2 = ['arc_voltage_scale', 'arc_voltage_scale_lbl', 'arc_voltage_offset', 'arc_voltage_offset_lbl',
                  'voidlock_frm', 'height_per_volt', 'height_per_volt_lbl', 'thc_delay', 'thc_delay_lbl',
                  'thc_sample_counts', 'thc_sample_counts_lbl', 'thc_sample_threshold', 'thc_sample_threshold_lbl',
                  'thc_threshold', 'thc_threshold_lbl', 'pid_i_gain', 'pid_i_gain_lbl', 'pid_d_gain', 'pid_d_gain_lbl',
                  'use_auto_volts', 'use_auto_volts_lbl', 'led_thc_active', 'led_thc_active_lbl', 'arc_voltage',
                  'arc_override_frm', 'voidlock_slope', 'voidlock_slope_lbl', 'thc_auto', 'thc_auto_lbl' ]
        if self.mode == 1:
            hal.set_p('plasmac.mode', '1')
            for widget in block1:
                self.w[widget].hide()
        elif self.mode == 2:
            hal.set_p('plasmac.mode', '2')
            for widget in block1 + block2:
                self.w[widget].hide()
                self.w.pid_p_gain_lbl.setText(_translate('HandlerClass', 'Speed %'))

    def set_spinbox_parameters(self):
        self.w.max_offset_velocity_in.setText('{}'.format(int(self.thcFeedRate)))
        if self.units == 'in':
            self.w.setup_feed_rate.setRange(4.0, int(self.thcFeedRate))
            self.w.setup_feed_rate.setDecimals(1)
            self.w.setup_feed_rate.setSingleStep(0.1)
            self.w.safe_height.setRange(0.75, int(self.maxHeight))
            self.w.safe_height.setDecimals(2)
            self.w.safe_height.setSingleStep(0.01)
            self.w.probe_feed_rate.setRange(4.0, int(self.thcFeedRate))
            self.w.probe_feed_rate.setDecimals(1)
            self.w.probe_feed_rate.setSingleStep(0.1)
            self.w.probe_start_height.setRange(0.1, int(self.maxHeight))
            self.w.probe_start_height.setDecimals(2)
            self.w.probe_start_height.setSingleStep(0.01)
            self.w.float_switch_travel.setRange(-1.0, 1.0)
            self.w.float_switch_travel.setDecimals(3)
            self.w.float_switch_travel.setSingleStep(0.001)
            self.w.height_per_volt.setRange(0.001, 0.01)
            self.w.height_per_volt.setDecimals(3)
            self.w.height_per_volt.setSingleStep(0.001)
            self.w.ohmic_probe_offset.setRange(-1.0, 1.0)
            self.w.ohmic_probe_offset.setDecimals(3)
            self.w.ohmic_probe_offset.setSingleStep(0.001)
            self.w.skip_ihs_distance.setRange(0.0, 40.0)
            self.w.skip_ihs_distance.setDecimals(1)
            self.w.skip_ihs_distance.setSingleStep(0.1)
            self.w.kerf_width.setRange(0.0, 1.0)
            self.w.kerf_width.setDecimals(3)
            self.w.kerf_width.setSingleStep(0.001)
#            self.w.cut_feed_rate.setRange(0.0, 999.0)
            self.w.cut_feed_rate.setDecimals(1)
            self.w.cut_feed_rate.setSingleStep(0.1)
#            self.w.cut_height.setRange(0.0, 1.0)
            self.w.cut_height.setDecimals(3)
            self.w.cut_height.setSingleStep(0.001)
#            self.w.pierce_height.setRange(0.0, 1.0)
            self.w.pierce_height.setDecimals(3)
            self.w.pierce_height.setSingleStep(0.001)
        else:
            self.w.setup_feed_rate.setMaximum(int(self.thcFeedRate))
            self.w.safe_height.setMaximum(int(self.maxHeight))
            self.w.probe_feed_rate.setMaximum(int(self.thcFeedRate))
            self.w.probe_start_height.setMaximum(int(self.maxHeight))

    def set_probe_offset_pins(self):
        hal.set_p('plasmac.offset-probe-x', '{}'.format(self.probeOffsetX))
        hal.set_p('plasmac.offset-probe-y', '{}'.format(self.probeOffsetY))
        hal.set_p('plasmac.offset-probe-delay', '{}'.format(self.probeDelay))

    def kb_jog(self, state, joint, direction, shift = False, linear = True):
        if linear:
            distance = STATUS.get_jog_increment()
            rate = STATUS.get_jograte()/60
        else:
            distance = STATUS.get_jog_increment_angular()
            rate = STATUS.get_jograte_angular()/60
        if state:
            if not STATUS.is_man_mode() or not STATUS.machine_is_on() or \
               (self.offsetsActivePin.get() and not self.manualCut):
                return
            if (shift or self.jogFast) and not self.manualCut:
                rate = INFO.MAX_LINEAR_JOG_VEL
            elif self.jogSlow and not self.w.jog_slow.isChecked():
                rate = STATUS.get_jograte()/60/self.slowJogFactor
            ACTION.JOG(joint, direction, rate, distance)
            self.w.grabKeyboard()
        else:
            self.w.releaseKeyboard()
            if not STATUS.get_jog_increment():
                ACTION.JOG(joint, 0, 0, 0)

    def keyboard_shortcuts(self):
        if self.w.chk_keyboard_shortcuts.isChecked():
            return True
        else:
            return False

    def soft_keyboard(self):
        if self.w.chk_soft_keyboard.isChecked():
            inputType = 'CALCULATOR'
            self.w.originoffsetview.setProperty('dialog_code_string','CALCULATOR')
            self.w.originoffsetview.setProperty('text_dialog_code_string','KEYBOARD')
            self.w.gcode_display.SendScintilla(QsciScintilla.SCI_SETEXTRAASCENT, 4)
            self.w.gcode_display.SendScintilla(QsciScintilla.SCI_SETEXTRADESCENT, 4)
            self.w.gcode_editor.editor.SendScintilla(QsciScintilla.SCI_SETEXTRAASCENT, 4)
            self.w.gcode_editor.editor.SendScintilla(QsciScintilla.SCI_SETEXTRADESCENT, 4)
            self.vkb_check()
            if self.w.main_tab_widget.currentIndex() == 2:
                self.vkb_show(True)
            elif self.w.main_tab_widget.currentIndex() == 3:
                self.vkb_show()
            self.w.chk_keyboard_shortcuts.setChecked(False)
            self.w.chk_keyboard_shortcuts.setEnabled(False)
        else:
            inputType = 'ENTRY'
            self.w.originoffsetview.setProperty('dialog_code_string','')
            self.w.originoffsetview.setProperty('text_dialog_code_string','')
            self.w.gcode_display.SendScintilla(QsciScintilla.SCI_SETEXTRAASCENT, 1)
            self.w.gcode_display.SendScintilla(QsciScintilla.SCI_SETEXTRADESCENT, 1)
            self.w.gcode_editor.editor.SendScintilla(QsciScintilla.SCI_SETEXTRAASCENT, 1)
            self.w.gcode_editor.editor.SendScintilla(QsciScintilla.SCI_SETEXTRADESCENT, 1)
            self.vkb_hide()
            self.w.chk_keyboard_shortcuts.setEnabled(True)
        for axis in 'xyzab':
            button = 'touch_{}'.format(axis)
            self.w[button].dialog_code = inputType

    def overlay_update(self, state):
        self.w.gcodegraphics.updateGL()
        self.w.conv_preview.updateGL()

    def dialog_show_ok(self, icon, title, error, bText=_translate('HandlerClass', 'OK')):
        msg = QMessageBox(self.w)
        buttonY = msg.addButton(QMessageBox.Yes)
        buttonY.setText(bText)
        buttonY.setIcon(QIcon())
        msg.setIcon(icon)
        msg.setWindowTitle(title)
        msg.setText(error)
        msg.setWindowFlag(Qt.Popup)
        msg.exec_()
        self.dialogError = False
        return msg

    def dialog_show_yesno(self, icon, title, error, bY=_translate('HandlerClass', 'YES'), bN=_translate('HandlerClass', 'NO')):
        msg = QMessageBox(self.w)
        buttonY = msg.addButton(QMessageBox.Yes)
        buttonY.setText(bY)
        buttonY.setIcon(QIcon())
        buttonN = msg.addButton(QMessageBox.No)
        buttonN.setText(bN)
        buttonN.setIcon(QIcon())
        msg.setIcon(icon)
        msg.setWindowTitle(title)
        msg.setText(error)
        msg.setWindowFlag(Qt.WindowStaysOnTopHint) if 'shutdown' in error else msg.setWindowFlag(Qt.Popup)
        choice = msg.exec_()
        if choice == QMessageBox.Yes:
            return True
        else:
            return False
    # virtkb: 0=none, 1=alpha~close, 2=num~close, 3=alpha~num, 4=num~num, 5=alpha~alpha, 6=num~alpha
    def dialog_input(self, virtkb, title, text, btn1, btn2, delay=None):
        input = QInputDialog(self.w)
        input.setWindowTitle(title)
        input.setLabelText('{}'.format(text))
        if btn1:
            input.setOkButtonText(btn1)
        if btn2:
            input.setCancelButtonText(btn2)
        if delay is not None:
            input.setTextValue('{:0.2f}'.format(delay))
        for button in input.findChildren(QPushButton):
            button.setIcon(QIcon())
        if virtkb in (1,3,5):
            self.vkb_show(False)
        elif virtkb in (2,4,6):
            self.vkb_show(True)
        valid = input.exec_()
        if virtkb < 3:
            self.vkb_hide()
        elif virtkb in (3,4):
            self.vkb_show(True)
        elif virtkb in (5,6):
            self.vkb_show(False)
        out = input.textValue()
        return valid, out

    def dialog_run_from_line(self):
        rFl = QDialog(self.w)
        rFl.setWindowTitle(_translate('HandlerClass', 'Run From Line'))
        lbl1 = QLabel(_translate('HandlerClass', 'USE LEADIN:'))
        lbl2 = QLabel(_translate('HandlerClass', 'LEADIN LENGTH:'))
        lbl3 = QLabel(_translate('HandlerClass', 'LEADIN ANGLE:'))
        lbl4 = QLabel('')
        leadinDo = QCheckBox()
        leadinLength = QDoubleSpinBox()
        leadinAngle = QDoubleSpinBox()
        buttons = QDialogButtonBox.Ok | QDialogButtonBox.Cancel
        buttonBox = QDialogButtonBox(buttons)
        buttonBox.accepted.connect(rFl.accept)
        buttonBox.rejected.connect(rFl.reject)
        buttonBox.button(QDialogButtonBox.Ok).setText(_translate('HandlerClass', 'LOAD'))
        buttonBox.button(QDialogButtonBox.Ok).setIcon(QIcon())
        buttonBox.button(QDialogButtonBox.Cancel).setText(_translate('HandlerClass', 'CANCEL'))
        buttonBox.button(QDialogButtonBox.Cancel).setIcon(QIcon())
        layout = QGridLayout()
        layout.addWidget(lbl1, 0, 0)
        layout.addWidget(lbl2, 1, 0)
        layout.addWidget(lbl3, 2, 0)
        layout.addWidget(lbl4, 3, 0)
        layout.addWidget(leadinDo, 0, 1)
        layout.addWidget(leadinLength, 1, 1)
        layout.addWidget(leadinAngle, 2, 1)
        layout.addWidget(buttonBox, 4, 0, 1, 2)
        rFl.setLayout(layout)
        lbl1.setAlignment(Qt.AlignRight | Qt.AlignBottom)
        lbl2.setAlignment(Qt.AlignRight | Qt.AlignBottom)
        lbl3.setAlignment(Qt.AlignRight | Qt.AlignBottom)
        if self.units == 'in':
            leadinLength.setDecimals(2)
            leadinLength.setSingleStep(0.05)
            leadinLength.setSuffix(' inch')
            leadinLength.setMinimum(0.05)
        else:
            leadinLength.setDecimals(0)
            leadinLength.setSingleStep(1)
            leadinLength.setSuffix(' mm')
            leadinLength.setMinimum(1)
        leadinAngle.setDecimals(0)
        leadinAngle.setSingleStep(1)
        leadinAngle.setSuffix(' deg')
        leadinAngle.setRange(-359, 359)
        leadinAngle.setWrapping(True)
        self.vkb_show(True)
        result = rFl.exec_()
        self.vkb_hide()
        # load clicked
        if result:
            return {'cancel':False, 'do':leadinDo.isChecked(), 'length':leadinLength.value(), 'angle':leadinAngle.value()}
        # cancel clicked
        else:
            return {'cancel':True}


    def invert_pin_state(self, halpin):
        if 'qtplasmac.ext_out_' in halpin:
            pin = 'out{}Pin'.format(halpin.split('out_')[1])
            self[pin].set(not hal.get_value(halpin))
        else:
            hal.set_p(halpin, str(not hal.get_value(halpin)))
        self.set_button_color()

    def set_button_color(self):
        for halpin in self.halTogglePins:
            color = self.w[self.halTogglePins[halpin][0]].palette().color(QtGui.QPalette.Background)
            if hal.get_value(halpin):
                if color != self.w.color_foregalt.palette().color(QPalette.Background):
                    self.button_active(self.halTogglePins[halpin][0])
            else:
                if color != self.w.color_backgrnd.palette().color(QPalette.Background):
                    self.button_normal(self.halTogglePins[halpin][0])
        for halpin in self.halPulsePins:
            color = self.w[self.halPulsePins[halpin][0]].palette().color(QtGui.QPalette.Background)
            if hal.get_value(halpin):
                if color != self.w.color_foregalt.palette().color(QPalette.Background):
                    self.button_active(self.halPulsePins[halpin][0])
            else:
                if color != self.w.color_backgrnd.palette().color(QPalette.Background):
                    self.button_normal(self.halPulsePins[halpin][0])
        if self.tlButton:
            if self.laserOnPin.get():
                self.button_active(self.tlButton)
            else:
                self.button_normal(self.tlButton)


    def run_critical_check(self):
        rcButtonList = []
        # halTogglePins format is: button name, run critical flag, button text
        for halpin in self.halTogglePins:
            if self.halTogglePins[halpin][1] and not hal.get_value(halpin):
                rcButtonList.append(self.halTogglePins[halpin][2].replace('\n', ' '))
        if rcButtonList and self.w.torch_enable.isChecked():
            head = _translate('HandlerClass', 'Run Critical Toggle')
            btn1 = _translate('HandlerClass', 'CONTINUE')
            btn2 = _translate('HandlerClass', 'CANCEL')
            msg0 = _translate('HandlerClass', 'Button not toggled')
            msg1 = '\n{}'.format('\n'.join(rcButtonList))
            if self.dialog_show_yesno(QMessageBox.Warning, '{}'.format(head), '\n{}:\n{}'.format(msg0, msg1), '{}'.format(btn1), '{}'.format(btn2)):
                return False
            else:
                return True
        else:
            return False

    def preview_stack_changed(self):
        if self.w.preview_stack.currentIndex() == 2:
            self.button_active(self.w.file_edit.objectName())
            text0 = _translate('HandlerClass', 'EDIT')
            text1 = _translate('HandlerClass', 'CLOSE')
            self.w.file_edit.setText('{}\n{}'.format(text0, text1))
            self.w.file_clear.setEnabled(False)
            self.w.file_open.setEnabled(False)
            self.w.file_reload.setEnabled(False)
            self.autorepeat_keys(True)
            self.w.jog_frame.setEnabled(False)
        else:
            self.button_normal(self.w.file_edit.objectName())
            self.w.file_edit.setText(_translate('HandlerClass', 'EDIT'))
            self.w.file_clear.setEnabled(True)
            self.w.file_open.setEnabled(True)
            self.w.file_reload.setEnabled(True)
            if self.w.gcode_stack.currentIndex() != 1:
                self.autorepeat_keys(False)
                self.w.jog_frame.setEnabled(True)

    def gcode_stack_changed(self):
        if self.w.gcode_stack.currentIndex() == 1:
            self.button_active(self.w.mdi_show.objectName())
            text0 = _translate('HandlerClass', 'MDI')
            text1 = _translate('HandlerClass', 'CLOSE')
            self.w.mdi_show.setText('{}\n{}'.format(text0, text1))
            self.w.mdihistory.reload()
            self.w.mdihistory.MDILine.setFocus()
            self.autorepeat_keys(True)
            self.w.jog_frame.setEnabled(False)
        else:
            self.button_normal(self.w.mdi_show.objectName())
            self.w.mdi_show.setText(_translate('HandlerClass', 'MDI'))
            if self.w.preview_stack.currentIndex() != 2:
                self.autorepeat_keys(False)
                self.w.jog_frame.setEnabled(True)

    def set_mc_states(self, state):
        if self.manualCut:
            self.jogPreManCut[0] = self.w.jog_slow.isChecked()
            self.jogPreManCut[1] = self.w.jog_slider.value()
            self.jogPreManCut[2] = self.w.jogincrements.currentIndex()
            if self.w.jog_slow.isChecked():
                self.jog_slow_pressed(True)
            self.w.jog_slider.setValue(self.w.cut_feed_rate.value())
            self.w.jogincrements.setCurrentIndex(0)
        else:
            if self.jogPreManCut[0]:
                self.jog_slow_pressed(True)
            self.w.jog_slider.setValue(self.jogPreManCut[1])
            self.w.jogincrements.setCurrentIndex(self.jogPreManCut[2])
        self.w.jog_z_plus.setEnabled(state)
        self.w.jog_z_minus.setEnabled(state)
        self.set_tab_jog_states(state)

    def set_tab_jog_states(self, state):
        if STATUS.is_auto_paused():
            if self.torchPulse:
                self.w.pause.setEnabled(state)
            for n in range(self.w.main_tab_widget.count()):
                if n > 1:
                    self.w.main_tab_widget.setTabEnabled(n, state)
        else:
            if self.tabsAlwaysEnabled.get():
                self.w.main_tab_widget.setTabEnabled(1, state)
            else:
                for n in range(self.w.main_tab_widget.count()):
                    if n != 0 and (not self.probeTest or n != self.w.main_tab_widget.currentIndex()):
                         self.w.main_tab_widget.setTabEnabled(n, state)
            # disable jog controls on MAIN tab (for manual cut)
            self.w.jog_slider.setEnabled(state)
            self.w.jogs_label.setEnabled(state)
            self.w.jog_slow.setEnabled(state)
            self.w.jogincrements.setEnabled(state)
            # disable material selector on MAIN tab
            self.w.material_label.setEnabled(state)
            self.w.material_selector.setEnabled(state)
            # disable materials items on PARAMETERS tab
            self.w.materials_box.setEnabled(state)
            self.w.save_material.setEnabled(state)
            self.w.new_material.setEnabled(state)
            self.w.delete_material.setEnabled(state)
            self.w.reload_material.setEnabled(state)
            self.w.cut_amps.setEnabled(state)
            self.w.cut_feed_rate.setEnabled(state)
            self.w.cut_height.setEnabled(state)
            self.w.cut_mode.setEnabled(state)
            self.w.cut_mode_label.setEnabled(state)
            self.w.cut_volts.setEnabled(state)
            self.w.gas_pressure.setEnabled(state)
            self.w.kerf_width.setEnabled(state)
            self.w.pause_at_end.setEnabled(state)
            self.w.pierce_delay.setEnabled(state)
            self.w.pierce_height.setEnabled(state)
            self.w.puddle_jump_delay.setEnabled(state)
            self.w.puddle_jump_height.setEnabled(state)
            # disable user button items on SETTINGS tab
            self.w.ub_save.setEnabled(state)
            self.w.ub_reload.setEnabled(state)
            for bNum in range(1,21):
                self.w['ub_name_{}'.format(bNum)].setEnabled(state)
                self.w['ub_code_{}'.format(bNum)].setEnabled(state)
            # disable Jog Frame during probe test or torch pulse
            if self.probeTest or self.torchPulse:
                self.w.jog_frame.setEnabled(state)

    def show_material_selector(self):
        self.w.material_selector.showPopup()

    def autorepeat_keys(self, state):
        if not self.autorepeat_skip:
            if state:
                ACTION.ENABLE_AUTOREPEAT_KEYS(' ')
            else:
                ACTION.DISABLE_AUTOREPEAT_KEYS(' ')

    def clear_rfl(self):
        self.rflActive = False
        self.startLine = 0
        self.preRflFile = ''
        self.w.gcodegraphics.clear_highlight()

    def ohmic_sensed(self, state):
        if state:
            hal.set_p('qtplasmac.led_ohmic_probe', '1')
            self.ohmicLedTimer.start(150)

    def vm_check(self):
        try:
            response = (Popen('cat /sys/class/dmi/id/product_name', stdout=PIPE, stderr=PIPE, shell=True).communicate()[0]).decode('utf-8')
            if 'virtual' in response.lower() or 'vmware' in response.lower():
                self.virtualMachine = True
                STATUS.emit('update-machine-log', '"{}" Virtual Machine detected'.format(response.strip()), 'TIME')
        except:
            pass

#########################################################################################################################
# TIMER FUNCTIONS #
#########################################################################################################################
    def shutdown_timeout(self):
        self.w.close()

    def startup_timeout(self):
        if STATUS.stat.estop:
            self.estop_state(True)
        self.w.run.setEnabled(False)
        if self.frButton:
            self.w[self.frButton].setEnabled(False)
        self.w.pause.setEnabled(False)
        self.w.abort.setEnabled(False)
        self.w.gcode_display.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)
        self.view_t_pressed()
        self.set_signal_connections()
        if self.firstRun is True:
            self.firstRun = False

    def update_periodic(self):
        if self.framing and STATUS.is_interp_idle():
            self.framing = False
            ACTION.SET_MANUAL_MODE()
            self.laserOnPin.set(0)
            self.w.gcodegraphics.logger.clear()
        self.set_button_color()
        if self.flasher:
            self.flasher -= 1
        else:
            self.flasher = self.flashRate
            self.flashState = not self.flashState
            self.flasher_timeout()
        if not self.firstRun and self.pmPort and not hal.component_exists('pmx485'):
            if self.pmx485_check(self.pmPort, True):
                self.w.gas_pressure.show()
                self.w.gas_pressure_label.show()
                self.w.cut_mode.show()
                self.w.cut_mode_label.show()
                self.w.pmx485_frame.show()
                self.w.pmx_stats_frame.show()
                self.pmx485_startup(self.pmPort)
        if not self.firstRun and os.path.isfile(self.upFile):
            exec(open(self.upFile).read())

    def flasher_timeout(self):
        if STATUS.is_auto_paused():
            if self.flashState:
                self.w.pause.setText(_translate('HandlerClass', 'CYCLE RESUME'))
            else:
                self.w.pause.setText('')
        elif not self.w.jog_stack.currentIndex():
            self.w.pause.setText(_translate('HandlerClass', 'CYCLE PAUSE'))
        text = _translate('HandlerClass', 'FEED')
        if self.w.feed_slider.value() != 100:
            if self.flashState:
                self.w.feed_label.setText('{}\n{:.0f}%'.format(text, STATUS.stat.feedrate * 100))
            else:
                self.w.feed_label.setText(' \n ')
        else:
            self.w.feed_label.setText('{}\n{:.0f}%'.format(text, STATUS.stat.feedrate * 100))
        text = _translate('HandlerClass', 'RAPID')
        if self.w.rapid_slider.value() != 100:
            if self.flashState:
                self.w.rapid_label.setText('{}\n{:.0f}%'.format(text, STATUS.stat.rapidrate * 100))
            else:
                self.w.rapid_label.setText(' \n ')
        else:
            self.w.rapid_label.setText('{}\n{:.0f}%'.format(text, STATUS.stat.rapidrate * 100))
        text = _translate('HandlerClass', 'JOG')
        if self.manualCut:
            if self.flashState:
                self.w.run.setText(_translate('HandlerClass', 'MANUAL CUT'))
                self.w.jogs_label.setText('{}\n{:.0f}'.format(text, STATUS.get_jograte()))
            else:
                self.w.run.setText('')
                self.w.jogs_label.setText(' \n ')
        if self.heightOvr > 0.01 or self.heightOvr < -0.01:
            if self.flashState:
                self.w.height_ovr_label.setText('{:.2f}'.format(self.heightOvr))
            else:
                self.w.height_ovr_label.setText('')
        else:
            self.w.height_ovr_label.setText('{:.2f}'.format(self.heightOvr))
        if self.flash_error and self.error_present:
            if self.flashState:
                self.w.error_label.setText(_translate('HandlerClass', 'ERROR SENT TO MACHINE LOG'))
            else:
                self.w.error_label.setText('')
        if self.startLine > 0:
            if not self.w.run.text().startswith(_translate('HandlerClass', 'RUN')):
                if self.flashState:
                    self.w.run.setText(self.runText)
                else:
                    self.w.run.setText('')
        elif not self.manualCut:
            self.w.run.setText(_translate('HandlerClass', 'CYCLE START'))
        if not self.w.pmx485_enable.isChecked():
            self.w.pmx485_label.setText('')
            self.pmx485LabelState = None
        elif self.pmx485CommsError:
            if self.flashState:
                self.w.pmx485_label.setText(_translate('HandlerClass', 'COMMS ERROR'))
                self.pmx485LabelState = None
            else:
                self.w.pmx485_label.setText('')
                self.pmx485LabelState = None
        elif not self.pmx485LabelState:
            if self.flashState:
                self.w.pmx485_label.setText('Fault Code: {}'.format(self.pmx485FaultCode))
                self.pmx485LabelState = None
            else:
                self.w.pmx485_label.setText('')
                self.pmx485LabelState = None

    def probe_timeout(self):
        if self.probeTime > 1:
            self.probeTime -= 1
            self.probeTimer.start(1000)
            self.w[self.ptButton].setText('{}'.format(self.probeTime))
        else:
            self.probe_test_stop()
            log = _translate('HandlerClass', 'Probe test completed')
            STATUS.emit('update-machine-log', log, 'TIME')

    def torch_timeout(self):
        if self.torchTime:
            self.torchTime -= 0.1
            self.torchTimer.start(100)
            self.w[self.tpButton].setText('{:.1f}'.format(self.torchTime))
        if self.torchTime <= 0:
            self.torchTimer.stop()
            self.torchTime = 0
            if not self.w[self.tpButton].isDown() and not self.extPulsePin.get():
                self.torch_pulse_states(True)
                log = _translate('HandlerClass', 'Torch pulse completed')
                STATUS.emit('update-machine-log', log, 'TIME')
            else:
                text0 = _translate('HandlerClass', 'TORCH')
                text1 = _translate('HandlerClass', 'ON')
                self.w[self.tpButton].setText('{}\n{}'.format(text0, text1))
        else:
            self.torchTimer.start(100)

    def pulse_timer_timeout(self):
        # halPulsePins format is: button name, pulse time, button text, remaining time, button number
        active = False
        try:
            for halpin in self.halPulsePins:
                if self.halPulsePins[halpin][3] > 0.05:
                    active = True
                    if self.halPulsePins[halpin][1] == self.halPulsePins[halpin][3]:
                        self.invert_pin_state(halpin)
                    self.halPulsePins[halpin][3] -= 0.1
                    self.w[self.halPulsePins[halpin][0]].setText('{:0.1f}'.format(self.halPulsePins[halpin][3]))
                elif self.w[self.halPulsePins[halpin][0]].text() != self.halPulsePins[halpin][2]:
                    self.invert_pin_state(halpin)
                    self.halPulsePins[halpin][3] = 0
                    self.w[self.halPulsePins[halpin][0]].setText('{}'.format(self.halPulsePins[halpin][2]))
        except Exception as err:
            self.halPulsePins[halpin][3] = 0
            bNum = self.halPulsePins[halpin][4]
            head = _translate('HandlerClass', 'HAL Pin Error')
            msg0 = _translate('HandlerClass', 'Invalid code for user button')
            msg1 = _translate('HandlerClass', 'Failed to pulse HAL pin')
            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}\n"{}" {}\n'.format(head, msg0, bNum, msg1, halpin, err))
        if not active:
            self.pulseTimer.stop()

    def laser_timeout(self):
        if self.w.laser.isDown() or self.extLaserButton:
            self.laserOnPin.set(0)
            self.laserButtonState = 'reset'
            self.w.laser.setText(_translate('HandlerClass', 'LASER'))
            self.button_normal('laser')
            self.w.touch_xy.setEnabled(True)
            self.w.camera.setEnabled(True)

    def ohmic_led_timeout(self):
        if not self.ohmicLedInPin.get():
            hal.set_p('qtplasmac.led_ohmic_probe', '0')
        else:
            self.ohmicLedTimer.start(50)

#########################################################################################################################
# USER BUTTON FUNCTIONS #
#########################################################################################################################
    def user_button_setup(self):
        self.iniButtonCodes = ['Codes']
        iniButtonCodes = ['Codes']
        self.probePressed = False
        self.probeTime = 0
        self.probeTimer = QTimer()
        self.probeTimer.setSingleShot(True)
        self.probeTimer.timeout.connect(self.probe_timeout)
        self.torchTime = 0.0
        self.torchTimer = QTimer()
        self.torchTimer.setSingleShot(True)
        self.torchTimer.timeout.connect(self.torch_timeout)
        self.pulseTime = 0
        self.pulseTimer = QTimer()
        self.pulseTimer.timeout.connect(self.pulse_timer_timeout)
        self.cutType = 0
        self.single_cut_request = False
        self.oldFile = None
        singleCodes = ['change-consumables', 'cut-type', 'framing', 'manual-cut', 'offsets-view', 'ohmic-test', 'probe-test', 'single-cut', 'torch-pulse']
        head = _translate('HandlerClass', 'User Button Error')
        for bNum in range(1,21):
            self.w['button_{}'.format(str(bNum))].setEnabled(False)
            bName = self.PREFS.getpref('{} Name'.format(bNum), '', str, 'BUTTONS') or None
            bCode = self.PREFS.getpref('{} Code'.format(bNum), '', str, 'BUTTONS') or None
            if bName or bCode:
                self.w['ub_name_{}'.format(bNum)].setText(bName)
                self.w['ub_code_{}'.format(bNum)].setText(bCode)
            if (bCode and not bName) or (not bCode and bName):
                msg0 = _translate('HandlerClass', 'are both required')
                msg1 = _translate('HandlerClass', 'only one has been specified for')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\nCODE + NAME {}\n{} BUTTON_{}\n'.format(head, msg0, msg1, bNum))
                self.w['button_{}'.format(str(bNum))].setText('')
                self.iniButtonCodes.append('')
                continue
            if not bCode:
                self.w['button_{}'.format(str(bNum))].setText('')
                self.iniButtonCodes.append('')
                continue
            code = bCode.lower().strip().split()[0]
            if code in singleCodes and code in iniButtonCodes:
                msg1 = _translate('HandlerClass', 'Duplicate code entry for')
                msg2 = _translate('HandlerClass', 'Using first instance only of')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} BUTTON_{} + BUTTON_{}\n{} {}\n'
                        .format(head, msg1, iniButtonCodes.index(code), bNum, msg2, bCode.split()[0]))
                self.w['button_{}'.format(str(bNum))].setText('')
                self.iniButtonCodes.append('')
                continue
            self.iniButtonCodes.append(bCode)
            iniButtonCodes.append(code)
            msg0 = _translate('HandlerClass', 'Invalid code for user button')
            bNames = bName.split('\\')
            bLabel = bNames[0]
            if len(bNames) > 1:
                for name in range(1, len(bNames)):
                    bLabel += '\n{}'.format(bNames[name])
            self.w['button_{}'.format(str(bNum))].setText(bLabel)
            if 'change-consumables' in bCode:
                self.ccParm = bCode.replace('change-consumables','').replace(' ','').lower() or None
                if self.ccParm != None and ('x' in self.ccParm or 'y' in self.ccParm) and 'f' in self.ccParm:
                    self.ccButton = 'button_{}'.format(str(bNum))
                    self.idleHomedList.append(self.ccButton)
                    self.pausedValidList.append(self.ccButton)
                else:
                    msg1 = _translate('HandlerClass', 'Check button code for invalid or missing arguments')
                    STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}\n'.format(head, msg0, bNum, msg1))
                    continue
            elif 'probe-test' in bCode:
                if len(bCode.split()) < 3:
                    if bCode.lower().replace('probe-test','').strip():
                        try:
                            self.ptTime = round(float(bCode.lower().replace('probe-test','').strip()))
                        except:
                            msg1 = _translate('HandlerClass', 'Check button code for invalid seconds argument')
                            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}\n'.format(head, msg0, bNum, msg1))
                            continue
                    else:
                        self.ptTime = 10
                    self.ptButton = 'button_{}'.format(str(bNum))
                    self.idleHomedList.append(self.ptButton)
                    self.probeText = self.w[self.ptButton].text()
                else:
                    msg1 = _translate('HandlerClass', 'Check button code for extra arguments')
                    STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}\n'.format(head, msg0, bNum, msg1))
                    continue
            elif 'torch-pulse' in bCode:
                if len(bCode.split()) < 3:
                    if bCode.lower().replace('torch-pulse','').strip():
                        try:
                            self.tpTime = round(float(bCode.lower().replace('torch-pulse','').strip()), 1)
                        except:
                            msg1 = _translate('HandlerClass', 'Check button code for invalid seconds argument')
                            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}\n'.format(head, msg0, bNum, msg1))
                            continue
                        self.tpTime = 3.0 if self.torchTime > 3.0 else self.tpTime
                    else:
                        self.tpTime = 1.0
                    self.tpButton = 'button_{}'.format(str(bNum))
                    self.idleOnList.append(self.tpButton)
                    self.pausedValidList.append(self.tpButton)
                    self.tpText = self.w[self.tpButton].text()
                else:
                    msg1 = _translate('HandlerClass', 'Check button code for extra arguments')
                    STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}\n'.format(head, msg0, bNum, msg1))
                    continue
            elif 'ohmic-test' in bCode:
                self.otButton = 'button_{}'.format(str(bNum))
                self.idleOnList.append(self.otButton)
                self.pausedValidList.append(self.otButton)
            elif 'framing' in bCode:
                frButton = True
                self.defaultZ = True
                self.frFeed = 0
                bCode = bCode.lower().replace('framing', '').strip()
                if 'usecurrentzheight' in bCode:
                    bCode = bCode.lower().replace('usecurrentzheight', '').strip()
                    self.defaultZ = False
                if len(bCode):
                    if bCode[0] == 'f':
                        try:
                            self.frFeed = float(bCode.replace('f', ''))
                        except:
                            msg1 = _translate('HandlerClass', 'Check button code for invalid feed argument')
                            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}\n'.format(head, msg0, bNum, msg1))
                            frButton = False
                            continue
                    else:
                        msg1 = _translate('HandlerClass', 'Check button code for invalid arguments')
                        STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}\n'.format(head, msg0, bNum, msg1))
                        continue
                if frButton:
                    self.frButton = 'button_{}'.format(str(bNum))
                    self.idleHomedList.append(self.frButton)
            elif 'cut-type' in bCode:
                self.ctButton = 'button_{}'.format(str(bNum))
                self.idleOnList.append(self.ctButton)
            elif 'single-cut' in bCode:
                self.scButton = 'button_{}'.format(str(bNum))
                self.idleHomedList.append(self.scButton)
            elif 'manual-cut' in bCode:
                self.mcButton = 'button_{}'.format(str(bNum))
                self.idleHomedList.append(self.mcButton)
            elif 'load' in bCode:
                self.idleOnList.append('button_{}'.format(str(bNum)))
            elif 'toggle-halpin' in bCode:
                head = _translate('HandlerClass', 'HAL Pin Error')
                if len(bCode.split()) == 3 and 'runcritical' in bCode.lower():
                    critical = True
                elif len(bCode.split()) == 2:
                    critical = False
                else:
                    head = _translate('HandlerClass', 'User Button Error')
                    msg1 = _translate('HandlerClass', 'Check button code for invalid arguments')
                    STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}\n'.format(head, msg0, bNum, msg1))
                    continue
                halpin = bCode.lower().split('toggle-halpin')[1].split(' ')[1].strip()
                excludedHalPins = ('plasmac.torch-pulse-start', 'plasmac.ohmic-test', \
                                'plasmac.probe-test', 'plasmac.consumable-change')
                if halpin in excludedHalPins:
                    msg1 = _translate('HandlerClass', 'HAL pin')
                    msg2 = _translate('HandlerClass', 'must be toggled')
                    msg3 = _translate('HandlerClass', 'using standard button code')
                    STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{} "{}" {}\n{}\n'.format(head, msg0, bNum, msg1, halpin, msg1, msg3))
                    continue
                else:
                    try:
                        hal.get_value(halpin)
                        self.idleOnList.append('button_{}'.format(str(bNum)))
                    except:
                        msg1 = _translate('HandlerClass', 'HAL pin')
                        msg2 = _translate('HandlerClass', 'does not exist')
                        STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{} "{}" {}\n'.format(head, msg0, bNum, msg1, halpin, msg2))
                        continue
                # halTogglePins format is: button name, run critical flag, button text
                self.halTogglePins[halpin] = ['button_{}'.format(str(bNum)), critical, bLabel]
            elif 'toggle-laser' in bCode:
                self.tlButton = 'button_{}'.format(str(bNum))
                self.idleHomedList.append('button_{}'.format(str(bNum)))
                continue
            elif 'pulse-halpin' in bCode:
                if len(bCode.split()) < 4:
                    try:
                        code, halpin, delay = bCode.lower().strip().split()
                    except:
                        try:
                            code, halpin = bCode.lower().strip().split()
                            delay = '1.0'
                        except:
                            head = _translate('HandlerClass', 'User Button Error')
                            msg1 = _translate('HandlerClass', 'Check button code for invalid arguments')
                            code = halpin = delay = ''
                            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}\n'.format(head, msg0, bNum, msg1))
                            continue
                    excludedHalPins = ('plasmac.torch-pulse-start', 'plasmac.ohmic-test', \
                                    'plasmac.probe-test', 'plasmac.consumable-change')
                    head = _translate('HandlerClass', 'HAL Pin Error')
                    if halpin in excludedHalPins:
                        msg1 = _translate('HandlerClass', 'HAL pin')
                        msg2 = _translate('HandlerClass', 'must be pulsed')
                        msg3 = _translate('HandlerClass', 'using standard button code')
                        STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{} "{}" {}\n{}\n'.format(head, msg0, bNum, msg1, halpin, msg1, msg3))
                        continue
                    else:
                        try:
                            hal.get_value(halpin)
                            self.idleOnList.append('button_{}'.format(str(bNum)))
                        except:
                            msg1 = _translate('HandlerClass', 'HAL pin')
                            msg2 = _translate('HandlerClass', 'does not exist')
                            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{} "{}" {}\n'.format(head, msg0, bNum, msg1, halpin, msg2))
                            continue
                    # halPulsePins format is: button name, pulse time, button text, remaining time, button number
                    try:
                        self.halPulsePins[halpin] = ['button_{}'.format(str(bNum)), float(delay), bLabel, 0.0, bNum]
                    except:
                        head = _translate('HandlerClass', 'User Button Error')
                        msg1 = _translate('HandlerClass', 'Check button code for invalid seconds argument')
                        STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}\n'.format(head, msg0, bNum, msg1))
                        continue
                else:
                    head = _translate('HandlerClass', 'User Button Error')
                    msg1 = _translate('HandlerClass', 'Check button code for invalid arguments')
                    STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}\n'.format(head, msg0, bNum, msg1))
                    continue
            elif 'offsets-view' in bCode:
                self.ovButton = 'button_{}'.format(str(bNum))
                self.idleHomedList.append(self.ovButton)
            elif 'latest-file' in bCode:
                self.llButton = 'button_{}'.format(str(bNum))
                self.idleList.append(self.llButton)
            else:
                for command in bCode.split('\\'):
                    command = command.strip()
                    if command and command[0].lower() in 'xyzabgmfsto' and command.replace(' ','')[1] in '0123456789<':
                        if 'button_{}'.format(str(bNum)) not in self.idleHomedList:
                            self.idleHomedList.append('button_{}'.format(str(bNum)))
                    elif command and command[0] == '%':
                        cmd = command.lstrip('%').lstrip(' ').split(' ', 1)[0]
                        if cmd[-3:] == '.py':
                            reply = os.path.exists(os.path.expanduser(cmd))
                        else:
                            reply = Popen('which {}'.format(cmd), stdout=PIPE, stderr=PIPE, shell=True).communicate()[0]
                        if not reply:
                            head = _translate('HandlerClass', 'External Code Error')
                            msg1 = _translate('HandlerClass', 'External command')
                            msg2 = _translate('HandlerClass', 'does not exist')
                            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{} "{}" {}\n'.format(head, msg0, bNum, msg1, cmd, msg2))
                        else:
                            self.estopOnList.append('button_{}'.format(str(bNum)))
                    else:
                        head = _translate('HandlerClass', 'Code Error')
                        msg1 = self.w['button_{}'.format(str(bNum))].text().replace('\n',' ')
                        STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}: "{}"\n'.format(head, msg0, bNum, msg1, command))
                        if 'button_{}'.format(str(bNum)) in self.idleHomedList:
                            self.idleHomedList.remove('button_{}'.format(str(bNum)))
                        break

    def user_button_down(self, bNum):
        commands = self.iniButtonCodes[bNum]
        if not commands: return
        if 'change-consumables' in commands.lower() and not 'e-halpin' in commands.lower():
            self.change_consumables(True)
        elif 'probe-test' in commands.lower() and not 'e-halpin' in commands.lower():
            self.probe_test(True)
        elif 'torch-pulse' in commands.lower() and not 'e-halpin' in commands.lower():
            self.torch_pulse(True)
        elif 'ohmic-test' in commands.lower() and not 'e-halpin' in commands.lower():
            self.ohmic_test(True)
        elif 'framing' in commands.lower():
            self.frame_job(True)
        elif 'cut-type' in commands.lower():
            self.w.gcodegraphics.logger.clear()
            self.cutType ^= 1
            if self.cutType:
                self.cutTypePin.set(1)
                self.button_active(self.ctButton)
                self.cutTypeText = self.w[self.ctButton].text()
                self.w[self.ctButton].setText(_translate('HandlerClass', 'PIERCE\nONLY'))
            else:
                self.cutTypePin.set(0)
                self.button_normal(self.ctButton)
                self.w[self.ctButton].setText(self.cutTypeText)
            self.overlayProgress.setValue(0)
            if self.fileOpened == True:
                self.file_reload_clicked()
        elif 'load' in commands.lower():
            lFile = '{}/{}'.format(self.programPrefix, commands.split('load', 1)[1].strip())
            self.overlayProgress.setValue(0)
            ACTION.OPEN_PROGRAM(lFile)
        elif 'toggle-halpin' in commands.lower():
            halpin = commands.lower().split('toggle-halpin')[1].split(' ')[1].strip()
            try:
                if halpin in self.halPulsePins and self.halPulsePins[halpin][3] > 0.05:
                    self.halPulsePins[halpin][3] = 0.0
                else:
                    self.invert_pin_state(halpin)
            except Exception as err:
                head = _translate('HandlerClass', 'HAL Pin Error')
                msg0 = _translate('HandlerClass', 'Invalid code for user button')
                msg1 = _translate('HandlerClass', 'Failed to toggle HAL pin')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}\n"{}" {}\n'.format(head, msg0, bNum, msg1, halpin, err))
        elif 'toggle-laser' in commands.lower():
            self.laserOnPin.set(not self.laserOnPin.get())
            for command in commands.split('\\'):
                command = command.strip()
                if command != 'toggle-laser':
                    self.user_button_command(bNum, command)
        elif 'pulse-halpin' in commands.lower():
            head = _translate('HandlerClass', 'HAL Pin Error')
            msg1 = _translate('HandlerClass', 'Failed to pulse HAL pin')
            try:
                code, halpin, delay = commands.lower().strip().split()
            except:
                try:
                    code, halpin = commands.lower().strip().split()
                    delay = '1.0'
                except:
                    msg0 = _translate('HandlerClass', 'Unknown error for user button')
                    STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{} "{}"\n'.format(head, msg0, bNum, msg1, halpin))
                    return
            # halPulsePins format is: button name, pulse time, button text, remaining time, button number
            try:
                if self.halPulsePins[halpin][3] > 0.05:
                    self.halPulsePins[halpin][3] = 0.0
                else:
                    self.w[self.halPulsePins[halpin][0]].setText('{}'.format(self.halPulsePins[halpin][2]))
                    self.halPulsePins[halpin][3] = self.halPulsePins[halpin][1]
                    if not self.pulseTimer.isActive():
                        self.pulseTimer.start(100)
            except:
                msg0 = _translate('HandlerClass', 'Invalid code for user button')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{} "{}"\n'.format(head, msg0, bNum, msg1, halpin))
        elif 'single-cut' in commands.lower():
            self.single_cut()
        elif 'manual-cut' in commands.lower():
            self.manual_cut()
        elif 'offsets-view' in commands.lower():
            if self.w.preview_stack.currentIndex() == 4:
                self.w.preview_stack.setCurrentIndex(0)
                self.button_normal(self.ovButton)
                self.set_buttons_state([self.idleList, self.idleOnList, self.idleHomedList], True)
                if self.w.gcode_display.lines() > 1:
                    self.w.run.setEnabled(True)
            else:
                self.w.preview_stack.setCurrentIndex(4)
                self.button_active(self.ovButton)
                buttonList = [button for button in self.idleHomedList if button != self.ovButton]
                self.set_buttons_state([self.idleList, self.idleOnList, buttonList], False)
                self.w.run.setEnabled(False)
        elif 'latest-file' in commands.lower():
            try:
                if len(commands.split()) == 2:
                    dir = commands.split()[1]
                else:
                    dir = self.w.PREFS_.getpref('last_loaded_directory', '', str, 'BOOK_KEEPING')
                files = glob.glob('{}/*.ngc'.format(dir))
                latest = max(files, key = os.path.getctime)
                self.overlayProgress.setValue(0)
                ACTION.OPEN_PROGRAM(latest)
            except:
                head = _translate('HandlerClass', 'File Error')
                msg0 = _translate('HandlerClass', 'Cannot open latest file from user button')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n'.format(head, msg0, bNum))
        else:
            self.reloadRequired = False
            for command in commands.split('\\'):
                command = command.strip()
                self.user_button_command(bNum, command)
                if command[0] == "%":
                    continue
                while not STATUS.is_interp_idle():
                    self.w.gcodegraphics.updateGL()
                    QApplication.processEvents()
                if command.lower().replace(' ', '').startswith('g10l20') and self.fileOpened:
                    self.reloadRequired = True
            if self.reloadRequired:
                self.file_reload_clicked()
            else:
                self.w.gcodegraphics.logger.clear()
            ACTION.SET_MANUAL_MODE()

    # for G-code commands and external commands
    def user_button_command(self, bNum, command):
        if command and command[0].lower() in 'xyzabgmfsto' and command.replace(' ','')[1] in '0123456789<':
            if '{' in command:
                newCommand = subCommand = ''
                for char in command:
                    if char == '{':
                        subCommand = ':'
                    elif char == '}':
                        f1, f2 = subCommand.replace(':','').split()
                        newCommand += self.iniFile.find(f1,f2)
                        subCommand = ''
                    elif subCommand.startswith(':'):
                        subCommand += char
                    else:
                        newCommand += char
                command = newCommand
            ACTION.CALL_MDI(command)
        elif command and command[0] == '%':
            command = command.lstrip('%').lstrip()
            if command[-3:] == '.py':
                cmd = 'python3 ' + command
                Popen(cmd, stdin = None, stdout=PIPE, stderr=PIPE, shell=True)
            else:
                command += '&'
                Popen(command, stdout=PIPE, stderr=PIPE, shell=True)
        else:
            head = _translate('HandlerClass', 'Code Error')
            msg0 = _translate('HandlerClass', 'Invalid code for user button')
            msg1 = self.w['button_{}'.format(str(bNum))].text().replace('\n',' ')
            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}: "{}"\n'.format(head, msg0, bNum, msg1, command))

    def user_button_up(self, bNum):
        commands = self.iniButtonCodes[bNum]
        if not commands: return
        elif 'torch-pulse' in commands.lower() and not 'e-halpin' in commands.lower():
            self.torch_pulse(False)
        if 'ohmic-test' in commands.lower() and not 'e-halpin' in commands.lower():
            self.ohmic_test(False)

    def torch_enable_changed(self, state):
        if self.tpButton:
            if state and STATUS.machine_is_on() and \
            (not STATUS.is_interp_running() or STATUS.is_interp_paused()) and \
            not hal.get_value('plasmac.consumable-changing'):
                self.w[self.tpButton].setEnabled(True)
            else:
                self.w[self.tpButton].setEnabled(False)

    def ext_torch_enable_changed(self, state):
        if (state):
            self.w.torch_enable.setChecked(not (self.w.torch_enable.isChecked()))

    def ext_thc_enable_changed(self, state):
        if (state):
            self.w.thc_enable.setChecked(not (self.w.thc_enable.isChecked()))

    def ext_corner_lock_enable_changed(self, state):
        if (state):
            self.w.cornerlock_enable.setChecked(not (self.w.cornerlock_enable.isChecked()))

    def ext_void_lock_enable_changed(self, state):
        if (state):
            self.w.voidlock_enable.setChecked(not (self.w.voidlock_enable.isChecked()))

    def ext_ignore_arc_ok_changed(self, state):
        if (state):
            self.w.ignore_arc_ok.setChecked(not (self.w.ignore_arc_ok.isChecked()))

    def ext_mesh_mode_changed(self, state):
        if (state):
            self.w.mesh_enable.setChecked(not (self.w.mesh_enable.isChecked()))

    def ext_ohmic_probe_enable_changed(self, state):
        if (state):
            self.w.ohmic_probe_enable.setChecked(not (self.w.ohmic_probe_enable.isChecked()))

    def ext_auto_volts_enable_changed(self, state):
        if (state):
            self.w.use_auto_volts.setChecked(not (self.w.use_auto_volts.isChecked()))

    def thc_auto_changed(self, state):
        if state:
            self.w.thc_delay.hide()
            self.w.thc_delay_lbl.hide()
            self.w.thc_sample_counts.show()
            self.w.thc_sample_counts_lbl.show()
            self.w.thc_sample_threshold.show()
            self.w.thc_sample_threshold_lbl.show()
        else:
            self.w.thc_delay.show()
            self.w.thc_delay_lbl.show()
            self.w.thc_sample_counts.hide()
            self.w.thc_sample_counts_lbl.hide()
            self.w.thc_sample_threshold.hide()
            self.w.thc_sample_threshold_lbl.hide()

    def ohmic_probe_enable_changed(self, state):
        if self.otButton:
            if state and STATUS.machine_is_on() and (not STATUS.is_interp_running() or STATUS.is_interp_paused()):
                self.w[self.otButton].setEnabled(True)
            else:
                self.w[self.otButton].setEnabled(False)

    def consumable_change_setup(self):
        self.ccXpos = self.ccYpos = self.ccFeed = 'None'
        X = Y = F = ''
        ccAxis = [X, Y, F]
        ccName = ['x', 'y', 'f']
        for loop in range(3):
            count = 0
            if ccName[loop] in self.ccParm:
                while 1:
                    if not self.ccParm[count]: break
                    if self.ccParm[count] == ccName[loop]:
                        count += 1
                        break
                    count += 1
                while 1:
                    if count == len(self.ccParm): break
                    if self.ccParm[count].isdigit() or self.ccParm[count] in '.-':
                        ccAxis[loop] += self.ccParm[count]
                    else:
                        break
                    count += 1
                if ccName[loop] == 'x' and ccAxis[loop]:
                    self.ccXpos = float(ccAxis[loop])
                elif ccName[loop] == 'y' and ccAxis[loop]:
                    self.ccYpos = float(ccAxis[loop])
                elif ccName[loop] == 'f' and ccAxis[loop]:
                    self.ccFeed = float(ccAxis[loop])

    def ext_change_consumables(self, state):
        if self.ccButton and self.w[self.ccButton].isEnabled():
            self.change_consumables(state)

    def change_consumables(self, state):
        self.w.laser.setEnabled(False)
        if hal.get_value('axis.x.eoffset-counts') or hal.get_value('axis.y.eoffset-counts'):
            hal.set_p('plasmac.consumable-change', '0')
            hal.set_p('plasmac.x-offset', '0')
            hal.set_p('plasmac.y-offset', '0')
            self.button_normal(self.ccButton)
            self.w[self.ccButton].setEnabled(False)
        else:
            self.consumable_change_setup()
            if self.ccFeed == 'None' or self.ccFeed < 1:
                head = _translate('HandlerClass', 'User Button Error')
                msg0 = _translate('HandlerClass', 'Feed rate for consumable change missing or less than 1')
                msg1 = _translate('HandlerClass', 'Check arguments for "change-consumables" button code in')
                msg2 = _translate('HandlerClass', '.prefs file')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n{}\n{}{}\n'.format(head, msg0, msg1, self.machineName, msg2))
                return
            else:
                hal.set_p('plasmac.xy-feed-rate', str(float(self.ccFeed)))
            self.w.run.setEnabled(False)
            if self.frButton:
                self.w[self.frButton].setEnabled(False)
            self.w.pause.setEnabled(False)
            if self.ccXpos == 'None':
                self.ccXpos = STATUS.get_position()[0][0]
            if self.ccXpos < round(self.xMin, 6) + (10 * self.unitsPerMm):
                self.ccXpos = round(self.xMin, 6) + (10 * self.unitsPerMm)
            elif self.ccXpos > round(self.xMax, 6) - (10 * self.unitsPerMm):
                self.ccXpos = round(self.xMax, 6) - (10 * self.unitsPerMm)
            if self.ccYpos == 'None':
                self.ccYpos = STATUS.get_position()[0][1]
            if self.ccYpos < round(self.yMin, 6) + (10 * self.unitsPerMm):
                self.ccYpos = round(self.yMin, 6) + (10 * self.unitsPerMm)
            elif self.ccYpos > round(self.yMax, 6) - (10 * self.unitsPerMm):
                self.ccYpos = round(self.yMax, 6) - (10 * self.unitsPerMm)
            hal.set_p('plasmac.x-offset', '{:.0f}'.format((self.ccXpos - STATUS.get_position()[0][0]) / hal.get_value('plasmac.offset-scale')))
            hal.set_p('plasmac.y-offset', '{:.0f}'.format((self.ccYpos - STATUS.get_position()[0][1]) / hal.get_value('plasmac.offset-scale')))
            hal.set_p('plasmac.consumable-change', '1')
            self.button_active(self.ccButton)

    def ext_probe_test(self, state):
        if self.ptButton and self.w[self.ptButton].isEnabled():
            self.probe_test(state)

    def probe_test(self, state):
        if state:
            if self.probeTimer.remainingTime() <= 0 and not self.offsetsActivePin.get():
                self.probeTime = self.ptTime
                self.probeTimer.start(1000)
                self.probeTest = True
                hal.set_p('plasmac.probe-test','1')
                self.w[self.ptButton].setText('{}'.format(self.probeTime))
                self.button_active(self.ptButton)
                self.w.run.setEnabled(False)
                self.w.abort.setEnabled(True)
                self.set_buttons_state([self.idleList, self.idleOnList, self.idleHomedList], False)
                self.w[self.ptButton].setEnabled(True)
                self.set_tab_jog_states(False)
                log = _translate('HandlerClass', 'Probe test started')
                STATUS.emit('update-machine-log', log, 'TIME')
            else:
                self.probe_test_stop()
                log = _translate('HandlerClass', 'Probe test aborted')
                STATUS.emit('update-machine-log', log, 'TIME')

    def probe_test_stop(self):
        self.probeTimer.stop()
        self.probeTime = 0
        self.w.abort.setEnabled(False)
        hal.set_p('plasmac.probe-test','0')
        self.w[self.ptButton].setText(self.probeText)
        self.button_normal(self.ptButton)
        self.w[self.ptButton].setEnabled(False)

    def probe_test_error(self, state):
        if state:
            self.probe_test(False)

    def ext_torch_pulse(self, state):
        if self.tpButton and self.w[self.tpButton].isEnabled():
            self.torch_pulse(state)

    def torch_pulse(self, state):
        if state:
            if not self.torchTime and \
               self.w.torch_enable.isChecked() and not hal.get_value('plasmac.torch-on'):
                self.torchTime = self.tpTime
                self.torchTimer.start(100)
                self.torchPulse = True
                hal.set_p('plasmac.torch-pulse-time', str(self.torchTime))
                hal.set_p('plasmac.torch-pulse-start', '1')
                self.w[self.tpButton].setText('{}'.format(self.torchTime))
                self.button_active(self.tpButton)
                self.torch_pulse_states(False)
                log = _translate('HandlerClass', 'Torch pulse started')
                STATUS.emit('update-machine-log', log, 'TIME')
            else:
                self.torchTimer.stop()
                self.torchTime = 0.0
                self.torch_pulse_states(True)
        else:
            hal.set_p('plasmac.torch-pulse-start','0')
            if self.torchTime == 0:
                self.torch_pulse_states(True)
                log = _translate('HandlerClass', 'Torch pulse ended manually')
                STATUS.emit('update-machine-log', log, 'TIME')

    def torch_pulse_states(self, state):
        self.set_tab_jog_states(state)
        if not STATUS.is_auto_paused():
            if STATUS.is_on_and_idle() and STATUS.is_all_homed():
                self.set_buttons_state([self.idleList, self.idleOnList, self.idleHomedList], state)
            else:
                self.set_buttons_state([self.idleList, self.idleOnList], state)
            if self.w.gcode_display.lines() > 1:
                self.w.run.setEnabled(state)
        if state:
            hal.set_p('plasmac.torch-pulse-time', '0')
            self.w[self.tpButton].setText(self.tpText)
            self.button_normal(self.tpButton)
            self.torchPulse = False

    def ext_ohmic_test(self, state):
        if self.otButton and self.w[self.otButton].isEnabled():
            self.ohmic_test(state)

    def ohmic_test(self, state):
        hal.set_p('plasmac.ohmic-test', '{}'.format(str(state)))
        buttonList = [button for button in self.idleOnList if button != self.otButton]
        if not STATUS.is_auto_paused():
            if STATUS.is_on_and_idle() and STATUS.is_all_homed():
                self.set_buttons_state([self.idleList, self.idleHomedList, buttonList], not state)
            else:
                self.set_buttons_state([self.idleList, buttonList], not state)
            if self.w.gcode_display.lines() > 1:
                self.w.run.setEnabled(not state)

    def ext_frame_job(self, state):
        if self.frButton and self.w[self.frButton].isEnabled():
            self.frame_job(state)

    def frame_job(self, state):
        if self.gcodeProps and state:
            self.w.run.setEnabled(False)
            response = False
            if self.w.laser.isVisible():
                msgList, units, xMin, yMin, xMax, yMax, frame_points = self.bounds_check('framing', self.laserOffsetX, self.laserOffsetY)
                if self.boundsError['framing']:
                    head = _translate('HandlerClass', 'Axis Limit Error')
                    btn1 = _translate('HandlerClass', 'YES')
                    btn2 = _translate('HandlerClass', 'NO')
                    msgs = ''
                    msg1 = _translate('HandlerClass', 'due to laser offset')
                    for n in range(0, len(msgList), 3):
                        if msgList[n + 1] == 'MAX':
                            msg0 = _translate('HandlerClass', 'portion of move would exceed the maximum limit by')
                        else:
                            msg0 = _translate('HandlerClass', 'portion of move would exceed the minimum limit by')
                        fmt = '\n{} {} {}{} {}' if msgs else '{} {} {}{} {}'
                        msgs += fmt.format(msgList[n], msg0, msgList[n + 2], units, msg1)
                    msgs += _translate('HandlerClass', '\n\nDo you want to try with the torch?\n')
                    response = self.dialog_show_yesno(QMessageBox.Warning, '{}'.format(head), '\n{}'.format(msgs), '{}'.format(btn1), '{}'.format(btn2))
                    if not response:
                        self.w.run.setEnabled(True)
                        return
                    msgList, units, xMin, yMin, xMax, yMax, frame_points = self.bounds_check('framing', 0, 0)
                    if self.boundsError['framing']:
                        self.w.run.setEnabled(True)
                        head = _translate('HandlerClass', 'Axis Limit Error')
                        msgs = ''
                        for n in range(0, len(msgList), 3):
                            if msgList[n + 1] == 'MAX':
                                msg0 = _translate('HandlerClass', 'portion of move would exceed the maximum limit by')
                            else:
                                msg0 = _translate('HandlerClass', 'portion of move would exceed the minimum limit by')
                            fmt = '\n{} {} {}{}' if msgs else '{} {} {}{}'
                            msgs += fmt.format(msgList[n], msg0, msgList[n + 2], units)
                        self.dialog_show_ok(QMessageBox.Warning, '{}'.format(head), '\n{}'.format(msgs))
                        return
                if not response:
                    self.laserOnPin.set(1)
            else:
                msgList, units, xMin, yMin, xMax, yMax, frame_points = self.bounds_check('framing', 0, 0)
                if self.boundsError['framing']:
                    self.w.run.setEnabled(True)
                    head = _translate('HandlerClass', 'Axis Limit Error')
                    msgs = ''
                    for n in range(0, len(msgList), 3):
                        if msgList[n + 1] == 'MAX':
                            msg0 = _translate('HandlerClass', 'move would exceed the maximum limit by')
                        else:
                            msg0 = _translate('HandlerClass', 'move would exceed the minimum limit by')
                        fmt = '\n{} {} {}{}' if msgs else '{} {} {}{}'
                        msgs += fmt.format(msgList[n], msg0, msgList[n + 2], units)
                    self.dialog_show_ok(QMessageBox.Warning, '{}'.format(head), '\n{}'.format(msgs))
                    return
            if not self.frFeed:
                feed = float(self.w.cut_feed_rate.text())
            else:
                feed = self.frFeed
            zHeight = self.zMax - (hal.get_value('plasmac.max-offset') * self.unitsPerMm)
            if STATUS.is_on_and_idle() and STATUS.is_all_homed():
                self.framing = True
                previousMode = ''
                if self.units == 'in' and STATUS.is_metric_mode():
                    previousMode = 'G21'
                    ACTION.CALL_MDI('G20')
                elif self.units == 'mm' and not STATUS.is_metric_mode():
                    previousMode = 'G20'
                    ACTION.CALL_MDI('G21')
                ACTION.CALL_MDI_WAIT('G64 P{:0.3f}'.format(0.25 * self.unitsPerMm))
                if self.defaultZ:
                    ACTION.CALL_MDI('G53 G0 Z{:0.4f}'.format(zHeight))
                ACTION.CALL_MDI('G53 G0 X{:0.2f} Y{:0.2f}'.format(frame_points[1][0], frame_points[1][1]))
                ACTION.CALL_MDI('G53 G1 X{:0.2f} Y{:0.2f} F{:0.0f}'.format(frame_points[2][0], frame_points[2][1], feed))
                ACTION.CALL_MDI('G53 G1 X{:0.2f} Y{:0.2f}'.format(frame_points[3][0], frame_points[3][1]))
                ACTION.CALL_MDI('G53 G1 X{:0.2f} Y{:0.2f}'.format(frame_points[4][0], frame_points[4][1]))
                ACTION.CALL_MDI('G53 G1 X{:0.2f} Y{:0.2f}'.format(frame_points[1][0], frame_points[1][1]))
                ACTION.CALL_MDI('G0 X0 Y0')
                ACTION.CALL_MDI(previousMode)

    def single_cut(self):
        self.set_buttons_state([self.idleList, self.idleOnList, self.idleHomedList], False)
        sC = QDialog(self.w)
        sC.setWindowTitle(_translate('HandlerClass', 'Single Cut'))
        l1 = QLabel(_translate('HandlerClass', 'X LENGTH:'))
        xLength = QDoubleSpinBox()
        xLength.setAlignment(Qt.AlignRight)
        xLength.setMinimum(-9999)
        xLength.setMaximum(9999)
        xLength.setDecimals(1)
        l2 = QLabel(_translate('HandlerClass', 'Y LENGTH:'))
        yLength = QDoubleSpinBox()
        yLength.setAlignment(Qt.AlignRight)
        yLength.setMinimum(-9999)
        yLength.setMaximum(9999)
        yLength.setDecimals(1)
        l3 = QLabel('')
        buttons = QDialogButtonBox.Ok | QDialogButtonBox.Cancel
        buttonBox = QDialogButtonBox(buttons)
        buttonBox.accepted.connect(sC.accept)
        buttonBox.rejected.connect(sC.reject)
        buttonBox.button(QDialogButtonBox.Ok).setText(_translate('HandlerClass', 'CUT'))
        buttonBox.button(QDialogButtonBox.Ok).setIcon(QIcon())
        buttonBox.button(QDialogButtonBox.Cancel).setText(_translate('HandlerClass', 'CANCEL'))
        buttonBox.button(QDialogButtonBox.Cancel).setIcon(QIcon())
        layout = QVBoxLayout()
        layout.addWidget(l1)
        layout.addWidget(xLength)
        layout.addWidget(l2)
        layout.addWidget(yLength)
        layout.addWidget(l3)
        layout.addWidget(buttonBox)
        sC.setLayout(layout)
        xLength.setValue(self.PREFS.getpref('X length', 0.0, float, 'SINGLE CUT'))
        yLength.setValue(self.PREFS.getpref('Y length', 0.0, float, 'SINGLE CUT'))
        self.vkb_show(True)
        result = sC.exec_()
        self.vkb_hide()
        if not result:
            self.set_buttons_state([self.idleList, self.idleOnList, self.idleHomedList], True)
            return
        self.PREFS.putpref('X length', xLength.value(), float, 'SINGLE CUT')
        self.PREFS.putpref('Y length', yLength.value(), float, 'SINGLE CUT')
        self.oldFile = ACTION.prefilter_path if ACTION.prefilter_path else None
        self.g91 = True if 910 in STATUS.stat.gcodes else False
        xEnd = STATUS.get_position()[0][0] + xLength.value()
        yEnd = STATUS.get_position()[0][1] + yLength.value()
        newFile = '{}single_cut.ngc'.format(self.tmpPath)
        with open(newFile, 'w') as f:
            f.write('G90\n')
            f.write('F#<_hal[plasmac.cut-feed-rate]>\n')
            f.write('G53 G0 X{:0.6f} Y{:0.6f}\n'.format(STATUS.get_position()[0][0], STATUS.get_position()[0][1]))
            f.write('M3 $0 S1\n')
            f.write('G53 G1 X{:0.6f} Y{:0.6f}\n'.format(xEnd, yEnd))
            f.write('M5 $0\n')
            f.write('M2\n')
        self.single_cut_request = True
        ACTION.OPEN_PROGRAM(newFile)

    def manual_cut(self):
        if self.manualCut:
            ACTION.SET_SPINDLE_STOP(0)
            self.w.abort.setEnabled(False)
            if self.mcButton:
                self.w[self.mcButton].setEnabled(False)
                self.button_normal(self.mcButton)
            log = _translate('HandlerClass', 'Manual cut aborted')
            STATUS.emit('update-machine-log', log, 'TIME')
        elif STATUS.machine_is_on() and STATUS.is_all_homed() and STATUS.is_interp_idle():
            self.manualCut = True
            self.set_mc_states(False)
            self.w.abort.setEnabled(True)
            self.set_buttons_state([self.idleList, self.idleOnList, self.idleHomedList], False)
            if self.mcButton:
                self.w[self.mcButton].setEnabled(True)
                self.button_active(self.mcButton)
            ACTION.SET_SPINDLE_ROTATION(1 ,1 , 0)
            log = _translate('HandlerClass', 'Manual cut started')
            STATUS.emit('update-machine-log', log, 'TIME')
        self.set_run_button_state()

    def button_active(self, button):
        self.w[button].setStyleSheet( \
                    'QPushButton {{ color: {0}; background: {1} }} \
                     QPushButton:pressed {{ color: {0}; background: {1} }} \
                     QPushButton:disabled {{ color: {2}}}' \
                     .format(self.backColor, self.fore1Color, self.disabledColor))

    def button_normal(self, button):
        self.w[button].setStyleSheet( \
                    'QPushButton {{ color: {0}; background: {1} }} \
                     QPushButton:pressed {{ color: {0}; background: {1} }} \
                     QPushButton:disabled {{ color: {2}}}' \
                     .format(self.foreColor, self.backColor, self.disabledColor))


#########################################################################################################################
# ONBOARD VIRTUAL KEYBOARD FUNCTIONS #
#########################################################################################################################
    def vkb_check(self):
        if self.w.chk_soft_keyboard.isChecked() and not os.path.isfile('/usr/bin/onboard'):
            head = _translate('HandlerClass', 'Virtual Keyboard Error')
            msg0  = _translate('HandlerClass', '"onboard" virtual keyboard is not installed')
            msg1 = _translate('HandlerClass', 'some keyboard functions are not available')
            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n{}\n'.format(head, msg0, msg1))
            return
        try:
            cmd = 'gsettings get org.onboard.window.landscape width'
            self.obWidth = Popen(cmd, stdout=PIPE, stderr=PIPE, shell=True).communicate()[0].decode().strip()
            cmd = 'gsettings get org.onboard.window.landscape height'
            self.obHeight = Popen(cmd, stdout=PIPE, stderr=PIPE, shell=True).communicate()[0].decode().strip()
            cmd = 'gsettings get org.onboard layout'
            layout = Popen(cmd, stdout=PIPE, stderr=PIPE, shell=True).communicate()[0].decode().strip()
            if '/numpad' in layout or '/keyboard' in layout:
                self.obLayout = 'compact'
            else:
                self.obLayout = layout
        except:
            self.obWidth = '700'
            self.obHeight = '300'
            self.obLayout = 'compact'

    def vkb_show(self, numpad=False):
        if self.firstRun: return
        if os.path.isfile('/usr/bin/onboard'):
            if self.w.chk_soft_keyboard.isChecked():
                w = '240' if numpad else '740'
                h = '240'
                l = 'numpad' if numpad else 'keyboard'
                self.vkb_setup(w, h, os.path.join(self.PATHS.IMAGEDIR, 'qtplasmac', l))
                cmd  = 'dbus-send'
                cmd += ' --type=method_call'
                cmd += ' --dest=org.onboard.Onboard'
                cmd += ' /org/onboard/Onboard/Keyboard'
                cmd += ' org.onboard.Onboard.Keyboard.Show'
                Popen(cmd, stdout=PIPE, shell=True)

    def vkb_hide(self, custom=False):
        if os.path.isfile('/usr/bin/onboard') and self.obLayout:
            cmd  = 'dbus-send'
            cmd += ' --type=method_call'
            cmd += ' --dest=org.onboard.Onboard'
            cmd += ' /org/onboard/Onboard/Keyboard'
            cmd += ' org.onboard.Onboard.Keyboard.Hide'
            Popen(cmd, stdout=PIPE, shell=True)
            if not custom:
                self.vkb_setup(self.obWidth, self.obHeight, self.obLayout)

    def vkb_setup(self, w, h, l):
        if os.path.isfile('/usr/bin/onboard'):
            Popen('gsettings set org.onboard.window.landscape width {}'.format(int(w)-1), stdout=PIPE, shell=True)
            Popen('gsettings set org.onboard.window.landscape height {}'.format(int(h)-1), stdout=PIPE, shell=True)
            Popen('gsettings set org.onboard layout {}'.format(l), stdout=PIPE, shell=True)
            Popen('gsettings set org.onboard.window.landscape width {}'.format(int(w)), stdout=PIPE, shell=True)
            Popen('gsettings set org.onboard.window.landscape height {}'.format(int(h)), stdout=PIPE, shell=True)


#########################################################################################################################
# MATERIAL HANDLING FUNCTIONS #
#########################################################################################################################
    def save_materials_clicked(self):
        material = self.materialChangeNumberPin.get()
        index = self.w.materials_box.currentIndex()
        self.save_materials(material, index)

    def reload_materials_clicked(self):
        self.materialUpdate = True
        index = self.w.materials_box.currentIndex()
        self.materialFileDict = {}
        self.materialNumList = []
        self.load_materials()
        self.w.materials_box.setCurrentIndex(index)
        self.materialUpdate = False
        self.materialReloadPin.set(0)

    def new_material_clicked(self, repeat, value):
        head = _translate('HandlerClass', 'Add Material')
        msg1 = _translate('HandlerClass', 'Enter New Material Number')
        msgs = msg1
        btn1 = _translate('HandlerClass', 'ADD')
        btn2 = _translate('HandlerClass', 'CANCEL')
        virtkb = 4
        while(1):
            valid, num = self.dialog_input(virtkb, head, '{}:'.format(msgs), btn1, btn2)
            if not valid:
                return
            try:
                num = int(num)
            except:
                if not num:
                    msg0 = _translate('HandlerClass', 'A material number is required')
                    msgs = '{}.\n\n{}:'.format(msg0, msg1)
                else:
                    msg0 = _translate('HandlerClass', 'is not a valid number')
                    msgs = '{} {}.\n\n{}:'.format(num, msg0, msg1)
                continue
            if num == 0 or num in self.materialNumList:
                msg0 = _translate('HandlerClass', 'Material')
                msg2 = _translate('HandlerClass', 'is in use')
                msgs = '{} #{} {}.\n\n{}:'.format(msg0, num, msg2, msg1)
                continue
            elif num >= 1000000:
                msg0 = _translate('HandlerClass', 'Material numbers need to be less than 1000000')
                msgs = '{}.\n\n{}:'.format(msg0, msg1)
                continue
            break
        msg1 = 'Enter New Material Name'
        virtkb = 3
        while(1):
            valid, nam = self.dialog_input(virtkb, head, msg1, btn1, btn2)
            if not valid:
                return
            if not nam:
                msg0 = _translate('HandlerClass', 'Material name is required')
                msgs = '{}.\n\n{}:'.format(msg0, msg1)
                continue
            break


        material = self.w.materials_box.currentText().split(': ', 1)[0].lstrip('0')
        material = int(material) if material else 0
        COPY(self.materialFile, self.tmpMaterialFile)
        with open(self.tmpMaterialFile, 'r') as inFile:
            with open(self.materialFile, 'w') as outFile:
                written = False
                while 1:
                    line = inFile.readline()
                    if not written:
                        if not line or \
                           (line.startswith('[MATERIAL_NUMBER_') and \
                           num < int(line.strip().split('NUMBER_')[1].rstrip(']'))):
                            outFile.write('[MATERIAL_NUMBER_{}]\n'.format(num))
                            outFile.write('NAME               = {}\n'.format(nam))
                            outFile.write('KERF_WIDTH         = {}\n'.format(self.materialFileDict[material][1]))
                            outFile.write('PIERCE_HEIGHT      = {}\n'.format(self.materialFileDict[material][2]))
                            outFile.write('PIERCE_DELAY       = {}\n'.format(self.materialFileDict[material][3]))
                            outFile.write('PUDDLE_JUMP_HEIGHT = {}\n'.format(self.materialFileDict[material][4]))
                            outFile.write('PUDDLE_JUMP_DELAY  = {}\n'.format(self.materialFileDict[material][5]))
                            outFile.write('CUT_HEIGHT         = {}\n'.format(self.materialFileDict[material][6]))
                            outFile.write('CUT_SPEED          = {}\n'.format(self.materialFileDict[material][7]))
                            outFile.write('CUT_AMPS           = {}\n'.format(self.materialFileDict[material][8]))
                            outFile.write('CUT_VOLTS          = {}\n'.format(self.materialFileDict[material][9]))
                            outFile.write('PAUSE_AT_END       = {}\n'.format(self.materialFileDict[material][10]))
                            outFile.write('GAS_PRESSURE       = {}\n'.format(self.materialFileDict[material][11]))
                            outFile.write('CUT_MODE           = {}\n\n'.format(self.materialFileDict[material][12]))
                            outFile.write(line)
                            written = True
                        elif not line:
                            break
                        else:
                            outFile.write(line)
                    elif not line:
                        break
                    else:
                        outFile.write(line)
        self.materialUpdate = True
        self.materialFileDict = {}
        self.materialNumList = []
        self.load_materials()
        self.w.materials_box.setCurrentIndex(self.materialList.index(num))
        self.materialUpdate = False

    def delete_material_clicked(self):
        head = _translate('HandlerClass', 'Delete Material')
        msg1 = _translate('HandlerClass', 'Enter Material Number To Delete')
        btn1 = _translate('HandlerClass', 'DELETE')
        btn2 = _translate('HandlerClass', 'CANCEL')
        msgs = msg1
        virtkb = 4
        while(1):
            valid, num = self.dialog_input(virtkb, head, '{}:'.format(msgs), btn1, btn2)
            if not valid:
                return
            try:
                num = int(num)
            except:
                if not num:
                    msg0 = _translate('HandlerClass', 'A material number is required')
                    msgs = '{}.\n\n\{}:'.format(msg0, msg1)
                else:
                    msg0 = _translate('HandlerClass', 'is not a valid number')
                    msgs = '{} {}.\n\n{}:'.format(num, msg0, msg1)
                continue
            if num == 0:
                msg0 = _translate('HandlerClass', 'Default material cannot be deleted')
                msgs = '{}.\n\n{}:'
                continue
            if num not in self.materialNumList:
                msg0 = _translate('HandlerClass', 'Material')
                msg3 = _translate('HandlerClass', 'does not exist')
                msgs = '{} #{} {}.\n\n{}:'.format(msg0, num, msg3, msg1)
                continue
            break
        head = _translate('HandlerClass', 'Delete Material')
        msg0 = _translate('HandlerClass', 'Do you really want to delete material')
        if not self.dialog_show_yesno(QMessageBox.Question, '{}'.format(head), '\n{} #{}?\n'.format(msg0, num)):
            return
        COPY(self.materialFile, self.tmpMaterialFile)
        with open(self.tmpMaterialFile, 'r') as inFile:
            with open(self.materialFile, 'w') as outFile:
                while 1:
                    line = inFile.readline()
                    if not line: break
                    elif line.startswith('[MATERIAL_NUMBER_') and \
                         int(line.strip().strip(']').split('[MATERIAL_NUMBER_')[1]) == num:
                        break
                    else:
                        outFile.write(line)
                while 1:
                    line = inFile.readline()
                    if not line: break
                    elif line.startswith('[MATERIAL_NUMBER_'):
                        outFile.write(line)
                        break
                while 1:
                    line = inFile.readline()
                    if not line: break
                    else:
                        outFile.write(line)
        self.materialUpdate = True
        self.materialFileDict = {}
        self.materialNumList = []
        self.load_materials()
        self.materialUpdate = False

    def selector_changed(self, index):
        if self.w.material_selector.currentIndex() != self.w.materials_box.currentIndex():
            self.w.materials_box.setCurrentIndex(index)
            self.w.conv_material.setCurrentIndex(index)

    def conv_material_changed(self, index):
        if self.w.conv_material.currentIndex() != self.w.materials_box.currentIndex():
            self.w.materials_box.setCurrentIndex(index)
            self.w.material_selector.setCurrentIndex(index)

    def material_changed(self, index):
        if self.w.materials_box.currentText():
            if self.getMaterialBusy:
                self.materialChangePin.set(0)
                self.autoChange = False
                return
            material = int(self.w.materials_box.currentText().split(': ', 1)[0])
            if material >= 1000000:
                self.w.save_material.setEnabled(False)
            else:
                self.w.save_material.setEnabled(True)
            if self.autoChange:
                hal.set_p('motion.digital-in-03','0')
                self.change_material(material)
                self.materialChangePin.set(2)
                hal.set_p('motion.digital-in-03','1')
            else:
                self.change_material(material)
            self.w.material_selector.setCurrentIndex(self.w.materials_box.currentIndex())
            self.w.conv_material.setCurrentIndex(self.w.materials_box.currentIndex())
        self.autoChange = False
        self.overlay_update(None)

    def material_change_pin_changed(self, halpin):
        if halpin == 0:
            hal.set_p('motion.digital-in-03','0')
        elif halpin == 3:
            hal.set_p('motion.digital-in-03','1')
            self.materialChangePin.set(0)

    def material_change_number_pin_changed(self, halpin):
        if self.getMaterialBusy:
            return
        if self.materialChangePin.get() == 1:
            self.autoChange = True
        if not self.material_exists(halpin):
            self.autoChange = False
            return
        self.w.materials_box.setCurrentIndex(self.materialList.index(halpin))

    def material_change_timeout_pin_changed(self, halpin):
        head = _translate('HandlerClass', 'Materials Error')
        if halpin:
            # should we stop or pause the program if a timeout occurs???
            material = int(self.w.materials_box.currentText().split(': ', 1)[0])
            msg0 = _translate('HandlerClass', 'Material change timeout occurred for material')
            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:{} #{}\n'.format(head, msg0, material))
            self.materialChangeNumberPin.set(material)
            self.materialChangeTimeoutPin.set(0)
            hal.set_p('motion.digital-in-03','0')

    def material_reload_pin_changed(self, halpin):
        if halpin:
            self.reload_materials_clicked()

    def material_temp_pin_changed(self, halpin):
        if halpin:
            t_name = 'Temporary {}'.format(halpin)
            k_width, p_height, p_delay, pj_height, pj_delay, c_height, \
            c_speed, c_amps, c_volts, pause, g_press, c_mode, t_item = \
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
            with open(self.tmpMaterialFileGCode, 'r') as f_in:
                for line in f_in:
                    if line.startswith('NAME'):
                        t_name = line.split('=')[1].strip()
                    if line.startswith('KERF_WIDTH'):
                        k_width = float(line.split('=')[1].strip())
                    elif line.startswith('PIERCE_HEIGHT'):
                        p_height = float(line.split('=')[1].strip())
                    elif line.startswith('PIERCE_DELAY'):
                        p_delay = float(line.split('=')[1].strip())
                    elif line.startswith('PUDDLE_JUMP_HEIGHT'):
                        pj_height = float(line.split('=')[1].strip())
                    elif line.startswith('PUDDLE_JUMP_DELAY'):
                        pj_delay = float(line.split('=')[1].strip())
                    elif line.startswith('CUT_HEIGHT'):
                        c_height = float(line.split('=')[1].strip())
                    elif line.startswith('CUT_SPEED'):
                        c_speed = float(line.split('=')[1].strip())
                    elif line.startswith('CUT_AMPS'):
                        c_amps = float(line.split('=')[1].strip())
                    elif line.startswith('CUT_VOLTS'):
                        c_volts = float(line.split('=')[1].strip())
                    elif line.startswith('PAUSE_AT_END'):
                        pause = float(line.split('=')[1].strip())
                    elif line.startswith('GAS_PRESSURE'):
                        g_press = float(line.split('=')[1].strip())
                    elif line.startswith('CUT_MODE'):
                        c_mode = float(line.split('=')[1].strip())
            self.write_materials(halpin, t_name, k_width, p_height, \
                                 p_delay, pj_height, pj_delay, c_height, c_speed, \
                                 c_amps, c_volts, pause, g_press, c_mode, t_item)
            self.display_materials()
            self.change_material(0)
            self.w.materials_box.setCurrentIndex(0)
            self.materialTempPin.set(0)

    def save_materials(self, material, index):
        if index == 0:
            self.save_default_material()
        self.save_material_file(material, index)

    def load_materials(self):
        self.load_default_material()
        self.load_material_file()

    def write_materials(self, *items):
        mat = []
        for item in items[1:]:
            mat.append(item)
        self.materialFileDict[items[0]] = mat

    def display_materials(self):
        self.materialList = []
        self.w.materials_box.clear()
        self.w.material_selector.clear()
        self.w.conv_material.clear()
        for key in sorted(self.materialFileDict):
            self.w.materials_box.addItem('{:05d}: {}'.format(key, self.materialFileDict[key][0]))
            self.w.material_selector.addItem('{:05d}: {}'.format(key, self.materialFileDict[key][0]))
            self.w.conv_material.addItem('{:05d}: {}'.format(key, self.materialFileDict[key][0]))
            self.materialList.append(key)

    def change_material(self, material):
            self.materialName = self.materialFileDict[material][0]
            self.w.kerf_width.setValue(self.materialFileDict[material][1])
            self.w.pierce_height.setValue(self.materialFileDict[material][2])
            self.w.pierce_delay.setValue(self.materialFileDict[material][3])
            self.w.puddle_jump_height.setValue(self.materialFileDict[material][4])
            self.w.puddle_jump_delay.setValue(self.materialFileDict[material][5])
            self.w.cut_height.setValue(self.materialFileDict[material][6])
            self.w.cut_feed_rate.setValue(self.materialFileDict[material][7])
            self.w.cut_amps.setValue(self.materialFileDict[material][8])
            self.w.cut_volts.setValue(self.materialFileDict[material][9])
            self.w.pause_at_end.setValue(self.materialFileDict[material][10])
            self.w.gas_pressure.setValue(self.materialFileDict[material][11])
            self.w.cut_mode.setValue(self.materialFileDict[material][12])
            self.materialChangeNumberPin.set(material)

    def save_material_file(self, material, index):
        COPY(self.materialFile, self.tmpMaterialFile)
        with open(self.tmpMaterialFile, 'r') as inFile:
            with open(self.materialFile, 'w') as outFile:
                while 1:
                    line = inFile.readline()
                    if not line: break
                    elif line.startswith('[MATERIAL_NUMBER_') and \
                         material == int(line.strip().strip(']').split('[MATERIAL_NUMBER_')[1]):
                        outFile.write(line)
                        break
                    else:
                        outFile.write(line)
                while 1:
                    line = inFile.readline()
                    if not line: break
                    elif line.startswith('[MATERIAL_NUMBER_'):
                        outFile.write(line)
                        break
                    elif line.startswith('NAME'):
                        outFile.write(line)
                    elif line.startswith('KERF_WIDTH'):
                        outFile.write('KERF_WIDTH         = {}\n'.format(self.w.kerf_width.value()))
                    elif line.startswith('PIERCE_HEIGHT'):
                        outFile.write('PIERCE_HEIGHT      = {}\n'.format(self.w.pierce_height.value()))
                    elif line.startswith('PIERCE_DELAY'):
                        outFile.write('PIERCE_DELAY       = {}\n'.format(self.w.pierce_delay.value()))
                    elif line.startswith('PUDDLE_JUMP_HEIGHT'):
                        outFile.write('PUDDLE_JUMP_HEIGHT = {}\n'.format(self.w.puddle_jump_height.value()))
                    elif line.startswith('PUDDLE_JUMP_DELAY'):
                        outFile.write('PUDDLE_JUMP_DELAY  = {}\n'.format(self.w.puddle_jump_delay.value()))
                    elif line.startswith('CUT_HEIGHT'):
                        outFile.write('CUT_HEIGHT         = {}\n'.format(self.w.cut_height.value()))
                    elif line.startswith('CUT_SPEED'):
                        outFile.write('CUT_SPEED          = {}\n'.format(self.w.cut_feed_rate.value()))
                    elif line.startswith('CUT_AMPS'):
                        outFile.write('CUT_AMPS           = {}\n'.format(self.w.cut_amps.value()))
                    elif line.startswith('CUT_VOLTS'):
                        outFile.write('CUT_VOLTS          = {}\n'.format(self.w.cut_volts.value()))
                    elif line.startswith('PAUSE_AT_END'):
                        outFile.write('PAUSE_AT_END       = {}\n'.format(self.w.pause_at_end.value()))
                    elif line.startswith('GAS_PRESSURE'):
                        outFile.write('GAS_PRESSURE       = {}\n'.format(self.w.gas_pressure.value()))
                    elif line.startswith('CUT_MODE'):
                        outFile.write('CUT_MODE           = {}\n'.format(self.w.cut_mode.value()))
                    else:
                         outFile.write(line)
                while 1:
                    line = inFile.readline()
                    if not line: break
                    outFile.write(line)
        self.materialUpdate = True
        self.materialFileDict = {}
        self.load_materials()
        self.w.materials_box.setCurrentIndex(index)
        self.materialUpdate = False
        self.set_saved_material()

    def set_saved_material(self):
        material = int(self.w.materials_box.currentText().split(': ', 1)[0])
        self.materialFileDict[material][0] = self.materialName
        self.materialFileDict[material][1] = self.w.kerf_width.value()
        self.materialFileDict[material][2] = self.w.pierce_height.value()
        self.materialFileDict[material][3] = self.w.pierce_delay.value()
        self.materialFileDict[material][4] = self.w.puddle_jump_height.value()
        self.materialFileDict[material][5] = self.w.puddle_jump_delay.value()
        self.materialFileDict[material][6] = self.w.cut_height.value()
        self.materialFileDict[material][7] = self.w.cut_feed_rate.value()
        self.materialFileDict[material][8] = self.w.cut_amps.value()
        self.materialFileDict[material][9] = self.w.cut_volts.value()
        self.materialFileDict[material][10] = self.w.pause_at_end.value()
        self.materialFileDict[material][11] = self.w.gas_pressure.value()
        self.materialFileDict[material][12] = self.w.cut_mode.value()

        self.write_materials( \
                material, self.materialName , \
                self.w.kerf_width.value(), \
                self.w.pierce_height.value(), \
                self.w.pierce_delay.value(), \
                self.w.puddle_jump_height.value(), \
                self.w.puddle_jump_delay.value(), \
                self.w.cut_height.value(), \
                self.w.cut_feed_rate.value(), \
                self.w.cut_amps.value(), \
                self.w.cut_volts.value(), \
                self.w.pause_at_end.value(), \
                self.w.gas_pressure.value(), \
                self.w.cut_mode.value())

    def load_material_file(self):
        self.getMaterialBusy = 1
        head = _translate('HandlerClass', 'Materials Error')
        with open(self.materialFile, 'r') as f_in:
            firstpass = True
            material_error = False
            t_item = 0
            required = ['PIERCE_HEIGHT', 'PIERCE_DELAY', 'CUT_HEIGHT', 'CUT_SPEED']
            received = []
            for line in f_in:
                try:
                    if line.startswith('#'):
                        continue
                    elif line.startswith('[MATERIAL_NUMBER_') and line.strip().endswith(']'):
                        if int(line.rsplit('_', 1)[1].strip().strip(']')) < 1000000:
                            if not firstpass:
                                self.write_materials(t_number,t_name,k_width,p_height,p_delay,pj_height,pj_delay,c_height,c_speed,c_amps,c_volts,pause,g_press,c_mode,t_item)
                                for item in required:
                                    if item not in received:
                                        msg0 = _translate('HandlerClass', 'is missing from Material')
                                        STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} {} #{}\n'.format(head, item, msg0, t_number))
                            firstpass = False
                            t_number = int(line.rsplit('_', 1)[1].strip().strip(']'))
                            self.materialNumList.append(t_number)
                            t_name = k_width = p_height = p_delay = pj_height = pj_delay = c_height = c_speed = c_amps = c_volts =  pause = g_press = c_mode = 0.0
                            t_item += 1
                            received = []
                        else:
                            msg0 = _translate('HandlerClass', 'Line')
                            msg1 = _translate('HandlerClass', 'is invalid')
                            msg2 = _translate('HandlerClass', 'Material numbers need to be less than 1000000')
                            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} "{}" {}\n{}\n'.format(head, msg0, line.strip(), msg1, msg2))
                            continue
                    elif line.startswith('NAME'):
                        if line.split('=')[1].strip():
                            t_name = line.split('=')[1].strip()
                    elif line.startswith('KERF_WIDTH'):
                        if line.split('=')[1].strip():
                            k_width = float(line.split('=')[1].strip())
                    elif line.startswith('PIERCE_HEIGHT'):
                        received.append('PIERCE_HEIGHT')
                        if line.split('=')[1].strip():
                            p_height = float(line.split('=')[1].strip())
                        elif t_number:
                            msg0 = _translate('HandlerClass', 'No value for PIERCE_HEIGHT in Material')
                            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n'.format(head, msg0, t_number))
                    elif line.startswith('PIERCE_DELAY'):
                        received.append('PIERCE_DELAY')
                        if line.split('=')[1].strip():
                            p_delay = float(line.split('=')[1].strip())
                        else:
                            msg0 = _translate('HandlerClass', 'No value for PIERCE_DELAY in Material')
                            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n'.format(head, msg0, t_number))
                    elif line.startswith('PUDDLE_JUMP_HEIGHT'):
                        if line.split('=')[1].strip():
                            pj_height = float(line.split('=')[1].strip())
                    elif line.startswith('PUDDLE_JUMP_DELAY'):
                        if line.split('=')[1].strip():
                            pj_delay = float(line.split('=')[1].strip())
                    elif line.startswith('CUT_HEIGHT'):
                        received.append('CUT_HEIGHT')
                        if line.split('=')[1].strip():
                            c_height = float(line.split('=')[1].strip())
                        else:
                            msg0 = _translate('HandlerClass', 'No value for CUT_HEIGHT in Material')
                            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n'.format(head, msg0, t_number))
                    elif line.startswith('CUT_SPEED'):
                        received.append('CUT_SPEED')
                        if line.split('=')[1].strip():
                            c_speed = float(line.split('=')[1].strip())
                        else:
                            msg0 = _translate('HandlerClass', 'No value for CUT_SPEED in Material')
                            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n'.format(head, msg0, t_number))
                    elif line.startswith('CUT_AMPS'):
                        if line.split('=')[1].strip():
                            c_amps = float(line.split('=')[1].strip().replace(' ',''))
                    elif line.startswith('CUT_VOLTS'):
                        if line.split('=')[1].strip():
                            c_volts = float(line.split('=')[1].strip())
                    elif line.startswith('PAUSE_AT_END'):
                        if line.split('=')[1].strip():
                            pause = float(line.split('=')[1].strip())
                    elif line.startswith('GAS_PRESSURE'):
                        if line.split('=')[1].strip():
                            g_press = float(line.split('=')[1].strip())
                    elif line.startswith('CUT_MODE'):
                        if line.split('=')[1].strip():
                            c_mode = float(line.split('=')[1].strip())
                except:
                    msg0 = _translate('HandlerClass', 'Material file processing was aborted')
                    msg1 = _translate('HandlerClass', 'The following line in the material file')
                    msg2 = _translate('HandlerClass', 'contains an erroneous character')
                    msg3 = _translate('HandlerClass', 'Fix the line and reload the material file')
                    STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n{}\n{}:\n{}{}\n'.format(head, msg0, msg1, msg2, line, msg3))
                    material_error = True
                    break
            if not firstpass and not material_error:
                self.write_materials(t_number,t_name,k_width,p_height,p_delay,pj_height,pj_delay,c_height,c_speed,c_amps,c_volts,pause,g_press,c_mode,t_item)
                for item in required:
                    if item not in received:
                        msg0 = _translate('HandlerClass', 'is missing from Material')
                        STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} {} #{}\n'.format(head, item, msg0, t_number))
        self.display_materials()
        self.change_material(0)
        self.getMaterialBusy = 0

    def check_material_file(self):
        # create a new material file if it doesn't exist
        if not os.path.exists(self.materialFile):
            if os.path.exists('{}_material.cfg'.format(self.machineName.lower())):
                MOVE('{}_material.cfg'.format(self.machineName.lower()), self.materialFile)
                return
            with open(self.materialFile, 'w') as f_out:
                f_out.write(\
                    '# plasmac material file\n'\
                    '# example only, may be deleted\n'\
                    '# items marked * are mandatory\n'\
                    '# other items are optional and will default to 0\n'\
                    '#[MATERIAL_NUMBER_1]\n'\
                    '#NAME               = \n'\
                    '#KERF_WIDTH         = \n'\
                    '#PIERCE_HEIGHT      = *\n'\
                    '#PIERCE_DELAY       = *\n'\
                    '#PUDDLE_JUMP_HEIGHT = \n'\
                    '#PUDDLE_JUMP_DELAY  = \n'\
                    '#CUT_HEIGHT         = *\n'\
                    '#CUT_SPEED          = *\n'\
                    '#CUT_AMPS           = \n'\
                    '#CUT_VOLTS          = \n'\
                    '#PAUSE_AT_END       = \n'\
                    '#GAS_PRESSURE       = \n'\
                    '#CUT_MODE           = \n'\
                    '\n')
            head = _translate('HandlerClass', 'Materials Setup')
            msg0 = _translate('HandlerClass', 'Creating New Material File')
            STATUS.emit('error', linuxcnc.OPERATOR_DISPLAY, '{}:\n{}: {}\n'.format(head, msg0, self.materialFile))

    def material_exists(self, material):
        if int(material) in self.materialList:
            return True
        else:
            if self.autoChange:
                self.materialChangePin.set(-1)
                self.materialChangeNumberPin.set(int(self.w.materials_box.currentText().split(': ', 1)[0]))
                head = _translate('HandlerClass', 'Materials Error')
                msg0 = _translate('HandlerClass', 'Material #')
                msg1 = _translate('HandlerClass', 'not in material list')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{} {}\n'.format(head, msg0, int(material),msg1))
            return False

    def save_default_material(self):
        self.PREFS.putpref('Kerf width', self.w.kerf_width.value(), float, 'DEFAULT MATERIAL')
        self.PREFS.putpref('Pierce height',self.w.pierce_height.value(), float, 'DEFAULT MATERIAL')
        self.PREFS.putpref('Pierce delay',self.w.pierce_delay.value(), float, 'DEFAULT MATERIAL')
        self.PREFS.putpref('Puddle jump height',self.w.puddle_jump_height.value(), float, 'DEFAULT MATERIAL')
        self.PREFS.putpref('Puddle jump delay',self.w.puddle_jump_delay.value(), float, 'DEFAULT MATERIAL')
        self.PREFS.putpref('Cut height',self.w.cut_height.value(), float, 'DEFAULT MATERIAL')
        self.PREFS.putpref('Cut feed rate',self.w.cut_feed_rate.value(), float, 'DEFAULT MATERIAL')
        self.PREFS.putpref('Cut amps',self.w.cut_amps.value(), float, 'DEFAULT MATERIAL')
        self.PREFS.putpref('Cut volts',self.w.cut_volts.value(), float, 'DEFAULT MATERIAL')
        self.PREFS.putpref('Pause at end',self.w.pause_at_end.value(), float, 'DEFAULT MATERIAL')
        self.PREFS.putpref('Gas pressure',self.w.gas_pressure.value(), float, 'DEFAULT MATERIAL')
        self.PREFS.putpref('Cut mode',self.w.cut_mode.value(), float, 'DEFAULT MATERIAL')

    def load_default_material(self):
        self.write_materials( \
                0, 'DEFAULT' , \
                self.PREFS.getpref('Kerf width', round(1 * self.unitsPerMm, 2), float, 'DEFAULT MATERIAL'), \
                self.PREFS.getpref('Pierce height', round(3 * self.unitsPerMm, 2), float, 'DEFAULT MATERIAL'), \
                self.PREFS.getpref('Pierce delay', 0, float, 'DEFAULT MATERIAL'), \
                self.PREFS.getpref('Puddle jump height', 0, float, 'DEFAULT MATERIAL'), \
                self.PREFS.getpref('Puddle jump delay', 0, float, 'DEFAULT MATERIAL'), \
                self.PREFS.getpref('Cut height', round(1 * self.unitsPerMm, 2), float, 'DEFAULT MATERIAL'), \
                self.PREFS.getpref('Cut feed rate', round(4000 * self.unitsPerMm, 0), float, 'DEFAULT MATERIAL'), \
                self.PREFS.getpref('Cut amps', 45, float, 'DEFAULT MATERIAL'), \
                self.PREFS.getpref('Cut volts', 99, float, 'DEFAULT MATERIAL'), \
                self.PREFS.getpref('Pause at end', 0, float, 'DEFAULT MATERIAL'), \
                self.PREFS.getpref('Gas pressure', 0, float, 'DEFAULT MATERIAL'), \
                self.PREFS.getpref('Cut mode', 1, float, 'DEFAULT MATERIAL'),\
                0)


#########################################################################################################################
# CAMERA AND LASER FUNCTIONS #
#########################################################################################################################
    def camera_pressed(self):
        self.w.camview.rotation = STATUS.stat.rotation_xy
        if self.w.preview_stack.currentIndex() != 3:
            self.w.preview_stack.setCurrentIndex(3)
            self.button_active('camera')
            self.w.touch_xy.setEnabled(False)
            self.w.laser.setEnabled(False)
            self.cameraOn = True
        else:
            self.w.preview_stack.setCurrentIndex(0)
            self.button_normal('camera')
            self.w.touch_xy.setEnabled(True)
            self.w.laser.setEnabled(True)
            self.cameraOn = False
            ACTION.SET_MANUAL_MODE()
            self.vkb_hide()

    def laser_recovery_state_changed(self, value):
        hal.set_p('plasmac.laser-recovery-start', '0')

    def laser_clicked(self):
        if STATUS.is_interp_paused():
            return
        if self.laserButtonState == 'reset':
            self.laserButtonState = 'laser'
            self.button_normal('laser')
            self.w.touch_xy.setEnabled(True)
            self.w.camera.setEnabled(True)
            return
        xPos = STATUS.get_position()[0][0] - self.laserOffsetX
        yPos = STATUS.get_position()[0][1] - self.laserOffsetY
        if xPos < self.xMin or xPos > self.xMax or yPos < self.yMin or yPos > self.yMax:
            head = _translate('HandlerClass', 'Laser Error')
            msg0 = _translate('HandlerClass', 'Laser is outside the machine boundary')
            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n'.format(head, msg0))
            return
        if self.laserButtonState == 'laser':
            self.w.laser.setText(_translate('HandlerClass', 'MARK\nEDGE'))
            self.laserButtonState = 'markedge'
            self.button_active('laser')
            self.w.touch_xy.setEnabled(False)
            self.w.camera.setEnabled(False)
            self.laserOnPin.set(1)
            return
        elif self.laserButtonState == 'setorigin':
            self.button_normal('laser')
            self.w.touch_xy.setEnabled(True)
            self.w.camera.setEnabled(True)
            self.laserOnPin.set(0)
        self.laserButtonState = self.sheet_align(self.laserButtonState, self.w.laser, self.laserOffsetX, self.laserOffsetY)

    def laser_pressed(self):
        if STATUS.is_interp_paused() and not self.laserRecStatePin.get():
            xPos = STATUS.get_position()[0][0] + self.laserOffsetX
            yPos = STATUS.get_position()[0][1] + self.laserOffsetY
            if xPos < self.xMin or xPos > self.xMax or yPos < self.yMin or yPos > self.yMax:
                head = _translate('HandlerClass', 'Laser Error')
                msg0 = _translate('HandlerClass', 'Torch cannot move outside the machine boundary')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n'.format(head, msg0))
                return
            hal.set_p('plasmac.laser-x-offset', '{}'.format(str(int(self.laserOffsetX / self.oScale))))
            hal.set_p('plasmac.laser-y-offset', '{}'.format(str(int(self.laserOffsetY / self.oScale))))
            hal.set_p('plasmac.laser-recovery-start', '1')
            hal.set_p('plasmac.cut-recovery', '1')
            self.laserOnPin.set(1)
            return
        self.laserTimer.start(750)

    def sheet_align(self, button_state, button, offsetX, offsetY):
        if button_state == 'markedge':
            zAngle = self.w.camview.rotation = 0
            ACTION.CALL_MDI_WAIT('G10 L2 P0 R0', 3)
            ACTION.SET_MANUAL_MODE()
            self.w.gcodegraphics.logger.clear()
            self.w.cam_goto.setEnabled(False)
            button.setText(_translate('HandlerClass', 'SET\nORIGIN'))
            button_state = 'setorigin'
            self.currentX = STATUS.get_position()[0][0]
            self.currentY = STATUS.get_position()[0][1]
        else:
            if button == self.w.cam_mark:
                button.setText(_translate('HandlerClass', 'MARK\nEDGE'))
                button_state = 'markedge'
            else:
                button.setText(_translate('HandlerClass', 'LASER'))
                button_state = 'laser'
            xDiff = STATUS.get_position()[0][0] - self.currentX
            yDiff = STATUS.get_position()[0][1] - self.currentY
            if xDiff and yDiff:
                zAngle = math.degrees(math.atan(yDiff / xDiff))
                if xDiff > 0:
                    zAngle += 180
                elif yDiff > 0:
                    zAngle += 360
                if abs(xDiff) < abs(yDiff):
                    zAngle -= 90
            elif xDiff:
                if xDiff > 0:
                    zAngle = 180
                else:
                    zAngle = 0
            elif yDiff:
                if yDiff > 0:
                    zAngle = 180
                else:
                    zAngle = 0
            else:
                zAngle = 0
            self.w.camview.rotation = zAngle
            ACTION.CALL_MDI_WAIT('G10 L20 P0 X{} Y{}'.format(offsetX, offsetY))
            ACTION.CALL_MDI_WAIT('G10 L2 P0 R{}'.format(zAngle))
            ACTION.CALL_MDI('G0 X0 Y0')
            while not STATUS.is_interp_idle():
                self.w.gcodegraphics.updateGL()
            if self.fileOpened == True:
                self.file_reload_clicked()
                self.w.gcodegraphics.logger.clear()
            self.w.cam_goto.setEnabled(True)
            ACTION.SET_MANUAL_MODE()
        return button_state

    def cam_mark_clicked(self):
        xPos = STATUS.get_position()[0][0] - self.camOffsetX
        yPos = STATUS.get_position()[0][1] - self.camOffsetY
        if xPos < self.xMin or xPos > self.xMax or yPos < self.yMin or yPos > self.yMax:
            head = _translate('HandlerClass', 'Camera Error')
            msg0 = _translate('HandlerClass', 'Camera is outside the machine boundary')
            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n'.format(head, msg0))
            return
        self.camButtonState = self.sheet_align(self.camButtonState, self.w.cam_mark, self.camOffsetX, self.camOffsetY)

    def cam_goto_clicked(self):
        ACTION.CALL_MDI_WAIT('G0 X0 Y0')
        while not STATUS.is_interp_idle():
            self.w.gcodegraphics.updateGL()
        ACTION.SET_MANUAL_MODE()

    def cam_zoom_plus_pressed(self):
        if self.w.camview.scale >= 5:
            return
        self.w.camview.scale += 0.1

    def cam_zoom_minus_pressed(self):
        if self.w.camview.scale <= 1:
            return
        self.w.camview.scale -= 0.1

    def cam_dia_plus_pressed(self):
        if self.w.camview.size().height() < self.w.camview.size().width():
            size = self.w.camview.size().height()
        else:
            size = self.w.camview.size().width()
        if self.w.camview.diameter >= size - 5:
            return
        self.w.camview.diameter += 2

    def cam_dia_minus_pressed(self):
        if self.w.camview.diameter <= 2:
            return
        self.w.camview.diameter -= 2


#########################################################################################################################
# STATISTICS FUNCTIONS #
#########################################################################################################################
    def statistics_show(self):
        for stat in ['cut', 'paused', 'probe', 'run', 'torch', 'rapid']:
            self.display_hms('{}_time'.format(stat), hal.get_value('plasmac.{}-time'.format(stat)))
        self.w.cut_length.setText('{:0.2f}'.format(hal.get_value('plasmac.cut-length') / self.statsDivisor))
        self.w.pierce_count.setText('{:d}'.format(hal.get_value('plasmac.pierce-count')))
        self.statistics_load()

    def statistics_save(self, reset=False):
        self.PREFS.putpref('Cut time', '{:0.2f}'.format(self.statsSaved['cut'] + hal.get_value('plasmac.cut-time')) , float,'STATISTICS')
        self.PREFS.putpref('Paused time', '{:0.2f}'.format(self.statsSaved['paused'] + hal.get_value('plasmac.paused-time')) , float,'STATISTICS')
        self.PREFS.putpref('Probe time', '{:0.2f}'.format(self.statsSaved['probe'] + hal.get_value('plasmac.probe-time')) , float,'STATISTICS')
        self.PREFS.putpref('Program run time', '{:0.2f}'.format(self.statsSaved['run'] + hal.get_value('plasmac.run-time')) , float,'STATISTICS')
        self.PREFS.putpref('Torch on time', '{:0.2f}'.format(self.statsSaved['torch'] + hal.get_value('plasmac.torch-time')) , float,'STATISTICS')
        self.PREFS.putpref('Rapid time', '{:0.2f}'.format(self.statsSaved['rapid'] + hal.get_value('plasmac.rapid-time')) , float,'STATISTICS')
        self.PREFS.putpref('Cut length', '{:0.2f}'.format(self.statsSaved['length'] + hal.get_value('plasmac.cut-length')) , float,'STATISTICS')
        self.PREFS.putpref('Pierce count', '{:d}'.format(self.statsSaved['pierce'] + hal.get_value('plasmac.pierce-count')) , int,'STATISTICS')
        self.statistics_load()
        self.jobRunning = False

    def statistics_load(self):
        self.statsSaved['cut'] = self.PREFS.getpref('Cut time', 0 , float,'STATISTICS')
        self.statsSaved['paused'] = self.PREFS.getpref('Paused time', 0 , float,'STATISTICS')
        self.statsSaved['probe'] = self.PREFS.getpref('Probe time', 0 , float,'STATISTICS')
        self.statsSaved['run'] = self.PREFS.getpref('Program run time', 0 , float,'STATISTICS')
        self.statsSaved['torch'] = self.PREFS.getpref('Torch on time', 0 , float,'STATISTICS')
        self.statsSaved['rapid'] = self.PREFS.getpref('Rapid time', 0 , float,'STATISTICS')
        self.statsSaved['length'] = self.PREFS.getpref('Cut length', 0 , float,'STATISTICS')
        self.statsSaved['pierce'] = self.PREFS.getpref('Pierce count', 0 , int,'STATISTICS')
        for stat in ['cut', 'paused', 'probe', 'run', 'torch', 'rapid']:
            self.display_hms('{}_time_t'.format(stat), self.statsSaved['{}'.format(stat)])
        self.w.cut_length_t.setText('{:0.2f}'.format(self.statsSaved['length'] / self.statsDivisor))
        self.w.pierce_count_t.setText('{:d}'.format(self.statsSaved['pierce']))

    def display_hms(self, widget, time):
        m, s = divmod(time, 60)
        h, m = divmod(m, 60)
        self.w[widget].setText('{:.0f}:{:02.0f}:{:02.0f}'.format(h,m,s))

    def statistic_reset(self, stat, statT):
        if stat in ['cut_time', 'paused_time', 'probe_time', 'run_time', 'torch_time', 'rapid_time']:
            self.display_hms('{}'.format(stat), 0)
        elif stat == 'cut_length':
            self.w.cut_length.setText('0.00')
        elif stat == 'pierce_count':
            self.w.pierce_count.setText('0')
        self.PREFS.putpref(statT, 0 , float,'STATISTICS')
        self.statistics_load()

    def statistics_reset(self):
        for stat in ['cut', 'paused', 'probe', 'run', 'torch', 'rapid']:
            self.display_hms('{}_time'.format(stat), 0)
        self.w.cut_length.setText('0.00')
        self.w.pierce_count.setText('0')
        for stat in ['Cut time', 'Paused time', 'Probe time', 'Program run time', 'Torch on time', \
                     'Rapid time', 'Cut length']:
            self.PREFS.putpref(stat, 0 , float,'STATISTICS')
        self.PREFS.putpref('Pierce count', 0 , int ,'STATISTICS')
        self.statistics_load()

    def statistics_init(self):
        if self.unitsPerMm == 1:
            self.statsDivisor = 1000
            unit = _translate('HandlerClass', 'Metres')
        else:
            self.statsDivisor = 1
            unit = _translate('HandlerClass', 'Inches')
        msg0 = _translate('HandlerClass', 'CUT LENGTH')
        self.w.cut_length_label.setText('{} ({})'.format(msg0, unit))
        self.statsSaved = {'cut':0, 'length':0, 'pierce':0, 'paused':0, 'probe':0, 'rapid':0, 'run':0, 'torch':0}
        self.jobRunning = False
        self.statistics_load()


#########################################################################################################################
# POWERMAX COMMUNICATIONS FUNCTIONS #
#########################################################################################################################
    def pmx485_check(self, port, periodic=False):
        try:
            import serial
            import serial.tools.list_ports as PORTS
            head = _translate('HandlerClass', 'Port Error')
            msg1 = _translate('HandlerClass', 'Powermax communications are disabled')
            ports = []
            for p in PORTS.comports():
                ports.append(p[0])
            if port in ports:
                try:
                    sPort = serial.Serial(port, 19200)
                    sPort.close()
                except Exception as err:
                    if not periodic:
                        STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n{}'.format(head, err, msg1))
                    return False
            else:
                if not periodic:
                    msg0 = _translate('cannot be found')
                    STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} {}\n{}'.format(head, port, msg0, msg1))
                return False
        except:
            if not periodic:
                head = _translate('HandlerClass', 'Module Error')
                msg0 = _translate('HandlerClass', 'python3-serial cannot be found')
                msg1 = _translate('HandlerClass', 'Install python3-serial or linuxcnc-dev')
            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n{}'.format(head, msg0, msg1))
            return False
        return True

    def pmx485_startup(self, port):
        self.pmx485CommsError = False
        self.w.pmx485Status = False
        self.meshMode = False
        self.w.pmx485_enable.stateChanged.connect(lambda w:self.pmx485_enable_changed(self.w.pmx485_enable.isChecked()))
        self.pmx485StatusPin.value_changed.connect(lambda w:self.pmx485_status_changed(w))
        self.pmx485ModePin.value_changed.connect(self.pmx485_mode_changed)
        self.pmx485FaultPin.value_changed.connect(lambda w:self.pmx485_fault_changed(w))
        self.pmx485ArcTimePin.value_changed.connect(lambda w:self.pmx485_arc_time_changed(w))
        self.w.gas_pressure.valueChanged.connect(self.pmx485_pressure_changed)
        self.w.mesh_enable.stateChanged.connect(lambda w:self.pmx485_mesh_enable_changed(self.w.mesh_enable.isChecked()))
        self.pmx485CommsTimer = QTimer()
        self.pmx485CommsTimer.timeout.connect(self.pmx485_timeout)
        self.pmx485RetryTimer = QTimer()
        self.pmx485RetryTimer.timeout.connect(lambda:self.pmx485_enable_changed(True))
        self.oldCutMode = self.w.cut_mode.value()
        self.pressure = self.w.gas_pressure.value()
        if self.pmx485_load(port):
            return
        self.pmx485Exists = True
        self.pmx485_setup()
        self.w.pmx485_enable.setChecked(True)

    def pmx485_load(self, port):
        head = _translate('HandlerClass', 'Comms Error')
        msg0 = _translate('HandlerClass', 'PMX485 component is not able to be loaded,')
        msg1 = _translate('HandlerClass', 'Powermax communications are not available')
        err = '{}:\n{}\n{}\n'.format(head, msg0, msg1)
        count = 0
        while not hal.component_exists('pmx485'):
            if count >= 3:
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, err)
                return 1
            RUN(['halcmd', 'loadusr', '-Wn', 'pmx485', 'pmx485', '{}'.format(port)])
            count += 1
        return 0

    def pmx485_setup(self):
        self.pins485Comp = ['pmx485.enable', 'pmx485.status', 'pmx485.fault', \
                    'pmx485.mode_set', 'pmx485.mode', \
                    'pmx485.current_set', 'pmx485.current', 'pmx485.current_min', 'pmx485.current_max', \
                    'pmx485.pressure_set', 'pmx485.pressure', 'pmx485.pressure_min', 'pmx485.pressure_max', 'pmx485.arcTime']
        pinsSelf = ['pmx485_enable', 'pmx485_status', 'pmx485_fault', \
                    'cut_mode-f', 'pmx485_mode', \
                    'cut_amps-f', 'pmx485_current', 'pmx485_current_min', 'pmx485_current_max', \
                    'gas_pressure-f', 'pmx485_pressure', 'pmx485_pressure_min', 'pmx485_pressure_max', 'pmx485_arc_time']
        pinType = [hal.HAL_BIT, hal.HAL_BIT, hal.HAL_FLOAT, \
                   hal.HAL_FLOAT, hal.HAL_FLOAT, \
                   hal.HAL_FLOAT, hal.HAL_FLOAT, hal.HAL_FLOAT, hal.HAL_FLOAT, \
                   hal.HAL_FLOAT, hal.HAL_FLOAT, hal.HAL_FLOAT, hal.HAL_FLOAT, hal.HAL_FLOAT]
        for pin in self.pins485Comp:
            hal.new_sig('plasmac:{}'.format(pin.replace('pmx485.', 'pmx485_')), pinType[self.pins485Comp.index(pin)])
            hal.connect(pin,'plasmac:{}'.format(pin.replace('pmx485.', 'pmx485_')))
            hal.connect('qtplasmac.{}'.format(pinsSelf[self.pins485Comp.index(pin)]),'plasmac:{}'.format(pin.replace('pmx485.', 'pmx485_')))
        self.pmx485_mesh_enable_changed(self.w.mesh_enable.isChecked())
        self.w.cut_amps.setToolTip(_translate('HandlerClass', 'Powermax cutting current'))

    def pmx485_enable_changed(self, state):
        if state:
            self.pmx485CommsError = False
            self.pmx485RetryTimer.stop()
            if self.pmx485_load(self.PREFS.getpref('Port', '', str, 'POWERMAX')):
                return
            # if pins not connected then connect them
            if not hal.pin_has_writer('pmx485.enable'):
                self.pmx485_setup()
            # ensure valid parameters before trying to connect
            if self.w.cut_mode.value() == 0 or self.w.cut_amps.value() == 0:
                head = _translate('HandlerClass', 'Materials Error')
                msg0 = _translate('HandlerClass', 'Invalid Cut Mode or Cut Amps,')
                msg1 = _translate('HandlerClass', 'cannot connect to Powermax')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n{}\n'.format(head, msg0, msg1))
                self.w.pmx485_enable.setChecked(False)
                return
            # good to go
            else:
                self.w.pmx485_label.setText(_translate('HandlerClass', 'CONNECTING'))
                self.pmx485LabelState = 'CONNECT'
                self.pmx485CommsTimer.start(3000)
        else:
            self.pmx485Connected = False
            self.pmx485CommsError = False
            self.w.pmx485_label.setText('')
            self.pmx485LabelState = None
            self.w.pmx485_label.setToolTip(_translate('HandlerClass', 'Status of PMX485 communications'))
            self.pmx485CommsTimer.stop()
            self.pmx485RetryTimer.stop()

    def pmx485_mode_changed(self, widget):
        if self.pmx485Connected:
            self.w.gas_pressure.setValue(0)

    def pmx485_pressure_changed(self, pressure):
        if self.pmx485Connected:
            if pressure < self.pressure:
                if pressure < 0:
                    self.w.gas_pressure.setValue(self.gas_maximum)
                elif self.w.gas_pressure.value() < self.gas_minimum:
                    self.w.gas_pressure.setValue(0)
            elif pressure > self.pressure:
                if pressure > self.gas_maximum:
                    self.w.gas_pressure.setValue(0)
                elif pressure < self.gas_minimum:
                    self.w.gas_pressure.setValue(self.gas_minimum)
            self.pressure = self.w.gas_pressure.value()

    def pmx485_min_max_changed(self):
        if not self.pmx485Connected: return
        self.w.cut_amps.setMinimum(self.pmx485CurrentMinPin.get())
        self.w.cut_amps.setMaximum(self.pmx485CurrentMaxPin.get())
        self.gas_minimum = self.pmx485PressureMinPin.get()
        self.gas_maximum = self.pmx485PressureMaxPin.get()
        self.w.gas_pressure.setMinimum(-1)
        self.w.gas_pressure.setMaximum(self.gas_maximum + 1)
        if self.gas_maximum > 15:
            self.w.gas_pressure.setSuffix(' psi')
            self.w.gas_pressure.setDecimals(0)
            self.w.gas_pressure.setSingleStep(1)
        else:
            self.w.gas_pressure.setSuffix(' bar')
            self.w.gas_pressure.setDecimals(1)
            self.w.gas_pressure.setSingleStep(0.1)

    def pmx485_status_changed(self, state):
        if state != self.pmx485Connected:
            if state:
                self.pmx485CommsError = False
                self.w.pmx485_label.setText(_translate('HandlerClass', 'CONNECTED'))
                self.pmx485LabelState = 'CONNECT'
                self.pmx485Connected = True
                self.pmx485_min_max_changed()
                if self.pmx485ArcTimePin.get():
                    self.pmx485_arc_time_changed(self.pmx485ArcTimePin.get())
                if self.pmx485FaultPin.get():
                    self.pmx485_fault_changed(self.pmx485FaultPin.get())
                self.pmx485CommsTimer.stop()
                self.pmx485RetryTimer.stop()
            else:
                self.w.pmx485_label.setText(_translate('HandlerClass', 'COMMS ERROR'))
                self.pmx485LabelState = None
                self.pmx485CommsError = True
                self.pmx485Connected = False
                self.pmx485RetryTimer.start(3000)

    def pmx485_arc_time_changed(self, time):
        if self.pmx485Connected:
            self.pmx485ArcTime = self.pmx485ArcTimePin.get()
            self.w.pmx_arc_time_label.setText(_translate('HandlerClass', 'ARC ON TIME'))
            self.display_hms('pmx_arc_time_t', self.pmx485ArcTime)

    def pmx485_fault_changed(self, fault):
        if self.pmx485Connected:
            faultRaw = '{:04.0f}'.format(fault)
            self.pmx485FaultCode = '{}-{}-{}'.format(faultRaw[0], faultRaw[1:3], faultRaw[3])
            head = _translate('HandlerClass', 'Powermax Error')
            code = _translate('HandlerClass', 'Fault Code')
            text = _translate('HandlerClass', 'Powermax error')
            if faultRaw == '0000':
                self.w.pmx485_label.setText(_translate('HandlerClass', 'CONNECTED'))
                self.pmx485LabelState = 'CONNECT'
                self.w.pmx485_label.setToolTip(_translate('HandlerClass', 'Status of PMX485 communications'))
            elif faultRaw in self.pmx485FaultName.keys():
                if faultRaw == '0210' and self.w.pmx485.current_max.value() > 110:
                    faultMsg = self.pmx485FaultName[faultRaw][1]
                elif faultRaw == '0210':
                    faultMsg = self.pmx485FaultName[faultRaw][0]
                else:
                    faultMsg = self.pmx485FaultName[faultRaw]
                self.w.pmx485_label.setText('{}: {}'.format(code, self.pmx485FaultCode))
                self.pmx485LabelState = None
                self.w.pmx485_label.setStatusTip('{} ({}) {}'.format(text, self.pmx485FaultCode, faultMsg))
                msg0 = _translate('HandlerClass', 'CODE')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}: {}\n{}\n'.format(head, msg0, self.pmx485FaultCode, faultMsg))
            else:
                self.w.pmx485_label.setText('{}: {}'.format(code, faultRaw))
                self.pmx485LabelState = None
                msg0 = _translate('HandlerClass', 'Unknown Powermax fault code')
                self.w.pmx485_label.setStatusTip('{} ({})'.format(msg0, faultRaw))
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}: {}\n'.format(head, msg0, faultRaw))

    def pmx485_mesh_enable_changed(self, state):
        if state and not self.meshMode:
            self.oldCutMode = self.w.cut_mode.value()
            self.w.cut_mode.setValue(2)
            self.w.cut_mode.setEnabled(False)
            self.meshMode = True
        elif not hal.get_value('plasmac.mesh-enable') and self.meshMode:
            self.w.cut_mode.setValue(self.oldCutMode)
            self.w.cut_mode.setEnabled(True)
            self.meshMode = False

    def pmx485_timeout(self):
        self.pmx485CommsTimer.stop()
        self.w.pmx485_label.setText(_translate('HandlerClass', 'COMMS ERROR'))
        self.pmx485LabelState = None
        self.pmx485CommsError = True
        self.pmx485Connected = False
        self.pmx485RetryTimer.start(3000)

    pmx485FaultName = {
                '0110': 'Remote controller mode invalid',
                '0111': 'Remote controller current invalid',
                '0112': 'Remote controller pressure invalid',
                '0120': 'Low input gas pressure',
                '0121': 'Output gas pressure low',
                '0122': 'Output gas pressure high',
                '0123': 'Output gas pressure unstable',
                '0130': 'AC input power unstable',
                '0199': 'Power board hardware protection',
                '0200': 'Low gas pressure',
                '0210': ('Gas flow lost while cutting', 'Excessive arc voltage'),
                '0220': 'No gas input',
                '0300': 'Torch stuck open',
                '0301': 'Torch stuck closed',
                '0320': 'End of consumable life',
                '0400': 'PFC/Boost IGBT module under temperature',
                '0401': 'PFC/Boost IGBT module over temperature',
                '0402': 'Inverter IGBT module under temperature',
                '0403': 'Inverter IGBT module over temperature',
                '0500': 'Retaining cap off',
                '0510': 'Start/trigger signal on at power up',
                '0520': 'Torch not connected',
                '0600': 'AC input voltage phase loss',
                '0601': 'AC input voltage too low',
                '0602': 'AC input voltage too high',
                '0610': 'AC input unstable',
                '0980': 'Internal communication failure',
                '0990': 'System hardware fault',
                '1000': 'Digital signal processor fault',
                '1100': 'A/D converter fault',
                '1200': 'I/O fault',
                '2000': 'A/D converter value out of range',
                '2010': 'Auxiliary switch disconnected',
                '2100': 'Inverter module temp sensor open',
                '2101': 'Inverter module temp sensor shorted',
                '2110': 'Pressure sensor is open',
                '2111': 'Pressure sensor is shorted',
                '2200': 'DSP does not recognize the torch',
                '3000': 'Bus voltage fault',
                '3100': 'Fan speed fault',
                '3101': 'Fan fault',
                '3110': 'PFC module temperature sensor open',
                '3111': 'PFC module temperature sensor shorted',
                '3112': 'PFC module temperature sensor circuit fault',
                '3200': 'Fill valve',
                '3201': 'Dump valve',
                '3201': 'Valve ID',
                '3203': 'Electronic regulator is disconnected',
                '3410': 'Drive fault',
                '3420': '5 or 24 VDC fault',
                '3421': '18 VDC fault',
                '3430': 'Inverter capacitors unbalanced',
                '3441': 'PFC over current',
                '3511': 'Inverter saturation fault',
                '3520': 'Inverter shoot-through fault',
                '3600': 'Power board fault',
                '3700': 'Internal serial communications fault',
                }


#########################################################################################################################
# CUT RECOVERY FUNCTIONS #
#########################################################################################################################
    def set_cut_recovery(self):
        if hal.get_value('plasmac.cut-recovering'):
            self.w.jog_stack.setCurrentIndex(1)
            self.cancelWait = True
            return
        self.w.jog_stack.setCurrentIndex(1)
        self.cancelWait = False
        self.w.cut_rec_cancel.setEnabled(False)
        self.cutrec_speed_changed(self.w.cut_rec_speed.value())
        hal.set_p('plasmac.cut-recovery', '0')
        self.laserOnPin.set(0)
        self.xOrig = hal.get_value('axis.x.eoffset-counts')
        self.yOrig = hal.get_value('axis.y.eoffset-counts')
        self.zOrig = hal.get_value('axis.z.eoffset-counts')
        self.oScale = hal.get_value('plasmac.offset-scale')

    def cutrec_speed_changed(self, speed):
        text = _translate('HandlerClass', 'FEED')
        if STATUS.is_metric_mode():
            self.w.cut_rec_feed.setText('{}\n{:0.0f}'.format(text, self.w.cut_feed_rate.value() * speed * 0.01))
        else:
            self.w.cut_rec_feed.setText('{}\n{:0.1f}'.format(text, self.w.cut_feed_rate.value() * speed * 0.01))

    def cutrec_move_changed(self, distance):
        text = _translate('HandlerClass', 'MOVE')
        self.w.cut_rec_move_label.setText('{}\n{}'.format(text, distance))
#        self.w.cut_rec_move_label.setText('MOVE\n{}'.format(distance))

    def cutrec_motion(self, direction):
        if self.w.cut_rec_fwd.isEnabled() and self.w.cut_rec_rev.isEnabled():
            speed = float(self.w.cut_rec_speed.value()) * 0.01 * direction
            hal.set_p('plasmac.paused-motion-speed',str(speed))
            hal.set_p('plasmac.cut-recovery', '1')

    def cutrec_move(self, state, x, y):
        if not STATUS.is_interp_paused():
            return
        if state:
            maxMove = 10
            if self.units == 'in':
                maxMove = 0.4
            laser = self.laserRecStatePin.get() > 0
            distX = hal.get_value('qtplasmac.kerf_width-f') * x
            distY = hal.get_value('qtplasmac.kerf_width-f') * y
            xNew = hal.get_value('plasmac.axis-x-position') + hal.get_value('axis.x.eoffset') - (self.laserOffsetX * laser) + distX
            yNew = hal.get_value('plasmac.axis-y-position') + hal.get_value('axis.y.eoffset') - (self.laserOffsetY * laser) + distY
            if xNew > self.xMax or xNew < self.xMin or yNew > self.yMax or yNew < self.yMin:
                return
            xTotal = hal.get_value('axis.x.eoffset') - (self.laserOffsetX * laser) + distX
            yTotal = hal.get_value('axis.y.eoffset') - (self.laserOffsetY * laser) + distY
            if xTotal > maxMove or xTotal < -maxMove or yTotal > maxMove or yTotal < -maxMove:
                return
            moveX = int(distX / self.oScale)
            moveY = int(distY / self.oScale)
            hal.set_p('plasmac.x-offset', '{}'.format(str(hal.get_value('plasmac.x-offset') + moveX)))
            hal.set_p('plasmac.y-offset', '{}'.format(str(hal.get_value('plasmac.y-offset') + moveY)))
            hal.set_p('plasmac.cut-recovery', '1')

    def cutrec_offset_changed(self, xOffset, yOffset):
        if hal.get_value('plasmac.consumable-changing'):
            return
        if xOffset > 0.001 * self.unitsPerMm or xOffset < -0.001 * self.unitsPerMm or \
           yOffset > 0.001 * self.unitsPerMm or yOffset < -0.001 * self.unitsPerMm:
            self.w.cut_rec_cancel.setEnabled(True)
            if self.laserRecStatePin.get():
                self.w.laser.setEnabled(False)
            if self.ccButton:
                self.w[self.ccButton].setEnabled(False)
        elif not self.laserRecStatePin.get():
            self.cancelWait = False
            self.cutrec_buttons_enable(True)
            self.cutrec_motion_enable(True)
            self.w.cut_rec_cancel.setEnabled(False)
            hal.set_p('plasmac.cut-recovery', '0')
            hal.set_p('plasmac.x-offset', '0')
            hal.set_p('plasmac.y-offset', '0')
            self.laserOnPin.set(0)
            if STATUS.is_interp_paused():
                self.w.laser.setEnabled(True)
                if self.ccButton:
                    self.w[self.ccButton].setEnabled(True)

    def cutrec_cancel_pressed(self, state):
        if (state):
            if hal.get_value('plasmac.cut-recovery'):
                self.cancelWait = True
                hal.set_p('plasmac.cut-recovery', '0')
                self.laserOnPin.set(0)

    def cutrec_motion_enable(self, state):
        for widget in ['fwd', 'rev', 'speed']:
            self.w['cut_rec_{}'.format(widget)].setEnabled(state)

    def cutrec_buttons_enable(self, state):
        for widget in ['n', 'ne', 'e', 'se', 's', 'sw', 'w', 'nw', 'cancel', 'feed', 'move_label']:
            self.w['cut_rec_{}'.format(widget)].setEnabled(state)


#########################################################################################################################
# STYLING FUNCTIONS #
#########################################################################################################################
    def openColorDialog(self, widget):
        initColor = QColor(widget.palette().color(QPalette.Background))
        options  = QColorDialog.DontUseNativeDialog
        options |= QColorDialog.ShowAlphaChannel
        color = QColorDialog.getColor(initColor, options=options)
        if color.isValid():
            widget.setStyleSheet('background-color: {}'.format(color.name()))
            buttons = ['foregrnd', 'foregalt', 'led', 'backgrnd', 'backgalt', 'frams', 'estop', 'disabled', 'preview']
            labels = ['Foreground', 'Highlight', 'LED', 'Background', 'Background Alt', 'Frames', 'Estop', 'Disabled', 'Preview']
            button = widget.objectName()
            label = labels[buttons.index(button.split('_')[1])]
            self.PREFS.putpref(label,  color.name(), str, 'COLOR_OPTIONS')
            self.set_basic_colors()
            self.set_color_styles()

    def set_basic_colors(self):
        self.foreColor = self.PREFS.getpref('Foreground', '#ffee06', str, 'COLOR_OPTIONS')
        self.fore1Color = self.PREFS.getpref('Highlight', '#ffee06', str, 'COLOR_OPTIONS')
        self.backColor = self.PREFS.getpref('Background', '#16160e', str, 'COLOR_OPTIONS')
        self.back1Color = self.PREFS.getpref('Background Alt', '#26261e', str, 'COLOR_OPTIONS')
        self.disabledColor = self.PREFS.getpref('Disabled', '#b0b0b0', str, 'COLOR_OPTIONS')
        self.estopColor = self.PREFS.getpref('Estop', '#ff0000', str, 'COLOR_OPTIONS')

    def set_color_styles(self):
        self.styleSheetFile = os.path.join(self.PATHS.CONFIGPATH, 'qtplasmac.qss')
        ssFile = self.PREFS.getpref('Custom style', 'None', str, 'GUI_OPTIONS')
        # if custom stylesheet try to use it
        if ssFile != 'None':
            COPY(ssFile, self.styleSheetFile)
            self.custom_stylesheet()
        # otherwise use the standard stylesheet
        else:
            self.standard_stylesheet()
        # apply the new stylesheet
        self.w.setStyleSheet('')
        with open(self.styleSheetFile, 'r') as set_style:
           self.w.setStyleSheet(set_style.read())
        # style some buttons
        buttons = ['jog_x_minus', 'jog_x_plus', 'jog_y_minus', 'jog_y_plus',
                   'jog_z_minus', 'jog_z_plus', 'jog_a_minus', 'jog_a_plus',
                   'jog_b_minus', 'jog_b_plus',
                   'cut_rec_n', 'cut_rec_ne', 'cut_rec_e', 'cut_rec_se',
                   'cut_rec_s', 'cut_rec_sw', 'cut_rec_w', 'cut_rec_nw',
                   'conv_line', 'conv_circle', 'conv_ellipse', 'conv_triangle',
                   'conv_rectangle', 'conv_polygon', 'conv_bolt', 'conv_slot',
                   'conv_star', 'conv_gusset', 'conv_sector', 'conv_block']
        conv_images = ['conv_line_point', 'conv_line_angle', 'conv_line_3p',
                      'conv_line_2pr', 'conv_arc_angle', 'conv_bolt_l',
                      'conv_circle_l', 'conv_ellipse_l', 'conv_gusset_l',
                      'conv_polygon_l', 'conv_rectangle_l', 'conv_sector_l',
                      'conv_slot_l', 'conv_star_l', 'conv_triangle_l']
        for button in buttons:
            self.color_item(button, self.foreColor, 'button')
            self.w[button].setStyleSheet(\
                    'QPushButton {{ background: {0} }} \
                     QPushButton:pressed {{ background: {0} }}'.format(self.backColor))
        for conv_image in conv_images:
            self.color_item(conv_image, self.foreColor, 'image')
        # some gcode display/editor colors cannot use .qss file
        # gcode display current gcode line
        self.w.gcode_display.setMarkerBackgroundColor(QColor(self.back1Color))
        # gcode display active line
        self.w.gcode_display.setCaretLineBackgroundColor(QColor(self.back1Color))
        # gcode editor current gcode line
        self.w.gcode_editor.editor.setMarkerBackgroundColor(QColor(self.back1Color))
        self.w.gcode_editor.editor.setCaretForegroundColor(QColor(self.fore1Color))
        # gcode editor active line
        self.w.gcode_editor.editor.setCaretLineBackgroundColor(QColor(self.backColor))

    def standard_stylesheet(self):
        # create stylesheet .qss file from template
        styleTemplateFile = os.path.join(self.PATHS.SCREENDIR, self.PATHS.BASEPATH, 'qtplasmac.style')
        with open(styleTemplateFile, 'r') as inFile:
            with open(self.styleSheetFile, 'w') as outFile:
                for line in inFile:
                    if 'foregnd' in line:
                        outFile.write(line.replace('foregnd', self.w.color_foregrnd.styleSheet().split(':')[1].strip()))
                        self.colorFgPin.set(int(self.w.color_foregrnd.styleSheet().split(':')[1].strip().lstrip('#'), 16))
                    elif 'highlight' in line:
                        outFile.write(line.replace('highlight', self.w.color_foregalt.styleSheet().split(':')[1].strip()))
                    elif 'l-e-d' in line:
                        outFile.write(line.replace('l-e-d', self.w.color_led.styleSheet().split(':')[1].strip()))
                    elif 'backgnd' in line:
                        outFile.write(line.replace('backgnd', self.w.color_backgrnd.styleSheet().split(':')[1].strip()))
                        self.colorBgPin.set(int(self.w.color_backgrnd.styleSheet().split(':')[1].strip().lstrip('#'), 16))
                    elif 'backalt' in line:
                        outFile.write(line.replace('backalt', self.w.color_backgalt.styleSheet().split(':')[1].strip()))
                        self.colorBgAltPin.set(int(self.w.color_backgalt.styleSheet().split(':')[1].strip().lstrip('#'), 16))
                    elif 'frames' in line:
                        outFile.write(line.replace('frames', self.w.color_frams.styleSheet().split(':')[1].strip()))
                    elif 'e-stop' in line:
                        outFile.write(line.replace('e-stop', self.w.color_estop.styleSheet().split(':')[1].strip()))
                    elif 'inactive' in line:
                        outFile.write(line.replace('inactive', self.w.color_disabled.styleSheet().split(':')[1].strip()))
                    elif 'prevu' in line:
                        outFile.write(line.replace('prevu', self.w.color_preview.styleSheet().split(':')[1].strip()))
                    else:
                        outFile.write(line)

        # append custom style if found
        if os.path.isfile(os.path.join(self.PATHS.CONFIGPATH, 'qtplasmac_custom.qss')):
            with open(os.path.join(self.PATHS.CONFIGPATH, 'qtplasmac_custom.qss'), 'r') as inFile:
                with open(self.styleSheetFile, 'a') as outFile:
                    outFile.write(inFile.read())

    def custom_stylesheet(self):
        head = _translate('HandlerClass', 'Stylesheet Error')
        try:
            # set basic colors from stylesheet header
            colors = [0,0,0,0,0]
            with open(self.styleSheetFile, 'r') as inFile:
                for line in inFile:
                    if line.startswith('color1'):
                        colors[0] += 1
                        self.foreColor = QColor(line.split('=')[1].strip()).name()
                        self.colorFgPin.set(int(QColor(line.split('=')[1].strip()).name().lstrip('#'), 16))
                    elif line.startswith('color2'):
                        colors[1] += 1
                        self.backColor = QColor(line.split('=')[1].strip()).name()
                        self.colorBgPin.set(int(QColor(line.split('=')[1].strip()).name().lstrip('#'), 16))
                    elif line.startswith('color3'):
                        colors[2] += 1
                        self.fore1Color = QColor(line.split('=')[1].strip()).name()
                    elif line.startswith('color4'):
                        colors[3] += 1
                        self.back1Color = QColor(line.split('=')[1].strip()).name()
                        self.colorBgAltPin.set(int(QColor(line.split('=')[1].strip()).name().lstrip('#'), 16))
                    elif line.startswith('color5'):
                        colors[4] += 1
                        self.disabledColor = QColor(line.split('=')[1].strip()).name()
                    if line.startswith('*'):
                        break
                if colors != [1,1,1,1,1]:
                    raise ColorError()
                # hide color buttons
                for button in ['color_foregrnd', 'color_foregrnd_lbl', 'color_foregalt', \
                               'color_foregalt_lbl', 'color_backgrnd', 'color_backgrnd_lbl', \
                               'color_backgalt', 'color_backgalt_lbl', 'color_frams', \
                               'color_frams_lbl', 'color_estop', 'color_estop_lbl', \
                               'color_disabled', 'color_disabled_lbl', 'color_preview', \
                               'color_preview_lbl', 'color_led', 'color_led_lbl']:
                    self.w[button].hide()
                for button in ['camera', 'laser', self.ctButton, self.tpButton, self.ptButton, self.ccButton]:
                    if button:
                        self.button_normal(button)
        except ColorError:
            msg0 = _translate('HandlerClass', 'Invalid number of colors defined')
            msg1 = _translate('HandlerClass', 'in custom stylesheet header')
            msg2 = _translate('HandlerClass', 'Reverting to standard stylesheet')
            self.dialog_show_ok(QMessageBox.Warning, '{}'.format(head), '\n{}\n{}\n\n{}\n'.format(msg0, msg1, msg2))
            self.standard_stylesheet()
        except:
            msg0 = _translate('HandlerClass', 'Cannot open custom stylesheet')
            msg1 = _translate('HandlerClass', 'Reverting to standard stylesheet')
            self.dialog_show_ok(QMessageBox.Warning, '{}'.format(head), '\n{}\n\n{}\n'.format(msg0, msg1))
            self.standard_stylesheet()

    def color_item(self, item, color, type):
        image_path = '{}{}.svg'.format(self.IMAGES, item)
        self.image = QPixmap(image_path)
        colorChange = QPainter(self.image)
        colorChange.setCompositionMode(QPainter.CompositionMode_SourceIn)
        colorChange.fillRect( self.image.rect(), QColor(color) )
        colorChange.end()
        if type == 'button':
            self.w[item].setIcon(QIcon(self.image))
        elif type == 'image':
            self[item] = QPixmap(self.image)


#########################################################################################################################
# KEY BINDING CALLS #
#########################################################################################################################
    def key_is_valid(self, event, state):
        return self.keyboard_shortcuts() and state and not event.isAutoRepeat()

    def jog_is_valid(self, key, event):
        return self.keyboard_shortcuts() and not event.isAutoRepeat() and not self.w.main_tab_widget.currentIndex() and self.w['jog_{}'.format(key)].isEnabled()

    def on_keycall_ESTOP(self, event, state, shift, cntrl):
        if self.key_is_valid(event, state) and self.w.estopButton == 2:
            ACTION.SET_ESTOP_STATE(STATUS.estop_is_clear())

    def on_keycall_POWER(self, event, state, shift, cntrl):
        if self.key_is_valid(event, state):
            ACTION.SET_MACHINE_STATE(not STATUS.machine_is_on())

    def on_keycall_ABORT(self, event, state, shift, cntrl):
        if self.key_is_valid(event, state):
            self.abort_pressed()

    def on_keycall_HOME(self, event, state, shift, cntrl):
        if self.key_is_valid(event, state) and cntrl and not shift and not self.w.main_tab_widget.currentIndex() and STATUS.is_on_and_idle() and self.w.home_all.isEnabled():
            if STATUS.is_all_homed():
                ACTION.SET_MACHINE_UNHOMED(-1)
            else:
                ACTION.SET_MACHINE_HOMING(-1)

    def on_keycall_RUN(self, event, state, shift, cntrl):
        if self.key_is_valid(event, state) and cntrl and not shift and not self.w.main_tab_widget.currentIndex():
            if self.w.run.isEnabled():
                self.run_clicked()
            elif self.w.pause.isEnabled():
                ACTION.PAUSE()

    def on_keycall_PAUSE(self, event, state, shift, cntrl):
        if self.key_is_valid(event, state) and not shift and not self.w.main_tab_widget.currentIndex():
            if cntrl:
                if self.w.screen_options.desktop_notify:
                    self.w.screen_options.QTVCP_INSTANCE_._NOTICE.external_close()
                self.error_status(False)
            elif self.w.pause.isEnabled() and STATUS.stat.interp_state != linuxcnc.INTERP_PAUSED:
                ACTION.PAUSE()

    def on_keycall_OPEN(self, event, state, shift, cntrl):
        if self.key_is_valid(event, state) and not self.w.main_tab_widget.currentIndex() and \
           self.w.file_open.isEnabled():
            self.file_open_clicked()

    def on_keycall_LOAD(self, event, state, shift, cntrl):
        if self.key_is_valid(event, state) and not self.w.main_tab_widget.currentIndex() and \
           self.w.file_reload.isEnabled():
            self.file_reload_clicked()

    def on_keycall_F12(self, event, state, shift, cntrl):
        if self.key_is_valid(event, state):
            self.STYLEEDITOR.load_dialog()

    def on_keycall_F9(self, event, state, shift, cntrl):
        if self.key_is_valid(event, state) and not self.w.main_tab_widget.currentIndex() \
           and not self.probeTest and not self.torchPulse and not self.framing and STATUS.is_interp_idle():
            self.manual_cut()

    def on_keycall_XPOS(self, event, state, shift, cntrl):
        if self.jog_is_valid('x_plus', event):
            if STATUS.is_joint_mode():
                self.kb_jog(state, self.coordinates.index('x'), 1, shift)
            else:
                self.kb_jog(state, 0, 1, shift)

    def on_keycall_XNEG(self, event, state, shift, cntrl):
        if self.jog_is_valid('x_minus', event):
            if STATUS.is_joint_mode():
                self.kb_jog(state, self.coordinates.index('x'), -1, shift)
            else:
                self.kb_jog(state, 0, -1, shift)

    def on_keycall_YPOS(self, event, state, shift, cntrl):
        if self.jog_is_valid('y_plus', event):
            if STATUS.is_joint_mode():
                self.kb_jog(state, self.coordinates.index('y'), 1, shift)
            else:
                self.kb_jog(state, 1, 1, shift)

    def on_keycall_YNEG(self, event, state, shift, cntrl):
        if self.jog_is_valid('y_minus', event):
            if STATUS.is_joint_mode():
                self.kb_jog(state, self.coordinates.index('y'), -1, shift)
            else:
                self.kb_jog(state, 1, -1, shift)

    def on_keycall_ZPOS(self, event, state, shift, cntrl):
        if self.jog_is_valid('z_plus', event):
            if self.zPlusOverrideJog and self.w.chk_override_jog.isEnabled():
                self.w.chk_override_jog.setChecked(True)
            if STATUS.is_joint_mode():
                self.kb_jog(state, self.coordinates.index('z'), 1, shift)
            else:
                self.kb_jog(state, 2, 1, shift)

    def on_keycall_ZNEG(self, event, state, shift, cntrl):
        if self.jog_is_valid('z_minus', event):
            if STATUS.is_joint_mode():
                self.kb_jog(state, self.coordinates.index('z'), -1, shift)
            else:
                self.kb_jog(state, 2, -1, shift)

    def on_keycall_APOS(self, event, state, shift, cntrl):
        if self.jog_is_valid('a_plus', event):
            if STATUS.is_joint_mode():
                self.kb_jog(state, self.coordinates.index('a'), 1, shift)
            else:
                self.kb_jog(state, 3, 1, shift)

    def on_keycall_ANEG(self, event, state, shift, cntrl):
        if self.jog_is_valid('a_minus', event):
            if STATUS.is_joint_mode():
                self.kb_jog(state, self.coordinates.index('a'), -1, shift)
            else:
                self.kb_jog(state, 3, -1, shift)

    def on_keycall_BPOS(self, event, state, shift, cntrl):
        if self.jog_is_valid('b_plus', event):
            if STATUS.is_joint_mode():
                self.kb_jog(state, self.coordinates.index('b'), 1, shift)
            else:
                self.kb_jog(state, 4, 1, shift)

    def on_keycall_BNEG(self, event, state, shift, cntrl):
        if self.jog_is_valid('b_minus', event):
            if STATUS.is_joint_mode():
                self.kb_jog(state, self.coordinates.index('b'), -1, shift)
            else:
                self.kb_jog(state, 4, -1, shift)

    def on_keycall_PLUS(self, event, state, shift, cntrl):
        if self.key_is_valid(event, state) and not self.w.main_tab_widget.currentIndex() and self.jogSlow and self.w.jog_slider.isEnabled():
            return
        if self.key_is_valid(event, state) and not self.w.main_tab_widget.currentIndex():
            self.jogFast = True
        else:
            self.jogFast = False

    def on_keycall_MINUS(self, event, state, shift, cntrl):
        if self.key_is_valid(event, state) and not self.w.main_tab_widget.currentIndex() and self.jogFast and self.w.jog_slider.isEnabled():
            return
        if self.key_is_valid(event, state) and not self.w.main_tab_widget.currentIndex():
            self.jogSlow = True
        else:
            self.jogSlow = False

    def on_keycall_NUMBER(self, event, state, shift, cntrl, number):
        if self.key_is_valid(event, state) and not self.w.main_tab_widget.currentIndex():
            if shift and cntrl:
                pass
            elif shift and not cntrl:
                if number:
                    self.w.rapid_slider.setValue(10 * number)
                else:
                    self.w.rapid_slider.setValue(100)
            elif cntrl and not shift:
                if number:
                    self.w.feed_slider.setValue(10 * number)
                else:
                    self.w.feed_slider.setValue(100)
            else:
                if number and self.w.jog_slider.isEnabled():
                    if self.w.jog_slow.isChecked():
                        self.w.jog_slider.setValue(INFO.DEFAULT_LINEAR_JOG_VEL * 0.10 * number / self.slowJogFactor)
                    else:
                        self.w.jog_slider.setValue(INFO.DEFAULT_LINEAR_JOG_VEL * 0.10 * number)
                elif self.w.jog_slider.isEnabled():
                    if self.w.jog_slow.isChecked():
                        self.w.jog_slider.setValue(INFO.DEFAULT_LINEAR_JOG_VEL / self.slowJogFactor)
                    else:
                        self.w.jog_slider.setValue(INFO.DEFAULT_LINEAR_JOG_VEL)

    def on_keycall_END(self, event, state, shift, cntrl):
        if self.key_is_valid(event, state) and not self.w.main_tab_widget.currentIndex() and self.w.touch_xy.isEnabled():
            self.touch_xy_clicked()

    def on_keycall_DELETE(self, event, state, shift, cntrl):
        if self.keyboard_shortcuts() and not self.w.main_tab_widget.currentIndex() and self.w.laser.isVisible():
            if state and not event.isAutoRepeat():
                self.extLaserButton = True
                self.laser_pressed()
            else:
                self.extLaserButton = False
                self.laser_clicked()

    def on_keycall_ALTRETURN(self, event, state, shift, cntrl):
        if self.key_is_valid(event, state) and not cntrl and not shift and not self.w.main_tab_widget.currentIndex() and self.w.mdi_show.isEnabled():
            self.mdi_show_clicked()

    def on_keycall_RETURN(self, event, state, shift, cntrl):
        if self.key_is_valid(event, state) and not cntrl and not shift and not self.w.main_tab_widget.currentIndex() and self.w.gcode_stack.currentIndex() and self.w.mdi_show.isEnabled():
            self.mdi_show_clicked()

    def on_keycall_QUOTELEFT(self, event, state, shift, cntrl):
        if self.key_is_valid(event, state) and not self.w.main_tab_widget.currentIndex():
            if shift and cntrl:
                pass
            elif cntrl and not shift:
                pass
            elif shift and not cntrl:
                self.w.rapid_slider.setValue(0)
            else:
                self.w.jog_slider.setValue(0)

#########################################################################################################################
# required class boiler code #
#########################################################################################################################
    def __getitem__(self, item):
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)


#########################################################################################################################
# required handler boiler code #
#########################################################################################################################
def get_handlers(halcomp, widgets, paths):
    return [HandlerClass(halcomp, widgets, paths)]
