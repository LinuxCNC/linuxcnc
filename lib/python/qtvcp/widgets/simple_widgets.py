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
from qtvcp.lib.aux_program_loader import Aux_program_loader as _loader
from qtvcp.core import Action
from functools import partial
import hal

AUX_PRGM = _loader()
ACTION = Action()

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
        self.valueChanged.connect(partial(_f))


class GridLayout(QtWidgets.QWidget, _HalSensitiveBase):
    def __init__(self, parent=None):
        super(GridLayout, self).__init__(parent)


# LED indicator on the right corner
class Indicated_PushButton(QtWidgets.QPushButton, _HalWidgetBase):
    def __init__(self, parent=None):
        super(Indicated_PushButton, self).__init__(parent)
        self._indicator_state = False
        self.draw_indicator = False
        self._HAL_pin = False
        self._state_text = False
        self._python_command = False
        self._on_color = QtGui.QColor("red")
        self._off_color = QtGui.QColor("black")
        self._size = .3
        self._true_string = 'True'
        self._false_string = 'False'
        self.true_python_command = '''print"true command"'''
        self.false_python_command = '''print"false command"'''

    # Override setText function so we can toggle displayed text
    def setText(self, text):
        if not self._state_text:
            super(Indicated_PushButton, self).setText(text)
            return
        if self.isCheckable():
            if self.isChecked():
                super(Indicated_PushButton, self).setText(self._true_string)
            else:
                super(Indicated_PushButton, self).setText(self._false_string)
        elif self._indicator_state:
            super(Indicated_PushButton, self).setText(self._true_string)
        else:
            super(Indicated_PushButton, self).setText(self._false_string)

    def _hal_init(self):
        if self._HAL_pin:
            self.hal_pin_led = self.HAL_GCOMP_.newpin(self.HAL_NAME_ + '-led', hal.HAL_BIT, hal.HAL_IN)
            self.hal_pin_led.value_changed.connect(lambda data: self.indicator_update(data))
        self._globalParameter = {'__builtins__' : None, 'INSTANCE':self.QTVCP_INSTANCE_,
                                 'PROGRAM_LOADER':AUX_PRGM, 'ACTION':ACTION}
        self._localsParameter = {'dir': dir}

    # arbitraray python commands are possible using 'INSTANCE' in the string
    # gives acess to widgets and handler functions 
    def python_command(self, state = None):
        if self._python_command:
            if state:
                exec(self.true_python_command, self._globalParameter, self._localsParameter)
            else:
                exec(self.false_python_command, self._globalParameter, self._localsParameter)

    # callback to toggle text when button is toggled
    def toggle_text(self, state=None):
        if not self._state_text:
            return
        else:
            self.setText(None)

    def indicator_update(self, data):
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

    def set_state_text(self, data):
        self._state_text = data
        if data:
            self.setText(None)
    def get_state_text(self):
        return self._state_text
    def reset_state_text(self):
        self._state_text = False

    def set_python_command(self, data):
        self._python_command = data
    def get_python_command(self):
        return self._python_command
    def reset_python_command(self):
        self._python_command = False

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

    def set_true_string(self, data):
        data = data.replace('\\n' , '\n')
        self._true_string = data
        if self._state_text:
            self.setText(None)
    def get_true_string(self):
        return self._true_string
    def reset_true_string(self):
        self._true_string = 'False'

    def set_false_string(self, data):
        data = data.replace('\\n' , '\n')
        self._false_string = data
        if self._state_text:
            self.setText(None)
    def get_false_string(self):
        return self._false_string
    def reset_false_string(self):
        self._false_string = 'False'

    def set_true_python_command(self, data):
        self.true_python_command = data
    def get_true_python_command(self):
        return self.true_python_command
    def reset_true_python_command(self):
        self.true_python_command = ''

    def set_false_python_command(self, data):
        self.false_python_command = data
    def get_false_python_command(self):
        return self.false_python_command
    def reset_false_python_command(self):
        self.false_python_command = ''

    indicator_option = QtCore.pyqtProperty(bool, get_indicator, set_indicator, reset_indicator)
    indicator_HAL_pin_option = QtCore.pyqtProperty(bool, get_HAL_pin, set_HAL_pin, reset_HAL_pin)
    checked_state_text_option = QtCore.pyqtProperty(bool, get_state_text, set_state_text, reset_state_text)
    python_command_option = QtCore.pyqtProperty(bool, get_python_command, set_python_command, reset_python_command)
    on_color = QtCore.pyqtProperty(QtGui.QColor, get_on_color, set_on_color)
    off_color = QtCore.pyqtProperty(QtGui.QColor, get_off_color, set_off_color)
    indicator_size = QtCore.pyqtProperty(float, get_indicator_size, set_indicator_size, reset_indicator_size)
    true_state_string = QtCore.pyqtProperty(str, get_true_string, set_true_string, reset_true_string)
    false_state_string = QtCore.pyqtProperty(str, get_false_string, set_false_string, reset_false_string)
    true_python_cmd_string = QtCore.pyqtProperty(str, get_true_python_command, set_true_python_command, reset_true_python_command)
    false_python_cmd_string = QtCore.pyqtProperty(str, get_false_python_command, set_false_python_command, reset_false_python_command)

