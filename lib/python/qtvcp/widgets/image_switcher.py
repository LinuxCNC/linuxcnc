#!/usr/bin/env python3
# qtvcp
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
#################################################################################

import os

from PyQt5.QtCore import pyqtSignal, pyqtProperty, QVariant
from PyQt5.QtWidgets import QLabel
from PyQt5.QtGui import QPixmap

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Info
from qtvcp import logger

# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# LOG is for running code logging
STATUS = Status()
INFO = Info()
LOG = logger.getLogger(__name__)

# Force the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

if INFO.IMAGE_PATH is not None:
    DEFAULTIMAGE = (os.path.join(INFO.IMAGE_PATH,'applet-critical.png')) or ''
else:
    INFO.IMAGE_PATH = ''
    DEFAULTIMAGE = ''

class ImageSwitcher(QLabel, _HalWidgetBase):
    widgetChanged = pyqtSignal(int)

    def __init__(self, parent=None):
        super(ImageSwitcher, self).__init__(parent)
        self._defaultImage = DEFAULTIMAGE
        self._imagePath = [DEFAULTIMAGE]
        self._current_number = 0
        self.setScaledContents(True)

    def _hal_init(self):
        self.show_image_by_number(self._current_number)

    def _designerInit(self):
        self._hal_init()

    # Show the widgets based on a reference number
    def show_image_by_number(self, number):
        #print self.objectName(),len(self._imagePath),number
        try:
            if self._imagePath[number].upper() == 'NONE':
                return
            if number <0 or number > len(self._imagePath)-1:
                LOG.debug('Path reference number out of range: {}'.format(number))
                return
            # resources file images.
            if ':/' in self._imagePath[number]:
                path = self._imagePath[number]
                pixmap = QPixmap(path)
                self.setPixmap(pixmap)
                return
            else:
                path = os.path.expanduser(self._imagePath[number])
        except Exception as e:
            LOG.error('Path reference number: {}'.format(e))
            path = os.path.expanduser(self._defaultImage)
        #print 'requested:',number,self._imagePath[number]
        # if path doesn't exist try referencing
        # from the built in image folder
        if not os.path.exists(path):
            path = os.path.join(INFO.IMAGE_PATH, path)
            if not os.path.exists(path):
                LOG.debug('No Path: {}'.format(path))
        pixmap = QPixmap(path)
        self.setPixmap(pixmap)

    def set_default_image(self, path):
        self._defaultImage = path

    def set_image_number(self, data):
        if data <0: data = 0
        if data > len(self._imagePath)-1: data = len(self._imagePath)-1
        self._current_number = data
        self.show_image_by_number(data)
    def get_image_number(self):
        return self._current_number
    def reset_image_number(self):
        self._current_number = 0
    image_number = pyqtProperty(int, get_image_number, set_image_number, reset_image_number)

    def set_image_l(self, data):
        self._imagePath = data
    def get_image_l(self):
        return self._imagePath
    def reset_image_l(self):
        self._imagePath = [self._defaultImage]
    image_list = pyqtProperty(QVariant.typeToName(QVariant.StringList), get_image_l, set_image_l, reset_image_l)

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)



