#!/usr/bin/env python3

import sip
from PyQt5 import QtCore, QtGui, QtWidgets
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin, \
    QPyDesignerTaskMenuExtension, QExtensionFactory, \
    QDesignerFormWindowInterface, QPyDesignerMemberSheetExtension
from qtvcp.widgets.action_button import ActionButton
from qtvcp.widgets.action_button_round import RoundButton
from qtvcp.widgets.qtvcp_icons import Icon
from qtvcp.widgets.richtext_selector import RichTextEditorDialog

ICON = Icon()

Q_TYPEID = {
    'QDesignerContainerExtension': 'org.qt-project.Qt.Designer.Container',
    'QDesignerPropertySheetExtension': 'org.qt-project.Qt.Designer.PropertySheet',
    'QDesignerTaskMenuExtension': 'org.qt-project.Qt.Designer.TaskMenu',
    'QDesignerMemberSheetExtension': 'org.qt-project.Qt.Designer.MemberSheet'
}


####################################
# ActionBUTTON
####################################
class ActionButtonPlugin(QPyDesignerCustomWidgetPlugin):

    # The __init__() method is only used to set up the plugin and define its
    # initialized variable.
    def __init__(self, parent=None):
        super(ActionButtonPlugin, self).__init__(parent)
        self.initialized = False

    # The initialize() and isInitialized() methods allow the plugin to set up
    # any required resources, ensuring that this can only happen once for each
    # plugin.
    def initialize(self, formEditor):

        if self.initialized:
            return
        manager = formEditor.extensionManager()
        if manager:
            self.factory = ActionButtonTaskMenuFactory(manager)
            manager.registerExtensions(self.factory, Q_TYPEID['QDesignerTaskMenuExtension'])
        self.initialized = True

    def isInitialized(self):
        return self.initialized

    # This factory method creates new instances of our custom widget
    def createWidget(self, parent):
        a = ActionButton(parent)
        a._designer_init()
        return a

    # This method returns the name of the custom widget class
    def name(self):
        return "ActionButton"

    # Returns the name of the group in Qt Designer's widget box
    def group(self):
        return "Linuxcnc - Controller"

    # Returns the icon
    def icon(self):
        return QtGui.QIcon(':/qt-project.org/styles/commonstyle/images/standardbutton-apply-128.png')

    # Returns a tool tip short description
    def toolTip(self):
        return "Action button widget"

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
        return '<widget class="ActionButton" name="actionbutton" />\n'

    # Returns the module containing the custom widget class. It may include
    # a module path.
    def includeFile(self):
        return "qtvcp.widgets.action_button"


class TreeComboBox(QtWidgets.QComboBox):
    def __init__(self, parent=None):
        super(TreeComboBox, self).__init__(parent)

        self.__skip_next_hide = False
        self._last_pick = None

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

    # hacky work around
    def updateLastPick(self, pick):
        self._last_pick = pick

    def getLastPick(self):
        return self._last_pick


