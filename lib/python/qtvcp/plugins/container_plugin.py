#!/usr/bin/env python

from PyQt5 import QtGui
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.container_widgets import StateEnableGridLayout
from qtvcp.widgets.qtvcp_icons import Icon
ICON = Icon()

####################################
# Linuxcnc State Enable GridLayout
####################################
class StateEnableGridLayoutPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        super(StateEnableGridLayoutPlugin, self).__init__(parent)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return StateEnableGridLayout(parent)
    def name(self):
        return "StateEnableGridLayout"
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
        return '<widget class="StateEnableGridLayout" name="state_enable_gridLayout" />\n'
    def includeFile(self):
        return "qtvcp.widgets.container_widgets"
