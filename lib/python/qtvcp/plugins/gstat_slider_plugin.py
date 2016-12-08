#!/usr/bin/python3

from PyQt4.QtGui import QIcon, QPixmap
from PyQt4.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.gstat_slider import Gstat_Slider
from qtvcp.widgets.qtvcp_icons import Icon
ICON = Icon()

class GstatSliderPlugin(QPyDesignerCustomWidgetPlugin):

    def __init__(self, parent=None):
        super(GstatSliderPlugin, self).__init__(parent)

        self.initialized = False

    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return Gstat_Slider(parent)

    def name(self):
        return "Gstat_Slider"

    def group(self):
        return "Linuxcnc - Controller"

    def icon(self):
        return QIcon(QPixmap(ICON.get_path('gstat_slider')))

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
        return '<widget class="Gstat_Slider" name="gstat_slider" />\n'

    def includeFile(self):
        return "qtvcp.widgets.gstat_slider"


