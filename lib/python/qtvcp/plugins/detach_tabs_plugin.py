#!/usr/bin/env python3

from PyQt5 import QtGui
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.detach_tabs import DetachTabWidget
from qtvcp.widgets.qtvcp_icons import Icon

ICON = Icon()


####################################
# Linuxcnc State Enable GridLayout
####################################
class DetachTabWidgetPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent=None):
        super(DetachTabWidgetPlugin, self).__init__(parent)
        self.initialized = False

    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return DetachTabWidget(parent)

    def name(self):
        return "DetachTabWidget"

    def group(self):
        return "Linuxcnc - HAL"

    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('detachtabwidget')))

    def toolTip(self):
        return "Tans can be detached from main widget"

    def whatsThis(self):
        return ""

    def isContainer(self):
        return True

    def domXml(self):
        return '<widget class="DetachTabWidget" name="dtabwidget" />\n'

    def includeFile(self):
        return "qtvcp.widgets.detach_tabs"
