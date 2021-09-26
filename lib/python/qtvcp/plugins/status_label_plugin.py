#!/usr/bin/env python3

import sip
from PyQt5 import QtCore, QtGui, QtWidgets
from PyQt5.QtGui import QIcon, QPixmap, QTextFormat
from PyQt5.QtWidgets import QDialog, QLabel
from PyQt5.QtCore import pyqtProperty, QVariant
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin, QExtensionFactory, QPyDesignerTaskMenuExtension, \
    QPyDesignerPropertySheetExtension, QDesignerFormWindowInterface

from qtvcp.widgets.richtext_selector import RichTextEditorDialog
from qtvcp.widgets.status_label import StatusLabel
from qtvcp.widgets.qtvcp_icons import Icon

ICON = Icon()

Q_TYPEID = {
    'QDesignerContainerExtension': 'org.qt-project.Qt.Designer.Container',
    'QDesignerPropertySheetExtension': 'org.qt-project.Qt.Designer.PropertySheet',
    'QDesignerTaskMenuExtension': 'org.qt-project.Qt.Designer.TaskMenu',
    'QDesignerMemberSheetExtension': 'org.qt-project.Qt.Designer.MemberSheet'
}


class StatusLabelPlugin(QPyDesignerCustomWidgetPlugin):

    def __init__(self, parent=None):
        super(StatusLabelPlugin, self).__init__(parent)

        self.initialized = False

    def initialize(self, formEditor):
        if self.initialized:
            return
        manager = formEditor.extensionManager()
        if manager:
            self.factory = StatusLabelTaskMenuFactory(manager)
            manager.registerExtensions(self.factory, Q_TYPEID['QDesignerTaskMenuExtension'])
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


# *************************************************************************
class GstatLabelPropertySheetExtension(QExtensionFactory):
    def __init__(self, parent=None):

        QExtensionFactory.__init__(self, parent)
        # print 'extension',parent

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
        print(self.formWindow)
        self.propertylist = ['objectName', 'geometry', 'text']
        self.temp_flag = True
        # print dir(self.widget.pyqtConfigure.__sizeof__)
        # print self.widget.pyqtConfigure.__sizeof__()
        for i in StatusLabel.__dict__:
            # print i
            if 'PyQt5.QtCore.pyqtProperty' in str(StatusLabel.__dict__[i]):
                self.propertylist.append(i)
                print(i)
        # print dir(self.widget)

    def count(self):
        return len(self.propertylist)

    def property(self, index):
        name = self.propertyName(index)
        print('property index:', index, name)
        if 'object' in name:
            return QVariant('default')
        if 'orient' in name:
            return QVariant(False)
        if 'text' == name or 'alt' in name:
            return QVariant(self.widget.text)
        return QVariant(self.widget[str(name)])

    def indexOf(self, name):
        # print 'NAME:',name
        for num, i in enumerate(self.propertylist):
            if i == name: return num
        self.propertylist.append(name)
        print('not found:', name, num + 1)
        return num + 1

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
        print('SET property', prop, index, value)
        if 'objectName' in prop:
            self.widget.setObjectName(value)
        if 'geometry' in prop:
            self.widget.setGeometry(value)
        if prop == 'text':
            self.widget.setText(value)
        if 'Status' in prop:
            self.do_alt_text_test(prop, index, value)
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
            print('flag:', value)
            self.temp_flag = value


class StatusLabelMenuEntry(QPyDesignerTaskMenuExtension):

    def __init__(self, widget, parent):
        super(QPyDesignerTaskMenuExtension, self).__init__(parent)
        self.widget = widget
        self.editStateAction = QtWidgets.QAction(
            self.tr("Set Actions"), self)
        self.editStateAction.triggered.connect(self.updateOptions)

    def preferredEditAction(self):
        return self.editStateAction

    def taskActions(self):
        return [self.editStateAction]

    def updateOptions(self):
        dialog = StatusLabelDialog(self.widget)
        dialog.exec_()


class StatusLabelTaskMenuFactory(QExtensionFactory):
    def __init__(self, parent=None):
        QExtensionFactory.__init__(self, parent)

    def createExtension(self, obj, iid, parent):

        if not isinstance(obj, StatusLabel):
            return None
        if iid == Q_TYPEID['QDesignerTaskMenuExtension']:
            return StatusLabelMenuEntry(obj, parent)
        elif iid == Q_TYPEID['QDesignerMemberSheetExtension']:
            return StatusLabelMemberSheet(obj, parent)
        return None


