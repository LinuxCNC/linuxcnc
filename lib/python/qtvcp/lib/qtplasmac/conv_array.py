'''
conv_array.py

Copyright (C) 2020, 2021  Phillip A Carter
Copyright (C) 2020, 2021  Gregory D Carl

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
'''

import math
from shutil import copy as COPY
from PyQt5.QtCore import Qt, QCoreApplication
from PyQt5.QtWidgets import QLabel, QLineEdit, QPushButton, QRadioButton, QButtonGroup, QMessageBox
from PyQt5.QtGui import QPixmap

_translate = QCoreApplication.translate

def cancel(P, W, widget):
    COPY(P.fNgcBkp, P.fNgc)
    if widget:
        W.conv_preview.load(P.fNgc)
    W.add.setEnabled(False)
    W.undo.setEnabled(False)

def accept(P, W):
    COPY(P.fNgc, P.fNgcBkp)
    W.conv_preview.load(P.fNgc)
    W.add.setEnabled(False)
    W.undo.setEnabled(False)
    W.conv_send.setEnabled(True)

def preview(P, W):
    if P.dialogError: return
    msg = []
    try:
        columns = int(W.cnEntry.text())
    except:
        msg.append(_translate('Conversational', 'COLUMNS NUMBER'))
    try:
        rows = int(W.rnEntry.text())
    except:
        msg.append(_translate('Conversational', 'ROWS NUMBER'))
    try:
        xOffset = float(W.coEntry.text())
    except:
        msg.append(_translate('Conversational', 'COLUMNS OFFSET'))
    try:
        yOffset = float(W.roEntry.text())
    except:
        msg.append(_translate('Conversational', 'ROWS OFFSET'))
    try:
        xOrgOffset = float(W.oxEntry.text())
    except:
        msg.append(_translate('Conversational', 'X OFFSET ORIGIN'))
    try:
        yOrgOffset = float(W.oyEntry.text())
    except:
        msg.append(_translate('Conversational', 'Y OFFSET ORIGIN'))
    if msg:
        msg0 = _translate('Conversational', 'Invalid entry detected in')
        msg1 = ''
        for m in msg:
            msg1 += '{}\n'.format(m)
        error_set(P, W, '{}:\n\n{}'.format(msg0, msg1))
        return
    if columns > 0 and rows > 0 and (columns == 1 or (columns > 1 and xOffset != 0)) and (rows == 1 or (rows > 1 and yOffset != 0)):
        cancel(P, W, None)
        if P.arrayMode == 'conversational':
            fPre = []
            fPst = []
            outCod = open(P.fTmp, 'w')
            inWiz = open(P.fNgc, 'r')
            while(1):
                d = inWiz.readline()
                if d.startswith('(conversational'):
                    outCod.write(d)
                    break
                fPre.append(d)
            while(1):
                d = inWiz.readline()
                if '(postamble' in d:
                    fPst.append(d)
                    break
                outCod.write(d)
            while(1):
                d = inWiz.readline()
                if not d: break
                fPst.append(d)
            outCod.close()
            inWiz.close()
            outNgc = open(P.fNgc, 'w')
            for line in fPre:
                outNgc.write(line)
            for row in range(rows):
                for column in range(columns):
                    outNgc.write('\n(row:{}  column:{})\n'.format(row + 1, column + 1))
                    inCod = open(P.fTmp, 'r')
                    for line in inCod:
                        raw = line.strip().lower()
                        if (raw.startswith('g0') or raw.startswith('g1') or raw.startswith('g2') or raw.startswith('g3')) and \
                           not raw.replace(' ','').startswith('g0z[#<_ini[axis_z]max_limit>-'):
                            a, b = raw.split('x')
                            c, d = b.split('y')
                            if ('i') in d:
                                e, f = d.split('i')
                                f = 'i' + f
                            else:
                                e = d
                                f = ''
                            outNgc.write('{}x{} y{} {}\n'.format \
                                (a, (float(c) + column * xOffset) + xOrgOffset, (float(e) + row * yOffset) + yOrgOffset, f))
                        else:
                            outNgc.write(line)
                    inCod.close()
            for line in fPst:
                outNgc.write(line)
            outNgc.close()
            W.conv_preview.load(P.fNgc)
            W.conv_preview.set_current_view()
            W.add.setEnabled(True)
            W.undo.setEnabled(True)
        else:
            COPY(P.fNgc, P.fTmp)
            inCod = open(P.fTmp, 'r')
            units = 1
            for line in inCod:
                if 'G21' in line.upper().replace(' ', '') and P.unitsPerMm != 1:
                    units = 25.4
                    break
                if 'G20' in line.upper().replace(' ', '') and P.unitsPerMm == 1:
                    units = 0.03937
                    break
            outNgc = open(P.fNgc, 'w')
            xIndex = [5221,5241,5261,5281,5301,5321,5341,5361,5381][0]
            outNgc.write('#<ucs_x_offset> = #{}\n'.format(xIndex))
            outNgc.write('#<ucs_y_offset> = #{}\n'.format(xIndex + 1))
            for row in range(rows):
                for column in range(columns):
                    outNgc.write('\n(row:{}  column:{})\n'.format(row + 1, column + 1))
                    if P.arrayMode == 'external':
                        outNgc.write('G10 L2 P0 X[{} + #<ucs_x_offset>] Y[{} + #<ucs_y_offset>]\n'.format\
                        ((column * xOffset * units) + xOrgOffset, (row * yOffset * units) + yOrgOffset))
                    else:
                        # check this down the track for arraying external arrays multiple times
                        outNgc.write('G10 L2 P0 X{} Y{} ($$$$$$)\n'.format\
                        ((column * xOffset * units) + xOrgOffset, (row * yOffset * units) + yOrgOffset))
                    inCod = open(P.fTmp, 'r')
                    for line in inCod:
                        a = b = c = ''
                        a = line.upper().replace(' ', '')
                        if 'M2' in a or 'M30' in a:
                            b = a.replace('M2', '')
                            c = b.replace('M30', '')
                            outNgc.write(c)
                        elif '(postamble)' in line:
                            pass
                        else:
                            outNgc.write(line)
                    inCod.close()
            outNgc.write('G10 L2 P0 X#<ucs_x_offset> Y#<ucs_y_offset>\n')
            outNgc.write('M2\n')
            outNgc.close()
            W.conv_preview.load(P.fNgc)
            W.conv_preview.set_current_view()
            W.add.setEnabled(True)
            W.undo.setEnabled(True)
    else:
        msg = []
        if columns <= 0:
            msg.append(_translate('Conversational', 'COLUMNS entries are required'))
        if rows <= 0:
            msg.append(_translate('Conversational', 'ROWS entries are required'))
        if xOffset == 0 and columns > 1:
            msg.append(_translate('Conversational', 'COLUMNS OFFSET is required'))
        if yOffset == 0 and rows > 1:
            msg.append(_translate('Conversational', 'ROWS OFFSET is required'))
        msg0 = ''
        for m in msg:
            msg0 += '{}.\n\n',format(m)
        error_set(P, W, msg0)
        return
    W.add.setEnabled(True)

