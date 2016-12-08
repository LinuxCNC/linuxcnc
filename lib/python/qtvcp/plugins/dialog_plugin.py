#!/usr/bin/python3

from PyQt4.QtGui import QIcon, QPixmap
from PyQt4.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.dialog_widget import Lcnc_Dialog, Lcnc_ToolDialog, Lcnc_FileDialog
from qtvcp.widgets.qtvcp_icons import Icon
ICON = Icon()
class DialogPlugin(QPyDesignerCustomWidgetPlugin):

    def __init__(self, parent=None):
        super(DialogPlugin, self).__init__(parent)

        self.initialized = False

    def initialize(self, core):
        if self.initialized:
            return

        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return Lcnc_Dialog(parent)

    def name(self):
        return "Lcnc_Dialog"

    def group(self):
        return "Linuxcnc - Controller"

    def icon(self):
        return QIcon(QPixmap(ICON.get_path('lcnc_dialog')))

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
        return '<widget class="Lcnc_Dialog" name="lcnc_dialog" />\n'

    def includeFile(self):
        return "qtvcp.widgets.dialog_widget"

###############################################################################
# manual Tool Change Dialog
###############################################################################
class ToolDialogPlugin(QPyDesignerCustomWidgetPlugin):

    def __init__(self, parent=None):
        super(ToolDialogPlugin, self).__init__(parent)

        self.initialized = False

    def initialize(self, core):
        if self.initialized:
            return

        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return Lcnc_ToolDialog(parent)

    def name(self):
        return "Lcnc_ToolDialog"

    def group(self):
        return "Linuxcnc - HAL"

    def icon(self):
        return QIcon(QPixmap(ICON.get_path('lcnc_tooldialog')))

    def toolTip(self):
        return "Manual Tool Change Prompt Widgets"

    def whatsThis(self):
        return ""

    def isContainer(self):
        return False

    # Returns an XML description of a custom widget instance that describes
    # default values for its properties. Each custom widget created by this
    # plugin will be configured using this description.
    def domXml(self):
        return '<widget class="Lcnc_ToolDialog" name="lcnc_tooldialog" />\n'

    def includeFile(self):
        return "qtvcp.widgets.dialog_widget"

###############################################################################
# File Dialog
###############################################################################
class FileDialogPlugin(QPyDesignerCustomWidgetPlugin):

    def __init__(self, parent=None):
        super(FileDialogPlugin, self).__init__(parent)

        self.initialized = False

    def initialize(self, core):
        if self.initialized:
            return

        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return Lcnc_FileDialog(parent)

    def name(self):
        return "Lcnc_FileDialog"

    def group(self):
        return "Linuxcnc - HAL"

    def icon(self):
        return QIcon(QPixmap(ICON.get_path('lcnc_filedialog')))

    def toolTip(self):
        return "Manual Tool Change Prompt Widgets"

    def whatsThis(self):
        return ""

    def isContainer(self):
        return False

    # Returns an XML description of a custom widget instance that describes
    # default values for its properties. Each custom widget created by this
    # plugin will be configured using this description.
    def domXml(self):
        return '<widget class="Lcnc_FileDialog" name="lcnc_filedialog" />\n'

    def includeFile(self):
        return "qtvcp.widgets.dialog_widget"
