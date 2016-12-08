#!/usr/bin/env python

from PyQt4 import QtCore, QtGui
from PyQt4.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.drowidget import Lcnc_DROLabel
from qtvcp.widgets.mdi_line import Lcnc_MDILine
from qtvcp.widgets.gcode_widget import GcodeEditor
from qtvcp.widgets.gstat_stacked import GstatStacked
from qtvcp.widgets.origin_offsetview import Lcnc_OriginOffsetView

from qtvcp.widgets.qtvcp_icons import Icon
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
        return "qtvcp.widgets.drowidget"

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
        return "qtvcp.widgets.mdi_line"

####################################
# Gcode editor
####################################
class GcodeEditorPlugin(QPyDesignerCustomWidgetPlugin):
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
        return GcodeEditor(parent)
    def name(self):
        return "GcodeEditor"
    def group(self):
        return "Linuxcnc - Controller"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('gcodeeditor')))
    def toolTip(self):
        return "Gcode display / editor Widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return True
    def domXml(self):
        return '<widget class="GcodeEditor" name="gcodeeditor" />\n'
    def includeFile(self):
        return "qtvcp.widgets.gcode_widget"

####################################
# GstatStacked
####################################
class GstatStackedPlugin(QPyDesignerCustomWidgetPlugin):
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
        return GstatStacked(parent)
    def name(self):
        return "GstatStacked"
    def group(self):
        return "Linuxcnc - Controller"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('gstatstacked')))
    def toolTip(self):
        return ""
    def whatsThis(self):
        return ""
    def isContainer(self):
        return True
    def domXml(self):
        return '<widget class="GstatStacked" name="gstatstacked" />\n'
    def includeFile(self):
        return "qtvcp.widgets.gstat_stacked"

####################################
# OriginOffsetView Widget
####################################
class Lcnc_OriginOffsetViewPlugin(QPyDesignerCustomWidgetPlugin):
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
        return Lcnc_OriginOffsetView(parent)
    def name(self):
        return "Lcnc_OriginOffsetView"
    def group(self):
        return "Linuxcnc - Controller"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('lcnc_originoffsetview')))
    def toolTip(self):
        return "Gcode display / editor Widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return True
    def domXml(self):
        return '<widget class="Lcnc_OriginOffsetView" name="lcnc_originoffsetview" />\n'
    def includeFile(self):
        return "qtvcp.widgets.origin_offsetview"

