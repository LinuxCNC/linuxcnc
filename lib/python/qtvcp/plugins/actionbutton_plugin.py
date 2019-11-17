#!/usr/bin/env python

import sip
from PyQt5 import QtCore, QtGui, QtWidgets
from PyQt5.QtDesigner import QPyDesignerCustomWidgetPlugin, \
                QPyDesignerTaskMenuExtension, QExtensionFactory, \
                QDesignerFormWindowInterface, QPyDesignerMemberSheetExtension
from qtvcp.widgets.action_button import ActionButton
from qtvcp.widgets.action_button_round import RoundButton
from qtvcp.widgets.qtvcp_icons import Icon
ICON = Icon()

Q_TYPEID = {
    'QDesignerContainerExtension':     'org.qt-project.Qt.Designer.Container',
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
        a =  ActionButton(parent)
        a._designer_running = True
        return a

    # This method returns the name of the custom widget class
    def name(self):
        return "ActionButton"

    # Returns the name of the group in Qt Designer's widget box
    def group(self):
        return "Linuxcnc - Controller"

    # Returns the icon
    def icon(self):
        return QtGui.QIcon(QtGui.QPixmap(ICON.get_path('actionbutton')))

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
            #print index.parent(),index.row(),index.column(),index.data(),index.data(QtCore.Qt.UserRole + 1)
            #print self.view().isExpanded(self.view().currentIndex())
            #if self.itemAt(event.pos()) is None
            self.__skip_next_hide = not self.view().visualRect(index).contains(event.pos())
        return False

    def addItems(self, parent, elements):
        for text, value, children in elements:
            item = QtGui.QStandardItem(text)
            item.setData(value[0], role = QtCore.Qt.UserRole + 1)
            item.setData(value[1], role = QtCore.Qt.UserRole + 2)
            parent.appendRow(item)
            if children:
                self.addItems(item, children)

    def select(self,nodeNum,row):
        #print 'select:', nodeNum, row
        # select last row
        newIndex = self.model().index(nodeNum,0)
        self.setRootModelIndex(newIndex)
        self.setCurrentIndex (row)

class ActionButtonDialog(QtWidgets.QDialog):
    def __init__(self, widget, parent = None):
        QtWidgets.QDialog.__init__(self, parent)

        self.setWindowTitle(self.tr("Set Options"))
        self.setGeometry(300, 300, 200, 200)
        self.setSizePolicy(QtWidgets.QSizePolicy.Minimum, QtWidgets.QSizePolicy.Minimum)
        self.widget = widget
        self.previewWidget = ActionButton()

        tab = QtWidgets.QTabWidget()
        self.tab1 = QtWidgets.QWidget()
        self.tab2 = QtWidgets.QWidget()
		
        tab.addTab(self.tab1,'Actions')
        tab.addTab(self.tab2,'Indicator Action')
        self.buildtab1()
        self.buildtab2()

        vbox = QtWidgets.QVBoxLayout()
        vbox.addWidget(tab)

        wid = QtWidgets.QWidget()
        vbox2 = QtWidgets.QVBoxLayout(wid)
        label = QtWidgets.QLabel('Default Text Template')
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

        self.onSetOptions()

    def buildtab1(self):
        self.combo = TreeComboBox()
        model = QtGui.QStandardItemModel()
        model.setHeaderData(0, QtCore.Qt.Horizontal, 'Name', QtCore.Qt.DisplayRole)

        # (('Displayed name',['widget property name', code to show related data],[])
        node_1 = (('Estop',['estop', 0], []),
                 ('Machine On',['machine_on', 0], []),
                ('Home',['home', 1], []),
                ('Unhome',['unhome', 1], []),
                ('Home Selected',['home_select', 0], []),
                ('Unhome Selected',['unhome_select', 0], []),
                ('Run',['run', 0], []),
                ('Abort',['abort', 0], []),
                 ('Pause',['pause', 0], []),
                 ('Step',['step', 0], []),
                ('Override Limits',['limits_override', 0], []),
                ('Zero Axis',['zero_axis', 1], []),
                ('Zero G5x',['zero_g5x', 0], []),
                ('Zero G92',['zero_g92', 0], []),
                ('Zero Z Rotaional',['zero_zrot', 0], []),
                ('Block Delete',['block_delete', 0], []),
                ('Optional Stop',['optional_stop', 0], []),
                ('Food Coolant',['flood', 0], []),
                ('Mist Coolant',['mist', 0], []),
                ('Exit Screen',['exit', 0], []) )
        node_2 = (('Jog Joint Positive',['jog_joint_pos', 1], []), 
                ('Jog Joint Negative',['jog_joint_neg', 1], []),
                ('Jog Selected Positive',['jog_selected_pos', 0], []),
                ('Jog Selected Negative',['jog_selected_neg', 0], []),
                ('Jog Increment',['jog_incr', 1038], []),
                ('Jog Rate',['jog_rate', 112], []) )
        node_3 = (('Load Dialog',['load_dialog', 0], []),
                ('Macro Dialog',['macro_dialog', 0], []),
                ('CamView Dialog',['camview_dialog', 0], []),
                ('Machine Log Dialog',['machine_log_dialog', 0], []),
                ('Origin Offset Dialog',['origin_offset_dialog', 0], []),
                ('Tool Offset Dialog',['tool_offset_dialog', 0], []))
        node_4 = (('Launch HALmeter',['launch_halmeter', 0], []),
                ('Launch Status',['launch_status', 0], []),
                ('Launch HALshow',['launch_halshow', 0], []),
                ('Launch HALscope',['launch_halscope', 0], [])  )
        node_5 = (('Set MDI Mode',['mdi', 0], []),
                ('Set Auto Mode',['auto', 0], []),
                ('Set Manual Mode',['manual', 0], []) )
        node_6 = (('Set Feed Override',['feed_over', 112], []),
                ('Set Rapid Override',['rapid_over', 112], []),
                ('Set Spindle Override',['spindle_over', 112], []),
                ('Set Max Velocity Override',['max_velocity_over', 112], [])  )
        node_7 = (('Set Spindle Forward',['spindle_fwd', 0], []),
                ('Set Spindle Reverse',['spindle_rev', 0], []),
                ('Set Spindle Stop',['spindle_stop', 0], []),
                ('Set Spindle Up',['spindle_up', 0], []),
                ('Set Spindle Down',['spindle_down', 0], []) )
        node_8 = (('Set DRO to Relative',['dro_relative', 0], []),
                ('Set DRO to Absolute',['dro_absolute', 0], []),
                ('Set DRO to DTG',['dro_dtg', 0], []), )
        node_9 = (('View Change',['view_change', 128], []),)
        node_10 = (('MDI Commands',['mdi_command', 256], []),
                  ('MDI Commands From INI',['ini_mdi_command', 512], []) )
        node_none = (('Unused',['unused', 0], []),)

        parent_node = [ ('Unset', [None,None], node_none),
                    ('MACHINE CONTROL',[None, None], node_1),
                    ('JOGGING CONTROLS',[None, None], node_2),
                    ('DIALOG LAUNCH',[None, None], node_3),
                    ('LAUNCH PROGRAMS',[None, None], node_4),
                    ('MODE SETTING',[None, None], node_5),
                    ('OVERRIDE SETTING',[None, None], node_6),
                    ('SPINDLE CONTROLS',[None, None], node_7),
                    ('DRO CONTROL',[None, None], node_8),
                    ('GRAPHIC CONTROLS',[None, None], node_9),
                    ('MDI',[None, None], node_10) ]

        self.combo.addItems(model,parent_node)
        self.combo.setModel(model)
        #self.combo.view().hideColumn (1)
        #self.combo.resize(300, 30)
        self.combo.activated.connect(self.selectionChanged)

        layout = QtWidgets.QVBoxLayout()
        layout.addWidget(self.combo)

        # related data selection - note the name 'self.ud' uses a number
        # that doubles each time - binary coded. thsi is so we can specify
        # multiple selectors  code 1 give ud1, code 2 gives ud2, code 3 gives
        # ud1 and ud2 etc
 
        # Joint number selection
        self.ud1 = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0,0,0,0)
        label = QtWidgets.QLabel('Joint/axis number')
        self.JNumSpinBox = QtWidgets.QSpinBox()
        self.JNumSpinBox.setRange(-1,8)
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
        hbox.setContentsMargins(0,0,0,0)
        label = QtWidgets.QLabel('Jog Increment Imperial')
        self.jogIncImpSpinBox = QtWidgets.QDoubleSpinBox()
        self.jogIncImpSpinBox.setRange(-1,1000)
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
        hbox.setContentsMargins(0,0,0,0)
        label = QtWidgets.QLabel('Jog Increment mm')
        self.jogIncMMSpinBox = QtWidgets.QDoubleSpinBox()
        self.jogIncMMSpinBox.setRange(-1,100000)
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
        hbox.setContentsMargins(0,0,0,0)
        label = QtWidgets.QLabel('Jog Increment Angular')
        self.jogIncAngSpinBox = QtWidgets.QDoubleSpinBox()
        self.jogIncAngSpinBox.setRange(-1,100000)
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
        hbox.setContentsMargins(0,0,0,0)
        label = QtWidgets.QLabel('Float')
        self.floatSpinBox = QtWidgets.QDoubleSpinBox()
        self.floatSpinBox.setRange(-1000,1000)
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
        hbox.setContentsMargins(0,0,0,0)
        label = QtWidgets.QLabel('Alternate Float')
        self.floatAltSpinBox = QtWidgets.QDoubleSpinBox()
        self.floatAltSpinBox.setRange(-1000,1000)
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
        hbox.setContentsMargins(0,0,0,0)
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
        hbox.setContentsMargins(0,0,0,0)
        label = QtWidgets.QLabel('Graphics View')
        self.viewComboBox = QtWidgets.QComboBox()
        flag = 0
        for num, i in enumerate(('P','X','Y','Y2','Z','Z2','Clear',
            'zoom-in','zoom-out','pan-up','pan-down','pan-left',
            'pan-right','rotate-up','rotate-down','rotate-cw',
            'rotate-ccw','reload')):
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
        vbox.setContentsMargins(0,0,0,0)
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
        hbox.setContentsMargins(0,0,0,0)
        label = QtWidgets.QLabel('MDI Command from INI File')
        self.MDISpinBox = QtWidgets.QSpinBox()
        self.MDISpinBox.setRange(0,50)
        #self.MDISpinBox.setValue(self.widget.INI_MDI_number)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.MDISpinBox)
        self.ud512.setLayout(hbox)
        layout.addWidget(self.ud512)
        self.ud512.hide()

        # text template edit box
        self.ud1024 = QtWidgets.QWidget()
        vbox = QtWidgets.QVBoxLayout()
        vbox.setContentsMargins(0,0,0,0)
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0,0,0,0)
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
        #print parent_node
        for pnum, pdata in enumerate(parent_node):
            #print pnum,pdata[0]
            if pnum == 0: continue
            if pdata[0]:
                for cnum, cdata in enumerate(pdata[2]):
                    #print 'child,state:',cdata[1][0],self.widget[cdata[1][0]]
                    if self.widget[cdata[1][0]]:
                        flag = True
                        #print 'found index:',pnum,cnum
                        break
            if flag:
                break
        if flag:
            self.combo.select(pnum,cnum)
        else:
            self.combo.select(0,0)

    def selectionChanged(self,i):
        winPropertyName = self.combo.itemData(i,role = QtCore.Qt.UserRole + 1)
        userDataCode = self.combo.itemData(i,role = QtCore.Qt.UserRole + 2)
        #print 'selected property,related data code:',winPropertyName,userDataCode,i
        if winPropertyName is None:
            #collapsed = self.combo.view().isExpanded(self.combo.view().currentIndex())
            #if collapsed: return True
            #self.combo.select(0,0)
            return True
        if not userDataCode is None:
            for i in (1,2,4,8,16,32,64,128,256,512,1024):
                widg = self['ud%s'% i]
                if userDataCode & i:
                    widg.show()
                else:
                    widg.hide()
        self.adjustSize()

    def buildtab2(self):
        layout = QtWidgets.QGridLayout()

        # Indicator option
        self.iud1 = QtWidgets.QFrame()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0,0,0,0)
        label = QtWidgets.QLabel('<b>State Indicator Option<\b>')
        self.indicatorCheckBox = QtWidgets.QCheckBox()
        self.indicatorCheckBox.setChecked(self.widget.draw_indicator)
        self.indicatorCheckBox.clicked.connect(self.onSetOptions)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.indicatorCheckBox)
        self.iud1.setLayout(hbox)
        layout.addWidget(self.iud1)
        #self.iud1.hide()

        # HAL pin option
        self.iud4 = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0,0,0,0)
        label = QtWidgets.QLabel('Hal Pin Option')
        self.halCheckBox = QtWidgets.QCheckBox()
        self.halCheckBox.setChecked(self.widget._HAL_pin)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.halCheckBox)
        self.iud4.setLayout(hbox)
        layout.addWidget(self.iud4)
        self.iud4.hide()

        # indicator size
        self.iud256 = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0,0,0,0)
        label = QtWidgets.QLabel('Indicator Size')
        self.floatSpinBox = QtWidgets.QDoubleSpinBox()
        self.floatSpinBox.setRange(0,2)
        self.floatSpinBox.setDecimals(4)
        self.floatSpinBox.setSingleStep(0.1)
        self.floatSpinBox.setValue(self.widget._size)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.floatSpinBox)
        self.iud256.setLayout(hbox)
        layout.addWidget(self.iud256)
        self.iud256.hide()

        # true state indicator color
        self.iud512 = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0,0,0,0)
        button = QtWidgets.QLabel('True State Indicator Color')
        self.trueColorButton = QtWidgets.QPushButton()
        self.trueColorButton.setToolTip('Opens color dialog')
        self.trueColorButton.clicked.connect(self.on_trueColorClick)
        self.trueColorButton.setStyleSheet('QPushButton {background-color: %s ;}'% self.widget._on_color.name())
        self._onColor = self.widget._on_color.name()
        hbox.addWidget(button)
        hbox.addWidget(self.trueColorButton)
        self.iud512.setLayout(hbox)
        layout.addWidget(self.iud512)
        self.iud512.hide()

        # False state indicator color
        self.iud1024 = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0,0,0,0)
        button = QtWidgets.QLabel('Falsee State Indicator Color')
        self.falseColorButton = QtWidgets.QPushButton()
        self.falseColorButton.setToolTip('Opens color dialog')
        self.falseColorButton.clicked.connect(self.on_falseColorClick)
        self.falseColorButton.setStyleSheet('QPushButton {background-color: %s ;}'% self.widget._off_color.name())
        self._offColor = self.widget._off_color.name()
        hbox.addWidget(button)
        hbox.addWidget(self.falseColorButton)
        self.iud1024.setLayout(hbox)
        layout.addWidget(self.iud1024)
        self.iud1024.hide()



        # State Text option
        self.iud2 = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0,0,0,0)
        label = QtWidgets.QLabel('<b>State Text Option<\b?')
        self.textCheckBox = QtWidgets.QCheckBox()
        self.textCheckBox.setChecked(self.widget._state_text)
        self.textCheckBox.clicked.connect(self.onSetOptions)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.textCheckBox)
        self.iud2.setLayout(hbox)
        layout.addWidget(self.iud2)
        #self.iud2.hide()

        # True text edit box
        self.iud64 = QtWidgets.QWidget()
        vbox = QtWidgets.QVBoxLayout()
        vbox.setContentsMargins(0,0,0,0)
        label = QtWidgets.QLabel('True State Text Line')
        self.tTextEditBox = QtWidgets.QLineEdit()
        self.tTextEditBox.setText(self.widget._true_string)
        vbox.addWidget(label)
        vbox.addWidget(self.tTextEditBox)
        self.iud64.setLayout(vbox)
        layout.addWidget(self.iud64)
        self.iud64.hide()

        # False text edit box
        self.iud128 = QtWidgets.QWidget()
        vbox = QtWidgets.QVBoxLayout()
        vbox.setContentsMargins(0,0,0,0)
        label = QtWidgets.QLabel('False State Text Line')
        self.fTextEditBox = QtWidgets.QLineEdit()
        self.fTextEditBox.setText(self.widget._false_string)
        vbox.addWidget(label)
        vbox.addWidget(self.fTextEditBox)
        self.iud128.setLayout(vbox)
        layout.addWidget(self.iud128)
        self.iud128.hide()



        # Python Text option
        self.iud8 = QtWidgets.QWidget()
        hbox = QtWidgets.QHBoxLayout()
        hbox.setContentsMargins(0,0,0,0)
        label = QtWidgets.QLabel('<b>Python Command Option<\b>')
        self.pythonCheckBox = QtWidgets.QCheckBox()
        self.pythonCheckBox.setChecked(self.widget._python_command)
        self.pythonCheckBox.clicked.connect(self.onSetOptions)
        hbox.addWidget(label)
        hbox.addStretch(1)
        hbox.addWidget(self.pythonCheckBox)
        self.iud8.setLayout(hbox)
        layout.addWidget(self.iud8)
        #self.iud8.hide()

        # True command edit box
        self.iud16 = QtWidgets.QWidget()
        vbox = QtWidgets.QVBoxLayout()
        vbox.setContentsMargins(0,0,0,0)
        label = QtWidgets.QLabel('True State Command Line')
        self.tCommandEditBox = QtWidgets.QLineEdit()
        self.tCommandEditBox.setText(self.widget.true_python_command)
        vbox.addWidget(label)
        vbox.addWidget(self.tCommandEditBox)
        self.iud16.setLayout(vbox)
        layout.addWidget(self.iud16)
        self.iud16.hide()

        # False command edit box
        self.iud32 = QtWidgets.QWidget()
        vbox = QtWidgets.QVBoxLayout()
        vbox.setContentsMargins(0,0,0,0)
        label = QtWidgets.QLabel('False State Command Line')
        self.fCommandEditBox = QtWidgets.QLineEdit()
        self.fCommandEditBox.setText(self.widget.false_python_command)
        vbox.addWidget(label)
        vbox.addWidget(self.fCommandEditBox)
        self.iud32.setLayout(vbox)
        layout.addWidget(self.iud32)
        self.iud32.hide()

        self.tab2.setLayout(layout)

    def onSetOptions(self):
        if self.textTemplateCheckBox.isChecked():
            self.vbox.show()
        else:
            self.vbox.hide()

        if self.indicatorCheckBox.isChecked():
            self.iud4.show()
            self.iud256.show()
            self.iud512.show()
            self.iud1024.show()
        else:
            self.iud4.hide()
            self.iud256.hide()
            self.iud512.hide()
            self.iud1024.hide()

        if self.pythonCheckBox.isChecked():
            self.iud16.show()
            self.iud32.show()
        else:
            self.iud16.hide()
            self.iud32.hide()

        if  self.textCheckBox.isChecked():
            self.iud64.show()
            self.iud128.show()
        else:
            self.iud64.hide()
            self.iud128.hide()

        self.adjustSize()

    def on_trueColorClick(self):
        color = QtWidgets.QColorDialog.getColor()
        if color.isValid():
            self._onColor = color.name()
            self.trueColorButton.setStyleSheet('QPushButton {background-color: %s ;}'% self._onColor)

    def on_falseColorClick(self):
        color = QtWidgets.QColorDialog.getColor()
        if color.isValid():
            self._offColor = color.name()
            self.falseColorButton.setStyleSheet('QPushButton {background-color: %s ;}'% self._offColor)

    def updateWidget(self):
        i = self.combo.currentIndex()
        winProperty = self.combo.itemData(i,role = QtCore.Qt.UserRole + 1)
        if winProperty is None:
            self.combo.select(0,0)
            return
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
            formWindow.cursor().setProperty('indicator_size',
              QtCore.QVariant(self.floatSpinBox.value()))
            formWindow.cursor().setProperty('indicator_size',
              QtCore.QVariant(self.floatSpinBox.value()))
            formWindow.cursor().setProperty('indicator_size',
              QtCore.QVariant(self.floatSpinBox.value()))


            formWindow.cursor().setProperty('on_color',
              QtCore.QVariant(self._onColor))
            formWindow.cursor().setProperty('off_color',
              QtCore.QVariant(self._offColor))
            formWindow.cursor().setProperty('true_python_cmd_string',
              QtCore.QVariant(self.tCommandEditBox.text()))
            formWindow.cursor().setProperty('false_python_cmd_string',
              QtCore.QVariant(self.fCommandEditBox.text()))

        if formWindow and winProperty =='unused' :
            formWindow.cursor().setProperty('estop_action',
                QtCore.QVariant(True))
            formWindow.cursor().setProperty('estop_action',
                QtCore.QVariant(False))
        elif formWindow:
            # set widget option
            formWindow.cursor().setProperty(winProperty+'_action',
              QtCore.QVariant(True))

        # set related data
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
          QtCore.QVariant( self.floatAltSpinBox.value()))
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
        self.editIndicatorAction = QtWidgets.QAction(
          self.tr("Set Indicator Actions"), self)
        self.editIndicatorAction.triggered.connect(self.indicatorOptions)

    def preferredEditAction(self):
        return self.editStateAction

    def taskActions(self):
        return [self.editStateAction,self.editIndicatorAction]

    def updateOptions(self):
        dialog = ActionButtonDialog(self.widget)
        dialog.exec_()

    def indicatorOptions(self):
        dialog = IndicatorButtonDialog(self.widget)
        dialog.exec_()

class ActionButtonTaskMenuFactory(QExtensionFactory):
    def __init__(self, parent = None):
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
    def __init__(self, parent = None):
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
