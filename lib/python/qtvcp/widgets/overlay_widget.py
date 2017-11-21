#!/usr/bin/python2.7

import os
from PyQt4.QtGui import *
from PyQt4.QtCore import *
from qtvcp.widgets.simple_widgets import _HalWidgetBase
from qtvcp.qt_glib import GStat
GSTAT = GStat()

# Set up logging
from qtvcp import logger
log = logger.getLogger(__name__)


class OverlayWidget(QWidget):
    def __init__(self, parent=None):
        self.last = None
        self.top_level = parent
        super(OverlayWidget, self).__init__(parent)
        self.parent = parent
        self.setAttribute(Qt.WA_NoSystemBackground)
        self.setAttribute(Qt.WA_TransparentForMouseEvents)
        self.setWindowFlags( self.windowFlags() |Qt.Tool |
                        Qt.FramelessWindowHint | Qt.Dialog |
                             Qt.WindowStaysOnTopHint |Qt.WindowSystemMenuHint)

    # Find the top level widget and add an event filter
    def newParent(self):
        if not self.parent: return
        self.parent = self.top_level
        if not self.last == None:
            log.debug('last removed: {}'.format(self.last))
            self.last.removeEventFilter(self)
        self.parent.installEventFilter(self)
        self.last = self.parent
        self.raise_()

    def eventFilter(self, obj, event):
        #print event,'parent',self.parent
        if obj == self.parent:
            #Catches resize and child events from the parent widget
            if event.type() == QEvent.Resize:
                self.resize(QResizeEvent.size(event))
                self.raise_()
            elif event.type() == QEvent.Move:
                self.move(QMoveEvent.pos(event))
                self.raise_()
            elif(event.type() == QEvent.ChildAdded):
                #print 'CHILD',QChildEvent.child(event)
                if not QChildEvent.child(event) is QDialog:
                    self.raise_()
            if event.type() == QEvent.Close:
                self.hide()
                self.closeEvent(event)
                event.accept()
        return super(OverlayWidget,self).eventFilter(obj, event)

    # Tracks parent widget changes
    def event(self, event):
        #print 'overlay:',event
        if event.type() == QEvent.ParentAboutToChange:
            #print 'REMOVE FILTER'
            self.parent.removeEventFilter()
            return True
        if event.type() == QEvent.ParentChange:
            #print 'parentEVENT:', self.parentWidget()
            self.newParent()
            return True
        if event.type() == QEvent.Paint:
            self.paintEvent(event)
            return True
        return False

