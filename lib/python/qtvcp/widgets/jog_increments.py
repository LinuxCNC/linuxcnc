#!/usr/bin/python2.7

from PyQt5 import QtCore, QtWidgets

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Info

STATUS = Status()
INFO = Info()

# Set up logging
from qtvcp import logger
log = logger.getLogger(__name__)


class Lcnc_JogIncrements(QtWidgets.QComboBox, _HalWidgetBase):
    def __init__(self, parent=None):
        super(Lcnc_JogIncrements, self).__init__(parent)
        for item in (INFO.JOG_INCREMENTS):
            self.addItem(item)
        self.currentIndexChanged.connect(self.selectionchange)

    def _hal_init(self):
        self.selectionchange(0)

    def selectionchange(self, i):
        text = str(self.currentText())
        log.debug("Current index: {}, selection changed {}".format(i, text))
        try:
            inc = self.parse_increment(text)
        except  Exception as e:
            log.debug('Exception parsing increment', exc_info=e)
            inc = 0
        STATUS.set_jog_increments(inc, text)

    def parse_increment(self, jogincr):
        if jogincr.endswith("mm"):
            scale = self.conversion(1/25.4)
        elif jogincr.endswith("cm"):
            scale = self.conversion(10/25.4)
        elif jogincr.endswith("um"):
            scale = self.conversion(.001/25.4)
        elif jogincr.endswith("in") or jogincr.endswith("inch"):
            scale = self.conversion(1.)
        elif jogincr.endswith("mil"):
            scale = self.conversion(.001)
        else:
            scale = 1
        jogincr = jogincr.rstrip(" inchmuil")
        if "/" in jogincr:
            p, q = jogincr.split("/")
            jogincr = float(p) / float(q)
        else:
            jogincr = float(jogincr)
        return jogincr * scale

    def conversion(self, data):
        if INFO.MACHINE_IS_METRIC:
            return INFO.convert_units(data)
        else:
            return data

if __name__ == "__main__":

    import sys

    app = QtWidgets.QApplication(sys.argv)
    combo = Lcnc_JogIncrements()
    combo.show()
    sys.exit(app.exec_())
