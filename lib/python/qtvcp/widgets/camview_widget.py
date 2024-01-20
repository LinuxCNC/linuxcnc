#!/usr/bin/env python3
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

import os
import _thread as Thread

import hal

from PyQt5 import QtWidgets, QtCore
from PyQt5.QtGui import QColor, QFont, QPainter, QPen, QImage

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp import logger

# Instiniate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# LOG is for running code logging
if __name__ != '__main__':  # This avoids segfault when testing directly in python
    from qtvcp.core import Status, Info
    STATUS = Status()
    INFO = Info()
LOG = logger.getLogger(__name__)

# Suppress cryptic messages when checking for useable ports
os.environ["OPENCV_LOG_LEVEL"]="FATAL"

# If the library is missing don't crash the GUI
# send an error and just make a blank widget.
LIB_GOOD = True
try:
    import cv2 as CV
    DEFAULT_API = CV.CAP_ANY
except:
    LOG.error('Qtvcp Error with camview - is python3-opencv installed?')
    LIB_GOOD = False
    DEFAULT_API = 0 # CV.CAP_ANY

import numpy as np



class CamView(QtWidgets.QWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(CamView, self).__init__(parent)
        self._qImageFormat=QImage.Format_RGB888
        self.video = None
        self.grabbed = None
        self.frame = None
        self._camNum = 0
        self.diameter = 20
        self.rotation = 0
        self.rotationIncrement = .5
        self.scale = 1
        self.scaleX = 1.0
        self.scaleY = 1.0
        self.gap = 5
        self._noRotate = False
        self.setWindowTitle('Cam View')
        self.setGeometry(100, 100, 200, 200)
        self.text_color = QColor(255, 255, 255)
        self.circle_color = QtCore.Qt.red
        self.cross_color = QtCore.Qt.yellow
        self.cross_pointer_color = QtCore.Qt.white
        self.font = QFont("arial,helvetica", 40)
        self.SUB = str.maketrans("0123456789", "₀₁₂₃₄₅₆₇₈₉")
        if LIB_GOOD:
            self.text = 'No Image'
        else:
            self.text = 'Missing\npython-opencv\nLibrary'
        self.pix = None
        self.stopped = False
        self.degree = str("\N{DEGREE SIGN}")

        # trap so can run script directly to test
        try:
            if INFO.PROGRAM_PREFIX is not None:
                self.user_path = os.path.expanduser(INFO.PROGRAM_PREFIX)
        except:
            self.user_path = (os.path.join(os.path.expanduser('~'), 'linuxcnc/nc_files'))

        #self.blobInit()

    def _hal_init(self):
        try:
            self.pin_ = self.HAL_GCOMP_.newpin('cam-rotation',hal.HAL_FLOAT, hal.HAL_OUT)
        except:
            pass
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
        if event.angleDelta().y() < 0:
            if mouse_state == QtCore.Qt.NoButton:
                self.diameter -= 2
            if mouse_state == QtCore.Qt.LeftButton:
                self.scale -= .1
            if mouse_state == QtCore.Qt.RightButton:
                if not self._noRotate:
                    self.rotation += self.rotationIncrement
        else:
            if mouse_state == QtCore.Qt.NoButton:
                self.diameter += 2
            if mouse_state == QtCore.Qt.LeftButton:
                self.scale += .1
            if mouse_state == QtCore.Qt.RightButton:
                if not self._noRotate:
                    self.rotation -= self.rotationIncrement
        self.limitChecks()

    def mousePressEvent(self, event):
        if event.button() & QtCore.Qt.LeftButton:
            if not self._noRotate:
                self.rotation += self.rotationIncrement
                self.limitChecks()
        elif event.button() & QtCore.Qt.RightButton:
            if not self._noRotate:
                self.rotation -= self.rotationIncrement
                self.limitChecks()
        elif event.button() & QtCore.Qt.MiddleButton:
            self.rotation_increments_changed()

    def mouseDoubleClickEvent(self, event):
        if event.button() & QtCore.Qt.LeftButton:
            self.scale = 1
        elif event.button() & QtCore.Qt.RightButton:
            self.rotation = 0
        elif event.button() & QtCore.Qt.MiddleButton:
            self.diameter = 20

    def zoom_in(self):
        if self.scale >= 5:
            return
        self.scale += 0.1

    def zoom_out(self, event):
        if self.scale <= 1:
            return
        self.scale -= 0.1

    def limitChecks(self):
        w = self.size().width()
        if self.diameter < 2: self.diameter = 2
        if self.diameter > w: self.diameter = w
        if self.rotation > 360 - self.rotationIncrement: self.rotation = 0
        if self.rotation < 0: self.rotation = 360 - self.rotationIncrement
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

        # set digital zoom
        frame = self.zoom(frame, self.scale)
        # make a Q image
        self.pix = self.makeImage(frame, self._qImageFormat)

        # make a CV window for testing
        #self.makeCVImage(self.canny(self.convertToGray(frame)))

        # repaint the window
        self.update()
        try:
            self.pin_.set(self.rotation)
        except:
            pass

    def convertToRGB(self, img):
        return CV.cvtColor(img, CV.COLOR_BGR2RGB)

    # new qt 5.13 + for QImage.Format_Grayscale16
    def convertToGray(self, img):
        return  CV.cvtColor(img, CV.COLOR_BGR2GRAY)

    def blur(self, img, B=7):
        return CV.GaussingBlur(img, (B,B), CV.BORDER_DEFAULT)

    def canny(self, img, x=125, y=175):
        return CV.Canny(img , x, y)

    def makeImage(self, image, qFormat=QImage.Format_RGB888):
        img = self.convertToRGB(image)
        return QImage(img, img.shape[1], img.shape[0], img.strides[0], qFormat)

    # openCV Window
    def makeCVImage(self, frame):
        CV.imshow('CV Image',frame)

    def rescaleFrame(self, frame, scale = 1, scale_x =1.0, scale_y=1.0):
        x = scale_x * scale
        y = scale_y * scale
        return CV.resize(frame, None, fx = x, fy = y, interpolation=CV.INTER_CUBIC)

    def zoom(self, frame, scale):
        # get original size of image
        (oh, ow) = frame.shape[:2]
        #############################
        # scale image
        #############################
        frame = self.rescaleFrame(frame, scale,self.scaleX,self.scaleY)
        ##########################
        # crop to the original size of the frame
        # measure from center, so we zoom on center
        # ch = center of current height
        # coh = center of original height
        ##########################
        (h, w) = frame.shape[:2]
        ch = int(h/2)
        cw = int(w/2)
        coh = int(oh/2)
        cow = int(ow/2)
        # NOTE: its img[y: y + h, x: x + w]
        return frame[ch-coh:ch+coh, cw-cow:cw+cow]

    # draw a circle around small holes
    #
    def findCircles(self,frame):
        # Our operations on the frame come here
        gray = CV.cvtColor(frame, CV.COLOR_BGR2GRAY)
        # Display the resulting frame

        circles = CV.HoughCircles(gray,CV.cv.CV_HOUGH_GRADIENT,1,20,param1=50,param2=30,minRadius=10,maxRadius=15)
        # print circles
        if circles is not None:
            #print circles
            circles = np.uint16(np.around(circles))
            for i in circles[0,:]:
                # draw the outer circle
                CV.circle(gray,(i[0],i[1]),i[2],(246,11,11),1)
                # draw the center of the circle
                CV.circle(gray,(i[0],i[1]),2,(246,11,11),1)

        CV.imshow('Circles',gray)

    def blobInit(self):
        # Setup BlobDetector
        detector = CV.SimpleBlobDetector()
        params = CV.SimpleBlobDetector_Params()

        # Filter by Area.
        params.filterByArea = True
        params.minArea = 20000
        params.maxArea = 40000

        # Filter by Circularity
        params.filterByCircularity = True
        params.minCircularity = 0.5

        # Filter by Convexity
        params.filterByConvexity = False
        #params.minConvexity = 0.87

        # Filter by Inertia
        params.filterByInertia = True
        params.minInertiaRatio = 0.8

        # Distance Between Blobs
        params.minDistBetweenBlobs = 200

        # Create a detector with the parameters
        self.detector = CV.SimpleBlobDetector(params)

    # find circles overlay and draw a cross
    # https://www.kurokesu.com/main/2016/07/25/advanced-opencv-3-python-hole-detection/
    def findBlob(self, image):
        overlay = image.copy()

        keypoints = self.detector.detect(image)
        for k in keypoints:
            CV.circle(overlay, (int(k.pt[0]), int(k.pt[1])), int(k.size/2), (0, 0, 255), -1)
            CV.line(overlay, (int(k.pt[0])-20, int(k.pt[1])), (int(k.pt[0])+20, int(k.pt[1])), (0,0,0), 3)
            CV.line(overlay, (int(k.pt[0]), int(k.pt[1])-20), (int(k.pt[0]), int(k.pt[1])+20), (0,0,0), 3)

        opacity = 0.5
        CV.addWeighted(overlay, opacity, image, 1 - opacity, 0, image)

        # Uncomment to resize to fit output window if needed
        #image = CV.resize(im, None,fx=0.5, fy=0.5, interpolation = CV.INTER_CUBIC)
        CV.imshow("Output", image)

    def showEvent(self, event):
        if LIB_GOOD:
            try:
                self.video = WebcamVideoStream(src=self._camNum)
                if not self.video.isOpened():
                    p = self.video.list_ports()[1]
                    self.text = 'Error with video {}\nAvailable ports:\n{}'.format(self._camNum, p)
                else:
                    self.video.start()
            except Exception as e:
                LOG.error('Video capture error: {}'.format(e))

    def hideEvent(self, event):
        if LIB_GOOD:
            try:
                self.video.stop()
            except:
                pass

    def resizeEvent(self, event):
        # Create a square base size of 10x10 and scale it to the new size
        # maintaining aspect ratio.
        new_size = QtCore.QSize(10, 10)
        new_size.scale(event.size(), QtCore.Qt.KeepAspectRatio)
        self.resize(new_size)

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
        if self._noRotate:
            inc =''
        else:
            inc = str(self.rotationIncrement).translate(self.SUB)
        if self.pix:
            qp.drawText(self.rect(), QtCore.Qt.AlignTop, '{:.1f}{} {:>}'.format(self.rotation,self.degree,
            inc))
        else:
            qp.drawText(self.rect(), QtCore.Qt.AlignCenter, self.text)

    def drawCircle(self, event, gp):
        size = self.size()
        w = size.width()
        h = size.height()
        radx = self.diameter/2
        rady = self.diameter/2
        # draw red circles
        gp.setPen(self.circle_color)
        center = QtCore.QPoint(w//2, h//2)
        gp.drawEllipse(center, radx, rady)

    def drawCrossHair(self, event, gp):
        size = self.size()
        w = size.width()//2
        h = size.height()//2
        pen0 = QPen(self.cross_pointer_color, 1, QtCore.Qt.SolidLine)
        pen = QPen(self.cross_color, 1, QtCore.Qt.SolidLine)
        gp.translate(w, h)
        gp.rotate(-self.rotation)
        gp.setPen(pen0)
        gp.drawLine(0, 0-self.gap, 0, -h)
        gp.setPen(pen)
        gp.drawLine(-w, 0, 0-self.gap, 0)
        gp.drawLine(0+self.gap, 0, w, 0)
        gp.drawLine(0, 0+self.gap, 0, h)

    def rotation_increments_changed(self,w=None):
        if self.rotationIncrement == 5.00:
            self.rotationIncrement = 1
        elif self.rotationIncrement == 1:
            self.rotationIncrement = 0.5
        elif self.rotationIncrement == 0.5:
            self.rotationIncrement = 0.1

        else:
            self.rotationIncrement = 5

    def setCircleColor(self, color):
        self.circle_color = color

    def setCrossColor(self, color):
        self.cross_color = color

    def setPointerColor(self, color):
        self.cross_pointer_color = color

    def saveImage(self):
        filepath = '{}/camImage.png'.format(self.user_path)
        self.video.writeFrame(filepath)

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    #
    # These can also be set as  WIDGET.setProperty('property_name', data)
    ########################################################################


    def set_wheel_rotation(self, value):
        self._noRotate = value
    def get_wheel_rotation(self):
        return self._noRotate
    def reset_wheel_rotation(self):
        self._noRotate = False
    def set_camnum(self, value):
        self._camNum = value
    def get_camnum(self):
        return self._camNum
    def reset_camnum(self):
        self._camNum = 0
    # designer will show these properties in this order:
    block_wheel_rotation = QtCore.pyqtProperty(bool, get_wheel_rotation, set_wheel_rotation, reset_wheel_rotation)
    camera_number = QtCore.pyqtProperty(int, get_camnum, set_camnum, reset_camnum)

class WebcamVideoStream:
    def __init__(self, src=0, api=DEFAULT_API):

        # initialize the video camera stream and read the first frame
        # from the stream
        self.stream = self.openStream(src, api)

        if not (self.stream.isOpened()):
            LOG.error('Could not open video device {}'.format(src))
            plist = self.list_ports()[1]
            if src not in plist:
                LOG.error('port number {}, is not a working port- trying: {}'.format(src,plist[0]))
            # try again with a hopefully functioning port
            if plist != []:
                self.stream = self.openStream(plist[0], api)

        # initialize the variable used to indicate if the thread should
        # be stopped
        self.stopped = False
        self.grabbed = None
        self.frame = None

    def isOpened(self):
        try:
           return self.stream.isOpened()
        except:
            return False

    def openStream(self, src, api):
        # initialize the video camera stream and read the first frame
        # from the stream
        try:
            stream = CV.VideoCapture(src, api)
        except:
            # try using an older function signature
            stream = CV.VideoCapture(src)
        return stream

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

    # TODO path checking
    def writeFrame(self, filepath):
        CV.imwrite(filepath, self.frame)
        print('saved camview image to: {}'.format(filepath))

    def list_ports(self):
        """
        Test the ports and returns a tuple with the available ports and the ones that are working.
        """
        non_working_ports = []
        dev_port = 0
        working_ports = []
        available_ports = []
        while len(non_working_ports) < 6: # if there are more than 5 non working ports stop the testing.
            camera = CV.VideoCapture(dev_port)
            if not camera.isOpened():
                non_working_ports.append(dev_port)
                LOG.debug("Port %s is not working." %dev_port)
            else:
                is_reading, img = camera.read()
                w = camera.get(3)
                h = camera.get(4)
                if is_reading:
                    LOG.debug("Port %s is working and reads images (%s x %s)" %(dev_port,h,w))
                    working_ports.append(dev_port)
                else:
                    LOG.debug("Port %s for camera ( %s x %s) is present but does not read." %(dev_port,h,w))
                    available_ports.append(dev_port)
            dev_port +=1
        return available_ports,working_ports,non_working_ports

class CamAngle(CamView):
    def __init__(self, parent=None):
        super(CamAngle, self).__init__(parent)

    def mouseDoubleClickEvent(self, event):
        if event.button() & QtCore.Qt.LeftButton:
            # zoom
            self.scale = 1
        elif event.button() & QtCore.Qt.MiddleButton:
            self.diameter = 40

    def wheelEvent(self, event):
        mouse_state = QtWidgets.qApp.mouseButtons()
        size = self.size()
        w = size.width()
        if event.angleDelta().y() < 0:
            if mouse_state == QtCore.Qt.NoButton:
                self.diameter -= 2
            if mouse_state == QtCore.Qt.LeftButton:
                self.scale -= .1
        else:
            if mouse_state == QtCore.Qt.NoButton:
                self.diameter += 2
            if mouse_state == QtCore.Qt.LeftButton:
                self.scale += .1
        if self.diameter < 2: self.diameter = 2
        if self.diameter > w: self.diameter = w
        if self.scale < 1: self.scale = 1
        if self.scale > 5: self.scale = 5

    def drawText(self, event, qp):
        size = self.size()
        w = size.width()
        h = size.height()
        qp.setPen(self.text_color)
        qp.setFont(self.font)
        if self.pix:
            angle = 0.0 if self.rotation == 0 else 360 - self.rotation
            qp.drawText(self.rect(), QtCore.Qt.AlignTop, '{:0.3f}{}'.format(angle, self.degree))
        else:
            qp.drawText(self.rect(), QtCore.Qt.AlignCenter, self.text)


if __name__ == '__main__':

    import sys
    app = QtWidgets.QApplication(sys.argv)
    capture = CamAngle()
    capture.show()

    def jump():
        capture.nextFrameSlot(None)
    timer = QtCore.QTimer()
    timer.timeout.connect(jump)
    timer.start(10)
    sys.exit(app.exec_())
