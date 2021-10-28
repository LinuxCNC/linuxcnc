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

from PyQt5.QtCore import pyqtProperty, pyqtSignal, QSize, QObject
from PyQt5.QtGui import QFont, QFontMetrics, QColor, QIcon, QPalette
from PyQt5.QtWidgets import QMainWindow, QWidget, QPushButton, QAction,\
         QVBoxLayout,QToolBar,QGroupBox,QLineEdit, QHBoxLayout,QMessageBox, \
            QFileDialog, QFrame, QLabel, QStyleOption

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

# Force the log level for this module
# LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

# load this after Logging set up so we get a nice dialog.
try:
    from PyQt5.Qsci import QsciScintilla, QsciLexerCustom, QsciLexerPython
except ImportError as e:
    LOG.critical("Can't import QsciScintilla - is package python3-pyqt5.qsci installed?", exc_info=e)
    sys.exit(1)

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
            5: 'Axes',
            6: 'Code1',
            7: 'Code2'
            }
        for key, value in self._styles.items():
            setattr(self, value, key)
        self._color6Codes = 'gst'
        self._color7Codes = 'mfpq'

    def setColor6Codes(self, data):
        self._color6Codes = data.lower()
    def getColor6Codes(self):
        return self._color6Codes

    def setColor7Codes(self, data):
        self._color7Codes = data.lower()
    def getColor7Codes(self):
        return self._color7Codes

    # Paper sets the background color of each style of text
    def setPaperBackground(self, color, style=None):
        if style is None:
            for i in range(0, 8):
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
        elif style == self.Axes:
            return QColor('darkgreen')
        elif style == self.Code1:
            return QColor('darkred')
        elif style == self.Code2:
            return QColor('deeppink')

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
                source = str(editor.text())[start:end]
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
        try:
            # scintilla always asks to style whole lines
            for line in source.splitlines(True):
                #print (line.decode('utf-8'))
                graymode = notcode = False
                line = line.decode('utf-8')
                msg = ('msg' in line.lower() or 'debug' in line.lower())
                for char in line:
                    #print (char,msg)
                    if char in('(',';'):
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
                    elif char == ('<'):
                        notcode = True
                        set_style(1, self.Assignment)
                        continue
                    elif char == ('>'):
                        notcode = False
                        set_style(1, self.Assignment)
                        continue
                    elif char in ('%', '#', '='):
                        state = self.Assignment
                    elif char in ('[', ']'):
                        state = self.Value
                    elif char.isalpha():
                        if not notcode:
                            if char.lower() in self._color6Codes:
                                state = self.Code1
                            elif char.lower() in self._color7Codes:
                                state = self.Code2
                            elif char.lower() in ('x','y','z','a','b','c','u','v','w'):
                                state = self.Axes
                        else:
                            state = self.Key
                    elif char.isdigit():
                        state = self.Default
                    else:
                        state = self.Default
                    set_style(1, state)

                # folding implementation goes here
                index += 1
        except Exception as e:
            print(e)


