#!/usr/bin/env python

from PyQt4 import QtCore, QtGui
from PyQt4.QtDesigner import QPyDesignerCustomWidgetPlugin, \
                QPyDesignerTaskMenuExtension, QExtensionFactory, \
                QDesignerFormWindowInterface

from qtvcp.widgets.screenoptions import Lcnc_ScreenOptions
from qtvcp.widgets.qtvcp_icons import Icon
ICON = Icon()

####################################
# ScreenOptions
####################################
class LcncScreenOptionsPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        QPyDesignerCustomWidgetPlugin.__init__(self)
        self.initialized = False
    def initialize(self, formEditor):
        if self.initialized:
            return
        manager = formEditor.extensionManager()
        if manager:
          self.factory = \
              GeoLocationTaskMenuFactory(manager)
          manager.registerExtensions(
              self.factory,
              "com.trolltech.Qt.Designer.TaskMenu")

        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return Lcnc_ScreenOptions(parent)
    def name(self):
        return "Lcnc_ScreenOptions"
    def group(self):
        return "Linuxcnc - Controller"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('lcnc_screenoptions')))
    def toolTip(self):
        return "ScreenOptions widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return True
    def domXml(self):
        return '<widget class="Lcnc_ScreenOptions" name="lcnc_screenoptions" />\n'
    def includeFile(self):
        return "qtvcp.widgets.screenoptions"

class GeoLocationDialog(QtGui.QDialog):

   def __init__(self, widget, parent = None):

      QtGui.QDialog.__init__(self, parent)

      self.widget = widget

      self.previewWidget = Lcnc_ScreenOptions()
      self.previewWidget.notify_option = widget.notify_option
      #self.previewWidget.longitude = widget.longitude

      buttonBox = QtGui.QDialogButtonBox()
      okButton = buttonBox.addButton(buttonBox.Ok)
      cancelButton = \
         buttonBox.addButton(buttonBox.Cancel)

      self.connect(okButton, QtCore.SIGNAL("clicked()"),
                   self.updateWidget)
      self.connect(cancelButton, QtCore.SIGNAL("clicked()"),
                   self, QtCore.SLOT("reject()"))

      layout = QtGui.QGridLayout()
      self.c_notify = QtGui.QCheckBox("Desktop Notify Errors")
      self.c_notify.setChecked(widget.desktop_notify )
      self.c_errors = QtGui.QCheckBox("Catch Errors")
      self.c_errors.setChecked(widget.catch_errors)
      self.c_close = QtGui.QCheckBox("Catch close Event")
      self.c_close.setChecked(widget.close_event)

      layout.addWidget(self.c_notify)
      layout.addWidget(self.c_errors)
      layout.addWidget(self.c_close)
      layout.addWidget(buttonBox, 3, 0, 1, 2)
      self.setLayout(layout)

      self.setWindowTitle(self.tr("Select Options"))

   def updateWidget(self):

      formWindow = \
        QDesignerFormWindowInterface.findFormWindow(
            self.widget)

      if formWindow:
          formWindow.cursor().setProperty("notify_option",
              QtCore.QVariant(self.c_notify.isChecked()))
          formWindow.cursor().setProperty("catch_errors_option",
              QtCore.QVariant(self.c_errors.isChecked()))
          formWindow.cursor().setProperty("catch_close_option",
              QtCore.QVariant(self.c_close.isChecked()))

      self.accept()

class GeoLocationMenuEntry(QPyDesignerTaskMenuExtension):

  def __init__(self, widget, parent):

      QPyDesignerTaskMenuExtension.__init__(self, parent)

      self.widget = widget
      self.editStateAction = QtGui.QAction(
          self.tr("Select Options..."), self)
      self.connect(self.editStateAction,
          QtCore.SIGNAL("triggered()"), self.updateLocation)

  def preferredEditAction(self):
      return self.editStateAction

  def taskActions(self):
      return [self.editStateAction]

  def updateLocation(self):
      dialog = GeoLocationDialog(self.widget)
      dialog.exec_()

class GeoLocationTaskMenuFactory(QExtensionFactory):

  def __init__(self, parent = None):

      QExtensionFactory.__init__(self, parent)

  def createExtension(self, obj, iid, parent):

      if iid != "com.trolltech.Qt.Designer.TaskMenu":
          return None

      if isinstance(obj, Lcnc_ScreenOptions):
          return GeoLocationMenuEntry(obj, parent)

      return None
