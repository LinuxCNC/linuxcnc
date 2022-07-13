#!/usr/bin/env python3

from PyQt5.QtGui import QIcon, QPixmap
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.nurbs_editor import NurbsEditor
from qtvcp.widgets.qtvcp_icons import Icon

ICON = Icon()

####################################
# HAL NurbsEditor widget
####################################
class NurbsEditorPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent=None):
        super(NurbsEditorPlugin, self).__init__(parent)
        self.initialized = False

    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return NurbsEditor(parent)

    def name(self):
        return "NurbsEditor"

    def group(self):
        return "Linuxcnc - Widgets"

    def icon(self):
        return QIcon(QPixmap(ICON.get_path('NurbsEditor')))

    def toolTip(self):
        return "Nurbs Gcode Editor Widget"

    def whatsThis(self):
        return ""

    def isContainer(self):
        return False

    def domXml(self):
        return '<widget class="NurbsEditor" name="nurbseditor" />\n'

    def includeFile(self):
        return "qtvcp.widgets.nurbseditor"

