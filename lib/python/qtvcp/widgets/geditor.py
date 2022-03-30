#!/usr/bin/env python3
# -*- encoding: utf-8 -*-
#    Gcode display / edit widget for QT_VCP
#    Copyright 2016 Chris Morley
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

# This was based on
# QScintilla sample with PyQt
# Eli Bendersky (eliben@gmail.com)
# Which is code in the public domain
#
# See also:
# http://pyqt.sourceforge.net/Docs/QScintilla2/index.html
# https://qscintilla.com/
# https://qscintilla.com/simple-example/

import sys
import os

from PyQt5.QtCore import pyqtProperty, pyqtSignal, QSize, QObject, QEvent, Qt, QByteArray, QVariant
from PyQt5.QtGui import QFont, QFontMetrics, QColor, QIcon, QPalette
from PyQt5.QtWidgets import QMainWindow, QWidget, QPushButton, QAction,\
         QVBoxLayout,QToolBar,QGroupBox,QLineEdit, QHBoxLayout,QMessageBox, \
            QFileDialog, QFrame, QLabel, QStyleOption

from qtvcp.widgets.gcode_editor import GcodeDisplay
from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Info, Action
from qtvcp import logger

# Instantiate the libraries with global reference
# INFO holds INI file details
# STATUS gives us status messages from linuxcnc
# LOG is for running code logging
STATUS = Status()
INFO = Info()
ACTION = Action()
LOG = logger.getLogger(__name__)


#############################################
# For Editing Gcode
#############################################

