#!/usr/bin/env python

from PyQt5 import QtCore, QtGui
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.dro_widget import DROLabel
from qtvcp.widgets.mdi_line import MDILine
from qtvcp.widgets.mdi_history import MDIHistory
from qtvcp.widgets.gcode_editor import GcodeEditor, GcodeDisplay
from qtvcp.widgets.status_stacked import StatusStacked
from qtvcp.widgets.widget_switcher import WidgetSwitcher
from qtvcp.widgets.origin_offsetview import OriginOffsetView
from qtvcp.widgets.tool_offsetview import ToolOffsetView
from qtvcp.widgets.macro_widget import MacroTab

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
# MDI History widget
####################################
class MDIHistoryPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        super(MDIHistoryPlugin, self).__init__(parent)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return MDIHistory(parent)
    def name(self):
        return "MDIHistory"
    def group(self):
        return "Linuxcnc - Controller"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('mdihistory')))
    def toolTip(self):
        return "MDI History Widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return False
    def domXml(self):
        return '<widget class="MDIHistory" name="mdihistory" />\n'
    def includeFile(self):
        return "qtvcp.widgets.mdi_history"

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
        return '<widget class="GcodeEditor" name="gcode_editor" />\n'
    def includeFile(self):
        return "qtvcp.widgets.gcode_editor"

####################################
# Gcode display (read only)
####################################
class GcodeDisplayPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        super(GcodeDisplayPlugin, self).__init__(parent)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return GcodeDisplay(parent)
    def name(self):
        return "GcodeDisplay"
    def group(self):
        return "Linuxcnc - Controller"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('gcode_display')))
    def toolTip(self):
        return "Gcode display Widget (read-only)"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return True
    def domXml(self):
        return '<widget class="GcodeDisplay" name="gcode_display" />\n'
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
# WidgetSwitcher
####################################
class WidgetSwitcherPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        super(WidgetSwitcherPlugin, self).__init__(parent)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return WidgetSwitcher(parent)
    def name(self):
        return "WidgetSwitcher"
    def group(self):
        return "Linuxcnc - HAL"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('widgetswitcher')))
    def toolTip(self):
        return ""
    def whatsThis(self):
        return ""
    def isContainer(self):
        return True
    def domXml(self):
        return '''<widget class="WidgetSwitcher" name="widgetswitcher">
<property name="widget_list" stdset="0">
   <stringlist>
   </stringlist>
  </property>
</widget>'''
    def includeFile(self):
        return "qtvcp.widgets.widget_switcher"

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
        return "User Origin Offset Editor Widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return True
    def domXml(self):
        return '<widget class="OriginOffsetView" name="originoffsetview" />\n'
    def includeFile(self):
        return "qtvcp.widgets.origin_offsetview"

####################################
# ToolOffsetView Widget
####################################
class ToolOffsetViewPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        super(ToolOffsetViewPlugin, self).__init__(parent)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return ToolOffsetView(parent)
    def name(self):
        return "ToolOffsetView"
    def group(self):
        return "Linuxcnc - Controller"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('tooloffsetview')))
    def toolTip(self):
        return "Tool Offset Editor Widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return True
    def domXml(self):
        return '<widget class="ToolOffsetView" name="tooloffsetview" />\n'
    def includeFile(self):
        return "qtvcp.widgets.tool_offsetview"

####################################
# MacroTab Widget
####################################
class MacroTabPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        super(MacroTabPlugin, self).__init__(parent)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return MacroTab(parent)
    def name(self):
        return "MacroTab"
    def group(self):
        return "Linuxcnc - Controller"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('macrotab')))
    def toolTip(self):
        return "Macro Subroutine Selection Widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return True
    def domXml(self):
        return '<widget class="MacroTab" name="macrotab" />\n'
    def includeFile(self):
        return "qtvcp.widgets.macro_widget"
