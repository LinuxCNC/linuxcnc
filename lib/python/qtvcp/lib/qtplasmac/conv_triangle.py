'''
conv_triangle.py

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
from PyQt5.QtCore import Qt, QCoreApplication
from PyQt5.QtWidgets import QLabel, QLineEdit, QPushButton, QRadioButton, QButtonGroup, QMessageBox
from PyQt5.QtGui import QPixmap

_translate = QCoreApplication.translate

def preview(P, W):
    if P.dialogError: return
    try:
        kOffset = float(W.kerf_width.value()) * W.kOffset.isChecked() / 2
    except:
        msg0 += _translate('Conversational', 'Kerf Width entry in material')
        error_set(P, '{}.\n'.format(msg0))
        return
    msg = []
    if not W.xsEntry.text():
        W.xsEntry.setText('{:0.3f}'.format(P.xOrigin))
    if not W.ysEntry.text():
        W.ysEntry.setText('{:0.3f}'.format(P.yOrigin))
    try:
        if W.cExt.isChecked():
            xBPoint = float(W.xsEntry.text()) + kOffset
            yBPoint = float(W.ysEntry.text()) + kOffset
        else:
            xBPoint = float(W.xsEntry.text()) - kOffset
            yBPoint = float(W.ysEntry.text()) - kOffset
    except:
        text1 = _translate('Conversational', 'or')
        text2 = _translate('Conversational', 'ORIGIN')
        msg.append(_translate('Conversational', 'X {} Y {}'.format(text1, text2)))
    try:
        if W.angEntry.text():
            angle = math.radians(float(W.angEntry.text()))
        else:
            angle = 0.0
    except:
        msg.append(_translate('Conversational', 'ANGLE'))
    try:
        if W.liEntry.text():
            leadInOffset = math.sin(math.radians(45)) * float(W.liEntry.text())
        else:
            leadInOffset = 0
    except:
        msg.append(_translate('Conversational', 'LEAD IN'))
    try:
        if W.loEntry.text():
            leadOutOffset = math.sin(math.radians(45)) * float(W.loEntry.text())
        else:
            leadOutOffset = 0
    except:
        msg.append(_translate('Conversational', 'LEAD OUT'))
    done = False
    a = b = c = A = B = C = 0
    text = _translate('Conversational', 'LENGTH')
    try:
        if W.aEntry.text():
            a = float(W.aEntry.text())
    except:
        msg.append(_translate('Conversational', 'a {}'.format(text)))
    try:
        if W.bEntry.text():
            b = float(W.bEntry.text())
    except:
        msg.append(_translate('Conversational', 'b {}'.format(text)))
    try:
        if W.cEntry.text():
            c = float(W.cEntry.text())
    except:
        msg.append(_translate('Conversational', 'c {}'.format(text)))
    text = _translate('Conversational', 'ANGLE')
    try:
        if W.AEntry.text():
            A = math.radians(float(W.AEntry.text()))
    except:
        msg.append(_translate('Conversational', 'A {}'.format(text)))
    try:
        if W.BEntry.text():
            B = math.radians(float(W.BEntry.text()))
    except:
        msg.append(_translate('Conversational', 'B {}'.format(text)))
    try:
        if W.CEntry.text():
            C = math.radians(float(W.CEntry.text()))
    except:
        msg.append(_translate('Conversational', 'C {}'.format(text)))
    if msg:
        msg0 = _translate('Conversational', 'Invalid entry detected in')
        msg1 = ''
        for m in msg:
            msg1 += '{}\n'.format(m)
        error_set(P, '{}:\n\n{}'.format(msg0, msg1))
        return
    if a and b and c:
        B = math.acos((a ** 2 + c ** 2 - b ** 2) / (2 * a * c))
        done = True
    if not done and a and B and c:
        done = True
    if not done and a and b and C:
        c = math.sqrt((a ** 2 + b ** 2) - 2 * a * b * math.cos(C))
        B = math.acos((a ** 2 + c ** 2 - b ** 2) / (2 * a * c))
        done = True
    if not done and A and b and c:
        a = math.sqrt((b ** 2 + c ** 2) - 2 * b * c * math.cos(A))
        B = math.acos((a ** 2 + c ** 2 - b ** 2) / (2 * a * c))
        done = True
    if not done and A and B and C:
        if A + B + C == math.radians(180):
            if a:
                c = a / math.sin(A) * math.sin(C)
                done = True
            elif b:
                a = b / math.sin(B) * math.sin(A)
                c = b / math.sin(B) * math.sin(C)
                done = True
            elif c:
                a = c / math.sin(C) * math.sin(A)
                done = True
    if done:
        right = math.radians(0)
        up = math.radians(90)
        left = math.radians(180)
        down = math.radians(270)
        xCPoint = xBPoint + a * math.cos(angle)
        yCPoint = yBPoint + a * math.sin(angle)
        xAPoint = xBPoint + c * math.cos(angle + B)
        yAPoint = yBPoint + c * math.sin(angle + B)
        hypotLength = math.sqrt((xAPoint - xCPoint) ** 2 + (yAPoint - yCPoint) ** 2)
        if xAPoint <= xCPoint:
            hypotAngle = left - math.atan((yAPoint - yCPoint) / (xCPoint - xAPoint))
        else:
            hypotAngle = right - math.atan((yAPoint - yCPoint) / (xCPoint - xAPoint))
        xS = xCPoint + (hypotLength / 2) * math.cos(hypotAngle)
        yS = yCPoint + (hypotLength / 2) * math.sin(hypotAngle)
        if W.cExt.isChecked():
            if yAPoint >= yBPoint:
                dir = [up, right]
            else:
                dir = [down, left]
        else:
            if yAPoint >= yBPoint:
                dir = [down, left]
            else:
                dir = [up, right]
        outTmp = open(P.fTmp, 'w')
        outNgc = open(P.fNgc, 'w')
        inWiz = open(P.fNgcBkp, 'r')
        for line in inWiz:
            if '(new conversational file)' in line:
                outNgc.write('\n{} (preamble)\n'.format(P.preAmble))
                break
            elif '(postamble)' in line:
                break
            elif 'm2' in line.lower() or 'm30' in line.lower():
                break
            outNgc.write(line)
        outTmp.write('\n(conversational triangle)\n')
        outTmp.write('M190 P{}\n'.format(int(W.conv_material.currentText().split(':')[0])))
        outTmp.write('M66 P3 L3 Q1\n')
        outTmp.write('f#<_hal[plasmac.cut-feed-rate]>\n')
        if leadInOffset > 0:
            xlCentre = xS + (leadInOffset * math.cos(hypotAngle - dir[0]))
            ylCentre = yS + (leadInOffset * math.sin(hypotAngle - dir[0]))
            xlStart = xlCentre + (leadInOffset * math.cos(hypotAngle - dir[1]))
            ylStart = ylCentre + (leadInOffset * math.sin(hypotAngle - dir[1]))
            outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xlStart, ylStart))
            if W.kOffset.isChecked():
                outTmp.write('g41.1 d#<_hal[qtplasmac.kerf_width-f]>\n')
            outTmp.write('m3 $0 s1\n')
            outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xS, yS , xlCentre - xlStart, ylCentre - ylStart))
        else:
            outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xS, yS))
            outTmp.write('m3 $0 s1\n')
        if W.cExt.isChecked():
            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xCPoint , yCPoint))
            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xBPoint , yBPoint))
            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xAPoint , yAPoint))
        else:
            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xAPoint , yAPoint))
            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xBPoint , yBPoint))
            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xCPoint , yCPoint))
        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xS, yS))
        if leadOutOffset > 0:
            if W.cExt.isChecked():
                if yAPoint >= yBPoint:
                    dir = [up, left]
                else:
                    dir = [down, right]
            else:
                if yAPoint >= yBPoint:
                    dir = [down, right]
                else:
                    dir = [up, left]
            xlCentre = xS + (leadOutOffset * math.cos(hypotAngle - dir[0]))
            ylCentre = yS + (leadOutOffset * math.sin(hypotAngle - dir[0]))
            xlEnd = xlCentre + (leadOutOffset * math.cos(hypotAngle - dir[1]))
            ylEnd = ylCentre + (leadOutOffset * math.sin(hypotAngle - dir[1]))
            outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xlEnd, ylEnd , xlCentre - xS, ylCentre - yS))
        outTmp.write('g40\n')
        outTmp.write('m5 $0\n')
        outTmp.close()
        outTmp = open(P.fTmp, 'r')
        for line in outTmp:
            outNgc.write(line)
        outTmp.close()
        outNgc.write('\n{} (postamble)\n'.format(P.postAmble))
        outNgc.write('m2\n')
        outNgc.close()
        W.conv_preview.load(P.fNgc)
        W.conv_preview.set_current_view()
        W.add.setEnabled(True)
        W.undo.setEnabled(True)
        P.conv_preview_button(True)
    else:
        if A != 0 and B != 0 and C != 0 and A + B + C != math.radians(180):
            msg0 = _translate('Conversational', 'A + B + C must equal 180')
        else:
            msg0 = 'Minimum requirements are:\n\n'\
                                     'a + b + c\n\n'\
                                     'or\n\n'\
                                     'a + b + C\n\n'\
                                     'or\n\n'\
                                     'a + B + c\n\n'\
                                     'or\n\n'\
                                     'A + b + c\n\n'\
                                     'or\n\n'\
                                     'A + B + C + (a or b or c)'
        error_set(P, msg0)

def error_set(P, msg):
    P.dialogError = True
    P.dialog_show_ok(QMessageBox.Warning, _translate('Conversational', 'Triangle Error'), msg)

def entry_changed(P, W, widget):
    char = P.conv_entry_changed(widget)
    msg = []
    try:
        li = float(W.liEntry.text())
    except:
        msg.append(_translate('Conversational', 'LEADIN'))
    try:
        kw = float(W.kerf_width.value())
    except:
        msg.append(_translate('Conversational', 'KERF'))
    if msg:
        msg0 = _translate('Conversational', 'Invalid entry detected in')
        msg1 = ''
        for m in msg:
            msg1 += '{}\n'.format(m)
        error_set(P, '{}:\n\n{}'.format(msg0, msg1))
        return
    if char == "operator" or not W.liEntry.text() or li == 0 or li <= kw / 2:
        W.kOffset.setEnabled(False)
        W.kOffset.setChecked(False)
    else:
        W.kOffset.setEnabled(True)

def auto_preview(P, W):
    if W.main_tab_widget.currentIndex() == 1 and \
       (W.AEntry.text() and W.BEntry.text() and W.CEntry.text() and \
       (W.aEntry.text() or W.bEntry.text() or W.cEntry.text())) or \
       (W.AEntry.text() and W.bEntry.text() and W.cEntry.text()) or \
       (W.aEntry.text() and W.BEntry.text() and W.cEntry.text()) or \
       (W.aEntry.text() and W.bEntry.text() and W.CEntry.text()) or \
       (W.aEntry.text() and W.bEntry.text() and W.cEntry.text()):
        preview(P, W)

def widgets(P, W):
    W.liLabel = QLabel(_translate('Conversational', 'LEAD IN'))
    W.liEntry = QLineEdit(str(P.leadIn), objectName = 'liEntry')
    W.loLabel = QLabel(_translate('Conversational', 'LEAD OUT'))
    W.loEntry = QLineEdit(str(P.leadOut), objectName = 'loEntry')
    if not P.convSettingsChanged:
        #widgets
        W.ctLabel = QLabel(_translate('Conversational', 'CUT TYPE'))
        W.ctGroup = QButtonGroup(W)
        W.cExt = QRadioButton(_translate('Conversational', 'EXTERNAL'))
        W.cExt.setChecked(True)
        W.ctGroup.addButton(W.cExt)
        W.cInt = QRadioButton(_translate('Conversational', 'INTERNAL'))
        W.ctGroup.addButton(W.cInt)
        W.koLabel = QLabel(_translate('Conversational', 'KERF'))
        W.kOffset = QPushButton(_translate('Conversational', 'OFFSET'))
        W.kOffset.setCheckable(True)
        text = _translate('Conversational', 'ORIGIN')
        W.xsLabel = QLabel(_translate('Conversational', 'X {}'.format(text)))
        W.xsEntry = QLineEdit(str(P.xSaved), objectName = 'xsEntry')
        W.ysLabel = QLabel(_translate('Conversational', 'Y {}'.format(text)))
        W.ysEntry = QLineEdit(str(P.ySaved), objectName = 'ysEntry')
        text = _translate('Conversational', 'ANGLE')
        W.ALabel = QLabel(_translate('Conversational', 'A {}'.format(text)))
        W.AEntry = QLineEdit()
        W.BLabel = QLabel(_translate('Conversational', 'B {}'.format(text)))
        W.BEntry = QLineEdit()
        W.CLabel = QLabel(_translate('Conversational', 'C {}'.format(text)))
        W.CEntry = QLineEdit()
        text = _translate('Conversational', 'LENGTH')
        W.aLabel = QLabel(_translate('Conversational', 'a {}'.format(text)))
        W.aEntry = QLineEdit()
        W.bLabel = QLabel(_translate('Conversational', 'b {}'.format(text)))
        W.bEntry = QLineEdit()
        W.cLabel = QLabel(_translate('Conversational', 'c {}'.format(text)))
        W.cEntry = QLineEdit()
        W.angLabel = QLabel(_translate('Conversational', 'ANGLE'))
        W.angEntry = QLineEdit('0.0', objectName='aEntry')
    W.add = QPushButton(_translate('Conversational', 'ADD'))
    W.lDesc = QLabel(_translate('Conversational', 'CREATING TRIANGLE'))
    W.iLabel = QLabel()
    pixmap = QPixmap('{}conv_triangle_l.png'.format(P.IMAGES)).scaledToWidth(196)
    W.iLabel.setPixmap(pixmap)
    #alignment and size
    rightAlign = ['ctLabel', 'koLabel', 'xsLabel', 'xsEntry', 'ysLabel', 'ysEntry', \
                  'liLabel', 'liEntry', 'loLabel', 'loEntry', 'ALabel', 'AEntry', \
                  'BLabel', 'BEntry', 'CLabel', 'CEntry', 'aLabel', 'aEntry',
                  'bLabel', 'bEntry', 'cLabel', 'cEntry', 'angLabel', 'angEntry']
    centerAlign = ['lDesc']
    rButton = ['cExt', 'cInt']
    pButton = ['preview', 'add', 'undo', 'kOffset']
    for widget in rightAlign:
        W[widget].setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        W[widget].setFixedWidth(80)
        W[widget].setFixedHeight(24)
    for widget in centerAlign:
        W[widget].setAlignment(Qt.AlignCenter | Qt.AlignBottom)
        W[widget].setFixedWidth(240)
        W[widget].setFixedHeight(24)
    for widget in rButton:
        W[widget].setFixedWidth(80)
        W[widget].setFixedHeight(24)
    for widget in pButton:
        W[widget].setFixedWidth(80)
        W[widget].setFixedHeight(24)
    #starting parameters
    W.add.setEnabled(False)
    if not W.liEntry.text() or float(W.liEntry.text()) == 0:
        W.kOffset.setChecked(False)
        W.kOffset.setEnabled(False)
    #connections
    W.preview.pressed.disconnect()
    W.undo.pressed.disconnect()
    W.conv_material.currentTextChanged.connect(lambda:auto_preview(P, W))
    W.cExt.toggled.connect(lambda:auto_preview(P, W))
    W.kOffset.toggled.connect(lambda:auto_preview(P, W))
    W.preview.pressed.connect(lambda:preview(P, W))
    W.add.pressed.connect(lambda:P.conv_add_shape_to_file())
    W.undo.pressed.connect(lambda:P.conv_undo_shape())
    entries = ['xsEntry', 'ysEntry', 'liEntry', 'loEntry', 'AEntry', 'BEntry', \
               'CEntry', 'aEntry', 'bEntry', 'cEntry', 'angEntry']
    for entry in entries:
        W[entry].textChanged.connect(lambda:entry_changed(P, W, W.sender()))
        W[entry].returnPressed.connect(lambda:preview(P, W))
    #add to layout
    if P.landscape:
        W.entries.addWidget(W.ctLabel, 0, 0)
        W.entries.addWidget(W.cExt, 0, 1)
        W.entries.addWidget(W.cInt, 0, 2)
        W.entries.addWidget(W.koLabel, 0, 3)
        W.entries.addWidget(W.kOffset, 0, 4)
        W.entries.addWidget(W.xsLabel, 1, 0)
        W.entries.addWidget(W.xsEntry, 1, 1)
        W.entries.addWidget(W.ysLabel, 2, 0)
        W.entries.addWidget(W.ysEntry, 2, 1)
        W.entries.addWidget(W.liLabel, 3, 0)
        W.entries.addWidget(W.liEntry, 3, 1)
        W.entries.addWidget(W.loLabel, 4, 0)
        W.entries.addWidget(W.loEntry, 4, 1)
        W.entries.addWidget(W.ALabel, 5, 0)
        W.entries.addWidget(W.AEntry, 5, 1)
        W.entries.addWidget(W.BLabel, 6, 0)
        W.entries.addWidget(W.BEntry, 6, 1)
        W.entries.addWidget(W.CLabel, 7, 0)
        W.entries.addWidget(W.CEntry, 7, 1)
        W.entries.addWidget(W.aLabel, 8, 0)
        W.entries.addWidget(W.aEntry, 8, 1)
        W.entries.addWidget(W.bLabel, 9, 0)
        W.entries.addWidget(W.bEntry, 9, 1)
        W.entries.addWidget(W.cLabel, 10, 0)
        W.entries.addWidget(W.cEntry, 10, 1)
        W.entries.addWidget(W.angLabel, 11, 0)
        W.entries.addWidget(W.angEntry, 11, 1)
        W.entries.addWidget(W.preview, 12, 0)
        W.entries.addWidget(W.add, 12, 2)
        W.entries.addWidget(W.undo, 12, 4)
        W.entries.addWidget(W.lDesc, 13 , 1, 1, 3)
        W.entries.addWidget(W.iLabel, 2 , 2, 7, 3)
    else:
        W.entries.addWidget(W.conv_material, 0, 0, 1, 5)
        W.entries.addWidget(W.ctLabel, 1, 0)
        W.entries.addWidget(W.cExt, 1, 1)
        W.entries.addWidget(W.cInt, 1, 2)
        W.entries.addWidget(W.koLabel, 1, 3)
        W.entries.addWidget(W.kOffset, 1, 4)
        W.entries.addWidget(W.xsLabel, 2, 0)
        W.entries.addWidget(W.xsEntry, 2, 1)
        W.entries.addWidget(W.ysLabel, 2, 2)
        W.entries.addWidget(W.ysEntry, 2, 3)
        W.entries.addWidget(W.liLabel, 3, 0)
        W.entries.addWidget(W.liEntry, 3, 1)
        W.entries.addWidget(W.loLabel, 3, 2)
        W.entries.addWidget(W.loEntry, 3, 3)
        W.entries.addWidget(W.ALabel, 4, 0)
        W.entries.addWidget(W.AEntry, 4, 1)
        W.entries.addWidget(W.BLabel, 4, 2)
        W.entries.addWidget(W.BEntry, 4, 3)
        W.entries.addWidget(W.CLabel, 5, 0)
        W.entries.addWidget(W.CEntry, 5, 1)
        W.entries.addWidget(W.aLabel, 6, 0)
        W.entries.addWidget(W.aEntry, 6, 1)
        W.entries.addWidget(W.bLabel, 6, 2)
        W.entries.addWidget(W.bEntry, 6, 3)
        W.entries.addWidget(W.cLabel, 7, 0)
        W.entries.addWidget(W.cEntry, 7, 1)
        W.entries.addWidget(W.angLabel, 8, 0)
        W.entries.addWidget(W.angEntry, 8, 1)
        W.entries.addWidget(W.preview, 9, 0)
        W.entries.addWidget(W.add, 9, 2)
        W.entries.addWidget(W.undo, 9, 4)
        W.entries.addWidget(W.lDesc, 10 , 1, 1, 3)
        W.entries.addWidget(W.iLabel, 0 , 5, 7, 3)
    W.AEntry.setFocus()
    P.convSettingsChanged = False
