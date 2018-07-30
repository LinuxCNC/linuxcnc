#!/usr/bin/env python
#
# QTVcp Widget
# Copyright (c) 2018 Chris Morley
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
###############################################################################

import os

from PyQt5.QtWidgets import QWidget, QProgressBar, QToolButton, QHBoxLayout,QLabel, QMenu, QAction, QSizePolicy
from PyQt5.QtCore import Qt, QEvent, pyqtProperty, QBasicTimer, pyqtSignal
from PyQt5.QtGui import QColor, QPainter, QFont, QIcon

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Action, Info
from qtvcp.lib.aux_program_loader import Aux_program_loader
from qtvcp import logger

# Instiniate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# AUX_PRGM holds helper program loader
# INI holds ini details
# ACTION gives commands to linuxcnc
# LOG is for running code logging
STATUS = Status()
AUX_PRGM = Aux_program_loader()
INFO = Info()
ACTION = Action()
LOG = logger.getLogger(__name__)
# Set the log level for this module
#LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


# So we can customize the label
# meaning we can use a new line
# to have two levels of text
class LabeledBar(QProgressBar):
    def __init__(self, parent=None):
        super(LabeledBar, self).__init__(parent)
        self.text = ''
        self.setTextVisible(False)
        self.font = QFont()
        self.font.setPointSize(8)
        self.font.setBold(True)
        self.font.setWeight(75)

    def paintEvent(self, event):
        super(LabeledBar, self).paintEvent(event)
        qp = QPainter()
        qp.begin(self)
        self.drawText(qp)
        qp.end()

    def drawText(self, qp):
        qp.setOpacity(1.0)
        qp.setFont(self.font)
        qp.drawText(self.rect(), Qt.AlignCenter, self.text)


class HAdjustmentBar(QWidget):
    valueChanged = pyqtSignal(int)
    def __init__(self, parent=None):
        super(HAdjustmentBar, self).__init__(parent)
        self.value = 50
        self.step = 1
        self.hi_value = 100
        self.low_value = 50
        self.timer_value = 25
        self.texttemplate = 'Value =  %s'
        self.timer = QBasicTimer()
        self.showToggleButton = True
        self.showSettingMenu = True
        self.bar = LabeledBar()
        #self.bar.setSizePolicy(QSizePolicy.Expanding,QSizePolicy.Expanding)
        # black magic class patching
        # so calling these functions actually calls the self.bar's functions.
        self.minimum = self.bar.minimum
        self.maximum = self.bar.maximum

    def buildWidget(self):
        layout = QHBoxLayout(self)
        layout.setContentsMargins(0,0,0,0)
        SettingMenu = QMenu()
        exitButton = QAction(QIcon('exit24.png'), 'Set As High', self)
        exitButton.triggered.connect(self.setHigh)
        SettingMenu.addAction(exitButton)
        setlowButton = QAction(QIcon('exit24.png'), 'Set As Low', self)
        setlowButton.triggered.connect(self.setLow)
        SettingMenu.addAction(setlowButton)

        self.tb_down = QToolButton()
        self.tb_down.pressed.connect(self.on_click_down)
        self.tb_down.released.connect(self.on_released)
        self.tb_down.setArrowType(Qt.LeftArrow)

        self.tb_up = QToolButton()
        self.tb_up.pressed.connect(self.on_click_up)
        self.tb_up.released.connect(self.on_released)
        self.tb_up.setArrowType(Qt.RightArrow)

        if self.showToggleButton:
            tb_set = QToolButton()
            tb_set.clicked.connect(self.on_click_set_value)
            tb_set.setText('<>')
            if self.showSettingMenu:
                tb_set.setMenu(SettingMenu)
                tb_set.setPopupMode(QToolButton.DelayedPopup)

        layout.addWidget(self.tb_down)
        layout.addWidget(self.bar)
        layout.addWidget(self.tb_up)
        if self.showToggleButton:
            layout.addWidget(tb_set)
        layout.setSpacing(0)

    def on_click_up(self):
        self.timer.start(self.timer_value, self)
        self.value += self.step
        self._setValue()

    def on_click_down(self):
        self.timer.start(self.timer_value, self)
        self.value -= self.step
        self._setValue()
    def on_released(self):
        self.timer.stop()

    def on_click_set_value(self):
        if self.value == self.hi_value:
            self.value = self.low_value
        else:
            self.value = self.hi_value
        self._setValue()

    def timerEvent(self, e):
        if self.value < self.maximum() and self.value > self.minimum():
            if self.tb_down.isDown():
                self.value -= self.step
            else:
                self.value += self.step
            self._setValue()
        else:
            self.timer.stop()

    def _setValue(self):
        if self.value < self.minimum():self. value = self.minimum()
        if self.value > self.maximum(): self.value = self.maximum()
        self.valueChanged.emit(self.value)

    def setValue(self, value):
        if value < self.minimum(): value = self.minimum()
        if value > self.maximum(): value = self.maximum()
        self.value = int(value)
        tmpl = lambda s: self.texttemplate % s
        try:
            self.bar.text = tmpl(int(value))
        except:
            self.bar.text = self.texttemplate
        self.bar.setValue(int(value))
        self.bar.update()

    def setHigh(self):
        self.hi_value = self.value

    def setLow(self):
        self.low_value = self.value

    def setMaximum(self, data):
        if data < self.hi_value:
            self.hi_value = data
        self.bar.setMaximum(data)

    def setMinimum(self, data):
        if data > self.low_value:
            self.low_value = data
        self.bar.setMinimum(data)

