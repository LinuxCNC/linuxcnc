#!/usr/bin/python2.7
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

from PyQt5 import QtCore, QtWidgets

from qtvcp import logger
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


class StatusLabel(QtWidgets.QLabel, _HalWidgetBase):
    def __init__(self, parent=None):
        super(StatusLabel, self).__init__(parent)
        self.display_units_mm = False
        self._textTemplate = '%s'
        self._alt_textTemplate = 'None'
        self._actual_RPM = 0
        self._diameter = 1

        self.feed_override = True
        self.rapid_override = False
        self.spindle_override = False
        self.jograte = False
        self.jogincr = False
        self.tool_number = False
        self.current_feedrate = False
        self.current_feedunit = False
        self.requested_spindle_speed = False
        self.actual_spindle_speed = False
        self.actual_surface_speed = False
        self.user_system = False
        self.gcodes = False
        self.mcodes = False
        self.tool_diameter = False
        self.tool_comment = False

    def _hal_init(self):
        def _f(data):
            self._set_text(data)

        if self.feed_override:
            STATUS.connect('feed-override-changed', lambda w, data: _f(data))
        elif self.rapid_override:
            STATUS.connect('rapid-override-changed', lambda w, data: _f(data))
        elif self.spindle_override:
            STATUS.connect('spindle-override-changed', lambda w, data: _f(data))
        elif self.jograte:
            STATUS.connect('jograte-changed', lambda w, data: _f(data))
        elif self.jogincr:
            STATUS.connect('jogincrement-changed', lambda w, data, text: _f(text))
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
        elif self.gcodes:
            STATUS.connect('g-code-changed', lambda w, data: _f(data))
        elif self.mcodes:
            STATUS.connect('m-code-changed', lambda w, data: _f(data))
        elif self.tool_diameter:
            STATUS.connect('tool-info-changed', lambda w, data: self._tool_info(data, 'diameter'))
        elif self.tool_comment:
            STATUS.connect('tool-info-changed', lambda w, data: self._tool_file_info(data, TOOL.COMMENTS))
        elif self.actual_surface_speed:
            STATUS.connect('tool-info-changed', lambda w, data: self._ss_tool_diam(data))
            STATUS.connect('actual-spindle-speed-changed', lambda w, data: self._ss_spindle_speed(data))
            STATUS.connect('metric-mode-changed', self._switch_units)

    def _set_text(self, data):
            tmpl = lambda s: str(self._textTemplate) % s
            self.setText(tmpl(data))
    def _set_alt_text(self, data):
            tmpl = lambda s: str(self._alt_textTemplate) % s
            self.setText(tmpl(data))

    def _set_feedrate_text(self, widget, data):
        if self.display_units_mm != INFO.MACHINE_IS_METRIC:
            data = INFO.convert_units(data)
        self._set_text(data)

    def _set_feedunit_text(self, widget, data):
        if self.display_units_mm != INFO.MACHINE_IS_METRIC:
            data = INFO.convert_units(data)
        try:
            data = (data/self._actual_RPM)
            self._set_text(data)
        except:
            self.setText('')
            return

    def _set_actual_rpm(self, w, rpm):
        self._actual_RPM = rpm

    def _set_user_system_text(self, widgets, data):
        self._set_text(int(data)+53)

    def _set_machine_units(self, u, c):
        self.machine_units_mm = u
        self.unit_convert = c

    def _switch_units(self, widget, data):
        self.display_units_mm = data

    def _tool_info(self, data, field):
        if data.id is not -1:
            if field == 'diameter':
                self._set_text(data.diameter)
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
        self._set_text(str(tool_table_line[index]))

    def _ss_tool_diam(self, data):
        if data.id is not -1:
            self._diameter = data.diameter
        else:
            self._diameter = 1
        self. _set_surface_speed()
    def _ss_spindle_speed(self, rpm):
        self._actual_RPM = rpm
        self._set_surface_speed()
    # TODO some sort of metric conversion for tool diameter
    def _set_surface_speed(self):
        circ = 3.14 * self._diameter
        if self.display_units_mm:
            self._set_alt_text(circ * self._actual_RPM)
        else:
            self._set_text(circ * self._actual_RPM)

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    #
    # _toggle_properties makes it so we can only select one option
    ########################################################################

    def _toggle_properties(self, picked):
        data = ('feed_override', 'rapid_override', 'spindle_override', 'jograte',
                'jogincr', 'tool_number', 'current_feedrate', 'current_feedunit',
                'requested_spindle_speed', 'actual_spindle_speed',
                'user_system', 'gcodes', 'mcodes', 'tool_diameter',
                'tool_comment',  'actual_surface_speed')

        for i in data:
            if not i == picked:
                self[i + '_status'] = False

    def set_textTemplate(self, data):
        self._textTemplate = data
        try:
            self._set_text(100.0)
        except Exception as e:
            LOG.exception("textTemplate: {}, Data: {}".format(self._textTemplate, data), exc_info=e)
            self.setText('Error')
    def get_textTemplate(self):
        return self._textTemplate
    def reset_textTemplate(self):
        self._textTemplate = '%s'

    def set_alt_textTemplate(self, data):
        self._alt_textTemplate = data
        try:
            self._set_text(200.0)
        except:
            self.setText('Error 2')
    def get_alt_textTemplate(self):
        return self._alt_textTemplate
    def reset_alt_textTemplate(self):
        self._alt_textTemplate = '%s'

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

    # jogincr status
    def set_jogincr(self, data):
        self.jogincr = data
        if data:
            self._toggle_properties('jogincr')
    def get_jogincr(self):
        return self.jogincr
    def reset_jogincr(self):
        self.jogincr = False

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
            self._toggle_properties('current_feedunit')
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

    textTemplate = QtCore.pyqtProperty(str, get_textTemplate, set_textTemplate, reset_textTemplate)
    alt_textTemplate = QtCore.pyqtProperty(str, get_alt_textTemplate, set_alt_textTemplate, reset_alt_textTemplate)
    feed_override_status = QtCore.pyqtProperty(bool, get_feed_override, set_feed_override, reset_feed_override)
    rapid_override_status = QtCore.pyqtProperty(bool, get_rapid_override, set_rapid_override, reset_rapid_override)
    spindle_override_status = QtCore.pyqtProperty(bool, get_spindle_override, set_spindle_override,
                                                  reset_spindle_override)
    jograte_status = QtCore.pyqtProperty(bool, get_jograte, set_jograte, reset_jograte)
    jogincr_status = QtCore.pyqtProperty(bool, get_jogincr, set_jogincr, reset_jogincr)
    tool_number_status = QtCore.pyqtProperty(bool, get_tool_number, set_tool_number, reset_tool_number)
    current_feedrate_status = QtCore.pyqtProperty(bool, get_current_feedrate, set_current_feedrate,
                                                  reset_current_feedrate)
    current_FPU_status = QtCore.pyqtProperty(bool, get_current_feedunit, set_current_feedunit, reset_current_feedunit)
    requested_spindle_speed_status = QtCore.pyqtProperty(bool, get_requested_spindle_speed,
                                                         set_requested_spindle_speed, reset_requested_spindle_speed)
    actual_spindle_speed_status = QtCore.pyqtProperty(bool, get_actual_spindle_speed, set_actual_spindle_speed,
                                                      reset_actual_spindle_speed)
    user_system_status = QtCore.pyqtProperty(bool, get_user_system, set_user_system, reset_user_system)
    gcodes_status = QtCore.pyqtProperty(bool, get_gcodes, set_gcodes, reset_gcodes)
    mcodes_status = QtCore.pyqtProperty(bool, get_mcodes, set_mcodes, reset_mcodes)
    tool_diameter_status = QtCore.pyqtProperty(bool, get_tool_diameter, set_tool_diameter, reset_tool_diameter)
    tool_comment_status = QtCore.pyqtProperty(bool, get_tool_comment, set_tool_comment, reset_tool_comment)
    actual_surface_speed_status = QtCore.pyqtProperty(bool, get_actual_surface_speed, set_actual_surface_speed,
                                                      reset_actual_surface_speed)

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
