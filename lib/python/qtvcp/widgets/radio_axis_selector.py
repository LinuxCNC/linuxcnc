#!/usr/bin/env python3
from PyQt5 import QtCore, QtWidgets

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Action
from qtvcp import logger

# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# ACTION is for linuxcnc commands
# LOG is for running code logging
STATUS = Status()
ACTION = Action()
LOG = logger.getLogger(__name__)

# Force the log level for this module
#LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


class RadioAxisSelector(QtWidgets.QRadioButton, _HalWidgetBase):
    def __init__(self, parent=None):
        super(RadioAxisSelector, self).__init__(parent)
        self.toggled.connect(lambda:self.btnstate(self))
        self.axis = ''
        self.joint = -1

    def _hal_init(self):
        STATUS.connect('motion-mode-changed', lambda w,data: self.modeChanged(data))

    def btnstate(self,b):
        if b.isChecked() == True:
            if STATUS.is_joint_mode():
                ACTION.SET_SELECTED_JOINT(self.joint)
            else:
                ACTION.SET_SELECTED_AXIS(self.axis)
                # set this without causing a STATUS message output
                # in case we are selecting an axis to un/home
                STATUS.selected_joint = self.joint

    # change of joints mode / axis mode
    def modeChanged(self, mode):
        self.btnstate(self)

    def setAxis(self, data):
        if data.upper() in('X','Y','Z','A','B','C','U','V','W'):
            self.axis = str(data.upper())
    def getAxis(self):
        return self.axis
    def resetAxis(self):
        self.axis = ''

    def setJoint(self, data):
        self.joint = data
    def getJoint(self):
        return self.joint
    def resetJoint(self):
        self.joint = -1

    axis_selection = QtCore.pyqtProperty(str, getAxis, setAxis, resetAxis)
    joint_selection = QtCore.pyqtProperty(int, getJoint, setJoint, resetJoint)

# for testing without editor:
def main():
    import sys
    from PyQt5.QtWidgets import QApplication
    app = QApplication(sys.argv)
    widget = QtWidgets.QWidget()
    layout = QtWidgets.QHBoxLayout()
    widget.setLayout(layout)

    cb = RadioAxisSelector()
    layout.addWidget(cb)
    cb2 = RadioAxisSelector()
    layout.addWidget(cb2)

    widget.show()
    sys.exit(app.exec_())
if __name__ == "__main__":
    main()
