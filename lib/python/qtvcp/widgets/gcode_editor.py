#!/usr/bin/python
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

from PyQt5.QtCore import pyqtProperty, pyqtSignal, QSize, QObject
from PyQt5.QtGui import QFont, QFontMetrics, QColor, QIcon
from PyQt5.QtWidgets import QMainWindow, QWidget, QPushButton, QAction,\
         QVBoxLayout,QToolBar,QGroupBox,QLineEdit, QHBoxLayout,QMessageBox, \
            QFileDialog, QFrame, QLabel
try:
    from PyQt5.Qsci import QsciScintilla, QsciLexerCustom, QsciLexerPython
except ImportError as e:
    LOG.critical("Can't import QsciScintilla - is package python-pyqt5.qsci installed?", exc_info=e)
    sys.exit(1)

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

# Set the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


##############################################################
# Simple custom lexer for Gcode
##############################################################
class GcodeLexer(QsciLexerCustom):
    def __init__(self, parent=None):
        super(GcodeLexer, self).__init__(parent)
        self._styles = {
            0: 'Default',
            1: 'Comment',
            2: 'Key',
            3: 'Assignment',
            4: 'Value',
            }
        for key, value in self._styles.iteritems():
            setattr(self, value, key)
        font = QFont()
        font.setFamily('Courier')
        font.setFixedPitch(True)
        font.setPointSize(12)
        font.setBold(True)
        self.setFont(font, 2)

    # Paper sets the background color of each style of text
    def setPaperBackground(self, color, style=None):
        if style is None:
            for i in range(0, 5):
                self.setPaper(color, i)
        else:
            self.setPaper(color, style)

    def language(self):
        return"G code"

    def description(self, style):
        if style < len(self._styles):
            description = "Custom lexer for the G code programming languages"
        else:
            description = ""
        return description

    def defaultColor(self, style):
        if style == self.Default:
            return QColor('#000000')  # black
        elif style == self.Comment:
            return QColor('#000000')  # black
        elif style == self.Key:
            return QColor('#0000CC')  # blue
        elif style == self.Assignment:
            return QColor('#CC0000')  # red
        elif style == self.Value:
            return QColor('#00CC00')  # green
        return QsciLexerCustom.defaultColor(self, style)

    def styleText(self, start, end):
        editor = self.editor()
        if editor is None:
            return

        # scintilla works with encoded bytes, not decoded characters.
        # this matters if the source contains non-ascii characters and
        # a multi-byte encoding is used (e.g. utf-8)
        source = ''
        if end > editor.length():
            end = editor.length()
        if end > start:
            if sys.hexversion >= 0x02060000:
                # faster when styling big files, but needs python 2.6
                source = bytearray(end - start)
                editor.SendScintilla(
                    editor.SCI_GETTEXTRANGE, start, end, source)
            else:
                source = unicode(editor.text()).encode('utf-8')[start:end]
        if not source:
            return

        # the line index will also be needed to implement folding
        index = editor.SendScintilla(editor.SCI_LINEFROMPOSITION, start)
        if index > 0:
            # the previous state may be needed for multi-line styling
            pos = editor.SendScintilla(
                      editor.SCI_GETLINEENDPOSITION, index - 1)
            state = editor.SendScintilla(editor.SCI_GETSTYLEAT, pos)
        else:
            state = self.Default

        set_style = self.setStyling
        self.startStyling(start, 0x1f)

        # scintilla always asks to style whole lines
        for line in source.splitlines(True):
            #print line
            length = len(line)
            graymode = False
            msg = ('msg' in line.lower() or 'debug' in line.lower())
            for char in str(line):
                #print char
                if char == ('('):
                    graymode = True
                    set_style(1, self.Comment)
                    continue
                elif char == (')'):
                    graymode = False
                    set_style(1, self.Comment)
                    continue
                elif graymode:
                    if (msg and char.lower() in ('m', 's', 'g', ',', 'd', 'e', 'b', 'u')):
                        set_style(1, self.Assignment)
                        if char == ',': msg = False
                    else:
                        set_style(1, self.Comment)
                    continue
                elif char in ('%', '<', '>', '#', '='):
                    state = self.Assignment
                elif char in ('[', ']'):
                    state = self.Value
                elif char.isalpha():
                    state = self.Key
                elif char.isdigit():
                    state = self.Default
                else:
                    state = self.Default
                set_style(1, state)

            # folding implementation goes here
            index += 1