##########################################################
# Base editor class
##########################################################
class EditorBase(QsciScintilla):
    CURRENT_MARKER_NUM = 0
    USER_MARKER_NUM = 1
    _styleMarginsForegroundColor = QColor("#000000")
    _styleMarginsBackgroundColor = QColor("#000000")
    _styleBackgroundColor = QColor("#000000")
    _styleSelectionForegroundColor = QColor("#ffffff")
    _styleSelectionBackgroundColor = QColor("#000000")

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
        self._lastUserLine = 0

        # Margin 0 is used for line numbers
        self.setMarginsFont(self.font)
        self.set_margin_width(7)
        self.setMarginLineNumbers(0, True)
        self.setMarginsBackgroundColor(QColor("#cccccc"))

        # Clickable margin for showing markers
        self.marginClicked.connect(self.on_margin_clicked)

        self.setMarginMarkerMask(0, 0b1111)
        self.setMarginSensitivity(0, True)
        # setting marker margin width to zero make the marker highlight line
        self.setMarginWidth(1, 5)


        # Gcode highlight line
        self.currentHandle = self.markerDefine(QsciScintilla.Background,
                          self.CURRENT_MARKER_NUM)
        self.setMarkerBackgroundColor(QColor("yellow"),
                                      self.CURRENT_MARKER_NUM)

        # user Highlight line
        self.userHandle = self.markerDefine(QsciScintilla.Background,
                          self.USER_MARKER_NUM)
        self.setMarkerBackgroundColor(QColor("red"),
                                      self.USER_MARKER_NUM)

        # Brace matching: enable for a brace immediately before or after
        # the current position
        self.setBraceMatching(QsciScintilla.SloppyBraceMatch)

        # Current line visible with special background color
        self.setCaretLineVisible(False)
        self.SendScintilla(QsciScintilla.SCI_GETCARETLINEVISIBLEALWAYS, True)
        self.setCaretLineBackgroundColor(QColor("#ffe4e4"))
        self.ensureLineVisible(True)

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
        self._stylebackgroundColor = '#C0C0C0'

        # not too small
        self.setMinimumSize(200, 100)
        self.filepath = None

    def mouseDoubleClickEvent(self, event):
        pass

    def setMarginsForegroundColor(self, color):
        super(EditorBase, self).setMarginsForegroundColor(color)
        self._styleMarginsForegroundColor = color

    def marginsForegroundColor(self):
        return self._styleMarginsForegroundColor

    def setMarginsBackgroundColor(self, color):
        super(EditorBase, self).setMarginsBackgroundColor(color)
        self._styleMarginsBackgroundColor = color

    def marginsBackgroundColor(self):
        return self._styleMarginsBackgroundColor

    def set_margin_width(self, width):
        fontmetrics = QFontMetrics(self.font)
        self.setMarginsFont(self.font)
        self.setMarginWidth(0, fontmetrics.width("0"*width) + 6)

    def setBackgroundColor(self, color):
        self._styleBackgroundColor = color
        self.set_background_color(color)

    def backgroundColor(self):
        return self._styleBackgroundColor

    # must set lexer paper background color _and_ editor background color it seems
    def set_background_color(self, color):
        self.SendScintilla(QsciScintilla.SCI_STYLESETBACK, QsciScintilla.STYLE_DEFAULT, QColor(color))
        self.lexer.setPaperBackground(QColor(color))

    def setSelectionBackgroundColor(self, color):
        super(EditorBase, self).setSelectionBackgroundColor(color)
        self._styleSelectionBackgroundColor = color

    def selectionBackgroundColor(self):
        return self._styleSelectionBackgroundColor

    def setSelectionForegroundColor(self, color):
        super(EditorBase, self).setSelectionForegroundColor(color)
        self._styleSelectionForegroundColor = color

    def selectionForegroundColor(self):
        return self._styleSelectionForegroundColor

    def on_margin_clicked(self, nmargin, nline, modifiers):
        # Toggle marker for the line the margin was clicked on
        # 2 means it's already there
        if self.markersAtLine(nline) != 2:
            self.markerDelete(self._lastUserLine, self.USER_MARKER_NUM)
            self.markerAdd(nline, self.USER_MARKER_NUM)
        elif self._lastUserLine != nline:
            self.markerAdd(nline, self.USER_MARKER_NUM)
            self.markerDelete(self._lastUserLine, self.USER_MARKER_NUM)
        else:
            self.markerDelete(self._lastUserLine, self.USER_MARKER_NUM)
            self._lastUserLine = 0
            return
        self._lastUserLine = nline

    def set_python_lexer(self):
        self.lexer = QsciLexerPython()
        self.lexer.setDefaultFont(self.font)
        self.setLexer(self.lexer)
        #self.SendScintilla(QsciScintilla.SCI_STYLESETFONT, 1, 'Courier')

    def set_gcode_lexer(self):
        self.lexer = GcodeLexer(self)
        self.setLexer(self.lexer)

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

    # this allows setting these properties in a stylesheet
    def getColor0(self):
        return self.lexer.color(0)
    def setColor0(self, value):
        self.lexer.setColor(value,0)
    styleColor0 = pyqtProperty(QColor, getColor0, setColor0)

    def getColor1(self):
        return self.lexer.color(1)
    def setColor1(self, value):
        self.lexer.setColor(value,1)
    styleColor1 = pyqtProperty(QColor, getColor1, setColor1)

    def getColor2(self):
        return self.lexer.color(2)
    def setColor2(self, value):
        self.lexer.setColor(value,2)
    styleColor2 = pyqtProperty(QColor, getColor2, setColor2)

    def getColor3(self):
        return self.lexer.color(3)
    def setColor3(self, value):
        self.lexer.setColor(value,3)
    styleColor3 = pyqtProperty(QColor, getColor3, setColor3)

    def getColor4(self):
        return self.lexer.color(4)
    def setColor4(self, value):
        self.lexer.setColor(value,4)
    styleColor4 = pyqtProperty(QColor, getColor4, setColor4)

    def getColor5(self):
        return self.lexer.color(5)
    def setColor5(self, value):
        self.lexer.setColor(value,5)
    styleColor5 = pyqtProperty(QColor, getColor5, setColor5)

    def getColor6(self):
        return self.lexer.color(6)
    def setColor6(self, value):
        self.lexer.setColor(value,6)
    styleColor6 = pyqtProperty(QColor, getColor6, setColor6)

    def getColor7(self):
        return self.lexer.color(7)
    def setColor7(self, value):
        self.lexer.setColor(value,7)
    styleColor7 = pyqtProperty(QColor, getColor7, setColor7)

    def getColor6Codes(self):
        return self.lexer.getColor6Codes()
    def setColor6Codes(self, value):
        self.lexer.setColor6Codes(value)
    def resetColor6Codes(self):
        self.lexer.setColor6Codes('gst')
    color6Codes = pyqtProperty(str, getColor6Codes, setColor6Codes,resetColor6Codes)

    def getColor7Codes(self):
        return self.lexer.getColor7Codes()
    def setColor7Codes(self, value):
        self.lexer.setColor7Codes(value)
    def resetColor7Codes(self):
        self.lexer.setColor7Codes('fmpq')
    color7Codes = pyqtProperty(str, getColor7Codes, setColor7Codes, resetColor7Codes)

    def getColorMarginText(self):
        return self.marginsForegroundColor()
    def setColorMarginText(self, value):
        self.setMarginsForegroundColor(value)
    styleColorMarginText = pyqtProperty(QColor, getColorMarginText, setColorMarginText)

    def getColorMarginBackground(self):
        return self.marginsBackgroundColor()
    def setColorMarginBackground(self, value):
        self.setMarginsBackgroundColor(value)
    styleColorMarginBackground = pyqtProperty(QColor, getColorMarginBackground, setColorMarginBackground)

    def getColorSelectionText(self):
        return self.selectionForegroundColor()
    def setColorSelectionText(self, value):
        self.setSelectionForegroundColor(value)
    styleColorSelectionText = pyqtProperty(QColor, getColorSelectionText, setColorSelectionText)

    def getColorSelectionBackground(self):
        return self.selectionBackgroundColor()
    def setColorSelectionBackground(self, value):
        self.setSelectionBackgroundColor(value)
    styleColorSelectionBackground = pyqtProperty(QColor, getColorSelectionBackground, setColorSelectionBackground)

    def getColorBackground(self):
        return self.backgroundColor()
    def setColorBackground(self, value):
        self.setBackgroundColor(value)
    styleColorBackground = pyqtProperty(QColor, getColorBackground, setColorBackground)

    def getFont0(self):
        return self.lexer.font(0)
    def setFont0(self, value):
        self.lexer.setFont(value,0)
    styleFont0 = pyqtProperty(QFont, getFont0, setFont0)

    def getFont1(self):
        return self.lexer.font(1)
    def setFont1(self, value):
        self.lexer.setFont(value,1)
    styleFont1 = pyqtProperty(QFont, getFont1, setFont1)

    def getFont2(self):
        return self.lexer.font(2)
    def setFont2(self, value):
        self.lexer.setFont(value,2)
    styleFont2 = pyqtProperty(QFont, getFont2, setFont2)

    def getFont3(self):
        return self.lexer.font(3)
    def setFont3(self, value):
        self.lexer.setFont(value,3)
    styleFont3 = pyqtProperty(QFont, getFont3, setFont3)

    def getFont4(self):
        return self.lexer.font(4)
    def setFont4(self, value):
        self.lexer.setFont(value,4)
    styleFont4 = pyqtProperty(QFont, getFont4, setFont4)

    def getFont5(self):
        return self.lexer.font(5)
    def setFont5(self, value):
        self.lexer.setFont(value,5)
    styleFont5 = pyqtProperty(QFont, getFont5, setFont5)

    def getFont6(self):
        return self.lexer.font(6)
    def setFont6(self, value):
        self.lexer.setFont(value,6)
    styleFont6 = pyqtProperty(QFont, getFont6, setFont6)

    def getFont7(self):
        return self.lexer.font(7)
    def setFont7(self, value):
        self.lexer.setFont(value,7)
    styleFont7 = pyqtProperty(QFont, getFont7, setFont7)

    def getFontMargin(self):
        return self.font
    def setFontMargin(self, value):
        self.setMarginsFont(value)
    styleFontMargin = pyqtProperty(QFont, getFontMargin, setFontMargin)


