#!/usr/bin/env python3
# Qtvcp versa probe
#
# Copyright (c) 2018  Chris Morley <chrisinnanaimo@hotmail.com>
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
#
# a probe screen based on Versa probe screen

import sys
import os
import hal
import json

from PyQt5 import QtGui, QtCore, QtWidgets, uic
from PyQt5.QtCore import QProcess, QEvent, Qt
from PyQt5.QtWidgets import QDialogButtonBox, QAbstractSlider

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Action, Info, Path
from qtvcp.widgets.dialogMixin import GeometryMixin
from qtvcp import logger
# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# LOG is for running code logging
STATUS = Status()
ACTION = Action()
INFO = Info()
PATH = Path()
LOG = logger.getLogger(__name__)
# Force the log level for this module
#LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

current_dir = os.path.dirname(__file__)
SUBPROGRAM = os.path.abspath(os.path.join(current_dir, 'probe_subprog.py'))

# can use/favours local image and help files
HELP = PATH.find_widget_path()
ICONPATH = os.path.join(PATH.find_image_path(), 'probe_icons')

class VersaProbe(QtWidgets.QWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(VersaProbe, self).__init__(parent)
        self.proc = None
        self.tool_diameter = None
        self.tool_number = None
        STATUS.connect('tool-info-changed', lambda w, data: self._tool_info(data))
        if INFO.MACHINE_IS_METRIC:
            self.valid = QtGui.QRegExpValidator(QtCore.QRegExp('^((\d{1,4}(\.\d{1,3})?)|(\.\d{1,3}))$'))
        else:
            self.valid = QtGui.QRegExpValidator(QtCore.QRegExp('^((\d{1,3}(\.\d{1,4})?)|(\.\d{1,4}))$'))
        self.setMinimumSize(600, 420)
        # Load the widgets UI file will use local file if available:
        self.filename = PATH.find_widget_path('versa_probe.ui')
        try:
            self.instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            LOG.critical(e)
        self.dialog_code = 'CALCULATOR'
        #create parameter dictionary
        self.send_dict = {}
        # these parameters are sent to the subprogram
        self.parm_list = ['adj_x', 'adj_y', 'adj_z', 'adj_angle',
                          'probe_diam', 'max_travel','max_z_travel', 'latch_return_dist',
                          'search_vel', 'probe_vel', 'rapid_vel',
                          'side_edge_length', 'tool_probe_height', 'tool_block_height',
                          'xy_clearance', 'z_clearance']
        self.status_list = ['xm', 'xc', 'xp', 'ym', 'yc', 'yp', 'lx', 'ly', 'z', 'd', 'a','th','bh']

        for i in self.parm_list:
            self['input_' + i].setValidator(self.valid)
        # button connections
        self.btn_help.clicked.connect(self.help_clicked)
        self.inside_buttonGroup.buttonClicked.connect(self.probe_btn_clicked)
        self.outside_buttonGroup.buttonClicked.connect(self.probe_btn_clicked)
        self.skew_buttonGroup.buttonClicked.connect(self.probe_btn_clicked)
        self.length_buttonGroup.buttonClicked.connect(self.probe_btn_clicked)
        self.tool_buttonGroup.buttonClicked.connect(self.probe_btn_clicked)
        self.pbtn_set_x.released.connect(self.pbtn_set_x_released)
        self.pbtn_set_y.released.connect(self.pbtn_set_y_released)	
        self.pbtn_set_z.released.connect(self.pbtn_set_z_released)
        self.pbtn_set_angle.released.connect(self.pbtn_set_angle_released)

        self.buildToolTip(self.input_search_vel, 'Search Velocity', 'search_vel')
        self.buildToolTip(self.input_probe_vel, 'Probe Velocity', 'probe_vel')
        self.buildToolTip(self.input_z_clearance, 'Z Clearence Distance', 'Zclearance')
        self.buildToolTip(self.input_max_travel, 'Maximum Probe Search Distance', 'rappid')
        self.buildToolTip(self.input_latch_return_dist, 'Return After Latch Distance', 'rapid_return')
        self.buildToolTip(self.input_probe_diam,'Probe Diameter','probe_diam')
        self.buildToolTip(self.input_xy_clearance, 'XY Clearence Distance', 'XYclearance')
        self.buildToolTip(self.input_side_edge_length, 'Edge length', 'edge_length')
        #self.buildToolTip(self.input_tool_probe_height, '', '')
        #self.buildToolTip(self.input_tool_block_height, '', '')
        #self.buildToolTip(self.input_adj_x, '', '')
        #self.buildToolTip(self.input_adj_y, '', '')
        #self.buildToolTip(self.input_adj_z, '', '')
        #self.buildToolTip(self.input_adj_angle, '', '')
        self.buildToolTip(self.input_rapid_vel, 'Rapid Velocity', 'rapid_vel')

    # catch focusIn event to pop calculator dialog
    def eventFilter(self, obj, event):
        if event.type() == QEvent.FocusIn:
            if isinstance(obj, QtWidgets.QLineEdit):
                # only if mouse selected
                if event.reason () == 0:
                    self.popEntry(obj)
        return super(VersaProbe, self).eventFilter(obj, event)


    def _tool_info(self, data):
        if data.id != -1:
            self.tool_diameter = data.diameter
            self.tool_number = data.id
            print(data)
            return
        self.tool_diameter = None
        self.tool_number = None

    def _hal_init(self):
        def homed_on_test():
            return (STATUS.machine_is_on() and (STATUS.is_all_homed() or INFO.NO_HOME_REQUIRED))

        # have to call hal_init on widgets in this widget ourselves
        # qtvcp doesn't see them otherwise
        oldname = self.HAL_GCOMP_.comp.getprefix()
        self.HAL_GCOMP_.comp.setprefix('qtversaprobe')
        self.pbtn_use_tool_measurement.setProperty('pin_name','enable')
        self.pbtn_use_tool_measurement.hal_init()
        self.HAL_GCOMP_.comp.setprefix(oldname)

        self.allow_auto_skew.hal_init()
        self.allow_auto_zero.hal_init()
        self.statuslabel_motiontype.hal_init()
        self.statelabel_machineUnits.hal_init()
        self.statelabel_machineUnits_2.hal_init()
        self.help = HelpDialog(self.QTVCP_INSTANCE_)
        self.help.hal_init(HAL_NAME='_versa_help')

        # connect to STATUS
        STATUS.connect('state-off', lambda w: self.setEnabled(False))
        STATUS.connect('state-estop', lambda w: self.setEnabled(False))
        STATUS.connect('interp-idle', lambda w: self.setEnabled(homed_on_test()))
        STATUS.connect('all-homed', lambda w: self.setEnabled(homed_on_test()))
#        STATUS.connect('error', self.send_error)
        STATUS.connect('periodic', lambda w: self.check_probe())
        STATUS.connect('general',self.return_value)

        # install event filters on all the lineedits
        # so we can call up a dialog when lineedit get focus
        self.input_search_vel.installEventFilter(self)
        self.input_probe_vel.installEventFilter(self)
        self.input_z_clearance.installEventFilter(self)
        self.input_max_travel.installEventFilter(self)
        self.input_max_z_travel.installEventFilter(self)
        self.input_latch_return_dist.installEventFilter(self)
        self.input_probe_diam.installEventFilter(self)
        self.input_xy_clearance.installEventFilter(self)
        self.input_side_edge_length.installEventFilter(self)
        self.input_tool_probe_height.installEventFilter(self)
        self.input_tool_block_height.installEventFilter(self)
        self.input_adj_x.installEventFilter(self)
        self.input_adj_y.installEventFilter(self)
        self.input_adj_z.installEventFilter(self)
        self.input_adj_angle.installEventFilter(self)
        self.input_rapid_vel.installEventFilter(self)

        if self.PREFS_:
            self.input_search_vel.setText(str(self.PREFS_.getpref( "ps_searchvel", 300.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_probe_vel.setText(str(self.PREFS_.getpref( "ps_probevel", 10.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_z_clearance.setText(str(self.PREFS_.getpref( "ps_z_clearance", 3.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_max_travel.setText(str(self.PREFS_.getpref( "ps_probe_max", 1.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_max_z_travel.setText(str(self.PREFS_.getpref( "ps_probe_max_z_travel", 1.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_latch_return_dist.setText(str(self.PREFS_.getpref( "ps_probe_latch", 0.5, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_probe_diam.setText(str(self.PREFS_.getpref( "ps_probe_diam", 2.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_xy_clearance.setText(str(self.PREFS_.getpref( "ps_xy_clearance", 5.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_side_edge_length.setText(str(self.PREFS_.getpref( "ps_side_edge_length", 5.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_tool_probe_height.setText(str(self.PREFS_.getpref( "ps_probe_height", 20.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_tool_block_height.setText(str(self.PREFS_.getpref( "ps_block_height", 20.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.pbtn_use_tool_measurement.setChecked((self.PREFS_.getpref( "use_tool_measurement", True, bool, 'VERSA_PROBE_OPTIONS')) )
            self.input_adj_x.setText(str(self.PREFS_.getpref( "ps_offs_x", 0.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_adj_y.setText(str(self.PREFS_.getpref( "ps_offs_y", 0.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_adj_z.setText(str(self.PREFS_.getpref( "ps_offs_z", 0.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_adj_angle.setText(str(self.PREFS_.getpref( "ps_offs_angle", 0.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_rapid_vel.setText(str(self.PREFS_.getpref( "ps_probe_rapid_vel", 60.0, float, 'VERSA_PROBE_OPTIONS')) )

        self.z_max_clear = INFO.get_safe_float("VERSA_TOOLSETTER", "Z_MAX_CLEAR")
        self.ts_x =  INFO.get_safe_float('VERSA_TOOLSETTER','X')
        self.ts_y = INFO.get_safe_float('VERSA_TOOLSETTER','Y')
        self.ts_z = INFO.get_safe_float('VERSA_TOOLSETTER','Z')
        self.ts_max = INFO.get_safe_float('VERSA_TOOLSETTER','MAXPROBE')
        self.ts_diam = INFO.get_safe_float('VERSA_TOOLSETTER','DIAMETER')

        # make pins available for tool measure remaps
        oldname = self.HAL_GCOMP_.comp.getprefix()
        self.HAL_GCOMP_.comp.setprefix('qtversaprobe')
        self.pin_svel = self.HAL_GCOMP_.newpin("searchvel", hal.HAL_FLOAT, hal.HAL_OUT)
        self.pin_svel.set(float(self.input_search_vel.text()))
        self.pin_pvel = self.HAL_GCOMP_.newpin("probevel", hal.HAL_FLOAT, hal.HAL_OUT)
        self.pin_pvel.set(float(self.input_probe_vel.text()))
        self.pin_pheight = self.HAL_GCOMP_.newpin("probeheight", hal.HAL_FLOAT, hal.HAL_OUT)
        self.pin_pheight.set(float(self.input_tool_probe_height.text()))
        self.pin_bheight = self.HAL_GCOMP_.newpin("blockheight", hal.HAL_FLOAT, hal.HAL_OUT)
        self.pin_bheight.set(float(self.input_tool_block_height.text()))
        self.pin_latch_rtn = self.HAL_GCOMP_.newpin("backoffdist", hal.HAL_FLOAT, hal.HAL_OUT)
        self.pin_latch_rtn.set(float(self.input_latch_return_dist.text()))

        self.HAL_GCOMP_.comp.setprefix(oldname)

        # install callbacks to update HAL pins
        self.input_search_vel.textChanged.connect(self.update_search_vel_pin)
        self.input_probe_vel.textChanged.connect(self.update_probe_vel_pin)
        self.input_tool_probe_height.textChanged.connect(self.update_probe_height_pin)
        self.input_tool_block_height.textChanged.connect(self.update_block_height_pin)
        self.input_latch_return_dist.textChanged.connect(self.update_latch_return_dist_pin)

    # when qtvcp closes this gets called
    def _hal_cleanup(self):
        if self.PREFS_:
            LOG.debug('Saving Versa probe data to preference file.')
            self.PREFS_.putpref( "ps_searchvel", float(self.input_search_vel.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_probevel", float(self.input_probe_vel.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_z_clearance", float(self.input_z_clearance.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_probe_max", float(self.input_max_travel.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_probe_max_z_travel", float(self.input_max_z_travel.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_probe_latch", float(self.input_latch_return_dist.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_probe_diam", float(self.input_probe_diam.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_xy_clearance", float(self.input_xy_clearance.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_side_edge_length", float(self.input_side_edge_length.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_probe_height", float(self.input_tool_probe_height.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_block_height", float(self.input_tool_block_height.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "use_tool_measurement", bool(self.pbtn_use_tool_measurement.isChecked()), bool, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_offs_x", float(self.input_adj_x.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_offs_y", float(self.input_adj_y.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_offs_z", float(self.input_adj_z.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_offs_angle", float(self.input_adj_angle.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_probe_rapid_vel", float(self.input_rapid_vel.text()), float, 'VERSA_PROBE_OPTIONS')

    # process the STATUS return message
    # set the line edit to the value if not cancelled
    def return_value(self, w, message):
        num = message['RETURN']
        code = bool(message.get('ID') == '%s__'% self.objectName())
        name = bool(message.get('NAME') == self.dialog_code)
        if code and name and num is not None:
            LOG.debug('message return:{}'.format (message))
            obj = message.get('OBJECT')
            obj.setText(str(num))

    def popEntry(self, obj):
            mess = {'NAME':self.dialog_code,
                    'ID':'%s__' % self.objectName(),
                    'OVERLAY':False,
                    'OBJECT':obj,
                    'TITLE':'Set Entry for {}'.format(obj.objectName().upper()),
                    'GEONAME':'_{}'.format(self.dialog_code)
            }
            STATUS.emit('dialog-request', mess)
            LOG.debug('message sent:{}'.format (mess))

    def buildToolTip(self,obj, text, icon):
        path = os.path.join(ICONPATH, icon)
        obj.setToolTip('''<html><head/><body><p align="center">
<span style=" font-weight:600;">{}</span></p><p align="center">
<img src="{}"/></p></body></html>''' .format(text, path))

#############################################
# process control
#############################################
    def start_process(self):
        self.proc = QProcess()
        self.proc.setReadChannel(QProcess.StandardOutput)
        self.proc.started.connect(self.process_started)
        self.proc.readyReadStandardOutput.connect(self.read_stdout)
        self.proc.readyReadStandardError.connect(self.read_stderror)
        self.proc.finished.connect(self.process_finished)
        self.proc.start('python3 {}'.format(SUBPROGRAM))

    def start_probe(self, cmd):
        if self.proc is not None:
            LOG.info("Probe Routine processor is busy")
            return
        self.start_process()
        string_to_send = cmd + '$' + json.dumps(self.send_dict) + '\n'
        #print("String to send ", string_to_send)
        STATUS.block_error_polling()
        self.proc.writeData(bytes(string_to_send, 'utf-8'))

    def process_started(self):
        LOG.info("Versa_Probe started with PID {}\n".format(self.proc.processId()))

    def read_stdout(self):
        qba = self.proc.readAllStandardOutput()
        line = qba.data()
        self.parse_input(line)

    def read_stderror(self):
        qba = self.proc.readAllStandardError()
        line = qba.data()
        self.parse_input(line)

    def process_finished(self, exitCode, exitStatus):
        LOG.info(("Probe Process finished - exitCode {} exitStatus {}".format(exitCode, exitStatus)))
        self.proc = None
        STATUS.unblock_error_polling()

    def parse_input(self, line):
        line = line.decode("utf-8")
        if "ERROR INFO" in line:
            ACTION.SET_ERROR_MESSAGE(line)
        elif "ERROR" in line:
            #print(line)
            STATUS.unblock_error_polling()
            ACTION.SET_ERROR_MESSAGE('Versa Probe process finished in error')
        elif "PROBE_ROUTINES" in line:
            if LOG.getEffectiveLevel() < logger.INFO:
                print(line)
        elif "INFO" in line:
            pass
        elif "COMPLETE" in line:
            STATUS.unblock_error_polling()
            LOG.info("Versa Probing routine completed without errors")
            return_data = line.rstrip().split('$')
            data = json.loads(return_data[1])
            self.show_results(data)
        elif "HISTORY" in line:
            if not self.set_statusbar(line,1):
                STATUS.emit('update-machine-log', line, 'TIME')
        elif "DEBUG" in line:
            pass
        else:
            LOG.error("Error parsing return data from sub_processor. Line={}".format(line))

#####################################################
# button callbacks
#####################################################

    def help_clicked(self):
        self.pop_help()

    def probe_btn_clicked(self, button):
        cmd = button.property('probe')
        print("Button clicked ", cmd)
        self.get_parms()
        self.start_probe(cmd)

    ###### set origin offset ######################
    def pbtn_set_x_released(self):
        ACTION.SET_AXIS_ORIGIN('X', float(self.input_adj_x.text()))

    def pbtn_set_y_released(self):
        ACTION.SET_AXIS_ORIGIN('Y', float(self.input_adj_y.text()))

    def pbtn_set_z_released(self):
        ACTION.SET_AXIS_ORIGIN('Z', float(self.input_adj_z.text()))

    def pbtn_set_angle_released(self):
        self.status_a = "%.3f" % float(self.input_adj_angle.text())
        s="G10 L2 P0"
        if self.allow_auto_zero.isChecked():
            s +=  " X%.4f"% float(self.input_adj_x.text())
            s +=  " Y%.4f"% float(self.input_adj_y.text())
        else :
            a = STATUS.get_probed_position_with_offsets()
            s +=  " X%.4f"%a[0]
            s +=  " Y%.4f"%a[1]
        s +=  " R%.4f"% float(self.input_adj_angle.text())
        ACTION.CALL_MDI_WAIT(s, 30)

    def input_next(self):
        next = self.stackedWidget_probe_type.currentIndex() +1
        if next == self.stackedWidget_probe_type.count():
            next = 0
        self.stackedWidget_probe_type.setCurrentIndex(next)

#####################################################
# Entry callbacks
#####################################################
    def update_search_vel_pin(self, text):
        try:
            self.pin_svel.set(float(text))
        except:
            pass
    def update_probe_vel_pin(self, text):
        try:
            self.pin_pvel.set(float(text))
        except:
            pass
    def update_probe_height_pin(self, text):
        try:
            self.pin_pheight.set(float(text))
        except:
            pass
    def update_block_height_pin(self, text):
        value = float(text)
        #if value == self.pin_bheight.get(): return
        origin = float(INFO.INI.find("AXIS_Z", "MIN_LIMIT")) + value
        ACTION.CALL_MDI_WAIT( "G10 L2 P0 Z%s" % origin )
        try:
            self.pin_bheight.set(value)
        except:
            pass
    def update_latch_return_dist_pin(self, text):
        try:
            self.pin_latch_rtn.set(float(text))
        except:
            pass

#####################################################
# Helper functions
#####################################################

    # return false if failed so other ways of reporting can be used.
    # there might not be a statusbar in main screen.
    def set_statusbar(self, msg, priority = 2):
        try:
            self.QTVCP_INSTANCE_.add_status(msg, priority)
        except:
            return False
        return True


    def get_parms(self):
        self.send_dict = {key: self['input_' + key].text() for key in (self.parm_list)}
        for key in ['allow_auto_zero', 'allow_auto_skew']:
            val = '1' if self[key].isChecked() else '0'
            self.send_dict.update( {key: val} )
        # come from INI
        for key in ['ts_diam','z_max_clear','ts_x','ts_y','ts_z','ts_max','tool_diameter','tool_number']:
            val = str(self[key])
            if val == 'NONE': val = None
            self.send_dict.update( {key: val} )

    def check_probe(self):
        try:
            self.led_probe_function_chk.setState(hal.get_value('motion.probe-input'))
        except:
            pass

    def show_results(self, line):
        for key in self.status_list:
            if key in('th','bh'):
                if key == 'bh' and line[key] != 'None':
                    self.input_tool_block_height.setText(line[key])
                elif line[key] != 'None':
                    self.input_tool_probe_height.setText(line[key])
            elif line[key] != 'None':
                self['status_' + key].setText(line[key])
            else:
                self['status_' + key].setText('')

    def pop_help(self):
        self.help.showDialog()

########################################
# required boiler code
########################################
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

class HelpDialog(QtWidgets.QDialog, GeometryMixin):
    def __init__(self, parent=None):
        super(HelpDialog, self).__init__(parent)
        self._title = 'Versa Help'
        self.setWindowFlags(self.windowFlags() | Qt.Tool |
                            Qt.Dialog | Qt.WindowStaysOnTopHint |
                            Qt.WindowSystemMenuHint)
        self.currentHelpPage=-1
        self.setMinimumWidth(600)
        self.setMinimumHeight(600)
        self.helpPages = ['versa_usage.html','versa_usage1.html','versa_usage2.html',
                        'versa_usage3.html','versa_usage4.html','versa_usage5.html',
                        'versa_usage6.html','versa_usage7.html','versa_usage8.html']

    def _hal_init(self):
        self.buildWidget()
        self.set_default_geometry()
        self.read_preference_geometry('VersaHelpDialog-geometry')

    def buildWidget(self):

        l = QtWidgets.QVBoxLayout()
        t = QtWidgets.QTextEdit('Versa Probe Help')
        t.setReadOnly(True)
        l.addWidget(t)

        buttons = QDialogButtonBox()

        closebutton = QtWidgets.QPushButton()
        closebutton.setIconSize(QtCore.QSize(38, 38))
        closebutton.setIcon(QtGui.QIcon(':/qt-project.org/styles/commonstyle/images/standardbutton-cancel-128.png'))
        closebutton.clicked.connect(lambda : self.close())

        nextbutton = QtWidgets.QPushButton()
        nextbutton.setIconSize(QtCore.QSize(38, 38))
        nextbutton.setIcon(QtGui.QIcon(':/qt-project.org/styles/commonstyle/images/right-32.png'))
        nextbutton.clicked.connect(lambda : self.next(t,True))

        previousbutton = QtWidgets.QPushButton()
        previousbutton.setIconSize(QtCore.QSize(38, 38))
        previousbutton.setIcon(QtGui.QIcon(':/qt-project.org/styles/commonstyle/images/left-32.png'))
        previousbutton.clicked.connect(lambda : self.next(t,False))

        self.pageStepUpbutton = QtWidgets.QPushButton()
        self.pageStepUpbutton.setIconSize(QtCore.QSize(38, 38))
        self.pageStepUpbutton.setIcon(QtGui.QIcon(':/qt-project.org/styles/commonstyle/images/up-32.png'))
        self.pageStepUpbutton.clicked.connect(lambda : self.pageStep(t,False))

        self.pageStepDwnbutton = QtWidgets.QPushButton()
        self.pageStepDwnbutton.setIconSize(QtCore.QSize(38, 38))
        self.pageStepDwnbutton.setIcon(QtGui.QIcon(':/qt-project.org/styles/commonstyle/images/down-32.png'))
        self.pageStepDwnbutton.clicked.connect(lambda : self.pageStep(t,True))

        bBox = QDialogButtonBox(buttons)
        bBox.addButton(self.pageStepUpbutton, QDialogButtonBox.ActionRole)
        bBox.addButton(self.pageStepDwnbutton, QDialogButtonBox.ActionRole)
        bBox.addButton(previousbutton, QDialogButtonBox.ActionRole)
        bBox.addButton(nextbutton, QDialogButtonBox.ActionRole)
        bBox.addButton(closebutton, QDialogButtonBox.DestructiveRole)
        bBox.rejected.connect(self.reject)

        l.addWidget(bBox)
        self.setLayout(l)

        try:
            self.next(t)
        except Exception as e:
                t.setText('Versa Probe Help file Unavailable:\n\n{}'.format(e))

    def next(self,t,direction=None):
            if direction is None:
                self.currentHelpPage = 0
            elif direction:
                self.currentHelpPage +=1
                if self.currentHelpPage > len(self.helpPages)-1:
                    self.currentHelpPage = len(self.helpPages)-1
            else:
                self.currentHelpPage -=1
                if self.currentHelpPage < 0:
                    self.currentHelpPage = 0
            try:
                pagePath = os.path.join(HELP, self.helpPages[self.currentHelpPage])
                file = QtCore.QFile(pagePath)
                file.open(QtCore.QFile.ReadOnly)
                html = file.readAll()
                html = str(html, encoding='utf8')
                html = html.replace("../images/probe_icons/","{}/probe_icons/".format(INFO.IMAGE_PATH))
                t.setHtml(html)
                if t.verticalScrollBar().isVisible():
                    t.verticalScrollBar().setPageStep(20)
                    self.pageStepDwnbutton.show()
                    self.pageStepUpbutton.show()
                else:
                    self.pageStepDwnbutton.hide()
                    self.pageStepUpbutton.hide()

            except Exception as e:
                t.setText('Versa Probe Help file Unavailable:\n\n{}'.format(e))
            if direction is None:
                return
            self.show()

    def pageStep(self, t, state):
        if state:
            t.verticalScrollBar().triggerAction (QAbstractSlider.SliderPageStepAdd)
        else:
            t.verticalScrollBar().triggerAction (QAbstractSlider.SliderPageStepSub)

    # accept button applies presets and if line number given starts linuxcnc
    def close(self):
        self.record_geometry()
        super(HelpDialog, self).close()

    def showDialog(self):
        self.setWindowTitle(self._title);
        self.set_geometry()
        retval = self.exec_()
        LOG.debug('Value of pressed button: {}'.format(retval))

####################################
# Testing
####################################
if __name__ == "__main__":
    from PyQt5.QtWidgets import *
    from PyQt5.QtCore import *
    from PyQt5.QtGui import *

    app = QtWidgets.QApplication(sys.argv)
    w = VersaProbe()
    w.setObjectName('versaprobe')
    w.show()
    sys.exit( app.exec_() )

