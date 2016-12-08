#!/usr/bin/python2.7

from PyQt4 import QtCore, QtGui

from qtvcp.widgets.simple_widgets import _HalWidgetBase
from qtvcp.qt_glib import GStat
from qtvcp.qt_istat import IStat
GSTAT = GStat()
INI = IStat()

class Lcnc_JogIncrements(QtGui.QComboBox, _HalWidgetBase):
    def __init__(self, parent=None):
        super(Lcnc_JogIncrements, self).__init__(parent)
        for item in (INI.JOG_INCREMENTS):
            self.addItem(item)
        self.currentIndexChanged.connect(self.selectionchange)

    def _hal_init(self):
        self.selectionchange(0)

    def selectionchange(self, i):
        text = str(self.currentText())
        print "Current index",i,"selection changed ",text
        try:
            inc = self.parse_increment(text)
        except  Exception, e:
            #print e
            inc = 0
        GSTAT.set_jog_increments(inc, text)

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
        if INI.MACHINE_IS_METRIC:
            return INI.convert_units(data)
        else:
            return data

if __name__ == "__main__":

    import sys

    app = QtGui.QApplication(sys.argv)
    combo = Lcnc_JogIncrements()
    combo.show()
    sys.exit(app.exec_())
