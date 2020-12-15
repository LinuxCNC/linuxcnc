import os, sys
from shutil import copy as COPY
from shutil import rmtree as RMDIR
from subprocess import Popen, PIPE
from subprocess import call as CALL
import time
import math

import linuxcnc
import hal, hal_glib

from PyQt5 import QtCore, QtWidgets, QtGui
from PyQt5.QtCore import * 
from PyQt5.QtWidgets import * 
from PyQt5.QtGui import * 
from PyQt5.Qsci import QsciScintilla, QsciLexerCustom, QsciLexerPython

from qtvcp import logger
from qtvcp.core import Status, Action, Info
from qtvcp.lib.gcodes import GCodes
from qtvcp.lib.keybindings import Keylookup
from qtvcp.widgets.gcode_editor import GcodeDisplay as DISPLAY
from qtvcp.widgets.gcode_editor import GcodeEditor as EDITOR
from qtvcp.widgets.gcode_editor import GcodeLexer as LEXER
from qtvcp.widgets.mdi_history import MDIHistory as MDI_WIDGET
from qtvcp.widgets.status_label import StatusLabel as STATLABEL
from qtvcp.widgets.stylesheeteditor import  StyleSheetEditor as SSE
from qtvcp.widgets import camview_widget as CAMWIDGET
from qtvcp.widgets.camview_widget import CamView as CAM
from qtvcp.widgets.gcode_graphics import GCodeGraphics as PREVIEW

from qtvcp.lib.conversational import conv_settings as CONVSET
from qtvcp.lib.conversational import conv_circle as CONVCIRC
from qtvcp.lib.conversational import conv_line as CONVLINE
from qtvcp.lib.conversational import conv_triangle as CONVTRIA
from qtvcp.lib.conversational import conv_rectangle as CONVRECT
from qtvcp.lib.conversational import conv_polygon as CONVPOLY
from qtvcp.lib.conversational import conv_bolt as CONVBOLT
from qtvcp.lib.conversational import conv_slot as CONVSLOT
from qtvcp.lib.conversational import conv_star as CONVSTAR
from qtvcp.lib.conversational import conv_gusset as CONVGUST
from qtvcp.lib.conversational import conv_sector as CONVSECT
from qtvcp.lib.conversational import conv_rotate as CONVROTA
from qtvcp.lib.conversational import conv_array as CONVARAY

VERSION = '0.9.000'

LOG = logger.getLogger(__name__)
KEYBIND = Keylookup()
STATUS = Status()
INFO = Info()
ACTION = Action()

class VLine(QFrame):
    def __init__(self):
        super(VLine, self).__init__()
        self.setFrameShape(self.VLine|self.Plain)

class HandlerClass:
    def __init__(self, halcomp, widgets, paths):
        self.h = halcomp
        self.w = widgets
        self.h.comp.setprefix('qtplasmac')
        self.PATHS = paths
# print all the paths
#        for item in dir(self.PATHS):
#            if item[0].isupper():
#                print('{} = {}'.format(item, getattr(self.PATHS, item)))
        self.foreColor = '#ffee06'
        try:
            pFile = '{}/qtplasmac.prefs'.format(self.PATHS.CONFIGPATH)
            with open(pFile, 'r') as inFile:
                for line in inFile:
                    if line.startswith('Foreground ='):
                        self.foreColor = line.split('=')[1].strip()
                        break
        except:
            pass
        INIPATH = os.environ.get('INI_FILE_NAME', '/dev/null')
        self.iniFile = linuxcnc.ini(INIPATH)
# keep users versions up to date
        if not self.PATHS.BASEDIR in self.PATHS.CONFIGPATH:
        # determine if a package install or a rip install
            if self.PATHS.SCREENDIR.startswith('/usr/share'):
                sourcePath = '/usr/share/doc/linuxcnc/examples/sample-configs/by_machine/qtplasmac/plasmac/'
            else:
                sourcePath = os.path.join(self.PATHS.BASEDIR, 'configs/by_machine/qtplasmac/plasmac/')
        # if old common folder exist rename it - we can delete this down the track as it only applies to testing versions
            commDir = '{}/common'.format(self.PATHS.CONFIGPATH)
            if os.path.exists(commDir):
                os.rename(commDir, '{}_redundant_{}'.format(commDir, time.time()))
        # if a plasmac file or folder exists, rename it. If a plasmac link exists delete it
            plasDir = '{}/plasmac'.format(self.PATHS.CONFIGPATH)
            if os.path.exists(plasDir):
                if os.path.isfile(plasDir) or os.path.isdir(plasDir):
                    os.rename(plasDir, '{}_redundant_{}'.format(plasDir, time.time()))
                elif os.path.islink(plasDir):
                    os.remove(plasDir)
                else:
                    print('unknown plasmac item in {}'.format(self.PATHS.CONFIGPATH))
                    sys.exit()
        # create new plasmac link to suit the installation type
            os.symlink(sourcePath, '{}/plasmac'.format(self.PATHS.CONFIGPATH))
# if old testing version, exit and warn the user. This can also be removed down the track
        if 'common' in self.iniFile.find('RS274NGC', 'SUBROUTINE_PATH'):
            err  = '\n**********************************************\n'
            err += '* This configuration requires changes        *\n'
            err += '*                                            *\n'
            err += '* edit the ini file and change "common"      *\n'
            err += '* in any of the following lines to "plasmac" *\n'
            err += '*                                            *\n'
            err += '* ngc =                                      *\n'
            err += '* nc =                                       *\n'
            err += '* tap =                                      *\n'
            err += '* SUBROUTINE_PATH =                          *\n'
            err += '* USER_M_PATH =                              *\n'
            err += '**********************************************\n\n'
            print(err)
            sys.exit()
        self.STYLEEDITOR = SSE(widgets, paths)
        self.GCODES = GCodes(widgets)
        self.valid = QDoubleValidator(0.0, 999.999, 3)
        self.IMAGES = os.path.join(self.PATHS.IMAGEDIR, 'qtplasmac/images/')
        self.w.setWindowIcon(QIcon(os.path.join(self.IMAGES, 'linuxcncicon.png')))
        self.landscape = True
        if os.path.basename(self.PATHS.XML) == 'qtplasmac_9x16.ui':
            self.landscape = False
        self.widgetsLoaded = 0
        KEYBIND.add_call('Key_F12','on_keycall_F12')
        KEYBIND.add_call('Key_F9','on_keycall_F9')
        KEYBIND.add_call('Key_Pause', 'on_keycall_pause')
        KEYBIND.add_call('Key_Plus', 'on_keycall_plus')
        KEYBIND.add_call('Key_Minus', 'on_keycall_minus')
        STATUS.connect('state-on', lambda w:self.power_state(True))
        STATUS.connect('state-off', lambda w:self.power_state(False))
        STATUS.connect('hard-limits-tripped', self.hard_limit_tripped)
        STATUS.connect('user-system-changed', self.user_system_changed)
        STATUS.connect('file-loaded', self.file_loaded)
        STATUS.connect('homed', self.joint_homed)
        STATUS.connect('all-homed', self.joints_all_homed)
        STATUS.connect('not-all-homed', self.joint_unhomed)
        STATUS.connect('g-code-changed', self.gcodes_changed)
        STATUS.connect('m-code-changed', self.mcodes_changed)
        STATUS.connect('program-pause-changed', self.pause_changed) 
        STATUS.connect('graphics-loading-progress', self.percent_loaded)
        STATUS.connect('interp-paused', self.interp_paused)
        STATUS.connect('interp-idle', self.interp_idle)
        STATUS.connect('interp-reading', self.interp_reading)
        STATUS.connect('interp-waiting', self.interp_waiting)
        STATUS.connect('interp-run', self.interp_running)
        self.axisList = INFO.AVAILABLE_AXES
        self.systemList = ['G53','G54','G55','G56','G57','G58','G59','G59.1','G59.2','G59.3']
        self.slowJogFactor = 10
        self.lastLoadedProgram = 'None'
        self.firstRun = True
        self.idleList = ['file_open', 'file_reload', 'file_edit']
        self.idleOnList = ['home_x', 'home_y', 'home_z', 'home_a', 'home_all', 'wcs_button']
        self.idleHomedList = ['touch_x', 'touch_y', 'touch_z', 'touch_a', 'touch_xy', 'mdi_show']
        self.idleHomedPlusPausedList = []
        self.axisAList = ['dro_a', 'dro_label_a', 'home_a', 'touch_a', 'jog_a_plus', 'jog_a_minus']
#                          'widget_jog_angular', 'widget_increments_angular']
        self.thcFeedRate = float(self.iniFile.find('AXIS_Z', 'MAX_VELOCITY')) * \
                           float(self.iniFile.find('AXIS_Z', 'OFFSET_AV_RATIO')) * 60
        self.maxHeight = hal.get_value('ini.z.max_limit') - hal.get_value('ini.z.min_limit')
        self.unitsPerMm = hal.get_value('halui.machine.units-per-mm')
        if self.unitsPerMm != 1:
            self.units = 'inch'
        else:
            self.units = 'metric'
        self.maxPidP = self.thcFeedRate / self.unitsPerMm * 0.1
        self.mode = int(self.iniFile.find('PLASMAC', 'MODE')) or 0
        self.tmpPath = '/tmp/qtplasmac/'
        if not os.path.isdir(self.tmpPath):
            os.mkdir(self.tmpPath)
        self.materialFile = self.iniFile.find('EMC', 'MACHINE').lower() + '_material.cfg'
        self.tmpMaterialFile = '{}{}'.format(self.tmpPath, self.materialFile.replace('.cfg','.tmp'))
        self.tmpMaterialFileGCode = '{}{}'.format(self.tmpPath, self.materialFile.replace('.cfg','.gcode'))
        self.materialFileDict = {}
        self.materialDict = {}
        self.materialNumList = []
        self.materialUpdate = False
        self.autoChange = False
        self.pmx485Loaded = False
        self.pmx485Connected = False
        self.fault = '0000'
        self.cutRecovering = False
        self.camCurrentX = self.camCurrentY = 0
        self.degreeSymbol = u"\u00b0"
        self.cameraOn = False
        self.fTmp = '{}/temp.ngc'.format(self.tmpPath)
        self.fNgc = '{}/shape.ngc'.format(self.tmpPath)
        self.fNgcBkp = '{}/backup.ngc'.format(self.tmpPath)
        self.oldConvButton = ''
        self.programPrefix = self.iniFile.find('DISPLAY', 'PROGRAM_PREFIX') or os.environ['LINUXCNC_NCFILES_DIR']
        self.dialogError = False
        self.cutTypeText = ''

    def initialized__(self):
        self.init_preferences()
        self.init_widgets()
        self.set_color_styles()
        self.set_signal_connections()
        self.set_axes_and_joints()
        self.set_spinbox_parameters()
        self.load_plasma_parameters()
        self.set_mode()
        self.user_button_setup()
        self.check_material_file()
        self.load_materials()
        self.pmx485_check()
        if self.firstRun is True:
            self.firstRun = False
        self.link_hal_pins()
        self.touchoff_buttons()
        self.widgetsLoaded = 1

#################################################################################################################################
# CLASS PATCHING SECTION #
#################################################################################################################################
    def class_patch__(self):
        self.old_styleText = LEXER.styleText
        LEXER.styleText = self.styleText
        self.old_defaultColor = LEXER.defaultColor
        LEXER.defaultColor = self.defaultColor
        self.old_exitCall = EDITOR.exitCall
        EDITOR.exitCall = self.exit_call
        self.old_gcodeLexerCall = EDITOR.gcodeLexerCall
        EDITOR.gcodeLexerCall = self.gcode_lexer_call
        self.old_pythonLexerCall = EDITOR.pythonLexerCall
        EDITOR.pythonLexerCall = self.python_lexer_call
        self.old_wheelEvent = CAM.wheelEvent
        CAM.wheelEvent = self.wheelEvent
        self.old_drawText = CAM.drawText
        CAM.drawText = self.drawText

# # testing to find who is updating their view
#        self.old_view_signal = PREVIEW.set_view_signal
#        PREVIEW.set_view_signal = self.set_view_signal
#
# #testing for different video sources
#         self.old_showEvent = CAM.showEvent
#         CAM.showEvent = self.showEvent
# 
#     def showEvent(self, event):
#         if CAMWIDGET.LIB_GOOD:
# #            try:
#             print('SETTING STREAM')
#             self.w.camview.video = CAMWIDGET.WebcamVideoStream(src=0).start()
# #            except:
# #                LOG.error('Video capture error: {}'.format(self.w.camview.video))
#
# # testing to find who is updating their view
# patched gcode graphics functions
#    def set_view_signal(self, w, view, args):
#        pass
#        print('GCODE VIEW CHANGED',self.w.sender().objectName(),w,view,args)

# patched style functions
    def styleText(self, start, end):
        pass

    def defaultColor(self, style):
        return QColor(self.foreColor)

# patched gcode editor functions
    def exit_call(self):
        if self.w.gcode_editor.editor.isModified():
            result = QMessageBox.question(self.w,
                                         'Warning!!',
                                         'Unsaved changes will be lost...\n\nDo you want to exit?\n',
                                         QMessageBox.Yes | QMessageBox.No)
            if result != QMessageBox.Yes:
                return
            else:
                self.file_reload_clicked()
        self.w.preview_stack.setCurrentIndex(0)

    def gcode_lexer_call(self):
        print('need to hide gcode lexer button')
        pass

    def python_lexer_call(self):
        print('need to hide python lexer button')
        pass

    def kill_check(self):
        choice = QMessageBox.question(self.w,
                                     'Warning!!',
                                     'Unsaved changes will be lost...\n\nDo you want to exit?\n',
                                     QMessageBox.Yes | QMessageBox.No)
        if choice == QMessageBox.Yes:
            return True
        else:
            return False

