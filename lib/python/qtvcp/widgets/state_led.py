#!/usr/bin/python2.7
# Qtvcp widget
#
# Copyright (c) 2017  Chris Morley <chrisinnanaimo@hotmail.com>
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

from PyQt5.QtCore import pyqtProperty
from qtvcp.widgets.led_widget import LED
from qtvcp.core import Status
from qtvcp import logger

# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# LOG is for running code logging
STATUS = Status()
LOG = logger.getLogger(__name__)

# Set the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


class StateLED(LED):

    def __init__(self, parent=None):
        super(StateLED, self).__init__(parent)
        self.has_hal_pins = False
        self.setState(False)

        self.is_estopped = False
        self.is_on = False
        self.is_homed = False
        self.is_idle = False
        self.is_paused = False
        self.invert_state = False
        self.is_flood = False
        self.is_mist = False
        self.is_block_delete = False
        self.is_optional_stop = False
        self.is_joint_homed = False
        self.is_limits_overridden = False

        self.joint_number = 0

    def _hal_init(self):
        def only_false(data):
            if data:
                return
            self._flip_state(False)

        if self.is_estopped:
            STATUS.connect('state-estop', lambda w: self._flip_state(True))
            STATUS.connect('state-estop-reset', lambda w: self._flip_state(False))
        elif self.is_on:
            STATUS.connect('state-on', lambda w: self._flip_state(True))
            STATUS.connect('state-off', lambda w: self._flip_state(False))
        elif self.is_homed:
            STATUS.connect('all-homed', lambda w: self._flip_state(True))
            STATUS.connect('not-all-homed', lambda w, axis: self._flip_state(False))
        elif self.is_idle:
            STATUS.connect('interp-idle', lambda w: self._flip_state(False))
            STATUS.connect('interp-run', lambda w: self._flip_state(False))
        elif self.is_paused:
            STATUS.connect('program-pause-changed', lambda w, data: self._flip_state(data))
        elif self.is_flood:
            STATUS.connect('flood-changed', lambda w, data: self._flip_state(data))
        elif self.is_mist:
            STATUS.connect('mist-changed', lambda w, data: self._flip_state(data))
        elif self.is_block_delete:
            STATUS.connect('block-delete-changed', lambda w, data: self._flip_state(data))
        elif self.is_optional_stop:
            STATUS.connect('optional-stop-changed', lambda w, data: self._flip_state(data))
        elif self.is_joint_homed:
            STATUS.connect('homed', lambda w, data: self.joint_homed(data))
            STATUS.connect('not-all-homed', lambda w, data: self.joints_unhomed(data))
        elif self.is_limits_overridden:
            STATUS.connect('override-limits-changed', self.check_override_limits)
            STATUS.connect('hard-limits-tripped', lambda w, data: only_false(data))

    def _flip_state(self, data):
            if self.invert_state:
                data = not data
            self.change_state(data)

    def joint_homed(self, joint):
        if int(joint) == self.joint_number:
            self._flip_state(True)

    def joints_unhomed(self, jlist):
        if str(self.joint_number) in jlist:
            self._flip_state(False)

    def check_override_limits(self, w, data):
        for i in data:
            if i == 1:
                self._flip_state(True)
                return
        self._flip_state(False)

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    #
    # _toggle_properties makes it so we can only select one option
    ########################################################################

    def _toggle_properties(self, picked):
        data = ('is_paused', 'is_estopped', 'is_on', 'is_idle', 'is_homed',
                'is_flood', 'is_mist', 'is_block_delete', 'is_optional_stop',
                'is_joint_homed', 'is_limits_overridden')

        for i in data:
            if not i == picked:
                self[i+'_status'] = False

