#!/usr/bin/env python
# Qtvcp camview
#
# Copyright (c) 2017  Chris Morley <chrisinnanaimo@hotmail.com>
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
#
# use open cv to do camera alignment

import sys
import thread as Thread

from PyQt5 import QtWidgets, QtCore
from PyQt5.QtGui import QColor, QFont, QPainter, QPen, QImage

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp import logger

# Instiniate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# LOG is for running code logging
if __name__ != '__main__':  # This avoids segfault when testing directly in python
    from qtvcp.core import Status
    STATUS = Status()
LOG = logger.getLogger(__name__)

# If the library is missing don't crash the GUI
# send an error and just make a blank widget.
LIB_GOOD = True
try:
    import cv2
except:
    LOG.error('Qtvcp Error with camview - is python-opencv installed?')
    LIB_GOOD = False




class CamView(QtWidgets.QWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(CamView, self).__init__(parent)
        self.video = None
        self.grabbed = None
        self.frame = None
        self.diameter = 20
        self.rotation = 0
        self.scale = 1
        self.gap = 5
        self.setWindowTitle('Cam View')
        self.setGeometry(100, 100, 200, 200)
        self.text_color = QColor(255, 255, 255)
        self.font = QFont("arial,helvetica", 40)
        self.text = ''
        self.pix = None
        self.stopped = False

    def _hal_init(self):
        if LIB_GOOD:
            STATUS.connect('periodic', self.nextFrameSlot)

    ##################################
    # no button scroll = circle dismater
    # left button scroll = zoom
    # right button scroll = cross hair rotation
    ##################################
    def wheelEvent(self, event):
        super(CamView, self).wheelEvent(event)
        mouse_state = QtWidgets.qApp.mouseButtons()
        size = self.size()
        w = size.width()
        if event.angleDelta().y() < 0:
            if mouse_state == QtCore.Qt.NoButton:
                self.diameter -= 2
            if mouse_state == QtCore.Qt.LeftButton:
                self.scale -= .1
            if mouse_state == QtCore.Qt.RightButton:
                self.rotation -= 2
        else:
            if mouse_state == QtCore.Qt.NoButton:
                self.diameter += 2
            if mouse_state == QtCore.Qt.LeftButton:
                self.scale += .1
            if mouse_state == QtCore.Qt.RightButton:
                self.rotation += 2
        if self.diameter < 2: self.diameter = 2
        if self.diameter > w: self.diameter = w
        if self.rotation > 360: self.rotation = 0
        if self.rotation < 0: self.rotation = 360
        if self.scale < 1: self.scale = 1
        if self.scale > 5: self.scale = 5

    def nextFrameSlot(self, w):
        if not self.video: return
        if not self.isVisible(): return

        ############################
        # capture a freme from cam
        ############################
        ret, frame = self.video.read()
        if not ret: return
        (oh, ow) = frame.shape[:2]

        #############################
        # scale image bigger
        #############################
        scale = self.scale
        frame = cv2.resize(frame, None, fx=scale, fy=scale, interpolation=cv2.INTER_CUBIC)

        ##########################
        # crop to the original size of the frame
        # measure from center so we zoom on center
        # ch = center of current height
        # coh = center of original height
        ##########################
        (h, w) = frame.shape[:2]
        ch = h/2
        cw = w/2
        coh = oh/2
        cow = ow/2
        # NOTE: its img[y: y + h, x: x + w]
        frame = frame[ch-coh:ch+coh, cw-cow:cw+cow]

        ########################################
        # My webcam yields frames in BGR format
        # this may need other options for other cameras
        ########################################
        frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        # fit to our window frame
        self.pix = QImage(frame, frame.shape[1], frame.shape[0], QImage.Format_RGB888)
        # repaint the window
        self.update()

    def showEvent(self, event):
        if LIB_GOOD:
            try:
                self.video = WebcamVideoStream(src=0).start()
            except:
                LOG.error('Video capture error: {}'.format(self.video))

    def hideEvent(self, event):
        if LIB_GOOD:
            try:
                self.video.stop()
            except:
                pass

    def paintEvent(self, event):
        qp = QPainter()
        qp.begin(self)
        if self.pix:
            qp.drawImage(self.rect(), self.pix)
        self.drawText(event, qp)
        self.drawCircle(event, qp)
        self.drawCrossHair(event, qp)
        qp.end()

    def drawText(self, event, qp):
        size = self.size()
        w = size.width()
        h = size.height()
        qp.setPen(self.text_color)
        qp.setFont(self.font)
        if self.pix:
            text = self.text
        else:
            text = 'No Image'
        qp.drawText(self.rect(), QtCore.Qt.AlignCenter, text)

    def drawCircle(self, event, gp):
        size = self.size()
        w = size.width()
        h = size.height()
        radx = self.diameter/2
        rady = self.diameter/2
        # draw red circles
        gp.setPen(QtCore.Qt.red)
        center = QtCore.QPoint(w/2, h/2)
        gp.drawEllipse(center, radx, rady)

    def drawCrossHair(self, event, gp):
        size = self.size()
        w = size.width()/2
        h = size.height()/2
        pen = QPen(QtCore.Qt.yellow, 1, QtCore.Qt.SolidLine)
        gp.setPen(pen)
        gp.translate(w, h)
        gp.rotate(self.rotation)
        gp.drawLine(-w, 0, 0-self.gap, 0)
        gp.drawLine(0+self.gap, 0, w, 0)
        gp.drawLine(0, 0+self.gap, 0, h)
        gp.drawLine(0, 0-self.gap, 0, -h)


class WebcamVideoStream:
    def __init__(self, src=0):
        # initialize the video camera stream and read the first frame
        # from the stream
        self.stream = cv2.VideoCapture(src)
        # initialize the variable used to indicate if the thread should
        # be stopped
        self.stopped = False
        self.grabbed = None
        self.frame = None

    def start(self):
        # start the thread to read frames from the video stream
        Thread.start_new_thread(self._update, ())
        return self

    def _update(self):
        # keep looping infinitely until the thread is stopped
        while True:
            # if the thread indicator variable is set, stop the thread
            if self.stopped:
                self.stream.release()
                return
            # otherwise, read the next frame from the stream
            (self.grabbed, self.frame) = self.stream.read()

    def read(self):
        # return the frame most recently read
        return (self.grabbed, self.frame)

    def stop(self):
        # indicate that the thread should be stopped
        self.stopped = True

if __name__ == '__main__':

    import sys
    app = QtWidgets.QApplication(sys.argv)
    capture = CamView()
    capture.show()

    def jump():
        capture.nextFrameSlot(None)
    timer = QtCore.QTimer()
    timer.timeout.connect(jump)
    timer.start(10)
    sys.exit(app.exec_())