##########################################################
# Base editor class
##########################################################
class EditorBase(QsciScintilla):
    ARROW_MARKER_NUM = 8

    def __init__(self, parent=None):
        super(EditorBase, self).__init__(parent)
        # don't allow editing by default
        self.setReadOnly(True)
        # Set the default font
        self.font = QFont()
        self.font.setFamily('Courier')
        self.font.setFixedPitch(True)
        self.font.setPointSize(12)
        self.setFont(self.font)
        self.setMarginsFont(self.font)

        # Margin 0 is used for line numbers
        fontmetrics = QFontMetrics(self.font)
        self.setMarginsFont(self.font)
        self.setMarginWidth(0, fontmetrics.width("00000") + 6)
        self.setMarginLineNumbers(0, True)
        self.setMarginsBackgroundColor(QColor("#cccccc"))

        # Clickable margin 1 for showing markers
        self.setMarginSensitivity(1, False)
        # setting marker margin width to zero make the marker highlight line
        self.setMarginWidth(1, 0)
        #self.matginClicked.connect(self.on_margin_clicked)
        self.markerDefine(QsciScintilla.RightArrow,
                          self.ARROW_MARKER_NUM)
        self.setMarkerBackgroundColor(QColor("#ffe4e4"),
                                      self.ARROW_MARKER_NUM)

        # Brace matching: enable for a brace immediately before or after
        # the current position
        #
        self.setBraceMatching(QsciScintilla.SloppyBraceMatch)

        # Current line visible with special background color
        self.setCaretLineVisible(True)
        self.setCaretLineBackgroundColor(QColor("#ffe4e4"))

        # Set custom gcode lexer
        self.set_gcode_lexer()

        # Don't want to see the horizontal scrollbar at all
        # Use raw message to Scintilla here (all messages are documented
        # here: http://www.scintilla.org/ScintillaDoc.html)
        #self.SendScintilla(QsciScintilla.SCI_SETHSCROLLBAR, 0)
        self.SendScintilla(QsciScintilla.SCI_SETSCROLLWIDTH,700)
        self.SendScintilla(QsciScintilla.SCI_SETSCROLLWIDTHTRACKING)

        # default gray background
        self.set_background_color('#C0C0C0')

        # not too small
        self.setMinimumSize(200, 100)
        self.filepath = None

    # must set lexer paper background color _and_ editor background color it seems
    def set_background_color(self, color):
        self.SendScintilla(QsciScintilla.SCI_STYLESETBACK, QsciScintilla.STYLE_DEFAULT, QColor(color))
        self.lexer.setPaperBackground(QColor(color))

    def on_margin_clicked(self, nmargin, nline, modifiers):
        # Toggle marker for the line the margin was clicked on
        if self.markersAtLine(nline) != 0:
            self.markerDelete(nline, self.ARROW_MARKER_NUM)
        else:
            self.markerAdd(nline, self.ARROW_MARKER_NUM)

    def set_python_lexer(self):
        self.lexer = QsciLexerPython()
        self.lexer.setDefaultFont(self.font)
        self.setLexer(self.lexer)
        self.SendScintilla(QsciScintilla.SCI_STYLESETFONT, 1, 'Courier')

    def set_gcode_lexer(self):
        self.lexer = GcodeLexer(self)
        self.lexer.setDefaultFont(self.font)
        self.setLexer(self.lexer)
        self.set_background_color('#C0C0C0')

    def new_text(self):
        self.setText('')

    def load_text(self, filepath):
        self.filepath = filepath
        try:
            fp = os.path.expanduser(filepath)
            self.setText(open(fp).read())
        except:
            LOG.error('File path is not valid: {}'.format(filepath))
            self.setText('')
            return
        self.ensureCursorVisible()
        self.SendScintilla(QsciScintilla.SCI_VERTICALCENTRECARET)
        self.setModified(False)

    def save_text(self):
        with open(self.filepath + 'text', "w") as text_file:
            text_file.write(self.text())

    def replace_text(self, text):
        self.replace(text)

    def search(self, text, re = False,case= False, word=False, wrap= False, fwd=True):
        self.findFirst(text, re, case, word, wrap, fwd)

    def search_Next(self):
        self.SendScintilla(QsciScintilla.SCI_SEARCHANCHOR)
        self.findNext()

