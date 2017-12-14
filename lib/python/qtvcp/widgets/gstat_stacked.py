#!/usr/bin/python2.7

from PyQt5 import QtCore, QtWidgets

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.qt_glib import GStat
GSTAT = GStat()

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
            GSTAT.connect('mode-auto', lambda w: _switch(2))
        if self.mdi:
            GSTAT.connect('mode-mdi', lambda w: _switch(1))
        if self.manual:
            GSTAT.connect('mode-manual', lambda w: _switch(0))

