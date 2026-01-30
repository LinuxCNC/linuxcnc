#!/usr/bin/env python3
# QtVcp Widget - user temporary DRO label widget
# This widgets displays linuxcnc axis position information.
#
# Copyright (c) 2025 Chris Morley
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

import linuxcnc
from math import isclose

from PyQt5 import QtCore
from PyQt5.QtWidgets import QMenu, QAction
from PyQt5.QtGui import QIcon, QColor

from qtvcp.widgets.simple_widgets import ScaledLabel
from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Info, Action
from qtvcp import logger

# Instantiate the libraries with global reference
# INFO holds INI file details
# STATUS gives us status messages from linuxcnc
# LOG is for running code logging
STATUS = Status()
INFO = Info()
ACTION = Action()
LOG = logger.getLogger(__name__)

# Force the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


class UserDROLabel(ScaledLabel, _HalWidgetBase):
    def __init__(self, parent=None):
        super(UserDROLabel, self).__init__(parent)
        # axis index of 9 axis
        self.axis_index = 0
        # linuxcnc joint number
        self._joint_type = 1
        self.display_units_mm = 0
        self.metric_text_template = '%10.3f'
        self.imperial_text_template = '%9.4f'
        self.angular_text_template = '%9.2f'
        self._text =' -000.0000'
        self._offset = [0.0] * 9
        self._lastOffset = [0.0] * 9
        self._negativeColor = QColor(0, 0, 100, 150)
        self._positiveColor = QColor(100, 0, 0, 150)
        self._zeroColor = QColor(0, 100, 0, 150)
        self._imperialTolerence = .00005
        self._metricTolerence = .005
        self._currentTolerence = .0005
        self._lastNum = 0.0

        # menu stuff
        self.dialog_code = 'CALCULATOR'

    def mousePressEvent(self, event):
        SettingMenu = QMenu(self)
        self.settingMenu = SettingMenu
        self.zeroButton = QAction(QIcon('exit24.png'), 'Zero', self)
        self.zeroButton.triggered.connect(self.Zero)
        SettingMenu.addAction(self.zeroButton)
        self.setButton = QAction(QIcon('exit24.png'), 'Set', self)
        self.setButton.triggered.connect(self.SetOrigin)
        SettingMenu.addAction(self.setButton)
        self.lastButton = QAction(QIcon('exit24.png'), 'Set To Last', self)
        self.lastButton.triggered.connect(self.Last)
        SettingMenu.addAction(self.lastButton)

        SettingMenu.popup(event.globalPos())

    def _hal_init(self):
        super(UserDROLabel, self)._hal_init()
        STATUS.connect('current-position', self.update)
        STATUS.connect('metric-mode-changed', self._switch_units)
        STATUS.connect('general',self.return_value)

        if self._joint_type == linuxcnc.ANGULAR:
            self._current_text_template =  self.angular_text_template
        elif self.display_units_mm:
            self._current_text_template = self.metric_text_template
        else:
            self._current_text_template = self.imperial_text_template

        if INFO.MACHINE_IS_METRIC:
            self._currentTolerence = self._metricTolerence
        else:
            self._currentTolerence = self._imperialTolerence

    def update(self, widget, absolute, relative, dtg, joint):
        if self.display_units_mm != INFO.MACHINE_IS_METRIC:
            absolute = INFO.convert_units_9(absolute)

        tmpl = lambda s: self._current_text_template % s

        try:
            num = absolute[self.axis_index] + self._offset[self.axis_index]
            self.setColor(num)
            self.setText(tmpl(num))
        except Exception as e:
            print(e)
            pass

    def setColor(self, data):
        if not data == self._lastNum:
            if isclose(data, 0,abs_tol = self._currentTolerence) :
                self.setStyleSheet("QLabel { background-color :"+self._zeroColor.name()+" ;}");
            elif data <0:
                self.setStyleSheet("QLabel { background-color :"+self._negativeColor.name()+" ;}");
            else:
                self.setStyleSheet("QLabel { background-color :"+self._positiveColor.name()+" ;}");
            self._lastNum = data

    def _switch_units(self, widget, data):
        self.display_units_mm = data
        self.update_units()

    def update_units(self):
        if self._joint_type == linuxcnc.ANGULAR:
            self._current_text_template =  self.angular_text_template
        elif self.display_units_mm:
            self._current_text_template = self.metric_text_template
        else:
            self._current_text_template = self.imperial_text_template

    def set_to_inch(self):
        self.display_units_mm = 0
        if self._joint_type == linuxcnc.ANGULAR:
            self._current_text_template =  self.angular_text_template
        else:
            self._current_text_template = self.imperial_text_template

    def set_to_mm(self):
        self.display_units_mm = 1
        if self._joint_type == linuxcnc.ANGULAR:
            self._current_text_template =  self.angular_text_template
        else:
            self._current_text_template = self.metric_text_template

    def get_current_position(self):
        if STATUS.is_joint_mode():
            return None
        p,r,d = STATUS.get_position()
        if self.display_units_mm != INFO.MACHINE_IS_METRIC:
            p = INFO.convert_units_9(p)
        return p[self.axis_index]

    @QtCore.pyqtSlot(bool)
    def zero (self, data=True):
        p = self.get_current_position()
        self._lastOffset[self.axis_index] = self._offset[self.axis_index]
        self._offset[self.axis_index] = - p

    @QtCore.pyqtSlot(int)
    def setAxisIndex(self, num):
        self.axis_index = num

    @QtCore.pyqtSlot(str)
    def setAxisLetter(self, let):
        self.axis_index = self.axisToIndex(let)
        print(let,self.axis_index)

    def axisToIndex(self, let):
        conversion = {"X":0, "Y":1, "Z":2, "A":3, "B":4, "C":5, "U":6, "V":7, "W":8}
        return conversion[let]

    def indexToAxis(self, index):
        conversion = {0:"X", 1:"Y", 2:"Z", 3:"A", 4:"B", 5:"C", 6:"U", 7:"V", 8:"W"}
        return conversion[index]

    #####################
    # Menu callback code
    #####################
    def Zero(self):
        self.zero()

    def SetOrigin(self):
        now = self.get_current_position()
        axis = self.indexToAxis(self.axis_index)
        if axis:
            mess = {'NAME':self.dialog_code,'ID':'%s__' % self.objectName(),
            'AXIS':axis,'CURRENT':now,
            'TITLE':'Set %s Reference'% axis,
            'GEONAME':'UserDROLabelDialog_{}'.format(self.dialog_code),
            'AXIS':axis}
            STATUS.emit('dialog-request', mess)
            LOG.debug('message sent:{}'.format (mess))

    def Last(self):
        axis = self.indexToAxis(self.axis_index)
        now = self.get_current_position() + self._offset[self.axis_index]
        if axis:
            temp = self._offset[self.axis_index]
            self._offset[self.axis_index] = self._lastOffset[self.axis_index]
            self._lastOffset[self.axis_index] = temp
            text = 'Reset Axis %s from %f to Last Value: %f' %(axis, now, self._lastOffset[self.axis_index] )

    # process the STATUS return message
    def return_value(self, w, message):
        num = message['RETURN']
        code = bool(message.get('ID') == '%s__'% self.objectName())
        name = bool(message.get('NAME') == self.dialog_code)
        if code and name and num is not None:
            LOG.debug('message return:{}'.format (message))
            axis = message.get('AXIS')
            # do something

            p = self.get_current_position()
            self._lastOffset[self.axis_index] = self._offset[self.axis_index]
            self._offset[self.axis_index] = - (p-num)

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    ########################################################################

    def setmetrictemplate(self, data):
        self.metric_text_template = data
        self.update_units()
    def getmetrictemplate(self):
        return self.metric_text_template
    def resetmetrictemplate(self):
        self.metric_text_template =  '%10.3f'
    metric_template = QtCore.pyqtProperty(str, getmetrictemplate, setmetrictemplate, resetmetrictemplate)

    def setimperialtexttemplate(self, data):
        self.imperial_text_template = data
        self.update_units()
    def getimperialtexttemplate(self):
        return self.imperial_text_template
    def resetimperialtexttemplate(self):
        self.imperial_text_template =  '%9.4f'
    imperial_template = QtCore.pyqtProperty(str, getimperialtexttemplate, setimperialtexttemplate, resetimperialtexttemplate)

    def setangulartexttemplate(self, data):
        self.angular_text_template = data
        self.update_units()
    def getangulartexttemplate(self):
        return self.angular_text_template
    def resetangulartexttemplate(self):
        self.angular_text_template =  '%9.2f'
    angularTemplate = QtCore.pyqtProperty(str, getangulartexttemplate, setangulartexttemplate, resetangulartexttemplate)

    def getNegativeColor(self):
        return self._negativeColor
    def setNegativeColor(self, value):
        self._negativeColor = value
    negativeColor = QtCore.pyqtProperty(QColor, getNegativeColor, setNegativeColor)

    def getPositiveColor(self):
        return self._positiveColor
    def setPositiveColor(self, value):
        self._positiveColor = value
    positiveColor = QtCore.pyqtProperty(QColor, getPositiveColor, setPositiveColor)

    ####################
    ## menu properties
    ####################

    def set_dialog_code(self, data):
        self.dialog_code = data
    def get_dialog_code(self):
        return self.dialog_code
    def reset_dialog_code(self):
        self.dialog_code = 'CALCULATOR'
    dialogName = QtCore.pyqtProperty(str, get_dialog_code, set_dialog_code, reset_dialog_code)

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
    widget = UserDROLabel()
    widget.show()
    sys.exit(app.exec_())
if __name__ == "__main__":
    main()
