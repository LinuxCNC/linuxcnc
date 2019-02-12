#!/usr/bin/python3

from PyQt5.QtGui import QIcon, QPixmap
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.dialog_widget import *
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
        return LcncDialog(parent)

    def name(self):
        return "LcncDialog"

    def group(self):
        return "Linuxcnc - Dialogs"

    def icon(self):
        return QIcon(QPixmap(ICON.get_path('lcncdialog')))

    def toolTip(self):
        return "Basic Message Dialog"

    def whatsThis(self):
        return ""

    def isContainer(self):
        return False

    # Returns an XML description of a custom widget instance that describes
    # default values for its properties. Each custom widget created by this
    # plugin will be configured using this description.
    def domXml(self):
        return '<widget class="LcncDialog" name="lcncdialog" />\n'

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
        return ToolDialog(parent)

    def name(self):
        return "ToolDialog"

    def group(self):
        return "Linuxcnc - Dialogs"

    def icon(self):
        return QIcon(QPixmap(ICON.get_path('tooldialog')))

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
        return '<widget class="ToolDialog" name="tooldialog" />\n'

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
        return FileDialog(parent)

    def name(self):
        return "FileDialog"

    def group(self):
        return "Linuxcnc - Dialogs"

    def icon(self):
        return QIcon(QPixmap(ICON.get_path('iledialog')))

    def toolTip(self):
        return "Gcode File Selection Dialog"

    def whatsThis(self):
        return ""

    def isContainer(self):
        return False

    # Returns an XML description of a custom widget instance that describes
    # default values for its properties. Each custom widget created by this
    # plugin will be configured using this description.
    def domXml(self):
        return '<widget class="FileDialog" name="filedialog" />\n'

    def includeFile(self):
        return "qtvcp.widgets.dialog_widget"

###############################################################################
# Cam Dialog
###############################################################################
class CamViewDialogPlugin(QPyDesignerCustomWidgetPlugin):

    def __init__(self, parent=None):
        super(CamViewDialogPlugin, self).__init__(parent)

        self.initialized = False

    def initialize(self, core):
        if self.initialized:
            return

        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return CamViewDialog(parent)

    def name(self):
        return "CamViewDialog"

    def group(self):
        return "Linuxcnc - Dialogs"

    def icon(self):
        return QIcon(QPixmap(ICON.get_path('camviewdialog')))

    def toolTip(self):
        return "Web Cam View alignment Dialog"

    def whatsThis(self):
        return ""

    def isContainer(self):
        return False

    # Returns an XML description of a custom widget instance that describes
    # default values for its properties. Each custom widget created by this
    # plugin will be configured using this description.
    def domXml(self):
        return '<widget class="CamViewDialog" name="camviewdialog" />\n'

    def includeFile(self):
        return "qtvcp.widgets.dialog_widget"

###############################################################################
# MacroTab Dialog
###############################################################################
class MacroTabDialogPlugin(QPyDesignerCustomWidgetPlugin):

    def __init__(self, parent=None):
        super(MacroTabDialogPlugin, self).__init__(parent)

        self.initialized = False

    def initialize(self, core):
        if self.initialized:
            return

        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return MacroTabDialog(parent)

    def name(self):
        return "MacroTabDialog"

    def group(self):
        return "Linuxcnc - Dialogs"

    def icon(self):
        return QIcon(QPixmap(ICON.get_path('macrotabdialog')))

    def toolTip(self):
        return "Macro program Selection Dialog"

    def whatsThis(self):
        return "Uses it to select short convience subroutines"

    def isContainer(self):
        return False

    # Returns an XML description of a custom widget instance that describes
    # default values for its properties. Each custom widget created by this
    # plugin will be configured using this description.
    def domXml(self):
        return '<widget class="MacroTabDialog" name="_macrotabdialog" />\n'

    def includeFile(self):
        return "qtvcp.widgets.dialog_widget"

