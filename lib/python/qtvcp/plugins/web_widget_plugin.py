#!/usr/bin/env python3

from PyQt5 import QtGui
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.web_widget import WebWidget
from qtvcp.widgets.qtvcp_icons import Icon

ICON = Icon()

class WebWidgetPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent=None):
        super(WebWidgetPlugin, self).__init__(parent)
        self.initialized = False

    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return WebWidget(parent)

    def name(self):
        return "WebWidget"

    def group(self):
        return "Linuxcnc - Widgets"

    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('webwidget')))

    def toolTip(self):
        return "HAL web viewing widget"

    def whatsThis(self):
        return ""

    def isContainer(self):
        return False

    def domXml(self):
        return '<widget class="WebWidget" name="webwidget" />\n'

    def includeFile(self):
        return "qtvcp.widgets.web_widget"

