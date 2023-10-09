#!/usr/bin/env python3
# Qtvcp basic probe
#
# Copyright (c) 2020  Chris Morley <chrisinnanaimo@hotmail.com>
# Copyright (c) 2020  Jim Sloot <persei802@gmail.com>
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
from PyQt5.QtCore import QProcess, QRegExp, QFile
from PyQt5 import QtGui, QtWidgets, uic
from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Action, Status, Info, Path, Tool
from qtvcp import logger

ACTION = Action()
STATUS = Status()
INFO = Info()
TOOL = Tool()
PATH = Path()
HERE = os.path.dirname(os.path.abspath(__file__))
LOG = logger.getLogger(__name__)
LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

current_dir =  os.path.dirname(__file__)
SUBPROGRAM = os.path.abspath(os.path.join(current_dir, 'probe_subprog.py'))
CONFIG_DIR = os.getcwd()
#HELP = PATH.find_widget_path()
HELP = os.path.join(PATH.CONFIGPATH, "help_files")

# StatusBar message alert levels
DEFAULT =  0
WARNING =  1
CRITICAL = 2

class BasicProbe(QtWidgets.QWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(BasicProbe, self).__init__()
        self.parent = parent
        self.tool_db = self.parent.tool_db
        self.proc = None
        if INFO.MACHINE_IS_METRIC:
            self.valid = QtGui.QRegExpValidator(QRegExp('^[+-]?((\d+(\.\d{,4})?)|(\.\d{,4}))$'))
        else:
            self.valid = QtGui.QRegExpValidator(QRegExp('^[+-]?((\d+(\.\d{,3})?)|(\.\d{,3}))$'))
        self.setMinimumSize(600, 420)
        # load the widgets ui file
#        self.filename = PATH.find_widget_path('basic_probe.ui')
        self.filename = os.path.join(HERE, 'basic_probe.ui')
        try:
            self.instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            LOG.critical(e)
        # load the probe help file
#        self.filename = PATH.find_widget_path('basic_probe_help.ui')
        self.filename = os.path.join(HERE, 'basic_probe_help.ui')
        try:
            self.dialog = uic.loadUi(self.filename)
        except AttributeError as e:
            LOG.critical(e)

        self.helpPages = ['basic_help.html','basic_help1.html','basic_help2.html',
                          'basic_help3.html','basic_help4.html','basic_help5.html',
                          'basic_help6.html']
        self.currentHelpPage = 0

        self.probe_page_list = ['OUTSIDE MEASUREMENTS',
                                'INSIDE MEASUREMENTS',
                                'ANGLE MEASUREMENTS',
                                'BOSS AND POCKET',
                                'RIDGE AND VALLEY',
                                'CALIBRATION']

        # populate probe page combobox
        self.cmb_probe_select.clear()
        self.cmb_probe_select.addItems(self.probe_page_list)
        self.cmb_probe_select.wheelEvent = lambda event: None

        self.status_list = ['xm', 'xc', 'xp', 'ym', 'yc', 'yp', 'lx', 'ly', 'z', 'd', 'a', 'delta']

        #create parameter dictionary
        self.send_dict = {}
        # these parameters are sent to the subprogram
        self.parm_list = ['probe_diam',
                          'max_travel',
                          'xy_clearance',
                          'adj_x',
                          'adj_y',
                          'adj_z',
                          'adj_angle',
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
                          'cal_offset']

        # signal connections
        self.lineEdit_extra_depth.editingFinished.connect(self.get_probe_max_depth)
        self.cmb_probe_select.activated.connect(self.cmb_probe_select_changed)
        self.outside_buttonGroup.buttonClicked.connect(self.probe_btn_clicked)
        self.inside_buttonGroup.buttonClicked.connect(self.probe_btn_clicked)
        self.skew_buttonGroup.buttonClicked.connect(self.probe_btn_clicked)
        self.boss_pocket_buttonGroup.buttonClicked.connect(self.boss_pocket_clicked)
        self.ridge_valley_buttonGroup.buttonClicked.connect(self.ridge_valley_clicked)
        self.cal_buttonGroup.buttonClicked.connect(self.cal_btn_clicked)
        self.clear_buttonGroup.buttonClicked.connect(self.clear_results_clicked)
        self.btn_load_probe.pressed.connect(self.load_probe_pressed)
        self.btn_probe_help.clicked.connect(self.probe_help_clicked)
        self.dialog.probe_help_close.clicked.connect(self.help_close_clicked)
        self.dialog.probe_help_prev.clicked.connect(self.help_prev_clicked)
        self.dialog.probe_help_next.clicked.connect(self.help_next_clicked)

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

        if self.PREFS_:
            self.lineEdit_probe_tool.setText(self.PREFS_.getpref('Probe tool', '0', str, 'PROBE OPTIONS'))
            self.lineEdit_probe_diam.setText(self.PREFS_.getpref('Probe diameter', '4', str, 'PROBE OPTIONS'))
            self.lineEdit_rapid_vel.setText(self.PREFS_.getpref('Probe rapid', '10', str, 'PROBE OPTIONS'))
            self.lineEdit_probe_vel.setText(self.PREFS_.getpref('Probe feed', '10', str, 'PROBE OPTIONS'))
            self.lineEdit_search_vel.setText(self.PREFS_.getpref('Probe search', '10', str, 'PROBE OPTIONS'))
            self.lineEdit_max_travel.setText(self.PREFS_.getpref('Probe max travel', '10', str, 'PROBE OPTIONS'))
            self.lineEdit_max_z.setText(self.PREFS_.getpref('Probe max z', '2', str, 'PROBE OPTIONS'))
            self.lineEdit_extra_depth.setText(self.PREFS_.getpref('Probe extra depth', '0', str, 'PROBE OPTIONS'))
            self.lineEdit_latch_return_dist.setText(self.PREFS_.getpref('Probe step off', '10', str, 'PROBE OPTIONS'))
            self.lineEdit_xy_clearance.setText(self.PREFS_.getpref('Probe xy clearance', '10', str, 'PROBE OPTIONS'))
            self.lineEdit_z_clearance.setText(self.PREFS_.getpref('Probe z clearance', '10', str, 'PROBE OPTIONS'))
            self.lineEdit_side_edge_length.setText(self.PREFS_.getpref('Probe edge width', '10', str, 'PROBE OPTIONS'))
            self.lineEdit_cal_x_width.setText(self.PREFS_.getpref('Cal x width', '0', str, 'PROBE OPTIONS'))
            self.lineEdit_cal_y_width.setText(self.PREFS_.getpref('Cal y width', '0', str, 'PROBE OPTIONS'))
            self.lineEdit_cal_diameter.setText(self.PREFS_.getpref('Cal diameter', '0', str, 'PROBE OPTIONS'))
            self.lineEdit_adj_x.setText(self.PREFS_.getpref('Adjust X', '0', str, 'PROBE OPTIONS'))
            self.lineEdit_adj_y.setText(self.PREFS_.getpref('Adjust Y', '0', str, 'PROBE OPTIONS'))
            self.lineEdit_adj_z.setText(self.PREFS_.getpref('Adjust Z', '0', str, 'PROBE OPTIONS'))
            self.lineEdit_adj_angle.setText(self.PREFS_.getpref('Adjust angle', '0', str, 'PROBE OPTIONS'))
            self.lineEdit_cal_offset.setText(self.PREFS_.getpref('Cal offset', '0', str, 'PROBE OPTIONS'))

    def _hal_cleanup(self):
        if self.PREFS_:
            LOG.debug('Saving Basic Probe data to preference file.')
            self.PREFS_.putpref('Probe tool', self.lineEdit_probe_tool.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe diameter', self.lineEdit_probe_diam.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe rapid', self.lineEdit_rapid_vel.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe feed', self.lineEdit_probe_vel.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe search', self.lineEdit_search_vel.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe max travel', self.lineEdit_max_travel.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe max z', self.lineEdit_max_z.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe extra depth', self.lineEdit_extra_depth.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe step off', self.lineEdit_latch_return_dist.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe xy clearance', self.lineEdit_xy_clearance.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe z clearance', self.lineEdit_z_clearance.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Probe edge width', self.lineEdit_side_edge_length.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Cal x width', self.lineEdit_cal_x_width.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Cal y width', self.lineEdit_cal_y_width.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Cal diameter', self.lineEdit_cal_diameter.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Adjust X', self.lineEdit_adj_x.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Adjust Y', self.lineEdit_adj_y.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Adjust Z', self.lineEdit_adj_z.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Adjust angle', self.lineEdit_adj_angle.text(), str, 'PROBE OPTIONS')
            self.PREFS_.putpref('Cal offset', self.lineEdit_cal_offset.text(), str, 'PROBE OPTIONS')
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
            self.parent.add_status("Probe Routine processor is busy", WARNING)
            return
        if int(self.lineEdit_probe_tool.text()) != STATUS.get_current_tool():
            self.parent.add_status("Probe tool not mounted in spindle", WARNING)
            return
        self.start_process()
        string_to_send = cmd + '$' + json.dumps(self.send_dict) + '\n'
#        print("String to send ", string_to_send)
        STATUS.block_error_polling()
        self.proc.writeData(bytes(string_to_send, 'utf-8'))

    def process_started(self):
        self.parent.add_status("Basic_Probe subprogram started with PID {}\n".format(self.proc.processId()))

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
        if "INFO" in line:
            print(line)
        elif "ERROR" in line:
#            print(line)
            self.parent.add_status(line, WARNING)
            STATUS.unblock_error_polling()
#            ACTION.SET_ERROR_MESSAGE('Basic Probe process finished in error')
        elif "DEBUG" in line:
            print(line)
        elif "COMPLETE" in line:
            STATUS.unblock_error_polling()
            self.parent.add_status("Basic Probing routine completed without errors")
            return_data = line.rstrip().split('$')
            data = json.loads(return_data[1])
            self.show_results(data)
        elif "HISTORY" in line:
            temp = line.strip('HISTORY$')
            STATUS.emit('update-machine-log', temp, 'TIME')
            self.parent.add_status("Probe history updated to machine log")
        else:
            self.parent.add_status(f"Error parsing return data from sub_processor. Line={line}", WARNING)

# Main button handler routines
    def load_probe_pressed(self):
        try:
            tool =  int(self.lineEdit_probe_tool.text())
        except:
            tool = 0
        if tool > 0:
            info = TOOL.GET_TOOL_INFO(tool)
            dia = info[11]
            self.lineEdit_probe_diam.setText(f"{dia:8.3f}")
            ACTION.CALL_MDI(f"M61 Q{tool}")
        else:
            self.parent.add_status("Invalid probe tool specified", WARNING)

    def probe_help_clicked(self):
        self.dialog.show()
        self.update_help_page()
       
    def help_close_clicked(self):
        self.dialog.hide()

    def help_prev_clicked(self):
        self.currentHelpPage -=1
        if self.currentHelpPage < 0:
            self.currentHelpPage = 0
        self.update_help_page()

    def help_next_clicked(self):
        self.currentHelpPage +=1
        if self.currentHelpPage > len(self.helpPages)-1:
            self.currentHelpPage = len(self.helpPages)-1
        self.update_help_page()
    
    def cmb_probe_select_changed(self, index):
        self.stackedWidget_probe_buttons.setCurrentIndex(index)
        
    def update_help_page(self):
        try:
            pagePath = os.path.join(HELP, self.helpPages[self.currentHelpPage])
            if not os.path.exists(pagePath): raise Exception("Missing File: {}".format(pagePath)) 
            file = QFile(pagePath)
            file.open(QFile.ReadOnly)
            html = file.readAll()
            html = str(html, encoding='utf8')
            html = html.replace("../images/widgets/","{}/widgets/".format(INFO.IMAGE_PATH))
            self.dialog.html_textEdit.setHtml(html)
        except Exception as e:
            print(e)
            self.dialog.html_textEdit.setHtml('''
<h1 style=" margin-top:18px; margin-bottom:12px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;"><span style=" font-size:xx-large; font-weight:600;">Basic Probe Help not available</span> </h1>
{}
'''.format(e))

    def probe_help_prev_clicked(self):
        i = self.probe_help_widget.currentIndex()
        if i > 0:
            self.probe_help_widget.setCurrentIndex(i - 1)

    def probe_help_next_clicked(self):
        i = self.probe_help_widget.currentIndex()
        if i < 5:
            self.probe_help_widget.setCurrentIndex(i + 1)

    def probe_btn_clicked(self, button):
        cmd = button.property('probe')
        if cmd == 'probe_xy_hole':
            self.parent.add_status("Use Rectangular Pocket for this probe function")
        else:
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
        for i in ['cal_diameter', 'cal_x_width', 'cal_y_width']:
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
    def get_probe_max_depth(self):
        probe_tool = int(self.lineEdit_probe_tool.text())
        extra_depth = float(self.lineEdit_extra_depth.text())
        maxz = float(self.tool_db.get_maxz(probe_tool))
        if extra_depth > maxz:
            self.parent.add_status("Warning - probing depth will exceed probe tool length", CRITICAL)

    def get_parms(self):
        self.send_dict = {key: self['lineEdit_' + key].text() for key in (self.parm_list)}
        for key in ['allow_auto_zero', 'allow_auto_skew', 'cal_avg_error', 'cal_x_error', 'cal_y_error']:
            val = '1' if self[key].isChecked() else '0'
            self.send_dict.update( {key: val} )

    def show_results(self, line):
        for key in self.status_list:
            self['status_' + key].setText(line[key])
        self.lineEdit_cal_offset.setText(line['offset'])

    def set_offsets(self, x, y):
        self.lineEdit_adj_x.setText(x)
        self.lineEdit_adj_y.setText(y)

    ##############################
    # required class boiler code #
    ##############################
    def __getitem__(self, item):
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)

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