##########################################################
# Gcode display widget (intended read-only)
##########################################################
class GcodeDisplay(EditorBase, _HalWidgetBase):
    CURRENT_MARKER_NUM = 0
    USER_MARKER_NUM = 1
    def __init__(self, parent=None):
        super(GcodeDisplay, self).__init__(parent)
        # linuxcnc defaults
        self.idle_line_reset = False
        self._last_filename = None
        self.auto_show_mdi = True
        self.auto_show_manual = False
        self.auto_show_preference = True
        self.last_line = 0

    def _hal_init(self):
        self.cursorPositionChanged.connect(self.line_changed)
        if self.auto_show_mdi:
            STATUS.connect('mode-mdi', self.load_mdi)
            STATUS.connect('mdi-history-changed', self.load_mdi)
            STATUS.connect('mode-auto', self.reload_last)
            STATUS.connect('move-text-lineup', self.select_lineup)
            STATUS.connect('move-text-linedown', self.select_linedown)
        if self.auto_show_manual:
            STATUS.connect('mode-manual', self.load_manual)
            STATUS.connect('machine-log-changed', self.load_manual)
        if self.auto_show_preference:
            STATUS.connect('show-preference', self.load_preference)
        STATUS.connect('file-loaded', self.load_program)
        STATUS.connect('line-changed', self.external_highlight_request)
        STATUS.connect('graphics-line-selected', self.external_highlight_request)
        STATUS.connect('command-stopped', lambda w: self.run_stopped())

        if self.idle_line_reset:
            STATUS.connect('interp_idle', lambda w: self.set_line_number(0))
        self.markerDeleteHandle(self.currentHandle)

    def load_program(self, w, filename=None):
        if filename is None:
            filename = self._last_filename
        else:
            self._last_filename = filename
        self.load_text(filename)
        #self.zoomTo(6)
        self.setCursorPosition(0, 0)
        self.markerDeleteHandle(self.currentHandle)
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
                self.last_line = 0
                self.ensureCursorVisible()
                self.SendScintilla(QsciScintilla.SCI_VERTICALCENTRECARET)
                return
            except:
                LOG.error('File path is not valid: {}'.format(filename))
        self.setText('')

    # external line numbers start at 1 - convert that to start at 0
    def external_highlight_request(self, w, line):
        if line in (-1, None):
            return
        if STATUS.is_auto_running():
            self.highlight_line(None, line-1)
            return
        LOG.debug('editor: got external highlight {}'.format(line))
        #self.highlight_line(None, line-1)
        self.ensureLineVisible(line-1)
        #self.setSelection(line-1,0,line-1,self.lineLength(line-1)-1)
        self.moveMarker(line-1)
        self.selectAll(False)

    def moveMarker(self, line):
        if STATUS.stat.file == '':
            self.last_line = 0
            return
        self.markerDeleteHandle(self.currentHandle)
        self.currentHandle = self.markerAdd(line, self.CURRENT_MARKER_NUM)
        self.last_line = line

    def highlight_line(self, w, line):
        LOG.debug('editor: highlight line {}'.format(line))
        if STATUS.is_auto_running():
            if not STATUS.old['file'] == self._last_filename:
                LOG.debug('should reload the display')
                self.load_text(STATUS.old['file'])
                self._last_filename = STATUS.old['file']
            self.emit_percent(line*100/self.lines())
        self.moveMarker(line)
        self.setCursorPosition(line, 0)
        self.ensureCursorVisible()
        self.SendScintilla(QsciScintilla.SCI_VERTICALCENTRECARET)

    def emit_percent(self, percent):
        pass

    def run_stopped(self):
        self.emit_percent(-1)

    def set_line_number(self, line):
        STATUS.emit('gcode-line-selected', line+1)

    def line_changed(self, line, index):
        LOG.debug('Line changed: {}'.format(line))
        if STATUS.is_auto_running() is False:
            if STATUS.is_mdi_mode():
                line_text = str(self.text(line)).strip()
                STATUS.emit('mdi-line-selected', line_text, self._last_filename)
            else:
                self.moveMarker(line)
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

    def jump_line(self, jump):
        line, col = self.getCursorPosition()
        line = line + jump
        LOG.debug(line)
        if line <0:
            line = 0
        elif line > self.lines():
            line = self.lines()
        self.setCursorPosition(line, 0)
        self.highlight_line(None, line)

    # designer recognized getter/setters
    # auto_show_mdi status
    def set_auto_show_mdi(self, data):
        self.auto_show_mdi = data
    def get_auto_show_mdi(self):
        return self.auto_show_mdi
    def reset_auto_show_mdi(self):
        self.auto_show_mdi = True
    auto_show_mdi_status = pyqtProperty(bool, get_auto_show_mdi, set_auto_show_mdi, reset_auto_show_mdi)

    # designer recognized getter/setters
    # auto_show_manual status
    def set_auto_show_manual(self, data):
        self.auto_show_manual = data
    def get_auto_show_manual(self):
        return self.auto_show_manual
    def reset_auto_show_manual(self):
        self.auto_show_manual = True
    auto_show_manual_status = pyqtProperty(bool, get_auto_show_manual, set_auto_show_manual, reset_auto_show_manual)

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
        self.exitAction.setStatusTip('Exit application')
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

        # Create toolbar and add action
        self.toolBar = QToolBar('File')
        self.toolBar.addAction(self.newAction)
        self.toolBar.addAction(self.openAction)
        self.toolBar.addAction(self.saveAction)
        self.toolBar.addAction(self.exitAction)

        self.toolBar.addSeparator()

        # add lexer actions
        self.toolBar.addAction(self.gCodeLexerAction)
        self.toolBar.addAction(self.pythonLexerAction)

        self.toolBar.addSeparator()
        self.label = QLabel('''<html><head/><body><p><span style=" font-size:20pt;
                         font-weight:600;">Edit Mode</span></p></body></html>''')
        self.toolBar.addWidget(self.label)

        # create a frame for buttons
        box = QHBoxLayout()
        box.addWidget(self.toolBar)

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
        self.searchText.setStatusTip('Text to search for')
        self.replaceText = QLineEdit(self)
        self.replaceText.setStatusTip('Replace search text with this text')
        toolBar = QToolBar()
        # Create new action
        undoAction = QAction(QIcon.fromTheme('edit-undo'), 'Undo', self)
        undoAction.setStatusTip('Undo')
        undoAction.triggered.connect(self.undoCall)
        toolBar.addAction(undoAction)

        # create redo action
        redoAction = QAction(QIcon.fromTheme('edit-redo'), 'Redo', self)
        redoAction.setStatusTip('Redo')
        redoAction.triggered.connect(self.redoCall)
        toolBar.addAction(redoAction)

        toolBar.addSeparator()

        # create replace action
        replaceAction = QAction(QIcon.fromTheme('edit-find-replace'), 'Replace', self)
        replaceAction.setStatusTip('Replace text')
        replaceAction.triggered.connect(self.replaceCall)
        toolBar.addAction(replaceAction)

        # create find action
        findAction = QAction(QIcon.fromTheme('edit-find'), 'Find', self)
        findAction.setStatusTip('Find next occurrence of text')
        findAction.triggered.connect(self.findCall)
        toolBar.addAction(findAction)

        # create next action
        previousAction = QAction(QIcon.fromTheme('go-previous'), 'Find Previous', self)
        previousAction.setStatusTip('Find previous occurrence of text')
        previousAction.triggered.connect(self.previousCall)
        toolBar.addAction(previousAction)

        toolBar.addSeparator()

        # create case action
        caseAction = QAction(QIcon.fromTheme('edit-case'), 'Aa', self)
        caseAction.setStatusTip('Toggle between any case and match case')
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

    def _hal_init(self):
        # name the top and bottom frames so it's easier to style
        self.bottomMenu.setObjectName('%sBottomButtonFrame'% self.objectName())
        self.topMenu.setObjectName('%sTopButtonFrame'% self.objectName())
        self.editor.setObjectName('{}_display'.format( self.objectName()))

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
    if 1:
        w.pythonLexerAction.setVisible(False)
        w.gCodeLexerAction.setVisible(False)
    if 1:
        w.openAction.setVisible(False)
        w.newAction.setVisible(False)
    if 0:
        w.saveAction.setVisible(False)
        w.exitAction.setVisible(False)
    if 1:
        w.label.setText('<b>Edit mode title label</b>')
    w.show()
    sys.exit( app.exec_() )


