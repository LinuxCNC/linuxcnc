#!/usr/bin/env python

from PyQt5 import QtCore, QtGui, QtWidgets
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin, \
                QPyDesignerTaskMenuExtension, QExtensionFactory, \
                QDesignerFormWindowInterface

from qtvcp.widgets.screen_options import ScreenOptions
from qtvcp.widgets.qtvcp_icons import Icon
ICON = Icon()

####################################
# ScreenOptions
####################################
class LcncScreenOptionsPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent = None):
        super(LcncScreenOptionsPlugin, self).__init__(parent)
        self.initialized = False

    def initialize(self, formEditor):
        if self.initialized:
            return
        manager = formEditor.extensionManager()
        if manager:
          self.factory = \
              screenOptionsTaskMenuFactory(manager)
          manager.registerExtensions(
              self.factory,
              "org.qt-project.Qt.Designer.TaskMenu")
        self.initialized = True
    def isInitialized(self):
        return self.initialized
    def createWidget(self, parent):
        return ScreenOptions(parent)
    def name(self):
        return "ScreenOptions"
    def group(self):
        return "Linuxcnc - Controller"
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('screen_options')))
    def toolTip(self):
        return "ScreenOptions widget"
    def whatsThis(self):
        return ""
    def isContainer(self):
        return True
    def domXml(self):
        return '<widget class="ScreenOptions" name="screen_options" />\n'
    def includeFile(self):
        return "qtvcp.widgets.screen_options"

class screenOptionsDialog(QtWidgets.QDialog):

   def __init__(self, widget, parent = None):

      QtWidgets.QDialog.__init__(self, parent)

      self.widget = widget

      self.previewWidget = ScreenOptions()
      self.previewWidget.notify_option = widget.notify_option

      buttonBox = QtWidgets.QDialogButtonBox()
      okButton = buttonBox.addButton(buttonBox.Ok)
      cancelButton = \
         buttonBox.addButton(buttonBox.Cancel)

      okButton.clicked.connect(self.updateWidget)
      cancelButton.clicked.connect(self.reject)

      layout = QtWidgets.QGridLayout()
      self.c_notify = QtWidgets.QCheckBox("Desktop Notify Errors")
      self.c_notify.setChecked(widget.desktop_notify )
      self.c_errors = QtWidgets.QCheckBox("Catch Errors")
      self.c_errors.setChecked(widget.catch_errors)
      self.c_close = QtWidgets.QCheckBox("Catch close Event")
      self.c_close.setChecked(widget.close_event)
      self.c_play_sounds = QtWidgets.QCheckBox("Play Sounds")
      self.c_play_sounds.setChecked(widget.play_sounds)
      self.c_use_pref_file = QtWidgets.QCheckBox("Set up a Preference File")
      self.c_use_pref_file.setChecked(widget.use_pref_file)

      layout.addWidget(self.c_notify)
      layout.addWidget(self.c_errors)
      layout.addWidget(self.c_close)
      layout.addWidget(self.c_play_sounds)
      layout.addWidget(self.c_use_pref_file)
      layout.addWidget(buttonBox, 5, 0, 1, 2)
      self.setLayout(layout)

      self.setWindowTitle(self.tr("Set Options"))

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
          formWindow.cursor().setProperty("play_sounds_option",
              QtCore.QVariant(self.c_play_sounds.isChecked()))
          formWindow.cursor().setProperty("use_pref_file_option",
              QtCore.QVariant(self.c_use_pref_file.isChecked()))

      self.accept()

class screenOptionsMenuEntry(QPyDesignerTaskMenuExtension):

  def __init__(self, widget, parent):

      QPyDesignerTaskMenuExtension.__init__(self, parent)

      self.widget = widget
      self.editStateAction = QtWidgets.QAction(
          self.tr("Set Options..."), self)
      self.editStateAction.triggered.connect(self.updateLocation)

  def preferredEditAction(self):
      return self.editStateAction

  def taskActions(self):
      return [self.editStateAction]

  def updateLocation(self):
      dialog = screenOptionsDialog(self.widget)
      dialog.exec_()

class screenOptionsTaskMenuFactory(QExtensionFactory):

  def __init__(self, parent = None):
      QExtensionFactory.__init__(self, parent)

  def createExtension(self, obj, iid, parent):
      if iid != "org.qt-project.Qt.Designer.TaskMenu":
          return None

      if isinstance(obj, ScreenOptions):
          return screenOptionsMenuEntry(obj, parent)

      return None
