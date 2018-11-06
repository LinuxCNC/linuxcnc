#!/usr/bin/env python

from PyQt5 import QtCore, QtGui
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.simple_widgets import PushButton
from qtvcp.widgets.simple_widgets import CheckBox
from qtvcp.widgets.simple_widgets import RadioButton
from qtvcp.widgets.simple_widgets import LCDNumber
from qtvcp.widgets.simple_widgets import Slider
from qtvcp.widgets.simple_widgets import GridLayout
from qtvcp.widgets.general_hal_output import GeneralHALOutput
from qtvcp.widgets.general_hal_input import GeneralHALInput
from qtvcp.widgets.xembed import XEmbed
from qtvcp.widgets.radio_axis_selector import RadioAxisSelector
from qtvcp.widgets.axis_tool_button import AxisToolButton
from qtvcp.widgets.file_manager import FileManager
from qtvcp.widgets.qtvcp_icons import Icon
ICON = Icon()

####################################
# PUSHBUTTON
####################################
class PushButtonPlugin(QPyDesignerCustomWidgetPlugin):

    # The __init__() method is only used to set up the plugin and define its
    # initialized variable.
    def __init__(self, parent = None):
        super(PushButtonPlugin, self).__init__(parent)
        self.initialized = False

    # The initialize() and isInitialized() methods allow the plugin to set up
    # any required resources, ensuring that this can only happen once for each
    # plugin.
    def initialize(self, formEditor):

        if self.initialized:
            return

        self.initialized = True

    def isInitialized(self):
        return self.initialized

    # This factory method creates new instances of our custom widget
    def createWidget(self, parent):
        return PushButton(parent)

    # This method returns the name of the custom widget class
    def name(self):
        return "PushButton"

    # Returns the name of the group in Qt Designer's widget box
    def group(self):
        return "Linuxcnc - HAL"

    # Returns the icon
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('pushbutton')))

    # Returns a tool tip short description
    def toolTip(self):
        return "Push button widget"

    # Returns a short description of the custom widget for use in a "What's
    # This?" help message for the widget.
    def whatsThis(self):
        return ""

    # Returns True if the custom widget acts as a container for other widgets;
    def isContainer(self):
        return False

    # Returns an XML description of a custom widget instance that describes
    # default values for its properties.
    def domXml(self):
        return '<widget class="PushButton" name="pushbutton" />\n'

    # Returns the module containing the custom widget class. It may include
    # a module path.
    def includeFile(self):
        return "qtvcp.widgets.simple_widgets"

####################################
# CHECKBUTTON
####################################
class CheckBoxPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        super(CheckBoxPlugin, self).__init__(parent)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return CheckBox(parent)
    def name(self):
        return "CheckBox"
    def group(self):
        return "Linuxcnc - HAL"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('checkbox')))
    def toolTip(self):
        return "HAL Checkbox widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return False
    def domXml(self):
        return '<widget class="CheckBox" name="checkbox" />\n'
    def includeFile(self):
        return "qtvcp.widgets.simple_widgets"

####################################
# RADIOBUTTON
####################################
class RadioButtonPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        super(RadioButtonPlugin, self).__init__(parent)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return RadioButton(parent)
    def name(self):
        return "RadioButton"
    def group(self):
        return "Linuxcnc - HAL"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('radiobutton')))
    def toolTip(self):
        return "HAL Radiobutton widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return False
    def domXml(self):
        return '<widget class="RadioButton" name="radiobutton" />\n'
    def includeFile(self):
        return "qtvcp.widgets.simple_widgets"
####################################
# LCD Display
####################################
class LCDNumberPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        super(LCDNumberPlugin, self).__init__(parent)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return LCDNumber(parent)
    def name(self):
        return "LCDNumber"
    def group(self):
        return "Linuxcnc - HAL"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('LCDNumber')))
    def toolTip(self):
        return "HAL LCD Display widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return False
    def domXml(self):
        return '<widget class="LCDNumber" name="lcdnumber" />\n'
    def includeFile(self):
        return "qtvcp.widgets.simple_widgets"

####################################
# Slider
####################################
class SliderPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        super(SliderPlugin, self).__init__(parent)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return Slider(parent)
    def name(self):
        return "Slider"
    def group(self):
        return "Linuxcnc - HAL"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('slider')))
    def toolTip(self):
        return "HAL Slider widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return False
    def domXml(self):
        return("""
                <widget class="Slider" name="slider">
                    <property name=\"maximum\">
                        <number>100</number>
                    </property>
                    <property name=\"orientation\">
                        <enum>Qt::Horizontal</enum>
                    </property>
                </widget>
                """)
    def includeFile(self):
        return "qtvcp.widgets.simple_widgets"

