#!/usr/bin/env python3
#############################################################################
##
## Copyright (C) 2010 Hans-Peter Jansen <hpj@urpla.net>.
## Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
## All rights reserved.
##
## This file is part of the examples of PyQt.
##
## $QT_BEGIN_LICENSE:BSD$
## You may use this file under the terms of the BSD license as follows:
##
## "Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are
## met:
##   * Redistributions of source code must retain the above copyright
##     notice, this list of conditions and the following disclaimer.
##   * Redistributions in binary form must reproduce the above copyright
##     notice, this list of conditions and the following disclaimer in
##     the documentation and/or other materials provided with the
##     distribution.
##   * Neither the name of Nokia Corporation and its Subsidiary(-ies) nor
##     the names of its contributors may be used to endorse or promote
##     products derived from this software without specific prior written
##     permission.
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
## "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
## LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
## A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
## OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
## SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
## LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
## DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
## THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
## (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
## OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
## $QT_END_LICENSE$
##
###########################################################################

import os
import sys
import traceback
from PyQt5 import uic
from PyQt5.QtCore import pyqtSlot, QFile, QRegExp, Qt, QTextStream
from PyQt5.QtWidgets import (QApplication, QDialog, QFileDialog, QMessageBox,
        QStyleFactory, QWidget, QColorDialog)
from PyQt5 import QtGui, QtCore

from qt5_graphics import Lcnc_3dGraphics
from qtvcp.core import Info, Path, Action

from qtvcp import logger
LOG = logger.getLogger(__name__)
#LOG = logger.initBaseLogger('QTvcp1', log_file=None, log_level=logger.DEBUG)

INFO = Info()
PATH = Path()
ACTION = Action()
DATADIR = os.path.abspath( os.path.dirname( __file__ ) )

