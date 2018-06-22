#!/usr/bin/env python
# qtVcp simple widgets
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

from PyQt5 import QtCore, QtGui, QtWidgets
from qtvcp.widgets.widget_baseclass import _HalWidgetBase, _HalToggleBase, _HalSensitiveBase
from functools import partial
import hal

# Set up logging
from qtvcp import logger
LOG = logger.getLogger(__name__)


# reacts to HAL pin changes
class LCDNumber(QtWidgets.QLCDNumber, _HalWidgetBase):
    def __init__(self, parent=None):
        super(LCDNumber, self).__init__(parent)

    def _hal_init(self):
        self.hal_pin = self.HAL_GCOMP_.newpin(self.HAL_NAME_, hal.HAL_FLOAT, hal.HAL_IN)
        self.hal_pin.value_changed.connect(lambda data: self.l_update(data))

    def l_update(self, data):
        self.display(data)


class CheckBox(QtWidgets.QCheckBox, _HalToggleBase):
    def __init__(self, parent=None):
        super(CheckBox, self).__init__(parent)


class RadioButton(QtWidgets.QRadioButton, _HalToggleBase):
    def __init__(self, parent=None):
        super(RadioButton, self).__init__(parent)


class Slider(QtWidgets.QSlider, _HalWidgetBase):
    def __init__(self, parent=None):
        super(Slider, self).__init__(parent)

    def _hal_init(self):
        self.hal_pin_s = self.HAL_GCOMP_.newpin(str(self.HAL_NAME_+'-s'), hal.HAL_S32, hal.HAL_OUT)
        self.hal_pin_f = self.HAL_GCOMP_.newpin(self.HAL_NAME_+'-f', hal.HAL_FLOAT, hal.HAL_OUT)
        self.hal_pin_scale = self.HAL_GCOMP_.newpin(self.HAL_NAME_+'-scale', hal.HAL_FLOAT, hal.HAL_IN)
        self.hal_pin_scale.set(1)
        def _f(data):
            scale = self.hal_pin_scale.get()
            self.hal_pin_s.set(data)
            self.hal_pin_f.set(data*scale)
        self.valueChanged.connect(partial(-f))


class GridLayout(QtWidgets.QWidget, _HalSensitiveBase):
    def __init__(self, parent=None):
        super(GridLayout, self).__init__(parent)


# LED indicator on the right corner
class Indicated_PushButton(QtWidgets.QPushButton, _HalWidgetBase):
    def __init__(self, parent=None):
        super(Indicated_PushButton, self).__init__(parent)
        self._indicator_state = False
        self.draw_indicator = False
        self._HAL_pin = True
        self._on_color = QtGui.QColor("red")
        self._off_color = QtGui.QColor("black")
        self._size = .3

    def _hal_init(self):
        if self._HAL_pin:
            self.hal_pin_led = self.HAL_GCOMP_.newpin(self.HAL_NAME_ + '-led', hal.HAL_BIT, hal.HAL_IN)
            self.hal_pin_led.value_changed.connect(lambda data: self.led_update(data))

    def led_update(self, data):
        self._indicator_state = data
        self.update()

    # override paint function to first paint the stock button
    # then our indicator paint routine
    def paintEvent(self, event):
        super(Indicated_PushButton, self).paintEvent(event)
        if self.draw_indicator:
            self.paintIndicator()

    # Paint specified size a triangle at the top right
    def paintIndicator(self):
            p = QtGui.QPainter(self)
            rect = p.window()
            top_right = rect.topRight()
            if self.width() < self.height():
                size = self.width() * self._size
            else:
                size = self.height() * self._size
            if self._indicator_state:
                color = self._on_color
            else:
                color = self._off_color
            triangle = QtGui.QPolygon([top_right, top_right - QtCore.QPoint(size, 0),
                                       top_right + QtCore.QPoint(0, size)])
            p.setPen(QtGui.QPen(QtGui.QBrush(QtGui.QColor(0, 0, 0, 120)), 6))
            p.drawLine(triangle.point(1), triangle.point(2))
            p.setBrush(QtGui.QBrush(color))
            p.setPen(color)
            p.drawPolygon(triangle)

    def set_indicator(self, data):
        self.draw_indicator = data
        self.update()
    def get_indicator(self):
        return self.draw_indicator
    def reset_indicator(self):
        self.draw_indicator = False

    def set_HAL_pin(self, data):
        self._HAL_pin = data
    def get_HAL_pin(self):
        return self._HAL_pin
    def reset_HAL_pin(self):
        self._HAL_pin = False

    def get_on_color(self):
        return self._on_color
    def set_on_color(self, value):
        self._on_color = value
        self.update()
    def get_off_color(self):
        return self._off_color
    def set_off_color(self, value):
        self._off_color = value
        self.update()

    def set_indicator_size(self, data):
        self._size = data
        self.update()
    def get_indicator_size(self):
        return self._size
    def reset_indicator_size(self):
        self._size = 0.3
        self.update()

    indicator_option = QtCore.pyqtProperty(bool, get_indicator, set_indicator, reset_indicator)
    HAL_pin_option = QtCore.pyqtProperty(bool, get_HAL_pin, set_HAL_pin, reset_HAL_pin)
    on_color = QtCore.pyqtProperty(QtGui.QColor, get_on_color, set_on_color)
    off_color = QtCore.pyqtProperty(QtGui.QColor, get_off_color, set_off_color)
    indicator_size = QtCore.pyqtProperty(float, get_indicator_size, set_indicator_size, reset_indicator_size)


class PushButton(Indicated_PushButton, _HalWidgetBase):
    def __init__(self, parent=None):
        super(PushButton, self).__init__(parent)

    # make the super class (pushbutton) HAL pins
    # then the button pins
    def _hal_init(self):
        super(PushButton, self)._hal_init()
        self.hal_pin = self.HAL_GCOMP_.newpin(str(self.HAL_NAME_), hal.HAL_BIT, hal.HAL_OUT)
        def _f(data):
                self.hal_pin.set(data)
        self.pressed.connect(partial(_f, True))
        self.released.connect(partial(_f, False))