class StatusAdjustmentBar(HAdjustmentBar, _HalWidgetBase):
    def __init__(self, parent=None):
        super(StatusAdjustmentBar, self).__init__(parent)
        self._block_signal = False
        self.rapid = True
        self.feed = False
        self.spindle = False
        self.jograte = False
        self.jograte_angular = False
        self.texttemplate = 'Value =  %s'

    # self.PREFS_
    # self.HAL_NAME_
    # comes from base class
    def _hal_init(self):

        # if we build the widget before now, designer options are not
        # taken account
        self.buildWidget()

        # when estopped disable buttons
        STATUS.connect('state-estop', lambda w: self.setEnabled(False))
        STATUS.connect('state-estop-reset', lambda w: self.setEnabled(True))

        # set options
        if self.rapid:
            STATUS.connect('rapid-override-changed', lambda w, data: self.setValue(data))
        elif self.feed:
            STATUS.connect('feed-override-changed', lambda w, data: self.setValue(data))
            self.setMaximum(int(INFO.MAX_FEED_OVERRIDE))
        elif self.spindle:
            STATUS.connect('spindle-override-changed', lambda w, data: self.setValue(data))
            self.setMaximum(int(INFO.MAX_SPINDLE_OVERRIDE))
            self.setMinimum(int(INFO.MIN_SPINDLE_OVERRIDE))
        elif self.jograte:
            STATUS.connect('jograte-changed', lambda w, data: self.setValue(data))
            self.setMaximum(int(INFO.MAX_LINEAR_JOG_VEL))
        elif self.jograte_angular:
            STATUS.connect('jograte-angular-changed', lambda w, data: self.setValue(data))
            print int(INFO.MAX_ANGULAR_JOG_VEL)
            self.setMaximum(int(INFO.MAX_ANGULAR_JOG_VEL))
        else:
           LOG.error('{} : no option recognised'.format(self.HAL_NAME_))

        # If there is a preference file object use it to load the hi/low toggle points
        if self.PREFS_:
            self.hi_value = self.PREFS_.getpref(self.HAL_NAME_+'-hi-value', 75, int, 'SCREEN_CONTROL_LAST_SETTING')
            self.low_value = self.PREFS_.getpref(self.HAL_NAME_+'-low-value', 25, int, 'SCREEN_CONTROL_LAST_SETTING')

        # connect a signal and callback function to the button
        self.valueChanged.connect(self._action)

    # when qtvcp closes this gets called
    def closing_cleanup__(self):
        if self.PREFS_:
            LOG.debug('Saving {} data to file.'.format(self.HAL_NAME_))
            self.PREFS_.putpref(self.HAL_NAME_+'-hi-value', self.hi_value, int, 'SCREEN_CONTROL_LAST_SETTING')
            self.PREFS_.putpref(self.HAL_NAME_+'-low-value', self.low_value, int, 'SCREEN_CONTROL_LAST_SETTING')

    def _action(self, value):
        if self.rapid:
            ACTION.SET_RAPID_RATE(value)
        elif self.feed:
            ACTION.SET_FEED_RATE(value)
        elif self.spindle:
            ACTION.SET_SPINDLE_RATE(value)
        elif self.jograte:
            ACTION.SET_JOG_RATE(value)
        elif self.jograte_angular:
            ACTION.SET_JOG_RATE_ANGULAR(value)
        else:
           LOG.error('{} no action recognised'.format(self.HAL_NAME_))
    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    #
    # _toggle_properties makes it so we can only select one option
    ########################################################################

    def _toggle_properties(self, picked):
        data = ('rapid', 'feed', 'spindle', 'jograte', 'jograte_angular')

        for i in data:
            if not i == picked:
                self[i+'_rate'] = False

    def setrapid(self, data):
        self.rapid = data
        if data:
            self._toggle_properties('rapid')
    def getrapid(self):
        return self.rapid
    def resetrapid(self):
        self.rapid = False

    def setfeed(self, data):
        self.feed = data
        if data:
            self._toggle_properties('feed')
    def getfeed(self):
        return self.feed
    def resetfeed(self):
        self.feed = False

    def setspindle(self, data):
        self.spindle = data
        if data:
            self._toggle_properties('spindle')
    def getspindle(self):
        return self.spindle
    def resetspindle(self):
        self.spindle = False

    def setjograte(self, data):
        self.jograte = data
        if data:
            self._toggle_properties('jograte')
    def getjograte(self):
        return self.jograte
    def resetjograte(self):
        self.jograte = False

    def setjograteangular(self, data):
        self.jograte_angular = data
        if data:
            self._toggle_properties('jograte_angular')
    def getjograteangular(self):
        return self.jograte_angular
    def resetjograteangular(self):
        self.jograte_angular = False

    def setshowtoggle(self, data):
        self.showToggleButton = data
    def getshowtoggle(self):
        return self.showToggleButton
    def resetshowtoggle(self):
        self.showToggleButton = True

    def setsettingmenu(self, data):
        self.showSettingMenu = data
    def getsettingmenu(self):
        return self.showSettingMenu
    def resetsettingmenu(self):
        self.showSettingMenu = True

    # Designer makes '\n' into '\\n' if added
    # we put fix it here so we actually get the newline
    def settexttemplate(self, data):
        data = data.replace('\\n' , '\n')
        self.texttemplate = data
    def gettexttemplate(self):
        return self.texttemplate
    def resettexttemplate(self):
        self.texttemplate = 'Value = %s'

    rapid_rate = pyqtProperty(bool, getrapid, setrapid, resetrapid)
    feed_rate = pyqtProperty(bool, getfeed, setfeed, resetfeed)
    spindle_rate = pyqtProperty(bool, getspindle, setspindle, resetspindle)
    jograte_rate = pyqtProperty(bool, getjograte, setjograte, resetjograte)
    jograte_angular_rate = pyqtProperty(bool, getjograteangular, setjograteangular, resetjograteangular)
    show_toggle_button = pyqtProperty(bool, getshowtoggle, setshowtoggle, resetshowtoggle)
    show_setting_menu = pyqtProperty(bool, getsettingmenu, setsettingmenu, resetsettingmenu)
    text_template = pyqtProperty(str, gettexttemplate, settexttemplate, resettexttemplate)

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

# for testing without editor:
def main():
    import sys
    from PyQt5.QtWidgets import QApplication
    app = QApplication(sys.argv)
    widget = StatusAdjustmentBar()
    widget.PREFS_=None
    widget.buildWidget()
    widget.settexttemplate('Text %s')
    widget.setValue(75)
    widget.show()

    sys.exit(app.exec_())
if __name__ == "__main__":
    main()
