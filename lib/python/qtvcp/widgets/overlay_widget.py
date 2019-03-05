#!/usr/bin/python2.7
#
# Qtvcp widget
# Copyright (c) 2017 Chris Morley
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
###############################################################################

import os

from PyQt5.QtWidgets import QWidget, QLabel, QHBoxLayout, QVBoxLayout, QPushButton, QDialog
from PyQt5.QtCore import Qt, QEvent, pyqtSlot, pyqtProperty, QChildEvent
from PyQt5.QtGui import QColor, QImage, QResizeEvent, QPainter

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status
from qtvcp import logger

# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# LOG is for running code logging
if __name__ != '__main__':
    STATUS = Status()
LOG = logger.getLogger(__name__)

# Set the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


################################################################
# Overlay base class
# based on c++ code on net
# Originally designed to allow an overlay on individual widgets
# which might be nice too but for now just the wholw window
################################################################
class OverlayWidget(QWidget):
    def __init__(self, parent=None):
        self.last = None
        self.top_level = parent
        super(OverlayWidget, self).__init__(parent)
        self.parent = parent
        self.setAttribute(Qt.WA_NoSystemBackground)
        self.setAttribute(Qt.WA_TransparentForMouseEvents)
#        self.setWindowFlags( self.windowFlags() |Qt.Tool |
#                        Qt.FramelessWindowHint | Qt.Dialog |
#                             Qt.WindowStaysOnTopHint |Qt.WindowSystemMenuHint)

    # There seems to be no way to get the very top level widget
    # in QT the parent widget can be owned in another parent
    # this addes an event filter the widget in 'top_level'
    # so we can track what is happenning to it.
    def newParent(self):
        self.parent = self.top_level
        self.last = self.parent
        if self.last is not None:
            LOG.debug('last removed: {}'.format(self.last))
            self.last.removeEventFilter(self)
        if not self.parent: return
        self.parent.installEventFilter(self)
        #self.raise_()

    # Here we track parent window events that we need  to react to.
    # then we pass then to the original widget
    def eventFilter(self, obj, event):
        # might be useful events to know
        # 103 window blocked by modal dialog
        # 99 activation changed
        # 25 deactivated
        # 24 activated
        #print event,'parent',self.parent
        #print event,'Event Type',self.event.type()
        if obj == self.parent:
            #Catches resize and child events from the parent widget
            if event.type() == QEvent.Resize:
                #print 'resize'
                self.resize(QResizeEvent.size(event))
            #elif event.type() == QEvent.Move:
                #self.move(QMoveEvent.pos(event))
            elif(event.type() == QEvent.ChildAdded):
                #print 'CHILD',QChildEvent.child(event)
                if not QChildEvent.child(event) is QDialog:
                    self.raise_()
            if event.type() == QEvent.Close:
                self.hide()
                self.closeEvent(event)
                event.accept()
        return super(OverlayWidget, self).eventFilter(obj, event)

    # Tracks our instanance of focus widget changes
    # most importantly the paint event
    # if the parent changes it adjusts the event filter
    # but in our case the parent won't because we are
    # tracking the main window rather the the widget focus is in
    def event(self, event):
        #print 'overlay:',event
        if event.type() == QEvent.ParentAboutToChange:
            #print 'REMOVE FILTER'
            self.parent.removeEventFilter(self)
            return True
        if event.type() == QEvent.ParentChange:
            #print 'parentEVENT:', self.parentWidget()
            self.newParent()
            return True
        if event.type() == QEvent.Paint:
            self.paintEvent(event)
            return True
        return False


