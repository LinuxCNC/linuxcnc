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
from PyQt5 import uic
from PyQt5.QtCore import pyqtSlot, QFile, QRegExp, Qt, QTextStream
from PyQt5.QtWidgets import (QApplication, QDialog, QFileDialog, QMessageBox,
        QStyleFactory, QWidget, QColorDialog)
from PyQt5 import QtGui, QtCore

DATADIR = os.path.abspath( os.path.dirname( __file__ ) )

class StyleSheetEditor(QDialog):
    def __init__(self, parent=None, path=None):
        super(StyleSheetEditor, self).__init__(parent)
        self.setMinimumSize(600, 400)
        # Load the widgets UI file:
        self.filename = os.path.join(DATADIR, 'style_dialog.ui')
        try:
            self.instance = uic.loadUi(self.filename, self)
        except AttributeError as e:
            LOG.critical(e)

        self.setWindowTitle('Style SHeet Editor Dialog');
        self.path = path
        self.parent = parent
        if path:
            self.setPath(path)
        self.styleSheetCombo.currentIndexChanged.connect(self.selectionChanged)

    def load_dialog(self):
        self.origStyleSheet = self.parent.styleSheet()
        self.styleTextView.setPlainText(self.origStyleSheet)
        self.show()
        self.activateWindow()

    def setPath(self, path):
        self.path = path
        self.styleSheetCombo.addItem('Default')
        model = self.styleSheetCombo.model()
        # check for default qss from qtvcp's default folders
        if self.path.IS_SCREEN:
            DIR = self.path.SCREENDIR
            BNAME = self.path.BASENAME
        else:
            DIR =self.path.PANELDIR
            BNAME = self.path.BASENAME
        qssname = os.path.join(DIR, BNAME)
        try:
            fileNames= [f for f in os.listdir(qssname) if f.endswith('.qss')]
            for i in(fileNames):
                item = QtGui.QStandardItem(i)
                item.setData(qssname, role = QtCore.Qt.UserRole + 1)
                model.appendRow(item)
        except Exception as e:
            print e

        # check for qss in the users's config folder 
        localqss = self.path.CONFIGPATH
        try:
            fileNames= [f for f in os.listdir(localqss) if f.endswith('.qss')]
            for i in(fileNames):
                item = QtGui.QStandardItem(i)
                item.setData(localqss, role = QtCore.Qt.UserRole + 1)
                model.appendRow(item)
        except Exception as e:
            print e

    def selectionChanged(self,i):
        path = self.styleSheetCombo.itemData(i,role = QtCore.Qt.UserRole + 1)
        name = self.styleSheetCombo.itemData(i,role = QtCore.Qt.DisplayRole)
        if name == 'Default':
            sheetName = name
        else:
            sheetName = os.path.join(path, name)
        self.loadStyleSheet(sheetName)

    @pyqtSlot()
    def on_styleTextView_textChanged(self):
        self.applyButton.setEnabled(True)

    @pyqtSlot()
    def on_applyButton_clicked(self):
        if self.tabWidget.currentIndex() == 0:
            self.parent.setStyleSheet(self.styleTextView.toPlainText())
        else:
           self.parent.setStyleSheet(self.styleTextEdit.toPlainText())

    @pyqtSlot()
    def on_openButton_clicked(self):
        dialog = QFileDialog(self)
        if self.path.IS_SCREEN:
            DIR = self.path.SCREENDIR
        else:
            DIR =self.path.PANELDIR
        print DIR
        dialog.setDirectory(DIR)
        fileName, _ = dialog.getOpenFileName()
        if fileName:
            file = QFile(fileName)
            file.open(QFile.ReadOnly)
            styleSheet = file.readAll()
            try:
                # Python v2.
                styleSheet = unicode(styleSheet, encoding='utf8')
            except NameError:
                # Python v3.
                styleSheet = str(styleSheet, encoding='utf8')

            self.styleTextView.setPlainText(styleSheet)

    @pyqtSlot()
    def on_saveButton_clicked(self):
        fileName, _ = QFileDialog.getSaveFileName(self)
        if fileName:
            self.saveStyleSheet(fileName)

    @pyqtSlot()
    def on_closeButton_clicked(self):
        self.close()

    @pyqtSlot()
    def on_clearButton_clicked(self):
        self.styleTextEdit.clear()

    @pyqtSlot()
    def on_copyButton_clicked(self):
        self.styleTextEdit.setPlainText(self.styleTextView.toPlainText())

    @pyqtSlot()
    def on_colorButton_clicked(self):
        _color = QColorDialog.getColor()
        if _color.isValid():
            Color = _color.name()
            self.colorButton.setStyleSheet('QPushButton {background-color: %s ;}'% Color)
            self.styleTextEdit.insertPlainText(Color)

    def loadStyleSheet(self, sheetName):
        if not sheetName == 'Default':
            if self.path.IS_SCREEN:
                DIR = self.path.SCREENDIR
                BNAME = self.path.BASENAME
            else:
                DIR =self.path.PANELDIR
                BNAME = self.path.BASENAME
            qssname = os.path.join(DIR, BNAME, sheetName)
            file = QFile(qssname)
            file.open(QFile.ReadOnly)
            styleSheet = file.readAll()
            try:
                # Python v2.
                styleSheet = unicode(styleSheet, encoding='utf8')
            except NameError:
                # Python v3.
                styleSheet = str(styleSheet, encoding='utf8')
        else:
            styleSheet = self.origStyleSheet

        self.styleTextView.setPlainText(styleSheet)

    def saveStyleSheet(self, fileName):
        styleSheet = self.styleTextEdit.toPlainText()
        file = QFile(fileName)
        if file.open(QFile.WriteOnly):
            QTextStream(file) << styleSheet
        else:
            QMessageBox.information(self, "Unable to open file",
                    file.errorString())