##########################################################
# Gcode display widget (intended read-only)
##########################################################
class GcodeDisplay(EditorBase, _HalWidgetBase):
    ARROW_MARKER_NUM = 8

    def __init__(self, parent=None):
        super(GcodeDisplay, self).__init__(parent)
        # linuxcnc defaults
        self.idle_line_reset = False
        self._last_filename = None
        self.auto_show_mdi = True
        self.auto_show_manual = False
        self.auto_show_preference = True
        self.last_line = None

    def _hal_init(self):
        self.cursorPositionChanged.connect(self.line_changed)
        if self.auto_show_mdi:
            STATUS.connect('mode-mdi', self.load_mdi)
            STATUS.connect('reload-mdi-history', self.load_mdi)
            STATUS.connect('mode-auto', self.reload_last)
            STATUS.connect('move-text-lineup', self.select_lineup)
            STATUS.connect('move-text-linedown', self.select_linedown)
        if self.auto_show_manual:
            STATUS.connect('mode-manual', self.load_manual)
            STATUS.connect('machine-log-changed', self.load_manual)
        if self.auto_show_preference:
            STATUS.connect('show-preference', self.load_preference)
        STATUS.connect('file-loaded', self.load_program)
        STATUS.connect('line-changed', self.highlight_line)
        if self.idle_line_reset:
            STATUS.connect('interp_idle', lambda w: self.set_line_number(None, 0))

    def load_program(self, w, filename=None):
        if filename is None:
            filename = self._last_filename
        else:
            self._last_filename = filename
        self.load_text(filename)
        #self.zoomTo(6)
        self.setCursorPosition(0, 0)
        self.setModified(False)

    # when switching from MDI to AUTO we need to reload the
    # last (linuxcnc loaded) program.
    def reload_last(self, w):
        self.load_text(STATUS.old['file'])
        self.setCursorPosition(0, 0)

    # With the auto_show__mdi option, MDI history is shown
    def load_mdi(self, w):
        self.load_text(INFO.MDI_HISTORY_PATH)
        self._last_filename = INFO.MDI_HISTORY_PATH
        #print 'font point size', self.font().pointSize()
        #self.zoomTo(10)
        #print 'font point size', self.font().pointSize()
        self.setCursorPosition(self.lines(), 0)

    # With the auto_show__mdi option, MDI history is shown
    def load_manual(self, w):
        if STATUS.is_man_mode():
            self.load_text(INFO.MACHINE_LOG_HISTORY_PATH)
            self.setCursorPosition(self.lines(), 0)

    def load_preference(self, w):
            self.load_text(self.PATHS_.PREFS_FILENAME)
            self.setCursorPosition(self.lines(), 0)

    def load_text(self, filename):
        if filename:
            try:
                fp = os.path.expanduser(filename)
                self.setText(open(fp).read())
                self.last_line = None
                self.ensureCursorVisible()
                self.SendScintilla(QsciScintilla.SCI_VERTICALCENTRECARET)
                return
            except:
                LOG.error('File path is not valid: {}'.format(filename))
        self.setText('')



    def highlight_line(self, w, line):
        if STATUS.is_auto_running():
            if not STATUS.old['file'] == self._last_filename:
                LOG.debug('should reload the display')
                self.load_text(STATUS.old['file'])
                self._last_filename = STATUS.old['file']
            self.emit_percent(line*100/self.lines())
        self.markerAdd(line, self.ARROW_MARKER_NUM)
        if self.last_line:
            self.markerDelete(self.last_line, self.ARROW_MARKER_NUM)
        self.setCursorPosition(line, 0)
        self.ensureCursorVisible()
        self.SendScintilla(QsciScintilla.SCI_VERTICALCENTRECARET)
        self.last_line = line

    def emit_percent(self, percent):
        pass

    def set_line_number(self, line):
        STATUS.emit('gcode-line-selected', line)

    def line_changed(self, line, index):
        #LOG.debug('Line changed: {}'.format(line))
        if STATUS.is_auto_running() is False:
            self.markerDeleteAll(-1)
            if STATUS.is_mdi_mode():
                line_text = str(self.text(line)).strip()
                STATUS.emit('mdi-line-selected', line_text, self._last_filename)
            else:
                self.set_line_number(line)

    def select_lineup(self, w):
        line, col = self.getCursorPosition()
        LOG.debug(line)
        self.setCursorPosition(line-1, 0)
        self.highlight_line(None, line-1)

    def select_linedown(self, w):
        line, col = self.getCursorPosition()
        LOG.debug(line)
        self.setCursorPosition(line+1, 0)
        self.highlight_line(None, line+1)

    # designer recognized getter/setters
    # auto_show_mdi status
    def set_auto_show_mdi(self, data):
        self.auto_show_mdi = data
    def get_auto_show_mdi(self):
        return self.auto_show_mdi
    def reset_auto_show_mdi(self):
        self.auto_show_mdi = True
    auto_show_mdi_status = pyqtProperty(bool, get_auto_show_mdi, set_auto_show_mdi, reset_auto_show_mdi)


