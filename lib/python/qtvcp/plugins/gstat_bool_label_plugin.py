#!/usr/bin/python3

from PyQt5.QtGui import QIcon, QPixmap
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.gstat_bool_label import Lcnc_Gstat_Bool_Label
from qtvcp.widgets.qtvcp_icons import Icon
ICON = Icon()

class GstatBoolLabelPlugin(QPyDesignerCustomWidgetPlugin):

    def __init__(self, parent=None):
        super(GstatBoolLabelPlugin, self).__init__(parent)

        self.initialized = False

    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return Lcnc_Gstat_Bool_Label(parent)

    def name(self):
        return "Lcnc_Gstat_Bool_Label"

    def group(self):
        return "Linuxcnc - Controller"

    def icon(self):
        return QIcon(QPixmap(ICON.get_path('lcnc_gstat_bool_label')))

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
        return '<widget class="Lcnc_Gstat_Bool_Label" name="lcnc_gstat_bool_label" />\n'

    def includeFile(self):
        return "qtvcp.widgets.gstat_bool_label"


