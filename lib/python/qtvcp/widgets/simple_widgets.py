#!/usr/bin/env python3
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

try:
    from PyQt5.QtCore import Q_ENUM
except:
    # before qt5.10
    from PyQt5.QtCore import Q_ENUMS as Q_ENUM

from PyQt5 import QtCore, QtGui, QtWidgets
from PyQt5.QtCore import pyqtProperty, pyqtSlot
from qtvcp.widgets.widget_baseclass import (_HalWidgetBase,
        _HalToggleBase, _HalSensitiveBase, _HalScaleBase)
from qtvcp.widgets.indicatorMixIn import IndicatedMixIn
from qtvcp.lib.aux_program_loader import Aux_program_loader as _loader
from qtvcp.core import Action, Status, Info
import hal

AUX_PRGM = _loader()
ACTION = Action()
STATUS = Status()
INFO = Info()

# Set up logging
from qtvcp import logger
LOG = logger.getLogger(__name__)

# Force the log level for this module
#LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

class HALPinType:
    NONE = 0
    BIT = hal.HAL_BIT
    S32 = hal.HAL_S32
    FLOAT = hal.HAL_FLOAT

    def name(enum):
        if enum == hal.HAL_BIT:
            return 'BIT'
        elif enum == hal.HAL_S32:
            return 'S32'
        elif enum == hal.HAL_FLOAT:
            return 'FLOAT'
        else:
            return 'NONE'

# reacts to HAL pin changes
class LCDNumber(QtWidgets.QLCDNumber, _HalWidgetBase):
    def __init__(self, parent=None):
        super(LCDNumber, self).__init__(parent)
        self._floatTemplate = ''
        self._bit_pin_type = False
        self._s32_pin_type = False
        self._float_pin_type = True

    def _hal_init(self):
        if self._pin_name_ == '':
            pname = self.HAL_NAME_
        else:
            pname = self._pin_name_
        if self._bit_pin_type:
            self.hal_pin = self.HAL_GCOMP_.newpin(pname, hal.HAL_BIT, hal.HAL_IN)
            self.hal_pin.value_changed.connect(lambda data: self.updateDisplay(data))
        elif self._float_pin_type:
            self.hal_pin = self.HAL_GCOMP_.newpin(pname, hal.HAL_FLOAT, hal.HAL_IN)
            self.hal_pin.value_changed.connect(lambda data: self.updateFloatDisplay(data))
        elif self._s32_pin_type:
            self.hal_pin = self.HAL_GCOMP_.newpin(pname, hal.HAL_S32, hal.HAL_IN)
            self.hal_pin.value_changed.connect(lambda data: self.updateDisplay(data))

    def updateDisplay(self, data):
        self.display(data)

    def updateFloatDisplay(self, data):
        try:
            if self._floatTemplate == '':
                self.updateDisplay(data)
                return
            t = self._floatTemplate.format
            self.display(t(data))
        except:
            self.display('{:.2f}'.format(data))
            LOG.warning("(LCDNumber) Float formatting string: {} not valid-using {}".format(
                                    self._floatTemplate, '{:.2f}'))

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    ########################################################################

    def _toggle_properties(self, picked):
        data = ('bit', 's32', 'float')

        for i in data:
            if not i == picked:
                self[i+'_pin_type'] = False

    def set_pin_name(self, value):
        self._pin_name_ = value
    def get_pin_name(self):
        return self._pin_name_
    def reset_pin_name(self):
        self._pin_name_ = ''

    def set_bit_pin_type(self, value):
        self._bit_pin_type = value
        if value:
            self._toggle_properties('bit')
    def get_bit_pin_type(self):
        return self._bit_pin_type
    def reset_bit_pin_type(self):
        self._bit_pin_type = ''

    def set_s32_pin_type(self, value):
        self._s32_pin_type = value
        if value:
            self._toggle_properties('s32')
    def get_s32_pin_type(self):
        return self._s32_pin_type
    def reset_s32_pin_type(self):
        self._s32_pin_type = ''

    def set_float_pin_type(self, value):
        self._float_pin_type = value
        if value:
            self._toggle_properties('float')
    def get_float_pin_type(self):
        return self._float_pin_type
    def reset_float_pin_type(self):
        self._float_pin_type = ''

    def set_floatTemplate(self, data):
        self._floatTemplate = data
    def get_floatTemplate(self):
        return self._floatTemplate
    def reset_floatTemplate(self):
        self._floatTemplate = ''

    # designer will show these properties in this order:
    pin_name = QtCore.pyqtProperty(str, get_pin_name, set_pin_name, reset_pin_name)
    bit_pin_type = QtCore.pyqtProperty(bool, get_bit_pin_type, set_bit_pin_type, reset_bit_pin_type)
    s32_pin_type = QtCore.pyqtProperty(bool, get_s32_pin_type, set_s32_pin_type, reset_s32_pin_type)
    float_pin_type = QtCore.pyqtProperty(bool, get_float_pin_type, set_float_pin_type, reset_float_pin_type)
    floatTemplate = QtCore.pyqtProperty(str, get_floatTemplate, set_floatTemplate, reset_floatTemplate)

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

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
        if self._pin_name_ == '':
            pname = self.HAL_NAME_
        else:
            pname = self._pin_name_
        self.hal_pin_s = self.HAL_GCOMP_.newpin(str(pname +'-s'), hal.HAL_S32, hal.HAL_OUT)
        self.hal_pin_f = self.HAL_GCOMP_.newpin(pname +'-f', hal.HAL_FLOAT, hal.HAL_OUT)
        self.hal_pin_scale = self.HAL_GCOMP_.newpin(pname +'-scale', hal.HAL_FLOAT, hal.HAL_IN)
        self.hal_pin_scale.set(1)
        self.updateValue(self.value())
        self.valueChanged.connect(lambda data:self.updateValue(data))

    def updateValue(self, data):
        scale = self.hal_pin_scale.get()
        self.hal_pin_s.set(data)
        self.hal_pin_f.set(data*scale)

    def set_pin_name(self, value):
        self._pin_name_ = value
    def get_pin_name(self):
        return self._pin_name_
    def reset_pin_name(self):
        self._pin_name_ = ''
    pin_name = QtCore.pyqtProperty(str, get_pin_name, set_pin_name, reset_pin_name)

