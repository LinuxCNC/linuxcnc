#!/usr/bin/env python3

from PyQt5.QtGui import QIcon, QPixmap
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.round_progress import RoundProgressBar
from qtvcp.widgets.qtvcp_icons import Icon

ICON = Icon()


class RoundProgressBarPlugin(QPyDesignerCustomWidgetPlugin):

    def __init__(self, parent=None):
        super(RoundProgressBarPlugin, self).__init__(parent)

        self.initialized = False

    def initialize(self, core):
        if self.initialized:
            return

        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        w = RoundProgressBar(parent)
        w.setFormat('%v')
        w.setValue(70)
        w.valueFormatChanged()
        return w

    def name(self):
        return "RoundProgressBar"

    def group(self):
        return "Linuxcnc - HAL"

    def icon(self):
        return QIcon(QPixmap(ICON.get_path('roundprogressbar')))

    def toolTip(self):
        return ""

    def whatsThis(self):
        return ""

    def isContainer(self):
        return False

    # Returns an XML description of a custom widget instance that describes
    # default values for its properties. Each custom widget created by this
    # plugin will be configured using this description.
    def domXml(self):
        return '<widget class="RoundProgressBar" name="RoundProgressBar" />\n'

    def includeFile(self):
        return "qtvcp.widgets.round_progress"
