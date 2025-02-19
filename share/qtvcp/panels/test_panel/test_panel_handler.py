############################
# **** IMPORT SECTION **** #
############################
import sys
import shutil
from PyQt5.QtCore import *
from PyQt5.QtGui import *
from PyQt5.QtWidgets import *
from qtvcp.core import Status
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

        self.timer = QTimer()
        self.timer.timeout.connect(self.announceLEDText)

        self.lastLED_1 = 0
        self.lastLED_2 = 0
        self.lastLED_3 = 0
        self.lastLED_4 = 0

        self.timer.start(1000)

    ##########################################
    # Special Functions called from QTVCP
    ##########################################

    # at this point:
    # the widgets are instantiated.
    # the HAL pins are built but HAL is not set ready
    def initialized__(self):

        # make sure scalewidgets get proper starting values
        self.w.dial_1.valueChanged.emit(self.w.dial_1.value())
        self.w.dial_2.valueChanged.emit(self.w.dial_2.value())
        self.w.dial_3.valueChanged.emit(self.w.dial_3.value())
        self.w.dial_4.valueChanged.emit(self.w.dial_4.value())

        # speak text when return pressed
        self.w.lineEdit_led_1.returnPressed.connect(lambda : self.announceText(self.w.lineEdit_led_1))
        self.w.lineEdit_led_2.returnPressed.connect(lambda : self.announceText(self.w.lineEdit_led_2))
        self.w.lineEdit_led_3.returnPressed.connect(lambda : self.announceText(self.w.lineEdit_led_3))
        self.w.lineEdit_led_4.returnPressed.connect(lambda : self.announceText(self.w.lineEdit_led_4))

        if shutil.which('urxvt') is not None:
            term = embterminal(name='urxvt')
            self.w.dockWidget.setWidget(term)
        elif shutil.which('xterm') is not None:
            term = embterminal(name='xterm')
            self.w.dockWidget.setWidget(term)
        else:
            self.w.dockWidget.setWidget(
                QLabel('''terminal program urxvt not available. \
Try sudo apt install rxvt-unicode-256color'''))

    ########################
    # callbacks from STATUS #
    ########################

    #######################
    # callbacks from form #
    #######################
    def actionTriggered(self, data):
        if data == self.w.actionSetD1_50_50:
            self.w.dial_1.setMinimum(-50)
            self.w.dial_1.setMaximum(50)
            self.w.dockWidget_2.setWindowTitle('Dial 1 (+-50)')
        elif data == self.w.actionSetD1_100_100:
            self.w.dial_1.setMinimum(-100)
            self.w.dial_1.setMaximum(100)
            self.w.dockWidget_2.setWindowTitle('Dial 1 (+-100)')
        elif data == self.w.actionSetD1_1000_1000:
            self.w.dial_1.setMinimum(-1000)
            self.w.dial_1.setMaximum(1000)
            self.w.dockWidget_2.setWindowTitle('Dial 1 (+-1000)')
        elif data == self.w.actionSetD1_0_100:
            self.w.dial_1.setMinimum(0)
            self.w.dial_1.setMaximum(100)
            self.w.dockWidget_2.setWindowTitle('Dial 1 (0-100)')
        elif data == self.w.actionSetD1_0_360:
            self.w.dial_1.setMinimum(0)
            self.w.dial_1.setMaximum(360)
            self.w.dockWidget_2.setWindowTitle('Dial 1 (0-360)')
        elif data == self.w.actionSetD1_0_1000:
            self.w.dial_1.setMinimum(0)
            self.w.dial_1.setMaximum(1000)
            self.w.dockWidget_2.setWindowTitle('Dial 1 (0-1000)')

        elif data == self.w.actionSetD2_50_50:
            self.w.dial_2.setMinimum(-50)
            self.w.dial_2.setMaximum(50)
            self.w.dockWidget_3.setWindowTitle('Dial 2 (+-50)')
        elif data == self.w.actionSetD2_100_100:
            self.w.dial_2.setMinimum(-100)
            self.w.dial_2.setMaximum(100)
            self.w.dockWidget_3.setWindowTitle('Dial 1 (+-100)')
        elif data == self.w.actionSetD2_1000_1000:
            self.w.dial_2.setMinimum(-1000)
            self.w.dial_2.setMaximum(1000)
            self.w.dockWidget_3.setWindowTitle('Dial 1 (+-1000)')
        elif data == self.w.actionSetD2_0_100:
            self.w.dial_2.setMinimum(0)
            self.w.dial_2.setMaximum(100)
            self.w.dockWidget_3.setWindowTitle('Dial 2 (0-100)')
        elif data == self.w.actionSetD2_0_360:
            self.w.dial_2.setMinimum(0)
            self.w.dial_2.setMaximum(360)
            self.w.dockWidget_3.setWindowTitle('Dial 2 (0-360)')
        elif data == self.w.actionSetD2_0_1000:
            self.w.dial_2.setMinimum(0)
            self.w.dial_2.setMaximum(1000)
            self.w.dockWidget_3.setWindowTitle('Dial 2 (0-1000)')

    def announceLEDText(self):
        # speak led label contents on state change, if checked
        for i in range(1,5):
            if self.w['actionLED_{}'.format(i)].isChecked():
                data = self.w['led_{}'.format(i)].getState()
                if self['lastLED_{}'.format(i)] != data:
                    name = self.w['lineEdit_led_{}'.format(i)].text()
                    STATUS.emit('play-sound', 'SPEAK {} {}'.format(name,data))
                    self['lastLED_{}'.format(i)] = data

    def announceText(self, widget):
        STATUS.emit('play-sound', 'SPEAK {}'.format(widget.text()))

    #####################
    # general functions #
    #####################

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

class embterminal(QWidget):

    def __init__(self, parent=None, name=None):
        super(embterminal, self).__init__(parent)
        self.process = QProcess(self)

        self.w = QWindow()
        self.terminal = QWidget.createWindowContainer(self.w)
        self.terminal.setMinimumSize(550,100)
        self.terminal.setMaximumSize(550,100)

        layout = QVBoxLayout(self)
        layout.addWidget(self.terminal)

        size_policy = QSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.setSizePolicy(size_policy)

        # xterm
        if name == 'xterm':
            self.process.start(
                'xterm',['-bg','black','-fg','green', '-cr','green',
                '-bd', 'green', '-fa', 'Monospace', '-fs', '10',
                '-into', str(int(self.w.winId()))])
        else:
            # Works also with urxvt:
            self.process.start(
                'urxvt',['-bg','black','-fg','green', '-cr','green',
                '-bd', 'green', '--font', 'xft:Monospace:size=10', '-embed', str(int(self.w.winId()))])

    def sizeHint(self):
        return QSize(550, 100)

################################
# required handler boiler code #
################################

def get_handlers(halcomp,widgets,paths):
     return [HandlerClass(halcomp,widgets,paths)]