class Dial(QtWidgets.QDial, _HalWidgetBase):
    def __init__(self, parent=None):
        super(Dial, self).__init__(parent)
        self._lastRawCount = 0
        self._currentTotalCount = 0
        self._deltaScaled  = 0
        self.epiLow = int(self.maximum() * .25)
        self.epiHigh = self.maximum() - self.epiLow
        self.scale = 1

    def _hal_init(self):
        if self._pin_name_ == '':
            pname = self.HAL_NAME_
        else:
            pname = self._pin_name_
        self.hal_pin_s = self.HAL_GCOMP_.newpin(str(pname +'-s'), hal.HAL_S32, hal.HAL_OUT)
        self.hal_pin_f = self.HAL_GCOMP_.newpin(pname +'-f', hal.HAL_FLOAT, hal.HAL_OUT)
        self.hal_pin_d = self.HAL_GCOMP_.newpin(pname +'-d', hal.HAL_FLOAT, hal.HAL_OUT)
        self.hal_pin_scale = self.HAL_GCOMP_.newpin(pname +'-scale', hal.HAL_FLOAT, hal.HAL_IN)
        self.hal_pin_scale.value_changed.connect(lambda data: self.updateScale(data))
        self.hal_pin_scale.set(1)
        self.updateCount(self.value())
        self.valueChanged.connect(lambda data:self.updateCount(data))

    def updateScale(self, data):
        self.scale = data
        self.hal_pin_f.set(self._currentTotalCount * data)

    def updateCount(self, count):
        # wrapping dials -> 0 and maximum is the same position
        if count == self.maximum() and self.wrapping(): count = 0

        delta = self._lastRawCount - count
        #print 'last:',self._lastRawCount ,'raw count',count,'delta',delta,'count:',count
        if self._lastRawCount > self.epiHigh and count < self.epiLow  :
                change = self.maximum() - self._lastRawCount + count
                self._currentTotalCount += change
                self._deltaScaled += change * self.scale
        elif count > self.epiHigh and self._lastRawCount < self.epiLow  :
                change = self.maximum() - count + self._lastRawCount
                self._currentTotalCount -= change
                self._deltaScaled -= change * self.scale
        else:
                self._currentTotalCount -= delta
                self._deltaScaled -= delta * self.scale


        self._lastRawCount = count
        self.hal_pin_s.set(self._currentTotalCount)
        self.hal_pin_f.set(self._currentTotalCount * self.scale)
        self.hal_pin_d.set(self._deltaScaled)

    def set_pin_name(self, value):
        self._pin_name_ = value
    def get_pin_name(self):
        return self._pin_name_
    def reset_pin_name(self):
        self._pin_name_ = ''
    pin_name = QtCore.pyqtProperty(str, get_pin_name, set_pin_name, reset_pin_name)

