#!/usr/bin/env python3

from PyQt5 import QtGui
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.bar import HalBar

from qtvcp.widgets.qtvcp_icons import Icon

ICON = Icon()

####################################
# DRO
####################################
class HalBarPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent=None):
        super(HalBarPlugin, self).__init__(parent)
        self.initialized = False

    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return HalBar(parent)

    def name(self):
        return "HalBar"

    def group(self):
        return "Linuxcnc - HAL"

    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('halbar_label')))

    def toolTip(self):
        return "HAL Bar Widget"

    def whatsThis(self):
        return ""

    def isContainer(self):
        return False

    def domXml(self):
        return '<widget class="HalBar" name="hal_bar" />\n'

    def includeFile(self):
        return "qtvcp.widgets.bar"

