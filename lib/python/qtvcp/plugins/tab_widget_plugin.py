#!/usr/bin/env python3

from PyQt5 import QtCore, QtGui
from PyQt5.QtWidgets import QWidget
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin

from qtvcp.widgets.tab_widget import TabWidget
from qtvcp.widgets.qtvcp_icons import Icon

ICON = Icon()


####################################
# Tab Widget
####################################
class TabWidgetPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent=None):
        super(TabWidgetPlugin, self).__init__(parent)
        self.initialized = False

    def initialize(self, formEditor):
        if self.initialized:
            return
        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return TabWidget(parent)

    def name(self):
        return "TabWidget"

    def group(self):
        return "Linuxcnc - HAL"

    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('tabwidget')))

    def toolTip(self):
        return "TabWidget with adjustable Tab Height"

    def whatsThis(self):
        return ""

    def isContainer(self):
        return True

    def domXml(self):
        return ("""
                <widget class="TabWidget" name="tabwidget">
'                   <widget class="QWidget" name="tab" />'
'                   <widget class="QWidget" name="tab_2" />'
                    <property name="geometry">
                        <rect>
                            <x>0</x>
                            <y>0</y>
                            <width>139</width>
                            <height>100</height>
                        </rect>
                    </property>
                </widget>
                """)

    def includeFile(self):
        return "qtvcp.widgets.tab_widget"