class DoubleScale(QtWidgets.QDoubleSpinBox, _HalScaleBase):
    intOutput = QtCore.pyqtSignal(int)
    floatOutput = QtCore.pyqtSignal(float)

    def __init__(self, parent=None):
        super(DoubleScale, self).__init__(parent)
        self.setValue(1)

    # one can connect signals to this widget to
    # feed an input that gets scaled by this widget. 
    @QtCore.pyqtSlot(float)
    @QtCore.pyqtSlot(int)
    def setInput(self, data):
        self.input = data
        self.valueChanged.emit(self.value())

    # call original pin update then
    # update anything connected to signals
    def _pin_update(self, data):
        super(DoubleScale, self)._pin_update(data)
        self.intOutput.emit(int(self.hal_pin_s.get()))
        self.floatOutput.emit(self.hal_pin_f.get())

    def set_pin_name(self, value):
        self._pin_name_ = value
    def get_pin_name(self):
        return self._pin_name_
    def reset_pin_name(self):
        self._pin_name_ = ''
    pin_name = QtCore.pyqtProperty(str, get_pin_name, set_pin_name, reset_pin_name)

class GridLayout(QtWidgets.QWidget, _HalSensitiveBase):
    def __init__(self, parent=None):
        super(GridLayout, self).__init__(parent)


class RichButton(QtWidgets.QPushButton):
    def __init__(self, parent=None):
        super(RichButton, self).__init__(parent)
        self._text = self.text()

        self._label = QtWidgets.QLabel(self._text, self)
        self._label.setAttribute(QtCore.Qt.WA_TransparentForMouseEvents )
        self._label.setAlignment(QtCore.Qt.AlignCenter|QtCore.Qt.AlignVCenter)
        self._label.show()

    def setText(self, text):
        self._label.setText(text)

    def text(self):
        try:
            return self._label.text()
        except:
            pass

    def event(self, event):
        if event.type() ==  QtCore.QEvent.Resize:
            w = QtGui.QResizeEvent.size(event)
            try:
                self._label.resize(w.width(), w.height())
                self.resize(w.width(), w.height())
            except:
                pass
            return True
        else:
            return super(RichButton, self).event( event)

    def set_richText(self, data):
        self.setText(data)
    def get_richText(self):
        return self.text()
    def reset_richText(self):
        self.setText('Button')
    richtext_string = QtCore.pyqtProperty(str, get_richText, set_richText, reset_richText)

# button for function callbacks rather then HAL pins
class IndicatedPushButton(QtWidgets.QPushButton, IndicatedMixIn):
    def __init__(self, parent=None):
        super(IndicatedPushButton, self).__init__(parent)

    # Override setText function so we can toggle displayed text
    def setText(self, text):
        if not self._state_text:
            super(IndicatedPushButton, self).setText(text)
            return
        if self.isCheckable():
            if self.isChecked():
                super(IndicatedPushButton, self).setText(self._true_string)
            else:
                super(IndicatedPushButton, self).setText(self._false_string)
        elif self._indicator_state:
            super(IndicatedPushButton, self).setText(self._true_string)
        else:
            super(IndicatedPushButton, self).setText(self._false_string)

