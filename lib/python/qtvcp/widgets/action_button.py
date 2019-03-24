#!/usr/bin/env python
# Qtvcp action widget
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

# Action buttons are used to change linuxcnc behaivor do to button pushing.
# By making the button 'checkable' in the designer editor,
# the buton will toggle.
# In the designer editor, it is possible to select what the button will do.
###############################################################################

from PyQt5 import QtCore, QtWidgets
import linuxcnc

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.widgets.simple_widgets import Indicated_PushButton
from qtvcp.core import Status, Action, Info
from qtvcp.lib.aux_program_loader import Aux_program_loader
from qtvcp import logger

# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# ACTION gives commands to linuxcnc
# INFO is INI file details
# LOG is for running code logging
STATUS = Status()
ACTION = Action()
INFO = Info()
AUX_PRGM = Aux_program_loader()
LOG = logger.getLogger(__name__)

# Set the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

class ActionButton(Indicated_PushButton, _HalWidgetBase):
    def __init__(self, parent=None):
        super(ActionButton, self).__init__(parent)
        self._block_signal = False
        self.estop = False
        self.machine_on = False
        self.home = False
        self.run = False
        self.abort = False
        self.pause = False
        self.load_dialog = False
        self.macro_dialog = False
        self.origin_offset_dialog = False
        self.camview_dialog = False
        self.jog_joint_pos = False
        self.jog_joint_neg = False
        self.jog_selected_pos = False
        self.jog_selected_neg = False
        self.zero_axis = False
        self.launch_halmeter = False
        self.launch_status = False
        self.launch_halshow = False
        self.launch_halscope = False
        self.mdi = False
        self.auto = False
        self.manual = False
        self.jog_incr = False
        self.jog_rate = False
        self.feed_over = False
        self.rapid_over = False
        self.max_velocity_over = False
        self.spindle_over = False
        self.view_change = False
        self.spindle_fwd = False
        self.spindle_rev = False
        self.spindle_stop = False
        self.spindle_up = False
        self.spindle_down = False
        self.limits_override = False
        self.flood = False
        self.mist = False
        self.block_delete = False
        self.optional_stop = False
        self.mdi_command = False
        self.ini_mdi_command = False
        self.dro_relative = False
        self.dro_absolute = False
        self.dro_dtg = False
        self.exit = False
        self.template_label = False

        self.toggle_float = False
        self._toggle_state = 0
        self.joint_number = 0
        self.jog_incr_imperial = .010
        self.jog_incr_mm = .025
        self.jog_incr_angle = -1
        self.float = 100.0
        self.float_alt = 50.0
        self.view_type = 'p'
        self.command_text = ''
        self.ini_mdi_num = 0
        self._textTemplate = '%1.3f in'
        self._alt_textTemplate = '%1.2f mm'

    ##################################################
    # This gets called by qtvcp_makepins
    # It infers HAL involvement but there is none
    # STATUS is used to synch the buttons in case
    # other entities change linuxcnc's state
    # also some buttons are disabled/enabled based on
    # linuxcnc state / possible actions
    #
    # If the indicator isn't controlled by a HAL pin
    # then update it's state.
    # It still might not show do to the draw_indicator option off
    #
    # _safecheck blocks the outgoing signal so
    # the buttons can be synced with linuxcnc
    # without setting an infinite loop
    ###################################################
    def _hal_init(self):
        super(ActionButton, self)._hal_init()
        def _safecheck(state, data=None):
            self._block_signal = True
            self.setChecked(state)
            if self._HAL_pin is False:
                self.indicator_update(state)
            # if using state labels option update the labels
            if self._state_text:
                self.setText(None)
            self._block_signal = False

        def _checkincrements(value, text):
            value = round(value , 5)
            if STATUS.is_metric_mode():
                machn_units = INFO.convert_metric_to_machine(self.jog_incr_mm)
                if round(machn_units , 5) == value:
                    _safecheck(True)
                    return
            else:
                machn_units = INFO.convert_imperial_to_machine(self.jog_incr_imperial)
                if  round(machn_units , 5) == value:
                    _safecheck(True)
                    return
            _safecheck(False)

        def homed_on_loaded_test():
            return (STATUS.machine_is_on()
                    and (STATUS.is_all_homed() or INFO.NO_HOME_REQUIRED)
                    and STATUS.is_file_loaded())
        def homed_on_test():
            return (STATUS.machine_is_on()
                    and (STATUS.is_all_homed() or INFO.NO_HOME_REQUIRED))
        if self.estop:
            # Estop starts with button down - in estop which
            # backwards logic for the button...
            if self.isCheckable(): _safecheck(True)
            STATUS.connect('state-estop', lambda w: _safecheck(True))
            STATUS.connect('state-estop-reset', lambda w: _safecheck(False))

        elif self.machine_on:
            #self.setEnabled(False)
            STATUS.connect('state-estop', lambda w: self.setEnabled(False))
            STATUS.connect('state-estop-reset', lambda w: self.setEnabled(True))
            STATUS.connect('state-on', lambda w: _safecheck(True))
            STATUS.connect('state-off', lambda w: _safecheck(False))

        elif self.home:
            #self.setEnabled(False)
            STATUS.connect('state-off', lambda w: self.setEnabled(False))
            STATUS.connect('state-estop', lambda w: self.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: self.setEnabled(STATUS.machine_is_on()))
            STATUS.connect('interp-run', lambda w: self.setEnabled(False))
            STATUS.connect('all-homed', lambda w: _safecheck(True))
            STATUS.connect('not-all-homed', lambda w, axis: _safecheck(False, axis))

        elif self.load_dialog:
            STATUS.connect('state-off', lambda w: self.setEnabled(False))
            STATUS.connect('state-estop', lambda w: self.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: self.setEnabled(STATUS.machine_is_on()))
            STATUS.connect('interp-run', lambda w: self.setEnabled(False))
            STATUS.connect('all-homed', lambda w: _safecheck(True))

        elif self.camview_dialog or self.macro_dialog or self.origin_offset_dialog:
            pass
        elif self.jog_joint_pos or self.jog_joint_neg or \
                    self.jog_selected_pos or self.jog_selected_neg:
            STATUS.connect('state-off', lambda w: self.setEnabled(False))
            STATUS.connect('state-estop', lambda w: self.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: self.setEnabled(STATUS.machine_is_on()))
            STATUS.connect('interp-run', lambda w: self.setEnabled(False))
            if self.jog_joint_pos:
                self.pressed.connect(lambda: self.jog_action(1))
                self.released.connect(lambda: self.jog_action(0))
            elif self.jog_joint_neg:
                self.pressed.connect(lambda: self.jog_action(-1))
                self.released.connect(lambda: self.jog_action(0))
            elif self.jog_selected_pos:
                self.pressed.connect(lambda: self.jog_selected_action(1))
                self.released.connect(lambda: self.jog_selected_action(0))
            elif self.jog_selected_neg:
                self.pressed.connect(lambda: self.jog_selected_action(-1))
                self.released.connect(lambda: self.jog_selected_action(0))

            # jog button use diferent action signals so
            # leave early to aviod the standard 'clicked' signal
            return

        elif self.zero_axis or self.run:
            STATUS.connect('state-off', lambda w: self.setEnabled(False))
            STATUS.connect('state-estop', lambda w: self.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: self.setEnabled(homed_on_test()))
            STATUS.connect('interp-run', lambda w: self.setEnabled(False))
            STATUS.connect('all-homed', lambda w: self.setEnabled(True))
            STATUS.connect('not-all-homed', lambda w, data: self.setEnabled(False))
            STATUS.connect('interp-paused', lambda w: self.setEnabled(True))
            if self.run:
                STATUS.connect('file-loaded', lambda w, f: self.setEnabled(True))

        elif self.abort or self.pause:
            self.setEnabled(False)
            if self.pause:
                STATUS.connect('program-pause-changed', lambda w, state: _safecheck(state))
            STATUS.connect('state-off', lambda w: self.setEnabled(False))
            STATUS.connect('state-estop', lambda w: self.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: self.setEnabled(homed_on_loaded_test()))
            STATUS.connect('all-homed', lambda w: self.setEnabled(True))

        elif self.launch_halmeter:
            pass
        elif self.launch_status:
            pass
        elif self.launch_halshow:
            pass
        elif self.launch_halscope:
            pass
        elif self.auto:
            STATUS.connect('mode-auto', lambda w: _safecheck(True))
            STATUS.connect('mode-mdi', lambda w: _safecheck(False))
            STATUS.connect('mode-manual', lambda w: _safecheck(False))
        elif self.mdi:
            STATUS.connect('mode-mdi', lambda w: _safecheck(True))
            STATUS.connect('mode-manual', lambda w: _safecheck(False))
            STATUS.connect('mode-auto', lambda w: _safecheck(False))
        elif self.manual:
            STATUS.connect('mode-manual', lambda w: _safecheck(True))
            STATUS.connect('mode-mdi', lambda w: _safecheck(False))
            STATUS.connect('mode-auto', lambda w: _safecheck(False))
        elif self.jog_incr:
            STATUS.connect('metric-mode-changed', lambda w, data:  self.incr_action())
            STATUS.connect('jogincrement-changed', lambda w, value, text: _checkincrements(value, text))

        elif self.feed_over or self.rapid_over or self.spindle_over or self.jog_rate or \
            self.max_velocity_over:
            STATUS.connect('state-estop', lambda w: self.setEnabled(False))
            STATUS.connect('state-estop-reset', lambda w: self.setEnabled(STATUS.machine_is_on()))
            STATUS.connect('state-on', lambda w: _safecheck(True))
            STATUS.connect('state-off', lambda w: _safecheck(False))
        elif self.view_change:
            pass
        elif self.spindle_fwd or self.spindle_rev or self.spindle_up or self.spindle_down:
            STATUS.connect('mode-manual', lambda w: _safecheck(True))
            STATUS.connect('mode-mdi', lambda w: _safecheck(False))
            STATUS.connect('mode-auto', lambda w: _safecheck(False))
            STATUS.connect('state-off', lambda w: self.setEnabled(False))
            STATUS.connect('state-estop', lambda w: self.setEnabled(False))
            STATUS.connect('state-on', lambda w: self.setEnabled(True))
        elif self.spindle_stop:
            STATUS.connect('mode-auto', lambda w: self.setEnabled(False))
            STATUS.connect('state-off', lambda w: self.setEnabled(False))
            STATUS.connect('state-estop', lambda w: self.setEnabled(False))
            STATUS.connect('spindle-control-changed', lambda w, e, d: self.setEnabled(e and not STATUS.is_auto_mode()))
            STATUS.connect('spindle-control-changed', lambda w, e, d: _safecheck(not e))

        elif self.limits_override:
            STATUS.connect('hard-limits-tripped', lambda w, data: self.setEnabled(data))
        elif self.flood:
            STATUS.connect('state-estop', lambda w: self.setEnabled(False))
            STATUS.connect('state-on', lambda w: self.setEnabled(True))
            STATUS.connect('state-off', lambda w: self.setEnabled(False))
            STATUS.connect('flood-changed', lambda w, data: _safecheck(data))
        elif self.mist:
            STATUS.connect('state-estop', lambda w: self.setEnabled(False))
            STATUS.connect('state-on', lambda w: self.setEnabled(True))
            STATUS.connect('state-off', lambda w: self.setEnabled(False))
            STATUS.connect('mist-changed', lambda w, data: _safecheck(data))
        elif self.block_delete:
            STATUS.connect('state-estop', lambda w: self.setEnabled(False))
            STATUS.connect('state-off', lambda w: self.setEnabled(False))
            STATUS.connect('mode-mdi', lambda w: self.setEnabled(True))
            STATUS.connect('mode-manual', lambda w: self.setEnabled(True))
            STATUS.connect('mode-auto', lambda w: self.setEnabled(False))
            STATUS.connect('block-delete-changed', lambda w, data: _safecheck(data))
        elif self.optional_stop:
            STATUS.connect('state-estop', lambda w: self.setEnabled(False))
            STATUS.connect('state-off', lambda w: self.setEnabled(False))
            STATUS.connect('mode-mdi', lambda w: self.setEnabled(True))
            STATUS.connect('mode-manual', lambda w: self.setEnabled(True))
            STATUS.connect('mode-auto', lambda w: self.setEnabled(False))
            STATUS.connect('optional-stop-changed', lambda w, data: _safecheck(data))
        elif self.mdi_command or self.ini_mdi_command:
            STATUS.connect('state-off', lambda w: self.setEnabled(False))
            STATUS.connect('state-estop', lambda w: self.setEnabled(False))
            STATUS.connect('interp-idle', lambda w: self.setEnabled(STATUS.machine_is_on()))
            STATUS.connect('all-homed', lambda w: self.setEnabled(True))
        elif self.dro_absolute or self.dro_relative or self.dro_dtg:
            pass
        elif self.exit:
            pass

        # connect a signal and callback function to the button
        if self.isCheckable():
            self.clicked[bool].connect(self.action)
        else:
            self.pressed.connect(self.action)

    ###################################
    # Here we do the actions
    ###################################
    def action(self, state=None):
        # don't do anything if the signal is blocked
        if self._block_signal: return
        if self.estop:
            if self.isCheckable():
                ACTION.SET_ESTOP_STATE(state)
            else:
                ACTION.SET_ESTOP_STATE(STATUS.estop_is_clear())
        elif self.machine_on:
            if self.isCheckable():
                ACTION.SET_MACHINE_STATE(state)
            else:
                LOG.debug('gstat machine is on: {}'.format(STATUS.machine_is_on))
                ACTION.SET_MACHINE_STATE(not STATUS.machine_is_on())
        elif self.home:
            if self.isCheckable():
                if state:
                    ACTION.SET_MACHINE_HOMING(self.joint_number)
                else:
                    ACTION.SET_MACHINE_UNHOMED(self.joint_number)
            else:
                if STATUS.is_all_homed():
                    ACTION.SET_MACHINE_UNHOMED(self.joint_number)
                else:
                    ACTION.SET_MACHINE_HOMING(self.joint_number)
        elif self.run:
            ACTION.RUN()
        elif self.abort:
            ACTION.ABORT()
        elif self.pause:
            ACTION.PAUSE()
        elif self.load_dialog:
            STATUS.emit('dialog-request',{'NAME':'LOAD'})
        elif self.camview_dialog:
            STATUS.emit('dialog-request', {'NAME':'CAMVIEW'})
        elif self.macro_dialog:
            STATUS.emit('dialog-request', {'NAME':'MACRO'})
        elif self.origin_offset_dialog:
            STATUS.emit('dialog-request', {'NAME':'ORIGINOFFSET'})
        elif self.zero_axis:
            j = "XYZABCUVW"
            try:
                axis = j[self.joint_number]
            except IndexError:
                LOG.error("can't zero origin for specified joint {}".format(self.joint_number))
            ACTION.SET_AXIS_ORIGIN(axis, 0)
        elif self.launch_halmeter:
            AUX_PRGM.load_halmeter()
        elif self.launch_status:
            AUX_PRGM.load_status()
        elif self.launch_halshow:
            AUX_PRGM.load_halshow()
        elif self.launch_halscope:
            AUX_PRGM.load_halscope()
        elif self.auto:
            ACTION.SET_AUTO_MODE()
        elif self.mdi:
            ACTION.SET_MDI_MODE()
        elif self.manual:
            ACTION.SET_MANUAL_MODE()
        elif self.jog_incr:
            self.incr_action()
        elif self.jog_rate:
            if self.toggle_float and not self._toggle_state:
                ACTION.SET_JOG_RATE(self.float_alt)
            else:
                ACTION.SET_JOG_RATE(self.float)
            self._toggle_state -= 1
            self._toggle_state = self._toggle_state * -1
        elif self.feed_over:
            if self.toggle_float and not self._toggle_state:
                ACTION.SET_FEED_RATE(self.float_alt)
            else:
                ACTION.SET_FEED_RATE(self.float)
            self._toggle_state -= 1
            self._toggle_state = self._toggle_state * -1
        elif self.rapid_over:
            if self.toggle_float and not self._toggle_state:
                ACTION.SET_RAPID_RATE(self.float_alt)
            else:
                ACTION.SET_RAPID_RATE(self.float)
            self._toggle_state -= 1
            self._toggle_state = self._toggle_state * -1
        elif self.max_velocity_over:
            if self.toggle_float and not self._toggle_state:
                ACTION.SET_MAX_VELOCITY_RATE(self.float_alt)
            else:
                ACTION.SET_MAX_VELOCITY_RATE(self.float)
            self._toggle_state -= 1
            self._toggle_state = self._toggle_state * -1
        elif self.spindle_over:
            if self.toggle_float and not self._toggle_state:
                ACTION.SET_SPINDLE_RATE(self.float_alt)
            else:
                ACTION.SET_SPINDLE_RATE(self.float)
            self._toggle_state -= 1
            self._toggle_state = self._toggle_state * -1
        elif self.view_change:
            try:
                STATUS.emit('view-changed', '%s' % self.view_type)
            except:
                pass
        elif self.spindle_fwd:
            ACTION.SET_SPINDLE_ROTATION(linuxcnc.SPINDLE_FORWARD, INFO.DEFAULT_SPINDLE_SPEED)
        elif self.spindle_rev:
            ACTION.SET_SPINDLE_ROTATION(linuxcnc.SPINDLE_REVERSE, INFO.DEFAULT_SPINDLE_SPEED)
        elif self.spindle_stop:
            ACTION.SET_SPINDLE_STOP()
        elif self.spindle_up:
            if STATUS.is_spindle_on():
                ACTION.SET_SPINDLE_FASTER()
            else:
                ACTION.SET_SPINDLE_ROTATION(linuxcnc.SPINDLE_FORWARD, INFO.DEFAULT_SPINDLE_SPEED)
        elif self.spindle_down:
            if STATUS.is_spindle_on():
                ACTION.SET_SPINDLE_SLOWER()
            else:
                ACTION.SET_SPINDLE_ROTATION(linuxcnc.SPINDLE_REVERSE, INFO.DEFAULT_SPINDLE_SPEED)
        elif self.limits_override:
            ACTION.SET_LIMITS_OVERRIDE()
        elif self.flood:
            if self.isCheckable() is False:
                ACTION.TOGGLE_FLOOD()
            else:
                if state:
                    ACTION.SET_FLOOD_ON()
                else:
                    ACTION.SET_FLOOD_OFF()
        elif self.mist:
            if self.isCheckable() is False:
                ACTION.TOGGLE_MIST()
            else:
                if state:
                    ACTION.SET_MIST_ON()
                else:
                    ACTION.SET_MIST_OFF()
        elif self.optional_stop:
            if self.isCheckable() is False:
                ACTION.TOGGLE_OPTIONAL_STOP()
            else:
                if state:
                    ACTION.SET_OPTIONAL_STOP_ON()
                else:
                    ACTION.SET_OPTIONAL_STOP_OFF()
        elif self.block_delete:
            if self.isCheckable() is False:
                ACTION.TOGGLE_BLOCK_DELETE()
            else:
                if state:
                    ACTION.SET_BLOCK_DELETE_ON()
                else:
                    ACTION.SET_BLOCK_DELETE_OFF()
        elif self.mdi_command:
            self.command_text = str(self.command_text)
            LOG.debug("MDI STRING COMMAND: {}".format(self.command_text))
            ACTION.CALL_MDI(self.command_text)
        elif self.ini_mdi_command:
            LOG.debug("INI MDI COMMAND #: {}".format(self.ini_mdi_num))
            ACTION.CALL_INI_MDI(self.ini_mdi_num)
        elif self.dro_absolute:
            STATUS.emit('dro-reference-change-request', 0)
        elif self.dro_relative:
            STATUS.emit('dro-reference-change-request', 1)
        elif self.dro_dtg:
            STATUS.emit('dro-reference-change-request', 2)
        elif self.exit:
            self.QTVCP_INSTANCE_.close()
        # defult error case
        elif not self._python_command:
            LOG.error('No action recognised')

        # This is check after because action buttons can do an action plus
        # a python command, or just either one.
        if self._python_command:
            if state == None:
                state = not self._indicator_state
            self.python_command(state)


    # If direction = 0 (button release) and distance is not 0, then we are
    # doing a jog increment so don't stop jog on release.
    def jog_selected_action(self, direction):
        number = STATUS.get_selected_axis()
        if direction == 0:
            if number in (3,4,5): # angualr axis
                if STATUS.get_jog_increment_angular() != 0: return
            elif STATUS.get_jog_increment() != 0: return
        if direction:
            ACTION.ensure_mode(linuxcnc.MODE_MANUAL)
        ACTION.DO_JOG(number, direction)

    # If direction = 0 (button release) and distance is not 0, then we are
    # doing a jog increment so don't stop jog on release.
    def jog_action(self, direction):
        if direction == 0:
            if self.joint_number in (3,4,5): # anglar axis
                if STATUS.get_jog_increment_angular() != 0: return
            elif STATUS.get_jog_increment() != 0: return
        if direction:
            ACTION.ensure_mode(linuxcnc.MODE_MANUAL)
        ACTION.DO_JOG(self.joint_number, direction)

    # We must convert the increments from current 'mode' units to
    # whatever units the machine is based on.
    # If the setting is negative don't set it
    def incr_action(self):
        if STATUS.is_metric_mode():  # metirc mode G21
            if self.jog_incr_mm < 0: return
            elif self.jog_incr_mm:
                incr = INFO.convert_metric_to_machine(self.jog_incr_mm)
                text = '%s mm' % str(self.jog_incr_mm)
            else:
                incr = 0
                text = 'Continous'
            if self.template_label:
                self._set_alt_text(self.jog_incr_mm)
        else:
            if self.jog_incr_imperial < 0: return
            elif self.jog_incr_imperial:
                incr = INFO.convert_imperial_to_machine(self.jog_incr_imperial)
                text = '''%s "''' % str(self.jog_incr_imperial)
            else:
                incr = 0
                text = 'Continous'
            if self.template_label:
                self._set_text(self.jog_incr_imperial)
        ACTION.SET_JOG_INCR(incr , text)

        # set an angular increment if not negative
        if self.jog_incr_angle < 0: return
        elif self.jog_incr_angle == 0:
            incr = 0
            text = 'Continous'
        else:
            incr = self.jog_incr_angle
            text = '''%s deg''' % str(self.jog_incr_angle)
        ACTION.SET_JOG_INCR_ANGULAR(incr , text)

    def _set_text(self, data):
            tmpl = lambda s: str(self._textTemplate) % s
            self.setText(tmpl(data))
    def _set_alt_text(self, data):
            tmpl = lambda s: str(self._alt_textTemplate) % s
            self.setText(tmpl(data))

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    #
    # _toggle_properties makes it so we can only select one option
    ########################################################################

    def _toggle_properties(self, picked):
        data = ('estop', 'machine_on', 'home', 'run', 'abort', 'pause',
                'load_dialog', 'jog_joint_pos', 'jog_joint_neg',
                'jog_selected_pos', 'jog_selected_neg', 'zero_axis',
                'launch_halmeter', 'launch_status', 'launch_halshow',
                'auto', 'mdi', 'manual', 'macro_dialog', 'origin_offset_dialog',
                'camview_dialog', 'jog_incr', 'feed_over', 'rapid_over',
                'spindle_over', 'jog_rate', 'view_change', 'spindle_fwd',
                'spindle_rev', 'spindle_stop', 'spindle_up', 'spindle_down',
                'limits_override', 'flood', 'mist', 'optional_stop', 'mdi_command',
                'ini_mdi_command', 'command_text', 'block_delete', 'dro_absolute',
                'dro_relative', 'dro_dtg','max_velocity_over', 'launch_halscope',
                 'exit')

        for i in data:
            if not i == picked:
                self[i+'_action'] = False

    # BOOL VARIABLES----------------------
    def set_estop(self, data):
        self.estop = data
        if data:
            self._toggle_properties('estop')
    def get_estop(self):
        return self.estop
    def reset_estop(self):
        self.estop = False

    def set_machine_on(self, data):
        self.machine_on = data
        if data:
            self._toggle_properties('machine_on')
    def get_machine_on(self):
        return self.machine_on
    def reset_machine_on(self):
        self.machine_on = False

    def set_home(self, data):
        self.home = data
        if data:
            self._toggle_properties('home')
    def get_home(self):
        return self.home
    def reset_home(self):
        self.home = False

    def set_zero_axis(self, data):
        self.zero_axis = data
        if data:
            self._toggle_properties('zero_axis')
    def get_zero_axis(self):
        return self.zero_axis
    def reset_zero_axis(self):
        self.zero_axis = False

    def set_load_dialog(self, data):
        self.load_dialog = data
        if data:
            self._toggle_properties('load_dialog')
    def get_load_dialog(self):
        return self.load_dialog
    def reset_load_dialog(self):
        self.load_dialog = False

    def set_macro_dialog(self, data):
        self.macro_dialog = data
        if data:
            self._toggle_properties('macro_dialog')
    def get_macro_dialog(self):
        return self.macro_dialog
    def reset_macro_dialog(self):
        self.macro_dialog = False

    def set_origin_offset_dialog(self, data):
        self.origin_offset_dialog = data
        if data:
            self._toggle_properties('origin_offset_dialog')
    def get_origin_offset_dialog(self):
        return self.origin_offset_dialog
    def reset_origin_offset_dialog(self):
        self.origin_offset_dialog = False

    def set_camview_dialog(self, data):
        self.camview_dialog = data
        if data:
            self._toggle_properties('camview_dialog')
    def get_camview_dialog(self):
        return self.camview_dialog
    def reset_camview_dialog(self):
        self.camview_dialog = False

    def set_jog_joint_pos(self, data):
        self.jog_joint_pos = data
        if data:
            self._toggle_properties('jog_joint_pos')
    def get_jog_joint_pos(self):
        return self.jog_joint_pos
    def reset_jog_joint_pos(self):
        self.jog_joint_pos = False

    def set_jog_joint_neg(self, data):
        self.jog_joint_neg = data
        if data:
            self._toggle_properties('jog_joint_neg')
    def get_jog_joint_neg(self):
        return self.jog_joint_neg
    def reset_jog_joint_neg(self):
        self.jog_joint_neg = False

    def set_jog_selected_pos(self, data):
        self.jog_selected_pos = data
        if data:
            self._toggle_properties('jog_selected_pos')
    def get_jog_selected_pos(self):
        return self.jog_selected_pos
    def reset_jog_selected_pos(self):
        self.jog_selected_pos = False

    def set_jog_selected_neg(self, data):
        self.jog_selected_neg = data
        if data:
            self._toggle_properties('jog_selected_neg')
    def get_jog_selected_neg(self):
        return self.jog_selected_neg
    def reset_jog_selected_neg(self):
        self.jog_selected_neg = False

    def set_run(self, data):
        self.run = data
        if data:
            self._toggle_properties('run')
    def get_run(self):
        return self.run
    def reset_run(self):
        self.run = False

    def set_abort(self, data):
        self.abort = data
        if data:
            self._toggle_properties('abort')
    def get_abort(self):
        return self.abort
    def reset_abort(self):
        self.abort = False

    def set_pause(self, data):
        self.pause = data
        if data:
            self._toggle_properties('pause')
    def get_pause(self):
        return self.pause
    def reset_pause(self):
        self.pause = False

    def set_launch_halmeter(self, data):
        self.launch_halmeter = data
        if data:
            self._toggle_properties('launch_halmeter')
    def get_launch_halmeter(self):
        return self.launch_halmeter
    def reset_launch_halmeter(self):
        self.launch_halmeter = False

    def set_launch_status(self, data):
        self.launch_status = data
        if data:
            self._toggle_properties('launch_status')
    def get_launch_status(self):
        return self.launch_status
    def reset_launch_status(self):
        self.launch_status = False

    def set_launch_halshow(self, data):
        self.launch_halshow = data
        if data:
            self._toggle_properties('launch_halshow')
    def get_launch_halshow(self):
        return self.launch_halshow
    def reset_launch_halshow(self):
        self.launch_halshow = False

    def set_launch_halscope(self, data):
        self.launch_halscope = data
        if data:
            self._toggle_properties('launch_halscope')
    def get_launch_halscope(self):
        return self.launch_halscope
    def reset_launch_halscope(self):
        self.launch_halscope = False

    def set_auto(self, data):
        self.auto = data
        if data:
            self._toggle_properties('auto')
    def get_auto(self):
        return self.auto
    def reset_auto(self):
        self.auto = False

    def set_mdi(self, data):
        self.mdi = data
        if data:
            self._toggle_properties('mdi')
    def get_mdi(self):
        return self.mdi
    def reset_mdi(self):
        self.mdi = False

    def set_manual(self, data):
        self.manual = data
        if data:
            self._toggle_properties('manual')
    def get_manual(self):
        return self.manual
    def reset_manual(self):
        self.manual = False

    def set_joint(self, data):
        self.joint = data
    def get_joint(self):
        return self.joint
    def reset_joint(self):
        self.joint = -1

    def set_jog_incr(self, data):
        self.jog_incr = data
        if data:
            self._toggle_properties('jog_incr')
    def get_jog_incr(self):
        return self.jog_incr
    def reset_jog_incr(self):
        self.jog_incr = False

    def set_jog_rate(self, data):
        self.jog_rate = data
        if data:
            self._toggle_properties('jog_rate')
    def get_jog_rate(self):
        return self.jog_rate
    def reset_jog_rate(self):
        self.jog_rate = False

    def set_feed_over(self, data):
        self.feed_over = data
        if data:
            self._toggle_properties('feed_over')
    def get_feed_over(self):
        return self.feed_over
    def reset_feed_over(self):
        self.feed_over = False

    def set_rapid_over(self, data):
        self.rapid_over = data
        if data:
            self._toggle_properties('rapid_over')
    def get_rapid_over(self):
        return self.rapid_over
    def reset_rapid_over(self):
        self.rapid_over = False

    def set_max_velocity_over(self, data):
        self.max_velocity_over = data
        if data:
            self._toggle_properties('max_velocity_over')
    def get_max_velocity_over(self):
        return self.max_velocity_over
    def reset_max_velocity_over(self):
        self.max_velocity_over = False

    def set_spindle_over(self, data):
        self.spindle_over = data
        if data:
            self._toggle_properties('spindle_over')
    def get_spindle_over(self):
        return self.spindle_over
    def reset_spindle_over(self):
        self.spindle_over = False

    def set_spindle_fwd(self, data):
        self.spindle_fwd = data
        if data:
            self._toggle_properties('spindle_fwd')
    def get_spindle_fwd(self):
        return self.spindle_fwd
    def reset_spindle_fwd(self):
        self.spindle_fwd = False

    def set_spindle_rev(self, data):
        self.spindle_rev = data
        if data:
            self._toggle_properties('spindle_rev')
    def get_spindle_rev(self):
        return self.spindle_rev
    def reset_spindle_rev(self):
        self.spindle_rev = False

    def set_spindle_stop(self, data):
        self.spindle_stop = data
        if data:
            self._toggle_properties('spindle_stop')
    def get_spindle_stop(self):
        return self.spindle_stop
    def reset_spindle_stop(self):
        self.spindle_stop = False

    def set_spindle_up(self, data):
        self.spindle_up = data
        if data:
            self._toggle_properties('spindle_up')
    def get_spindle_up(self):
        return self.spindle_up
    def reset_spindle_up(self):
        self.spindle_up = False

    def set_spindle_down(self, data):
        self.spindle_down = data
        if data:
            self._toggle_properties('spindle_down')
    def get_spindle_down(self):
        return self.spindle_down
    def reset_spindle_down(self):
        self.spindle_down = False

    def set_toggle_float(self, data):
        self.toggle_float = data
    def get_toggle_float(self):
        return self.toggle_float
    def reset_toggle_float(self):
        self.toggle_float = False

    def set_limits_override(self, data):
        self.limits_override = data
        if data:
            self._toggle_properties('limits_override')
    def get_limits_override(self):
        return self.limits_override
    def reset_limits_override(self):
        self.limits_override = False

    def set_flood(self, data):
        self.flood = data
        if data:
            self._toggle_properties('flood')
    def get_flood(self):
        return self.flood
    def reset_flood(self):
        self.flood = False

    def set_mist(self, data):
        self.mist = data
        if data:
            self._toggle_properties('mist')
    def get_mist(self):
        return self.mist
    def reset_mist(self):
        self.mist = False

    def set_block_delete(self, data):
        self.block_delete = data
        if data:
            self._toggle_properties('block_delete')
    def get_block_delete(self):
        return self.block_delete
    def reset_block_delete(self):
        self.block_delete = False

    def set_optional_stop(self, data):
        self.optional_stop = data
        if data:
            self._toggle_properties('optional_stop')
    def get_optional_stop(self):
        return self.optional_stop
    def reset_optional_stop(self):
        self.optional_stop = False

    def set_mdi_command(self, data):
        self.mdi_command = data
        if data:
            self._toggle_properties('mdi_command')
    def get_mdi_command(self):
        return self.mdi_command
    def reset_mdi_command(self):
        self.mdi_command = False

    def set_ini_mdi_command(self, data):
        self.ini_mdi_command = data
        if data:
            self._toggle_properties('ini_mdi_command')
    def get_ini_mdi_command(self):
        return self.ini_mdi_command
    def reset_ini_mdi_command(self):
        self.ini_mdi_command = False

    def set_dro_absolute(self, data):
        self.dro_absolute = data
        if data:
            self._toggle_properties('dro_absolute')
    def get_dro_absolute(self):
        return self.dro_absolute
    def reset_dro_absolute(self):
        self.dro_absolute = False

    def set_dro_relative(self, data):
        self.dro_relative = data
        if data:
            self._toggle_properties('dro_relative')
    def get_dro_relative(self):
        return self.dro_relative
    def reset_dro_relative(self):
        self.dro_relative = False

    def set_dro_dtg(self, data):
        self.dro_dtg = data
        if data:
            self._toggle_properties('dro_dtg')
    def get_dro_dtg(self):
        return self.dro_dtg
    def reset_dro_dtg(self):
        self.dro_dtg = False

    def set_exit(self, data):
        self.exit = data
        if data:
            self._toggle_properties('exit')
    def get_exit(self):
        return self.exit
    def reset_exit(self):
        self.exit = False

    # NON BOOL VARIABLES------------------
    def set_incr_imperial(self, data):
        self.jog_incr_imperial = data
    def get_incr_imperial(self):
        return self.jog_incr_imperial
    def reset_incr_imperial(self):
        self.jog_incr_imperial = 0.010

    def set_incr_mm(self, data):
        self.jog_incr_mm = data
    def get_incr_mm(self):
        return self.jog_incr_mm
    def reset_incr_mm(self):
        self.jog_incr_mm = 0.025

    def set_incr_angle(self, data):
        self.jog_incr_angle = data
    def get_incr_angle(self):
        return self.jog_incr_angle
    def reset_incr_angle(self):
        self.jog_incr_angle = -1

    def set_float(self, data):
        self.float = data
    def get_float(self):
        return self.float
    def reset_float(self):
        self.float = 100.0

    def set_float_alt(self, data):
        self.float_alt = data
    def get_float_alt(self):
        return self.float_alt
    def reset_float_alt(self):
        self.float_alt = 100.0

    def set_view_change(self, data):
        self.view_change = data
        if data:
            self._toggle_properties('view_change')
    def get_view_change(self):
        return self.view_change
    def reset_view_change(self):
        self.view_change = False

    def set_view_type(self, data):
        if not data.lower() in('x', 'y', 'y2', 'z', 'z2', 'p', 'clear',
                    'zoom-in','zoom-out','pan-up','pan-down',
                    'pan-left','pan-right','rotate-up','rotate-down',
                    'rotate-cw','rotate-ccw'):
            data = 'p'
        self.view_type = data
    def get_view_type(self):
        return self.view_type
    def reset_view_type(self):
        self.view_type = 'p'

    def set_command_text(self, data):
        self.command_text = data
    def get_command_text(self):
        return self.command_text
    def reset_command_text(self):
        self.command_text = ''

    def set_ini_mdi_num(self, data):
        self.ini_mdi_num = data
    def get_ini_mdi_num(self):
        return self.ini_mdi_num
    def reset_ini_mdi_num(self):
        self.ini_mdi_num = 0

    # designer will show these properties in this order:
    # BOOL
    estop_action = QtCore.pyqtProperty(bool, get_estop, set_estop, reset_estop)
    machine_on_action = QtCore.pyqtProperty(bool, get_machine_on, set_machine_on, reset_machine_on)
    auto_action = QtCore.pyqtProperty(bool, get_auto, set_auto, reset_auto)
    mdi_action = QtCore.pyqtProperty(bool, get_mdi, set_mdi, reset_mdi)
    manual_action = QtCore.pyqtProperty(bool, get_manual, set_manual, reset_manual)
    run_action = QtCore.pyqtProperty(bool, get_run, set_run, reset_run)
    abort_action = QtCore.pyqtProperty(bool, get_abort, set_abort, reset_abort)
    pause_action = QtCore.pyqtProperty(bool, get_pause, set_pause, reset_pause)
    load_dialog_action = QtCore.pyqtProperty(bool, get_load_dialog, set_load_dialog, reset_load_dialog)
    camview_dialog_action = QtCore.pyqtProperty(bool,
                                                get_camview_dialog, set_camview_dialog, reset_camview_dialog)
    origin_offset_dialog_action = QtCore.pyqtProperty(bool,
                                                      get_origin_offset_dialog, set_origin_offset_dialog,
                                                      reset_origin_offset_dialog)
    macro_dialog_action = QtCore.pyqtProperty(bool, get_macro_dialog, set_macro_dialog, reset_macro_dialog)
    launch_halmeter_action = QtCore.pyqtProperty(bool, get_launch_halmeter, set_launch_halmeter, reset_launch_halmeter)
    launch_status_action = QtCore.pyqtProperty(bool, get_launch_status, set_launch_status, reset_launch_status)
    launch_halshow_action = QtCore.pyqtProperty(bool, get_launch_halshow, set_launch_halshow, reset_launch_halshow)
    launch_halscope_action = QtCore.pyqtProperty(bool, get_launch_halscope, set_launch_halscope, reset_launch_halscope)
    home_action = QtCore.pyqtProperty(bool, get_home, set_home, reset_home)
    zero_axis_action = QtCore.pyqtProperty(bool, get_zero_axis, set_zero_axis, reset_zero_axis)
    jog_joint_pos_action = QtCore.pyqtProperty(bool, get_jog_joint_pos, set_jog_joint_pos, reset_jog_joint_pos)
    jog_joint_neg_action = QtCore.pyqtProperty(bool, get_jog_joint_neg, set_jog_joint_neg, reset_jog_joint_neg)
    jog_selected_pos_action = QtCore.pyqtProperty(bool, get_jog_selected_pos, set_jog_selected_pos, reset_jog_selected_pos)
    jog_selected_neg_action = QtCore.pyqtProperty(bool, get_jog_selected_neg, set_jog_selected_neg, reset_jog_selected_neg)
    jog_incr_action = QtCore.pyqtProperty(bool, get_jog_incr, set_jog_incr, reset_jog_incr)
    jog_rate_action = QtCore.pyqtProperty(bool, get_jog_rate, set_jog_rate, reset_jog_rate)
    feed_over_action = QtCore.pyqtProperty(bool, get_feed_over, set_feed_over, reset_feed_over)
    rapid_over_action = QtCore.pyqtProperty(bool, get_rapid_over, set_rapid_over, reset_rapid_over)
    max_velocity_over_action = QtCore.pyqtProperty(bool, get_max_velocity_over, set_max_velocity_over, reset_max_velocity_over)
    spindle_over_action = QtCore.pyqtProperty(bool, get_spindle_over, set_spindle_over, reset_spindle_over)
    spindle_fwd_action = QtCore.pyqtProperty(bool, get_spindle_fwd, set_spindle_fwd, reset_spindle_fwd)
    spindle_rev_action = QtCore.pyqtProperty(bool, get_spindle_rev, set_spindle_rev, reset_spindle_rev)
    spindle_stop_action = QtCore.pyqtProperty(bool, get_spindle_stop, set_spindle_stop, reset_spindle_stop)
    spindle_up_action = QtCore.pyqtProperty(bool, get_spindle_up, set_spindle_up, reset_spindle_up)
    spindle_down_action = QtCore.pyqtProperty(bool, get_spindle_down, set_spindle_down, reset_spindle_down)
    view_change_action = QtCore.pyqtProperty(bool, get_view_change, set_view_change, reset_view_change)
    limits_override_action = QtCore.pyqtProperty(bool, get_limits_override, set_limits_override, reset_limits_override)
    flood_action = QtCore.pyqtProperty(bool, get_flood, set_flood, reset_flood)
    mist_action = QtCore.pyqtProperty(bool, get_mist, set_mist, reset_mist)
    block_delete_action = QtCore.pyqtProperty(bool, get_block_delete, set_block_delete, reset_block_delete)
    optional_stop_action = QtCore.pyqtProperty(bool, get_optional_stop, set_optional_stop, reset_optional_stop)
    mdi_command_action = QtCore.pyqtProperty(bool, get_mdi_command, set_mdi_command, reset_mdi_command)
    ini_mdi_command_action = QtCore.pyqtProperty(bool, get_ini_mdi_command, set_ini_mdi_command, reset_ini_mdi_command)
    dro_absolute_action = QtCore.pyqtProperty(bool, get_dro_absolute, set_dro_absolute, reset_dro_absolute)
    dro_relative_action = QtCore.pyqtProperty(bool, get_dro_relative, set_dro_relative, reset_dro_relative)
    dro_dtg_action = QtCore.pyqtProperty(bool, get_dro_dtg, set_dro_dtg, reset_dro_dtg)
    exit_action = QtCore.pyqtProperty(bool, get_exit, set_exit, reset_exit)

    def set_template_label(self, data):
        self.template_label = data
    def get_template_label(self):
        return self.template_label
    def reset_template_label(self):
        self.template_label = False
    template_label_option = QtCore.pyqtProperty(bool, get_template_label, set_template_label, reset_template_label)

    # NON BOOL
    joint_number = QtCore.pyqtProperty(int, get_joint, set_joint, reset_joint)
    incr_imperial_number = QtCore.pyqtProperty(float, get_incr_imperial, set_incr_imperial, reset_incr_imperial)
    incr_mm_number = QtCore.pyqtProperty(float, get_incr_mm, set_incr_mm, reset_incr_mm)
    incr_angular_number = QtCore.pyqtProperty(float, get_incr_angle, set_incr_angle, reset_incr_angle)
    toggle_float_option = QtCore.pyqtProperty(bool, get_toggle_float, set_toggle_float, reset_toggle_float)
    float_num = QtCore.pyqtProperty(float, get_float, set_float, reset_float)
    float_alt_num = QtCore.pyqtProperty(float, get_float_alt, set_float_alt, reset_float_alt)
    view_type_string = QtCore.pyqtProperty(str, get_view_type, set_view_type, reset_view_type)
    command_text_string = QtCore.pyqtProperty(str, get_command_text, set_command_text, reset_command_text)
    ini_mdi_number = QtCore.pyqtProperty(int, get_ini_mdi_num, set_ini_mdi_num, reset_ini_mdi_num)

    def set_textTemplate(self, data):
        self._textTemplate = data
        try:
            self._set_text(200.0)
        except:
            self.setText('Error 2')
    def get_textTemplate(self):
        return self._textTemplate
    def reset_textTemplate(self):
        self._textTemplate = '%1.3f in'
    textTemplate = QtCore.pyqtProperty(str, get_textTemplate, set_textTemplate, reset_textTemplate)

    def set_alt_textTemplate(self, data):
        self._alt_textTemplate = data
        try:
            self._set_text(200.0)
        except:
            self.setText('Error 2')
    def get_alt_textTemplate(self):
        return self._alt_textTemplate
    def reset_alt_textTemplate(self):
        self._alt_textTemplate = '%1.2f mm'
    alt_textTemplate = QtCore.pyqtProperty(str, get_alt_textTemplate, set_alt_textTemplate, reset_alt_textTemplate)

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

if __name__ == "__main__":

    import sys
    from PyQt4.QtGui import QApplication
    app = QApplication(sys.argv)

    widget = ActionButton('Action')
    # this doesn't get called without qtvcp loading the widget
    widget._hal_init()

    widget.show()
    sys.exit(app.exec_())
