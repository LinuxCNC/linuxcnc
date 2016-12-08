#!/usr/bin/env python

from PyQt4 import QtCore, QtGui
from PyQt4.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.container_widgets import State_Enable_GridLayout
from qtvcp.widgets.qtvcp_icons import Icon
ICON = Icon()

####################################
# Linuxcnc State Enable GridLayout
####################################
class StateEnableGridLayoutPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        QPyDesignerCustomWidgetPlugin.__init__(self)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return State_Enable_GridLayout(parent)
    def name(self):
        return "State_Enable_GridLayout"
    def group(self):
        return "Linuxcnc - Controller"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('state_enable_gridlayout')))
    def toolTip(self):
        return "Linuxcnc State enable/disable GridLayout widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return True
    def domXml(self):
        return '<widget class="State_Enable_GridLayout" name="state_enable_gridLayout" />\n'
    def includeFile(self):
        return "qtvcp.widgets.container_widgets"
