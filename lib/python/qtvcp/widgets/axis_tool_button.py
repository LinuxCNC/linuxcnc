#!/usr/bin/env python3
#
# QTVcp Widget
# Copyright (c) 2018 Chris Morley
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

import hal

from PyQt5.QtWidgets import QToolButton, QMenu, QAction
from PyQt5.QtCore import pyqtProperty
from PyQt5.QtGui import QIcon

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.widgets.indicatorMixIn import IndicatedMixIn
from qtvcp.core import Status, Action, Info
from qtvcp import logger

# Instiniate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# AUX_PRGM holds helper program loader
# INI holds ini details
# ACTION gives commands to linuxcnc
# LOG is for running code logging
STATUS = Status()
INFO = Info()
ACTION = Action()
LOG = logger.getLogger(__name__)
# Force the log level for this module
#LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

class AxisToolButton(QToolButton, IndicatedMixIn):
    def __init__(self, parent=None):
        super(AxisToolButton, self).__init__(parent)
        self._joint = 0
        self._axis = ''
        self._last = 0
        self._block_signal = False
        self._halpin_option = True
        self.dialog_code = 'ENTRY'
        self.display_units_mm = 0
        homeOption = False

        self._showGoto = False
        self._showZero = True
        self._showSet = True
        self._showLast = True
        self._showDivide = True

        SettingMenu = QMenu(self)
        self.settingMenu = SettingMenu
        self.zeroButton = QAction(QIcon('exit24.png'), 'Zero', self)
        self.zeroButton.triggered.connect(self.Zero)
        SettingMenu.addAction(self.zeroButton)
        self.setButton = QAction(QIcon('exit24.png'), 'Set', self)
        self.setButton.triggered.connect(self.SetOrigin)
        SettingMenu.addAction(self.setButton)
        self.divideButton = QAction(QIcon('exit24.png'), 'Divide By 2', self)
        self.divideButton.triggered.connect(self.Divide)
        SettingMenu.addAction(self.divideButton)
        self.lastButton = QAction(QIcon('exit24.png'), 'Set To Last', self)
        self.lastButton.triggered.connect(self.Last)
        SettingMenu.addAction(self.lastButton)
        self.goToG53Button = QAction(self)
        self.goToG53Button.triggered.connect(self.goToG53)
        SettingMenu.addAction(self.goToG53Button)
        self.goToG5xButton = QAction(self)
        self.goToG5xButton.triggered.connect(self.goToG5x)
        SettingMenu.addAction(self.goToG5xButton)

        # option for later - should put a sub menu in for joints homing though
        if homeOption:
            SettingMenu.addSeparator()
            self.homeButton = QAction(QIcon('exit24.png'), 'Home', self)
            self.homeButton.triggered.connect(self.Home)
            SettingMenu.addAction(self.homeButton)
            #self.homeButton.setEnabled(False)

            self.UnHomeButton = QAction(QIcon('exit24.png'), 'Unhome', self)
            self.UnHomeButton.triggered.connect(self.Unhome)
            SettingMenu.addAction(self.UnHomeButton)
            #self.homeButton.setEnabled(False)

        self.setMenu(SettingMenu)
        self.clicked.connect(self.selectJoint)
        self.toggled.connect(self.selectJoint)

    # Override setText function so we can toggle displayed text
    def setText(self, text):
        if not self._state_text:
            super(AxisToolButton,self).setText(text)
            return
        if self.isCheckable():
            if self.isChecked():
                super(AxisToolButton,self).setText(self._true_string)
            else:
                super(AxisToolButton,self).setText(self._false_string)
        elif self._indicator_state:
            super(AxisToolButton,self).setText(self._true_string)
        else:
            super(AxisToolButton,self).setText(self._false_string)

    def _hal_init(self):
        super(AxisToolButton, self)._hal_init()
        def homed_on_test():
            return (STATUS.machine_is_on()
                    and (STATUS.is_all_homed() or INFO.NO_HOME_REQUIRED))

        STATUS.connect('metric-mode-changed', self._switch_units)
        STATUS.connect('state-off', lambda w: self._enableGroup(True,False))
        STATUS.connect('state-on', lambda w: self._enableGroup(True, True))
        STATUS.connect('interp-idle', lambda w: self._enableGroup(homed_on_test(),STATUS.machine_is_on()))
        STATUS.connect('interp-run', lambda w: self._enableGroup(False, False))
        STATUS.connect('all-homed', lambda w: self._enableGroup(homed_on_test(), STATUS.machine_is_on()))
        STATUS.connect('not-all-homed', lambda w, data: self._enableGroup(False,STATUS.machine_is_on()))
        STATUS.connect('interp-paused', lambda w: self._enableGroup(False,False))
        STATUS.connect('motion-mode-changed', lambda w,data: self.modeChanged(data))
        STATUS.connect('joint-selection-changed', lambda w,data: self.ChangeState(joint = data, axis= STATUS.get_selected_axis()))
        STATUS.connect('axis-selection-changed', lambda w,data: self.ChangeState(joint = STATUS.get_selected_joint(), axis = data))
        if self._halpin_option and self._joint != -1:
            if self._pin_name_ == '':
                pname = self.HAL_NAME_
            else:
                pname = self._pin_name_

            self.hal_pin_joint = self.HAL_GCOMP_.newpin(str(pname + '-joint'), hal.HAL_BIT, hal.HAL_OUT)
            self.hal_pin_axis = self.HAL_GCOMP_.newpin(str(pname + '-axis'), hal.HAL_BIT, hal.HAL_OUT)
        STATUS.connect('general',self.return_value)

    def _enableGroup(self, state,bstate):
        for i in(self.zeroButton, self.setButton,self.divideButton,
                self.lastButton,self.goToG53Button, self.goToG5xButton ):
            i.setEnabled(state)

        self.setEnabled(bstate)

    def Zero(self):
        axis, now = self._a_from_j(self._axis)
        if axis:
            self._last = now
            ACTION.SET_AXIS_ORIGIN(axis, 0)
            STATUS.emit('update-machine-log', 'Zeroed Axis %s' % axis, 'TIME')

    def SetOrigin(self):
        axis, now = self._a_from_j(self._axis)
        if axis:
            mess = {'NAME':self.dialog_code,'ID':'%s__' % self.objectName(),
            'AXIS':axis,'CURRENT':now,
            'TITLE':'Set %s Origin'% axis,
            'GEONAME':'axisToolButtonDialog_{}'.format(self.dialog_code),
            'AXIS':axis}
            STATUS.emit('dialog-request', mess)
            LOG.debug('message sent:{}'.format (mess))

    # process the STATUS return message
    def return_value(self, w, message):
        num = message['RETURN']
        code = bool(message.get('ID') == '%s__'% self.objectName())
        name = bool(message.get('NAME') == self.dialog_code)
        if code and name and num is not None:
            LOG.debug('message return:{}'.format (message))
            axis = message.get('AXIS')
            self._last = message.get('CURRENT')
            ACTION.SET_AXIS_ORIGIN(axis, num)
            STATUS.emit('update-machine-log', 'Set Origin of Axis %s to %f' %(axis, num), 'TIME')

    def Divide(self):
        axis, now = self._a_from_j(self._axis)
        if axis:
            self._last = now
            try:
                x = now/2.0
                ACTION.SET_AXIS_ORIGIN(axis, x)
                text = 'Divided Axis %s in Half - %f'% (axis, x)
                STATUS.emit('update-machine-log', text, 'TIME')
            except ZeroDivisionError:
                pass

    def Last(self):
        axis, now = self._a_from_j(self._axis)
        if axis:
            ACTION.SET_AXIS_ORIGIN(axis, self._last)
            text = 'Reset Axis %s from %f to Last Value: %f' %(axis, now, self._last)
            STATUS.emit('update-machine-log', text, 'TIME')
            self._last = now

    def Home(self):
        #axis, now = self._a_from_j(self._axis)
        #if axis:
            ACTION.SET_MACHINE_HOMING(self._joint)
            STATUS.emit('update-machine-log', 'Homed Axis %s' % self._joint, 'TIME')

    def Unhome(self):
        #axis, now = self._a_from_j(self._axis)
        #if axis:
            ACTION.SET_MACHINE_UNHOMED(self._joint)
            STATUS.emit('update-machine-log', 'Unhomed Axis %s' % self._joint, 'TIME')

    def goToG53(self):
        j = "XYZABCUVW"
        jnum = j.find(self._axis)
        ACTION.CALL_MDI('G53 G0 {}0'.format(INFO.GET_NAME_FROM_JOINT.get(jnum)))
    def goToG5x(self):
        j = "XYZABCUVW"
        jnum = j.find(self._axis)
        ACTION.CALL_MDI('G0 {}0'.format(INFO.GET_NAME_FROM_JOINT.get(jnum)))

    def _a_from_j(self, axis):
        if STATUS.is_joint_mode():
            return None, None
        if axis == '':
            axis = STATUS.get_selected_axis()
        j = "XYZABCUVW"
        try:
            jnum = j.find(axis)
        except:
            return None, None
        p,r,d = STATUS.get_position()
        if self.display_units_mm != INFO.MACHINE_IS_METRIC:
            r = INFO.convert_units_9(r)
        return axis, r[jnum]

    def selectJoint(self):
        if self._block_signal or self._joint == -1 or self._axis == '': return
        if self.isChecked() == True:
            if STATUS.is_joint_mode():
                ACTION.SET_SELECTED_JOINT(self._joint)
                if self._halpin_option:
                    self.hal_pin_joint.set(self.isChecked())
            else:
                ACTION.SET_SELECTED_AXIS(self._axis)
                # set this without causing a STATUS message output
                # in case we are selecting an axis to un/home
                STATUS.selected_joint = self._joint
                if self._halpin_option:
                    self.hal_pin_axis.set(self.isChecked())
        else:
            if STATUS.is_joint_mode():
                ACTION.SET_SELECTED_JOINT(-1)
            else:
                ACTION.SET_SELECTED_AXIS('None')
                # set this without causing a STATUS message output
                # in case we are selecting an axis to un/home
                STATUS.selected_joint = -1
            if self._halpin_option:
                self.hal_pin_axis.set(False)

    def ChangeState(self, joint = None, axis = None):
        if STATUS.is_joint_mode():
            if int(joint) != self._joint:
                self._block_signal = True
                self.setChecked(False)
                self._block_signal = False
                if self._halpin_option and self._joint != -1:
                    self.hal_pin_joint.set(False)
        else:
            if str(axis) != self._axis:
                self._block_signal = True
                self.setChecked(False)
                self._block_signal = False
                if self._halpin_option and self._axis != '':
                    self.hal_pin_joint.set(False)

    def modeChanged(self, mode):
        self.selectJoint()
        return
        if mode == STATUS.linuxcnc.TRAJ_MODE_FREE:
            ACTION.SET_SELECTED_JOINT(self._joint)
        else:
            ACTION.SET_SELECTED_AXIS(self._axis)

    def _switch_units(self, widget, data):
        self.display_units_mm = data
        if self.display_units_mm != INFO.MACHINE_IS_METRIC:
            self._last = INFO.convert_units(self._last)

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    #
    ########################################################################

    def set_joint(self, data):
        self._joint = data
        if data == -1:
            self.setPopupMode(QToolButton.InstantPopup)
    def get_joint(self):
        return self._joint
    def reset_joint(self):
        self._joint = -1
    joint_number = pyqtProperty(int, get_joint, set_joint, reset_joint)

    def set_axis(self, data):
        if data.upper() in('X','Y','Z','A','B','C','U','V','W'):
            self._axis = str(data.upper())
            text = 'Go To G53 Origin in {}'.format(self._axis)
            self.goToG53Button.setText(text)
            text = 'Go To G5x Origin in {}'.format(self._axis)
            self.goToG5xButton.setText(text)
    def get_axis(self):
        return self._axis
    def reset_axis(self):
        self._axis = 'X'
    axis_letter = pyqtProperty(str, get_axis, set_axis, reset_axis)

    def set_halpin_option(self, value):
        self._halpin_option = value
    def get_halpin_option(self):
        return self._halpin_option
    def reset_halpin_option(self):
        self._halpin_option = True
    halpin_option = pyqtProperty(bool, get_halpin_option, set_halpin_option, reset_halpin_option)

    def set_dialog_code(self, data):
        self.dialog_code = data
    def get_dialog_code(self):
        return self.dialog_code
    def reset_dialog_code(self):
        self.dialog_code = 'ENTRY'
    dialog_code_string = pyqtProperty(str, get_dialog_code, set_dialog_code, reset_dialog_code)

    ####################
    ## menu properties
    ####################

    def set_showSet(self, data):
        self._showSet = data
        self.setButton.setVisible(data)
    def get_showSet(self):
        return self._showSet
    def reset_showSet(self):
        self._showSet = True
    showSetOrigin = pyqtProperty(bool, get_showSet, set_showSet, reset_showSet)

    def set_showZero(self, data):
        self._showZero = data
        self.zeroButton.setVisible(data)
    def get_showZero(self):
        return self._showZero
    def reset_showZero(self):
        self._showZero = True
    showZeroOrigin = pyqtProperty(bool, get_showZero, set_showZero, reset_showZero)

    def set_showGoto(self, data):
        self._showGoto = data
        self.goToG53Button.setVisible(data)
        self.goToG5xButton.setVisible(data)
    def get_showGoto(self):
        return self._showGoto
    def reset_showGoto(self):
        self._showGoto = False
    showGotoOrigin = pyqtProperty(bool, get_showGoto, set_showGoto, reset_showGoto)

    def set_showLast(self, data):
        self._showLast = data
        self.lastButton.setVisible(data)
    def get_showLast(self):
        return self._showLast
    def reset_showLast(self):
        self._showLast = True
    showLast = pyqtProperty(bool, get_showLast, set_showLast, reset_showLast)

    def set_showDivide(self, data):
        self._showDivide = data
        self.divideButton.setVisible(data)
    def get_showDivide(self):
        return self._showDivide
    def reset_showDivide(self):
        self._showDivide = True
    showDivide = pyqtProperty(bool, get_showDivide, set_showDivide, reset_showDivide)

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

# for testing without editor:
def main():
    import sys
    from PyQt5.QtWidgets import QApplication
    app = QApplication(sys.argv)
    widget = AxisToolButton()
    widget.show()

    sys.exit(app.exec_())
if __name__ == "__main__":
    main()