# patched camera functions
    def drawText(self, event, qp):
        qp.setPen(self.w.camview.text_color)
        qp.setFont(self.w.camview.font)
        if self.w.camview.pix:
            angle = 0.0 if self.w.camview.rotation == 0 else 360 - self.w.camview.rotation
            qp.drawText(self.w.camview.rect(), QtCore.Qt.AlignTop, '{:0.3f}{}'.format(angle, self.degreeSymbol))
        else:
            qp.drawText(self.w.camview.rect(), QtCore.Qt.AlignCenter, self.w.camview.text)

    def wheelEvent(self, event):
        mouseState = qApp.mouseButtons()
        size = self.w.camview.size()
        w = size.width()
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
        if self.w.camview.diameter > w: self.w.camview.diameter = w
        if self.w.camview.scale < 1: self.w.camview.scale = 1
        if self.w.camview.scale > 5: self.w.camview.scale = 5


#################################################################################################################################
# SPECIAL FUNCTIONS SECTION #
#################################################################################################################################
    def link_hal_pins(self):
        #ini
        CALL(['halcmd', 'net', 'plasmac:axis-max-limit', 'ini.z.max_limit', 'plasmac.axis-z-max-limit'])
        CALL(['halcmd', 'net', 'plasmac:axis-min-limit', 'ini.z.min_limit', 'plasmac.axis-z-min-limit'])
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
        CALL(['halcmd', 'net','plasmac:thc-delay','qtplasmac.thc_delay-f','plasmac.thc-delay'])
        CALL(['halcmd', 'net','plasmac:thc-threshold','qtplasmac.thc_threshold-f','plasmac.thc-threshold'])
        CALL(['halcmd', 'net','plasmac:pid-p-gain','qtplasmac.pid_p_gain-f','plasmac.pid-p-gain'])
        CALL(['halcmd', 'net','plasmac:cornerlock-threshold','qtplasmac.cornerlock_threshold-f','plasmac.cornerlock-threshold'])
        CALL(['halcmd', 'net','plasmac:kerfcross-override','qtplasmac.kerfcross_override-f','plasmac.kerfcross-override'])
        CALL(['halcmd', 'net','plasmac:pid-i-gain','qtplasmac.pid_i_gain-f','plasmac.pid-i-gain'])
        CALL(['halcmd', 'net','plasmac:pid-d-gain','qtplasmac.pid_d_gain-f','plasmac.pid-d-gain'])
        #probe parameters
        CALL(['halcmd', 'net','plasmac:float-switch-travel','qtplasmac.float_switch_travel-f','plasmac.float-switch-travel'])
        CALL(['halcmd', 'net','plasmac:probe-feed-rate','qtplasmac.probe_feed_rate-f','plasmac.probe-feed-rate'])
        CALL(['halcmd', 'net','plasmac:probe-start-height','qtplasmac.probe_start_height-f','plasmac.probe-start-height'])
        CALL(['halcmd', 'net','plasmac:ohmic-probe-offset','qtplasmac.ohmic_probe_offset-f','plasmac.ohmic-probe-offset'])
        CALL(['halcmd', 'net','plasmac:ohmic-max-attempts','qtplasmac.ohmic_max_attempts-s','plasmac.ohmic-max-attempts'])
        CALL(['halcmd', 'net','plasmac:skip-ihs-distance','qtplasmac.skip_ihs_distance-f','plasmac.skip-ihs-distance'])
        #safety parameters
        CALL(['halcmd', 'net','plasmac:safe-height','qtplasmac.safe_height-f','plasmac.safe-height'])
        #scribe parameters
        CALL(['halcmd', 'net','plasmac:scribe-arm-delay','qtplasmac.scribe_arm_delay-f','plasmac.scribe-arm-delay'])
        CALL(['halcmd', 'net','plasmac:scribe-on-delay','qtplasmac.scribe_on_delay-f','plasmac.scribe-on-delay'])
        #spotting parameters
        CALL(['halcmd', 'net','plasmac:spotting-threshold','qtplasmac.spotting_threshold-f','plasmac.spotting-threshold'])
        CALL(['halcmd', 'net','plasmac:spotting-time','qtplasmac.spotting_time-f','plasmac.spotting-time'])
        #motion parameters
        CALL(['halcmd', 'net','plasmac:setup-feed-rate','qtplasmac.setup_feed_rate-f','plasmac.setup-feed-rate'])
        #material parameters
        CALL(['halcmd', 'net','plasmac:cut-feed-rate','qtplasmac.cut_feed_rate-f','plasmac.cut-feed-rate'])
        CALL(['halcmd', 'net','plasmac:cut-height','qtplasmac.cut_height-f','plasmac.cut-height'])
        CALL(['halcmd', 'net','plasmac:cut-volts','qtplasmac.cut_volts-f','plasmac.cut-volts'])
        CALL(['halcmd', 'net','plasmac:pause-at-end','qtplasmac.pause_at_end-f','plasmac.pause-at-end'])
        CALL(['halcmd', 'net','plasmac:pierce-delay','qtplasmac.pierce_delay-f','plasmac.pierce-delay'])
        CALL(['halcmd', 'net','plasmac:pierce-height','qtplasmac.pierce_height-f','plasmac.pierce-height'])
        CALL(['halcmd', 'net','plasmac:puddle-jump-delay','qtplasmac.puddle_jump_delay-f','plasmac.puddle-jump-delay'])
        CALL(['halcmd', 'net','plasmac:puddle-jump-height','qtplasmac.puddle_jump_height-f','plasmac.puddle-jump-height'])
        #monitor
        CALL(['halcmd', 'net','plasmac:arc_ok_out','plasmac.arc-ok-out','qtplasmac.led_arc_ok'])
        CALL(['halcmd', 'net','plasmac:arc_voltage_out','plasmac.arc-voltage-out','qtplasmac.arc_voltage'])
        CALL(['halcmd', 'net','plasmac:breakaway-switch-out','qtplasmac.led_breakaway_switch'])
        CALL(['halcmd', 'net','plasmac:cornerlock-is-locked','plasmac.cornerlock-is-locked','qtplasmac.led_corner_lock'])
        CALL(['halcmd', 'net','plasmac:float-switch-out','qtplasmac.led_float_switch'])
        CALL(['halcmd', 'net','plasmac:kerfcross-is-locked','plasmac.kerfcross-is-locked','qtplasmac.led_kerf_lock'])
        CALL(['halcmd', 'net','plasmac:move-up','plasmac.led-up','qtplasmac.led_thc_up'])
        CALL(['halcmd', 'net','plasmac:move-down','plasmac.led-down','qtplasmac.led_thc_down'])
        CALL(['halcmd', 'net','plasmac:ohmic-probe-out','qtplasmac.led_ohmic_probe'])
        CALL(['halcmd', 'net','plasmac:thc-active','plasmac.thc-active','qtplasmac.led_thc_active'])
        CALL(['halcmd', 'net','plasmac:thc-enabled','plasmac.thc-enabled','qtplasmac.led_thc_enabled'])
        CALL(['halcmd', 'net','plasmac:torch-on','qtplasmac.led_torch_on'])
        #control
        CALL(['halcmd', 'net','plasmac:cornerlock-enable','qtplasmac.cornerlock_enable','plasmac.cornerlock-enable'])
        CALL(['halcmd', 'net','plasmac:kerfcross-enable','qtplasmac.kerfcross_enable','plasmac.kerfcross-enable'])
        CALL(['halcmd', 'net','plasmac:mesh-enable','qtplasmac.mesh_enable','plasmac.mesh-enable'])
        CALL(['halcmd', 'net','plasmac:ignore-arc-ok-1','qtplasmac.ignore_arc_ok','plasmac.ignore-arc-ok-1'])
        CALL(['halcmd', 'net','plasmac:ohmic-probe-enable','qtplasmac.ohmic_probe_enable','plasmac.ohmic-probe-enable'])
        CALL(['halcmd', 'net','plasmac:thc-enable','qtplasmac.thc_enable','plasmac.thc-enable'])
        CALL(['halcmd', 'net','plasmac:use-auto-volts','qtplasmac.use_auto_volts','plasmac.use-auto-volts'])
        CALL(['halcmd', 'net','plasmac:torch-enable','qtplasmac.torch_enable','plasmac.torch-enable'])
        #offsets
        CALL(['halcmd', 'net','plasmac:x-offset-current','qtplasmac.x_offset'])
        CALL(['halcmd', 'net','plasmac:y-offset-current','qtplasmac.y_offset'])

    def init_preferences(self):
        if not self.w.PREFS_:
#            self.add_alarm('CRITICAL - no preference file found, enable preferences in screenoptions widget')
            print('CRITICAL - no preference file found, enable preferences in screenoptions widget')
#            return
        self.lastLoadedProgram = self.w.PREFS_.getpref('RecentPath_0', 'None', str,'BOOK_KEEPING')
        self.w.chk_keyboard_shortcuts.setChecked(self.w.PREFS_.getpref('Use keyboard shortcuts', False, bool, 'GUI_OPTIONS'))
        self.w.chk_soft_keyboard.setChecked(self.w.PREFS_.getpref('Use soft keyboard', False, bool, 'GUI_OPTIONS'))
#        self.w.chk_run_from_line.setChecked(self.w.PREFS_.getpref('Run from line', False, bool, 'GUI_OPTIONS'))
        self.w.cone_size.setValue(self.w.PREFS_.getpref('Preview cone size', 0.5, float, 'GUI_OPTIONS'))
        self.w.grid_size.setValue(self.w.PREFS_.getpref('Preview grid size', 0, float, 'GUI_OPTIONS'))
        self.w.color_foregrnd.setStyleSheet('background-color: {}'.format(self.w.PREFS_.getpref('Foreground', '#ffee06', str, 'COLOR_OPTIONS')))
        self.w.color_foregalt.setStyleSheet('background-color: {}'.format(self.w.PREFS_.getpref('Highlight', '#ffee06', str, 'COLOR_OPTIONS')))
        self.w.color_led.setStyleSheet('background-color: {}'.format(self.w.PREFS_.getpref('LED', '#ffee06', str, 'COLOR_OPTIONS')))
        self.w.color_backgrnd.setStyleSheet('background-color: {}'.format(self.w.PREFS_.getpref('Background', '#16160e', str, 'COLOR_OPTIONS')))
        self.w.color_backgalt.setStyleSheet('background-color: {}'.format(self.w.PREFS_.getpref('Background Alt', '#26261e', str, 'COLOR_OPTIONS')))
        self.w.color_frams.setStyleSheet('background-color: {}'.format(self.w.PREFS_.getpref('Frames', '#ffee06', str, 'COLOR_OPTIONS')))
        self.w.color_estop.setStyleSheet('background-color: {}'.format(self.w.PREFS_.getpref('Estop', '#ff0000', str, 'COLOR_OPTIONS')))
        self.w.color_disabled.setStyleSheet('background-color: {}'.format(self.w.PREFS_.getpref('Disabled', '#b0b0b0', str, 'COLOR_OPTIONS')))
        self.w.color_preview.setStyleSheet('background-color: {}'.format(self.w.PREFS_.getpref('Preview', '#000000', str, 'COLOR_OPTIONS')))
        self.soft_keyboard()
        self.cone_size_changed(self.w.cone_size.value())
        self.grid_size_changed(self.w.grid_size.value())

    def init_widgets(self):
        self.w.main_tab_widget.setCurrentIndex(0)
        self.w.preview_stack.setCurrentIndex(0)
        self.w.gcode_stack.setCurrentIndex(0)
        self.w.jog_stack.setCurrentIndex(0)
        self.w.gcode_progress.hide()
        self.w.jog_slider.setMaximum(INFO.MAX_LINEAR_JOG_VEL)
        self.w.jog_slider.setValue(INFO.DEFAULT_LINEAR_JOG_VEL)
#        self.w.slider_jog_angular.setMaximum(INFO.MAX_ANGULAR_JOG_VEL)
#        self.w.slider_jog_angular.setValue(INFO.DEFAULT_ANGULAR_JOG_VEL)
        self.w.chk_override_limits.setChecked(False)
        self.w.chk_override_limits.setEnabled(False)
        self.w.thc_enable.setChecked(self.w.PREFS_.getpref('THC enable', True, bool, 'ENABLE_OPTIONS'))
        self.w.cornerlock_enable.setChecked(self.w.PREFS_.getpref('Corner lock enable', True, bool, 'ENABLE_OPTIONS'))
        self.w.kerfcross_enable.setChecked(self.w.PREFS_.getpref('Kerf cross enable', True, bool, 'ENABLE_OPTIONS'))
        self.w.use_auto_volts.setChecked(self.w.PREFS_.getpref('Use auto volts', True, bool, 'ENABLE_OPTIONS'))
        self.w.ohmic_probe_enable.setChecked(self.w.PREFS_.getpref('Ohmic probe enable', False, bool, 'ENABLE_OPTIONS'))
        self.w.lbl_gcodes = STATLABEL()
        self.w.lbl_mcodes = STATLABEL()
        self.w.statusbar.addPermanentWidget(VLine())    # <---
        self.w.statusbar.addPermanentWidget(self.w.lbl_gcodes)
        self.w.statusbar.addPermanentWidget(VLine())    # <---
        self.w.statusbar.addPermanentWidget(self.w.lbl_mcodes)
        self.w.window().setWindowTitle('blah blah blah')
