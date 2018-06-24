#!/usr/bin/env python

from PyQt5 import QtCore, QtGui
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.dro_widget import DROLabel
from qtvcp.widgets.mdi_line import MDILine
from qtvcp.widgets.gcode_widget import GcodeEditor
from qtvcp.widgets.status_stacked import StatusStacked
from qtvcp.widgets.origin_offsetview import OriginOffsetView

from qtvcp.widgets.qtvcp_icons import Icon
ICON = Icon()

####################################
# DRO
####################################
class LcncDROLabelPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        super(LcncDROLabelPlugin, self).__init__(parent)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return DROLabel(parent)
    def name(self):
        return "DROLabel"
    def group(self):
        return "Linuxcnc - Controller"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('dro_label')))
    def toolTip(self):
        return "DRO Display Widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return True
    def domXml(self):
        return '<widget class="DROLabel" name="dro_label" />\n'
    def includeFile(self):
        return "qtvcp.widgets.dro_widget"

####################################
# MDI edit line
####################################
class MDILinePlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        super(MDILinePlugin, self).__init__(parent)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return MDILine(parent)
    def name(self):
        return "MDILine"
    def group(self):
        return "Linuxcnc - Controller"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('mdiline')))
    def toolTip(self):
        return "MDI edit line Widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return True
    def domXml(self):
        return '<widget class="MDILine" name="mdiline" />\n'
    def includeFile(self):
        return "qtvcp.widgets.mdi_line"

####################################
# Gcode editor
####################################
class GcodeEditorPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        super(GcodeEditorPlugin, self).__init__(parent)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return GcodeEditor(parent)
    def name(self):
        return "GcodeEditor"
    def group(self):
        return "Linuxcnc - Controller"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('gcode_editor')))
    def toolTip(self):
        return "Gcode display / editor Widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return True
    def domXml(self):
        return '<widget class="GcodeEditor" name="gcodee_ditor" />\n'
    def includeFile(self):
        return "qtvcp.widgets.gcode_editor"

####################################
# StatusStacked
####################################
class StatusStackedPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        super(StatusStackedPlugin, self).__init__(parent)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return StatusStacked(parent)
    def name(self):
        return "StatusStacked"
    def group(self):
        return "Linuxcnc - Controller"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('statusstacked')))
    def toolTip(self):
        return ""
    def whatsThis(self):
        return ""
    def isContainer(self):
        return True
    def domXml(self):
        return '<widget class="StatusStacked" name="statusstacked" />\n'
    def includeFile(self):
        return "qtvcp.widgets.status_stacked"

####################################
# OriginOffsetView Widget
####################################
class OriginOffsetViewPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        super(OriginOffsetViewPlugin, self).__init__(parent)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return OriginOffsetView(parent)
    def name(self):
        return "OriginOffsetView"
    def group(self):
        return "Linuxcnc - Controller"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('originoffsetview')))
    def toolTip(self):
        return "Gcode display / editor Widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return True
    def domXml(self):
        return '<widget class="OriginOffsetView" name="originoffsetview" />\n'
    def includeFile(self):
        return "qtvcp.widgets.origin_offsetview"