# property getter/setters

    # invert status
    def set_invert_state(self, data):
        self.invert_state = data
    def get_invert_state(self):
        return self.invert_state
    def reset_invert_state(self):
        self.invert_state = False

    # machine is paused status
    def set_is_paused(self, data):
        self.is_paused = data
        if data:
            self._toggle_properties('is_paused')
    def get_is_paused(self):
        return self.is_paused
    def reset_is_paused(self):
        self.is_paused = False

    # machine is estopped status
    def set_is_estopped(self, data):
        self.is_estopped = data
        if data:
            self._toggle_properties('is_estopped')
    def get_is_estopped(self):
        return self.is_estopped
    def reset_is_estopped(self):
        self.is_estopped = False

    # machine is on status
    def set_is_on(self, data):
        self.is_on = data
        if data:
            self._toggle_properties('is_on')
    def get_is_on(self):
        return self.is_on
    def reset_is_on(self):
        self.is_on = False

    # machine is idle status
    def set_is_idle(self, data):
        self.is_idle = data
        if data:
            self._toggle_properties('is_idle')
    def get_is_idle(self):
        return self.is_idle
    def reset_is_idle(self):
        self.is_idle = False

    # machine_is_homed status
    def set_is_homed(self, data):
        self.is_homed = data
        if data:
            self._toggle_properties('is_homed')
    def get_is_homed(self):
        return self.is_homed
    def reset_is_homed(self):
        self.is_homed = False

    # machine is_flood status
    def set_is_flood(self, data):
        self.is_flood = data
        if data:
            self._toggle_properties('is_flood')
    def get_is_flood(self):
        return self.is_flood
    def reset_is_flood(self):
        self.is_flood = False

    # machine is_mist status
    def set_is_mist(self, data):
        self.is_mist = data
        if data:
            self._toggle_properties('is_mist')
    def get_is_mist(self):
        return self.is_mist
    def reset_is_mist(self):
        self.is_mist = False

    # machine_is_block_delete status
    def set_is_block_delete(self, data):
        self.is_block_delete = data
        if data:
            self._toggle_properties('is_block_delete')
    def get_is_block_delete(self):
        return self.is_block_delete
    def reset_is_block_delete(self):
        self.is_block_delete = False

    # machine_is_optional_stop status
    def set_is_optional_stop(self, data):
        self.is_optional_stop = data
        if data:
            self._toggle_properties('is_optional_stop')
    def get_is_optional_stop(self):
        return self.is_optional_stop
    def reset_is_optional_stop(self):
        self.is_optional_stop = False

    # machine_is_joint_homed status
    def set_is_joint_homed(self, data):
        self.is_joint_homed = data
        if data:
            self._toggle_properties('is_joint_homed')
    def get_is_joint_homed(self):
        return self.is_joint_homed
    def reset_is_joint_homed(self):
        self.is_joint_homed = False

    # machine_is_limits_overridden status
    def set_is_limits_overridden(self, data):
        self.is_limits_overridden = data
        if data:
            self._toggle_properties('is_limits_overridden')
    def get_is_limits_overridden(self):
        return self.is_limits_overridden
    def reset_is_limits_overridden(self):
        self.is_limits_overridden = False

    # Non bool

    # machine_joint_number status
    def set_joint_number(self, data):
        self.joint_number = data
    def get_joint_number(self):
        return self.joint_number
    def reset_joint_number(self):
        self.joint_number = 0

    # designer will show these properties in this order:
    # BOOL
    invert_state_status = pyqtProperty(bool, get_invert_state, set_invert_state, reset_invert_state)
    is_paused_status = pyqtProperty(bool, get_is_paused, set_is_paused, reset_is_paused)
    is_estopped_status = pyqtProperty(bool, get_is_estopped, set_is_estopped, reset_is_estopped)
    is_on_status = pyqtProperty(bool, get_is_on, set_is_on, reset_is_on)
    is_idle_status = pyqtProperty(bool, get_is_idle, set_is_idle, reset_is_idle)
    is_homed_status = pyqtProperty(bool, get_is_homed, set_is_homed, reset_is_homed)
    is_flood_status = pyqtProperty(bool, get_is_flood, set_is_flood, reset_is_flood)
    is_mist_status = pyqtProperty(bool, get_is_mist, set_is_mist, reset_is_mist)
    is_block_delete_status = pyqtProperty(bool, get_is_block_delete, set_is_block_delete, reset_is_block_delete)
    is_optional_stop_status = pyqtProperty(bool, get_is_optional_stop, set_is_optional_stop, reset_is_optional_stop)
    is_joint_homed_status = pyqtProperty(bool, get_is_joint_homed, set_is_joint_homed, reset_is_joint_homed)
    is_limits_overridden_status = pyqtProperty(bool, get_is_limits_overridden, set_is_limits_overridden,
                                               reset_is_limits_overridden)

    # NON BOOL
    joint_number_status = pyqtProperty(int, get_joint_number, set_joint_number, reset_joint_number)

    # boilder code
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

if __name__ == "__main__":

    import sys
    from PyQt4.QtGui import QApplication
    app = QApplication(sys.argv)
    led = StateLED()
    led.show()
    #led.startFlashing()
    sys.exit(app.exec_())