#############################################
# For Editing Gcode
#############################################

class GcodeEditor(QWidget, _HalWidgetBase):
    percentDone = pyqtSignal(int)

    def __init__(self, parent=None):
        super(GcodeEditor, self).__init__(parent)
        self.load_dialog_code = 'LOAD'
        self.save_dialog_code = 'SAVE'
        STATUS.connect('general',self.returnFromDialog)

        self.isCaseSensitive = 0

        self.setMinimumSize(QSize(300, 200))    
        self.setWindowTitle("PyQt5 editor test example") 

        lay = QVBoxLayout()
        lay.setContentsMargins(0,0,0,0)
        self.setLayout(lay)

        # make editor
        self.editor = GcodeDisplay(self)

        # class patch editor's function to ours
        # so we get the lines percent update
        self.editor.emit_percent = self.emit_percent

        self.editor.setReadOnly(True)
        self.editor.setModified(False)

        ################################
        # add menubar actions
        ################################

        # Create new action
        newAction = QAction(QIcon.fromTheme('document-new'), 'New', self)        
        newAction.setShortcut('Ctrl+N')
        newAction.setStatusTip('New document')
        newAction.triggered.connect(self.newCall)

        # Create open action
        openAction = QAction(QIcon.fromTheme('document-open'), '&Open', self)        
        openAction.setShortcut('Ctrl+O')
        openAction.setStatusTip('Open document')
        openAction.triggered.connect(self.openCall)

        # Create save action
        saveAction = QAction(QIcon.fromTheme('document-save'), '&save', self)        
        saveAction.setShortcut('Ctrl+S')
        saveAction.setStatusTip('save document')
        saveAction.triggered.connect(self.saveCall)

        # Create exit action
        exitAction = QAction(QIcon.fromTheme('application-exit'), '&Exit', self)        
        exitAction.setShortcut('Ctrl+Q')
        exitAction.setStatusTip('Exit application')
        exitAction.triggered.connect(self.exitCall)

        # Create gcode lexer action
        gCodeLexerAction = QAction(QIcon.fromTheme('lexer.png'), '&Gcode\n lexer', self)
        gCodeLexerAction.setCheckable(1)
        gCodeLexerAction.setShortcut('Ctrl+G')
        gCodeLexerAction.setStatusTip('Set Gcode highlighting')
        gCodeLexerAction.triggered.connect(self.gcodeLexerCall)

        # Create gcode lexer action
        pythonLexerAction = QAction(QIcon.fromTheme('lexer.png'), '&python\n lexer', self)        
        pythonLexerAction.setShortcut('Ctrl+P')
        pythonLexerAction.setStatusTip('Set Python highlighting')
        pythonLexerAction.triggered.connect(self.pythonLexerCall)

        # Create toolbar and add action
        toolBar = QToolBar('File')
        toolBar.addAction(newAction)
        toolBar.addAction(openAction)
        toolBar.addAction(saveAction)
        toolBar.addAction(exitAction)

        toolBar.addSeparator()

        # add lexer actions
        toolBar.addAction(gCodeLexerAction)
        toolBar.addAction(pythonLexerAction)

        toolBar.addSeparator()
        toolBar.addWidget(QLabel('<html><head/><body><p><span style=" font-size:20pt; font-weight:600;">Edit Mode</span></p></body></html>'))

        # create a frame for buttons
        box = QHBoxLayout()
        box.addWidget(toolBar)

        self.topMenu = QFrame()
        self.topMenu.setLayout(box)

        # add widgets
        lay.addWidget(self.topMenu)
        lay.addWidget(self.editor)
        lay.addWidget(self.createGroup())

        self.readOnlyMode()

    def createGroup(self):
        self.bottomMenu = QFrame()

        self.searchText = QLineEdit(self)
        self.replaceText = QLineEdit(self)

        toolBar = QToolBar()
        # Create new action
        undoAction = QAction(QIcon.fromTheme('edit-undo'), 'Undo', self)        
        undoAction.setStatusTip('Undo')
        undoAction.triggered.connect(self.undoCall)
        toolBar.addAction(undoAction)

        # create redo action
        redoAction = QAction(QIcon.fromTheme('edit-redo'), 'Redo', self)        
        redoAction.setStatusTip('Undo')
        redoAction.triggered.connect(self.redoCall)
        toolBar.addAction(redoAction)

        toolBar.addSeparator()

        # create replace action
        replaceAction = QAction(QIcon.fromTheme('edit-find-replace'), 'Replace', self)        
        replaceAction.triggered.connect(self.replaceCall)
        toolBar.addAction(replaceAction)

        # create find action
        findAction = QAction(QIcon.fromTheme('edit-find'), 'Find', self)        
        findAction.triggered.connect(self.findCall)
        toolBar.addAction(findAction)

        # create next action
        nextAction = QAction(QIcon.fromTheme('go-previous'), 'Find Previous', self)        
        nextAction.triggered.connect(self.nextCall)
        toolBar.addAction(nextAction)

        toolBar.addSeparator()

        # create case action
        caseAction = QAction(QIcon.fromTheme('edit-case'), 'Aa', self)  
        caseAction.setCheckable(1)      
        caseAction.triggered.connect(self.caseCall)
        toolBar.addAction(caseAction)

        box = QHBoxLayout()
        box.addWidget(toolBar)
        box.addWidget(self.searchText)
        box.addWidget(self.replaceText)
        box.addStretch(1)
        self.bottomMenu.setLayout(box)

        return self.bottomMenu

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
        print self.isCaseSensitive

    def exitCall(self):
        self.exit()
    def exit(self):
        if self.editor.isModified():
            result = self.killCheck()
            if result:
                self.readOnlyMode()

    def findCall(self):
        self.find()
    def find(self):
        self.editor.search(str(self.searchText.text()),
                             re=False, case=self.isCaseSensitive,
                             word=False, wrap= False, fwd=True)

    def gcodeLexerCall(self):
        self.gcodeLexer()
    def gcodeLexer(self):
        self.editor.set_gcode_lexer()

    def nextCall(self):
        self.next()
    def next(self):
        self.editor.search(str(self.searchText.text()),False)
        self.editor.search_Next()

    def newCall(self):
        self.new()
    def new(self):
        if self.editor.isModified():
            result = self.killCheck()
            if result:
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

    def saveCall(self):
        self.save()
    def save(self):
        self.getSaveFileName()
    def saveReturn(self, fname):
        ACTION.SAVE_PROGRAM(self.editor.text(), fname)
        self.editor.setModified(False)
        ACTION.OPEN_PROGRAM(fname)

    def undoCall(self):
        self.undo()
    def undo(self):
        self.editor.undo()

    # helper functions ############################################

    def _hal_init(self):
        # name the top and bottom frames so it's easier to style
        self.bottomMenu.setObjectName('%sBottomButtonFrame'% self.objectName())
        self.topMenu.setObjectName('%sTopButtonFrame'% self.objectName())

    def editMode(self):
        self.topMenu.show()
        self.bottomMenu.show()
        self.editor.setReadOnly(False)

    def readOnlyMode(self):
        self.topMenu.hide()
        self.bottomMenu.hide()
        self.editor.setReadOnly(True)

    def getFileName(self):
        mess = {'NAME':self.load_dialog_code,'ID':'%s__' % self.objectName(),
            'TITLE':'Load Editor'}
        STATUS.emit('dialog-request', mess)

    def getSaveFileName(self):
        mess = {'NAME':self.save_dialog_code,'ID':'%s__' % self.objectName(),
            'TITLE':'Save Editor'}
        STATUS.emit('dialog-request', mess)

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

    def killCheck(self):
        choice = QMessageBox.question(self, 'Warning!!',
                                            "This file has changed since loading...Still want to proceed?",
                                            QMessageBox.Yes | QMessageBox.No)
        if choice == QMessageBox.Yes:
            return True
        else:
            return False

    def emit_percent(self, percent):
        self.percentDone.emit(percent)

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

# for direct testing
if __name__ == "__main__":
    from PyQt5.QtWidgets import *
    from PyQt5.QtCore import *
    from PyQt5.QtGui import *

    app = QtWidgets.QApplication(sys.argv)
    w = GcodeEditor()
    w.editMode()
    w.show()
    sys.exit( app.exec_() )