###############################################################################
# OriginOffset Dialog
###############################################################################
class OriginOffsetDialogPlugin(QPyDesignerCustomWidgetPlugin):

    def __init__(self, parent=None):
        super(OriginOffsetDialogPlugin, self).__init__(parent)

        self.initialized = False

    def initialize(self, core):
        if self.initialized:
            return

        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return OriginOffsetDialog(parent)

    def name(self):
        return "OriginOffsetDialog"

    def group(self):
        return "Linuxcnc - Dialogs"

    def icon(self):
        return QIcon(QPixmap(ICON.get_path('originoffsetdialog')))

    def toolTip(self):
        return "Orgin Offset Editting Dialog"

    def whatsThis(self):
        return ""

    def isContainer(self):
        return False

    # Returns an XML description of a custom widget instance that describes
    # default values for its properties. Each custom widget created by this
    # plugin will be configured using this description.
    def domXml(self):
        return '<widget class="OriginOffsetDialog" name="originoffsetdialog" />\n'

    def includeFile(self):
        return "qtvcp.widgets.dialog_widget"

###############################################################################
# ToolOffset Dialog
###############################################################################
class ToolOffsetDialogPlugin(QPyDesignerCustomWidgetPlugin):

    def __init__(self, parent=None):
        super(ToolOffsetDialogPlugin, self).__init__(parent)

        self.initialized = False

    def initialize(self, core):
        if self.initialized:
            return

        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return ToolOffsetDialog(parent)

    def name(self):
        return "ToolOffsetDialog"

    def group(self):
        return "Linuxcnc - Dialogs"

    def icon(self):
        return QIcon(QPixmap(ICON.get_path('Tooloffsetdialog')))

    def toolTip(self):
        return "Tool Offset Editting Dialog"

    def whatsThis(self):
        return ""

    def isContainer(self):
        return False

    # Returns an XML description of a custom widget instance that describes
    # default values for its properties. Each custom widget created by this
    # plugin will be configured using this description.
    def domXml(self):
        return '<widget class="ToolOffsetDialog" name="tooloffsetdialog" />\n'

    def includeFile(self):
        return "qtvcp.widgets.dialog_widget"

###############################################################################
# VersaProbe Dialog
###############################################################################
class VersaProbeDialogPlugin(QPyDesignerCustomWidgetPlugin):

    def __init__(self, parent=None):
        super(VersaProbeDialogPlugin, self).__init__(parent)

        self.initialized = False

    def initialize(self, core):
        if self.initialized:
            return

        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return VersaProbeDialog(parent)

    def name(self):
        return "VersaProbeDialog"

    def group(self):
        return "Linuxcnc - Dialogs"

    def icon(self):
        return QIcon(QPixmap(ICON.get_path('versaprobedialog')))

    def toolTip(self):
        return "Probe Dialog Widget"

    def whatsThis(self):
        return ""

    def isContainer(self):
        return False

    # Returns an XML description of a custom widget instance that describes
    # default values for its properties. Each custom widget created by this
    # plugin will be configured using this description.
    def domXml(self):
        return '<widget class="VersaProbeDialog" name="versaprobedialog" />\n'

    def includeFile(self):
        return "qtvcp.widgets.dialog_widget"

###############################################################################
# Entry Dialog
###############################################################################
class EntryDialogPlugin(QPyDesignerCustomWidgetPlugin):

    def __init__(self, parent=None):
        super(EntryDialogPlugin, self).__init__(parent)
        self.initialized = False

    def initialize(self, core):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return EntryDialog(parent)
    def name(self):
        return "EntryDialog"
    def group(self):
        return "Linuxcnc - Dialogs"
    def icon(self):
        return QIcon(QPixmap(ICON.get_path('entrydialog')))
    def toolTip(self):
        return "Basic Entry Dialog"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return False
    # Returns an XML description of a custom widget instance that describes
    # default values for its properties. Each custom widget created by this
    # plugin will be configured using this description.
    def domXml(self):
        return '<widget class="EntryDialog" name="entrydialog" />\n'
    def includeFile(self):
        return "qtvcp.widgets.dialog_widget"

