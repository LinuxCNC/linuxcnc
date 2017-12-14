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

import sys, os
from PyQt5.QtCore import pyqtProperty
from PyQt5.QtGui import QFont, QFontMetrics, QColor

# Set up logging
from qtvcp import logger
log = logger.getLogger(__name__)

try:
    from PyQt5.Qsci import QsciScintilla, QsciLexerCustom
except ImportError as e:
    log.critical("Can't import QsciScintilla - is package python-pyqt5.qsci installed?", exc_info=e)
    sys.exit(1)
from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.qt_glib import GStat
from qtvcp.qt_istat import IStat
GSTAT = GStat()
INI = IStat()

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
        for key,value in self._styles.iteritems():
            setattr(self, value, key)
        font = QFont()
        font.setFamily('Courier')
        font.setFixedPitch(True)
        font.setPointSize(12)
        font.setBold(True)
        self.setFont(font,2)

    # Paper sets the background color of each style of text
    def setPaperBackground(self,color,style=None):
        if style is None:
            for i in range(0,5):
                self.setPaper(color,i)
        else:
            self.setPaper(color,style)

    def description(self, style):
        return self._styles.get(style, '')

    def defaultColor(self, style):
        if style == self.Default:
            return QColor('#000000') # black
        elif style == self.Comment:
            return QColor('#000000') # black
        elif style == self.Key:
            return QColor('#0000CC') # blue
        elif style == self.Assignment:
            return QColor('#CC0000') # red
        elif style == self.Value:
            return QColor('#00CC00') # green
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
                source = unicode(editor.text()
                                ).encode('utf-8')[start:end]
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
                    if (msg and char.lower() in ('m','s','g',',','d','e','b','u')):
                        set_style(1, self.Assignment)
                        if char == ',': msg = False
                    else:
                        set_style(1, self.Comment)
                    continue
                elif char in ('%','<','>','#','='):
                    state = self.Assignment
                elif char in ('[',']'):
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
        # linuxcnc defaults
        self.idle_line_reset = False
        # don't allow editing by default
        self.setReadOnly(True)
        # Set the default font
        font = QFont()
        font.setFamily('Courier')
        font.setFixedPitch(True)
        font.setPointSize(12)
        self.setFont(font)
        self.setMarginsFont(font)

        # Margin 0 is used for line numbers
        fontmetrics = QFontMetrics(font)
        self.setMarginsFont(font)
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
        self.lexer = GcodeLexer(self)
        self.lexer.setDefaultFont(font)
        self.setLexer(self.lexer)
        # Set style for Python comments (style number 1) to a fixed-width
        # courier.
        #self.SendScintilla(QsciScintilla.SCI_STYLESETFONT, 1, 'Courier')

        # Don't want to see the horizontal scrollbar at all
        # Use raw message to Scintilla here (all messages are documented
        # here: http://www.scintilla.org/ScintillaDoc.html)
        #self.SendScintilla(QsciScintilla.SCI_SETHSCROLLBAR, 0)

        # default gray background
        self.set_background_color('#C0C0C0')

        # not too small
        self.setMinimumSize(200, 100)

    # must set lexer paper background color _and_ editor background color it seems
    def set_background_color(self,color):
        self.SendScintilla(QsciScintilla.SCI_STYLESETBACK, QsciScintilla.STYLE_DEFAULT, QColor(color))
        self.lexer.setPaperBackground(QColor(color))

    def on_margin_clicked(self, nmargin, nline, modifiers):
        # Toggle marker for the line the margin was clicked on
        if self.markersAtLine(nline) != 0:
            self.markerDelete(nline, self.ARROW_MARKER_NUM)
        else:
            self.markerAdd(nline, self.ARROW_MARKER_NUM)

##########################################################
# Gcode widget
##########################################################
class GcodeEditor(EditorBase, _HalWidgetBase):
    ARROW_MARKER_NUM = 8

    def __init__(self, parent=None):
        super(GcodeEditor, self).__init__(parent)
        self._last_filename = None
        self.auto_show_mdi = True
        #self.setEolVisibility(True)

    def _hal_init(self):
        self.cursorPositionChanged.connect(self.line_changed)
        if self.auto_show_mdi:
            GSTAT.connect('mode-mdi', self.load_mdi)
            GSTAT.connect('reload-mdi-history', self.load_mdi)
            GSTAT.connect('mode-auto', self.reload_last)
            GSTAT.connect('move-text-lineup', self.select_lineup)
            GSTAT.connect('move-text-linedown', self.select_linedown)
        GSTAT.connect('file-loaded', self.load_program)
        GSTAT.connect('line-changed', self.highlight_line)
        if self.idle_line_reset:
            GSTAT.connect('interp_idle', lambda w: self.set_line_number(None, 0))

    def load_program(self, w, filename = None):
        if filename is None:
            filename =  self._last_filename
        else:
            self._last_filename = filename
        self.load_text(filename)
        #self.zoomTo(6)
        self.setCursorPosition(0,0)

    # when switching from MDI to AUTO we need to reload the
    # last (linuxcnc loaded) program.
    def reload_last(self,w):
        self.load_text(self._last_filename)
        self.setCursorPosition(0,0)

    # With the auto_show__mdi option, MDI history is shown
    def load_mdi(self,w):
        self.load_text(INI.MDI_HISTORY_PATH)
        self._last_filename = INI.MDI_HISTORY_PATH
        #print 'font point size', self.font().pointSize()
        #self.zoomTo(10)
        #print 'font point size', self.font().pointSize()
        self.setCursorPosition(self.lines(),0)

    def load_text(self, filename):
        try:
            fp = os.path.expanduser(filename)
        except:
            log.error('File path is not valid: {}'.format(filename))
            self.setText('')
            return

        self.setText(open(fp).read())
        self.last_line = None
        self.ensureCursorVisible()
        self.SendScintilla(QsciScintilla.SCI_VERTICALCENTRECARET)

    def highlight_line(self, w, line):
        if GSTAT.is_auto_running():
            if not GSTAT.old['file']  == self._last_filename:
                log.debug('should reload the display')
                self.load_text(GSTAT.old['file'])
                self._last_filename = GSTAT.old['file']
        if 1==1:
            self.markerAdd(line, self.ARROW_MARKER_NUM)
            if self.last_line:
                self.markerDelete(self.last_line, self.ARROW_MARKER_NUM)
            self.setCursorPosition(line,0)
            self.ensureCursorVisible()
            self.SendScintilla(QsciScintilla.SCI_VERTICALCENTRECARET)
            self.last_line = line

    def set_line_number(self, w, line):
        pass

    def line_changed(self, line, index):
        log.debug('Line changed: {}'.format(GSTAT.is_auto_mode()))
        self.line_text = str(self.text(line)).strip()
        self.line = line
        if GSTAT.is_auto_running() == False:
            GSTAT.emit('mdi-line-selected',self.line_text, self._last_filename)

    def select_lineup(self,w):
        line,col = self.getCursorPosition()
        log.debug(line)
        self.setCursorPosition(line-1,0)
        self.highlight_line(None,line-1)

    def select_linedown(self,w):
        line, col = self.getCursorPosition()
        log.debug(line)
        self.setCursorPosition(line+1,0)
        self.highlight_line(None,line+1)

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
# For testing
#############################################
if __name__ == "__main__":
    from PyQt4.QtGui import QApplication
    app = QApplication(sys.argv)
    editor = GcodeEditor()
    editor.show()

    editor.setText(open(sys.argv[0]).read())
    app.exec_()