class PushButton(Indicated_PushButton, _HalWidgetBase):
    def __init__(self, parent=None):
        super(PushButton, self).__init__(parent)

    # make the super class (pushbutton) HAL pins
    # then the button pins
    def _hal_init(self):
        super(PushButton, self)._hal_init()
        self.hal_pin = self.HAL_GCOMP_.newpin(str(self.HAL_NAME_), hal.HAL_BIT, hal.HAL_OUT)

        def _update(state):
            self.hal_pin.set(state)
            self.setChecked(state)
            if self._HAL_pin is False:
                self.indicator_update(state)
            # if using state labels option update the labels
            if self._state_text:
                self.setText(None)
            # if python commands call them 
            if self._python_command:
                if state == None:
                    state = self._indicator_state
                self.python_command(state)

        if self.isCheckable():
            self.toggled[bool].connect(_update)
        else:
            self.pressed.connect(partial(_update, True))
            self.released.connect(partial(_update, False))

class ScaledLabel(QtWidgets.QLabel):
    def __init__(self, parent=None):
        super(ScaledLabel, self).__init__(parent)

    def _hal_init(self):
        if self.textFormat() == 0:
            self.setSizePolicy(QtWidgets.QSizePolicy(QtWidgets.QSizePolicy.Ignored,
                                             QtWidgets.QSizePolicy.Ignored))
            self.setMinSize(14)

    def setMinSize(self, minfs):        
        f = self.font()
        f.setPixelSize(minfs)
        br = QtGui.QFontMetrics(f).boundingRect(self.text())
        self.setMinimumSize(br.width(), br.height())

    def resizeEvent(self, event):
        super(ScaledLabel, self).resizeEvent(event)        

        if not self.text() or self.textFormat() in(1, 2):
            return

        #--- fetch current parameters ----
        f = self.font()
        cr = self.contentsRect()

        #--- iterate to find the font size that fits the contentsRect ---
        dw = event.size().width() - event.oldSize().width()   # width change
        dh = event.size().height() - event.oldSize().height() # height change
        fs = max(f.pixelSize(), 1)
        while True:
            f.setPixelSize(fs)
            br =  QtGui.QFontMetrics(f).boundingRect(self.text())

            if dw >= 0 and dh >= 0: # label is expanding
                if br.height() <= cr.height() and br.width() <= cr.width():
                    fs += 1
                else:
                    f.setPixelSize(max(fs - 1, 1)) # backtrack
                    break

            else: # label is shrinking
                if br.height() > cr.height() or br.width() > cr.width():
                    fs -= 1
                else:
                    break

            if fs < 1: break

        #--- update font size ---
        self.setFont(f)
