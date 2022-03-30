#!/usr/bin/env python3

from PyQt5.QtGui import QIcon, QPixmap
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.joypad import JoyPad, HALPad
from qtvcp.widgets.qtvcp_icons import Icon

ICON = Icon()

####################################
# HAL Joypad widget
####################################
class JoyPadPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent=None):
        super(JoyPadPlugin, self).__init__(parent)
        self.initialized = False

    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return JoyPad(parent)

    def name(self):
        return "JoyPad"

    def group(self):
        return "Linuxcnc - Widgets"

    def icon(self):
        return QIcon(QPixmap(ICON.get_path('JoyPad')))

    def toolTip(self):
        return "JoyPad Selection Widget"

    def whatsThis(self):
        return ""

    def isContainer(self):
        return False

    def domXml(self):
        return '<widget class="JoyPad" name="joypad" />\n'

    def includeFile(self):
        return "qtvcp.widgets.joypad"

####################################
# HAL Joypad widget
####################################
class HALPadPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent=None):
        super(HALPadPlugin, self).__init__(parent)
        self.initialized = False

    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return HALPad(parent)

    def name(self):
        return "HALPad"

    def group(self):
        return "Linuxcnc - HAL"

    def icon(self):
        return QIcon(QPixmap(ICON.get_path('HALPad')))

    def toolTip(self):
        return "HAL JoyPad Selection Widget"

    def whatsThis(self):
        return ""

    def isContainer(self):
        return False

    def domXml(self):
        return '<widget class="HALPad" name="halpad" />\n'

    def includeFile(self):
        return "qtvcp.widgets.joypad"
