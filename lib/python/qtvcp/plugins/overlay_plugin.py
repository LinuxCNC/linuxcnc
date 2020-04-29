#!/usr/bin/python3

from PyQt5.QtGui import QIcon, QPixmap
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.overlay_widget import FocusOverlay
from qtvcp.widgets.qtvcp_icons import Icon
ICON = Icon()

class FocusOverlayPlugin(QPyDesignerCustomWidgetPlugin):

    def __init__(self, parent=None):
        super(FocusOverlayPlugin, self).__init__(parent)

        self.initialized = False

    def initialize(self, core):
        if self.initialized:
            return

        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return FocusOverlay(parent)

    def name(self):
        return "FocusOverlay"

    def group(self):
        return "Linuxcnc - HAL"

    def icon(self):
        return QIcon(QPixmap(ICON.get_path('focusoverlay')))

    def toolTip(self):
        return ""

    def whatsThis(self):
        return ""

    def isContainer(self):
        return True

    # Returns an XML description of a custom widget instance that describes
    # default values for its properties. Each custom widget created by this
    # plugin will be configured using this description.
    def domXml(self):
        return '<widget class="FocusOverlay" name="focusoverlay" />\n'

    def includeFile(self):
        return "qtvcp.widgets.overlay_widget"
