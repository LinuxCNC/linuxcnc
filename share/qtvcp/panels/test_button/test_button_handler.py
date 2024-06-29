############################
# **** IMPORT SECTION **** #
############################
import sys
from PyQt5.QtCore import *
from PyQt5.QtGui import *
from PyQt5.QtWidgets import *
from qtvcp.core import Status
from qtvcp.widgets.hal_selectionbox import HALSelectionBox
from qtvcp.widgets.simple_widgets import PushButton
from qtvcp.lib.aux_program_loader import Aux_program_loader

###########################################
# **** instantiate libraries section **** #
###########################################
STATUS = Status()
EXTPROG = Aux_program_loader()
###################################
# **** HANDLER CLASS SECTION **** #
###################################

class HandlerClass:

    ########################
    # **** INITIALIZE **** #
    ########################
    # widgets allows access to  widgets from the qtvcp files
    # at this point the widgets and hal pins are not instantiated
    def __init__(self, halcomp,widgets,paths):
        self.w = widgets
        self.h = halcomp
        self.buttonDict = dict()

    ##########################################
    # Special Functions called from QTVCP
    ##########################################

    # at this point:
    # the widgets are instantiated.
    # the HAL pins are built but HAL is not set ready
    def initialized__(self):
        geom = self.w.frameGeometry()
        geom.moveCenter(QDesktopWidget().availableGeometry().center())
        self.w.setGeometry(geom)

        if self.w.USEROPTIONS_ is not None:
            try:
                num = int(self.w.USEROPTIONS_[0])
            except:
                print('Error with test_button number selection - not a number - using 1')
                num = 1

            if num >1:
                for i in range(num-1):
                    self.addButton()
                    # make window taller now
                    x = self.w.geometry().x()
                    y = self.w.geometry().y()
                    w = self.w.geometry().width()
                    h = self.w.geometry().height() + 37
                    self.w.setGeometry(x,y,w,h)
                    self.w.setMinimumHeight(h)

        self.addButton()
        self.w.setWindowFlags(Qt.WindowStaysOnTopHint)
        self.w.setWindowTitle('{}'.format(self.h.comp.getprefix()))

    # build a button/controls line
    def addButton(self):

        # new toolbar added to window
        toolbar = QToolBar(self.w)
        self.w.addToolBar(Qt.LeftToolBarArea, toolbar)

        # containers
        w = QWidget()
        w.setContentsMargins(0,0,0,0)
        hbox = QHBoxLayout(w)
        hbox.setContentsMargins(0,0,0,0)

        # for button name
        le = QLineEdit()
        le.setText(self.h.comp.getprefix())

        # HAL button to display state
        button = PushButton()
        button.setProperty('indicator_option',True)
        button._halpin_option = False
        button.hal_init('button')
        button.setMaximumWidth(30)
        self.connectOurSignals(button)

        hbox.addWidget(button)
        hbox.addWidget(le)

        # menu for controls
        menu = QMenu()
        menu.setMinimumWidth(259)

        # Check option checkbutton
        actionCheck = QAction('Set Checkable',menu)
        actionCheck.setCheckable(True)
        actionCheck.triggered.connect(lambda w, b=(actionCheck,button): self.actionTriggered('Checkable',b))
        menu.addAction(actionCheck)

        # color option launch button
        actionColor = QAction('Set Indicator Color',menu)
        actionColor.triggered.connect(lambda w, b=(actionColor,button): self.actionTriggered('Color',b))
        menu.addAction(actionColor)

        # remember signal watched, line edit widget, button widget and last state
        self.buttonDict[button] = [{},le,None,button,False]

        # button to pop menu
        btn = QPushButton('Opt')
        btn.setMaximumWidth(50)
        btn.setMenu(menu)

        hbox.addWidget(btn)

        # combo box for HAL pin selection
        cb = HALSelectionBox()
        cb.setShowTypes([cb.PINS,cb.SIGNALS])
        cb.setPinTypes([cb.HAL_BIT], direction = [cb.HAL_IN])
        cb.setSignalTypes([cb.HAL_BIT], driven = [False,True])
        cb.hal_init()
        cb.objectSelected.connect(lambda w: self.selectedUpdated(w, button))

        # wrap combobox so as to add it to menu
        action = QWidgetAction(menu)
        action.setDefaultWidget(cb)
        menu.addAction(action)

        # HalMeter option launch button
        actionHalMeter = QAction('Load HalMeter',menu)
        actionHalMeter.triggered.connect(lambda w, b=(actionHalMeter, button): self.actionTriggered('HalMeter',b))
        menu.addAction(actionHalMeter)

        # Led option launch button
        actionLed = QAction('Load Test Led',menu)
        actionLed.triggered.connect(lambda w, b=(actionLed,button): self.actionTriggered('Led',b))
        menu.addAction(actionLed)

        # button to add another button toolbar
        actionAdd = QAction('Add Button',menu)
        actionAdd.triggered.connect(lambda b: self.actionTriggered('Add', None))
        menu.addAction(actionAdd)

        toolbar.addWidget(w)

    ########################
    # callbacks from STATUS #
    ########################

    #######################
    # callbacks from form #
    #######################

    def actionTriggered(self, widget,b=None):
        # b[0] is action button
        # b[1] is main button
        if widget == 'Add':
            # add a toolbar with button and controls
            self.addButton()
            # make window taller now
            x = self.w.geometry().x()
            y = self.w.geometry().y()
            w = self.w.geometry().width()
            h = self.w.geometry().height() + 37
            self.w.setGeometry(x,y,w,h)
            self.w.setMinimumHeight(h)
        elif widget == 'Checkable':
            b[1].setCheckable(b[0].isChecked())
            self.connectOurSignals(b[1])
        elif widget == 'Color':
            # Get a color from the text dialog
            color = QColorDialog.getColor()
            b[1].setProperty('on_color',color)
        elif widget == 'HalMeter':
            EXTPROG.load_halmeter()
        elif widget == 'Led':
            EXTPROG.load_test_led()

    #####################
    # general functions #
    #####################

    def updateLabel(self,v):
        self.w.hallabel.setDisplay(v)

    def selectedUpdated(self, meta, button):
        #print('Watching:',meta,self.buttonDict[button])
        self.buttonDict[button][0] = meta

    def _updatePin(self,button, data):
        name = self.buttonDict[button][0].get('NAME')
        obj = self.buttonDict[button][0].get('OBJECT')
        if name is None: return
        if obj == 'pin':
            try:
                self.h.setp(name,str(int(data)))
            except Exception as e:
                print(e)
        elif obj == 'signal':
            try:
                self.h.sets(name,str(int(data)))
            except Exception as e:
                print('QtVCP Testbutton:',e)


    def connectOurSignals(self, button):

        #reconnect indicator class signals
        button.disconnectSignals()
        button.connectSignals()

        if button.isCheckable():
            button.toggled[bool].connect(lambda data,b=button: self._updatePin(b,data))
        else:
            button.pressed.connect(lambda b=button: self._updatePin(b,True))
            button.released.connect(lambda b=button: self._updatePin(b, False))

    #####################
    # KEY BINDING CALLS #
    #####################

    ###########################
    # **** closing event **** #
    ###########################

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)


################################
# required handler boiler code #
################################

def get_handlers(halcomp,widgets,paths):
     return [HandlerClass(halcomp,widgets,paths)]