#        self.w.filemanager.button.setText('MEDIA')
#        self.w.filemanager.button2.setText('USER')
        self.font = QFont()
        self.font.setFamily('Lato')
        self.font.setFixedPitch(True)
        self.font.setBold(False)
        self.font.setPointSize(12)
        for i in range(0,4):
            self.w.gcode_display.lexer.setFont(self.font, i)
            self.w.gcode_editor.editor.lexer.setFont(self.font, i)
        self.font.setBold(True)
        self.w.gcode_display.font = self.font
        self.w.gcode_editor.editor.font = self.font
        self.w.gcode_display.set_margin_width(3)
        self.w.gcode_display.setBraceMatching(False)
        self.w.gcode_editor.set_margin_width(3)
        self.w.gcode_editor.editor.setBraceMatching(False)
        self.w.gcode_editor.editor.setCaretWidth(2)
        self.w.gcode_editor.editMode()
        self.w.gcodegraphics.set_alpha_mode(True)
        self.w.conv_preview.set_cone_basesize(0.1)
        self.w.conv_preview.set_view('Z')
        self.w.conv_preview.set_alpha_mode(True)
        self.w.conv_preview.setShowOffsets(False)
        self.w.conv_preview.setdro(False)
        self.w.conv_preview.inhibit_selection = True
        self.w.conv_preview.updateGL()
        self.pauseTimer = QTimer()
        self.pauseTimer.timeout.connect(self.pause_timeout)
        self.w.camview.cross_color = QtCore.Qt.red
        self.w.camview.cross_pointer_color = QtCore.Qt.red
        self.w.camview.font = QFont("arial,helvetica", 16)

    def touchoff_buttons(self):
        cCode = self.iniFile.find('PLASMAC', 'CAMERA_TOUCHOFF') or '0'
        if cCode == '0':
            self.w.camera.hide()
        else:
            try:
                parms = cCode.lower().split()
                if len(parms) == 2:
                    self.cam_offsetX = float(parms[0].replace('x', ''))
                    self.cam_offsetY = float(parms[1].replace('y', ''))
                    self.idleHomedList.append('camera')
                    self.w.camera.setEnabled(False)
                else:
                    self.w.camera.hide()
                    msg = '000 Invalid entry for camera offset'
                    self.dialog_error('INI FILE ERROR', msg)
            except:
                self.w.camera.hide()
                msg = '111 Invalid entry for camera offset'
                self.dialog_error('INI FILE ERROR', msg)

        lCode = self.iniFile.find('PLASMAC', 'LASER_TOUCHOFF') or '0'
        if lCode == '0':
            self.w.laser.hide()
        else:
            try:
                parms = lCode.lower().split()
                if len(parms) == 2:
                    self.laserOffsetX = float(parms[0].replace('x', ''))
                    self.laserOffsetY = float(parms[1].replace('y', ''))
                    self.idleHomedList.append('laser')
                    self.w.laser.setEnabled(False)
                else:
                    self.w.laser.hide()
                    msg = 'Invalid entry for laser offset'
                    self.dialog_error('INI FILE ERROR', msg)
            except:
                self.w.laser.hide()
                msg = 'Invalid entry for laser offset'
                self.dialog_error('INI FILE ERROR', msg)

    def closing_cleanup__(self):
        if not self.w.PREFS_: return
        self.w.PREFS_.putpref('Use keyboard shortcuts', self.w.chk_keyboard_shortcuts.isChecked(), bool, 'GUI_OPTIONS')
        self.w.PREFS_.putpref('Use soft keyboard', self.w.chk_soft_keyboard.isChecked(), bool, 'GUI_OPTIONS')
#        self.w.PREFS_.putpref('Run from line', self.w.chk_run_from_line.isChecked(), bool, 'GUI_OPTIONS')
#        self.w.PREFS_.putpref('Preview cone size', int(self.w.cone_size.value() * 100) / 100, float, 'GUI_OPTIONS')
        self.w.PREFS_.putpref('Preview cone size', self.w.cone_size.value(), float, 'GUI_OPTIONS')
#        self.w.PREFS_.putpref('Preview grid size', int(self.w.grid_size.value() * 100) / 100, float, 'GUI_OPTIONS')
        self.w.PREFS_.putpref('Preview grid size', self.w.grid_size.value(), float, 'GUI_OPTIONS')
        self.w.PREFS_.putpref('Foreground',  self.w.color_foregrnd.styleSheet().split(':')[1].strip(), str, 'COLOR_OPTIONS')
        self.w.PREFS_.putpref('Highlight', self.w.color_foregalt.styleSheet().split(':')[1].strip(), str, 'COLOR_OPTIONS')
        self.w.PREFS_.putpref('LED', self.w.color_led.styleSheet().split(':')[1].strip(), str, 'COLOR_OPTIONS')
        self.w.PREFS_.putpref('Background', self.w.color_backgrnd.styleSheet().split(':')[1].strip(), str, 'COLOR_OPTIONS')
        self.w.PREFS_.putpref('Background Alt', self.w.color_backgalt.styleSheet().split(':')[1].strip(), str, 'COLOR_OPTIONS')
        self.w.PREFS_.putpref('Frames', self.w.color_frams.styleSheet().split(':')[1].strip(), str, 'COLOR_OPTIONS')
        self.w.PREFS_.putpref('Estop', self.w.color_estop.styleSheet().split(':')[1].strip(), str, 'COLOR_OPTIONS')
        self.w.PREFS_.putpref('Disabled', self.w.color_disabled.styleSheet().split(':')[1].strip(), str, 'COLOR_OPTIONS')
        self.w.PREFS_.putpref('Preview', self.w.color_preview.styleSheet().split(':')[1].strip(), str, 'COLOR_OPTIONS')
        self.w.PREFS_.putpref('THC enable', self.w.thc_enable.isChecked(), bool, 'ENABLE_OPTIONS')
        self.w.PREFS_.putpref('Corner lock enable', self.w.cornerlock_enable.isChecked(), bool, 'ENABLE_OPTIONS')
        self.w.PREFS_.putpref('Kerf cross enable', self.w.kerfcross_enable.isChecked(), bool, 'ENABLE_OPTIONS')
        self.w.PREFS_.putpref('Use auto volts', self.w.use_auto_volts.isChecked(), bool, 'ENABLE_OPTIONS')
        self.w.PREFS_.putpref('Ohmic probe enable', self.w.ohmic_probe_enable.isChecked(), bool, 'ENABLE_OPTIONS')

    def processed_key_event__(self,receiver,event,is_pressed,key,code,shift,cntrl):
        # when typing in MDI, we don't want keybinding to call functions
        # so we catch and process the events directly.
        # We do want ESC, F1 and F2 to call keybinding functions though
        if code not in(Qt.Key_Escape,Qt.Key_F1 ,Qt.Key_F2):
                    #Qt.Key_F3,Qt.Key_F4,Qt.Key_F5):
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
                if isinstance(receiver2, EDITOR):
                    flag = True
                    break
                receiver2 = receiver2.parent()
            if flag:
                if isinstance(receiver2, EDITOR):
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
            self.add_alarm('Exception in KEYBINDING: {}'.format (e))
        except Exception as e:
            LOG.error('Exception in KEYBINDING:', exc_info=e)
            print('Error in, or no function for: {} in handler file for-{}'.format(KEYBIND.convert(event),key))
            return False


#############################################################################################################################
# CALLBACKS FROM STATUS #
#############################################################################################################################
    def power_state(self, state):
        if state:
            for widget in self.idleOnList:
                self.w[widget].setEnabled(True)
            if self.tpButton and not self.w.torch_enable.isChecked():
                self.w[self.tpButton].setEnabled(False)
            if STATUS.is_all_homed():
                for widget in self.idleHomedList:
                    self.w[widget].setEnabled(True)
                for widget in self.idleHomedPlusPausedList:
                    self.w[widget].setEnabled(True)
            else :
                for widget in self.idleHomedList:
                    self.w[widget].setEnabled(False)
                for widget in self.idleHomedPlusPausedList:
                    self.w[widget].setEnabled(False)
        else:
            for widget in self.idleOnList:
                self.w[widget].setEnabled(False)
            for widget in self.idleHomedList:
                self.w[widget].setEnabled(False)

    def interp_idle(self, obj):
        if self.single_cut_request:
            self.single_cut_request = False
            if self.oldFile:
                ACTION.OPEN_PROGRAM(self.oldFile)
            self.w[self.scButton].setEnabled(True)
        for widget in self.idleList:
            self.w[widget].setEnabled(True)
            if self.w.file_open.text() != 'OPEN':
                self.w.file_edit.setEnabled(False)
        if self.lastLoadedProgram == 'None':
            self.w.file_reload.setEnabled(False)
        if STATUS.machine_is_on():
            for widget in self.idleOnList:
                self.w[widget].setEnabled(True)
            if self.tpButton and not self.w.torch_enable.isChecked():
                self.w[self.tpButton].setEnabled(False)
            if STATUS.is_all_homed():
                for widget in self.idleHomedList:
                    self.w[widget].setEnabled(True)
                for widget in self.idleHomedPlusPausedList:
                    self.w[widget].setEnabled(True)
            else :
                for widget in self.idleHomedList:
                    self.w[widget].setEnabled(False)
                for widget in self.idleHomedPlusPausedList:
                    self.w[widget].setEnabled(False)
        else:
            for widget in self.idleOnList:
                self.w[widget].setEnabled(False)
            for widget in self.idleHomedList:
                self.w[widget].setEnabled(False)
        ACTION.SET_MANUAL_MODE()

    def interp_paused(self, obj):
        pass

    def interp_running(self, obj):
        for widget in self.idleList:
            self.w[widget].setEnabled(False)
        for widget in self.idleOnList:
            self.w[widget].setEnabled(False)
        for widget in self.idleHomedList:
            self.w[widget].setEnabled(False)
        for widget in self.idleHomedPlusPausedList:
            self.w[widget].setEnabled(False)
        if STATUS.is_auto_mode() and self.w.mdi_show.text() == 'MDI\nCLOSE':
            self.w.mdi_show.setText('MDI')
            self.w.gcode_stack.setCurrentIndex(0)

    def interp_reading(self, obj):
        pass

    def interp_waiting(self, obj):
        pass

    def pause_changed(self, obj, state):
        if state:
            self.w.run.setText('STEP')
            self.w.pause.setText('RESUME')
            self.pauseTimer.start(250)
            for widget in self.idleHomedPlusPausedList:
                self.w[widget].setEnabled(True)
            self.w.set_cut_recovery()
        elif not self.w.cut_rec_fwd.isDown() and not self.w.cut_rec_rev.isDown():
            self.w.jog_stack.setCurrentIndex(0)
            self.pauseTimer.stop()
            self.w.run.setText('CYCLE START')
            self.w.pause.setText('PAUSE')
            for widget in self.idleHomedPlusPausedList:
                self.w[widget].setEnabled(False)

    def pause_timeout(self):
        if self.w.pause.text() == '':
            self.w.pause.setText('RESUME')
        else:
            self.w.pause.setText('')

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

    def file_loaded(self, obj, filename):
        if filename is not None:
            self.w.gcode_progress.setValue(0)
            self.lastLoadedProgram = filename
            if not self.cameraOn:
                self.w.preview_stack.setCurrentIndex(0)
            self.w.file_open.setText(os.path.basename(filename))
            self.w.edit_label.setText('EDIT: {}'.format(filename))
            if self.w.mdi_show.text() == 'MDI\nCLOSE':
                self.w.mdi_show.setText('MDI')
                self.w.gcode_stack.setCurrentIndex(0)
            self.w.file_reload.setEnabled(True)
        else:
            self.add_alarm('Filename not valid')
        if self.single_cut_request:
            ACTION.RUN()
        self.w.file_edit.setEnabled(True)
        self.w.window().setWindowTitle('QtPlasmaC')

    def joints_all_homed(self, obj):
        hal.set_p('plasmac.homed', '1')
        self.interp_idle(None)

    def joint_homed(self, obj, joint):
        dro = self.coordinates.lower()[int(joint)]
        self.w['dro_{}'.format(dro)].setProperty('homed', True)
        self.w["dro_{}".format(dro)].setStyle(self.w["dro_{}".format(dro)].style())
        self.w.update
        STATUS.emit('dro-reference-change-request', 1)

    def joint_unhomed(self, obj, joints):
        for joint in joints:
            dro = self.coordinates.lower()[int(joint)]
            self.w['dro_{}'.format(dro)].setProperty('homed', False)
            self.w["dro_{}".format(dro)].setStyle(self.w["dro_{}".format(dro)].style())
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

    def gcodes_changed(self, obj, cod):
        self.w.lbl_gcodes.setText('G-Codes: {}'.format(cod))

    def mcodes_changed(self, obj, cod):
        self.w.lbl_mcodes.setText('M-Codes: {}'.format(cod))