class FocusOverlay(OverlayWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(FocusOverlay, self).__init__(parent)
        self.setAttribute(Qt.WA_TranslucentBackground)
        self.bg_color = QColor(0, 0, 0,150)
        self.text_color = QColor(0,0,0)
        self.dialog_color = QColor(255,255,255)
        self.font = QFont("arial,helvetica", 40)
        self.text = "Loading..."
        self._state = False
        self._image_path = '/home/chris/emc-dev/linuxcnc2.gif'
        self._image = QImage(os.path.join(os.path.expanduser('~'), self._image_path))
        self._show_buttons = False
        self._show_image = False
        self._image_transp = 0.3
        self.box()

    def _hal_init(self):
        self.top_level = self.QTVCP_INSTANCE_
        self.newParent()

        def _f(data,text,color):
            if data:
                if color:
                    self.bg_color = color
                else:
                    self.bg_color = QColor(0, 0, 0,150)
                if text:
                    self.text = text
                self.show()

            else:
                self.hide()
        GSTAT.connect('focus-overlay-changed', lambda w, data, text, color: _f(data, text, color))

    def paintEvent(self, event):
        qp = QPainter()
        qp.begin(self)
        self.colorBackground(qp)
        if self._show_image:
            qp.setOpacity(self._image_transp)
            qp.drawImage(self.rect(), self._image)
        qp.setOpacity(1.0)
        self.drawText(event, qp)
        qp.end()
        self.mb.setText('<html><head/><body><p><span style=" font-size:30pt; font-weight:600;">%s</span></p></body></html>'%self.text)
    #################################################
    # Helper functions
    #################################################

    def colorBackground(self, qp):
        qp.fillRect(self.rect(),self.bg_color)

    def box(self):
        self.mb=QLabel('<html><head/><body><p><span style=" font-size:30pt; font-weight:600;">%s</span></p></body></html>'%self.text,self)
        self.mb.setStyleSheet("background-color: black; color: white")
        self.mb.setAlignment(Qt.AlignVCenter | Qt.AlignCenter)
        hbox = QHBoxLayout()
        hbox.addStretch(1)
        if self._show_buttons:
            okButton = QPushButton("OK")
            okButton.pressed.connect(self.okChecked)
            cancelButton = QPushButton("Cancel")
            cancelButton.pressed.connect(self.cancelChecked)
            hbox.addWidget(okButton)
            hbox.addWidget(cancelButton)
        vbox = QVBoxLayout()
        vbox.addStretch(1)
        vbox.addWidget(self.mb)
        vbox.addLayout(hbox)
        self.setLayout(vbox)
        self.setGeometry(300, 300, 300, 150)

    def okChecked(self):
        self.hide()
    def cancelChecked(self):
        pass

    def drawText(self, event, qp):
        return
        size = self.size()
        w = size.width()
        h = size.height()
        #qp.fillRect(100,100,w/2,h/2,self.dialog_color)
        qp.setPen(self.text_color)
        qp.setFont(self.font)
        qp.drawText(self.rect(), Qt.AlignCenter, self.text)

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    ########################################################################

    @pyqtSlot(bool)
    def setState(self, value):
        self._state = value
        if value:
            self.show()
        else:
            self.hide()
    def getState(self):
        return self._state
    def resetState(self):
        self._state = False

    def getOverayColor(self):
        return self.bg_color
    @pyqtSlot(QColor)
    def setOverayColor(self, value):
        self.bg_color = value
    def resetOverayColor(self, value):
        self.bg_color = value

    def setshow_image(self, data):
        self._show_image = data
        #self.hide()
        #self.show()
    def getshow_image(self):
        return self._show_image
    def resetshow_image(self):
        self._show_image = False

    def set_image_transp(self, data):
        self._image_transp = data
        #self.hide()
        #self.show()
    def get_image_transp(self):
        return self._image_transp
    def reset_image_transp(self):
        self._image_transp = 0.3

    def setshow_buttons(self, data):
        self._show_buttons = data
        #self.hide()
        #self.show()
    def getshow_buttons(self):
        return self._show_buttons
    def resetshow_buttons(self):
        self._show_buttons = False

    def setimage_path(self, data):
        self._image_path = data
        try:
            self._image = QImage(os.path.join(os.path.expanduser('~'), self._image_path))
        except:
            pass
    def getimage_path(self):
        return self._image_path
    def resetimage_path(self):
        self._image_path = False

    overlay_color = pyqtProperty(QColor, getOverayColor, setOverayColor,resetOverayColor)
    state = pyqtProperty(bool, getState, setState, resetState)
    show_image_option = pyqtProperty(bool, getshow_image, setshow_image, resetshow_image)
    image_transparency = pyqtProperty(float, get_image_transp, set_image_transp, reset_image_transp)
    show_buttons_option = pyqtProperty(bool, getshow_buttons, setshow_buttons, resetshow_buttons)
    image_path = pyqtProperty(str, getimage_path, setimage_path, resetimage_path)

#################
# Testing
#################
def main():
    import sys
    app = QApplication(sys.argv)

    w = QWidget()
    #w.setWindowFlags( Qt.FramelessWindowHint | Qt.Dialog | Qt.WindowStaysOnTopHint )
    w.setGeometry(300, 300, 250, 150)
    w.setWindowTitle('Test')

    l = QLabel('Hello, world!',w)
    l.show()
    log.debug('Label: {}'.format(l))

    o = LoadingOverlay(l)
    o.setObjectName("overlay")
    w.show()
    #o.show()


    timer2 = QTimer()
    timer2.timeout.connect(lambda : o.show())
    timer2.start(1500)

    sys.exit(app.exec_())


if __name__ == '__main__':
    main()


