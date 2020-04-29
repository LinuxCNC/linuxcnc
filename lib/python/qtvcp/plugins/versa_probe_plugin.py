#!/usr/bin/env python

from PyQt5 import QtCore, QtGui, QtWidgets
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.versa_probe import VersaProbe
from qtvcp.widgets.qtvcp_icons import Icon
ICON = Icon()

####################################
# VersaProbe
####################################
class VersaProbePlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        super(VersaProbePlugin, self).__init__(parent)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return VersaProbe(parent)
    def name(self):
        return "VersaProbe"
    def group(self):
        return "Linuxcnc - Controller"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('versaprobe')))
    def toolTip(self):
        return "Probe Screen Widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return False
    def domXml(self):
        return '<widget class="VersaProbe" name="versaprobe" />\n'
    def includeFile(self):
        return "qtvcp.widgets.versa_probe"
