from PyQt5.QtWidgets import (QToolButton, QMenu, QAction,
    QComboBox, QWidgetAction, QSizePolicy)
from PyQt5.QtCore import pyqtProperty, pyqtSignal
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

class AxisPickToolButton(QToolButton, IndicatedMixIn):
    AxisSelected = pyqtSignal(str)
    CurrentAxisPosition = pyqtSignal(str,float)

    def __init__(self, parent=None):
        super(AxisPickToolButton, self).__init__(parent)
        self._textTemplate =  'Axis: %s'
        self._currentAxis = 'X'


    def _hal_init(self):
        self.createAxisButton(self._currentAxis)
        self.axisTriggered(self._currentAxis)

    def createAxisButton(self, text):
        tmpl = lambda s: self._textTemplate % s
        self.setText(tmpl(text))
        self.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Preferred)
        self.setMinimumSize(70, 40)
        self.clicked.connect(self.axisClicked)
        SettingMenu = QMenu()
        self.settingMenu = SettingMenu
        for i in INFO.AVAILABLE_AXES:
            axisButton = QAction(QIcon('exit24.png'), i, self)
            # weird lambda i=i to work around 'function closure'
            axisButton.triggered.connect(lambda state, i=i: self.axisTriggered(i))
            SettingMenu.addAction(axisButton)
        self.setMenu(SettingMenu)

    def axisTriggered(self, data):
        tmpl = lambda s: self._textTemplate % s
        self.setText(tmpl(data))
        self.AxisSelected.emit(data)

    def axisClicked(self):
        conversion = {'X':0, 'Y':1, "Z":2, 'A':3, "B":4, "C":5, 'U':6, 'V':7, 'W':8}
        digitValue = 0.0
        try:
            p,relp,dtg = STATUS.get_position()
            text = self.text()
            for let in INFO.AVAILABLE_AXES:
                if let == text[-1]:
                    digitValue =  round(p[conversion[let]],5)
                    break
        except Exception as e:
            print(e)
            return
        print(let, digitValue)
        self.AxisSelected.emit(let)
        self.CurrentAxisPosition.emit(let, digitValue)

    def setTextTemplate(self, data):
        self._textTemplate = data
    def getTextTemplate(self):
        return self._textTemplate
    def resetTextTemplate(self):
        self._textTemplate =  'Axis: %s'
    textTemplate = pyqtProperty(str, getTextTemplate, setTextTemplate, resetTextTemplate)

    def setCurrentAxis(self, data):
        self._currentAxis = data
    def getCurrentAxis(self):
        return self._currentAxis
    def resetCurrentAxis(self):
        self._currentAxis = 'X'
    currentAxis = pyqtProperty(str, getCurrentAxis, setCurrentAxis, resetCurrentAxis)

