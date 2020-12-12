'''
conv_rotate.py

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
from re import compile as COMPILE
from PyQt5.QtCore import Qt 
from PyQt5.QtWidgets import QLabel, QLineEdit, QPushButton, QRadioButton, QButtonGroup 
from PyQt5.QtGui import QPixmap 

def cancel(P, W):
    COPY(P.fNgcBkp, P.fNgc)
    W.conv_preview.load(P.fNgc)
    W.conv_preview.set_current_view()
    W.add.setEnabled(False)

def accept(P, W):
    COPY(P.fNgc, P.fNgcBkp)
    W.conv_preview.load(P.fNgc)
    W.conv_preview.set_current_view()
    W.add.setEnabled(False)

def preview(P, W):
    if P.dialogError: return
    if W.aEntry.text():
        angle = float(W.aEntry.text())
    else:
        angle = 0
    if W.xEntry.text():
        xOffset = float(W.xEntry.text())
    else:
        xOffset = 0
    if W.yEntry.text():
        yOffset = float(W.yEntry.text())
    else:
        yOffset = 0
    outNgc = open(P.fNgc, 'w')
    inCod = open(P.fNgcBkp, 'r')
    for line in inCod:
        if line.strip().lower().startswith('g'):
            rLine = rotate(P, W, angle, xOffset, yOffset, line)
            if rLine is not None:
                outNgc.write(rLine)
            else:
                return
        else:
            outNgc.write(line)
    inCod.close()
    outNgc.close()
    W.conv_preview.load(P.fNgc)
    W.conv_preview.set_current_view()
    W.add.setEnabled(True)

def rotate(P, W, angle, xOffset, yOffset, line):
    REGCODE = COMPILE('([a-z]-?[0-9]+\.?([0-9]+)?)|\(.*\)')
    inLine = line.strip()
    comment = ''
    i = inLine.find('(')
    if i >= 0:
      comment = inLine[i:]
      inLine = inLine[:i - 1].strip()
    if len(inLine) > 0:
        parts = list([ list(cmd)[0] for cmd in REGCODE.findall(inLine) ])
        if len(parts) == 0 or parts[0] not in ['g0', 'g1', 'g2', 'g3']:
            return line
        angle = math.radians(angle)
        params = {'x':0.0, 'y':0.0, 'i':0.0, 'j':0.0,}
        used = ''
        for p in parts:
            for n in 'xyij':
                if n in p:
                    if n == 'x':
                        params['x'] = float(p.strip(n))
                        used += 'x'
                    elif n == 'y':
                        params['y'] = float(p.strip(n))
                        used += 'y'
                    elif n == 'i':
                        params['i'] = float(p.strip(n))
                        used += 'i'
                    elif n == 'j':
                        params['j'] = float(p.strip(n))
                        used += 'j'
        newLine = ('{}'.format(parts[0]))
        if not 'x' in used and not 'y' in used:
            P.dialogError = True
            P.dialog_error('ROTATE', 'Cannot decipher G-Code correctly')
            return None
        if 'x' in used:
            newLine += (' x{:.6f}'.format(params['x'] * math.cos(angle) - params['y'] * math.sin(angle) + xOffset))
        if 'y' in used:
            newLine += (' y{:.6f}'.format(params['y'] * math.cos(angle) + params['x'] * math.sin(angle) + yOffset))
        if parts[0] in {'g2', 'g3'}:
            newLine += (' i{:.6f}'.format(params['i'] * math.cos(angle) - params['j'] * math.sin(angle)))
            newLine += (' j{:.6f}'.format(params['j'] * math.cos(angle) + params['i'] * math.sin(angle)))
        return ('{}\n'.format(newLine))

def widgets(P, W):
    #widgets
    W.aLabel = QLabel('Angle')
    W.aEntry = QLineEdit()
    W.xLabel = QLabel('X Offset')
    W.xEntry = QLineEdit()
    W.yLabel = QLabel('Y Offset')
    W.yEntry = QLineEdit()
    W.preview = QPushButton('Preview')
    W.add = QPushButton('Add')
    W.undo = QPushButton('Undo')
    W.lDesc = QLabel('Rotating Shape')
    #alignment and size
    rightAlign = ['aLabel', 'aEntry', 'xLabel', 'xEntry', 'yLabel', 'yEntry']
    centerAlign = ['lDesc']
    pButton = ['preview', 'add', 'undo']
    for widget in rightAlign:
        W[widget].setAlignment(Qt.AlignRight | Qt.AlignVCenter)
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
    W.aEntry.setText('0')
    W.xEntry.setText('0')
    W.yEntry.setText('0')
    P.conv_undo_shape('add')
    W.aEntry.setFocus()
    #connections
    W.preview.pressed.connect(lambda:preview(P, W))
    W.add.pressed.connect(lambda:accept(P, W))
    W.undo.pressed.connect(lambda:cancel(P, W))
    entries = ['aEntry', 'xEntry', 'yEntry']
    for entry in entries:
        W[entry].textChanged.connect(lambda:P.conv_entry_changed(W.sender()))
        W[entry].editingFinished.connect(lambda:preview(P, W))
    #add to layout
    W.blank0 = QLabel('')
    W.blank0.setFixedHeight(24)
    W.entries.addWidget(W.blank0, 0, 0)
    W.entries.addWidget(W.aLabel, 1, 1)
    W.entries.addWidget(W.aEntry, 1, 2)
    W.blank2 = QLabel('')
    W.blank2.setFixedHeight(24)
    W.entries.addWidget(W.blank2, 2, 0)
    W.entries.addWidget(W.xLabel, 3, 1)
    W.entries.addWidget(W.xEntry, 3, 2)
    W.blank4 = QLabel('')
    W.blank4.setFixedHeight(24)
    W.entries.addWidget(W.blank4, 4, 0)
    W.entries.addWidget(W.yLabel, 5 , 1)
    W.entries.addWidget(W.yEntry, 5, 2)
    for blank in range(6):
        W['{}'.format(blank)] = QLabel('')
        W['{}'.format(blank)].setFixedHeight(24)
        W.entries.addWidget(W['{}'.format(blank)], 6 + blank, 0)
    W.entries.addWidget(W.preview, 12, 0)
    W.entries.addWidget(W.add, 12, 2)
    W.entries.addWidget(W.undo, 12, 4)
    W.entries.addWidget(W.lDesc, 13 , 1, 1, 3)
