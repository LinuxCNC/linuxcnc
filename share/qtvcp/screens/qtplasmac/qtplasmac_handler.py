VERSION = '1.217.122'

'''
qtplasmac_handler.py

Copyright (C) 2020, 2021  Phillip A Carter
Copyright (C) 2020, 2021  Gregory D Carl

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

import os, sys
from shutil import copy as COPY
from shutil import move as MOVE
from subprocess import Popen, PIPE
from subprocess import call as CALL
from subprocess import check_output as CHKOP
import time
import tarfile
import math
import glob
import linuxcnc
import hal, hal_glib
from PyQt5 import QtCore, QtWidgets, QtGui
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *
from PyQt5.QtGui import *
from PyQt5.Qsci import QsciScintilla
from qtvcp import logger
from qtvcp.core import Status, Action, Info, Tool
from qtvcp.lib.gcodes import GCodes
from qtvcp.lib.keybindings import Keylookup
from qtvcp.widgets.file_manager import FileManager as FILE_MAN
from qtvcp.widgets.gcode_editor import GcodeEditor as EDITOR
from qtvcp.widgets.mdi_history import MDIHistory as MDI_HISTORY
from qtvcp.widgets.mdi_line import MDILine as MDI_LINE
from qtvcp.widgets.status_label import StatusLabel as STATLABEL
from qtvcp.widgets.stylesheeteditor import  StyleSheetEditor as SSE
from qtvcp.widgets.camview_widget import CamView as CAM
from qtvcp.widgets.simple_widgets import DoubleScale as DOUBLESCALE
from qtvcp.widgets.origin_offsetview import OriginOffsetView as OFFSETVIEW
from qtvcp.widgets.origin_offsetview import MyTableModel as OFFSET_TABLE
from qtvcp.lib.aux_program_loader import Aux_program_loader
from qtvcp.lib.qtplasmac import conv_settings as CONVSET
from qtvcp.lib.qtplasmac import conv_line as CONVLINE
from qtvcp.lib.qtplasmac import conv_circle as CONVCIRC
from qtvcp.lib.qtplasmac import conv_ellipse as CONVELLI
from qtvcp.lib.qtplasmac import conv_triangle as CONVTRIA
from qtvcp.lib.qtplasmac import conv_rectangle as CONVRECT
from qtvcp.lib.qtplasmac import conv_polygon as CONVPOLY
from qtvcp.lib.qtplasmac import conv_bolt as CONVBOLT
from qtvcp.lib.qtplasmac import conv_slot as CONVSLOT
from qtvcp.lib.qtplasmac import conv_star as CONVSTAR
from qtvcp.lib.qtplasmac import conv_gusset as CONVGUST
from qtvcp.lib.qtplasmac import conv_sector as CONVSECT
from qtvcp.lib.qtplasmac import conv_block as CONVBLCK
from qtvcp.lib.qtplasmac import tooltips as TOOLTIPS
from qtvcp.lib.qtplasmac import set_offsets as OFFSETS

# **** TEMP FOR CONVERSATIONAL TESTING 1 of 3 ****
# **** TEMP FOR OFFSET TESTING 1 of 2 ****
#from importlib import reload

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

# overlays some parameters on the preview screen
class OverlayMaterial(QLabel):
    def __init__(self, parent=None):
        super(OverlayMaterial, self).__init__(parent)
        self.setStyleSheet('font: 12pt "Courier";\
                            color: #cccccc;\
                            background: rgba(1,0,0,255)')
        self.setAlignment(Qt.AlignTop|Qt.AlignLeft)

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
        self.iniFile = linuxcnc.ini(INIPATH)
        self.foreColor = '#ffee06'
# changes sim common folder to a link so sims keep up to date
        if 'by_machine.qtplasmac' in self.PATHS.CONFIGPATH:
            if os.path.isdir(os.path.join(self.PATHS.CONFIGPATH, 'qtplasmac')):
                if '/usr' in self.PATHS.BASEDIR:
                    linkFolder = os.path.join(self.PATHS.BASEDIR, 'share/doc/linuxcnc/examples/sample-configs/by_machine/qtplasmac/qtplasmac/')
                else:
                    linkFolder = os.path.join(self.PATHS.BASEDIR, 'configs/by_machine/qtplasmac/qtplasmac/')
                os.rename(os.path.join(self.PATHS.CONFIGPATH, 'qtplasmac'), os.path.join(self.PATHS.CONFIGPATH, 'qtplasmac' + str(time.time())))
                os.symlink(linkFolder, os.path.join(self.PATHS.CONFIGPATH, 'qtplasmac'))
        self.STYLEEDITOR = SSE(widgets, paths)
        self.GCODES = GCodes(widgets)
        self.valid = QDoubleValidator(0.0, 999.999, 3)
        self.IMAGES = os.path.join(self.PATHS.IMAGEDIR, 'qtplasmac/images/')
        self.w.setWindowIcon(QIcon(os.path.join(self.IMAGES, 'linuxcncicon.png')))
        self.landscape = True
        if os.path.basename(self.PATHS.XML) == 'qtplasmac_9x16.ui':
            self.landscape = False
        self.gui43 = False
        if os.path.basename(self.PATHS.XML) == 'qtplasmac_4x3.ui':
            self.gui43 = True
        self.widgetsLoaded = 0
        KEYBIND.add_call('Key_F12','on_keycall_F12')
        KEYBIND.add_call('Key_F9','on_keycall_F9')
        KEYBIND.add_call('Key_Plus', 'on_keycall_PLUS')
        KEYBIND.add_call('Key_Minus', 'on_keycall_MINUS')
        KEYBIND.add_call('Key_H', 'on_keycall_HOME')
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
        self.axisList = [x.lower() for x in INFO.AVAILABLE_AXES]
        self.systemList = ['G53','G54','G55','G56','G57','G58','G59','G59.1','G59.2','G59.3']
        self.slowJogFactor = 10
        self.jogFast = False
        self.jogSlow = False
        self.lastLoadedProgram = 'None'
        self.estopOnList = []
        self.idleList = ['file_open', 'file_reload', 'file_edit']
        self.idleOnList = ['home_x', 'home_y', 'home_z', 'home_a', 'home_all']
        self.idleHomedList = ['touch_x', 'touch_y', 'touch_z', 'touch_a', 'touch_b', 'touch_xy', \
                              'mdi_show', 'height_lower', 'height_raise', 'wcs_button']
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
        self.thcFeedRate = float(self.iniFile.find('AXIS_Z', 'MAX_VELOCITY')) * \
                           float(self.iniFile.find('AXIS_Z', 'OFFSET_AV_RATIO')) * 60
        self.maxHeight = self.zMax - self.zMin
        self.unitsPerMm = 1
        self.units = self.iniFile.find('TRAJ', 'LINEAR_UNITS')
        if self.units == 'inch':
            self.unitsPerMm = 0.03937
        self.maxPidP = self.thcFeedRate / self.unitsPerMm * 0.1
        self.mode = int(self.iniFile.find('QTPLASMAC', 'MODE')) or 0
        self.tmpPath = '/tmp/qtplasmac/'
        if not os.path.isdir(self.tmpPath):
            os.mkdir(self.tmpPath)
        self.machineName = self.iniFile.find('EMC', 'MACHINE')
        self.materialFile = '{}_material.cfg'.format(self.machineName)
        self.tmpMaterialFile = '{}{}'.format(self.tmpPath, self.materialFile.replace('.cfg','.tmp'))
        self.tmpMaterialFileGCode = '{}{}'.format(self.tmpPath, self.materialFile.replace('.cfg','.gcode'))
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
        self.oldConvButton = ''
        self.programPrefix = self.iniFile.find('DISPLAY', 'PROGRAM_PREFIX') or os.environ['LINUXCNC_NCFILES_DIR']
        self.dialogError = False
        self.cutTypeText = ''
        self.heightOvr = 0.0
        self.heightOvrScale = 0.1
        self.old_ovr_counts = 0
        self.startLine = 0
        self.preRflFile = ''
        self.rflActive = False
        self.jogInhibit = ''
        self.isJogging = {0:False, 1:False, 2:False, 3:False}
        self.ccButton, self.otButton, self.ptButton, self.tpButton = '', '', '', ''
        self.ctButton, self.scButton, self.frButton, self.mcButton = '', '', '', ''
        self.ovButton, self.llButton = '', ''
        self.halTogglePins = {}
        self.halPulsePins = {}
        self.torchOn = False
        self.progRun = False
        self.rapidOn = False
        self.probeOn = False
        self.gcodeProps = ''
        self.framing = False
        self.boundsError = {'loaded': False, 'framing': False}
        self.obLayout = ''
        self.notifyColor = 'normal'
        self.firstHoming = False
        self.droScale = 1
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
        self.CUTTING        = 11
        self.PAUSE_AT_END   = 12
        self.SAFE_HEIGHT    = 13
        self.MAX_HEIGHT     = 14
        self.FINISH         = 15
        self.TORCH_PULSE    = 16
        self.PAUSED_MOTION  = 17
        self.OHMIC_TEST     = 18
        self.PROBE_TEST     = 19
        self.SCRIBING       = 20
        self.CONS_CHNG_ON   = 21
        self.CONS_CHNG_OFF  = 22
        self.CUT_REC_ON     = 23
        self.CUT_REC_OFF    = 24
        self.DEBUG          = 25

    def initialized__(self):
        # ensure we get all startup errors
        STATUS.connect('error', self.error_update)
        STATUS.connect('graphics-gcode-error', lambda o, e:self.error_update(o, linuxcnc.OPERATOR_ERROR, e))
        self.make_hal_pins()
        self.init_preferences()
        self.hide_widgets()
        self.init_widgets()
        self.w.button_frame.installEventFilter(self.w)
        self.link_hal_pins()
        self.set_signal_connections()
        self.statistics_init()
        self.set_axes_and_joints()
        self.set_spinbox_parameters()
        self.load_plasma_parameters()
        self.set_mode()
        self.user_button_setup()
        self.set_buttons_state([self.estopOnList], True)
        self.check_material_file()
        self.load_materials()
        self.pmx485_check()
#        if self.firstRun is True:
#            self.firstRun = False
        self.offset_peripherals()
        self.set_probe_offset_pins()
        self.wcs_rotation('get')
        self.widgetsLoaded = 1
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
        STATUS.connect('graphics-loading-progress', self.percent_loaded)
        STATUS.connect('interp-paused', self.interp_paused)
        STATUS.connect('interp-idle', self.interp_idle)
        STATUS.connect('interp-reading', self.interp_reading)
        STATUS.connect('interp-waiting', self.interp_waiting)
        STATUS.connect('interp-run', self.interp_running)
        STATUS.connect('jograte-changed', self.jog_rate_changed)
        STATUS.connect('graphics-gcode-properties', lambda w, d:self.update_gcode_properties(d))
        STATUS.connect('system_notify_button_pressed', self.system_notify_button_pressed)
        STATUS.connect('tool-in-spindle-changed', self.tool_changed)
        self.overlay.setText(self.get_overlay_text(False))
        self.overlayConv.setText(self.get_overlay_text(True))
        if not self.w.chk_overlay.isChecked():
            self.overlay.hide()
            self.overlayConv.hide()
        self.w.setWindowTitle('{} - QtPlasmaC v{}, powered by QtVCP on LinuxCNC v{}'.format(self.machineName, VERSION, linuxcnc.version.split(':')[0]))
        self.startupTimer = QTimer()
        self.startupTimer.timeout.connect(self.startup_timeout)
        self.startupTimer.setSingleShot(True)
        self.set_color_styles()
        self.autorepeat_keys(False)
        # only set hal pins after initialized__ has begun
        # some locales won't set pins before this phase
        self.thcFeedRatePin.set(self.thcFeedRate)
        self.startupTimer.start(250)
        if self.firstRun is True:
            self.firstRun = False


#########################################################################################################################
# CLASS PATCHING SECTION #
#########################################################################################################################
    def class_patch__(self):
        self.gcode_editor_patch()
        self.camview_patch()
        self.mdi_line_patch()
        self.offset_table_patch()

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

    # save a non gcode file and don't load it into linuxcnc
    def new_saveReturn(self, filename):
        saved = ACTION.SAVE_PROGRAM(self.w.gcode_editor.editor.text(), filename)
        if saved is not None:
            self.w.gcode_editor.editor.setModified(False)
            if saved[-3:] in ['ngc', '.nc', '.tap']:
                ACTION.OPEN_PROGRAM(saved)

    # open a non gcode file and don't load it into linuxcnc
    def new_openReturn(self, filename):
        if filename[-3:] in ['ngc', '.nc', '.tap']:
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
            if not self.dialog_show_yesno(QMessageBox.Question, head, '{}\n\n{}?\n'.format(msg0, msg1)):
                return
        if self.fileOpened == True:
            self.file_reload_clicked()
        else:
            self.w.gcode_editor.editor.new_text()
            self.w.gcode_editor.editor.setModified(False)
        self.w.gcode_editor.editMode()
        self.w.preview_stack.setCurrentIndex(0)
        self.vkb_hide()
        if self.w.chk_overlay.isChecked():
            self.overlay.show()
        ACTION.SET_MANUAL_MODE()

    # we don't use lexer colors
    def new_gcodeLexerCall(self):
        pass

    # we don't use lexer colors
    def new_pythonLexerCall(self):
        pass

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

# patched mdi_line functions
    def mdi_line_patch(self):
        self.old_submit = MDI_LINE.submit
        MDI_LINE.submit = self.new_submit

    # don't allow M3 or M5 in MDI codes
    def new_submit(self):
        intext = str(self.w.mdihistory.MDILine.text()).rstrip()
        if intext == '': return
        if intext.upper() == 'HALMETER':
            AUX_PRGM.load_halmeter()
        elif intext.upper() == 'STATUS':
            AUX_PRGM.load_status()
        elif intext.upper() == 'HALSHOW':
            AUX_PRGM.load_halshow()
        elif intext.upper() == 'CLASSICLADDER':
            AUX_PRGM.load_ladder()
        elif intext.upper() == 'HALSCOPE':
            AUX_PRGM.load_halscope()
        elif intext.upper() == 'CALIBRATION':
            AUX_PRGM.load_calibration()
        elif intext.upper() == 'PREFERENCE':
            self.new_openReturn(os.path.join(self.PATHS.CONFIGPATH, 'qtplasmac.prefs'))
            self.w.gcode_editor.readOnlyMode()
            self.w.preview_stack.setCurrentIndex(2)
        else:
            head = _translate('HandlerClass', 'MDI ERROR')
            if 'm3' in intext.lower().replace(' ',''):
                msg0 = _translate('HandlerClass', 'M3 commands are not allowed in MDI mode')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}'.format(head, msg0))
                return
            elif 'm5' in intext.lower().replace(' ',''):
                msg0 = _translate('HandlerClass', 'M5 commands are not allowed in MDI mode')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}'.format(head, msg0))
                return
            ACTION.CALL_MDI(intext + '\n')
            try:
                fp = os.path.expanduser(INFO.MDI_HISTORY_PATH)
                fp = open(fp, 'a')
                fp.write(intext + '\n')
                fp.close()
            except:
                pass
            STATUS.emit('mdi-history-changed')

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


#########################################################################################################################
# SPECIAL FUNCTIONS SECTION #
#########################################################################################################################
    def make_hal_pins(self):
        self.colorBgPin = self.h.newpin('color_bg', hal.HAL_S32, hal.HAL_OUT)
        self.colorFgPin = self.h.newpin('color_fg', hal.HAL_S32, hal.HAL_OUT)
        self.cutTypePin = self.h.newpin('cut_type', hal.HAL_S32, hal.HAL_IN)
        self.heightOverridePin = self.h.newpin('height_override', hal.HAL_FLOAT, hal.HAL_OUT)
        self.laserOnPin = self.h.newpin('laser_on', hal.HAL_BIT, hal.HAL_OUT)
        self.materialChangePin = self.h.newpin('material_change', hal.HAL_S32, hal.HAL_IN)
        self.materialChangeNumberPin = self.h.newpin('material_change_number', hal.HAL_S32, hal.HAL_IN)
        self.materialChangeTimeoutPin = self.h.newpin('material_change_timeout', hal.HAL_BIT, hal.HAL_IN)
        self.materialReloadPin = self.h.newpin('material_reload', hal.HAL_BIT, hal.HAL_IN)
        self.materialTempPin = self.h.newpin('material_temp', hal.HAL_S32, hal.HAL_IN)
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
        self.xOffsetPin = self.h.newpin('x_offset', hal.HAL_FLOAT, hal.HAL_IN)
        self.yOffsetPin = self.h.newpin('y_offset', hal.HAL_FLOAT, hal.HAL_IN)
        self.offsetsActivePin = self.h.newpin('offsets_active', hal.HAL_BIT, hal.HAL_IN)
        self.zHeightPin = self.h.newpin('z_height', hal.HAL_FLOAT, hal.HAL_IN)
        self.plasmacStatePin = self.h.newpin('plasmac_state', hal.HAL_S32, hal.HAL_IN)
        self.jogInhibitPin = self.h.newpin('jog_inhibit', hal.HAL_BIT, hal.HAL_OUT)
        self.paramTabDisable = self.h.newpin('param_disable', hal.HAL_BIT, hal.HAL_IN)
        self.convTabDisable = self.h.newpin('conv_disable', hal.HAL_BIT, hal.HAL_IN)
        self.consChangePin = self.h.newpin('consumable_changing', hal.HAL_BIT, hal.HAL_IN)
        self.cutLengthPin = self.h.newpin('cut_length', hal.HAL_FLOAT, hal.HAL_IN)
        self.cutTimePin = self.h.newpin('cut_time', hal.HAL_FLOAT, hal.HAL_IN)
        self.pierceCountPin = self.h.newpin('pierce_count', hal.HAL_S32, hal.HAL_IN)
        self.motionTypePin = self.h.newpin('motion_type', hal.HAL_S32, hal.HAL_IN)
        self.torchOnPin = self.h.newpin('torch_on', hal.HAL_BIT, hal.HAL_IN)
        self.extPowerPin = self.h.newpin('ext_power', hal.HAL_BIT, hal.HAL_IN)
        self.extRunPin = self.h.newpin('ext_run', hal.HAL_BIT, hal.HAL_IN)
        self.extPausePin = self.h.newpin('ext_pause', hal.HAL_BIT, hal.HAL_IN)
        self.extAbortPin = self.h.newpin('ext_abort', hal.HAL_BIT, hal.HAL_IN)
        self.extTouchOffPin = self.h.newpin('ext_touchoff', hal.HAL_BIT, hal.HAL_IN)
        self.extRunPausePin = self.h.newpin('ext_run_pause', hal.HAL_BIT, hal.HAL_IN)
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
        self.extCutReccancelPin = self.h.newpin('ext_cutrec_cancel', hal.HAL_BIT, hal.HAL_IN)
        self.extTorchEnablePin = self.h.newpin('ext_torch_enable', hal.HAL_BIT, hal.HAL_IN)
        self.extThcEnablePin = self.h.newpin('ext_thc_enable', hal.HAL_BIT, hal.HAL_IN)
        self.extCornerLockEnablePin = self.h.newpin('ext_cornerlock_enable', hal.HAL_BIT, hal.HAL_IN)
        self.extKerfCrossEnablePin = self.h.newpin('ext_kerfcross_enable', hal.HAL_BIT, hal.HAL_IN)
        self.extIgnoreArcOkPin = self.h.newpin('ext_ignore_arc_ok', hal.HAL_BIT, hal.HAL_IN)
        self.extMeshModePin = self.h.newpin('ext_mesh_mode', hal.HAL_BIT, hal.HAL_IN)
        self.extOhmicProbeEnablePin = self.h.newpin('ext_ohmic_probe_enable', hal.HAL_BIT, hal.HAL_IN)
        self.extAutoVoltsEnablePin = self.h.newpin('ext_auto_volts_enable', hal.HAL_BIT, hal.HAL_IN)
        self.extHeightOvrPlusPin = self.h.newpin('ext_height_ovr_plus', hal.HAL_BIT, hal.HAL_IN)
        self.extHeightOvrMinusPin = self.h.newpin('ext_height_ovr_minus', hal.HAL_BIT, hal.HAL_IN)
        self.extHeightOvrResetPin = self.h.newpin('ext_height_ovr_reset', hal.HAL_BIT, hal.HAL_IN)
        self.extHeightOvrCountsPin = self.h.newpin('ext_height_ovr_counts', hal.HAL_S32, hal.HAL_IN)
        self.extHeightOvrScalePin = self.h.newpin('ext_height_ovr_scale', hal.HAL_FLOAT, hal.HAL_IN)
        self.extHeightOvrCountEnablePin = self.h.newpin('ext_height_ovr_count_enable', hal.HAL_BIT, hal.HAL_IN)
        self.extJogSlowPin = self.h.newpin('ext_jog_slow', hal.HAL_BIT, hal.HAL_IN)
        self.probeTestErrorPin = self.h.newpin('probe_test_error', hal.HAL_BIT, hal.HAL_IN)
        self.out0Pin = self.h.newpin('ext_out_0', hal.HAL_BIT, hal.HAL_OUT)
        self.out1Pin = self.h.newpin('ext_out_1', hal.HAL_BIT, hal.HAL_OUT)
        self.out2Pin = self.h.newpin('ext_out_2', hal.HAL_BIT, hal.HAL_OUT)
        self.thcFeedRatePin = self.h.newpin('thc_feed_rate', hal.HAL_FLOAT, hal.HAL_OUT)

    def link_hal_pins(self):
        CALL(['halcmd', 'net', 'plasmac:state', 'plasmac.state-out', 'qtplasmac.plasmac_state'])
        CALL(['halcmd', 'net', 'plasmac:z-height', 'plasmac.z-height', 'qtplasmac.z_height'])
        CALL(['halcmd', 'net', 'plasmac:consumable-changing', 'plasmac.consumable-changing', 'qtplasmac.consumable_changing'])
        CALL(['halcmd', 'net', 'plasmac:laser-on', 'qtplasmac.laser_on'])
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
        CALL(['halcmd', 'net', 'plasmac:thc-delay', 'qtplasmac.thc_delay-f', 'plasmac.thc-delay'])
        CALL(['halcmd', 'net', 'plasmac:thc-threshold', 'qtplasmac.thc_threshold-f', 'plasmac.thc-threshold'])
        CALL(['halcmd', 'net', 'plasmac:pid-p-gain', 'qtplasmac.pid_p_gain-f', 'plasmac.pid-p-gain'])
        CALL(['halcmd', 'net', 'plasmac:cornerlock-threshold', 'qtplasmac.cornerlock_threshold-f', 'plasmac.cornerlock-threshold'])
        CALL(['halcmd', 'net', 'plasmac:kerfcross-override', 'qtplasmac.kerfcross_override-f', 'plasmac.kerfcross-override'])
        CALL(['halcmd', 'net', 'plasmac:pid-i-gain', 'qtplasmac.pid_i_gain-f', 'plasmac.pid-i-gain'])
        CALL(['halcmd', 'net', 'plasmac:pid-d-gain', 'qtplasmac.pid_d_gain-f', 'plasmac.pid-d-gain'])
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
        CALL(['halcmd', 'net', 'plasmac:kerfcross-is-locked', 'plasmac.kerfcross-is-locked', 'qtplasmac.led_kerf_lock'])
        CALL(['halcmd', 'net', 'plasmac:led-up', 'plasmac.led-up', 'qtplasmac.led_thc_up'])
        CALL(['halcmd', 'net', 'plasmac:led-down', 'plasmac.led-down', 'qtplasmac.led_thc_down'])
        CALL(['halcmd', 'net', 'plasmac:ohmic-probe-out', 'qtplasmac.led_ohmic_probe'])
        CALL(['halcmd', 'net', 'plasmac:thc-active', 'plasmac.thc-active', 'qtplasmac.led_thc_active'])
        CALL(['halcmd', 'net', 'plasmac:thc-enabled', 'plasmac.thc-enabled', 'qtplasmac.led_thc_enabled'])
        CALL(['halcmd', 'net', 'plasmac:torch-on', 'qtplasmac.led_torch_on'])
        #control
        CALL(['halcmd', 'net', 'plasmac:cornerlock-enable', 'qtplasmac.cornerlock_enable', 'plasmac.cornerlock-enable'])
        CALL(['halcmd', 'net', 'plasmac:kerfcross-enable', 'qtplasmac.kerfcross_enable', 'plasmac.kerfcross-enable'])
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
        #ini
        CALL(['halcmd', 'net', 'plasmac:axis-x-max-limit', 'ini.x.max_limit', 'plasmac.axis-x-max-limit'])
        CALL(['halcmd', 'net', 'plasmac:axis-x-min-limit', 'ini.x.min_limit', 'plasmac.axis-x-min-limit'])
        CALL(['halcmd', 'net', 'plasmac:axis-y-max-limit', 'ini.y.max_limit', 'plasmac.axis-y-max-limit'])
        CALL(['halcmd', 'net', 'plasmac:axis-y-min-limit', 'ini.y.min_limit', 'plasmac.axis-y-min-limit'])
        CALL(['halcmd', 'net', 'plasmac:axis-z-max-limit', 'ini.z.max_limit', 'plasmac.axis-z-max-limit'])
        CALL(['halcmd', 'net', 'plasmac:axis-z-min-limit', 'ini.z.min_limit', 'plasmac.axis-z-min-limit'])
        # statistics
        CALL(['halcmd', 'net', 'plasmac:cut-length', 'plasmac.cut-length', 'qtplasmac.cut_length'])
        CALL(['halcmd', 'net', 'plasmac:cut-time', 'plasmac.cut-time', 'qtplasmac.cut_time'])
        CALL(['halcmd', 'net', 'plasmac:pierce-count', 'plasmac.pierce-count', 'qtplasmac.pierce_count'])
        CALL(['halcmd', 'net', 'plasmac:motion-type', 'qtplasmac.motion_type'])
        CALL(['halcmd', 'net', 'plasmac:torch-on', 'qtplasmac.torch_on'])
        # misc
        CALL(['halcmd', 'net', 'plasmac:probe-test-error', 'plasmac.probe-test-error', 'qtplasmac.probe_test_error'])

    def init_preferences(self):
        self.lastLoadedProgram = self.w.PREFS_.getpref('RecentPath_0', 'None', str,'BOOK_KEEPING')
        self.w.chk_keyboard_shortcuts.setChecked(self.w.PREFS_.getpref('Use keyboard shortcuts', False, bool, 'GUI_OPTIONS'))
        self.w.chk_soft_keyboard.setChecked(self.w.PREFS_.getpref('Use soft keyboard', False, bool, 'GUI_OPTIONS'))
        self.w.chk_overlay.setChecked(self.w.PREFS_.getpref('Show materials', True, bool, 'GUI_OPTIONS'))
        self.w.chk_run_from_line.setChecked(self.w.PREFS_.getpref('Run from line', False, bool, 'GUI_OPTIONS'))
        self.w.chk_tool_tips.setChecked(self.w.PREFS_.getpref('Tool tips', True, bool, 'GUI_OPTIONS'))
        self.w.cone_size.setValue(self.w.PREFS_.getpref('Preview cone size', 0.5, float, 'GUI_OPTIONS'))
        self.w.grid_size.setValue(self.w.PREFS_.getpref('Preview grid size', 0, float, 'GUI_OPTIONS'))
        self.w.color_foregrnd.setStyleSheet('background-color: {}'.format(self.w.PREFS_.getpref('Foreground', '#ffee06', str, 'COLOR_OPTIONS')))
        self.w.color_foregalt.setStyleSheet('background-color: {}'.format(self.w.PREFS_.getpref('Highlight', '#ffee06', str, 'COLOR_OPTIONS')))
        self.w.color_led.setStyleSheet('background-color: {}'.format(self.w.PREFS_.getpref('LED', '#ffee06', str, 'COLOR_OPTIONS')))
        self.w.color_backgrnd.setStyleSheet('background-color: {}'.format(self.w.PREFS_.getpref('Background', '#16160e', str, 'COLOR_OPTIONS')))
        self.w.color_backgalt.setStyleSheet('background-color: {}'.format(self.w.PREFS_.getpref('Background Alt', '#36362e', str, 'COLOR_OPTIONS')))
        self.w.color_frams.setStyleSheet('background-color: {}'.format(self.w.PREFS_.getpref('Frames', '#ffee06', str, 'COLOR_OPTIONS')))
        self.w.color_estop.setStyleSheet('background-color: {}'.format(self.w.PREFS_.getpref('Estop', '#ff0000', str, 'COLOR_OPTIONS')))
        self.w.color_disabled.setStyleSheet('background-color: {}'.format(self.w.PREFS_.getpref('Disabled', '#b0b0b0', str, 'COLOR_OPTIONS')))
        self.w.color_preview.setStyleSheet('background-color: {}'.format(self.w.PREFS_.getpref('Preview', '#000000', str, 'COLOR_OPTIONS')))
        TOOLTIPS.tool_tips_changed(self.w)
        self.soft_keyboard()
        self.cone_size_changed(self.w.cone_size.value())
        self.grid_size_changed(self.w.grid_size.value())
        self.set_basic_colors()

    def hide_widgets(self):
        if not self.gui43:
            self.w.main_tab_widget.removeTab(3)
        for b in ['RUN', 'PAUSE', 'ABORT']:
            if int(self.iniFile.find('QTPLASMAC', 'HIDE_{}'.format(b)) or 0):
                self.w[b.lower()].hide()
                if self.landscape:
                    self.w.machine_frame.setMaximumHeight(self.w.machine_frame.maximumHeight() - 44)
                    self.w.machine_frame.setMinimumHeight(self.w.machine_frame.maximumHeight())

    def init_widgets(self):
        droPos = self.iniFile.find('QTPLASMAC', 'DRO_POSITION') or 'None'
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
        self.w.gcode_progress.hide()
        self.w.jog_slider.setMaximum(INFO.MAX_LINEAR_JOG_VEL)
        self.w.jog_slider.setValue(INFO.DEFAULT_LINEAR_JOG_VEL)
        self.w.chk_override_limits.setChecked(False)
        self.w.chk_override_limits.setEnabled(False)
        self.w.thc_enable.setChecked(self.w.PREFS_.getpref('THC enable', True, bool, 'ENABLE_OPTIONS'))
        self.w.cornerlock_enable.setChecked(self.w.PREFS_.getpref('Corner lock enable', True, bool, 'ENABLE_OPTIONS'))
        self.w.kerfcross_enable.setChecked(self.w.PREFS_.getpref('Kerf cross enable', True, bool, 'ENABLE_OPTIONS'))
        self.w.use_auto_volts.setChecked(self.w.PREFS_.getpref('Use auto volts', True, bool, 'ENABLE_OPTIONS'))
        self.w.ohmic_probe_enable.setChecked(self.w.PREFS_.getpref('Ohmic probe enable', False, bool, 'ENABLE_OPTIONS'))
        self.w.error_label = QLabel()
        self.w.lbl_tool = STATLABEL()
        self.w.lbl_gcodes = STATLABEL()
        self.w.lbl_mcodes = STATLABEL()
        self.w.statusbar.addPermanentWidget(self.w.error_label, stretch=1)
        self.w.statusbar.addPermanentWidget(VLine())
        self.w.statusbar.addPermanentWidget(self.w.lbl_tool)
        self.w.statusbar.addPermanentWidget(VLine())
        self.w.statusbar.addPermanentWidget(self.w.lbl_gcodes)
        self.w.statusbar.addPermanentWidget(VLine())
        self.w.statusbar.addPermanentWidget(self.w.lbl_mcodes)
        text = _translate('HandlerClass', 'MOVE')
        self.w.cut_rec_move_label.setText('{}\n{}'.format(text, self.w.kerf_width.text()))
        self.w.filemanager.button2.setText(_translate('HandlerClass', 'USER'))
        self.w.filemanager.button3.setText(_translate('HandlerClass', 'ADD JUMP'))
        # for copy/paste control if required
        #self.w.filemanager.copyButton.setText(_translate('HandlerClass', 'COPY'))
        #self.w.filemanager.pasteButton.setText(_translate('HandlerClass', 'PASTE'))
        #self.w.filemanager.showCopyControls(True)
        self.w.gcode_display.set_margin_width(3)
        self.w.gcode_display.setBraceMatching(False)
        self.w.gcode_display.setCaretWidth(0)
        self.w.gcode_editor.set_margin_width(3)
        self.w.gcode_editor.editor.setBraceMatching(False)
        self.w.gcode_editor.editor.setCaretWidth(4)
        self.w.gcode_editor.editMode()
        self.w.gcode_editor.pythonLexerAction.setVisible(False)
        self.w.gcode_editor.gCodeLexerAction.setVisible(False)
        self.w.gcode_editor.label.setText('')
        self.w.gcodegraphics.set_alpha_mode(True)
        self.w.conv_preview.set_cone_basesize(0.1)
        self.w.conv_preview.set_view('Z')
        self.w.conv_preview.set_alpha_mode(True)
        self.w.conv_preview.setShowOffsets(False)
        self.w.conv_preview.setdro(False)
        self.w.conv_preview.inhibit_selection = True
        self.w.conv_preview.updateGL()
        self.w.estopButton = int(self.iniFile.find('QTPLASMAC', 'ESTOP_TYPE') or 0)
        if self.w.estopButton == 0:
            self.w.estop.setEnabled(False)
        if self.w.estopButton == 1:
            self.w.estop.hide()
        self.w.camview.cross_color = QtCore.Qt.red
        self.w.camview.cross_pointer_color = QtCore.Qt.red
        self.w.camview.font = QFont('arial,helvetica', 16)
        self.overlay = OverlayMaterial(self.w.gcodegraphics)
        self.overlayConv = OverlayMaterial(self.w.conv_preview)
        self.flasher = QTimer()
        self.flasher.timeout.connect(self.flasher_timeout)
        self.flasher.start(500)
        self.manualCut = False
        self.jogPreManCut = [False, INFO.DEFAULT_LINEAR_JOG_VEL, 0]
        self.probeTest = False
        self.torchPulse = False
        self.rflSelected = False
        self.fileOpened = False
        self.laserButtonState = 'laser'
        self.camButtonState = 'markedge'

    def get_overlay_text(self, kerf):
        if '.' in self.w.cut_feed_rate.text() and len(self.w.cut_feed_rate.text().split('.')[0]) > 3:
            text  = ('FR: {}\n'.format(self.w.cut_feed_rate.text().split('.')[0]))
        else:
            text  = ('FR: {}\n'.format(self.w.cut_feed_rate.text()))
        text += ('PH: {}\n'.format(self.w.pierce_height.text()))
        text += ('PD: {}\n'.format(self.w.pierce_delay.text()))
        text += ('CH: {}'.format(self.w.cut_height.text()))
        if kerf == True:
            text += ('\nKW: {}'.format(self.w.kerf_width.text()))
        if self.pmx485Exists:
            text += ('\nCA: {}'.format(self.w.cut_amps.text()))
        return text

    def offset_peripherals(self):
        self.camOffsetX = 0.0
        self.camOffsetY = 0.0
        self.laserOffsetX = 0.0
        self.laserOffsetY = 0.0
        self.probeOffsetX = 0.0
        self.probeOffsetY = 0.0
        self.probeDelay = 0.0
        head = _translate('HandlerClass', 'INI FILE ERROR')
        inCode = self.iniFile.find('QTPLASMAC', 'LASER_TOUCHOFF') or '0'
        msg0 = _translate('HandlerClass', 'Invalid entry for laser offset')
        if inCode == '0':
            self.w.laser.hide()
        else:
            try:
                parms = inCode.lower().split()
                for parm in parms:
                    if parm.startswith('x'):
                        self.laserOffsetX = float(parms[0].replace('x', ''))
                    elif parm.startswith('y'):
                        self.laserOffsetY = float(parms[1].replace('y', ''))
            except:
                self.w.laser.hide()
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}'.format(head, msg0))
            if self.laserOffsetX or self.laserOffsetY:
                self.idleHomedList.append('laser')
                self.w.laser.setEnabled(False)
            else:
                self.w.laser.hide()
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}'.format(head, msg0))
        inCode = self.iniFile.find('QTPLASMAC', 'CAMERA_TOUCHOFF') or '0'
        msg0 = _translate('HandlerClass', 'Invalid entry for camera offset')
        if inCode == '0':
            self.w.camera.hide()
        else:
            try:
                parms = inCode.lower().split()
                for parm in parms:
                    if parm.startswith('x'):
                        self.camOffsetX = float(parms[0].replace('x', ''))
                    elif parm.startswith('y'):
                        self.camOffsetY = float(parms[1].replace('y', ''))
            except:
                self.w.camera.hide()
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}'.format(head, msg0))
            if self.camOffsetX or self.camOffsetY:
                self.idleHomedList.append('camera')
                self.w.camera.setEnabled(False)
            else:
                self.w.camera.hide()
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}'.format(head, msg0))
        inCode = self.iniFile.find('QTPLASMAC', 'OFFSET_PROBING') or '0'
        msg0 = _translate('HandlerClass', 'Invalid entry for probe offset')
        if inCode != '0':
            try:
                parms = inCode.lower().split()
                if len(parms) > 3:
                    raise Exception()
                for parm in parms:
                    if parm.startswith('x'):
                        self.probeOffsetX = float(parm.replace('x', ''))
                    elif parm.startswith('y'):
                        self.probeOffsetY = float(parm.replace('y', ''))
                    else:
                        self.probeDelay = float(parm)
            except:
                self.w.camera.hide()
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}'.format(head, msg0))

    def closing_cleanup__(self):
        # disconnect powermax
        self.w.pmx485_enable.setChecked(False)
        # close soft keyboard
        self.vkb_hide()
        # save preferences
        if not self.w.PREFS_: return
        self.w.PREFS_.putpref('Use keyboard shortcuts', self.w.chk_keyboard_shortcuts.isChecked(), bool, 'GUI_OPTIONS')
        self.w.PREFS_.putpref('Use soft keyboard', self.w.chk_soft_keyboard.isChecked(), bool, 'GUI_OPTIONS')
        self.w.PREFS_.putpref('Show materials', self.w.chk_overlay.isChecked(), bool, 'GUI_OPTIONS')
        self.w.PREFS_.putpref('Run from line', self.w.chk_run_from_line.isChecked(), bool, 'GUI_OPTIONS')
        self.w.PREFS_.putpref('Tool tips', self.w.chk_tool_tips.isChecked(), bool, 'GUI_OPTIONS')
        self.w.PREFS_.putpref('Preview cone size', self.w.cone_size.value(), float, 'GUI_OPTIONS')
        self.w.PREFS_.putpref('Preview grid size', self.w.grid_size.value(), float, 'GUI_OPTIONS')
        self.w.PREFS_.putpref('THC enable', self.w.thc_enable.isChecked(), bool, 'ENABLE_OPTIONS')
        self.w.PREFS_.putpref('Corner lock enable', self.w.cornerlock_enable.isChecked(), bool, 'ENABLE_OPTIONS')
        self.w.PREFS_.putpref('Kerf cross enable', self.w.kerfcross_enable.isChecked(), bool, 'ENABLE_OPTIONS')
        self.w.PREFS_.putpref('Use auto volts', self.w.use_auto_volts.isChecked(), bool, 'ENABLE_OPTIONS')
        self.w.PREFS_.putpref('Ohmic probe enable', self.w.ohmic_probe_enable.isChecked(), bool, 'ENABLE_OPTIONS')
        self.autorepeat_keys(True)
        self.save_logfile(5)

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

    def processed_key_event__(self,receiver,event,is_pressed,key,code,shift,cntrl):
        # when typing in MDI, we don't want keybinding to call functions
        # so we catch and process the events directly.
        # We do want ESC, F1 and F2 to call keybinding functions though
        if code not in(Qt.Key_Escape, Qt.Key_F1, Qt.Key_F2):
            # search for the top widget of whatever widget received the event
            # then check if it's one we want the keypress events to go to
            flag = False
            conversational = False
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
                    flag = True
                    break
                if isinstance(receiver2, MDI_HISTORY):
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
                receiver2 = receiver2.parent()
            if flag:
                if is_pressed:
                    if conversational and (code == Qt.Key_Tab or code == Qt.Key_BackTab):
                        self.keyPressEvent(event)
                    else:
                        receiver.keyPressEvent(event)
                    event.accept()
                    return True
                else:
                    event.accept()
                    return True
        if event.isAutoRepeat():
            return True
        return KEYBIND.manage_function_calls(self, event, is_pressed, key, shift, cntrl)

    def size_changed(self, object):
        rows = int((object.height() - 24) / 44)
        if self.landscape:
            if rows == 5:
                self.buttons_hide(9, 20)
            elif rows == 6:
                self.buttons_hide(11, 20)
                self.buttons_show(9, 10)
            elif rows == 7:
                self.buttons_hide(13, 20)
                self.buttons_show(9, 12)
            elif rows == 8:
                self.buttons_hide(15, 20)
                self.buttons_show(9, 14)
            elif rows == 9:
                self.buttons_hide(17, 20)
                self.buttons_show(9, 16)
            elif rows == 10:
                self.buttons_hide(19, 20)
                self.buttons_show(9, 18)
            else:
                self.buttons_show(9, 20)
        else:
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
                self.buttons_hide(12, 20)
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
    def power_state(self, state):
        if state:
            self.set_buttons_state([self.idleOnList], True)
            if self.tpButton and not self.w.torch_enable.isChecked():
                self.w[self.tpButton].setEnabled(False)
            if self.otButton and not self.w.ohmic_probe_enable.isChecked():
                self.w[self.otButton].setEnabled(False)
            if STATUS.is_all_homed():
                self.set_buttons_state([self.idleHomedList], True)
                self.wcs_rotation('set')
            else:
                self.set_buttons_state([self.idleHomedList], False)
        else:
            self.set_buttons_state([self.idleOnList, self.idleHomedList], False)
            if self.ptButton and hal.get_value('plasmac.probe-test'):
                self.probe_test(False)
        self.set_run_button_state()
        self.set_jog_button_state()

    def interp_idle(self, obj):
        hal.set_p('plasmac.consumable-change', '0')
        if self.single_cut_request:
            self.single_cut_request = False
            if self.oldFile:
                ACTION.OPEN_PROGRAM(self.oldFile)
            self.w[self.scButton].setEnabled(True)
            if self.g91:
                ACTION.CALL_MDI_WAIT('G91')
        if not self.manualCut:
            self.set_buttons_state([self.idleList], True)
        if self.fileOpened == False:
            self.w.file_edit.setEnabled(False)
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
        self.stats_idle()
        ACTION.SET_MANUAL_MODE()

    def set_run_button_state(self):
        if STATUS.machine_is_on() and STATUS.is_all_homed() and \
           STATUS.is_interp_idle() and not self.offsetsActivePin.get() and \
           self.plasmacStatePin.get() == 0 and not self.boundsError['loaded']:
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
        self.stats_run()

    def interp_reading(self, obj):
        pass

    def interp_waiting(self, obj):
        pass

    def pause_changed(self, obj, state):
        if state:
            if self.ccButton and not hal.get_value('plasmac.cut-recovering') and hal.get_value('plasmac.stop-type-out'):
                self.w[self.ccButton].setEnabled(True)
            if self.tpButton and self.w.torch_enable.isChecked():
                self.w[self.tpButton].setEnabled(True)
            if self.otButton and self.w.ohmic_probe_enable.isChecked():
                self.w[self.otButton].setEnabled(True)
            self.w.wcs_button.setEnabled(False)
            if hal.get_value('plasmac.stop-type-out'):
                self.w.set_cut_recovery()
            self.set_tab_jog_states(True)
        elif not self.w.cut_rec_fwd.isDown() and not self.w.cut_rec_rev.isDown():
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

    def percent_loaded(self, object, percent):
        if percent < 1:
            self.w.gcode_progress.setValue(0)
            self.w.gcode_progress.setFormat('LOADING COMPLETE')
            self.w.gcode_progress.hide()
            self.w.mdi_frame.show()
        else:
            self.w.gcode_progress.setValue(percent)
            self.w.gcode_progress.setFormat('Loading: {}%'.format(percent))
            self.w.gcode_progress.show()

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
            self.w.gcode_progress.setValue(0)
            self.lastLoadedProgram = filename
            if not self.cameraOn and self.w.preview_stack.currentIndex() != 4:
                self.w.preview_stack.setCurrentIndex(0)
                self.vkb_hide()
                if self.w.chk_overlay.isChecked():
                    self.overlay.show()
            self.w.file_open.setText(os.path.basename(filename))
            self.fileOpened = True
            text = _translate('HandlerClass', 'EDIT')
            self.w.edit_label.setText('{}: {}'.format(text, filename))
            if self.w.gcode_stack.currentIndex() != 0:
                self.w.gcode_stack.setCurrentIndex(0)
            self.w.file_reload.setEnabled(True)
        self.w.gcodegraphics.logger.clear()
        self.w.file_edit.setEnabled(True)
        if self.preRflFile and self.preRflFile != ACTION.prefilter_path:
            self.rflActive = False
            self.startLine = 0
            self.preRflFile = ''
        msgList, units, xMin, yMin, xMax, yMax = self.bounds_check('loaded', 0, 0)
        if self.boundsError['loaded']:
            head = _translate('HandlerClass', 'AXIS LIMIT ERROR')
            msgs = ''
            for n in range(0, len(msgList), 3):
                if msgList[n + 1] == 'MAX':
                    msg0 = _translate('HandlerClass', 'move would exceed the maximum limit by')
                else:
                    msg0 = _translate('HandlerClass', 'move would exceed the minimum limit by')
                msgs += '{} {} {}{}\n'.format(msgList[n], msg0, msgList[n + 2], units)
            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}'.format(head, msgs))
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
        ACTION.SET_MANUAL_MODE()
        self.w.gcodegraphics.load(filename)
        if self.w.main_tab_widget.currentIndex():
            self.w.main_tab_widget.setCurrentIndex(0)

    def joints_all_homed(self, obj):
        hal.set_p('plasmac.homed', '1')
        self.interp_idle(None)
        if not self.firstHoming:
            ACTION.CALL_MDI_WAIT('T0 M6')
            ACTION.SET_MANUAL_MODE()
            self.firstHoming = True

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
        hal.set_p('plasmac.homed', '0')
        self.interp_idle(None)

    def hard_limit_tripped(self, obj, tripped, list_of_tripped):
        self.w.chk_override_limits.setEnabled(tripped)
        if not tripped:
            self.w.chk_override_limits.setChecked(False)

    def tool_changed(self, obj, tool):
        if tool == 0:
            self.w.lbl_tool.setText('Tool: TORCH')
        elif tool == 1:
            self.w.lbl_tool.setText('Tool: SCRIBE')
        else:
            self.w.lbl_tool.setText('')

    def gcodes_changed(self, obj, cod):
        if self.units == 'inch' and STATUS.is_metric_mode():
            self.droScale = 25.4
        elif self.units == 'mm' and not STATUS.is_metric_mode():
            self.droScale = 1 / 25.4
        else:
            self.droScale = 1
        self.w.lbl_gcodes.setText('G-Codes: {}'.format(cod))

    def mcodes_changed(self, obj, cod):
        self.w.lbl_mcodes.setText('M-Codes: {}'.format(cod))

    def set_start_line(self, line):
        if self.w.chk_run_from_line.isChecked():
            if self.w.sender():
                if self.w.sender().objectName() == 'gcode_editor_display':
                    return
            if line > 1:
                if not 'rfl.ngc' in self.lastLoadedProgram:
                    msg0 = _translate('HandlerClass', 'SELECTED')
                    self.runText = '{} {}'.format(msg0, line)
                    self.rflSelected = True
                    self.startLine = line - 1
                else:
                    head = _translate('HandlerClass', 'RUN FROM LINE ERROR')
                    msg0 = _translate('HandlerClass', 'Cannot select line while')
                    msg1 = _translate('HandlerClass', 'run from line is active')
                    STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n{}\n'.format(head, msg0, msg1))
            elif self.rflActive:
                txt0 = _translate('HandlerClass', 'RUN FROM LINE')
                txt1 = _translate('HandlerClass', 'CYCLE START')
                self.runText = '{}\n{}'.format(txt0, txt1)
            else:
                self.startLine = 0
                self.rflSelected = False
                self.w.gcode_display.setCursorPosition(0, 0)

    def update_gcode_properties(self, props):
        if props:
            self.gcodeProps = props
            for axis in 'XY':
                if not axis in self.gcodeProps:
                    self.gcodeProps[axis] = '{0} to {0} = {1}'.format(STATUS.stat.g5x_offset['XY'.index(axis)], 0)
            if props['GCode Units'] == 'in':
                STATUS.emit('metric-mode-changed', False)
            else:
                STATUS.emit('metric-mode-changed', True)

    def error_update(self, obj, kind, error):
        if kind == linuxcnc.OPERATOR_ERROR or kind == linuxcnc.NML_ERROR:
            self.error_status(True)


#########################################################################################################################
# CALLBACKS FROM FORM #
#########################################################################################################################

    def ext_power(self, state):
        if self.w.power.isEnabled() and state:
            ACTION.SET_MACHINE_STATE(not STATUS.machine_is_on())

    def ext_run(self, state):
        if self.w.run.isEnabled() and state:
            self.run_pressed()

    def ext_abort(self, state):
        if self.w.abort.isEnabled() and state:
            self.abort_pressed()

    def ext_pause(self, state):
        if self.w.pause.isEnabled() and state:
            ACTION.PAUSE()

    def ext_touch_off(self, state):
        if self.w.touch_xy.isEnabled() and state:
            self.touch_xy_clicked()

    def ext_jog_slow(self, state):
        if self.w.jog_slow.isEnabled() and state:
            self.jog_slow_pressed(True)

    def ext_run_pause(self, state):
        if self.w.run.isEnabled() and state:
            self.run_pressed()
        elif self.w.pause.isEnabled() and state:
            ACTION.PAUSE()

    def run_pressed(self):
        self.wcs_rotation('get')
        if self.startLine and self.rflSelected:
            self.w.run.setEnabled(False)
            if self.frButton:
                self.w[self.frButton].setEnabled(False)
            self.rflActive = True
            self.rflSelected = False
            self.run_from_line()
        elif not self.run_critical_check():
            ACTION.RUN(0)

    def abort_pressed(self):
        if self.manualCut:
            ACTION.SET_SPINDLE_STOP(0)
            if self.mcButton:
                self.button_normal(self.mcButton)
                self.w[self.mcButton].setEnabled(False)
            self.w.abort.setEnabled(False)
            return
        elif self.probeTest:
            self.probe_test_stop()
            return
        else:
            ACTION.ABORT()
            if self.torchPulse:
                self.torch_pulse(True)
            hal.set_p('plasmac.cut-recovery', '0')
            self.interp_idle(None)
            self.wcs_rotation('set')

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

    def height_ovr_scale_change (self,value):
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
        with tarfile.open('{}/{}'.format(bkpPath, bkpName), mode='w:gz', ) as archive:
            archive.add('{}'.format(self.PATHS.CONFIGPATH))
        head = _translate('HandlerClass', 'BACKUP COMPLETE')
        msg0 = _translate('HandlerClass', 'A compressed backup of the machine configuration including the machine logs has been saved in your home directory as')
        msg1 = _translate('HandlerClass', 'It is safe to delete this file at any time')
        self.dialog_show_ok(QMessageBox.Information, head, '{}:\n{}\n\n{}\n'.format(msg0, bkpName, msg1))

    def set_offsets_clicked(self):
# **** TEMP FOR OFFSET TESTING 2 of 2 ****
#        reload(OFFSETS)
        OFFSETS.dialog_show(self, self.w, INIPATH, STATUS, ACTION, TOOL, self.foreColor, self.backColor)

    def feed_label_pressed(self):
        self.w.feed_slider.setValue(100)

    def rapid_label_pressed(self):
        self.w.rapid_slider.setValue(100)

    def jogs_label_pressed(self):
        self.w.jog_slider.setValue(INFO.DEFAULT_LINEAR_JOG_VEL)

    def gui_button_jog(self, state, joint, direction):
        shift = False
        if STATUS.is_joint_mode():
            self.kb_jog(state, self.coordinates.index(joint), direction, shift)
        else:
            self.kb_jog(state, ['x','y','z','a','b'].index(joint), direction, shift)

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

    def file_open_clicked(self):
        self.w.preview_stack.setCurrentIndex(1)
        self.vkb_hide()
        self.overlay.hide()
        self.w.filemanager.table.setFocus()

    def file_edit_clicked(self):
        if STATUS.stat.interp_state == linuxcnc.INTERP_IDLE and self.w.preview_stack.currentIndex() != 2:
            self.w.preview_stack.setCurrentIndex(2)
            self.overlay.hide()
            self.w.gcode_editor.editor.setFocus()
            self.vkb_show()
        else:
            self.new_exitCall()

    def mdi_show_clicked(self):
        if STATUS.is_on_and_idle() and STATUS.is_all_homed() and self.w.gcode_stack.currentIndex() != 1:
            self.w.gcode_stack.setCurrentIndex(1)
        else:
            self.w.gcode_stack.setCurrentIndex(0)
            ACTION.SET_MANUAL_MODE()

    def file_cancel_clicked(self):
        self.w.preview_stack.setCurrentIndex(0)
        self.vkb_hide()
        if self.w.chk_overlay.isChecked():
            self.overlay.show()
        ACTION.SET_MANUAL_MODE()

    def cone_size_changed(self, data):
        self.w.gcodegraphics.set_cone_basesize(data)

    def grid_size_changed(self, data):
        # grid size is in inches
        grid = data / self.unitsPerMm / 25.4
        self.w.gcodegraphics.grid_size = grid

    def main_tab_changed(self, tab):
        if tab == 0:
            if self.w.view_p.isChecked():
                self.w.gcodegraphics.set_view('P')
            else:
                self.w.gcodegraphics.set_view('Z')
            self.w.gcodegraphics.set_current_view()
            if self.w.preview_stack.currentIndex() == 2:
                self.vkb_show()
            else:
                self.vkb_hide()
            self.autorepeat_keys(False)
        elif tab == 1:
            self.w.conv_preview.logger.clear()
            self.w.conv_preview.set_current_view()
            self.conv_button_color('conv_line')
            self.oldConvButton = False
            self.conv_setup()
            self.vkb_show(True)
            self.autorepeat_keys(True)
        elif tab == 2:
            self.vkb_show(True)
            self.autorepeat_keys(True)
        elif tab == 3 and self.gui43:
            self.vkb_show(True)
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

    def plasmac_state_changed(self, state):
        if ((state > self.PROBE_UP and not STATUS.is_interp_idle()) or state == self.PROBE_TEST) and hal.get_value('axis.z.eoffset-counts'):
            # set z dro to offset mode
            self.w.dro_z.setProperty('Qreference_type', 10)
        if state == self.IDLE:
            self.set_run_button_state()
            self.set_jog_button_state()
        self.stats_state_changed(state)

    def file_reload_clicked(self):
        if self.rflActive:
            self.rflActive = False
            self.set_run_button_state()
            self.startLine = 0
            self.preRflFile = ''
        if ACTION.prefilter_path or self.lastLoadedProgram != 'None':
            file = ACTION.prefilter_path or self.lastLoadedProgram
            if os.path.exists(file):
                self.w.gcode_progress.setValue(0)
                ACTION.OPEN_PROGRAM(file)
            else:
                head = _translate('HandlerClass', 'FILE ERROR')
                msg0 = _translate('HandlerClass', 'does not exist')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} {}'.format(head, file, msg0))

    def jog_inhibit_changed(self, state, switch):
        if state and not self.jogInhibit:
            for axis in [0,1,2,3]:
                if self.isJogging[axis]:
                    ACTION.JOG(axis, 0, 0, 0)
                    head = _translate('HandlerClass', 'JOG ERROR')
                    msg0 = _translate('HandlerClass', 'Jogging stopped')
                    msg1 = _translate('HandlerClass', 'tripped')
                    STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n{} {}'.format(head, msg0, switch, msg1))
                    self.isJogging[axis] = False
            self.jogInhibit = switch
            self.jogInhibitPin.set(True)
        else:
            if self.w.led_float_switch.hal_pin.get():
                self.jogInhibit = 'float switch'
                self.jogInhibitPin.set(True)
            elif self.w.led_ohmic_probe.hal_pin.get():
                self.jogInhibit = 'ohmic probe'
                self.jogInhibitPin.set(True)
            elif self.w.led_breakaway_switch.hal_pin.get():
                self.jogInhibit = 'breakaway switch'
                self.jogInhibitPin.set(True)
            else:
                self.jogInhibit = ''
                self.jogInhibitPin.set(False)

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
            if self.gui43:
                self.w.main_tab_widget.setTabEnabled(3, False)
        else:
            self.w.main_tab_widget.setTabEnabled(2, True)
            if self.gui43:
                self.w.main_tab_widget.setTabEnabled(3, True)

    def conv_tab_changed(self, state):
        if state:
            self.w.main_tab_widget.setTabEnabled(1, False)
        else:
            self.w.main_tab_widget.setTabEnabled(1, True)


#########################################################################################################################
# GENERAL FUNCTIONS #
#########################################################################################################################

    def wcs_rotation(self, wcs):
        if wcs == 'get':
            self.currentRotation = STATUS.stat.rotation_xy
        elif wcs == 'set' and self.currentRotation != STATUS.stat.rotation_xy:
            ACTION.CALL_MDI_WAIT('G10 L2 P0 R{}'.format(self.currentRotation))
            ACTION.SET_MANUAL_MODE()
            self.w.gcodegraphics.set_current_view()

    def set_buttons_state(self, buttonLists, state):
        for buttonList in buttonLists:
            for button in buttonList:
                if state and STATUS.is_interp_paused() and button not in self.pausedValidList:
                    continue
                if not state and button == self.tpButton and self.torchTimer.isActive():
                    continue
                self.w[button].setEnabled(state)
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
        if state:
            text = _translate('HandlerClass', 'Error sent to machine log')
        else:
            text = ''
        self.w.error_label.setText("{}".format(text))

    def touch_off_xy(self, x, y):
        if STATUS.is_on_and_idle() and STATUS.is_all_homed():
            ACTION.CALL_MDI_WAIT('G10 L20 P0 X{} Y{}'.format(x, y))
            if self.fileOpened == True:
                self.file_reload_clicked()
            ACTION.SET_MANUAL_MODE()

    def bounds_check(self, boundsType, xOffset , yOffset):
        self.boundsError[boundsType] = False
        msgList = []
        boundsMultiplier = 1
        if self.units == 'inch':
            units = 'in'
            if self.gcodeProps['Units'] == 'mm':
                boundsMultiplier = 0.03937
        else:
            units = 'mm'
            if self.gcodeProps['Units'] == 'in':
                boundsMultiplier = 25.4
        xMin = float(self.gcodeProps['X'].split()[0]) * boundsMultiplier + xOffset
        if xMin < self.xMin:
            amount = float(self.xMin - xMin)
            msgList.append('X')
            msgList.append('MIN')
            msgList.append('{:0.2f}'.format(amount))
            self.boundsError[boundsType] = True
        xMax = float(self.gcodeProps['X'].split()[2]) * boundsMultiplier + xOffset
        if xMax > self.xMax:
            amount = float(xMax - self.xMax)
            msgList.append('X')
            msgList.append('MAX')
            msgList.append('{:0.2f}'.format(amount))
            self.boundsError[boundsType] = True
        yMin = float(self.gcodeProps['Y'].split()[0]) * boundsMultiplier + yOffset
        if yMin < self.yMin:
            amount = float(self.yMin - yMin)
            msgList.append('Y')
            msgList.append('MIN')
            msgList.append('{:0.2f}'.format(amount))
            self.boundsError[boundsType] = True
        yMax = float(self.gcodeProps['Y'].split()[2]) * boundsMultiplier + yOffset
        if yMax > self.yMax:
            amount = float(yMax - self.yMax)
            msgList.append('Y')
            msgList.append('MAX')
            msgList.append('{:0.2f}'.format(amount))
            self.boundsError[boundsType] = True
        return msgList, units, xMin, yMin, xMax, yMax

    def save_plasma_parameters(self):
        self.w.PREFS_.putpref('Arc OK High', self.w.arc_ok_high.value(), float, 'PLASMA_PARAMETERS')
        self.w.PREFS_.putpref('Arc OK Low', self.w.arc_ok_low.value(), float, 'PLASMA_PARAMETERS')
        self.w.PREFS_.putpref('Arc Maximum Starts', self.w.arc_max_starts.value(), int, 'PLASMA_PARAMETERS')
        self.w.PREFS_.putpref('Arc Fail Timeout', self.w.arc_fail_delay.value(), float, 'PLASMA_PARAMETERS')
        self.w.PREFS_.putpref('Arc Voltage Offset', self.w.arc_voltage_offset.value(), float, 'PLASMA_PARAMETERS')
        self.w.PREFS_.putpref('Arc Voltage Scale', self.w.arc_voltage_scale.value(), float, 'PLASMA_PARAMETERS')
        self.w.PREFS_.putpref('Velocity Anti Dive Threshold', self.w.cornerlock_threshold.value(), float, 'PLASMA_PARAMETERS')
        self.w.PREFS_.putpref('Float Switch Travel', self.w.float_switch_travel.value(), float, 'PLASMA_PARAMETERS')
        self.w.PREFS_.putpref('Height Per Volt', self.w.height_per_volt.value(), float, 'PLASMA_PARAMETERS')
        self.w.PREFS_.putpref('Void Sense Override', self.w.kerfcross_override.value(), float, 'PLASMA_PARAMETERS')
        self.w.PREFS_.putpref('Ohmic Maximum Attempts', self.w.ohmic_max_attempts.value(), int, 'PLASMA_PARAMETERS')
        self.w.PREFS_.putpref('Ohmic Probe Offset', self.w.ohmic_probe_offset.value(), float, 'PLASMA_PARAMETERS')
        self.w.PREFS_.putpref('Pid P Gain', self.w.pid_p_gain.value(), float, 'PLASMA_PARAMETERS')
        self.w.PREFS_.putpref('Pid D Gain', self.w.pid_d_gain.value(), float, 'PLASMA_PARAMETERS')
        self.w.PREFS_.putpref('Pid I Gain', self.w.pid_i_gain.value(), float, 'PLASMA_PARAMETERS')
        self.w.PREFS_.putpref('Probe Feed Rate', self.w.probe_feed_rate.value(), float, 'PLASMA_PARAMETERS')
        self.w.PREFS_.putpref('Probe Start Height', self.w.probe_start_height.value(), float, 'PLASMA_PARAMETERS')
        self.w.PREFS_.putpref('Arc Restart Delay', self.w.arc_restart_delay.value(), float, 'PLASMA_PARAMETERS')
        self.w.PREFS_.putpref('Safe Height', self.w.safe_height.value(), float, 'PLASMA_PARAMETERS')
        self.w.PREFS_.putpref('Scribe Arming Delay', self.w.scribe_arm_delay.value(), float, 'PLASMA_PARAMETERS')
        self.w.PREFS_.putpref('Scribe On Delay', self.w.scribe_on_delay.value(), float, 'PLASMA_PARAMETERS')
        self.w.PREFS_.putpref('Setup Feed Rate', self.w.setup_feed_rate.value(), float, 'PLASMA_PARAMETERS')
        self.w.PREFS_.putpref('Skip IHS Distance', self.w.skip_ihs_distance.value(), float, 'PLASMA_PARAMETERS')
        self.w.PREFS_.putpref('Spotting Threshold', self.w.spotting_threshold.value(), float, 'PLASMA_PARAMETERS')
        self.w.PREFS_.putpref('Spotting Time', self.w.spotting_time.value(), float, 'PLASMA_PARAMETERS')
        self.w.PREFS_.putpref('THC Delay', self.w.thc_delay.value(), float, 'PLASMA_PARAMETERS')
        self.w.PREFS_.putpref('THC Threshold', self.w.thc_threshold.value(), float, 'PLASMA_PARAMETERS')

    def load_plasma_parameters(self):
        self.w.setup_feed_rate.setValue(self.w.PREFS_.getpref('Setup Feed Rate', self.thcFeedRate * 0.8, float, 'PLASMA_PARAMETERS'))
        self.w.probe_feed_rate.setMaximum(self.w.setup_feed_rate.value())
        self.w.arc_fail_delay.setValue(self.w.PREFS_.getpref('Arc Fail Timeout', 3, float, 'PLASMA_PARAMETERS'))
        self.w.arc_ok_high.setValue(self.w.PREFS_.getpref('Arc OK High', 250, float, 'PLASMA_PARAMETERS'))
        self.w.arc_ok_low.setValue(self.w.PREFS_.getpref('Arc OK Low', 60, float, 'PLASMA_PARAMETERS'))
        self.w.arc_max_starts.setValue(self.w.PREFS_.getpref('Arc Maximum Starts', 3, int, 'PLASMA_PARAMETERS'))
        self.w.arc_voltage_offset.setValue(self.w.PREFS_.getpref('Arc Voltage Offset', 0, float, 'PLASMA_PARAMETERS'))
        self.w.arc_voltage_scale.setValue(self.w.PREFS_.getpref('Arc Voltage Scale', 1, float, 'PLASMA_PARAMETERS'))
        self.w.cornerlock_threshold.setValue(self.w.PREFS_.getpref('Velocity Anti Dive Threshold', 90, float, 'PLASMA_PARAMETERS'))
        self.w.float_switch_travel.setValue(self.w.PREFS_.getpref('Float Switch Travel', round(1.5 * self.unitsPerMm, 2), float, 'PLASMA_PARAMETERS'))
        self.w.height_per_volt.setValue(self.w.PREFS_.getpref('Height Per Volt', round(0.1 * self.unitsPerMm, 3), float, 'PLASMA_PARAMETERS'))
        self.w.kerfcross_override.setValue(self.w.PREFS_.getpref('Void Sense Override', 100, float, 'PLASMA_PARAMETERS'))
        self.w.ohmic_max_attempts.setValue(self.w.PREFS_.getpref('Ohmic Maximum Attempts', 0, int, 'PLASMA_PARAMETERS'))
        self.w.ohmic_probe_offset.setValue(self.w.PREFS_.getpref('Ohmic Probe Offset', 0, float, 'PLASMA_PARAMETERS'))
        self.w.pid_p_gain.setValue(self.w.PREFS_.getpref('Pid P Gain', 10, float, 'PLASMA_PARAMETERS'))
        self.w.pid_d_gain.setValue(self.w.PREFS_.getpref('Pid D Gain', 0, float, 'PLASMA_PARAMETERS'))
        self.w.pid_i_gain.setValue(self.w.PREFS_.getpref('Pid I Gain', 0, float, 'PLASMA_PARAMETERS'))
        self.w.probe_feed_rate.setValue(self.w.PREFS_.getpref('Probe Feed Rate', round(300 * self.unitsPerMm, 0), float, 'PLASMA_PARAMETERS'))
        self.w.probe_start_height.setValue(self.w.PREFS_.getpref('Probe Start Height', round(25 * self.unitsPerMm, 0), float, 'PLASMA_PARAMETERS'))
        self.w.arc_restart_delay.setValue(self.w.PREFS_.getpref('Arc Restart Delay', 0, float, 'PLASMA_PARAMETERS'))
        self.w.safe_height.setValue(self.w.PREFS_.getpref('Safe Height', round(25 * self.unitsPerMm, 0), float, 'PLASMA_PARAMETERS'))
        self.w.setup_feed_rate.setValue(self.w.PREFS_.getpref('Setup Feed Rate', self.thcFeedRate * 0.8, float, 'PLASMA_PARAMETERS'))
        self.w.scribe_arm_delay.setValue(self.w.PREFS_.getpref('Scribe Arming Delay', 0, float, 'PLASMA_PARAMETERS'))
        self.w.scribe_on_delay.setValue(self.w.PREFS_.getpref('Scribe On Delay', 0, float, 'PLASMA_PARAMETERS'))
        self.w.skip_ihs_distance.setValue(self.w.PREFS_.getpref('Skip IHS Distance', 0, float, 'PLASMA_PARAMETERS'))
        self.w.spotting_threshold.setValue(self.w.PREFS_.getpref('Spotting Threshold', 0, float, 'PLASMA_PARAMETERS'))
        self.w.spotting_time.setValue(self.w.PREFS_.getpref('Spotting Time', 0, float, 'PLASMA_PARAMETERS'))
        self.w.thc_delay.setValue(self.w.PREFS_.getpref('THC Delay', 0.5, float, 'PLASMA_PARAMETERS'))
        self.w.thc_threshold.setValue(self.w.PREFS_.getpref('THC Threshold', 1, float, 'PLASMA_PARAMETERS'))

    def set_signal_connections(self):
        self.w.run.pressed.connect(self.run_pressed)
        self.w.abort.pressed.connect(self.abort_pressed)
        self.w.file_reload.clicked.connect(self.file_reload_clicked)
        self.w.jog_slow.pressed.connect(self.jog_slow_pressed)
        self.w.chk_soft_keyboard.stateChanged.connect(self.soft_keyboard)
        self.w.chk_override_limits.stateChanged.connect(self.chk_override_limits_changed)
        self.w.chk_overlay.stateChanged.connect(self.overlay_changed)
        self.w.chk_tool_tips.stateChanged.connect(lambda:TOOLTIPS.tool_tips_changed(self.w))
        self.w.torch_enable.stateChanged.connect(lambda w:self.torch_enable_changed(w))
        self.w.ohmic_probe_enable.stateChanged.connect(lambda w:self.ohmic_probe_enable_changed(w))
        self.w.cone_size.valueChanged.connect(self.cone_size_changed)
        self.w.grid_size.valueChanged.connect(self.grid_size_changed)
        self.w.gcode_display.linesChanged.connect(self.gcode_display_loaded)
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
        self.xOffsetPin.value_changed.connect(self.cutrec_offset_changed)
        self.yOffsetPin.value_changed.connect(self.cutrec_offset_changed)
        self.offsetsActivePin.value_changed.connect(lambda v:self.offsets_active_changed(v))
        self.consChangePin.value_changed.connect(lambda v:self.consumable_change_changed(v))
        self.w.cam_mark.pressed.connect(self.cam_mark_pressed)
        self.w.cam_goto.pressed.connect(self.cam_goto_pressed)
        self.w.cam_zoom_plus.pressed.connect(self.cam_zoom_plus_pressed)
        self.w.cam_zoom_minus.pressed.connect(self.cam_zoom_minus_pressed)
        self.w.cam_dia_plus.pressed.connect(self.cam_dia_plus_pressed)
        self.w.cam_dia_minus.pressed.connect(self.cam_dia_minus_pressed)
        self.w.conv_line.pressed.connect(lambda:self.conv_shape_request('conv_line', CONVLINE, True))
        self.w.conv_circle.pressed.connect(lambda:self.conv_shape_request('conv_circle', CONVCIRC, True))
        self.w.conv_ellipse.pressed.connect(lambda:self.conv_shape_request('conv_ellipse', CONVELLI, True))
        self.w.conv_triangle.pressed.connect(lambda:self.conv_shape_request('conv_triangle', CONVTRIA, True))
        self.w.conv_rectangle.pressed.connect(lambda:self.conv_shape_request('conv_rectangle', CONVRECT, True))
        self.w.conv_polygon.pressed.connect(lambda:self.conv_shape_request('conv_polygon', CONVPOLY, True))
        self.w.conv_bolt.pressed.connect(lambda:self.conv_shape_request('conv_bolt', CONVBOLT, True))
        self.w.conv_slot.pressed.connect(lambda:self.conv_shape_request('conv_slot', CONVSLOT, True))
        self.w.conv_star.pressed.connect(lambda:self.conv_shape_request('conv_star', CONVSTAR, True))
        self.w.conv_gusset.pressed.connect(lambda:self.conv_shape_request('conv_gusset', CONVGUST, True))
        self.w.conv_sector.pressed.connect(lambda:self.conv_shape_request('conv_sector', CONVSECT, True))
        self.w.conv_block.pressed.connect(self.conv_block_pressed)
        self.w.conv_new.pressed.connect(lambda:self.conv_new_pressed('button'))
        self.w.conv_save.pressed.connect(self.conv_save_pressed)
        self.w.conv_settings.pressed.connect(self.conv_settings_pressed)
        self.w.conv_send.pressed.connect(self.conv_send_pressed)
        self.w.view_p.pressed.connect(self.view_p_pressed)
        self.w.view_z.pressed.connect(self.view_z_pressed)
        self.w.view_clear.pressed.connect(self.view_clear_pressed)
        self.w.pan_left.pressed.connect(self.pan_left_pressed)
        self.w.pan_right.pressed.connect(self.pan_right_pressed)
        self.w.pan_up.pressed.connect(self.pan_up_pressed)
        self.w.pan_down.pressed.connect(self.pan_down_pressed)
        self.w.zoom_in.pressed.connect(self.zoom_in_pressed)
        self.w.zoom_out.pressed.connect(self.zoom_out_pressed)
        self.w.camera.pressed.connect(self.camera_pressed)
        self.w.laser.pressed.connect(self.laser_pressed)
        self.w.main_tab_widget.currentChanged.connect(lambda w:self.main_tab_changed(w))
        self.zHeightPin.value_changed.connect(lambda v:self.z_height_changed(v))
        self.plasmacStatePin.value_changed.connect(lambda v:self.plasmac_state_changed(v))
        self.w.feed_label.pressed.connect(self.feed_label_pressed)
        self.w.rapid_label.pressed.connect(self.rapid_label_pressed)
        self.w.jogs_label.pressed.connect(self.jogs_label_pressed)
        self.w.led_float_switch.hal_pin.value_changed.connect(lambda v:self.jog_inhibit_changed(v, 'float switch'))
        self.w.led_ohmic_probe.hal_pin.value_changed.connect(lambda v:self.jog_inhibit_changed(v, 'ohmic probe'))
        self.w.led_breakaway_switch.hal_pin.value_changed.connect(lambda v:self.jog_inhibit_changed(v, 'breakaway switch'))
        self.paramTabDisable.value_changed.connect(lambda v:self.param_tab_changed(v))
        self.convTabDisable.value_changed.connect(lambda v:self.conv_tab_changed(v))
        self.pierceCountPin.value_changed.connect(self.pierce_count_changed)
        self.cutLengthPin.value_changed.connect(lambda v:self.cut_length_changed(v))
        self.cutTimePin.value_changed.connect(lambda v:self.cut_time_changed(v))
        self.torchOnPin.value_changed.connect(lambda v:self.torch_on_changed(v))
        self.motionTypePin.value_changed.connect(lambda v:self.motion_type_changed(v))
        self.w.pierce_reset.pressed.connect(self.pierce_reset)
        self.w.cut_length_reset.pressed.connect(self.cut_length_reset)
        self.w.cut_time_reset.pressed.connect(self.cut_time_reset)
        self.w.torch_time_reset.pressed.connect(self.torch_time_reset)
        self.w.run_time_reset.pressed.connect(self.run_time_reset)
        self.w.rapid_time_reset.pressed.connect(self.rapid_time_reset)
        self.w.probe_time_reset.pressed.connect(self.probe_time_reset)
        self.w.all_reset.pressed.connect(self.all_reset)
        self.extPowerPin.value_changed.connect(lambda v:self.ext_power(v))
        self.extRunPin.value_changed.connect(lambda v:self.ext_run(v))
        self.extPausePin.value_changed.connect(lambda v:self.ext_pause(v))
        self.extAbortPin.value_changed.connect(lambda v:self.ext_abort(v))
        self.extTouchOffPin.value_changed.connect(lambda v:self.ext_touch_off(v))
        self.extRunPausePin.value_changed.connect(lambda v:self.ext_run_pause(v))
        self.extHeightOvrPlusPin.value_changed.connect(lambda v:self.height_ovr_pressed(v,1))
        self.extHeightOvrMinusPin.value_changed.connect(lambda v:self.height_ovr_pressed(v,-1))
        self.extHeightOvrResetPin.value_changed.connect(lambda v:self.height_ovr_pressed(v,0))
        self.extHeightOvrCountsPin.value_changed.connect(lambda v:self.height_ovr_encoder(v))
        self.extHeightOvrScalePin.value_changed.connect(lambda v:self.height_ovr_scale_change(v))
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
        self.extCutReccancelPin.value_changed.connect(lambda v:self.cutrec_cancel_pressed(v))
        self.extTorchEnablePin.value_changed.connect(lambda v:self.ext_torch_enable_changed(v))
        self.extThcEnablePin.value_changed.connect(lambda v:self.ext_thc_enable_changed(v))
        self.extCornerLockEnablePin.value_changed.connect(lambda v:self.ext_corner_lock_enable_changed(v))
        self.extKerfCrossEnablePin.value_changed.connect(lambda v:self.ext_kerf_cross_enable_changed(v))
        self.extIgnoreArcOkPin.value_changed.connect(lambda v:self.ext_ignore_arc_ok_changed(v))
        self.extMeshModePin.value_changed.connect(lambda v:self.ext_mesh_mode_changed(v))
        self.extOhmicProbeEnablePin.value_changed.connect(lambda v:self.ext_ohmic_probe_enable_changed(v))
        self.extAutoVoltsEnablePin.value_changed.connect(lambda v:self.ext_auto_volts_enable_changed(v))
        self.extJogSlowPin.value_changed.connect(self.ext_jog_slow)
        self.probeTestErrorPin.value_changed.connect(lambda v:self.probe_test_error(v))
        self.w.preview_stack.currentChanged.connect(self.preview_stack_changed)
        self.w.gcode_stack.currentChanged.connect(self.gcode_stack_changed)
        click_signal(self.w.material_label).connect(self.show_material_selector)
        click_signal(self.w.velocity_label).connect(self.show_material_selector)
        click_signal(self.w.velocity_show).connect(self.show_material_selector)

    def set_axes_and_joints(self):
        kinematics = self.iniFile.find('KINS', 'KINEMATICS').lower().replace('=','').replace('trivkins','').replace(' ','') or None
        kinstype = None
        self.coordinates = 'xyz'
        if kinematics:
            if 'kinstype' in kinematics:
                kinstype = kinematics.lower().replace(' ','').split('kinstype')[1]
                if 'coordinates' in kinematics:
                    kinematics = kinematics.lower().replace(' ','').split('kinstype')[0]
            if 'coordinates' in kinematics:
                self.coordinates = kinematics.split('coordinates')[1].lower()
        else:
            head = _translate('HandlerClass', 'INI FILE ERROR')
            msg0  = _translate('HandlerClass', 'Error in [KINS]KINEMATICS in the ini file')
            msg1 = _translate('HandlerClass', 'reverting to default coordinates of xyz')
            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n{}'.format(head, msg0, msg1))
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
        # check if home all button required
        for joint in range(len(self.coordinates)):
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
                      'kerfcross_frm', 'height_per_volt', 'height_per_volt_lbl',
                      'thc_delay', 'thc_delay_lbl', 'thc_threshold', 'thc_threshold_lbl',
                      'pid_i_gain', 'pid_i_gain_lbl', 'pid_d_gain', 'pid_d_gain_lbl',
                      'use_auto_volts', 'use_auto_volts_lbl', 'led_thc_active', 'led_thc_active_lbl',
                       'arc_voltage', 'arc_override_frm','kerfcross_override', 'kerfcross_override_lbl' ]
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
        if self.units == 'inch':
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
        if self.jogInhibit and state and (joint != 2 or direction != 1):
            head = _translate('HandlerClass', 'JOG ERROR')
            msg0 = _translate('HandlerClass', 'Cannot jog')
            msg1 = _translate('HandlerClass', 'tripped')
            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n{} {}'.format(head, msg0, self.jogInhibit, msg1))
            return
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
            self.isJogging[joint] = True
            self.w.grabKeyboard()
        else:
            self.w.releaseKeyboard()
            if not STATUS.get_jog_increment():
                ACTION.JOG(joint, 0, 0, 0)
                self.isJogging[joint] = False

    def keyboard_shortcuts(self):
        if self.w.chk_keyboard_shortcuts.isChecked():
            return True
        else:
            return False

    def soft_keyboard(self):
        if self.w.chk_soft_keyboard.isChecked():
            self.w.mdihistory.MDILine.setProperty('dialog_keyboard_option',True)
            inputType = 'CALCULATOR'
            self.w.originoffsetview.setProperty('dialog_code_string','CALCULATOR')
            self.w.originoffsetview.setProperty('text_dialog_code_string','KEYBOARD')
            self.w.gcode_display.SendScintilla(QsciScintilla.SCI_SETEXTRAASCENT, 4)
            self.w.gcode_display.SendScintilla(QsciScintilla.SCI_SETEXTRADESCENT, 4)
            self.w.gcode_editor.editor.SendScintilla(QsciScintilla.SCI_SETEXTRAASCENT, 4)
            self.w.gcode_editor.editor.SendScintilla(QsciScintilla.SCI_SETEXTRADESCENT, 4)
            self.vkb_check()
            if (self.w.main_tab_widget.currentIndex() == 2 and not self.gui43) or \
               (self.w.main_tab_widget.currentIndex() == 3 and self.gui43):
                self.vkb_show(True)
        else:
            self.w.mdihistory.MDILine.setProperty('dialog_keyboard_option',False)
            inputType = 'ENTRY'
            self.w.originoffsetview.setProperty('dialog_code_string','')
            self.w.originoffsetview.setProperty('text_dialog_code_string','')
            self.w.gcode_display.SendScintilla(QsciScintilla.SCI_SETEXTRAASCENT, 1)
            self.w.gcode_display.SendScintilla(QsciScintilla.SCI_SETEXTRADESCENT, 1)
            self.w.gcode_editor.editor.SendScintilla(QsciScintilla.SCI_SETEXTRAASCENT, 1)
            self.w.gcode_editor.editor.SendScintilla(QsciScintilla.SCI_SETEXTRADESCENT, 1)
            self.vkb_hide()
        for axis in 'xyzab':
            button = 'touch_{}'.format(axis)
            self.w[button].dialog_code = inputType

    def overlay_changed(self, state):
        if self.w.chk_overlay.isChecked():
            self.overlay.show()
            self.overlayConv.show()
        else:
            self.overlay.hide()
            self.overlayConv.hide()

    def dialog_show_ok(self, icon, title, error, bText=_translate('HandlerClass', 'OK')):
        msg = QMessageBox(self.w)
        buttonY = msg.addButton(QMessageBox.Yes)
        buttonY.setText(bText)
        buttonY.setIcon(QIcon())
        msg.setIcon(icon)
        msg.setWindowTitle(title)
        msg.setText(error)
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
        choice = msg.exec_()
        if choice == QMessageBox.Yes:
            return True
        else:
            return False

    def dialog_input(self, title, text, btn1, btn2, delay=None):
        input = QInputDialog(self.w)
        input.setWindowTitle(title)
        input.setLabelText('{}'.format(text))
        if btn1:
            input.setOkButtonText(btn1)
        if btn2:
            input.setCancelButtonText(btn2)
        if delay is not None:
            print("DELAY:", delay)
            input.setTextValue('{:0.2f}'.format(delay))
        for button in input.findChildren(QPushButton):
            button.setIcon(QIcon())
        valid = input.exec_()
        out = input.textValue()
        return valid, out

    def run_from_line(self):
        inData,outData,newFile,params = [],[],[],[]
        g2,g4,g6,g9,d3,d2,a3,material,x,y,code,rflSpindle = '','','','','','','','','','','',''
        oSub = False
        count = 0
        tmpMat = False
        head = _translate('HandlerClass', 'GCODE ERROR')
        with open(self.lastLoadedProgram, 'r') as inFile:
            for line in inFile:
                if count < self.startLine:
                    inData.append(line.lower())
                else:
                    if count == self.startLine:
                        if 'g21' in line:
                            newFile.append('g21')
                        elif 'g20' in line:
                            newFile.append('g20')
                    outData.append(line.lower())
                count += 1
        cutComp = False
        for line in inData:
            if line.startswith('('):
                if line.startswith('(o='):
                    material = line.strip()
                continue
            if line.startswith('#'):
                params.append(line.strip())
                continue
            if line.startswith('m190'):
               mat = line.split('p')[1]
               if '(' in mat:
                   num = int(mat.split('(')[0])
               else:
                   num = int(mat)
               if num >= 1000000:
                   tmpMat = True
               else:
                   material = line.strip()
               continue
            if line.replace(' ','').startswith('m66p3') and tmpMat:
                tmpMat = False
                continue
            for t1 in ['g20','g21','g40','g41.1','g42.1','g61', 'g61.1', 'g64', 'g90','g91']:
                if t1 in line:
                    if t1[1] == '2':
                        g2 = t1
                    elif t1[1] == '4':
                        g4 = t1
                        if t1 != 'g40':
                            cutComp = True
                        else:
                            cutComp = False
                    elif t1[1] == '6':
                        g6 = t1
                        if t1 == 'g64':
                            tmp = line.split('64')[1]
                            if tmp[0] == 'p':
                                p = ''
                                tmp = tmp[1:]
                                while 1:
                                    if tmp[0] in '.0123456789q':
                                        p += tmp[0]
                                        tmp = tmp[1:]
                                    else:
                                        break
                                g6 = 'g64p{}'.format(p)
                    elif t1[1] == '9':
                        g9 = t1
            if 'g0' in line:
                code = 'g0'
            if 'g1' in line:
                tmp = line.split('g1')[1]
                if tmp[0] not in '0123456789':
                    code = 'g1'
            if 'g2' in line:
                tmp = line.split('g2')[1]
                if tmp[0] not in '0123456789':
                    code = 'g2'
            if 'g3' in line:
                tmp = line.split('g3')[1]
                if tmp[0] not in '0123456789':
                    code = 'g3'
            if 'x' in line:
                x = self.get_rfl_pos(line.strip(), x, 'x')
            if 'y' in line:
                y = self.get_rfl_pos(line.strip(), y, 'y')
            if 'm3' in line:
                rflSpindle = 'm3'
                tmp = line.split('m3')[1]
                while 1:
                    if tmp[0] in '0123456789s$':
                        rflSpindle += tmp[0]
                        tmp = tmp[1:]
                    else:
                        break
            if 'm5' in line:
                rflSpindle = ''
            if 'm62p3' in line:
                d3 = 'm62p3 (Disable Torch)'
            elif 'm63p3' in line:
                d3 = 'm63p3 (Enable Torch)'
            elif 'm64p3' in line:
                d3 = 'm64p3 (Disable Torch)'
            elif 'm65p3' in line:
                d3 = 'm65p3 (Enable Torch)'
            if 'm62p2' in line:
                d2 = 'm62p2 (Disable THC)'
            elif 'm63p2' in line:
                d2 = 'm63p2 (Enable THC)'
            elif 'm64p2' in line:
                d2 = 'm64p2 (Disable THC)'
            elif 'm65p2' in line:
                d2 = 'm65p2 (Enable THC)'
            if 'm67e3q' in line:
                a3 = 'm67e3q'
                tmp = line.split('m67e3q')[1]
                while 1:
                    if tmp[0] in '-.0123456789':
                        a3 += tmp[0]
                        tmp = tmp[1:]
                    else:
                        break
                a3 += ' (Velocity {}%)'.format(a3.split('m67e3q')[1])
            if 'm68e3q' in line:
                a3 = 'm68e3q'
                tmp = line.split('m68e3q')[1]
                bb=1
                while 1:
                    if tmp[0] in '-.0123456789':
                        a3 += tmp[0]
                        tmp = tmp[1:]
                    else:
                        break
                a3 += ' (Velocity {}%)'.format(a3.split('m68e3q')[1])
            if line.startswith('o'):
                if 'end' in line:
                    oSub = False
                else:
                    oSub = True
        if cutComp or oSub:
            if cutComp:
                msg0 = _translate('HandlerClass', 'Cannot run from line while')
                msg1 = _translate('HandlerClass', 'cutter compensation is active')
            elif oSub:
                msg0 = _translate('HandlerClass', 'Cannot do run from line')
                msg1 = _translate('HandlerClass', 'inside a subroutine')
            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n{}'.format(head, msg0, msg1))
            self.rflActive = False
            self.set_run_button_state()
            self.startLine = 0
            return
        # show the dialog
        rFl = QDialog(self.w)
        rFl.setWindowTitle(_translate('HandlerClass', 'RUN FROM LINE'))
        l1 = QLabel(_translate('HandlerClass', 'USE LEADIN:'))
        l2 = QLabel(_translate('HandlerClass', 'LEADIN LENGTH:'))
        l3 = QLabel(_translate('HandlerClass', 'LEADIN ANGLE:'))
        l4 = QLabel('')
        use = QCheckBox()
        len = QDoubleSpinBox()
        ang = QDoubleSpinBox()
        buttons = QDialogButtonBox.Ok | QDialogButtonBox.Cancel
        buttonBox = QDialogButtonBox(buttons)
        buttonBox.accepted.connect(rFl.accept)
        buttonBox.rejected.connect(rFl.reject)
        buttonBox.button(QDialogButtonBox.Ok).setText(_translate('HandlerClass', 'LOAD'))
        buttonBox.button(QDialogButtonBox.Ok).setIcon(QIcon())
        buttonBox.button(QDialogButtonBox.Cancel).setText(_translate('HandlerClass', 'CANCEL'))
        buttonBox.button(QDialogButtonBox.Cancel).setIcon(QIcon())
        layout = QGridLayout()
        layout.addWidget(l1, 0, 0)
        layout.addWidget(l2, 1, 0)
        layout.addWidget(l3, 2, 0)
        layout.addWidget(l4, 3, 0)
        layout.addWidget(use, 0, 1)
        layout.addWidget(len, 1, 1)
        layout.addWidget(ang, 2, 1)
        layout.addWidget(buttonBox, 4, 0, 1, 2)
        rFl.setLayout(layout)
        l1.setAlignment(Qt.AlignRight | Qt.AlignBottom)
        l2.setAlignment(Qt.AlignRight | Qt.AlignBottom)
        l3.setAlignment(Qt.AlignRight | Qt.AlignBottom)
        if self.units == 'inch':
            len.setDecimals(2)
            len.setSingleStep(0.05)
            len.setSuffix(' inch')
            len.setMinimum(0.05)
        else:
            len.setDecimals(0)
            len.setSingleStep(1)
            len.setSuffix(' mm')
            len.setMinimum(1)
        ang.setDecimals(0)
        ang.setSingleStep(1)
        ang.setSuffix(' deg')
        ang.setRange(-359, 359)
        ang.setWrapping(True)
        result = rFl.exec_()
        # cancel from dialog
        if not result:
            self.rflActive = False
            self.set_run_button_state()
            self.startLine = 0
            self.w.gcode_display.setCursorPosition(0, 0)
            return
        # run from dialog
        for param in params:
            if param:
                newFile.append(param)
        scale = 1
        zMax = ''
        if self.unitsPerMm == 1:
            if g2 == 'g20':
                scale = 0.03937
                zMax = 'g53 g0z[[#<_ini[axis_z]max_limit> - 5] * 0.03937]'
            else:
                zMax = 'g53 g0z[#<_ini[axis_z]max_limit> - 5]'
        elif self.unitsPerMm == 0.03937:
            if g2 == 'g21':
                scale = 25.4
                zMax = 'g53 g0z[[#<_ini[axis_z]max_limit> * 25.4] - 5]'
            else:
                zMax = 'g53 g0z[#<_ini[axis_z]max_limit> - 0.02]'
        if g2:
            newFile.append(g2)
        if g4:
            newFile.append(g4)
        if g6:
            newFile.append(g6)
        if g9:
            newFile.append(g9)
        newFile.append('M52 P1')
        if d3:
            newFile.append(d3)
        if d2:
            newFile.append(d2)
        if a3:
            newFile.append(a3)
        if zMax:
            newFile.append(zMax)
        if material:
            newFile.append(material)
            if not '(o=' in material:
                newFile.append('m66p3l3q1')
        # don't scale feedrate, parameters should be set correctly in material file
        newFile.append('f#<_hal[plasmac.cut-feed-rate]>')
        xL = x
        yL = y
        try:
            if use.isChecked():
                if x[-1] == ']':
                    xL = '{}[[{}]+{:0.6f}]'.format(x[:1], x[1:], (len.value() * scale) * math.cos(math.radians(ang.value())))
                    yL = '{}[[{}]+{:0.6f}]'.format(y[:1], y[1:], (len.value() * scale) * math.sin(math.radians(ang.value())))
                else:
                    xL = float(x) + ((len.value() * scale) * math.cos(math.radians(ang.value())))
                    yL = float(y) + ((len.value() * scale) * math.sin(math.radians(ang.value())))
        except:
            msg0 = _translate('HandlerClass', 'Unable to calculate a leadin for this cut')
            msg1 = _translate('HandlerClass', 'Program will run from selected line with no leadin applied')
            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n{}'.format(head, msg0, msg1))
        if xL != x and yL != y:
            newFile.append('G0 X{} Y{}'.format(xL, yL))
            rflLead = [x, y]
        else:
            if x and y:
                newFile.append('G0 X{} Y{}'.format(x, y))
            elif x:
                newFile.append('G0 X{}'.format(x))
            elif y:
                newFile.append('G0 Y{}'.format(y))
            rflLead = None
        if rflSpindle:
            newFile.append(rflSpindle)
        if rflLead:
            newFile.append('G1 X{} Y{}'.format(rflLead[0], rflLead[1]))
        for line in outData:
            if outData.index(line) == 0 and (line.startswith('x') or line.startswith('y')):
                line = '{}{}'.format(code, line)
            elif line.startswith('m190'):
                mat = line.split('p')[1]
                if '(' in mat:
                    num = int(mat.split('(')[0])
                else:
                    num = int(mat)
                if num >= 1000000:
                    tmpMat = True
                    continue
            elif line.replace(' ','').startswith('m66p3') and tmpMat:
                tmpMat = False
                continue
            newFile.append(line.strip())
        rflFile = '{}rfl.ngc'.format(self.tmpPath)
        with open(rflFile, 'w') as outFile:
            for line in newFile:
                outFile.write('{}\n'.format(line))
        if ACTION.prefilter_path or self.lastLoadedProgram != 'None':
            self.preRflFile = ACTION.prefilter_path or self.lastLoadedProgram
        ACTION.OPEN_PROGRAM(rflFile)
        ACTION.prefilter_path = self.preRflFile
        self.set_run_button_state()
        txt0 = _translate('HandlerClass', 'RUN FROM LINE')
        txt1 = _translate('HandlerClass', 'CYCLE START')
        self.runText = '{}\n{}'.format(txt0, txt1)
        self.w.gcodegraphics.highlight_graphics(None)

    def get_rfl_pos(self, line, axisPos, axisLetter):
        maths = 0
        pos = ''
        done = False
        if line.startswith('(') or line.startswith(';'):
            return pos if pos else axisPos
        while len(line):
            if line[0] == ('('):
                break
            if not line[0] == axisLetter:
                line = line[1:]
            else:
                while 1:
                    line = line[1:]
                    if line[0] in '-.0123456789#':
                        pos += line[0]
                    elif line[0] == '[' or line[0] == '<':
                        pos += line[0]
                        maths += 1
                    elif (line[0] == ']' or line[0] == '>') and maths > 0:
                        pos += line[0]
                        maths -= 1
                    elif maths:
                        pos += line[0]
                    elif (pos and not maths) or line[0] == '(':
                        done = True
                        break
                    else:
                        if len(line) == 1: break
                        break
                    if len(line) == 1:
                        break
            if done:
                break
        return pos if pos else axisPos

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

    def run_critical_check(self):
        rcButtonList = []
        # halTogglePins format is: button name, run critical flag, button text
        for halpin in self.halTogglePins:
            if self.halTogglePins[halpin][1] and not hal.get_value(halpin):
                rcButtonList.append(self.halTogglePins[halpin][2].replace('\n', ' '))
        if rcButtonList:
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
            self.w.file_reload.setEnabled(False)
            self.w.file_open.setEnabled(False)
            self.autorepeat_keys(True)
            self.w.jog_frame.setEnabled(False)
        else:
            self.button_normal(self.w.file_edit.objectName())
            self.w.file_edit.setText(_translate('HandlerClass', 'EDIT'))
            self.w.file_reload.setEnabled(True)
            self.w.file_open.setEnabled(True)
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
            for n in range(self.w.main_tab_widget.count()):
                if n != 0 and (not self.probeTest or n != self.w.main_tab_widget.currentIndex()):
                     self.w.main_tab_widget.setTabEnabled(n, state)
            self.w.jog_slider.setEnabled(state)
            self.w.jogs_label.setEnabled(state)
            self.w.jog_slow.setEnabled(state)
            self.w.jogincrements.setEnabled(state)
            self.w.material_selector.setEnabled(state)
            if self.probeTest or self.torchPulse:
                self.w.jog_frame.setEnabled(state)

    def show_material_selector(self):
        self.w.material_selector.showPopup()

    def autorepeat_keys(self, state):
        if not self.iniFile.find('QTPLASMAC', 'AUTOREPEAT_ALL') == 'ENABLE':
            if state:
                ACTION.ENABLE_AUTOREPEAT_KEYS(' ')
            else:
                ACTION.DISABLE_AUTOREPEAT_KEYS(' ')


#########################################################################################################################
# TIMER FUNCTIONS #
#########################################################################################################################
    def startup_timeout(self):
        if STATUS.stat.estop:
            self.w.power.setEnabled(False)
        self.w.run.setEnabled(False)
        if self.frButton:
            self.w[self.frButton].setEnabled(False)
        self.w.pause.setEnabled(False)
        self.w.abort.setEnabled(False)

    def flasher_timeout(self):
        if STATUS.is_auto_paused():
            if self.w.pause.text() == '':
                self.w.pause.setText(_translate('HandlerClass', 'CYCLE RESUME'))
            else:
                self.w.pause.setText('')
        else:
            self.w.pause.setText(_translate('HandlerClass', 'CYCLE PAUSE'))
        text = _translate('HandlerClass', 'FEED')
        if self.w.feed_slider.value() != 100 and \
           self.w.feed_label.text() == '{}\n{:.0f}%'.format(text, STATUS.stat.feedrate * 100):
                self.w.feed_label.setText(' \n ')
        else:
            self.w.feed_label.setText('{}\n{:.0f}%'.format(text, STATUS.stat.feedrate * 100))
        text = _translate('HandlerClass', 'RAPID')
        if self.w.rapid_slider.value() != 100 and \
           self.w.rapid_label.text() == '{}\n{:.0f}%'.format(text, STATUS.stat.rapidrate * 100):
                self.w.rapid_label.setText(' \n ')
        else:
            self.w.rapid_label.setText('{}\n{:.0f}%'.format(text, STATUS.stat.rapidrate * 100))
        text = _translate('HandlerClass', 'JOG')
        if self.manualCut and text in self.w.jogs_label.text():
            self.w.jogs_label.setText(' \n ')
        else:
            self.w.jogs_label.setText('{}\n{:.0f}'.format(text, STATUS.get_jograte()))
        if self.heightOvr > 0.01 or self.heightOvr < -0.01:
            if self.w.height_ovr_label.text() == '':
                self.w.height_ovr_label.setText('{:.2f}'.format(self.heightOvr))
            else:
                self.w.height_ovr_label.setText('')
        else:
            self.w.height_ovr_label.setText('{:.2f}'.format(self.heightOvr))
        if self.manualCut:
            if self.w.run.text() == '':
                self.w.run.setText(_translate('HandlerClass', 'MANUAL CUT'))
            else:
                self.w.run.setText('')
        if self.startLine > 0:
            if not self.w.run.text().startswith(_translate('HandlerClass', 'RUN')):
                if self.w.run.text() == (''):
                    self.w.run.setText(self.runText)
                else:
                    self.w.run.setText('')
        elif not self.manualCut:
            self.w.run.setText(_translate('HandlerClass', 'CYCLE START'))
        if not self.w.pmx485_enable.isChecked():
            self.w.pmx485_label.setText('')
            self.pmx485LabelState = None
            self.w.pmx_stats_frame.hide()
        elif self.pmx485CommsError:
            if self.w.pmx485_label.text() == '':
                self.w.pmx485_label.setText(_translate('HandlerClass', 'COMMS ERROR'))
                self.pmx485LabelState = None
            else:
                self.w.pmx485_label.setText('')
                self.pmx485LabelState = None
        elif not self.pmx485LabelState:
            if self.w.pmx485_label.text() == '':
                self.w.pmx485_label.setText('Fault Code: {}'.format(self.pmx485FaultCode))
                self.pmx485LabelState = None
            else:
                self.w.pmx485_label.setText('')
                self.pmx485LabelState = None
        if self.framing and STATUS.is_interp_idle():
            self.framing = False
            ACTION.SET_MANUAL_MODE()
            self.laserOnPin.set(0)
            self.w.gcodegraphics.logger.clear()
        self.set_button_color()
        self.stats_update()

    def probe_timeout(self):
        if self.probeTime > 1:
            self.probeTime -= 1
            self.probeTimer.start(1000)
            self.w[self.ptButton].setText('{}'.format(self.probeTime))
        else:
            self.probe_test_stop()

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
            else:
                text0 = _translate('HandlerClass', 'TORCH')
                text1 = _translate('HandlerClass', 'ON')
                self.w[self.tpButton].setText('{}\n{}'.format(text0, text1))
        else:
            self.torchTimer.start(100)

    def pulse_timer_timeout(self):
        # halPulsePins format is: button name, pulse time, button text, remaining time
        active = False
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
        if not active:
            self.pulseTimer.stop()


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
        head = _translate('HandlerClass', 'USER BUTTON ERROR')
        for bNum in range(1,21):
            self.w['button_{}'.format(str(bNum))].setEnabled(False)
            bName = self.iniFile.find('QTPLASMAC', 'BUTTON_' + str(bNum) + '_NAME') or ''
            bCode = self.iniFile.find('QTPLASMAC', 'BUTTON_' + str(bNum) + '_CODE') or ''
            bNameDup = self.iniFile.find('QTPLASMAC', 'BUTTON_' + str(bNum) + '_NAME', 2)
            bCodeDup = self.iniFile.find('QTPLASMAC', 'BUTTON_' + str(bNum) + '_CODE', 2)
            if bNameDup or bCodeDup:
                msg0 = _translate('HandlerClass', 'is already assigned')
                msg1 = _translate('HandlerClass', 'Using first instance only of')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\nBUTTON_{} {}\n{} BUTTON_{}'.format(head, bNum, msg0, msg1, bNum))
            if (bCode and not bName) or (not bCode and bName):
                msg0 = _translate('HandlerClass', 'are both required')
                msg1 = _translate('HandlerClass', 'only one has been specified for')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\nCODE + NAME {}\n{} BUTTON_{}'.format(head, msg0, msg1, bNum))
                self.w['button_{}'.format(str(bNum))].setText('')
                self.iniButtonCodes.append('')
                continue
            if bCode == '':
                self.w['button_{}'.format(str(bNum))].setText('')
                self.iniButtonCodes.append('')
                continue
            code = bCode.lower().strip().split()[0]
            if code in singleCodes and code in iniButtonCodes:
                msg1 = _translate('HandlerClass', 'Duplicate code entry for')
                msg2 = _translate('HandlerClass', 'Using first instance only of')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} BUTTON_{} + BUTTON_{}\n{} {}'
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
                self.ccParm = self.iniFile.find('QTPLASMAC','BUTTON_' + str(bNum) + '_CODE').replace('change-consumables','').replace(' ','').lower() or None
                if self.ccParm != None and ('x' in self.ccParm or 'y' in self.ccParm) and 'f' in self.ccParm:
                    self.ccButton = 'button_{}'.format(str(bNum))
                    self.idleHomedList.append(self.ccButton)
                    self.pausedValidList.append(self.ccButton)
                    self.extChangeConsPin = self.h.newpin('ext_consumables', hal.HAL_BIT, hal.HAL_IN)
                    self.extChangeConsPin.value_changed.connect(lambda v:self.ext_change_consumables(v))
                else:
                    msg1 = _translate('HandlerClass', 'Check button code for invalid or missing arguments')
                    STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}'.format(head, msg0, bNum, msg1))
                    continue
            elif 'probe-test' in bCode:
                if len(bCode.split()) < 3:
                    if bCode.lower().replace('probe-test','').strip():
                        try:
                            self.ptTime = round(float(bCode.lower().replace('probe-test','').strip()))
                        except:
                            msg1 = _translate('HandlerClass', 'Check button code for invalid seconds argument')
                            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}'.format(head, msg0, bNum, msg1))
                            continue
                    else:
                        self.ptTime = 10
                    self.ptButton = 'button_{}'.format(str(bNum))
                    self.idleHomedList.append(self.ptButton)
                    self.probeText = self.w[self.ptButton].text()
                    self.extProbePin = self.h.newpin('ext_probe', hal.HAL_BIT, hal.HAL_IN)
                    self.extProbePin.value_changed.connect(lambda v:self.ext_probe_test(v))
                else:
                    msg1 = _translate('HandlerClass', 'Check button code for extra arguments')
                    STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}'.format(head, msg0, bNum, msg1))
                    continue
            elif 'torch-pulse' in bCode:
                if len(bCode.split()) < 3:
                    if bCode.lower().replace('torch-pulse','').strip():
                        try:
                            self.tpTime = round(float(bCode.lower().replace('torch-pulse','').strip()), 1)
                        except:
                            msg1 = _translate('HandlerClass', 'Check button code for invalid seconds argument')
                            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}'.format(head, msg0, bNum, msg1))
                            continue
                        self.tpTime = 3.0 if self.torchTime > 3.0 else self.tpTime
                    else:
                        self.tpTime = 1.0
                    self.tpButton = 'button_{}'.format(str(bNum))
                    self.idleOnList.append(self.tpButton)
                    self.pausedValidList.append(self.tpButton)
                    self.tpText = self.w[self.tpButton].text()
                    self.extPulsePin = self.h.newpin('ext_pulse', hal.HAL_BIT, hal.HAL_IN)
                    self.extPulsePin.value_changed.connect(lambda v:self.ext_torch_pulse(v))
                else:
                    msg1 = _translate('HandlerClass', 'Check button code for extra arguments')
                    STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}'.format(head, msg0, bNum, msg1))
                    continue
            elif 'ohmic-test' in bCode:
                self.otButton = 'button_{}'.format(str(bNum))
                self.idleOnList.append(self.otButton)
                self.pausedValidList.append(self.otButton)
                self.extOhmicPin = self.h.newpin('ext_ohmic', hal.HAL_BIT, hal.HAL_IN)
                self.extOhmicPin.value_changed.connect(lambda v:self.ext_ohmic_test(v))
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
                            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}'.format(head, msg0, bNum, msg1))
                            frButton = False
                            continue
                    else:
                        msg1 = _translate('HandlerClass', 'Check button code for invalid arguments')
                        STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}'.format(head, msg0, bNum, msg1))
                        continue
                if frButton:
                    self.frButton = 'button_{}'.format(str(bNum))
                    self.idleHomedList.append(self.frButton)
                    self.extFramingPin = self.h.newpin('ext_frame_job', hal.HAL_BIT, hal.HAL_IN)
                    self.extFramingPin.value_changed.connect(lambda v:self.ext_frame_job(v))
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
                head = _translate('HandlerClass', 'HAL PIN ERROR')
                if len(bCode.split()) == 3 and 'runcritical' in bCode.lower():
                    critical = True
                elif len(bCode.split()) == 2:
                    critical = False
                else:
                    head = _translate('HandlerClass', 'USER BUTTON ERROR')
                    msg1 = _translate('HandlerClass', 'Check button code for invalid arguments')
                    STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}'.format(head, msg0, bNum, msg1))
                    continue
                halpin = bCode.lower().split('toggle-halpin')[1].split(' ')[1].strip()
                excludedHalPins = ('plasmac.torch-pulse-start', 'plasmac.ohmic-test', \
                                'plasmac.probe-test', 'plasmac.consumable-change')
                if halpin in excludedHalPins:
                    msg1 = _translate('HandlerClass', 'HAL pin')
                    msg2 = _translate('HandlerClass', 'must be toggled')
                    msg3 = _translate('HandlerClass', 'using standard button code')
                    STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{} "{}" {}\n{}'.format(head, msg0, bNum, msg1, halpin, msg1, msg3))
                    continue
                else:
                    try:
                        pinstate = hal.get_value(halpin)
                        self.idleOnList.append('button_{}'.format(str(bNum)))
                    except:
                        msg1 = _translate('HandlerClass', 'HAL pin')
                        msg2 = _translate('HandlerClass', 'does not exist')
                        STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{} "{}" {}'.format(head, msg0, bNum, msg1, halpin, msg2))
                        continue
                # halTogglePins format is: button name, run critical flag, button text
                self.halTogglePins[halpin] = ['button_{}'.format(str(bNum)), critical, bLabel]
            elif 'pulse-halpin' in bCode:
                if len(bCode.split()) < 4:
                    try:
                        code, halpin, delay = bCode.lower().strip().split()
                    except:
                        try:
                            code, halpin = bCode.lower().strip().split()
                            delay = '1.0'
                        except:
                            head = _translate('HandlerClass', 'USER BUTTON ERROR')
                            msg1 = _translate('HandlerClass', 'Check button code for invalid arguments')
                            code = halpin = delay = ''
                            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}'.format(head, msg0, bNum, msg1))
                            continue
                    excludedHalPins = ('plasmac.torch-pulse-start', 'plasmac.ohmic-test', \
                                    'plasmac.probe-test', 'plasmac.consumable-change')
                    head = _translate('HandlerClass', 'HAL PIN ERROR')
                    if halpin in excludedHalPins:
                        msg1 = _translate('HandlerClass', 'HAL pin')
                        msg2 = _translate('HandlerClass', 'must be pulsed')
                        msg3 = _translate('HandlerClass', 'using standard button code')
                        STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{} "{}" {}\n{}'.format(head, msg0, bNum, msg1, halpin, msg1, msg3))
                        continue
                    else:
                        try:
                            pinstate = hal.get_value(halpin)
                            self.idleOnList.append('button_{}'.format(str(bNum)))
                        except:
                            msg1 = _translate('HandlerClass', 'HAL pin')
                            msg2 = _translate('HandlerClass', 'does not exist')
                            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{} "{}" {}'.format(head, msg0, bNum, msg1, halpin, msg2))
                            continue
                    # halPulsePins format is: button name, pulse time, button text, remaining time
                    try:
                        self.halPulsePins[halpin] = ['button_{}'.format(str(bNum)), float(delay), bLabel, 0.0]
                    except:
                        head = _translate('HandlerClass', 'USER BUTTON ERROR')
                        msg1 = _translate('HandlerClass', 'Check button code for invalid seconds argument')
                        STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}'.format(head, msg0, bNum, msg1))
                        continue
                else:
                    head = _translate('HandlerClass', 'USER BUTTON ERROR')
                    msg1 = _translate('HandlerClass', 'Check button code for invalid arguments')
                    STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}'.format(head, msg0, bNum, msg1))
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
                        reply = Popen('which {}'.format(cmd), stdout=PIPE, stderr=PIPE, shell=True).communicate()[0]
                        if not reply:
                            head = _translate('HandlerClass', 'EXTERNAL CODE ERROR')
                            msg1 = _translate('HandlerClass', 'External command')
                            msg2 = _translate('HandlerClass', 'does not exist')
                            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{} "{}" {}'.format(head, msg0, bNum, msg1, cmd, msg2))
                        else:
                            self.estopOnList.append('button_{}'.format(str(bNum)))
                    else:
                        head = _translate('HandlerClass', 'CODE ERROR')
                        msg1 = self.w['button_{}'.format(str(bNum))].text().replace('\n',' ')
                        STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}: "{}"'.format(head, msg0, bNum, msg1, command))
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
            self.w.gcode_progress.setValue(0)
            if self.fileOpened == True:
                self.file_reload_clicked()
        elif 'load' in commands.lower():
            lFile = '{}/{}'.format(self.programPrefix, commands.split('load', 1)[1].strip())
            self.w.gcode_progress.setValue(0)
            ACTION.OPEN_PROGRAM(lFile)
        elif 'toggle-halpin' in commands.lower():
            halpin = commands.lower().split('toggle-halpin')[1].split(' ')[1].strip()
            try:
                if halpin in self.halPulsePins and self.halPulsePins[halpin][3] > 0.05:
                    self.halPulsePins[halpin][3] = 0.0
                else:
                    self.invert_pin_state(halpin)
            except:
                head = _translate('HandlerClass', 'HAL PIN ERROR')
                msg0 = _translate('HandlerClass', 'Invalid code for user button')
                msg1 = _translate('HandlerClass', 'Failed to toggle HAL pin')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{} "{}"'.format(head, msg0, bNum, msg1, halpin))
        elif 'pulse-halpin' in commands.lower():
            head = _translate('HandlerClass', 'HAL PIN ERROR')
            msg1 = _translate('HandlerClass', 'Failed to pulse HAL pin')
            try:
                code, halpin, delay = commands.lower().strip().split()
            except:
                try:
                    code, halpin = commands.lower().strip().split()
                    delay = '1.0'
                except:
                    msg0 = _translate('HandlerClass', 'Unknown error for user button')
                    STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{} "{}"'.format(head, msg0, bNum, msg1, halpin))
                    return
            # halPulsePins format is: button name, pulse time, button text, remaining time
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
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{} "{}"'.format(head, msg0, bNum, msg1, halpin))
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
                buttonList = []
                for button in self.idleHomedList:
                    if button != self.ovButton:
                        buttonList.append(button)
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
                self.w.gcode_progress.setValue(0)
                ACTION.OPEN_PROGRAM(latest)
            except:
                head = _translate('HandlerClass', 'FILE ERROR')
                msg0 = _translate('HandlerClass', 'Cannot open latest file from user button')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}'.format(head, msg0, bNum))
        else:
            for command in commands.split('\\'):
                command = command.strip()
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
                    if command.lower().replace(' ', '').startswith('g10l20'):
                        self.file_reload_clicked()
                elif command and command[0] == '%':
                    command = command.lstrip('%').lstrip() + '&'
                    msg = Popen(command, stdout=PIPE, stderr=PIPE, shell=True)
                else:
                    head = _translate('HandlerClass', 'CODE ERROR')
                    msg0 = _translate('HandlerClass', 'Invalid code for user button')
                    msg1 = self.w['button_{}'.format(str(bNum))].text().replace('\n',' ')
                    STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}\n{}: "{}"'.format(head, msg0, bNum, msg1, command))

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

    def ext_kerf_cross_enable_changed(self, state):
        if (state):
            self.w.kerfcross_enable.setChecked(not (self.w.kerfcross_enable.isChecked()))

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
        if hal.get_value('axis.x.eoffset-counts') or hal.get_value('axis.y.eoffset-counts'):
            hal.set_p('plasmac.consumable-change', '0')
            hal.set_p('plasmac.x-offset', '0')
            hal.set_p('plasmac.y-offset', '0')
            self.button_normal(self.ccButton)
            self.w[self.ccButton].setEnabled(False)
        else:
            self.consumable_change_setup()
            if self.ccFeed == 'None' or self.ccFeed < 1:
                head = _translate('HandlerClass', 'USER BUTTON ERROR')
                msg0 = _translate('HandlerClass', 'Invalid feed rate for consumable change')
                msg1 = _translate('HandlerClass', 'check .ini file settings')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n{}\nBUTTON_{}_CODE'.format(head, msg0, msg1, str(button)))
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
            else:
                self.probe_test_stop()

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
            else:
                self.torchTimer.stop()
                self.torchTime = 0.0
                self.torch_pulse_states(True)
        else:
            hal.set_p('plasmac.torch-pulse-start','0')
            if self.torchTime == 0:
                self.torch_pulse_states(True)

    def torch_pulse_states(self, state):
        self.set_tab_jog_states(state)
        if not STATUS.is_auto_paused():
            self.set_buttons_state([self.idleList, self.idleOnList, self.idleHomedList], state)
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
        buttonList = []
        for button in self.idleOnList:
            if button != self.otButton:
                buttonList.append(button)
        if not STATUS.is_auto_paused():
            if self.w.gcode_display.lines() > 1:
                self.w.run.setEnabled(not state)
            self.set_buttons_state([self.idleList, buttonList, self.idleHomedList], not state)

    def ext_frame_job(self, state):
        if self.frButton and self.w[self.frButton].isEnabled():
            self.frame_job(state)

    def frame_job(self, state):
        if self.gcodeProps and state:
            self.framing = True
            self.w.run.setEnabled(False)
            msgList, units, xMin, yMin, xMax, yMax = self.bounds_check('framing', self.laserOffsetX, self.laserOffsetY)
            if self.boundsError['framing']:
                head = _translate('HandlerClass', 'AXIS LIMIT ERROR')
                msgs = ''
                msg1 = _translate('HandlerClass', 'due to laser offset')
                for n in range(0, len(msgList), 3):
                    if msgList[n + 1] == 'MAX':
                        msg0 = _translate('HandlerClass', 'move would exceed the maximum limit by')
                    else:
                        msg0 = _translate('HandlerClass', 'move would exceed the minimum limit by')
                    msgs += '{} {} {}{} {}\n'.format(msgList[n], msg0, msgList[n + 2], units, msg1)
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}'.format(head, msgs))
                self.framing = False
                self.w.run.setEnabled(True)
                self.boundsError['framing'] = False
                return
            if not self.frFeed:
                feed = float(self.w.cut_feed_rate.text())
            else:
                feed = self.frFeed
            zHeight = self.zMax - (hal.get_value('plasmac.max-offset') * self.unitsPerMm)
            if STATUS.is_on_and_idle() and STATUS.is_all_homed():
                self.laserOnPin.set(1)
                ACTION.CALL_MDI_WAIT('G64 P{:0.3}'.format(0.25 * self.unitsPerMm))
                if self.defaultZ:
                    ACTION.CALL_MDI('G53 G0 Z{}'.format(zHeight))
                ACTION.CALL_MDI('G53 G0 X{} Y{} F{}'.format(xMin, yMin, feed))
                ACTION.CALL_MDI('G53 G1 Y{} F{}'.format(yMax, feed))
                ACTION.CALL_MDI('G53 G1 X{} F{}'.format(xMax, feed))
                ACTION.CALL_MDI('G53 G1 Y{} F{}'.format(yMin, feed))
                ACTION.CALL_MDI('G53 G1 X{} F{}'.format(xMin, feed))

    def single_cut(self):
        self.set_buttons_state([self.idleList, self.idleOnList, self.idleHomedList], False)
        sC = QDialog(self.w)
        sC.setWindowTitle(_translate('HandlerClass', 'SINGLE CUT'))
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
        xLength.setValue(self.w.PREFS_.getpref('X length', 0.0, float, 'SINGLE CUT'))
        yLength.setValue(self.w.PREFS_.getpref('Y length', 0.0, float, 'SINGLE CUT'))
        result = sC.exec_()
        if not result:
            self.set_buttons_state([self.idleList, self.idleOnList, self.idleHomedList], True)
            return
        self.w.PREFS_.putpref('X length', xLength.value(), float, 'SINGLE CUT')
        self.w.PREFS_.putpref('Y length', yLength.value(), float, 'SINGLE CUT')
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
        elif STATUS.machine_is_on() and STATUS.is_all_homed() and STATUS.is_interp_idle():
            self.manualCut = True
            self.set_mc_states(False)
            self.w.abort.setEnabled(True)
            self.set_buttons_state([self.idleList, self.idleOnList, self.idleHomedList], False)
            if self.mcButton:
                self.w[self.mcButton].setEnabled(True)
                self.button_active(self.mcButton)
            ACTION.SET_SPINDLE_ROTATION(1 ,1 , 0)
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
            head = _translate('HandlerClass', 'VIRTUAL KB ERROR')
            msg0  = _translate('HandlerClass', '"onboard" virtual keyboard is not installed')
            msg1 = _translate('HandlerClass', 'some keyboard functions are not available')
            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n{}'.format(head, msg0, msg1))
            return
        try:
            cmd = 'gsettings get org.onboard.window.landscape width'
            self.obWidth = Popen(cmd, stdout=PIPE, stderr=PIPE, shell=True).communicate()[0].strip()
            cmd = 'gsettings get org.onboard.window.landscape height'
            self.obHeight = Popen(cmd, stdout=PIPE, stderr=PIPE, shell=True).communicate()[0].strip()
            cmd = 'gsettings get org.onboard layout'
            layout = Popen(cmd, stdout=PIPE, stderr=PIPE, shell=True).communicate()[0].strip()
            if '/numpad' in layout or '/keyboard' in layout:
                self.obLayout = 'compact'
            else:
                self.obLayout = layout
        except:
            self.obWidth = '700'
            self.obHeight = '300'
            self.obLayout = 'compact'

    def vkb_show(self, numpad=False):
        if os.path.isfile('/usr/bin/onboard'):
            if self.w.chk_soft_keyboard.isChecked():
                w = '240' if numpad else '740'
                h = '240'
                l = 'numpad' if numpad else 'keyboard'
                self.vkb_hide(True)
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
            Popen('gsettings set org.onboard layout {}'.format(l), stdout=PIPE, shell=True)
            Popen('gsettings set org.onboard.window.landscape width {}'.format(int(w)-1), stdout=PIPE, shell=True)
            Popen('gsettings set org.onboard.window.landscape height {}'.format(int(h)-1), stdout=PIPE, shell=True)
            time.sleep(0.25)
            Popen('gsettings set org.onboard layout {}'.format(l), stdout=PIPE, shell=True)
            Popen('gsettings set org.onboard.window.landscape width {}'.format(w), stdout=PIPE, shell=True)
            Popen('gsettings set org.onboard.window.landscape height {}'.format(h), stdout=PIPE, shell=True)
#            time.sleep(0.5)


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
        while(1):
            valid, num = self.dialog_input(head, '{}:'.format(msgs), btn1, btn2)
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
        while(1):
            valid, nam = self.dialog_input(head, msg1, btn1, btn2)
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
        inFile = open(self.tmpMaterialFile, 'r')
        outFile = open('{}'.format(self.materialFile), 'w')
        written = False
        while 1:
            line = inFile.readline()
            if not written:
                if not line or \
                   (line.startswith('[MATERIAL_NUMBER_') and \
                   num < int(line.strip().split('NUMBER_')[1].rstrip(']'))):
                    outFile.write('[MATERIAL_NUMBER_{}]  \n'.format(num))
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
        inFile.close()
        outFile.close()
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
        while(1):
            valid, num = self.dialog_input(head, '{}:'.format(msgs), btn1, btn2)
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
        if not self.dialog_show_yesno(QMessageBox.Question, '{}'.format(head), '{} #{}?\n'.format(msg0, num)):
            return
        COPY(self.materialFile, self.tmpMaterialFile)
        inFile = open(self.tmpMaterialFile, 'r')
        outFile = open('{}'.format(self.materialFile), 'w')
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
        outFile.close()
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
        self.overlay.setText(self.get_overlay_text(False))
        self.overlayConv.setText(self.get_overlay_text(True))

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
        head = _translate('HandlerClass', 'MATERIALS ERROR')
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
            t_item = 0
            with open(self.tmpMaterialFileGCode, 'r') as f_in:
                for line in f_in:
                    if line.startswith('name'):
                        t_name = line.split('=')[1].strip()
                    if line.startswith('kerf-width'):
                        k_width = float(line.split('=')[1].strip())
                    elif line.startswith('pierce-height'):
                        p_height = float(line.split('=')[1].strip())
                    elif line.startswith('pierce-delay'):
                        p_delay = float(line.split('=')[1].strip())
                    elif line.startswith('puddle-jump-height'):
                        pj_height = float(line.split('=')[1].strip())
                    elif line.startswith('puddle-jump-delay'):
                        pj_delay = float(line.split('=')[1].strip())
                    elif line.startswith('cut-height'):
                        c_height = float(line.split('=')[1].strip())
                    elif line.startswith('cut-feed-rate'):
                        c_speed = float(line.split('=')[1].strip())
                    elif line.startswith('cut-amps'):
                        c_amps = float(line.split('=')[1].strip())
                    elif line.startswith('cut-volts'):
                        c_volts = float(line.split('=')[1].strip())
                    elif line.startswith('pause-at-end'):
                        pause = float(line.split('=')[1].strip())
                    elif line.startswith('gas-pressure'):
                        g_press = float(line.split('=')[1].strip())
                    elif line.startswith('cut-mode'):
                        c_mode = float(line.split('=')[1].strip())
            self.write_materials(halpin, t_name, k_width, p_height, \
                                 p_delay, pj_height, pj_delay, c_height, c_speed, \
                                 c_amps, c_volts, pause, g_press, c_mode, 0)
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
        inFile = open(self.tmpMaterialFile, 'r')
        outFile = open('{}'.format(self.materialFile), 'w')
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
        inFile.close()
        outFile.close()
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
        head = _translate('HandlerClass', 'MATERIALS ERROR')
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
                            newMaterial = True
                            if not firstpass:
                                self.write_materials(t_number,t_name,k_width,p_height,p_delay,pj_height,pj_delay,c_height,c_speed,c_amps,c_volts,pause,g_press,c_mode,t_item)
                                for item in required:
                                    if item not in received:
                                        msg0 = _translate('HandlerClass', 'is missing from Material')
                                        STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} {} #{}'.format(head, item, msg0, t_number))
                            firstpass = False
                            t_number = int(line.rsplit('_', 1)[1].strip().strip(']'))
                            self.materialNumList.append(t_number)
                            t_name = k_width = p_height = p_delay = pj_height = pj_delay = c_height = c_speed = c_amps = c_volts =  pause = g_press = c_mode = 0.0
                            t_item += 1
                            received = []
                        else:
                            msg0 = _translate('HandlerClass', 'Material number')
                            msg1 = _translate('HandlerClass', 'is invalid')
                            msg2 = _translate('HandlerClass', 'Material numbers need to be less than 1000000')
                            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{} {}{}'.format(head, msg0, msg1, matnum, msg2))
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
                            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}'.format(head, msg0, t_number))
                    elif line.startswith('PIERCE_DELAY'):
                        received.append('PIERCE_DELAY')
                        if line.split('=')[1].strip():
                            p_delay = float(line.split('=')[1].strip())
                        else:
                            msg0 = _translate('HandlerClass', 'No value for PIERCE_DELAY in Material')
                            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}'.format(head, msg0, t_number))
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
                            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}'.format(head, msg0, t_number))
                    elif line.startswith('CUT_SPEED'):
                        received.append('CUT_SPEED')
                        if line.split('=')[1].strip():
                            c_speed = float(line.split('=')[1].strip())
                        else:
                            msg0 = _translate('HandlerClass', 'No value for CUT_SPEED in Material')
                            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{}'.format(head, msg0, t_number))
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
                    msg0 = _translate('Material file processing was aborted')
                    msg1 += _translate('The following line in the material file')
                    msg2 += _translate('contains an erroneous character')
                    msg3 += _translate('Fix the line and reload the material file')
                    STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n{}\n{}:\n{}{}'.format(head, msg0, msg1, msg2, line, msg3))
                    material_error = True
                    break
            if not firstpass and not material_error:
                self.write_materials(t_number,t_name,k_width,p_height,p_delay,pj_height,pj_delay,c_height,c_speed,c_amps,c_volts,pause,g_press,c_mode,t_item)
                for item in required:
                    if item not in received:
                        msg0 = _translate('HandlerClass', 'is missing from Material')
                        STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} {} #{}'.format(head, item, msg0, t_number))
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
                    '#[MATERIAL_NUMBER_1]  \n'\
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
            head = _translate('HandlerClass', 'MATERIALS SETUP')
            msg0 = _translate('HandlerClass', 'Creating New Material File')
            STATUS.emit('error', linuxcnc.OPERATOR_DISPLAY, '{}:\n{}: {}'.format(head, msg0, self.materialFile))

    def material_exists(self, material):
        if int(material) in self.materialList:
            return True
        else:
            if self.autoChange:
                self.materialChangePin.set(-1)
                self.materialChangeNumberPin.set(int(self.w.materials_box.currentText().split(': ', 1)[0]))
                head = _translate('HandlerClass', 'MATERIALS ERROR')
                msg0 = _translate('HandlerClass', 'Material #')
                msg1 = _translate('HandlerClass', 'not in material list')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{} #{} {}'.format(head, msg0, int(material),msg1))
            return False

    def save_default_material(self):
        self.w.PREFS_.putpref('Kerf width', self.w.kerf_width.value(), float, 'DEFAULT MATERIAL')
        self.w.PREFS_.putpref('Pierce height',self.w.pierce_height.value(), float, 'DEFAULT MATERIAL')
        self.w.PREFS_.putpref('Pierce delay',self.w.pierce_delay.value(), float, 'DEFAULT MATERIAL')
        self.w.PREFS_.putpref('Puddle jump height',self.w.puddle_jump_height.value(), float, 'DEFAULT MATERIAL')
        self.w.PREFS_.putpref('Puddle jump delay',self.w.puddle_jump_delay.value(), float, 'DEFAULT MATERIAL')
        self.w.PREFS_.putpref('Cut height',self.w.cut_height.value(), float, 'DEFAULT MATERIAL')
        self.w.PREFS_.putpref('Cut feed rate',self.w.cut_feed_rate.value(), float, 'DEFAULT MATERIAL')
        self.w.PREFS_.putpref('Cut amps',self.w.cut_amps.value(), float, 'DEFAULT MATERIAL')
        self.w.PREFS_.putpref('Cut volts',self.w.cut_volts.value(), float, 'DEFAULT MATERIAL')
        self.w.PREFS_.putpref('Pause at end',self.w.pause_at_end.value(), float, 'DEFAULT MATERIAL')
        self.w.PREFS_.putpref('Gas pressure',self.w.gas_pressure.value(), float, 'DEFAULT MATERIAL')
        self.w.PREFS_.putpref('Cut mode',self.w.cut_mode.value(), float, 'DEFAULT MATERIAL')

    def load_default_material(self):
        self.write_materials( \
                0, 'DEFAULT' , \
                self.w.PREFS_.getpref('Kerf width', round(1 * self.unitsPerMm, 2), float, 'DEFAULT MATERIAL'), \
                self.w.PREFS_.getpref('Pierce height', round(3 * self.unitsPerMm, 2), float, 'DEFAULT MATERIAL'), \
                self.w.PREFS_.getpref('Pierce delay', 0, float, 'DEFAULT MATERIAL'), \
                self.w.PREFS_.getpref('Puddle jump height', 0, float, 'DEFAULT MATERIAL'), \
                self.w.PREFS_.getpref('Puddle jump delay', 0, float, 'DEFAULT MATERIAL'), \
                self.w.PREFS_.getpref('Cut height', round(1 * self.unitsPerMm, 2), float, 'DEFAULT MATERIAL'), \
                self.w.PREFS_.getpref('Cut feed rate', round(4000 * self.unitsPerMm, 0), float, 'DEFAULT MATERIAL'), \
                self.w.PREFS_.getpref('Cut amps', 45, float, 'DEFAULT MATERIAL'), \
                self.w.PREFS_.getpref('Cut volts', 99, float, 'DEFAULT MATERIAL'), \
                self.w.PREFS_.getpref('Pause at end', 0, float, 'DEFAULT MATERIAL'), \
                self.w.PREFS_.getpref('Gas pressure', 0, float, 'DEFAULT MATERIAL'), \
                self.w.PREFS_.getpref('Cut mode', 1, float, 'DEFAULT MATERIAL'),\
                0)


#########################################################################################################################
# CAMERA AND LASER FUNCTIONS #
#########################################################################################################################
    def camera_pressed(self):
        self.w.camview.rotation = STATUS.stat.rotation_xy
        if self.w.preview_stack.currentIndex() != 3:
            self.w.preview_stack.setCurrentIndex(3)
            self.overlay.hide()
            self.button_active('camera')
            self.cameraOn = True
        else:
            self.w.preview_stack.setCurrentIndex(0)
            if self.w.chk_overlay.isChecked():
                self.overlay.show()
            self.button_normal('camera')
            self.cameraOn = False
            ACTION.SET_MANUAL_MODE()
            self.vkb_hide()

    def laser_pressed(self):
        if self.laserButtonState == 'laser':
            self.w.laser.setText(_translate('HandlerClass', 'MARK\nEDGE'))
            self.laserButtonState = 'markedge'
            self.laserOnPin.set(1)
            return
        elif self.laserButtonState == 'setorigin':
            self.laserOnPin.set(0)
        self.laserButtonState = self.sheet_align(self.laserButtonState, self.w.laser, self.laserOffsetX, self.laserOffsetY)

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
            if self.fileOpened == True:
                self.file_reload_clicked()
                self.w.gcodegraphics.logger.clear()
            self.w.cam_goto.setEnabled(True)
            ACTION.SET_MANUAL_MODE()
        return button_state

    def cam_mark_pressed(self):
        self.camButtonState = self.sheet_align(self.camButtonState, self.w.cam_mark, self.camOffsetX, self.camOffsetY)

    def cam_goto_pressed(self):
        ACTION.CALL_MDI('G0 X0 Y0')
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
    def pierce_count_changed(self):
        if self.plasmacStatePin.get() >= self.TORCH_ON:
            self.PIERCE_COUNT += 1
            self.pierce_count += 1
            if self.w.torch_enable.isChecked():
                self.w.pierce_count_t.setText('{:d}'.format(self.PIERCE_COUNT))
            self.w.pierce_count.setText('{:d}'.format(self.pierce_count))

    def cut_length_changed(self, value):
        if value:
            self.thisCutLength = value
            if self.unitsPerMm == 1:
                if self.w.torch_enable.isChecked():
                    self.w.cut_length_t.setText('{:.2f}'.format((self.CUT_LENGTH + self.thisCutLength) * 0.001))
                self.w.cut_length.setText('{:.2f}'.format((self.cut_length + self.thisCutLength) * 0.001))
            else:
                if self.w.torch_enable.isChecked():
                    self.w.cut_length_t.setText('{:.2f}'.format(self.CUT_LENGTH + self.thisCutLength))
                self.w.cut_length.setText('{:.2f}'.format(self.cut_length + self.thisCutLength))
        else:
            if self.w.torch_enable.isChecked():
                self.CUT_LENGTH += self.thisCutLength
            self.cut_length += self.thisCutLength
            if self.unitsPerMm == 1:
                self.w.cut_length_t.setText('{:.2f}'.format(self.CUT_LENGTH * 0.001))
            else:
                self.w.cut_length_t.setText('{:.2f}'.format(self.CUT_LENGTH))
            self.thisCutLength = 0

    def cut_time_changed(self, value):
        if value:
            self.thisCutTime = value
            if self.w.torch_enable.isChecked():
                self.display_time('cut_time_t', self.CUT_TIME + self.thisCutTime)
            self.display_time('cut_time', self.cut_time + self.thisCutTime)
        else:
            if self.w.torch_enable.isChecked():
                self.CUT_TIME += self.thisCutTime
            self.cut_time += self.thisCutTime
            self.display_time('cut_time_t', self.CUT_TIME)
            thisCutTime = 0

    def torch_on_changed(self, value):
        if value and not self.torchOn:
            self.torchStart = time.time()
        elif not value and self.torchOn:
            self.TORCH_TIME += (time.time() - self.torchStart)
            self.torch_time += (time.time() - self.torchStart)
            self.display_time('torch_time_t', self.TORCH_TIME)
        self.torchOn = value

    def stats_run(self):
        if not self.progRun:
            self.clear_job_values()
            self.runStart = time.time()
            self.progRun = True

    def stats_idle(self):
        if self.progRun:
            if self.w.torch_enable.isChecked():
                self.RUN_TIME += (time.time() - self.runStart)
            self.display_time('run_time_t', self.RUN_TIME)
            self.progRun = False
            self.stats_save()

    def motion_type_changed(self, value):
        if value == 1 and self.oldMotionType != 1:
            self.rapidStart = time.time()
            self.rapidOn = True
        elif value != 1 and self.oldMotionType == 1:
            if self.w.torch_enable.isChecked():
                self.RAPID_TIME += (time.time() - self.rapidStart)
            self.rapid_time += (time.time() - self.rapidStart)
            self.display_time('rapid_time_t', self.RAPID_TIME)
            self.rapidOn = False
        elif value == 0 and STATUS.is_mdi_mode():
            ACTION.SET_MANUAL_MODE()
        self.oldMotionType = value

    def stats_state_changed(self, state):
        if state == self.PROBE_HEIGHT and self.oldState == self.IDLE:
            self.probeStart = time.time()
            self.probeOn = True
        elif (state > self.ZERO_HEIGHT or state == self.IDLE) and not hal.get_value('plasmac.x-offset-counts') and not hal.get_value('plasmac.y-offset-counts') and self.probeOn:
            if self.w.torch_enable.isChecked():
                self.PROBE_TIME += (time.time() - self.probeStart)
            self.probe_time += (time.time() - self.probeStart)
            self.display_time('probe_time_t', self.PROBE_TIME)
            self.probeOn = False
        self.oldState = state

    def pierce_reset(self):
        self.PIERCE_COUNT = 0
        self.w.pierce_count_t.setText('{:d}'.format(self.PIERCE_COUNT))
        self.stats_save()

    def cut_length_reset(self):
        self.CUT_LENGTH = 0.0
        self.w.cut_length_t.setText('{:.2f}'.format(self.CUT_LENGTH))
        self.stats_save()

    def cut_time_reset(self):
        self.CUT_TIME = 0.0
        self.display_time('cut_time_t', self.CUT_TIME)
        self.stats_save()

    def torch_time_reset(self):
        self.TORCH_TIME = 0.0
        self.display_time('torch_time_t', self.TORCH_TIME)
        self.stats_save()

    def run_time_reset(self):
        self.RUN_TIME = 0.0
        self.display_time('run_time_t', self.RUN_TIME)
        self.stats_save()

    def rapid_time_reset(self):
        self.RAPID_TIME = 0.0
        self.display_time('rapid_time_t', self.RAPID_TIME)
        self.stats_save()

    def probe_time_reset(self):
        self.PROBE_TIME = 0.0
        self.display_time('probe_time_t', self.PROBE_TIME)
        self.stats_save()

    def clear_job_values(self):
        self.pierce_count = 0
        self.w.pierce_count.setText('{:d}'.format(0))
        self.cut_length = 0
        self.w.cut_length.setText('{:.2f}'.format(0))
        self.cut_time = 0
        self.display_time('cut_time', 0)
        self.torch_time = 0
        self.display_time('torch_time', 0)
        self.run_time = 0
        self.display_time('run_time', 0)
        self.rapid_time = 0
        self.display_time('rapid_time', 0)
        self.probe_time = 0
        self.display_time('probe_time', 0)
        self.torchOn = False
        self.progRun = False
        self.rapidOn = False
        self.probeOn = False

    def all_reset(self):
        self.pierce_reset()
        self.cut_length_reset()
        self.cut_time_reset()
        self.torch_time_reset()
        self.run_time_reset()
        self.rapid_time_reset()
        self.probe_time_reset()
        self.stats_save()

    def display_time(self, widget, time):
        m, s = divmod(time, 60)
        h, m = divmod(m, 60)
        self.w[widget].setText('{:.0f}:{:02.0f}:{:02.0f}'.format(h,m,s))

    def stats_update(self):
        if self.torchOn:
            self.display_time('torch_time_t', self.TORCH_TIME + (time.time() - self.torchStart))
            self.display_time('torch_time', self.torch_time + (time.time() - self.torchStart))
        if self.progRun:
            if self.w.torch_enable.isChecked():
                self.display_time('run_time_t', self.RUN_TIME + (time.time() - self.runStart))
            self.display_time('run_time', time.time() - self.runStart)
        if self.rapidOn:
            if self.w.torch_enable.isChecked():
                self.display_time('rapid_time_t', self.RAPID_TIME + (time.time() - self.rapidStart))
            self.display_time('rapid_time', self.rapid_time + (time.time() - self.rapidStart))
        if self.probeOn:
            if self.w.torch_enable.isChecked():
                self.display_time('probe_time_t', self.PROBE_TIME + (time.time() - self.probeStart))
            self.display_time('probe_time', self.probe_time + (time.time() - self.probeStart))

    def stats_save(self):
        self.w.PREFS_.putpref('Pierce count', self.PIERCE_COUNT , int,'STATISTICS')
        self.w.PREFS_.putpref('Cut length', self.CUT_LENGTH , float,'STATISTICS')
        self.w.PREFS_.putpref('Cut time', self.CUT_TIME , float,'STATISTICS')
        self.w.PREFS_.putpref('Torch on time', self.TORCH_TIME , float,'STATISTICS')
        self.w.PREFS_.putpref('Program run time', self.RUN_TIME , float,'STATISTICS')
        self.w.PREFS_.putpref('Rapid time', self.RAPID_TIME , float,'STATISTICS')
        self.w.PREFS_.putpref('Probe time', self.PROBE_TIME , float,'STATISTICS')

    def statistics_init(self):
        # get saved prefs
        self.PIERCE_COUNT = self.w.PREFS_.getpref('Pierce count', 0 , int,'STATISTICS')
        self.CUT_LENGTH = self.w.PREFS_.getpref('Cut length', 0 , float,'STATISTICS')
        self.CUT_TIME = self.w.PREFS_.getpref('Cut time', 0 , float,'STATISTICS')
        self.TORCH_TIME = self.w.PREFS_.getpref('Torch on time', 0 , float,'STATISTICS')
        self.RUN_TIME = self.w.PREFS_.getpref('Program run time', 0 , float,'STATISTICS')
        self.RAPID_TIME = self.w.PREFS_.getpref('Rapid time', 0 , float,'STATISTICS')
        self.PROBE_TIME = self.w.PREFS_.getpref('Probe time', 0 , float,'STATISTICS')
        # set variables
        self.oldState      = 0
        self.oldMotionType = 0
        self.pierce_count  = 0
        self.cut_length    = 0
        self.thisCutLength = 0
        self.cut_time      = 0.0
        self.thisCutTime   = 0.0
        self.torch_time    = 0.0
        self.rapid_time    = 0.0
        self.probe_time    = 0.0
        self.w.pierce_count_t.setText('{:d}'.format(self.PIERCE_COUNT))
        self.w.pierce_count.setText('{:d}'.format(0))
        if self.unitsPerMm == 1:
            self.w.cut_length_t.setText('{:0.2f}'.format(self.CUT_LENGTH * 0.001))
            self.w.cut_length_label.setText(_translate('HandlerClass', 'CUT LENGTH (Metres)'))
        else:
            self.w.cut_length_t.setText('{:0.2f}'.format(self.CUT_LENGTH))
            self.w.cut_length_label.setText(_translate('HandlerClass', 'CUT LENGTH (Inches)'))
        self.w.cut_length.setText('0.00')
        self.display_time('cut_time_t', self.CUT_TIME)
        self.display_time('torch_time_t', self.TORCH_TIME)
        self.display_time('run_time_t', self.RUN_TIME)
        self.display_time('rapid_time_t', self.RAPID_TIME)
        self.display_time('probe_time_t', self.PROBE_TIME)


#########################################################################################################################
# POWERMAX COMMUNICATIONS FUNCTIONS #
#########################################################################################################################
    def pmx485_timeout(self):
        self.pmx485CommsTimer.stop()
        self.w.pmx485_label.setText(_translate('HandlerClass', 'COMMS ERROR'))
        self.pmx485LabelState = None
        self.w.pmx_stats_frame.hide()
        self.pmx485CommsError = True
        self.pmx485Connected = False
        self.pmx485RetryTimer.start(3000)

    def pmx485_check(self):
        if self.iniFile.find('QTPLASMAC', 'PM_PORT'):
            self.pmx485Exists = True
            self.pmx485CommsError = False
            if not hal.component_exists('pmx485'):
                head = _translate('HandlerClass', 'COMMS ERROR')
                msg0 = _translate('HandlerClass', 'PMX485 component is not loaded,')
                msg1 = _translate('HandlerClass', 'Powermax communications are not available')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n{}'.format(head, msg0, msg1))
                return
            self.w.pmx485Status = False
            self.w.pmx485_enable.stateChanged.connect(lambda w:self.pmx485_enable_changed(self.w.pmx485_enable.isChecked()))
            self.pmx485StatusPin.value_changed.connect(lambda w:self.pmx485_status_changed(w))
            self.pmx485ModePin.value_changed.connect(self.pmx485_mode_changed)
            self.pmx485FaultPin.value_changed.connect(lambda w:self.pmx485_fault_changed(w))
            self.pmx485ArcTimePin.value_changed.connect(lambda w:self.pmx485_arc_time_changed(w))
            self.w.gas_pressure.valueChanged.connect(self.pmx485_pressure_changed)
            self.w.mesh_enable.stateChanged.connect(lambda w:self.pmx485_mesh_enable_changed(self.w.mesh_enable.isChecked()))
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
            self.pressure = self.w.gas_pressure.value()
            self.pmx485CommsTimer = QTimer()
            self.pmx485CommsTimer.timeout.connect(self.pmx485_timeout)
            self.pmx485RetryTimer = QTimer()
            self.pmx485RetryTimer.timeout.connect(lambda:self.pmx485_enable_changed(True))
            self.meshMode = False
            self.oldCutMode = self.w.cut_mode.value()
            self.pmx485_mesh_enable_changed(self.w.mesh_enable.isChecked())
            self.w.cut_amps.setToolTip(_translate('HandlerClass', 'Powermax cutting current'))
            self.w.pmx485_enable.setChecked(True)
        else:
            if hal.component_exists('pmx485'):
                Popen('halcmd unloadusr pmx485', stdout = PIPE, shell = True)
                head = _translate('HandlerClass', 'INI FILE ERROR')
                msg0 = _translate('HandlerClass', 'Powermax comms not specified in ini file,')
                msg1 = _translate('HandlerClass', 'unloading pmx485 component')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n{}'.format(head, msg0, msg1))
            self.w.gas_pressure.hide()
            self.w.gas_pressure_label.hide()
            self.w.cut_mode.hide()
            self.w.cut_mode_label.hide()
            self.w.pmx485_frame.hide()

    def pmx485_enable_changed(self, state):
        if state:
            self.pmx485CommsError = False
            self.pmx485RetryTimer.stop()
            # if component not loaded then load it and wait 3 secs for it to be loaded
            if not hal.component_exists('pmx485'):
                head = _translate('HandlerClass', 'COMMS ERROR')
                port = self.iniFile.find('QTPLASMAC', 'PM_PORT')
                try:
                    Popen('halcmd loadusr -Wn pmx485 pmx485 {}'.format(port), stdout = PIPE, shell = True)
                    timeout = time.time() + 3
                    while 1:
                        time.sleep(0.1)
                        if time.time() > timeout:
                            self.w.pmx485_enable.setChecked(False)
                            self.w.pmx485_label.setText('')
                            self.pmx485LabelState = None
                            self.w.pmx485_label.setToolTip(_translate('HandlerClass', 'Status of PMX485 communications'))
                            msg0 = _translate('HandlerClass', 'Timeout while reconnecting,')
                            msg1 = _translate('HandlerClass', 'check cables and connections then re-enable')
                            STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n{}'.format(head, msg0, msg1))
                            return
                        if hal.component_exists('pmx485'):
                            break
                except:
                    msg0 = _translate('HandlerClass', 'PMX485 component is not loaded,')
                    msg1 = _translate('HandlerClass', 'Powermax communications are not available')
                    STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n{}'.format(head, msg0, msg1))
                    return
            # if pins not connected then connect them
            if not hal.pin_has_writer('pmx485.enable'):
                for pin in self.pins485Comp:
                    hal.connect(pin,'plasmac:{}'.format(pin.replace('pmx485.', 'pmx485_')))
            # ensure valid parameters before trying to connect
            if self.w.cut_mode.value() == 0 or self.w.cut_amps.value() == 0:
                head = _translate('HandlerClass', 'MATERIALS ERROR')
                msg0 = _translate('HandlerClass', 'Invalid Cut Mode or Cut Amps,')
                msg1 = _translate('HandlerClass', 'cannot connect to Powermax')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n{}'.format(head, msg0, msg1))
                self.w.pmx485_enable.setChecked(False)
                return
            # good to go
            else:
                self.w.pmx485_label.setText(_translate('HandlerClass', 'CONNECTING'))
                self.pmx485LabelState = 'CONNECT'
                self.w.pmx_stats_frame.hide()
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
                self.w.pmx_stats_frame.hide()
                self.pmx485CommsError = True
                self.pmx485Connected = False
                self.pmx485RetryTimer.start(3000)

    def pmx485_arc_time_changed(self, time):
        if self.pmx485Connected:
            self.pmx485ArcTime = self.pmx485ArcTimePin.get()
            self.w.pmx_stats_frame.show()
            self.w.pmx_arc_time_label.setText(_translate('HandlerClass', 'ARC ON TIME'))
            self.display_time('pmx_arc_time_t', self.pmx485ArcTime)

    def pmx485_fault_changed(self, fault):
        if self.pmx485Connected:
            faultRaw = '{:04.0f}'.format(fault)
            self.pmx485FaultCode = '{}-{}-{}'.format(faultRaw[0], faultRaw[1:3], faultRaw[3])
            head = _translate('HandlerClass', 'POWERMAX ERROR')
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
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}: {}\n{}'.format(head, msg0, self.pmx485FaultCode, faultMsg))
            else:
                self.w.pmx485_label.setText('{}: {}'.format(code, faultRaw))
                self.pmx485LabelState = None
                msg0 = _translate('HandlerClass', 'Unknown Powermax fault code')
                self.w.pmx485_label.setStatusTip('{} ({})'.format(msg0, faultRaw))
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}: {}'.format(head, msg0, faultRaw))

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
            self.w.dro_z.setProperty('Qreference_type', 1)
            return
        self.w.jog_stack.setCurrentIndex(1)
        self.cancelWait = False
        self.cutrec_speed_changed(self.w.cut_rec_speed.value())
        hal.set_p('plasmac.cut-recovery', '0')
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

    def cutrec_move(self, state, x, y):
        if state:
            head = _translate('HandlerClass', 'CUT RECOVERY ERROR')
            distX = hal.get_value('qtplasmac.kerf_width-f') * x
            distY = hal.get_value('qtplasmac.kerf_width-f') * y
            if hal.get_value('plasmac.axis-x-position') + \
                hal.get_value('axis.x.eoffset-counts') * self.oScale + distX > self.xMax:
                msg0 = _translate('HandlerClass', 'X axis motion would trip X maximum limit')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}'.format(head, msg0))
                return
            moveX = int(distX / self.oScale)
            if hal.get_value('plasmac.axis-y-position') + \
                hal.get_value('axis.y.eoffset-counts') * self.oScale + distY > self.yMax:
                msg0 = _translate('HandlerClass', 'Y axis motion would trip Y maximum limit')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}'.format(head, msg0))
                return
            moveY = int(distY / self.oScale)
            hal.set_p('plasmac.x-offset', '{}'.format(str(hal.get_value('axis.x.eoffset-counts') + moveX)))
            hal.set_p('plasmac.y-offset', '{}'.format(str(hal.get_value('axis.y.eoffset-counts') + moveY)))
            hal.set_p('plasmac.cut-recovery', '1')

    def cutrec_offset_changed(self):
        if hal.get_value('plasmac.consumable-changing'):
            return
        if self.xOffsetPin.get() > 0.001 * self.unitsPerMm or self.xOffsetPin.get() < -0.001 * self.unitsPerMm or \
           self.yOffsetPin.get() > 0.001 * self.unitsPerMm or self.yOffsetPin.get() < -0.001 * self.unitsPerMm:
            self.cutrec_motion_enable(False)
            if self.cancelWait:
                self.cutrec_buttons_enable(False)
            if self.ccButton:
                self.w[self.ccButton].setEnabled(False)
        else:
            self.cancelWait = False
            self.cutrec_motion_enable(True)
            self.cutrec_buttons_enable(True)
            hal.set_p('plasmac.cut-recovery', '0')
            if self.ccButton and STATUS.is_interp_paused():
                self.w[self.ccButton].setEnabled(True)

    def cutrec_cancel_pressed(self, state):
        if (state):
            if hal.get_value('plasmac.cut-recovery'):
                self.cancelWait = True
                hal.set_p('plasmac.cut-recovery', '0')

    def cutrec_motion_enable(self, state):
        for widget in ['fwd', 'rev', 'speed']:
            self.w['cut_rec_{}'.format(widget)].setEnabled(state)

    def cutrec_buttons_enable(self, state):
        for widget in ['n', 'ne', 'e', 'se', 's', 'sw', 'w', 'nw', 'cancel', 'feed', 'move_label']:
            self.w['cut_rec_{}'.format(widget)].setEnabled(state)


#########################################################################################################################
# CONVERSATIONAL FUNCTIONS #
#########################################################################################################################
    def conv_setup(self):
        self.convSettingsChanged = False
        self.validShape = False
        self.w.preview = QPushButton(_translate('Conversational', 'PREVIEW'))
        self.w.undo = QPushButton(_translate('Conversational', 'RELOAD'))
        if not ACTION.prefilter_path:
            self.w.undo.setEnabled(False)
        self.conv_preview_button(False)
        self.convButtonState = {}
        self.convCommonButtons = ['new', 'save', 'send', 'settings']
        for w in self.convCommonButtons:
            self.convButtonState[w] = False
        if self.unitsPerMm == 1:
            self.unitCode = ['21', '0.25', 32]
        else:
            self.unitCode = ['20', '0.004', 1.26]
        self.ambles = 'G{} G64P{} G40 G49 G80 G90 G92.1 G94 G97'.format(self.unitCode[0], self.unitCode[1])
        CONVSET.load(self, self.w)
        # grid size is in inches
        self.w.conv_preview.grid_size = self.gridSize / self.unitsPerMm / 25.4
        self.w.conv_preview.set_current_view()
        self.w.conv_save.setEnabled(False)
        self.w.conv_send.setEnabled(False)
        self.w.conv_settings.setEnabled(True)
        if ACTION.prefilter_path:
#            try:
            if ACTION.prefilter_path != self.fNgc:
                COPY(ACTION.prefilter_path, self.fNgc)
            COPY(ACTION.prefilter_path, self.fNgcBkp)
            self.w.conv_preview.load(ACTION.prefilter_path)
            self.w.conv_preview.set_current_view()
#            except:
#                print('EXCEPTION')
#                self.conv_new_pressed()
            self.conv_enable_tabs()
        else:
            self.conv_new_pressed(None)
        self.xOrigin = STATUS.get_position()[0][0]
        self.yOrigin = STATUS.get_position()[0][1]
        self.xSaved = '0.000'
        self.ySaved = '0.000'
        self.oSaved = self.origin
        self.convBlock = [False, False]
        if not self.oldConvButton:
            self.conv_shape_request('conv_line', CONVLINE, True)

    def conv_new_pressed(self, button):
        if button and (self.w.conv_save.isEnabled() or self.w.conv_send.isEnabled() or self.convPreviewActive):
            head = _translate('HandlerClass', 'Unsaved Shape')
            btn1 = _translate('HandlerClass', 'CONTINUE')
            btn2 = _translate('HandlerClass', 'CANCEL')
            msg0 = _translate('HandlerClass', 'You have an unsaved, unsent, or active previewed shape')
            msg1 = _translate('HandlerClass', 'If you continue it will be deleted')
            if not self.dialog_show_yesno(QMessageBox.Warning, '{}'.format(head), '{}\n\n{}\n'.format(msg0, msg1), '{}'.format(btn1), '{}'.format(btn2)):
                return
        if self.oldConvButton == 'conv_line':
            if self.lAlias == 'LP2P':
                CONVLINE.set_line_point_to_point(self, self.w, False)
            elif self.lAlias == 'LBLA':
                CONVLINE.set_line_by_angle(self, self.w, False)
            elif self.lAlias == 'A3Pt':
                CONVLINE.set_arc_3_points(self, self.w, False)
            elif self.lAlias == 'A2PR':
                CONVLINE.set_arc_2_points_radius(self, self.w, False)
            elif self.lAlias == 'ALAR':
                CONVLINE.set_arc_by_angle_radius(self, self.w, False)
        outNgc = open(self.fNgc, 'w')
        outNgc.write('(new conversational file)\nM2\n')
        outNgc.close()
        COPY(self.fNgc, self.fTmp)
        COPY(self.fNgc, self.fNgcBkp)
        self.w.conv_preview.load(self.fNgc)
        self.w.conv_save.setEnabled(False)
        self.w.conv_send.setEnabled(False)
        self.validShape = False
        self.conv_preview_button(False)
        self.conv_enable_tabs()

    def conv_save_pressed(self):
        head = _translate('HandlerClass', 'Save Error')
        with open(self.fNgc) as inFile:
            for line in inFile:
                if '(new conversational file)' in line:
                    msg0 = _translate('HandlerClass', 'An empty file cannot be saved')
                    self.dialog_show_ok(QMessageBox.Warning, '{}'.format(head), '{}\n'.format(msg0))
                    return
        self.vkb_show()
        dlg = QFileDialog(self.w)
        dlg.setOptions(QFileDialog.DontUseNativeDialog)
        dlg.setAcceptMode(QFileDialog.AcceptSave)
        dlg.setNameFilters(['G-Code Files (*.ngc *.nc *.tap)', 'All Files (*)'])
        dlg.setDefaultSuffix('ngc')
        dlg.setDirectory(self.programPrefix)
        name = ''
        if dlg.exec_():
            name = dlg.selectedFiles()[0]
        if name:
            COPY(self.fNgc, name)
            self.w.conv_save.setEnabled(False)
            self.conv_enable_tabs()
        self.vkb_show(True)

    def conv_settings_pressed(self):
# **** TEMP FOR CONVERSATIONAL TESTING 2 of 3 ****
#        reload(CONVSET)
        self.color_button_image(self.oldConvButton, self.foreColor)
        self.w[self.oldConvButton].setStyleSheet(\
                'QPushButton {{ background: {0} }} \
                 QPushButton:pressed {{ background: {0} }}'.format(self.backColor))
        for w in self.convCommonButtons:
            self.convButtonState[w] = self.w['conv_{}'.format(w)].isEnabled()
            self.w['conv_{}'.format(w)].setEnabled(False)
        self.conv_clear_widgets()
        CONVSET.widgets(self, self.w)
        CONVSET.show(self, self.w)

    def conv_send_pressed(self):
        COPY(self.fNgcBkp, self.fNgc.replace('shape','sent_shape'))
        self.w.conv_send.setEnabled(False)
        self.w.conv_save.setEnabled(False)
        self.conv_enable_tabs()
        self.vkb_hide()
        ACTION.OPEN_PROGRAM(self.fNgc.replace('shape','sent_shape'))

    def conv_block_pressed(self):
        if not self.convSettingsChanged:
            if self.convPreviewActive and not self.conv_active_shape():
                return
            head = _translate('HandlerClass', 'Array Error')
            with open(self.fNgc) as inFile:
                for line in inFile:
                    if '(new conversational file)' in line:
                        msg0 = _translate('HandlerClass', 'An empty file cannot be arrayed, rotated, or scaled')
                        self.dialog_show_ok(QMessageBox.Warning, '{}'.format(head), '{}\n'.format(msg0))
                        inFile.close()
                        return
                    # see if we can do something about NURBS blocks down the track
                    # elif 'g5.2' in line.lower() or 'g5.3' in line.lower():
                    #     head = _translate('HandlerClass', 'Scale Error')
                    #     msg0 = _translate('HandlerClass', 'Cannot scale a GCode NURBS block')
                    #     self.dialog_show_ok(QMessageBox.Warning, '{}'.format(head), '{}\n\n{}'.format(msg0, line))
                    #     return
                    elif 'M3' in line or 'm3' in line:
                        break
        self.conv_shape_request(self.w.sender().objectName(), CONVBLCK, False)

    def conv_shape_request(self, shape, module, material):
# **** TEMP FOR CONVERSATIONAL TESTING 3 of 3****
#        reload(module)
        if not self.convSettingsChanged:
            if self.convPreviewActive and not self.conv_active_shape():
                return
            self.conv_preview_button(False)
        if material:
            self.w.conv_material.show()
        else:
            self.w.conv_material.hide()
        try:
            self.w.conv_material.currentTextChanged.disconnect()
        except:
            pass
        self.conv_button_color(shape)
        self.w.conv_settings.setEnabled(True)
        self.w.preview.setEnabled(True)
        if self.validShape:
            self.w.undo.setEnabled(True)
        self.conv_clear_widgets()
        module.widgets(self, self.w)

    def conv_preview_button(self, state):
        self.convPreviewActive = state
        self.conv_enable_tabs()
        if state:
            self.w.preview.setStyleSheet('QPushButton {{ color: {} }} \
                                          QPushButton:disabled {{ color: {} }}' \
                                          .format(self.estopColor, self.disabledColor))
            self.w.conv_save.setEnabled(False)
            self.w.conv_send.setEnabled(False)
            self.w.undo.setText(_translate('HandlerClass', 'UNDO'))
        else:
            self.w.preview.setStyleSheet('QPushButton {{ color: {} }} \
                                          QPushButton:disabled {{ color: {} }}' \
                                          .format(self.foreColor, self.disabledColor))
            if self.validShape:
                self.w.conv_save.setEnabled(True)
                self.w.conv_send.setEnabled(True)
            self.w.undo.setText(_translate('HandlerClass', 'RELOAD'))

    def conv_active_shape(self):
        btn1 = _translate('HandlerClass', 'CONTINUE')
        btn2 = _translate('HandlerClass', 'CANCEL')
        head = _translate('HandlerClass', 'Active Preview')
        msg0 = _translate('HandlerClass', 'You have an active previewed shape')
        msg1 = _translate('HandlerClass', 'If you continue it will be deleted')
        response = self.dialog_show_yesno(QMessageBox.Warning, '{}'.format(head), '{}\n\n{}\n'.format(msg0, msg1), '{}'.format(btn1), '{}'.format(btn2))
        if response:
            self.conv_undo_shape()
            self.conv_preview_button(False)
        return response

    def conv_button_color(self, button):
        if self.oldConvButton:
            self.color_button_image(self.oldConvButton, self.foreColor)
            self.w[self.oldConvButton].setStyleSheet(\
                    'QPushButton {{ background: {0} }} \
                     QPushButton:pressed {{ background: {0} }}'.format(self.backColor))
        self.oldConvButton = button
        self.color_button_image(button, self.backColor)
        self.w[button].setStyleSheet(\
                'QPushButton {{ background: {0} }} \
                 QPushButton:pressed {{ background: {0} }}'.format(self.foreColor))

    def conv_enable_buttons(self, state):
        for button in ['new', 'save', 'settings', 'send']:
            self.w['conv_{}'.format(button)].setEnabled(state)

    def conv_restore_buttons(self):
        for button in self.convCommonButtons:
            self.w['conv_{}'.format(button)].setEnabled(self.convButtonState[button])

    def conv_enable_tabs(self):
        if self.w.conv_save.isEnabled() or self.convPreviewActive:
            for n in range(self.w.main_tab_widget.count()):
                if n != 1:
                    self.w.main_tab_widget.setTabEnabled(n, False)
        else:
            for n in range(self.w.main_tab_widget.count()):
                self.w.main_tab_widget.setTabEnabled(n, True)
                # enabling tabs causes issues with the gcode widgets margin styles
                # so we refresh the style here as a workaround
                self.w.gcode_editor.setStyleSheet( \
                        'EditorBase{{ qproperty-styleColorMarginText: {} }}'.format(self.foreColor))
                self.w.gcode_display.setStyleSheet( \
                        'EditorBase{{ qproperty-styleColorMarginText: {} }}'.format(self.foreColor))

    def conv_entry_changed(self, widget):
        name = widget.objectName()
        if widget.text():
            if name in ['intEntry', 'hsEntry', 'cnEntry', 'rnEntry']:
                good = '0123456789'
            elif name in ['xsEntry', 'ysEntry', 'aEntry']:
                good = '-.0123456789'
            else:
                good = '.0123456789'
            out = ''
            for t in widget.text():
                if t in good and not(t == '-' and len(out) > 0) and not(t == '.' and t in out):
                    out += t
            widget.setText(out)
            if widget.text() in '-.' or widget.text() == '-.':
                return 'operator'
            try:
                a = float(widget.text())
            except:
                head = _translate('HandlerClass', 'Numeric Entry Error')
                msg0 = _translate('HandlerClass', 'An invalid entry has been detected')
                self.dialog_show_ok(QMessageBox.Warning, '{}'.format(head), '{}\n'.format(msg0))
                widget.setText('0')
        if name == 'gsEntry':
            # grid size is in inches
            self.w.conv_preview.grid_size = float(widget.text()) / self.unitsPerMm / 25.4
            self.w.conv_preview.set_current_view()

    def conv_undo_shape(self):
        # setup for a reload if required
        if not self.convPreviewActive:
            head = _translate('HandlerClass', 'Reload Request')
            btn1 = _translate('HandlerClass', 'CONTINUE')
            btn2 = _translate('HandlerClass', 'CANCEL')
            if ACTION.prefilter_path:
                name = os.path.basename(ACTION.prefilter_path)
                msg0 = _translate('HandlerClass', 'The original file will be loaded')
                msg1 = _translate('HandlerClass', 'If you continue all changes will be deleted')
                if not self.dialog_show_yesno(QMessageBox.Warning, '{}'.format(head), '{}:\n\n{}\n\n{}\n'.format(msg0, name, msg1), '{}'.format(btn1), '{}'.format(btn2)):
                    return(True)
            else:
                msg0 = _translate('HandlerClass', 'An empty file will be loaded')
                msg1 = _translate('HandlerClass', 'If you continue all changes will be deleted')
                if not self.dialog_show_yesno(QMessageBox.Warning, '{}'.format(head), '{}\n\n{}\n'.format(msg0, msg1), '{}'.format(btn1), '{}'.format(btn2)):
                    return(True)
            if ACTION.prefilter_path:
                COPY(ACTION.prefilter_path, self.fNgcBkp)
            else:
                outNgc = open(self.fNgcBkp, 'w')
                outNgc.write('(new conversational file)\nM2\n')
                outNgc.close()
            self.validShape = False
            self.w.preview.setEnabled(True)
            self.w.undo.setEnabled(False)
            self.w.conv_save.setEnabled(False)
            self.w.conv_send.setEnabled(False)
        # undo the shape
        if os.path.exists(self.fNgcBkp):
            COPY(self.fNgcBkp, self.fNgc)
            self.w.conv_preview.load(self.fNgc)
            self.w.conv_preview.set_current_view()
            self.w.add.setEnabled(False)
            if not self.validShape:
                self.w.undo.setEnabled(False)
            if not self.convBlock[1]:
                self.convBlock[0] = False
            self.conv_preview_button(False)
            self.conv_enable_tabs()

    def conv_add_shape_to_file(self):
        COPY(self.fNgc, self.fNgcBkp)
        try:
            if self.w.xsEntry.text():
                self.xSaved = self.w.xsEntry.text()
        except:
            pass
        try:
            if self.w.ysEntry.text():
                self.ySaved = self.w.ysEntry.text()
        except:
            pass
        try:
            self.oSaved = self.w.center.isChecked()
        except:
            pass
        self.validShape = True
        self.w.add.setEnabled(False)
        self.w.conv_save.setEnabled(True)
        self.w.conv_send.setEnabled(True)
        self.conv_preview_button(False)
        self.conv_enable_tabs()

    def conv_accept(self):
        self.validShape = True
        self.conv_preview_button(False)
        COPY(self.fNgc, self.fNgcBkp)
        self.w.conv_preview.load(self.fNgc)
        self.w.add.setEnabled(False)
        self.w.conv_save.setEnabled(True)
        self.w.conv_send.setEnabled(True)
        self.conv_enable_tabs()

    def conv_clear_widgets(self):
        for i in reversed(range(self.w.entries.count())):
            widgetToRemove = self.w.entries.itemAt(i).widget()
            if widgetToRemove:
                self.w.entries.removeWidget(widgetToRemove)
                widgetToRemove.setParent(None)


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
            self.w.PREFS_.putpref(label,  color.name(), str, 'COLOR_OPTIONS')
            self.set_basic_colors()
            self.set_color_styles()

    def set_basic_colors(self):
        self.foreColor = self.w.PREFS_.getpref('Foreground', '#ffee06', str, 'COLOR_OPTIONS')
        self.fore1Color = self.w.PREFS_.getpref('Highlight', '#ffee06', str, 'COLOR_OPTIONS')
        self.backColor = self.w.PREFS_.getpref('Background', '#16160e', str, 'COLOR_OPTIONS')
        self.back1Color = self.w.PREFS_.getpref('Background Alt', '#26261e', str, 'COLOR_OPTIONS')
        self.disabledColor = self.w.PREFS_.getpref('Disabled', '#b0b0b0', str, 'COLOR_OPTIONS')
        self.estopColor = self.w.PREFS_.getpref('Estop', '#ff0000', str, 'COLOR_OPTIONS')

    def set_color_styles(self):
        self.styleSheetFile = os.path.join(self.PATHS.CONFIGPATH, 'qtplasmac.qss')
        ssFile = self.iniFile.find('QTPLASMAC', 'CUSTOM_STYLE') or ''
        # if custom stylesheet try to use it
        if ssFile:
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
        for button in buttons:
            self.color_button_image(button, self.foreColor)
            self.w[button].setStyleSheet(\
                    'QPushButton {{ background: {0} }} \
                     QPushButton:pressed {{ background: {0} }}'.format(self.backColor))
        # the error message label on the status bar
        self.w.error_label.setStyleSheet('QLabel {{ color: {} }}'.format(self.estopColor))
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
            self.dialog_show_ok(QMessageBox.Warning, '{}'.format(head), '{}\n{}\n\n{}\n'.format(msg0, msg1, msg2))
            self.standard_stylesheet()
        except:
            msg0 = _translate('HandlerClass', 'Cannot open custom stylesheet')
            msg1 = _translate('HandlerClass', 'Reverting to standard stylesheet')
            self.dialog_show_ok(QMessageBox.Warning, '{}'.format(head), '{}\n\n{}\n'.format(msg0, msg1))
            self.standard_stylesheet()

    def color_button_image(self, button, color):
        image_path = '{}{}.png'.format(self.IMAGES, button)
        self.image = QImage(image_path)
        for x in range(self.image.width()):
            for y in range(self.image.height()):
                pColor = self.image.pixelColor(x, y)
                if pColor.alpha() > 0:
                    newColor = QColor(color)
                    newColor.setAlpha(pColor.alpha())
                    self.image.setPixelColor(x, y, newColor)
        self.w['{}'.format(button)].setIcon(QIcon(QPixmap.fromImage(self.image)))


#########################################################################################################################
# KEY BINDING CALLS #
#########################################################################################################################
    def key_is_valid(self, event, state):
        return self.keyboard_shortcuts() and state and not event.isAutoRepeat()

    def jog_is_valid(self, key, event):
        return self.keyboard_shortcuts() and not event.isAutoRepeat() and not self.w.main_tab_widget.currentIndex() and self.w['jog_{}'.format(key)].isEnabled()

    def on_keycall_ESTOP(self, event, state, shift, cntrl):
        if self.key_is_valid(event, state):
            ACTION.SET_ESTOP_STATE(STATUS.estop_is_clear())

    def on_keycall_POWER(self, event, state, shift, cntrl):
        if self.key_is_valid(event, state):
            ACTION.SET_MACHINE_STATE(not STATUS.machine_is_on())

    def on_keycall_ABORT(self, event, state, shift, cntrl):
        if self.key_is_valid(event, state):
            self.abort_pressed()

    def on_keycall_HOME(self, event, state, shift, cntrl):
        if self.key_is_valid(event, state) and not shift and not self.w.main_tab_widget.currentIndex() and STATUS.is_on_and_idle() and self.w.home_all.isEnabled():
            if STATUS.is_all_homed():
                ACTION.SET_MACHINE_UNHOMED(-1)
            else:
                ACTION.SET_MACHINE_HOMING(-1)

    def on_keycall_RUN(self, event, state, shift, cntrl):
        if self.key_is_valid(event, state) and not shift and not self.w.main_tab_widget.currentIndex():
            if self.w.run.isEnabled():
                self.run_pressed()
            elif self.w.pause.isEnabled():
                ACTION.PAUSE()

    def on_keycall_PAUSE(self, event, state, shift, cntrl):
        if self.key_is_valid(event, state) and not self.w.main_tab_widget.currentIndex() and \
           self.w.pause.isEnabled() and not STATUS.stat.interp_state == linuxcnc.INTERP_PAUSED:
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
