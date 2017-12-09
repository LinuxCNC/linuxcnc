#!/usr/bin/python2.7

from PyQt4 import QtCore, QtGui

from qtvcp.widgets.simple_widgets import _HalWidgetBase
from qtvcp.qt_glib import GStat
from qtvcp.qt_istat import IStat
GSTAT = GStat()
INI = IStat()

# Set up logging
from qtvcp import logger
log = logger.getLogger(__name__)

class Lcnc_Gstat_Label(QtGui.QLabel, _HalWidgetBase):

    def __init__(self, parent=None):

        super(Lcnc_Gstat_Label, self).__init__(parent)
        self.display_units_mm = False
        self._textTemplate = '%s'
        self._alt_textTemplate = 'None'
        self.feed_override = True
        self.rapid_override = False
        self.spindle_override = False
        self.jograte = False
        self.jogincr = False
        self.tool_number = False
        self.current_feedrate = False
        self.requested_spindle_speed = False
        self.user_system = False
        self.gcodes = False
        self.mcodes = False

    def _hal_init(self):
        def _f(data):
            self._set_text(data)
        if self.feed_override:
            GSTAT.connect('feed-override-changed', lambda w,data: _f(data))
        elif self.rapid_override:
            GSTAT.connect('rapid-override-changed', lambda w,data: _f(data))
        elif self.spindle_override:
            GSTAT.connect('spindle-override-changed', lambda w,data: _f(data))
        elif self.jograte:
            GSTAT.connect('jograte-changed', lambda w,data: _f(data))
        elif self.jogincr:
            GSTAT.connect('jogincrement-changed', lambda w,data,text: _f(text))
        elif self.tool_number:
            GSTAT.connect('tool-in-spindle-changed', lambda w,data: _f(data))
        elif self.current_feedrate:
            GSTAT.connect('current-feed-rate', self._set_feedrate_text)
            GSTAT.connect('metric-mode-changed',self._switch_units)
        elif self.requested_spindle_speed:
            GSTAT.connect('requested-spindle-speed-changed', lambda w,data: _f(data))
        elif self.user_system:
            GSTAT.connect('user-system-changed', self._set_user_system_text)
        elif self.gcodes:
            GSTAT.connect('g-code-changed', lambda w,data: _f(data))
        elif self.mcodes:
            GSTAT.connect('m-code-changed', lambda w,data: _f(data))

    def _set_text(self, data):
            tmpl = lambda s: str(self._textTemplate) % s
            self.setText(tmpl(data))

    def _set_feedrate_text(self, widget, data):
        if self.display_units_mm != INI.MACHINE_IS_METRIC:
            data = INI.convert_units(data)
        self._set_text(data)

    def _set_user_system_text(self, widgets, data):
        self._set_text(int(data)+53)

    def _set_machine_units(self,u,c):
        self.machine_units_mm = u
        self.unit_convert = c

    def _switch_units(self, widget, data):
        self.display_units_mm = data

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    #
    # _toggle_properties makes it so we can only select one option
    ########################################################################

    def _toggle_properties(self, picked):
        data = ('feed_override','rapid_override','spindle_override','jograte',
                'jogincr','tool_number','current_feedrate',
                'requested_spindle_speed','user_system','gcodes','mcodes')

        for i in data:
            if not i == picked:
                self[i+'_status'] = False

    def set_textTemplate(self, data):
        self._textTemplate = data
        try:
            self._set_text(100.0)
        except Exception as e:
            log.exception("textTemplate: {}, Data: {}".format(self._textTemplate, data), exc_info=e)
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

    # spindle speed status
    def set_requested_spindle_speed(self, data):
        self.requested_spindle_speed = data
        if data:
            self._toggle_properties('requested_spindle_speed')
    def get_requested_spindle_speed(self):
        return self.requested_spindle_speed
    def reset_requested_spindle_speed(self):
        self.requested_spindle_speed = False

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

    textTemplate = QtCore.pyqtProperty(str, get_textTemplate, set_textTemplate, reset_textTemplate)
    alt_textTemplate = QtCore.pyqtProperty(str, get_alt_textTemplate, set_alt_textTemplate, reset_alt_textTemplate)
    feed_override_status = QtCore.pyqtProperty(bool, get_feed_override, set_feed_override, reset_feed_override)
    rapid_override_status = QtCore.pyqtProperty(bool, get_rapid_override, set_rapid_override, reset_rapid_override)
    spindle_override_status = QtCore.pyqtProperty(bool, get_spindle_override, set_spindle_override, reset_spindle_override)
    jograte_status = QtCore.pyqtProperty(bool, get_jograte, set_jograte, reset_jograte)
    jogincr_status = QtCore.pyqtProperty(bool, get_jogincr, set_jogincr, reset_jogincr)
    tool_number_status = QtCore.pyqtProperty(bool, get_tool_number, set_tool_number, reset_tool_number)
    current_feedrate_status = QtCore.pyqtProperty(bool, get_current_feedrate, set_current_feedrate, reset_current_feedrate)
    requested_spindle_speed_status = QtCore.pyqtProperty(bool, get_requested_spindle_speed, set_requested_spindle_speed, reset_requested_spindle_speed)
    user_system_status = QtCore.pyqtProperty(bool, get_user_system, set_user_system, reset_user_system)
    gcodes_status = QtCore.pyqtProperty(bool, get_gcodes, set_gcodes, reset_gcodes)
    mcodes_status = QtCore.pyqtProperty(bool, get_mcodes, set_mcodes, reset_mcodes)

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
