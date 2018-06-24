#!/usr/bin/python3

from PyQt5.QtGui import QIcon, QPixmap, QTextFormat
from PyQt5.QtWidgets import QDialog, QLabel
from PyQt5.QtCore import pyqtProperty,QVariant
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin, QExtensionFactory, QPyDesignerTaskMenuExtension, QPyDesignerPropertySheetExtension,QDesignerFormWindowInterface
from qtvcp.widgets.status_label import StatusLabel
from qtvcp.widgets.qtvcp_icons import Icon
ICON = Icon()

class StatusLabelPlugin(QPyDesignerCustomWidgetPlugin):

    def __init__(self, parent=None):
        super(StatusLabelPlugin, self).__init__(parent)

        self.initialized = False

    def initialize(self, formEditor):
        if self.initialized:
            return
        # add a custom property editor
#        manager = formEditor.extensionManager()
#        if manager:
#          self.factory = GstatLabelPropertySheetExtension(manager)
#          manager.registerExtensions(self.factory,"com.trolltech.Qt.Designer.PropertySheet")

        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return StatusLabel(parent)

    def name(self):
        return "StatusLabel"

    def group(self):
        return "Linuxcnc - Controller"

    def icon(self):
        return QIcon(QPixmap(ICON.get_path('statuslabel')))

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
        return '<widget class="StatusLabel" name="statuslabel" />\n'

    def includeFile(self):
        return "qtvcp.widgets.status_label"

#*************************************************************************
class GstatLabelPropertySheetExtension(QExtensionFactory):
  def __init__(self, parent = None):

      QExtensionFactory.__init__(self, parent)
      #print 'extension',parent

  def createExtension(self, obj, iid, parent):

      if iid != "com.trolltech.Qt.Designer.PropertySheet":
          return None

      if isinstance(obj, StatusLabel):
          return GstatLabelPropertySheet(obj, parent)

      return None

class GstatLabelPropertySheet(QPyDesignerPropertySheetExtension):

    def __init__(self, widget, parent):
        QPyDesignerPropertySheetExtension.__init__(self, parent)
        self.widget = widget
        self.formWindow = QDesignerFormWindowInterface.findFormWindow(self.widget)
        print self.formWindow
        self.propertylist=['objectName','geometry','text']
        self.temp_flag = True
        #print dir(self.widget.pyqtConfigure.__sizeof__)
        #print self.widget.pyqtConfigure.__sizeof__()
        for i in StatusLabel.__dict__:
            #print i
            if 'PyQt4.QtCore.pyqtProperty'  in str(StatusLabel.__dict__[i]):
                self.propertylist.append(i)
                print i
        #print dir(self.widget)

    def count(self):
        return len(self.propertylist)

    def property(self,index):
        name = self.propertyName(index)
        print 'property index:', index,name
        if 'object' in name:
            return QVariant('default')
        if 'orient' in name:
            return QVariant(False)
        if 'text' == name or 'alt' in name:
            return QVariant(self.widget.text)
        return QVariant(self.widget[str(name)])

    def indexOf(self,name):
        #print 'NAME:',name
        for num,i in enumerate(self.propertylist):
            if i == name: return num
        self.propertylist.append(name)
        print 'not found:',name, num+1
        return num +1

    def setChanged(self, index, value):
        return

    def isChanged(self, index):
        return False

    def hasReset(self, index):
        return True

    def isAttribute(self, index):
        return False

    def propertyGroup(self, index):
        name = self.propertyName(index)
        if 'geometry' in name:
            return 'QObject'
        if 'objectName' in name:
            return 'QWidget'
        if 'text' in name:
            return 'Text'
        return 'Bool'

    def setProperty(self, index, vvalue):
        prop = self.propertyName(index)
        value = vvalue.toPyObject()
        print 'SET property',prop,index,value
        if 'objectName' in prop:
            self.widget.setObjectName(value)
        if 'geometry' in prop:
            self.widget.setGeometry(value)
        if prop is 'text':
            self.widget.setText(value)
        if 'Status' in prop:
            self.do_alt_text_test(prop, index,value)
        self.widget[prop] = value

        return
        if self.formWindow:
            self.formWindow.cursor().setProperty(self.propertyName(index), QVariant(value))
        return

    def getVisible(self, index, data):
        pass

    def isVisible(self, index):
        prop = self.propertyName(index)
        if 'alt_text' in prop:
            return self.temp_flag
        return True

    def propertyName(self, index):
        return self.propertylist[index]

    def do_alt_text_test(self, prop, indexm, value):
        if 'jograte' in prop:
            print 'flag:',value
            self.temp_flag = value