class ActionButtonDialog(QtWidgets.QDialog):
    def __init__(self, widget, parent=None):
        QtWidgets.QDialog.__init__(self, parent)

        self.setWindowTitle(self.tr("Set Options"))
        self.setGeometry(300, 300, 300, 300)
        self.setSizePolicy(QtWidgets.QSizePolicy.Minimum, QtWidgets.QSizePolicy.Preferred)
        self.widget = widget
        self.previewWidget = ActionButton()

        self.tab = QtWidgets.QTabWidget()
        self.tab1 = QtWidgets.QWidget()
        self.tab2 = QtWidgets.QWidget()
        self.tab3 = QtWidgets.QWidget()

        self.tab.addTab(self.tab1, 'Actions')
        self.tab.addTab(self.tab2, 'LED')
        self.tab.addTab(self.tab3, 'Other')
        self.buildtab1()
        self.buildtab2()
        self.buildtab3()

        vbox = QtWidgets.QVBoxLayout()
        vbox.addWidget(self.tab)

        wid = QtWidgets.QWidget()
        vbox2 = QtWidgets.QVBoxLayout(wid)
        self.defaultTextTemplateEditBox = QtWidgets.QLineEdit()
        self.defaultTextTemplateEditBox.setText(self.widget.text())

        hbox = QtWidgets.QHBoxLayout()
        label = QtWidgets.QLabel('Default Text Template')
        hbox.addWidget(label)
        # dialogButton = QtWidgets.QPushButton('RichText Editor')
        # dialogButton.clicked.connect(lambda :self.launchDialog(self.defaultTextTemplateEditBox))
        # hbox.addWidget(dialogButton)
        vbox2.addLayout(hbox)

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

        self.onSetOptions()

    def buildtab1(self):
        self.combo = TreeComboBox()
        model = QtGui.QStandardItemModel()
        model.setHeaderData(0, QtCore.Qt.Horizontal, 'Name', QtCore.Qt.DisplayRole)

        # (('Displayed name',['widget property name', code to show related data],[])
        node_1 = (('Estop', ['estop', 0], []),
                  ('Machine On', ['machine_on', 0], []),
                  ('Home', ['home', 1], []),
                  ('Unhome', ['unhome', 1], []),
                  ('Home Selected', ['home_select', 0], []),
                  ('Unhome Selected', ['unhome_select', 0], []),
                  ('Run', ['run', 0], []),
                  ('Run from Line Status', ['run_from_status', 0], []),
                  ('Run From Line Slot', ['run_from_slot', 0], []),
                  ('Abort', ['abort', 0], []),
                  ('Pause', ['pause', 0], []),
                  ('Step', ['step', 0], []),
                  ('Override Limits', ['limits_override', 0], []),
                  ('Zero Axis', ['zero_axis', 1], []),
                  ('Zero G5x', ['zero_g5x', 0], []),
                  ('Zero G92', ['zero_g92', 0], []),
                  ('Zero Z Rotational', ['zero_zrot', 0], []),
                  ('Lathe Mirror X', ['lathe_mirror_x', 0], []),
                  ('Block Delete', ['block_delete', 0], []),
                  ('Optional Stop', ['optional_stop', 0], []),
                  ('Food Coolant', ['flood', 0], []),
                  ('Mist Coolant', ['mist', 0], []),
                  ('Exit Screen', ['exit', 0], []))
        node_2 = (('Jog Joint Positive', ['jog_joint_pos', 1], []),
                  ('Jog Joint Negative', ['jog_joint_neg', 1], []),
                  ('Jog Selected Positive', ['jog_selected_pos', 0], []),
                  ('Jog Selected Negative', ['jog_selected_neg', 0], []),
                  ('Jog Increment', ['jog_incr', 1038], []),
                  ('Jog Rate', ['jog_rate', 112], []))
        node_3 = (('Load Dialog', ['load_dialog', 0], []),
                  ('Macro Dialog', ['macro_dialog', 0], []),
                  ('CamView Dialog', ['camview_dialog', 0], []),
                  ('Machine Log Dialog', ['machine_log_dialog', 0], []),
                  ('Origin Offset Dialog', ['origin_offset_dialog', 0], []),
                  ('Tool Offset Dialog', ['tool_offset_dialog', 0], []))
        node_4 = (('Launch HALmeter', ['launch_halmeter', 0], []),
                  ('Launch Status', ['launch_status', 0], []),
                  ('Launch HALshow', ['launch_halshow', 0], []),
                  ('Launch HALscope', ['launch_halscope', 0], []),
                  ('Launch Calibration', ['launch_calibration', 0], []))
        node_5 = (('Set MDI Mode', ['mdi', 0], []),
                  ('Set Auto Mode', ['auto', 0], []),
                  ('Set Manual Mode', ['manual', 0], []))
        node_6 = (('Set Feed Override', ['feed_over', 112], []),
                  ('Set Rapid Override', ['rapid_over', 112], []),
                  ('Set Spindle Override', ['spindle_over', 112], []),
                  ('Set Max Velocity Override', ['max_velocity_over', 112], []))
        node_7 = (('Set Spindle Forward', ['spindle_fwd', 0], []),
                  ('Set Spindle Reverse', ['spindle_rev', 0], []),
                  ('Set Spindle Stop', ['spindle_stop', 0], []),
                  ('Set Spindle Up', ['spindle_up', 0], []),
                  ('Set Spindle Down', ['spindle_down', 0], []))
        node_8 = (('Set DRO to Relative', ['dro_relative', 0], []),
                  ('Set DRO to Absolute', ['dro_absolute', 0], []),
                  ('Set DRO to DTG', ['dro_dtg', 0], []),)
        node_9 = (('View Change', ['view_change', 128], []),)
        node_10 = (('MDI Commands', ['mdi_command', 256], []),
                   ('MDI Commands From INI', ['ini_mdi_command', 512], []))
        node_none = (('Unused', ['unused', 0], []),)

        parent_node = [('Unset', [None, None], node_none),
                       ('MACHINE CONTROL', [None, None], node_1),
                       ('JOGGING CONTROLS', [None, None], node_2),
                       ('DIALOG LAUNCH', [None, None], node_3),
                       ('LAUNCH PROGRAMS', [None, None], node_4),
                       ('MODE SETTING', [None, None], node_5),
                       ('OVERRIDE SETTING', [None, None], node_6),
                       ('SPINDLE CONTROLS', [None, None], node_7),
                       ('DRO CONTROL', [None, None], node_8),
                       ('GRAPHIC CONTROLS', [None, None], node_9),
                       ('MDI', [None, None], node_10)]

        self.combo.addItems(model, parent_node)
        self.combo.setModel(model)
        # self.combo.view().hideColumn (1)
        # self.combo.resize(300, 30)
        self.combo.activated.connect(self.selectionChanged)

        layout = QtWidgets.QVBoxLayout()
        layout.addWidget(self.combo)

        # related data selection - note the name 'self.ud' uses a number
        # that doubles each time - binary coded. this is so we can specify
        # multiple selectors  code 1 give ud1, code 2 gives ud2, code 3 gives
        # ud1 and ud2 etc

        # Joint number selection
        self.ud1 = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('Joint/axis number')
        self.JNumSpinBox = QtWidgets.QSpinBox()
        self.JNumSpinBox.setRange(-1, 8)
        self.JNumSpinBox.setValue(self.widget.joint_number)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.JNumSpinBox)
        self.ud1.setLayout(hbox)
        layout.addWidget(self.ud1)
        self.ud1.hide()

        # Jog imperial increment
        self.ud2 = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('Jog Increment Imperial')
        self.jogIncImpSpinBox = QtWidgets.QDoubleSpinBox()
        self.jogIncImpSpinBox.setRange(-1, 1000)
        self.jogIncImpSpinBox.setDecimals(4)
        self.jogIncImpSpinBox.setValue(self.widget.jog_incr_imperial)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.jogIncImpSpinBox)
        self.ud2.setLayout(hbox)
        layout.addWidget(self.ud2)
        self.ud2.hide()

        # Jog mm increment
        self.ud4 = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('Jog Increment mm')
        self.jogIncMMSpinBox = QtWidgets.QDoubleSpinBox()
        self.jogIncMMSpinBox.setRange(-1, 100000)
        self.jogIncMMSpinBox.setDecimals(4)
        self.jogIncMMSpinBox.setValue(self.widget.jog_incr_mm)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.jogIncMMSpinBox)
        self.ud4.setLayout(hbox)
        layout.addWidget(self.ud4)
        self.ud4.hide()

        # Jog Angular Increments
        self.ud8 = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('Jog Increment Angular')
        self.jogIncAngSpinBox = QtWidgets.QDoubleSpinBox()
        self.jogIncAngSpinBox.setRange(-1, 100000)
        self.jogIncAngSpinBox.setDecimals(4)
        self.jogIncAngSpinBox.setValue(self.widget.jog_incr_angle)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.jogIncAngSpinBox)
        self.ud8.setLayout(hbox)
        layout.addWidget(self.ud8)
        self.ud8.hide()

        # float
        self.ud16 = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('Float')
        self.floatSpinBox = QtWidgets.QDoubleSpinBox()
        self.floatSpinBox.setRange(-1000, 1000)
        self.floatSpinBox.setDecimals(4)
        self.floatSpinBox.setValue(self.widget.float)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.floatSpinBox)
        self.ud16.setLayout(hbox)
        layout.addWidget(self.ud16)
        self.ud16.hide()

        # Alternate float
        self.ud32 = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('Alternate Float')
        self.floatAltSpinBox = QtWidgets.QDoubleSpinBox()
        self.floatAltSpinBox.setRange(-1000, 1000)
        self.floatAltSpinBox.setDecimals(4)
        self.floatAltSpinBox.setValue(self.widget.float_alt)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.floatAltSpinBox)
        self.ud32.setLayout(hbox)
        layout.addWidget(self.ud32)
        self.ud32.hide()

        # Toggle float option
        self.ud64 = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('Toggle Float Option')
        self.toggleCheckBox = QtWidgets.QCheckBox()
        self.toggleCheckBox.setChecked(self.widget.toggle_float)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.toggleCheckBox)
        self.ud64.setLayout(hbox)
        layout.addWidget(self.ud64)
        self.ud64.hide()

        # View
        self.ud128 = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('Graphics View')
        self.viewComboBox = QtWidgets.QComboBox()
        flag = 0
        for num, i in enumerate(('P', 'X', 'Y', 'Y2', 'Z', 'Z2', 'Clear',
                                 'zoom-in', 'zoom-out', 'pan-up', 'pan-down', 'pan-left',
                                 'pan-right', 'rotate-up', 'rotate-down', 'rotate-cw',
                                 'rotate-ccw', 'reload', 'overlay_dro_on', 'overlay_dro_off',
                                 'overlay-offsets-on', 'overlay-offsets-off',
                                 'inhibit-selection-on', 'inhibit-selection-off',
                                 'alpha-mode-on', 'alpha-mode-off', 'dimensions-on', 'dimensions-off')):
            if self.widget.view_type.lower() == i.lower():
                flag = num
            self.viewComboBox.addItem(i)
        self.viewComboBox.setCurrentIndex(flag)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.viewComboBox)
        self.ud128.setLayout(hbox)
        layout.addWidget(self.ud128)
        self.ud128.hide()

        # MDI command edit box
        self.ud256 = QtWidgets.QWidget()
        vbox = QtWidgets.QVBoxLayout()
        vbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('MDI Command Line')
        self.commandEditBox = QtWidgets.QLineEdit()
        self.commandEditBox.setText(self.widget.command_text)
        vbox.addWidget(label)
        vbox.addWidget(self.commandEditBox)
        self.ud256.setLayout(vbox)
        layout.addWidget(self.ud256)
        self.ud256.hide()

        # MDI command from INI edit box
        self.ud512 = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('MDI Command from INI File')
        self.MDISpinBox = QtWidgets.QSpinBox()
        self.MDISpinBox.setRange(0, 50)
        # self.MDISpinBox.setValue(self.widget.INI_MDI_number)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.MDISpinBox)
        self.ud512.setLayout(hbox)
        layout.addWidget(self.ud512)
        self.ud512.hide()

        # text template edit box
        self.ud1024 = QtWidgets.QWidget()
        vbox = QtWidgets.QVBoxLayout()
        vbox.setContentsMargins(0, 0, 0, 0)
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('Template Label Option')
        self.textTemplateCheckBox = QtWidgets.QCheckBox()
        self.textTemplateCheckBox.setChecked(self.widget.template_label)
        self.textTemplateCheckBox.clicked.connect(self.onSetOptions)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.textTemplateCheckBox)
        vbox.addLayout(hbox)

        self.vbox = QtWidgets.QWidget()
        vbox2 = QtWidgets.QVBoxLayout(self.vbox)
        label = QtWidgets.QLabel('Imperial Text Template')
        self.textTemplateEditBox = QtWidgets.QLineEdit()
        self.textTemplateEditBox.setText(self.widget._textTemplate)
        vbox2.addWidget(label)
        vbox2.addWidget(self.textTemplateEditBox)

        label = QtWidgets.QLabel('Metric Text Template')
        self.altTextTemplateEditBox = QtWidgets.QLineEdit()
        self.altTextTemplateEditBox.setText(self.widget._alt_textTemplate)
        vbox2.addWidget(label)
        vbox2.addWidget(self.altTextTemplateEditBox)
        vbox.addWidget(self.vbox)

        self.ud1024.setLayout(vbox)
        layout.addWidget(self.ud1024)
        self.ud1024.hide()

        self.tab1.setLayout(layout)

        # set combo to currently set property
        # widget[cdata[1][0]] will tell us the widgets property state
        # eg widget.estop or widget.machine_on
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
            winPropertyName = self.combo.itemData(cnum, role=QtCore.Qt.UserRole + 1)
            # print 'selected property,related data code:',winPropertyName
            self.combo.updateLastPick(winPropertyName)
        else:
            self.combo.select(0, 0)

    def selectionChanged(self, i):
        winPropertyName = self.combo.itemData(i, role=QtCore.Qt.UserRole + 1)
        userDataCode = self.combo.itemData(i, role=QtCore.Qt.UserRole + 2)
        # print 'selected property,related data code:',winPropertyName,userDataCode,i
        self.combo.updateLastPick(winPropertyName)
        if winPropertyName is None:
            # collapsed = self.combo.view().isExpanded(self.combo.view().currentIndex())
            # if collapsed: return True
            return True
        if not userDataCode is None:
            for i in (1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024):
                widg = self['ud%s' % i]
                if userDataCode & i:
                    widg.show()
                else:
                    widg.hide()
        self.adjustSize()

    # Indicator Tab
    def buildtab2(self):
        statusProperties = [['None', None], ['Is Estopped', 'is_estopped'],
                            ['Is On', 'is_on'], ['All Homed', 'is_homed'],
                            ['Is Joint Homed', 'is_joint_homed'],
                            ['Idle', 'is_idle'], ['Paused', 'is_paused'],
                            ['Flood', 'is_flood'], ['Mist', 'is_mist'],
                            ['Block Delete', 'is_block_delete'],
                            ['Optional Stop', 'is_optional_stop'],
                            ['Manual', 'is_manual'], ['MDI', 'is_mdi'],
                            ['Auto', 'is_auto'],
                            ['Spindle Stopped', 'is_spindle_stopped'],
                            ['Spindle Fwd', 'is_spindle_fwd'],
                            ['Spindle Reverse', 'is_spindle_rev'],
                            ['On Limits', 'is_limits_overridden']]
        layout = QtWidgets.QGridLayout()

        # Indicator option
        self.indicatorO = QtWidgets.QFrame()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('<b>Led Indicator Option<\b>')
        self.indicatorCheckBox = QtWidgets.QCheckBox()
        self.indicatorCheckBox.setChecked(self.widget.draw_indicator)
        self.indicatorCheckBox.clicked.connect(self.onSetOptions)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.indicatorCheckBox)
        self.indicatorO.setLayout(hbox)
        layout.addWidget(self.indicatorO)
        # self.indicatorO.hide()

        # HAL pin LED option
        self.halP = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('Hal Pin Controlled LED')
        self.halCheckBox = QtWidgets.QCheckBox()
        self.halCheckBox.setChecked(self.widget._HAL_pin)
        self.halCheckBox.clicked.connect(self.onSetOptions)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.halCheckBox)
        self.halP.setLayout(hbox)
        layout.addWidget(self.halP)
        self.halP.hide()

        line = QtWidgets.QFrame()
        line.setFrameShape(QtWidgets.QFrame.HLine)
        line.setFrameShadow(QtWidgets.QFrame.Sunken)
        layout.addWidget(line)

        # Linuxcnc Status LED option
        self.statusO = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('Status Controlled LED')
        self.statusCheckBox = QtWidgets.QCheckBox()
        self.statusCheckBox.setChecked(self.widget._ind_status)
        self.statusCheckBox.clicked.connect(self.onSetOptions)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.statusCheckBox)
        self.statusO.setLayout(hbox)
        layout.addWidget(self.statusO)
        self.statusO.hide()

        # Invert Status LED option
        self.invertStatus = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('Invert Status LED State')
        self.invertCheckBox = QtWidgets.QCheckBox()
        self.invertCheckBox.setChecked(self.widget._invert_status)
        self.invertCheckBox.clicked.connect(self.onSetOptions)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.invertCheckBox)
        self.invertStatus.setLayout(hbox)
        layout.addWidget(self.invertStatus)
        # self.invertStatus.hide()

        # status watch combo box
        self.watch = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('Watch Status:')
        self.statusCombo = QtWidgets.QComboBox()
        flag = 0
        for index, data in enumerate(statusProperties):
            if data[1] and self.widget['_{}'.format(data[1])]:
                flag = index
            self.statusCombo.addItem(data[0])
            self.statusCombo.setItemData(index, data[1], QtCore.Qt.UserRole + 1)
        self.statusCombo.setCurrentIndex(flag)
        self.statusCombo.activated.connect(self.statusSelectionChanged)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.statusCombo)
        self.watch.setLayout(hbox)
        layout.addWidget(self.watch)
        self.watch.hide()

        # joint number selection
        self.jnum = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('Joint/Spindle Selection')
        self.jnumCombo = QtWidgets.QComboBox()
        self.jnumCombo.activated.connect(self.onSetOptions)
        for i in range(0, 10):
            self.jnumCombo.addItem('{}'.format(i), i)
        self.jnumCombo.setCurrentIndex(self.widget._joint_number)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.jnumCombo)
        self.jnum.setLayout(hbox)
        layout.addWidget(self.jnum)
        self.jnum.hide()

        line = QtWidgets.QFrame()
        line.setFrameShape(QtWidgets.QFrame.HLine)
        line.setFrameShadow(QtWidgets.QFrame.Sunken)
        layout.addWidget(line)

        # Shape selection
        self.shape = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('Shape Selection')
        self.shapeCombo = QtWidgets.QComboBox()
        self.shapeCombo.activated.connect(self.onSetOptions)
        self.shapeCombo.addItem('Triangle', 0)
        self.shapeCombo.addItem('Circle', 1)
        self.shapeCombo.addItem('TopBar', 2)
        self.shapeCombo.addItem('SideBar', 3)
        self.shapeCombo.setCurrentIndex(self.widget._shape)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.shapeCombo)
        self.shape.setLayout(hbox)
        layout.addWidget(self.shape)
        self.shape.hide()

        # Round indicator diameter
        self.diam = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('Round Diamter')
        self.diamSpinBox = QtWidgets.QDoubleSpinBox()
        self.diamSpinBox.setRange(2, 100)
        self.diamSpinBox.setDecimals(1)
        self.diamSpinBox.setSingleStep(1)
        self.diamSpinBox.setValue(self.widget._diameter)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.diamSpinBox)
        self.diam.setLayout(hbox)
        layout.addWidget(self.diam)
        self.diam.hide()

        # Triangle indicator size
        self.size = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('Triangle Size')
        self.floatSpinBox = QtWidgets.QDoubleSpinBox()
        self.floatSpinBox.setRange(0, 2)
        self.floatSpinBox.setDecimals(4)
        self.floatSpinBox.setSingleStep(0.1)
        self.floatSpinBox.setValue(self.widget._size)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.floatSpinBox)
        self.size.setLayout(hbox)
        layout.addWidget(self.size)
        self.size.hide()

        # Bar LED corner radius
        self.radius = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('Corner Radius')
        self.radiusSpinBox = QtWidgets.QDoubleSpinBox()
        self.radiusSpinBox.setRange(0, 20)
        self.radiusSpinBox.setDecimals(1)
        self.radiusSpinBox.setSingleStep(1)
        self.radiusSpinBox.setValue(self.widget._corner_radius)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.radiusSpinBox)
        self.radius.setLayout(hbox)
        layout.addWidget(self.radius)
        self.radius.hide()

        # Bar LED right edge offset
        self.rightedgeoffset = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('Right Edge Offset')
        self.rightedgeoffsetSpinBox = QtWidgets.QDoubleSpinBox()
        self.rightedgeoffsetSpinBox.setRange(0, 50)
        self.rightedgeoffsetSpinBox.setDecimals(1)
        self.rightedgeoffsetSpinBox.setSingleStep(.1)
        self.rightedgeoffsetSpinBox.setValue(self.widget._right_edge_offset)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.rightedgeoffsetSpinBox)
        self.rightedgeoffset.setLayout(hbox)
        layout.addWidget(self.rightedgeoffset)
        self.rightedgeoffset.hide()

        # Bar LED top edge offset
        self.topedgeoffset = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('Top Edge Offset')
        self.topedgeoffsetSpinBox = QtWidgets.QDoubleSpinBox()
        self.topedgeoffsetSpinBox.setRange(0, 50)
        self.topedgeoffsetSpinBox.setDecimals(1)
        self.topedgeoffsetSpinBox.setSingleStep(.1)
        self.topedgeoffsetSpinBox.setValue(self.widget._top_edge_offset)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.topedgeoffsetSpinBox)
        self.topedgeoffset.setLayout(hbox)
        layout.addWidget(self.topedgeoffset)
        self.topedgeoffset.hide()

        # Bar LED height fraction
        self.hfraction = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('Height Fraction')
        self.hfractionSpinBox = QtWidgets.QDoubleSpinBox()
        self.hfractionSpinBox.setRange(0, 1)
        self.hfractionSpinBox.setDecimals(1)
        self.hfractionSpinBox.setSingleStep(.1)
        self.hfractionSpinBox.setValue(self.widget._h_fraction)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.hfractionSpinBox)
        self.hfraction.setLayout(hbox)
        layout.addWidget(self.hfraction)
        self.hfraction.hide()

        # Bar LED width fraction
        self.wfraction = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('Width Fraction')
        self.wfractionSpinBox = QtWidgets.QDoubleSpinBox()
        self.wfractionSpinBox.setRange(0, 1)
        self.wfractionSpinBox.setDecimals(1)
        self.wfractionSpinBox.setSingleStep(.1)
        self.wfractionSpinBox.setValue(self.widget._w_fraction)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.wfractionSpinBox)
        self.wfraction.setLayout(hbox)
        layout.addWidget(self.wfraction)
        self.wfraction.hide()

        # true state indicator color
        self.colorTrue = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        button = QtWidgets.QLabel('True State LED Color')
        self.trueColorButton = QtWidgets.QPushButton()
        self.trueColorButton.setToolTip('Opens color dialog')
        self.trueColorButton.clicked.connect(self.on_trueColorClick)
        self.trueColorButton.setStyleSheet('QPushButton {background-color: %s ;}' % self.widget._on_color.name())
        self._onColor = self.widget._on_color.name()
        hbox.addWidget(button)
        hbox.addWidget(self.trueColorButton)
        self.colorTrue.setLayout(hbox)
        layout.addWidget(self.colorTrue)
        self.colorTrue.hide()

        # False state indicator color
        self.colorFalse = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        button = QtWidgets.QLabel('False State LED Color')
        self.falseColorButton = QtWidgets.QPushButton()
        self.falseColorButton.setToolTip('Opens color dialog')
        self.falseColorButton.clicked.connect(self.on_falseColorClick)
        self.falseColorButton.setStyleSheet('QPushButton {background-color: %s ;}' % self.widget._off_color.name())
        self._offColor = self.widget._off_color.name()
        hbox.addWidget(button)
        hbox.addWidget(self.falseColorButton)
        self.colorFalse.setLayout(hbox)
        layout.addWidget(self.colorFalse)
        self.colorFalse.hide()

        line = QtWidgets.QFrame()
        line.setFrameShape(QtWidgets.QFrame.HLine)
        line.setFrameShadow(QtWidgets.QFrame.Sunken)
        layout.addWidget(line)

        self.tab2.setLayout(layout)

    def statusSelectionChanged(self, index):
        choice = self.statusCombo.itemData(self.statusCombo.currentIndex(), QtCore.Qt.UserRole + 1)
        if choice is None:
            self.jnum.hide()
            return
        if choice == 'is_joint_homed' or 'is_spindle' in choice:
            self.jnum.show()
        else:
            self.jnum.hide()

    # Indicator Tab 3
    def buildtab3(self):
        layout = QtWidgets.QGridLayout()
        # State Text option
        self.textO = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('<b>State Changes Text Option<\b?')
        self.textCheckBox = QtWidgets.QCheckBox()
        self.textCheckBox.setChecked(self.widget._state_text)
        self.textCheckBox.clicked.connect(self.onSetOptions)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.textCheckBox)
        self.textO.setLayout(hbox)
        layout.addWidget(self.textO)
        # self.textO.hide()

        # True text edit box
        self.txtTrue = QtWidgets.QWidget()
        vbox = QtWidgets.QVBoxLayout()
        vbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('True State Text Line')
        self.tTextEditBox = QtWidgets.QLineEdit()
        self.tTextEditBox.setText(self.widget._true_string)
        vbox.addWidget(label)
        vbox.addWidget(self.tTextEditBox)
        self.txtTrue.setLayout(vbox)
        layout.addWidget(self.txtTrue)
        self.txtTrue.hide()

        # False text edit box
        self.txtFalse = QtWidgets.QWidget()
        vbox = QtWidgets.QVBoxLayout()
        vbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('False State Text Line')
        self.fTextEditBox = QtWidgets.QLineEdit()
        self.fTextEditBox.setText(self.widget._false_string)
        vbox.addWidget(label)
        vbox.addWidget(self.fTextEditBox)
        self.txtFalse.setLayout(vbox)
        layout.addWidget(self.txtFalse)
        self.txtFalse.hide()

        # Python Command option
        self.cmdPython = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('<b>State Calls Python Command<\b>')
        self.pythonCheckBox = QtWidgets.QCheckBox()
        self.pythonCheckBox.setChecked(self.widget._python_command)
        self.pythonCheckBox.clicked.connect(self.onSetOptions)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.pythonCheckBox)
        self.cmdPython.setLayout(hbox)
        layout.addWidget(self.cmdPython)
        # self.cmdPython.hide()

        # True command edit box
        self.cmdTrue = QtWidgets.QWidget()
        vbox = QtWidgets.QVBoxLayout()
        vbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('True State Command Line')
        self.tCommandEditBox = QtWidgets.QLineEdit()
        self.tCommandEditBox.setText(self.widget.true_python_command)
        vbox.addWidget(label)
        vbox.addWidget(self.tCommandEditBox)
        self.cmdTrue.setLayout(vbox)
        layout.addWidget(self.cmdTrue)
        self.cmdTrue.hide()

        # False command edit box
        self.cmdFalse = QtWidgets.QWidget()
        vbox = QtWidgets.QVBoxLayout()
        vbox.setContentsMargins(0, 0, 0, 0)
        label = QtWidgets.QLabel('False State Command Line')
        self.fCommandEditBox = QtWidgets.QLineEdit()
        self.fCommandEditBox.setText(self.widget.false_python_command)
        vbox.addWidget(label)
        vbox.addWidget(self.fCommandEditBox)
        self.cmdFalse.setLayout(vbox)
        layout.addWidget(self.cmdFalse)
        self.cmdFalse.hide()
        self.tab3.setLayout(layout)

    def launchDialog(self, widget):
        d = RichTextEditorDialog()
        style = d.showDialog(pretext=widget.text())
        if style:
            self.defaultTextTemplateEditBox.setText(style)
            widget.setText(style)

    def onSetOptions(self):
        if self.textTemplateCheckBox.isChecked():
            self.vbox.show()
        else:
            self.vbox.hide()

        # mutually exclusive options
        if self.halCheckBox.isChecked() and self.statusCheckBox.isChecked():
            if self.sender() == self.halCheckBox:
                self.statusCheckBox.setChecked(False)
            elif self.sender() == self.statusCheckBox:
                self.halCheckBox.setChecked(False)

        if self.statusCheckBox.isChecked():
            self.watch.show()
            self.invertStatus.show()
        else:
            self.watch.hide()
            self.invertStatus.hide()

        if self.indicatorCheckBox.isChecked():
            self.halP.show()
            self.colorTrue.show()
            self.colorFalse.show()
            self.statusO.show()
            self.shape.show()
            self.rightedgeoffset.show()
            self.topedgeoffset.show()
            shape = self.shapeCombo.itemData(self.shapeCombo.currentIndex())
            if shape == 0:
                self.size.show()
                self.diam.hide()
                self.radius.hide()
                self.hfraction.hide()
                self.wfraction.hide()
            elif shape == 1:
                self.diam.show()
                self.size.hide()
                self.radius.hide()
                self.hfraction.hide()
                self.wfraction.hide()
            elif shape in (1, 2):
                self.size.hide()
                self.diam.hide()
                self.radius.show()
                self.hfraction.show()
                self.wfraction.show()

            choice = self.statusCombo.itemData(self.statusCombo.currentIndex(), QtCore.Qt.UserRole + 1)
            if choice is not None:
                if choice == 'is_joint_homed' or 'is_spindle' in choice:
                    self.jnum.show()
                else:
                    self.jnum.hide()
        else:
            self.halP.hide()
            self.size.hide()
            self.colorTrue.hide()
            self.colorFalse.hide()
            self.statusO.hide()
            self.diam.hide()
            self.shape.hide()
            self.watch.hide()
            self.invertStatus.hide()
            self.rightedgeoffset.hide()
            self.topedgeoffset.hide()

        if self.pythonCheckBox.isChecked():
            self.cmdTrue.show()
            self.cmdFalse.show()
        else:
            self.cmdTrue.hide()
            self.cmdFalse.hide()

        if self.textCheckBox.isChecked():
            self.txtTrue.show()
            self.txtFalse.show()
        else:
            self.txtTrue.hide()
            self.txtFalse.hide()

        self.adjustSize()

    def on_trueColorClick(self):
        color = QtWidgets.QColorDialog.getColor()
        if color.isValid():
            self._onColor = color.name()
            self.trueColorButton.setStyleSheet('QPushButton {background-color: %s ;}' % self._onColor)

    def on_falseColorClick(self):
        color = QtWidgets.QColorDialog.getColor()
        if color.isValid():
            self._offColor = color.name()
            self.falseColorButton.setStyleSheet('QPushButton {background-color: %s ;}' % self._offColor)

    def updateWidget(self):
        # check action-combobox last pick because it sometimes
        # forgets what's selected. anyways...
        winProperty = self.combo.getLastPick()
        if winProperty is None:
            winProperty = 'unused'
        formWindow = QDesignerFormWindowInterface.findFormWindow(self.widget)
        if formWindow:
            # set widget option
            formWindow.cursor().setProperty('indicator_option',
                                            QtCore.QVariant(self.indicatorCheckBox.isChecked()))
            formWindow.cursor().setProperty('python_command_option',
                                            QtCore.QVariant(self.pythonCheckBox.isChecked()))
            formWindow.cursor().setProperty('checked_state_text_option',
                                            QtCore.QVariant(self.textCheckBox.isChecked()))

            formWindow.cursor().setProperty('indicator_HAL_pin_option',
                                            QtCore.QVariant(self.halCheckBox.isChecked()))
            formWindow.cursor().setProperty('indicator_status_option',
                                            QtCore.QVariant(self.statusCheckBox.isChecked()))
            formWindow.cursor().setProperty('indicator_size',
                                            QtCore.QVariant(self.floatSpinBox.value()))

            formWindow.cursor().setProperty('right_edge_offset',
                                            QtCore.QVariant(self.rightedgeoffsetSpinBox.value()))
            formWindow.cursor().setProperty('top_edge_offset',
                                            QtCore.QVariant(self.topedgeoffsetSpinBox.value()))
            formWindow.cursor().setProperty('corner_radius',
                                            QtCore.QVariant(self.radiusSpinBox.value()))
            formWindow.cursor().setProperty('height_fraction',
                                            QtCore.QVariant(self.hfractionSpinBox.value()))
            formWindow.cursor().setProperty('width_fraction',
                                            QtCore.QVariant(self.wfractionSpinBox.value()))

            formWindow.cursor().setProperty('circle_diameter',
                                            QtCore.QVariant(self.diamSpinBox.value()))
            formWindow.cursor().setProperty('shape_option',
                                            QtCore.QVariant(self.shapeCombo.itemData(
                                                self.shapeCombo.currentIndex())))
            formWindow.cursor().setProperty('joint_number_status',
                                            QtCore.QVariant(self.jnumCombo.itemData(
                                                self.jnumCombo.currentIndex())))

            formWindow.cursor().setProperty('on_color',
                                            QtCore.QVariant(self._onColor))
            formWindow.cursor().setProperty('off_color',
                                            QtCore.QVariant(self._offColor))

            formWindow.cursor().setProperty('true_state_string',
                                            QtCore.QVariant(self.tTextEditBox.text()))
            formWindow.cursor().setProperty('false_state_string',
                                            QtCore.QVariant(self.fTextEditBox.text()))

            formWindow.cursor().setProperty('true_python_cmd_string',
                                            QtCore.QVariant(self.tCommandEditBox.text()))
            formWindow.cursor().setProperty('false_python_cmd_string',
                                            QtCore.QVariant(self.fCommandEditBox.text()))

        if formWindow and winProperty == 'unused':
            formWindow.cursor().setProperty('estop_action',
                                            QtCore.QVariant(True))
            formWindow.cursor().setProperty('estop_action',
                                            QtCore.QVariant(False))
        elif formWindow:
            # set widget option
            formWindow.cursor().setProperty(winProperty + '_action',
                                            QtCore.QVariant(True))
        # set status data fom combo box
        # we read all data from combo and set each property to its
        # current dialog state (ie. selected is true, all others false)
        for i in range(1, self.statusCombo.count()):
            data = self.statusCombo.itemData(i, QtCore.Qt.UserRole + 1)
            propertyText = '{}_status'.format(data)
            if self.statusCombo.itemData(self.statusCombo.currentIndex(), QtCore.Qt.UserRole + 1) == data:
                state = True
            else:
                state = False
            formWindow.cursor().setProperty(propertyText,
                                            QtCore.QVariant(state))
        formWindow.cursor().setProperty('invert_the_status',
                                        QtCore.QVariant(self.invertCheckBox.isChecked()))

        #####################
        # set related data 
        #####################
        formWindow.cursor().setProperty('joint_number',
                                        QtCore.QVariant(self.JNumSpinBox.value()))
        formWindow.cursor().setProperty('incr_imperial_number',
                                        QtCore.QVariant(self.jogIncImpSpinBox.value()))
        formWindow.cursor().setProperty('incr_mm_number',
                                        QtCore.QVariant(self.jogIncMMSpinBox.value()))
        formWindow.cursor().setProperty('incr_angular_number',
                                        QtCore.QVariant(self.jogIncAngSpinBox.value()))
        formWindow.cursor().setProperty('float_num',
                                        QtCore.QVariant(self.floatSpinBox.value()))
        formWindow.cursor().setProperty('float_alt_num',
                                        QtCore.QVariant(self.floatAltSpinBox.value()))
        formWindow.cursor().setProperty('view_type_string',
                                        QtCore.QVariant(self.viewComboBox.currentText()))
        formWindow.cursor().setProperty('toggle_float_option',
                                        QtCore.QVariant(self.toggleCheckBox.isChecked()))
        formWindow.cursor().setProperty('command_text_string',
                                        QtCore.QVariant(self.commandEditBox.text()))
        formWindow.cursor().setProperty('ini_mdi_number',
                                        QtCore.QVariant(self.MDISpinBox.value()))
        formWindow.cursor().setProperty('template_label_option',
                                        QtCore.QVariant(self.textTemplateCheckBox.isChecked()))
        # block signal so button text doesn't change when selecting action
        self.widget._designer_block_signal = True
        formWindow.cursor().setProperty('text',
                                        QtCore.QVariant(self.defaultTextTemplateEditBox.text()))
        formWindow.cursor().setProperty('textTemplate',
                                        QtCore.QVariant(self.textTemplateEditBox.text()))
        formWindow.cursor().setProperty('alt_textTemplate',
                                        QtCore.QVariant(self.altTextTemplateEditBox.text()))
        self.widget._designer_block_signal = False

        self.accept()

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)

    def __setitem__(self, item, value):
        return setattr(self, item, value)