class GEditor(QMainWindow, _HalWidgetBase):
    percentDone = pyqtSignal(int)

    def __init__(self, parent=None, designer=False):
        if not designer:
            parent = None
        super().__init__(parent)
        self._show_editor = True
        self.load_dialog_code = 'LOAD'
        self.save_dialog_code = 'SAVE'
        self.dialog_code = 'KEYBOARD'

        STATUS.connect('general',self.returnFromDialog)

        self.isCaseSensitive = 0

        self.setMinimumSize(QSize(300, 200))    
        self.setWindowTitle("PyQt5 editor test example") 

        # make editor
        self.editor = GcodeDisplay(self)
        self.setCentralWidget(self.editor)

        # class patch editor's function to ours
        # so we get the lines percent update
        self.editor.emit_percent = self.emit_percent

        self.editor.setReadOnly(True)
        self.editor.setModified(False)

        self.createActions()

        # Create toolbar and add action
        self.toolBar = QToolBar('File')
        self.toolBar.setObjectName('{}_toolbarfile'.format( self.objectName()))
        self.toolBar.addAction(self.newAction)
        self.toolBar.addAction(self.openAction)
        self.toolBar.addAction(self.saveAction)
        self.toolBar.addAction(self.exitAction)
        self.addToolBar(Qt.TopToolBarArea, self.toolBar)

        #self.toolBar.addSeparator()
        self.toolBarLexer = QToolBar('Lexer')
        self.toolBarLexer.setObjectName('{}_toolbarlexer'.format( self.objectName()))

        # add lexer actions
        self.toolBarLexer.addAction(self.gCodeLexerAction)
        self.toolBarLexer.addAction(self.pythonLexerAction)

        self.toolBarLexer.addSeparator()
        self.label = QLabel('''<html><head/><body><p><span style=" font-size:20pt;
                         font-weight:600;">Edit Mode</span></p></body></html>''')
        self.toolBarLexer.addWidget(self.label)

        self.addToolBar(Qt.TopToolBarArea, self.toolBarLexer)

        # Create toolbar and add action
        self.toolBarEdit = QToolBar('Edit')
        self.toolBarEdit.setObjectName('{}_toolbaredit'.format( self.objectName()))

        self.toolBarEdit.addAction(self.undoAction)
        self.toolBarEdit.addAction(self.redoAction)
        self.toolBarEdit.addSeparator()
        self.toolBarEdit.addAction(self.replaceAction)
        self.toolBarEdit.addAction(self.findAction)
        self.toolBarEdit.addAction(self.previousAction)
        self.toolBarEdit.addSeparator()
        self.toolBarEdit.addAction(self.caseAction)
        self.addToolBar(Qt.BottomToolBarArea, self.toolBarEdit)

        self.toolBarEntry = QToolBar('entry')
        self.toolBarEntry.setObjectName('{}_toolbarentry'.format( self.objectName()))

        frame = QFrame()
        box = QHBoxLayout()
        box.addWidget(self.searchText)
        box.addWidget(self.replaceText)
        box.addStretch(1)
        frame.setLayout(box)
        self.toolBarEntry.addWidget(frame)
        self.addToolBar(Qt.BottomToolBarArea, self.toolBarEntry)
        self.readOnlyMode(save = False)

    def createActions(self):
        # Create new action
        self.newAction = QAction(QIcon.fromTheme('document-new'), 'New', self)       
        self.newAction.setShortcut('Ctrl+N')
        self.newAction.setStatusTip('New document')
        self.newAction.triggered.connect(self.newCall)

        # Create open action
        self.openAction = QAction(QIcon.fromTheme('document-open'), '&Open', self)
        self.openAction.setShortcut('Ctrl+O')
        self.openAction.setStatusTip('Open document')
        self.openAction.triggered.connect(self.openCall)

        # Create save action
        self.saveAction = QAction(QIcon.fromTheme('document-save'), '&Save', self)
        self.saveAction.setShortcut('Ctrl+S')
        self.saveAction.setStatusTip('Save document')
        self.saveAction.triggered.connect(self.saveCall)

        # Create exit action
        self.exitAction = QAction(QIcon.fromTheme('application-exit'), '&Exit', self)
        self.exitAction.setShortcut('Ctrl+Q')
        self.exitAction.setToolTip('Exit Edit Mode')
        self.exitAction.setStatusTip('Exit Edit Mode')
        self.exitAction.triggered.connect(self.exitCall)

        # Create gcode lexer action
        self.gCodeLexerAction = QAction(QIcon.fromTheme('lexer.png'), '&Gcode\nLexer', self)
        self.gCodeLexerAction.setCheckable(1)
        self.gCodeLexerAction.setShortcut('Ctrl+G')
        self.gCodeLexerAction.setStatusTip('Set Gcode highlighting')
        self.gCodeLexerAction.triggered.connect(self.gcodeLexerCall)

        # Create gcode lexer action
        self.pythonLexerAction = QAction(QIcon.fromTheme('lexer.png'), '&Python\nLexer', self)
        self.pythonLexerAction.setShortcut('Ctrl+P')
        self.pythonLexerAction.setStatusTip('Set Python highlighting')
        self.pythonLexerAction.triggered.connect(self.pythonLexerCall)

        self.searchText = QLineEdit(self)
        self.searchText.setToolTip('Search Text')
        self.searchText.setStatusTip('Text to search for')
        self.searchText.installEventFilter(self)

        self.replaceText = QLineEdit(self)
        self.replaceText.setToolTip('Replace Text')
        self.replaceText.setStatusTip('Replace search text with this text')
        self.replaceText.installEventFilter(self)

        # Create new action
        self.undoAction = QAction(QIcon.fromTheme('edit-undo'), 'Undo', self)
        self.undoAction.setStatusTip('Undo')
        self.undoAction.triggered.connect(self.undoCall)

        # create redo action
        self.redoAction = QAction(QIcon.fromTheme('edit-redo'), 'Redo', self)
        self.redoAction.setStatusTip('Redo')
        self.redoAction.triggered.connect(self.redoCall)

        # create replace action
        self.replaceAction = QAction(QIcon.fromTheme('edit-find-replace'), 'Replace', self)
        self.replaceAction.setStatusTip('Replace text')
        self.replaceAction.triggered.connect(self.replaceCall)

        # create find action
        self.findAction = QAction(QIcon.fromTheme('edit-find'), 'Find', self)
        self.findAction.setStatusTip('Find next occurrence of text')
        self.findAction.triggered.connect(self.findCall)

        # create next action
        self.previousAction = QAction(QIcon.fromTheme('go-previous'), 'Find Previous', self)
        self.previousAction.setStatusTip('Find previous occurrence of text')
        self.previousAction.triggered.connect(self.previousCall)

        # create case action
        self.caseAction = QAction(QIcon.fromTheme('edit-case'), 'Aa', self)
        self.caseAction.setToolTip('Toggle Match Case')
        self.caseAction.setStatusTip('Toggle between any case and match case')
        self.caseAction.setCheckable(1)      
        self.caseAction.triggered.connect(self.caseCall)

    # catch focusIn event to pop keyboard dialog
    def eventFilter(self, obj, event):
        if event.type() == QEvent.FocusIn:
            if isinstance(obj, QLineEdit):
                # only if mouse selected
                if event.reason () == 0:
                    self.popEntry(obj)
        return super().eventFilter(obj, event)

    # callback functions built for easy class patching ##########
    # want to refrain from renaming these functions as it will break
    # any class patch user's use
    # we split them like this so a user can intercept the callback
    # but still call the original functionality

    def caseCall(self):
        self.case()
    def case(self):
        self.isCaseSensitive -=1
        self.isCaseSensitive *=-1

    def exitCall(self):
        self.exit()
    def exit(self):
        if self.editor.isModified():
            result = self.killCheck()
            if result:
                try:
                    self.editor.reload_last(None)
                except Exception as e:
                    print (e)
                self.readOnlyMode()
            return result
        return True

    def findCall(self):
        self.find()
    def find(self):
        self.editor.search(str(self.searchText.text()),
                             re=False, case=self.isCaseSensitive,
                             word=False, wrap= True, fwd=True)

    def previousCall(self):
        self.previous()
    def previous(self):
        self.editor.setCursorPosition(self.editor.getSelection()[0],
                                      self.editor.getSelection()[1])
        self.editor.search(str(self.searchText.text()),
                           re=False, case=self.isCaseSensitive,
                           word=False, wrap=True, fwd=False)

    def gcodeLexerCall(self):
        self.gcodeLexer()
    def gcodeLexer(self):
        self.editor.set_gcode_lexer()

    def nextCall(self):
        self.next()
    def next(self):
        self.editor.search(str(self.searchText.text()),
                             re=False, case=self.isCaseSensitive,
                             word=False, wrap=True, fwd=False)
        self.editor.search_Next()

    def newCall(self):
        self.new()
    def new(self):
        if self.editor.isModified():
            result = self.killCheck()
            if result:
                self.editor.new_text()
        else:
            self.editor.new_text()

    def openCall(self):
        self.open()
    def open(self):
        self.getFileName()
    def openReturn(self,f):
        ACTION.OPEN_PROGRAM(f)
        self.editor.setModified(False)

    def pythonLexerCall(self):
        self.pythonLexer()
    def pythonLexer(self):
        self.editor.set_python_lexer()

    def redoCall(self):
        self.redo()
    def redo(self):
        self.editor.redo()

    def replaceCall(self):
        self.replace()
    def replace(self):
        self.editor.replace_text(str(self.replaceText.text()))
        self.editor.search(str(self.searchText.text()),
                             re=False, case=self.isCaseSensitive,
                             word=False, wrap=True, fwd=True)

    def saveCall(self):
        self.save()
    def save(self):
        self.getSaveFileName()
    def saveReturn(self, fname):
        saved = ACTION.SAVE_PROGRAM(self.editor.text(), fname)
        if saved is not None:
            self.editor.setModified(False)
            ACTION.OPEN_PROGRAM(saved)

    def undoCall(self):
        self.undo()
    def undo(self):
        self.editor.undo()

    # helper functions ############################################

    def saveSettings(self):
        self.SETTINGS_.beginGroup("geditor-{}".format(self.objectName()))
        self.SETTINGS_.setValue('state', QVariant(self.saveState().data()))
        self.SETTINGS_.endGroup()

    def restoreSettings(self):
        # set recorded toolbar settings
        self.SETTINGS_.beginGroup("geditor-{}".format(self.objectName()))
        state = self.SETTINGS_.value('state')
        self.SETTINGS_.endGroup()
        if not state is None:
            try:
                self.restoreState(QByteArray(state))
            except Exception as e:
                print(e)
            else:
                return True
        return False

    def editMode(self):
        self.editor.setReadOnly(False)
        result = self.restoreSettings()
        check = (self.toolBar.toggleViewAction().isChecked() and
                    self.toolBarEdit.toggleViewAction().isChecked() and
                    self.toolBarEntry.toggleViewAction().isChecked() and
                    self.toolBarLexer.toggleViewAction().isChecked())
        if not check:
            self.toolBar.toggleViewAction().setChecked(False)
            self.toolBar.toggleViewAction().trigger()
        if not result:
            self.toolBarEdit.toggleViewAction().setChecked(False)
            self.toolBarEdit.toggleViewAction().trigger()
            self.toolBarEntry.toggleViewAction().setChecked(False)
            self.toolBarEntry.toggleViewAction().trigger()
            self.toolBarLexer.toggleViewAction().setChecked(False)
            self.toolBarLexer.toggleViewAction().trigger()

    def readOnlyMode(self, save = True):
        if save:
            self.saveSettings()
        self.editor.setReadOnly(True)
        self.toolBar.toggleViewAction().setChecked(True)
        self.toolBar.toggleViewAction().trigger()
        self.toolBarEdit.toggleViewAction().setChecked(True)
        self.toolBarEdit.toggleViewAction().trigger()
        self.toolBarEntry.toggleViewAction().setChecked(True)
        self.toolBarEntry.toggleViewAction().trigger()
        self.toolBarLexer.toggleViewAction().setChecked(True)
        self.toolBarLexer.toggleViewAction().trigger()


    def getFileName(self):
        mess = {'NAME':self.load_dialog_code,'ID':'%s__' % self.objectName(),
            'TITLE':'Load Editor'}
        STATUS.emit('dialog-request', mess)

    def getSaveFileName(self):
        mess = {'NAME':self.save_dialog_code,'ID':'%s__' % self.objectName(),
            'TITLE':'Save Editor'}
        STATUS.emit('dialog-request', mess)


    def popEntry(self, obj):
            mess = {'NAME':self.dialog_code,
                    'ID':'%s__' % self.objectName(),
                    'OVERLAY':False,
                    'OBJECT':obj,
                    'TITLE':'Set Entry for {}'.format(obj.objectName().upper()),
                    'GEONAME':'_{}'.format(self.dialog_code)
            }
            STATUS.emit('dialog-request', mess)
            LOG.debug('message sent:{}'.format (mess))

    # process the STATUS return message
    def returnFromDialog(self, w, message):
        if message.get('NAME') == self.load_dialog_code:
            path = message.get('RETURN')
            code = bool(message.get('ID') == '%s__'% self.objectName())
            if path and code:
                self.openReturn(path)
        elif message.get('NAME') == self.save_dialog_code:
            path = message.get('RETURN')
            code = bool(message.get('ID') == '%s__'% self.objectName())
            if path and code:
                self.saveReturn(path)
        elif message.get('NAME') == self.dialog_code:
            txt = message['RETURN']
            code = bool(message.get('ID') == '%s__'% self.objectName())
            if code and txt is not None:
                LOG.debug('message return:{}'.format (message))
                obj = message.get('OBJECT')
                obj.setText(str(txt))

    def killCheck(self):
        choice = QMessageBox.question(self, 'Warning!',
             """This file has changed since loading and
has not been saved. You will lose your changes.
Still want to proceed?""",
                                            QMessageBox.Yes | QMessageBox.No)
        if choice == QMessageBox.Yes:
            return True
        else:
            return False

    def emit_percent(self, percent):
        self.percentDone.emit(percent)

    def select_lineup(self):
        self.editor.select_lineup(None)

    def select_linedown(self):
        self.editor.select_linedown(None)

    def select_line(self, line):
        self.editor.highlight_line(None, line)

    def jump_line(self, jump):
        self.editor.jump_line(jump)

    def get_line(self):
        return self.editor.getCursorPosition()[0] +1

    def set_margin_width(self,width):
        self.editor.set_margin_width(width)

    def set_font(self, font):
        self.editor.font = font
        for i in range(0,4):
            self.editor.lexer.setFont(font,i)

    def set_background_color(self, color):
        self.editor.set_background_color(color)

    def isReadOnly(self):
        return self.editor.isReadOnly()

    # designer recognized getter/setters

    # auto_show_mdi status
    # These adjust the self.editor instance
    def set_auto_show_mdi(self, data):
        self.editor.auto_show_mdi = data
    def get_auto_show_mdi(self):
        return self.editor.auto_show_mdi
    def reset_auto_show_mdi(self):
        self.editor.auto_show_mdi = True
    auto_show_mdi_status = pyqtProperty(bool, get_auto_show_mdi, set_auto_show_mdi, reset_auto_show_mdi)

    # designer recognized getter/setters
    # auto_show_manual status
    def set_auto_show_manual(self, data):
        self.editor.auto_show_manual = data
    def get_auto_show_manual(self):
        return self.editor.auto_show_manual
    def reset_auto_show_manual(self):
        self.editor.auto_show_manual = True
    auto_show_manual_status = pyqtProperty(bool, get_auto_show_manual, set_auto_show_manual, reset_auto_show_manual)

    # designer recognized getter/setters
    # show_editor status
    def set_show_editor(self, data):
        self._show_editor = data
        if data:
            self.toolBar.show()
            self.toolBarLexer.show()
            self.toolBarEntry.show()
            self.toolBarEdit.show()
        else:
            self.toolBar.hide()
            self.toolBarLexer.hide()
            self.toolBarEntry.hide()
            self.toolBarEdit.hide()
    def get_show_editor(self):
        return self._show_editor
    def reset_show_editor(self):
        self._show_editor = True
        self.toolBar.show()
        self.toolBarLexer.show()
        self.toolBarEntry.show()
        self.toolBarEdit.show()
    show_editor = pyqtProperty(bool, get_show_editor, set_show_editor, reset_show_editor)


# for direct testing
if __name__ == "__main__":
    from PyQt5.QtWidgets import *
    from PyQt5.QtCore import *
    from PyQt5.QtGui import *

    app = QApplication(sys.argv)
    w = GcodeEditor()

    w.editMode()
    w.editor.setText(''' This is test text
a
a
a
B
b
n
C
C
C

This is the end of the test text.''')
    if 0:
        w.toolBar.hide()
    if 0:
        w.pythonLexerAction.setVisible(False)
        w.gCodeLexerAction.setVisible(False)
    if 0:
        w.openAction.setVisible(False)
        w.newAction.setVisible(False)
    if 0:
        w.saveAction.setVisible(False)
        w.exitAction.setVisible(False)
    if 0:
        w.label.setText('<b>Edit mode title label</b>')
    w.show()
    sys.exit( app.exec_() )