###########################################################################################################################
# CALLBACKS FROM FORM #
###########################################################################################################################
    def user_button_pressed(self, button):
        self.user_button_down(button)

    def user_button_released(self, button):
        self.user_button_up(button)

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

    def backup_config_clicked(self):
        print('BACKUP CONFIGURATION NOT DONE YET')

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
        if self.w.mdi_show.text() == 'MDI\nCLOSE':
            self.w.mdi_show.setText('MDI')
            self.w.gcode_stack.setCurrentIndex(0)

    def file_edit_clicked(self):
        if STATUS.stat.interp_state == linuxcnc.INTERP_IDLE:
            self.w.preview_stack.setCurrentIndex(2)

    def mdi_show_clicked(self):
        if STATUS.is_on_and_idle() and STATUS.is_all_homed():
            if self.w.mdi_show.text() == 'MDI':
                self.w.mdi_show.setText('MDI\nCLOSE')
                self.w.gcode_stack.setCurrentIndex(1)
            else:
                self.w.mdi_show.setText('MDI')
                self.w.gcode_stack.setCurrentIndex(0)

    def file_cancel_clicked(self):
        self.w.preview_stack.setCurrentIndex(0)

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
        elif tab == 1:
            self.w.conv_preview.logger.clear()
            self.w.conv_preview.set_current_view()
            self.conv_setup()

    def file_reload_clicked(self):
        if ACTION.prefilter_path or self.lastLoadedProgram != 'None':
            file = ACTION.prefilter_path or self.lastLoadedProgram
            self.w.gcode_progress.setValue(0)
            ACTION.OPEN_PROGRAM(file)

    def jog_slow_clicked(self, state):
        slider = self.w.jog_slider
        current = slider.value()
        max = slider.maximum()
        if state:
            self.w.sender().setText('SLOW')
            slider.setMaximum(max / self.slowJogFactor)
            slider.setValue(current / self.slowJogFactor)
            slider.setPageStep(10)
        else:
            self.w.sender().setText('FAST')
            slider.setMaximum(max * self.slowJogFactor)
            slider.setValue(current * self.slowJogFactor)
            slider.setPageStep(100)

    def chk_override_limits_checked(self, state):
        if state:
            ACTION.SET_LIMITS_OVERRIDE()

#    def chk_run_from_line_checked(self, state):
#        if not state:
#            self.w.lbl_start_line.setText('1')


#########################################################################################################################
# GENERAL FUNCTIONS #
#########################################################################################################################
    def touch_off_xy(self, x, y):
        if STATUS.is_on_and_idle() and STATUS.is_all_homed():
            ACTION.CALL_MDI('G10 L20 P0 X{} Y{}'.format(x, y))
            if self.w.file_open.text() != 'OPEN':
                self.file_reload_clicked()
            ACTION.SET_MANUAL_MODE()

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
        self.w.arc_ok_high.setValue(self.w.PREFS_.getpref('Arc OK High', 99999, float, 'PLASMA_PARAMETERS'))
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
        self.w.file_reload.clicked.connect(self.file_reload_clicked)
        self.w.jog_slow.clicked.connect(self.jog_slow_clicked)
        self.w.chk_soft_keyboard.stateChanged.connect(self.soft_keyboard)
        self.w.torch_enable.stateChanged.connect(lambda w:self.torch_enable_changed(w))
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
        self.w.backup_config.clicked.connect(self.backup_config_clicked)
        self.w.save_material.clicked.connect(self.save_materials_clicked)
        self.w.reload_material.clicked.connect(self.reload_materials_clicked)
        self.w.new_material.clicked.connect(lambda:self.new_material_clicked(0, 0))
        self.w.delete_material.clicked.connect(self.delete_material_clicked)
        self.w.setup_feed_rate.valueChanged.connect(self.setup_feed_rate_changed)
        self.w.touch_xy.clicked.connect(self.touch_xy_clicked)
        self.w.materials_box.currentIndexChanged.connect(lambda w:self.material_changed(w))
        self.w.material_selector.currentIndexChanged.connect(lambda w:self.selector_changed(w))
        self.w.material_change.hal_pin_changed.connect(lambda w:self.material_change_pin_changed(w))
        self.w.material_change_number.hal_pin_changed.connect(lambda w:self.material_change_number_pin_changed(w))
        self.w.material_change_timeout.hal_pin_changed.connect(lambda w:self.material_change_timeout_pin_changed(w))
        self.w.material_reload.hal_pin_changed.connect(lambda w:self.material_reload_pin_changed(w))
        self.w.material_temp.hal_pin_changed.connect(lambda w:self.material_temp_pin_changed(w))
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
        self.w.cut_rec_speed.valueChanged.connect(lambda w:self.cutrec_speed_changed(w))
        self.w.cut_rec_fwd.pressed.connect(lambda:self.cutrec_motion(1))
        self.w.cut_rec_fwd.released.connect(lambda:self.cutrec_motion(0))
        self.w.cut_rec_rev.pressed.connect(lambda:self.cutrec_motion(-1))
        self.w.cut_rec_rev.released.connect(lambda:self.cutrec_motion(0))
        self.w.cut_rec_cancel.pressed.connect(self.cutrec_cancel_pressed)
        self.w.cut_rec_n.pressed.connect(lambda:self.cutrec_move(0, 1))
        self.w.cut_rec_ne.pressed.connect(lambda:self.cutrec_move(1, 1))
        self.w.cut_rec_e.pressed.connect(lambda:self.cutrec_move(1, 0))
        self.w.cut_rec_se.pressed.connect(lambda:self.cutrec_move(1, -1))
        self.w.cut_rec_s.pressed.connect(lambda:self.cutrec_move(0, -1))
        self.w.cut_rec_sw.pressed.connect(lambda:self.cutrec_move(-1, -1))
        self.w.cut_rec_w.pressed.connect(lambda:self.cutrec_move(-1, 0))
        self.w.cut_rec_nw.pressed.connect(lambda:self.cutrec_move(-1, 1))
        self.w.x_offset.hal_pin_changed.connect(lambda:self.cutrec_offset_changed(self.w.x_offset.hal_pin.get()))
        self.w.y_offset.hal_pin_changed.connect(lambda:self.cutrec_offset_changed(self.w.y_offset.hal_pin.get()))
        self.w.cam_mark.pressed.connect(self.cam_mark_pressed)
        self.w.cam_goto.pressed.connect(self.cam_goto_pressed)
        self.w.cam_zoom_plus.pressed.connect(self.cam_zoom_plus_pressed)
        self.w.cam_zoom_minus.pressed.connect(self.cam_zoom_minus_pressed)
        self.w.cam_dia_plus.pressed.connect(self.cam_dia_plus_pressed)
        self.w.cam_dia_minus.pressed.connect(self.cam_dia_minus_pressed)
        self.w.conv_line.pressed.connect(self.conv_line_pressed)
        self.w.conv_circle.pressed.connect(self.conv_circle_pressed)
        self.w.conv_triangle.pressed.connect(self.conv_triangle_pressed)
        self.w.conv_rectangle.pressed.connect(self.conv_rectangle_pressed)
        self.w.conv_polygon.pressed.connect(self.conv_polygon_pressed)
        self.w.conv_bolt.pressed.connect(self.conv_bolt_pressed)
        self.w.conv_slot.pressed.connect(self.conv_slot_pressed)
        self.w.conv_star.pressed.connect(self.conv_star_pressed)
        self.w.conv_gusset.pressed.connect(self.conv_gusset_pressed)
        self.w.conv_sector.pressed.connect(self.conv_sector_pressed)
        self.w.conv_rotate.pressed.connect(self.conv_rotate_pressed)
        self.w.conv_array.pressed.connect(self.conv_array_pressed)
        self.w.conv_new.pressed.connect(self.conv_new_pressed)
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

    def set_axes_and_joints(self):
        kinematics = self.iniFile.find('KINS', 'KINEMATICS').lower().replace('=','').replace('trivkins','').replace(' ','') or None
        kinstype = None
        self.coordinates = 'XYZ'
        if 'kinstype' in kinematics:
            kinstype = kinematics.lower().replace(' ','').split('kinstype')[1]
            if 'coordinates' in kinematics:
                kinematics = kinematics.lower().replace(' ','').split('kinstype')[0]
        if 'coordinates' in kinematics:
            self.coordinates = kinematics.split('coordinates')[1].upper()
# map axes to joints
        for joint in range(len(self.axisList)):
            if self.axisList[joint] == 'X':
                self.w.home_x.set_joint(self.coordinates.index(self.axisList[joint]))
                self.w.home_x.set_joint_number(self.coordinates.index(self.axisList[joint]))
                self.w.jog_x_minus.set_joint(self.coordinates.index(self.axisList[joint]))
                self.w.jog_x_plus.set_joint(self.coordinates.index(self.axisList[joint]))
            elif self.axisList[joint] == 'Y':
               self.w.home_y.set_joint(self.coordinates.index(self.axisList[joint]))
               self.w.home_y.set_joint_number(self.coordinates.index(self.axisList[joint]))
               self.w.jog_y_minus.set_joint(self.coordinates.index(self.axisList[joint]))
               self.w.jog_y_plus.set_joint(self.coordinates.index(self.axisList[joint]))
            elif self.axisList[joint] == 'Z':
                self.w.home_z.set_joint(self.coordinates.index(self.axisList[joint]))
                self.w.home_z.set_joint_number(self.coordinates.index(self.axisList[joint]))
                self.w.jog_z_minus.set_joint(self.coordinates.index(self.axisList[joint]))
                self.w.jog_z_plus.set_joint(self.coordinates.index(self.axisList[joint]))
            elif self.axisList[joint] == 'A':
                self.w.home_a.set_joint(self.coordinates.index(self.axisList[joint]))
                self.w.home_a.set_joint_number(self.coordinates.index(self.axisList[joint]))
                self.w.jog_a_minus.set_joint(self.coordinates.index(self.axisList[joint]))
                self.w.jog_a_plus.set_joint(self.coordinates.index(self.axisList[joint]))
# hide axis a if not being used
        if 'A' not in self.axisList:
            for i in self.axisAList:
                self.w[i].hide()
# see if home all button required
        for joint in range(len(self.coordinates)):
            if not self.iniFile.find('JOINT_{}'.format(joint), 'HOME_SEQUENCE'):
                self.w.home_all.hide()

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
                self.w.pid_p_gain_lbl.setText('Speed %')

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
            self.w.height_per_volt.setDecimals(4)
            self.w.height_per_volt.setSingleStep(0.0001)
            self.w.ohmic_probe_offset.setRange(-1.0, 1.0)
            self.w.ohmic_probe_offset.setDecimals(3)
            self.w.ohmic_probe_offset.setSingleStep(0.001)
            self.w.skip_ihs_distance.setRange(0.0, 40.0)
            self.w.skip_ihs_distance.setDecimals(1)
            self.w.skip_ihs_distance.setSingleStep(0.1)
            self.w.kerf_width.setRange(0.0, 1.0)
            self.w.kerf_width.setDecimals(4)
            self.w.kerf_width.setSingleStep(0.0001)
            self.w.cut_feed_rate.setRange(0.0, 999.0)
            self.w.cut_feed_rate.setDecimals(1)
            self.w.cut_feed_rate.setSingleStep(0.1)
            self.w.cut_height.setRange(0.0, 1.0)
            self.w.cut_height.setDecimals(3)
            self.w.cut_height.setSingleStep(0.001)
            self.w.pierce_height.setRange(0.0, 1.0)
            self.w.pierce_height.setDecimals(3)
            self.w.pierce_height.setSingleStep(0.001)
        else:
            self.w.setup_feed_rate.setMaximum(int(self.thcFeedRate))
            self.w.safe_height.setMaximum(int(self.maxHeight))
            self.w.probe_feed_rate.setMaximum(int(self.thcFeedRate))
            self.w.probe_start_height.setMaximum(int(self.maxHeight))

    def kb_jog(self, state, joint, direction, fast = False, linear = True):
        if not STATUS.is_man_mode() or not STATUS.machine_is_on():
            self.add_alarm('Machine must be ON and in Manual mode to jog')
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

    def keyboard_shortcuts(self):
        if self.w.chk_keyboard_shortcuts.isChecked():
            return True
        else:
            self.add_alarm('Keyboard shortcuts are disabled')
            return False

    def soft_keyboard(self):
        if self.w.chk_soft_keyboard.isChecked():
            self.w.mdihistory.soft_keyboard_option = True
            input = 'CALCULATOR'
        else:
            self.w.mdihistory.soft_keyboard_option = False
            input = 'ENTRY'
        for axis in 'xyza':
            button = 'touch_{}'.format(axis)
            self.w[button].dialog_code = input

    def dialog_error(self, title, error):
        msg = QMessageBox(self.w)
        msg.setIcon(QMessageBox.Critical)
        msg.setWindowTitle(title)
        msg.setText(error)
        msg.exec_()
        self.dialogError = False
        return msg

