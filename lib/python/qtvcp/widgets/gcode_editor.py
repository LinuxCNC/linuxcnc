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
import re

from PyQt5.QtCore import pyqtProperty, pyqtSignal, QSize
from PyQt5.QtGui import QFont, QFontMetrics, QColor, QIcon
from PyQt5.QtWidgets import QWidget, QAction,\
        QVBoxLayout, QToolBar, QLineEdit, QHBoxLayout, QMessageBox, \
        QFrame, QLabel

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


class GcodeLexer(QsciLexerCustom):
    """ QSciLexer for parsing and highlighting G-code """

    def __init__(self, parent):
        super(GcodeLexer, self).__init__(parent)

        # Style/Token Types (Used for Styling & Regex)
        self._styles = {
            0: 'Default',
            1: 'Comment',
            2: 'Gcode',
            3: 'Mcode',
            4: 'Axis',
            5: 'Other',
            6: 'AxisValue',
            7: 'OtherValue',
        }

        for key, value in self._styles.items():
            setattr(self, value, key)

    def language(self):
        return "G-code"

    def description(self, style):
        return self._styles.get(style, "")

    def styleText(self, start, end):
        editor = self.editor()
        if editor is None:
            return

        self.startStyling(start)

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
                source = source.decode("utf-8", "ignore")
            else:
                source = str(editor.text())[start:end]
        if not source:
            return

        re_tokens = {
            1: r"(?:[N]\d+|\(.*?\)|;.*)",                                       # LineNo and Comment
            2: r"[G]\d{1,2}\.\d|[G]\d{1,2}",                                    # Gcode
            3: r"[M]\d{1,3}",                                                   # Mcode
            4: r"[XYZABCUVW]{1}(?:[+-]?[\d\.]+|\#\<.*\>|\[.*\]|\#\d+)",         # Axis
            5: r"[EFHIJKDQLRPST$]{1}(?:[+-]?[\d\.]+|\#\<.*\>|\[.*\]|\#\d+)",    # Other (feed,rpm,radius,etc)
            0: r"\s+|\w+|\W",                                                   # Default (fallback)
        }

        re_comment_cmd = r"(?:\(\s*(?:print,|debug,|msg,|logopen,|logappend,|logclose|log,|pyrun,|pyreload|abort,|probeopen|probeclose)|^\s*\;py,)"

        re_string = "|".join(re_tokens.values())
        p = re.compile(re_string, re.IGNORECASE)

        for line in source.splitlines(True):
            token_list = [(token, len(bytearray(token, "utf-8"))) for token in p.findall(line)]
            num_comment_cmds = len(re.findall(re_comment_cmd, line, re.IGNORECASE))

            for token in token_list:
                if re.match(re_tokens[self.Comment], token[0], re.IGNORECASE):
                    m = re.search(re_comment_cmd, token[0], re.IGNORECASE)
                    if m:
                        num_comment_cmds -= 1

                    if m and num_comment_cmds == 0:
                        # Only highlight last comment_cmd on line
                        self.setStyling(1, self.Comment)
                        self.setStyling(m.span()[1] - 1, self.Other)
                        self.setStyling(len(token[0]) - m.span()[1], self.Comment)
                    else:
                        self.setStyling(token[1], self.Comment)
                elif re.match(re_tokens[self.Gcode], token[0], re.IGNORECASE):
                    self.setStyling(token[1], self.Gcode)
                elif re.match(re_tokens[self.Mcode], token[0], re.IGNORECASE):
                    self.setStyling(token[1], self.Mcode)
                elif re.match(re_tokens[self.Axis], token[0], re.IGNORECASE):
                    self.setStyling(1, self.Axis)
                    self.setStyling(token[1] - 1, self.AxisValue)
                elif re.match(re_tokens[self.Other], token[0], re.IGNORECASE):
                    self.setStyling(1, self.Other)
                    self.setStyling(token[1] - 1, self.OtherValue)
                else:
                    self.setStyling(token[1], self.Default)