class StatusImageSwitcher(ImageSwitcher):

    def __init__(self, parent=None):
        super(StatusImageSwitcher, self).__init__(parent)
        self._imagePath = [os.path.join(INFO.IMAGE_PATH,'applet-critical.png'),
                    os.path.join(INFO.IMAGE_PATH,'spindle_ccw.gif'),
                    os.path.join(INFO.IMAGE_PATH,'spindle_cw.gif')]
        self.spindle = True
        self.all_homed = False
        self.axis_homed = False
        self.hard_limits = False
        self.machine_state = False
        self.command_state = False
        self.feedmode_state = False
        self.spindlemode_state = False

        self._last_limit = []
        self.axis = 'X'
        for i in range(0,len(INFO.AVAILABLE_JOINTS)):
            self._last_limit.append([0,0])

    def _hal_init(self):
        if self.spindle:
            STATUS.connect('spindle-control-changed', lambda w, a, b, c, d: self.switch_on_spindle(b,c,d))
        elif self.all_homed:
            STATUS.connect('not-all-homed', lambda w, data: self.switch_on_homed(0))
            STATUS.connect('all-homed', lambda w: self.switch_on_homed(1))
        elif self.axis_homed:
            STATUS.connect('unhomed', lambda w, data: self.switch_on_axis_unhomed(data))
            STATUS.connect('homed', lambda w, data: self.switch_on_axis_homed(data))
        elif self.hard_limits:
            STATUS.connect('hard-limits-tripped', lambda w, data, group: self.switch_on_hard_limits(data, group))
        elif self.machine_state:
            STATUS.connect('state-estop', lambda w: self.switch_on_machine_state(0))
            STATUS.connect('interp-run', lambda w: self.switch_on_machine_state(1))
            STATUS.connect('interp-idle', lambda w: self.switch_on_machine_state(2))
            STATUS.connect('interp-paused', lambda w: self.switch_on_machine_state(3))
            STATUS.connect('interp-waiting', lambda w: self.switch_on_machine_state(4))
            STATUS.connect('interp-reading', lambda w: self.switch_on_machine_state(5))
        elif self.command_state:
            STATUS.connect('command-running', lambda w: self.switch_on_command_state(0))
            STATUS.connect('command-stopped', lambda w: self.switch_on_command_state(1))
            STATUS.connect('command-error', lambda w: self.switch_on_command_state(2))
        elif self.feedmode_state:
            STATUS.connect('fpm-mode', lambda w, d: self.switch_on_feedmode_state(0, d))
            STATUS.connect('fpr-mode', lambda w, d: self.switch_on_feedmode_state(1,d))
            STATUS.connect('itime-mode', lambda w, d: self.switch_on_feedmode_state(2,d))
        elif self.spindlemode_state:
            STATUS.connect('rpm-mode', lambda w, d: self.switch_on_spindlemode_state(0, d))
            STATUS.connect('css-mode', lambda w, d: self.switch_on_spindlemode_state(1, d))

    def _designerInit(self):
        self.show_image_by_number(0)

    # M5 disabled = image 0
    # M3 at speed = image 1
    # M4 at speed = image 2
    # M3 accelerating = image 3
    # M4 accelerating = image 4
    def switch_on_spindle(self, on, speed, up):
        if not on:
            num = 0
        elif not up:
            if speed<0: num = 4
            else: num = 3
        elif speed <0: num = 2
        else: num = 1
        self.set_image_number(num)

    def switch_on_homed(self, data):
        if not data <0:
            self.set_image_number(data)

    def switch_on_axis_homed(self, num):
        data = INFO.GET_NAME_FROM_JOINT[int(num)]
        if data == self.axis:
            self.set_image_number(1)

    def switch_on_axis_unhomed(self, num):
        data = INFO.GET_NAME_FROM_JOINT[int(num)]
        if data == self.axis:
            self.set_image_number(0)

    def switch_on_hard_limits(self, data, group):
        if not data:
            # tripped
            self.set_image_number(0)
        elif (len(self._imagePath)) == 2:
            #print 'bool images'
            self.set_image_number(1)
        elif (len(self._imagePath)-1) == (len(INFO.AVAILABLE_JOINTS)):
            #print 'per joint limits images', self._last_limit, group
            for i in range(0,len(INFO.AVAILABLE_JOINTS)):
                if group[i] == self._last_limit[i]:
                    pass
                elif group[i] == [0,0]:
                    pass
                else:
                    self.set_image_number(i+1)
                    break
        elif (len(self._imagePath)-1) == (len(INFO.AVAILABLE_JOINTS) * 2):
            pass
            #print 'per joint and per end limits images'
        self._last_limit = group

    def switch_on_machine_state(self, state):
        self.set_image_number(state)

    def switch_on_command_state(self, state):
        self.set_image_number(state)

    def switch_on_feedmode_state(self, mode, state):
        if state:
            self.set_image_number(mode)

    def switch_on_spindlemode_state(self, mode, state):
        if state:
            self.set_image_number(mode)

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    #
    # _toggle_properties makes it so we can only select one option
    ########################################################################

    def _toggle_properties(self, picked):
        data = ('spindle','all_homed', 'axis_homed','hard_limits',
                'machine_state', 'command_state', 'feedmode_state',
                'spindlemode_state' )

        for i in data:
            if not i == picked:
                self['watch_'+i] = False