####################################################################
# subclass for using as a fancy dialog focus overlay
####################################################################
class FocusOverlay(OverlayWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(FocusOverlay, self).__init__(parent)
        self.setAttribute(Qt.WA_TranslucentBackground)
        self.bg_color = QColor(0, 0, 0, 150)
        self.text = "Loading..."
        self._state = False
        self._image_path = '~/emc-dev/linuxcnc-wizard.gif'
        self._image = QImage(os.path.expanduser(self._image_path))
        self._show_buttons = False
        self._show_image = False
        self._image_transp = 0.3
        self._show_text = True
        self.box()

    # This is how we plant the very top level parent into
    # our overlay widget
    # QTVCP_INSTANCE is not accessable from Designer
    # it should work with any main window but doesn't.
    # this worked so i stopped looking why
    # this also sets up following show or hide etc based on
    # STATUS messages
    # adjust image path name at runtime
    def _hal_init(self):
        STATUS.connect('focus-overlay-changed', lambda w, data, text, color: 
                        self._status_response(data, text, color))
        if self.PREFS_:
            self.play_sound = self.PREFS_.getpref('overlay_play_sound', False, bool, 'SCREEN_OPTIONS')
            self.sound_type = self.PREFS_.getpref('overlay_sound_type', 'RING', str, 'SCREEN_OPTIONS')
        else:
            self.play_sound = False

        # reparent on top window
        self.top_level = self.QTVCP_INSTANCE_
        self.newParent()

        # adjust size and location to top window
        self.setGeometry(self.top_level.geometry())
        self.hide()

        # look for special path names and change to real path
        if 'STD_IMAGE_DIR/' in self._image_path:
            t = self._image_path.split('STD_IMAGE_DIR/', )[1]
            d = os.path.join(self.PATHS_.IMAGEDIR, t)
        elif 'CONFIG_DIR/' in self._image_path:
            t = self._image_path.split('CONFIG_DIR/', )[1]
            d = os.path.join(self.PATHS_.CONFIGPATH, t)
        else:
            return
        if os.path.exists(d):
            self._image = QImage(d)
        else:
            LOG.debug('Focus Overlay image path runtime error: {}'.format(self._image_path))

    def _status_response(self, data, text, color):
            if data:
                if color:
                    self.bg_color = color
                else:
                    self.bg_color = QColor(0, 0, 0, 150)
                if text:
                    self.text = text
                self.show()
                self.update()
                LOG.debug('Overlay - Show')
                if self.play_sound:
                    STATUS.emit('play-alert', '%s' % self.sound_type)
                #os.system("beep -f 555 ")
            else:
                self.hide()
                LOG.debug('Overlay - Hide')

    # Ok paint everything
    def paintEvent(self, event):
        qp = QPainter()
        qp.begin(self)
        self.colorBackground(qp)
        if self._show_image:
            self.draw_image(qp)
        self.drawText(qp)
        qp.end()

    #################################################
    # Helper functions
    #################################################

    # The basic overlay -transparence is set by bg_color
    def colorBackground(self, qp):
        qp.fillRect(self.rect(), self.bg_color)

    # we can add an image like a watermark transparence set by
    # _image_transp
    def draw_image(self, qp):
        #print self._image, self._image_transp
        qp.setOpacity(self._image_transp)
        qp.drawImage(self.rect(), self._image)

    # This sets the text that was built in the box
    # old code for text is skipped
    def drawText(self, qp):
        qp.setOpacity(1.0)
        self.mb.setText('<html><head/><body><p><span style=" font-size:30pt; \
                        font-weight:600;">%s</span></p></body></html>' % self.text)
        return
        size = self.size()
        w = size.width()
        h = size.height()
        qp.setPen(self.text_color)
        qp.setFont(self.font)
        qp.drawText(self.rect(), Qt.AlignCenter, self.text)

    # build a black label with text and optionally some buttons
    # using a label rather then drawing text allows options suck as rich text
    # and a solid back ground so text is easy to see
    # buttons could be used as a dialog but would require a focus widget per
    # dialog action or some fancy coding for responses.
    def box(self):
        self.mb = QLabel('<html><head/><body><p><span style=" font-size:30pt; \
                       font-weight:600;">%s</span></p></body></html>' % self.text, self)
        self.mb.setStyleSheet("background-color: black; color: white")
        self.mb.setAlignment(Qt.AlignVCenter | Qt.AlignCenter)
        self.mb.setVisible(self._show_text)
        hbox = QHBoxLayout()
        hbox.addStretch(1)

        self.okButton = QPushButton("OK")
        self.okButton.pressed.connect(self.okChecked)
        self.cancelButton = QPushButton("Cancel")
        self.cancelButton.pressed.connect(self.cancelChecked)
        self.okButton.setVisible(self._show_buttons)
        self.cancelButton.setVisible(self._show_buttons)
        l = QLabel()
        hbox.addWidget(l)
        hbox.addWidget(self.okButton)
        hbox.addWidget(self.cancelButton)
        vbox = QVBoxLayout()
        vbox.addStretch(1)
        vbox.addWidget(self.mb)
        vbox.addLayout(hbox)
        self.setLayout(vbox)
        self.setGeometry(300, 300, 300, 150)

    # would need to class patch for something realy useful
    def okChecked(self):
        self.hide()
    def cancelChecked(self):
        self.hide()

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

    def setShowText(self, value):
        self._show_text = value
        self.mb.setVisible(value)
    def getShowText(self):
        return self._show_text
    def resetShowText(self):
        self._show_text = True
        self.mb.setVisible(value)

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
        self.okButton.setVisible(data)
        self.cancelButton.setVisible(data)

    def getshow_buttons(self):
        return self._show_buttons
    def resetshow_buttons(self):
        self._show_buttons = False

    def setimage_path(self, data):
        self._image_path = str(data)
        try:
            path = os.path.expanduser(self._image_path)
            self._image = QImage(path)
        except Exception as e:
            LOG.debug('Focus Overlay image path error: {}'.format(self._image_path))
    def getimage_path(self):
        return self._image_path
    def resetimage_path(self):
        self._image_path = False

    overlay_color = pyqtProperty(QColor, getOverayColor, setOverayColor, resetOverayColor)
    state = pyqtProperty(bool, getState, setState, resetState)
    show_text = pyqtProperty(bool, getShowText, setShowText, resetShowText)
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

    l = QLabel('Hello, world!', w)
    l.show()
    LOG.debug('Label: {}'.format(l))

    # class patching is my new thing
    # class patch the OK button
    def newOk(w):
        print 'Ok'
        w.text = 'OK'
        # make update
        w.hide()
        w.show()
    FocusOverlay.okChecked = newOk

    # could use f = FocusOverlay( w )
    # then dont need to adjust top level
    f = FocusOverlay()
    f.top_level = w
    f.newParent()
    # set with qtproperty call
    f.setshow_buttons(True)

    w.show()

    timer2 = QTimer()
    timer2.timeout.connect(lambda: f.show())
    timer2.start(1500)

    sys.exit(app.exec_())


if __name__ == '__main__':
    main()
