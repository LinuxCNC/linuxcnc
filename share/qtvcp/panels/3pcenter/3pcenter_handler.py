#!/usr/bin/env python3

from qtvcp import logger
from qtpy import QtCore, QtGui
import os
import hal
import linuxcnc

import time


c = linuxcnc.command()

log = logger.getLogger(__name__)


DEFAULT = 0
WARNING = 1
CRITICAL = 2


###################################
# **** HANDLER CLASS SECTION **** #
###################################

class HandlerClass:

    def __init__(self, halcomp,widgets,paths):
        self.hal = halcomp
        self.w = widgets
        self.PATHS = paths


    def initialized__(self):
        log.debug('INIT qtvcp handler')

        self.w.pushButton_1.pressed.connect(self.XY_A)
        self.w.pushButton_2.pressed.connect(self.XY_B)
        self.w.pushButton_3.pressed.connect(self.XY_C)
        self.w.pushButton_4.pressed.connect(self.goCenter)
        self.w.pushButton_5.pressed.connect(self.setCenter)

        self.w.pushButton_4.hide()
        self.w.pushButton_5.hide()

        fileqss = os.path.dirname(__file__)+'/3pcenter.qss'
        if os.path.exists(fileqss):
            file = open(fileqss, "r")
            Styles = file.read()
            self.w.setStyleSheet(Styles)
        else:
            print ("file qss not found")



#######################################################################################################



    def XY_A(self):
          self.X_A = hal.get_value("halui.axis.x.pos-relative")
          self.w.label_2.setText(str(round(self.X_A, 4)))
          self.Y_A = hal.get_value("halui.axis.y.pos-relative")
          self.w.label_3.setText(str(round(self.Y_A, 4)))
          self.obliczXY()

    def XY_B(self):
          self.X_B = hal.get_value("halui.axis.x.pos-relative")
          self.w.label_5.setText(str(round(self.X_B, 4)))
          self.Y_B = hal.get_value("halui.axis.y.pos-relative")
          self.w.label_6.setText(str(round(self.Y_B, 4)))
          self.obliczXY()

    def XY_C(self):
          self.X_C = hal.get_value("halui.axis.x.pos-relative")
          self.w.label_7.setText(str(round(self.X_C, 4)))
          self.Y_C = hal.get_value("halui.axis.y.pos-relative")
          self.w.label_8.setText(str(round(self.Y_C, 4)))
          self.obliczXY()

    def obliczXY(self):
        try:
            if ((self.X_A, self.Y_A) != ( self.X_B, self.Y_B) and (self.X_A, self.Y_A) != (self.X_C, self.Y_C) and (self.X_B, self.Y_B) != (self.X_C, self.Y_C) ):
                try:
                    self.X = ((self.X_A**2+self.Y_A**2)*(self.Y_B-self.Y_C)+(self.X_B**2+self.Y_B**2)*(self.Y_C-self.Y_A)+(self.X_C**2+self.Y_C**2)*(self.Y_A-self.Y_B))/(2*(self.X_A*(self.Y_B-self.Y_C)-self.Y_A*(self.X_B-self.X_C)+self.X_B*self.Y_C-self.X_C*self.Y_B))
                    self.Y = ((self.X_A**2+self.Y_A**2)*(self.X_C-self.X_B)+(self.X_B**2+self.Y_B**2)*(self.X_A-self.X_C)+(self.X_C**2+self.Y_C**2)*(self.X_B-self.X_A))/(2*(self.X_A*(self.Y_B-self.Y_C)-self.Y_A*(self.X_B-self.X_C)+self.X_B*self.Y_C-self.X_C*self.Y_B))
                    self.w.label_9.setText(str(round(self.X, 4)))
                    self.w.label_11.setText(str(round(self.Y, 4)))
                    self.w.label_4.setText("X0 Y0 is not in the center of the circle")
                    self.w.label_4.setStyleSheet("background-color: rgba(250, 250, 30, 120);")
                    self.w.label_9.setStyleSheet("background-color: rgba(50, 150, 50, 120);")
                    self.w.label_11.setStyleSheet("background-color: rgba(50, 150, 50, 120);")
                    self.w.pushButton_4.show()
                    self.w.pushButton_5.show()
                    #self.setCenter()
                except:
                    self.set_no_ok()
            else:
                self.set_no_ok()
        except:
            self.set_no_ok()


    def set_no_ok(self):

        self.w.label_4.setText("More Sample Points Needed")
        self.w.label_4.setStyleSheet("background-color: rgba(223, 13, 15, 120);")
        self.w.label_9.setStyleSheet("background-color: rgba(223, 13, 15, 120);")
        self.w.label_11.setStyleSheet("background-color: rgba(223, 13, 15, 120);")
        self.w.label_9.setText("X")
        self.w.label_11.setText("Y")
        self.w.pushButton_4.hide()
        self.w.pushButton_5.hide()


    def goCenter(self):
            c.mode(linuxcnc.MODE_MDI)
            c.wait_complete()
            c.mdi('g53 g0 z0')
            c.mdi('g54 g0 x'+str(round(self.X, 4))+'y'+str(round(self.Y, 4)))

    def setCenter(self):
            offset_X = (hal.get_value("halui.axis.x.pos-relative") - self.X)
            offset_Y = (hal.get_value("halui.axis.y.pos-relative") - self.Y)
            c.mode(linuxcnc.MODE_MDI)
            c.wait_complete()
            c.mdi('g10 l20 p1 x'+str(round(offset_X, 4))+'y'+str(round(offset_Y, 4)))
            self.w.label_4.setText("OK")
            self.w.label_4.setStyleSheet("background-color: rgba(50, 150, 50, 120);")
            self.X = 0
            self.Y = 0
            self.w.label_9.setText(str(self.X))
            self.w.label_11.setText(str(self.Y))
            self.X_A = ""
            self.w.label_2.setText('X')
            self.Y_A = ""
            self.w.label_3.setText('Y')
            self.X_B = ""
            self.w.label_5.setText('X')
            self.Y_B = ""
            self.w.label_6.setText('Y')
            self.X_C = ""
            self.w.label_7.setText('X')
            self.Y_C = ""
            self.w.label_8.setText('Y')




################################
# required handler boiler code #
################################

def get_handlers(halcomp,widgets,paths):
     return [HandlerClass(halcomp,widgets,paths)]