class ActionButtonMenuEntry(QPyDesignerTaskMenuExtension):

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
        dialog = ActionButtonDialog(self.widget)
        dialog.exec_()


class ActionButtonTaskMenuFactory(QExtensionFactory):
    def __init__(self, parent=None):
        QExtensionFactory.__init__(self, parent)

    def createExtension(self, obj, iid, parent):

        if not isinstance(obj, ActionButton):
            return None
        if iid == Q_TYPEID['QDesignerTaskMenuExtension']:
            return ActionButtonMenuEntry(obj, parent)
        elif iid == Q_TYPEID['QDesignerMemberSheetExtension']:
            return ActionButtonMemberSheet(obj, parent)
        return None


####################################
# RoundButton
####################################
class RoundButtonPlugin(QPyDesignerCustomWidgetPlugin):
    def __init__(self, parent=None):
        super(RoundButtonPlugin, self).__init__(parent)
        self.initialized = False

    def initialize(self, formEditor):
        if self.initialized:
            return
        manager = formEditor.extensionManager()
        if manager:
            self.factory = ActionButtonTaskMenuFactory(manager)
            manager.registerExtensions(self.factory, Q_TYPEID['QDesignerTaskMenuExtension'])
        self.initialized = True

    def isInitialized(self):
        return self.initialized

    def createWidget(self, parent):
        return RoundButton(parent)

    def name(self):
        return "RoundButton"

    def group(self):
        return "Linuxcnc - Controller"

    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('roundbutton')))

    def toolTip(self):
        return "Round shaped Button for actions"

    def whatsThis(self):
        return ""

    def isContainer(self):
        return False

    def domXml(self):
        return '<widget class="RoundButton" name="Roundbutton" />\n'

    def includeFile(self):
        return "qtvcp.widgets.action_button_round"
