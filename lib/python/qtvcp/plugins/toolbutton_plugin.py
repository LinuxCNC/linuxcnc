#!/usr/bin/env python3

from PyQt5 import QtCore, QtGui
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.system_tool_button import SystemToolButton
from qtvcp.widgets.action_tool_button import ActionToolButton
from qtvcp.widgets.qtvcp_icons import Icon

ICON = Icon()


####################################
# SystemToolButton
####################################
class SystemToolButtonPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent=None):
        super(SystemToolButtonPlugin, self).__init__(parent)
        self.initialized = False

    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return SystemToolButton(parent)

    def name(self):
        return "SystemToolButton"

    def group(self):
        return "Linuxcnc - Controller"

    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('systemtoolbutton')))

    def toolTip(self):
        return "Button for selecting a User Coordinate System"

    def whatsThis(self):
        return ""

    def isContainer(self):
        return False

    def domXml(self):
        return '<widget class="SystemToolButton" name="systemtoolbutton" />\n'

    def includeFile(self):
        return "qtvcp.widgets.system_tool_button"

####################################
# ActionToolButton
####################################
class ActionToolButtonPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent=None):
        super(ActionToolButtonPlugin, self).__init__(parent)
        self.initialized = False

    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return ActionToolButton(parent)

    def name(self):
        return "ActionToolButton"

    def group(self):
        return "Linuxcnc - Controller"

    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('actiontoolbutton')))

    def toolTip(self):
        return "Tool Button for selecting selectable Actions"

    def whatsThis(self):
        return ""

    def isContainer(self):
        return False

    def domXml(self):
        return '<widget class="ActionToolButton" name="actiontoolbutton" />\n'

    def includeFile(self):
        return "qtvcp.widgets.action_tool_button"