##########################################################
# Base editor class
##########################################################
class EditorBase(QsciScintilla):
    CURRENT_MARKER_NUM = 0
    USER_MARKER_NUM = 1

    # Default Styles
    # get/set function for font and colors styles will return
    # styleFont[0] and _styleColor[0] as defaults when they are not
    # set in the dict manually, or by CSS properties.
    _styleFont = {
        0: QFont("Courier", 11),                    # Default Font
#        2: QFont("Courier", 20, weight=QFont.Bold), # Gcode
#        5: QFont("Courier", 11, weight=QFont.Bold), # Other
#        "Margins": QFont("Courier", 9),            # Margins
    }

    _styleColor = {
        0: QColor("#000000"),               # Color0 (Default)
        1: QColor("#434d3f"),               # Color1 (Lexer Comment)
        2: QColor("#ba220b"),               # Color2 (Lexer Gcode)
        3: QColor("#f56b1b"),               # Color3 (Lexer Mcode)
        4: QColor("#1883c9"),               # Color4 (Lexer Axis)
        5: QColor("#dd30f0"),               # Color5 (Lexer Other)
        6: QColor("#0e5482"),               # Color6 (Lexer Axis Value)
        7: QColor("#a420b3"),               # Color7 (Lexer Other Value)
        "Margins":  QColor("#666769"),      # Margins
    }

    _styleBackgroundColor = QColor("#c0c0c0")
    _styleMarginsBackgroundColor = QColor("#cccccc")
    _styleMarkerBackgroundColor = QColor("#a5a526")
    _styleSelectionBackgroundColor = QColor("#001111")
    _styleSelectionForegroundColor = QColor("#ffffff")
    _styleSyntaxHighlightEnabled = True

    def __init__(self, parent=None):
        super(EditorBase, self).__init__(parent)

        self.lexer = None
        self.lexer_num_styles = 0   # updated when lexer is loaded

        self._lastUserLine = 0
        self.setReadOnly(True)      # don't allow editing by default

        self.set_lexer("g-code")

        self._marginWidth = '00000'

        # Margin 0 is used for line numbers
        self.setMarginWidth(0, self._marginWidth)
        self.linesChanged.connect(self.on_lines_changed)
        self.setMarginLineNumbers(0, True)

        # Clickable margin for showing markers
        self.marginClicked.connect(self.on_margin_clicked)
        self.setMarginMarkerMask(0, 0b1111)
        self.setMarginSensitivity(0, True)
        # setting _marker_ margin width
        self.setMarginWidth(1, 0)

        # Gcode highlight current line
        self.currentHandle = self.markerDefine(QsciScintilla.Background,
                          self.CURRENT_MARKER_NUM)
        self.setColorMarkerBackground(self.getColorMarkerBackground())

        # User Highlight line (when clicking margin)
        self.userHandle = self.markerDefine(QsciScintilla.Background,
                          self.USER_MARKER_NUM)
        self.setMarkerBackgroundColor(QColor("#ffc0c0"),
                                      self.USER_MARKER_NUM)

        # Brace matching: enable for a brace immediately before or after
        # the current position
        self.setBraceMatching(QsciScintilla.SloppyBraceMatch)

        # Current line visible with special background color
        self.setCaretLineVisible(False)
        self.SendScintilla(QsciScintilla.SCI_GETCARETLINEVISIBLEALWAYS, True)
        self.setCaretLineBackgroundColor(QColor("#ffe4e4"))
        self.ensureLineVisible(True)

        # Don't want to see the horizontal scrollbar at all
        # Use raw message to Scintilla here (all messages are documented
        # here: http://www.scintilla.org/ScintillaDoc.html)
        #self.SendScintilla(QsciScintilla.SCI_SETHSCROLLBAR, 0)
        self.SendScintilla(QsciScintilla.SCI_SETSCROLLWIDTH, 700)
        self.SendScintilla(QsciScintilla.SCI_SETSCROLLWIDTHTRACKING)

        # not too small
        self.setMinimumSize(200, 100)
        self.filepath = None

    def set_lexer(self, lexer_type=None):
        self.lexer = None
        self.lexer_num_styles = 0

        if lexer_type is None or not self._styleSyntaxHighlightEnabled:
            self.lexer = None
        elif lexer_type.lower() == "g-code":
            self.lexer = GcodeLexer(self)
        elif lexer_type.lower() == "python":
            self.lexer = QsciLexerPython(self)

        # Get Number of lexer styles
        if self.lexer is not None:
            while self.lexer.description(self.lexer_num_styles) != "":
                self.lexer_num_styles += 1

        self.setLexer(self.lexer)
        self.refresh_styles()
        #print("Loaded Lexer {} with {} Styles".format(lexer_type, self.lexer_num_styles))

    def refresh_styles(self):
        self.setDefaultFont(self.getDefaultFont())
        self.set_font_colors()
        self.setColorBackground(self.getColorBackground())
        self.setColorMarginsBackground(self.getColorMarginsBackground())
        self.setSelectionBackgroundColor(self.getColorSelectionBackground())
        self.setSelectionForegroundColor(self.getColorSelectionForeground())

    def set_font_colors(self):
        self.setColor(self.getColor0())
        self.setColorMarginsForeground(self.getColorMarginsForeground())

        if self.lexer is not None:
            for i in range(0, self.lexer_num_styles):
                self.lexer.setColor(self._styleColor.get(i, self._styleColor[0]), i)

    def set_margin_width(self):
        self.setMarginWidth(0, self._marginWidth)

    def set_margin_metric(self, width):
        fontmetrics = QFontMetrics(self.getFontMargins())
        self.setMarginWidth(0, fontmetrics.width("0" * width) + 6)

    # reset margin width when number od lines change
    def on_lines_changed(self):
        if len(str(self.lines())) < 3:
            self._marginWidth = '0000'
        else:
            self._marginWidth = str(self.lines())+'0'
        self.setMarginWidth(0, self._marginWidth)

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

    def mouseDoubleClickEvent(self, event):
        pass

    def new_text(self):
        self.setText('')

    def load_text(self, filepath):
        self.filepath = filepath
        if filepath is None:
            return
        try:
            fp = os.path.expanduser(filepath)
            with open(fp) as f:
                self.setText(f.read())
        except OSError as e:
            LOG.error("load_text(): {}".format(e))
            self.setText('')
            return
        except Exception as e:
            LOG.error("load_text(): {}".format(e))
            self.setText('')
            return
        self.ensureCursorVisible()
        self.SendScintilla(QsciScintilla.SCI_VERTICALCENTRECARET)
        self.setModified(False)

    def save_text(self):
        try:
            with open(self.filepath + 'text', "w") as f:
                f.write(self.text())
        except OSError as e:
            LOG.error("save_text(): {}".format(e))

    def replace_text(self, text):
        self.replace(text)

    def search(self, text, re=False, case=False, word=False, wrap=False, fwd=True):
        self.findFirst(text, re, case, word, wrap, fwd)

    def search_Next(self):
        self.SendScintilla(QsciScintilla.SCI_SEARCHANCHOR)
        self.findNext()

    # Style get/set functions

    # Default Color
    def getColor0(self):
        return self._styleColor[0]
    def setColor0(self, value):
        self._styleColor[0] = value
        self.setColor(value)
        if self.lexer is not None:
            self.lexer.setColor(value, 0)
    styleColor0 = pyqtProperty(QColor, getColor0, setColor0)

    # Lexer Colors
    def getColor1(self):
        return self._styleColor.get(1, self._styleColor[0])
    def setColor1(self, value):
        self._styleColor[1] = value
        if self.lexer is not None:
            self.lexer.setColor(value, 1)
    styleColor1 = pyqtProperty(QColor, getColor1, setColor1)

    def getColor2(self):
        return self._styleColor.get(2, self._styleColor[0])
    def setColor2(self, value):
        self._styleColor[2] = value
        if self.lexer is not None:
            self.lexer.setColor(value, 2)
    styleColor2 = pyqtProperty(QColor, getColor2, setColor2)

    def getColor3(self):
        return self._styleColor.get(3, self._styleColor[0])
    def setColor3(self, value):
        self._styleColor[3] = value
        if self.lexer is not None:
            self.lexer.setColor(value, 3)
    styleColor3 = pyqtProperty(QColor, getColor3, setColor3)

    def getColor4(self):
        return self._styleColor.get(4, self._styleColor[0])
    def setColor4(self, value):
        self._styleColor[4] = value
        if self.lexer is not None:
            self.lexer.setColor(value, 4)
    styleColor4 = pyqtProperty(QColor, getColor4, setColor4)

    def getColor5(self):
        return self._styleColor.get(5, self._styleColor[0])
    def setColor5(self, value):
        self._styleColor[5] = value
        if self.lexer is not None:
            self.lexer.setColor(value, 5)
    styleColor5 = pyqtProperty(QColor, getColor5, setColor5)

    def getColor6(self):
        return self._styleColor.get(6, self._styleColor[0])
    def setColor6(self, value):
        self._styleColor[6] = value
        if self.lexer is not None:
            self.lexer.setColor(value, 6)
    styleColor6 = pyqtProperty(QColor, getColor6, setColor6)

    def getColor7(self):
        return self._styleColor.get(7, self._styleColor[0])
    def setColor7(self, value):
        self._styleColor[7] = value
        if self.lexer is not None:
            self.lexer.setColor(value, 7)
    styleColor7 = pyqtProperty(QColor, getColor7, setColor7)

    # Margins Text Color
    def getColorMarginsForeground(self):
        return self._styleColor.get("Margins", self._styleColor[0])
    def setColorMarginsForeground(self, value):
        super(EditorBase, self).setMarginsForegroundColor(value)
        self._styleColor["Margins"] = value
    styleColorMarginText = pyqtProperty(QColor, getColorMarginsForeground, setColorMarginsForeground)

    # Backgrounds
    def getColorBackground(self):
        return self._styleBackgroundColor
    def setColorBackground(self, color):
        self._styleBackgroundColor = color
        #self.SendScintilla(QsciScintilla.SCI_STYLESETBACK, QsciScintilla.STYLE_DEFAULT, color)
        self.setPaper(color)
        if self.lexer is not None:
            self.lexer.setDefaultPaper(color)
            for i in range(0, self.lexer_num_styles):
                self.lexer.setPaper(color, i)
    styleColorBackground = pyqtProperty(QColor, getColorBackground, setColorBackground)

    # Margins Background
    def setColorMarginsBackground(self, color):
        super(EditorBase, self).setMarginsBackgroundColor(color)
        self._styleMarginsBackgroundColor = color
    def getColorMarginsBackground(self):
        return self._styleMarginsBackgroundColor
    styleColorMarginBackground = pyqtProperty(QColor, getColorMarginsBackground, setColorMarginsBackground)

    # Selection Highlight Background & Foreground
    def getColorSelectionBackground(self):
        return self._styleSelectionBackgroundColor
    def setColorSelectionBackground(self, value):
        self._styleSelectionBackgroundColor = value
        self.setSelectionBackgroundColor(value)
    styleColorSelectionBackground = pyqtProperty(QColor, getColorSelectionBackground, setColorSelectionBackground)

    def getColorSelectionForeground(self):
        return self._styleSelectionForegroundColor
    def setColorSelectionForeground(self, value):
        self._styleSelectionForegroundColor = value
        self.setSelectionForegroundColor(value)
    styleColorSelectionText = pyqtProperty(QColor, getColorSelectionForeground, setColorSelectionForeground)

    # Current Line Marker Background
    def getColorMarkerBackground(self):
        return self._styleMarkerBackgroundColor
    def setColorMarkerBackground(self, value):
        self._styleMarkerBackgroundColor = value
        self.setMarkerBackgroundColor(value, self.CURRENT_MARKER_NUM)
    styleColorMarkerBackground = pyqtProperty(QColor, getColorMarkerBackground, setColorMarkerBackground)

    # Fonts
    def setDefaultFont(self, value):
        self._styleFont[0] = value
        self.setFont(value)
        self.setFontMargins(self.getFontMargins())

        if self.lexer is not None:
            self.lexer.setFont(value)
            for i in range(0, self.lexer_num_styles):
                self.lexer.setFont(self._styleFont.get(i, self._styleFont[0]), i)
    def getDefaultFont(self):
        return self._styleFont[0]
    styleFont = pyqtProperty(QFont, getDefaultFont, setDefaultFont)

    def getFont0(self):
        return self._styleFont[0]
    def setFont0(self, value):
        self._styleFont[0] = value
        self.setFont(value)
        if self.lexer is not None:
            self.lexer.setFont(value, 0)
    styleFont0 = pyqtProperty(QFont, getFont0, setFont0)

    def getFont1(self):
        return self._styleFont.get(1, self._styleFont[0])
    def setFont1(self, value):
        self._styleFont[1] = value
        if self.lexer is not None:
            self.lexer.setFont(value, 1)
    styleFont1 = pyqtProperty(QFont, getFont1, setFont1)

    def getFont2(self):
        return self._styleFont.get(2, self._styleFont[0])
    def setFont2(self, value):
        self._styleFont[2] = value
        if self.lexer is not None:
            self.lexer.setFont(value, 2)
    styleFont2 = pyqtProperty(QFont, getFont2, setFont2)

    def getFont3(self):
        return self._styleFont.get(3, self._styleFont[0])
    def setFont3(self, value):
        self._styleFont[3] = value
        if self.lexer is not None:
            self.lexer.setFont(value, 3)
    styleFont3 = pyqtProperty(QFont, getFont3, setFont3)

    def getFont4(self):
        return self._styleFont.get(4, self._styleFont[0])
    def setFont4(self, value):
        self._styleFont[4] = value
        if self.lexer is not None:
            self.lexer.setFont(value, 4)
    styleFont4 = pyqtProperty(QFont, getFont4, setFont4)

    def getFont5(self):
        return self._styleFont.get(5, self._styleFont[0])
    def setFont5(self, value):
        self._styleFont[5] = value
        if self.lexer is not None:
            self.lexer.setFont(value, 5)
    styleFont5 = pyqtProperty(QFont, getFont5, setFont5)

    def getFont6(self):
        return self._styleFont.get(6, self._styleFont[0])
    def setFont6(self, value):
        self._styleFont[6] = value
        if self.lexer is not None:
            self.lexer.setFont(value, 6)
    styleFont6 = pyqtProperty(QFont, getFont6, setFont6)

    def getFont7(self):
        return self._styleFont.get(7, self._styleFont[0])
    def setFont7(self, value):
        self._styleFont[7] = value
        if self.lexer is not None:
            self.lexer.setFont(value, 7)
    styleFont7 = pyqtProperty(QFont, getFont7, setFont7)

    def getFontMargins(self):
        return self._styleFont.get("Margins", self._styleFont[0])
    def setFontMargins(self, value):
        self._styleFont["Margins"] = value
        self.setMarginsFont(value)
    styleFontMargin = pyqtProperty(QFont, getFontMargins, setFontMargins)

    # Syntax Highlighting Bool
    def getSyntaxHighlightEnabled(self):
        return self._styleSyntaxHighlightEnabled
    def setSyntaxHighlightEnabled(self, value):
        if value is not self._styleSyntaxHighlightEnabled:
            self._styleSyntaxHighlightEnabled = value
            if not value:
                self.set_lexer(None)
            else:
                self.set_lexer("g-code")
    styleSyntaxHighlightEnabled = pyqtProperty(bool, getSyntaxHighlightEnabled, setSyntaxHighlightEnabled)


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
        # keep track of vertical scroll setting in auto mode
        self._last_auto_scroll = 0

    def _hal_init(self):
        self.cursorPositionChanged.connect(self.line_changed)
        if self.auto_show_mdi:
            STATUS.connect('mode-mdi', self.load_mdi)
            STATUS.connect('mdi-history-changed', self.load_mdi)
            STATUS.connect('mode-auto', self.reload_last)
            STATUS.connect('move-text-lineup', self.select_lineup)
            STATUS.connect('move-text-linedown', self.select_linedown)
            STATUS.connect('mode-manual', self.load_manual)
        if self.auto_show_manual:
            STATUS.connect('mode-manual', self.load_manual)
            STATUS.connect('machine-log-changed', self.load_manual)
        if self.auto_show_preference:
            STATUS.connect('show-preference', self.load_preference)
        STATUS.connect('file-loaded', self.load_program)
        STATUS.connect('reload-display', self.load_program)
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
        self._lastUserLine = 0

    # when switching from MDI to AUTO we need to reload the
    # last (linuxcnc loaded) program.
    def reload_last(self, w):
        self.load_text(STATUS.old['file'])
        self.setCursorPosition(0, 0)
        # keep track of vertical scroll bar setting
        self.verticalScrollBar().setValue(self._last_auto_scroll)
        #  and margin marker if it is showing
        if self._lastUserLine >0:
            self.markerAdd(self._lastUserLine, self.USER_MARKER_NUM)

    # With the auto_show__mdi option, MDI history is shown
    def load_mdi(self, w):
        # record scroll position in auto mode's gcode
        if STATUS.get_previous_mode() == STATUS.AUTO: 
            self._last_auto_scroll = self.verticalScrollBar().value()

        self.load_text(INFO.MDI_HISTORY_PATH)
        self._last_filename = INFO.MDI_HISTORY_PATH
        self.setCursorPosition(self.lines(), 0)

    # With the auto_show__mdi option, MDI history is shown
    def load_manual(self, w):
        # record scroll position in auto mode's gcode
        if STATUS.get_previous_mode() == STATUS.AUTO: 
            self._last_auto_scroll = self.verticalScrollBar().value()

        if self.auto_show_manual and STATUS.is_man_mode():
            self.load_text(INFO.MACHINE_LOG_HISTORY_PATH)
            self.setCursorPosition(self.lines(), 0)

    def load_preference(self, w):
        self.load_text(self.PATHS_.PREFS_FILENAME)
        self.setCursorPosition(self.lines(), 0)

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
        LOG.verbose('editor: highlight line {}'.format(line))
        if STATUS.is_auto_running():
            if not STATUS.old['file'] == self._last_filename:
                LOG.debug('should reload the display')
                self.load_text(STATUS.old['file'])
                self._last_filename = STATUS.old['file']
            self.emit_percent(round(line*100/self.lines()))
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
        LOG.verbose('Line changed: {}'.format(line))
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

    # overridden functions               #
    #####################################
    def zoomIn(self):
        super().zoomIn()
        self.set_margin_width()
    def zoomOut(self):
        super().zoomOut()
        self.set_margin_width()
    #####################################

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
        self.topBox = QHBoxLayout()
        self.topBox.addWidget(self.toolBar)

        self.topMenu = QFrame()
        self.topMenu.setLayout(self.topBox)

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

        self.bottomBox = QHBoxLayout()
        self.bottomBox.addWidget(toolBar)
        self.bottomBox.addWidget(self.searchText)
        self.bottomBox.addWidget(self.replaceText)
        self.bottomBox.addStretch(1)
        self.bottomMenu.setLayout(self.bottomBox)

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
        self.editor.set_lexer("g-code")

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
        self.editor.set_lexer("python")

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
            'TITLE':'Save Editor', 'FILENAME':self.editor._last_filename}
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
        self.percentDone.emit(int(percent))

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

    def set_margin_metric(self,width):
        self.editor.set_margin_metric(width)

    def set_font(self, font):
        self.editor.setDefaultFont(font)

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

    sample_text = r"""( ----- Simple G-Code -----)
G17 G20 G40 G49 G54 G80 G90 G94
M7

G0 Z10
F400
G1 Z1


G3 X50 Y50 Z1 I-7.5 J0
G0 Z30


G5.2 X3.53   Y-1.50   P2
     X5.33   Y-11.01  P1
     X3.52   Y-24.00  P1
     X0.0    Y-29.56  P1
G5.3

G0 Z2
G1 Z-5 F100
F600

( ----- Line Numbers ----- )
N1
N20
N300
N030 (insert numbers in place of .)
N000   G1 X20
N98765 G0 Z30"""

    app = QApplication(sys.argv)
    w = GcodeEditor()
    w.editMode()

    if len(sys.argv) > 1:
        w.editor.load_text(sys.argv[1])
    else:
        w.editor.setText(sample_text)

    if 0:
        w.toolBar.hide()
    if 1:
        w.pythonLexerAction.setVisible(True)
        w.gCodeLexerAction.setVisible(True)
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
