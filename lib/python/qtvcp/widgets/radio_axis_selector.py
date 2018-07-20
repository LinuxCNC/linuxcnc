from PyQt5 import QtCore, QtWidgets

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Info
from qtvcp import logger

# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# INFO is INI file details
# LOG is for running code logging
STATUS = Status()
INFO = Info()
LOG = logger.getLogger(__name__)

# Set the log level for this module
#LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


class RadioAxisSelector(QtWidgets.QRadioButton, _HalWidgetBase):
    def __init__(self, parent=None):
        super(RadioAxisSelector, self).__init__(parent)
        self.toggled.connect(lambda:self.btnstate(self))
        self.axis = 'x'
        self.joint = 0

    def _hal_init(self):
        pass

    def btnstate(self,b):
       if b.isChecked() == True:
            STATUS.set_selected_axis(self.joint)

    def setAxis(self, data):
        self.axis = data
        if self.axis.isdigit():
            self.joint = int(data)
        else:
            conversion = {"X":0, "Y":1, "Z":2, "A":3, "B":4, "C":5, "U":6, "V":7, "W":8}
            self.joint = int(conversion[data.upper()])
    def getAxis(self):
        return self.axis
    def resetAxis(self):
        self.axis = 'x'
        self.joint = 0

    axis_selection = QtCore.pyqtProperty(str, getAxis, setAxis, resetAxis)

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
