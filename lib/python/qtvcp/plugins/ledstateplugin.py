#!/usr/bin/python3

from PyQt5.QtGui import QIcon, QPixmap
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.led_state_widget import Lcnc_State_Led
from qtvcp.widgets.qtvcp_icons import Icon
ICON = Icon()
class LedStatePlugin(QPyDesignerCustomWidgetPlugin):

    def __init__(self, parent=None):
        super(LedStatePlugin, self).__init__(parent)

        self.initialized = False

    def initialize(self, core):
        if self.initialized:
            return

        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return Lcnc_State_Led(parent)

    def name(self):
        return "Lcnc_State_Led"

    def group(self):
        return "Linuxcnc - Controller"

    def icon(self):
        return QIcon(QPixmap(ICON.get_path('lcnc_led')))

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
        return '<widget class="Lcnc_State_Led" name="lcnc_state_led" />\n'

    def includeFile(self):
        return "qtvcp.widgets.led_state_widget"