#########################################################################################################################
# USER BUTTON FUNCTIONS #
#########################################################################################################################
    def user_button_setup(self):
        self.iniButtonName = ['Names']
        self.iniButtonCode = ['Codes']
        self.probePressed = False
        self.probeTime = 0
        self.probeTimer = QTimer()
        self.probeTimer.timeout.connect(self.probe_timeout)
        self.ptButton = ''
        self.torchTimer = QTimer()
        self.torchTimer.timeout.connect(self.torch_timeout)
        self.tpButton = ''
        self.cutType = 0
        self.scButton = ''
        self.single_cut_request = False
        self.oldFile = None
        for button in range(1,9):
            bname = self.iniFile.find('PLASMAC', 'BUTTON_' + str(button) + '_NAME') or '0'
            self.iniButtonName.append(bname)
            code = self.iniFile.find('PLASMAC', 'BUTTON_' + str(button) + '_CODE') or ''
            self.iniButtonCode.append(code)
            if bname != '0':
                bname = bname.split('\\')
                blabel = bname[0]
                if len(bname) > 1:
                    for name in range(1, len(bname)):
                        blabel += '\n{}'.format(bname[name])
                self.w['button_{}'.format(str(button))].setText(blabel)
            if not code:
                self.w['button_{}'.format(str(button))].setText('')
                self.w['button_{}'.format(str(button))].setEnabled(False)
                continue
            elif 'change-consumables' in code:
                self.ccParm = self.iniFile.find('PLASMAC','BUTTON_' + str(button) + '_CODE').replace('change-consumables','').replace(' ','').lower() or None
                self.ccButton = 'button_{}'.format(str(button))
                self.idleHomedPlusPausedList.append(self.ccButton)
                self.w[self.ccButton].setEnabled(False)
            elif 'ohmic-test' in code:
                self.otButton = 'button_{}'.format(str(button))
                self.idleOnList.append(self.otButton)
                self.w[self.otButton].setEnabled(False)
            elif 'probe-test' in code:
                self.ptButton = 'button_{}'.format(str(button))
                self.idleHomedList.append(self.ptButton)
                self.w[self.ptButton].setEnabled(False)
            elif 'torch-pulse' in code:
                self.tpButton = 'button_{}'.format(str(button))
                self.idleOnList.append(self.tpButton)
                self.w[self.tpButton].setEnabled(False)
            elif 'cut-type' in code:
                self.ctButton = 'button_{}'.format(str(button))
                self.idleOnList.append(self.ctButton)
                self.w[self.ctButton].setEnabled(False)
            elif 'load' in code:
                self.lpButton = 'button_{}'.format(str(button))
                self.idleOnList.append(self.lpButton)
                self.w[self.lpButton].setEnabled(False)
            elif 'toggle-halpin' in code:
                self.thButton = 'button_{}'.format(str(button))
                self.idleOnList.append(self.thButton)
                self.w[self.thButton].setEnabled(False)
            elif 'single-cut' in code:
                self.scButton = 'button_{}'.format(str(button))
                self.idleHomedList.append(self.scButton)
                self.w[self.scButton].setEnabled(False)
            else:
                for command in code.split('\\'):
                    if command.strip()[0] != '%':
                        self.idleHomedList.append('button_{}'.format(str(button)))
                        self.w['button_{}'.format(str(button))].setEnabled(False)
                        continue

    def user_button_down(self, button):
        bNum = button
        commands = self.iniButtonCode[bNum]
        if not commands: return
        if 'change-consumables' in commands.lower():
            self.consumable_change_setup()
            if hal.get_value('axis.x.eoffset-counts') or hal.get_value('axis.y.eoffset-counts'):
                print('reset consumable change')
                hal.set_p('plasmac.consumable-change', '0')
                hal.set_p('plasmac.x-offset', '0')
                hal.set_p('plasmac.y-offset', '0')
                self.button_normal(self.ccButton)
            else:
                if self.ccFeed == 'None' or self.ccFeed < 1:
                    self.dialog_error('USER BUTTON ERROR', 'Invalid feed rate for consumable change\n\nCheck .ini file settings\n\nBUTTON_{}_CODE'.format(str(button)))
                    return
                else:
                    hal.set_p('plasmac.xy-feed-rate', str(float(self.ccFeed)))
                if self.ccXpos == 'None':
                    self.ccXpos = STATUS.get_position()[0][0]
                if self.ccXpos < round(float(self.iniFile.find('AXIS_X', 'MIN_LIMIT')), 6) + (10 * self.unitsPerMm):
                    self.ccXpos = round(float(self.iniFile.find('AXIS_X', 'MIN_LIMIT')), 6) + (10 * self.unitsPerMm)
                elif self.ccXpos > round(float(self.iniFile.find('AXIS_X', 'MAX_LIMIT')), 6) - (10 * self.unitsPerMm):
                    self.ccXpos = round(float(self.iniFile.find('AXIS_X', 'MAX_LIMIT')), 6) - (10 * self.unitsPerMm)
                if self.ccYpos == 'None':
                    self.ccYpos = STATUS.get_position()[0][1]
                if self.ccYpos < round(float(self.iniFile.find('AXIS_Y', 'MIN_LIMIT')), 6) + (10 * self.unitsPerMm):
                    self.ccYpos = round(float(self.iniFile.find('AXIS_Y', 'MIN_LIMIT')), 6) + (10 * self.unitsPerMm)
                elif self.ccYpos > round(float(self.iniFile.find('AXIS_Y', 'MAX_LIMIT')), 6) - (10 * self.unitsPerMm):
                    self.ccYpos = round(float(self.iniFile.find('AXIS_Y', 'MAX_LIMIT')), 6) - (10 * self.unitsPerMm)
                hal.set_p('plasmac.x-offset', '{:.0f}'.format((self.ccXpos - STATUS.get_position()[0][0]) / hal.get_value('plasmac.offset-scale')))
                hal.set_p('plasmac.y-offset', '{:.0f}'.format((self.ccYpos - STATUS.get_position()[0][1]) / hal.get_value('plasmac.offset-scale')))
                hal.set_p('plasmac.consumable-change', '1')
                self.button_active(self.ccButton)
        elif 'ohmic-test' in commands.lower():
            hal.set_p('plasmac.ohmic-test','1')
        elif 'probe-test' in commands.lower():
            if not self.probeTime and \
               self.probeTimer.remainingTime() <= 0 and \
               not hal.get_value('plasmac.z-offset-counts'):
                self.probeTime = 30
                if commands.lower().replace('probe-test','').strip():
                    self.probeTime = int(commands.lower().replace('probe-test','').strip())
                self.probeTimer.start(1000)
                hal.set_p('plasmac.probe-test','1')
                self.probeText = self.w[self.ptButton].text()
                self.w[self.ptButton].setText('{}'.format(self.probeTime))
                self.button_active(self.ptButton)
            else:
                self.probeTimer.stop()
                self.probeTime = 0
                hal.set_p('plasmac.probe-test','0')
                self.w[self.ptButton].setText(self.probeText)
                self.button_normal(self.ptButton)
        elif 'torch-pulse' in commands.lower():
            if self.w.torch_enable.isChecked() and not hal.get_value('plasmac.torch-on'):
                torchTime = 1.0
                if commands.lower().replace('torch-pulse','').strip():
                    torchTime = float(commands.lower().replace('torch-pulse','').strip())
                    torchTime = 3.0 if torchTime > 3.0 else torchTime
                self.torchTimer.start(torchTime * 1000)
                hal.set_p('plasmac.torch-pulse-time', str(torchTime))
                hal.set_p('plasmac.torch-pulse-start', '1')
                self.button_active(self.tpButton)
            else:
                self.torchTimer.stop()
                hal.set_p('plasmac.torch-pulse-time', '0')
                hal.set_p('plasmac.torch-pulse-start','0')
                self.button_normal(self.tpButton)
        elif 'cut-type' in commands.lower():
            self.cutType ^= 1
            if self.cutType:
                hal.set_p('qtplasmac.cut_type','1')
                self.button_active(self.ctButton)
                self.cutTypeText = self.w[self.ctButton].text()
                self.w[self.ctButton].setText('PIERCE\nONLY')
            else:
                hal.set_p('qtplasmac.cut_type','0')
                self.button_normal(self.ctButton)
                self.w[self.ctButton].setText(self.cutTypeText)
            self.w.gcode_progress.setValue(0)
            if self.w.file_open.text() != 'OPEN':
                self.file_reload_clicked()
        elif 'load' in commands.lower():
            lFile = '{}/{}'.format(self.programPrefix, commands.split('load')[1].strip())
            self.w.gcode_progress.setValue(0)
            ACTION.OPEN_PROGRAM(lFile)
        elif 'toggle-halpin' in commands.lower():
            halpin = commands.lower().split('toggle-halpin')[1].strip()
            pinstate = hal.get_value(halpin)
            hal.set_p(halpin, str(not pinstate))
            if pinstate:
                self.button_normal(self.thButton)
            else:
                self.button_active(self.thButton)
        elif 'single-cut' in commands.lower():
            self.do_single_cut()
        else:
            for command in commands.split('\\'):
                if command.strip()[0] == '%':
                    command = command.strip().strip('%') + '&'
                    msg = Popen(command,stdout=PIPE,stderr=PIPE, shell=True)
                    print(msg.communicate()[0])
                else:
                    if '{' in command:
                        newCommand = subCommand = ''
                        for char in command:
                            if char == '{':
                                subCommand = ':'
                            elif char == '}':
                                f1, f2 = subCommand.replace(':',"").split()
                                newCommand += self.iniFile.find(f1,f2)
                                subCommand = ''
                            elif subCommand.startswith(':'):
                                subCommand += char
                            else:
                                newCommand += char
                        command = newCommand
                    if STATUS.is_on_and_idle() and STATUS.is_all_homed():
                        ACTION.CALL_MDI(command)
                        if command.lower().replace(' ', '').startswith('g10l20'):
                            self.file_reload_clicked()
                        ACTION.SET_MANUAL_MODE()

    def user_button_up(self, button):
        bNum = button
        commands = self.iniButtonCode[bNum]
        if not commands: return
        if 'ohmic-test' in commands.lower():
            hal.set_p('plasmac.ohmic-test','0')
        elif 'torch-pulse' in commands.lower():
            hal.set_p('plasmac.torch-pulse-start','0')

    def torch_enable_changed(self, state):
        if self.tpButton:
            if state and STATUS.is_interp_idle():
                self.w[self.tpButton].setEnabled(True)
            else:
                self.w[self.tpButton].setEnabled(False)

    def probe_timeout(self):
        if self.probeTime > 1:
            self.probeTime -= 1
            self.probeTimer.start(1000)
            self.w[self.ptButton].setText('{}'.format(self.probeTime))
        else:
            self.probeTimer.stop()
            self.probeTime = 0
            hal.set_p('plasmac.probe-test','0')
            self.w[self.ptButton].setText(self.probeText)
            self.button_normal(self.ptButton)

    def torch_timeout(self):
        self.torchTimer.stop()
        hal.set_p('plasmac.torch-pulse-start','0')
        hal.set_p('plasmac.torch-pulse-time', '0')
        self.button_normal(self.tpButton)

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

    def do_single_cut(self):
        self.w[self.scButton].setEnabled(False)
        sC = QDialog(self.w)
        sC.setWindowTitle('SINGLE CUT')
        l1 = QLabel('X Length:')
        singleX = QDoubleSpinBox()
        singleX.setAlignment(Qt.AlignRight)
        singleX.setMinimum(-9999)
        singleX.setMaximum(9999)
        singleX.setDecimals(1)
        l2 = QLabel('Y Length:')
        singleY = QDoubleSpinBox()
        singleY.setAlignment(Qt.AlignRight)
        singleY.setMinimum(-9999)
        singleY.setMaximum(9999)
        singleY.setDecimals(1)
        l3 = QLabel('')
        buttons = QDialogButtonBox.Ok | QDialogButtonBox.Cancel
        buttonBox = QDialogButtonBox(buttons)
        buttonBox.accepted.connect(sC.accept)
        buttonBox.rejected.connect(sC.reject)
        buttonBox.button(QDialogButtonBox.Ok).setText('Start Cut')
        buttonBox.button(QDialogButtonBox.Ok).setIcon(QIcon())
        buttonBox.button(QDialogButtonBox.Cancel).setText('Cancel')
        buttonBox.button(QDialogButtonBox.Cancel).setIcon(QIcon())
        layout = QVBoxLayout()
        layout.addWidget(l1)
        layout.addWidget(singleX)
        layout.addWidget(l2)
        layout.addWidget(singleY)
        layout.addWidget(l3)
        layout.addWidget(buttonBox)
        sC.setLayout(layout)
        singleX.setValue(self.w.PREFS_.getpref('X length', 0.0, float, 'SINGLE CUT'))
        singleY.setValue(self.w.PREFS_.getpref('Y length', 0.0, float, 'SINGLE CUT'))
        result = sC.exec_()
        if not result:
            self.w[self.scButton].setEnabled(True)
            return
        self.w.PREFS_.putpref('X length', singleX.value(), float, 'SINGLE CUT')
        self.w.PREFS_.putpref('Y length', singleY.value(), float, 'SINGLE CUT')
        self.oldFile = ACTION.prefilter_path if ACTION.prefilter_path else None
        newFile = '{}single_cut.ngc'.format(self.tmpPath)
        with open(newFile, 'w') as f:
            f.write('M3 $0 S1\n')
            f.write('G91\n')
            f.write('G1 X{} Y{} F#<_hal[plasmac.cut-feed-rate]>\n'.format(singleX.value(), singleY.value()))
            f.write('G90\n')
            f.write('M5 $0\n')
            f.write('M2\n')
        self.single_cut_request = True
        ACTION.OPEN_PROGRAM(newFile)

    def button_active(self, button):
        self.w[button].setStyleSheet( \
                    'QPushButton {{ color: {0}; background: {1} }} \
                     QPushButton:pressed {{ color: {0}; background: {1} }} \
                     QPushButton:disabled {{ color: {2}}}' \
                     .format(self.backColor, self.highColor, self.inactive))

    def button_normal(self, button):
        self.w[button].setStyleSheet( \
                    'QPushButton {{ color: {0}; background: {1} }} \
                     QPushButton:pressed {{ color: {0}; background: {1} }} \
                     QPushButton:disabled {{ color: {2}}}' \
                     .format(self.foreColor, self.backColor, self.inactive))