class NurbsEditor(QDialog):
    def __init__(self, parent=None, path=None):
        super(NurbsEditor, self).__init__(parent)
        self.setMinimumSize(500, 500)
        self.bluck_update = True

        # Load the widgets UI file:
        self.filename = os.path.join(INFO.LIB_PATH,'widgets_ui', 'nurbs_editor.ui')
        try:
            self.instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            #exc_type, exc_value, exc_traceback = sys.exc_info()
            formatted_lines = traceback.format_exc().splitlines()
            print()
            print("Ui loadinr error",formatted_lines[0])
            #traceback.print_tb(exc_traceback, limit=1, file=sys.stdout)
            print(formatted_lines[-1])
            if 'slotname' in formatted_lines[-2]:
                LOG.critical('Missing slot name {}'.format(e))
            else:
                LOG.critical(e)

        self.setWindowTitle('NurbsEditor Dialog');


        self.workpath = os.path.join( "/tmp/LINUXCNCtempWork.ngc")
        self.emptypath = os.path.join( "/tmp/LINUXCNCtempEmpty.ngc")

        emptyfile = open(self.emptypath, "w")
        print(("m2"), file=emptyfile)
        emptyfile.close()

        self.set_spinbuttons_default()
        self.graphics = Lcnc_3dGraphics()

        self.setGraphicsDisplay()
        self.main.addWidget(self.graphics)
        self.updateDisplay(self.emptypath)
        self.update()

    def setGraphicsDisplay(self):
        # class patch to catch gcode errors - in theory 
        self.graphics.report_gcode_error = self.report_gcode_error
        # reset trverse color so other displays don;t change
        self.defaultColor = self.graphics.colors['traverse']
        self.graphics.current_view = 'z'
        self.graphics.metric_units = INFO.MACHINE_IS_METRIC
        self.graphics.use_gradient_background = True
        self.graphics.show_tool = False
        self.graphics.grid_size = 2
        self.graphics.cancel_rotate = True

    def set_spinbuttons_default(self):
        self.use_ctrl1.setChecked(True)
        self.use_ctrl2.setChecked(True)
        self.use_ctrl3.setChecked(True)
        self.use_ctrl4.setChecked(True)

        self.x1.setValue(3.53)
        self.x2.setValue(5.53)
        self.x3.setValue(3.52)
        self.x4.setValue(0)

        self.y1.setValue(-1.50)
        self.y2.setValue(-11.01)
        self.y3.setValue(-24.00)
        self.y4.setValue(-29.56)

        self.w1.setValue(2)
        self.w2.setValue(1)
        self.w3.setValue(1)
        self.w4.setValue(1)
        self.gridSpace.setValue(2)

        self.unblock()

    def checkChanged(self):
        c = 0
        for i in range(1,8):
            u = 'use_ctrl{}'.format(i)
            if self[u].isChecked(): c +=1
        if c < 2:
            self.sender().setChecked(True)
        elif not self.bluck_update:
            self.update()

    def spinChanged(self,data):
        if self.bluck_update: return
        self.update()

    def gridSpinChanged(self):
        if self.bluck_update: return
        self.graphics.grid_size = self.gridSpace.value()
        self.graphics.updateGL()

    def block(self):
        self.bluck_update = True
    def unblock(self):
        self.bluck_update = False

    def invertX(self):
        self.block()
        for i in range(1,8):
            X = 'x{}'.format(i)
            self[X].setValue(self[X].value() * -1)
        self.rapidX.setValue(self.rapidX.value() * -1)
        self.unblock()
        self.update()

    def invertY(self):
        self.block()
        for i in range(1,8):
            Y = 'y{}'.format(i)
            self[Y].setValue(self[Y].value() * -1)
        self.rapidY.setValue(self.rapidY.value() * -1)
        self.unblock()
        self.update()

    def resetView(self):
        self.graphics.set_current_view()

    def update(self):
        self.graphics.colors['traverse'] = (0.0, 1.0, 0.0)
        self.workfile = open(self.workpath, "w")

        ###############################
        # Setup
        ###############################
        t = self.toolNum.value()
        if t:
            print(('T{} M6 G43').format(self.toolNum.value()), file=self.workfile)
        print(('( Starting position )'), file=self.workfile)
        print(('G0 X {} Y {} Z {}').format(self.rapidX.value(),self.rapidY.value(),self.rapidZ.value()), file=self.workfile)

        ###############################
        # Nurbs block
        ###############################
        print((''), file=self.workfile)
        print(('( Nurbs block )'), file=self.workfile)
        f = self.feedRate.value()
        if f:
            print(('F{}').format(self.feedRate.value()), file=self.workfile)
        #print >> self.workfile, ('F')
        print(('G5.2'), file=self.workfile)
        for i in range(1,8):
            u = 'use_ctrl{}'.format(i)
            X = 'x{}'.format(i)
            Y = 'y{}'.format(i)
            P = 'w{}'.format(i)
            if self[u].isChecked():
                print(('X {} Y {} P {}').format(self[X].value(),self[Y].value(),self[P].value()), file=self.workfile)
        print(('G5.3'), file=self.workfile)

        ##############################
        # rapids to show boundary
        ##############################
        print((''), file=self.workfile)
        print(('( Show control points with rapid lines )'), file=self.workfile)
        print(('G0 X {} Y {} Z {}').format(self.rapidX.value(),self.rapidY.value(),self.rapidZ.value()), file=self.workfile)
        for i in range(1,8):
            u = 'use_ctrl{}'.format(i)
            X = 'x{}'.format(i)
            Y = 'y{}'.format(i)
            if self[u].isChecked():
                print(('G0 X{} y{}').format(self[X].value(),self[Y].value()), file=self.workfile)

        ##############################
        # cleanup
        ##############################
        print(('m2'), file=self.workfile)

        self.workfile.close()
        self.updateDisplay(self.workpath)
        self.graphics.colors['traverse'] = self.defaultColor

    def finalizeGcode(self):
        gcode = self.gcodeText.toPlainText()
        file = QFile(self.workpath)
        if file.open(QFile.WriteOnly):
            QTextStream(file) << gcode

    def updateDisplay(self,fn):
        self.graphics.load(fn)

    def load_dialog(self):
        #self.updateDisplay(self.emptypath)
        self.graphics.current_view = 'z'
        self.show() # must be realized before view will change
        self.graphics.set_current_view()
        #self.activateWindow()

    @pyqtSlot()
    def on_makeButton_clicked(self):
        print('make')
        file = QFile(self.workpath)
        file.open(QFile.ReadOnly)
        gcode = file.readAll()
        try:
            # Python v2.
            gcode = str(gcode, encoding='utf8')
        except NameError:
            # Python v3.
            gcode = str(gcode, encoding='utf8')
        self.gcodeText.setPlainText(gcode)

    @pyqtSlot()
    def on_applyButton_clicked(self):
        self.finalizeGcode()
        ACTION.OPEN_PROGRAM(self.workpath)

    @pyqtSlot()
    def on_closeButton_clicked(self):
        self.close()

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)


###############
# class patch
############
    def report_gcode_error(self, result, seq, filename):
        error_str = gcode.strerror(result)
        errortext = "G-Code error in " + os.path.basename(filename) + "\n" + "Near line " \
                    + str(seq) + " of\n" + filename + "\n" + error_str + "\n"
        print(errortext)

###########
# Testing
###########
if __name__ == '__main__':

    app = QApplication(sys.argv)
    if len(sys.argv) == 1:
        inifilename = None
    elif len(sys.argv) == 2:
        inifilename = sys.argv[1]
    else:
        usage()
    window = NurbsEditor(path = inifilename)
    window.load_dialog()
    sys.exit(app.exec_())
  

