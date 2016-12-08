#!/usr/bin/python2.7

from PyQt4 import QtCore, QtGui
import os
from qtvcp.widgets.simple_widgets import _HalWidgetBase
from qtvcp.qt_glib import GStat
GSTAT = GStat()

class Lcnc_Gstat_Bool_Label(QtGui.QLabel, _HalWidgetBase):
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
            GSTAT.connect('metric-mode-changed', lambda w,data: _f(data))
        elif self.css_mode:
            GSTAT.connect('css-mode', lambda w,data: _f(data))
        elif self.fpr_mode:
            GSTAT.connect('fpr-mode', lambda w,data: _f(data))
        elif self.diameter_mode:
            GSTAT.connect('diameter-mode', lambda w,data: _f(data))

    def _set_text(self, data):
            if data:
                self.setText(self._true_textTemplate)
            else:
                self.setText(self._false_textTemplate)

# property getter/setters

    def set_true_textTemplate(self, data):
        self._true_textTemplate = data
        try:
            self._set_text(True)
        except Exception, e:
            print e,self._textTemplate,'data:',data
            self.setText('Error')
    def get_true_textTemplate(self):
        return self._true_textTemplate
    def reset_true_textTemplate(self):
        self._true_textTemplate = '%s'
    true_textTemplate = QtCore.pyqtProperty(str, get_true_textTemplate, set_true_textTemplate, reset_true_textTemplate)

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
    false_textTemplate = QtCore.pyqtProperty(str, get_false_textTemplate, set_false_textTemplate, reset_false_textTemplate)

    # metric mode status
    def set_metric_mode(self, data):
        self.metric_mode = data
    def get_metric_mode(self):
        return self.metric_mode
    def reset_metric_mode(self):
        self.metric_mode = True
    metric_mode_status = QtCore.pyqtProperty(bool, get_metric_mode, set_metric_mode, reset_metric_mode)

    # css mode status
    def set_css_mode(self, data):
        self.css_mode = data
    def get_css_mode(self):
        return self.css_mode
    def reset_css_mode(self):
        self.css_mode = True
    css_mode_status = QtCore.pyqtProperty(bool, get_css_mode, set_css_mode, reset_css_mode)

    # fpr mode status
    def set_fpr_mode(self, data):
        self.fpr_mode = data
    def get_fpr_mode(self):
        return self.fpr_mode
    def reset_fpr_mode(self):
        self.fpr_mode = True
    fpr_mode_status = QtCore.pyqtProperty(bool, get_fpr_mode, set_fpr_mode, reset_fpr_mode)

    # diameter mode status
    def set_diameter_mode(self, data):
        self.diameter_mode = data
    def get_diameter_mode(self):
        return self.diameter_mode
    def reset_diameter_mode(self):
        self.diameter_mode = True
    diameter_mode_status = QtCore.pyqtProperty(bool, get_diameter_mode, set_diameter_mode, reset_diameter_mode)

if __name__ == "__main__":

    import sys

    app = QApplication(sys.argv)
    label = Lcnc_Gstat_Bool_Label()
    label.show()
    sys.exit(app.exec_())