# button controls HAL pins
class PushButton(QtWidgets.QPushButton, IndicatedMixIn, HALPinType):
    HALPinType = HALPinType
    Q_ENUM(HALPinType)

    # older version of pyqt5 need this as well as Q_ENUM
    NONE = 0
    BIT = hal.HAL_BIT
    S32 = hal.HAL_S32
    FLOAT = hal.HAL_FLOAT

    def __init__(self, parent=None):
        super(PushButton, self).__init__(parent)

        self._pin_type = HALPinType.S32
        self._groupPinName = ''
        self._exclusiveValue = 0.0
        self.halPinGroup = None

    # Override setText function so we can toggle displayed text
    def setText(self, text):
        if not self._state_text:
            super(PushButton,self).setText(text)
            return
        if self.isCheckable():
            if self.isChecked():
                super(PushButton, self).setText(self._true_string)
            else:
                super(PushButton, self).setText(self._false_string)
        elif self._indicator_state:
            super(PushButton, self).setText(self._true_string)
        else:
            super(PushButton, self).setText(self._false_string)

    # make the super class (pushbutton) HAL pins
    # then the button pins
    # this overrides the super class function
    # but then calls it after to connect signals etc.
    def _hal_init(self):
        if self.autoExclusive():
            self.makeGroupPin()
        if self._pin_name_ == '':
            pname = self.HAL_NAME_
        else:
            pname = self._pin_name_
        self.hal_pin = self.HAL_GCOMP_.newpin(str(pname), hal.HAL_BIT, hal.HAL_OUT)

        super(PushButton, self)._hal_init()

    # done so we can adjust 'checkable' signals at run time
    # ie delete all signals an re apply them
    # this overrides the super class function
    def connectSignals(self):
        super().connectSignals()
        def _update(state):
            self.hal_pin.set(state)
            if self.autoExclusive() and state:
                self.updateGroup()
        if self.isCheckable():
            self.toggled[bool].connect(_update)
        else:
            self.pressed.connect(lambda: _update(True))
            self.released.connect(lambda: _update(False))
        _update(self.isChecked())

    # find the first button in group
    # it's the 'driver' button widget
    # then if it's this instance, make a HAL pin for the group
    def makeGroupPin(self):
        for i in self.parent().children():
            if isinstance(i, PushButton):

                # are we the group's driver widget?
                if i is self:
                    # make pin based on type and name
                    if not self._pin_type == HALPinType.NONE:
                        ptype = self._pin_type
                        if self._groupPinName == '':
                            name = HALPinType.name(self._pin_type)
                            pname = self.HAL_NAME_ + '.exclusive'+name
                        else:
                            pname = self._groupPinName
                        self.halPinGroup = self.HAL_GCOMP_.newpin(pname , ptype, hal.HAL_OUT)
                    break
                break

    # find the driver widget and update it's group HAL pin
    def updateGroup(self):
        value = 0 # flag and value of pin
        for i in self.parent().children():
            if isinstance(i, QtWidgets.QAbstractButton):
                if value == 0:
                    driver = i
                    value = -1 # flag we found the driver
                if i.isChecked():
                    #print(self.objectName(),' Checked:',i.objectName()
                    value = i._exclusiveValue
                    #print('value:',value)
                    break

        if self._pin_type == HALPinType.BIT:
            data = bool(value)
        elif self._pin_type == HALPinType.FLOAT:
            data = float(value)
        elif self._pin_type == HALPinType.S32:
            data = int(value)
        else:
            return

        # update the pin of the 'driver' widget
        try:
            if not driver.halPinGroup is None:
                driver.halPinGroup.set(data)
        except Exception as e:
            print(e)

    ########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the QtCore.pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    ########################################################################

    def set_pin_type(self, value):
        self._pin_type = value
    def get_pin_type(self):
        return self._pin_type
    def reset_pin_type(self):
        self._pin_type = HALPinType.S32

    # designer will show these properties in this order:
    pin_type = QtCore.pyqtProperty(HALPinType, get_pin_type, set_pin_type, reset_pin_type)

    def set_group_pin_name(self, value):
        self._groupPinName = value
    def get_group_pin_name(self):
        return self._groupPinName
    def reset_group_pin_name(self):
        self._groupPinName = ''
    groupPinName = QtCore.pyqtProperty(str, get_group_pin_name, set_group_pin_name, reset_group_pin_name)

    def set_exclusive_value(self, data):
        self._exclusiveValue = data
        self.updateGroup()
    def get_exclusive_value(self):
        return self._exclusiveValue
    def reset_exclusive_value(self):
        self._exclusiveValue = 0.0
        self.updateGroup()

    exclusiveHALValue = QtCore.pyqtProperty(float, get_exclusive_value, set_exclusive_value, reset_exclusive_value)

