#!/usr/bin/env python3

from PyQt5.QtGui import QIcon, QPixmap
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.hal_label import HALLabel
from qtvcp.widgets.qtvcp_icons import Icon

ICON = Icon()


####################################
# Label Display
####################################
class HALLabelPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent=None):
        super(HALLabelPlugin, self).__init__(parent)
        self.initialized = False

    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return HALLabel(parent)

    def name(self):
        return "HALLabel"

    def group(self):
        return "Linuxcnc - HAL"

    def icon(self):
        return QIcon(QPixmap(ICON.get_path('hallabel')))

    def toolTip(self):
        return "HAL Pin Display widget"

    def whatsThis(self):
        return ""

    def isContainer(self):
        return False

    def domXml(self):
        return '<widget class="HALLabel" name="hallabel" />\n'

    def includeFile(self):
        return "qtvcp.widgets.hal_label"
