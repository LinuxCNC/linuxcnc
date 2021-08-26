'''
conv_scale.py

Copyright (C) 2021  Phillip A Carter
Copyright (C) 2021  Gregory D Carl

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
from PyQt5.QtCore import Qt, QCoreApplication
from PyQt5.QtWidgets import QLabel, QLineEdit, QPushButton, QRadioButton, QButtonGroup, QMessageBox
from PyQt5.QtGui import QPixmap

_translate = QCoreApplication.translate

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
    msg = []
    try:
        if W.scEntry.text():
            scale = float(W.scEntry.text())
        else:
            scale = 1
        if scale == 0:
            scale = 1
            W.scEntry.setText('1')
    except:
        msg.append(_translate('Conversational', 'SCALE'))
    text = _translate('Conversational', 'OFFSET')
    try:
        if W.xEntry.text():
            xOffset = float(W.xEntry.text())
        else:
            xOffset = 0
    except:
        msg.append(_translate('Conversational', 'X {}'.format(text)))
    try:
        if W.yEntry.text():
            yOffset = float(W.yEntry.text())
        else:
            yOffset = 0
    except:
        msg.append(_translate('Conversational', 'Y {}'.format(text)))
    if msg:
        msg0 = _translate('Conversational', 'Invalid entry detected in')
        msg1 = ''
        for m in msg:
            msg1 += '{}\n'.format(m)
        error_set(P, W, '{}:\n\n{}'.format(msg0, msg1))
        return
    if scale == 1 and not xOffset and not yOffset:
        cancel(P, W)
        return
    scl = '#<conv_scale>'
    inCod = open(P.fNgcBkp, 'r')
    if scl in inCod.read():
        inCod.seek(0, 0)
        outNgc = open(P.fNgc, 'w')
        for line in inCod:
            if line.strip().startswith(scl):
                line = ('{} = {}\n'.format(scl, scale))
            outNgc.write(line)
    else:
        inCod.seek(0, 0)
        outNgc = open(P.fNgc, 'w')
        outNgc.write('{} = {}\n'.format(scl, scale))
        for line in inCod:
            line = line.strip().lower()
            # remove line numbers
            if line.startswith('n'):
                line = line[1:]
                while line[0].isdigit() or line[0] == '.':
                    line = line[1:].lstrip()
                    if not line:
                        break
            if len(line) and line[0] in 'gxy':
                rLine = scale_shape(P, W, scl, line, xOffset, yOffset)
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

def scale_shape(P, W, scale, line, xOffset, yOffset):
    if line[0] == 'g' and (line[1] not in '0123' or (line[1] in '0123' and len(line) > 2 and line[2] in '0123456789')):
        return '{}\n'.format(line)
    if line[0] == 'g' and line[1] in '0123' and line[2] == 'z':
        return '{}\n'.format(line)
    newLine = ''
    multiAxis = False
    numParam = False
    namParam = False
    zAxis = False
    lastAxis = ''
    offset = ''
    while 1:
        # remove spaces
        if line[0] == ' ':
            line = line[1:]
        # if beginning of comment
        if line[0] == '(' or line[0] == ';':
            if multiAxis:
                print("0", line[0])
                if lastAxis in 'xy':
                    newLine += '*{} + {}]'.format(scale, offset)
                else:
                    newLine += '*{}]'.format(scale)
            newLine += line
            break
        # if alpha character
        elif line[0].isalpha():
            if not numParam and not namParam:
                if multiAxis:
                    if lastAxis in 'xy':
                        newLine += '*{} + {}]'.format(scale, offset)
                    else:
                        newLine += '*{}]'.format(scale)
                lastAxis = line[0]
                if line[0] == 'x':
                    offset = xOffset
                elif line[0] == 'y':
                    offset = yOffset
                elif line[0] == 'z':
                    zAxis = True
            newLine += line[0]
            line = line[1:]
        # if beginning of parameter
        elif line[0] == '#':
            numParam = True
            newLine += line[0]
            line = line[1:]
        # if parameter should be a named parameter
        elif line[0] == '<' and numParam:
            numParam = False
            namParam = True
            newLine += line[0]
            line = line[1:]
        #if end of numbered parameter
        elif not line[0].isdigit() and numParam:
            numParam = False
            newLine += line[0]
            line = line[1:]
        # if end of named parameter
        elif line[0] == '>' and namParam:
            namParam = False
            newLine += line[0]
            line = line[1:]
        #if last axis was x, y, i, or j
        elif newLine[-1] in 'xyij' and not numParam and not namParam:
            multiAxis = True
            newLine += '[{}'.format(line[0])
            line = line[1:]
        # everything else
        else:
            newLine += line[0]
            line = line[1:]
        # empty line, must be finished
        if not line:
            if not zAxis:
                if lastAxis in 'xy':
                    newLine += '*{} + {}]'.format(scale, offset)
                else:
                    newLine += '*{}]'.format(scale)
            break
    return ('{}\n'.format(newLine))

def error_set(P, W, msg):
    cancel(P, W)
    P.dialogError = True
    P.dialog_show_ok(QMessageBox.Warning, _translate('Conversational', 'Scaling Error'), msg)

def widgets(P, W):
    #widgets
    W.scLabel = QLabel(_translate('Conversational', 'SCALE'))
    W.scEntry = QLineEdit('1.0')
    text = _translate('Conversational', 'OFFSET')
    W.xLabel = QLabel(_translate('Conversational', 'X {}'.format(text)))
    W.xEntry = QLineEdit('0.0', objectName = 'xsEntry')
    W.yLabel = QLabel(_translate('Conversational', 'Y {}'.format(text)))
    W.yEntry = QLineEdit('0.0', objectName = 'ysEntry')
    W.preview = QPushButton(_translate('Conversational', 'PREVIEW'))
    W.add = QPushButton(_translate('Conversational', 'ADD'))
    W.undo = QPushButton(_translate('Conversational', 'UNDO'))
    W.lDesc = QLabel(_translate('Conversational', 'SCALING SHAPE'))
    #alignment and size
    rightAlign = ['scLabel', 'scEntry', 'xLabel', 'xEntry', 'yLabel', 'yEntry']
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
    P.conv_undo_shape()
    #connections
    W.preview.pressed.connect(lambda:preview(P, W))
    W.add.pressed.connect(lambda:accept(P, W))
    W.undo.pressed.connect(lambda:cancel(P, W))
    entries = ['scEntry', 'xEntry', 'yEntry']
    for entry in entries:
        W[entry].textChanged.connect(lambda:P.conv_entry_changed(W.sender()))
        W[entry].returnPressed.connect(lambda:preview(P, W))
    #add to layout
    if P.landscape:
        for r in range(14):
            W['{}'.format(r)] = QLabel('')
            W['{}'.format(r)].setFixedHeight(24)
            W.entries.addWidget(W['{}'.format(r)], 0 + r, 0)
        W.entries.addWidget(W.scLabel, 2, 1)
        W.entries.addWidget(W.scEntry, 2, 2)
        W.entries.addWidget(W.xLabel, 4, 1)
        W.entries.addWidget(W.xEntry, 4, 2)
        W.entries.addWidget(W.yLabel, 6 , 1)
        W.entries.addWidget(W.yEntry, 6, 2)
        W.entries.addWidget(W.preview, 13, 0)
        W.entries.addWidget(W.add, 13, 2)
        W.entries.addWidget(W.undo, 13, 4)
        W.entries.addWidget(W.lDesc, 14 , 1, 1, 3)
    else:
        W.entries.addWidget(W.scLabel, 1, 1)
        W.entries.addWidget(W.scEntry, 1, 2)
        W.entries.addWidget(W.xLabel, 3, 1)
        W.entries.addWidget(W.xEntry, 3, 2)
        W.entries.addWidget(W.yLabel, 5 , 1)
        W.entries.addWidget(W.yEntry, 5, 2)
        W.entries.addWidget(W.preview, 9, 0)
        W.entries.addWidget(W.add, 9, 2)
        W.entries.addWidget(W.undo, 9, 4)
        W.entries.addWidget(W.lDesc, 10 , 1, 1, 3)
    W.scEntry.setFocus()