####################################
# GridLayout
####################################
class LcncGridLayoutPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        super(LcncGridLayoutPlugin, self).__init__(parent)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return GridLayout(parent)
    def name(self):
        return "GridLayout"
    def group(self):
        return "Linuxcnc - HAL"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('gridlayout')))
    def toolTip(self):
        return "HAL enable/disable GridLayout widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return True
    def domXml(self):
        return '<widget class="GridLayout" name="gridlayout" />\n'
    def includeFile(self):
        return "qtvcp.widgets.simple_widgets"


####################################
# GeneralHALOutput
####################################
class GeneralHALOutputPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        super(GeneralHALOutputPlugin, self).__init__(parent)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return GeneralHALOutput(parent)
    def name(self):
        return "GeneralHALOutput"
    def group(self):
        return "Linuxcnc - HAL"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('generalhaloutput')))
    def toolTip(self):
        return "Generalized HAL Output Pin Widget"
    def whatsThis(self):
        return "Used to add HAl Pins to Arbritrary widgets"
    def isContainer(self):
        return True
    def domXml(self):
        return '<widget class="GeneralHALOutput" name="generalhaloutput" />\n'
    def includeFile(self):
        return "qtvcp.widgets.general_hal_output"


####################################
# GeneralHALInput
####################################
class GeneralHALInputPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        super(GeneralHALInputPlugin, self).__init__(parent)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return GeneralHALInput(parent)
    def name(self):
        return "GeneralHALInput"
    def group(self):
        return "Linuxcnc - HAL"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('generalhalinput')))
    def toolTip(self):
        return "Generalized HAL Input Pin Widget"
    def whatsThis(self):
        return "Used to add HAl Pins to Arbritrary widgets"
    def isContainer(self):
        return True
    def domXml(self):
        return '<widget class="GeneralHALInput" name="generalhalinput" />\n'
    def includeFile(self):
        return "qtvcp.widgets.general_hal_input"

####################################
# XEmbed
####################################
class XEmbedPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        super(XEmbedPlugin, self).__init__(parent)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return XEmbed(parent)
    def name(self):
        return "XEmbed"
    def group(self):
        return "Linuxcnc - HAL"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('xembed')))
    def toolTip(self):
        return "Widget for embedding thirdparty programs"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return True
    def domXml(self):
        return '<widget class="XEmbed" name="xembed" />\n'
    def includeFile(self):
        return "qtvcp.widgets.xembed"

####################################
# RadioAxisSelector
####################################
class RadioAxisSelectorPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        super(RadioAxisSelectorPlugin, self).__init__(parent)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return RadioAxisSelector(parent)
    def name(self):
        return "RadioAxisSelector"
    def group(self):
        return "Linuxcnc - Controller"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('radioaxisselector')))
    def toolTip(self):
        return "RadioAxisSelector widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return False
    def domXml(self):
        return '<widget class="RadioAxisSelector" name="radioaxisselector" />\n'
    def includeFile(self):
        return "qtvcp.widgets.radio_axis_selector"

####################################
# AxisToolButton
####################################
class AxisToolButtonPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        super(AxisToolButtonPlugin, self).__init__(parent)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return AxisToolButton(parent)
    def name(self):
        return "AxisToolButton"
    def group(self):
        return "Linuxcnc - Controller"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('axistoolbutton')))
    def toolTip(self):
        return "Button for selecting an Axis and setting the Origin"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return False
    def domXml(self):
        return '<widget class="AxisToolButton" name="axistoolbutton" />\n'
    def includeFile(self):
        return "qtvcp.widgets.axis_tool_button"

####################################
# FileManager
####################################
class FileManagerPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        super(FileManagerPlugin, self).__init__(parent)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return FileManager(parent)
    def name(self):
        return "FileManager"
    def group(self):
        return "Linuxcnc - Controller"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('filemanager')))
    def toolTip(self):
        return "Button for selecting an Axis and setting the Origin"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return False
    def domXml(self):
        return '<widget class="FileManager" name="filemanager" />\n'
    def includeFile(self):
        return "qtvcp.widgets.file_manager"