# property getter/setters

    # machine_spindle status
    def set_spindle(self, data):
        self.spindle = data
        if data:
            self._toggle_properties('spindle')
    def get_spindle(self):
        return self.spindle
    def reset_spindle(self):
        self.spindle = False
    watch_spindle = pyqtProperty(bool, get_spindle, set_spindle, reset_spindle)

    # machine_homed status
    def set_homed(self, data):
        self.all_homed = data
        if data:
            self._toggle_properties('all_homed')
    def get_homed(self):
        return self.all_homed
    def reset_homed(self):
        self.all_homed = False
    watch_all_homed = pyqtProperty(bool, get_homed, set_homed, reset_homed)

    # machine_axis_homed status
    def set_axis_homed(self, data):
        self.axis_homed = data
        if data:
            self._toggle_properties('axis_homed')
    def get_axis_homed(self):
        return self.axis_homed
    def reset_axis_homed(self):
        self.axis_homed = False
    watch_axis_homed = pyqtProperty(bool, get_axis_homed, set_axis_homed, reset_axis_homed)


    # machine_limits status
    def set_limits(self, data):
        self.hard_limits = data
        if data:
            self._toggle_properties('hard_limits')
    def get_limits(self):
        return self.hard_limits
    def reset_limits(self):
        self.hard_limits = False
    watch_hard_limits = pyqtProperty(bool, get_limits, set_limits, reset_limits)

    # machine_state status
    def set_machine_state(self, data):
        self.machine_state = data
        if data:
            self._toggle_properties('machine_state')
    def get_machine_state(self):
        return self.machine_state
    def reset_machine_state(self):
        self.machine_state = False
    watch_machine_state = pyqtProperty(bool, get_machine_state, set_machine_state,
                                                      reset_machine_state)

    # command_state status
    def set_command_state(self, data):
        self.command_state = data
        if data:
            self._toggle_properties('command_state')
    def get_command_state(self):
        return self.command_state
    def reset_command_state(self):
        self.command_state = False
    watch_command_state = pyqtProperty(bool, get_command_state, set_command_state,
                                                      reset_command_state)

    # feedmode_state status
    def set_feedmode_state(self, data):
        self.feedmode_state = data
        if data:
            self._toggle_properties('feedmode_state')
    def get_feedmode_state(self):
        return self.feedmode_state
    def reset_feedmode_state(self):
        self.feedmode_state = False
    watch_feedmode_state = pyqtProperty(bool, get_feedmode_state, set_feedmode_state,
                                                      reset_feedmode_state)
    # spindlemode_state status
    def set_spindlemode_state(self, data):
        self.spindlemode_state = data
        if data:
            self._toggle_properties('spindlemode_state')
    def get_spindlemode_state(self):
        return self.spindlemode_state
    def reset_spindlemode_state(self):
        self.spindlemode_state = False
    watch_spindlemode_state = pyqtProperty(bool, get_spindlemode_state, set_spindlemode_state,
                                                      reset_spindlemode_state)

    def set_axis(self, data):
        if data.upper() in('X','Y','Z','A','B','C','U','V','W'):
            self.axis = data.upper()
    def get_axis(self):
        return self.axis
    def reset_axis(self):
        self.axis = 'X'
    axis_letter = pyqtProperty(str, get_axis, set_axis, reset_axis)

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
    widget = ImageSwitcher()
    widget._hal_init()

    widget.show()
    sys.exit(app.exec_())
if __name__ == "__main__":
    main()