def error_set(P, W, msg):
    cancel(P, W, 'dummy')
    P.dialogError = True
    P.dialog_show_ok(QMessageBox.Warning, _translate('Conversational', 'Array Error'), msg)

def widgets(P, W):
    #widgets
    W.cLabel = QLabel(_translate('Conversational', 'COLUMNS'))
    W.cnLabel = QLabel(_translate('Conversational', 'NUMBER'))
    W.cnEntry = QLineEdit('1', objectName='cnEntry')
    W.coEntry = QLineEdit('0.0')
    W.coLabel = QLabel(_translate('Conversational', 'OFFSET'))
    W.rLabel = QLabel(_translate('Conversational', 'ROWS'))
    W.rnLabel = QLabel(_translate('Conversational', 'NUMBER'))
    W.rnEntry = QLineEdit('1', objectName='rnEntry')
    W.roEntry = QLineEdit('0.0')
    W.roLabel = QLabel(_translate('Conversational', 'OFFSET'))
    W.oLabel = QLabel(_translate('Conversational', 'ORIGIN'))
    text = _translate('Conversational', 'OFFSET')
    W.oxLabel = QLabel('X {}'.format(text))
    W.oxEntry = QLineEdit('0.0', objectName = 'xsEntry')
    W.oyEntry = QLineEdit('0.0', objectName = 'ysEntry')
    W.oyLabel = QLabel('Y {}'.format(text))
    W.preview = QPushButton(_translate('Conversational', 'PREVIEW'))
    W.add = QPushButton(_translate('Conversational', 'ADD'))
    W.undo = QPushButton(_translate('Conversational', 'UNDO'))
    W.lDesc = QLabel(_translate('Conversational', 'CREATE ARRAY OF SHAPES'))
    #alignment and size
    rightAlign = ['cnLabel', 'cnEntry', 'coEntry', \
                  'rnLabel', 'rnEntry', 'roEntry', \
                  'oxLabel', 'oxEntry', 'oyEntry']
    leftAlign = ['coLabel', 'roLabel', 'oyLabel']
    centerAlign = ['lDesc', 'cLabel', 'rLabel', 'oLabel']
    pButton = ['preview', 'add', 'undo']
    for widget in rightAlign:
        W[widget].setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        W[widget].setFixedWidth(80)
        W[widget].setFixedHeight(24)
    for widget in leftAlign:
        W[widget].setAlignment(Qt.AlignLeft | Qt.AlignVCenter)
        W[widget].setFixedWidth(80)
        W[widget].setFixedHeight(24)
    for widget in centerAlign:
        W[widget].setAlignment(Qt.AlignCenter | Qt.AlignBottom)
        W[widget].setFixedWidth(240)
        W[widget].setFixedHeight(24)
    for widget in pButton:
        W[widget].setFixedWidth(80)
        W[widget].setFixedHeight(24)
    #starting parameters
    W.conv_send.setEnabled(False)
    W.add.setEnabled(False)
    W.undo.setEnabled(False)
    P.conv_undo_shape()
    #connections
    W.preview.pressed.connect(lambda:preview(P, W))
    W.add.pressed.connect(lambda:accept(P, W))
    W.undo.pressed.connect(lambda:cancel(P, W, W.sender()))
    entries = ['cnEntry', 'coEntry', 'rnEntry', 'roEntry', 'oxEntry', 'oyEntry']
    for entry in entries:
        W[entry].textChanged.connect(lambda:P.conv_entry_changed(W.sender()))
        W[entry].returnPressed.connect(lambda:preview(P, W))
    #add to layout
    if P.landscape:
        W.s0 = QLabel('')
        W.s0.setFixedHeight(24)
        W.entries.addWidget(W.s0, 0, 0)
        W.entries.addWidget(W.cLabel, 1, 1, 1, 3)
        W.entries.addWidget(W.cnLabel, 2, 0)
        W.entries.addWidget(W.cnEntry, 2, 1)
        W.entries.addWidget(W.coEntry, 2, 3)
        W.entries.addWidget(W.coLabel, 2, 4)
        W.entries.addWidget(W.rLabel, 3, 1, 1, 3)
        W.entries.addWidget(W.rnLabel, 4, 0)
        W.entries.addWidget(W.rnEntry, 4, 1)
        W.entries.addWidget(W.roEntry, 4, 3)
        W.entries.addWidget(W.roLabel, 4, 4)
        W.entries.addWidget(W.oLabel, 5, 1, 1, 3)
        W.entries.addWidget(W.oxLabel, 6, 0)
        W.entries.addWidget(W.oxEntry, 6, 1)
        W.entries.addWidget(W.oyEntry, 6, 3)
        W.entries.addWidget(W.oyLabel, 6, 4)
        for r in range(7, 13):
            W['{}'.format(r)] = QLabel('')
            W['{}'.format(r)].setFixedHeight(24)
            W.entries.addWidget(W['{}'.format(r)], r, 0)
        W.entries.addWidget(W.preview, 13, 0)
        W.entries.addWidget(W.add, 13, 2)
        W.entries.addWidget(W.undo, 13, 4)
        W.entries.addWidget(W.lDesc, 14 , 1, 1, 3)
    else:
        W.entries.addWidget(W.cLabel, 0, 1, 1, 3)
        W.entries.addWidget(W.cnLabel, 1, 0)
        W.entries.addWidget(W.cnEntry, 1, 1)
        W.entries.addWidget(W.coEntry, 1, 3)
        W.entries.addWidget(W.coLabel, 1, 4)
        W.entries.addWidget(W.rLabel, 2, 1, 1, 3)
        W.entries.addWidget(W.rnLabel, 3, 0)
        W.entries.addWidget(W.rnEntry, 3, 1)
        W.entries.addWidget(W.roEntry, 3, 3)
        W.entries.addWidget(W.roLabel, 3, 4)
        W.entries.addWidget(W.oLabel, 4, 1, 1, 3)
        W.entries.addWidget(W.oxLabel, 5, 0)
        W.entries.addWidget(W.oxEntry, 5, 1)
        W.entries.addWidget(W.oyEntry, 5, 3)
        W.entries.addWidget(W.oyLabel, 5, 4)
        W.entries.addWidget(W.preview, 9, 0)
        W.entries.addWidget(W.add, 9, 2)
        W.entries.addWidget(W.undo, 9, 4)
        W.entries.addWidget(W.lDesc, 10 , 1, 1, 3)
    W.cnEntry.setFocus()
