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
    W.undo.setEnabled(False)

def accept(P, W):
    COPY(P.fNgc, P.fNgcBkp)
    W.conv_preview.load(P.fNgc)
    W.conv_preview.set_current_view()
    W.add.setEnabled(False)
    W.undo.setEnabled(False)
    W.conv_send.setEnabled(True)

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
        line = line.strip().lower()
        # remove line numbers
        if line.startswith('n'):
            line = line[1:]
            while line[0].isdigit() or line[0] == '.':
                line = line[1:].lstrip()
                if not line:
                    break
        if line.startswith('g'):
            rLine = rotate(P, W, angle, xOffset, yOffset, line)
            if rLine is not None:
                outNgc.write(rLine)
            else:
                return
        else:
            outNgc.write('{}\n'.format(line))
    inCod.close()
    outNgc.close()
    W.conv_preview.load(P.fNgc)
    W.conv_preview.set_current_view()
    W.add.setEnabled(True)
    W.undo.setEnabled(True)

def rotate(P, W, angle, xOffset, yOffset, line):
    REGCODE = COMPILE('([a-z]-?[0-9]+\.?([0-9]+)?)|\(.*\)')
    inLine = line
    comment = ''
    i = inLine.find('(')
    if i >= 0:
      comment = inLine[i:]
      inLine = inLine[:i - 1].strip()
    if len(inLine) > 0:
        parts = list([ list(cmd)[0] for cmd in REGCODE.findall(inLine) ])
        if len(parts) <= 1 or parts[0] not in ['g0', 'g1', 'g2', 'g3', 'x', 'y'] or \
           inLine.replace(' ','').startswith('g0z[#<_ini[axis_z]max_limit>-'):
            return '{}\n'.format(line)
        angle = math.radians(angle)
        params = {'x':0.0, 'y':0.0, 'r':0.0, 'i':0.0, 'j':0.0,}
        used = ''
        for p in parts:
            for n in 'xyrij':
                if n in p:
                    if n == 'x':
                        params['x'] = float(p.strip(n))
                        used += 'x'
                    elif n == 'y':
                        params['y'] = float(p.strip(n))
                        used += 'y'
                    elif n == 'r':
                        params['r'] = float(p.strip(n))
                        used += 'r'
                    elif n == 'i':
                        params['i'] = float(p.strip(n))
                        used += 'i'
                    elif n == 'j':
                        params['j'] = float(p.strip(n))
                        used += 'j'
        newLine = ('{}'.format(parts[0]))
        if 'x' in used:
            newLine += (' x{:.6f}'.format(params['x'] * math.cos(angle) - params['y'] * math.sin(angle) + xOffset))
        if 'y' in used:
            newLine += (' y{:.6f}'.format(params['y'] * math.cos(angle) + params['x'] * math.sin(angle) + yOffset))
        if 'r' in used:
            newLine += (' r{:.6f}'.format(params['r']))
        if 'i' in used:
            newLine += (' i{:.6f}'.format(params['i'] * math.cos(angle) - params['j'] * math.sin(angle)))
        if 'j' in used:
            newLine += (' j{:.6f}'.format(params['j'] * math.cos(angle) + params['i'] * math.sin(angle)))
        return ('{}\n'.format(newLine))

def undo_pressed(P, W):
    P.conv_undo_shape()

def widgets(P, W):
    #widgets
    W.aLabel = QLabel('ANGLE')
    W.aEntry = QLineEdit()
    W.xLabel = QLabel('X OFFSET')
    W.xEntry = QLineEdit()
    W.yLabel = QLabel('Y OFFSET')
    W.yEntry = QLineEdit()
    W.preview = QPushButton('PREVIEW')
    W.add = QPushButton('ADD')
    W.undo = QPushButton('UNDO')
    W.lDesc = QLabel('ROTATING SHAPE')
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
    W.conv_send.setEnabled(False)
    W.add.setEnabled(False)
    W.undo.setEnabled(False)
    W.aEntry.setText('0')
    W.xEntry.setText('0')
    W.yEntry.setText('0')
    P.conv_undo_shape()
    #connections
    W.preview.pressed.connect(lambda:preview(P, W))
    W.add.pressed.connect(lambda:accept(P, W))
    W.undo.pressed.connect(lambda:cancel(P, W))
    entries = ['aEntry', 'xEntry', 'yEntry']
    for entry in entries:
        W[entry].textChanged.connect(lambda:P.conv_entry_changed(W.sender()))
        W[entry].editingFinished.connect(lambda:preview(P, W))
    #add to layout
    if P.landscape:
        for r in range(14):
            W['{}'.format(r)] = QLabel('')
            W['{}'.format(r)].setFixedHeight(24)
            W.entries.addWidget(W['{}'.format(r)], 0 + r, 0)
        W.entries.addWidget(W.aLabel, 2, 1)
        W.entries.addWidget(W.aEntry, 2, 2)
        W.entries.addWidget(W.xLabel, 4, 1)
        W.entries.addWidget(W.xEntry, 4, 2)
        W.entries.addWidget(W.yLabel, 6 , 1)
        W.entries.addWidget(W.yEntry, 6, 2)
        W.entries.addWidget(W.preview, 13, 0)
        W.entries.addWidget(W.add, 13, 2)
        W.entries.addWidget(W.undo, 13, 4)
        W.entries.addWidget(W.lDesc, 14 , 1, 1, 3)
    else:
        W.entries.addWidget(W.aLabel, 1, 1)
        W.entries.addWidget(W.aEntry, 1, 2)
        W.entries.addWidget(W.xLabel, 3, 1)
        W.entries.addWidget(W.xEntry, 3, 2)
        W.entries.addWidget(W.yLabel, 5 , 1)
        W.entries.addWidget(W.yEntry, 5, 2)
        W.entries.addWidget(W.preview, 9, 0)
        W.entries.addWidget(W.add, 9, 2)
        W.entries.addWidget(W.undo, 9, 4)
        W.entries.addWidget(W.lDesc, 10 , 1, 1, 3)
    W.aEntry.setFocus()
