#!/usr/bin/python2.7
# qtvcp
#
# Copyright (c) 2017  Chris Morley <chrisinnanaimo@hotmail.com>
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
#
#################################################################################

from PyQt5.QtCore import pyqtProperty, pyqtSlot, Qt, QTimer, QSize
from PyQt5.QtGui import QColor, QPainter, QBrush, QRadialGradient
from PyQt5.QtWidgets import QWidget
from qtvcp.widgets.widget_baseclass import _HalWidgetBase, hal
from qtvcp import logger

# Instantiate the libraries with global reference
# LOG is for running code logging
LOG = logger.getLogger(__name__)

# Set the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


class LED(QWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(LED, self).__init__(parent)
        self._diamX = 0
        self._diamY = 0
        self._diameter = 15
        self._color = QColor("red")
        self._alignment = Qt.AlignCenter
        self.state = False
        self.flash = False
        self._state = False
        self._flashing = False
        self._flashRate = 200

        self._timer = QTimer()
        self._timer.timeout.connect(self.toggleState)

        self.setDiameter(self._diameter)

        self._halpin_option = True

    def _hal_init(self):
        if (self._halpin_option):
            _HalWidgetBase._hal_init(self)
            self.hal_pin = self.HAL_GCOMP_.newpin(self.HAL_NAME_, hal.HAL_BIT, hal.HAL_IN)
            self.hal_pin.value_changed.connect(lambda s: self.change_state(s))
            # not sure we need a flash pin
            #self.hal_pin_flash = self.HAL_GCOMP_.newpin(self.HAL_NAME_+'-flash', hal.HAL_BIT, hal.HAL_IN)
            #self.hal_pin_flash.value_changed.connect( lambda s: self.setFlashing(s))

    @pyqtSlot(bool)
    def change_state(self, data):
        self.state = data
        if data and self.flash:
            self.setFlashing(True)
        elif data and not self.flash:
            self.setState(True)
        else:
            self.setFlashing(False)
            self.setState(False)

    def paintEvent(self, event):
        painter = QPainter()
        x = 0
        y = 0
        if self._alignment & Qt.AlignLeft:
            x = 0
        elif self._alignment & Qt.AlignRight:
            x = self.width() - self._diameter
        elif self._alignment & Qt.AlignHCenter:
            x = (self.width() - self._diameter) / 2
        elif self._alignment & Qt.AlignJustify:
            x = 0

        if self._alignment & Qt.AlignTop:
            y = 0
        elif self._alignment & Qt.AlignBottom:
            y = self.height() - self._diameter
        elif self._alignment & Qt.AlignVCenter:
            y = (self.height() - self._diameter) / 2

        gradient = QRadialGradient(x + self._diameter / 2, y + self._diameter / 2,
                                   self._diameter * 0.4, self._diameter * 0.4, self._diameter * 0.4)
        gradient.setColorAt(0, Qt.white)

        if self._state:
            gradient.setColorAt(1, self._color)
        else:
            gradient.setColorAt(1, Qt.black)

        painter.begin(self)
        brush = QBrush(gradient)
        painter.setPen(self._color)
        painter.setRenderHint(QPainter.Antialiasing, True)
        painter.setBrush(brush)
        painter.drawEllipse(x, y, self._diameter - 1, self._diameter - 1)

        if self._flashRate > 0 and self._flashing:
            self._timer.start(self._flashRate)
        else:
            self._timer.stop()

        painter.end()

    def minimumSizeHint(self):
        return QSize(self._diameter, self._diameter)

    def sizeHint(self):
        return QSize(self._diameter, self._diameter)

    def getDiameter(self):
        return self._diameter

    def set_halpin_option(self, value):
        self._halpin_option = value
    def get_halpin_option(self):
        return self._halpin_option
    def reset_halpin_option(self):
        self._halpin_option = True

    @pyqtSlot(int)
    def setDiameter(self, value):
        self._diameter = value
        self.update()

    def getColor(self):
        return self._color

    @pyqtSlot(QColor)
    def setColor(self, value):
        self._color = value
        self.update()

    def getAlignment(self):
        return self._alignment

    @pyqtSlot(Qt.Alignment)
    def setAlignment(self, value):
        self._alignment = value
        self.update()

    def resetAlignment(self):
        self._alignment = Qt.AlignCenter

    @pyqtSlot(bool)
    @pyqtSlot(int)
    def setState(self, value):
        self._state = value
        self.update()

    def getState(self):
        return self._state

    def resetState(self):
        self._state = False

    @pyqtSlot()
    def toggleState(self):
        self._state = not self._state
        self.update()

    def isFlashing(self):
        return self._flashing

    @pyqtSlot(bool)
    def setFlashing(self, value):
        self._flashing = value
        self.update()

    def setFlashState(self, value):
        self.flash = self._flashing = value
        self.update()

    def getFlashState(self):
        return self.flash

    def getFlashRate(self):
        return self._flashRate

    @pyqtSlot(int)
    def setFlashRate(self, value):
        self._flashRate = value
        self.update()

    halpin_option = pyqtProperty(bool, get_halpin_option, set_halpin_option, reset_halpin_option)
    diameter = pyqtProperty(int, getDiameter, setDiameter)
    color = pyqtProperty(QColor, getColor, setColor)
    alignment = pyqtProperty(Qt.Alignment, getAlignment, setAlignment,resetAlignment)
    state = pyqtProperty(bool, getState, setState, resetState)
    flashing = pyqtProperty(bool, getFlashState, setFlashState)
    flashRate = pyqtProperty(int, getFlashRate, setFlashRate)

if __name__ == "__main__":

    import sys
    from PyQt4.QtGui import QApplication
    app = QApplication(sys.argv)
    led = LED()
    led.show()
    led.setFlashing(True)
    sys.exit(app.exec_())
