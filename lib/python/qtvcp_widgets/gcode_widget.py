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
from PyQt4.QtCore import SIGNAL
from PyQt4.QtGui import QFont, QFontMetrics, QColor
try:
    from PyQt4.Qsci import QsciScintilla, QsciLexerPython
except:
    print '**** QTVCP ERROR: Gcode widget can not import QsciScintilla - is package python-scintilla2 installed?'
    sys.exit(1)
from qtvcp_widgets.simple_widgets import _HalWidgetBase
from qtvcp.qt_glib import GStat
GSTAT = GStat()

class GcodeEditor(QsciScintilla, _HalWidgetBase):
    ARROW_MARKER_NUM = 8

    def __init__(self, parent=None):
        super(GcodeEditor, self).__init__(parent)
        # linuxcnc defaults
        self.idle_line_reset = False
        self.setReadOnly(True)
        #self.setEanble(False)
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
        self.connect(self,
            SIGNAL('marginClicked(int, int, Qt::KeyboardModifiers)'),
            self.on_margin_clicked)
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

        # Set Python lexer
        # Set style for Python comments (style number 1) to a fixed-width
        # courier.
        #
        lexer = QsciLexerPython()
        lexer.setDefaultFont(font)
        self.setLexer(lexer)
        self.SendScintilla(QsciScintilla.SCI_STYLESETFONT, 1, 'Courier')

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
        self.last_line = line

    def set_line_number(self, w, line):
        pass

if __name__ == "__main__":
    app = QApplication(sys.argv)
    editor = SimplePythonEditor()
    editor.show()
    editor.setText(open(sys.argv[0]).read())
    app.exec_()

