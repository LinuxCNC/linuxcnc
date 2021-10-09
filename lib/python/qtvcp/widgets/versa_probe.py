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
from PyQt5.QtCore import QProcess, QByteArray, QEvent

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Action, Info
from qtvcp import logger
# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# LOG is for running code logging
STATUS = Status()
ACTION = Action()
INFO = Info()
LOG = logger.getLogger(__name__)

current_dir = os.path.dirname(__file__)
SUBPROGRAM = os.path.abspath(os.path.join(current_dir, 'probe_subprog.py'))
HELP = os.path.join(INFO.LIB_PATH,'widgets_ui', 'versa_usage.html')
ICONPATH = os.path.join(INFO.IMAGE_PATH, 'probe_icons')

class VersaProbe(QtWidgets.QWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(VersaProbe, self).__init__(parent)
        self.proc = None
        if INFO.MACHINE_IS_METRIC:
            self.valid = QtGui.QDoubleValidator(0.0, 999.999, 3)
        else:
            self.valid = QtGui.QDoubleValidator(0.0, 99.9999, 4)
        self.setMinimumSize(600, 420)
        # Load the widgets UI file:
        self.filename = os.path.join(INFO.LIB_PATH,'widgets_ui', 'versa_probe.ui')
        try:
            self.instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            LOG.critical(e)
        self.dialog_code = 'CALCULATOR'
        #create parameter dictionary
        self.send_dict = {}
        # these parameters are sent to the subprogram
        self.parm_list = ['adj_x', 'adj_y', 'adj_z', 'adj_angle',
                          'probe_diam', 'max_travel', 'latch_return_dist',
                          'search_vel', 'probe_vel', 'rapid_vel',
                          'side_edge_length', 'tool_probe_height', 'tool_block_height',
                          'xy_clearance', 'z_clearance']
        self.status_list = ['xm', 'xc', 'xp', 'ym', 'yc', 'yp', 'lx', 'ly', 'z', 'd', 'a']

        for i in self.parm_list:
            self['input_' + i].setValidator(self.valid)
        # button connections
        self.btn_help.clicked.connect(self.help_clicked)
        self.inside_buttonGroup.buttonClicked.connect(self.probe_btn_clicked)
        self.outside_buttonGroup.buttonClicked.connect(self.probe_btn_clicked)
        self.skew_buttonGroup.buttonClicked.connect(self.probe_btn_clicked)
        self.length_buttonGroup.buttonClicked.connect(self.probe_btn_clicked)

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

    def _hal_init(self):
        def homed_on_test():
            return (STATUS.machine_is_on() and (STATUS.is_all_homed() or INFO.NO_HOME_REQUIRED))

        STATUS.connect('state-off', lambda w: self.setEnabled(False))
        STATUS.connect('state-estop', lambda w: self.setEnabled(False))
        STATUS.connect('interp-idle', lambda w: self.setEnabled(homed_on_test()))
        STATUS.connect('all-homed', lambda w: self.setEnabled(homed_on_test()))
#        STATUS.connect('error', self.send_error)
        STATUS.connect('periodic', lambda w: self.check_probe())
        STATUS.connect('general',self.return_value)

        # install event filters on all the lineedits
        self.input_search_vel.installEventFilter(self)
        self.input_probe_vel.installEventFilter(self)
        self.input_z_clearance.installEventFilter(self)
        self.input_max_travel.installEventFilter(self)
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
            self.input_latch_return_dist.setText(str(self.PREFS_.getpref( "ps_probe_latch", 0.5, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_probe_diam.setText(str(self.PREFS_.getpref( "ps_probe_diam", 2.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_xy_clearance.setText(str(self.PREFS_.getpref( "ps_xy_clearance", 5.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_side_edge_length.setText(str(self.PREFS_.getpref( "ps_side_edge_length", 5.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_tool_probe_height.setText(str(self.PREFS_.getpref( "ps_probe_height", 20.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_tool_block_height.setText(str(self.PREFS_.getpref( "ps_block_height", 20.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_adj_x.setText(str(self.PREFS_.getpref( "ps_offs_x", 0.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_adj_y.setText(str(self.PREFS_.getpref( "ps_offs_y", 0.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_adj_z.setText(str(self.PREFS_.getpref( "ps_offs_z", 0.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_adj_angle.setText(str(self.PREFS_.getpref( "ps_offs_angle", 0.0, float, 'VERSA_PROBE_OPTIONS')) )
            self.input_rapid_vel.setText(str(self.PREFS_.getpref( "ps_probe_rapid_vel", 60.0, float, 'VERSA_PROBE_OPTIONS')) )

    # when qtvcp closes this gets called
    def _hal_cleanup(self):
        if self.PREFS_:
            LOG.debug('Saving Versa probe data to preference file.')
            self.PREFS_.putpref( "ps_searchvel", float(self.input_search_vel.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_probevel", float(self.input_probe_vel.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_z_clearance", float(self.input_z_clearance.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_probe_max", float(self.input_max_travel.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_probe_latch", float(self.input_latch_return_dist.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_probe_diam", float(self.input_probe_diam.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_xy_clearance", float(self.input_xy_clearance.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_side_edge_length", float(self.input_side_edge_length.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_probe_height", float(self.input_tool_probe_height.text()), float, 'VERSA_PROBE_OPTIONS')
            self.PREFS_.putpref( "ps_block_height", float(self.input_tool_block_height.text()), float, 'VERSA_PROBE_OPTIONS')
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
#        print("String to send ", string_to_send)
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

    def parse_input(self, line):
        line = line.decode("utf-8")
        if "ERROR" in line:
            print(line)
        elif "DEBUG" in line:
            print(line)
        elif "INFO" in line:
            print(line)
        elif "COMPLETE" in line:
            LOG.info("Probing routine completed without errors")
            return_data = line.rstrip().split('$')
            data = json.loads(return_data[1])
            self.show_results(data)
        elif "HISTORY" in line:
            temp = line.strip('HISTORY$')
            STATUS.emit('update-machine-log', temp, 'TIME')
            LOG.info("Probe history updated to machine log")
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

#####################################################
# Helper functions
#####################################################
    def get_parms(self):
        self.send_dict = {key: self['input_' + key].text() for key in (self.parm_list)}
        for key in ['allow_auto_zero', 'allow_auto_skew']:
            val = '1' if self[key].isChecked() else '0'
            self.send_dict.update( {key: val} )
        
    def check_probe(self):
        self.led_probe_function_chk.setState(hal.get_value('motion.probe-input'))

    def show_results(self, line):
        for key in self.status_list:
            self['status_' + key].setText(line[key])

    def pop_help(self):
        d = QtWidgets.QDialog(self)
        d.setMinimumWidth(600)
        l = QtWidgets.QVBoxLayout()
        t = QtWidgets.QTextEdit()
        t.setReadOnly(False)
        l.addWidget(t)

        bBox = QtWidgets.QDialogButtonBox()
        bBox.addButton('Ok', QtWidgets.QDialogButtonBox.AcceptRole)
        bBox.accepted.connect(d.accept)
        l.addWidget(bBox)
        d.setLayout(l)

        try:
            file = QtCore.QFile(HELP)
            file.open(QtCore.QFile.ReadOnly)
            html = file.readAll()
            html = str(html, encoding='utf8')
            html = html.replace("../images/probe_icons/","{}/probe_icons/".format(INFO.IMAGE_PATH))
            t.setHtml(html)
        except Exception as e:
            t.setText('Versa Probe Help file Unavailable:\n\n{}'.format(e))

        d.show()
        d.exec_()

########################################
# required boiler code
########################################
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

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

