#!/usr/bin/env python

from PyQt4 import QtCore, QtGui
from PyQt4.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp_widgets.kled import Lcnc_KLed
from qtvcp_widgets.qtvcp_icons import Icon
ICON = Icon()

####################################
# KLed Widget
####################################
class KLedPlugin(QPyDesignerCustomWidgetPlugin):
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
        return Lcnc_KLed(parent)
    def name(self):
        return "Lcnc_KLed"
    def group(self):
        return "Linuxcnc"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('lcnc_kled')))
    def toolTip(self):
        return "HAL KLed widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return False
    def domXml(self):
        return '<widget class="Lcnc_KLed" name="lcnc_kled" />\n'
    def includeFile(self):
        return "qtvcp_widgets.simple_widgets"