class TreeComboBox(QtWidgets.QComboBox):
    def __init__(self, parent=None):
        super(TreeComboBox, self).__init__(parent)

        self.__skip_next_hide = False

        tree_view = QtWidgets.QTreeView(self)
        tree_view.setFrameShape(QtWidgets.QFrame.NoFrame)
        tree_view.setEditTriggers(tree_view.NoEditTriggers)
        tree_view.setAlternatingRowColors(True)
        tree_view.setSelectionBehavior(tree_view.SelectRows)
        tree_view.setWordWrap(True)
        tree_view.setAllColumnsShowFocus(True)
        self.setView(tree_view)

        self.view().viewport().installEventFilter(self)

    def showPopup(self):
        self.setRootModelIndex(QtCore.QModelIndex())
        super(TreeComboBox, self).showPopup()

    def hidePopup(self):
        self.setRootModelIndex(self.view().currentIndex().parent())
        self.setCurrentIndex(self.view().currentIndex().row())
        if self.__skip_next_hide:
            self.__skip_next_hide = False
        else:
            super(TreeComboBox, self).hidePopup()

    def selectIndex(self, index):
        self.setRootModelIndex(index.parent())
        self.setCurrentIndex(index.row())

    def eventFilter(self, object, event):
        if event.type() == QtCore.QEvent.MouseButtonPress and object is self.view().viewport():
            index = self.view().indexAt(event.pos())
            # print index.parent(),index.row(),index.column(),index.data(),index.data(QtCore.Qt.UserRole + 1)
            # print self.view().isExpanded(self.view().currentIndex())
            # if self.itemAt(event.pos()) is None
            self.__skip_next_hide = not self.view().visualRect(index).contains(event.pos())
        return False

    def addItems(self, parent, elements):
        for text, value, children in elements:
            item = QtGui.QStandardItem(text)
            item.setData(value[0], role=QtCore.Qt.UserRole + 1)
            item.setData(value[1], role=QtCore.Qt.UserRole + 2)
            parent.appendRow(item)
            if children:
                self.addItems(item, children)

    def select(self, nodeNum, row):
        # print 'select:', nodeNum, row
        # select last row
        newIndex = self.model().index(nodeNum, 0)
        self.setRootModelIndex(newIndex)
        self.setCurrentIndex(row)


