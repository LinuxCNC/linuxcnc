#!/usr/bin/env python3
#
# QTVcp Widget - Machine Log
# Copyright (c) 2017 Chris Morley with additions by Steve Richardson
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
###############################################################################

import os
import hashlib
import time

from PyQt5.QtWidgets import QWidget, QTextEdit, QTableWidget, QTableWidgetItem, QVBoxLayout
from PyQt5.QtCore import QFile, pyqtProperty
import PyQt5.QtWidgets as QtWidgets
from PyQt5.QtGui import QColor
from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Info
from qtvcp import logger

# Instiniate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# AUX_PRGM holds helper program loader
# INI holds ini details
# ACTION gives commands to linuxcnc
# LOG is for running code logging
STATUS = Status()
INFO = Info()
LOG = logger.getLogger(__name__)


class MachineLog(QWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(MachineLog, self).__init__(parent)
        self._delay = 0
        self._hash_code = None
        self._machine_log = True
        self._machine_log_severity = False
        self._integrator_log = False
        self._critical_fg_color = QColor(255, 255, 255)
        self._critical_bg_color = QColor(255, 0, 0)
        self._error_fg_color = QColor(255, 0, 0)
        self._error_bg_color = QColor(255,255,255)
        self._warning_fg_color = QColor(255, 255, 0)
        self._warning_bg_color = QColor(255, 255, 255)
        self._debug_fg_color = QColor(128, 128, 128)
        self._debug_bg_color = QColor(255, 255, 255)
        self._info_fg_color = QColor(0, 0, 0)
        self._info_bg_color = QColor(255, 255, 255)

        self.integratorPath = os.path.expanduser(INFO.QTVCP_LOG_HISTORY_PATH)
        self.machineLogPath = os.path.expanduser(INFO.MACHINE_LOG_HISTORY_PATH)
        
        self.logText = QTextEdit(self)
        self.logTable = QTableWidget(self)

        layout = QVBoxLayout(self)
        layout.addWidget(self.logText)
        layout.addWidget(self.logTable)
        self.setLayout(layout)

        self.logTable.setColumnCount(3)
        self.logTable.setEditTriggers(QtWidgets.QAbstractItemView.NoEditTriggers)
        self.logTable.setSelectionMode(QtWidgets.QAbstractItemView.NoSelection)
        self.logTable.setSelectionBehavior(QtWidgets.QAbstractItemView.SelectRows)
        self.logTable.setShowGrid(False)
        self.logTable.setSortingEnabled(False)

        horizontalHeader = self.logTable.horizontalHeader()
        horizontalHeader.resizeSection(0, 100)
        horizontalHeader.resizeSection(1, 75)
        horizontalHeader.setSectionResizeMode(2, QtWidgets.QHeaderView.Stretch)
        horizontalHeader.setVisible(False)

        verticalHeader = self.logTable.verticalHeader()
        verticalHeader.setSectionResizeMode(QtWidgets.QHeaderView.ResizeToContents)
        verticalHeader.setVisible(False)

    def _hal_init(self):
        if self._machine_log:
            STATUS.connect('machine-log-changed',lambda w: self.loadLog())
            self.logTable.hide()
            self.logText.show()
        elif self._machine_log_severity:
            STATUS.connect('update-machine-log', lambda w, d, o: self.updateMachineLog(d, o))
            self.logTable.show()
            self.logText.hide()
        elif self._integrator_log:
            STATUS.connect('periodic', self._periodicCheck)
            self.logTable.hide()
            self.logText.show()

    def _periodicCheck(self, w):
        if self._delay < 9:
            self._delay += 1
            return
        if STATUS.is_status_valid() == False:
            return
        self._delay = 0
        m1 = self.md5sum(self.integratorPath)
        if m1 and self._hash_code != m1:
            self._hash_code = m1
            self.loadIntegratorLog()

       # create a hash code
    def md5sum(self,filename):
        try:
            f = open(filename, "rb")
        except:
            return None
        else:
            return hashlib.md5(f.read()).hexdigest()


    def remove_options(self, option, to_remove):
        options = option.split(',')
        options = [opt for opt in options if opt not in to_remove]
        return ','.join(options)

    def updateMachineLog(self, message, option):
        if message:
            if option is None: option = ''

            if 'DATE' in option:
                dateItem = QTableWidgetItem(time.strftime("%a, %b %d %Y %X"))
            else:
                dateItem = QTableWidgetItem(time.strftime("%H:%M:%S"))

            option = self.remove_options(option, ['DATE', 'TIME', 'INITIAL'])

            severityItem = QTableWidgetItem(option.strip())

            msgItem = QTableWidgetItem(message.strip())

            if 'DEBUG' in option:
                msgItem.setForeground(self._debug_fg_color)
                dateItem.setForeground(self._debug_fg_color)
                severityItem.setForeground(self._debug_fg_color)
                msgItem.setBackground(self._debug_bg_color)
                dateItem.setBackground(self._debug_bg_color)
                severityItem.setBackground(self._debug_bg_color)
            elif 'SUCCESS' in option:
                msgItem.setForeground(QColor(0, 128, 0))
                dateItem.setForeground(QColor(0, 128, 0))
                severityItem.setForeground(QColor(0, 128, 0))
            elif 'WARNING' in option:
                msgItem.setForeground(self._warning_fg_color)
                dateItem.setForeground(self._warning_fg_color)
                severityItem.setForeground(self._warning_fg_color)
                msgItem.setBackground(self._warning_bg_color)
                dateItem.setBackground(self._warning_bg_color)
                severityItem.setBackground(self._warning_bg_color)
            elif 'ERROR' in option:
                msgItem.setForeground(self._error_fg_color)
                dateItem.setForeground(self._error_fg_color)
                severityItem.setForeground(self._error_fg_color)
                msgItem.setBackground(self._error_bg_color)
                dateItem.setBackground(self._error_bg_color)
                severityItem.setBackground(self._error_bg_color)
            elif 'CRITICAL' in option:
                msgItem.setForeground(self._critical_fg_color)
                dateItem.setForeground(self._critical_fg_color)
                severityItem.setForeground(self._critical_fg_color)
                msgItem.setBackground(self._critical_bg_color)
                dateItem.setBackground(self._critical_bg_color)
                severityItem.setBackground(self._critical_bg_color)
            else:
                msgItem.setForeground(self._info_fg_color)
                dateItem.setForeground(self._info_fg_color)
                severityItem.setForeground(self._info_fg_color)
                msgItem.setBackground(self._info_bg_color)
                dateItem.setBackground(self._info_bg_color)
                severityItem.setBackground(self._info_bg_color)


            self.logTable.insertRow(self.logTable.rowCount())
            self.logTable.setItem(self.logTable.rowCount()-1, 0, dateItem)
            self.logTable.setItem(self.logTable.rowCount()-1, 1, severityItem)
            self.logTable.setItem(self.logTable.rowCount()-1, 2, msgItem)
            self.logTable.scrollToBottom()

    def loadLog(self):
        file = QFile(self.machineLogPath)
        file.open(QFile.ReadOnly)
        logText = file.readAll()
        file.close()
        self.logText.setPlainText(str(logText, encoding='utf8'))
        # scroll down to show last entry
        self.logText.verticalScrollBar().setSliderPosition(self.logText.verticalScrollBar().maximum())

    def loadIntegratorLog(self):
        file = QFile(self.integratorPath)
        file.open(QFile.ReadOnly)
        logText = file.readAll()
        file.close()
        if str(logText, encoding='utf8') == "":
            self.logText.setPlainText('No Logging found. Is QtVcp in debugging or verbose mode (-i, -d or -v)?')
            return
        self.logText.setPlainText(str(logText, encoding='utf8'))
        # scroll down to show last entry
        self.logText.verticalScrollBar().setSliderPosition(self.logText.verticalScrollBar().maximum())

    def showEvent(self, ev):
        self.scrollToBottom()

    def scrollToBottom(self):
        # scroll down to show last entry
        self.logText.verticalScrollBar().setSliderPosition(self.logText.verticalScrollBar().maximum())
        self.logTable.scrollToBottom()

    def hideCursor(self):
        self.logText.setCursorWidth(0)

    def getLogText(self):
        return self.logText.toPlainText()

    def clear(self):
        self.logTable.clearContents()
        self.logTable.setRowCount(0)
        self.logText.setPlainText('')


################## properties ###################

    def _toggle_properties(self, picked):
        data = ('machine_log', 'machine_log_severity', 'integrator_log')

        for i in data:
            if not i == picked:
                self[i+'_option'] = False

    def set_machine_log(self, value):
        self._machine_log = value
        if value:
            self._toggle_properties('machine_log')
    def get_machine_log(self):
        return self._machine_log
    def reset_machine_log(self):
        self._machine_log = True
    machine_log_option = pyqtProperty(bool, get_machine_log, set_machine_log, reset_machine_log)

    def set_machine_log_severity(self, value):
            self._machine_log_severity = value
            if value:
                self._toggle_properties('machine_log_severity')
    def get_machine_log_severity(self):
            return self._machine_log_severity
    def reset_machine_log_severity(self):
            self._machine_log_severity = False
    machine_log_severity_option = pyqtProperty(bool, get_machine_log_severity, set_machine_log_severity, reset_machine_log_severity)


    def set_integrator_log(self, value):
        self._integrator_log = value
        if value:
            self._toggle_properties('integrator_log')
    def get_integrator_log(self):
        return self._integrator_log
    def reset_integrator_log(self):
        self._integrator_log = False
    integrator_log_option = pyqtProperty(bool, get_integrator_log, set_integrator_log, reset_integrator_log)

    def set_critical_fg_color(self, value):
        self._critical_fg_color = value
    def get_critical_fg_color(self):
        return self._critical_fg_color
    def reset_critical_fg_color(self):
        self._critical_fg_color = QColor(255, 255, 255)

    critical_fg_color_option = pyqtProperty(QColor, get_critical_fg_color, set_critical_fg_color, reset_critical_fg_color)

    def set_critical_bg_color(self, value):
        self._critical_bg_color = value
    def get_critical_bg_color(self):
        return self._critical_bg_color
    def reset_critical_bg_color(self):
        self._critical_bg_color = QColor(255, 0, 0)

    critical_bg_color_option = pyqtProperty(QColor, get_critical_bg_color, set_critical_bg_color, reset_critical_bg_color)

    def set_error_fg_color(self, value):
        self._error_fg_color = value
    def get_error_fg_color(self):
        return self._error_fg_color
    def reset_error_fg_color(self):
        self._error_fg_color = QColor(255, 0, 0)

    error_fg_color_option = pyqtProperty(QColor, get_error_fg_color, set_error_fg_color, reset_error_fg_color)

    def set_error_bg_color(self, value):
        self._error_bg_color = value
    def get_error_bg_color(self):
        return self._error_bg_color
    def reset_error_bg_color(self):
        self._error_bg_color = QColor(255, 255, 255)

    error_bg_color_option = pyqtProperty(QColor, get_error_bg_color, set_error_bg_color, reset_error_bg_color)

    def set_warning_fg_color(self, value):
        self._warning_fg_color = value
    def get_warning_fg_color(self):
        return self._warning_fg_color
    def reset_warning_fg_color(self):
        self._warning_fg_color = QColor(255, 255, 0)

    warning_fg_color_option = pyqtProperty(QColor, get_warning_fg_color, set_warning_fg_color, reset_warning_fg_color)

    def set_warning_bg_color(self, value):
        self._warning_bg_color = value
    def get_warning_bg_color(self):
        return self._warning_bg_color
    def reset_warning_bg_color(self):
        self._warning_bg_color = QColor(255, 255, 255)

    warning_bg_color_option = pyqtProperty(QColor, get_warning_bg_color, set_warning_bg_color, reset_warning_bg_color)

    def set_debug_fg_color(self, value):
        self._debug_fg_color = value
    def get_debug_fg_color(self):
        return self._debug_fg_color
    def reset_debug_fg_color(self):
        self._debug_fg_color = QColor(128, 128, 128)

    debug_fg_color_option = pyqtProperty(QColor, get_debug_fg_color, set_debug_fg_color, reset_debug_fg_color)

    def set_debug_bg_color(self, value):
        self._debug_bg_color = value
    def get_debug_bg_color(self):
        return self._debug_bg_color
    def reset_debug_bg_color(self):
        self._debug_bg_color = QColor(255, 255, 255)

    debug_bg_color_option = pyqtProperty(QColor, get_debug_bg_color, set_debug_bg_color, reset_debug_bg_color)

    def set_info_fg_color(self, value):
        self._info_fg_color = value
    def get_info_fg_color(self):
        return self._info_fg_color
    def reset_info_fg_color(self):
        self._info_fg_color = QColor(0, 0, 0)

    info_fg_color_option = pyqtProperty(QColor, get_info_fg_color, set_info_fg_color, reset_info_fg_color)

    def set_info_bg_color(self, value):
        self._info_bg_color = value
    def get_info_bg_color(self):
        return self._info_bg_color
    def reset_info_bg_color(self):
        self._info_bg_color = QColor(255, 255, 255)

    info_bg_color_option = pyqtProperty(QColor, get_info_bg_color, set_info_bg_color, reset_info_bg_color)

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

# for testing without editor:
def main():
    import sys
    from PyQt5.QtWidgets import QApplication
    app = QApplication(sys.argv)
    widget = MachineLog()
    widget.show()
    sys.exit(app.exec_())
if __name__ == "__main__":
    main()
