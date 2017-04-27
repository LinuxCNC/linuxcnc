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

import sys
from PyQt5.QtGui import QFont, QFontMetrics, QColor
try:
    from PyQt5.Qsci import QsciScintilla, QsciLexerCustom
except:
    print '**** QTVCP ERROR: Gcode widget can not import QsciScintilla - is package python-qscintilla2 installed?'
    sys.exit(1)
from qtvcp_widgets.simple_widgets import _HalWidgetBase
from qtvcp.qt_glib import GStat
GSTAT = GStat()

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
        #self.setPaper(QColor('#ffe4e4'),3)
        #self.setPaper(QColor(3,234,255),2)
        font = QFont()
        font.setFamily('Courier')
        font.setFixedPitch(True)
        font.setPointSize(12)
        font.setBold(True)
        self.setFont(font,2)

    def description(self, style):
        return self._styles.get(style, '')

    def defaultColor(self, style):
        if style == self.Default:
            return QColor('#000000') # black
        elif style == self.Comment:
            return QColor('#C0C0C0') # gray
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

class GcodeEditor(QsciScintilla, _HalWidgetBase):
    ARROW_MARKER_NUM = 8

    def __init__(self, parent=None):
        super(GcodeEditor, self).__init__(parent)
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
        self.setMarginSensitivity(1, True)
        self.marginClicked.connect(self.on_margin_clicked)
        self.markerDefine(QsciScintilla.RightArrow,
            self.ARROW_MARKER_NUM)
        self.setMarkerBackgroundColor(QColor("#ee1111"),
            self.ARROW_MARKER_NUM)

        # Brace matching: enable for a brace immediately before or after
        # the current position
        #
        self.setBraceMatching(QsciScintilla.SloppyBraceMatch)

        # Current line visible with special background color
        self.setCaretLineVisible(True)
        self.setCaretLineBackgroundColor(QColor("#ffe4e4"))

        # Set custom gcode lexer
        lexer = GcodeLexer(self)
        lexer.setDefaultFont(font)
        self.setLexer(lexer)
        # Set style for Python comments (style number 1) to a fixed-width
        # courier.
        #self.SendScintilla(QsciScintilla.SCI_STYLESETFONT, 1, 'Courier')

        # Don't want to see the horizontal scrollbar at all
        # Use raw message to Scintilla here (all messages are documented
        # here: http://www.scintilla.org/ScintillaDoc.html)
        #self.SendScintilla(QsciScintilla.SCI_SETHSCROLLBAR, 0)

        # not too small
        self.setMinimumSize(200, 200)

    def on_margin_clicked(self, nmargin, nline, modifiers):
        # Toggle marker for the line the margin was clicked on
        if self.markersAtLine(nline) != 0:
            self.markerDelete(nline, self.ARROW_MARKER_NUM)
        else:
            self.markerAdd(nline, self.ARROW_MARKER_NUM)

    def _hal_init(self):
        GSTAT.connect('file-loaded', self.load_file)
        GSTAT.connect('line-changed', self.highlight_line)
        if self.idle_line_reset:
            GSTAT.connect('interp_idle', lambda w: self.set_line_number(None, 0))

    def load_file(self, w, filename):
        self.setText(open(filename).read())
        self.last_line = None
        self.setCursorPosition(0,0)
        self.ensureCursorVisible()

    def highlight_line(self, w, line):
        self.markerAdd(line, self.ARROW_MARKER_NUM)
        if self.last_line:
            self.markerDelete(self.last_line, self.ARROW_MARKER_NUM)
        self.setCursorPosition(line,0)
        self.ensureCursorVisible()
        self.SendScintilla(QsciScintilla.SCI_VERTICALCENTRECARET)
        self.last_line = line

    def set_line_number(self, w, line):
        pass

if __name__ == "__main__":
    from PyQt5.QtWidgets import QApplication
    app = QApplication(sys.argv)
    editor = GcodeEditor()
    editor.show()

    editor.setText(open(sys.argv[0]).read())
    app.exec_()

