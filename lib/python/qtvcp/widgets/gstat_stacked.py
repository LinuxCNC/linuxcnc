#!/usr/bin/python2.7

from PyQt5 import QtCore, QtWidgets

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status
STATUS = Status()

# Set up logging
from qtvcp import logger
log = logger.getLogger(__name__)


class GstatStacked(QtWidgets.QStackedWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(GstatStacked, self).__init__(parent)
        self.auto = True
        self.mdi = True
        self.manual = True

    def _hal_init(self):
        def _switch( index):
            log.debug('index: {}'.format(index))
            self.setCurrentIndex(index)
        if self.auto:
            STATUS.connect('mode-auto', lambda w: _switch(2))
        if self.mdi:
            STATUS.connect('mode-mdi', lambda w: _switch(1))
        if self.manual:
            STATUS.connect('mode-manual', lambda w: _switch(0))