class ScaledLabel(QtWidgets.QLabel):
    '''
    Label that scales the text based on available size.
    Base widget for DRO display.
    '''
    def __init__(self, parent=None):
        super(ScaledLabel, self).__init__(parent)
        self._text = ''
        self._scaled = True

    def _hal_init(self):
        if self.textFormat() in( 0,1) and self._scaled:
            self.setSizePolicy(QtWidgets.QSizePolicy(QtWidgets.QSizePolicy.Ignored,
                                             QtWidgets.QSizePolicy.Ignored))
            self.setMinSize(15)

    def textSample(self):
        '''
        Holds a sample of text to reserve space for the longest text.
        '''
        if self._text =='':
            return self.text()
        return self._text

    def setMinSize(self, minfs):
        f = self.font()
        f.setPointSizeF(minfs)
        br = QtGui.QFontMetrics(f).boundingRect(self.textSample())
        self.setMinimumSize(br.width(), br.height())

    def setMaxSize(self, maxfs):
        f = self.font()
        f.setPointSizeF(maxfs)
        br = QtGui.QFontMetrics(f).boundingRect(self.textSample())
        self.setMaximumSize(br.width(), br.height())

    def resizeEvent(self, event):
        super(ScaledLabel, self).resizeEvent(event)
        if not self._scaled:
            return
        #if  self.textFormat() == QtCore.Qt.RichText:
            #print(self.text())
            #print(self.styleSheet(),self.text(),self.font().pointSizeF())
        if not self.text() or self.textFormat() == QtCore.Qt.AutoText:
            return

        #--- fetch current parameters ----
        f = self.font()
        cr = self.contentsRect()

        #--- iterate to find the font size that fits the contentsRect ---
        dw = event.size().width() - event.oldSize().width()   # width change
        dh = event.size().height() - event.oldSize().height() # height change
        fs = max(f.pointSizeF(), .5)
        while True:
            f.setPointSize(int(fs))
            #gives bigger text
            #br =  QtGui.QFontMetrics(f).tightBoundingRect(self.textSample())
            # then this
            br = QtGui.QFontMetrics(f).boundingRect(self.textSample())
            if dw >= 0 and dh >= 0: # label is expanding
                if br.height() <= cr.height() and br.width() <= cr.width():
                    fs += .5
                else:
                    f.setPointSizeF(max(fs - .5, .5)) # backtrack
                    break

            else: # label is shrinking
                if br.height() > cr.height() or br.width() > cr.width():
                    fs -= .5
                else:
                    break

            if fs < .5: break

        #print (br, cr)
        #--- update font size ---
        if self.textFormat() == QtCore.Qt.RichText:
            self.setStyleFontSize(f.pointSizeF())
        else:
            self.setFont(f)

    def setStyleFontSize(self, fs):
        self.setStyleSheet(' font: {}pt ;'.format(fs))

    def set_scaleText(self, data):
        self._scaled = data
    def get_scaleText(self):
        return self._scaled
    def reset_scaleText(self):
        self._scaled = True

    def set_testSample(self, data):
        self._text = data
    def get_testSample(self):
        return self._text
    def reset_testSample(self):
        self._text = ''

    scaleText = QtCore.pyqtProperty(bool, get_scaleText, set_scaleText, reset_scaleText)
    textSpaceSample = QtCore.pyqtProperty(str, get_testSample, set_testSample, reset_testSample)

