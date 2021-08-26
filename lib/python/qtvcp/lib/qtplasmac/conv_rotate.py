'''
conv_rotate.py

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
    global PARAMS
    PARAMS = {}
    if P.dialogError: return
    msg = []
    try:
        if W.aEntry.text():
            angle = float(W.aEntry.text())
        else:
            angle = 0
    except:
        msg.append(_translate('Conversational', 'ANGLE'))
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
    if not angle and not xOffset and not yOffset:
        cancel(P, W)
        return
    outNgc = open(P.fNgc, 'w')
    outNgc.write(';rotated conversational shape\n')
    with open(P.fNgcBkp, 'r') as inFile:
        inCod = inFile.readlines()
    relative = False
    lastCoords = ['','','','','','','']
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
            if 'g91' in line:
                relative = True
            elif 'g90' in line:
                relative = False
            rLine = rotate(P, W, angle, xOffset, yOffset, line, relative, lastCoords)
            if rLine is not None:
                outNgc.write(rLine)
            else:
                return
        else:
            outNgc.write('{}\n'.format(line))
    outNgc.close()
    W.conv_preview.load(P.fNgc)
    W.conv_preview.set_current_view()
    W.add.setEnabled(True)
    W.undo.setEnabled(True)

def rotate(P, W, angle, xOffset, yOffset, line, relative, lastCoords):
    global PARAMS
    if line[0] == 'g' and (line.replace(' ','')[1] not in '0123' or (line.replace(' ','')[1] in '0123' and len(line.replace(' ','')) > 2 and line.replace(' ','')[2] in '0123456789')):
        return '{}\n'.format(line)
    if line[0] == 'g' and line.replace(' ','')[1] in '0123' and line.replace(' ','')[2] == 'z':
        return '{}\n'.format(line)
    bracket = 0
    param = 0
    params = []
    pcount = 0
    zed = False
    coords = ['','','','','','','']
    clist = 'gxyijrc'
    current = ''
    # break line up into a list of command plus coordinates
    while line:
        if line[0] == ' ':
            line = line[1:]
        elif line[0] == '(' or line[0] == ';':
            current = 'c'
            coords[clist.index(current)] += line[0]
            line = line[1:]
        elif line[0] == '#' and current != 'c':
            param = 1
            pcount += 1
            params.append(line[0])
            if current:
                coords[clist.index(current)] += line[0]
            line = line[1:]
        elif line[0] == '<' and current != 'c':
            if param:
                params[pcount-1] += line[0]
            if param == 1:
                param = 2
            if current:
                coords[clist.index(current)] += line[0]
            line = line[1:]
        elif line[0] == '>' and current != 'c':
            if param:
                params[pcount-1] += line[0]
            param = 0
            if current:
                coords[clist.index(current)] += line[0]
            line = line[1:]
        elif line[0].isalpha() and not param and not bracket and current != 'c':
            if line[0] in 'gxyijr':
                current = line[0]
            else:
                current = ''
            if line[0] == 'z':
                zed = True
            line = line[1:]
        elif line[0] == '[' and not param and current != 'c':
            bracket += 1
            if current:
                coords[clist.index(current)] += line[0]
            line = line[1:]
#        elif line[0] == ']' and not param and current != 'c':

        elif line[0] == ']' and current != 'c':
            if param:
                params[pcount-1] += line[0]
            param = 0

            bracket -= 1
            if current:
                coords[clist.index(current)] += line[0]
            line = line[1:]
        else:
            if param:
                params[pcount-1] += line[0]
            if current and (line[0] in '-.0123456789' or bracket):
                coords[clist.index(current)] += line[0]
            elif current == 'c':
                coords[clist.index(current)] += line[0]
            line = line[1:]
    # create dict of all discovered parameters plus values
    if params:
        for p in params:
            if p not in PARAMS:
                with open(P.fNgcBkp, 'r') as inFile:
                    for l in inFile:
                        l = l.replace(' ','')
                        if l.startswith('{}='.format(p)):
                            v = l.split('=')[1]
                            PARAMS[p] = v.strip()
    # replace params with values
    if PARAMS:
        for p in PARAMS:
            coords = [sub.replace(p, PARAMS[p]) for sub in coords]
    # relative coordinates need no calcs
    if relative:
        newLine = 'g{}'.format(coords[clist.index('g')])
        if coords[clist.index('x')]:
            newLine += ' x{}'.format(coords[clist.index('x')])
        if coords[clist.index('y')]:
            newLine += ' y{}'.format(coords[clist.index('y')])
    else:
        # if no x/y value replace with last value
        if not coords[clist.index('x')]:
            coords[clist.index('x')] = lastCoords[clist.index('x')]
        if not coords[clist.index('y')]:
            coords[clist.index('y')] = lastCoords[clist.index('y')]
        # calculate new coordinates
        if len(coords[clist.index('g')]):
            newLine = 'g{}'.format(coords[clist.index('g')])
        s = math.sin(math.radians(angle))
        c = math.cos(math.radians(angle))
        # attempt to create a float from the x/y coordinates
        try:
            x = float(coords[clist.index('x')].replace('[','(').replace(']',')'))
            y = float(coords[clist.index('y')].replace('[','(').replace(']',')'))
            xo = float(xOffset)
            yo = float(yOffset)
            newLine += ' x{:.6f}'.format(x * c - y * s + xo)
            newLine += ' y{:.6f}'.format(y * c + x * s + yo)
        # otherwise use the original x/y calculation
        except:
            newLine += ' x[{}*{:.6f}-{}*{:.6f}+{}]'.format(coords[clist.index('x')], c, coords[clist.index('y')], s, xOffset)
            newLine += ' y[{}*{:.6f}+{}*{:.6f}+{}]'.format(coords[clist.index('y')], c, coords[clist.index('x')], s, yOffset)

            # if coords[clist.index('x')] and coords[clist.index('y')]:
            #     newLine += ' x[{}*{:.6f}-{}*{:.6f}+{}]'.format(coords[clist.index('x')], c, coords[clist.index('y')], s, xOffset)
            #     newLine += ' y[{}*{:.6f}+{}*{:.6f}+{}]'.format(coords[clist.index('y')], c, coords[clist.index('x')], s, yOffset)
            # elif coords[clist.index('x')]:
            #     newLine += ' x[{}*{:.6f}-{}*{:.6f}+{}]'.format(coords[clist.index('x')], c, lastCoords[clist.index('y')], s, xOffset)
            #     newLine += ' y[{}*{:.6f}+{}*{:.6f}+{}]'.format(lastCoords[clist.index('y')], c, coords[clist.index('x')], s, yOffset)
            # elif coords[clist.index('y')]:
            #     newLine += ' x[{}*{:.6f}-{}*{:.6f}+{}]'.format(lastCoords[clist.index('x')], c, coords[clist.index('y')], s, xOffset)
            #     newLine += ' y[{}*{:.6f}+{}*{:.6f}+{}]'.format(coords[clist.index('y')], c, lastCoords[clist.index('x')], s, yOffset)


        # do arc calculations
        if newLine[1] == '2' or newLine[1] == '3':
            # center format arc
            if not len(coords[clist.index('r')]):
                # attempt to create a float from the i/j value
                try:
                    i = float(coords[clist.index('i')])
                    j = float(coords[clist.index('j')])
                    newLine += (' i{:.6f}'.format(i * c - j * s))
                    newLine += (' j{:.6f}'.format(j * c + i * s))
                # otherwise use the original i/j calculation
                except:
                    newLine += ' i[{}*{:.6f}-{}*{:.6f}]'.format(coords[clist.index('i')], c, coords[clist.index('j')], s)
                    newLine += ' j[{}*{:.6f}+{}*{:.6f}]'.format(coords[clist.index('j')], c, coords[clist.index('i')], s)
            # radius format arc
            else:
                # attempt to create a float from the r value
                try:
                    r = float(coords[clist.index('r')])
                    newLine += (' r{:.6f}'.format(r))
                # otherwise use the original r calculation
                except:
                    newLine += ' r[{}]'.format(coords[clist.index('r')])
        # save the coordinates
        for co in coords:
            if co:
                lastCoords[coords.index(co)] = co
        newLine += ' {}'.format(coords[clist.index('c')])
    return '{}\n'.format(newLine)

def error_set(P, W, msg):
    cancel(P, W)
    P.dialogError = True
    P.dialog_show_ok(QMessageBox.Warning, _translate('Conversational', 'Rotate Error'), msg)

def widgets(P, W):
    #widgets
    W.aLabel = QLabel(_translate('Conversational', 'ANGLE'))
    W.aEntry = QLineEdit('0.0', objectName='aEntry')
    text = _translate('Conversational', 'OFFSET')
    W.xLabel = QLabel(_translate('Conversational', 'X {}'.format(text)))
    W.xEntry = QLineEdit('0.0', objectName = 'xsEntry')
    W.yLabel = QLabel(_translate('Conversational', 'Y {}'.format(text)))
    W.yEntry = QLineEdit('0.0', objectName = 'ysEntry')
    W.preview = QPushButton(_translate('Conversational', 'PREVIEW'))
    W.add = QPushButton(_translate('Conversational', 'ADD'))
    W.undo = QPushButton(_translate('Conversational', 'UNDO'))
    W.lDesc = QLabel(_translate('Conversational', 'ROTATING SHAPE'))
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
    P.conv_undo_shape()
    #connections
    W.preview.pressed.connect(lambda:preview(P, W))
    W.add.pressed.connect(lambda:accept(P, W))
    W.undo.pressed.connect(lambda:cancel(P, W))
    entries = ['aEntry', 'xEntry', 'yEntry']
    for entry in entries:
        W[entry].textChanged.connect(lambda:P.conv_entry_changed(W.sender()))
        W[entry].returnPressed.connect(lambda:preview(P, W))
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
