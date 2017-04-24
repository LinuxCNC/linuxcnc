
from PyQt5 import QtCore, QtWidgets
from functools import partial
import hal


###########################
""" Set of base classes """
###########################

class _HalWidgetBase:
    def hal_init(self, comp, name, object):
        self.hal, self.hal_name = comp, name
        self.qt_object = object
        self._hal_init()

    def _hal_init(self):
        """ Child HAL initialization functions """
        pass

    def hal_update(self):
        """ Update HAL state """
        pass

class _HalToggleBase(_HalWidgetBase):
    def _hal_init(self):
        self.hal_pin = self.hal.newpin(self.hal_name, hal.HAL_BIT, hal.HAL_OUT)
        self.hal_pin_not = self.hal.newpin(self.hal_name + "-not", hal.HAL_BIT, hal.HAL_OUT)
        QtCore.QObject.connect(self.qt_object, QtCore.SIGNAL("stateChanged(int)"), partial(self.t_update))

    def t_update(self,state):
        sender = self.sender()
        self.hal_pin.set(bool(state))
        self.hal_pin_not.set(not bool(state))

# reacts to HAL pin changes
class _HalScaleBase(_HalWidgetBase):
    def _hal_init(self):
        self.hal_pin = self.hal.newpin(self.hal_name, hal.HAL_FLOAT, hal.HAL_OUT)
        self.connect.value_changed.connect(lambda data: self.l_update(data))

    def l_update(self, *a):
        pass
        self.hal_pin.set(self.get_value())

# reacts to HAL pin changes
class _HalSensitiveBase(_HalWidgetBase):
    def _hal_init(self):
        self.hal_pin = self.hal.newpin(self.hal_name, hal.HAL_BIT, hal.HAL_IN)
        self.hal_pin.value_changed.connect( lambda s: self.setEnabled(s))

######################
# REAL WIDGETS
######################

# reacts to HAL pin changes
class Lcnc_LCDNumber(QtWidgets.QLCDNumber, _HalWidgetBase):
    def __init__(self, parent = None):
        QtWidgets.QLCDNumber.__init__(self, parent)
    def _hal_init(self):
        self.hal_pin = self.hal.newpin(self.hal_name, hal.HAL_FLOAT, hal.HAL_IN)
        self.hal_pin.value_changed.connect(lambda data: self.l_update(data))
    def l_update(self,data):
        self.display(data)

class Lcnc_CheckBox(QtWidgets.QCheckBox, _HalToggleBase):
    def __init__(self, parent = None):
        QtWidgets.QCheckBox.__init__(self, parent)

class Lcnc_RadioButton(QtWidgets.QRadioButton, _HalToggleBase):
    def __init__(self, parent = None):
        QtWidgets.QRadioButton.__init__(self, parent)

class Lcnc_PushButton(QtWidgets.QPushButton, _HalWidgetBase):
    def __init__(self, parent = None):
        QtWidgets.QPushButton.__init__(self, parent)
    def _hal_init(self):
        self.hal_pin = self.hal.newpin(str(self.hal_name), hal.HAL_BIT, hal.HAL_OUT)
        def _f(data):
                self.hal_pin.set(data)
        self.pressed.connect(partial(_f,True))
        self.released.connect(partial(_f,False))
        #QtCore.QObject.connect(self.qt_object, QtCore.SIGNAL("pressed()"), partial(_f,True))
        #QtCore.QObject.connect(self.qt_object, QtCore.SIGNAL("released()"), partial(_f,False))

class Lcnc_QSlider(QtWidgets.QSlider, _HalWidgetBase):
    def __init__(self, parent = None):
        QtWidgets.QSlider.__init__(self,parent)
    def _hal_init(self):
        self.hal_pin_s = self.hal.newpin(str(self.hal_name+'-s'), hal.HAL_S32, hal.HAL_OUT)
        self.hal_pin_f = self.hal.newpin(self.hal_name+'-f', hal.HAL_FLOAT, hal.HAL_OUT)
        self.hal_pin_scale = self.hal.newpin(self.hal_name+'-scale', hal.HAL_FLOAT, hal.HAL_IN)
        self.hal_pin_scale.set(1)
        def _f(data):
            scale = self.hal_pin_scale.get()
            self.hal_pin_s.set(data)
            self.hal_pin_f.set(data*scale)
        self.valueChange.connect(partial(_f))
        #QtCore.QObject.connect(self.qt_object, QtCore.SIGNAL("valueChanged(int)"), partial(_f))

class Lcnc_GridLayout(QtWidgets.QWidget, _HalSensitiveBase):
    def __init__(self, parent = None):
        QtWidgets.QGridLayout.__init__(self, parent)

