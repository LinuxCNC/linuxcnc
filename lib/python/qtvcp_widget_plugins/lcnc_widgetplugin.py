#!/usr/bin/env python

from PyQt4 import QtCore, QtGui
from PyQt4.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp_widgets.drowidget import Lcnc_DROLabel
from qtvcp_widgets.mdi_line import Lcnc_MDILine

from qtvcp_widgets.qtvcp_icons import Icon
ICON = Icon()

####################################
# DRO
####################################
class LcncDROLabelPlugin(QPyDesignerCustomWidgetPlugin):
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
        return Lcnc_DROLabel(parent)
    def name(self):
        return "Lcnc_DROLabel"
    def group(self):
        return "Linuxcnc - Controller"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('lcnc_drolabel')))
    def toolTip(self):
        return "DRO Display Widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return True
    def domXml(self):
        return '<widget class="Lcnc_DROLabel" name="lcnc_drolabel" />\n'
    def includeFile(self):
        return "qtvcp_widgets.drowidget"

####################################
# MDI edit line
####################################
class Lcnc_MDIlinePlugin(QPyDesignerCustomWidgetPlugin):
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
        return Lcnc_MDILine(parent)
    def name(self):
        return "Lcnc_MDILine"
    def group(self):
        return "Linuxcnc - Controller"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('lcnc_mdiLine')))
    def toolTip(self):
        return "MDI edit line Widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return True
    def domXml(self):
        return '<widget class="Lcnc_MDILine" name="lcnc_mdiline" />\n'
    def includeFile(self):
        return "qtvcp_widgets.mdi_line"