class StatusLabelDialog(QtWidgets.QDialog):
    def __init__(self, widget, parent=None):
        QtWidgets.QDialog.__init__(self, parent)

        self.setWindowTitle(self.tr("Set Options"))
        self.setGeometry(300, 300, 200, 200)
        self.setSizePolicy(QtWidgets.QSizePolicy.Minimum, QtWidgets.QSizePolicy.Minimum)
        self.widget = widget
        self.previewWidget = StatusLabel()

        tab = QtWidgets.QTabWidget()
        self.tab1 = QtWidgets.QWidget()
        self.tab2 = QtWidgets.QWidget()

        tab.addTab(self.tab1, 'Actions')
        self.buildtab1()

        vbox = QtWidgets.QVBoxLayout()
        vbox.addWidget(tab)

        wid = QtWidgets.QWidget()
        vbox2 = QtWidgets.QVBoxLayout(wid)
        label = QtWidgets.QLabel('Default Designer Text')
        self.defaultTextTemplateEditBox = QtWidgets.QLineEdit()
        self.defaultTextTemplateEditBox.setText(self.widget.text())
        vbox2.addWidget(label)
        vbox2.addWidget(self.defaultTextTemplateEditBox)
        vbox.addWidget(wid)

        # Dialog control buttons
        buttonBox = QtWidgets.QDialogButtonBox()
        okButton = buttonBox.addButton(buttonBox.Ok)
        cancelButton = buttonBox.addButton(buttonBox.Cancel)
        okButton.clicked.connect(self.updateWidget)
        cancelButton.clicked.connect(self.reject)
        vbox.addWidget(buttonBox, 1)
        self.setLayout(vbox)

    def buildtab1(self):
        self.combo = TreeComboBox()
        model = QtGui.QStandardItemModel()
        model.setHeaderData(0, QtCore.Qt.Horizontal, 'Name', QtCore.Qt.DisplayRole)

        # (('Displayed name',['widget property name', code to show related data],[])
        node_1 = (('Set Feed Override', ['feed_override', 2], []),
                  ('Rapid Override', ['rapid_override', 2], []),
                  ('Spindle Override', ['spindle_override', 2], []),
                  ('Max Velocity Override', ['max_velocity_override', 2], []))
        node_2 = (('Jog Rate', ['jograte', 6], []),
                  ('Jog Rate Angular', ['jograte_angular', 2], []),
                  ('Jog Increment', ['jogincr', 6], []),
                  ('Jog Increment Angular', ['jogincr_angular', 2], []))
        node_3 = (('Spindle Rate Requested', ['requested_spindle_speed', 6], []),
                  ('Spindle Rate Actual', ['actual_spindle_speed', 6], []))
        node_4 = (('Current Feed Rate', ['current_feedrate', 6], []),
                  ('Current Feed Unit', ['current_feedunit', 6], []))
        node_5 = (('Tool Number', ['tool_number', 2], []),
                  ('Tool Diameter', ['tool_diameter', 6], []),
                  ('Tool Offset', ['tool_offset', 3], []),
                  ('Tool Comment', ['tool_comment', 2], []))
        node_6 = (('Active G Codes', ['gcodes', 2], []),
                  ('Active M Codes', ['mcodes', 2], []),
                  ('Active G5X System', ['user_system', 2], []))
        node_7 = (('File Name', ['filename', 2], []),
                  ('File Path', ['filepath', 2], []))
        node_8 = (('Machine State', ['machine_state', 8], []),)
        node_9 = (('Time', ['time_stamp', 2], []),
                    ('HAL Pin Status', ['halpin', 22], []))
        node_none = (('Unused', ['unused', 2], []),)

        parent_node = [('Unset', [None, None], node_none),
                       ('OVERRIDES', [None, None], node_1),
                       ('JOGGING', [None, None], node_2),
                       ('SPINDLE', [None, None], node_3),
                       ('FEED', [None, None], node_4),
                       ('TOOL', [None, None], node_5),
                       ('ACTIVE CODE', [None, None], node_6),
                       ('FILE', [None, None], node_7),
                       ('MACHINE STATE', [None, None], node_8),
                       ('MISC', [None, None], node_9)]

        self.combo.addItems(model, parent_node)
        self.combo.setModel(model)
        # self.combo.view().hideColumn (1)
        # self.combo.resize(300, 30)
        self.combo.currentIndexChanged.connect(self.selectionChanged)

        layout = QtWidgets.QVBoxLayout()
        layout.addWidget(self.combo)

        # related data selection - note the name 'self.ud' uses a number
        # that doubles each time - binary coded. this is so we can specify
        # multiple selectors  code 1 give ud1, code 2 gives ud2, code 3 gives
        # ud1 and ud2 etc

        # Tool offset Index number selection
        self.ud1 = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('Index number')
        self.JNumSpinBox = QtWidgets.QSpinBox()
        self.JNumSpinBox.setRange(-1, 8)
        self.JNumSpinBox.setValue(self.widget._index)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.JNumSpinBox)
        self.ud1.setLayout(hbox)
        layout.addWidget(self.ud1)
        self.ud1.hide()

        # text template
        self.ud2 = QtWidgets.QWidget()
        vbox2 = QtWidgets.QVBoxLayout(self.ud2)
        label = QtWidgets.QLabel('Imperial/Angular Text Template')
        vbox2.addWidget(label)

        hbox = QtWidgets.QHBoxLayout()
        self.textTemplateEditBox = QtWidgets.QLineEdit()
        self.textTemplateEditBox.setText(self.widget._textTemplate)
        hbox.addWidget(self.textTemplateEditBox)
        dialogButton = QtWidgets.QPushButton('Edit')
        dialogButton.clicked.connect(lambda: self.launchDialog(self.textTemplateEditBox))
        hbox.addWidget(dialogButton)

        vbox2.addLayout(hbox)
        layout.addWidget(self.ud2)

        # metric text template
        self.ud4 = QtWidgets.QWidget()
        vbox4 = QtWidgets.QVBoxLayout(self.ud4)
        label = QtWidgets.QLabel('Metric Text Template')
        vbox4.addWidget(label)

        hbox = QtWidgets.QHBoxLayout()
        self.altTextTemplateEditBox = QtWidgets.QLineEdit()
        self.altTextTemplateEditBox.setText(self.widget._alt_textTemplate)
        hbox.addWidget(self.altTextTemplateEditBox)

        dialogButton = QtWidgets.QPushButton('Edit')
        dialogButton.clicked.connect(lambda: self.launchDialog(self.altTextTemplateEditBox))
        hbox.addWidget(dialogButton)

        vbox4.addLayout(hbox)
        layout.addWidget(self.ud4)
        self.ud4.hide()

        self.ud8 = QtWidgets.QWidget()
        vbox8 = QtWidgets.QVBoxLayout(self.ud8)
        label = QtWidgets.QLabel('Please edit state_label_list \nproperty directly')
        vbox8.addWidget(label)
        layout.addWidget(self.ud8)
        self.ud8.hide()

        self.ud16 = QtWidgets.QWidget()
        vbox2 = QtWidgets.QVBoxLayout(self.ud16)
        label = QtWidgets.QLabel('HalPin Name')
        vbox2.addWidget(label)

        hbox = QtWidgets.QHBoxLayout()
        self.halpinEditBox = QtWidgets.QLineEdit()
        self.halpinEditBox.setText(self.widget._halpin_name)
        hbox.addWidget(self.halpinEditBox)

        vbox2.addLayout(hbox)
        layout.addWidget(self.ud16)


        self.tab1.setLayout(layout)

        # set combo to currently set property
        # widget[cdata[1][0]] will tell us the widgets property state
        # eg widget.feed_override etc
        flag = False
        # print parent_node
        for pnum, pdata in enumerate(parent_node):
            # print pnum,pdata[0]
            if pnum == 0: continue
            if pdata[0]:
                for cnum, cdata in enumerate(pdata[2]):
                    # print 'child,state:',cdata[1][0],self.widget[cdata[1][0]]
                    if self.widget[cdata[1][0]]:
                        flag = True
                        # print 'found index:',pnum,cnum
                        break
            if flag:
                break
        if flag:
            self.combo.select(pnum, cnum)
        else:
            self.combo.select(0, 0)

    def launchDialog(self, widget):
        d = RichTextEditorDialog()
        style = d.showDialog(pretext=widget.text())
        if style:
            self.defaultTextTemplateEditBox.setText(style)
            widget.setText(style)

    def selectionChanged(self, i):
        winPropertyName = self.combo.itemData(i, role=QtCore.Qt.UserRole + 1)
        userDataCode = self.combo.itemData(i, role=QtCore.Qt.UserRole + 2)
        # print 'selected property,related data code:',winPropertyName,userDataCode,i
        if winPropertyName is None:
            # collapsed = self.combo.view().isExpanded(self.combo.view().currentIndex())
            # if collapsed: return True
            # self.combo.select(0,0)
            return True
        # hide/show user data widgets
        if not userDataCode is None:
            for i in (1, 2, 4, 8, 16):
                widg = self['ud%s' % i]
                if userDataCode & i:
                    widg.show()
                else:
                    widg.hide()
        self.adjustSize()

    def updateWidget(self):
        i = self.combo.currentIndex()
        winProperty = self.combo.itemData(i, role=QtCore.Qt.UserRole + 1)
        if winProperty is None:
            self.combo.select(0, 0)
            return
        formWindow = QDesignerFormWindowInterface.findFormWindow(self.widget)

        if formWindow and winProperty == 'unused':
            formWindow.cursor().setProperty('feed_override_status',
                                            QtCore.QVariant(True))
            formWindow.cursor().setProperty('feed_override_status',
                                            QtCore.QVariant(False))
        elif formWindow:
            # set widget option
            formWindow.cursor().setProperty(winProperty + '_status',
                                            QtCore.QVariant(True))

        # set related data
        formWindow.cursor().setProperty('index_number',
                                        QtCore.QVariant(self.JNumSpinBox.value()))
        # block signal so button text doesn't change when selecting action
        self.widget._designer_block_signal = True
        formWindow.cursor().setProperty('textTemplate',
                                        QtCore.QVariant(self.textTemplateEditBox.text()))
        formWindow.cursor().setProperty('alt_textTemplate',
                                        QtCore.QVariant(self.altTextTemplateEditBox.text()))
        formWindow.cursor().setProperty('text',
                                        QtCore.QVariant(self.defaultTextTemplateEditBox.text()))
        formWindow.cursor().setProperty('halpin_name',
                                        QtCore.QVariant(self.halpinEditBox.text()))
        self.widget._designer_block_signal = False

        self.accept()

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)
