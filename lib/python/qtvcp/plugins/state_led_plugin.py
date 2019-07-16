#!/usr/bin/python3

from PyQt5.QtGui import QIcon, QPixmap
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.state_led import StateLED
from qtvcp.widgets.qtvcp_icons import Icon
ICON = Icon()
class StateLEDPlugin(QPyDesignerCustomWidgetPlugin):

    def __init__(self, parent=None):
        super(StateLEDPlugin, self).__init__(parent)

        self.initialized = False

    def initialize(self, core):
        if self.initialized:
            return

        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return StateLED(parent)

    def name(self):
        return "StateLED"

    def group(self):
        return "Linuxcnc - Controller"

    def icon(self):
        return QIcon(QPixmap(ICON.get_path('led')))

    def toolTip(self):
        return ""

    def whatsThis(self):
        return ""

    def isContainer(self):
        return False

    # Returns an XML description of a custom widget instance that describes
    # default values for its properties. Each custom widget created by this
    # plugin will be configured using this description.
    def domXml(self):
        return '<widget class="StateLED" name="stateled" />\n'

    def includeFile(self):
        return "qtvcp.widgets.state_led"
