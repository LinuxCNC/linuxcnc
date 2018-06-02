#!/usr/bin/python2.7

from PyQt5 import QtCore, QtWidgets
import os
from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status
STATUS = Status()

# Set up logging
from qtvcp import logger
log = logger.getLogger(__name__)

class Lcnc_Gstat_Bool_Label(QtWidgets.QLabel, _HalWidgetBase):
    def __init__(self, parent=None):
        super(Lcnc_Gstat_Bool_Label, self).__init__(parent)

        self._true_textTemplate = 'True'
        self._false_textTemplate = 'False'
        self.metric_mode = True
        self.css_mode = False
        self.fpr_mode = False
        self.diameter_mode = False

    def _hal_init(self):
        def _f(data):
            self._set_text(data)
        if self.metric_mode:
            STATUS.connect('metric-mode-changed', lambda w,data: _f(data))
        elif self.css_mode:
            STATUS.connect('css-mode', lambda w,data: _f(data))
        elif self.fpr_mode:
            STATUS.connect('fpr-mode', lambda w,data: _f(data))
        elif self.diameter_mode:
            STATUS.connect('diameter-mode', lambda w,data: _f(data))

    def _set_text(self, data):
            if data:
                self.setText(self._true_textTemplate)
            else:
                self.setText(self._false_textTemplate)

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    #
    # _toggle_properties makes it so we can only select one option
    ########################################################################

    def _toggle_properties(self, picked):
        data = ('metric_mode','css_mode','fpr_mode','diameter_mode',
                )

        for i in data:
            if not i == picked:
                self[i+'_status'] = False

# property getter/setters

    def set_true_textTemplate(self, data):
        self._true_textTemplate = data
        try:
            self._set_text(True)
        except Exception as e:
            log.exception("textTemplate: {}, Data: {}".format(self._textTemplate, data), exc_info=e)
            self.setText('Error')
    def get_true_textTemplate(self):
        return self._true_textTemplate
    def reset_true_textTemplate(self):
        self._true_textTemplate = '%s'

    def set_false_textTemplate(self, data):
        self._false_textTemplate = data
        try:
            self._set_text(False)
        except:
            self.setText('Error 2')
    def get_false_textTemplate(self):
        return self._false_textTemplate
    def reset_false_textTemplate(self):
        self._false_textTemplate = '%s'

    # metric mode status
    def set_metric_mode(self, data):
        self.metric_mode = data
        if data:
            self._toggle_properties('metric_mode')
    def get_metric_mode(self):
        return self.metric_mode
    def reset_metric_mode(self):
        self.metric_mode = True

    # css mode status
    def set_css_mode(self, data):
        self.css_mode = data
        if data:
            self._toggle_properties('css_mode')
    def get_css_mode(self):
        return self.css_mode
    def reset_css_mode(self):
        self.css_mode = True

    # fpr mode status
    def set_fpr_mode(self, data):
        self.fpr_mode = data
        if data:
            self._toggle_properties('fpr_modee')
    def get_fpr_mode(self):
        return self.fpr_mode
    def reset_fpr_mode(self):
        self.fpr_mode = True

    # diameter mode status
    def set_diameter_mode(self, data):
        self.diameter_mode = data
        if data:
            self._toggle_properties('diameter_mode')
    def get_diameter_mode(self):
        return self.diameter_mode
    def reset_diameter_mode(self):
        self.diameter_mode = True


    # designer will show these properties in this order:
    # BOOL
    metric_mode_status = QtCore.pyqtProperty(bool, get_metric_mode, set_metric_mode, reset_metric_mode)
    css_mode_status = QtCore.pyqtProperty(bool, get_css_mode, set_css_mode, reset_css_mode)
    fpr_mode_status = QtCore.pyqtProperty(bool, get_fpr_mode, set_fpr_mode, reset_fpr_mode)
    diameter_mode_status = QtCore.pyqtProperty(bool, get_diameter_mode, set_diameter_mode, reset_diameter_mode)

    # Non BOOL 
    true_textTemplate = QtCore.pyqtProperty(str, get_true_textTemplate, set_true_textTemplate, reset_true_textTemplate)
    false_textTemplate = QtCore.pyqtProperty(str, get_false_textTemplate, set_false_textTemplate, reset_false_textTemplate)

    # boilder code
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)


if __name__ == "__main__":

    import sys

    app = QApplication(sys.argv)
    label = Lcnc_STATUS_Bool_Label()
    label.show()
    sys.exit(app.exec_())
