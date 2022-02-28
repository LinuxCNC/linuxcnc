'''
run_from_line.py

Copyright (C) 2019, 2020, 2021  Phillip A Carter
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
with this program; if not, write to the Free Software Foundation, Inc
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
'''

import math
from PyQt5.QtCore import Qt, QCoreApplication
from PyQt5.QtWidgets import QDialog, QLabel, QCheckBox, QDoubleSpinBox, QDialogButtonBox, QGridLayout
from PyQt5.QtGui import QIcon

_translate = QCoreApplication.translate

def run_from_line(P, W, ACTION, STATUS, linuxcnc):
    inData,outData,newFile,params = [],[],[],[]
    g2,g4,g6,g9,g9arc,d3,d2,a3,material,x,y,code,rflSpindle = '','','','','','','','','','','','',''
    oSub = False
    count = 0
    tmpMat = False
    head = _translate('HandlerClass', 'GCODE ERROR')
    with open(P.lastLoadedProgram, 'r') as inFile:
        for line in inFile:
            if count < P.startLine:
                inData.append(line.lower())
            else:
                if count == P.startLine:
                    if 'g21' in line:
                        newFile.append('g21')
                    elif 'g20' in line:
                        newFile.append('g20')
                outData.append(line.lower())
            count += 1
    cutComp = False
    for line in inData:
        if line.startswith('('):
            if line.startswith('(o='):
                material = line.strip()
            continue
        if line.startswith('#'):
            params.append(line.strip())
            continue
        if line.startswith('m190'):
            mat = line.split('p')[1]
            try:
                if '(' in mat:
                    num = int(mat.split('(')[0])
                else:
                    num = int(mat)
            except:
                head = _translate('HandlerClass', 'G-CODE ERROR')
                msg0 = _translate('HandlerClass', 'is an invalid material number')
                msg1 = _translate('HandlerClass', 'Material #0 will be selected')
                STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n"{}" {}\n{}\n'.format(head, mat, msg0, msg1))
                num = 0
            if num >= 1000000:
                tmpMat = True
            else:
                material = line.strip()
            continue
        if line.replace(' ','').startswith('m66p3') and tmpMat:
            tmpMat = False
            continue
        if line.replace(' ','').startswith('m67e3'):
            continue
        for t1 in ['g20','g21','g40','g41.1','g42.1','g61','g61.1','g64','g90','g90.1','g91','g91.1']:
            if t1 in line:
                if t1[1] == '2':
                    g2 = t1
                elif t1[1] == '4':
                    g4 = t1
                    if t1 != 'g40':
                        cutComp = True
                    else:
                        cutComp = False
                elif t1[1] == '6':
                    g6 = t1
                    if t1 == 'g64':
                        tmp = line.split('64')[1]
                        if tmp[0] == 'p':
                            p = ''
                            tmp = tmp[1:]
                            while 1:
                                if tmp[0] in '.0123456789q':
                                    p += tmp[0]
                                    tmp = tmp[1:]
                                else:
                                    break
                            g6 = 'g64p{}'.format(p)
                elif t1 == 'g90' and not 'g90.1' in line:
                    g9 = 'g90'
                elif t1 == 'g91' and not 'g91.1' in line:
                    g9 = 'g91'
                elif t1 == 'g90.1' in line:
                    g9arc = 'g90.1'
                elif t1 == 'g91.1' in line:
                    g9arc = 'g91.1'
        if 'g0' in line:
            code = 'g0'
        if 'g1' in line:
            tmp = line.split('g1')[1]
            if tmp[0] not in '0123456789':
                code = 'g1'
        if 'g2' in line:
            tmp = line.split('g2')[1]
            if tmp[0] not in '0123456789':
                code = 'g2'
        if 'g3' in line:
            tmp = line.split('g3')[1]
            if tmp[0] not in '0123456789':
                code = 'g3'
        if 'x' in line:
            x = get_rfl_pos(line.strip(), x, 'x')
        if 'y' in line:
            y = get_rfl_pos(line.strip(), y, 'y')
        if 'm3' in line:
            rflSpindle = 'm3'
            tmp = line.split('m3')[1]
            while 1:
                if tmp[0] in '0123456789s$':
                    rflSpindle += tmp[0]
                    tmp = tmp[1:]
                else:
                    break
        if 'm5' in line:
            rflSpindle = ''
        if 'm62p3' in line:
            d3 = 'm62p3 (Disable Torch)'
        elif 'm63p3' in line:
            d3 = 'm63p3 (Enable Torch)'
        elif 'm64p3' in line:
            d3 = 'm64p3 (Disable Torch)'
        elif 'm65p3' in line:
            d3 = 'm65p3 (Enable Torch)'
        if 'm62p2' in line:
            d2 = 'm62p2 (Disable THC)'
        elif 'm63p2' in line:
            d2 = 'm63p2 (Enable THC)'
        elif 'm64p2' in line:
            d2 = 'm64p2 (Disable THC)'
        elif 'm65p2' in line:
            d2 = 'm65p2 (Enable THC)'
        if 'm67e3q' in line:
            a3 = 'm67e3q'
            tmp = line.split('m67e3q')[1]
            while 1:
                if tmp[0] in '-.0123456789':
                    a3 += tmp[0]
                    tmp = tmp[1:]
                else:
                    break
            a3 += ' (Velocity {}%)'.format(a3.split('m67e3q')[1])
        if 'm68e3q' in line:
            a3 = 'm68e3q'
            tmp = line.split('m68e3q')[1]
            bb=1
            while 1:
                if tmp[0] in '-.0123456789':
                    a3 += tmp[0]
                    tmp = tmp[1:]
                else:
                    break
            a3 += ' (Velocity {}%)'.format(a3.split('m68e3q')[1])
        if line.startswith('o'):
            if 'end' in line:
                oSub = False
            else:
                oSub = True
    if cutComp or oSub:
        if cutComp:
            msg0 = _translate('HandlerClass', 'Cannot run from line while')
            msg1 = _translate('HandlerClass', 'cutter compensation is active')
        elif oSub:
            msg0 = _translate('HandlerClass', 'Cannot do run from line')
            msg1 = _translate('HandlerClass', 'inside a subroutine')
        STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n{}\n'.format(head, msg0, msg1))
        P.rflActive = False
        P.set_run_button_state()
        P.startLine = 0
        return
    # show the dialog
    rFl = QDialog(W)
    rFl.setWindowTitle(_translate('HandlerClass', 'RUN FROM LINE'))
    lbl1 = QLabel(_translate('HandlerClass', 'USE LEADIN:'))
    lbl2 = QLabel(_translate('HandlerClass', 'LEADIN LENGTH:'))
    lbl3 = QLabel(_translate('HandlerClass', 'LEADIN ANGLE:'))
    lbl4 = QLabel('')
    leadinDo = QCheckBox()
    leadinLength = QDoubleSpinBox()
    leadinAngle = QDoubleSpinBox()
    buttons = QDialogButtonBox.Ok | QDialogButtonBox.Cancel
    buttonBox = QDialogButtonBox(buttons)
    buttonBox.accepted.connect(rFl.accept)
    buttonBox.rejected.connect(rFl.reject)
    buttonBox.button(QDialogButtonBox.Ok).setText(_translate('HandlerClass', 'LOAD'))
    buttonBox.button(QDialogButtonBox.Ok).setIcon(QIcon())
    buttonBox.button(QDialogButtonBox.Cancel).setText(_translate('HandlerClass', 'CANCEL'))
    buttonBox.button(QDialogButtonBox.Cancel).setIcon(QIcon())
    layout = QGridLayout()
    layout.addWidget(lbl1, 0, 0)
    layout.addWidget(lbl2, 1, 0)
    layout.addWidget(lbl3, 2, 0)
    layout.addWidget(lbl4, 3, 0)
    layout.addWidget(leadinDo, 0, 1)
    layout.addWidget(leadinLength, 1, 1)
    layout.addWidget(leadinAngle, 2, 1)
    layout.addWidget(buttonBox, 4, 0, 1, 2)
    rFl.setLayout(layout)
    lbl1.setAlignment(Qt.AlignRight | Qt.AlignBottom)
    lbl2.setAlignment(Qt.AlignRight | Qt.AlignBottom)
    lbl3.setAlignment(Qt.AlignRight | Qt.AlignBottom)
    if P.units == 'inch':
        leadinLength.setDecimals(2)
        leadinLength.setSingleStep(0.05)
        leadinLength.setSuffix(' inch')
        leadinLength.setMinimum(0.05)
    else:
        leadinLength.setDecimals(0)
        leadinLength.setSingleStep(1)
        leadinLength.setSuffix(' mm')
        leadinLength.setMinimum(1)
    leadinAngle.setDecimals(0)
    leadinAngle.setSingleStep(1)
    leadinAngle.setSuffix(' deg')
    leadinAngle.setRange(-359, 359)
    leadinAngle.setWrapping(True)
    result = rFl.exec_()
    # cancel from dialog
    if not result:
        P.rflActive = False
        P.set_run_button_state()
        P.startLine = 0
        W.gcode_display.setCursorPosition(0, 0)
        return
    # run from dialog
    for param in params:
        if param:
            newFile.append(param)
    scale = 1
    zMax = ''
    if P.unitsPerMm == 1:
        if g2 == 'g20':
            scale = 0.03937
            zMax = 'g53 g0z[[#<_ini[axis_z]max_limit> - 5] * 0.03937]'
        else:
            zMax = 'g53 g0z[#<_ini[axis_z]max_limit> - 5]'
    elif P.unitsPerMm == 0.03937:
        if g2 == 'g21':
            scale = 25.4
            zMax = 'g53 g0z[[#<_ini[axis_z]max_limit> * 25.4] - 5]'
        else:
            zMax = 'g53 g0z[#<_ini[axis_z]max_limit> - 0.02]'
    if g2:
        newFile.append(g2)
    if g4:
        newFile.append(g4)
    if g6:
        newFile.append(g6)
    if g9:
        newFile.append(g9)
    if g9arc:
        newFile.append(g9arc)
    newFile.append('M52 P1')
    if d3:
        newFile.append(d3)
    if d2:
        newFile.append(d2)
    if a3:
        newFile.append(a3)
    if zMax:
        newFile.append(zMax)
    if material:
        newFile.append(material)
        if not '(o=' in material:
            newFile.append('m66p3l3q1')
    # don't scale feedrate, parameters should be set correctly in material file
    newFile.append('f#<_hal[plasmac.cut-feed-rate]>')
    xL = x
    yL = y
    try:
        if leadinDo.isChecked():
            if x[-1] == ']':
                xL = '{}[[{}]+{:0.6f}]'.format(x[:1], x[1:], (leadinLength.value() * scale) * math.cos(math.radians(leadinAngle.value())))
                yL = '{}[[{}]+{:0.6f}]'.format(y[:1], y[1:], (leadinLength.value() * scale) * math.sin(math.radians(leadinAngle.value())))
            else:
                xL = float(x) + ((leadinLength.value() * scale) * math.cos(math.radians(leadinAngle.value())))
                yL = float(y) + ((leadinLength.value() * scale) * math.sin(math.radians(leadinAngle.value())))
    except:
        msg0 = _translate('HandlerClass', 'Unable to calculate a leadin for this cut')
        msg1 = _translate('HandlerClass', 'Program will run from selected line with no leadin applied')
        STATUS.emit('error', linuxcnc.OPERATOR_ERROR, '{}:\n{}\n{}\n'.format(head, msg0, msg1))
    if xL != x and yL != y:
        newFile.append('G0 X{} Y{}'.format(xL, yL))
        rflLead = [x, y]
    else:
        if x and y:
            newFile.append('G0 X{} Y{}'.format(x, y))
        elif x:
            newFile.append('G0 X{}'.format(x))
        elif y:
            newFile.append('G0 Y{}'.format(y))
        rflLead = None
    if rflSpindle:
        newFile.append(rflSpindle)
    if rflLead:
        newFile.append('G1 X{} Y{}'.format(rflLead[0], rflLead[1]))
    for line in outData:
        if outData.index(line) == 0 and (line.startswith('x') or line.startswith('y')):
            line = '{}{}'.format(code, line)
        elif line.startswith('m190'):
            mat = line.split('p')[1]
            if '(' in mat:
                num = int(mat.split('(')[0])
            else:
                num = int(mat)
            if num >= 1000000:
                tmpMat = True
                continue
        elif line.replace(' ','').startswith('m66p3') and tmpMat:
            tmpMat = False
            continue
        elif line.replace(' ','').startswith('m67e3'):
            continue
        elif line.replace(' ','').startswith('m68e3'):
            continue
        newFile.append(line.strip())
    rflFile = '{}rfl.ngc'.format(P.tmpPath)
    with open(rflFile, 'w') as outFile:
        for line in newFile:
            outFile.write('{}\n'.format(line))
    if ACTION.prefilter_path or P.lastLoadedProgram != 'None':
        P.preRflFile = ACTION.prefilter_path or P.lastLoadedProgram
    ACTION.OPEN_PROGRAM(rflFile)
    ACTION.prefilter_path = P.preRflFile
    P.set_run_button_state()
    txt0 = _translate('HandlerClass', 'RUN FROM LINE')
    txt1 = _translate('HandlerClass', 'CYCLE START')
    P.runText = '{}\n{}'.format(txt0, txt1)
    W.gcodegraphics.highlight_graphics(None)

def get_rfl_pos(line, axisPos, axisLetter):
    maths = 0
    pos = ''
    done = False
    if line.startswith('(') or line.startswith(';'):
        return pos if pos else axisPos
    while len(line):
        if line[0] == ('('):
            break
        if not line[0] == axisLetter:
            line = line[1:]
        else:
            while 1:
                line = line[1:]
                if line[0] in '-.0123456789#':
                    pos += line[0]
                elif line[0] == '[' or line[0] == '<':
                    pos += line[0]
                    maths += 1
                elif (line[0] == ']' or line[0] == '>') and maths > 0:
                    pos += line[0]
                    maths -= 1
                elif maths:
                    pos += line[0]
                elif (pos and not maths) or line[0] == '(':
                    done = True
                    break
                else:
                    if len(line) == 1: break
                    break
                if len(line) == 1:
                    break
        if done:
            break
    return pos if pos else axisPos

