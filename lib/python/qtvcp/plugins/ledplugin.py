#!/usr/bin/python3

from PyQt4.QtGui import QIcon, QPixmap
from PyQt4.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.ledwidget import Lcnc_Led
from qtvcp.widgets.qtvcp_icons import Icon
ICON = Icon()

class LedPlugin(QPyDesignerCustomWidgetPlugin):

    def __init__(self, parent=None):
        super(LedPlugin, self).__init__(parent)

        self.initialized = False

    def initialize(self, core):
        if self.initialized:
            return

        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return Lcnc_Led(parent)

    def name(self):
        return "Lcnc_Led"

    def group(self):
        return "Linuxcnc - HAL"

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
        return '<widget class="Lcnc_Led" name="lcnc_led" />\n'

    def includeFile(self):
        return "qtvcp.widgets.ledwidget"
