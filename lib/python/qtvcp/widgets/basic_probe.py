#!/usr/bin/env python3
# Qtvcp basic probe
#
# Copyright (c) 2020  Chris Morley <chrisinnanaimo@hotmail.com>
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
# a probe screen based on ProbeBasic screen

import sys
import os
import json
from PyQt5.QtCore import QProcess, QRegExp, QFile, Qt
from PyQt5 import QtGui, QtWidgets, uic, QtCore
from PyQt5.QtWidgets import QDialogButtonBox, QAbstractSlider
from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Action, Status, Info, Path
from qtvcp.widgets.dialogMixin import GeometryMixin
from qtvcp import logger

ACTION = Action()
STATUS = Status()
INFO = Info()
PATH = Path()
LOG = logger.getLogger(__name__)
# Force the log level for this module
#LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

current_dir =  os.path.dirname(__file__)
SUBPROGRAM = os.path.abspath(os.path.join(current_dir, 'probe_subprog.py'))
CONFIG_DIR = os.getcwd()

# can use/favours local image and help files
HELP = PATH.find_widget_path()

class BasicProbe(QtWidgets.QWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(BasicProbe, self).__init__(parent)
        self.proc = None
        if INFO.MACHINE_IS_METRIC:
            self.valid = QtGui.QRegExpValidator(QRegExp('^[+-]?((\d+(\.\d{,4})?)|(\.\d{,4}))$'))
        else:
            self.valid = QtGui.QRegExpValidator(QRegExp('^[+-]?((\d+(\.\d{,3})?)|(\.\d{,3}))$'))
        self.setMinimumSize(600, 420)
        # load the widgets ui file
        self.filename = PATH.find_widget_path('basic_probe.ui')
        try:
            self.instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            LOG.critical(e)
        # load the probe help file
        self.filename = PATH.find_widget_path('basic_probe_help.ui')
        try:
            self.dialog = uic.loadUi(self.filename)
        except AttributeError as e:
            LOG.critical(e)

        self.probe_list = ["OUTSIDE CORNERS", "INSIDE CORNERS", "EDGE ANGLE", "BOSS and POCKETS",
                           "RIDGE and VALLEY", "CALIBRATE"]
        self.status_list = ['xm', 'xc', 'xp', 'ym', 'yc', 'yp', 'lx', 'ly', 'z', 'd', 'a', 'delta']

        #create parameter dictionary
        self.send_dict = {}
        # these parameters are sent to the subprogram
        self.parm_list = ['probe_diam',
                          'max_travel',
                          'max_z_travel',
                          'xy_clearance',
                          'z_clearance',
                          'extra_depth',
                          'latch_return_dist',
                          'search_vel',
                          'probe_vel',
                          'rapid_vel',
                          'side_edge_length',
                          'x_hint_bp',
                          'y_hint_bp',
                          'x_hint_rv',
                          'y_hint_rv',
                          'diameter_hint',
                          'cal_diameter',
                          'cal_x_width',
                          'cal_y_width',
                          'calibration_offset']

        # button connections
        self.cmb_probe_select.activated.connect(self.cmb_probe_select_changed)
        self.outside_buttonGroup.buttonClicked.connect(self.probe_btn_clicked)
        self.inside_buttonGroup.buttonClicked.connect(self.probe_btn_clicked)
        self.skew_buttonGroup.buttonClicked.connect(self.probe_btn_clicked)
        self.boss_pocket_buttonGroup.buttonClicked.connect(self.boss_pocket_clicked)
        self.ridge_valley_buttonGroup.buttonClicked.connect(self.ridge_valley_clicked)
        self.cal_buttonGroup.buttonClicked.connect(self.cal_btn_clicked)
        self.clear_buttonGroup.buttonClicked.connect(self.clear_results_clicked)
        self.btn_probe_help.clicked.connect(self.probe_help_clicked)

        self.cmb_probe_select.clear()
        self.cmb_probe_select.addItems(self.probe_list)
        self.stackedWidget_probe_buttons.setCurrentIndex(0)
        # define validators for all lineEdit widgets
        self.lineEdit_probe_tool.setValidator(QtGui.QRegExpValidator(QRegExp('[0-9]{0,5}')))
        for i in self.parm_list:
            self['lineEdit_' + i].setValidator(self.valid)

    def _hal_init(self):
        def homed_on_status():
            return (STATUS.machine_is_on() and (STATUS.is_all_homed() or INFO.NO_HOME_REQUIRED))
        STATUS.connect('state_off', lambda w: self.setEnabled(False))
        STATUS.connect('state_estop', lambda w: self.setEnabled(False))
        STATUS.connect('interp-idle', lambda w: self.setEnabled(homed_on_status()))
        STATUS.connect('all-homed', lambda w: self.setEnabled(True))

        # must directly initialize
        self.statuslabel_motiontype.hal_init()
        self.led_probe.hal_init()
        self.statelabel_machineUnits.hal_init()
        self.help = HelpDialog(self.QTVCP_INSTANCE_)
        self.help.hal_init(HAL_NAME='_basic_help')

        if self.PREFS_:
            self.lineEdit_probe_tool.setText(self.PREFS_.getpref('Probe tool', '0', str, 'PROBE OPTIONS'))
            self.lineEdit_probe_diam.setText(self.PREFS_.getpref('Probe diameter', '4', str, 'PROBE OPTIONS'))
            self.lineEdit_rapid_vel.setText(self.PREFS_.getpref('Probe rapid', '10', str, 'PROBE OPTIONS'))
            self.lineEdit_probe_vel.setText(self.PREFS_.getpref('Probe feed', '10', str, 'PROBE OPTIONS'))
            self.lineEdit_search_vel.setText(self.PREFS_.getpref('Probe search', '10', str, 'PROBE OPTIONS'))
            self.lineEdit_max_travel.setText(self.PREFS_.getpref('Probe max travel', '10', str, 'PROBE OPTIONS'))
            self.lineEdit_max_z_travel.setText(self.PREFS_.getpref('Probe max z', '2', str, 'PROBE OPTIONS'))
            self.lineEdit_extra_depth.setText(self.PREFS_.getpref('Probe extra depth', '0', str, 'PROBE OPTIONS'))
            self.lineEdit_latch_return_dist.setText(self.PREFS_.getpref('Probe step off', '10', str, 'PROBE OPTIONS'))
            self.lineEdit_xy_clearance.setText(self.PREFS_.getpref('Probe xy clearance', '10', str, 'PROBE OPTIONS'))
            self.lineEdit_z_clearance.setText(self.PREFS_.getpref('Probe z clearance', '10', str, 'PROBE OPTIONS'))
            self.lineEdit_side_edge_length.setText(self.PREFS_.getpref('Probe edge width', '10', str, 'PROBE OPTIONS'))
            self.lineEdit_calibration_offset.setText(self.PREFS_.getpref('Calibration offset', '0', str, 'PROBE OPTIONS'))
            self.lineEdit_cal_x_width.setText(self.PREFS_.getpref('Cal x width', '0', str, 'PROBE OPTIONS'))
            self.lineEdit_cal_y_width.setText(self.PREFS_.getpref('Cal y width', '0', str, 'PROBE OPTIONS'))
            self.lineEdit_cal_diameter.setText(self.PREFS_.getpref('Cal diameter', '0', str, 'PROBE OPTIONS'))

    def _hal_cleanup(self):
        if self.PREFS_:
            LOG.debug('Saving Basic Probe data to preference file.')
            self.PREFS_.putpref('Probe tool', self.lineEdit_probe_tool.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe diameter', self.lineEdit_probe_diam.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe rapid', self.lineEdit_rapid_vel.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe feed', self.lineEdit_probe_vel.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe search', self.lineEdit_search_vel.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe max travel', self.lineEdit_max_travel.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe max z', self.lineEdit_max_z_travel.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe extra depth', self.lineEdit_extra_depth.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe step off', self.lineEdit_latch_return_dist.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe xy clearance', self.lineEdit_xy_clearance.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe z clearance', self.lineEdit_z_clearance.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe edge width', self.lineEdit_side_edge_length.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Calibration offset', self.lineEdit_calibration_offset.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Cal x width', self.lineEdit_cal_x_width.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Cal y width', self.lineEdit_cal_y_width.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Cal diameter', self.lineEdit_cal_diameter.text(), str, 'PROBE OPTIONS')
        if self.proc is not None: self.proc.terminate()

#################
# process control
#################
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
        if int(self.lineEdit_probe_tool.text()) != STATUS.get_current_tool():
            msg = "Probe tool not mounted in spindle"
            if not self.set_statusbar(msg):
                STATUS.emit('update-machine-log', msg, 'TIME')
                ACTION.SET_ERROR_MESSAGE(msg)
            return

        self.start_process()
        string_to_send = cmd + '$' + json.dumps(self.send_dict) + '\n'
        #print("String to send ", string_to_send)
        STATUS.block_error_polling()
        self.proc.writeData(bytes(string_to_send, 'utf-8'))

    def process_started(self):
        LOG.info("Basic_Probe subprogram started with PID {}\n".format(self.proc.processId()))

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
            ACTION.SET_ERROR_MESSAGE('Basic Probe process finished  in error')
        elif "INFO" in line:
            pass
        elif "PROBE_ROUTINES" in line:
            if LOG.getEffectiveLevel() < logger.INFO:
                print(line)
        elif "COMPLETE" in line:
            STATUS.unblock_error_polling()
            LOG.info("Basic Probing routine completed without errors")
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

    # return false if failed so other ways of reporting can be used.
    # there might not be a statusbar in main screen.
    def set_statusbar(self, msg, priority = 2):
        try:
            self.QTVCP_INSTANCE_.add_status(msg, priority)
            return True
        except:
            return False

# Main button handler routines
    def probe_help_clicked(self):
        self.help.showDialog()

    def cmb_probe_select_changed(self, index):
        self.stackedWidget_probe_buttons.setCurrentIndex(index)

    def probe_btn_clicked(self, button):
        cmd = button.property('probe')
        #print("Button clicked ", cmd)
        self.get_parms()
        self.start_probe(cmd)

    def boss_pocket_clicked(self, button):
        for i in ['x_hint_bp', 'y_hint_bp', 'diameter_hint']:
            if self['lineEdit_' + i].text() is None: return
        cmd = button.property('probe')
        self.get_parms()
        self.start_probe(cmd)

    def ridge_valley_clicked(self, button):
        for i in ['x_hint_rv', 'y_hint_rv']:
            if self['lineEdit_' + i].text() is None: return
        cmd = button.property('probe')
        self.get_parms()
        self.start_probe(cmd)

    def cal_btn_clicked(self, button):
        for i in ['calibration_offset', 'cal_diameter', 'cal_x_width', 'cal_y_width']:
            if self['lineEdit_' + i].text() is None: return
        cmd = button.property('probe')
        self.get_parms()
        self.start_probe(cmd)

    def clear_results_clicked(self, button):
        cmd = button.property('clear')
        if cmd in dir(self): self[cmd]()

    def clear_x(self):
        self.status_xm.setText('0')
        self.status_xp.setText('0')
        self.status_xc.setText('0')
        self.status_lx.setText('0')

    def clear_y(self):
        self.status_ym.setText('0')
        self.status_yp.setText('0')
        self.status_yc.setText('0')
        self.status_ly.setText('0')

    def clear_all(self):
        self.clear_x()
        self.clear_y()
        self.status_z.setText('0')
        self.status_d.setText('0')
        self.status_delta.setText('0')
        self.status_a.setText('0')

# Helper functions
    def get_parms(self):
        self.send_dict = {key: self['lineEdit_' + key].text() for key in (self.parm_list)}
        #print(self.send_dict)
        for key in ['allow_auto_zero', 'allow_auto_skew', 'cal_avg_error', 'cal_x_error', 'cal_y_error']:
            val = '1' if self[key].isChecked() else '0'
            self.send_dict.update( {key: val} )

    def show_results(self, line):
        for key in self.status_list:
            if line[key] != 'None':
                self['status_' + key].setText(line[key])
            else:
                self['status_' + key].setText('')

    ##############################
    # required class boiler code #
    ##############################
    def __getitem__(self, item):
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)

class HelpDialog(QtWidgets.QDialog, GeometryMixin):
    def __init__(self, parent=None):
        super(HelpDialog, self).__init__(parent)
        self._title = 'Basic Probe Help'
        self.setWindowFlags(self.windowFlags() | Qt.Tool |
                            Qt.Dialog | Qt.WindowStaysOnTopHint |
                            Qt.WindowSystemMenuHint)
        self.currentHelpPage=-1
        self.setMinimumWidth(600)
        self.setMinimumHeight(600)
        self.helpPages = ['basic_help.html','basic_help1.html','basic_help2.html',
                        'basic_help3.html','basic_help4.html','basic_help5.html',
                        'basic_help6.html','basic_help7.html','basic_help8.html']

    def _hal_init(self):
        self.buildWidget()
        self.set_default_geometry()
        self.read_preference_geometry('basicProbeHelpDialog-geometry')

    def buildWidget(self):

        l = QtWidgets.QVBoxLayout()
        t = QtWidgets.QTextEdit('Basic Probe Help')
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
                t.setText('Basic Probe Help file Unavailable:\n\n{}'.format(e))

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
                html = html.replace("../images/widgets/","{}/widgets/".format(INFO.IMAGE_PATH))
                t.setHtml(html)
                if t.verticalScrollBar().isVisible():
                    t.verticalScrollBar().setPageStep(100)
                    self.pageStepDwnbutton.show()
                    self.pageStepUpbutton.show()
                else:
                    self.pageStepDwnbutton.hide()
                    self.pageStepUpbutton.hide()

            except Exception as e:
                t.setHtml('''
<h1 style=" margin-top:18px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;"><span style=" font-size:xx-large; font-weight:600;">Basic Probe Help not available</span> </h1>
{}
'''.format(e))
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


    #############################
    # Testing                   #
    #############################
if __name__ == "__main__":
    from PyQt5.QtWidgets import *
    from PyQt5.QtCore import *
    from PyQt5.QtGui import *
    app = QtWidgets.QApplication(sys.argv)
    w = BasicProbe()
    w.show()
    sys.exit( app.exec_() )