#########################################################################################################################
# MATERIAL HANDLING FUNCTIONS #
#########################################################################################################################
    def save_materials_clicked(self):
        material = self.w.material_change_number.hal_pin.get()
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
        self.w.material_reload.hal_pin.set(0)

    def new_material_clicked(self, repeat, value):
        text = 'New Material Number:'
        while(1):
            num = QInputDialog.getText(self.w, 'Add New Material', '{}'.format(text))
            if not num[1]:
                return
            try:
                if num != 0:
                    num = int(num[0])
            except:
                text = '{} is not a valid number.\n\nNew Material Number:'.format(num[0])
                continue
            if num == 0 or num in self.materialNumList:
                text = 'Material #{} is in use.\n\nNew Material Number:'.format(num)
                continue
            break
        text = 'New Naterial Name:'
        while(1):
            nam = QInputDialog.getText(self.w, 'Add New Material', '{}'.format(text))
            if not nam[1]:
                return
            if not nam[0]:
                text = 'Material name is required.\n\nNew Material Name:'.format(num)
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
                    outFile.write('NAME               = {}\n'.format(nam[0]))
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
        text = 'Material Number To Delete:'
        while(1):
            num = QInputDialog.getText(self.w, 'Delete Material', '{}'.format(text))
            if not num[1]:
                return
            try:
                if num != 0:
                    num = int(num[0])
            except:
                text = '{} is not a valid number.\n\nMaterial Number To Delete:'.format(num[0])
                continue
            if num == 0:
                text = 'Default material cannot be deleted.\n\nMaterial Number To Delete:'
                continue
            if num not in self.materialNumList:
                text = 'Material #{} does not exist.\n\nMaterial Number To Delete:'.format(num)
                continue
            break
        result = QMessageBox.question(self.w,
                                     'Warning!!',
                                     'Do you really want to delete material #{}?\n'.format(num),
                                     QMessageBox.Yes | QMessageBox.No)
        if result != QMessageBox.Yes:
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

    def material_changed(self, index):
        if self.w.materials_box.currentText():
            if self.getMaterialBusy:
                self.w.material_change.hal_pin.set(0)
                self.autoChange = False
                return
            material = int(self.w.materials_box.currentText().split(': ', 1)[0])
            if self.autoChange:
                hal.set_p('motion.digital-in-03','0')
                self.change_material(material)
                self.w.material_change.hal_pin.set(2)
                hal.set_p('motion.digital-in-03','1')
            else:
                self.change_material(material)
            self.w.material_selector.setCurrentIndex(self.w.materials_box.currentIndex())
        self.autoChange = False

    def material_change_pin_changed(self, halpin):
        if halpin == 0:
            hal.set_p('motion.digital-in-03','0')

    def material_change_number_pin_changed(self, halpin):
        if self.getMaterialBusy:
            return
        if self.w.material_change.hal_pin.get() == 1:
            self.autoChange = True
            # material already loaded so do a phantom handshake
            if halpin < 0:
                self.w.material_change.hal_pin.set(2)
                hal.set_p('motion.digital-in-03','1')
                self.w.material_change_number.hal_pin.set(halpin * -1)
                return
        if not self.material_exists(halpin):
            self.autoChange = False
            return
        self.w.materials_box.setCurrentIndex(self.materialList.index(halpin))

    def material_change_timeout_pin_changed(self, halpin):
        if halpin:
            material = int(self.w.materials_box.currentText().split(': ', 1)[0])
#           FIX_ME do we need to stop the program if a timeout occurs???
            print('\nMaterial change timeout occured for material #{}'.format(material))
            self.w.material_change_number.hal_pin.set(material)
            self.w.material_change_timeout.hal_pin.set(0)
            hal.set_p('motion.digital-in-03','0')

    def material_reload_pin_changed(self, halpin):
        if halpin:
            self.reload_materials_clicked()

    def material_temp_pin_changed(self, halpin):
        if halpin:
            t_number = 0
            t_name = 'Temporary'
            t_item = 0
            with open(self.tmpMaterialFileGCode, 'r') as f_in:
                for line in f_in:
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
            self.write_materials(0, 'Temporary', k_width, p_height, \
                                 p_delay, pj_height, pj_delay, c_height, c_speed, \
                                 c_amps, c_volts, pause, g_press, c_mode, 0)
            self.display_materials()
            self.change_material(0)
            self.w.materials_box.setCurrentIndex(0)
            self.w.material_temp.hal_pin.set(0)

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
        for key in sorted(self.materialFileDict):
            self.w.materials_box.addItem('{:05d}: {}'.format(key, self.materialFileDict[key][0]))
            self.w.material_selector.addItem('Material = {:05d}: {}'.format(key, self.materialFileDict[key][0]))
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
            self.w.material_change_number.hal_pin.set(material)

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
        with open(self.materialFile, 'r') as f_in:
            firstpass = True
            t_item = 0
            required = ['PIERCE_HEIGHT', 'PIERCE_DELAY', 'CUT_HEIGHT', 'CUT_SPEED']
            received = []
            for line in f_in:
                if line.startswith('#'):
                    continue
                elif line.startswith('[MATERIAL_NUMBER_') and line.strip().endswith(']'):
                    newMaterial = True
                    if not firstpass:
                        self.write_materials(t_number,t_name,k_width,p_height,p_delay,pj_height,pj_delay,c_height,c_speed,c_amps,c_volts,pause,g_press,c_mode,t_item)
                        for item in required:
                            if item not in received:
                                self.dialog_error('MATERIALS ERROR', '\n{} is missing from Material #{}'.format(item, t_number))
                    firstpass = False
                    t_number = int(line.rsplit('_', 1)[1].strip().strip(']'))
                    self.materialNumList.append(t_number)
                    t_name = k_width = p_height = p_delay = pj_height = pj_delay = c_height = c_speed = c_amps = c_volts =  pause = g_press = c_mode = 0.0
                    t_item += 1
                    received = []
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
                        self.dialog_error('MATERIALS ERROR', '\nNo value for PIERCE_HEIGHT in Material #{}'.format(t_number))
                elif line.startswith('PIERCE_DELAY'):
                    received.append('PIERCE_DELAY')
                    if line.split('=')[1].strip():
                        p_delay = float(line.split('=')[1].strip())
                    else:
                        self.dialog_error('MATERIALS ERROR', '\nNo value for PIERCE_DELAY in Material #{}'.format(t_number))
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
                        self.dialog_error('MATERIALS ERROR', '\nNo value for CUT_HEIGHT in Material #{}'.format(t_number))
                elif line.startswith('CUT_SPEED'):
                    received.append('CUT_SPEED')
                    if line.split('=')[1].strip():
                        c_speed = float(line.split('=')[1].strip())
                    else:
                        self.dialog_error('MATERIALS ERROR', '\nNo value for CUT_SPEED in Material #{}'.format(t_number))
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
            if not firstpass:
                self.write_materials(t_number,t_name,k_width,p_height,p_delay,pj_height,pj_delay,c_height,c_speed,c_amps,c_volts,pause,g_press,c_mode,t_item)
                for item in required:
                    if item not in received:
                        self.dialog_error('MATERIALS ERROR', '\n{} is missing from Material #{}'.format(item, t_number))
        self.display_materials()
        self.change_material(0)
        self.getMaterialBusy = 0

    def check_material_file(self):
        # create a new material file if it doesn't exist
        if not os.path.exists(self.materialFile):
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
            print('*** creating new material file, {}'.format(self.materialFile))

    def material_exists(self, material):
        if int(material) in self.materialList:
            return True
        else:
            if self.autoChange:
                self.w.material_change.hal_pin.set(-1)
                self.w.material_change_number.hal_pin.set(int(self.w.materials_box.currentText().split(': ', 1)[0]))
            self.dialog_error('MATERIALS ERROR', '\nMaterial #{} not in material list'.format(int(material)))
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
                0, 'Default' , \
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

#################################################################################################################################
# CAMERA AND LASER FUNCTIONS #
#################################################################################################################################
    def camera_pressed(self):
        # camview rotation is opposite direction to cartesian polar coordinates
        self.w.camview.rotation = 0.0 if STATUS.stat.rotation_xy == 0 else 360 - STATUS.stat.rotation_xy
        if self.w.preview_stack.currentIndex() != 3:
            self.w.preview_stack.setCurrentIndex(3)
            self.button_active('camera')
            self.cameraOn = True
        else:
            self.w.preview_stack.setCurrentIndex(0)
            self.button_normal('camera')
            self.cameraOn = False

    def laser_pressed(self):
        if self.w.laser.text() == 'LASER':
            self.w.laser_on.hal_pin.set(1)
            self.w.laser.setText('MARK\nEDGE')
            return
        elif self.w.laser.text() == 'SET\nORIGIN':
            self.w.laser_on.hal_pin.set(0)
        self.sheet_align(self.w.laser, self.laserOffsetX, self.laserOffsetY)

    def sheet_align(self, button, offsetX, offsetY):
        if button.text() == 'MARK\nEDGE':
            self.w.cam_goto.setEnabled(False)
            button.setText('SET\nORIGIN')
            self.camCurrentX = STATUS.get_position()[0][0]
            self.camCurrentY = STATUS.get_position()[0][1]
            zAngle = 0
        else:
            if button == self.w.cam_mark:
                button.setText('MARK\nEDGE')
            else:
                button.setText('LASER')
            xDiff = STATUS.get_position()[0][0] - self.camCurrentX
            yDiff = STATUS.get_position()[0][1] - self.camCurrentY
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
            # camview rotation is opposite direction to cartesian polar coordinates
            self.w.camview.rotation = 360 - zAngle
            ACTION.CALL_MDI_WAIT('G10 L2 P0 R{}'.format(zAngle), 3)
            ACTION.CALL_MDI_WAIT('G10 L20 P0 X{} Y{}'.format(offsetX, offsetY), 3)
            if self.w.file_open.text() != 'OPEN':
                self.file_reload_clicked()
                self.w.gcodegraphics.logger.clear()
            self.w.cam_goto.setEnabled(True)
            ACTION.SET_MANUAL_MODE()

    def cam_mark_pressed(self):
        self.sheet_align(self.w.cam_mark, self.cam_offsetX, self.cam_offsetY)

    def cam_goto_pressed(self):
        if self.w.cam_goto.text() == 'GOTO\nORIGIN':
            ACTION.CALL_MDI_WAIT('G0 X0 Y0')
            ACTION.SET_MANUAL_MODE()
        else:
            self.w.cam_goto.setText('GOTO\nORIGIN')
            self.w.cam_mark.setText('MARK\nEDGE')
            self.w.camview.rotation = 0
            ACTION.CALL_MDI_WAIT('G10 L2 P0 R0', 0.5)
            if self.w.file_open.text() != 'OPEN':
                self.file_reload_clicked()
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
        if self.w.camview.size().height() > self.w.camview.size().width():
            size = self.w.camview.size().height()
        else:
            size = self.w.camview.size().width()
        if self.w.camview.diameter >= size:
            return
        self.w.camview.diameter += 2

    def cam_dia_minus_pressed(self):
        if self.w.camview.diameter <= 2:
            return
        self.w.camview.diameter -= 2


