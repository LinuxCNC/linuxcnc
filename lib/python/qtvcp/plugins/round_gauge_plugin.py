#!/usr/bin/env python3

from PyQt5.QtGui import QIcon, QPixmap
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.round_gauge import Gauge
from qtvcp.widgets.qtvcp_icons import Icon

ICON = Icon()


class GaugePlugin(QPyDesignerCustomWidgetPlugin):

    def __init__(self, parent=None):
        super(GaugePlugin, self).__init__(parent)
        self.initialized = False

    def initialize(self, core):
        if self.initialized:
            return
        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return Gauge(parent)

    def name(self):
        return "Gauge"

    def group(self):
        return "Linuxcnc - HAL"

    def icon(self):
        return QIcon(QPixmap(ICON.get_path('gauge')))

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
        return '<widget class="Gauge" name="gauge" />\n'

    def includeFile(self):
        return "qtvcp.widgets.round_gauge"
