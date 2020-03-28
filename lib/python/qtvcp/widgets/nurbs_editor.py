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
from PyQt5 import uic
from PyQt5.QtCore import pyqtSlot, QFile, QRegExp, Qt, QTextStream
from PyQt5.QtWidgets import (QApplication, QDialog, QFileDialog, QMessageBox,
        QStyleFactory, QWidget, QColorDialog)
from PyQt5 import QtGui, QtCore

from qt5_graphics import Lcnc_3dGraphics
from qtvcp.core import Info, Path, Action

INFO = Info()
PATH = Path()
ACTION = Action()
DATADIR = os.path.abspath( os.path.dirname( __file__ ) )

class NurbsEditor(QDialog):
    def __init__(self, parent=None, path=None):
        super(NurbsEditor, self).__init__(parent)
        self.setMinimumSize(800, 800)
        self.block = True

        # Load the widgets UI file:
        self.filename = os.path.join(INFO.LIB_PATH,'widgets_ui', 'nurbs_editor.ui')
        try:
            self.instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            LOG.critical(e)

        self.setWindowTitle('NurbsEditor Dialog');


        self.workpath = os.path.join( "/tmp/LINUXCNCtempWork.ngc")
        self.emptypath = os.path.join( "/tmp/LINUXCNCtempEmpty.ngc")

        emptyfile = open(self.emptypath, "w")
        print >> emptyfile, ("m2")
        emptyfile.close()

        self.set_spinbuttons_default()

        self.graphics = Lcnc_3dGraphics()
        self.setGraphicsDisplay()
        self.main.addWidget(self.graphics)
        self.updateDisplay(self.emptypath)


    def setGraphicsDisplay(self):
        self.defaultColor = self.graphics.colors['traverse']
        # must start in default view(P) or else later rotation errors (glnav's lon is not initiated?) 
        #self.graphics.current_view = 'Z'
        self.graphics.metric_units = INFO.MACHINE_IS_METRIC
        self.graphics.use_gradient_background = True
        self.graphics.show_tool = False

    def set_spinbuttons_default(self):
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
        self.block = False

    def spinChanged(self,data):
        if self.block: return
        self.update()

    def update(self):
        self.graphics.colors['traverse'] = (0.0, 0.0, 1.0)
        self.workfile = open(self.workpath, "w")
        print >> self.workfile, ('( Starting position )')
        print >> self.workfile, ('G0 x0 y0')

        # Nurbs block
        print >> self.workfile, ('')
        print >> self.workfile, ('( Nurbs block )')
        print >> self.workfile, ('f100')
        print >> self.workfile, ('G5.2')
        for i in range(1,5):
            X = 'x{}'.format(i)
            Y = 'y{}'.format(i)
            P = 'w{}'.format(i)
            print >> self.workfile, ('X {} y {} P {}').format(self[X].value(),self[Y].value(),self[P].value())
        print >> self.workfile, ('G5.3')
        print >> self.workfile, ('G0 x0 y0')

        # rapids to show boundry
        print >> self.workfile, ('')
        print >> self.workfile, ('( Show control points with rapid lines )')
        for i in range(1,5):
            X = 'x{}'.format(i)
            Y = 'y{}'.format(i)
            print >> self.workfile, ('G0 X{} y{}').format(self[X].value(),self[Y].value())
        print >> self.workfile, ('G0 x0 y0')

        print >> self.workfile, ('m2')

        self.workfile.close()
        self.updateDisplay(self.workpath)
        self.graphics.colors['traverse'] = self.defaultColor

    def finalizeGcode(self):
        gcode = self.gcodeText.toPlainText()
        file = QFile(self.workpath)
        if file.open(QFile.WriteOnly):
            QTextStream(file) << gcode

    def updateDisplay(self,fn):
        print fn
        dist = self.graphics.get_zoom_distance()
        #LOG.debug('load the display: {}'.format(fname))
        self.graphics.load(fn)
        self.graphics.set_zoom_distance(dist)

    def load_dialog(self):
        #self.updateDisplay(self.emptypath)
        self.graphics.current_view = 'Z'
        self.graphics.updateGL()
        self.show()
        self.activateWindow()
        self.update()

    @pyqtSlot()
    def on_makeButton_clicked(self):
        print 'make'
        file = QFile(self.workpath)
        file.open(QFile.ReadOnly)
        gcode = file.readAll()
        try:
            # Python v2.
            gcode = unicode(gcode, encoding='utf8')
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
    window.show()
    sys.exit(app.exec_())
  

