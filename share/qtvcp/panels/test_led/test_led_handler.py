############################
# **** IMPORT SECTION **** #
############################
import sys
from PyQt5.QtCore import *
from PyQt5.QtGui import *
from PyQt5.QtWidgets import *
from qtvcp.core import Status
from qtvcp.widgets.hal_selectionbox import HALSelectionBox
from qtvcp.widgets.led_widget import LED
###########################################
# **** instantiate libraries section **** #
###########################################
STATUS = Status()
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
        self.ledDict = dict()
        STATUS.connect('periodic', lambda w: self.watch_halpin())
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
                print('Error with test_led number selection - not a number - using 1')
                num = 1

            if num >1:
                for i in range(num-1):
                    self.addLED()
                    # make window taller now
                    x = self.w.geometry().x()
                    y = self.w.geometry().y()
                    w = self.w.geometry().width()
                    h = self.w.geometry().height() + 37
                    self.w.setGeometry(x,y,w,h)
                    self.w.setMinimumHeight(h)

        self.addLED()
        self.w.setWindowFlags(Qt.WindowStaysOnTopHint)
        self.w.setWindowTitle('{}'.format(self.h.comp.getprefix()))

    # build a LED/controls line
    def addLED(self):

        # new toolbar added to window
        toolbar = QToolBar(self.w)
        self.w.addToolBar(Qt.LeftToolBarArea, toolbar)

        # containers
        w = QWidget()
        w.setContentsMargins(0,0,0,0)
        hbox = QHBoxLayout(w)
        hbox.setContentsMargins(0,0,0,0)

        # for led name
        le = QLineEdit()
        le.setText(self.h.comp.getprefix())
        # speak text when return pressed
        le.returnPressed.connect(lambda : self.announceText(le))

        # LED to display state
        led = LED()
        led._halpin_option = False
        led.hal_init('led')
        led.setMaximumWidth(30)

        hbox.addWidget(led)
        hbox.addWidget(le)

        # menu for controls
        menu = QMenu()
        menu.setMinimumWidth(259)

        # sound option checkbutton
        actionSound = QAction('Sound',menu)
        actionSound.setCheckable(True)
        actionSound.triggered.connect(lambda b: self.actionTriggered('Sound',None))

        # remember signal watched, line edit widget, led widget and last state
        self.ledDict[led] = [None,le,actionSound,led,False]

        # color option launch button
        actionColor = QAction('Color',menu)
        actionColor.triggered.connect(lambda b: self.actionTriggered('Color', led))

        menu.addAction(actionSound)
        menu.addAction(actionColor)

        # button to pop menu
        btn = QPushButton('Opt')
        btn.setMaximumWidth(50)
        btn.setMenu(menu)

        hbox.addWidget(btn)

        # combo box for HAL pin selection
        cb = HALSelectionBox()
        cb.setShowTypes([cb.PINS,cb.SIGNALS])
        cb.setPinTypes([cb.HAL_BIT])
        cb.setSignalTypes([cb.HAL_BIT], driven = [False,True])
        cb.hal_init()
        cb.selectionUpdated.connect(lambda w: self.signalSelected(w,led))

        # wrap combobox so as to add it to menu
        action = QWidgetAction(menu)
        action.setDefaultWidget(cb)
        menu.addAction(action)

        # button to add another LED toolbar
        actionAdd = QAction('Add LED',menu)
        actionAdd.triggered.connect(lambda b: self.actionTriggered('Add', None))
        menu.addAction(actionAdd)

        toolbar.addWidget(w)

    ########################
    # callbacks from STATUS #
    ########################

    #######################
    # callbacks from form #
    #######################

    def actionTriggered(self, widget,led):
        if widget == 'Color':
            # Get a color from the text dialog
            color = QColorDialog.getColor()
            led.setProperty('color',color)

        elif widget == 'Add':
            # add a toolbar with LED and controls
            self.addLED()
            # make window taller now
            x = self.w.geometry().x()
            y = self.w.geometry().y()
            w = self.w.geometry().width()
            h = self.w.geometry().height() + 37
            self.w.setGeometry(x,y,w,h)
            self.w.setMinimumHeight(h)

    def announceLEDText(self,ledstate,linetext,sound):
        # speak led label contents on state change, if checked
        if sound.isChecked():
            name = linetext.text()
            STATUS.emit('play-sound', 'SPEAK {} {}'.format(name,ledstate))

    def announceText(self, widget):
        STATUS.emit('play-sound', 'SPEAK {}'.format(widget.text()))

    #####################
    # general functions #
    #####################

    def updateLabel(self,v):
        self.w.hallabel.setDisplay(v)

    def signalSelected(self, sig,led):
        #print('Watching:',sig)
        if sig != '':
            self.ledDict[led][0] = sig
        return

    def watch_halpin(self):
        for i in self.ledDict:
            sig = self.ledDict[i][0]
            if sig is not None:
                try:
                    state = self.h.hal.get_value(sig)
                except:
                    return
                lastState = self.ledDict[i][4]
                if state != lastState:
                    lineEdit = self.ledDict[i][1]
                    actionSound = self.ledDict[i][2]
                    led =  self.ledDict[i][3]
                    if actionSound:
                        self.announceLEDText(state,lineEdit,actionSound)
                    led.change_state(state)
                    self.ledDict[i][4] = state
            else:
                state = 'None'

            #print(self.ledDict[i][0],state)

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