#########################################################################################################################
# POWERMAX COMMUNICATIONS FUNCTIONS #
#########################################################################################################################
    def rs485_timeout(self):
        self.w.pmx485_enable.setChecked(False)
        self.pmx485Connected = False
        self.rs485Timer.stop()
        self.dialog_error('COMMUNICATIONS ERROR', \
                          '\nCould not connect to Powermax\n' \
                          '\nCheck cables and connections\n' \
                          '\nCheck PM_PORT in .ini file\n')

    def pmx485_check(self):
        if self.iniFile.find('PLASMAC', 'PM_PORT'):
            self.w.pmx485_enable.setChecked(True)
            if not hal.component_exists('pmx485'):
                self.dialog_error('COMMUNICATIONS ERROR', '\npmx485 component is not loaded.\n' \
                                                  '\nPowermax communications is not available\n')
                return
            self.w.pmx485Status = False
            self.w.pmx485_enable.stateChanged.connect(lambda w:self.pmx485_enable_changed(self.w.pmx485_enable.isChecked()))
            self.w.pmx485_status.hal_pin_changed.connect(lambda w:self.pmx485_status_changed(w))
            self.w.pmx485_mode.hal_pin_changed.connect(self.pmx485_mode_changed)
            self.w.pmx485_fault.hal_pin_changed.connect(lambda w:self.pmx485_fault_changed(w))
            self.w.pmx485_current_min.hal_pin_changed.connect(self.pmx485_min_max_changed)
            self.w.pmx485_current_max.hal_pin_changed.connect(self.pmx485_min_max_changed)
            self.w.pmx485_pressure_min.hal_pin_changed.connect(self.pmx485_min_max_changed)
            self.w.pmx485_pressure_max.hal_pin_changed.connect(self.pmx485_min_max_changed)
            self.w.gas_pressure.valueChanged.connect(self.pmx485_pressure_changed)
            self.w.mesh_enable.stateChanged.connect(lambda w:self.pmx485_mesh_enable_changed(self.w.mesh_enable.isChecked()))
            pinsComp = ['pmx485.enable', 'pmx485.status', 'pmx485.fault', \
                        'pmx485.mode_set', 'pmx485.mode', \
                        'pmx485.current_set', 'pmx485.current', 'pmx485.current_min', 'pmx485.current_max', \
                        'pmx485.pressure_set', 'pmx485.pressure', 'pmx485.pressure_min', 'pmx485.pressure_max']
            pinsSelf = ['pmx485_enable', 'pmx485_status', 'pmx485_fault', \
                        'cut_mode-f', 'pmx485_mode', \
                        'cut_amps-f', 'pmx485_current', 'pmx485_current_min', 'pmx485_current_max', \
                        'gas_pressure-f', 'pmx485_pressure', 'pmx485_pressure_min', 'pmx485_pressure_max']
            pinType = [hal.HAL_BIT, hal.HAL_BIT, hal.HAL_FLOAT, \
                       hal.HAL_FLOAT, hal.HAL_FLOAT, \
                       hal.HAL_FLOAT, hal.HAL_FLOAT, hal.HAL_FLOAT, hal.HAL_FLOAT, \
                       hal.HAL_FLOAT, hal.HAL_FLOAT, hal.HAL_FLOAT, hal.HAL_FLOAT]
            for pin in pinsComp:
                hal.new_sig('plasmac:{}'.format(pin.replace('pmx485.', 'pmx485_')), pinType[pinsComp.index(pin)])
                hal.connect(pin,'plasmac:{}'.format(pin.replace('pmx485.', 'pmx485_')))
                hal.connect('qtplasmac.{}'.format(pinsSelf[pinsComp.index(pin)]),'plasmac:{}'.format(pin.replace('pmx485.', 'pmx485_')))
            self.pressure = self.w.gas_pressure.value()
            self.rs485Timer = QTimer()
            self.rs485Timer.timeout.connect(self.rs485_timeout)
            self.meshMode = False
            self.oldCutMode = self.w.cut_mode.value()
            self.pmx485_mesh_enable_changed(self.w.mesh_enable.isChecked())
            self.pmx485_enable_changed(self.w.pmx485_enable.isChecked())
            self.w.cut_amps.setStatusTip('Powermax cutting current')
        else:
            self.w.gas_pressure.hide()
            self.w.gas_pressure_label.hide()
            self.w.cut_mode.hide()
            self.w.cut_mode_label.hide()
            self.w.pmx485_frame.hide()

    def pmx485_enable_changed(self, state):
        if state:
            if not hal.component_exists('pmx485'):
                port = self.iniFile.find('PLASMAC', 'PM_PORT')
                try:
                    Popen('halcmd loadusr -Wn pmx485 ./plasmac/pmx485.py {}'.format(port), stdout = PIPE, shell = True)
                except:
                    self.dialog_error('COMMUNICATIONS ERROR', '\npmx485 component is not loaded.\n' \
                                                      '\nPowermax communications is not available\n')
                    return
                timeout = time.time() + 3
                self.w.pmx485_label.setText('CONNECTING')
                while 1:
                    time.sleep(0.1)
                    if time.time() > timeout:
                        self.w.pmx485_enable.setChecked(False)
                        self.w.pmx485_label.setText('')
                        self.w.pmx485_label.setStatusTip('status of pmx485 communications')
                        self.dialog_error('COMMUNICATIONS ERROR', '\nTimeout while reconnecting\n\nCheck cables and connections\n\nThen re-enable\n')
                        return
                    if hal.component_exists('pmx485'):
                        print('pmx485 component reloaded') 
                        pinsComp = ['pmx485.enable', 'pmx485.status', 'pmx485.fault', \
                                    'pmx485.mode_set', 'pmx485.mode', \
                                    'pmx485.current_set', 'pmx485.current', 'pmx485.current_min', 'pmx485.current_max', \
                                    'pmx485.pressure_set', 'pmx485.pressure', 'pmx485.pressure_min', 'pmx485.pressure_max']
                        for pin in pinsComp:
                            hal.connect(pin,'plasmac:{}'.format(pin.replace('pmx485.', 'pmx485_')))
                        break
            if self.w.cut_mode.value() == 0 or self.w.cut_amps.value() == 0:
                self.dialog_error('MATERIALS ERROR', '\nInvalid Cut Mode or Cut Amps\n\nCannot connect to Powermax\n')
                self.w.pmx485_enable.setChecked(False)
                self.pmx485Loaded = False
                return
            else:
                self.pmx485Loaded = True
                self.w.pmx485_enable.setChecked(True)
                self.rs485Timer.stop()
        else:
            self.pmx485Connected = False
            self.w.pmx485_label.setText('')
            self.w.pmx485_label.setStatusTip('status of pmx485 communications')

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
        self.w.cut_amps.setMinimum(self.w.pmx485_current_min.hal_pin.get())
        self.w.cut_amps.setMaximum(self.w.pmx485_current_max.hal_pin.get())
        self.gas_minimum = self.w.pmx485_pressure_min.hal_pin.get()
        self.gas_maximum = self.w.pmx485_pressure_max.hal_pin.get()
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
                self.w.pmx485_label.setText('CONNECTED')
                self.pmx485Connected = True
                self.rs485Timer.stop()
            else:
                self.w.pmx485_label.setText('COMMS ERROR')
                self.pmx485Connected = False
                self.rs485Timer.start(5000)

    def pmx485_fault_changed(self, fault):
        if self.pmx485Connected:
            faultRaw = '{:04.0f}'.format(fault)
            faultCode = '{}-{}-{}'.format(faultRaw[0], faultRaw[1:3], faultRaw[3])
            if faultRaw == '0000':
                self.w.pmx485_label.setText('CONNECTED')
                self.w.pmx485_label.setStatusTip('status of pmx485 communications')
                self.w.pmx485_label.setStyleSheet('QLabel {{ color: {} }}'.format(self.w.color_foregrnd.styleSheet().split(':')[1].strip()))
            elif faultRaw in self.pmx485FaultName.keys():
                if faultRaw == '0210' and self.w.pmx485.current_max.value() > 110:
                    faultMsg = self.pmx485FaultName[faultRaw][1]
                elif faultRaw == '0210':
                    faultMsg = self.pmx485FaultName[faultRaw][0]
                else:
                    faultMsg = self.pmx485FaultName[faultRaw]
                if faultRaw != self.fault:
                    self.fault = faultRaw
                    self.w.pmx485_label.setText('Fault Code: {}'.format(faultCode))
                    self.w.pmx485_label.setStatusTip('Powermax error ({}) {}'.format(faultCode, faultMsg))
                    self.w.pmx485_label.setStyleSheet('QLabel { color: #ff0000 }')
                    self.dialog_error('POWERMAX ERROR', '\nPowermax fault code: {}\n\n{}'.format(faultCode, faultMsg))
            else:
                self.w.pmx485_label.setText('Fault Code: {}'.format(faultRaw))
                self.w.pmx485_label.setStatusTip('Powermax error ({}) Unknown Powermax fault code'.format(faultRaw))
                self.w.pmx485_label.setStyleSheet('QLabel { color: #ff0000 }')
                self.dialog_error('Powermax Error','Unknown Powermax fault code: {}'.format(faultCode))

    def pmx485_mesh_enable_changed(self, state):
        print('MESH MODE =', state)
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


###########################################################################################################################
# CUT RECOVERY FUNCTIONS #
###########################################################################################################################
    def set_cut_recovery(self):
        self.cutRecovering = True
        self.w.jog_stack.setCurrentIndex(1)
        self.cancelWait = False
        self.cutrec_speed_changed(self.w.cut_rec_speed.value())
        self.clear_offsets()
        self.xOrig = hal.get_value('axis.x.eoffset-counts')
        self.yOrig = hal.get_value('axis.y.eoffset-counts')
        self.zOrig = hal.get_value('axis.z.eoffset-counts')
        self.oScale = hal.get_value('plasmac.offset-scale')
        self.xMin = float(self.iniFile.find('AXIS_X', 'MIN_LIMIT'))
        self.xMax = float(self.iniFile.find('AXIS_X', 'MAX_LIMIT'))
        self.yMin = float(self.iniFile.find('AXIS_Y', 'MIN_LIMIT'))
        self.yMax = float(self.iniFile.find('AXIS_Y', 'MAX_LIMIT'))
        self.zMin = float(self.iniFile.find('AXIS_Z', 'MIN_LIMIT'))
        self.zMax = float(self.iniFile.find('AXIS_Z', 'MAX_LIMIT'))

    def cutrec_speed_changed(self, speed):
        if STATUS.is_metric_mode():
            self.w.cut_rec_feed.setText('{:0.0f}'.format(self.w.cut_feed_rate.value() * speed * 0.01))
        else:
            self.w.cut_rec_feed.setText('{:0.1f}'.format(self.w.cut_feed_rate.value() * speed * 0.01))

    def cutrec_motion(self, direction):
        speed = float(self.w.cut_rec_speed.value()) * 0.01 * direction
        hal.set_p('plasmac.paused-motion-speed',str(speed))

    def cutrec_move(self, x, y):
        distX = hal.get_value('qtplasmac.kerf_width-f') * x
        distY = hal.get_value('qtplasmac.kerf_width-f') * y
        if hal.get_value('plasmac.axis-x-position') + \
           hal.get_value('axis.x.eoffset-counts') * self.oScale + distX > self.xMax:
            msg = 'X axis motion would trip X maximum limit'
            self.dialog_error('CUT RECOVERY ERROR', msg)
            return
        moveX = int(distX / self.oScale)
        if hal.get_value('plasmac.axis-y-position') + \
           hal.get_value('axis.y.eoffset-counts') * self.oScale + distY > self.yMax:
            msg = 'Y axis motion would trip Y maximum limit'
            self.dialog_error('CUT RECOVERY ERROR', msg)
            return
        moveY = int(distY / self.oScale)
        hal.set_p('plasmac.x-offset', '{}'.format(str(hal.get_value('axis.x.eoffset-counts') + moveX)))
        hal.set_p('plasmac.y-offset', '{}'.format(str(hal.get_value('axis.y.eoffset-counts') + moveY)))
        hal.set_p('plasmac.cut-recovery', '1')

    def cutrec_offset_changed(self, offset):
        if offset > 0.05 * self.unitsPerMm or offset < -0.05 * self.unitsPerMm:
            self.cutrec_motion_enable(False)
            if self.cancelWait:
                self.cutrec_buttons_enable(False)
        else:
            self.cancelWait = False
            self.cutrec_motion_enable(True)
            self.cutrec_buttons_enable(True)

    def cutrec_cancel_pressed(self):
        self.cancelWait = True
        self.clear_offsets()

    def clear_offsets(self):
        hal.set_p('plasmac.x-offset', '0')
        hal.set_p('plasmac.y-offset', '0')

    def cutrec_motion_enable(self, state):
        for widget in ['fwd', 'rev', 'speed']:
            self.w['cut_rec_{}'.format(widget)].setEnabled(state)

    def cutrec_buttons_enable(self, state):
        for widget in ['n', 'ne', 'e', 'se', 's', 'sw', 'w', 'nw', 'cancel']:
            self.w['cut_rec_{}'.format(widget)].setEnabled(state)
        for widget in ['run', 'abort', 'pause', 'power']:
            self.w['{}'.format(widget)].setEnabled(state)


