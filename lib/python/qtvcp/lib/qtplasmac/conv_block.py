'''
conv_block.py

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

import sys, io
import math
from shutil import copy as COPY
from PyQt5.QtCore import Qt, QCoreApplication
from PyQt5.QtWidgets import QLabel, QLineEdit, QPushButton, QRadioButton, QButtonGroup, QMessageBox
from PyQt5.QtGui import QPixmap
from qtvcp.core import Action, Status

_translate = QCoreApplication.translate

ACTION = Action()
STATUS = Status()

def preview(P, W):
    if P.dialogError or not W.preview.isEnabled():
        return
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
    try:
        angle = float(W.aEntry.text())
    except:
        msg.append(_translate('Conversational', 'ANGLE'))
    try:
        scale = float(W.scEntry.text())
        if scale == 0:
            scale = 1
            W.scEntry.setText('1')
    except:
        msg.append(_translate('Conversational', 'SCALE'))
    try:
        rotation = float(W.rtEntry.text())
    except:
        msg.append(_translate('Conversational', 'ROTATION'))
    if msg:
        msg0 = _translate('Conversational', 'Invalid entry detected in')
        msg1 = ''
        for m in msg:
            msg1 += '{}\n'.format(m)
        error_set(P, W, '{}:\n\n{}'.format(msg0, msg1))
        return
#    if columns > 0 and rows > 0 and (columns == 1 or (columns > 1 and xOffset != 0)) and (rows == 1 or (rows > 1 and yOffset != 0)):
    if 1:
        COPY(P.fNgc, P.fTmp)
        inCode = open(P.fTmp, 'r')
        outNgc = open(P.fNgc, 'w')
        # edit existing parameters
        if P.convBlock[0]:
            indent = False
            for line in inCode:
                if line.startswith('#<array_x_offset>'):
                    line = '#<array_x_offset> = {}\n'.format(xOffset)
                elif line.startswith('#<array_y_offset>'):
                    line = '#<array_y_offset> = {}\n'.format(yOffset)
                elif line.startswith('#<array_columns>'):
                    line = '#<array_columns> = {}\n'.format(columns)
                elif line.startswith('#<array_rows>'):
                    line = '#<array_rows> = {}\n'.format(rows)
                elif line.startswith('#<origin_x_offset>'):
                    line = '#<origin_x_offset> = {}\n'.format(xOrgOffset)
                elif line.startswith('#<origin_y_offset>'):
                    line = '#<origin_y_offset> = {}\n'.format(yOrgOffset)
                elif line.startswith('#<array_angle>'):
                    line ='#<array_angle> = {}\n'.format(angle)
                elif line.startswith('#<blk_scale>'):
                    line ='#<blk_scale> = {}\n'.format(scale)
                elif line.startswith('#<shape_angle>'):
                    line ='#<shape_angle> = {}\n'.format(rotation)
                elif line.startswith('#<shape_mirror>'):
                    line ='#<shape_mirror> = {}\n'.format(P.convMirror)
                elif line.startswith('#<shape_flip>'):
                    line ='#<shape_flip> = {}\n'.format(P.convFlip)
                elif '#<shape_mirror>' in line and (P.convMirrorToggle or P.convFlipToggle):
                    if 'g2' in line:
                        line = line.replace('g2', 'g3')
                    elif 'g3' in line:
                        line = line.replace('g3', 'g2')
                outNgc.write('{}'.format(line))
        # create new array
        else:
            xIndex = [5221,5241,5261,5281,5301,5321,5341,5361,5381][0]
            outNgc.write(';conversational block\n\n')
            # inputs
            outNgc.write(';inputs\n')
            outNgc.write('#<ucs_x_offset> = #{}\n'.format(xIndex))
            outNgc.write('#<ucs_y_offset> = #{}\n'.format(xIndex + 1))
            outNgc.write('#<ucs_r_offset> = #{}\n'.format(xIndex + 9))
            outNgc.write('#<array_x_offset> = {}\n'.format(xOffset))
            outNgc.write('#<array_y_offset> = {}\n'.format(yOffset))
            outNgc.write('#<array_columns> = {}\n'.format(columns))
            outNgc.write('#<array_rows> = {}\n'.format(rows))
            outNgc.write('#<origin_x_offset> = {}\n'.format(xOrgOffset))
            outNgc.write('#<origin_y_offset> = {}\n'.format(yOrgOffset))
            outNgc.write('#<array_angle> = {}\n'.format(angle))
            outNgc.write('#<blk_scale> = {}\n'.format(scale))
            outNgc.write('#<shape_angle> = {}\n'.format(rotation))
            outNgc.write('#<shape_mirror> = {}\n'.format(P.convMirror))
            outNgc.write('#<shape_flip> = {}\n\n'.format(P.convFlip))
            # calculations
            outNgc.write(';calculations\n')
            outNgc.write('#<this_col> = 0\n')
            outNgc.write('#<this_row> = 0\n')
            outNgc.write('#<blk_x_offset> = [#<origin_x_offset> + [#<ucs_x_offset> * {}]]\n'.format(P.convUnits[0]))
            outNgc.write('#<blk_y_offset> = [#<origin_y_offset> + [#<ucs_y_offset> * {}]]\n'.format(P.convUnits[0]))
            outNgc.write('#<x_sin> = [[#<array_x_offset> * #<blk_scale>] * SIN[#<array_angle>]]\n')
            outNgc.write('#<x_cos> = [[#<array_x_offset> * #<blk_scale>] * COS[#<array_angle>]]\n')
            outNgc.write('#<y_sin> = [[#<array_y_offset> * #<blk_scale>] * SIN[#<array_angle>]]\n')
            outNgc.write('#<y_cos> = [[#<array_y_offset> * #<blk_scale>] * COS[#<array_angle>]]\n\n')
            # main loop
            outNgc.write(';main loop\n')
            outNgc.write('o<loop> while [#<this_row> LT #<array_rows>]\n')
            outNgc.write('    #<shape_x_start> = [[#<this_col> * #<x_cos>] - [#<this_row> * #<y_sin>] + #<blk_x_offset>]\n')
            outNgc.write('    #<shape_y_start> = [[#<this_row> * #<y_cos>] + [#<this_col> * #<x_sin>] + #<blk_y_offset>]\n')
            outNgc.write('    #<blk_angle> = [#<shape_angle> + #<array_angle> + #<ucs_r_offset>]\n')
            if P.convUnits[1]:
                outNgc.write('    {}\n'.format(P.convUnits[1]))
            outNgc.write('    G10 L2 P0 X#<shape_x_start> Y#<shape_y_start> R#<blk_angle>\n\n')
            # the shape
            started, ended = False, False
            for line in inCode:
                line = line.strip().lower()
                # remove line numbers
                if line.startswith('n'):
                    line = line[1:]
                    while line[0].isdigit() or line[0] == '.':
                        line = line[1:].lstrip()
                        if not line:
                            break
                # remove leading 0's from G & M codes
                elif (line.lower().startswith('g') or \
                   line.lower().startswith('m')) and \
                   len(line) > 2:
                    while line[1] == '0' and len(line) > 2:
                        if line[2].isdigit():
                            line = line[:1] + line[2:]
                        else:
                            break
                # scale the shape
                if len(line) and line[0] in 'gxy':
                    started = True
                    rLine = scale_shape(P, W, line)
                    if rLine is not None:
                        outNgc.write('    {}\n'.format(rLine))
                    else:
                        return
                # loop counter
                elif not ended and ('m2' in line or 'm30' in line or (line.startswith('%') and started)):
                    ended = True
                    outNgc.write('\n    #<this_col> = [#<this_col> + 1]\n')
                    outNgc.write('    o<count> if [#<this_col> EQ #<array_columns>]\n')
                    outNgc.write('        #<this_col> = 0\n')
                    outNgc.write('        #<this_row> = [#<this_row> + 1]\n')
                    outNgc.write('    o<count> endif\n')
                    outNgc.write('o<loop> endwhile\n')
                elif not line:
                    outNgc.write('\n')
                elif ended and ('m2' in line or 'm30' in line or line.startswith('%')):
                    pass
                else:
                    outNgc.write('    {}\n'.format(line))
            # reset offsets to original
            outNgc.write('\nG10 L2 P0 X[#<ucs_x_offset> * {0}] Y[#<ucs_y_offset> * {0}] R#<ucs_r_offset>\n'.format(P.convUnits[0]))
            outNgc.write('\nM2\n')
        inCode.close()
        outNgc.close()
        W.conv_preview.load(P.fNgc)
        W.conv_preview.set_current_view()
        W.add.setEnabled(True)
        W.undo.setEnabled(True)
        P.conv_preview_button(True)
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
            msg0 += '{}.\n\n'.format(m)
        error_set(P, W, msg0)
        return
    W.add.setEnabled(True)
    P.convBlock[0] = True
    P.convMirrorToggle = False
    P.convFlipToggle = False

def scale_shape(P, W, line):
    if line[0] == 'g' and (line[1] not in '0123' or (line[1] in '0123' and len(line) > 2 and line[2] in '0123456789')):
        return '{}'.format(line)
    if line[0] == 'g' and line[1] in '0123' and line[2] == 'z':
        return '{}'.format(line)
    newLine = ''
    multiAxis = False
    numParam = False
    namParam = False
    zAxis = False
    fWord = False
    lastAxis = ''
    offset = ''
    while 1:
        # remove spaces
        if line[0] == ' ':
            line = line[1:]
        # if beginning of comment
        if line[0] == '(' or line[0] == ';':
            if multiAxis and not zAxis and not fWord:
                if lastAxis == 'x':
                    newLine += '*#<blk_scale>*#<shape_mirror>]'
                elif lastAxis == 'i':
                    newLine += '*#<blk_scale>*#<shape_mirror>]'
                elif lastAxis == 'y':
                    newLine += '*#<blk_scale>*#<shape_flip>]'
                elif lastAxis == 'j':
                    newLine += '*#<blk_scale>*#<shape_flip>]'
                else:
                    newLine += '*#<blk_scale>]'
            newLine += line
            break
        # if beginning of parameter
        elif line[0] == 'p':
            if not numParam and not namParam:
                if multiAxis and not zAxis and not fWord:
                    if lastAxis == 'x':
                        newLine += '*#<blk_scale>*#<shape_mirror>]'
                    elif lastAxis == 'i':
                        newLine += '*#<blk_scale>*#<shape_mirror>]'
                    elif lastAxis == 'y':
                        newLine += '*#<blk_scale>*#<shape_flip>]'
                    elif lastAxis == 'j':
                        newLine += '*#<blk_scale>*#<shape_flip>]'
                    else:
                        newLine += '*#<blk_scale>]'
                lastAxis = line[0]
            newLine += line[0]
            line = line[1:]
        # if alpha character
        elif line[0].isalpha():
            if not numParam and not namParam:
                if multiAxis and not zAxis and not fWord:
                    if lastAxis == 'x':
                        newLine += '*#<blk_scale>*#<shape_mirror>]'
                    elif lastAxis == 'i':
                        newLine += '*#<blk_scale>*#<shape_mirror>]'
                    elif lastAxis == 'y':
                        newLine += '*#<blk_scale>*#<shape_flip>]'
                    elif lastAxis == 'j':
                        newLine += '*#<blk_scale>*#<shape_flip>]'
                    else:
#                    elif lastAxis not in 'p':
                        newLine += '*#<blk_scale>]'
                lastAxis = line[0]
                if line[0] == 'z':
                    zAxis = True
                else:
                    zAxis = False
                if line[0] == 'f':
                    fWord = True
            if not zAxis:
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
        #if last axis was x, y, i, j, or r
        elif newLine[-1] in 'xyijr' and not numParam and not namParam:
            multiAxis = True
            newLine += '[{}'.format(line[0])
            line = line[1:]
        # everything else
        else:
            if not zAxis:
                newLine += line[0]
            line = line[1:]
        # empty line, must be finished
        if not line:
            if not zAxis and not fWord:
                if lastAxis == 'x':
                    newLine += '*#<blk_scale>*#<shape_mirror>]'
                elif lastAxis == 'i':
                    newLine += '*#<blk_scale>*#<shape_mirror>]'
                elif lastAxis == 'y':
                    newLine += '*#<blk_scale>*#<shape_flip>]'
                elif lastAxis == 'j':
                    newLine += '*#<blk_scale>*#<shape_flip>]'
                elif lastAxis not in 'p':
                    newLine += '*#<blk_scale>]'
            break
    if '#<shape_mirror>' in newLine and (P.convMirrorToggle or P.convFlipToggle):
        if 'g2' in newLine:
            newLine = newLine.replace('g2', 'g3')
        elif 'g3' in newLine:
            newLine = newLine.replace('g3', 'g2')
    return ('{}'.format(newLine))

def mirror_shape(P, W):
    if P.convMirror == 1:
        P.convMirror = -1
    else:
        P.convMirror = 1
    P.convMirrorToggle = True
    preview(P, W)

def flip_shape(P, W):
    if P.convFlip == 1:
        P.convFlip = -1
    else:
        P.convFlip = 1
    P.convFlipToggle = True
    preview(P, W)

def undo_shape(P, W):
    P.convMirror = 1
    P.convMirrorToggle = False
    P.convFlip = 1
    P.convFlipToggle = False
    P.conv_undo_shape()

def get_parameters(P, W):
    P.wcs_rotation('get')
    inCode = open(P.fNgc, 'r')
    P.convBlock = [False, False]
    P.convUnits = [1, None]
    P.convMirror = 1
    P.convMirrorToggle = False
    P.convFlip = 1
    P.convFlipToggle = False
    for line in inCode:
        line = line.strip().lower()
        # maybe check here for old style rotate, scale, and array
        if line.startswith(';conversational block'):
            P.convBlock = [True, True]
        elif 'G21' in line.upper().replace(' ', '') and P.unitsPerMm != 1:
            P.convUnits = [25.4, 'G21']
        elif 'G20' in line.upper().replace(' ', '') and P.unitsPerMm == 1:
            P.convUnits = [0.03937, 'G20']
        elif 'm3' in line:
            break
    inCode.seek(0, 0)
    if P.convBlock[0]:
        for line in inCode:
            line = line.strip().lower()
            if line.startswith('#<array_x_offset>'):
                W.coEntry.setText('{:0.4f}'.format(float(line.split('=')[1].strip())))
            elif line.startswith('#<array_y_offset>'):
                W.roEntry.setText('{:0.4f}'.format(float(line.split('=')[1].strip())))
            elif line.startswith('#<array_columns>'):
                W.cnEntry.setText(line.split('=')[1].strip())
            elif line.startswith('#<array_rows>'):
                W.rnEntry.setText(line.split('=')[1].strip())
            elif line.startswith('#<origin_x_offset>'):
                W.oxEntry.setText('{:0.4f}'.format(float(line.split('=')[1].strip())))
            elif line.startswith('#<origin_y_offset>'):
                W.oyEntry.setText('{:0.4f}'.format(float(line.split('=')[1].strip())))
            elif line.startswith('#<array_angle>'):
                W.aEntry.setText(line.split('=')[1].strip())
            elif line.startswith('#<blk_scale>'):
                W.scEntry.setText(line.split('=')[1].strip())
            elif line.startswith('#<shape_angle>'):
                W.rtEntry.setText(line.split('=')[1].strip())
            elif line.startswith('#<shape_mirror>'):
                P.convMirror = int(line.split('=')[1].strip())
            elif line.startswith('#<shape_flip>'):
                P.convFlip = int(line.split('=')[1].strip())
            elif 'm3' in line:
                break
    inCode.seek(0, 0)

def error_set(P, W, msg):
    P.dialogError = True
    P.dialog_show_ok(QMessageBox.Warning, _translate('Conversational', 'Array Error'), msg)

def widgets(P, W):
    if not P.convSettingsChanged:
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
        W.aEntry = QLineEdit('0.0', objectName='aEntry')
        W.aLabel = QLabel(_translate('Conversational', 'ANGLE'))
        W.scLabel = QLabel(_translate('Conversational', 'SCALE'))
        W.scEntry = QLineEdit('1.0')
        W.rtEntry = QLineEdit('0.0', objectName='aEntry')
        W.rtLabel = QLabel(_translate('Conversational', 'ROTATION'))
        W.mirror = QPushButton(_translate('Conversational', 'MIRROR'))
        W.flip = QPushButton(_translate('Conversational', 'FLIP'))
        W.add = QPushButton(_translate('Conversational', 'ADD'))
        W.lDesc = QLabel(_translate('Conversational', 'CREATE ARRAY OF SHAPES'))
        W.shLabel = QLabel(_translate('Conversational', 'SHAPE'))
        #alignment and size
        rightAlign = ['cnLabel', 'cnEntry', 'coEntry', \
                      'rnLabel', 'rnEntry', 'roEntry', \
                      'oxLabel', 'oxEntry', 'oyEntry', \
                      'scLabel', 'aEntry', 'scEntry', \
                      'rtEntry']
        leftAlign = ['coLabel', 'roLabel', 'oyLabel', 'aLabel', 'rtLabel']
        centerAlign = ['lDesc', 'cLabel', 'rLabel', 'oLabel','shLabel']
        pButton = ['preview', 'add', 'undo', 'mirror', 'flip']
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
            if widget == 'lDesc':
                W[widget].setFixedWidth(240)
            else:
                W[widget].setFixedWidth(80)
            W[widget].setFixedHeight(24)
        for widget in pButton:
            W[widget].setFixedWidth(80)
            W[widget].setFixedHeight(24)
        #starting parameters
        W.add.setEnabled(False)
    #connections
    W.preview.pressed.disconnect()
    W.undo.pressed.disconnect()
    W.preview.pressed.connect(lambda:preview(P, W))
    W.add.pressed.connect(lambda:P.conv_accept())
    W.undo.pressed.connect(lambda:undo_shape(P, W))
    W.mirror.clicked.connect(lambda:mirror_shape(P, W, ))
    W.flip.clicked.connect(lambda:flip_shape(P, W))
    entries = ['cnEntry', 'coEntry', 'rnEntry', 'roEntry', 'oxEntry',
               'oyEntry', 'scEntry', 'aEntry', 'rtEntry']
    for entry in entries:
        W[entry].textChanged.connect(lambda:P.conv_entry_changed(W.sender()))
        W[entry].returnPressed.connect(lambda:preview(P, W))
    #add to layout
    if P.landscape:
        W.s0 = QLabel('')
        W.s0.setFixedHeight(51)
        W.entries.addWidget(W.s0, 0, 0)
        W.entries.addWidget(W.cLabel, 1, 2)
        W.entries.addWidget(W.cnLabel, 1, 0)
        W.entries.addWidget(W.cnEntry, 1, 1)
        W.entries.addWidget(W.coEntry, 1, 3)
        W.entries.addWidget(W.coLabel, 1, 4)
        W.entries.addWidget(W.rLabel, 3, 2)
        W.entries.addWidget(W.rnLabel, 3, 0)
        W.entries.addWidget(W.rnEntry, 3, 1)
        W.entries.addWidget(W.roEntry, 3, 3)
        W.entries.addWidget(W.roLabel, 3, 4)
        W.entries.addWidget(W.oLabel, 5, 2)
        W.entries.addWidget(W.oxLabel, 5, 0)
        W.entries.addWidget(W.oxEntry, 5, 1)
        W.entries.addWidget(W.oyEntry, 5, 3)
        W.entries.addWidget(W.oyLabel, 5, 4)
        W.entries.addWidget(W.aEntry, 7, 3)
        W.entries.addWidget(W.aLabel, 7, 4)
        W.entries.addWidget(W.scLabel, 9, 0)
        W.entries.addWidget(W.scEntry, 9, 1)
        W.entries.addWidget(W.shLabel, 9, 2)
        W.entries.addWidget(W.rtEntry, 9, 3)
        W.entries.addWidget(W.rtLabel, 9, 4)
        W.entries.addWidget(W.mirror, 11, 1)
        W.entries.addWidget(W.flip, 11, 3)
        for r in [2,4,6,8,10]:
            W['{}'.format(r)] = QLabel('')
            W['{}'.format(r)].setFixedHeight(24)
            W.entries.addWidget(W['{}'.format(r)], r, 0)
        W.entries.addWidget(W.preview, 12, 0)
        W.entries.addWidget(W.add, 12, 2)
        W.entries.addWidget(W.undo, 12, 4)
        W.entries.addWidget(W.lDesc, 13 , 1, 1, 3)
    else:
        W.entries.addWidget(W.cLabel, 0, 2)
        W.entries.addWidget(W.cnLabel, 0, 0)
        W.entries.addWidget(W.cnEntry, 0, 1)
        W.entries.addWidget(W.coEntry, 0, 3)
        W.entries.addWidget(W.coLabel, 0, 4)
        W.entries.addWidget(W.rLabel, 2, 2)
        W.entries.addWidget(W.rnLabel, 2, 0)
        W.entries.addWidget(W.rnEntry, 2, 1)
        W.entries.addWidget(W.roEntry, 2, 3)
        W.entries.addWidget(W.roLabel, 2, 4)
        W.entries.addWidget(W.oLabel, 4, 2)
        W.entries.addWidget(W.oxLabel, 4, 0)
        W.entries.addWidget(W.oxEntry, 4, 1)
        W.entries.addWidget(W.oyEntry, 4, 3)
        W.entries.addWidget(W.oyLabel, 4, 4)
        W.entries.addWidget(W.aEntry, 6, 3)
        W.entries.addWidget(W.aLabel, 6, 4)
        W.entries.addWidget(W.scLabel, 8, 0)
        W.entries.addWidget(W.scEntry, 8, 1)
        W.entries.addWidget(W.shLabel, 8, 2)
        W.entries.addWidget(W.rtEntry, 8, 3)
        W.entries.addWidget(W.rtLabel, 8, 4)
        W.entries.addWidget(W.mirror, 9, 1)
        W.entries.addWidget(W.flip, 9, 3)
        for r in [1,3,5,7]:
            W['{}'.format(r)] = QLabel('')
            W['{}'.format(r)].setFixedHeight(24)
            W.entries.addWidget(W['{}'.format(r)], r, 0)
        W.entries.addWidget(W.preview, 10, 0)
        W.entries.addWidget(W.add, 10, 2)
        W.entries.addWidget(W.undo, 10, 4)
    W.cnEntry.setFocus()
    P.convSettingsChanged = False
    get_parameters(P, W)
