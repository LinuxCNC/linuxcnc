#!/usr/bin/env python3
# qtVcp Widget - DRO label widget
# This widgets displays linuxcnc axis position information.
#
# Copyright (c) 2017 Chris Morley
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

from PyQt5 import QtCore
from PyQt5.QtWidgets import QMenu, QAction
from PyQt5.QtGui import QIcon

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


class DROLabel(ScaledLabel, _HalWidgetBase):
    def __init__(self, parent=None):
        super(DROLabel, self).__init__(parent)
        self._mode = False
        self._joint_type = 1
        self.diameter = False
        self.reference_type = 0
        # joint index of 9 axis
        self.joint_number = 0
        # linuxcnc joint number
        self._jointNum = 0
        self.display_units_mm = 0
        self.metric_text_template = '%10.3f'
        self.imperial_text_template = '%9.4f'
        self.angular_text_template = '%9.2f'
        self.setText('--------------')
        self.allow_reference_change_requests = True
        self.follow_m7m8_mode = True
        self.force_diameter = False
        self.force_radius = False
        self._scale = 1
        self._user = 0
        self._text =' -000.0000'

        # menu stuff
        self.dialog_code = 'CALCULATOR'
        self._showGoto = False
        self._showZero = False
        self._showSet = False
        self._showLast = False
        self._showDivide = False

        # for stylesheet reading
        self._isHomed = False

    def mousePressEvent(self, event):
        menu = QMenu(self)
        state = (STATUS.machine_is_on()
                    and (STATUS.is_all_homed() or INFO.NO_HOME_REQUIRED))

        if self._showZero:
            self.zeroButton = QAction(QIcon('exit24.png'), 'Zero', self)
            self.zeroButton.triggered.connect(self.Zero)
            self.zeroButton.setEnabled(state)
            menu.addAction(self.zeroButton)
        if self._showSet:
            self.setButton = QAction(QIcon('exit24.png'), 'Set', self)
            self.setButton.triggered.connect(self.SetOrigin)
            self.setButton.setEnabled(state)
            menu.addAction(self.setButton)
        if self._showDivide:
            self.divideButton = QAction(QIcon('exit24.png'), 'Divide By 2', self)
            self.divideButton.triggered.connect(self.Divide)
            self.divideButton.setEnabled(state)
            menu.addAction(self.divideButton)
        if self._showLast:
            self.lastButton = QAction(QIcon('exit24.png'), 'Set To Last', self)
            self.lastButton.triggered.connect(self.Last)
            self.lastButton.setEnabled(state)
            menu.addAction(self.lastButton)
        if  self._showGoto:
            text = 'Go To G53 Origin in {}'.format(INFO.GET_NAME_FROM_JOINT.get(self.joint_number))
            self.goToG53Button = QAction(QIcon('exit24.png'), text, self)
            self.goToG53Button.triggered.connect(self.goToG53)
            self.goToG53Button.setEnabled(state)
            menu.addAction(self.goToG53Button)
            text = 'Go To G5x Origin in {}'.format(INFO.GET_NAME_FROM_JOINT.get(self.joint_number))
            self.goToG5xButton = QAction(QIcon('exit24.png'), text, self)
            self.goToG5xButton.triggered.connect(self.goToG5x)
            self.goToG5xButton.setEnabled(state)
            menu.addAction(self.goToG5xButton)

        menu.popup(event.globalPos())

    def _hal_init(self):
        super(DROLabel, self)._hal_init()
        STATUS.connect('homed', lambda w,d: self._home_status_polish(int(d), True))
        STATUS.connect('unhomed', lambda w,d: self._home_status_polish(int(d), False))
        # get position update from STATUS every 100 ms
        if self.joint_number == 10:
            STATUS.connect('current-z-rotation', self.update_rotation)
        else:
            self._jointNum = INFO.GET_JOINT_NUM_FROM_AXIS_INDEX.get(self.joint_number)
            if self._jointNum is None:
                LOG.debug('axis number {} not found in available-axis to joint conversion dict {} of widget: {}'.format(self.joint_number, INFO.GET_JOINT_NUM_FROM_AXIS_INDEX, self.objectName()))
                self._jointNum = 0

            STATUS.connect('motion-mode-changed',self.motion_mode)
            STATUS.connect('current-position', self.update)
            STATUS.connect('metric-mode-changed', self._switch_units)
            if self.follow_m7m8_mode:
                STATUS.connect('diameter-mode', self._switch_modes)
            if self.allow_reference_change_requests:
                STATUS.connect('dro-reference-change-request', self._status_reference_change)

            self._joint_type  = INFO.JOINT_TYPE_INT[self._jointNum]
        if self._joint_type == linuxcnc.ANGULAR:
            self._current_text_template =  self.angular_text_template
        elif self.display_units_mm:
            self._current_text_template = self.metric_text_template
        else:
            self._current_text_template = self.imperial_text_template

        STATUS.connect('general',self.return_value)

    # update ishomed property
    # polish widget so stylesheet sees the property change
    # some stylesheets color the text on home/unhome
    def _home_status_polish(self, d, state):
        if d == self._jointNum or (self.joint_number==10 and d==1):
            self.setProperty('isHomed', state)
            self.style().unpolish(self)
            self.style().polish(self)

    def motion_mode(self, w, mode):
        if mode == linuxcnc.TRAJ_MODE_COORD:
            pass
        # Joint mode
        elif mode == linuxcnc.TRAJ_MODE_FREE:
            self._mode = False
        # axis
        elif mode == linuxcnc.TRAJ_MODE_TELEOP:
            self._mode = True

    @QtCore.pyqtSlot(int)
    @QtCore.pyqtSlot(float)
    def update_user(self, data):
        self._user = data

    def update_rotation(self, widget, rotation):
        degtmpl = lambda s: self.angular_text_template % s
        self.setText(degtmpl(rotation))

    def update(self, widget, absolute, relative, dtg, joint):
        if self.display_units_mm != INFO.MACHINE_IS_METRIC:
            absolute = INFO.convert_units_9(absolute)
            relative = INFO.convert_units_9(relative)
            dtg = INFO.convert_units_9(dtg)

        tmpl = lambda s: self._current_text_template % s

        try:
            if self.reference_type == 0:
                if not self._mode and STATUS.stat.kinematics_type != linuxcnc.KINEMATICS_IDENTITY:
                    self.setText(tmpl(joint[self._jointNum]))
                else:
                    self.setText(tmpl(absolute[self.joint_number]*self._scale))
            elif self.reference_type == 1:
                self.setText(tmpl(relative[self.joint_number]*self._scale))
            elif self.reference_type == 2:
                self.setText(tmpl(dtg[self.joint_number]*self._scale))

            elif self.reference_type == 10:
                self.setText(tmpl(self._user*self._scale))
        except:
            pass

    def _status_reference_change(self,w ,value):
        self.reference_type = value

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

    def _switch_modes(self, w, mode):
        self.diameter = mode
        # only joint 0 (X) can use diameter mode
        if mode and self.joint_number == 0:
            self._scale = 2.0
        else:
            self._scale = 1

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

    def set_to_diameter(self):
        self.diameter = True
        if self.joint_number == 0:
            self._scale = 2.0

    def set_to_radius(self):
        self.diameter = False
        self._scale = 1.0

    # index = index of 9 axis
    def set_joint_type(self, index):
        # convert to linxcnc joint number
        self._jointNum = INFO.GET_JOINT_NUM_FROM_AXIS_INDEX.get(index)
        if self._jointNum is None:
            self._joint_type  = 1
        else:
            self._joint_type  = INFO.JOINT_TYPE_INT[self._jointNum]
        self. update_units()

    #######################
    # menu option code 
    #######################

    def Zero(self):
        pos = self.get_current_position()
        if not pos is None:
            axis = INFO.GET_NAME_FROM_JOINT.get(self.joint_number)
            ACTION.SET_AXIS_ORIGIN(axis, 0)
            STATUS.emit('update-machine-log', 'Zeroed Axis %s' % axis, 'TIME')

    def SetOrigin(self):
        pos = self.get_current_position()
        if not pos is None:
            axis = INFO.GET_NAME_FROM_JOINT.get(self.joint_number)
            mess = {'NAME':self.dialog_code,'ID':'%s__' % self.objectName(),
            'AXIS':axis,
            'CURRENT':pos,
            'TITLE':'Set %s Origin'% axis,
            'GEONAME':'DROWidgetDialog_{}'.format(self.dialog_code),
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
            ACTION.SET_AXIS_ORIGIN(axis, num)
            STATUS.emit('update-machine-log', 'Set Origin of Axis %s to %f' %(axis, num), 'TIME')

    def Divide(self):
        pos = self.get_current_position()
        if not pos is None:
            axis = INFO.GET_NAME_FROM_JOINT.get(self.joint_number)
            try:
                x = pos/2.0
                ACTION.SET_AXIS_ORIGIN(axis, x)
                text = 'Divided Axis %s in Half - %f'% (axis, x)
                STATUS.emit('update-machine-log', text, 'TIME')
            except ZeroDivisionError:
                pass

    def Last(self):
        pos = self.get_current_position()
        if not pos is None:
            axis = INFO.GET_NAME_FROM_JOINT.get(self.joint_number)
            last = ACTION.GET_LAST_RECORDED_ORIGIN(axis)
            ACTION.SET_AXIS_ORIGIN(axis, last)
            text = 'Reset Axis %s from %f to Last Value: %f' %(axis, pos, last)
            STATUS.emit('update-machine-log', text, 'TIME')

    def get_current_position(self):
        if STATUS.is_joint_mode():
            return None
        p,r,d = STATUS.get_position()
        if self.display_units_mm != INFO.MACHINE_IS_METRIC:
            r = INFO.convert_units_9(r)
        return r[self.joint_number]

    def goToG53(self):
        ACTION.CALL_MDI('G53 G0 {}0'.format(INFO.GET_NAME_FROM_JOINT.get(self.joint_number)))
    def goToG5x(self):
        ACTION.CALL_MDI('G0 {}0'.format(INFO.GET_NAME_FROM_JOINT.get(self.joint_number)))

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    #
    # _toggle_properties makes it so we can only select one option
    ########################################################################

    def _toggle_properties(self, picked):
        data = ('always_display_diameter','always_display_radius',
        'display_as_per_m7m8')

        for i in data:
            if not i == picked:
                self[i] = False

    # property getter/setters
    def set_force_diameter(self, data):
        self.force_diameter = data
        if data:
            self.set_to_diameter()
            self._toggle_properties('always_display_diameter')
    def get_force_diameter(self):
        return self.force_diameter
    def reset_force_diameter(self):
        self.force_diameter = False
    always_display_diameter = QtCore.pyqtProperty(bool, get_force_diameter, set_force_diameter, reset_force_diameter)

    def set_force_radius(self, data):
        self.force_radius = data
        if data:
            self.set_to_radius()
            self._toggle_properties('always_display_radius')
    def get_force_radius(self):
        return self.force_radius
    def reset_force_radius(self):
        self.force_radius = False
    always_display_radius = QtCore.pyqtProperty(bool, get_force_radius, set_force_radius, reset_force_radius)

    def set_follow_m7m8_mode(self, data):
        self.follow_m7m8_mode = data
        if data:
            self._toggle_properties('display_as_per_m7m8')
    def get_follow_m7m8_mode(self):
        return self.follow_m7m8_mode
    def reset_follow_m7m8_mode(self):
        self.follow_m7m8_mode = True
    display_as_per_m7m8 = QtCore.pyqtProperty(bool, get_follow_m7m8_mode, set_follow_m7m8_mode, reset_follow_m7m8_mode)

    def set_follow_reference(self, data):
        self.allow_reference_change_requests = data
    def get_follow_reference(self):
        return self.allow_reference_change_requests
    def reset_follow_reference(self):
        self.allow_reference_change_requests = True
    follow_reference_changes = QtCore.pyqtProperty(bool, get_follow_reference, set_follow_reference, reset_follow_reference)

    # JOINT Number TODO this is actually joint index of the 9 axes
    # rather then linuxcnc's idea of joint number
    def setjoint_number(self, data):
        if data >10: data = 10
        if data < 0: data = 0
        self.joint_number = data
        if data < 10:
            self.set_joint_type(data)
    def getjoint_number(self):
        return self.joint_number
    def resetjoint_number(self):
        self.joint_number = 0
    Qjoint_number = QtCore.pyqtProperty(int, getjoint_number, setjoint_number, resetjoint_number)

    # user system Number
    def setreference_type(self, data):
        self.reference_type = data
    def getreference_type(self):
        return self.reference_type
    def resetreference_type(self):
        self.reference_type = 0
    Qreference_type = QtCore.pyqtProperty(int, getreference_type, setreference_type, resetreference_type)

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
    angular_template = QtCore.pyqtProperty(str, getangulartexttemplate, setangulartexttemplate, resetangulartexttemplate)

    def setisHomed(self, data):
        self._isHomed = data
    def getisHomed(self):
        return self._isHomed

    isHomed = QtCore.pyqtProperty(bool, getisHomed, setisHomed)

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

    def set_showSet(self, data):
        self._showSet = data
    def get_showSet(self):
        return self._showSet
    def reset_showSet(self):
        self._showSet = False
    showSetOrigin = QtCore.pyqtProperty(bool, get_showSet, set_showSet, reset_showSet)

    def set_showZero(self, data):
        self._showZero = data
    def get_showZero(self):
        return self._showZero
    def reset_showZero(self):
        self._showZero = False
    showZeroOrigin = QtCore.pyqtProperty(bool, get_showZero, set_showZero, reset_showZero)

    def set_showGoto(self, data):
        self._showGoto = data
    def get_showGoto(self):
        return self._showGoto
    def reset_showGoto(self):
        self._showGoto = False
    showGotoOrigin = QtCore.pyqtProperty(bool, get_showGoto, set_showGoto, reset_showGoto)

    def set_showLast(self, data):
        self._showLast = data
    def get_showLast(self):
        return self._showLast
    def reset_showLast(self):
        self._showLast = False
    showLast = QtCore.pyqtProperty(bool, get_showLast, set_showLast, reset_showLast)

    def set_showDivide(self, data):
        self._showDivide = data
    def get_showDivide(self):
        return self._showDivide
    def reset_showDivide(self):
        self._showDivide = False
    showDivide = QtCore.pyqtProperty(bool, get_showDivide, set_showDivide, reset_showDivide)

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
    widget = DROLabel()
    widget.show()
    sys.exit(app.exec_())
if __name__ == "__main__":
    main()
