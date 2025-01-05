#!/usr/bin/env python3
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

from PyQt5 import QtCore, QtWidgets

import hal
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

# Force the log level for this module
#LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


class JogIncrements(QtWidgets.QComboBox, _HalWidgetBase):
    def __init__(self, parent=None):
        super(JogIncrements, self).__init__(parent)
        self._pin_name = ''
        self.linear = True
        self._block_signal = False

    # Default to continuous jogging (selection 0)
    # with a combo box display, it's assumed the showing increment
    # is valid - so we must update the rate if the units mode changes.
    def _hal_init(self):
        # TODO should this be optional?
        if 1==1:
            if self._pin_name == '':
                pname = self.HAL_NAME_
            else:
                pname = self._pin_name
            self.hal_pin = self.HAL_GCOMP_.newpin(pname, hal.HAL_FLOAT, hal.HAL_OUT)
        if self.linear:
            for item in (INFO.JOG_INCREMENTS):
                self.addItem(item)
            STATUS.connect('metric-mode-changed', self._switch_units)
            STATUS.connect('jogincrement-changed', lambda w, value, text: self._checkincrements(value, text))
        else:
            for item in (INFO.ANGULAR_INCREMENTS):
                self.addItem(item)
            STATUS.connect('jogincrement-angular-changed', lambda w, value, text: self._checkincrements(value, text))
        self.currentIndexChanged.connect(self.selectionchange)
        self.selectionchange(0)

    # if units switch - current jog rate in invalid
    def _switch_units(self, w, data):
        if self.currentIndex() > 0:
            self.selectionchange(-1)

    # search the combo box for the value STATUS sent us
    # If there is a match, change the combobox to it
    # otherwise display a blank in the combobox.
    # in this way if some other widget sets a rate change
    # the combo box doesn't lie
    def _checkincrements(self,value, text):
        self.hal_pin.set(value)
        for count in range(self.count()):
            label = self.itemText(count)
            try:
                number = float(label.rstrip(" inchmuildeg").lower())
            except:
                number = None
            # assume continuous jogging (selection 0)
            if number is None:
                machn_incr = 0
            else:
                if self.linear:
                    machn_incr = self.parse_increment(label)
                else:
                    machn_incr = self.parse_angular_increment(label)
            #print count,self.itemText(count), machn_incr,value
            if machn_incr is not None and round(machn_incr,6) == round(value,6):
                self._block_signal = True
                self.setCurrentIndex(count)
                self._block_signal = False
                return
        self.setCurrentIndex(-1)

    def selectionchange(self, i):
        if self._block_signal: return
        text = str(self.currentText())
        if i == -1 and text == '': return
        try:
            # continuous jog (selection 0)
            if i == 0:
                inc = 0
            else:
                if self.linear:
                    inc = self.parse_increment(text)
                else:
                    inc = self.parse_angular_increment(text)

        except Exception as e:
            LOG.debug('Exception parsing increment - setting increment to None', exc_info=e)
            inc = None
        if inc is None:
            LOG.warning("parsed: text not recognized : {} Increment: {}".format(text, inc))
            return
        if self.linear:
            LOG.debug("Linear Current index: {} Increment: {} , selection changed {}".format(i, inc, text))
            STATUS.set_jog_increments(inc, text)
        else:
            LOG.debug("Angular Current index: {} Increment: {} , selection changed {}".format(i, inc, text))
            STATUS.set_jog_increment_angular(inc, text)

    def parse_angular_increment(self, jogincr):
        scale = 1
        if jogincr.endswith("deg"):
            scale = 1
        incr = jogincr.rstrip(" deg")
        incr = float(incr)
        return incr * scale

    # We convert INI parced increments to machine units
    def parse_increment(self, jogincr):
        if jogincr.endswith("mm"):
            scale = self.conversion(1)
        elif jogincr.endswith("cm"):
            scale = self.conversion(10)
        elif jogincr.endswith("um"):
            scale = self.conversion(.001)
        elif jogincr.endswith("in") or jogincr.endswith("inch"):
            scale = self.conversion(1., metric = False)
        elif jogincr.endswith("mil"):
            scale = self.conversion(.001, metric = False)
        else:
            scale = 1
        incr = jogincr.rstrip(" inchmuil")
        try:
            if "/" in incr:
                p, q = incr.split("/")
                incr = float(p) / float(q)
            else:
                incr = float(incr)
        except:
            return None
        LOG.debug("parsed: text: {} Increment: {} scaled: {}".format(jogincr, incr, (incr * scale)))
        return incr * scale

    # This does the conversion
    # calling function must tell us if the data is metric or not.
    def conversion(self, data, metric = True):
        if STATUS.is_metric_mode():
            if metric:
                return INFO.convert_metric_to_machine(data)
            else:
                return INFO.convert_imperial_to_machine(data)
        else:
            if metric:
                return INFO.convert_metric_to_machine(data)
            else:
                return INFO.convert_imperial_to_machine(data)


    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    #
    # _toggle_properties makes it so we can only select one option
    ########################################################################

    def _toggle_properties(self, picked):
        data = ('linear')
        for i in data:
            if not i == picked:
                self[i+'_option'] = False

    # BOOL VARIABLES----------------------
    def set_linear(self, data):
        self.linear = data
    def get_linear(self):
        return self.linear
    def reset_linear(self):
        self.linear = True

    # designer will show these properties in this order:
    # BOOL
    linear_option = QtCore.pyqtProperty(bool, get_linear, set_linear, reset_linear)

    # VARIABLES---------------------------
    def set_pin_name(self, value):
        self._pin_name = value
    def get_pin_name(self):
        return self._pin_name
    def reset_pin_name(self):
        self._pin_name = ''

    pinName = QtCore.pyqtProperty(str, get_pin_name, set_pin_name, reset_pin_name)

if __name__ == "__main__":

    import sys

    app = QtWidgets.QApplication(sys.argv)
    combo = JogIncrements()
    combo.show()
    sys.exit(app.exec_())
