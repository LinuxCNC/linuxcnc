#!/usr/bin/env python3

from PyQt5 import QtCore, QtGui, QtWidgets
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.virtualkeyboard import VirtualKeyboard
from qtvcp.widgets.qtvcp_icons import Icon

ICON = Icon()


####################################
# VirtualKeyboard
####################################
class VirtualKeyboardPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent=None):
        super(VirtualKeyboardPlugin, self).__init__(parent)
        self.initialized = False

    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return VirtualKeyboard(parent)

    def name(self):
        return "VirtualKeyboard"

    def group(self):
        return "Linuxcnc - Widgets"

    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('virtualkeyboard')))

    def toolTip(self):
        return "On Screen Virtual Keyboard Widget"

    def whatsThis(self):
        return ""

    def isContainer(self):
        return False

    def domXml(self):
        return '<widget class="VirtualKeyboard" name="virtualkeyboard" />\n'

    def includeFile(self):
        return "qtvcp.widgets.virtualkeyboard"
