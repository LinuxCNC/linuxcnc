#!/usr/bin/python2.7
# qtvcp
#
# Copyright (c) 2018  Chris Morley <chrisinnanaimo@hotmail.com>
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

import os
import sys

from PyQt5.QtCore import pyqtSignal, pyqtProperty, QVariant
from PyQt5.QtWidgets import QLabel
from PyQt5.QtGui import QPixmap

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status
from qtvcp import logger

# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# LOG is for running code logging
STATUS = Status()
LOG = logger.getLogger(__name__)

# Set the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

class ImageSwitcher(QLabel, _HalWidgetBase):
    widgetChanged = pyqtSignal(int)

    def __init__(self, parent=None):
        super(ImageSwitcher, self).__init__(parent)
        self.IMAGEDIR = os.path.join(os.environ['EMC2_HOME'], "share","qtvcp","images")
        self._imagePath = [os.path.join(self.IMAGEDIR,'applet-critical.png')]
        self._current_number = 0
        self.show_image_by_number(0)

    # Show the widgets based on a reference number
    def show_image_by_number(self, number):
        if number <0 or number > len(self._imagePath)-1:
            LOG.debug('Path reference number out of range: {}'.format(number))
            return
        #print 'requested:',number,self._imagePath[number]
        path = os.path.expanduser(self._imagePath[number])
        if not os.path.exists(path):
            LOG.debug('No Path: {}'.format(path))
        pixmap = QPixmap(path)
        self.setPixmap(pixmap)

    def set_image_number(self, data):
        self._current_number = data
        self.show_image_by_number(data)
    def get_image_number(self):
        return self._current_number
    def reset_image_number(self):
        self._current_number = 0
    image_number = pyqtProperty(int, get_image_number, set_image_number, reset_image_number)

    def set_image_l(self, data):
        self._imagePath = data
    def get_image_l(self):
        return self._imagePath
    def reset_image_l(self):
        self._imagePath = [os.path.join(self.IMAGEDIR,'applet-critical.png')]
    image_list = pyqtProperty(QVariant.typeToName(QVariant.StringList), get_image_l, set_image_l, reset_image_l)

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)



class StatusImageSwitcher(ImageSwitcher):

    def __init__(self, parent=None):
        super(StatusImageSwitcher, self).__init__(parent)
        self._imagePath = [os.path.join(self.IMAGEDIR,'applet-critical.png'),
                    os.path.join(self.IMAGEDIR,'spindle_ccw.gif'),
                    os.path.join(self.IMAGEDIR,'spindle_cw.gif')]
        self.spindle = True
        self.all_homed = False

    def _hal_init(self):
        if self.spindle:
            STATUS.connect('spindle-control-changed', lambda w, b, d: self.switch_on_spindle(b,d))
        elif self.all_homed:
            STATUS.connect('not-all-homed', lambda w, data: self.switch_on_homed(0))
            STATUS.connect('all-homed', lambda w: self.switch_on_homed(1))

    def switch_on_spindle(self, b, data):
        if data <0: data= 2
        self.set_image_number(data)

    def switch_on_homed(self, data):
        self.set_image_number(data)

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    #
    # _toggle_properties makes it so we can only select one option
    ########################################################################

    def _toggle_properties(self, picked):
        data = ('spindle','all_homed' )

        for i in data:
            if not i == picked:
                self['watch_'+i] = False

# property getter/setters

    # machine_spindle status
    def set_spindle(self, data):
        self.spindle = data
        if data:
            self._toggle_properties('spindle')
    def get_spindle(self):
        return self.spindle
    def reset_spindle(self):
        self.spindle = False
    watch_spindle = pyqtProperty(bool, get_spindle, set_spindle, reset_spindle)

    # machine_homed status
    def set_homed(self, data):
        self.all_homed = data
        if data:
            self._toggle_properties('all_homed')
    def get_homed(self):
        return self.all_homed
    def reset_homed(self):
        self.all_homed = False
    watch_all_homed = pyqtProperty(bool, get_homed, set_homed, reset_homed)

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
    widget = ImageSwitcher()
    widget._hal_init()

    widget.show()
    sys.exit(app.exec_())
if __name__ == "__main__":
    main()

