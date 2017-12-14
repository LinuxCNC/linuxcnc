#!/usr/bin/env python

from PyQt5 import QtCore, QtGui
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp.widgets.simple_widgets import Lcnc_PushButton
from qtvcp.widgets.simple_widgets import Lcnc_CheckBox
from qtvcp.widgets.simple_widgets import Lcnc_RadioButton
from qtvcp.widgets.simple_widgets import Lcnc_LCDNumber
from qtvcp.widgets.simple_widgets import Lcnc_QSlider
from qtvcp.widgets.simple_widgets import Lcnc_GridLayout
from qtvcp.widgets.qtvcp_icons import Icon
ICON = Icon()

####################################
# PUSHBUTTON
####################################
class PushButtonPlugin(QPyDesignerCustomWidgetPlugin):

    # The __init__() method is only used to set up the plugin and define its
    # initialized variable.
    def __init__(self, parent = None):

        QPyDesignerCustomWidgetPlugin.__init__(self)
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
        return Lcnc_PushButton(parent)

    # This method returns the name of the custom widget class
    def name(self):
        return "Lcnc_PushButton"

    # Returns the name of the group in Qt Designer's widget box
    def group(self):
        return "Linuxcnc - HAL"

    # Returns the icon
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('lcnc_pushbutton')))

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
        return '<widget class="Lcnc_PushButton" name="lcnc_pushbutton" />\n'

    # Returns the module containing the custom widget class. It may include
    # a module path.
    def includeFile(self):
        return "qtvcp.widgets.simple_widgets"

####################################
# CHECKBUTTON
####################################
class CheckBoxPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        QPyDesignerCustomWidgetPlugin.__init__(self)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return Lcnc_CheckBox(parent)
    def name(self):
        return "Lcnc_CheckBox"
    def group(self):
        return "Linuxcnc - HAL"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('lcnc_checkbox')))
    def toolTip(self):
        return "HAL Checkbox widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return False
    def domXml(self):
        return '<widget class="Lcnc_CheckBox" name="lcnc_checkbox" />\n'
    def includeFile(self):
        return "qtvcp.widgets.simple_widgets"

####################################
# RADIOBUTTON
####################################
class RadioButtonPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        QPyDesignerCustomWidgetPlugin.__init__(self)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return Lcnc_RadioButton(parent)
    def name(self):
        return "Lcnc_RadioButton"
    def group(self):
        return "Linuxcnc - HAL"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('lcnc_radiobutton')))
    def toolTip(self):
        return "HAL Radiobutton widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return False
    def domXml(self):
        return '<widget class="Lcnc_RadioButton" name="lcnc_radiobutton" />\n'
    def includeFile(self):
        return "qtvcp.widgets.simple_widgets"
####################################
# LCD Display
####################################
class LCDNumberPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        QPyDesignerCustomWidgetPlugin.__init__(self)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return Lcnc_LCDNumber(parent)
    def name(self):
        return "Lcnc_LCDNumber"
    def group(self):
        return "Linuxcnc - HAL"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('Lcnc_LCDNumber')))
    def toolTip(self):
        return "HAL LCD Display widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return False
    def domXml(self):
        return '<widget class="Lcnc_LCDNumber" name="lcnc_lcdnumber" />\n'
    def includeFile(self):
        return "qtvcp.widgets.simple_widgets"

####################################
# Slider
####################################
class QSliderPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        QPyDesignerCustomWidgetPlugin.__init__(self)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return Lcnc_QSlider(parent)
    def name(self):
        return "Lcnc_QSlider"
    def group(self):
        return "Linuxcnc - HAL"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('Lcnc_QSlider')))
    def toolTip(self):
        return "HAL Slider widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return False
    def domXml(self):
        return("""
                <widget class="Lcnc_QSlider" name="lcnc_qslider">
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
        QPyDesignerCustomWidgetPlugin.__init__(self)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return Lcnc_GridLayout(parent)
    def name(self):
        return "Lcnc_GridLayout"
    def group(self):
        return "Linuxcnc - HAL"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('lcnc_gridlayout')))
    def toolTip(self):
        return "HAL enable/disable GridLayout widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return True
    def domXml(self):
        return '<widget class="Lcnc_GridLayout" name="lcnc_gridlayout" />\n'
    def includeFile(self):
        return "qtvcp.widgets.simple_widgets"
