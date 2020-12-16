'''
conv_array.py

Copyright (C) 2020  Phillip A Carter

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
from PyQt5.QtCore import Qt 
from PyQt5.QtWidgets import QLabel, QLineEdit, QPushButton, QRadioButton, QButtonGroup 
from PyQt5.QtGui import QPixmap 

def cancel(P, W, widget):
    COPY(P.fNgcBkp, P.fNgc)
    if widget:
        P.conv_preview.load(P.fNgc)

def accept(P, W):
    COPY(P.fNgc, P.fNgcBkp)
    P.conv_preview.load(P.fNgc)

def preview(P, W):
    if P.dialogError: return
    try:
        columns = int(W.cnEntry.text())
    except:
        columns = 1
    try:
        rows = int(W.rnEntry.text())
    except:
        rows = 1
    try:
        xOffset = float(W.coEntry.text())
    except:
        xOffset = 0
    try:
        yOffset = float(W.roEntry.text())
    except:
        yOffset = 0
    try:
        xOrgOffset = float(W.oxEntry.text())
    except:
        xOrgOffset = 0
    try:
        yOrgOffset = float(W.oyEntry.text())
    except:
        yOrgOffset = 0
    if columns > 0 and rows > 0 and (columns == 1 or (columns > 1 and xOffset <> 0)) and (rows == 1 or (rows > 1 and yOffset <> 0)):
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
                        if raw.startswith('g0') or raw.startswith('g1') or raw.startswith('g2') or raw.startswith('g3'):
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
    else:
        msg = ''
        if columns <= 0:
            msg += 'Columns are required\n\n'
        if rows <= 0:
            msg += 'Rows are required\n\n'
        if xOffset == 0 and columns > 1:
            msg += 'Column Offset is required\n\n'
        if yOffset == 0 and rows > 1:
            msg += 'Row Offset is required'
        P.dialogError = True
        P.dialog_error('ARRAY', msg)
        return
    W.add.setEnabled(True)

def auto_preview(P, W):
    try:
        if (int(W.cnEntry.text()) == 1 or (int(W.cnEntry.text()) > 1 and float(W.coEntry.text()) > 0)) and \
           (int(W.rnEntry.text()) == 1 or (int(W.rnEntry.text()) > 1 and float(W.roEntry.text()) > 0)): 
            preview(P, W) 
    except:
        pass

def widgets(P, W):
    #widgets
    W.cLabel = QLabel('COLUMNS')
    W.cnLabel = QLabel('Number')
    W.cnEntry = QLineEdit()
    W.coEntry = QLineEdit()
    W.coLabel = QLabel('Offset')
    W.rLabel = QLabel('ROWS')
    W.rnLabel = QLabel('Number')
    W.rnEntry = QLineEdit()
    W.roEntry = QLineEdit()
    W.roLabel = QLabel('Offset')
    W.oLabel = QLabel('ORIGIN')
    W.oxLabel = QLabel('X Offset')
    W.oxEntry = QLineEdit()
    W.oyEntry = QLineEdit()
    W.oyLabel = QLabel('Y Offset')
    W.preview = QPushButton('Preview')
    W.add = QPushButton('Add')
    W.undo = QPushButton('Undo')
    W.lDesc = QLabel('Create Array Of Shapes')
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
    W.add.setEnabled(False)
    W.cnEntry.setText('1')
    W.coEntry.setText('0')
    W.rnEntry.setText('1')
    W.roEntry.setText('0')
    W.oxEntry.setText('0')
    W.oyEntry.setText('0')
    P.conv_undo_shape('add')
    W.cnEntry.setFocus()
    #connections
    W.preview.pressed.connect(lambda:preview(P, W))
    W.add.pressed.connect(lambda:accept(P, W))
    W.undo.pressed.connect(lambda:cancel(P, W, W.sender()))
    entries = ['cnEntry', 'coEntry', 'rnEntry', 'roEntry', 'oxEntry', 'oyEntry']
    for entry in entries:
        W[entry].textChanged.connect(lambda:P.conv_entry_changed(W.sender()))
        W[entry].editingFinished.connect(lambda:auto_preview(P, W))
    #add to layout
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
    for blank in range(6):
        W['{}'.format(blank)] = QLabel('')
        W['{}'.format(blank)].setFixedHeight(24)
        W.entries.addWidget(W['{}'.format(blank)], 6 + blank, 0)
    W.entries.addWidget(W.preview, 12, 0)
    W.entries.addWidget(W.add, 12, 2)
    W.entries.addWidget(W.undo, 12, 4)
    W.entries.addWidget(W.lDesc, 13 , 1, 1, 3)
