'''
conv_scale.py

Copyright (C) 2021  Phillip A Carter

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
from PyQt5.QtWidgets import QLabel, QLineEdit, QPushButton, QRadioButton, QButtonGroup, QMessageBox
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
    if W.scEntry.text():
        try:
            scale = float(W.scEntry.text())
        except:
            msg = 'Invalid SCALE entry detected.\n'
            P.dialogError = True
            P.dialog_show_ok(QMessageBox.Warning, 'Scaling Error', msg)
            return
        if scale == 0:
            msg = 'A scale greater then zero is required.\n'
            P.dialogError = True
            P.dialog_show_ok(QMessageBox.Warning, 'Scaling Error', msg)
            return
    else:
        scale = 1
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
                rLine = scale_shape(P, W, scl, line)
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

def scale_shape(P, W, scale, line):
    if line[0] == 'g' and (line[1] not in '0123' or (line[1] in '0123' and len(line) > 2 and line[2] in '0123456789')):
        return '{}\n'.format(line)
    if line[0] == 'g' and line[1] in '0123' and line[2] == 'z':
        return '{}\n'.format(line)
    newLine = ''
    multi = False
    vary = False
    zed = False
    while 1:
        if line[0] == ' ':
            line = line[1:]
        if line[0] == '(' or line[0] == ';':
            if multi:
                newLine += '*{}]'.format(scale)
            newLine += line
            break
        elif line[0].isalpha():
            if multi and not vary:
                multi = False
                newLine += '*{}]'.format(scale)
            if line[0] == 'z':
                zed = True
            newLine += line[0]
            line = line[1:]
        elif line[0] == '<':
            vary = True
            newLine += line[0]
            line = line[1:]
        elif line[0] == '>':
            vary = False
            newLine += line[0]
            line = line[1:]
        elif newLine[-1] in 'xyij' and not vary:
            multi = True
            newLine += '[{}'.format(line[0])
            line = line[1:]
        else:
            newLine += line[0]
            line = line[1:]
        if not line:
            if not zed:
                newLine += '*{}]'.format(scale)
            break
    return ('{}\n'.format(newLine))

def undo_pressed(P, W):
    P.conv_undo_shape()

def widgets(P, W):
    #widgets
    W.scLabel = QLabel('SCALE')
    W.scEntry = QLineEdit()
    W.preview = QPushButton('PREVIEW')
    W.add = QPushButton('ADD')
    W.undo = QPushButton('UNDO')
    W.lDesc = QLabel('SCALING SHAPE')
    #alignment and size
    rightAlign = ['scLabel', 'scEntry']
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
    W.scEntry.setText('1')
    P.conv_undo_shape()
    #connections
    W.preview.pressed.connect(lambda:preview(P, W))
    W.add.pressed.connect(lambda:accept(P, W))
    W.undo.pressed.connect(lambda:cancel(P, W))
    entries = ['scEntry']
    for entry in entries:
        W[entry].textChanged.connect(lambda:P.conv_entry_changed(W.sender()))
        W[entry].editingFinished.connect(lambda:preview(P, W))
    #add to layout
    if P.landscape:
        for r in range(14):
            W['{}'.format(r)] = QLabel('')
            W['{}'.format(r)].setFixedHeight(24)
            W.entries.addWidget(W['{}'.format(r)], 0 + r, 0)
        W.entries.addWidget(W.scLabel, 2, 1)
        W.entries.addWidget(W.scEntry, 2, 2)
        W.entries.addWidget(W.preview, 13, 0)
        W.entries.addWidget(W.add, 13, 2)
        W.entries.addWidget(W.undo, 13, 4)
        W.entries.addWidget(W.lDesc, 14 , 1, 1, 3)
    else:
        W.entries.addWidget(W.scLabel, 1, 1)
        W.entries.addWidget(W.scEntry, 1, 2)
        W.entries.addWidget(W.preview, 9, 0)
        W.entries.addWidget(W.add, 9, 2)
        W.entries.addWidget(W.undo, 9, 4)
        W.entries.addWidget(W.lDesc, 10 , 1, 1, 3)
    W.scEntry.setFocus()