#########################################################################################################################
# CONVERSATIONAL FUNCTIONS #
#########################################################################################################################
    def conv_setup(self):
        if self.unitsPerMm == 1:
            self.unitCode = ['21', '0.25', 32]
        else:
            self.unitCode = ['20', '0.004', 1.26]
        self.ambles = 'G{} G64P{} G40 G49 G80 G90 G92.1 G94 G97'.format(self.unitCode[0], self.unitCode[1])
        CONVSET.load(self, self.w)
        if self.gridSize:
            # grid size is in inches
            self.w.conv_preview.grid_size = self.gridSize / self.unitsPerMm / 25.4
            self.w.conv_preview.set_current_view()
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
        else:
            self.conv_new_pressed()
        self.xOrigin = STATUS.get_position()[0][0]
        self.yOrigin = STATUS.get_position()[0][1]
        self.xSaved = '0.000'
        self.ySaved = '0.000'
        self.oSaved = self.origin
        if not self.oldConvButton:
            self.conv_shape_request('conv_line', CONVLINE)
        self.conv_enable_buttons(True)

    def conv_new_pressed(self):
        outNgc = open(self.fNgc, 'w')
        outNgc.write('(new conversational file)\nM2\n')
        outNgc.close()
        COPY(self.fNgc, self.fTmp)
        COPY(self.fNgc, self.fNgcBkp)
        self.w.conv_preview.load(self.fNgc)

    def conv_save_pressed(self):
        with open(self.fNgc) as inFile:
            for line in inFile:
                if '(new conversational file)' in line:
                    self.dialog_error('SAVE ERROR', 'The empty file: {}\n\ncannot be saved'.format(os.path.basename(self.fNgc)))
                    return

        options = QFileDialog.Options()
        options |= QFileDialog.DontUseNativeDialog
        fileName, _ = QFileDialog.getSaveFileName(self.w,
                                                  'QFileDialog.getSaveFileName()',
                                                  self.programPrefix,
                                                  'G-Code Files (*.ngc *.nc *.tap);;All Files (*)',
                                                  options=options)
        if fileName:
            COPY(self.fNgc, fileName)

    def conv_settings_pressed(self):
        self.color_button_image(self.oldConvButton, self.foreColor)
        self.w[self.oldConvButton].setStyleSheet(\
                'QPushButton {{ background: {0} }} \
                 QPushButton:pressed {{ background: {0} }}'.format(self.backColor))
        self.conv_enable_buttons(False)
        self.conv_clear_widgets()
        CONVSET.widgets(self, self.w)
        CONVSET.show(self, self.w)

    def conv_send_pressed(self):
        COPY(self.fNgcBkp, self.fNgc)
        ACTION.OPEN_PROGRAM(self.fNgc)
        self.w.main_tab_widget.setCurrentIndex(0)

    def conv_line_pressed(self):
        self.conv_shape_request(self.w.sender().objectName(), CONVLINE)

    def conv_circle_pressed(self):
        self.conv_shape_request(self.w.sender().objectName(), CONVCIRC)

    def conv_triangle_pressed(self):
        self.conv_shape_request(self.w.sender().objectName(), CONVTRIA)

    def conv_rectangle_pressed(self):
        self.conv_shape_request(self.w.sender().objectName(), CONVRECT)

    def conv_polygon_pressed(self):
        self.conv_shape_request(self.w.sender().objectName(), CONVPOLY)

    def conv_bolt_pressed(self):
        self.conv_shape_request(self.w.sender().objectName(), CONVBOLT)

    def conv_slot_pressed(self):
        self.conv_shape_request(self.w.sender().objectName(), CONVSLOT)

    def conv_star_pressed(self):
        self.conv_shape_request(self.w.sender().objectName(), CONVSTAR)

    def conv_gusset_pressed(self):
        self.conv_shape_request(self.w.sender().objectName(), CONVGUST)

    def conv_sector_pressed(self):
        self.conv_shape_request(self.w.sender().objectName(), CONVSECT)

    def conv_rotate_pressed(self):
        with open(self.fNgc) as inFile:
            for line in inFile:
                if '(new conversational file)' in line:
                    self.dialog_error('ROTATE', 'The empty file: {}\n\ncannot be rotated'.format(os.path.basename(self.fNgc)))
                    return
        self.conv_shape_request(self.w.sender().objectName(), CONVROTA)

    def conv_array_pressed(self):
        with open(self.fNgc) as inFile:
            for line in inFile:
                if '(new conversational file)' in line:
                    self.dialog_error('ARRAY', 'The empty file: {}\n\ncannot be arrayed'.format(os.path.basename(self.fNgc)))
                    return
                elif '#<ucs_' in line:
                    self.dialog_error('ARRAY', 'This existing array: {}\n\ncannot be arrayed'.format(os.path.basename(self.fNgc)))
                    return
                elif '(conversational' in line:
                    self.arrayMode = 'conversational'
                    break
                else:
                    self.arrayMode = 'external'
        self.conv_shape_request(self.w.sender().objectName(), CONVARAY)

    def conv_shape_request(self, shape, module):
        self.conv_button_color(shape)
        self.conv_enable_buttons(True)
        self.conv_clear_widgets()
        module.widgets(self, self.w)

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

    def conv_entry_changed(self, widget):
        name = widget.objectName()
        if widget.text():
            if name == 'hsEntry':
                good = '0123456789'
            else:
                good = '-.0123456789'
            out = ''
            for t in widget.text():
                if t in good:
                    out += t
            widget.setText(out)
            if widget.text() in '.-':
                return
            try:
                a = float(widget.text())
            except:
                self.dialog_error('NUMERIC ENTRY', 'An invalid entry has been detected')
                widget.setText('0')
        if name == 'gsEntry':
            print('grid change to', widget.text())
            # grid size is in inches
            self.w.conv_preview.grid_size = float(widget.text()) / self.unitsPerMm / 25.4
            self.w.conv_preview.set_current_view()

    def conv_undo_shape(self, button):
        if os.path.exists(self.fNgcBkp):
            COPY(self.fNgcBkp, self.fNgc)
            self.w.conv_preview.load(self.fNgc)
            self.w.conv_preview.set_current_view()
            if button:
                self.w[button].setEnabled(False)

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
        self.w.add.setEnabled(False)


    def conv_clear_widgets(self):
        for i in reversed(range(self.w.entries.count())): 
            widgetToRemove = self.w.entries.itemAt(i).widget()
            self.w.entries.removeWidget(widgetToRemove)
            widgetToRemove.setParent(None)


#########################################################################################################################
# STYLING FUNCTIONS #
#########################################################################################################################
    def openColorDialog(self, widget):
        color = QColorDialog.getColor(QColor(widget.palette().color(QPalette.Background)))
        if color.isValid():
            widget.setStyleSheet('background-color: {}'.format(color.name()))
            self.set_color_styles()

    def set_color_styles(self):
# create stylesheet .qss file from template
        styleTemplateFile = os.path.join(self.PATHS.SCREENDIR, self.PATHS.BASEPATH, 'qtplasmac.style')
        styleSheetFile = os.path.join(self.PATHS.CONFIGPATH, 'qtplasmac.qss')
        with open(styleTemplateFile, 'r') as inFile:
            with open(styleSheetFile, 'w') as outFile:
                for line in inFile:
                    if 'foregnd' in line:
                        outFile.write(line.replace('foregnd', self.w.color_foregrnd.styleSheet().split(':')[1].strip()))
                        self.w.colorfg.hal_pin.set(int(self.w.color_foregrnd.styleSheet().split(':')[1].strip().lstrip('#'), 16))
                    elif 'highlight' in line:
                        outFile.write(line.replace('highlight', self.w.color_foregalt.styleSheet().split(':')[1].strip()))
                    elif 'l-e-d' in line:
                        outFile.write(line.replace('l-e-d', self.w.color_led.styleSheet().split(':')[1].strip()))
                    elif 'backgnd' in line:
                        outFile.write(line.replace('backgnd', self.w.color_backgrnd.styleSheet().split(':')[1].strip()))
                        self.w.colorbg.hal_pin.set(int(self.w.color_backgrnd.styleSheet().split(':')[1].strip().lstrip('#'), 16))
                    elif 'backalt' in line:
                        outFile.write(line.replace('backalt', self.w.color_backgalt.styleSheet().split(':')[1].strip()))
                    elif 'frames' in line:
                        outFile.write(line.replace('frames', self.w.color_frams.styleSheet().split(':')[1].strip()))
                    elif 'e-stop' in line:
                        outFile.write(line.replace('e-stop', self.w.color_estop.styleSheet().split(':')[1].strip()))
                    elif 'inactive' in line:
                        outFile.write(line.replace('inactive', self.w.color_disabled.styleSheet().split(':')[1].strip()))
                    else:
                        outFile.write(line)
# apply the new stylesheet
        self.w.setStyleSheet('')
        with open(styleSheetFile, 'r') as set_style:
           self.w.setStyleSheet(set_style.read())
# set colors
        self.foreColor = QColor(self.w.color_foregrnd.palette().color(QPalette.Background)).name()
        self.backColor = QColor(self.w.color_backgrnd.palette().color(QPalette.Background)).name()
        self.highColor = QColor(self.w.color_foregalt.palette().color(QPalette.Background)).name()
        self.inactive = QColor(self.w.color_disabled.palette().color(QPalette.Background)).name()
        fcolor = QColor(self.foreColor)
        bcolor = QColor(self.backColor)
        hcolor = QColor(self.highColor)
# set icon colors
        buttons = ['jog_x_minus', 'jog_x_plus', 'jog_y_minus', 'jog_y_plus',
                   'jog_z_minus', 'jog_z_plus', 'jog_a_minus', 'jog_a_plus',
                   'cut_rec_n', 'cut_rec_ne', 'cut_rec_e', 'cut_rec_se', 
                   'cut_rec_s', 'cut_rec_sw', 'cut_rec_w', 'cut_rec_nw',
                   'conv_line', 'conv_circle', 'conv_triangle', 'conv_rectangle',
                   'conv_polygon', 'conv_bolt', 'conv_slot', 'conv_star',
                   'conv_gusset', 'conv_sector', 'conv_rotate', 'conv_array']
        for button in buttons:
            if self.w[button].isChecked():
                self.color_button_image(button, self.backColor)
            else:
                self.color_button_image(button, self.foreColor)
# gcode display, editor and graphics cannot use .qss file
        for i in range(0,4):
            self.w.gcode_display.lexer.setColor(fcolor, i)
            self.w.gcode_editor.editor.lexer.setColor(fcolor, i)
        self.w.gcode_display.set_background_color(bcolor)
        self.w.gcode_display.setMarginsForegroundColor(fcolor)
        self.w.gcode_display.setMarginsBackgroundColor(QColor(self.w.color_backgalt.palette().color(QPalette.Background)))
        self.w.gcode_display.setMarkerBackgroundColor(QColor(self.w.color_backgalt.palette().color(QPalette.Background)))
        self.w.gcode_display.setCaretForegroundColor(bcolor)
        self.w.gcode_display.setCaretLineBackgroundColor(QColor(self.w.color_backgalt.palette().color(QPalette.Background)))
        self.w.gcode_display.setSelectionForegroundColor(fcolor)
        self.w.gcode_display.setSelectionBackgroundColor(bcolor)
        self.w.gcode_editor.editor.set_background_color(bcolor)
        self.w.gcode_editor.editor.setMarginsForegroundColor(fcolor)
        self.w.gcode_editor.editor.setMarginsBackgroundColor(QColor(self.w.color_backgalt.palette().color(QPalette.Background)))
        self.w.gcode_editor.editor.setMarkerBackgroundColor(bcolor)
        self.w.gcode_editor.editor.setCaretForegroundColor(hcolor)
        self.w.gcode_editor.editor.setCaretLineBackgroundColor(bcolor)
        self.w.gcode_editor.editor.setSelectionForegroundColor(bcolor)
        self.w.gcode_editor.editor.setSelectionBackgroundColor(fcolor)
        self.w.gcodegraphics.setBackgroundColor(QColor(self.w.color_preview.palette().color(QPalette.Background)))
        self.w.conv_preview.setBackgroundColor(QColor(self.w.color_preview.palette().color(QPalette.Background)))

    def color_button_image(self, button, color):
        image_path = '{}{}.png'.format(self.IMAGES, button)
        self.image = QImage(image_path)
        for x in range(self.image.width()):
            for y in range(self.image.height()):
                pcolor = self.image.pixelColor(x, y)
                if pcolor.alpha() > 0:
                    newColor = QColor(color)
                    newColor.setAlpha(pcolor.alpha())
                    self.image.setPixelColor(x, y, newColor)
        self.w['{}'.format(button)].setIcon(QIcon(QPixmap.fromImage(self.image)))


#########################################################################################################################
# KEY BINDING CALLS #
#########################################################################################################################
    def on_keycall_ESTOP(self,event,state,shift,cntrl):
        if not event.isAutoRepeat() and state:
            ACTION.SET_ESTOP_STATE(STATUS.estop_is_clear())

    def on_keycall_POWER(self,event,state,shift,cntrl):
        if not event.isAutoRepeat() and state:
            ACTION.SET_MACHINE_STATE(not STATUS.machine_is_on())

    def on_keycall_ABORT(self,event,state,shift,cntrl):
        if not event.isAutoRepeat() and state and STATUS.stat.interp_state != linuxcnc.INTERP_IDLE:
            ACTION.ABORT()

    def on_keycall_HOME(self,event,state,shift,cntrl):
        if state and self.keyboard_shortcuts():
            if STATUS.is_all_homed():
                ACTION.SET_MACHINE_UNHOMED(-1)
            else:
                ACTION.SET_MACHINE_HOMING(-1)

    def on_keycall_pause(self,event,state,shift,cntrl):
        if state and STATUS.is_auto_mode() and self.keyboard_shortcuts():
            ACTION.PAUSE()

    def on_keycall_XPOS(self,event,state,shift,cntrl):
        if not event.isAutoRepeat() and self.keyboard_shortcuts():
            self.kb_jog(state, 0, 1, shift)

    def on_keycall_XNEG(self,event,state,shift,cntrl):
        if not event.isAutoRepeat() and self.keyboard_shortcuts():
            self.kb_jog(state, 0, -1, shift)

    def on_keycall_YPOS(self,event,state,shift,cntrl):
        if not event.isAutoRepeat() and self.keyboard_shortcuts():
            self.kb_jog(state, 1, 1, shift)

    def on_keycall_YNEG(self,event,state,shift,cntrl):
        if not event.isAutoRepeat() and self.keyboard_shortcuts():
            self.kb_jog(state, 1, -1, shift)

    def on_keycall_ZPOS(self,event,state,shift,cntrl):
        if not event.isAutoRepeat() and self.keyboard_shortcuts():
            self.kb_jog(state, 2, 1, shift)

    def on_keycall_ZNEG(self,event,state,shift,cntrl):
        if not event.isAutoRepeat() and self.keyboard_shortcuts():
            self.kb_jog(state, 2, -1, shift)

    # def on_keycall_APOS(self,event,state,shift,cntrl):
    #     if not event.isAutoRepeat() and self.keyboard_shortcuts():
    #         self.kb_jog(state, 2, 1, shift)
    # 
    # def on_keycall_ANEG(self,event,state,shift,cntrl):
    #     if not event.isAutoRepeat() and self.keyboard_shortcuts():
    #         self.kb_jog(state, 2, -1, shift)

    def on_keycall_plus(self,event,state,shift,cntrl):
        if not event.isAutoRepeat() and self.keyboard_shortcuts():
            self.kb_jog(state, 3, 1, shift, False)

    def on_keycall_minus(self,event,state,shift,cntrl):
        if not event.isAutoRepeat() and self.keyboard_shortcuts():
            self.kb_jog(state, 3, -1, shift, False)

    def on_keycall_F12(self,event,state,shift,cntrl):
        if not event.isAutoRepeat() and state:
            self.STYLEEDITOR.load_dialog()

    def on_keycall_F9(self,event,state,shift,cntrl):
        if state and not event.isAutoRepeat() and self.keyboard_shortcuts():
            print('START A MANUAL CUT', state)
            if STATUS.is_spindle_on():
                print('spindle was on')
                ACTION.SET_SPINDLE_STOP(0)
            else:
                print('spindle was off')
                ACTION.SET_SPINDLE_ROTATION(1 ,1, 0)

##################################################################################################################################
# required class boiler code #
##################################################################################################################################
    def __getitem__(self, item):
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)


####################################################################################################################################
# required handler boiler code #
####################################################################################################################################
def get_handlers(halcomp, widgets, paths):
    return [HandlerClass(halcomp, widgets, paths)]
