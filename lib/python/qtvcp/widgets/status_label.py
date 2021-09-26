#!/usr/bin/env python3
# QTVcp Widget
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
###############################################################################

import time
import os
from PyQt5 import QtCore, QtWidgets

from qtvcp import logger
from qtvcp.widgets.simple_widgets import ScaledLabel
from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Info, Tool

# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# INFO is INI file details
# LOG is for running code logging
STATUS = Status()
INFO = Info()
TOOL = Tool()
LOG = logger.getLogger(__name__)


class StatusLabel(ScaledLabel, _HalWidgetBase):
    def __init__(self, parent=None):
        super(StatusLabel, self).__init__(parent)
        self.display_units_mm = False
        self._textTemplate = '%s'
        self._alt_textTemplate = '%s'
        self._actual_RPM = 0
        self._diameter = 1
        self._index = 0
        self._tool_dia = 0
        self._tool_offset = 0
        self._state_label_list = ['Estopped','Running','Stopped','Paused','Waiting','Reading']
        self._halpin_name = 'remapStat.tool'

        self.feed_override = True
        self.rapid_override = False
        self.max_velocity_override = False
        self.spindle_override = False
        self.jograte = False
        self.jograte_angular = False
        self.jogincr = False
        self.jogincr_angular = False
        self.tool_number = False
        self.current_feedrate = False
        self.current_feedunit = False
        self.requested_spindle_speed = False
        self.actual_spindle_speed = False
        self.actual_surface_speed = False
        self.user_system = False
        self.blendcode = False
        self.fcode = False
        self.gcodes = False
        self.mcodes = False
        self.tool_diameter = False
        self.tool_comment = False
        self.filename = False
        self.filepath = False
        self.machine_state = False
        self.time_stamp = False
        self.tool_offset = False
        self.gcode_selected = False
        self.halpin = False

    def _hal_init(self):
        super(StatusLabel, self)._hal_init()
        def _f(data):
            self._set_text(data)

        if self.feed_override:
            STATUS.connect('feed-override-changed', lambda w, data: _f(data))
        elif self.rapid_override:
            STATUS.connect('rapid-override-changed', lambda w, data: _f(data))
        elif self.max_velocity_override:
            STATUS.connect('max-velocity-override-changed', lambda w, data: self._set_max_velocity(data))
            STATUS.connect('metric-mode-changed', self._switch_max_velocity_units)
        elif self.spindle_override:
            STATUS.connect('spindle-override-changed', lambda w, data: _f(data))
        elif self.jograte:
            STATUS.connect('jograte-changed', lambda w, data: self._set_jograte(data))
            STATUS.connect('metric-mode-changed', self._switch_jog_units)
        elif self.jograte_angular:
            STATUS.connect('jograte-angular-changed', lambda w, data: _f(data))
        elif self.jogincr:
            STATUS.connect('jogincrement-changed', lambda w, data, text: _f(text))
        elif self.jogincr_angular:
            STATUS.connect('jogincrement-angular-changed', lambda w, data, text: _f(text))
        elif self.tool_number:
            STATUS.connect('tool-in-spindle-changed', lambda w, data: _f(data))
        elif self.current_feedrate:
            STATUS.connect('current-feed-rate', self._set_feedrate_text)
            STATUS.connect('metric-mode-changed', self._switch_units)
        elif self.current_feedunit:
            STATUS.connect('current-feed-rate', self._set_feedunit_text)
            STATUS.connect('metric-mode-changed', self._switch_units)
            STATUS.connect('actual-spindle-speed-changed', self._set_actual_rpm)
        elif self.requested_spindle_speed:
            STATUS.connect('requested-spindle-speed-changed', lambda w, data: _f(data))
        elif self.actual_spindle_speed:
            STATUS.connect('actual-spindle-speed-changed', lambda w, data: _f(data))
        elif self.user_system:
            STATUS.connect('user-system-changed', self._set_user_system_text)
        elif self.blendcode:
            STATUS.connect('blend-code-changed', lambda w, blend, cam: self._set_blend_text(blend, cam))
        elif self.fcode:
            STATUS.connect('f-code-changed', lambda w, data: self._set_fcode_text(data))
        elif self.gcodes:
            STATUS.connect('g-code-changed', lambda w, data: _f(data))
        elif self.mcodes:
            STATUS.connect('m-code-changed', lambda w, data: _f(data))
        elif self.tool_diameter:
            STATUS.connect('tool-info-changed', lambda w, data: self._tool_info(data, 'diameter'))
            STATUS.connect('metric-mode-changed', lambda w, data: self._switch_tool_diam_units(data))
        elif self.tool_comment:
            STATUS.connect('tool-info-changed', lambda w, data: self._tool_file_info(data, TOOL.COMMENTS))
        elif self.actual_surface_speed:
            if INFO.MACHINE_IS_LATHE:
                STATUS.connect('current-x-rel-position', lambda w, data: self._set_work_diameter(data*2))
            else:
                STATUS.connect('tool-info-changed', lambda w, data: self._ss_tool_diam(data))
            STATUS.connect('actual-spindle-speed-changed', lambda w, data: self._ss_spindle_speed(data))
            STATUS.connect('metric-mode-changed', self._switch_units)
            STATUS.connect('metric-mode-changed', lambda o, state: self._set_surface_speed())

        elif self.filename or self.filepath:
            STATUS.connect('file-loaded', self._file_loaded)
        elif self.machine_state:
            STATUS.connect('state-estop', lambda w: self._machine_state(self._state_label_list[0]))
            STATUS.connect('interp-run', lambda w: self._machine_state(self._state_label_list[1]))
            STATUS.connect('interp-idle', lambda w: self._machine_state(self._state_label_list[2]))
            STATUS.connect('interp-paused', lambda w: self._machine_state(self._state_label_list[3]))
            STATUS.connect('interp-waiting', lambda w: self._machine_state(self._state_label_list[4]))
            STATUS.connect('interp-reading', lambda w: self._machine_state(self._state_label_list[5]))
        elif self.time_stamp:
            STATUS.connect('periodic', self._set_timestamp)
        elif self.tool_offset:
            STATUS.connect('current-tool-offset', self._set_tool_offset_text)
            STATUS.connect('metric-mode-changed', lambda w, data: self._switch_tool_offsets_units(data))
        elif self.gcode_selected:
            STATUS.connect('gcode-line-selected', lambda w, line: _f(int(line)+1))
        elif self.halpin:
            STATUS.connect('periodic', lambda w: self._set_halpin_text())
        else:
            LOG.warning('{} : no option recognised'.format(self.HAL_NAME_))

    def _set_text(self, data):
            tmpl = lambda s: str(self._textTemplate) % s
            self.setText(tmpl(data))
    def _set_alt_text(self, data):
            tmpl = lambda s: str(self._alt_textTemplate) % s
            self.setText(tmpl(data))

    def _set_blend_text(self, blend, cam):
        tmpl = lambda s: str(self._textTemplate) % s
        alttmpl = lambda s: str(self._alt_textTemplate) % s
        self.setText(tmpl(blend) + alttmpl(cam))

    def _set_fcode_text(self, data):
        self._set_text(data)

    def _set_feedrate_text(self, widget, data):
        if self.display_units_mm != INFO.MACHINE_IS_METRIC:
            data = INFO.convert_units(data)
        if self.display_units_mm:
            self._set_alt_text(data)
        else:
            self._set_text(data)

    def _set_feedunit_text(self, widget, data):
        if self.display_units_mm != INFO.MACHINE_IS_METRIC:
            data = INFO.convert_units(data)
        try:
            data = abs(data/self._actual_RPM)
            if self.display_units_mm:
                self._set_alt_text(data)
            else:
                self._set_text(data)
        except:
            self.setText('')
            return

    def _set_actual_rpm(self, w, rpm):
        self._actual_RPM = rpm

    def _set_user_system_text(self, widgets, data):
        convert = { 1:"54", 2:"55", 3:"56", 4:"57", 5:"58", 6:"59", 7:"59.1", 8:"59.2", 9:"59.3"}
        self._set_text(convert[int(data)])

    def _set_machine_units(self, u, c):
        self.machine_units_mm = u
        self.unit_convert = c

    def _switch_units(self, widget, data):
        self.display_units_mm = data

    def _switch_jog_units(self, widget, data):
        self.display_units_mm = data
        self._set_jograte(STATUS.get_jograte())

    def _switch_tool_diam_units(self, units):
        self.display_units_mm = units
        data = self.conversion(self._tool_dia)
        if self.display_units_mm:
           self._set_alt_text(data)
        else:
           self._set_text(data)

    def _switch_tool_offsets_units(self, units):
        self.display_units_mm = units
        data = self.conversion(self._tool_offset)
        if self.display_units_mm:
           self._set_alt_text(data)
        else:
           self._set_text(data)

    def _switch_max_velocity_units(self, widget, data):
        self.display_units_mm = data
        self._set_max_velocity(STATUS.get_max_velocity())

    def _tool_info(self, data, field):
        if data.id != -1:
            if field == 'diameter':
                data = self.conversion(data.diameter)
                self._tool_dia = data
                if self.display_units_mm:
                    self._set_alt_text(data)
                else:
                    self._set_text(data)
                return
        self._set_text(0)

        # [0] = cell toggle
        # [1] = tool number
        # [2] = pocket number
        # [3] = X offset
        # [4] = Y offset
        # [5] = Z offset
        # [6] = A offset
        # [7] = B offset
        # [8] = C offset
        # [9] = U offset
        # [10] = V offset
        # [11] = W offset
        # [12] = tool diameter
        # [13] = frontangle
        # [14] = backangle
        # [15] = tool orientation
        # [16] = tool comment
    def _tool_file_info(self, tool_entry, index):
        toolnum = tool_entry[0]
        tool_table_line = TOOL.GET_TOOL_INFO(toolnum)
        try:
            self._set_text(str(tool_table_line[index]))
        except:
            LOG.warning('Problem with tool file info')
            self._set_text('')

    def _set_tool_offset_text(self, w, offsets):
        data = self.conversion(offsets[self._index])
        self._tool_offset = offsets[self._index]
        if self.display_units_mm:
           self._set_alt_text(data)
        else:
           self._set_text(data)

    def _ss_tool_diam(self, data):
        if data.id != -1:
            self._diameter = data.diameter
        else:
            self._diameter = 1
        self. _set_surface_speed()
    def _set_work_diameter(self, dia):
        self._diameter = dia
        self._set_surface_speed()
    def _ss_spindle_speed(self, rpm):
        self._actual_RPM = rpm
        self._set_surface_speed()
    def _set_surface_speed(self):
        diam = self.conversion(self._diameter)
        circ = abs(3.14 * self._actual_RPM * diam)
        if self.display_units_mm:
            self._set_alt_text(circ/1000) # meters per minute
        else:
            self._set_text(circ/12) # feet per minute

    # This does the conversion units
    # data must always be in machine units
    def conversion(self, data):
        if self.display_units_mm :
            return INFO.convert_machine_to_metric(data)
        else:
            return INFO.convert_machine_to_imperial(data)

    def _file_loaded(self, w, name):
        if self.filename:
            name = os.path.basename(name)
        self._set_text(name)

    def _machine_state(self, text):
        self._set_text(text)

    def _set_timestamp(self, w):
        self.setText(time.strftime(self._textTemplate))

    def _set_jograte(self, data):
        rate = self.conversion(data)
        if self.display_units_mm:
            self._set_alt_text(rate)
        else:
            self._set_text(rate)

    def _set_max_velocity(self, data):
        rate = self.conversion(data)
        if self.display_units_mm:
            self._set_alt_text(rate)
        else:
            self._set_text(rate)

    def _set_halpin_text(self):
        try:
            rate = self.HAL_GCOMP_.hal.get_value(self._halpin_name)
        except Exception as e:
            return
        if self.display_units_mm:
            self._set_alt_text(rate)
        else:
            self._set_text(rate)
    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    #
    # _toggle_properties makes it so we can only select one option
    ########################################################################

    def _toggle_properties(self, picked):
        data = ('feed_override', 'rapid_override', 'spindle_override', 'jograte',
                'jograte_angular', 'jogincr', 'jogincr_angular', 'tool_number',
                'current_feedrate', 'current_FPU',
                'requested_spindle_speed', 'actual_spindle_speed',
                'user_system', 'gcodes', 'mcodes', 'tool_diameter',
                'tool_comment',  'actual_surface_speed', 'filename', 'filepath',
                'machine_state', 'time_stamp', 'max_velocity_override', 'tool_offset',
                'gcode_selected', 'fcode', 'blendcode', 'halpin')

        for i in data:
            if not i == picked:
                self[i + '_status'] = False

    def set_textTemplate(self, data):
        self._textTemplate = data
        try:
            self._set_text(100.0)
        except ValueError:
            try:
                self.setText(time.strftime(self._textTemplate))
            except:
                raise
        except TypeError:
            try:
                self.setText(data)
            except ValueError:
                raise
        except Exception as e:
            LOG.error("textTemplate: {}, Data: {}".format(self._textTemplate, data), exc_info=e)
            self.setText('Error')
    def get_textTemplate(self):
        return self._textTemplate
    def reset_textTemplate(self):
        self._textTemplate = '%s'

    def set_alt_textTemplate(self, data):
        self._alt_textTemplate = data
        try:
            self._set_text(200.0)
        except TypeError:
            try:
                self.setText(data)
            except ValueError:
                raise
        except Exception as e:
            LOG.error("altTextTemplate: {}, Data: {}".format(self._textTemplate, data), exc_info=e)
            self.setText('Error 2')
    def get_alt_textTemplate(self):
        return self._alt_textTemplate
    def reset_alt_textTemplate(self):
        self._alt_textTemplate = '%s'

    # index for tool offset
    def set_index(self, data):
        if data < 0: data = 0
        self._index = data
    def get_index(self):
        return self._index
    def reset_index(self):
        self._index = 0


    # feed override status
    def set_feed_override(self, data):
        self.feed_override = data
        if data:
            self._toggle_properties('feed_override')
    def get_feed_override(self):
        return self.feed_override
    def reset_feed_override(self):
        self.feed_override = False

    # rapid override status
    def set_rapid_override(self, data):
        self.rapid_override = data
        if data:
            self._toggle_properties('rapid_override')
    def get_rapid_override(self):
        return self.rapid_override
    def reset_rapid_override(self):
        self.rapid_override = False

    # max_velocity override status
    def set_max_velocity_override(self, data):
        self.max_velocity_override = data
        if data:
            self._toggle_properties('max_velocity_override')
    def get_max_velocity_override(self):
        return self.max_velocity_override
    def reset_max_velocity_override(self):
        self.max_velocity_override = False

    # spindle override status
    def set_spindle_override(self, data):
        self.spindle_override = data
        if data:
            self._toggle_properties('spindle_override')
    def get_spindle_override(self):
        return self.spindle_override
    def reset_spindle_override(self):
        self.spindle_override = False

    # jograte status
    def set_jograte(self, data):
        self.jograte = data
        if data:
            self._toggle_properties('jograte')
    def get_jograte(self):
        return self.jograte
    def reset_jograte(self):
        self.jograte = False

    # jograte_angular status
    def set_jograte_angular(self, data):
        self.jograte_angular = data
        if data:
            self._toggle_properties('jograte_angular')
    def get_jograte_angular(self):
        return self.jograte_angular
    def reset_jograte_angular(self):
        self.jograte_angular = False

    # jogincr status
    def set_jogincr(self, data):
        self.jogincr = data
        if data:
            self._toggle_properties('jogincr')
    def get_jogincr(self):
        return self.jogincr
    def reset_jogincr(self):
        self.jogincr = False

    # jogincr_angular status
    def set_jogincr_angular(self, data):
        self.jogincr_angular = data
        if data:
            self._toggle_properties('jogincr_angular')
    def get_jogincr_angular(self):
        return self.jogincr_angular
    def reset_jogincr_angular(self):
        self.jogincr_angular = False

    # tool number status
    def set_tool_number(self, data):
        self.tool_number = data
        if data:
            self._toggle_properties('tool_number')
    def get_tool_number(self):
        return self.tool_number
    def reset_tool_number(self):
        self.tool_number = False

    # current feedrate status
    def set_current_feedrate(self, data):
        self.current_feedrate = data
        if data:
            self._toggle_properties('current_feedrate')
    def get_current_feedrate(self):
        return self.current_feedrate
    def reset_current_feedrate(self):
        self.current_feedrate = False

    # current feedunit status
    def set_current_feedunit(self, data):
        self.current_feedunit = data
        if data:
            self._toggle_properties('current_FPU')
    def get_current_feedunit(self):
        return self.current_feedunit
    def reset_current_feedunit(self):
        self.current_feedunit = False

    # spindle speed status
    def set_requested_spindle_speed(self, data):
        self.requested_spindle_speed = data
        if data:
            self._toggle_properties('requested_spindle_speed')
    def get_requested_spindle_speed(self):
        return self.requested_spindle_speed
    def reset_requested_spindle_speed(self):
        self.requested_spindle_speed = False

    # spindle speed status
    def set_actual_spindle_speed(self, data):
        self.actual_spindle_speed = data
        if data:
            self._toggle_properties('actual_spindle_speed')
    def get_actual_spindle_speed(self):
        return self.actual_spindle_speed
    def reset_actual_spindle_speed(self):
        self.actual_spindle_speed = False

    # user_system status
    def set_user_system(self, data):
        self.user_system = data
        if data:
            self._toggle_properties('user_system')
    def get_user_system(self):
        return self.user_system
    def reset_user_system(self):
        self.user_system = False

    # fcode status
    def set_fcode(self, data):
        self.fcode = data
        if data:
            self._toggle_properties('fcode')
    def get_fcode(self):
        return self.fcode
    def reset_fcode(self):
        self.fcode = False

    # blendcode status
    def set_blendcode(self, data):
        self.blendcode = data
        if data:
            self._toggle_properties('blendcode')
    def get_blendcode(self):
        return self.blendcode
    def reset_blendcode(self):
        self.blendcode = False

    # gcodes status
    def set_gcodes(self, data):
        self.gcodes = data
        if data:
            self._toggle_properties('gcodes')
    def get_gcodes(self):
        return self.gcodes
    def reset_gcodes(self):
        self.gcodes = False

    # mcodes status
    def set_mcodes(self, data):
        self.mcodes = data
        if data:
            self._toggle_properties('mcodes')
    def get_mcodes(self):
        return self.mcodes
    def reset_mcodes(self):
        self.mcodes = False

    # tool_diameter status
    def set_tool_diameter(self, data):
        self.tool_diameter = data
        if data:
            self._toggle_properties('tool_diameter')
    def get_tool_diameter(self):
        return self.tool_diameter
    def reset_tool_diameter(self):
        self.tool_diameter = False

    # tool_comment status
    def set_tool_comment(self, data):
        self.tool_comment = data
        if data:
            self._toggle_properties('tool_comment')
    def get_tool_comment(self):
        return self.tool_comment
    def reset_tool_comment(self):
        self.tool_comment = False

    # actual_surface_speed status
    def set_actual_surface_speed(self, data):
        self.actual_surface_speed = data
        if data:
            self._toggle_properties('actual_surface_speed')
    def get_actual_surface_speed(self):
        return self.actual_surface_speed
    def reset_actual_surface_speed(self):
        self.actual_surface_speed = False

    # filename status
    def set_filename(self, data):
        self.filename = data
        if data:
            self._toggle_properties('filename')
    def get_filename(self):
        return self.filename
    def reset_filename(self):
        self.filename = False

    # filepath status
    def set_filepath(self, data):
        self.filepath = data
        if data:
            self._toggle_properties('filepath')
    def get_filepath(self):
        return self.filepath
    def reset_filepath(self):
        self.filepath = False

    # machine_state status
    def set_machine_state(self, data):
        self.machine_state = data
        if data:
            self._toggle_properties('machine_state')
    def get_machine_state(self):
        return self.machine_state
    def reset_machine_state(self):
        self.machine_state = False

    # time_stamp status
    def set_time_stamp(self, data):
        self.time_stamp = data
        if data:
            self._toggle_properties('time_stamp')
    def get_time_stamp(self):
        return self.time_stamp
    def reset_time_stamp(self):
        self.time_stamp = False

    # tool_offset status
    def set_tool_offset(self, data):
        self.tool_offset = data
        if data:
            self._toggle_properties('tool_offset')
    def get_tool_offset(self):
        return self.tool_offset
    def reset_tool_offset(self):
        self.tool_offset = False

    def set_state_label_l(self, data):
        self._state_label_list = data
    def get_state_label_l(self):
        return self._state_label_list
    def reset_state_label_l(self):
        self._state_label_list = ['Estopped','Running','Stopped','Paused','Waiting','Reading']

    # gcode line selected
    def set_gcode_selected(self, data):
        self.gcode_selected = data
        if data:
            self._toggle_properties('gcode_selected')
    def get_gcode_selected(self):
        return self.gcode_selected
    def reset_gcode_selected(self):
        self.gcode_selected = False

    # gcode line selected
    def set_halpin(self, data):
        self.halpin = data
        if data:
            self._toggle_properties('halpin')
    def get_halpin(self):
        return self.halpin
    def reset_halpin(self):
        self.halpin = False

    def set_halpin_name(self, data):
        self._halpin_name = data
    def get_halpin_name(self):
        return self._halpin_name
    def reset_halpin_name(self):
        self._halpin_name = ''

    textTemplate = QtCore.pyqtProperty(str, get_textTemplate, set_textTemplate, reset_textTemplate)
    alt_textTemplate = QtCore.pyqtProperty(str, get_alt_textTemplate, set_alt_textTemplate, reset_alt_textTemplate)
    index_number = QtCore.pyqtProperty(int, get_index, set_index, reset_index)

    feed_override_status = QtCore.pyqtProperty(bool, get_feed_override, set_feed_override, reset_feed_override)
    rapid_override_status = QtCore.pyqtProperty(bool, get_rapid_override, set_rapid_override, reset_rapid_override)
    max_velocity_override_status = QtCore.pyqtProperty(bool, get_max_velocity_override, set_max_velocity_override, reset_max_velocity_override)
    spindle_override_status = QtCore.pyqtProperty(bool, get_spindle_override, set_spindle_override,
                                                  reset_spindle_override)
    jograte_status = QtCore.pyqtProperty(bool, get_jograte, set_jograte, reset_jograte)
    jograte_angular_status = QtCore.pyqtProperty(bool, get_jograte_angular, set_jograte_angular, reset_jograte_angular)
    jogincr_status = QtCore.pyqtProperty(bool, get_jogincr, set_jogincr, reset_jogincr)
    jogincr_angular_status = QtCore.pyqtProperty(bool, get_jogincr_angular, set_jogincr_angular, reset_jogincr_angular)
    current_feedrate_status = QtCore.pyqtProperty(bool, get_current_feedrate, set_current_feedrate,
                                                  reset_current_feedrate)
    current_FPU_status = QtCore.pyqtProperty(bool, get_current_feedunit, set_current_feedunit, reset_current_feedunit)
    requested_spindle_speed_status = QtCore.pyqtProperty(bool, get_requested_spindle_speed,
                                                         set_requested_spindle_speed, reset_requested_spindle_speed)
    actual_spindle_speed_status = QtCore.pyqtProperty(bool, get_actual_spindle_speed, set_actual_spindle_speed,
                                                      reset_actual_spindle_speed)
    user_system_status = QtCore.pyqtProperty(bool, get_user_system, set_user_system, reset_user_system)
    blendcode_status = QtCore.pyqtProperty(bool, get_blendcode, set_blendcode, reset_blendcode)
    fcode_status = QtCore.pyqtProperty(bool, get_fcode, set_fcode, reset_fcode)
    gcodes_status = QtCore.pyqtProperty(bool, get_gcodes, set_gcodes, reset_gcodes)
    mcodes_status = QtCore.pyqtProperty(bool, get_mcodes, set_mcodes, reset_mcodes)
    tool_diameter_status = QtCore.pyqtProperty(bool, get_tool_diameter, set_tool_diameter, reset_tool_diameter)
    tool_comment_status = QtCore.pyqtProperty(bool, get_tool_comment, set_tool_comment, reset_tool_comment)
    tool_number_status = QtCore.pyqtProperty(bool, get_tool_number, set_tool_number, reset_tool_number)
    tool_offset_status = QtCore.pyqtProperty(bool, get_tool_offset, set_tool_offset, reset_tool_offset)
    gcode_selected_status = QtCore.pyqtProperty(bool, get_gcode_selected, set_gcode_selected, reset_gcode_selected)
    actual_surface_speed_status = QtCore.pyqtProperty(bool, get_actual_surface_speed, set_actual_surface_speed,
                                                      reset_actual_surface_speed)
    filename_status = QtCore.pyqtProperty(bool, get_filename, set_filename,
                                                      reset_filename)
    filepath_status = QtCore.pyqtProperty(bool, get_filepath, set_filepath,
                                                      reset_filepath)
    machine_state_status = QtCore.pyqtProperty(bool, get_machine_state, set_machine_state,
                                                      reset_machine_state)
    time_stamp_status = QtCore.pyqtProperty(bool, get_time_stamp, set_time_stamp,
                                                      reset_time_stamp)
    halpin_status = QtCore.pyqtProperty(bool, get_halpin, set_halpin, reset_halpin)
    state_label_list = QtCore.pyqtProperty(QtCore.QVariant.typeToName(QtCore.QVariant.StringList), get_state_label_l, set_state_label_l, reset_state_label_l)
    halpin_name = QtCore.pyqtProperty(str, get_halpin_name, set_halpin_name, reset_halpin_name)
    # boilder code
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

if __name__ == "__main__":

    import sys

    app = QApplication(sys.argv)
    label = Lcnc_State_Label()
    label.show()
    sys.exit(app.exec_())
