#!/usr/bin/env python

from PyQt5 import QtCore, QtGui
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.gcode_graphics import  GCodeGraphics
from qtvcp.widgets.qtvcp_icons import Icon
ICON = Icon()

####################################
# GCodeGraphics
####################################
class  GCodeGraphicsPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        super(GCodeGraphicsPlugin, self).__init__(parent)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return  GCodeGraphics(parent)
    def name(self):
        return "GCodeGraphics"
    def group(self):
        return "Linuxcnc - Controller"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('gcodegraphics')))
    def toolTip(self):
        return "Graphics5 widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return True
    def domXml(self):
        return '<widget class="GCodeGraphics" name="gcodegraphics" />\n'
    def includeFile(self):
        return "qtvcp.widgets.gcode_graphics"
