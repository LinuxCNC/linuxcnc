#!/usr/bin/env python3

from PyQt5 import QtCore, QtGui, QtWidgets
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.basic_probe import BasicProbe
from qtvcp.widgets.qtvcp_icons import Icon

ICON = Icon()


####################################
# BasicProbe
####################################
class BasicProbePlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent=None):
        super(BasicProbePlugin, self).__init__(parent)
        self.initialized = False

    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return BasicProbe(parent)

    def name(self):
        return "BasicProbe"

    def group(self):
        return "Linuxcnc - Widgets"

    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('basicprobe')))

    def toolTip(self):
        return "Probe Screen Widget"

    def whatsThis(self):
        return ""

    def isContainer(self):
        return False

    def domXml(self):
        return '<widget class="BasicProbe" name="basicprobe" />\n'

    def includeFile(self):
        return "qtvcp.widgets.basic_probe"
